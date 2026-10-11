#include "eng.h"
#include "fx.h"
#include "perf.h"
#include "plat.h"
#include "presets.h"
#include "smp.h"

#define CHUNK 256       // frames per pass (the tracks render a pass in parallel); timing is exact whatever it is
#define NEV   512
#define NTEV  64
#define NPEND 256

static float sr;
static doc_t ed;                         // the engine's copy of the document
static uint64_t frames;
static drum_t drum[NTRK];
static smp_track_t smp[NTRK];
static sample_t slot[NSMPSLOT];
static int8_t bind[NTRK];
static delay_fx_t dly;
static reverb_fx_t rev;
static master_fx_t mst;
static perf_t perf;
static smp_track_t pv_smp;            // the browser's listening
static drum_t pv_drum;
static int base_os;
static capture_t cap;
static float out_l, out_r;
static uint32_t cap_pos;
static bool cap_on, cap_wait_bar;

// live values (the sticks): on top of the document, never saved
static float live[NTRK][DRUM_NP];
static uint8_t live_on[NTRK][DRUM_NP];
static bool trk_dirty[NTRK];
static float tempo_override;             // tap or MIDI clock; 0 = the document's

static struct {
    bool playing, fill, song, song_fill;   // song_fill: a song entry's last bar fills
    // Time: sixteenths since play started. Positions come from an anchor (a frame and the sixteenth it is at) and
    // the tempo, never by adding up blocks, so the block size cannot move a hit by a frame.
    uint64_t anchor_frame;
    double anchor_pos, spf;              // spf: sixteenths per frame
    double stopped_pos;
    double pat_start;                    // where the current pattern started
    int pat, next;                       // next: -1 when nothing is queued
    uint32_t loops;                      // times the current pattern has gone round
    int64_t last[NTRK];                  // the last step each track has dealt with, counted from pat_start
    int64_t grace[NTRK];                 // the step whose flam grace note has gone already
    bool pre[NTRK];
    int song_pos, song_rep;
    double clock_next, pulse_next;
    int pulse_left;
    int pulse_at[16], npulse;
    uint32_t bar;
} sq;

typedef struct { int off; uint8_t trk, kind, step, locks; int8_t note; float vel; } ev_t;
enum { EV_HIT, EV_CHOKE };
static ev_t ev[NEV];
static int nev;
typedef struct { uint64_t at; uint8_t trk, kind, step, locks; int8_t note; float vel; uint8_t midi[3]; } pend_t;
enum { PD_HIT, PD_MIDI };
static pend_t pend[NPEND];
static int npend;

static uint8_t midi_in_mem[4096], midi_out_mem[16384];
static ring_t midi_in_q, midi_out_q;
static uint8_t mi_status, mi_data[2], mi_n;
static uint64_t mi_last_clock;
static float mi_clock_avg;

static uint32_t hits[NTRK];
static float meter[NTRK];
static float g_gain[NTRK], g_gate[NTRK];
static uint64_t prof_trk[NTRK], prof_seq, prof_mix, prof_dly, prof_rev, prof_mst, prof_frames;

uint64_t eng_frames(void) { return frames; }
int eng_drum_os = DRUM_MAXOS;
void (*eng_hit_hook)(int trk, uint64_t frame, float vel);
void (*eng_parallel)(void (*job)(int index), int count);

static float bpm_now(void) { return tempo_override > 0 ? tempo_override : MAX(ed.bpm, 20.0f); }

// ------------------------------------------------------------------------------------------------- MIDI out

// What the MIDI page's monitor shows: every message in and out (dir 0 in, 1 out).
static void monitor(int dir, uint8_t st, uint8_t d1, uint8_t d2)
{
    const msg_t m = { M_MON, (uint8_t) dir, st, d1, (float) d2, 0 };
    ring_put(&to_ui, &m, sizeof m);
}

static void midi_out(uint64_t at, uint8_t a, uint8_t b, uint8_t c, int n)
{
    monitor(1, a, n > 1 ? b : 0, n > 2 ? c : 0);
    uint8_t m[12];
    m_copy(m, &at, 8);
    m[8] = (uint8_t) n; m[9] = a; m[10] = b; m[11] = c;
    ring_put(&midi_out_q, m, 12);
}

int eng_midi_out(uint8_t *bytes, int max, uint64_t *frame)
{
    uint8_t m[12];
    if (ring_get(&midi_out_q, m, sizeof m) != 12) return 0;
    m_copy(frame, m, 8);
    const int n = MIN(m[8], max);
    for (int i = 0; i < n; i++) bytes[i] = m[9 + i];
    return n;
}

void eng_midi_in(const uint8_t *bytes, int n)
{
    while (n > 0) {
        const int k = MIN(n, 64);
        ring_put(&midi_in_q, bytes, (uint32_t) k);
        bytes += k; n -= k;
    }
}

// ------------------------------------------------------------------------------------------------- tracks

static void track_apply(int t)
{
    float v[DRUM_NP];
    m_copy(v, ed.trk[t].synth, sizeof v);
    for (int p = 0; p < DRUM_NP; p++) if (live_on[t][p]) v[p] = live[t][p];
    drum_set(&drum[t], v);
    trk_dirty[t] = false;
}

static void choke(int t)
{
    drum_choke(&drum[t]);
    smp_choke(&smp[t]);
}

static void pend_add(uint64_t at, int t, int kind, int step, int8_t note, float vel, bool locks)
{
    if (npend >= NPEND) return;
    pend[npend++] = (pend_t) { at, (uint8_t) t, (uint8_t) kind, (uint8_t) step, locks, note, vel, { 0, 0, 0 } };
}

static void ev_add(int off, int t, int kind, int step, int8_t note, float vel, bool locks)
{
    if (nev >= NEV) return;
    ev[nev++] = (ev_t) { off, (uint8_t) t, (uint8_t) kind, (uint8_t) step, locks, note, vel };
}

// What a hit does outside its track (the screen's flash, MIDI, the checks' hook): done in order, before the tracks
// render in parallel.
static void fire_side(int t, const ev_t *e, uint64_t at)
{
    if (eng_hit_hook) eng_hit_hook(t, at, e->vel);
    hits[t]++;
    const track_t *tr = &ed.trk[t];
    if (tr->send != SEND_INT && ed.io.notes_out) {
        const uint8_t ch = (uint8_t) (tr->midi_ch & 15), note = (uint8_t) (tr->midi_note & 127);
        midi_out(at, (uint8_t) (0x90 | ch), note, (uint8_t) CLAMP((int) (e->vel * 127.0f + 0.5f), 1, 127), 3);
        if (npend < NPEND)
            pend[npend++] = (pend_t) { .at = at + (uint64_t) (sr * 0.05f), .trk = (uint8_t) t, .kind = PD_MIDI, .midi = { (uint8_t) (0x80 | ch), note, 0 } };
    }
}

// The hit itself: a voice on track t (only track t's state: safe on any core).
static void fire_voice(int t, const ev_t *e, uint64_t at)
{
    track_t *tr = &ed.trk[t];
    if (tr->send == SEND_MIDI) return;
    const pattern_t *p = &ed.pat[CLAMP(sq.pat, 0, NPAT - 1)];
    if (tr->engine == ENG_SYNTH) {
        float v[DRUM_NP];
        bool any = e->note != 0;
        m_copy(v, drum[t].vals, sizeof v);
        if (e->locks)
            for (int i = 0; i < p->nlock; i++) {
                const lock_t *l = &p->lock[i];
                if (l->trk == t && l->step == e->step && l->param < DRUM_NP) { v[l->param] = l->val; any = true; }
            }
        if (e->note) v[P_TUNE] = m_clampf(v[P_TUNE] + (float) e->note, -24, 24);
        drum_trigger(&drum[t], e->vel, hash32((uint32_t) at * 31u + (uint32_t) t), any ? v : NULL);
    } else {
        smp_t sp = tr->smp;
        if (e->locks)
            for (int i = 0; i < p->nlock; i++) {
                const lock_t *l = &p->lock[i];
                if (l->trk != t || l->step != e->step) continue;
                switch (l->param) {
                case LK_SMP_PITCH: sp.pitch = l->val; break;
                case LK_SMP_START: sp.start = l->val; break;
                case LK_SMP_DECAY: sp.decay = l->val; break;
                case LK_SMP_FILTER: sp.filter = l->val; break;
                case LK_SMP_REVERSE: sp.reverse = l->val > 0.5f; break;
                default: break;
                }
            }
        smp[t].s = bind[t] >= 0 ? &slot[(int) bind[t]] : NULL;
        smp_trigger(&smp[t], &sp, e->vel, e->note > 0 ? e->note : 0);
    }
}


// ------------------------------------------------------------------------------------------------- sequencer

static inline double pos_at(uint64_t f) { return sq.anchor_pos + (double) (int64_t) (f - sq.anchor_frame) * sq.spf; }
static inline int64_t frame_of(double tk)
{
    const double x = (tk - sq.anchor_pos) / sq.spf + 1e-7;
    const int64_t i = (int64_t) x;
    return (int64_t) sq.anchor_frame + (i > x ? i - 1 : i);
}
static double spf_now(void) { return bpm_now() * 4.0 / (60.0 * sr); }

static void seq_reset_tracks(void)
{
    for (int t = 0; t < NTRK; t++) { sq.last[t] = -1; sq.grace[t] = -1; sq.pre[t] = false; }
}

static void seq_start(bool cont)
{
    sq.anchor_frame = frames;
    sq.spf = spf_now();
    if (!cont || sq.stopped_pos <= 0) {
        sq.anchor_pos = 0; sq.pat_start = 0; sq.loops = 0; sq.bar = 0;
        sq.clock_next = 0; sq.pulse_next = 0;
        seq_reset_tracks();
        if (sq.song && ed.song_len) { sq.song_rep = 0; sq.pat = ed.song[CLAMP(sq.song_pos, 0, ed.song_len - 1)].pat; }
    } else sq.anchor_pos = sq.stopped_pos;
    sq.playing = true;
    if (ed.io.transport_out) {
        if (ed.io.spp_out) { const int beats = (int) sq.anchor_pos; midi_out(frames, 0xF2, (uint8_t) (beats & 127), (uint8_t) ((beats >> 7) & 127), 3); }
        midi_out(frames, cont && sq.anchor_pos > 0 ? 0xFB : 0xFA, 0, 0, 1);
    }
}

static void seq_stop(void)
{
    if (!sq.playing) return;
    sq.stopped_pos = pos_at(frames);
    sq.playing = false;
    if (ed.io.transport_out) midi_out(frames, 0xFC, 0, 0, 1);
    for (int i = 0; i < npend; i++) if (pend[i].kind == PD_MIDI) midi_out(frames, pend[i].midi[0], pend[i].midi[1], 0, 3);
    npend = 0;
}

static bool cond_ok(const step_t *s, int t, int64_t k, int len)
{
    const uint32_t it = (uint32_t) (k / MAX(len, 1));
    bool r;
    switch (s->cond) {
    case COND_ALWAYS: return true;
    case COND_FILL: r = sq.fill || sq.song_fill; break;
    case COND_NOT_FILL: r = !(sq.fill || sq.song_fill); break;
    case COND_FIRST: r = sq.loops == 0 && it == 0; break;
    case COND_NOT_FIRST: r = !(sq.loops == 0 && it == 0); break;
    case COND_PRE: return sq.pre[t];
    case COND_NOT_PRE: return !sq.pre[t];
    default: {
        const int c = s->cond - COND_AB, a = c % 8 + 1, b = c / 8 + 2;
        r = (int) (it % (uint32_t) b) == a - 1;
    }
    }
    sq.pre[t] = r;
    return r;
}

// A song entry: its mutes, and its last bar on the last time round (FILL turns fills on, DROP leaves the last beat
// empty). True when track t stays quiet at sixteenth tk.
static bool song_quiet(int t, double tk, const pattern_t *p)
{
    sq.song_fill = false;
    if (!sq.song || !ed.song_len) return false;
    const song_t *e = &ed.song[CLAMP(sq.song_pos, 0, ed.song_len - 1)];
    if (e->mutes >> t & 1) return true;
    const int plen = CLAMP(p->len, 1, NSTEP);
    const double into = tk - sq.pat_start - (double) sq.loops * plen;
    if (sq.song_rep == MAX(e->repeat, 1) - 1 && e->next != SN_LOOP) {
        if (e->last_bar == LB_FILL && into >= plen - MIN(16, plen)) sq.song_fill = true;
        if (e->last_bar == LB_DROP && into >= plen - MIN(4, plen)) return true;
    }
    return false;
}

// Step k of track t is due at sixteenth tk, frame fk: the hit, its ratchets, chance and conditions.
static void seq_hit(int t, int64_t k, double tk, int64_t fk, int c, const pattern_t *p, int len, double step_len)
{
    const int si = (int) (k % len);
    const step_t *s = &p->step[t][si];
    if (song_quiet(t, tk, p)) return;
    if (!cond_ok(s, t, k, len)) return;
    if (s->chance < 100) {
        const uint32_t h = hash32((uint32_t) k * 2654435761u ^ (uint32_t) t * 40503u ^ (uint32_t) sq.loops * 97u ^ (uint32_t) sq.pat * 7919u);
        if (h % 100 >= s->chance) return;
    }
    const float vel = (s->flags & STEP_ACCENT) ? 1.0f : (float) s->vel / 127.0f;
    bool locks = false;
    for (int i = 0; i < p->nlock && !locks; i++) if (p->lock[i].trk == t && p->lock[i].step == si) locks = true;
    const int off = (int) CLAMP(fk - (int64_t) frames, 0, c - 1);
    ev_add(off, t, EV_HIT, si, s->note, vel, locks);
    const int n = step_hits(s);
    for (int j = 1; j < n; j++) {
        const float u = (float) j / (float) n;
        const float vj = step_rshape(s) == RAT_FADE ? vel * (1.0f - 0.65f * u) : step_rshape(s) == RAT_RISE ? vel * (0.35f + 0.65f * u) : vel;
        const int64_t fj = frame_of(tk + step_len * u);
        if (fj < (int64_t) frames + c) ev_add((int) MAX(fj - (int64_t) frames, off), t, EV_HIT, si, s->note, vj, locks);
        else pend_add((uint64_t) fj, t, PD_HIT, si, s->note, vj, locks);
    }
}

// The steps of track t that are due before the end of this pass (c frames) and before the pattern's end.
static void seq_track(int t, int c, double boundary)
{
    const pattern_t *p = &ed.pat[CLAMP(sq.pat, 0, NPAT - 1)];
    const int len = CLAMP(p->trk[t].len, 1, NSTEP);
    const double s = speed_mul[CLAMP(p->trk[t].speed, 0, SPD_COUNT - 1)];
    const double step_len = 1.0 / s;
    const double sw = (CLAMP(p->swing, 50, 75) - 50) / 50.0;
    const double flam_len = 0.018 * sr * sq.spf;              // 18 ms, in sixteenths
    const int64_t end = (int64_t) frames + c;
    for (int64_t k = sq.last[t] + 1;; k++) {
        const step_t *st = &p->step[t][k % len];
        double tk = sq.pat_start + ((double) k + ((k & 1) ? sw : 0.0) + (st->on ? st->nudge / 48.0 : 0.0)) / s;
        if (tk < sq.pat_start) tk = sq.pat_start;
        if (tk >= boundary) break;
        const int64_t fk = frame_of(tk);
        if (fk >= end) break;
        sq.last[t] = k;
        if (st->on) seq_hit(t, k, tk, fk, c, p, len, step_len);
    }
    // a flam's grace note comes before its step
    const int64_t kn = sq.last[t] + 1;
    const step_t *sn = &p->step[t][kn % len];
    if (sn->on && (sn->flags & STEP_FLAM) && sq.grace[t] != kn) {
        const double tn = sq.pat_start + ((double) kn + ((kn & 1) ? sw : 0.0) + sn->nudge / 48.0) / s;
        const int64_t fg = frame_of(tn - flam_len);
        if (fg < end && tn < boundary) {
            sq.grace[t] = kn;
            if (song_quiet(t, tn, p)) return;
            ev_add((int) CLAMP(fg - (int64_t) frames, 0, c - 1), t, EV_HIT, (uint8_t) (kn % len), sn->note, (float) sn->vel / 127.0f * 0.5f, false);
        }
    }
}

static void seq_boundary(void)
{
    sq.bar++;
    cap_wait_bar = false;
    if (sq.song && ed.song_len) {
        song_t *e = &ed.song[CLAMP(sq.song_pos, 0, ed.song_len - 1)];
        if (++sq.song_rep >= MAX(e->repeat, 1)) {
            sq.song_rep = 0;
            if (e->next == SN_STOP) { seq_stop(); sq.song_pos = 0; sq.stopped_pos = 0; return; }
            if (e->next != SN_LOOP) sq.song_pos = (sq.song_pos + 1) % ed.song_len;
            sq.next = ed.song[sq.song_pos].pat;
            if (ed.song[sq.song_pos].tempo > 0) tempo_override = ed.song[sq.song_pos].tempo;
        }
    }
    if (sq.next >= 0) {
        sq.pat_start = sq.pat_start + (double) (sq.loops + 1) * CLAMP(ed.pat[CLAMP(sq.pat, 0, NPAT - 1)].len, 1, NSTEP);
        sq.pat = sq.next; sq.next = -1; sq.loops = 0;
        seq_reset_tracks();
    } else sq.loops++;
}

// The sequencer's share of one pass of c frames: hits into ev[], timed to the frame.
static void seq_run(int c)
{
    const double spf = spf_now();
    if (spf != sq.spf) { sq.anchor_pos = pos_at(frames); sq.anchor_frame = frames; sq.spf = spf; }   // a new tempo from here
    const int64_t end = (int64_t) frames + c;
    for (int guard = 0; guard < 8; guard++) {
        const int len16 = CLAMP(ed.pat[CLAMP(sq.pat, 0, NPAT - 1)].len, 1, NSTEP);
        double boundary = sq.pat_start + (double) (sq.loops + 1) * len16;
        while (frame_of(boundary) < (int64_t) frames) { sq.loops++; boundary += len16; }   // the pattern got shorter under us
        for (int t = 0; t < NTRK; t++) seq_track(t, c, boundary);
        if (frame_of(boundary) < end) { seq_boundary(); if (!sq.playing) return; } else break;
    }
    // MIDI clock (24 a beat) and the sync pulse's starts
    while (ed.io.clock_out && frame_of(sq.clock_next) < end) {
        midi_out((uint64_t) MAX(frame_of(sq.clock_next), (int64_t) frames), 0xF8, 0, 0, 1);
        sq.clock_next += 1.0 / 6.0;
    }
    sq.npulse = 0;
    const double every = 4.0 / CLAMP(ed.io.pulse_ppq, 1, 24);
    const int64_t off = (int64_t) (m_clampf(ed.io.pulse_offset_ms, -20, 20) * 0.001f * sr);   // early or late, for what it drives
    while (frame_of(sq.pulse_next) + off < end) {
        if (sq.npulse < COUNT(sq.pulse_at)) sq.pulse_at[sq.npulse++] = (int) MAX(frame_of(sq.pulse_next) + off - (int64_t) frames, 0);
        sq.pulse_next += every;
    }
}

// ------------------------------------------------------------------------------------------------- MIDI in

static void midi_message(uint8_t st, uint8_t d1, uint8_t d2)
{
    const uint8_t type = st & 0xF0;
    if (type == 0x90 && d2 > 0) {
        if (!ed.io.notes_in) return;
        for (int t = 0; t < NTRK; t++)
            if (ed.trk[t].midi_note == d1) pend_add(frames, t, PD_HIT, 0, 0, d2 / 127.0f, false);
    } else if (type == 0xC0) {
        if (ed.io.prog_in && d1 < NPAT) sq.next = d1;
    }
}

static void midi_realtime(uint8_t b)
{
    switch (b) {
    case 0xF8:
        if (ed.io.clock_in != CLK_INTERNAL) {
            if (mi_last_clock) {
                const float dt = (float) (frames - mi_last_clock);
                if (dt > 0 && dt < sr) {
                    mi_clock_avg = mi_clock_avg > 0 ? mi_clock_avg * 0.9f + dt * 0.1f : dt;
                    tempo_override = 60.0f * sr / (mi_clock_avg * 24.0f);
                }
            }
            mi_last_clock = frames;
        }
        break;
    case 0xFA: if (ed.io.clock_in != CLK_INTERNAL) seq_start(false); break;
    case 0xFB: if (ed.io.clock_in != CLK_INTERNAL) seq_start(true); break;
    case 0xFC: if (ed.io.clock_in != CLK_INTERNAL) seq_stop(); break;
    default: break;
    }
}

static void midi_drain(void)
{
    uint8_t b[64];
    uint32_t n;
    while ((n = ring_get(&midi_in_q, b, sizeof b)) > 0)
        for (uint32_t i = 0; i < n; i++) {
            const uint8_t x = b[i];
            if (x >= 0xF8) { monitor(0, x, 0, 0); midi_realtime(x); continue; }
            if (x & 0x80) { mi_status = x; mi_n = 0; continue; }
            if (!mi_status) continue;
            mi_data[mi_n++] = x;
            const uint8_t type = mi_status & 0xF0;
            const int need = (type == 0xC0 || type == 0xD0) ? 1 : 2;
            if (mi_status >= 0xF0) { mi_n = 0; continue; }
            if (mi_n >= need) { monitor(0, mi_status, mi_data[0], need > 1 ? mi_data[1] : 0); midi_message(mi_status, mi_data[0], need > 1 ? mi_data[1] : 0); mi_n = 0; }
        }
}

// ------------------------------------------------------------------------------------------------- messages

static void handle(const uint8_t *m, uint32_t len)
{
    msg_t g;
    if (m[0] != M_PATCH && m[0] != M_SAMPLE) m_copy(&g, m, MIN(len, sizeof g));
    switch (m[0]) {
    case M_PATCH: {
        msg_patch_t h;
        m_copy(&h, m, sizeof h);
        if ((uint64_t) h.off + h.len > sizeof ed || len < sizeof h + h.len) break;
        m_copy((uint8_t *) &ed + h.off, m + sizeof h, h.len);
        const uint32_t t0 = (uint32_t) ((uint8_t *) &ed.trk[0] - (uint8_t *) &ed);
        for (int t = 0; t < NTRK; t++) {
            const uint32_t a = t0 + (uint32_t) (t * sizeof(track_t)), b = a + (uint32_t) sizeof(track_t);
            if (h.off < b && h.off + h.len > a) trk_dirty[t] = true;
        }
        break;
    }
    case M_PLAY: seq_start(g.a != 0); break;
    case M_STOP: seq_stop(); break;
    case M_QUEUE:
        if (g.a >= NPAT) break;
        if (!sq.playing || g.b) {
            sq.pat = g.a; sq.next = -1; sq.loops = 0;
            sq.pat_start = sq.playing ? pos_at(frames) : 0;
            seq_reset_tracks();
        } else sq.next = g.a;
        break;
    case M_SONG:
        sq.song = g.a != 0;
        sq.song_pos = CLAMP(g.b, 0, MAX(ed.song_len - 1, 0));
        sq.song_rep = 0;
        if (sq.song && ed.song_len) {
            sq.next = -1;
            if (!sq.playing) sq.pat = ed.song[sq.song_pos].pat; else sq.next = ed.song[sq.song_pos].pat;
        }
        break;
    case M_HIT:
        if (g.a < NTRK) pend_add(frames, g.a, PD_HIT, 0, (int8_t) (g.b ? (int) g.b - 64 : 0), m_clampf(g.f, 0, 1), false);
        break;
    case M_LIVE:
        if (g.a == 255) { m_set(live_on, 0, sizeof live_on); for (int t = 0; t < NTRK; t++) trk_dirty[t] = true; break; }
        if (g.a < NTRK && g.b < DRUM_NP) { live[g.a][g.b] = g.f; live_on[g.a][g.b] = g.c ? 0 : 1; trk_dirty[g.a] = true; }
        break;
    case M_FILL: sq.fill = g.a != 0; break;
    case M_PERF:
        if (g.a == 255) for (int k = 0; k < PF_COUNT; k++) perf_set(&perf, k, 0);
        else perf_set(&perf, g.a, g.f);
        break;
    case M_SAMPLE: {
        const uint8_t s = m[1];
        if (s >= NSMPSLOT || len < 4 + sizeof(sample_t)) break;
        sample_t smpl;
        m_copy(&smpl, m + 4, sizeof smpl);
        if (!smpl.data) {
            for (int t = 0; t < NTRK; t++) if (bind[t] == s) { for (int i = 0; i < SMP_VOICES; i++) smp[t].v[i].on = false; smp[t].s = NULL; }
            slot[s] = smpl;
            msg_t ack = { M_ACK, M_SAMPLE, s, 0, 0, 0 };
            ring_put(&to_ui, &ack, sizeof ack);
        } else slot[s] = smpl;
        break;
    }
    case M_CAPTURE:
        if (len >= 4 + sizeof(capture_t)) {
            m_copy(&cap, m + 4, sizeof cap);
            cap_pos = 0;
            cap_on = cap.buf && cap.frames;
            cap_wait_bar = cap.start == 2 && sq.playing;
            if (cap.start == 1) { seq_start(false); }
        }
        break;
    case M_BIND:
        if (g.a < NTRK) { bind[g.a] = g.b < NSMPSLOT ? (int8_t) g.b : -1; smp[g.a].s = bind[g.a] >= 0 ? &slot[bind[g.a]] : NULL; }
        break;
    case M_TEMPO: tempo_override = g.f > 0 ? m_clampf(g.f, 20, 400) : 0; break;
    case M_QUALITY: {
        const int os = g.a ? g.a : base_os;
        for (int t = 0; t < NTRK; t++) { if (drum[t].os != os) { drum_init(&drum[t], sr, os); trk_dirty[t] = true; } }
        break;
    }
    case M_PREVIEW:
        if (g.b == 1) {
            if (g.a < drum_preset_count) { float v[DRUM_NP]; drum_preset_values(g.a, v, true); v[P_PAN] = 0; drum_set(&pv_drum, v); drum_trigger(&pv_drum, 0.85f, 7, NULL); }
        } else if (g.a < NSMPSLOT && slot[g.a].data) {
            smp_t sp; m_set(&sp, 0, sizeof sp);
            sp.end = 1; sp.filter = 20000; sp.bits = 16; sp.rate = sr; sp.choke_self = 1;
            pv_smp.s = &slot[g.a];
            smp_trigger(&pv_smp, &sp, g.f > 0 ? g.f : 0.8f, 0);
        }
        break;
    default: break;
    }
}

// ------------------------------------------------------------------------------------------------- render

void eng_init(float rate)
{
    sr = rate;
    doc_defaults(&ed);
    frames = 0;
    for (int t = 0; t < NTRK; t++) {
        drum_init(&drum[t], sr, eng_drum_os);
        base_os = eng_drum_os;
        smp_init(&smp[t], sr);
        bind[t] = -1;
        g_gain[t] = 0; g_gate[t] = 1;
        trk_dirty[t] = true;
    }
    m_set(slot, 0, sizeof slot);
    smp_init(&pv_smp, sr);
    drum_init(&pv_drum, sr, 2);
    dly_init(&dly, sr);
    rev_init(&rev, sr);
    mst_init(&mst, sr);
    perf_init(&perf, sr);
    m_set(&sq, 0, sizeof sq);
    sq.next = -1;
    seq_reset_tracks();
    m_set(live_on, 0, sizeof live_on);
    tempo_override = 0;
    npend = nev = 0;
    ring_init(&midi_in_q, midi_in_mem, sizeof midi_in_mem);
    ring_init(&midi_out_q, midi_out_mem, sizeof midi_out_mem);
}

static float tl[NTRK][CHUNK], tr[NTRK][CHUNK];
static ev_t tev[NTRK][NTEV];
static int ntev[NTRK], pass_c;
static uint64_t pass_frame;

// One track through one pass: its voices start and stop where its events say. Touches only track t.
static void track_job(int t)
{
    const uint64_t t0 = plat_us();
    const int c = pass_c;
    m_set(tl[t], 0, sizeof(float) * (size_t) c);
    m_set(tr[t], 0, sizeof(float) * (size_t) c);
    const bool synth = ed.trk[t].engine == ENG_SYNTH;
    int cur = 0;
    for (int i = 0; i <= ntev[t]; i++) {
        const int to = i < ntev[t] ? tev[t][i].off : c;
        if (to > cur) {
            if (synth) drum_render(&drum[t], tl[t] + cur, tr[t] + cur, to - cur);
            else smp_render(&smp[t], tl[t] + cur, tr[t] + cur, to - cur);
            cur = to;
        }
        if (i == ntev[t]) break;
        if (tev[t][i].kind == EV_CHOKE) choke(t);
        else fire_voice(t, &tev[t][i], pass_frame + (uint64_t) tev[t][i].off);
    }
    prof_trk[t] += plat_us() - t0;
}

static void chunk(float *L, float *R, int c)
{
    uint64_t t0 = plat_us(), t1;
    nev = 0;
    for (int t = 0; t < NTRK; t++) if (trk_dirty[t]) track_apply(t);
    sq.npulse = 0;
    if (sq.playing) seq_run(c);
    // due pending hits and note-offs
    for (int i = 0; i < npend;) {
        if (pend[i].at < frames + (uint64_t) c) {
            const int off = pend[i].at > frames ? (int) (pend[i].at - frames) : 0;
            if (pend[i].kind == PD_HIT) ev_add(off, pend[i].trk, EV_HIT, pend[i].step, pend[i].note, pend[i].vel, pend[i].locks);
            else midi_out(frames + (uint64_t) off, pend[i].midi[0], pend[i].midi[1], 0, 3);
            pend[i] = pend[--npend];
        } else i++;
    }
    // chokes go with the hits that cause them
    const int nhits = nev;
    for (int i = 0; i < nhits; i++) {
        const int g = ed.trk[ev[i].trk].choke;
        if (g && ev[i].kind == EV_HIT)
            for (int u = 0; u < NTRK; u++) if (u != ev[i].trk && ed.trk[u].choke == g) ev_add(ev[i].off, u, EV_CHOKE, 0, 0, 0, false);
    }
    // in time order (stable), the side effects in that order, then one list per track
    for (int i = 1; i < nev; i++) { ev_t e = ev[i]; int j = i - 1; while (j >= 0 && ev[j].off > e.off) { ev[j + 1] = ev[j]; j--; } ev[j + 1] = e; }
    m_set(ntev, 0, sizeof ntev);
    for (int i = 0; i < nev; i++) {
        const int t = ev[i].trk;
        if (ev[i].kind == EV_HIT) fire_side(t, &ev[i], frames + (uint64_t) ev[i].off);
        if (ntev[t] < NTEV) tev[t][ntev[t]++] = ev[i];
    }
    t1 = plat_us(); prof_seq += t1 - t0;
    // the tracks, on as many cores as the platform gives
    pass_c = c; pass_frame = frames;
    if (eng_parallel) eng_parallel(track_job, NTRK);
    else for (int t = 0; t < NTRK; t++) track_job(t);
    t0 = plat_us();

    // mix: level, pan, mute and solo (smoothed), sends
    static float mixL[CHUNK], mixR[CHUNK], dL[CHUNK], dR[CHUNK], rL[CHUNK], rR[CHUNK], splitL[CHUNK], splitR[CHUNK];
    m_set(mixL, 0, sizeof mixL); m_set(mixR, 0, sizeof mixR); m_set(dL, 0, sizeof dL); m_set(dR, 0, sizeof dR);
    m_set(rL, 0, sizeof rL); m_set(rR, 0, sizeof rR); m_set(splitL, 0, sizeof splitL); m_set(splitR, 0, sizeof splitR);
    bool solo = false;
    for (int t = 0; t < NTRK; t++) if (ed.trk[t].solo) solo = true;
    const float gate_k = 1.0f - m_exp(-1.0f / (0.006f * sr));
    for (int t = 0; t < NTRK; t++) {
        const track_t *trk = &ed.trk[t];
        const float target = m_db(m_clampf(trk->level, -60, 12));
        const float gate_to = (trk->mute || (solo && !trk->solo)) ? 0.0f : 1.0f;
        const float pan = m_clampf(trk->pan, -1, 1);
        const float pl = m_cos((pan + 1) * K_PI * 0.25f) * 1.41421356f, pr = m_sin((pan + 1) * K_PI * 0.25f) * 1.41421356f;
        const float g0 = g_gain[t], dg = (target - g0) / (float) c;
        const float sd = m_clampf(trk->send_dly, 0, 1), srv = m_clampf(trk->send_rev, 0, 1);
        float pk = 0;
        for (int i = 0; i < c; i++) {
            g_gate[t] += (gate_to - g_gate[t]) * gate_k;
            const float g = (g0 + dg * (float) (i + 1)) * g_gate[t];
            const float l = tl[t][i] * g, r = tr[t][i] * g;
            const float ol = l * pl, or_ = r * pr;
            mixL[i] += ol; mixR[i] += or_;
            dL[i] += ol * sd; dR[i] += or_ * sd;
            rL[i] += ol * srv; rR[i] += or_ * srv;
            if (t < 6) splitL[i] += (l + r) * 0.5f; else splitR[i] += (l + r) * 0.5f;
            pk = MAX(pk, MAX(m_fabs(l), m_fabs(r)));
        }
        g_gain[t] = target;
        meter[t] = MAX(pk, meter[t] * 0.92f);
    }
    t1 = plat_us(); prof_mix += t1 - t0; t0 = t1;
    // the perform effects: DUB and FREEZE through the sends, the rest on the mix before the master
    const bool perf_on = perf_any(&perf);
    delay_t dp = ed.dly; reverb_t rp = ed.rev;
    if (perf_on) {
        float dub; bool freeze;
        perf_sends(&perf, mixL, mixR, dL, dR, rL, rR, c, &dub, &freeze);
        if (dub > 0.01f) { dp.repeats = MAX(dp.repeats, 0.55f + 0.4f * dub); dp.mix = MAX(dp.mix, 0.8f * dub); }
        if (freeze) { rp.freeze = 1; rp.ret = MAX(rp.ret, 0.7f); }
    }
    dly_process(&dly, &dp, bpm_now(), dL, dR, mixL, mixR, c);
    t1 = plat_us(); prof_dly += t1 - t0; t0 = t1;
    rev_process(&rev, &rp, rL, rR, mixL, mixR, c);
    t1 = plat_us(); prof_rev += t1 - t0; t0 = t1;
    perf_main(&perf, mixL, mixR, c, bpm_now());
    mst_process(&mst, &ed.mst, mixL, mixR, c);
    t1 = plat_us(); prof_mst += t1 - t0; t0 = t1;
    // recording: the output, or one track as it comes out of its engine
    if (cap_on && !cap_wait_bar) {
        const float *cl = cap.src ? tl[CLAMP(cap.src - 1, 0, NTRK - 1)] : mixL, *cr = cap.src ? tr[CLAMP(cap.src - 1, 0, NTRK - 1)] : mixR;
        const uint32_t k = MIN((uint32_t) c, cap.frames - cap_pos);
        for (uint32_t i = 0; i < k; i++) {
            cap.buf[2 * (cap_pos + i)] = (int16_t) (m_clampf(cl[i], -1, 1) * 32767.0f);
            cap.buf[2 * (cap_pos + i) + 1] = (int16_t) (m_clampf(cr[i], -1, 1) * 32767.0f);
        }
        cap_pos += k;
        if (cap_pos >= cap.frames) { cap_on = false; msg_t ack = { M_ACK, M_CAPTURE, 0, 0, 0, 0 }; ring_put(&to_ui, &ack, sizeof ack); }
    }
    if (smp_active(&pv_smp)) smp_render(&pv_smp, mixL, mixR, c);   // listening in the browser: after the master
    if (drum_active(&pv_drum)) drum_render(&pv_drum, mixL, mixR, c);

    // the headphone jack
    const int pulse_w = (int) (CLAMP(ed.io.pulse_ms, 1, 50) * 0.001f * sr);
    int pi = 0;
    for (int i = 0; i < c; i++) {
        float l = mixL[i], r = mixR[i];
        if (ed.io.jack == JACK_SYNC_MONO) {
            if (sq.playing && pi < sq.npulse && sq.pulse_at[pi] <= i) { sq.pulse_left = pulse_w; pi++; }
            r = (l + r) * 0.5f;
            l = sq.pulse_left > 0 ? m_clampf(ed.io.pulse_level, 0, 1) : 0.0f;
            if (sq.pulse_left > 0) sq.pulse_left--;
        } else if (ed.io.jack == JACK_SPLIT) { l = m_clampf(splitL[i], -1, 1); r = m_clampf(splitR[i], -1, 1); }
        L[i] = l; R[i] = r;
    }
    // what the screen sees: peaks and the scope
    {
        float pl = 0, pr = 0;
        uint32_t h = atomic_load_explicit(&eng_scope_head, memory_order_relaxed);
        for (int i = 0; i < c; i++) {
            pl = MAX(pl, m_fabs(mixL[i])); pr = MAX(pr, m_fabs(mixR[i]));
            eng_scope_l[h & (SCOPE_LEN - 1)] = mixL[i]; eng_scope_r[h & (SCOPE_LEN - 1)] = mixR[i]; h++;
        }
        atomic_store_explicit(&eng_scope_head, h, memory_order_release);
        out_l = MAX(pl, out_l * 0.92f); out_r = MAX(pr, out_r * 0.92f);
    }
    frames += (uint64_t) c;
    prof_frames += (uint64_t) c;
}

void eng_render(float *L, float *R, int n)
{
    const uint64_t t0 = plat_us();
    static uint8_t m[sizeof(msg_patch_t) + PATCH_MAX + 64];
    uint32_t len;
    while ((len = ring_get(&to_eng, m, sizeof m)) > 0) handle(m, len);
    midi_drain();
    const int total = n;
    while (n > 0) {
        const int c = MIN(n, CHUNK);
        chunk(L, R, c);
        L += c; R += c; n -= c;
    }
    // status for the screen
    status_begin();
    eng_status.playing = sq.playing;
    eng_status.pat = (uint8_t) sq.pat;
    eng_status.next_pat = (uint8_t) (sq.next < 0 ? 255 : sq.next);
    eng_status.song_on = sq.song;
    eng_status.song_pos = (uint8_t) sq.song_pos;
    eng_status.song_rep = (uint8_t) sq.song_rep;
    const pattern_t *p = &ed.pat[CLAMP(sq.pat, 0, NPAT - 1)];
    for (int t = 0; t < NTRK; t++) {
        const double s = speed_mul[CLAMP(p->trk[t].speed, 0, SPD_COUNT - 1)];
        const double pos = sq.playing ? pos_at(frames) : sq.stopped_pos;
        const int64_t k = pos >= sq.pat_start ? (int64_t) ((pos - sq.pat_start) * s) : 0;
        eng_status.step[t] = (uint8_t) (k % CLAMP(p->trk[t].len, 1, NSTEP));
        eng_status.hits[t] = hits[t];
        eng_status.level[t] = meter[t];
    }
    eng_status.bar = sq.bar;
    const double into = (sq.playing ? pos_at(frames) : sq.stopped_pos) - sq.pat_start;
    eng_status.beat = (float) (into - (double) (int64_t) (into / CLAMP(p->len, 1, NSTEP)) * CLAMP(p->len, 1, NSTEP)) / 4.0f;
    eng_status.frames = (uint32_t) frames;
    eng_status.comp_gr = mst.env_db;
    eng_status.limit_gr = mst.lim_gr;
    eng_status.bpm = bpm_now();
    eng_status.cap_pos = cap_on || cap_pos ? cap_pos : 0;
    eng_status.cap_frames = cap.frames;
    eng_status.out_l = out_l; eng_status.out_r = out_r;
    const uint64_t t1 = plat_us();
    const float block_us = (float) total / sr * 1e6f;
    eng_status.cpu = block_us > 0 ? eng_status.cpu * 0.9f + 0.1f * (float) (t1 - t0) / block_us : 0;
    if (prof_frames >= (uint64_t) (sr / 4)) {   // four times a second
        const float rt = (float) prof_frames / sr * 1e6f;
        for (int t = 0; t < NTRK; t++) { eng_status.cpu_trk[t] = (float) prof_trk[t] / rt; prof_trk[t] = 0; }
        eng_status.cpu_seq = (float) prof_seq / rt; eng_status.cpu_mix = (float) prof_mix / rt;
        eng_status.cpu_dly = (float) prof_dly / rt; eng_status.cpu_rev = (float) prof_rev / rt; eng_status.cpu_mst = (float) prof_mst / rt;
        prof_seq = prof_mix = prof_dly = prof_rev = prof_mst = 0; prof_frames = 0;
    }
    status_end();
}
