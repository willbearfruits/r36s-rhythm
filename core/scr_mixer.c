// MIXER: twelve strips and the master (docs/mockups/screens/16). Each strip: drive, the two sends, pan, level.
#include "ui_int.h"

enum { MR_DRIVE, MR_DELAY, MR_REVERB, MR_PAN, MR_LEVEL, MR_COUNT };
static const char *const rows[MR_COUNT] = { "DRIVE", "DELAY", "REVERB", "PAN", "LEVEL" };

static prm_t strip_prm(int t, int r)
{
    track_t *tr = &doc.trk[t];
    switch (r) {
    case MR_DRIVE: return tr->engine == ENG_SYNTH ? prm_f("Drive", &tr->synth[P_DRIVE_AMOUNT], 0, 1, 0.02f, UN_PCT) : prm_f("Drive", &tr->smp.drive, 0, 1, 0.02f, UN_PCT);
    case MR_DELAY: return prm_f("Delay", &tr->send_dly, 0, 1, 0.02f, UN_PCT);
    case MR_REVERB: return prm_f("Reverb", &tr->send_rev, 0, 1, 0.02f, UN_PCT);
    case MR_PAN: return prm_f("Pan", &tr->pan, -1, 1, 0.05f, UN_NONE);
    default: return prm_f("Level", &tr->level, -60, 12, 0.5f, UN_DB);
    }
}

static prm_t master_prm(int r)
{
    switch (r) {
    case MR_DELAY: return prm_f("Delay return", &doc.dly.mix, 0, 1, 0.02f, UN_PCT);
    case MR_REVERB: return prm_f("Reverb return", &doc.rev.ret, 0, 1, 0.02f, UN_PCT);
    case MR_DRIVE: return prm_f("Drive", &doc.mst.drive, 0, 1, 0.02f, UN_PCT);
    case MR_PAN: return prm_f("Width", &doc.mst.width, 0, 2, 0.05f, UN_PCT);
    default: return prm_f("Volume", &doc.mst.volume, -40, 6, 0.5f, UN_DB);
    }
}

void mixer_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) {
        if (b == BTN_A && !ui.used[BTN_A] && ui.cur_t < NTRK) { track_t *tr = &doc.trk[ui.cur_t]; tr->mute ^= 1; doc_touch(&tr->mute, 1); }
        return;
    }
    int *r = &ui.sel[NAV_MIXER];
    const int s = ui.tab[NAV_MIXER];   // the strip: 0..11, 12 = the master
    if (ui.held[BTN_A] && b >= BTN_UP && b <= BTN_RIGHT) {
        ui.used[BTN_A] = true;
        const prm_t p = s < NTRK ? strip_prm(s, *r) : master_prm(*r);
        prm_step(&p, (b == BTN_RIGHT || b == BTN_UP) ? 1 : -1, b == BTN_UP || b == BTN_DOWN);
        return;
    }
    switch (b) {
    case BTN_LEFT: ui.tab[NAV_MIXER] = MAX(s - 1, 0); if (ui.tab[NAV_MIXER] < NTRK) ui.cur_t = ui.tab[NAV_MIXER]; break;
    case BTN_RIGHT: ui.tab[NAV_MIXER] = MIN(s + 1, NTRK); if (ui.tab[NAV_MIXER] < NTRK) ui.cur_t = ui.tab[NAV_MIXER]; break;
    case BTN_UP: *r = MAX(*r - 1, 0); break;
    case BTN_DOWN: *r = MIN(*r + 1, MR_COUNT - 1); break;
    case BTN_X: if (s < NTRK) { track_t *tr = &doc.trk[s]; tr->solo ^= 1; doc_touch(&tr->solo, 1); } break;
    case BTN_Y: if (s < NTRK) go(doc.trk[s].engine == ENG_SYNTH ? NAV_SOUND : NAV_SAMPLE); else go(NAV_MASTER); break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

static void small_knob(int cx, int cy, float v, col_t c, bool bip)
{
    g_arc(cx, cy, 9, 135, 405, C_KTRACK, 3);
    if (bip) { const float a = 270 + v * 135; if (m_fabs(v) > 0.01f) g_arc(cx, cy, 9, MIN(270.0f, a), MAX(270.0f, a), c, 3); }
    else if (v > 0.02f) g_arc(cx, cy, 9, 135, 135 + 270 * v, c, 3);
}

void draw_mixer(void)
{
    g_clear(C_BG);
    const col_t mcol = nav_col(NAV_MIXER);
    w_header("MIXER", mcol);
    const int sel_s = ui.tab[NAV_MIXER], sel_r = ui.sel[NAV_MIXER];
    static const int row_y[MR_COUNT] = { 62, 88, 114, 138, 0 };
    for (int r = 0; r < MR_COUNT - 1; r++) g_text(4, row_y[r] - 6, rows[r], &f12, sel_r == r ? C_TEXT : C_DIMMER);
    g_text(4, 196, "LEVEL", &f12, sel_r == MR_LEVEL ? C_TEXT : C_DIMMER);
    static const struct { int y; const char *s; } scale[] = { { 160, "+12" }, { 196, "0" }, { 256, "-12" }, { 316, "-24" }, { 388, "-60" } };
    for (int k = 0; k < COUNT(scale); k++) g_text_r(46, scale[k].y + 8, scale[k].s, &f12, C_DIMMER);
    bool solo = false;
    for (int t = 0; t < NTRK; t++) if (doc.trk[t].solo) solo = true;
    char b[16];
    for (int t = 0; t < NTRK; t++) {
        const track_t *tr = &doc.trk[t];
        const int x = 50 + t * 40, cx = x + 18;
        const col_t col = trk_col[t];
        const bool off = tr->mute || (solo && !tr->solo);
        if (t == sel_s) g_rect(x, 34, x + 37, 452, c_mix(C_BG, col, 0.12f));
        g_rect(x + 2, 36, x + 35, 38, col);
        char nm[6]; k_strcpy(nm, tr->name, sizeof nm);
        g_text_c(cx, 42, nm, &f12b, off ? C_DIM : col);
        for (int r = 0; r < 3; r++) {
            const prm_t p = strip_prm(t, r);
            small_knob(cx, row_y[r], prm_norm(&p), r == 0 ? col : r == 1 ? C_AMBER : C_SKY, false);
        }
        g_line(cx - 14, 138, cx + 14, 138, C_KTRACK, 2); g_line(cx, 134, cx, 142, C_DIMMER, 1);
        const int px = cx + (int) (tr->pan * 14);
        g_ellipse(px - 3, 135, px + 3, 141, col);
        // the fader: 0 dB about a third of the way down
        const float lv = (tr->level + 60) / 72.0f;
        const int y0 = 154, y1 = 392, yc = y1 - (int) ((float) (y1 - y0) * lv);
        g_rect(cx - 6, y0, cx - 4, y1, C_KTRACK);
        g_rect(cx - 6, yc, cx - 4, y1, c_scale(col, 0.85f));
        g_rrect(cx - 13, yc - 4, cx + 3, yc + 4, 2, C_TEXT);
        g_line(cx - 11, yc, cx + 1, yc, col, 1);
        w_meter_v(cx + 7, y0, y1, off ? 0 : MIN(ui.st.level[t] * 1.4f, 1.0f), col, 5);
        k_snprintf(b, sizeof b, "%+.1f", (double) tr->level);
        g_text_c(cx, 398, b, &f12, C_TEXT);
        w_chip(x + 3, 418, "M", &f12b, tr->mute ? C_INK : C_DIM, tr->mute ? C_AMBER : C_PANEL2, 4, 15);
        w_chip(x + 20, 418, "S", &f12b, tr->solo ? C_INK : C_DIM, tr->solo ? C_GREEN : C_PANEL2, 4, 15);
        k_snprintf(b, sizeof b, "%02d", t + 1);
        g_text_c(cx, 438, b, &f12, C_DIMMER);
        if (t == sel_s) {
            const int ry = sel_r == MR_LEVEL ? 154 : row_y[sel_r] - 12;
            g_frame(x, ry, x + 37, sel_r == MR_LEVEL ? 410 : ry + 24, C_WHITE, 1);
        }
    }
    // the master
    const col_t col = nav_col(NAV_MASTER);
    const int x = 534;
    g_rrect(x, 34, 636, 452, 4, sel_s == NTRK ? c_mix(C_PANEL, col, 0.15f) : C_PANEL);
    g_text_c(585, 40, "MASTER", &f12b, col);
    for (int r = 0; r < MR_COUNT - 1; r++) {
        const prm_t p = master_prm(r);
        static const char *const ml[4] = { "DRIVE", "DLY RTN", "REV RTN", "WIDTH" };
        g_text(544, row_y[r] - 6, ml[r], &f12, sel_s == NTRK && sel_r == r ? C_TEXT : C_DIM);
        prm_text(&p, b, sizeof b);
        g_text_r(628, row_y[r] - 6, b, &f12b, C_TEXT);
    }
    const float vol = (doc.mst.volume + 40) / 46.0f;
    const int y0 = 154, y1 = 392, yc = y1 - (int) ((float) (y1 - y0) * vol);
    g_rect(554, y0, 556, y1, C_KTRACK);
    g_rect(554, yc, 556, y1, col);
    g_rrect(547, yc - 4, 563, yc + 4, 2, C_TEXT);
    w_meter_v(580, y0, y1, MIN(ui.st.out_l, 1.0f), C_GREEN, 10);
    w_meter_v(596, y0, y1, MIN(ui.st.out_r, 1.0f), C_GREEN, 10);
    k_snprintf(b, sizeof b, "%+.1f", (double) doc.mst.volume);
    g_text_c(555, 398, b, &f12, C_TEXT);
    k_snprintf(b, sizeof b, "LIM %.1f", (double) ui.st.limit_gr);
    g_text_c(585, 418, b, &f12, ui.st.limit_gr > 0.5f ? C_AMBER : C_DIM);
    if (sel_s == NTRK) g_frame(x + 2, sel_r == MR_LEVEL ? 154 : row_y[sel_r] - 12, 634, sel_r == MR_LEVEL ? 410 : row_y[sel_r] + 12, C_WHITE, 1);
    static const char *const hints[] = { "\xe2\x86\x90\xe2\x86\x92", "strip", "\xe2\x86\x91\xe2\x86\x93", "row", "A+\xe2\x86\x91\xe2\x86\x93", "change", "A", "mute", "X", "solo", "Y", "sound", "B", "back" };
    w_footer(hints, COUNT(hints));
}
