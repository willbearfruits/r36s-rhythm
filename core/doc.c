#include "doc.h"
#include "presets.h"

const float speed_mul[SPD_COUNT] = { 0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f };
const char *const speed_name[SPD_COUNT] = { "1/8", "1/4", "1/2", "3/4", "1", "3/2", "2" };

void doc_pattern_clear(pattern_t *p)
{
    m_set(p, 0, sizeof *p);
    p->len = 16;
    p->swing = 50;
    for (int t = 0; t < NTRK; t++) { p->trk[t].len = 16; p->trk[t].speed = SPD_1; }
    for (int t = 0; t < NTRK; t++)
        for (int s = 0; s < NSTEP; s++) { p->step[t][s].vel = 100; p->step[t][s].chance = 100; }
}

static void track_base(track_t *t, const char *name)
{
    m_set(t, 0, sizeof *t);
    k_strcpy(t->name, name, sizeof t->name);
    t->level = 0; t->pan = 0;
    t->midi_ch = 9; t->midi_note = 36;
    t->smp.start = 0; t->smp.end = 1; t->smp.pitch = 0;
    t->smp.attack = 0; t->smp.hold = 0; t->smp.decay = 0;
    t->smp.filter = 20000; t->smp.reso = 0; t->smp.bits = 16; t->smp.rate = 48000;
    t->smp.mode = SMP_ONESHOT; t->smp.choke_self = 1;
    drum_defaults(t->synth, DT_KICK);
}

void track_from_preset(track_t *t, int preset, const char *name)
{
    track_base(t, name);
    t->engine = ENG_SYNTH;
    drum_preset_values(preset, t->synth, true);
    t->synth[P_PAN] = 0;
    k_strcpy(t->sound, drum_presets[preset].name, sizeof t->sound);
    t->choke = (uint8_t) m_lround(t->synth[P_CHOKE]);
}

void track_sample(track_t *t, const char *path, const char *name)
{
    track_base(t, name);
    t->engine = ENG_SAMPLE;
    k_strcpy(t->smp.path, path, sizeof t->smp.path);
    const char *f = path;
    for (const char *c = path; *c; c++) if (*c == '/') f = c + 1;
    k_strcpy(t->sound, f, sizeof t->sound);
}

// One row of a pattern from text: . off, x hit, X accent, o ghost, ? half chance, p a locked hit, R ratchet x3
// fading, f flam, 1-9 a slice.
static void row(pattern_t *p, int t, const char *s)
{
    for (int i = 0; s[i] && i < NSTEP; i++) {
        step_t *st = &p->step[t][i];
        const char c = s[i];
        if (c == '.') continue;
        st->on = 1; st->vel = 100; st->chance = 100;
        if (c == 'X') { st->vel = 127; st->flags |= STEP_ACCENT; }
        else if (c == 'o') st->vel = 50;
        else if (c == '?') st->chance = 50;
        else if (c == 'R') st->ratchet = 2 | (RAT_FADE << 4);
        else if (c == 'f') st->flags |= STEP_FLAM;
        else if (c >= '1' && c <= '9') st->note = (int8_t) (c - '0');
    }
}

static void lock(pattern_t *p, int t, int s, int param, float v)
{
    if (p->nlock >= NLOCK) return;
    p->lock[p->nlock++] = (lock_t) { (uint8_t) t, (uint8_t) s, (uint8_t) param, 0, v };
}

static const char *const basement_rows[NTRK] = {
    "X.......X.x.....", "....X..o....X..o", "....x.......x..?", "..x....p..x...x.",
    "xoxoxoxoxoxoxoRo", "......x.......x.", ".............ppp", "..x..x...x.x",
    "oxoxoxoxoxoxoxox", "x.x.x.x.x.x.x.x.", "1..3..5.2..6.78.", ".......?......f.",
};

void doc_defaults(doc_t *d)
{
    m_set(d, 0, sizeof *d);
    d->magic = DOC_MAGIC; d->version = DOC_VERSION; d->size = sizeof *d;
    k_strcpy(d->name, "BASEMENT SESSIONS", sizeof d->name);
    k_strcpy(d->kit, "BASEMENT", sizeof d->kit);
    d->bpm = 124;
    static const struct { const char *name, *preset; uint8_t note; } kit[NTRK] = {
        { "KICK", "808 Classic", 36 }, { "SNARE", "909 Snare", 38 }, { "CLAP", "909 Clap", 39 }, { "RIM", "Woodblock", 37 },
        { "C.HAT", "808 Closed", 42 }, { "O.HAT", "808 Open", 46 }, { "TOM", "808 Tom Low", 41 }, { "CONGA", "Conga", 63 },
        { "SHAKER", "Shaker", 70 }, { "RIDE", "Ride", 51 }, { "CHOP", NULL, 60 }, { "TICK", "Metal Tick", 68 },
    };
    for (int t = 0; t < NTRK; t++) {
        if (kit[t].preset) track_from_preset(&d->trk[t], drum_preset_find(kit[t].preset), kit[t].name);
        else track_sample(&d->trk[t], "BUNDLED/BREAKS/DUSTY_94.WAV", kit[t].name);
        d->trk[t].midi_note = kit[t].note;
    }
    d->trk[10].smp.slices = 8;
    d->trk[9].mute = 1;
    d->trk[0].send_dly = 0.0f; d->trk[1].send_rev = 0.25f; d->trk[2].send_rev = 0.3f; d->trk[3].send_dly = 0.35f;
    d->trk[5].send_rev = 0.2f; d->trk[7].send_dly = 0.2f; d->trk[11].send_dly = 0.4f;
    d->trk[4].pan = -0.3f; d->trk[5].pan = -0.3f; d->trk[7].pan = -0.4f; d->trk[8].pan = 0.5f; d->trk[3].pan = 0.35f;

    for (int i = 0; i < NPAT; i++) doc_pattern_clear(&d->pat[i]);
    pattern_t *p = &d->pat[0];
    p->used = 1; p->swing = 54;
    for (int t = 0; t < NTRK; t++) row(p, t, basement_rows[t]);
    p->trk[7].len = 12;
    p->trk[8].speed = SPD_2;
    lock(p, 3, 7, P_TUNE, 5);
    lock(p, 6, 13, P_TUNE, 7); lock(p, 6, 14, P_TUNE, 3); lock(p, 6, 15, P_TUNE, -2);
    // A02: the same with a snare roll into the bar line
    d->pat[1] = *p;
    row(&d->pat[1], 1, "....X..o....XxRX");
    // A03..A05: half time, busy hats, the break up front
    d->pat[2] = *p;
    for (int t = 0; t < NTRK; t++) m_set(d->pat[2].step[t], 0, sizeof d->pat[2].step[t]);
    d->pat[2].nlock = 0;
    row(&d->pat[2], 0, "X.........x.....");
    row(&d->pat[2], 1, "........X.......");
    row(&d->pat[2], 4, "x.o.x.o.x.o.x.o.");
    row(&d->pat[2], 10, "1.....3.5...2...");
    for (int t = 0; t < NTRK; t++)
        for (int s = 0; s < NSTEP; s++) if (!d->pat[2].step[t][s].on) { d->pat[2].step[t][s].vel = 100; d->pat[2].step[t][s].chance = 100; }
    d->pat[3] = *p;
    row(&d->pat[3], 4, "xxxxxxxxxxxxxxxx");
    row(&d->pat[3], 5, "..x...x...x...x.");
    d->pat[4] = *p;
    row(&d->pat[4], 10, "1.2.3.4.5.6.7.8.");
    row(&d->pat[4], 0, "X.........X.....");
    for (int i = 0; i < NSONG; i++) d->song[i] = (song_t) { 0, 1, 0, 0, 0, 0, 0 };
    static const uint8_t chain[][2] = { { 0, 4 }, { 1, 2 }, { 0, 4 }, { 2, 4 }, { 3, 8 }, { 1, 1 }, { 4, 8 } };
    d->song_len = (uint8_t) COUNT(chain);
    for (int i = 0; i < d->song_len; i++) { d->song[i].pat = chain[i][0]; d->song[i].repeat = chain[i][1]; }

    d->dly = (delay_t) { DLY_SYNC_3_16, 1, { 0, 0 }, 0.3f, 0.55f, 0.4f, 0.3f, 0.6f, 0.25f, 0.4f };
    d->rev = (reverb_t) { 0.7f, 2.4f, 0.5f, 12, 1.0f, 0.2f, 0.4f, 0, { 0, 0, 0 } };
    d->mst = (master_t) { -12, 4, 10, 120, 3, 30, 18000, 0.2f, 1.0f, 1, OUT_CLEAN, 16, 0, -0.3f, 0 };
    d->io = (io_t) { 1, 1, 1, 0, 1, 1, 1, 1, JACK_STEREO, 2, 5, 0, 1.0f, 0 };
}

static uint8_t cond_list[64];
static int cond_count;

static void build_conds(void)
{
    if (cond_count) return;
    cond_list[cond_count++] = COND_ALWAYS;
    for (int b = 2; b <= 8; b++)
        for (int a = 1; a <= b; a++) cond_list[cond_count++] = (uint8_t) (COND_AB + (b - 2) * 8 + (a - 1));
    static const uint8_t rest[] = { COND_FILL, COND_NOT_FILL, COND_FIRST, COND_NOT_FIRST, COND_PRE, COND_NOT_PRE };
    for (int i = 0; i < COUNT(rest); i++) cond_list[cond_count++] = rest[i];
}

int cond_next(int cond, int dir)
{
    build_conds();
    int i = 0;
    while (i < cond_count && cond_list[i] != cond) i++;
    if (i == cond_count) return COND_ALWAYS;
    return cond_list[CLAMP(i + dir, 0, cond_count - 1)];
}

const char *cond_name(int cond, char *buf, int cap)
{
    static const char *const n[] = { "ALWAYS", "FILL", "NOT FILL", "FIRST", "NOT FIRST", "PRE", "NOT PRE" };
    if (cond < COND_AB) k_strcpy(buf, cond <= COND_NOT_PRE ? n[cond] : "?", cap);
    else k_snprintf(buf, cap, "%d:%d", (cond - COND_AB) % 8 + 1, (cond - COND_AB) / 8 + 2);
    return buf;
}

int lock_find(const pattern_t *p, int trk, int step, int param)
{
    for (int i = 0; i < p->nlock; i++)
        if (p->lock[i].trk == trk && p->lock[i].step == step && p->lock[i].param == param) return i;
    return -1;
}

const char *lock_param_name(int param)
{
    if (param < DRUM_NP) return drum_params[param].label;
    static const char *const n[] = { "Pitch", "Start", "Decay", "Filter", "Reverse", "Level", "Pan", "Delay", "Reverb" };
    return param >= LK_SMP_PITCH && param < LK_LAST ? n[param - LK_SMP_PITCH] : "?";
}
