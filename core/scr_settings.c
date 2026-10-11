// SETTINGS (docs/mockups/screens/26): what the device remembers between projects (SETTINGS.CFG). From it:
// DIAGNOSTICS (28), BUTTON MAP (27) and FILES (23: on this version, how files get onto the card).
#include "plat.h"
#include "ui_int.h"

enum { CAT_AUDIO, CAT_CONTROLS, CAT_PADS, CAT_STORAGE, CAT_BUTTONS, CAT_DIAG, CAT_FILES, CAT_ABOUT, CAT_COUNT };
static const char *const cat_name[CAT_COUNT] = { "AUDIO", "CONTROLS", "PADS", "STORAGE", "BUTTON MAP", "DIAGNOSTICS", "FILES", "ABOUT" };
static bool in_page;

static const uint8_t q_val[4] = { 0, 1, 2, 4 };
static const char *const q_name[4] = { "AUTO", "LOW  1x", "NORMAL  2x", "HIGH  4x" };
static const char *const buf_name[4] = { "AUTO", "256", "512", "1024" };
static const char *const curve_name[3] = { "STRAIGHT", "SOFT", "STEEP" };
static const char *const macro_name[5] = { "TUNE", "PUNCH", "LENGTH", "TONE", "GRIT" };   // P_TUNE .. P_GRIT

static int rows(int cat)
{
    switch (cat) {
    case CAT_AUDIO: return 2;
    case CAT_CONTROLS: return 7;
    case CAT_PADS: return 1;
    case CAT_STORAGE: return 1;
    default: return 0;
    }
}

static void row_text(int cat, int r, const char **label, char *v, int cap)
{
    const prefs_t *p = &ui.prefs;
    int qi = 0;
    while (qi < 3 && q_val[qi] != p->quality) qi++;
    switch (cat * 16 + r) {
    case CAT_AUDIO * 16 + 0: *label = "DRUM QUALITY"; k_snprintf(v, cap, "%s", q_name[qi]); return;
    case CAT_AUDIO * 16 + 1: *label = "AUDIO BUFFER"; k_snprintf(v, cap, "%s%s", buf_name[p->buffer & 3], p->buffer ? " FRAMES" : ""); return;
    case CAT_CONTROLS * 16 + 0: *label = "STICK DEAD ZONE"; k_snprintf(v, cap, "%d%%", (int) (p->dead * 100 + 0.5f)); return;
    case CAT_CONTROLS * 16 + 1: *label = "STICK CURVE"; k_snprintf(v, cap, "%s", curve_name[p->curve % 3]); return;
    case CAT_CONTROLS * 16 + 2: *label = "LEFT STICK \xe2\x86\x90\xe2\x86\x92"; k_snprintf(v, cap, "%s", macro_name[CLAMP(p->l_x - P_TUNE, 0, 4)]); return;
    case CAT_CONTROLS * 16 + 3: *label = "LEFT STICK \xe2\x86\x91\xe2\x86\x93"; k_snprintf(v, cap, "%s", macro_name[CLAMP(p->l_y - P_TUNE, 0, 4)]); return;
    case CAT_CONTROLS * 16 + 4: *label = "STICK LET GO"; k_snprintf(v, cap, "%s", p->stick_return ? "STAYS" : "SPRINGS BACK"); return;
    case CAT_CONTROLS * 16 + 5: *label = "BUTTON REPEAT"; k_snprintf(v, cap, "%d ms", CLAMP(p->repeat_ms, 30, 250)); return;
    case CAT_CONTROLS * 16 + 6: *label = "SWAP A AND B"; k_snprintf(v, cap, "%s", p->swap_ab ? "ON" : "OFF"); return;
    case CAT_PADS * 16 + 0: *label = "R2 IN THE PADS"; k_snprintf(v, cap, "%s", p->pad_accent ? "ACCENT" : "GHOST"); return;
    case CAT_STORAGE * 16 + 0: *label = "SAVE THE PROJECT NOW"; k_snprintf(v, cap, "A"); return;
    default: *label = ""; v[0] = 0; return;
    }
}

static void row_change(int cat, int r, int d)
{
    prefs_t *p = &ui.prefs;
    switch (cat * 16 + r) {
    case CAT_AUDIO * 16 + 0: {
        int qi = 0;
        while (qi < 3 && q_val[qi] != p->quality) qi++;
        p->quality = q_val[CLAMP(qi + d, 0, 3)];
        ui_msg(M_QUALITY, p->quality, 0, 0, 0);
        break;
    }
    case CAT_AUDIO * 16 + 1: p->buffer = (uint8_t) CLAMP(p->buffer + d, 0, 3); ui_toast("FROM THE NEXT START"); break;
    case CAT_CONTROLS * 16 + 0: p->dead = m_clampf(p->dead + 0.01f * (float) d, 0, 0.4f); break;
    case CAT_CONTROLS * 16 + 1: p->curve = (uint8_t) CLAMP(p->curve + d, 0, 2); break;
    case CAT_CONTROLS * 16 + 2: p->l_x = (uint8_t) CLAMP(p->l_x + d, P_TUNE, P_GRIT); break;
    case CAT_CONTROLS * 16 + 3: p->l_y = (uint8_t) CLAMP(p->l_y + d, P_TUNE, P_GRIT); break;
    case CAT_CONTROLS * 16 + 4: p->stick_return = !p->stick_return; break;
    case CAT_CONTROLS * 16 + 5: p->repeat_ms = (uint8_t) CLAMP(p->repeat_ms + 10 * d, 30, 250); break;
    case CAT_CONTROLS * 16 + 6: p->swap_ab = !p->swap_ab; break;
    case CAT_PADS * 16 + 0: p->pad_accent = !p->pad_accent; break;
    default: return;
    }
    prefs_save();
}

void settings_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    int *cat = &ui.tab[NAV_SETTINGS], *r = &ui.sel[NAV_SETTINGS];
    if (b == BTN_R1 || b == BTN_R2) { *cat = (*cat + (b == BTN_R2 ? 1 : CAT_COUNT - 1)) % CAT_COUNT; *r = 0; in_page = rows(*cat) > 0; return; }
    if (!in_page) {
        switch (b) {
        case BTN_UP: *cat = MAX(*cat - 1, 0); break;
        case BTN_DOWN: *cat = MIN(*cat + 1, CAT_COUNT - 1); break;
        case BTN_A: case BTN_RIGHT:
            if (*cat == CAT_BUTTONS) go(SCR_BUTTONS);
            else if (*cat == CAT_DIAG) go(SCR_DIAG);
            else if (*cat == CAT_FILES) go(SCR_USB);
            else if (rows(*cat)) { in_page = true; *r = 0; }
            break;
        case BTN_B: go(NAV_PATTERN); break;
        default: break;
        }
        return;
    }
    const int n = rows(*cat);
    switch (b) {
    case BTN_UP: *r = MAX(*r - 1, 0); break;
    case BTN_DOWN: *r = MIN(*r + 1, n - 1); break;
    case BTN_LEFT: case BTN_RIGHT:
        if (ui.held[BTN_A]) ui.used[BTN_A] = true;
        row_change(*cat, *r, b == BTN_RIGHT ? 1 : -1);
        break;
    case BTN_A:
        if (*cat == CAT_STORAGE && *r == 0) ui_toast(proj_save(doc.name) ? "SAVED" : "COULD NOT SAVE");
        break;
    case BTN_B: in_page = false; break;
    default: break;
    }
}

static int count_dir(const char *path, bool want_dirs)
{
    char name[64]; bool dir;
    int n = 0;
    for (int i = 0; i < 2000 && plat_dir(path, i, name, sizeof name, &dir); i++) if (dir == want_dirs) n++;
    return n;
}

static void cpu_bars(int x, int y, int w)
{
    const status_t *st = &ui.st;
    char b[32];
    g_text(x, y, "WHERE THE TIME GOES", &f12b, C_DIM);
    k_snprintf(b, sizeof b, "%d%% OF ONE CORE", (int) (st->cpu * 100 + 0.5f));
    g_text_r(x + w, y, b, &f12, st->cpu > 0.8f ? C_RED : C_TEXT);
    y += 20;
    float trk = 0;
    for (int t = 0; t < NTRK; t++) trk += st->cpu_trk[t];
    const struct { const char *n; float v; col_t c; } parts[] = {
        { "TRACKS", trk, C_AMBER }, { "SEQUENCER", st->cpu_seq, C_SKY }, { "MIX", st->cpu_mix, C_GREEN },
        { "DELAY", st->cpu_dly, RGB(255, 140, 70) }, { "REVERB", st->cpu_rev, RGB(170, 210, 255) }, { "MASTER", st->cpu_mst, RGB(255, 120, 200) },
    };
    for (int k = 0; k < COUNT(parts); k++) {
        g_text(x, y + k * 18, parts[k].n, &f12, C_DIM);
        w_bar(x + 90, y + k * 18 + 4, w - 160, parts[k].v, parts[k].c, false, 6);
        k_snprintf(b, sizeof b, "%.1f%%", (double) (parts[k].v * 100));
        g_text_r(x + w, y + k * 18, b, &f12, C_TEXT);
    }
}

void draw_settings(void)
{
    g_clear(C_BG);
    const col_t sc = nav_col(NAV_SETTINGS);
    w_header("SETTINGS", sc);
    const int cat = ui.tab[NAV_SETTINGS], sel = ui.sel[NAV_SETTINGS];
    char b[64];
    g_rrect(4, 34, 172, 452, 4, C_PANEL);
    for (int c = 0; c < CAT_COUNT; c++) {
        const int y = 44 + c * 30;
        if (c == cat) g_rrect(8, y - 4, 168, y + 20, 3, in_page ? c_mix(C_PANEL, sc, 0.35f) : sc);
        g_text(18, y, cat_name[c], &f14, c == cat ? (in_page ? C_TEXT : C_INK) : C_TEXT);
    }
    g_rrect(176, 34, 636, 452, 4, C_PANEL);
    g_text(186, 42, cat_name[cat], &f12b, sc);
    const int n = rows(cat);
    for (int r = 0; r < n; r++) {
        const char *label; char v[40];
        row_text(cat, r, &label, v, sizeof v);
        const int y = 64 + r * 24;
        const bool s = in_page && r == sel;
        if (s) { g_rect(180, y - 4, 632, y + 17, c_mix(C_PANEL, sc, 0.25f)); g_rect(180, y - 4, 182, y + 17, sc); }
        g_text(190, y, label, &f12, s ? C_TEXT : C_DIM);
        g_text_r(624, y, v, &f12b, C_TEXT);
    }
    switch (cat) {
    case CAT_AUDIO:
        g_text(190, 124, "Drum quality is how finely the drums are", &f12, C_DIMMER);
        g_text(190, 140, "computed. LOW leaves the most time free: try it", &f12, C_DIMMER);
        g_text(190, 156, "when many tracks stutter. A bigger buffer", &f12, C_DIMMER);
        g_text(190, 172, "stutters less and answers a little later.", &f12, C_DIMMER);
        cpu_bars(190, 204, 420);
        if (ui_host_text[0]) g_text(190, 340, ui_host_text, &f12, C_DIM);
        break;
    case CAT_CONTROLS: {
        g_text(190, 238, "STICKS NOW", &f12, C_DIM);
        char v[24];
        k_snprintf(v, sizeof v, "%+.2f %+.2f", (double) ui.lx, (double) ui.ly);
        w_stick(250, 340, ui.lx, ui.ly, sc, "LEFT", v, C_TEXT, 46);
        k_snprintf(v, sizeof v, "%+.2f %+.2f", (double) ui.rx, (double) ui.ry);
        w_stick(450, 340, ui.rx, ui.ry, sc, "RIGHT", v, C_TEXT, 46);
        break;
    }
    case CAT_PADS:
        g_text(190, 100, "Hold L1: tracks 1-8 on the D-pad and A B X Y.", &f12, C_DIM);
        g_text(190, 116, "Hold L2: tracks 9-12, and FILL.", &f12, C_DIM);
        g_text(190, 132, "In the pads, SELECT turns recording on and off.", &f12, C_DIM);
        break;
    case CAT_STORAGE: {
        k_snprintf(b, sizeof b, "PROJECT  %s%s", doc.name, ui.modified ? "  (CHANGED)" : "");
        g_text(190, 100, b, &f12, C_TEXT);
        k_snprintf(b, sizeof b, "PROJECTS  %d", count_dir("PROJECTS", false));
        g_text(190, 124, b, &f12, C_DIM);
        k_snprintf(b, sizeof b, "KITS  %d", count_dir("KITS", false));
        g_text(190, 140, b, &f12, C_DIM);
        k_snprintf(b, sizeof b, "SAMPLES  %d FILES, %d FOLDERS AT THE TOP", count_dir("SAMPLES", false), count_dir("SAMPLES", true));
        g_text(190, 156, b, &f12, C_DIM);
        k_snprintf(b, sizeof b, "RENDERS  %d FOLDERS", count_dir("RENDERS", true));
        g_text(190, 172, b, &f12, C_DIM);
        uint32_t used, total;
        ui_mem(&used, &total);
        k_snprintf(b, sizeof b, "SAMPLE MEMORY  %d OF %d MB", (int) (used >> 20), (int) (total >> 20));
        g_text(190, 204, b, &f12, C_TEXT);
        w_bar(190, 222, 430, total ? (float) used / (float) total : 0, sc, false, 6);
        g_text(190, 252, "Projects save by themselves a few seconds after", &f12, C_DIMMER);
        g_text(190, 268, "a change.", &f12, C_DIMMER);
        break;
    }
    case CAT_BUTTONS: g_text(190, 70, "A drawing of what every button does.", &f12, C_DIM); g_text(190, 92, "A opens it.", &f12, C_TEXT); break;
    case CAT_DIAG: g_text(190, 70, "Buttons, sticks, the audio and where the time goes.", &f12, C_DIM); g_text(190, 92, "A opens it.", &f12, C_TEXT); break;
    case CAT_FILES: g_text(190, 70, "How samples get onto the card.", &f12, C_DIM); g_text(190, 92, "A opens it.", &f12, C_TEXT); break;
    default:
        g_text(190, 70, "R36S RHYTHM MACHINE", &f20, C_TEXT);
        g_text(190, 100, "12 tracks: synthesized drums and samples.", &f12, C_DIM);
        g_text(190, 116, "The drums are thump's engine, ported to plain C.", &f12, C_DIM);
        g_text(190, 132, "48 kHz inside; the master can bring it down.", &f12, C_DIM);
        k_snprintf(b, sizeof b, "BUILT %s", __DATE__);
        g_text(190, 164, b, &f12, C_DIMMER);
        break;
    }
    static const char *const hc[] = { "\xe2\x86\x91\xe2\x86\x93", "page", "A", "open", "R1/R2", "page", "B", "back" };
    static const char *const hp[] = { "\xe2\x86\x91\xe2\x86\x93", "setting", "\xe2\x86\x90\xe2\x86\x92", "change", "R1/R2", "page", "B", "pages" };
    if (in_page) w_footer(hp, COUNT(hp)); else w_footer(hc, COUNT(hc));
}

// ------------------------------------------------------------------------------------------------- DIAGNOSTICS

static uint32_t pressed_at[BTN_COUNT];

void diag_button(int b, bool down, bool rep)
{
    (void) rep;
    if (down) pressed_at[b] = ui.now;
    // leaving needs three at once: every button is under test here
    if (down && ui.held[BTN_L1] && ui.held[BTN_R1] && ui.held[BTN_START]) go(NAV_SETTINGS);
    if (down && b == BTN_B && ui.held[BTN_SELECT]) go(NAV_SETTINGS);
}

void draw_diag(void)
{
    g_clear(C_BG);
    w_header("DIAGNOSTICS", RGB(255, 90, 160));
    char b[96];
    g_rrect(4, 34, 252, 176, 4, C_PANEL);
    g_text(12, 40, "BUTTONS", &f12b, C_DIM);
    static const struct { int b, x, y; } bt[] = {
        { BTN_L2, 14, 58 }, { BTN_L1, 50, 58 }, { BTN_R1, 168, 58 }, { BTN_R2, 204, 58 },
        { BTN_UP, 44, 86 }, { BTN_LEFT, 14, 108 }, { BTN_RIGHT, 74, 108 }, { BTN_DOWN, 44, 130 },
        { BTN_L3, 98, 108 }, { BTN_R3, 132, 108 }, { BTN_X, 204, 86 }, { BTN_Y, 172, 108 }, { BTN_A, 220, 108 }, { BTN_B, 204, 130 },
        { BTN_SELECT, 92, 152 }, { BTN_START, 130, 152 },
    };
    static const char *const lab[BTN_COUNT] = { "\xe2\x86\x91", "\xe2\x86\x93", "\xe2\x86\x90", "\xe2\x86\x92", "A", "B", "X", "Y", "L1", "L2", "R1", "R2", "SEL", "START", "L3", "R3" };
    for (int k = 0; k < COUNT(bt); k++) {
        const bool h = ui.held[bt[k].b], seen = pressed_at[bt[k].b] != 0;
        w_chip(bt[k].x, bt[k].y, lab[bt[k].b], &f12, h ? C_INK : seen ? C_TEXT : C_DIM, h ? C_GREEN : seen ? C_PANEL2 : C_BG, 5, 17);
    }
    g_rrect(4, 182, 252, 300, 4, C_PANEL);
    g_text(12, 188, "STICKS", &f12b, C_DIM);
    w_stick(66, 240, ui.lx, ui.ly, RGB(255, 90, 160), "", "", C_TEXT, 36);
    k_snprintf(b, sizeof b, "%+.2f %+.2f", (double) ui.lx, (double) ui.ly);
    g_text_c(66, 280, b, &f12, C_TEXT);
    w_stick(186, 240, ui.rx, ui.ry, RGB(255, 90, 160), "", "", C_TEXT, 36);
    k_snprintf(b, sizeof b, "%+.2f %+.2f", (double) ui.rx, (double) ui.ry);
    g_text_c(186, 280, b, &f12, C_TEXT);

    // the audio and its time
    g_rrect(256, 34, 636, 300, 4, C_PANEL);
    g_text(264, 40, "AUDIO", &f12b, C_DIM);
    g_text(264, 58, ui_host_text[0] ? ui_host_text : "48000 Hz", &f12, C_TEXT);
    g_text(264, 80, "L", &f12, C_DIM); w_meter_h(280, 84, 620, MIN(ui.st.out_l, 1.0f), C_GREEN, 6);
    g_text(264, 96, "R", &f12, C_DIM); w_meter_h(280, 100, 620, MIN(ui.st.out_r, 1.0f), C_GREEN, 6);
    cpu_bars(264, 122, 364);
    g_text(264, 270, "Per track:", &f12, C_DIM);
    for (int t = 0; t < NTRK; t++) {
        const float v = ui.st.cpu_trk[t];
        const int x = 340 + t * 24, h = (int) (MIN(v * 4.0f, 1.0f) * 20);
        g_rect(x, 290 - h, x + 18, 290, trk_col[t]);
        g_rect(x, 291, x + 18, 292, C_PANEL2);
    }

    // the system
    g_rrect(4, 306, 636, 452, 4, C_PANEL);
    g_text(12, 312, "SYSTEM", &f12b, C_DIM);
    uint32_t used, total;
    ui_mem(&used, &total);
    k_snprintf(b, sizeof b, "SAMPLE MEMORY   %d OF %d MB", (int) (used >> 20), (int) (total >> 20));
    g_text(14, 334, b, &f12, C_TEXT);
    if (ui.battery >= 0) k_snprintf(b, sizeof b, "BATTERY   %d%%%s", ui.battery, ui.charging ? "  CHARGING" : "");
    else k_snprintf(b, sizeof b, "BATTERY   NONE (MAINS)");
    g_text(14, 352, b, &f12, C_TEXT);
    const uint32_t up = ui.now / 1000;
    k_snprintf(b, sizeof b, "UP   %d:%02d:%02d", (int) (up / 3600), (int) (up / 60 % 60), (int) (up % 60));
    g_text(14, 370, b, &f12, C_TEXT);
    k_snprintf(b, sizeof b, "DRUM QUALITY   %s", q_name[ui.prefs.quality == 4 ? 3 : ui.prefs.quality == 2 ? 2 : ui.prefs.quality == 1 ? 1 : 0]);
    g_text(14, 388, b, &f12, C_TEXT);
    g_text(330, 334, "Every button lights while it is held, and stays", &f12, C_DIM);
    g_text(330, 350, "white once it has worked.", &f12, C_DIM);
    static const char *const hints[] = { "SELECT+B", "leave" };
    w_footer(hints, COUNT(hints));
}

// ------------------------------------------------------------------------------------------------- BUTTON MAP

void buttons_button(int b, bool down, bool rep) { (void) rep; if (down && b == BTN_B) go(NAV_SETTINGS); }

static void callout(int x, int y, int tx, int ty, const char *a, const char *b, bool right)
{
    const int w = MAX(g_tw(a, &f12b), g_tw(b, &f12)) + 6;
    (void) tx;
    g_line(right ? 632 - w : 8 + w, ty + 6, x, y, C_DIMMER, 1);
    if (right) { g_text_r(632, ty - 8, a, &f12b, C_TEXT); g_text_r(632, ty + 6, b, &f12, C_DIM); }
    else { g_text(8, ty - 8, a, &f12b, C_TEXT); g_text(8, ty + 6, b, &f12, C_DIM); }
}

void draw_buttons(void)
{
    g_clear(C_BG);
    w_header("BUTTON MAP", RGB(255, 90, 160));
    // the device, roughly
    g_rrect(214, 52, 426, 446, 22, RGB(40, 40, 58));
    g_rframe(214, 52, 426, 446, 22, RGB(70, 70, 96), 2);
    g_rrect(228, 70, 412, 206, 4, C_BG);
    for (int t = 0; t < NTRK; t++)
        for (int s = 0; s < 16; s++) if ((s * 7 + t * 3) % 5 < 2) g_rect(238 + s * 10, 78 + t * 10, 245 + s * 10, 85 + t * 10, c_scale(trk_col[t], 0.8f));
    w_chip(228, 38, "L2", &f12, C_TEXT, C_PANEL2, 6, 14); w_chip(266, 38, "L1", &f12, C_TEXT, C_PANEL2, 6, 14);
    w_chip(346, 38, "R1", &f12, C_TEXT, C_PANEL2, 6, 14); w_chip(384, 38, "R2", &f12, C_TEXT, C_PANEL2, 6, 14);
    g_rect(240, 240, 254, 282, C_BG); g_rect(226, 254, 268, 268, C_BG);   // the D-pad
    static const struct { int x, y; const char *s; } face[] = { { 374, 232, "X" }, { 354, 252, "Y" }, { 394, 252, "A" }, { 374, 272, "B" } };
    for (int k = 0; k < 4; k++) { g_eframe(face[k].x - 9, face[k].y - 9, face[k].x + 9, face[k].y + 9, C_TEXT, 1); g_text_c(face[k].x, face[k].y - 6, face[k].s, &f12, C_TEXT); }
    g_ellipse(256, 316, 312, 372, RGB(26, 26, 38)); g_ellipse(268, 328, 300, 360, RGB(56, 56, 76));
    g_ellipse(328, 316, 384, 372, RGB(26, 26, 38)); g_ellipse(340, 328, 372, 360, RGB(56, 56, 76));
    g_rrect(286, 396, 314, 404, 4, C_BG); g_rrect(326, 396, 354, 404, 4, C_BG);
    g_text_c(300, 408, "SELECT", &f12, C_DIMMER); g_text_c(340, 408, "START", &f12, C_DIMMER);
    callout(266, 44, 8, 58, "L1  HOLD", "pads, tracks 1-8", false);
    callout(228, 44, 8, 100, "L2  HOLD", "pads 9-12, fill", false);
    callout(226, 260, 8, 210, "D-PAD", "move; with A held: change", false);
    callout(264, 340, 8, 296, "LEFT STICK", "two track macros, live", false);
    callout(300, 400, 8, 380, "SELECT", "menu; hold + D-pad: jump", false);
    callout(346, 44, 632, 58, "R1  HOLD", "record the stick (PERFORM)", true);
    callout(408, 44, 632, 100, "R2  HOLD", "edit: copy paste clear undo", true);
    callout(374, 232, 632, 160, "X", "step details", true);
    callout(354, 252, 632, 196, "Y", "track sound", true);
    callout(394, 252, 632, 232, "A", "hit, choose", true);
    callout(374, 272, 632, 268, "B", "back", true);
    callout(356, 340, 632, 314, "RIGHT STICK", "perform FX, R3 latches", true);
    callout(340, 400, 632, 380, "START", "play / stop", true);
    g_text(8, 430, "SELECT + START held: leave.", &f12, C_DIMMER);
    static const char *const hints[] = { "B", "back" };
    w_footer(hints, COUNT(hints));
}

// ------------------------------------------------------------------------------------------------- FILES

void usb_button(int b, bool down, bool rep) { (void) rep; if (down && b == BTN_B) go(NAV_SETTINGS); }

void draw_usb(void)
{
    g_clear(C_BG);
    w_header("FILES", RGB(170, 110, 255));
    // a computer and the card
    g_rframe(120, 80, 240, 160, 6, C_TEXT, 2);
    g_rect(130, 90, 230, 150, C_PANEL);
    g_rect(138, 100, 158, 106, C_RED); g_rect(138, 112, 172, 118, RGB(150, 230, 90)); g_rect(138, 124, 186, 130, C_SKY); g_rect(138, 136, 200, 142, RGB(220, 90, 255));
    g_line(96, 176, 264, 176, C_TEXT, 2); g_line(96, 176, 120, 164, C_TEXT, 2); g_line(264, 176, 240, 164, C_TEXT, 2);
    for (int k = 0; k < 6; k++) g_rect(270 + k * 22, 126, 282 + k * 22, 130, k % 2 ? C_DIM : RGB(170, 110, 255));
    g_rrect(420, 92, 470, 160, 4, C_PANEL2); g_rframe(420, 92, 470, 160, 4, C_TEXT, 2);
    g_tri(450, 92, 470, 92, 470, 108, C_BG);
    g_text_c(445, 120, "SD", &f14, C_TEXT);
    g_text_c(320, 204, "PUT THE CARD IN A COMPUTER", &f20, C_TEXT);
    g_text_c(320, 236, "This version runs on ArkOS, which keeps the card busy:", &f12, C_DIM);
    g_text_c(320, 252, "it cannot be a USB drive while it plays.", &f12, C_DIM);
    g_text_c(320, 282, "On the EASYROMS partition, in ports/rhythm/card:", &f12, C_DIM);
    static const char *const dirs[] = { "SAMPLES", "KITS", "PROJECTS", "RENDERS" };
    static const col_t dc[] = { RGB(255, 140, 70), RGB(150, 230, 90), RGB(90, 120, 255), RGB(220, 90, 255) };
    int w = 0;
    for (int k = 0; k < 4; k++) w += g_tw(dirs[k], &f14) + 16 + 8;
    int x = 320 - w / 2;
    for (int k = 0; k < 4; k++) x += w_chip(x, 300, dirs[k], &f14, C_INK, dc[k], 8, 22) + 8;
    g_text_c(320, 340, "Copy WAV files into SAMPLES; folders inside it are fine.", &f12, C_TEXT);
    g_text_c(320, 358, "8 to 32 bit, any rate, mono or stereo.", &f12, C_TEXT);
    g_text_c(320, 376, "RENDERS has what PROJECT > RENDER wrote.", &f12, C_TEXT);
    g_text_c(320, 412, "The bare-metal version will show up as a drive here.", &f12, C_DIMMER);
    static const char *const hints[] = { "B", "back" };
    w_footer(hints, COUNT(hints));
}
