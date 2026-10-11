// PATTERN: twelve tracks by sixteen steps, the selected track's sound underneath (docs/mockups/screens/03, 04),
// and STEP, the overlay X opens on a step (05).
#include "ui_int.h"

#define GRID_Y 48
#define ROW_H 26
#define CELL 22
#define GRID_X 104
#define PITCH 26
#define GAP 6
#define PANEL_Y 364

static int cell_x(int i) { return GRID_X + i * PITCH + (i / 4) * GAP; }
static int row_y(int r) { return GRID_Y + r * ROW_H; }

static bool step_locked(const pattern_t *p, int t, int s)
{
    for (int i = 0; i < p->nlock; i++) if (p->lock[i].trk == t && p->lock[i].step == s) return true;
    return false;
}

// One step, as the mockups draw it: solid (brighter when louder), a top bar for accent, an outline for a ghost,
// a checkerboard for chance, split bars for a ratchet, a sliver for a flam, a white corner for locks, a slice's number.
static void draw_cell(int x, int y, const step_t *s, bool locked, bool slice_num, col_t col, bool beat, bool hot, bool muted, bool outside, bool sel, int ch)
{
    const col_t c = muted ? c_grey(col) : col;
    if (outside) {
        g_rect(x, y, x + CELL - 1, y + ch - 1, RGB(14, 14, 20));
        for (int k = 0; k < CELL + ch; k += 5)
            g_line(x + MAX(0, k - ch + 1), y + MIN(ch - 1, k), x + MIN(CELL - 1, k), y + MAX(0, k - CELL + 1), RGB(24, 24, 32), 1);
        return;
    }
    const col_t empty = c_mix(C_BG, c, sel ? (beat ? 0.32f : 0.24f) : (beat ? 0.20f : 0.12f));
    const int rr = ch > 12 ? 3 : 1;
    if (!s || !s->on) { g_rrect(x, y, x + CELL - 1, y + ch - 1, rr, empty); return; }
    const bool accent = s->flags & STEP_ACCENT, ghost = !accent && s->vel < 64;
    col_t fill = c_scale(c, accent ? 1.0f : 0.55f + 0.4f * (float) s->vel / 127.0f);
    if (hot) {
        fill = c_mix(c, C_WHITE, 0.55f);
        g_rframe(x - 2, y - 2, x + CELL + 1, y + ch + 1, rr + 1, c, 1);
    }
    if (ghost) {
        g_rrect(x, y, x + CELL - 1, y + ch - 1, rr, c_mix(C_BG, c, 0.22f));
        g_rframe(x, y, x + CELL - 1, y + ch - 1, rr, hot ? fill : c_scale(c, 0.85f), 1);
    } else if (s->chance < 100) {
        g_rrect(x, y, x + CELL - 1, y + ch - 1, rr, empty);
        g_checker(x + 2, y + 2, x + CELL - 3, y + ch - 3, fill);
    } else if (step_hits(s) > 1) {
        const int n = MIN(step_hits(s), 4), w = (CELL - 2 * (n - 1)) / n;
        for (int k = 0; k < n; k++) g_rect(x + k * (w + 2), y, x + k * (w + 2) + w - 1, y + ch - 1, fill);
    } else if (s->flags & STEP_FLAM) {
        g_rrect(x, y, x + CELL - 1, y + ch - 1, rr, empty);
        g_rect(x, y + 3, x + 3, y + ch - 4, c_scale(c, 0.55f));
        g_rrect(x + 6, y, x + CELL - 1, y + ch - 1, rr, fill);
    } else {
        g_rrect(x, y, x + CELL - 1, y + ch - 1, rr, fill);
        if (accent && ch > 12) g_rect(x + 2, y + 1, x + CELL - 3, y + 3, c_mix(c, C_WHITE, 0.7f));
    }
    if (ch > 12) {
        if (locked) g_tri(x + CELL - 8, y, x + CELL - 1, y, x + CELL - 1, y + 7, C_WHITE);
        if (slice_num && s->note > 0) {
            char b[4]; k_snprintf(b, sizeof b, "%d", s->note);
            g_text(x + (CELL - g_tw(b, &f14)) / 2, y + 4, b, &f14, RGB(12, 6, 14));
        }
        if (s->cond != COND_ALWAYS) g_rect(x + 1, y + ch - 4, x + 3, y + ch - 2, C_AMBER);
    }
}

static void header_main(void)
{
    const pattern_t *p = cur_pat();
    g_rect(0, 0, SCR_W - 1, 29, C_PANEL);
    char b[24];
    k_snprintf(b, sizeof b, "%c%02d", 'A' + ui.pat / NPERBANK, ui.pat % NPERBANK + 1);
    w_chip(6, 5, b, &f16, C_TEXT, C_PANEL2, 4, 20);
    if (ui.st.playing) g_tri(50, 8, 50, 22, 61, 15, C_GREEN); else g_rect(51, 9, 60, 20, C_DIM);
    k_snprintf(b, sizeof b, "%.1f", (double) (ui.st.bpm > 0 ? ui.st.bpm : doc.bpm));
    g_text(68, 5, b, &f20, C_TEXT);
    g_text(68 + g_tw(b, &f20) + 2, 12, "BPM", &f12, C_DIM);
    char sw[8], bar[12];
    k_snprintf(sw, sizeof sw, "%d%%", p->swing);
    k_snprintf(bar, sizeof bar, "%02d.%02d", (int) ui.st.bar % 100 + 1, (int) (ui.st.beat * 4) + 1);
    const struct { int x; const char *lab, *val; } f[] = { { 150, "SWING", sw }, { 192, "BAR", bar }, { 240, "KIT", doc.kit } };
    for (int i = 0; i < COUNT(f); i++) { g_text(f[i].x, 1, f[i].lab, &f12, C_DIM); g_text(f[i].x, 14, f[i].val, &f14, C_TEXT); }
    const int bank = ui.pat / NPERBANK;
    k_snprintf(b, sizeof b, "BANK %c", 'A' + bank);
    g_text(322, 1, b, &f12, C_DIM);
    for (int k = 0; k < NPERBANK; k++) {
        const int n = bank * NPERBANK + k, x = 322 + k * 8, y = 16;
        const bool playing = ui.st.playing && ui.st.pat == n, queued = ui.st.next_pat == n;
        if (playing) g_rect(x, y, x + 6, y + 10, C_WHITE);
        else if (doc.pat[n].used) g_rect(x, y, x + 6, y + 10, n == ui.pat ? C_DIM : C_DIMMER);
        else g_frame(x, y, x + 6, y + 10, C_LINE, 1);
        if (queued) g_frame(x - 1, y - 1, x + 7, y + 11, C_AMBER, 1);
        if (n == ui.pat && !playing) g_rect(x, y + 12, x + 6, y + 12, C_TEXT);
    }
    w_status();
    w_rainbow(30);
}

static void grid(void)
{
    const pattern_t *p = cur_pat();
    const int len = pattern_len(p), pages = (len + 15) / 16, page = ui.cur_s >= 0 ? ui.cur_s / 16 : ui.page;
    ui.page = page;
    char b[16];
    k_snprintf(b, sizeof b, "PAGE %d/%d", page + 1, pages);
    g_text(8, 34, b, &f12, pages > 1 ? C_DIM : C_DIMMER);
    const bool here = ui.st.playing && ui.st.pat == ui.pat;
    const int master = here ? (int) (ui.st.beat * 4) % MAX(p->len, 1) : -1;
    const int mcol = master >= page * 16 && master < page * 16 + 16 ? master - page * 16 : -1;
    for (int i = 0; i < 16; i++) {
        const int x = cell_x(i);
        if (i == mcol) g_rect(x - 2, 33, x + CELL + 1, 46, C_PANEL2);
        if (i % 4 == 0 || i == mcol) {
            k_snprintf(b, sizeof b, "%d", page * 16 + i + 1);
            g_text(x + (CELL - g_tw(b, &f12)) / 2, 34, b, &f12, i == mcol ? C_TEXT : C_DIM);
        } else g_rect(x + CELL / 2 - 1, 39, x + CELL / 2, 40, C_DIMMER);
    }
    g_text(546, 34, "LEVEL", &f12, C_DIMMER);
    g_text(596, 34, "LEN", &f12, C_DIMMER);
    if (mcol >= 0) { const int x = cell_x(mcol); g_rect(x - 3, GRID_Y - 2, x + CELL + 2, row_y(NTRK) - 3, c_mix(C_BG, C_WHITE, 0.07f)); }
    bool solo = false;
    for (int t = 0; t < NTRK; t++) if (doc.trk[t].solo) solo = true;
    for (int t = 0; t < NTRK; t++) {
        const track_t *tr = &doc.trk[t];
        const int y = row_y(t);
        const bool muted = tr->mute || (solo && !tr->solo);
        const col_t col = trk_col[t], c = muted ? c_grey(col) : col;
        const bool sel = t == ui.cur_t;
        if (sel) {
            g_rect(0, y - 2, SCR_W - 1, y + CELL + 1, c_mix(C_BG, col, 0.07f));
            if (mcol >= 0) { const int x = cell_x(mcol); g_rect(x - 3, y - 2, x + CELL + 2, y + CELL + 1, c_mix(c_mix(C_BG, col, 0.07f), C_WHITE, 0.07f)); }
        }
        g_rect(4, y, 7, y + CELL - 1, c);
        g_text(12, y + 4, tr->name, &f14, muted ? C_DIM : c);
        w_chip(74, y + 4, tr->engine == ENG_SYNTH ? "SYN" : "SMP", &f12, muted ? C_DIMMER : c_scale(c, 0.9f), c_mix(C_BG, c, 0.18f), 2, 14);
        const int tlen = CLAMP(p->trk[t].len, 1, NSTEP);
        const int cur = here ? ui.st.step[t] : -1;
        for (int i = 0; i < 16; i++) {
            const int si = page * 16 + i;
            const step_t *s = &p->step[t][si];
            const bool hot = si == cur && s->on && !muted && ui.now < ui.flash[t];
            draw_cell(cell_x(i), y, s, step_locked(p, t, si), tr->engine == ENG_SAMPLE, col, i % 4 == 0, hot, muted, si >= tlen, sel, CELL);
            if (si == cur && !hot && !(s->on)) g_rframe(cell_x(i) - 1, y - 1, cell_x(i) + CELL, y + CELL, 3, c_scale(c, 0.6f), 1);
        }
        const float lvl = muted ? 0 : MIN(ui.st.level[t] * 1.4f, 1.0f);
        g_rect(546, y + 8, 586, y + 13, RGB(26, 26, 36));
        if (lvl > 0.01f) g_rect(546, y + 8, 546 + (int) (40 * lvl), y + 13, c);
        k_snprintf(b, sizeof b, "%d", tlen);
        g_text(596, y + 4, b, &f14, tlen != 16 ? C_TEXT : C_DIMMER);
        if (tr->mute) w_chip(614, y + 4, "M", &f12, RGB(20, 12, 0), C_AMBER, 3, 14);
        else if (tr->solo) w_chip(614, y + 4, "S", &f12, RGB(0, 20, 12), C_GREEN, 3, 14);
        else if (p->trk[t].speed != SPD_1) {
            static const char *const sp[SPD_COUNT] = { "⅛", "¼", "½", "¾", "", "×1½", "×2" };
            (void) sp;
            const char *names[SPD_COUNT] = { "/8", "/4", "/2", ".75", "", "1.5", "×2" };
            g_text(614, y + 4, names[p->trk[t].speed], &f12b, C_AMBER);
        }
    }
    // the cursor
    const int y = row_y(ui.cur_t);
    if (ui.cur_s < 0) g_frame(1, y - 3, 100, y + CELL + 2, C_WHITE, 2);
    else { const int x = cell_x(ui.cur_s % 16); g_frame(x - 4, y - 4, x + CELL + 3, y + CELL + 3, C_WHITE, 2); }
}

static void panel_frame(col_t c)
{
    g_rect(0, PANEL_Y, SCR_W - 1, 457, C_PANEL);
    g_rect(0, PANEL_Y, SCR_W - 1, PANEL_Y + 1, c);
}

static void panel_synth(int t)
{
    const track_t *tr = &doc.trk[t];
    const col_t col = trk_col[t];
    panel_frame(col);
    char b[32];
    k_snprintf(b, sizeof b, "%02d", t + 1);
    g_text(10, 372, b, &f14, C_DIM);
    g_text(34, 368, tr->name, &f24, col);
    const int type = CLAMP(m_lround(tr->synth[P_TYPE]), 0, DT_COUNT - 1);
    int x = 10 + w_chip(10, 398, "SYNTH", &f12, RGB(14, 8, 8), col, 3, 14) + 4;
    k_snprintf(b, sizeof b, "%s MODEL", drum_type_name(type));
    for (char *c = b; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    w_chip(x, 398, b, &f12, col, c_mix(C_BG, col, 0.2f), 3, 14);
    k_strcpy(b, tr->sound, sizeof b);
    for (char *c = b; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    b[18] = 0;
    g_text(10, 416, b, &f16, C_TEXT);
    char ln[40];
    k_snprintf(ln, sizeof ln, "CHOKE %c  VEL %d%%", tr->choke ? 'A' + tr->choke - 1 : '-', (int) (tr->synth[P_VELSENS] * 100 + 0.5f));
    g_text(10, 436, ln, &f12, C_DIM);
    const float *v = tr->synth;
    const struct { const char *lab; int p; bool bip; } m[] = { { "TUNE", P_TUNE, true }, { "PUNCH", P_PUNCH, false }, { "LENGTH", P_LENGTH, false }, { "TONE", P_TONE, true }, { "GRIT", P_GRIT, false } };
    for (int k = 0; k < COUNT(m); k++) {
        const int y = 372 + k * 16;
        const float n = drum_to_norm(m[k].p, v[m[k].p]);
        g_text(192, y - 3, m[k].lab, &f12, C_DIM);
        w_bar(238, y, 150, m[k].bip ? n * 2 - 1 : n, col, m[k].bip, 8);
        if (m[k].p == P_TUNE) k_snprintf(b, sizeof b, "%+d", m_lround(v[P_TUNE]));
        else if (m[k].p == P_LENGTH) k_snprintf(b, sizeof b, "x%.2f", (double) v[P_LENGTH]);
        else if (m[k].p == P_TONE) k_snprintf(b, sizeof b, "%+d", m_lround(v[P_TONE] * 100));
        else k_snprintf(b, sizeof b, "%d", m_lround(v[m[k].p] * 100));
        g_text(394, y - 3, b, &f12b, C_TEXT);
    }
    // the drum's shape
    const int bx0 = 428, by0 = 370, bx1 = 632, by1 = 418, cols = bx1 - bx0 - 2, mid = (by0 + by1) / 2;
    g_rect(bx0, by0, bx1, by1, RGB(12, 12, 20));
    g_frame(bx0, by0, bx1, by1, C_LINE, 1);
    static float pk[480];
    float secs;
    ui_preview(t, pk, cols, &secs);
    for (int c = 0; c < cols; c++) {
        const int lo = m_lround(pk[2 * c] * 21), hi = m_lround(pk[2 * c + 1] * 21);
        g_line(bx0 + 1 + c, mid - hi, bx0 + 1 + c, mid - lo, c_scale(col, 0.32f), 1);
        g_pixel(bx0 + 1 + c, mid - hi, col);
        g_pixel(bx0 + 1 + c, mid - lo, col);
    }
    k_snprintf(b, sizeof b, "%.2f s", (double) secs);
    g_text(bx1 - g_tw(b, &f12) - 4, by0 + 2, b, &f12, C_DIM);
    const bool live = m_fabs(ui.lx) > 0.12f || m_fabs(ui.ly) > 0.12f;
    w_stick(444, 440, ui.lx, ui.ly, col, "L STICK", ui.held[BTN_A] && ui.cur_s >= 0 ? "LOCK STEP" : "TUNE/LEN", live ? C_AMBER : col, 12);
    w_stick(540, 440, ui.rx, ui.ry, C_WHITE, "R STICK", "FX: LATER", C_DIM, 12);
}

static float smp_peaks[NTRK][2 * 440];
static const void *smp_peaks_key[NTRK];

static void panel_sample(int t)
{
    const track_t *tr = &doc.trk[t];
    const col_t col = trk_col[t];
    const pattern_t *p = cur_pat();
    panel_frame(col);
    char b[48];
    k_snprintf(b, sizeof b, "%02d", t + 1);
    g_text(10, 372, b, &f14, C_DIM);
    g_text(34, 368, tr->name, &f24, col);
    int x = 10 + w_chip(10, 398, "SAMPLE", &f12, RGB(14, 8, 14), col, 3, 14) + 4;
    if (tr->smp.slices) k_snprintf(b, sizeof b, "SLICE %d", tr->smp.slices); else k_strcpy(b, "WHOLE", sizeof b);
    w_chip(x, 398, b, &f12, col, c_mix(C_BG, col, 0.2f), 3, 14);
    k_strcpy(b, tr->sound, 18);
    g_text(10, 416, b, &f16, C_TEXT);
    const sample_t *s = ui_sample(t);
    if (s) k_snprintf(b, sizeof b, "%.2f s  %dk  %s", (double) s->frames / s->rate, (int) (s->rate / 1000), s->chans > 1 ? "STEREO" : "MONO");
    else k_strcpy(b, "NOT ON THE CARD", sizeof b);
    g_text(10, 436, b, &f12, s ? C_DIM : C_RED);
    // the waveform, its slices, the one playing lit
    const int bx0 = 192, by0 = 368, bx1 = 632, by1 = 424, w = bx1 - bx0 - 2;
    g_rect(bx0, by0, bx1, by1, RGB(12, 12, 20));
    g_frame(bx0, by0, bx1, by1, C_LINE, 1);
    const int n = tr->smp.slices;
    int playing = -1;
    if (ui.st.playing && ui.st.pat == ui.pat) {
        const step_t *st = &p->step[t][ui.st.step[t]];
        if (st->on && n) playing = (st->note > 0 ? st->note : 1) - 1;
    }
    int sx[NSLICE + 1];
    for (int k = 0; k <= MAX(n, 1); k++) {
        float a, bb;
        smp_slice_range(&tr->smp, k == MAX(n, 1) ? MAX(n, 1) : k + 1, &a, &bb);
        sx[k] = bx0 + 1 + (int) ((k == MAX(n, 1) ? bb : a) * (float) w);
    }
    if (playing >= 0 && playing < n) g_rect(sx[playing], by0 + 1, sx[playing + 1], by1 - 1, c_mix(C_BG, col, 0.22f));
    if (s) {
        if (smp_peaks_key[t] != (const void *) s->data) {
            for (int c = 0; c < w; c++) {
                const uint32_t a = (uint32_t) ((uint64_t) c * s->frames / (uint32_t) w), e = (uint32_t) ((uint64_t) (c + 1) * s->frames / (uint32_t) w);
                int hi = 0;
                for (uint32_t i = a; i < e; i++) { const int v = s->data[i * s->chans]; hi = MAX(hi, v < 0 ? -v : v); }
                smp_peaks[t][c] = (float) hi / 32768.0f;
            }
            smp_peaks_key[t] = s->data;
        }
        const int mid = (by0 + by1) / 2 + 4, amp = (by1 - by0) / 2 - 8;
        for (int c = 0; c < w; c++) {
            const int h = (int) (smp_peaks[t][c] * (float) amp);
            const int xx = bx0 + 1 + c;
            const bool lit = playing >= 0 && xx >= sx[playing] && xx < sx[playing + 1];
            g_line(xx, mid - h, xx, mid + h, lit ? c_mix(col, C_WHITE, 0.35f) : c_scale(col, 0.85f), 1);
        }
    }
    for (int k = 0; k < n; k++) {
        g_line(sx[k], by0 + 1, sx[k], by1 - 1, k == playing ? C_WHITE : C_DIMMER, 1);
        k_snprintf(b, sizeof b, "%d", k + 1);
        w_chip(sx[k] + 1, by0 + 1, b, &f12, k == playing ? C_INK : C_TEXT, k == playing ? C_WHITE : C_PANEL2, 2, 12);
    }
    x = 192;
    char pv[6][8];
    k_snprintf(pv[0], 8, "%+d", m_lround(tr->smp.pitch));
    if (tr->smp.decay > 0) k_snprintf(pv[1], 8, "%d", m_lround(tr->smp.decay)); else k_strcpy(pv[1], "END", 8);
    k_snprintf(pv[2], 8, "%d", m_lround(tr->smp.bits));
    k_snprintf(pv[3], 8, "%dk", m_lround(tr->smp.rate / 1000));
    k_strcpy(pv[4], tr->smp.mode == SMP_LOOP ? "ON" : "OFF", 8);
    k_strcpy(pv[5], tr->smp.reverse ? "ON" : "OFF", 8);
    const char *labs[6] = { "PITCH", "DECAY", "BITS", "RATE", "LOOP", "REV" };
    for (int k = 0; k < 6; k++) {
        const int lw = g_tw(labs[k], &f12) + g_tw(pv[k], &f12b) + 14;
        g_rrect(x, 432, x + lw - 1, 450, 2, C_PANEL2);
        g_text(x + 4, 436, labs[k], &f12, C_DIM);
        const bool grit = (k == 2 && tr->smp.bits < 15.9f) || (k == 3 && tr->smp.rate < 47900);
        g_text(x + 8 + g_tw(labs[k], &f12), 436, pv[k], &f12b, grit ? C_AMBER : C_TEXT);
        x += lw + 6;
    }
}

void draw_pattern(void)
{
    g_noclip();
    g_clear(C_BG);
    header_main();
    grid();
    if (doc.trk[ui.cur_t].engine == ENG_SYNTH) panel_synth(ui.cur_t); else panel_sample(ui.cur_t);
    static const char *const base[] = { "A", "hit", "A+↑↓", "velocity", "X", "step", "Y", "listen", "L1", "pads", "R2", "edit", "SEL", "menu", "START", "play" };
    static const char *const held_a[] = { "↑↓", "velocity", "←→", "nudge", "L STICK", "lock tune/length", "A", "let go: done" };
    static const char *const held_r2[] = { "A", "copy row", "X", "paste", "Y", "clear row", "B", "undo", "←→", "page", "↑↓", "pattern" };
    static const char *const held_y[] = { "←→", "another sound", "Y", "let go: done" };
    if (ui.held[BTN_A] && ui.cur_s >= 0) w_footer(held_a, COUNT(held_a));
    else if (ui.held[BTN_R2]) w_footer(held_r2, COUNT(held_r2));
    else if (ui.held[BTN_Y]) w_footer(held_y, COUNT(held_y));
    else w_footer(base, COUNT(base));
}

// ------------------------------------------------------------------------------------------- editing

static step_t *cur_step(void) { return ui.cur_s >= 0 ? &cur_pat()->step[ui.cur_t][ui.cur_s] : NULL; }

static void touch_step(const step_t *s) { doc_touch(s, sizeof *s); cur_pat()->used = 1; doc_touch(&cur_pat()->used, 1); }

static void audition(int t) { if (!ui.st.playing) ui_msg(M_HIT, t, 0, 0, 0.8f); }

void lock_set(pattern_t *p, int t, int s, int param, float v)
{
    int i = lock_find(p, t, s, param);
    if (i < 0) {
        if (p->nlock >= NLOCK) { ui_toast("LOCKS FULL"); return; }
        i = p->nlock++;
        p->lock[i] = (lock_t) { (uint8_t) t, (uint8_t) s, (uint8_t) param, 0, v };
        doc_touch(&p->nlock, sizeof p->nlock);
    } else p->lock[i].val = v;
    doc_touch(&p->lock[i], sizeof p->lock[i]);
}

static void clear_locks(pattern_t *p, int t, int s)
{
    int k = 0;
    for (int i = 0; i < p->nlock; i++) if (!(p->lock[i].trk == t && p->lock[i].step == s)) p->lock[k++] = p->lock[i];
    if (k != p->nlock) { p->nlock = (uint16_t) k; doc_touch(&p->nlock, sizeof p->nlock); doc_touch(p->lock, sizeof p->lock); }
}

static void next_sound(int t, int dir)
{
    track_t *tr = &doc.trk[t];
    if (tr->engine != ENG_SYNTH) { ui_toast("SAMPLES: BROWSER COMES NEXT"); return; }
    int i = drum_preset_find(tr->sound);
    i = ((i < 0 ? 0 : i) + dir + drum_preset_count) % drum_preset_count;
    const uint8_t choke = tr->choke;
    char name[8];
    k_strcpy(name, tr->name, sizeof name);
    const track_t keep = *tr;
    track_from_preset(tr, i, name);
    tr->choke = choke; tr->level = keep.level; tr->pan = keep.pan; tr->send_dly = keep.send_dly; tr->send_rev = keep.send_rev;
    tr->mute = keep.mute; tr->solo = keep.solo; tr->send = keep.send; tr->midi_ch = keep.midi_ch; tr->midi_note = keep.midi_note;
    doc_touch(tr, sizeof *tr);
    audition(t);
    ui_msg(M_HIT, t, 0, 0, 0.8f);
}

void pattern_button(int b, bool down, bool rep)
{
    pattern_t *p = cur_pat();
    step_t *s = cur_step();
    const int len = pattern_len(p);
    // the edit layer
    if (ui.held[BTN_R2] && down && b != BTN_R2) {
        ui.used[BTN_R2] = true;
        switch (b) {
        case BTN_A: m_copy(ui.clip, p->step[ui.cur_t], sizeof ui.clip); ui.clip_trk = p->trk[ui.cur_t]; ui.clip_ok = true; ui_toast("ROW COPIED"); return;
        case BTN_X:
            if (!ui.clip_ok) return;
            undo_save();
            m_copy(p->step[ui.cur_t], ui.clip, sizeof ui.clip); p->trk[ui.cur_t] = ui.clip_trk;
            doc_touch(p->step[ui.cur_t], sizeof p->step[ui.cur_t]); doc_touch(&p->trk[ui.cur_t], sizeof p->trk[ui.cur_t]);
            p->used = 1; doc_touch(&p->used, 1);
            ui_toast("PASTED"); return;
        case BTN_Y:
            undo_save();
            for (int i = 0; i < NSTEP; i++) { p->step[ui.cur_t][i] = (step_t) { 0, 100, 0, 0, 100, 0, 0, 0 }; }
            doc_touch(p->step[ui.cur_t], sizeof p->step[ui.cur_t]);
            ui_toast("ROW CLEARED"); return;
        case BTN_B:
            if (!ui.undo_ok || ui.undo_pat != ui.pat) { ui_toast("NOTHING TO UNDO"); return; }
            { const pattern_t now = *p; *p = ui.undo; ui.undo = now; }
            doc_touch(p, sizeof *p);
            ui_toast("UNDONE"); return;
        case BTN_LEFT: case BTN_RIGHT: {
            const int pages = (len + 15) / 16;
            const int pg = CLAMP(MAX(ui.cur_s, 0) / 16 + (b == BTN_RIGHT ? 1 : -1), 0, pages - 1);
            ui.cur_s = pg * 16 + MAX(ui.cur_s, 0) % 16;
            return;
        }
        case BTN_UP: case BTN_DOWN:
            ui.pat = CLAMP(ui.pat + (b == BTN_DOWN ? 1 : -1), 0, NPAT - 1);
            if (ui.st.playing) ui_msg(M_QUEUE, ui.pat, 0, 0, 0);
            else ui_msg(M_QUEUE, ui.pat, 1, 0, 0);
            return;
        default: return;
        }
    }
    switch (b) {
    case BTN_UP: case BTN_DOWN: {
        if (!down) return;
        const int d = b == BTN_UP ? 1 : -1;
        if (ui.held[BTN_A] && s) {
            ui.used[BTN_A] = true;
            if (!s->on) { s->on = 1; s->vel = 100; }
            s->vel = (uint8_t) CLAMP(s->vel + d * 8, 1, 127);
            s->flags &= (uint8_t) ~STEP_ACCENT;
            if (s->vel == 127) s->flags |= STEP_ACCENT;
            touch_step(s);
            return;
        }
        if (ui.held[BTN_Y]) { ui.used[BTN_Y] = true; return; }
        ui.cur_t = CLAMP(ui.cur_t - d, 0, NTRK - 1);
        return;
    }
    case BTN_LEFT: case BTN_RIGHT: {
        if (!down) return;
        const int d = b == BTN_RIGHT ? 1 : -1;
        if (ui.held[BTN_A] && s) {
            ui.used[BTN_A] = true;
            if (!s->on) { s->on = 1; s->vel = 100; }
            s->nudge = (int8_t) CLAMP(s->nudge + d, -23, 23);
            touch_step(s);
            return;
        }
        if (ui.held[BTN_Y]) { ui.used[BTN_Y] = true; next_sound(ui.cur_t, d); return; }
        ui.cur_s = CLAMP(ui.cur_s + d, -1, len - 1);
        (void) rep;
        return;
    }
    case BTN_A:
        if (down) {
            if (!s) { doc.trk[ui.cur_t].mute ^= 1; doc_touch(&doc.trk[ui.cur_t].mute, 1); return; }
            if (s->on) ui.a_off = true;
            else {
                undo_save();
                s->on = 1; if (!s->vel) s->vel = 100; if (!s->chance) s->chance = 100;
                touch_step(s);
                audition(ui.cur_t);
                ui.a_off = false;
            }
        } else {
            if (ui.a_off && !ui.used[BTN_A] && s) { undo_save(); s->on = 0; touch_step(s); }
            ui.a_off = false;
        }
        return;
    case BTN_X:
        if (down && s) { ui.overlay = OV_STEP; ui.step_row = 0; }
        else if (down) go(SCR_GENERATE);
        return;
    case BTN_Y:
        if (!down && !ui.used[BTN_Y]) ui_msg(M_HIT, ui.cur_t, 0, 0, 0.8f);
        return;
    case BTN_B:
        if (down && !s) { doc.trk[ui.cur_t].solo ^= 1; doc_touch(&doc.trk[ui.cur_t].solo, 1); }
        return;
    default: return;
    }
}

// The left stick: two of the selected track's macros (SETTINGS chooses them: tune across and length up, at first),
// live; with A held on a step, locked into that step. Let go, they spring back, or stay where they were put.
static float stick_value(const track_t *tr, int p, float axis)
{
    const drum_param_t *d = &drum_params[p];
    if (p == P_TUNE) return m_clampf(tr->synth[p] + axis * 12.0f, d->min, d->max);
    if (p == P_LENGTH) return m_clampf(tr->synth[p] * m_exp2(axis), d->min, d->max);
    return m_clampf(tr->synth[p] + axis * (d->max - d->min) * 0.5f, d->min, d->max);
}

void pattern_sticks(void)
{
    static float sent_x = 1e9f, sent_y = 1e9f;
    static int live_t = -1;
    const int t = ui.cur_t;
    track_t *tr = &doc.trk[t];
    const int px = CLAMP(ui.prefs.l_x, 0, DRUM_NP - 1), py = CLAMP(ui.prefs.l_y, 0, DRUM_NP - 1);
    const bool active = m_fabs(ui.lx) > 0.12f || m_fabs(ui.ly) > 0.12f;
    if (tr->engine != ENG_SYNTH) return;
    const float vx = stick_value(tr, px, ui.lx), vy = stick_value(tr, py, -ui.ly);
    if (active && ui.held[BTN_A] && ui.cur_s >= 0) {
        pattern_t *p = cur_pat();
        step_t *s = &p->step[t][ui.cur_s];
        ui.used[BTN_A] = true;
        if (!s->on) { s->on = 1; s->vel = 100; touch_step(s); }
        if (m_fabs(ui.lx) > 0.12f) lock_set(p, t, ui.cur_s, px, px == P_TUNE ? m_round(vx) : vx);
        if (m_fabs(ui.ly) > 0.12f) lock_set(p, t, ui.cur_s, py, py == P_TUNE ? m_round(vy) : vy);
        return;
    }
    if (active) {
        if (m_fabs(vx - sent_x) > 0.01f) { ui_msg(M_LIVE, t, px, 0, vx); sent_x = vx; }
        if (m_fabs(vy - sent_y) > 0.01f) { ui_msg(M_LIVE, t, py, 0, vy); sent_y = vy; }
        ui.l_live = true; live_t = t;
    } else if (ui.l_live) {
        if (ui.prefs.stick_return && live_t >= 0) {   // it stays: the track keeps the value
            track_t *lt = &doc.trk[live_t];
            if (sent_x < 1e8f) lt->synth[px] = px == P_TUNE ? m_round(sent_x) : sent_x;
            if (sent_y < 1e8f) lt->synth[py] = py == P_TUNE ? m_round(sent_y) : sent_y;
            doc_touch(lt->synth, sizeof lt->synth);
        }
        for (int k = 0; k < NTRK; k++) { ui_msg(M_LIVE, k, px, 1, 0); ui_msg(M_LIVE, k, py, 1, 0); }
        ui.l_live = false; sent_x = sent_y = 1e9f; live_t = -1;
    }
}

// ------------------------------------------------------------------------------------------- STEP

enum { SR_VEL, SR_NUDGE, SR_RATCHET, SR_SHAPE, SR_CHANCE, SR_COND, SR_NOTE, SR_ACCENT, SR_FLAM, SR_COUNT };

void draw_step(void)
{
    const pattern_t *p = cur_pat();
    const int t = ui.cur_t, si = MAX(ui.cur_s, 0);
    const step_t *s = &p->step[t][si];
    const col_t col = trk_col[t];
    const track_t *tr = &doc.trk[t];
    const bool top = ui.cur_t >= 7;
    const int y0 = top ? 36 : 236, y1 = y0 + 216;
    // dim all but the step itself
    for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W; x++) {
        const int cy = row_y(t), cx = cell_x(si % 16);
        if (x >= cx - 5 && x <= cx + CELL + 4 && y >= cy - 5 && y <= cy + CELL + 4) continue;
        gfx_fb[y * SCR_W + x] = c_scale(gfx_fb[y * SCR_W + x], 0.5f);
    }
    g_rrect(12, y0, 627, y1, 6, C_PANEL);
    g_rframe(12, y0, 627, y1, 6, col, 1);
    char b[48];
    k_snprintf(b, sizeof b, "STEP %d", si + 1);
    const int x = 24 + w_chip(24, y0 + 8, b, &f14, C_INK, col, 5, 18) + 8;
    k_snprintf(b, sizeof b, "%s  ·  %s", tr->name, s->on ? "ON" : "OFF (A turns it on)");
    g_text(x, y0 + 10, b, &f14, col);
    const int L = 28, ry = y0 + 38;
    char v[24];
    for (int r = 0; r < SR_COUNT; r++) {
        const int y = ry + r * 19;
        const bool sel = r == ui.step_row;
        switch (r) {
        case SR_VEL: k_snprintf(v, sizeof v, "%d", s->vel); w_param(L, y, 280, "VELOCITY", (float) s->vel / 127.0f, v, col, false, sel, 0); break;
        case SR_NUDGE: k_snprintf(v, sizeof v, "%+.1f ms", (double) ((float) s->nudge / 48.0f * 15000.0f / doc.bpm)); w_param(L, y, 280, "NUDGE", (float) s->nudge / 23.0f, v, col, true, sel, 0); break;
        case SR_CHANCE: k_snprintf(v, sizeof v, "%d%%", s->chance); w_param(L, y, 280, "CHANCE", (float) s->chance / 100.0f, v, col, false, sel, 0); break;
        case SR_RATCHET: case SR_SHAPE: case SR_COND: case SR_NOTE: case SR_ACCENT: case SR_FLAM: {
            static const char *const labs[] = { "", "", "RATCHET", "SHAPE", "", "CONDITION", "", "ACCENT", "FLAM" };
            const char *lab = r == SR_NOTE ? (tr->engine == ENG_SAMPLE ? "SLICE" : "NOTE") : labs[r];
            if (sel) { g_rect(L - 6, y - 4, L + 284, y + 13, c_mix(C_PANEL, col, 0.18f)); g_rect(L - 6, y - 4, L - 5, y + 13, col); }
            g_text(L, y - 1, lab, &f12, sel ? C_TEXT : C_DIM);
            if (r == SR_RATCHET) { static const char *const it[] = { "×1", "×2", "×3", "×4", "×5", "×6", "×7", "×8" }; w_chips(L + 72, y - 3, it, 8, step_hits(s) - 1, col, 15, 3, 3); }
            else if (r == SR_SHAPE) { static const char *const it[] = { "FLAT", "FADE", "RISE" }; w_chips(L + 72, y - 3, it, 3, step_rshape(s), col, 15, 4, 4); }
            else if (r == SR_COND) { char cb[16]; cond_name(s->cond, cb, sizeof cb); w_chip(L + 72, y - 3, cb, &f12, s->cond ? C_INK : C_DIM, s->cond ? C_AMBER : C_PANEL2, 5, 15); g_text(L + 160, y - 1, "←→ all 42", &f12, C_DIMMER); }
            else if (r == SR_NOTE) {
                if (tr->engine == ENG_SAMPLE) { if (s->note > 0) k_snprintf(v, sizeof v, "%d of %d", s->note, MAX(tr->smp.slices, 1)); else k_strcpy(v, "THE TRACK'S", sizeof v); }
                else k_snprintf(v, sizeof v, "%+d st", s->note);
                g_text(L + 72, y - 1, v, &f12b, C_TEXT);
            }
            else { const bool on = r == SR_ACCENT ? (s->flags & STEP_ACCENT) : (s->flags & STEP_FLAM); w_toggle(L + 72, y - 2, on, col); }
            break;
        }
        default: break;
        }
    }
    // locks on this step
    const int R = 334;
    g_text(R, ry - 2, "LOCKED ON THIS STEP", &f12b, col);
    int n = 0;
    for (int i = 0; i < p->nlock && n < 4; i++) {
        const lock_t *l = &p->lock[i];
        if (l->trk != t || l->step != si) continue;
        const int y = ry + 18 + n * 19;
        if (l->param < DRUM_NP) {
            drum_format(l->param, l->val, v, sizeof v);
            const float nv = drum_to_norm(l->param, l->val);
            const bool bip = drum_params[l->param].min < 0;
            char lab[16]; k_strcpy(lab, drum_params[l->param].label, sizeof lab);
            for (char *c = lab; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
            w_param(R, y, 278, lab, bip ? nv * 2 - 1 : nv, v, col, bip, false, 60);
        } else { k_snprintf(v, sizeof v, "%.2f", (double) l->val); w_param(R, y, 278, lock_param_name(l->param), 0.5f, v, col, false, false, 60); }
        n++;
    }
    if (!n) g_text(R, ry + 18, "Nothing locked yet.", &f12, C_DIMMER);
    g_text(R, ry + 18 + 4 * 19, "Hold A on the step and move the", &f12, C_DIM);
    g_text(R, ry + 18 + 4 * 19 + 13, "left stick to lock tune and length.", &f12, C_DIM);
    // timing: the step between its neighbours, the ratchet's hits, the nudge
    const int bx0 = R, by0 = y1 - 50, bx1 = 612, by1 = y1 - 10, sw = (bx1 - bx0) / 3;
    g_rect(bx0, by0, bx1, by1, RGB(12, 12, 20)); g_frame(bx0, by0, bx1, by1, C_LINE, 1);
    for (int k = 0; k < 3; k++) {
        const int xs = bx0 + k * sw;
        if (k) g_line(xs, by0 + 1, xs, by1 - 1, C_LINE, 1);
        k_snprintf(b, sizeof b, "%d", si + k);
        if (si + k >= 1) g_text(xs + 4, by0 + 2, b, &f12, k == 1 ? C_TEXT : C_DIM);
    }
    if (s->on) {
        const int nh = step_hits(s), s0 = bx0 + sw + (int) ((float) s->nudge / 48.0f * (float) sw);
        for (int j = 0; j < nh; j++) {
            const float u = (float) j / (float) nh;
            const float h = step_rshape(s) == RAT_FADE ? 1.0f - 0.65f * u : step_rshape(s) == RAT_RISE ? 0.35f + 0.65f * u : 1.0f;
            const int xx = s0 + j * sw / nh;
            g_rect(xx, by1 - 4 - (int) (26 * h), xx + 4, by1 - 4, col);
        }
        if (s->nudge) g_line(bx0 + sw, by1 - 2, s0, by1 - 2, C_AMBER, 2);
    }
    static const char *const hints[] = { "↑↓", "choose", "←→", "change", "A", "on/off", "Y", "clear locks", "B", "close" };
    w_footer(hints, COUNT(hints));
}

void step_button(int b, bool down, bool rep)
{
    if (!down) return;
    pattern_t *p = cur_pat();
    step_t *s = &p->step[ui.cur_t][MAX(ui.cur_s, 0)];
    const track_t *tr = &doc.trk[ui.cur_t];
    switch (b) {
    case BTN_B: case BTN_X: ui.overlay = OV_NONE; return;
    case BTN_UP: ui.step_row = (ui.step_row + SR_COUNT - 1) % SR_COUNT; return;
    case BTN_DOWN: ui.step_row = (ui.step_row + 1) % SR_COUNT; return;
    case BTN_A: s->on ^= 1; if (s->on && !s->chance) s->chance = 100; touch_step(s); return;
    case BTN_Y: clear_locks(p, ui.cur_t, MAX(ui.cur_s, 0)); ui_toast("LOCKS CLEARED"); return;
    case BTN_LEFT: case BTN_RIGHT: {
        const int d = b == BTN_RIGHT ? 1 : -1;
        const int big = rep ? 2 : 1;
        if (!s->on) { s->on = 1; if (!s->vel) s->vel = 100; if (!s->chance) s->chance = 100; }
        switch (ui.step_row) {
        case SR_VEL: s->vel = (uint8_t) CLAMP(s->vel + d * 4 * big, 1, 127); break;
        case SR_NUDGE: s->nudge = (int8_t) CLAMP(s->nudge + d, -23, 23); break;
        case SR_RATCHET: s->ratchet = (uint8_t) ((s->ratchet & 0xF0) | CLAMP((s->ratchet & 7) + d, 0, 7)); break;
        case SR_SHAPE: s->ratchet = (uint8_t) ((s->ratchet & 7) | (CLAMP(step_rshape(s) + d, 0, 2) << 4)); break;
        case SR_CHANCE: s->chance = (uint8_t) CLAMP(s->chance + d * (s->chance + d <= 10 ? 1 : 5), 1, 100); break;
        case SR_COND: s->cond = (uint8_t) cond_next(s->cond, d); break;
        case SR_NOTE:
            if (tr->engine == ENG_SAMPLE) s->note = (int8_t) CLAMP(s->note + d, 0, MAX(tr->smp.slices, 1));
            else s->note = (int8_t) CLAMP(s->note + d, -24, 24);
            break;
        case SR_ACCENT: if (d > 0) s->flags |= STEP_ACCENT; else s->flags &= (uint8_t) ~STEP_ACCENT; break;
        case SR_FLAM: if (d > 0) s->flags |= STEP_FLAM; else s->flags &= (uint8_t) ~STEP_FLAM; break;
        default: break;
        }
        touch_step(s);
        if (!ui.st.playing) ui_msg(M_HIT, ui.cur_t, s->note > 0 ? 64 + s->note : 0, 0, (float) s->vel / 127.0f);
        return;
    }
    default: return;
    }
}
