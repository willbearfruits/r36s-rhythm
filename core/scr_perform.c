// PERFORM (docs/mockups/screens/08): the right stick plays two of the eight effects, one on each axis, from any
// screen. R3 latches where it is. Here: choose the effects, mute tracks, and record the stick into the pattern
// (hold R1 while it plays; what the stick says is written step by step, centred erases).
#include "perf.h"
#include "ui_int.h"

static float sx, sy;                 // the stick, or where it was latched: -1..1, up is +
static float sent[PF_COUNT];
static struct { int16_t x, y; } trail[48];
static int trail_n;
static int rec_step = -1;

static const col_t fx_col[PF_COUNT] = { RGB(255, 190, 60), RGB(255, 120, 200), RGB(255, 140, 70), RGB(150, 230, 90),
                                        RGB(90, 190, 255), RGB(190, 130, 255), RGB(80, 220, 200), RGB(170, 210, 255) };

static int perf_pat(void) { return ui.st.playing ? ui.st.pat : ui.pat; }
static bool bipolar(int fx) { return fx == PF_FILTER; }
static float amount(int fx, float v) { return bipolar(fx) ? v : m_fabs(v); }

void perform_latch(void)
{
    ui.perf_latch = !ui.perf_latch;
    if (!ui.perf_latch && ui.rx == 0 && ui.ry == 0) sx = sy = 0;
    ui_toast(ui.perf_latch ? "LATCHED" : "LATCH OFF");
}

static void motion_del(pattern_t *p, int s, int param)
{
    const int i = lock_find(p, NTRK, s, param);
    if (i < 0) return;
    p->lock[i] = p->lock[--p->nlock];
    doc_touch(&p->nlock, sizeof p->nlock);
    doc_touch(&p->lock[i], sizeof p->lock[i]);
}

static bool has_motion(const pattern_t *p, int fx)
{
    for (int i = 0; i < p->nlock; i++) if (p->lock[i].trk == NTRK && p->lock[i].param == LK_PERF + fx) return true;
    return false;
}

void perform_sticks(void)
{
    const bool moving = ui.rx != 0 || ui.ry != 0;
    if (moving) { sx = ui.rx; sy = -ui.ry; }
    else if (!ui.perf_latch) sx = sy = 0;
    const int fx = CLAMP(ui.perf_fx_x, 0, PF_COUNT - 1), fy = CLAMP(ui.perf_fx_y, 0, PF_COUNT - 1);
    float want[PF_COUNT] = { 0 };
    // what the pattern recorded, then the stick over it
    pattern_t *p = &doc.pat[CLAMP(perf_pat(), 0, NPAT - 1)];
    const int step = ui.st.playing ? CLAMP((int) (ui.st.beat * 4.0f), 0, NSTEP - 1) : -1;
    if (step >= 0)
        for (int i = 0; i < p->nlock; i++) {
            const lock_t *l = &p->lock[i];
            if (l->trk == NTRK && l->step == step && l->param >= LK_PERF && l->param < LK_PERF + PF_COUNT) want[l->param - LK_PERF] = l->val;
        }
    const bool rec = ui.screen == NAV_PERFORM && ui.held[BTN_R1] && ui.overlay == OV_NONE && step >= 0;
    if (sx != 0 || rec) want[fx] = amount(fx, sx);
    if (sy != 0 || rec) want[fy] = fy == fx ? MAX(want[fx], amount(fy, sy)) : amount(fy, sy);
    if (rec && step != rec_step) {   // once a step
        rec_step = step;
        for (int k = 0; k < 2; k++) {
            const int f = k ? fy : fx;
            if (k && fy == fx) break;
            if (m_fabs(want[f]) > 0.02f) lock_set(p, NTRK, step, LK_PERF + f, want[f]);
            else motion_del(p, step, LK_PERF + f);
        }
    }
    if (!rec) rec_step = -1;
    for (int f = 0; f < PF_COUNT; f++)
        if (m_fabs(want[f] - sent[f]) > 0.004f || (want[f] == 0 && sent[f] != 0)) { sent[f] = want[f]; ui_msg(M_PERF, f, 0, 0, want[f]); }
}

// ------------------------------------------------------------------------------------------------- the screen

enum { TILES = PF_COUNT, MUTES = NTRK };

static void set_axis(int which, int fx)
{
    if (which) { ui.perf_fx_y = fx; ui.prefs.r_y = (uint8_t) fx; }
    else { ui.perf_fx_x = fx; ui.prefs.r_x = (uint8_t) fx; }
    for (int f = 0; f < PF_COUNT; f++) if (sent[f] != 0) { sent[f] = 0; ui_msg(M_PERF, f, 0, 0, 0); }
    prefs_save();
}

static void motion_clear(void)
{
    pattern_t *p = &doc.pat[CLAMP(perf_pat(), 0, NPAT - 1)];
    int k = 0;
    for (int i = 0; i < p->nlock; i++) if (p->lock[i].trk != NTRK) p->lock[k++] = p->lock[i];
    if (k == p->nlock) return;
    p->nlock = (uint16_t) k;
    doc_touch(&p->nlock, sizeof p->nlock);
    doc_touch(p->lock, sizeof p->lock);
    ui_toast("MOTION CLEARED");
}

void perform_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    int *sel = &ui.sel[NAV_PERFORM];
    const bool tile = *sel < TILES;
    const int col = tile ? *sel % 4 : (*sel - TILES) % 6, row = tile ? *sel / 4 : 2 + (*sel - TILES) / 6;
    switch (b) {
    case BTN_LEFT: if (col > 0) (*sel)--; break;
    case BTN_RIGHT: if (col < (tile ? 3 : 5)) (*sel)++; break;
    case BTN_UP:
        if (row == 1) *sel -= 4;
        else if (row == 3) *sel -= 6;
        else if (row == 2) *sel = 4 + MIN(col * 3 / 5 + (col == 5), 3);
        break;
    case BTN_DOWN:
        if (row == 0) *sel += 4;
        else if (row == 2) *sel += 6;
        else if (row == 1) *sel = TILES + MIN((col * 5 + 1) / 3, 5);
        break;
    case BTN_A:
    case BTN_X:
        if (b == BTN_X && ui.held[BTN_R1]) { ui.used[BTN_R1] = true; motion_clear(); break; }
        if (tile && b == BTN_A) set_axis(0, *sel);
        else if (!tile) { track_t *tr = &doc.trk[*sel - TILES]; tr->mute ^= 1; doc_touch(&tr->mute, 1); }
        break;
    case BTN_Y: if (tile) set_axis(1, *sel); break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

static const char *const rep_div[] = { "1/4", "1/8", "1/16", "1/32" };

static void val_text(int fx, float v, char *b, int cap)
{
    if (fx == PF_REPEAT && v > 0.15f) k_snprintf(b, cap, "%s", rep_div[v < 0.4f ? 0 : v < 0.6f ? 1 : v < 0.8f ? 2 : 3]);
    else if (fx == PF_GATE && v > 0.01f) k_snprintf(b, cap, "%s", v < 0.33f ? "1/8" : v < 0.66f ? "1/16" : "1/32");
    else if (fx == PF_FILTER) k_snprintf(b, cap, "%s %d%%", v < 0 ? "LOW" : v > 0 ? "HIGH" : "", (int) (m_fabs(v) * 100 + 0.5f));
    else if (fx == PF_FREEZE && v > 0.5f) k_snprintf(b, cap, "HOLD");
    else k_snprintf(b, cap, "%d%%", (int) (m_fabs(v) * 100 + 0.5f));
}

void draw_perform(void)
{
    g_clear(C_BG);
    const col_t pcol = nav_col(NAV_PERFORM);
    w_header("PERFORM", pcol);
    const int fx = CLAMP(ui.perf_fx_x, 0, PF_COUNT - 1), fy = CLAMP(ui.perf_fx_y, 0, PF_COUNT - 1);
    const pattern_t *p = &doc.pat[CLAMP(perf_pat(), 0, NPAT - 1)];
    char b[32];

    // the pad: where the stick is, and where it has been
    const int X0 = 8, Y0 = 36, X1 = 300, Y1 = 328, cx = (X0 + X1) / 2, cy = (Y0 + Y1) / 2;
    g_rrect(X0, Y0, X1, Y1, 4, C_PANEL);
    for (int k = 1; k < 8; k++) {
        const int gx = X0 + (X1 - X0) * k / 8, gy = Y0 + (Y1 - Y0) * k / 8;
        g_line(gx, Y0 + 2, gx, Y1 - 2, k == 4 ? C_LINE : C_PANEL2, 1);
        g_line(X0 + 2, gy, X1 - 2, gy, k == 4 ? C_LINE : C_PANEL2, 1);
    }
    const int dx = cx + (int) (sx * (float) (X1 - X0 - 24) / 2), dy = cy - (int) (sy * (float) (Y1 - Y0 - 24) / 2);
    if (sx != 0 || sy != 0) {
        trail[trail_n % COUNT(trail)].x = (int16_t) dx; trail[trail_n % COUNT(trail)].y = (int16_t) dy; trail_n++;
    } else trail_n = 0;
    const int nt = MIN(trail_n, COUNT(trail));
    for (int k = 1; k < nt; k++) {
        const int i0 = (trail_n - nt + k - 1) % COUNT(trail), i1 = (trail_n - nt + k) % COUNT(trail);
        g_line(trail[i0].x, trail[i0].y, trail[i1].x, trail[i1].y, c_mix(C_PANEL, fx_col[fy], (float) k / (float) nt), 2);
    }
    k_snprintf(b, sizeof b, "\xe2\x86\x91 %s", perf_name[fy]);
    g_text(X0 + 8, Y0 + 6, b, &f12b, fx_col[fy]);
    k_snprintf(b, sizeof b, "%s \xe2\x86\x92", perf_name[fx]);
    g_text_r(X1 - 8, Y1 - 34, b, &f12b, fx_col[fx]);
    if (fx == PF_FILTER) { g_text(X0 + 8, Y1 - 18, "LOW", &f12, C_DIMMER); g_text_r(X1 - 8, Y1 - 18, "HIGH", &f12, C_DIMMER); }
    if (ui.perf_latch) w_chip(X1 - 70, Y0 + 6, "LATCHED", &f12b, C_INK, C_AMBER, 5, 16);
    if (sx != 0 || sy != 0) {
        g_line(X0 + 2, dy, X1 - 2, dy, c_scale(fx_col[fy], 0.6f), 1);
        g_line(dx, Y0 + 2, dx, Y1 - 2, c_scale(fx_col[fx], 0.6f), 1);
        g_ellipse(dx - 9, dy - 9, dx + 9, dy + 9, C_AMBER);
        g_ellipse(dx - 5, dy - 5, dx + 5, dy + 5, C_WHITE);
    } else {
        g_eframe(cx - 9, cy - 9, cx + 9, cy + 9, C_DIM, 2);
        g_text_c(cx, cy + 18, "MOVE THE RIGHT STICK", &f12, C_DIMMER);
    }

    // which effects are on the axes
    const int sel = ui.sel[NAV_PERFORM];
    w_box(308, 36, 632, 150, "ON THE STICK", C_DIM, C_PANEL, NULL);
    for (int f = 0; f < PF_COUNT; f++) {
        const int x = 316 + (f % 4) * 79, y = 56 + (f / 4) * 46;
        const bool on_x = f == fx, on_y = f == fy;
        const col_t bg = on_x ? C_SKY : on_y ? C_AMBER : C_PANEL2;
        g_rrect(x, y, x + 73, y + 40, 4, bg);
        g_text_c(x + 37, y + 6, perf_name[f], &f12b, on_x || on_y ? C_INK : C_TEXT);
        if (on_x || on_y) g_text_c(x + 37, y + 21, on_x && on_y ? "X + Y" : on_x ? "X AXIS" : "Y AXIS", &f12, C_INK);
        const float a = MIN(m_fabs(sent[f]), 1.0f);
        if (a > 0.01f) g_rect(x + 3, y + 35, x + 3 + (int) (67 * a), y + 37, on_x || on_y ? C_INK : fx_col[f]);
        if (has_motion(p, f)) g_ellipse(x + 64, y + 4, x + 69, y + 9, on_x || on_y ? C_INK : fx_col[f]);
        if (sel == f) g_rframe(x - 2, y - 2, x + 75, y + 42, 5, C_WHITE, 2);
    }

    // mutes
    w_box(308, 156, 632, 240, "MUTES", C_DIM, C_PANEL, NULL);
    for (int t = 0; t < NTRK; t++) {
        const int x = 316 + (t % 6) * 52, y = 176 + (t / 6) * 30;
        const track_t *tr = &doc.trk[t];
        const bool lit = ui.now < ui.flash[t];
        g_rrect(x, y, x + 48, y + 24, 3, tr->mute ? C_PANEL2 : lit ? c_mix(trk_col[t], C_WHITE, 0.4f) : trk_col[t]);
        char nm[6]; k_strcpy(nm, tr->name, sizeof nm);
        g_text_c(x + 24, y + 5, nm, &f12b, tr->mute ? C_DIMMER : C_INK);
        if (sel == TILES + t) g_rframe(x - 2, y - 2, x + 50, y + 26, 4, C_WHITE, 2);
    }

    // now
    w_box(308, 246, 632, 328, "NOW", C_DIM, C_PANEL, NULL);
    const bool rec = ui.held[BTN_R1] && ui.st.playing;
    for (int k = 0; k < 2; k++) {
        const int f = k ? fy : fx, y = 266 + k * 26;
        g_text(318, y, k ? "Y" : "X", &f14, k ? C_AMBER : C_SKY);
        g_text(338, y + 1, perf_name[f], &f12b, C_TEXT);
        val_text(f, sent[f], b, sizeof b);
        g_text_r(476, y + 1, b, &f12b, m_fabs(sent[f]) > 0.01f ? fx_col[f] : C_DIMMER);
        w_bar(486, y + 5, 136, bipolar(f) ? sent[f] : m_fabs(sent[f]), fx_col[f], bipolar(f), 5);
    }
    if (rec) w_chip(318, 312 - 2, "\xe2\x97\x8f REC", &f12b, C_INK, C_RED, 5, 15);
    else g_text(318, 311, ui.st.playing ? "hold R1 to record the stick" : "press START, then hold R1 to record", &f12, C_DIMMER);

    // the motion in the pattern
    const int len = CLAMP(p->len, 1, NSTEP);
    k_snprintf(b, sizeof b, "MOTION IN %c%02d", 'A' + perf_pat() / NPERBANK, perf_pat() % NPERBANK + 1);
    w_box(8, 336, 632, 452, b, pcol, C_PANEL, "R1+X clear");
    const int play = ui.st.playing ? CLAMP((int) (ui.st.beat * 4.0f), 0, len - 1) : -1;
    const float bw = 600.0f / (float) len;
    for (int k = 0; k < 2; k++) {
        const int f = k ? fy : fx, y0 = 358 + k * 46, y1 = y0 + 40;
        g_text(16, y0 + 14, k ? "Y" : "X", &f12b, k ? C_AMBER : C_SKY);
        for (int s = 0; s < len; s++) {
            const int x0 = 28 + (int) (bw * (float) s), x1 = 28 + (int) (bw * (float) (s + 1)) - 2;
            const int li = lock_find(p, NTRK, s, LK_PERF + f);
            g_rect(x0, y1, x1, y1, s % 4 == 0 ? C_LINE : C_PANEL2);
            if (li >= 0) {
                const float v = MIN(m_fabs(p->lock[li].val), 1.0f);
                g_rect(x0, y1 - (int) (38 * v), x1, y1 - 1, s == play ? C_WHITE : fx_col[f]);
            }
            if (s == play) g_rect(x0, y0, x1, y0 + 1, C_WHITE);
        }
    }
    static const char *const hints[] = { "R STICK", "play", "R3", "latch", "R1", "record", "A", "on X", "Y", "on Y", "X", "mute", "B", "back" };
    w_footer(hints, COUNT(hints));
}
