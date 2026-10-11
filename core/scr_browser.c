// BROWSER: samples on the card, the factory drums, kits and projects (docs/mockups/screens/19). It plays what the
// cursor rests on; A loads it into the track that is selected.
#include "plat.h"
#include "ui_int.h"

enum { BT_SAMPLES, BT_DRUMS, BT_KITS, BT_PROJECTS, BT_COUNT };
static const char *const tabs[BT_COUNT] = { "SAMPLES", "DRUMS", "KITS", "PROJECTS" };

#define NENT 256
typedef struct { char name[48]; bool dir; } ent_t;
static ent_t ent[NENT];
static int nent, sel[BT_COUNT], top[BT_COUNT];
static char path[160], listed_path[160];
static int listed_tab = -1;
static uint32_t heard_at;
static int heard_sel = -1, heard_tab = -1, heard_slot = -1;
static int drum_order[64];
static char projs[64][32];
static int nprojs;

static bool is_wav(const char *n) { const int l = k_strlen(n); return l > 4 && (!k_strcmp(n + l - 4, ".WAV") || !k_strcmp(n + l - 4, ".wav")); }
static bool is_kit(const char *n) { const int l = k_strlen(n); return l > 4 && !k_strcmp(n + l - 4, ".KIT"); }

static void sort_ents(int from)
{
    for (int i = from + 1; i < nent; i++)
        for (int j = i; j > from; j--) {
            const ent_t *a = &ent[j - 1], *b = &ent[j];
            const bool swap = (a->dir != b->dir) ? !a->dir : k_strcmp(a->name, b->name) > 0;
            if (!swap) break;
            const ent_t t = ent[j]; ent[j] = ent[j - 1]; ent[j - 1] = t;
        }
}

static void list(void)
{
    const int tab = ui.tab[NAV_BROWSER];
    if (listed_tab == tab && !k_strcmp(listed_path, path)) return;
    listed_tab = tab;
    k_strcpy(listed_path, path, sizeof listed_path);
    nent = 0;
    char e[64]; bool dir;
    if (tab == BT_SAMPLES) {
        if (!path[0]) {
            static const char *const roots[] = { "SAMPLES", "BUNDLED", "RECORDINGS" };
            for (int i = 0; i < 3; i++) if (i < 2 || plat_dir(roots[i], 0, e, sizeof e, &dir)) { k_strcpy(ent[nent].name, roots[i], 48); ent[nent++].dir = true; }
        } else {
            k_strcpy(ent[nent].name, "..", 48); ent[nent++].dir = true;
            for (int i = 0; nent < NENT && plat_dir(path, i, e, sizeof e, &dir); i++)
                if (dir || is_wav(e)) { k_strcpy(ent[nent].name, e, 48); ent[nent++].dir = dir; }
            sort_ents(1);
        }
    } else if (tab == BT_KITS) {
        for (int k = 0; k < kit8_count && nent < NENT; k++) { k_strcpy(ent[nent].name, kit8_presets[k].name, 48); w_upper(ent[nent].name); ent[nent++].dir = false; }
        for (int i = 0; nent < NENT && plat_dir("KITS", i, e, sizeof e, &dir); i++)
            if (!dir && is_kit(e)) { e[k_strlen(e) - 4] = 0; k_snprintf(ent[nent].name, 48, "~ %s", e); ent[nent++].dir = false; }
    } else if (tab == BT_PROJECTS) {
        nprojs = proj_list(projs, 64);
    }
}

static void full(const char *name, char *out, int cap)
{
    if (!path[0]) k_strcpy(out, name, cap);
    else k_snprintf(out, cap, "%s/%s", path, name);
}

static int count(void)
{
    switch (ui.tab[NAV_BROWSER]) {
    case BT_DRUMS: return drum_preset_count;
    case BT_PROJECTS: return nprojs;
    default: return nent;
    }
}

static void load_sample(const char *file)
{
    const int t = ui.cur_t;
    track_t *tr = &doc.trk[t];
    if (tr->engine != ENG_SAMPLE) {   // a synth track becomes a sample track, keeping its place in the mix
        tr->engine = ENG_SAMPLE;
        tr->smp.start = 0; tr->smp.end = 1; tr->smp.filter = 20000; tr->smp.bits = 16; tr->smp.rate = 48000;
        tr->smp.mode = SMP_ONESHOT; tr->smp.choke_self = 1; tr->smp.slices = 0;
    }
    k_strcpy(tr->smp.path, file, sizeof tr->smp.path);
    m_set(tr->smp.slice_pos, 0, sizeof tr->smp.slice_pos);
    const char *f = file;
    for (const char *c = file; *c; c++) if (*c == '/') f = c + 1;
    k_strcpy(tr->sound, f, sizeof tr->sound);
    doc_touch(tr, sizeof *tr);
    ui_bind(t);
    char b[48]; k_snprintf(b, sizeof b, "IN TRACK %d", t + 1);
    ui_toast(ui_sample(t) ? b : "COULD NOT LOAD IT");
}

static void load_drum(int i)
{
    const int t = ui.cur_t;
    track_t *tr = &doc.trk[t];
    const track_t keep = *tr;
    track_from_preset(tr, i, keep.name);
    tr->choke = keep.choke; tr->level = keep.level; tr->pan = keep.pan; tr->send_dly = keep.send_dly; tr->send_rev = keep.send_rev;
    tr->mute = keep.mute; tr->solo = keep.solo; tr->send = keep.send; tr->midi_ch = keep.midi_ch; tr->midi_note = keep.midi_note;
    tr->smp = keep.smp;
    doc_touch(tr, sizeof *tr);
    ui_bind(t);
    char b[48]; k_snprintf(b, sizeof b, "IN TRACK %d", t + 1);
    ui_toast(b);
}

static int pending_kit = -1;
static char pending_proj[32];

static void do_kit(void)
{
    const int k = pending_kit;
    if (k < 0) return;
    if (k < kit8_count) {   // thump's eight drums into tracks 1-8
        const kit8_t *kp = &kit8_presets[k];
        for (int t = 0; t < 8; t++) {
            const int i = drum_preset_find(kp->drums[t]);
            if (i < 0) continue;
            track_t *tr = &doc.trk[t];
            const track_t keep = *tr;
            track_from_preset(tr, i, keep.name);
            tr->level = keep.level; tr->pan = keep.pan; tr->send_dly = keep.send_dly; tr->send_rev = keep.send_rev;
            tr->mute = keep.mute; tr->solo = keep.solo; tr->send = keep.send; tr->midi_ch = keep.midi_ch; tr->midi_note = keep.midi_note;
            if (t == 4 || t == 5) tr->choke = 1;
            ui_bind(t);
        }
        doc_touch(doc.trk, sizeof doc.trk);
        k_strcpy(doc.kit, kp->name, sizeof doc.kit); w_upper(doc.kit); doc_touch(doc.kit, sizeof doc.kit);
        ui_toast("KIT LOADED");
    } else {
        char p[64];
        k_snprintf(p, sizeof p, "KITS/%s.KIT", ent[k].name + 2);
        for (char *c = p; *c; c++) if (*c == ' ') *c = '_';
        ui_toast(kit_load(p) ? "KIT LOADED" : "COULD NOT LOAD IT");
    }
}

static void do_proj(void) { if (proj_load(pending_proj)) { ui_toast("OPENED"); go(NAV_PATTERN); } else ui_toast("COULD NOT OPEN IT"); }

void browser_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    const int tab = ui.tab[NAV_BROWSER];
    list();
    const int n = count();
    int *s = &sel[tab];
    switch (b) {
    case BTN_R1: ui.tab[NAV_BROWSER] = (tab + BT_COUNT - 1) % BT_COUNT; return;
    case BTN_R2: ui.tab[NAV_BROWSER] = (tab + 1) % BT_COUNT; return;
    case BTN_UP: if (n) *s = (*s + n - 1) % n; return;
    case BTN_DOWN: if (n) *s = (*s + 1) % n; return;
    case BTN_B: go(ui.from == NAV_BROWSER || ui.from >= SCR_COUNT ? NAV_PATTERN : ui.from); return;
    case BTN_LEFT:
        if (tab == BT_SAMPLES && path[0]) {   // up a folder
            char *slash = NULL;
            for (char *c = path; *c; c++) if (*c == '/') slash = c;
            if (slash) *slash = 0; else path[0] = 0;
            *s = 0;
        }
        return;
    case BTN_X: heard_sel = -1; heard_at = 0; return;   // play it again
    case BTN_RIGHT: case BTN_A:
        if (*s >= n) return;
        if (tab == BT_SAMPLES) {
            if (ent[*s].dir) {
                if (!k_strcmp(ent[*s].name, "..")) { browser_button(BTN_LEFT, true, false); return; }
                char p[160]; full(ent[*s].name, p, sizeof p);
                k_strcpy(path, p, sizeof path);
                sel[tab] = 0; top[tab] = 0;
            } else if (b == BTN_A) { char p[160]; full(ent[*s].name, p, sizeof p); load_sample(p); }
        } else if (b == BTN_A) {
            if (tab == BT_DRUMS) load_drum(drum_order[*s]);
            else if (tab == BT_KITS) { pending_kit = *s; confirm_open("LOAD THE KIT?", *s < kit8_count ? "Its eight drums go into tracks 1-8. Tracks 9-12 stay." : "All twelve tracks change.", "LOAD", do_kit); }
            else if (tab == BT_PROJECTS) {
                k_strcpy(pending_proj, projs[*s], sizeof pending_proj);
                if (!k_strcmp(pending_proj, doc.name)) { ui_toast("ALREADY OPEN"); return; }
                if (ui.modified) proj_save(doc.name);
                confirm_open("OPEN THE PROJECT?", "The one open now is saved first.", "OPEN", do_proj);
            }
        }
        return;
    default: return;
    }
}

// Listening: a moment after the cursor stops on a sample or a drum.
static void listen(void)
{
    const int tab = ui.tab[NAV_BROWSER], s = sel[tab];
    if (s != heard_sel || tab != heard_tab) { heard_sel = s; heard_tab = tab; heard_at = ui.now + 180; heard_slot = -1; return; }
    if (!heard_at || ui.now < heard_at) return;
    heard_at = 0;
    if (tab == BT_SAMPLES && s < nent && !ent[s].dir) {
        char p[160]; full(ent[s].name, p, sizeof p);
        heard_slot = ui_sample_preview(p);
        if (heard_slot >= 0) ui_msg(M_PREVIEW, heard_slot, 0, 0, 0.8f);
    } else if (tab == BT_DRUMS && s < drum_preset_count) ui_msg(M_PREVIEW, drum_order[s], 1, 0, 0.85f);
}

void draw_browser(void)
{
    if (!drum_order[1]) {   // the factory drums by model, then name
        int k = 0;
        for (int type = 0; type < DT_COUNT; type++)
            for (int i = 0; i < drum_preset_count; i++) if (drum_presets[i].type == type) drum_order[k++] = i;
    }
    g_clear(C_BG);
    const col_t col = nav_col(NAV_BROWSER);
    w_header("BROWSER", col);
    list();
    listen();
    const int tab = ui.tab[NAV_BROWSER], n = count();
    int *s = &sel[tab];
    *s = CLAMP(*s, 0, MAX(n - 1, 0));
    w_tabs(8, 36, tabs, BT_COUNT, tab, col);
    char b[96];
    k_snprintf(b, sizeof b, "INTO TRACK %d  %s", ui.cur_t + 1, doc.trk[ui.cur_t].name);
    g_text_r(632, 40, b, &f12b, trk_col[ui.cur_t]);
    // the list
    w_box(4, 62, 400, 440, NULL, C_DIM, C_PANEL, NULL);
    const int rows = 17;
    int *tp = &top[tab];
    if (*s < *tp) *tp = *s;
    if (*s >= *tp + rows) *tp = *s - rows + 1;
    if (tab == BT_SAMPLES) { k_snprintf(b, sizeof b, "/%s", path); g_text(12, 66, b, &f12, C_DIM); }
    for (int r = 0; r < rows && *tp + r < n; r++) {
        const int i = *tp + r, y = (tab == BT_SAMPLES ? 84 : 70) + r * 21;
        const bool on = i == *s;
        if (on) { g_rect(8, y - 3, 396, y + 15, c_mix(C_PANEL, col, 0.3f)); g_rect(8, y - 3, 9, y + 15, col); }
        switch (tab) {
        case BT_SAMPLES:
            if (ent[i].dir) { g_text(16, y, "\xe2\x96\xba", &f12, C_DIM); k_strcpy(b, ent[i].name, 44); g_text(30, y, b, &f12b, C_TEXT); }
            else { k_strcpy(b, ent[i].name, 52); w_upper(b); g_text(16, y, b, on ? &f12b : &f12, C_TEXT); }
            break;
        case BT_DRUMS: {
            const drum_preset_t *d = &drum_presets[drum_order[i]];
            k_strcpy(b, d->name, 40); w_upper(b);
            g_text(16, y, b, on ? &f12b : &f12, C_TEXT);
            char m[16]; k_strcpy(m, drum_type_name(d->type), sizeof m); w_upper(m);
            g_text_r(392, y, m, &f12, C_DIM);
            break;
        }
        case BT_KITS: g_text(16, y, ent[i].name, on ? &f12b : &f12, i < kit8_count ? C_TEXT : C_AMBER); break;
        default:
            g_text(16, y, projs[i], on ? &f12b : &f12, C_TEXT);
            if (!k_strcmp(projs[i], doc.name)) w_chip(340, y - 1, "OPEN", &f12b, C_INK, C_GREEN, 4, 14);
            break;
        }
    }
    if (!n) g_text(16, 90, tab == BT_PROJECTS ? "No projects saved yet." : "Nothing here.", &f12, C_DIMMER);
    if (n > rows) { const int h = 370 * rows / n, y = 66 + 370 * *tp / n; g_rect(396, 66, 398, 436, C_PANEL2); g_rect(396, y, 398, y + h, C_DIM); }
    // what is under the cursor
    w_box(406, 62, 636, 440, NULL, C_DIM, C_PANEL, NULL);
    int y = 70;
    if (tab == BT_SAMPLES && *s < nent && !ent[*s].dir) {
        k_strcpy(b, ent[*s].name, 28); w_upper(b);
        g_text(414, y, b, &f12b, C_TEXT); y += 20;
        const sample_t *sm = heard_slot >= 0 && heard_sel == *s ? ui_slot(heard_slot) : NULL;
        g_rect(414, y, 628, y + 70, RGB(12, 12, 20)); g_frame(414, y, 628, y + 70, C_LINE, 1);
        if (sm) {
            for (int c = 0; c < 212; c++) {
                const uint32_t a = (uint32_t) ((uint64_t) c * sm->frames / 212), e = (uint32_t) ((uint64_t) (c + 1) * sm->frames / 212);
                int hi = 0;
                for (uint32_t k = a; k < e; k += 4) { const int v = sm->data[k * sm->chans]; hi = MAX(hi, v < 0 ? -v : v); }
                const int h = hi * 32 / 32768;
                g_line(415 + c, y + 35 - h, 415 + c, y + 35 + h, col, 1);
            }
            y += 82;
            k_snprintf(b, sizeof b, "%.2f s", (double) sm->frames / sm->rate); w_setting(414, y, 210, "LENGTH", b, false, col, C_TEXT); y += 18;
            k_snprintf(b, sizeof b, "%d HZ", (int) sm->rate); w_setting(414, y, 210, "RATE", b, false, col, C_TEXT); y += 18;
            w_setting(414, y, 210, "CHANNELS", sm->chans > 1 ? "STEREO" : "MONO", false, col, C_TEXT); y += 18;
        } else { y += 82; g_text(414, y, "Listening in a moment...", &f12, C_DIMMER); y += 18; }
    } else if (tab == BT_DRUMS && *s < drum_preset_count) {
        const drum_preset_t *d = &drum_presets[drum_order[*s]];
        k_strcpy(b, d->name, 28); w_upper(b);
        g_text(414, y, b, &f16, col); y += 26;
        char m[16]; k_strcpy(m, drum_type_name(d->type), sizeof m); w_upper(m);
        w_setting(414, y, 210, "MODEL", m, false, col, C_TEXT); y += 18;
        k_strcpy(m, d->genre, sizeof m); w_upper(m);
        w_setting(414, y, 210, "STYLE", m, false, col, C_TEXT); y += 18;
        k_snprintf(b, sizeof b, "%+.1f dB", (double) drum_preset_level[drum_order[*s]]);
        w_setting(414, y, 210, "LEVEL", b, false, col, C_TEXT); y += 18;
        g_text(414, y + 8, "From thump's library.", &f12, C_DIMMER); y += 26;
    } else if (tab == BT_KITS && *s < nent && *s < kit8_count) {
        const kit8_t *kp = &kit8_presets[*s];
        k_snprintf(b, sizeof b, "%.0f BPM", (double) kp->tempo);
        g_text(414, y, b, &f12b, C_TEXT); y += 18;
        for (int t = 0; t < 8; t++) { k_strcpy(b, kp->drums[t], 30); w_upper(b); g_text(414, y, b, &f12, trk_col[t]); y += 15; }
    } else if (tab == BT_PROJECTS && *s < nprojs) {
        g_text(414, y, projs[*s], &f14, C_TEXT); y += 22;
        g_text(414, y, "A opens it.", &f12, C_DIM);
    }
    y = 360;
    static const char *const acts[] = { "A", "LOAD", "X", "LISTEN AGAIN", "R1 R2", "TABS" };
    for (int k = 0; k < COUNT(acts); k += 2) {
        const int x = 414 + w_chip(414, y, acts[k], &f12b, C_INK, C_KEY, 3, 14) + 4;
        g_text(x, y + 1, acts[k + 1], &f12, C_TEXT);
        y += 22;
    }
    g_text(8, 446, tab == BT_SAMPLES ? "Your own WAVs go in SAMPLES on the card (any folders inside)." : "", &f12, C_DIMMER);
    static const char *const hints[] = { "R1/R2", "tab", "\xe2\x86\x91\xe2\x86\x93", "move", "\xe2\x86\x90\xe2\x86\x92", "folder", "A", "load", "B", "back" };
    w_footer(hints, COUNT(hints));
}
