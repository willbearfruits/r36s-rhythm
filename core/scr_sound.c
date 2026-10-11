// SOUND: thump's drum engine, one section at a time, every parameter a knob (docs/mockups/screens/12). R1 and R2
// step through the sections the model has; X keeps an A and a B to flip between; Y rolls the dice on the section.
#include "ui_int.h"

static int sections(int type, int *out)
{
    int n = 0;
    for (int s = 0; s < DS_COUNT; s++) if (drum_shown(s, type)) out[n++] = s;
    return n;
}

static int section_params(int s, int *out)
{
    int n = 0;
    for (int p = 0; p < DRUM_NP; p++) if (drum_params[p].section == s && p != P_PAN && p != P_CHOKE) out[n++] = p;
    return n;
}

static float ab[NTRK][2][DRUM_NP];
static int8_t ab_side[NTRK];   // 0: never stored; 1: A is on; 2: B is on

static int cur_section(int type)
{
    int list[DS_COUNT];
    const int n = sections(type, list);
    ui.tab[NAV_SOUND] = CLAMP(ui.tab[NAV_SOUND], 0, n - 1);
    return list[ui.tab[NAV_SOUND]];
}

static void dice(int t, int s, float wild)
{
    track_t *tr = &doc.trk[t];
    const int type = CLAMP(m_lround(tr->synth[P_TYPE]), 0, DT_COUNT - 1);
    int ps[DRUM_NP];
    const int n = section_params(s, ps);
    rng_t r; rng_seed(&r, ui.now * 2654435761u + (uint32_t) t);
    for (int i = 0; i < n; i++) {
        const int p = ps[i];
        if (p == P_TYPE || p == P_LEVEL) continue;
        float lo, hi;
        drum_range(p, type, &lo, &hi);
        const drum_param_t *d = &drum_params[p];
        float target;
        if (d->log && lo > 0) target = lo * m_exp2(rng_uni(&r) * m_log2(hi / lo));
        else target = lo + (hi - lo) * rng_uni(&r);
        float v = tr->synth[p] + (target - tr->synth[p]) * wild;
        if (d->unit == DU_CHOICE || p == P_MODES_COUNT || p == P_CLAP_COUNT) v = m_round(v);
        tr->synth[p] = m_clampf(v, d->min, d->max);
    }
    doc_touch(tr->synth, sizeof tr->synth);
}

void sound_button(int b, bool down, bool rep)
{
    (void) rep;
    const int t = ui.cur_t;
    track_t *tr = &doc.trk[t];
    if (tr->engine != ENG_SYNTH) { if (down && b == BTN_B) go(NAV_PATTERN); return; }
    const int type = CLAMP(m_lround(tr->synth[P_TYPE]), 0, DT_COUNT - 1);
    int list[DS_COUNT];
    const int ns = sections(type, list);
    if (!down) return;
    switch (b) {
    case BTN_R1: ui.tab[NAV_SOUND] = (ui.tab[NAV_SOUND] + ns - 1) % ns; ui.sel[NAV_SOUND] = 0; return;
    case BTN_R2: ui.tab[NAV_SOUND] = (ui.tab[NAV_SOUND] + 1) % ns; ui.sel[NAV_SOUND] = 0; return;
    case BTN_B: go(NAV_PATTERN); return;
    case BTN_X:   // A/B: the first press keeps A and makes B from it; then each press flips
        if (!ab_side[t]) { m_copy(ab[t][0], tr->synth, sizeof ab[t][0]); m_copy(ab[t][1], tr->synth, sizeof ab[t][1]); ab_side[t] = 2; ui_toast("A KEPT. NOW EDITING B"); }
        else {
            const int on = ab_side[t] - 1;
            m_copy(ab[t][on], tr->synth, sizeof ab[t][on]);
            m_copy(tr->synth, ab[t][on ^ 1], sizeof tr->synth);
            doc_touch(tr->synth, sizeof tr->synth);
            ab_side[t] = (int8_t) ((on ^ 1) + 1);
            ui_toast(ab_side[t] == 1 ? "A" : "B");
        }
        ui_msg(M_HIT, t, 0, 0, 0.8f);
        return;
    case BTN_Y: dice(t, cur_section(type), 0.35f); ui_toast("DICE"); ui_msg(M_HIT, t, 0, 0, 0.8f); return;
    default: break;
    }
    int ps[DRUM_NP];
    const int n = section_params(cur_section(type), ps);
    prm_t pr[DRUM_NP];
    for (int i = 0; i < n; i++) pr[i] = prm_drum(t, ps[i]);
    if (prm_nav(pr, n, &ui.sel[NAV_SOUND], 5, b, down)) {
        if (!ui.st.playing) ui_msg(M_HIT, t, 0, 0, 0.8f);
    }
}

void draw_sound(void)
{
    g_clear(C_BG);
    const int t = ui.cur_t;
    const track_t *tr = &doc.trk[t];
    const col_t col = trk_col[t];
    char b[48];
    k_snprintf(b, sizeof b, "%02d", t + 1);
    w_header("SOUND", nav_col(NAV_SOUND));
    if (tr->engine != ENG_SYNTH) {
        g_text_c(SCR_W / 2, 200, "THIS TRACK PLAYS A SAMPLE", &f16, C_DIM);
        g_text_c(SCR_W / 2, 224, "Its settings are on the SAMPLE screen.", &f12, C_DIM);
        static const char *const hints[] = { "B", "back" };
        w_footer(hints, COUNT(hints));
        return;
    }
    const int type = CLAMP(m_lround(tr->synth[P_TYPE]), 0, DT_COUNT - 1);
    g_text(10, 38, b, &f14, C_DIM);
    g_text(34, 34, tr->name, &f24, col);
    k_strcpy(b, tr->sound, 20); w_upper(b);
    g_text(10, 62, b, &f16, C_TEXT);
    int x = 150 + w_chip(150, 40, "SYNTH", &f12, C_INK, col, 3, 14) + 4;
    k_snprintf(b, sizeof b, "%s MODEL", drum_type_name(type)); w_upper(b);
    w_chip(x, 40, b, &f12, col, c_mix(C_BG, col, 0.2f), 3, 14);
    k_snprintf(b, sizeof b, "LEVEL %+.1f  VEL %d%%", (double) tr->synth[P_LEVEL], (int) (tr->synth[P_VELSENS] * 100 + 0.5f));
    g_text(150, 62, b, &f12, C_DIM);
    g_text(388, 40, "A/B", &f12, C_DIM);
    w_chip(414, 37, "A", &f14, ab_side[t] == 2 ? col : C_INK, ab_side[t] == 2 ? C_PANEL2 : col, 5, 18);
    w_chip(436, 37, "B", &f14, ab_side[t] == 2 ? C_INK : col, ab_side[t] == 2 ? col : C_PANEL2, 5, 18);
    g_text(462, 42, ab_side[t] ? "X flips" : "X keeps A", &f12, C_DIMMER);
    w_chip(556, 37, "DICE", &f14, C_INK, C_AMBER, 5, 18);
    g_text(556, 58, "Y rolls", &f12, C_DIMMER);
    // the sections
    int list[DS_COUNT];
    const int ns = sections(type, list);
    const int sec = cur_section(type);
    x = 8;
    for (int i = 0; i < ns; i++) {
        char nm[12]; k_strcpy(nm, drum_section_name(list[i]), sizeof nm); w_upper(nm);
        const bool on = list[i] == sec;
        const int w = g_tw(nm, &f14) + 14;
        g_rrect(x, 84, x + w, 104, 3, on ? col : C_PANEL2);
        g_text(x + 7, 87, nm, &f14, on ? C_INK : C_TEXT);
        x += w + 3;
    }
    g_rect(0, 105, SCR_W - 1, 106, col);
    // the knobs: five a row, two rows at a time
    int ps[DRUM_NP];
    const int n = section_params(sec, ps);
    prm_t pr[DRUM_NP];
    for (int i = 0; i < n; i++) pr[i] = prm_drum(t, ps[i]);
    const int sel = CLAMP(ui.sel[NAV_SOUND], 0, n - 1);
    const int first_row = MAX(0, sel / 5 - 1);
    w_box(4, 110, 404, 336, NULL, C_DIM, C_PANEL, NULL);
    const int shown = MIN(n - first_row * 5, 10);
    w_knobs(pr + first_row * 5, shown, sel - first_row * 5, 44, 158, 5, 79, 104, 24, col);
    if (n > 10) { k_snprintf(b, sizeof b, "%d-%d of %d", first_row * 5 + 1, first_row * 5 + shown, n); g_text_r(398, 116, b, &f12, C_DIM); }
    // the sound
    w_box(410, 110, 636, 214, "SOUND", C_DIM, C_PANEL, NULL);
    static float pk[480];
    float secs;
    ui_preview(t, pk, 216, &secs);
    const int mid = 166;
    for (int c = 0; c < 216; c++) {
        const int lo = m_lround(pk[2 * c] * 36), hi = m_lround(pk[2 * c + 1] * 36);
        g_line(415 + c, mid - hi, 415 + c, mid - lo, c_scale(col, 0.75f), 1);
    }
    k_snprintf(b, sizeof b, "%.2f s", (double) secs);
    g_text_r(628, 115, b, &f12, C_DIM);
    // what this section does, in a word
    w_box(410, 220, 636, 336, "SECTIONS IN THIS MODEL", C_DIM, C_PANEL, NULL);
    for (int i = 0; i < ns; i++) {
        char nm[12]; k_strcpy(nm, drum_section_name(list[i]), sizeof nm); w_upper(nm);
        g_text(420 + (i % 3) * 72, 240 + (i / 3) * 16, nm, &f12, list[i] == sec ? col : C_DIM);
    }
    g_text(420, 304, "R1 R2 step through them.", &f12, C_DIMMER);
    // the macros
    g_text(10, 340, "MACROS", &f12b, C_DIM);
    static const int mac[5] = { P_TUNE, P_PUNCH, P_LENGTH, P_TONE, P_GRIT };
    for (int k = 0; k < 5; k++) {
        const prm_t p = prm_drum(t, mac[k]);
        const int xx = 4 + k * 127;
        g_rrect(xx, 356, xx + 122, 452, 4, C_PANEL);
        char lab[12]; k_strcpy(lab, p.label, sizeof lab); w_upper(lab);
        g_text(xx + 8, 362, lab, &f12b, C_DIM);
        if (mac[k] == ui.prefs.l_x) g_text_r(xx + 114, 362, "L \xe2\x86\x90\xe2\x86\x92", &f12b, col);
        if (mac[k] == ui.prefs.l_y) g_text_r(xx + 114, 362, "L \xe2\x86\x91\xe2\x86\x93", &f12b, col);
        prm_text(&p, b, sizeof b);
        g_text(xx + 8, 380, b, &f24, C_TEXT);
        const float nv = prm_norm(&p);
        w_bar(xx + 8, 414, 106, prm_bipolar(&p) ? nv * 2 - 1 : nv, col, prm_bipolar(&p), 10);
        g_text(xx + 8, 432, "every section", &f12, C_DIMMER);
    }
    static const char *const hints[] = { "R1/R2", "section", "D-PAD", "move", "A+\xe2\x86\x90\xe2\x86\x92", "change", "X", "A/B", "Y", "dice", "B", "back" };
    w_footer(hints, COUNT(hints));
}
