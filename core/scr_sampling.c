// SAMPLING (docs/mockups/screens/15): records the output, or one track, into a WAV in RECORDINGS; then puts the
// take on a track, sliced if wanted. Without TWOFOLD there is no input from outside: the R36S has none.
#include "plat.h"
#include "ui_int.h"

enum { SR_SOURCE, SR_START, SR_LENGTH, SR_TO, SR_THEN, SR_NORM, SR_COUNT };
static const char *const sr_name[SR_COUNT] = { "SOURCE", "START", "LENGTH", "TO", "THEN", "NORMALIZE" };
static const char *const start_name[3] = { "NOW", "FROM THE TOP", "NEXT BAR" };
static const uint8_t start_val[3] = { 0, 1, 2 };
static const char *const len_name[] = { "1/2 BAR", "1 BAR", "2 BARS", "4 BARS", "8 BARS" };
static const char *const then_name[] = { "AS ONE", "SLICE 4", "SLICE 8", "SLICE 16" };
static const uint8_t then_slices[] = { 0, 4, 8, 16 };

static struct { uint8_t source, start, len, to, then, norm; } sp = { 0, 2, 1, 255, 2, 1 };
static char take[64];          // the last take on the card
static int take_slot = -1;     // and in memory, to draw and to listen to
static uint32_t take_frames;
static float rec_bars;

static uint32_t length_frames(void)
{
    const double bar = 60.0 / MAX(doc.bpm, 20.0f) * 4.0 * 48000.0;
    static const double mul[] = { 0.5, 1, 2, 4, 8 };
    rec_bars = (float) mul[sp.len];
    return (uint32_t) (bar * mul[sp.len]);
}

static int to_track(void) { return sp.to == 255 ? ui.cur_t : sp.to; }

static void taken(const uint8_t *wav, int bytes)
{
    int16_t *d = (int16_t *) (wav + 44);   // our own memory: the capture buffer
    const int n = (bytes - 44) / 2;
    if (sp.norm) {
        int peak = 1;
        for (int i = 0; i < n; i++) { const int a = d[i] < 0 ? -d[i] : d[i]; if (a > peak) peak = a; }
        const float g = MIN(32000.0f / (float) peak, 16.0f);
        for (int i = 0; i < n; i++) d[i] = (int16_t) CLAMP((int) ((float) d[i] * g), -32767, 32767);
    }
    for (int k = 1; k < 10000; k++) {
        k_snprintf(take, sizeof take, "RECORDINGS/REC_%04d.WAV", k);
        if (plat_file_size(take) < 0) break;
    }
    if (plat_file_write(take, wav, bytes) != bytes) { ui_toast("COULD NOT WRITE IT"); take[0] = 0; return; }
    take_frames = (uint32_t) n / 2;
    take_slot = ui_sample_preview(take);
    if (take_slot < 0) take_slot = ui_sample_load(take);   // no room set aside for listening: the main memory
    ui_toast("TAKE SAVED. Y PUTS IT ON THE TRACK");
}

static void record(void)
{
    if (ui_capturing()) { ui_toast("ALREADY RECORDING"); return; }
    if (!ui_capture(sp.source, start_val[sp.start], length_frames(), taken)) { ui_toast("NO ROOM TO RECORD"); return; }
    if (sp.start == 1) ui_msg(M_PLAY, 0, 0, 0, 0);
    else if (sp.start == 2 && !ui.st.playing) ui_msg(M_PLAY, 0, 0, 0, 0);
}

static void keep(void)
{
    if (!take[0]) { ui_toast("NO TAKE YET"); return; }
    const int t = to_track();
    track_t *tr = &doc.trk[t];
    const track_t was = *tr;
    track_sample(tr, take, was.name);
    tr->level = was.level; tr->pan = was.pan; tr->send_dly = was.send_dly; tr->send_rev = was.send_rev;
    tr->choke = was.choke; tr->mute = was.mute; tr->send = was.send; tr->midi_ch = was.midi_ch; tr->midi_note = was.midi_note;
    tr->smp.start = 0; tr->smp.end = 1; tr->smp.filter = 20000; tr->smp.bits = 16; tr->smp.rate = 48000; tr->smp.choke_self = 1;
    tr->smp.mode = SMP_ONESHOT;
    tr->smp.slices = then_slices[sp.then];
    m_set(tr->smp.slice_pos, 0, sizeof tr->smp.slice_pos);
    doc_touch(tr, sizeof *tr);
    ui_bind(t);
    char b[32]; k_snprintf(b, sizeof b, "ON TRACK %d", t + 1);
    ui_toast(ui_sample(t) ? b : "COULD NOT LOAD IT");
}

static void change(int r, int d)
{
    switch (r) {
    case SR_SOURCE: sp.source = (uint8_t) CLAMP(sp.source + d, 0, NTRK); break;
    case SR_START: sp.start = (uint8_t) CLAMP(sp.start + d, 0, 2); break;
    case SR_LENGTH: sp.len = (uint8_t) CLAMP(sp.len + d, 0, 4); break;
    case SR_TO: { int t = sp.to == 255 ? -1 : sp.to; t = CLAMP(t + d, -1, NTRK - 1); sp.to = t < 0 ? 255 : (uint8_t) t; break; }
    case SR_THEN: sp.then = (uint8_t) CLAMP(sp.then + d, 0, 3); break;
    default: sp.norm = !sp.norm; break;
    }
}

void sampling_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    int *r = &ui.sel[NAV_SAMPLING];
    switch (b) {
    case BTN_UP: *r = MAX(*r - 1, 0); break;
    case BTN_DOWN: *r = MIN(*r + 1, SR_COUNT - 1); break;
    case BTN_LEFT: case BTN_RIGHT: if (ui.held[BTN_A]) ui.used[BTN_A] = true; change(*r, b == BTN_RIGHT ? 1 : -1); break;
    case BTN_A: record(); break;
    case BTN_Y: keep(); break;
    case BTN_X: if (take_slot >= 0) ui_msg(M_PREVIEW, take_slot, 0, 0, 1.0f); else ui_toast("NO TAKE YET"); break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

static void value(int r, char *b, int cap)
{
    switch (r) {
    case SR_SOURCE: if (sp.source) k_snprintf(b, cap, "TRACK %d  %s", sp.source, doc.trk[sp.source - 1].name); else k_snprintf(b, cap, "MASTER"); return;
    case SR_START: k_snprintf(b, cap, "%s", start_name[sp.start]); return;
    case SR_LENGTH: k_snprintf(b, cap, "%s", len_name[sp.len]); return;
    case SR_TO: k_snprintf(b, cap, "TRACK %d  %s%s", to_track() + 1, doc.trk[to_track()].name, sp.to == 255 ? "  (THE ONE ON)" : ""); return;
    case SR_THEN: k_snprintf(b, cap, "%s", then_name[sp.then]); return;
    default: k_snprintf(b, cap, "%s", sp.norm ? "ON" : "OFF"); return;
    }
}

void draw_sampling(void)
{
    g_clear(C_BG);
    const col_t sc = nav_col(NAV_SAMPLING);
    w_header("SAMPLING", sc);
    const int sel = ui.sel[NAV_SAMPLING];
    char b[64];
    // what to record, and when
    g_rrect(4, 34, 226, 334, 4, C_PANEL);
    g_text(12, 40, "SOURCE", &f12b, sc);
    static const char *const src_l[3][2] = { { "MASTER", "resample the output" }, { "ONE TRACK", "as its engine plays it" }, { "TWOFOLD IN", "not connected" } };
    for (int k = 0; k < 3; k++) {
        const int y = 58 + k * 34;
        const bool on = k == 0 ? sp.source == 0 : k == 1 ? sp.source > 0 : false;
        if (on) g_rect(6, y - 2, 224, y + 30, c_mix(C_PANEL, sc, 0.3f));
        g_text(14, y, src_l[k][0], &f14, k == 2 ? C_DIMMER : C_TEXT);
        if (k == 1 && sp.source) k_snprintf(b, sizeof b, "track %d, %s", sp.source, doc.trk[sp.source - 1].name);
        g_text(14, y + 16, k == 1 && sp.source ? b : src_l[k][1], &f12, k == 2 ? C_DIMMER : C_DIM);
    }
    for (int r = 0; r < 3; r++) {
        const int y = 172 + r * 22;
        if (r == sel) { g_rect(6, y - 4, 224, y + 16, c_mix(C_PANEL, sc, 0.25f)); g_rect(6, y - 4, 8, y + 16, sc); }
        g_text(14, y, sr_name[r], &f12, r == sel ? C_TEXT : C_DIM);
        value(r, b, sizeof b);
        if (r == SR_SOURCE) k_snprintf(b, sizeof b, "%s", sp.source ? "ONE TRACK" : "MASTER");
        g_text_r(218, y, b, &f12b, C_TEXT);
    }
    g_text(14, 252, "A records. START NEXT BAR", &f12, C_DIMMER);
    g_text(14, 268, "waits for the bar line.", &f12, C_DIMMER);

    // the level coming in
    g_rrect(230, 34, 304, 334, 4, C_PANEL);
    g_text(238, 40, "IN", &f12b, C_DIM);
    const float il = sp.source ? ui.st.level[sp.source - 1] : ui.st.out_l, ir = sp.source ? ui.st.level[sp.source - 1] : ui.st.out_r;
    w_meter_v(246, 60, 312, MIN(il, 1.0f), C_GREEN, 14);
    w_meter_v(272, 60, 312, MIN(ir, 1.0f), C_GREEN, 14);
    g_text(248, 316, "L", &f12, C_DIM); g_text(274, 316, "R", &f12, C_DIM);

    // the take
    g_rrect(308, 34, 636, 200, 4, C_PANEL);
    const bool rec = ui_capturing();
    const uint32_t pos = ui.st.cap_pos, total = MAX(ui.st.cap_frames, 1u);
    if (rec) {
        const int cs = (int) (pos / 480u);   // hundredths
        k_snprintf(b, sizeof b, "\xe2\x97\x8f REC  %d:%02d.%02d", cs / 6000, cs / 100 % 60, cs % 100);
        w_chip(316, 40, pos ? b : "\xe2\x97\x8f WAITING", &f14, C_WHITE, pos ? C_RED : C_PANEL2, 8, 20);
        const float bars = (float) pos / (float) total * rec_bars;
        k_snprintf(b, sizeof b, "BAR %d OF %d", MIN((int) bars + 1, MAX((int) (rec_bars + 0.5f), 1)), MAX((int) (rec_bars + 0.5f), 1));
        g_text_r(628, 44, b, &f12, C_DIM);
    } else g_text(316, 42, take[0] ? take : "NO TAKE YET", &f12b, take[0] ? C_TEXT : C_DIM);
    const int X0 = 316, X1 = 628, Y0 = 68, Y1 = 192, cy = (Y0 + Y1) / 2;
    for (int k = 1; k < 8; k++) g_line(X0 + (X1 - X0) * k / 8, Y0, X0 + (X1 - X0) * k / 8, Y1, C_PANEL2, 1);
    g_line(X0, cy, X1, cy, C_LINE, 1);
    const sample_t *s = !rec && take_slot >= 0 ? ui_slot(take_slot) : NULL;
    if (s && s->frames) {
        for (int x = X0; x < X1; x++) {
            const uint32_t a = (uint32_t) ((uint64_t) (x - X0) * s->frames / (uint32_t) (X1 - X0)), e = (uint32_t) ((uint64_t) (x - X0 + 1) * s->frames / (uint32_t) (X1 - X0));
            int pk = 0;
            for (uint32_t i = a; i < e; i += 7) { const int v = s->data[i * s->chans]; pk = MAX(pk, v < 0 ? -v : v); }
            const int h = pk * (Y1 - Y0) / 2 / 32768;
            g_line(x, cy - h, x, cy + h, sc, 1);
        }
        if (sp.then) for (int k = 1; k < then_slices[sp.then]; k++) { const int x = X0 + (X1 - X0) * k / then_slices[sp.then]; g_line(x, Y0, x, Y1, C_AMBER, 1); }
    } else if (rec) {
        const int px = X0 + (int) ((float) (X1 - X0) * (float) pos / (float) total);
        g_rect(X0, cy - 1, px, cy + 1, C_RED);
        g_rect(px, Y0, px + 1, Y1, C_WHITE);
    }

    // afterwards
    g_rrect(308, 206, 636, 334, 4, C_PANEL);
    g_text(316, 212, "AFTER", &f12b, sc);
    for (int r = SR_TO; r < SR_COUNT; r++) {
        const int y = 232 + (r - SR_TO) * 22;
        if (r == sel) { g_rect(310, y - 4, 634, y + 16, c_mix(C_PANEL, sc, 0.25f)); g_rect(310, y - 4, 312, y + 16, sc); }
        g_text(318, y, sr_name[r], &f12, r == sel ? C_TEXT : C_DIM);
        value(r, b, sizeof b);
        g_text_r(626, y, b, &f12b, r == SR_TO ? trk_col[to_track()] : C_TEXT);
    }
    uint32_t used, room;
    ui_mem(&used, &room);
    k_snprintf(b, sizeof b, "ROOM  %d MB  \xc2\xb7  THIS TAKE %.1f MB", (int) ((room - used) >> 20), (double) length_frames() * 4.0 / 1048576.0);
    g_text(318, 304, b, &f12, C_DIM);

    // outside
    g_rrect(4, 340, 636, 452, 4, C_PANEL);
    g_text(12, 346, "INPUT FROM OUTSIDE", &f12b, C_DIM);
    g_text(14, 368, "The R36S has no input of its own. With TWOFOLD on the USB port, its two inputs", &f12, C_DIM);
    g_text(14, 384, "will show up here as sources.", &f12, C_DIM);
    g_text(14, 408, "Takes go to RECORDINGS on the card; the BROWSER finds them there too.", &f12, C_DIMMER);
    static const char *const hints[] = { "\xe2\x86\x91\xe2\x86\x93", "setting", "\xe2\x86\x90\xe2\x86\x92", "change", "A", "record", "Y", "put on track", "X", "listen", "B", "back" };
    w_footer(hints, COUNT(hints));
}
