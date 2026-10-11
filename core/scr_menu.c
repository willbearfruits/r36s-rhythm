// MENU: every screen as a tile (docs/mockups/screens/02). SELECT opens it.
#include "ui_int.h"

static void ic_grid(int cx, int cy, col_t c)
{
    static const uint8_t on[3][5] = { { 1, 0, 0, 1, 0 }, { 0, 0, 1, 0, 0 }, { 1, 1, 1, 1, 1 } };
    for (int r = 0; r < 3; r++) for (int k = 0; k < 5; k++) {
        const int x = cx - 22 + k * 9, y = cy - 13 + r * 9;
        g_rect(x, y, x + 6, y + 6, on[r][k] ? c : c_scale(c, 0.4f));
    }
}
static void ic_tiles(int cx, int cy, col_t c)
{
    for (int r = 0; r < 2; r++) for (int k = 0; k < 4; k++) { const int x = cx - 23 + k * 12, y = cy - 12 + r * 13; g_rect(x, y, x + 9, y + 10, r * 4 + k < 5 ? c : c_scale(c, 0.4f)); }
}
static void ic_song(int cx, int cy, col_t c)
{
    static const int w[] = { 10, 6, 14, 9 };
    int x = cx - 24;
    for (int i = 0; i < 4; i++) { g_rect(x, cy - 6, x + w[i] - 1, cy + 6, c); x += w[i] + 3; }
    g_line(cx - 6, cy - 12, cx - 6, cy + 12, c_scale(c, 0.6f), 2);
}
static void ic_xy(int cx, int cy, col_t c)
{
    g_frame(cx - 15, cy - 14, cx + 15, cy + 14, c, 2);
    g_line(cx - 13, cy + 2, cx + 13, cy + 2, c_scale(c, 0.5f), 1); g_line(cx + 5, cy - 12, cx + 5, cy + 12, c_scale(c, 0.5f), 1);
    g_ellipse(cx + 1, cy - 2, cx + 9, cy + 6, c);
}
static void ic_kit(int cx, int cy, col_t c)
{
    for (int r = 0; r < 3; r++) for (int k = 0; k < 4; k++) { const int x = cx - 18 + k * 12, y = cy - 12 + r * 12; g_ellipse(x - 4, y - 4, x + 4, y + 4, (r + k) % 2 ? c_scale(c, 0.5f) : c); }
}
static void ic_wave(int cx, int cy, col_t c)
{
    int px = cx - 22, py = cy;
    for (int k = 1; k < 45; k++) {
        const int x = cx - 22 + k, y = cy - m_lround(11 * m_sin((float) k / 44.0f * 4.0f * K_PI) * m_exp(-(float) k / 40.0f));
        g_line(px, py, x, y, c, 2); px = x; py = y;
    }
}
static void ic_slices(int cx, int cy, col_t c)
{
    rng_t r; rng_seed(&r, 2);
    for (int k = 0; k < 44; k++) { const int h = (int) (12 * m_exp(-(float) (k % 11) / 5.0f) * (0.5f + 0.5f * rng_uni(&r))); g_line(cx - 22 + k, cy - h, cx - 22 + k, cy + h, c, 1); }
    for (int k = 11; k < 44; k += 11) g_line(cx - 22 + k, cy - 14, cx - 22 + k, cy + 14, c_scale(c, 0.5f), 1);
}
static void ic_rec(int cx, int cy, col_t c) { g_eframe(cx - 13, cy - 13, cx + 13, cy + 13, c, 2); g_ellipse(cx - 7, cy - 7, cx + 7, cy + 7, c); }
static void ic_faders(int cx, int cy, col_t c)
{
    static const float v[] = { 0.7f, 0.4f, 0.85f, 0.55f };
    for (int k = 0; k < 4; k++) { const int x = cx - 18 + k * 12, y = (int) ((float) (cy + 14) - 28 * v[k]); g_line(x, cy - 14, x, cy + 14, c_scale(c, 0.5f), 2); g_rect(x - 4, y - 2, x + 4, y + 2, c); }
}
static void ic_knobs(int cx, int cy, col_t c)
{
    const int xs[2] = { cx - 12, cx + 12 }; const float v[2] = { 0.7f, 0.35f };
    for (int k = 0; k < 2; k++) { g_arc(xs[k], cy, 10, 135, 405, c_scale(c, 0.4f), 3); g_arc(xs[k], cy, 10, 135, 135 + 270 * v[k], c, 3); }
}
static void ic_meter(int cx, int cy, col_t c) { for (int k = 0; k < 6; k++) { const int h = 6 + k * 4, x = cx - 20 + k * 7; g_rect(x, cy + 12 - h, x + 4, cy + 12, c); } }
static void ic_folder(int cx, int cy, col_t c)
{
    const int p[] = { cx - 18, cy - 12, cx - 6, cy - 12, cx - 2, cy - 8, cx + 18, cy - 8, cx + 18, cy + 12, cx - 18, cy + 12 };
    g_poly(p, 6, c);
}
static void ic_disk(int cx, int cy, col_t c)
{
    const int p[] = { cx - 14, cy - 14, cx + 10, cy - 14, cx + 14, cy - 10, cx + 14, cy + 14, cx - 14, cy + 14 };
    g_poly(p, 5, c);
    g_rect(cx - 8, cy - 14, cx + 6, cy - 6, c_scale(c, 0.4f)); g_rect(cx - 9, cy + 2, cx + 9, cy + 14, c_scale(c, 0.6f));
}
static void ic_midi(int cx, int cy, col_t c)
{
    g_eframe(cx - 14, cy - 14, cx + 14, cy + 14, c, 2);
    for (int a = 180; a <= 360; a += 45) { const float r = (float) a * K_PI / 180.0f; const int x = cx + m_lround(m_cos(r) * 8), y = cy + m_lround(m_sin(r) * 8) + 2; g_ellipse(x - 2, y - 2, x + 2, y + 2, c); }
    g_rect(cx - 3, cy + 11, cx + 3, cy + 14, c);
}
static void ic_pulse(int cx, int cy, col_t c)
{
    int x = cx - 24;
    for (int k = 0; k < 4; k++) { g_line(x, cy + 8, x, cy - 8, c, 2); g_line(x, cy - 8, x + 5, cy - 8, c, 2); g_line(x + 5, cy - 8, x + 5, cy + 8, c, 2); g_line(x + 5, cy + 8, x + 12, cy + 8, c, 2); x += 12; }
}
static void ic_gear(int cx, int cy, col_t c)
{
    for (int k = 0; k < 8; k++) { const float a = (float) k * K_PI / 4.0f; const int x = cx + m_lround(m_cos(a) * 12), y = cy + m_lround(m_sin(a) * 12); g_ellipse(x - 4, y - 4, x + 4, y + 4, c); }
    g_ellipse(cx - 11, cy - 11, cx + 11, cy + 11, c);
    g_ellipse(cx - 4, cy - 4, cx + 4, cy + 4, C_PANEL);
}
static void (*const icons[NAV_COUNT])(int, int, col_t) = { ic_grid, ic_tiles, ic_song, ic_xy, ic_kit, ic_wave, ic_slices, ic_rec,
                                                            ic_faders, ic_knobs, ic_meter, ic_folder, ic_disk, ic_midi, ic_pulse, ic_gear };

void draw_menu(void)
{
    g_dim(0.72f);
    g_rrect(16, 36, 623, 454, 6, RGB(16, 16, 26));
    g_rframe(16, 36, 623, 454, 6, C_LINE, 1);
    g_text(28, 42, "MENU", &f14, C_TEXT);
    g_text_r(612, 44, "SELECT + D-PAD:  \u2190 PATTERN  \u2191 MIXER  \u2192 SOUND  \u2193 PERFORM", &f12, C_DIM);
    for (int i = 0; i < NAV_COUNT; i++) {
        const int r = i / 4, c = i % 4, x = 27 + c * 148, y = 62 + r * 98;
        const col_t col = nav_col(i);
        const bool sel = i == ui.menu_sel, built = true;
        g_rrect(x, y, x + 141, y + 91, 5, sel ? col : c_mix(C_BG, col, built ? 0.16f : 0.08f));
        g_rframe(x, y, x + 141, y + 91, 5, sel ? C_WHITE : c_scale(col, built ? 0.7f : 0.35f), sel ? 2 : 1);
        icons[i](x + 70, y + 38, sel ? C_INK : built ? col : c_scale(col, 0.45f));
        g_text_c(x + 70, y + 68, nav_name[i], &f14, sel ? C_INK : built ? C_TEXT : C_DIM);
    }
    static const char *const hints[] = { "D-PAD", "move", "A", "open", "B", "close", "SEL", "close" };
    w_footer(hints, COUNT(hints));
}

void menu_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    switch (b) {
    case BTN_LEFT: ui.menu_sel = (ui.menu_sel + NAV_COUNT - 1) % NAV_COUNT; break;
    case BTN_RIGHT: ui.menu_sel = (ui.menu_sel + 1) % NAV_COUNT; break;
    case BTN_UP: ui.menu_sel = (ui.menu_sel + NAV_COUNT - 4) % NAV_COUNT; break;
    case BTN_DOWN: ui.menu_sel = (ui.menu_sel + 4) % NAV_COUNT; break;
    case BTN_A: go(ui.menu_sel); break;
    case BTN_B: case BTN_SELECT: ui.overlay = OV_NONE; break;
    default: break;
    }
}
