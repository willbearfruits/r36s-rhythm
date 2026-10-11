// The overlays any screen can open: typing a name with the D-pad (docs/mockups/screens/22) and asking before
// something is thrown away (20).
#include "ui_int.h"

static struct { char title[24], buf[32]; int max, row, col; void (*done)(const char *); } nm;
static const char *const keys[4] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-_.&" };
static const char *const acts[4] = { "SPACE", "DELETE", "CLEAR", "DONE" };

void name_open(const char *title, const char *init, int maxlen, void (*done)(const char *))
{
    k_strcpy(nm.title, title, sizeof nm.title);
    k_strcpy(nm.buf, init, sizeof nm.buf);
    nm.max = MIN(maxlen, (int) sizeof nm.buf - 1);
    nm.row = 0; nm.col = 0;
    nm.done = done;
    ui.overlay = OV_NAME;
    for (int k = 0; k < BTN_COUNT; k++) if (ui.held[k]) ui.used[k] = true;
}

static void type(char c) { const int n = k_strlen(nm.buf); if (n < nm.max) { nm.buf[n] = c; nm.buf[n + 1] = 0; } }
static void del(void) { const int n = k_strlen(nm.buf); if (n) nm.buf[n - 1] = 0; }
static void finish(void) { ui.overlay = OV_NONE; if (nm.done && nm.buf[0]) nm.done(nm.buf); }

void name_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    switch (b) {
    case BTN_UP: nm.row = (nm.row + 4) % 5; if (nm.row == 4) nm.col = MIN(nm.col / 3, 3); break;
    case BTN_DOWN: { const int was = nm.row; nm.row = (nm.row + 1) % 5; if (nm.row == 4) nm.col = MIN(nm.col / 3, 3); else if (was == 4) nm.col = MIN(nm.col * 3, 9); break; }
    case BTN_LEFT: nm.col = (nm.col + (nm.row == 4 ? 3 : 9)) % (nm.row == 4 ? 4 : 10); break;
    case BTN_RIGHT: nm.col = (nm.col + 1) % (nm.row == 4 ? 4 : 10); break;
    case BTN_A:
        if (nm.row < 4) type(keys[nm.row][nm.col]);
        else if (nm.col == 0) type(' ');
        else if (nm.col == 1) del();
        else if (nm.col == 2) nm.buf[0] = 0;
        else finish();
        break;
    case BTN_B: del(); break;
    case BTN_Y: type(' '); break;
    case BTN_START: finish(); break;
    case BTN_SELECT: ui.overlay = OV_NONE; break;
    default: break;
    }
}

void draw_name(void)
{
    g_dim(0.62f);
    const col_t col = nav_col(NAV_PROJECT);
    g_rrect(36, 70, 604, 448, 6, C_PANEL);
    g_rframe(36, 70, 604, 448, 6, col, 2);
    g_text(52, 80, nm.title, &f16, col);
    g_rect(52, 106, 588, 138, RGB(10, 10, 16));
    g_frame(52, 106, 588, 138, col, 1);
    g_text(62, 113, nm.buf, &f20, C_TEXT);
    const int cx = 62 + g_tw(nm.buf, &f20);
    if ((ui.now / 400) & 1) g_rect(cx + 1, 113, cx + 10, 133, col);
    char b[16];
    k_snprintf(b, sizeof b, "%d / %d", k_strlen(nm.buf), nm.max);
    g_text_r(584, 142, b, &f12, C_DIM);
    for (int r = 0; r < 4; r++)
        for (int k = 0; k < 10; k++) {
            const int x = 73 + k * 50, y = 166 + r * 44;
            const bool sel = nm.row == r && nm.col == k;
            const col_t c = trk_col[(r * 3 + k) % NTRK];
            g_rrect(x, y, x + 44, y + 36, 5, sel ? c : c_mix(C_PANEL, c, 0.16f));
            g_rframe(x, y, x + 44, y + 36, 5, sel ? C_WHITE : c_scale(c, 0.6f), sel ? 2 : 1);
            char ch[2] = { keys[r][k], 0 };
            g_text_c(x + 22, y + 8, ch, &f20, sel ? C_INK : C_TEXT);
        }
    static const int w[4] = { 140, 120, 100, 104 };
    int x = 73;
    for (int k = 0; k < 4; k++) {
        const bool sel = nm.row == 4 && nm.col == k, done = k == 3;
        g_rrect(x, 346, x + w[k], 376, 5, done ? (sel ? C_WHITE : C_GREEN) : (sel ? C_WHITE : C_PANEL2));
        g_text_c(x + w[k] / 2, 353, acts[k], &f14, sel || done ? C_INK : C_TEXT);
        x += w[k] + 10;
    }
    g_text(73, 392, "A types, B deletes, Y is a space.", &f12, C_DIM);
    static const char *const hints[] = { "D-PAD", "move", "A", "type", "B", "delete", "START", "done", "SEL", "cancel" };
    w_footer(hints, COUNT(hints));
}

static struct { char title[32], body[160], yes[16]; void (*fn)(void); } cf;

void confirm_open(const char *title, const char *body, const char *yes, void (*fn)(void))
{
    k_strcpy(cf.title, title, sizeof cf.title);
    k_strcpy(cf.body, body, sizeof cf.body);
    k_strcpy(cf.yes, yes, sizeof cf.yes);
    cf.fn = fn;
    ui.overlay = OV_CONFIRM;
    for (int k = 0; k < BTN_COUNT; k++) if (ui.held[k]) ui.used[k] = true;
}

void confirm_button(int b, bool down)
{
    if (!down) return;
    if (b == BTN_A) { ui.overlay = OV_NONE; if (cf.fn) cf.fn(); }
    else if (b == BTN_B || b == BTN_SELECT) ui.overlay = OV_NONE;
}

void draw_confirm(void)
{
    g_dim(0.6f);
    const col_t col = C_AMBER;
    g_rrect(110, 150, 530, 330, 6, C_PANEL);
    g_rframe(110, 150, 530, 330, 6, col, 2);
    g_text(128, 162, cf.title, &f16, col);
    // the body, wrapped
    int y = 194, x = 128;
    char word[40]; int wn = 0;
    for (const char *c = cf.body;; c++) {
        if (*c == ' ' || !*c) {
            word[wn] = 0;
            if (x + g_tw(word, &f14n) > 512) { x = 128; y += 18; }
            x = g_text(x, y, word, &f14n, C_TEXT) + 8;
            wn = 0;
            if (!*c) break;
        } else if (wn < 38) word[wn++] = *c;
    }
    char b[24];
    k_snprintf(b, sizeof b, "A  %s", cf.yes);
    x = 128 + w_chip(128, 288, b, &f14, C_INK, col, 8, 24) + 8;
    w_chip(x, 288, "B  CANCEL", &f14, C_TEXT, C_PANEL2, 8, 24);
    static const char *const hints[] = { "A", "yes", "B", "cancel" };
    w_footer(hints, COUNT(hints));
}
