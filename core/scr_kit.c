// KIT: the twelve sounds at a glance, and where each one goes (docs/mockups/screens/11). Kits load and save as
// KITS/NAME.KIT.
#include "plat.h"
#include "ui_int.h"

enum { KC_NAME, KC_ENGINE, KC_SOUND, KC_LEVEL, KC_PAN, KC_CHOKE, KC_SEND, KC_NOTE, KC_CH, KC_COUNT };
static const int col_x[KC_COUNT] = { 34, 104, 156, 290, 380, 432, 480, 530, 596 };
static const char *const col_name[KC_COUNT] = { "NAME", "ENGINE", "SOUND", "LEVEL", "PAN", "CHOKE", "MIDI", "NOTE", "CH" };

#define KIT_MAGIC 0x5449484bu   // "KHIT"
typedef struct { uint32_t magic, version, size; char name[16]; track_t trk[NTRK]; } kitfile_t;
static kitfile_t kf;

static void note_name(int n, char *out, int cap)
{
    static const char *const nn[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    k_snprintf(out, cap, "%s%d %d", nn[n % 12], n / 12 - 1, n);
}

static void rename_done(const char *s) { track_t *t = &doc.trk[ui.cur_t]; k_strcpy(t->name, s, sizeof t->name); w_upper(t->name); doc_touch(t->name, sizeof t->name); }

static void save_kit(const char *name)
{
    m_set(&kf, 0, sizeof kf);
    kf.magic = KIT_MAGIC; kf.version = DOC_VERSION; kf.size = sizeof kf;
    k_strcpy(kf.name, name, sizeof kf.name); w_upper(kf.name);
    m_copy(kf.trk, doc.trk, sizeof kf.trk);
    char path[48], f[20]; int n = 0;
    for (const char *c = kf.name; *c && n < 16; c++) f[n++] = (*c == ' ') ? '_' : *c;
    f[n] = 0;
    k_snprintf(path, sizeof path, "KITS/%s.KIT", f);
    k_strcpy(doc.kit, kf.name, sizeof doc.kit); doc_touch(doc.kit, sizeof doc.kit);
    ui_toast(plat_file_write(path, &kf, (int) sizeof kf) == (int) sizeof kf ? "KIT SAVED" : "COULD NOT SAVE");
}

bool kit_load(const char *path)
{
    if (plat_file_read(path, &kf, (int) sizeof kf) != (int) sizeof kf || kf.magic != KIT_MAGIC || kf.size != sizeof kf) return false;
    m_copy(doc.trk, kf.trk, sizeof doc.trk);
    doc_touch(doc.trk, sizeof doc.trk);
    k_strcpy(doc.kit, kf.name, sizeof doc.kit); doc_touch(doc.kit, sizeof doc.kit);
    for (int t = 0; t < NTRK; t++) ui_bind(t);
    return true;
}

static void change(int c, int d, bool big)
{
    track_t *t = &doc.trk[ui.cur_t];
    switch (c) {
    case KC_ENGINE:
        t->engine = (uint8_t) (t->engine == ENG_SYNTH ? ENG_SAMPLE : ENG_SYNTH);
        doc_touch(&t->engine, 1);
        if (t->engine == ENG_SAMPLE && !t->smp.path[0]) { k_strcpy(t->smp.path, "BUNDLED/BREAKS/DUSTY_94.WAV", sizeof t->smp.path); t->smp.end = 1; t->smp.filter = 20000; t->smp.bits = 16; t->smp.rate = 48000; doc_touch(&t->smp, sizeof t->smp); }
        k_strcpy(t->sound, t->engine == ENG_SYNTH ? drum_presets[MAX(drum_preset_find(t->sound), 0)].name : t->smp.path, sizeof t->sound);
        ui_bind(ui.cur_t);
        break;
    case KC_SOUND:
        if (t->engine == ENG_SYNTH) {
            int i = drum_preset_find(t->sound);
            i = ((i < 0 ? 0 : i) + d * (big ? 8 : 1) + drum_preset_count * 8) % drum_preset_count;
            const track_t keep = *t;
            track_from_preset(t, i, keep.name);
            t->choke = keep.choke; t->level = keep.level; t->pan = keep.pan; t->send_dly = keep.send_dly; t->send_rev = keep.send_rev;
            t->mute = keep.mute; t->solo = keep.solo; t->send = keep.send; t->midi_ch = keep.midi_ch; t->midi_note = keep.midi_note;
            t->smp = keep.smp;
            doc_touch(t, sizeof *t);
        }
        break;
    case KC_LEVEL: t->level = m_clampf(t->level + (float) d * (big ? 3.0f : 0.5f), -60, 12); doc_touch(&t->level, 4); break;
    case KC_PAN: t->pan = m_clampf(t->pan + (float) d * (big ? 0.25f : 0.05f), -1, 1); doc_touch(&t->pan, 4); break;
    case KC_CHOKE: t->choke = (uint8_t) CLAMP(t->choke + d, 0, 4); doc_touch(&t->choke, 1); break;
    case KC_SEND: t->send = (uint8_t) CLAMP(t->send + d, 0, 2); doc_touch(&t->send, 1); break;
    case KC_NOTE: t->midi_note = (uint8_t) CLAMP(t->midi_note + d * (big ? 12 : 1), 0, 127); doc_touch(&t->midi_note, 1); break;
    case KC_CH: t->midi_ch = (uint8_t) CLAMP(t->midi_ch + d, 0, 15); doc_touch(&t->midi_ch, 1); break;
    default: break;
    }
    if (!ui.st.playing && (c == KC_SOUND || c == KC_ENGINE)) ui_msg(M_HIT, ui.cur_t, 0, 0, 0.8f);
}

void kit_button(int b, bool down, bool rep)
{
    (void) rep;
    int *c = &ui.sel[NAV_KIT];
    if (ui.held[BTN_R2] && down && b != BTN_R2) {
        ui.used[BTN_R2] = true;
        if (b == BTN_X) name_open("SAVE KIT AS", doc.kit, 15, save_kit);
        else if (b == BTN_A) { ui.tab[NAV_BROWSER] = 2; go(NAV_BROWSER); }
        return;
    }
    if (!down) {
        if (b == BTN_A && !ui.used[BTN_A]) {
            if (*c == KC_NAME) name_open("TRACK NAME", doc.trk[ui.cur_t].name, 7, rename_done);
            else if (*c == KC_SOUND) { ui.tab[NAV_BROWSER] = doc.trk[ui.cur_t].engine == ENG_SYNTH ? 1 : 0; go(NAV_BROWSER); }
            else if (*c == KC_ENGINE) change(KC_ENGINE, 1, false);
        }
        return;
    }
    if (ui.held[BTN_A] && b >= BTN_UP && b <= BTN_RIGHT) {
        ui.used[BTN_A] = true;
        const int d = (b == BTN_RIGHT || b == BTN_UP) ? 1 : -1;
        change(*c, d, b == BTN_UP || b == BTN_DOWN);
        return;
    }
    switch (b) {
    case BTN_UP: ui.cur_t = (ui.cur_t + NTRK - 1) % NTRK; break;
    case BTN_DOWN: ui.cur_t = (ui.cur_t + 1) % NTRK; break;
    case BTN_LEFT: *c = MAX(*c - 1, 0); break;
    case BTN_RIGHT: *c = MIN(*c + 1, KC_COUNT - 1); break;
    case BTN_X: ui_msg(M_HIT, ui.cur_t, 0, 0, 0.8f); break;
    case BTN_Y: go(doc.trk[ui.cur_t].engine == ENG_SYNTH ? NAV_SOUND : NAV_SAMPLE); break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

void draw_kit(void)
{
    g_clear(C_BG);
    w_header("KIT", nav_col(NAV_KIT));
    for (int k = 0; k < KC_COUNT; k++) g_text(col_x[k], 36, col_name[k], &f12, ui.sel[NAV_KIT] == k ? C_TEXT : C_DIMMER);
    g_text(14, 36, "#", &f12, C_DIMMER);
    char b[40];
    for (int r = 0; r < NTRK; r++) {
        const track_t *t = &doc.trk[r];
        const int y = 54 + r * 25;
        const col_t col = trk_col[r];
        if (r == ui.cur_t) { g_rect(0, y - 3, SCR_W - 1, y + 18, c_mix(C_BG, col, 0.14f)); g_rect(0, y - 3, 1, y + 18, col); }
        g_rect(4, y - 1, 7, y + 15, col);
        k_snprintf(b, sizeof b, "%02d", r + 1);
        g_text(14, y + 1, b, &f12, C_DIM);
        g_text(34, y, t->name, &f14, col);
        w_chip(104, y + 1, t->engine == ENG_SYNTH ? "SYNTH" : "SAMPLE", &f12, c_scale(col, 0.9f), c_mix(C_BG, col, 0.18f), 3, 14);
        k_strcpy(b, t->sound, 22); w_upper(b);
        g_text(156, y + 1, b, &f12b, C_TEXT);
        w_bar(290, y + 4, 70, (t->level + 60) / 72.0f, col, false, 8);
        g_line(380, y + 7, 420, y + 7, C_KTRACK, 2); g_line(400, y + 3, 400, y + 11, C_DIMMER, 1);
        const int px = 400 + (int) (t->pan * 20);
        g_ellipse(px - 3, y + 4, px + 3, y + 10, col);
        if (t->choke) { char ch[2] = { (char) ('A' + t->choke - 1), 0 }; w_chip(444, y + 1, ch, &f12b, C_INK, t->choke & 1 ? C_AMBER : C_SKY, 3, 14); }
        else g_text(446, y + 1, "-", &f12, C_DIMMER);
        static const char *const sends[3] = { "INT", "MIDI", "BOTH" };
        g_text(480, y + 1, sends[t->send % 3], &f12, t->send ? C_GREEN : C_DIM);
        note_name(t->midi_note, b, sizeof b);
        g_text(530, y + 1, b, &f12, C_TEXT);
        k_snprintf(b, sizeof b, "%d", t->midi_ch + 1);
        g_text(598, y + 1, b, &f12, C_DIM);
        if (r == ui.cur_t) {
            const int c = ui.sel[NAV_KIT], x0 = col_x[c] - 3, x1 = c + 1 < KC_COUNT ? col_x[c + 1] - 6 : 636;
            g_frame(x0, y - 3, x1, y + 18, C_WHITE, 2);
        }
    }
    g_rect(0, 360, SCR_W - 1, 362, nav_col(NAV_KIT));
    w_box(4, 368, 636, 456, NULL, C_DIM, C_PANEL, NULL);
    g_text(14, 374, doc.kit, &f20, C_TEXT);
    int nsmp = 0, nsyn = 0;
    for (int t = 0; t < NTRK; t++) { if (doc.trk[t].engine == ENG_SAMPLE) nsmp++; else nsyn++; }
    k_snprintf(b, sizeof b, "%d SYNTH  ·  %d SAMPLE", nsyn, nsmp);
    g_text(14, 398, b, &f12, C_DIM);
    g_text(14, 414, "A on NAME types it, on SOUND opens the browser.", &f12, C_DIM);
    int x = 14;
    static const char *const acts[] = { "R2+A", "LOAD KIT", "R2+X", "SAVE KIT", "Y", "EDIT SOUND", "X", "LISTEN" };
    for (int k = 0; k < COUNT(acts); k += 2) {
        x += w_chip(x, 432, acts[k], &f12b, C_INK, C_KEY, 3, 14) + 3;
        x = g_text(x, 433, acts[k + 1], &f12, C_TEXT) + 12;
    }
    for (int i = 0; i < NTRK; i++) {
        const int xx = 430 + i * 17, h = (int) (40 * MIN(ui.st.level[i] * 1.5f, 1.0f)) + 2;
        g_rect(xx, 418 - h, xx + 12, 418, trk_col[i]);
    }
    g_text(430, 422, "LEVELS", &f12, C_DIMMER);
    static const char *const hints[] = { "\xe2\x86\x91\xe2\x86\x93", "track", "\xe2\x86\x90\xe2\x86\x92", "column", "A+\xe2\x86\x90\xe2\x86\x92", "change", "Y", "sound", "B", "back" };
    w_footer(hints, COUNT(hints));
}
