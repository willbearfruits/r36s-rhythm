// PADS: while L1 is held the D-pad and ABXY play tracks 1-8, with L2 tracks 9-12 (docs/mockups/screens/06).
// With REC on (SELECT while holding) and the pattern running, hits go into it where they were played: the step
// they fall nearest, nudged to the exact moment.
#include "ui_int.h"

typedef struct { int btn, trk; const char *lab; int dx, dy; } pad_t;
static const pad_t bank_a[8] = {
    { BTN_UP, 0, "↑", -42, -90 }, { BTN_LEFT, 1, "←", -132, -27 }, { BTN_RIGHT, 3, "→", 48, -27 }, { BTN_DOWN, 2, "↓", -42, 36 },
    { BTN_X, 4, "X", -42, -90 }, { BTN_Y, 5, "Y", -132, -27 }, { BTN_A, 7, "A", 48, -27 }, { BTN_B, 6, "B", -42, 36 },
};
static const pad_t bank_b[4] = { { BTN_X, 8, "X", -42, -90 }, { BTN_Y, 9, "Y", -132, -27 }, { BTN_A, 11, "A", 48, -27 }, { BTN_B, 10, "B", -42, 36 } };

static int pad_track(int b)
{
    if (!ui.pads_b) { for (int i = 0; i < 8; i++) if (bank_a[i].btn == b) return bank_a[i].trk; }
    else for (int i = 0; i < 4; i++) if (bank_b[i].btn == b) return bank_b[i].trk;
    return -1;
}

// Writes the hit into the running pattern. True when the sequencer will play it (so the pad does not).
static bool record(int t, float vel)
{
    const int pn = ui.st.pat;
    pattern_t *p = &doc.pat[CLAMP(pn, 0, NPAT - 1)];
    const int len = CLAMP(p->trk[t].len, 1, NSTEP);
    const double s = speed_mul[CLAMP(p->trk[t].speed, 0, SPD_COUNT - 1)];
    const double pos = (double) ui.st.beat * 4.0 * s;
    const double frac = pos - (double) (int64_t) pos;
    int k = ui.st.step[t];
    int nudge;
    bool seq_plays = false;
    if (frac <= 0.5) nudge = (int) (frac * 48.0 + 0.5);
    else { k = (k + 1) % len; nudge = -(int) ((1.0 - frac) * 48.0 + 0.5); seq_plays = true; }
    step_t *st = &p->step[t][k];
    st->on = 1;
    st->vel = (uint8_t) CLAMP((int) (vel * 127.0f + 0.5f), 1, 127);
    st->flags = vel >= 0.99f ? STEP_ACCENT : 0;
    st->nudge = (int8_t) CLAMP(nudge, -23, 23);
    st->chance = 100; st->cond = 0; st->ratchet = 0;
    doc_touch(st, sizeof *st);
    p->used = 1; doc_touch(&p->used, 1);
    return seq_plays;
}

void pads_button(int b, bool down)
{
    if (b == BTN_SELECT && down) {
        ui.rec = !ui.rec;
        if (ui.rec) { ui.pat = ui.st.playing ? ui.st.pat : ui.pat; undo_save(); }
        ui_toast(ui.rec ? "REC ON" : "REC OFF");
        return;
    }
    if (ui.pads_b && b == BTN_UP) { ui_msg(M_FILL, down, 0, 0, 0); return; }
    const int t = pad_track(b);
    if (t < 0 || !down) return;
    const float vel = !ui.held[BTN_R2] ? 100.0f / 127.0f : ui.prefs.pad_accent ? 1.0f : 50.0f / 127.0f;   // R2: an accent, or a ghost
    bool seq = false;
    if (ui.rec && ui.st.playing) seq = record(t, vel);
    if (!seq) ui_msg(M_HIT, t, 0, 0, vel);
}

static void cluster(int cx, int cy, const pad_t *pads, int n)
{
    for (int i = 0; i < n; i++) {
        const pad_t *p = &pads[i];
        const col_t col = trk_col[p->trk];
        const int x0 = cx + p->dx, y0 = cy + p->dy, w = 84, h = 54;
        const bool lit = ui.held[p->btn] || ui.now < ui.flash[p->trk];
        if (lit) g_rframe(x0 - 4, y0 - 4, x0 + w + 3, y0 + h + 3, 9, col, 2);
        g_rrect(x0, y0, x0 + w - 1, y0 + h - 1, 7, lit ? c_mix(col, C_WHITE, 0.45f) : c_mix(C_BG, col, 0.28f));
        g_rframe(x0, y0, x0 + w - 1, y0 + h - 1, 7, col, 1);
        g_text(x0 + 6, y0 + 4, p->lab, &f12b, lit ? C_INK : c_scale(col, 0.9f));
        g_text_c(x0 + w / 2, y0 + 20, doc.trk[p->trk].name, &f16, lit ? C_INK : C_TEXT);
        char b[4]; k_snprintf(b, sizeof b, "%02d", p->trk + 1);
        g_text_c(x0 + w / 2, y0 + 38, b, &f12, lit ? C_INK : C_DIM);
    }
}

void draw_pads(void)
{
    g_noclip();
    g_clear(C_BG);
    w_header("PADS", nav_col(NAV_PATTERN));
    const int pn = ui.st.playing ? ui.st.pat : ui.pat;
    const pattern_t *p = &doc.pat[CLAMP(pn, 0, NPAT - 1)];
    const int cell_w = 22, x0 = 104;
    // the pattern, small, filling up as you play
    for (int t = 0; t < NTRK; t++) {
        const int y = 38 + t * 13;
        const col_t col = trk_col[t];
        g_text(8, y, doc.trk[t].name, &f12, doc.trk[t].mute ? C_DIM : col);
        const int tlen = CLAMP(p->trk[t].len, 1, NSTEP), cur = ui.st.playing ? ui.st.step[t] : -1;
        for (int i = 0; i < 16; i++) {
            const int x = x0 + i * 26 + (i / 4) * 6;
            const step_t *s = &p->step[t][i];
            col_t c = i >= tlen ? RGB(16, 16, 22) : s->on ? c_scale(col, 0.55f + 0.4f * (float) s->vel / 127.0f) : c_mix(C_BG, col, i % 4 ? 0.12f : 0.2f);
            if (i == cur && s->on && ui.now < ui.flash[t]) c = c_mix(col, C_WHITE, 0.55f);
            g_rect(x, y + 1, x + cell_w - 1, y + 10, c);
            if (i == cur) g_frame(x - 1, y, x + cell_w, y + 11, s->on ? C_WHITE : C_DIMMER, 1);
        }
    }
    int x = 8;
    if (ui.rec) x += w_chip(8, 202, "● REC", &f12b, C_WHITE, C_RED, 5, 16) + 4;
    else x += w_chip(8, 202, "REC OFF", &f12b, C_DIM, C_PANEL2, 5, 16) + 4;
    const char *const info[] = { "TIMING AS PLAYED", ui.prefs.pad_accent ? "R2 = ACCENT" : "R2 = GHOST", "SELECT = REC" };
    x = w_chips(x, 202, info, COUNT(info), -1, nav_col(NAV_PATTERN), 16, 5, 4);
    g_text_c(SCR_W / 2, 228, ui.pads_b ? "L2 HELD  ·  TRACKS 9-12" : "L1 HELD  ·  TRACKS 1-8", &f12b, C_TEXT);
    if (!ui.pads_b) { cluster(170, 336, bank_a, 4); cluster(470, 336, bank_a + 4, 4); }
    else {
        cluster(470, 336, bank_b, 4);
        const bool fill = ui.held[BTN_UP];
        g_rrect(128, 246, 211, 299, 7, fill ? C_AMBER : c_mix(C_BG, C_AMBER, 0.25f));
        g_rframe(128, 246, 211, 299, 7, C_AMBER, 1);
        g_text(134, 250, "↑", &f12b, fill ? C_INK : C_AMBER);
        g_text_c(170, 266, "FILL", &f16, fill ? C_INK : C_TEXT);
        g_text_c(170, 284, "hold", &f12, fill ? C_INK : C_DIM);
    }
    g_text_c(SCR_W / 2, 440, ui.pads_b ? "L1: TRACKS 1-8" : "L2: TRACKS 9-12 AND FILL", &f12, C_DIM);
    const char *const hints[] = { "L1", "hold", "D-PAD ABXY", "play", "R2", ui.prefs.pad_accent ? "accent" : "ghost", "SEL", "rec on/off", "START", "play" };
    w_footer(hints, COUNT(hints));
}
