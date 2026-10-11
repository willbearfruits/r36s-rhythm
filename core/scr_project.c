// PROJECT: what is on the card, saving and opening, and rendering to WAV (docs/mockups/screens/21). The open
// project also saves itself a few seconds after each change.
#include "plat.h"
#include "ui_int.h"

static char names[64][32];
static int nnames;
static uint32_t listed_at;
static uint8_t what, times = 3;   // render: pattern / song; pattern loops: 1 2 4 8
static char pending[32];
static char last_render[96];

static void relist(void) { nnames = proj_list(names, 64); listed_at = ui.now; }

static void save_as(const char *name) { ui_toast(proj_save(name) ? "SAVED" : "COULD NOT SAVE"); relist(); }
static void do_open(void) { if (proj_load(pending)) { ui_toast("OPENED"); go(NAV_PATTERN); } else ui_toast("COULD NOT OPEN IT"); relist(); }
static void do_new(void) { if (ui.modified) proj_save(doc.name); proj_new(); ui_toast("NEW PROJECT"); relist(); go(NAV_PATTERN); }

static void rendered(const uint8_t *wav, int bytes)
{
    char dir[48], f[40]; int n = 0;
    for (const char *c = doc.name; *c && n < 30; c++) f[n++] = (*c == ' ') ? '_' : *c;
    f[n] = 0;
    k_snprintf(dir, sizeof dir, "%s", f);
    k_snprintf(last_render, sizeof last_render, "RENDERS/%s/%s.WAV", dir, what ? "SONG" : "PATTERN");
    ui_toast(plat_file_write(last_render, wav, bytes) == bytes ? "RENDERED" : "COULD NOT WRITE IT");
    ui_msg(M_STOP, 0, 0, 0, 0);
    if (what) ui_msg(M_SONG, 0, 0, 0, 0);
}

static void render(void)
{
    const double bar = 60.0 / doc.bpm * 4.0 * 48000.0 * (CLAMP(doc.pat[ui.pat].len, 1, NSTEP) / 16.0);
    uint32_t frames;
    if (what == 0) frames = (uint32_t) (bar * (double) (1 << times) + 48000.0 * 1.5);
    else {
        double total = 0;
        for (int i = 0; i < doc.song_len; i++) total += 60.0 / doc.bpm * 4.0 * 48000.0 * (CLAMP(doc.pat[doc.song[i].pat].len, 1, NSTEP) / 16.0) * MAX(doc.song[i].repeat, 1);
        frames = (uint32_t) (total + 48000.0 * 2);
    }
    if (frames > 48000u * 600) { ui_toast("TOO LONG (10 MIN MOST)"); return; }
    ui_msg(M_STOP, 0, 0, 0, 0);
    if (what) ui_msg(M_SONG, 1, 0, 0, 0); else ui_msg(M_QUEUE, ui.pat, 1, 0, 0);
    if (!ui_capture(0, 1, frames, rendered)) ui_toast("NO ROOM TO RECORD");
}

void project_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    int *s = &ui.sel[NAV_PROJECT];
    if (ui.held[BTN_R2] && b != BTN_R2) {
        ui.used[BTN_R2] = true;
        if (b == BTN_A) save_as(doc.name);
        else if (b == BTN_X && !ui_capturing()) render();
        else if (b == BTN_LEFT || b == BTN_RIGHT) what ^= 1;
        else if (b == BTN_UP || b == BTN_DOWN) times = (uint8_t) CLAMP(times + (b == BTN_UP ? 1 : -1), 0, 3);
        return;
    }
    switch (b) {
    case BTN_UP: if (nnames) *s = (*s + nnames - 1) % nnames; break;
    case BTN_DOWN: if (nnames) *s = (*s + 1) % nnames; break;
    case BTN_A:
        if (*s < nnames && k_strcmp(names[*s], doc.name)) {
            k_strcpy(pending, names[*s], sizeof pending);
            if (ui.modified) proj_save(doc.name);
            confirm_open("OPEN THE PROJECT?", "The one open now is saved first.", "OPEN", do_open);
        } else ui_toast("THAT ONE IS OPEN");
        break;
    case BTN_X: name_open("SAVE AS", doc.name, 24, save_as); break;
    case BTN_Y: confirm_open("START A NEW PROJECT?", "The one open now is saved first. The new one gets the BASEMENT kit and empty patterns.", "NEW", do_new); break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

void draw_project(void)
{
    if (!listed_at || ui.now - listed_at > 3000) relist();
    g_clear(C_BG);
    const col_t col = nav_col(NAV_PROJECT);
    w_header("PROJECT", col);
    w_box(4, 36, 300, 392, "ON THE CARD", C_DIM, C_PANEL, NULL);
    int *s = &ui.sel[NAV_PROJECT];
    *s = CLAMP(*s, 0, MAX(nnames - 1, 0));
    const int top = MAX(0, *s - 7);
    for (int i = top; i < nnames && i < top + 8; i++) {
        const int y = 58 + (i - top) * 41;
        if (i == *s) { g_rect(8, y - 4, 296, y + 33, c_mix(C_PANEL, col, 0.22f)); g_rect(8, y - 4, 9, y + 33, col); }
        g_text(18, y, names[i], &f14, C_TEXT);
        if (!k_strcmp(names[i], doc.name)) w_chip(240, y, "OPEN", &f12b, C_INK, C_GREEN, 4, 14);
    }
    if (!nnames) g_text(18, 60, "Nothing saved yet.", &f12, C_DIMMER);
    // the open one
    w_box(306, 36, 636, 196, NULL, C_DIM, C_PANEL, NULL);
    g_text(316, 44, doc.name, &f20, C_TEXT);
    char b[64];
    int used = 0, bars = 0, smp = 0;
    for (int i = 0; i < NPAT; i++) if (doc.pat[i].used) used++;
    for (int i = 0; i < doc.song_len; i++) bars += MAX(doc.song[i].repeat, 1);
    for (int t = 0; t < NTRK; t++) if (doc.trk[t].engine == ENG_SAMPLE) smp++;
    k_snprintf(b, sizeof b, "%.1f BPM", (double) doc.bpm); w_setting(316, 74, 310, "TEMPO", b, false, col, C_TEXT);
    k_snprintf(b, sizeof b, "%d USED", used); w_setting(316, 92, 310, "PATTERNS", b, false, col, C_TEXT);
    k_snprintf(b, sizeof b, "%d ENTRIES  %d BARS", doc.song_len, bars); w_setting(316, 110, 310, "SONG", b, false, col, C_TEXT);
    k_snprintf(b, sizeof b, "%s  %d SAMPLE TRACKS", doc.kit, smp); w_setting(316, 128, 310, "KIT", b, false, col, C_TEXT);
    if (ui.modified) k_strcpy(b, "CHANGED, SAVING SOON", sizeof b);
    else k_snprintf(b, sizeof b, "%d S AGO", (int) ((ui.now - ui.saved_at) / 1000));
    w_setting(316, 146, 310, "SAVED", b, false, col, ui.modified ? C_AMBER : C_GREEN);
    int x = 316;
    static const char *const acts[] = { "R2+A", "SAVE", "X", "SAVE AS", "Y", "NEW", "A", "OPEN" };
    for (int k = 0; k < COUNT(acts); k += 2) { x += w_chip(x, 172, acts[k], &f12b, C_INK, C_KEY, 3, 14) + 3; x = g_text(x, 173, acts[k + 1], &f12, C_TEXT) + 10; }
    // rendering
    w_box(306, 202, 636, 392, "RENDER TO WAV", col, C_PANEL, NULL);
    static const char *const wh[2] = { "PATTERN", "SONG" };
    g_text(316, 226, "WHAT", &f12, C_DIM);
    w_chips(370, 224, wh, 2, what, col, 15, 5, 4);
    if (!what) { k_snprintf(b, sizeof b, "\xc3\x97%d", 1 << times); g_text(500, 226, b, &f12b, C_TEXT); }
    w_setting(316, 250, 310, "FORMAT", "WAV 48 KHZ 16 BIT", false, col, C_TEXT);
    w_setting(316, 268, 310, "OUTPUT MODE", doc.mst.out_rate ? "AS SET (GRITTY)" : "AS SET (CLEAN)", false, col, doc.mst.out_rate ? C_AMBER : C_TEXT);
    w_setting(316, 286, 310, "TO", "RENDERS/<PROJECT>/", false, col, C_TEXT);
    if (ui_capturing()) {
        const float u = ui.st.cap_frames ? (float) ui.st.cap_pos / (float) ui.st.cap_frames : 0;
        for (int i = 0; i < 12; i++) g_rect(316 + i * 26, 312, 338 + i * 26, 326, (float) i / 12.0f < u ? trk_col[i] : C_PANEL2);
        k_snprintf(b, sizeof b, "RECORDING  %d%%  (IT PLAYS AS IT RECORDS)", (int) (u * 100));
        g_text(316, 334, b, &f12b, C_TEXT);
    } else {
        g_text(316, 312, "R2+X renders. It plays while it records, so", &f12, C_DIM);
        g_text(316, 326, "what you hear is what you get.", &f12, C_DIM);
        if (last_render[0]) { g_text(316, 346, "LAST:", &f12, C_DIMMER); char lr[48]; k_strcpy(lr, last_render, 46); g_text(354, 346, lr, &f12, C_DIMMER); }
    }
    g_text(316, 370, "R2+\xe2\x86\x90\xe2\x86\x92 what, R2+\xe2\x86\x91\xe2\x86\x93 how many times", &f12, C_DIMMER);
    // getting files on and off
    w_box(4, 398, 636, 456, NULL, C_DIM, C_PANEL, NULL);
    g_text(14, 406, "FILES", &f12b, col);
    g_text(14, 422, "With a card reader: your WAVs go in ports/rhythm/card/SAMPLES; renders come out of", &f12, C_DIM);
    g_text(14, 436, "ports/rhythm/card/RENDERS. (A USB drive mode comes with the bare-metal version.)", &f12, C_DIM);
    static const char *const hints[] = { "\xe2\x86\x91\xe2\x86\x93", "project", "A", "open", "X", "save as", "Y", "new", "R2+X", "render", "B", "back" };
    w_footer(hints, COUNT(hints));
}
