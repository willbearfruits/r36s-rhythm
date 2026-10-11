// Inside the screen side: its state, the palette, the screens, the parameter tables and the widgets they share
// (docs/mockups/sheet.py is the picture they follow).
#pragma once
#include "fx.h"
#include "gfx.h"
#include "link.h"
#include "presets.h"
#include "smp.h"
#include "ui.h"

#define C_BG      RGB(10, 10, 16)
#define C_PANEL   RGB(18, 18, 28)
#define C_PANEL2  RGB(30, 30, 46)
#define C_LINE    RGB(50, 50, 72)
#define C_KTRACK  RGB(42, 42, 62)
#define C_TEXT    RGB(236, 236, 244)
#define C_DIM     RGB(132, 132, 154)
#define C_DIMMER  RGB(74, 74, 96)
#define C_WHITE   RGB(255, 255, 255)
#define C_INK     RGB(14, 10, 18)
#define C_AMBER   RGB(255, 190, 60)
#define C_GREEN   RGB(96, 236, 132)
#define C_RED     RGB(255, 76, 76)
#define C_SKY     RGB(90, 190, 255)
#define C_KEY     RGB(200, 200, 214)

extern const col_t trk_col[NTRK];
enum { NAV_PATTERN, NAV_PATTERNS, NAV_SONG, NAV_PERFORM, NAV_KIT, NAV_SOUND, NAV_SAMPLE, NAV_SAMPLING,
       NAV_MIXER, NAV_FX, NAV_MASTER, NAV_BROWSER, NAV_PROJECT, NAV_MIDI, NAV_SYNC, NAV_SETTINGS, NAV_COUNT,
       // reached from other screens, not the menu
       SCR_GENERATE = NAV_COUNT, SCR_DIAG, SCR_BUTTONS, SCR_USB, SCR_COUNT };
extern const char *const nav_name[NAV_COUNT];
col_t nav_col(int i);

typedef struct { void (*draw)(void); void (*button)(int b, bool down, bool rep); } screen_t;
void go(int screen);                               // to a screen (B on it comes back to PATTERN)

enum { OV_NONE, OV_MENU, OV_STEP, OV_PADS, OV_NAME, OV_CONFIRM };

// What the device remembers between sessions (not part of a project).
typedef struct {
    uint32_t magic, version;
    char project[32];                  // the project that was open
    float dead;                        // stick dead zone, 0..0.4
    uint8_t curve, repeat_ms, swap_ab, pad_accent;   // curve: 0 straight 1 soft 2 steep; pad_accent: R2 = accent
    uint8_t quality, stick_return, l_x, l_y;         // drum oversampling 1/2/4; l_x/l_y: what the left stick moves
    uint8_t r_x, r_y;                                // the perform effects on the right stick's axes
    uint8_t buffer, pad;                             // audio buffer: 0 the host's choice, 1 256, 2 512, 3 1024 frames
} prefs_t;

typedef struct {
    int screen, overlay, from;         // from: the screen before (B in the browser goes back there)
    int cur_t, cur_s, page, pat;      // the cursor: track, step (-1: the names), page of 16, pattern being edited
    bool held[BTN_COUNT];
    uint32_t down_at[BTN_COUNT], rep_at[BTN_COUNT];
    bool used[BTN_COUNT];             // a held button took part in a combination: no tap on release
    bool a_off;                       // A went down on a lit step: it goes out on release, unless A was used
    float lx, ly, rx, ry;
    bool l_live;                      // the left stick is moving live values
    status_t st;
    uint32_t seen_hits[NTRK], flash[NTRK];
    uint32_t now;
    bool rec, pads_b;
    int step_row, menu_sel;
    char toast[48];
    uint32_t toast_until;
    int battery;
    bool charging;
    step_t clip[NSTEP];
    ptrk_t clip_trk;
    bool clip_ok;
    pattern_t undo;
    int undo_pat;
    bool undo_ok;
    pattern_t pat_clip;
    bool pat_clip_ok;
    float sr;
    int sel[SCR_COUNT];               // each screen's selection
    int tab[SCR_COUNT];               // each screen's tab or section
    bool modified;                    // the project changed since it was saved
    uint32_t modified_at, saved_at;
    prefs_t prefs;
    bool perf_latch;
    int perf_fx_x, perf_fx_y;
} ui_t;
extern ui_t ui;
extern doc_t doc;

// ui.c
void doc_touch(const void *p, size_t n);          // a part of the document changed: send it to the engine
void doc_all(void);                               // the whole document changed (a project loaded)
void ui_msg(int type, int a, int b, int c, float f);
void ui_toast(const char *s);
pattern_t *cur_pat(void);
void undo_save(void);
const sample_t *ui_sample(int track);             // what a sample track has loaded, or NULL
int  ui_sample_load(const char *path);            // into a slot (once per path): the slot, or -1
const sample_t *ui_slot(int slot);
int  ui_sample_preview(const char *path);         // into a listening slot: the slot, or -1
void ui_bind(int track);                          // (re)load and bind a sample track's file
void ui_preview(int t, float *peaks, int cols, float *secs);   // the drum's shape: min/max pairs per column
int  pattern_len(const pattern_t *p);             // the longest track, in steps
void prefs_save(void);
void ui_mem(uint32_t *used, uint32_t *total);     // the sample memory
extern char ui_host_text[160];                   // what the host says about itself (ui_host_info)
// Records frames of the output (src 0) or a track (1..12) into memory, then calls done with a whole WAV file
// (header and all) to save or use. start: 0 now, 1 play from the top, 2 from the next bar. False if busy or full.
bool ui_capture(uint8_t src, uint8_t start, uint32_t frames, void (*done)(const uint8_t *wav, int bytes));
bool ui_capturing(void);

// ui_prm.c: one value on screen, wherever it lives in the document
enum { PT_F32, PT_U8, PT_I8 };
enum { UN_NONE, UN_HZ, UN_MS, UN_DB, UN_ST, UN_PCT, UN_X, UN_S, UN_INT, UN_CHOICE, UN_BPM, UN_SIGNED, UN_SEC };
typedef struct {
    const char *label;
    void *p;
    uint8_t type, unit;
    float min, max, step;      // step: per click (log: a ratio per click)
    bool log;
    const char *choices;       // UN_CHOICE: "A|B|C"
} prm_t;
float prm_get(const prm_t *p);
void  prm_set(const prm_t *p, float v);
void  prm_step(const prm_t *p, int dir, bool big);
float prm_norm(const prm_t *p);
bool  prm_bipolar(const prm_t *p);
void  prm_text(const prm_t *p, char *out, int cap);
prm_t prm_drum(int t, int param);                 // a synth parameter of track t
prm_t prm_f(const char *label, float *v, float min, float max, float step, uint8_t unit);
prm_t prm_flog(const char *label, float *v, float min, float max, uint8_t unit);
prm_t prm_u8(const char *label, uint8_t *v, float min, float max, uint8_t unit);
prm_t prm_choice(const char *label, uint8_t *v, const char *choices);
// D-pad moves the selection over a grid of cols; with A held it changes the value (left/right fine, up/down
// coarse). True when it changed a value.
bool  prm_nav(const prm_t *ps, int n, int *sel, int cols, int b, bool down);
void  w_knobs(const prm_t *ps, int n, int sel, int x0, int y0, int cols, int dx, int dy, int r, col_t col);
void  w_list(const prm_t *ps, int n, int sel, int x, int y, int w, int rowh, col_t col);

// ui_draw.c
int  w_chip(int x, int y, const char *s, const font_t *f, col_t fg, col_t bg, int padx, int h);
int  w_chips(int x, int y, const char *const *items, int n, int active, col_t col, int h, int padx, int gap);
void w_bar(int x, int y, int w, float v, col_t c, bool bipolar, int h);
void w_param(int x, int y, int w, const char *label, float v, const char *val, col_t c, bool bipolar, bool sel, int lw);
void w_setting(int x, int y, int w, const char *label, const char *val, bool sel, col_t c, col_t vcol);
void w_knob(int cx, int cy, int r, float v, col_t c, const char *label, const char *val, bool bipolar, int w);
void w_stick(int cx, int cy, float dx, float dy, col_t c, const char *lab, const char *val, col_t vcol, int r);
void w_box(int x0, int y0, int x1, int y1, const char *title, col_t tcol, col_t fill, const char *right);
void w_toggle(int x, int y, bool on, col_t c);
void w_footer(const char *const *hints, int n);   // pairs: button, what it does
void w_status(void);
void w_rainbow(int y);
void w_header(const char *title, col_t c);
void w_toast(void);
void w_tabs(int x, int y, const char *const *tabs, int n, int active, col_t col);
void w_upper(char *s);
void w_meter_v(int x, int y0, int y1, float v, col_t col, int w);
void w_meter_h(int x0, int y, int x1, float v, col_t col, int h);

// overlays
void name_open(const char *title, const char *init, int maxlen, void (*done)(const char *));
void confirm_open(const char *title, const char *body, const char *yes, void (*fn)(void));
void draw_name(void);
void draw_confirm(void);
void name_button(int b, bool down, bool rep);
void confirm_button(int b, bool down);

bool kit_load(const char *path);                   // scr_kit.c
void lock_set(pattern_t *p, int t, int s, int param, float v);   // scr_pattern.c
void perform_latch(void);                          // scr_perform.c: R3
void midi_monitor(int dir, uint8_t st, uint8_t d1, uint8_t d2);   // scr_io.c: a message the engine saw
extern char ui_midi_ports_text[160];              // what the host is connected to (ui_midi_ports)

// projects (proj.c)
bool proj_save(const char *name);
bool proj_load(const char *name);
int  proj_list(char (*names)[32], int max);
void proj_new(void);
void proj_autosave(void);

// screens
extern const screen_t screens[SCR_COUNT];
void draw_pattern(void);
void draw_step(void);
void draw_pads(void);
void draw_menu(void);
void pattern_button(int b, bool down, bool rep);
void step_button(int b, bool down, bool rep);
void pads_button(int b, bool down);
void menu_button(int b, bool down, bool rep);
void pattern_sticks(void);
void perform_sticks(void);
#define SCREEN(name) void draw_##name(void); void name##_button(int b, bool down, bool rep);
SCREEN(patterns) SCREEN(song) SCREEN(perform) SCREEN(kit) SCREEN(sound) SCREEN(sample) SCREEN(sampling)
SCREEN(mixer) SCREEN(fx) SCREEN(master) SCREEN(browser) SCREEN(project) SCREEN(midi) SCREEN(sync) SCREEN(settings)
SCREEN(generate) SCREEN(diag) SCREEN(buttons) SCREEN(usb)
