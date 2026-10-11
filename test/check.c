// Pass/fail checks on the core, run on the PC. `build/host/check` runs every group; name groups to run only those.
// Groups that need reference files (made by `make ref`) skip themselves when the files are not there.
#include "../core/drum.h"
#include "../core/eng.h"
#include "../core/perf.h"
#include "../core/presets.h"
#include "check.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int check_failed, check_passed;

void check_report(bool ok, const char *file, int line, const char *what)
{
    if (ok) { check_passed++; return; }
    check_failed++;
    printf("  FAIL %s:%d  %s\n", file, line, what);
}

// ---------------------------------------------------------------------------------------------- maths
static void g_maths(void)
{
    double worst = 0;
    for (int i = -4000; i <= 4000; i++) { float p = i / 997.0f; worst = fmax(worst, fabs(m_sin2pi(p) - sin(6.283185307179586 * p))); }
    CHECKF(worst < 1e-4, "sin2pi (fast) error %.2e", worst);
    worst = 0;
    for (int i = -4000; i <= 4000; i++) { float p = i / 997.0f; worst = fmax(worst, fabs(m_sin2pi_hq(p) - sin(6.283185307179586 * p))); }
    CHECKF(worst < 3e-7, "sin2pi (exact) error %.2e", worst);
    worst = 0;
    for (int i = -3000; i <= 3000; i++) { float x = i / 100.0f; double r = exp2((double) x); worst = fmax(worst, fabs(m_exp2(x) - r) / r); }
    CHECKF(worst < 3e-7, "exp2 (exact) relative error %.2e", worst);
    worst = 0;
    for (int i = -3000; i <= 3000; i++) { float x = i / 100.0f; double r = exp2((double) x); worst = fmax(worst, fabs(m_exp2_fast(x) - r) / r); }
    CHECKF(worst < 1e-4, "exp2 (fast) relative error %.2e", worst);
    // the case that matters: a per-sample decay multiplier near 1, within 2 ulp of the correctly rounded value
    int ulps = 0;
    for (int i = 1; i <= 1000; i++) {
        float x = -1.0f / (i * 0.0001f * 192000.0f), a = m_exp(x), b = (float) exp((double) x);
        int32_t ia, ib; memcpy(&ia, &a, 4); memcpy(&ib, &b, 4);
        ulps = MAX(ulps, abs(ia - ib));
    }
    CHECKF(ulps <= 2, "exp near 1: %d ulp off", ulps);
    worst = 0;
    for (int i = 1; i <= 20000; i++) { float x = i * 0.37f / 100.0f; worst = fmax(worst, fabs(m_log2(x) - log2((double) x))); }
    CHECKF(worst < 2e-6, "log2 error %.2e", worst);
    worst = 0;
    for (int i = 1; i < 490; i++) { float x = 3.14159265f * i / 1000.0f; double r = tan((double) x); worst = fmax(worst, fabs(m_tan(x) - r) / r); }
    CHECKF(worst < 1e-5, "tan relative error %.2e (up to 0.49 of the sample rate)", worst);
    worst = 0;
    for (int i = -900; i <= 900; i++) { float x = i / 100.0f; worst = fmax(worst, fabs(m_tanh(x) - tanh((double) x))); }
    CHECKF(worst < 1e-6, "tanh error %.2e", worst);
    CHECK(m_floor(-0.5f) == -1.0f && m_floor(2.0f) == 2.0f && m_floor(-3.0f) == -3.0f && m_floor(1e10f) == 1e10f);
    char b[64];
    k_snprintf(b, sizeof b, "%d|%5.1f|%-4s|%+d|%03d|%x|%.0f", -42, 3.14159, "ab", 7, 5, 255, 2.5);
    CHECKF(!strcmp(b, "-42|  3.1|ab  |+7|005|ff|3"), "snprintf gave \"%s\"", b);
}

// ---------------------------------------------------------------------------------------------- drum
// Renders a factory drum the way tools/thump_ref.cpp does: raw values, centred, one hit, blocks of 256.
static int render_drum_seed(int i, int os, uint32_t seed, float *out, int max);
static int render_drum(int i, int os, float *out, int max) { return render_drum_seed(i, os, 1, out, max); }
static int render_drum_seed(int i, int os, uint32_t seed, float *out, int max)
{
    static drum_t s;
    float v[DRUM_NP];
    drum_preset_values(i, v, false);
    v[P_PAN] = 0;
    drum_init(&s, 48000, os);
    drum_set(&s, v);
    drum_trigger(&s, 1.0f, seed, NULL);
    int n = 0;
    float L[256], R[256];
    while (n + 256 <= max) {
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        drum_render(&s, L, R, 256);
        memcpy(out + n, L, sizeof L);
        n += 256;
        if (!drum_active(&s)) break;
    }
    return n;
}

// thump's loudness: the loudest 10 ms window, dB RMS.
static double loudest_10ms(const float *x, int n)
{
    const int win = 480;
    double acc = 0, best = 0;
    for (int i = 0; i < n; i++) {
        acc += (double) x[i] * x[i];
        if (i >= win) acc -= (double) x[i - win] * x[i - win];
        best = fmax(best, acc);
    }
    return 10.0 * log10(best / win + 1e-24);
}

static void g_drum(void)
{
    static float ours[48000 * 4], ref[48000 * 4];
    int compared = 0;
    double worst_err = 0; int worst_i = -1;
    for (int i = 0; i < drum_preset_count; i++) {
        const int n = render_drum(i, 4, ours, COUNT(ours));
        double pk = 0, e2 = 0, s2 = 0;
        for (int k = 0; k < n; k++) pk = fmax(pk, fabs(ours[k]));
        CHECKF(pk > 0.01 && pk < 4.0, "%s: peak %.3f", drum_presets[i].name, pk);
        char path[64];
        snprintf(path, sizeof path, "build/ref/%02d.f32", i);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        const int m = (int) fread(ref, sizeof(float), COUNT(ref), f);
        fclose(f);
        CHECKF(abs(m - n) <= 256, "%s: %d samples here, %d in thump", drum_presets[i].name, n, m);
        for (int k = 0; k < MIN(n, m); k++) { const double d = ours[k] - ref[k]; e2 += d * d; s2 += (double) ref[k] * ref[k]; }
        const double err = sqrt(e2 / fmax(s2, 1e-30));    // error relative to the signal, RMS
        CHECKF(err < 0.01, "%s: differs from thump by %.2e (RMS, relative)", drum_presets[i].name, err);
        if (err > worst_err) { worst_err = err; worst_i = i; }
        compared++;
    }
    if (compared) printf("  %d drums against thump, worst %.2e (%s)\n", compared, worst_err, worst_i >= 0 ? drum_presets[worst_i].name : "");
    else printf("  no reference renders (make ref): checked levels only\n");
    // 2x, a fallback if the R36S runs short: it keeps each drum's loudness, averaged over 8 hits the way thump
    // levels its drums (single hits of noisy, resonant drums vary by several dB). Heavy fold and clip alias more at 2x.
    double worst2 = 0; int worst2_i = 0;
    static float two[48000 * 4];
    for (int i = 0; i < drum_preset_count; i++) {
        double a = 0, b = 0;
        for (int seed = 101; seed < 109; seed++) {
            const int n4 = render_drum_seed(i, 4, seed, ours, COUNT(ours)), n2 = render_drum_seed(i, 2, seed, two, COUNT(two));
            a += loudest_10ms(ours, n4) / 8; b += loudest_10ms(two, n2) / 8;
        }
        if (fabs(a - b) > worst2) { worst2 = fabs(a - b); worst2_i = i; }
    }
    printf("  2x oversampling against 4x: loudness within %.2f dB over 8 hits (%s)\n", worst2, drum_presets[worst2_i].name);
    CHECKF(worst2 < 4.0, "2x changes %s's loudness by %.2f dB", drum_presets[worst2_i].name, worst2);
}


// ---------------------------------------------------------------------------------------------- seq
// The engine plays; a hook records every hit's frame; the checks work out independently where each should be.
#define SR 48000
typedef struct { int trk; int64_t frame; float vel; } hit_t;
static hit_t hitlog[16384];
static int nhit;
static int64_t play_at;
static void hook(int t, uint64_t f, float v) { if (nhit < COUNT(hitlog)) hitlog[nhit++] = (hit_t) { t, (int64_t) f - play_at, v }; }

static void send(int type, int a, int b, float f) { msg_t m = { (uint8_t) type, (uint8_t) a, (uint8_t) b, 0, f, 0 }; ring_put(&to_eng, &m, sizeof m); }
static void run(int frames, int block) { static float L[4096], R[4096]; while (frames > 0) { const int n = MIN(frames, block); eng_render(L, R, n); frames -= n; } }

static void start(const doc_t *d)
{
    link_init();
    eng_init(SR);
    eng_hit_hook = hook;
    uint32_t off = 0;
    while (off < sizeof *d) { off += link_send_doc(d, off, (uint32_t) sizeof *d - off); run(64, 64); }
    run(64, 64);
    nhit = 0;
    play_at = (int64_t) eng_frames();
    send(M_PLAY, 0, 0, 0);
}

static double f16(const doc_t *d) { return 60.0 / (d->bpm * 4.0) * SR; }   // frames per sixteenth

// Where pattern p's hits fall in [0, until) frames, starting at sixteenth `from`, as hit_t; chance < 100 skipped.
static hit_t maybe[1024];
static int nmaybe;
static int expect(const doc_t *d, const pattern_t *p, double from, int64_t until, hit_t *out, int max, int only)
{
    int n = 0;
    nmaybe = 0;
    const double fps = f16(d), sw = (p->swing - 50) / 50.0, flam = 0.018 * SR;
    for (int t = 0; t < NTRK; t++) {
        if (only >= 0 && t != only) continue;
        const double s = speed_mul[p->trk[t].speed];
        for (int64_t k = 0;; k++) {
            const step_t *st = &p->step[t][k % p->trk[t].len];
            const double tk = from + ((double) k + ((k & 1) ? sw : 0) + (st->on ? st->nudge / 48.0 : 0)) / s;
            if ((int64_t) (tk * fps) >= until) break;
            if (st->on && st->chance < 100 && nmaybe < COUNT(maybe)) maybe[nmaybe++] = (hit_t) { t, (int64_t) (tk * fps), 0 };
            if (!st->on || st->chance < 100 || st->cond) continue;
            const double v = (st->flags & STEP_ACCENT) ? 1.0 : st->vel / 127.0;
            if (st->flags & STEP_FLAM) { if (n < max) out[n++] = (hit_t) { t, (int64_t) (tk * fps - flam), (float) (st->vel / 127.0 * 0.5) }; }
            const int hits = step_hits(st);
            for (int j = 0; j < hits && n < max; j++) out[n++] = (hit_t) { t, (int64_t) ((tk + (double) j / hits / s) * fps), (float) v };
        }
    }
    return n;
}

static int cmp_hit(const void *a, const void *b)
{
    const hit_t *x = a, *y = b;
    if (x->trk != y->trk) return x->trk - y->trk;
    return x->frame < y->frame ? -1 : x->frame > y->frame;
}

// Do the engine's hits (minus chance steps' tracks) match the expected ones, to a frame?
static bool same_hits(hit_t *want, int nw, hit_t *got, int ng, const char *what, int skip_trk)
{
    static hit_t g[16384];
    int n = 0;
    for (int i = 0; i < ng; i++) {
        bool chance = false;
        for (int k = 0; k < nmaybe && !chance; k++) chance = maybe[k].trk == got[i].trk && llabs(maybe[k].frame - got[i].frame) <= 1;
        if (got[i].trk != skip_trk && !chance) g[n++] = got[i];
    }
    qsort(want, (size_t) nw, sizeof *want, cmp_hit);
    qsort(g, (size_t) n, sizeof *g, cmp_hit);
    if (n != nw) { CHECKF(false, "%s: %d hits, expected %d", what, n, nw); return false; }
    for (int i = 0; i < n; i++)
        if (g[i].trk != want[i].trk || llabs(g[i].frame - want[i].frame) > 1) {
            CHECKF(false, "%s: track %d hit at frame %lld, expected track %d at %lld", what, g[i].trk, (long long) g[i].frame, want[i].trk, (long long) want[i].frame);
            return false;
        }
    CHECK(true);
    return true;
}

static void g_seq(void)
{
    static doc_t d;
    static hit_t want[16384], first[16384];
    doc_defaults(&d);
    const int64_t bar = (int64_t) (16 * f16(&d));
    // 1. the default pattern for two bars: swing, accents, ghosts, polymeter (CONGA 12), double speed (SHAKER),
    //    a ratchet (C.HAT), a flam (TICK), locks; the two 50 % steps may or may not play
    start(&d);
    run((int) (bar * 2), 256);
    const int64_t until = bar * 2;
    int nw = expect(&d, &d.pat[0], 0, until, want, COUNT(want), -1);
    int n = 0; for (int i = 0; i < nhit; i++) if (hitlog[i].frame < until) hitlog[n++] = hitlog[i];
    int m = n;
    if (!same_hits(want, nw, hitlog, m, "A01, two bars", -1)) {
        int cw[NTRK] = { 0 }, cg[NTRK] = { 0 };
        for (int i = 0; i < nw; i++) cw[want[i].trk]++;
        for (int i = 0; i < m; i++) cg[hitlog[i].trk]++;
        for (int t = 0; t < NTRK; t++) if (cw[t] != cg[t]) printf("    track %d: %d hits, expected %d\n", t, cg[t], cw[t]);
    }
    printf("  A01: %d hits in two bars at the frames worked out by hand\n", m);
    // 2. the block size does not change a single frame
    start(&d); run((int) bar * 2, 256); memcpy(first, hitlog, sizeof(hit_t) * (size_t) nhit); const int n0 = nhit;
    const int blocks[] = { 1, 37, 64, 100, 1024 };
    for (int b = 0; b < COUNT(blocks); b++) {
        start(&d); run((int) bar * 2, blocks[b]);
        bool same = nhit == n0;
        for (int i = 0; same && i < nhit; i++) same = hitlog[i].trk == first[i].trk && hitlog[i].frame == first[i].frame;
        CHECKF(same, "blocks of %d frames: %d hits, %d with blocks of 256 (or different frames)", blocks[b], nhit, n0);
    }
    // 3. a queued pattern starts exactly on the bar line
    start(&d);
    run((int) bar / 2, 256);
    send(M_QUEUE, 1, 0, 0);
    run((int) bar * 3 / 2, 256);
    nw = expect(&d, &d.pat[1], 16, bar * 2, want, COUNT(want), 1);   // the snare in bar 2 is A02's
    m = 0; for (int i = 0; i < nhit; i++) if (hitlog[i].trk == 1 && hitlog[i].frame >= bar && hitlog[i].frame < bar * 2) hitlog[m++] = hitlog[i];
    int w2 = 0; for (int i = 0; i < nw; i++) if (want[i].frame >= bar) want[w2++] = want[i];
    same_hits(want, w2, hitlog, m, "A02 queued: its snare in bar 2", -1);
    // 4. conditions: 1:2 plays on loops 1, 3; 2:2 on loops 2, 4; FILL only with fill held; NOT PRE after a 1:2
    static doc_t c;
    doc_defaults(&c);
    pattern_t *p = &c.pat[0];
    doc_pattern_clear(p);
    for (int t = 0; t < 4; t++) p->step[t][0].on = 1;
    p->step[0][0].cond = COND_AB + 0;        // 1:2
    p->step[1][0].cond = COND_AB + 1;        // 2:2
    p->step[2][0].cond = COND_FILL;
    p->step[3][0].cond = COND_NOT_FILL;
    start(&c);
    run((int) bar * 4, 256);
    int per[4] = { 0 }; int64_t at[4][8]; for (int i = 0; i < nhit; i++) if (hitlog[i].trk < 4 && per[hitlog[i].trk] < 8) at[hitlog[i].trk][per[hitlog[i].trk]++] = hitlog[i].frame / bar;
    CHECKF(per[0] == 2 && at[0][0] == 0 && at[0][1] == 2, "1:2 played %d times", per[0]);
    CHECKF(per[1] == 2 && at[1][0] == 1 && at[1][1] == 3, "2:2 played %d times", per[1]);
    CHECKF(per[2] == 0 && per[3] == 4, "FILL %d, NOT FILL %d (fill off)", per[2], per[3]);
    send(M_FILL, 1, 0, 0); nhit = 0; run((int) bar * 2, 256);
    per[2] = per[3] = 0; for (int i = 0; i < nhit; i++) if (hitlog[i].trk == 2 || hitlog[i].trk == 3) per[hitlog[i].trk]++;
    CHECKF(per[2] == 2 && per[3] == 0, "with fill held: FILL %d, NOT FILL %d", per[2], per[3]);
    // 5. chance: 50 % over 64 loops lands near 32 and is the same every time it is played
    doc_pattern_clear(p);
    p->step[0][0].on = 1; p->step[0][0].chance = 50; p->len = 1; p->trk[0].len = 1;
    start(&c); run((int) (f16(&c) * 200), 256);
    const int got = nhit;
    start(&c); run((int) (f16(&c) * 200), 256);
    CHECKF(got > 70 && got < 130, "50 %% chance hit %d of 200", got);
    CHECKF(got == nhit, "chance is repeatable: %d then %d", got, nhit);
    // 6. MIDI out: notes where the hits are, clock at 24 a beat
    doc_defaults(&c);
    c.trk[0].send = SEND_BOTH;
    start(&c);
    run((int) bar, 256);
    int notes = 0, clocks = 0; uint8_t b[3]; uint64_t fr;
    int kicks = 0; for (int i = 0; i < nhit; i++) if (hitlog[i].trk == 0) kicks++;
    int nb;
    while ((nb = eng_midi_out(b, 3, &fr)) > 0) { if (b[0] == 0xF8) clocks++; if ((b[0] & 0xF0) == 0x90) notes++; }
    CHECKF(notes == kicks && kicks == 3, "MIDI: %d notes for %d kick hits", notes, kicks);
    CHECKF(clocks >= 96 && clocks <= 97, "MIDI clock: %d in a bar", clocks);
    // 7. MIDI in: note 36 plays the kick
    start(&c); send(M_STOP, 0, 0, 0); run(256, 256); nhit = 0;
    const uint8_t note_on[3] = { 0x99, 36, 100 };
    eng_midi_in(note_on, 3);
    run(256, 256);
    CHECKF(nhit == 1 && hitlog[0].trk == 0, "MIDI note 36 in: %d hits", nhit);
    eng_hit_hook = NULL;
}

// A song: an entry's mutes, its last bar dropped, and a stop at the end.
static void g_song(void)
{
    static doc_t d;
    doc_defaults(&d);
    d.song_len = 2;
    d.song[0] = (song_t) { 0, 1, LB_DROP, SN_GO_ON, 1u << 1, 0, 0 };
    d.song[1] = (song_t) { 0, 1, LB_AS_IS, SN_STOP, 0, 0, 0 };
    d.pat[0].len = 16;
    for (int t = 0; t < NTRK; t++) d.pat[0].trk[t].len = 16;
    for (int s = 12; s < 16; s++) d.pat[0].step[0][s] = (step_t) { 1, 100, 0, 0, 100, 0, 0, 0 };   // the kick's last beat
    link_init();
    eng_init(SR);
    eng_hit_hook = hook;
    uint32_t off = 0;
    while (off < sizeof d) { off += link_send_doc(&d, off, (uint32_t) sizeof d - off); run(64, 64); }
    nhit = 0;
    play_at = (int64_t) eng_frames();
    send(M_SONG, 1, 0, 0);
    send(M_PLAY, 0, 0, 0);
    const double bar = f16(&d) * 16;
    run((int) (bar * 3), 256);
    int snare1 = 0, snare2 = 0, drop = 0, kick2 = 0, after = 0;
    for (int i = 0; i < nhit; i++) {
        const double at = (double) hitlog[i].frame;
        if (at >= bar * 2 + 64) after++;
        else if (at < bar - 2) { if (hitlog[i].trk == 1) snare1++; if (at >= bar * 0.75 - 2) drop++; }
        else { if (hitlog[i].trk == 1) snare2++; if (hitlog[i].trk == 0 && at >= bar * 1.75 - 2) kick2++; }
    }
    CHECKF(snare1 == 0 && snare2 > 0, "muted in the first entry: %d snare hits there, %d in the second", snare1, snare2);
    CHECKF(drop == 0 && kick2 == 4, "the first entry's last beat dropped: %d hits in it, %d kicks in the second's", drop, kick2);
    status_t st;
    status_read(&st);
    CHECKF(!st.playing && after == 0, "stopped after the last entry: playing %d, %d hits after", st.playing, after);
    eng_hit_hook = NULL;
}

// The perform effects: off, nothing changes; on, each one sounds (and stays sane).
static void g_perf(void)
{
    static perf_t p;
    static float L[256], R[256], L0[256];
    perf_init(&p, SR);
    rng_t r; rng_seed(&r, 5);
    bool same = true;
    for (int b = 0; b < 400; b++) {   // two seconds of history
        for (int i = 0; i < 256; i++) L0[i] = L[i] = R[i] = (rng_uni(&r) * 2 - 1) * 0.3f * ((b * 256 + i) % 12000 < 3000);
        perf_main(&p, L, R, 256, 120);
        for (int i = 0; i < 256; i++) if (L[i] != L0[i]) same = false;
    }
    CHECKF(same, "with nothing on, the sound goes through untouched");
    for (int f = 0; f < PF_COUNT; f++) {
        if (f == PF_DUB || f == PF_FREEZE) continue;   // those go through the sends
        for (int dir = (f == PF_FILTER ? -1 : 1); dir <= 1; dir += 2) {
            for (int b = 0; b < 20; b++) { for (int i = 0; i < 256; i++) L[i] = R[i] = (rng_uni(&r) * 2 - 1) * 0.3f; perf_main(&p, L, R, 256, 120); }
            perf_set(&p, f, 0.9f * (float) dir);
            double e = 0, pk = 0; bool finite = true;
            for (int b = 0; b < 188; b++) {
                for (int i = 0; i < 256; i++) L[i] = R[i] = (rng_uni(&r) * 2 - 1) * 0.3f;
                float dL[256] = { 0 }, dR[256] = { 0 }, rL[256] = { 0 }, rR[256] = { 0 }, dub; bool frz;
                perf_sends(&p, L, R, dL, dR, rL, rR, 256, &dub, &frz);
                perf_main(&p, L, R, 256, 120);
                for (int i = 0; i < 256; i++) { if (!isfinite(L[i])) finite = false; e += (double) L[i] * L[i]; pk = fmax(pk, fabs(L[i])); }
            }
            const double rms = sqrt(e / (188 * 256));
            CHECKF(finite && pk < 2 && rms > (f == PF_TAPE ? 1e-4 : 0.01), "%s at %+.1f: rms %.4f, peak %.2f", perf_name[f], 0.9 * dir, rms, pk);
            perf_set(&p, f, 0);
            for (int b = 0; b < 40; b++) { for (int i = 0; i < 256; i++) L[i] = R[i] = 0; perf_main(&p, L, R, 256, 120); }
        }
    }
}

// ---------------------------------------------------------------------------------------------- main
typedef struct { const char *name; void (*fn)(void); } group_t;
static const group_t groups[] = {
    { "maths", g_maths },
    { "drum", g_drum },
    { "seq", g_seq },
    { "song", g_song },
    { "perf", g_perf },
};

int main(int argc, char **argv)
{
    for (int g = 0; g < COUNT(groups); g++) {
        bool run = argc < 2;
        for (int a = 1; a < argc; a++) if (!strcmp(argv[a], groups[g].name)) run = true;
        if (!run) continue;
        const int before = check_failed;
        printf("%s\n", groups[g].name);
        groups[g].fn();
        if (check_failed == before) printf("  ok\n");
    }
    printf("%d passed, %d failed\n", check_passed, check_failed);
    return check_failed ? 1 : 0;
}
