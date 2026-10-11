// GENERATE (docs/mockups/screens/07): Euclid rhythms per track, written into the pattern as they change; and the
// dice: vary the steps and sounds of the tracks not kept, or write a fill (it plays when FILL is on).
#include "ui_int.h"

enum { EU_HITS, EU_STEPS, EU_ROTATE, EU_ACCENT, EU_COUNT };
enum { DI_AMOUNT, DI_SOUND, DI_KEEP, DI_FILL, DI_LENGTH, DI_COUNT };
enum { FILL_ROLL, FILL_BUILD, FILL_BREAK, FILL_DROP };
static const char *const eu_name[EU_COUNT] = { "HITS", "STEPS", "ROTATE", "ACCENT" };
static const char *const di_name[DI_COUNT] = { "PATTERN", "SOUND", "KEEP", "FILL", "LENGTH" };
static const char *const fill_name[] = { "ROLL", "BUILD", "BREAK", "DROP" };
static const char *const len_name[] = { "1/2 BAR", "1 BAR", "2 BARS" };

static struct { int8_t hits, steps, rot, acc; } eu[NTRK];
static bool eu_known[NTRK];
static uint8_t amount = 35, sound = 20, fill = FILL_BUILD, flen = 1, keep_cur;
static uint16_t keep = 0x7;     // the kick, snare and clap stay as they are
static bool dice_focus;
static int eu_row;
static bool undo_taken;
static int known_pat = -1;

static bool euclid_on(int i, int k, int n, int rot) { i = ((i - rot) % n + n) % n; return (i * k) % n < k; }

static void eu_read(int t)   // what the track has now, the first time it is looked at
{
    if (known_pat != ui.pat) { for (int k = 0; k < NTRK; k++) eu_known[k] = false; known_pat = ui.pat; undo_taken = false; }
    if (eu_known[t]) return;
    const pattern_t *p = cur_pat();
    const int n = CLAMP(p->trk[t].len, 1, NSTEP);
    int k = 0;
    for (int s = 0; s < n; s++) if (p->step[t][s].on) k++;
    eu[t].hits = (int8_t) k; eu[t].steps = (int8_t) n; eu[t].rot = 0; eu[t].acc = 0;
    eu_known[t] = true;
}

static bool is_euclid(int t)
{
    const pattern_t *p = cur_pat();
    const int n = CLAMP(p->trk[t].len, 1, NSTEP);
    if (!eu_known[t] || eu[t].steps != n || eu[t].hits == 0) return false;
    for (int s = 0; s < n; s++) if ((p->step[t][s].on != 0) != euclid_on(s, eu[t].hits, n, eu[t].rot)) return false;
    return true;
}

static void take_undo(void) { if (!undo_taken) { undo_save(); undo_taken = true; } }

static void eu_write(int t)
{
    take_undo();
    pattern_t *p = cur_pat();
    const int n = CLAMP(eu[t].steps, 1, NSTEP), k = CLAMP(eu[t].hits, 0, n);
    p->trk[t].len = (uint8_t) n;
    int h = 0;
    for (int s = 0; s < NSTEP; s++) {
        step_t *st = &p->step[t][s];
        const bool on = s < n && k > 0 && euclid_on(s, k, n, eu[t].rot);
        if (on && !st->on) *st = (step_t) { 1, 100, 0, 0, 100, 0, 0, 0 };
        else if (!on) st->on = 0;
        if (on) {
            st->flags = (uint8_t) ((st->flags & ~STEP_ACCENT) | (eu[t].acc > 0 && h % eu[t].acc == 0 ? STEP_ACCENT : 0));
            h++;
        }
    }
    p->used = 1;
    doc_touch(p->step[t], sizeof p->step[t]);
    doc_touch(&p->trk[t], sizeof p->trk[t]);
    doc_touch(&p->used, 1);
}

// The dice: the steps of the tracks not kept move, drop out or gain ghosts; their sounds drift.
static void roll(bool steps, bool sounds)
{
    take_undo();
    pattern_t *p = cur_pat();
    rng_t r; rng_seed(&r, ui.now * 2654435761u + 77u);
    const float a = (float) amount / 100.0f, sa = (float) sound / 100.0f;
    for (int t = 0; t < NTRK; t++) {
        if (keep >> t & 1) continue;
        const int n = CLAMP(p->trk[t].len, 1, NSTEP);
        if (steps)
            for (int s = 0; s < n; s++) {
                step_t *st = &p->step[t][s];
                const float u = rng_uni(&r);
                if (st->on && u < a * 0.35f) {   // moves to a free neighbour, or goes
                    const int to = (s + (rng_uni(&r) < 0.5f ? 1 : n - 1)) % n;
                    if (!p->step[t][to].on && rng_uni(&r) < 0.7f) { p->step[t][to] = *st; }
                    st->on = 0;
                } else if (!st->on && u < a * 0.18f) {
                    *st = (step_t) { 1, (uint8_t) (40 + rng_uni(&r) * 50), 0, 0, 100, 0, 0, 0 };
                    if (rng_uni(&r) < a * 0.3f) st->chance = 50;
                    if (rng_uni(&r) < a * 0.15f) st->ratchet = 1;
                }
            }
        if (sounds && sa > 0) {
            track_t *tr = &doc.trk[t];
            if (tr->engine == ENG_SYNTH) {
                static const int ps[] = { P_TUNE, P_LENGTH, P_TONE, P_PUNCH, P_GRIT };
                for (int i = 0; i < COUNT(ps); i++) {
                    const drum_param_t *d = &drum_params[ps[i]];
                    const float span = (d->max - d->min) * 0.25f * sa;
                    tr->synth[ps[i]] = m_clampf(tr->synth[ps[i]] + (rng_uni(&r) * 2 - 1) * span, d->min, d->max);
                }
                doc_touch(tr->synth, sizeof tr->synth);
            } else {
                tr->smp.pitch = m_clampf(tr->smp.pitch + m_round((rng_uni(&r) * 2 - 1) * 5 * sa), -24, 24);
                doc_touch(&tr->smp.pitch, sizeof tr->smp.pitch);
            }
        }
    }
    p->used = 1;
    doc_touch(p->step, sizeof p->step);
    doc_touch(&p->used, 1);
}

// A fill over the pattern's last half bar, bar or two: only heard when FILL is on (the pads, or a song's last bar).
static void write_fill(void)
{
    take_undo();
    pattern_t *p = cur_pat();
    const int plen = CLAMP(p->len, 1, NSTEP), span = MIN(plen, flen == 0 ? 8 : flen == 1 ? 16 : 32), from = plen - span;
    const int t0 = ui.cur_t;
    for (int t = 0; t < NTRK; t++) {
        const int n = CLAMP(p->trk[t].len, 1, NSTEP);
        for (int s = from; s < plen && s < n; s++) {
            step_t *st = &p->step[t][s];
            const float u = (float) (s - from + 1) / (float) span;
            if (t == t0 && (fill == FILL_ROLL || fill == FILL_BUILD)) {
                if (!st->on) { *st = (step_t) { 1, 100, 0, 0, 100, COND_FILL, 0, 0 }; }
                st->vel = (uint8_t) CLAMP((int) (50 + 77 * u), 1, 127);
                if (fill == FILL_BUILD) st->ratchet = (uint8_t) ((u < 0.5f ? 0 : u < 0.8f ? 1 : 3) | (RAT_RISE << 4));
                else st->ratchet = (uint8_t) (s >= plen - 2 ? 2 | (RAT_RISE << 4) : 0);
            } else if (st->on && (fill == FILL_DROP || (fill == FILL_BREAK && t != 0 && t != t0))) st->cond = COND_NOT_FILL;
        }
    }
    p->used = 1;
    doc_touch(p->step, sizeof p->step);
    doc_touch(&p->used, 1);
    ui_toast("FILL WRITTEN: PLAYS WITH FILL ON");
}

void generate_button(int b, bool down, bool rep)
{
    (void) rep;
    const int t = ui.cur_t;
    eu_read(t);
    if (!down) {
        if (b == BTN_A && !ui.used[BTN_A]) {
            if (!dice_focus) eu_write(t);
            else if (ui.sel[SCR_GENERATE] == DI_KEEP) keep ^= (uint16_t) (1u << keep_cur);
            else if (ui.sel[SCR_GENERATE] >= DI_FILL) write_fill();
            else { roll(ui.sel[SCR_GENERATE] == DI_AMOUNT, true); ui_toast("ROLLED"); }
        }
        return;
    }
    int *row = &ui.sel[SCR_GENERATE];
    if (ui.held[BTN_A] && b >= BTN_UP && b <= BTN_RIGHT) {
        ui.used[BTN_A] = true;
        const int d = (b == BTN_RIGHT || b == BTN_UP) ? 1 : -1;
        const bool big = b == BTN_UP || b == BTN_DOWN;
        if (!dice_focus) {
            switch (eu_row) {
            case EU_HITS: eu[t].hits = (int8_t) CLAMP(eu[t].hits + d, 0, eu[t].steps); break;
            case EU_STEPS: eu[t].steps = (int8_t) CLAMP(eu[t].steps + d * (big ? 4 : 1), 1, NSTEP); eu[t].hits = MIN(eu[t].hits, eu[t].steps); break;
            case EU_ROTATE: eu[t].rot = (int8_t) ((eu[t].rot + d + eu[t].steps) % MAX(eu[t].steps, 1)); break;
            default: eu[t].acc = (int8_t) CLAMP(eu[t].acc + d, 0, 8); break;
            }
            eu_write(t);
        } else switch (*row) {
            case DI_AMOUNT: amount = (uint8_t) CLAMP(amount + d * (big ? 10 : 5), 0, 100); break;
            case DI_SOUND: sound = (uint8_t) CLAMP(sound + d * (big ? 10 : 5), 0, 100); break;
            case DI_FILL: fill = (uint8_t) CLAMP(fill + d, 0, 3); break;
            case DI_LENGTH: flen = (uint8_t) CLAMP(flen + d, 0, 2); break;
            default: break;
            }
        return;
    }
    switch (b) {
    case BTN_UP:
        if (dice_focus) *row = MAX(*row - 1, 0); else { ui.cur_t = MAX(t - 1, 0); undo_taken = false; }
        break;
    case BTN_DOWN:
        if (dice_focus) *row = MIN(*row + 1, DI_COUNT - 1); else { ui.cur_t = MIN(t + 1, NTRK - 1); undo_taken = false; }
        break;
    case BTN_LEFT: if (dice_focus) { if (*row == DI_KEEP) keep_cur = (uint8_t) MAX(keep_cur - 1, 0); } else eu_row = MAX(eu_row - 1, 0); break;
    case BTN_RIGHT: if (dice_focus) { if (*row == DI_KEEP) keep_cur = (uint8_t) MIN(keep_cur + 1, NTRK - 1); } else eu_row = MIN(eu_row + 1, EU_COUNT - 1); break;
    case BTN_R1: case BTN_R2: dice_focus = !dice_focus; break;
    case BTN_X: ui_msg(M_HIT, t, 0, 0, 0.8f); break;
    case BTN_Y:
        if (!ui.undo_ok || ui.undo_pat != ui.pat) { ui_toast("NOTHING TO UNDO"); break; }
        { pattern_t *p = cur_pat(); const pattern_t was = *p; *p = ui.undo; ui.undo = was; doc_touch(p, sizeof *p); }
        for (int k = 0; k < NTRK; k++) eu_known[k] = false;
        undo_taken = false;
        ui_toast("UNDONE");
        break;
    case BTN_B: undo_taken = false; go(NAV_PATTERN); break;
    default: break;
    }
}

void draw_generate(void)
{
    g_clear(C_BG);
    const col_t gc = nav_col(NAV_PATTERN);
    w_header("GENERATE", C_RED);
    const int t = ui.cur_t;
    eu_read(t);
    const pattern_t *p = cur_pat();
    const col_t tc = trk_col[t];
    char b[48];

    // the circle
    g_rrect(4, 34, 300, 292, 4, dice_focus ? C_PANEL : c_mix(C_PANEL, tc, 0.06f));
    k_snprintf(b, sizeof b, "EUCLID  \xc2\xb7  %02d %s", t + 1, doc.trk[t].name);
    g_text(12, 40, b, &f12b, tc);
    const int n = CLAMP(p->trk[t].len, 1, NSTEP), cx = 152, cy = 172, R = 96;
    int xy[2 * NSTEP], np = 0;
    for (int s = 0; s < n; s++) {
        if (!p->step[t][s].on) continue;
        const float a = (float) s / (float) n;
        xy[2 * np] = cx + (int) (m_sin2pi_hq(a) * R); xy[2 * np + 1] = cy - (int) (m_cos(a * 2 * K_PI) * R); np++;
    }
    if (np >= 3) g_poly(xy, np, c_mix(C_PANEL, tc, 0.22f));
    for (int k = 0; k < np && np >= 2; k++) { const int j = (k + 1) % np; g_line(xy[2 * k], xy[2 * k + 1], xy[2 * j], xy[2 * j + 1], c_scale(tc, 0.7f), 1); }
    const int play = ui.st.playing && ui.st.pat == ui.pat ? ui.st.step[t] : -1;
    for (int s = 0; s < n; s++) {
        const float a = (float) s / (float) n;
        const int x = cx + (int) (m_sin2pi_hq(a) * R), y = cy - (int) (m_cos(a * 2 * K_PI) * R);
        const step_t *st = &p->step[t][s];
        if (st->on) {
            const int r = (st->flags & STEP_ACCENT) ? 10 : 8;
            g_ellipse(x - r, y - r, x + r, y + r, s == play ? C_WHITE : tc);
        } else g_eframe(x - 4, y - 4, x + 4, y + 4, s == play ? C_WHITE : C_DIM, 1);
        if (s == 0) g_tri(x - 5, y - 18, x + 5, y - 18, x, y - 12, C_AMBER);
    }
    int hits = 0;
    for (int s = 0; s < n; s++) if (p->step[t][s].on) hits++;
    k_snprintf(b, sizeof b, "%d / %d", hits, n);
    g_text_c(cx, cy - 12, b, &f24, C_TEXT);

    // its numbers
    g_rrect(4, 298, 300, 452, 4, C_PANEL);
    for (int r = 0; r < EU_COUNT; r++) {
        const int y = 308 + r * 26;
        const bool s = !dice_focus && r == eu_row;
        if (s) { g_rect(6, y - 4, 298, y + 18, c_mix(C_PANEL, tc, 0.2f)); g_rect(6, y - 4, 8, y + 18, tc); }
        g_text(16, y, eu_name[r], &f12, s ? C_TEXT : C_DIM);
        float v = 0;
        switch (r) {
        case EU_HITS: v = (float) eu[t].hits / (float) MAX(eu[t].steps, 1); k_snprintf(b, sizeof b, "%d", eu[t].hits); break;
        case EU_STEPS: v = (float) eu[t].steps / NSTEP; k_snprintf(b, sizeof b, "%d", eu[t].steps); break;
        case EU_ROTATE: v = (float) eu[t].rot / (float) MAX(eu[t].steps, 1); k_snprintf(b, sizeof b, "%+d", eu[t].rot); break;
        default: v = (float) eu[t].acc / 8.0f; if (eu[t].acc) k_snprintf(b, sizeof b, "EVERY %d", eu[t].acc); else k_snprintf(b, sizeof b, "NONE"); break;
        }
        w_bar(80, y + 4, 140, v, tc, false, 6);
        g_text_r(292, y, b, &f12b, C_TEXT);
    }
    g_text(14, 414, "Changes write into the pattern as you go.", &f12, C_DIM);
    g_text(14, 430, "Y undoes the last change.", &f12, C_DIM);

    // every track
    g_rrect(304, 34, 636, 262, 4, C_PANEL);
    g_text(312, 40, "ALL TRACKS", &f12b, C_DIM);
    for (int k = 0; k < NTRK; k++) {
        const int y = 56 + k * 17;
        if (k == t) g_rect(306, y - 2, 634, y + 14, c_mix(C_PANEL, trk_col[k], 0.25f));
        char nm[8]; k_strcpy(nm, doc.trk[k].name, sizeof nm);
        g_text(312, y, nm, &f12, trk_col[k]);
        const int kn = CLAMP(p->trk[k].len, 1, NSTEP);
        int kh = 0;
        for (int s = 0; s < kn; s++) if (p->step[k][s].on) kh++;
        if (kh) { k_snprintf(b, sizeof b, "%s%d\xc2\xb7%d", is_euclid(k) ? "E " : "", kh, kn); g_text(372, y, b, &f12, C_TEXT); }
        else g_text(372, y, "\xe2\x80\x94", &f12, C_DIMMER);
        for (int s = 0; s < 16; s++) {
            const int x = 452 + s * 11;
            const bool on = s < kn && p->step[k][s].on;
            g_rect(x, y + 1, x + 8, y + 11, on ? trk_col[k] : s < kn ? c_mix(C_PANEL, trk_col[k], 0.18f) : C_PANEL);
        }
    }

    // the dice
    g_rrect(304, 268, 636, 452, 4, dice_focus ? c_mix(C_PANEL, C_AMBER, 0.06f) : C_PANEL);
    g_text(312, 274, "DICE  \xc2\xb7  FILL", &f12b, dice_focus ? C_AMBER : C_DIM);
    const int row = ui.sel[SCR_GENERATE];
    for (int r = 0; r < DI_COUNT; r++) {
        const int y = 294 + r * 24;
        const bool s = dice_focus && r == row;
        if (s) { g_rect(306, y - 4, 634, y + 17, c_mix(C_PANEL, C_AMBER, 0.2f)); g_rect(306, y - 4, 308, y + 17, C_AMBER); }
        g_text(316, y, di_name[r], &f12, s ? C_TEXT : C_DIM);
        switch (r) {
        case DI_AMOUNT: case DI_SOUND: {
            const int v = r == DI_AMOUNT ? amount : sound;
            w_bar(390, y + 4, 180, (float) v / 100.0f, C_AMBER, false, 6);
            k_snprintf(b, sizeof b, "%d%%", v); g_text_r(628, y, b, &f12b, C_TEXT);
            break;
        }
        case DI_KEEP:
            for (int k = 0; k < NTRK; k++) {
                const int x = 390 + k * 19;
                const bool on = keep >> k & 1;
                g_rect(x, y, x + 14, y + 13, on ? trk_col[k] : c_mix(C_PANEL, trk_col[k], 0.3f));
                if (on) g_text_c(x + 7, y, "\xe2\x9c\x93", &f12b, C_INK);
                if (s && k == keep_cur) g_frame(x - 2, y - 2, x + 16, y + 15, C_WHITE, 1);
            }
            break;
        case DI_FILL: w_chips(390, y - 1, fill_name, 4, fill, C_AMBER, 16, 5, 4); break;
        default: w_chips(390, y - 1, len_name, 3, flen, C_AMBER, 16, 5, 4); break;
        }
    }
    const char *act = !dice_focus ? "WRITE" : row == DI_KEEP ? "KEEP" : row >= DI_FILL ? "FILL" : "ROLL";
    int x = 314 + w_chip(314, 420, "A", &f14, C_INK, C_AMBER, 6, 22);
    x += w_chip(x, 420, act, &f14, C_AMBER, C_PANEL2, 8, 22) + 10;
    x += w_chip(x, 420, "Y", &f14, C_INK, C_TEXT, 6, 22);
    w_chip(x, 420, "UNDO", &f14, C_TEXT, C_PANEL2, 8, 22);
    (void) gc;
    static const char *const he[] = { "\xe2\x86\x91\xe2\x86\x93", "track", "\xe2\x86\x90\xe2\x86\x92", "row", "A+\xe2\x86\x90\xe2\x86\x92", "change", "X", "listen", "R1", "dice", "Y", "undo", "B", "back" };
    static const char *const hd[] = { "\xe2\x86\x91\xe2\x86\x93", "row", "A+\xe2\x86\x90\xe2\x86\x92", "change", "A", "roll / fill", "R1", "euclid", "Y", "undo", "B", "back" };
    if (dice_focus) w_footer(hd, COUNT(hd)); else w_footer(he, COUNT(he));
}
