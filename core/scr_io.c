// MIDI (docs/mockups/screens/24): what goes in and out, each track's channel and note, and a monitor of every
// message. SYNC (25): where the clock comes from, tap tempo, swing, and the headphone jack's sync pulse.
#include "ui_int.h"

char ui_midi_ports_text[160];
void ui_midi_ports(const char *list) { k_strcpy(ui_midi_ports_text, list, sizeof ui_midi_ports_text); }

static void note_name(int n, char *b, int cap)
{
    static const char *const nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    k_snprintf(b, cap, "%s%d", nm[n % 12], n / 12 - 2);
}

// ------------------------------------------------------------------------------------------------- the monitor

#define MON_N 22
static struct { uint8_t dir, st, d1, d2; uint16_t count; } mon[MON_N];
static int mon_n;
static bool learning;

void midi_monitor(int dir, uint8_t st, uint8_t d1, uint8_t d2)
{
    if (learning && dir == 0 && (st & 0xF0) == 0x90 && d2 > 0) {
        track_t *tr = &doc.trk[ui.cur_t];
        tr->midi_note = d1; tr->midi_ch = st & 15;
        doc_touch(&tr->midi_ch, 1); doc_touch(&tr->midi_note, 1);
        learning = false;
        ui_toast("LEARNED");
    }
    if (mon_n && st >= 0xF8 && mon[mon_n - 1].st == st && mon[mon_n - 1].dir == dir) { mon[mon_n - 1].count++; return; }
    if ((st & 0xF0) == 0x80) return;   // note offs: noise in a list this short
    if (mon_n == MON_N) { for (int i = 1; i < MON_N; i++) mon[i - 1] = mon[i]; mon_n--; }
    mon[mon_n].dir = (uint8_t) dir; mon[mon_n].st = st; mon[mon_n].d1 = d1; mon[mon_n].d2 = d2; mon[mon_n].count = 1;
    mon_n++;
}

static void mon_text(int i, char *l, int lc, char *r, int rc)
{
    const uint8_t st = mon[i].st, ch = (st & 15) + 1;
    r[0] = 0;
    switch (st & 0xF0) {
    case 0x90: k_snprintf(l, lc, "NOTE %02d", ch); note_name(mon[i].d1, r, rc); k_snprintf(r + k_strlen(r), rc - k_strlen(r), " %d", mon[i].d2); return;
    case 0xB0: k_snprintf(l, lc, "CC %d #%d", ch, mon[i].d1); k_snprintf(r, rc, "%d", mon[i].d2); return;
    case 0xC0: k_snprintf(l, lc, "PROG %d", ch); k_snprintf(r, rc, "%d", mon[i].d1); return;
    case 0xE0: k_snprintf(l, lc, "BEND %d", ch); return;
    default: break;
    }
    switch (st) {
    case 0xF8: k_snprintf(l, lc, "CLOCK"); k_snprintf(r, rc, "\xc3\x97%d", mon[i].count); return;
    case 0xFA: k_snprintf(l, lc, "START"); return;
    case 0xFB: k_snprintf(l, lc, "CONTINUE"); return;
    case 0xFC: k_snprintf(l, lc, "STOP"); return;
    case 0xF2: k_snprintf(l, lc, "SONG POS"); k_snprintf(r, rc, "%d", mon[i].d1 | mon[i].d2 << 7); return;
    case 0xFE: k_snprintf(l, lc, "SENSING"); k_snprintf(r, rc, "\xc3\x97%d", mon[i].count); return;
    default: k_snprintf(l, lc, "%02X", st); return;
    }
}

// ------------------------------------------------------------------------------------------------- MIDI

enum { MC_SEND, MC_CH, MC_NOTE, MC_COUNT };
static int midi_col;

static int midi_settings(prm_t *ps)
{
    int n = 0;
    ps[n++] = prm_choice("Notes out", &doc.io.notes_out, "OFF|ON");
    ps[n++] = prm_choice("Notes in", &doc.io.notes_in, "OFF|ON");
    ps[n++] = prm_choice("Clock out", &doc.io.clock_out, "OFF|ON");
    ps[n++] = prm_choice("Follow clock", &doc.io.clock_in, "NO|MIDI|AUTO");
    ps[n++] = prm_choice("Start/stop", &doc.io.transport_out, "OFF|SEND");
    ps[n++] = prm_choice("Song position", &doc.io.spp_out, "OFF|SEND");
    ps[n++] = prm_choice("Program in", &doc.io.prog_in, "OFF|PATTERN");
    return n;
}

void midi_button(int b, bool down, bool rep)
{
    (void) rep;
    const bool trk = ui.tab[NAV_MIDI] == 1;
    if (!down) {
        if (b == BTN_A && !ui.used[BTN_A] && trk) ui_msg(M_HIT, ui.cur_t, 0, 0, 0.8f);
        return;
    }
    if (b == BTN_R1 || b == BTN_R2) { ui.tab[NAV_MIDI] = !trk; return; }
    if (b == BTN_B) { if (learning) learning = false; else go(NAV_PATTERN); return; }
    if (!trk) {
        prm_t ps[8];
        const int n = midi_settings(ps);
        prm_nav(ps, n, &ui.sel[NAV_MIDI], 1, b, down);
        return;
    }
    int *col = &midi_col;
    track_t *tr = &doc.trk[ui.cur_t];
    if (ui.held[BTN_A] && b >= BTN_UP && b <= BTN_RIGHT) {
        ui.used[BTN_A] = true;
        const int d = (b == BTN_RIGHT || b == BTN_UP) ? 1 : -1, big = b == BTN_UP || b == BTN_DOWN;
        if (*col == MC_SEND) { tr->send = (uint8_t) CLAMP(tr->send + d, 0, 2); doc_touch(&tr->send, 1); }
        else if (*col == MC_CH) { tr->midi_ch = (uint8_t) CLAMP(tr->midi_ch + d, 0, 15); doc_touch(&tr->midi_ch, 1); }
        else { tr->midi_note = (uint8_t) CLAMP(tr->midi_note + d * (big ? 12 : 1), 0, 127); doc_touch(&tr->midi_note, 1); }
        return;
    }
    switch (b) {
    case BTN_UP: ui.cur_t = MAX(ui.cur_t - 1, 0); break;
    case BTN_DOWN: ui.cur_t = MIN(ui.cur_t + 1, NTRK - 1); break;
    case BTN_LEFT: *col = MAX(*col - 1, 0); break;
    case BTN_RIGHT: *col = MIN(*col + 1, MC_COUNT - 1); break;
    case BTN_X: learning = !learning; if (learning) ui_toast("PLAY A NOTE"); break;
    default: break;
    }
}

void draw_midi(void)
{
    g_clear(C_BG);
    const col_t mc = nav_col(NAV_MIDI);
    w_header("MIDI", mc);
    char b[48];
    const bool trk = ui.tab[NAV_MIDI] == 1;
    // the ports and the settings
    g_rrect(4, 34, 212, 452, 4, trk ? C_PANEL : c_mix(C_PANEL, mc, 0.05f));
    g_text(12, 40, "CONNECTED", &f12b, C_DIM);
    int y = 58, lines = 0;
    const char *s = ui_midi_ports_text;
    while (*s && lines < 4) {
        char line[40]; int k = 0;
        while (*s && *s != '\n' && k < (int) sizeof line - 1) line[k++] = *s++;
        line[k] = 0;
        if (*s == '\n') s++;
        if (k > 24) { line[23] = 0; }
        g_ellipse(12, y + 4, 17, y + 9, C_GREEN);
        g_text(24, y, line, &f12, C_TEXT);
        y += 16; lines++;
    }
    if (!lines) { g_text(12, y, ui_midi_ports_text[0] ? ui_midi_ports_text : "No devices. The computer can", &f12, C_DIM); g_text(12, y + 16, "subscribe to \"rhythm out\".", &f12, C_DIM); y += 32; }
    prm_t ps[8];
    const int n = midi_settings(ps);
    w_list(ps, n, trk ? -1 : ui.sel[NAV_MIDI], 8, 140, 200, 24, mc);
    g_text(12, 320, "Notes out: tracks set to MIDI", &f12, C_DIMMER);
    g_text(12, 336, "or BOTH. Notes in: a track", &f12, C_DIMMER);
    g_text(12, 352, "plays when its note comes in.", &f12, C_DIMMER);
    g_text(12, 376, "Program change picks the next", &f12, C_DIMMER);
    g_text(12, 392, "pattern (0 = A01).", &f12, C_DIMMER);

    // the tracks
    g_rrect(216, 34, 432, 452, 4, trk ? c_mix(C_PANEL, mc, 0.05f) : C_PANEL);
    g_text(224, 40, "TRACKS", &f12b, mc);
    g_text(300, 56, "SEND", &f12, C_DIMMER); g_text(354, 56, "CH", &f12, C_DIMMER); g_text(384, 56, "NOTE", &f12, C_DIMMER);
    static const char *const send_name[3] = { "INT", "MIDI", "BOTH" };
    const int col = midi_col;
    for (int t = 0; t < NTRK; t++) {
        const track_t *tr = &doc.trk[t];
        const int ty = 76 + t * 30;
        if (trk && t == ui.cur_t) g_rect(218, ty - 5, 430, ty + 19, c_mix(C_PANEL, trk_col[t], 0.2f));
        g_rect(224, ty, 226, ty + 14, trk_col[t]);
        char nm[8]; k_strcpy(nm, tr->name, sizeof nm);
        g_text(234, ty + 1, nm, &f12, trk_col[t]);
        const bool ext = tr->send != SEND_INT;
        w_chip(300, ty, send_name[CLAMP(tr->send, 0, 2)], &f12, ext ? C_INK : C_DIM, ext ? RGB(220, 90, 255) : C_PANEL2, 4, 16);
        k_snprintf(b, sizeof b, "%02d", tr->midi_ch + 1);
        g_text(354, ty + 1, b, &f12, C_TEXT);
        note_name(tr->midi_note, b, sizeof b);
        k_snprintf(b + k_strlen(b), (int) sizeof b - k_strlen(b), " %d", tr->midi_note);
        g_text(384, ty + 1, b, &f12, learning && t == ui.cur_t ? C_AMBER : C_TEXT);
        if (trk && t == ui.cur_t) {
            static const int cx[MC_COUNT][2] = { { 297, 342 }, { 350, 376 }, { 380, 430 } };
            g_frame(cx[col][0], ty - 3, cx[col][1], ty + 17, C_WHITE, 1);
        }
    }
    if (learning) g_text(224, 436, "LEARNING: play a note", &f12b, C_AMBER);

    // the monitor
    g_rrect(436, 34, 636, 452, 4, C_PANEL);
    g_text(444, 40, "MONITOR", &f12b, C_DIM);
    if (!mon_n) g_text(444, 64, "Nothing yet.", &f12, C_DIMMER);
    for (int i = 0; i < mon_n; i++) {
        const int my = 58 + i * 17;
        char l[24], r[24];
        mon_text(i, l, sizeof l, r, sizeof r);
        const col_t c = mon[i].dir ? C_AMBER : C_GREEN;
        g_text(444, my, mon[i].dir ? "\xe2\x86\x92" : "\xe2\x86\x90", &f12, c);
        g_text(460, my, l, &f12, i == mon_n - 1 ? C_TEXT : c_mix(C_DIM, C_TEXT, 0.4f));
        g_text_r(628, my, r, &f12, C_DIM);
    }
    static const char *const hs[] = { "\xe2\x86\x91\xe2\x86\x93", "setting", "\xe2\x86\x90\xe2\x86\x92", "change", "R1", "tracks", "B", "back" };
    static const char *const ht[] = { "\xe2\x86\x91\xe2\x86\x93", "track", "\xe2\x86\x90\xe2\x86\x92", "column", "A+\xe2\x86\x90\xe2\x86\x92", "change", "A", "listen", "X", "learn", "R1", "settings", "B", "back" };
    if (trk) w_footer(ht, COUNT(ht)); else w_footer(hs, COUNT(hs));
}

// ------------------------------------------------------------------------------------------------- SYNC

enum { SY_SOURCE, SY_BPM, SY_SWING, SY_JACK, SY_PULSES, SY_WIDTH, SY_LEVEL, SY_OFFSET, SY_PRESET, SY_COUNT };
static const uint8_t ppq_vals[4] = { 1, 2, 4, 24 };
static int preset_sel;
static const struct { const char *name, *l1, *l2; uint8_t jack, ppq, ms; } presets[] = {
    { "POCKET OP", "Pocket Operators: 2 a beat on the left (SY1, SY3).", "The beat is in mono on the right.", JACK_SYNC_MONO, 2, 5 },
    { "VOLCA", "Volcas and most sync-in jacks: 2 a beat, longer.", "The beat is in mono on the right.", JACK_SYNC_MONO, 2, 15 },
    { "LUNETTA", "CMOS and modular clocks: one a sixteenth.", "The beat is in mono on the right.", JACK_SYNC_MONO, 4, 5 },
    { "DAW (MIDI)", "A computer: clock, start and stop over MIDI.", "The jack plays the mix in stereo.", JACK_STEREO, 2, 5 },
};
static uint32_t taps[5];
static int ntap;

static prm_t sync_prm(int r)
{
    switch (r) {
    case SY_SOURCE: return prm_choice("Source", &doc.io.clock_in, "INTERNAL|MIDI|AUTO");
    case SY_BPM: return prm_f("Tempo", &doc.bpm, 40, 300, 1, UN_BPM);
    case SY_SWING: return prm_u8("Swing", &cur_pat()->swing, 50, 75, UN_INT);
    case SY_JACK: return prm_choice("Jack", &doc.io.jack, "STEREO|SYNC + MONO|SPLIT");
    case SY_WIDTH: return prm_u8("Width", &doc.io.pulse_ms, 1, 50, UN_INT);
    case SY_LEVEL: return prm_f("Level", &doc.io.pulse_level, 0, 1, 0.05f, UN_PCT);
    default: return prm_f("Offset", &doc.io.pulse_offset_ms, -20, 20, 0.5f, UN_MS);
    }
}

static void tap(void)
{
    const uint32_t now = ui.now;
    if (ntap && now - taps[(ntap - 1) % 5] > 2000) ntap = 0;
    taps[ntap % 5] = now; ntap++;
    if (ntap < 2) { ui_toast("TAP"); return; }
    const int k = MIN(ntap, 5);
    const uint32_t first = taps[(ntap - k) % 5];
    const float beat = (float) (now - first) / (float) (k - 1);
    doc.bpm = m_clampf(m_round(60000.0f / beat * 10) / 10, 40, 300);
    doc_touch(&doc.bpm, sizeof doc.bpm);
}

void sync_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) {
        if (b == BTN_A && !ui.used[BTN_A] && ui.sel[NAV_SYNC] == SY_PRESET) {
            const int p = preset_sel;
            doc.io.jack = presets[p].jack; doc.io.pulse_ppq = presets[p].ppq; doc.io.pulse_ms = presets[p].ms;
            if (p == 3) { doc.io.clock_out = 1; doc.io.transport_out = 1; }
            doc_touch(&doc.io, sizeof doc.io);
            ui_toast("SET");
        }
        return;
    }
    int *r = &ui.sel[NAV_SYNC];
    if (b == BTN_Y) { tap(); return; }
    if (b == BTN_B) { go(NAV_PATTERN); return; }
    if (b == BTN_UP) { *r = MAX(*r - 1, 0); return; }
    if (b == BTN_DOWN) { *r = MIN(*r + 1, SY_COUNT - 1); return; }
    if (b != BTN_LEFT && b != BTN_RIGHT) return;
    if (ui.held[BTN_A]) ui.used[BTN_A] = true;
    const int d = b == BTN_RIGHT ? 1 : -1;
    if (*r == SY_PRESET) { preset_sel = CLAMP(preset_sel + d, 0, COUNT(presets) - 1); return; }
    if (*r == SY_PULSES) {
        int i = 0;
        while (i < 3 && ppq_vals[i] < doc.io.pulse_ppq) i++;
        doc.io.pulse_ppq = ppq_vals[CLAMP(i + d, 0, 3)];
        doc_touch(&doc.io.pulse_ppq, 1);
        return;
    }
    const prm_t p = sync_prm(*r);
    prm_step(&p, d, false);
}

void draw_sync(void)
{
    g_clear(C_BG);
    const col_t sc = nav_col(NAV_SYNC);
    w_header("SYNC", sc);
    const int sel = ui.sel[NAV_SYNC];
    char b[64];
    // the clock
    g_rrect(4, 34, 212, 452, 4, C_PANEL);
    g_text(12, 40, "CLOCK", &f12b, sc);
    static const char *const src[] = { "INTERNAL", "MIDI", "AUTO" };
    if (sel == SY_SOURCE) g_rect(6, 56, 8, 78, sc);
    w_chips(14, 58, src, 3, doc.io.clock_in, sc, 18, 5, 4);
    const bool following = doc.io.clock_in != CLK_INTERNAL && m_fabs(ui.st.bpm - doc.bpm) > 0.05f;
    k_snprintf(b, sizeof b, "%.1f", (double) (following ? ui.st.bpm : doc.bpm));
    g_text(14, 90, b, &f32, following ? C_GREEN : C_TEXT);
    g_text(20 + g_tw(b, &f32), 108, "BPM", &f12, C_DIM);
    if (sel == SY_BPM) g_rect(6, 90, 8, 124, sc);
    const int beat = ui.st.playing ? (int) ui.st.beat % 4 : -1;
    for (int k = 0; k < 4; k++) g_ellipse(16 + k * 24, 134, 30 + k * 24, 148, k == beat ? sc : C_PANEL2);
    if (beat >= 0) { k_snprintf(b, sizeof b, "BEAT %d", beat + 1); g_text(116, 135, b, &f12, C_DIM); }
    w_chip(14, 160, "Y", &f12b, C_INK, C_TEXT, 5, 16);
    g_text(36, 161, "TAP TEMPO", &f12, C_TEXT);
    {
        const prm_t p = sync_prm(SY_SWING);
        if (sel == SY_SWING) g_rect(6, 190, 210, 210, c_mix(C_PANEL, sc, 0.25f));
        g_text(14, 194, "SWING", &f12, sel == SY_SWING ? C_TEXT : C_DIM);
        w_bar(70, 198, 90, (prm_get(&p) - 50) / 25.0f, sc, false, 6);
        k_snprintf(b, sizeof b, "%d%%", (int) prm_get(&p));
        g_text_r(204, 194, b, &f12b, C_TEXT);
    }
    g_text(14, 230, "MIDI IN", &f12, C_DIM);
    g_text_r(204, 230, doc.io.clock_in == CLK_INTERNAL ? "IGNORED" : following ? "FOLLOWING" : "WAITING", &f12, following ? C_GREEN : C_TEXT);
    g_text(14, 270, doc.io.clock_in == CLK_AUTO ? "AUTO: follows MIDI clock" : doc.io.clock_in == CLK_MIDI ? "MIDI: tempo, start and stop" : "INTERNAL: its own tempo;", &f12, C_DIMMER);
    g_text(14, 286, doc.io.clock_in == CLK_AUTO ? "when it comes, else runs" : doc.io.clock_in == CLK_MIDI ? "come from the clock in." : "MIDI clock in is ignored.", &f12, C_DIMMER);
    if (doc.io.clock_in == CLK_AUTO) g_text(14, 302, "on its own.", &f12, C_DIMMER);
    g_text(14, 330, "Swing is the pattern's:", &f12, C_DIMMER);
    k_snprintf(b, sizeof b, "%c%02d.", 'A' + ui.pat / NPERBANK, ui.pat % NPERBANK + 1);
    g_text(14, 346, b, &f12, C_DIMMER);

    // the jack
    g_rrect(216, 34, 636, 250, 4, C_PANEL);
    g_text(224, 40, "HEADPHONE JACK", &f12b, sc);
    static const char *const jn[3] = { "STEREO", "SYNC + MONO", "SPLIT" };
    static const char *const jl[3][2] = { { "MIX", "MIX" }, { "PULSE", "MIX" }, { "TRK 1-6", "TRK 7-12" } };
    for (int j = 0; j < 3; j++) {
        const int x = 224 + j * 136, on = doc.io.jack == j;
        g_rrect(x, 58, x + 128, 132, 5, on ? c_mix(C_PANEL2, C_AMBER, 0.15f) : C_PANEL2);
        if (on) g_rframe(x, 58, x + 128, 132, 5, C_AMBER, 2);
        if (sel == SY_JACK && on) g_rframe(x - 3, 55, x + 131, 135, 7, C_WHITE, 1);
        g_text_c(x + 64, 64, jn[j], &f12b, on ? C_TEXT : C_DIM);
        for (int ch = 0; ch < 2; ch++) {
            const int yy = 88 + ch * 24;
            g_text(x + 8, yy, ch ? "R" : "L", &f14, C_DIM);
            const bool pulse = j == JACK_SYNC_MONO && ch == 0;
            const col_t wc = j == 0 ? (ch ? C_SKY : C_RED) : j == 1 ? (pulse ? C_AMBER : RGB(255, 120, 200)) : (ch ? C_AMBER : RGB(150, 120, 255));
            for (int k = 0; k < (j == JACK_SPLIT ? 13 : 18); k++) {
                const int xx = x + 24 + k * 3;
                if (pulse) { const bool hi = (k / 4) % 2 == 0; g_rect(xx, hi ? yy + 2 : yy + 12, xx + 2, hi ? yy + 4 : yy + 14, on ? wc : C_DIMMER); if (k % 4 == 0) g_rect(xx, yy + 2, xx, yy + 14, on ? wc : C_DIMMER); }
                else { const int h = 2 + (int) (6 * m_fabs(m_sin((float) (k * 7 + ch * 3 + j)))); g_rect(xx, yy + 8 - h, xx + 1, yy + 8 + h, on ? wc : C_DIMMER); }
            }
            g_text_r(x + 122, yy + 2, jl[j][ch], &f12, on ? (pulse ? C_AMBER : C_TEXT) : C_DIMMER);
        }
    }
    // the pulse
    const bool sync = doc.io.jack == JACK_SYNC_MONO;
    const int ry0 = 146;
    if (sel == SY_PULSES) g_rect(218, ry0 - 4, 634, ry0 + 17, c_mix(C_PANEL, sc, 0.2f));
    g_text(228, ry0, "PULSES", &f12, sel == SY_PULSES ? C_TEXT : C_DIM);
    int ppi = 0;
    while (ppi < 3 && ppq_vals[ppi] < doc.io.pulse_ppq) ppi++;
    static const char *const pq[] = { "1", "2", "4", "24" };
    const int xe = w_chips(300, ry0 - 1, pq, 4, ppi, C_AMBER, 18, 6, 4);
    g_text(xe + 10, ry0, "A BEAT", &f12, C_DIM);
    for (int r = SY_WIDTH; r <= SY_OFFSET; r++) {
        const int yy = ry0 + 26 + (r - SY_WIDTH) * 24;
        const prm_t p = sync_prm(r);
        if (sel == r) g_rect(218, yy - 4, 634, yy + 17, c_mix(C_PANEL, sc, 0.2f));
        g_text(228, yy, r == SY_WIDTH ? "WIDTH" : r == SY_LEVEL ? "LEVEL" : "OFFSET", &f12, sel == r ? C_TEXT : sync ? C_DIM : C_DIMMER);
        w_bar(300, yy + 4, 260, r == SY_OFFSET ? prm_get(&p) / 20.0f : prm_norm(&p), sync ? C_AMBER : C_DIMMER, r == SY_OFFSET, 7);
        if (r == SY_WIDTH) k_snprintf(b, sizeof b, "%d ms", doc.io.pulse_ms);
        else if (r == SY_LEVEL) k_snprintf(b, sizeof b, "%d%%", (int) (doc.io.pulse_level * 100 + 0.5f));
        else k_snprintf(b, sizeof b, "%+.1f ms", (double) doc.io.pulse_offset_ms);
        g_text_r(628, yy, b, &f12b, C_TEXT);
    }

    // what goes out
    g_rrect(216, 256, 636, 452, 4, C_PANEL);
    g_text(224, 262, "WHAT GOES OUT", &f12b, C_DIM);
    const int step = ui.st.playing ? (int) (ui.st.beat * 4.0f) % 16 : -1;
    const float per_step = (float) doc.io.pulse_ppq / 4.0f;
    for (int s = 0; s < 16; s++) {
        const int x = 232 + s * 25;
        const bool p = per_step >= 1 || (s % (int) (1.0f / per_step + 0.5f)) == 0;
        g_rect(x, 282, x + 15, 297, s == step ? C_WHITE : p && sync ? C_AMBER : c_mix(C_PANEL, C_RED, 0.3f));
        if (sync && p) {
            const int n = per_step >= 1 ? (int) per_step : 1;
            for (int k = 0; k < MIN(n, 6); k++) {
                const int px = x + k * 25 / MAX(n, 1), w = MAX(2, MIN(doc.io.pulse_ms / 2, 25 / MAX(n, 1) - 2));
                g_line(px, 326, px, 308, C_AMBER, 1); g_line(px, 308, px + w, 308, C_AMBER, 1); g_line(px + w, 308, px + w, 326, C_AMBER, 1);
            }
        }
        if (s % 4 == 0) { k_snprintf(b, sizeof b, "%d", s + 1); g_text(x + 4, 332, b, &f12, C_DIMMER); }
    }
    g_line(232, 326, 628, 326, sync ? C_AMBER : C_DIMMER, 1);
    if (!sync) g_text_c(430, 306, "THE JACK PLAYS NO PULSE: SET SYNC + MONO", &f12, C_DIM);
    if (sel == SY_PRESET) g_rect(218, 350, 634, 372, c_mix(C_PANEL, sc, 0.2f));
    g_text(228, 355, "PRESETS", &f12, sel == SY_PRESET ? C_TEXT : C_DIM);
    int x = 296;
    for (int p = 0; p < COUNT(presets); p++) x += w_chip(x, 353, presets[p].name, &f12, p == preset_sel ? C_INK : C_DIM, p == preset_sel ? C_AMBER : C_PANEL2, 5, 16) + 4;
    g_text(228, 384, presets[preset_sel].l1, &f12, C_DIM);
    g_text(228, 400, presets[preset_sel].l2, &f12, C_DIM);
    if (sel == SY_PRESET) g_text(228, 424, "A sets it.", &f12, C_TEXT);
    static const char *const hints[] = { "\xe2\x86\x91\xe2\x86\x93", "setting", "\xe2\x86\x90\xe2\x86\x92", "change", "Y", "tap", "A", "set preset", "B", "back" };
    w_footer(hints, COUNT(hints));
}
