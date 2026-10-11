// The widgets every screen uses, drawn as docs/mockups/sheet.py draws them.
#include "ui_int.h"

int w_chip(int x, int y, const char *s, const font_t *f, col_t fg, col_t bg, int padx, int h)
{
    const int w = g_tw(s, f) + 2 * padx;
    if (h <= 0) h = f->h + 2;
    g_rrect(x, y, x + w - 1, y + h - 1, 2, bg);
    g_text(x + padx, y + (h - f->h) / 2, s, f, fg);
    return w;
}

int w_chips(int x, int y, const char *const *items, int n, int active, col_t col, int h, int padx, int gap)
{
    for (int i = 0; i < n; i++) {
        const bool on = i == active;
        x += w_chip(x, y, items[i], &f12, on ? C_INK : C_DIM, on ? col : C_PANEL2, padx, h) + gap;
    }
    return x;
}

void w_bar(int x, int y, int w, float v, col_t c, bool bipolar, int h)
{
    g_rect(x, y, x + w - 1, y + h - 1, C_KTRACK);
    if (bipolar) {
        const int m = x + w / 2, e = m + (int) (v * (float) (w / 2));
        g_rect(MIN(m, e), y, MAX(m, e), y + h - 1, c);
        g_line(m, y - 2, m, y + h + 1, C_DIM, 1);
    } else if (v > 0) g_rect(x, y, x + (int) ((float) (w - 1) * MIN(v, 1.0f)), y + h - 1, c);
}

void w_param(int x, int y, int w, const char *label, float v, const char *val, col_t c, bool bipolar, bool sel, int lw)
{
    if (lw <= 0) lw = 72;
    if (sel) { g_rect(x - 6, y - 4, x + w + 4, y + 13, c_mix(C_PANEL, c, 0.18f)); g_rect(x - 6, y - 4, x - 5, y + 13, c); }
    g_text(x, y - 1, label, &f12, sel ? C_TEXT : C_DIM);
    w_bar(x + lw, y + 2, w - lw - 40, v, c, bipolar, 8);
    g_text_r(x + w, y - 1, val, &f12b, C_TEXT);
}

void w_setting(int x, int y, int w, const char *label, const char *val, bool sel, col_t c, col_t vcol)
{
    if (sel) { g_rect(x - 6, y - 3, x + w + 4, y + 14, c_mix(C_PANEL, c, 0.2f)); g_rect(x - 6, y - 3, x - 5, y + 14, c); }
    g_text(x, y, label, &f12, sel ? C_TEXT : C_DIM);
    g_text_r(x + w, y, val, &f12b, vcol);
}

void w_knob(int cx, int cy, int r, float v, col_t c, const char *label, const char *val, bool bipolar, int w)
{
    g_arc(cx, cy, r, 135, 405, C_KTRACK, w);
    float t;
    if (bipolar) {
        const float a = 270 + v * 135;
        if (m_fabs(v) > 0.01f) g_arc(cx, cy, r, MIN(270.0f, a), MAX(270.0f, a), c, w);
        t = (v + 1) / 2;
    } else {
        if (v > 0.005f) g_arc(cx, cy, r, 135, 135 + 270 * MIN(v, 1.0f), c, w);
        t = v;
    }
    const float ang = (135 + 270 * t) * (K_PI / 180.0f);
    const float ri = (float) MAX(2, r - w - 5);
    g_line(cx + m_lround(m_cos(ang) * ri), cy + m_lround(m_sin(ang) * ri), cx + m_lround(m_cos(ang) * (float) (r - 1)), cy + m_lround(m_sin(ang) * (float) (r - 1)), C_TEXT, 2);
    if (val) g_text_c(cx, cy - 6, val, &f12b, C_TEXT);
    if (label) g_text_c(cx, cy + r + 3, label, &f12, C_DIM);
}

void w_stick(int cx, int cy, float dx, float dy, col_t c, const char *lab, const char *val, col_t vcol, int r)
{
    g_eframe(cx - r, cy - r, cx + r, cy + r, C_LINE, 1);
    g_line(cx - r + 4, cy, cx + r - 4, cy, C_DIMMER, 1);
    g_line(cx, cy - r + 4, cx, cy + r - 4, C_DIMMER, 1);
    const int px = cx + (int) (dx * (float) (r - 3)), py = cy + (int) (dy * (float) (r - 3));
    g_ellipse(px - 3, py - 3, px + 3, py + 3, c);
    if (lab) { g_text(cx + r + 6, cy - 11, lab, &f12, C_DIM); g_text(cx + r + 6, cy + 1, val, &f12b, vcol); }
}

void w_box(int x0, int y0, int x1, int y1, const char *title, col_t tcol, col_t fill, const char *right)
{
    g_rrect(x0, y0, x1, y1, 4, fill);
    if (title) g_text(x0 + 8, y0 + 5, title, &f12b, tcol);
    if (right) g_text_r(x1 - 8, y0 + 5, right, &f12, C_DIM);
}

void w_toggle(int x, int y, bool on, col_t c)
{
    g_rrect(x, y, x + 25, y + 12, 6, on ? c : C_KTRACK);
    const int cx = on ? x + 19 : x + 6;
    g_ellipse(cx - 4, y + 2, cx + 4, y + 10, C_TEXT);
}

void w_footer(const char *const *hints, int n)
{
    g_rect(0, 458, SCR_W - 1, SCR_H - 1, C_BG);
    int x = 8;
    for (int i = 0; i + 1 < n; i += 2) {
        x += w_chip(x, 464, hints[i], &f12b, RGB(16, 16, 24), C_KEY, 3, 13) + 4;
        x = g_text(x, 464, hints[i + 1], &f12, C_DIM) + 12;
    }
}

void w_rainbow(int y)
{
    for (int k = 0; k < NTRK; k++) g_rect(k * SCR_W / NTRK, y, (k + 1) * SCR_W / NTRK - 1, y + 1, trk_col[k]);
}

// OUT, MIDI, CLK, the battery and the CPU, top right on every screen.
void w_status(void)
{
    static const char *const outs[OUT_COUNT] = { "48k", "32k", "26k", "22k", "12k", "8k" };
    const int o = CLAMP(doc.mst.out_rate, 0, OUT_COUNT - 1);
    char cpu[8];
    k_snprintf(cpu, sizeof cpu, "%d%%", CLAMP((int) (ui.st.cpu * 100 + 0.5f), 0, 999));
    const struct { int x; const char *lab, *val; col_t c; } it[] = {
        { 452, "OUT", outs[o], o ? C_AMBER : C_TEXT }, { 484, "MIDI", doc.io.notes_out ? "ON" : "OFF", doc.io.notes_out ? C_GREEN : C_DIM },
        { 518, "CLK", doc.io.clock_in == CLK_MIDI ? "IN" : doc.io.clock_out ? "OUT" : "INT", C_GREEN }, { 606, "CPU", cpu, C_TEXT },
    };
    for (int i = 0; i < COUNT(it); i++) { g_text(it[i].x, 1, it[i].lab, &f12, C_DIM); g_text(it[i].x, 14, it[i].val, &f14, it[i].c); }
    const int bx = 554, by = 9;
    if (ui.battery < 0) { g_text(bx, 1, "POWER", &f12, C_DIM); g_text(bx, 14, "AC", &f14, C_GREEN); return; }
    g_frame(bx, by, bx + 30, by + 13, C_DIM, 1);
    g_rect(bx + 31, by + 4, bx + 33, by + 9, C_DIM);
    const col_t bc = ui.battery < 15 ? C_RED : ui.battery < 30 ? C_AMBER : C_GREEN;
    g_rect(bx + 2, by + 2, bx + 2 + (int) (26.0f * (float) CLAMP(ui.battery, 0, 100) / 100.0f), by + 11, bc);
    char b[8];
    k_snprintf(b, sizeof b, "%d", CLAMP(ui.battery, 0, 99));
    g_text(bx + 6, by + 1, b, &f12, RGB(8, 30, 12));
}

void w_header(const char *title, col_t c)
{
    g_rect(0, 0, SCR_W - 1, 29, C_PANEL);
    int x = 6 + w_chip(6, 5, title, &f16, C_INK, c, 5, 20) + 10;
    if (ui.st.playing) g_tri(x, 8, x, 22, x + 11, 15, C_GREEN);
    else g_rect(x + 1, 9, x + 10, 20, C_DIM);
    char b[16];
    k_snprintf(b, sizeof b, "%.1f", (double) ui.st.bpm > 0 ? (double) ui.st.bpm : (double) doc.bpm);
    g_text(x + 17, 5, b, &f20, C_TEXT);
    g_text(x + 17 + g_tw(b, &f20) + 2, 12, "BPM", &f12, C_DIM);
    x += 104;
    k_snprintf(b, sizeof b, "%c%02d", 'A' + ui.pat / NPERBANK, ui.pat % NPERBANK + 1);
    g_text(x, 1, "PAT", &f12, C_DIM); g_text(x, 14, b, &f14, C_TEXT);
    w_status();
    w_rainbow(30);
}

void w_toast(void)
{
    if (!ui.toast[0] || ui.now > ui.toast_until) return;
    const int w = g_tw(ui.toast, &f14) + 16;
    g_rrect(SCR_W - 8 - w, 40, SCR_W - 9, 61, 3, C_GREEN);
    g_text(SCR_W - w, 44, ui.toast, &f14, C_INK);
}

void w_tabs(int x, int y, const char *const *tabs, int n, int active, col_t col)
{
    for (int i = 0; i < n; i++) {
        const bool on = i == active;
        const int w = g_tw(tabs[i], &f14) + 16;
        g_rrect(x, y, x + w, y + 20, 3, on ? col : C_PANEL2);
        g_text(x + 8, y + 3, tabs[i], &f14, on ? C_INK : C_TEXT);
        x += w + 4;
    }
}

void w_upper(char *s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s -= 32; }

void w_meter_v(int x, int y0, int y1, float v, col_t col, int w)
{
    const int n = (y1 - y0) / 4, lit = (int) ((float) n * m_clampf(v, 0, 1));
    for (int k = 0; k < n; k++) {
        const int yb = y1 - k * 4;
        const col_t c = k < n * 72 / 100 ? col : k < n * 9 / 10 ? C_AMBER : C_RED;
        g_rect(x, yb - 2, x + w - 1, yb, k < lit ? c : RGB(28, 28, 40));
    }
}

void w_meter_h(int x0, int y, int x1, float v, col_t col, int h)
{
    const int n = (x1 - x0) / 4, lit = (int) ((float) n * m_clampf(v, 0, 1));
    for (int k = 0; k < n; k++) {
        const int xb = x0 + k * 4;
        const col_t c = k < n * 72 / 100 ? col : k < n * 9 / 10 ? C_AMBER : C_RED;
        g_rect(xb, y, xb + 2, y + h - 1, k < lit ? c : RGB(28, 28, 40));
    }
}
