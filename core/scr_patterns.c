// PATTERNS (docs/mockups/screens/09): the bank's sixteen patterns as tiles; queue, jump, copy, paste, clear.
// SONG (10): the chain of patterns, each played a number of times, with its tempo, mutes, last bar and what follows.
#include "ui_int.h"

static void pat_name(int n, char *b, int cap) { k_snprintf(b, cap, "%c%02d", 'A' + n / NPERBANK, n % NPERBANK + 1); }
static col_t pat_col(int n) { return trk_col[n % NPERBANK % NTRK]; }

static bool pat_empty(const pattern_t *p)
{
    for (int t = 0; t < NTRK; t++)
        for (int s = 0; s < NSTEP; s++) if (p->step[t][s].on) return false;
    return true;
}

// The pattern in small: cols columns of w px, NTRK rows of h px, from step s0 for n steps.
static void mini(const pattern_t *p, int x, int y, int cols, int w, int h, bool dim)
{
    const int len = CLAMP(p->len, 1, NSTEP);
    for (int c = 0; c < cols; c++) {
        const int s0 = c * len / cols, s1 = MAX((c + 1) * len / cols, s0 + 1);
        for (int t = 0; t < NTRK; t++) {
            int v = 0;
            for (int s = s0; s < s1; s++) if (p->step[t][s].on) v = MAX(v, p->step[t][s].vel);
            const int x0 = x + c * w, y0 = y + t * h;
            if (v) g_rect(x0, y0, x0 + w - 2, y0 + h - 2, c_scale(trk_col[t], dim ? 0.45f : v < 64 ? 0.55f : 1.0f));
            else g_rect(x0, y0, x0 + w - 2, y0 + h - 2, C_PANEL2);
        }
    }
}

// ------------------------------------------------------------------------------------------------- PATTERNS

static int clear_target;

static void do_clear(void)
{
    pattern_t *p = &doc.pat[clear_target];
    ui.undo = *p; ui.undo_pat = clear_target; ui.undo_ok = true;
    doc_pattern_clear(p);
    doc_touch(p, sizeof *p);
    ui_toast("CLEARED");
}

void patterns_button(int b, bool down, bool rep)
{
    (void) rep;
    int *sel = &ui.sel[NAV_PATTERNS];
    if (!down) return;
    const int bank = *sel / NPERBANK, i = *sel % NPERBANK, col = i % 4;
    if (ui.held[BTN_R2] && (b == BTN_A || b == BTN_X || b == BTN_Y)) {
        ui.used[BTN_R2] = true;
        char nm[8]; pat_name(*sel, nm, sizeof nm);
        if (b == BTN_A) { ui.pat_clip = doc.pat[*sel]; ui.pat_clip_ok = true; ui_toast("COPIED"); }
        else if (b == BTN_X) {
            if (!ui.pat_clip_ok) { ui_toast("NOTHING COPIED"); return; }
            pattern_t *p = &doc.pat[*sel];
            ui.undo = *p; ui.undo_pat = *sel; ui.undo_ok = true;
            *p = ui.pat_clip;
            p->used = 1;
            doc_touch(p, sizeof *p);
            ui_toast("PASTED");
        } else {
            clear_target = *sel;
            char t[24]; k_snprintf(t, sizeof t, "CLEAR %s?", nm);
            confirm_open(t, "Its steps and locks go. The pattern screen can undo it.", "CLEAR", do_clear);
        }
        return;
    }
    switch (b) {
    case BTN_LEFT: *sel = col > 0 ? *sel - 1 : ((bank + NBANK - 1) % NBANK) * NPERBANK + i + 3; break;
    case BTN_RIGHT: *sel = col < 3 ? *sel + 1 : ((bank + 1) % NBANK) * NPERBANK + i - 3; break;
    case BTN_UP: if (i >= 4) *sel -= 4; break;
    case BTN_DOWN: if (i < 12) *sel += 4; break;
    case BTN_R1: *sel = ((bank + 1) % NBANK) * NPERBANK + i; break;
    case BTN_A:   // next at the end of this one (now, when stopped)
        ui.pat = *sel;
        ui_msg(M_QUEUE, *sel, ui.st.playing ? 0 : 1, 0, 0);
        break;
    case BTN_Y:   // now
        ui.pat = *sel;
        ui_msg(M_QUEUE, *sel, 1, 0, 0);
        if (!ui.st.playing) ui_msg(M_PLAY, 0, 0, 0, 0);
        break;
    case BTN_X:
        ui.pat = *sel;
        if (!ui.st.playing) ui_msg(M_QUEUE, *sel, 1, 0, 0);
        go(NAV_PATTERN);
        break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

void draw_patterns(void)
{
    static bool placed;
    if (!placed) { ui.sel[NAV_PATTERNS] = ui.pat; placed = true; }
    g_clear(C_BG);
    const col_t pc = nav_col(NAV_PATTERNS);
    w_header("PATTERNS", pc);
    const int sel = ui.sel[NAV_PATTERNS], bank = sel / NPERBANK;
    char b[32];
    // the banks
    int x = 8;
    for (int k = 0; k < NBANK; k++) {
        bool any = false;
        for (int i = 0; i < NPERBANK; i++) if (doc.pat[k * NPERBANK + i].used) any = true;
        const char s[2] = { (char) ('A' + k), 0 };
        g_rrect(x, 36, x + 28, 54, 3, k == bank ? C_TEXT : any ? C_PANEL2 : C_PANEL);
        g_text_c(x + 14, 38, s, &f14, k == bank ? C_INK : any ? C_TEXT : C_DIMMER);
        if (ui.st.playing && ui.st.pat / NPERBANK == k) g_rect(x + 4, 56, x + 24, 57, C_GREEN);
        x += 33;
    }
    int used = 0;
    for (int i = 0; i < NPERBANK; i++) if (doc.pat[bank * NPERBANK + i].used && !pat_empty(&doc.pat[bank * NPERBANK + i])) used++;
    k_snprintf(b, sizeof b, "BANK %c  \xc2\xb7  %d OF 16 USED", 'A' + bank, used);
    g_text_r(632, 40, b, &f12, C_DIM);
    // the tiles
    for (int i = 0; i < NPERBANK; i++) {
        const int n = bank * NPERBANK + i, x0 = 8 + (i % 4) * 157, y0 = 62 + (i / 4) * 98, x1 = x0 + 150, y1 = y0 + 92;
        const pattern_t *p = &doc.pat[n];
        const bool empty = pat_empty(p);
        const bool playing = ui.st.playing && ui.st.pat == n, next = ui.st.playing && ui.st.next_pat == n;
        pat_name(n, b, sizeof b);
        if (empty) {
            g_rframe(x0, y0, x1, y1, 4, C_PANEL2, 1);
            g_text(x0 + 7, y0 + 5, b, &f14, C_DIMMER);
            g_text_c((x0 + x1) / 2, y0 + 44, "EMPTY", &f12, C_DIMMER);
        } else {
            g_rrect(x0, y0, x1, y1, 4, C_PANEL);
            g_text(x0 + 7, y0 + 5, b, &f14, C_TEXT);
            mini(p, x0 + 7, y0 + 25, 16, 9, 5, false);
            if (playing) { g_tri(x0 + 50, y0 + 7, x0 + 50, y0 + 17, x0 + 58, y0 + 12, C_GREEN); g_text_r(x1 - 7, y0 + 6, "PLAY", &f12, C_GREEN); }
            else if (next) g_text_r(x1 - 7, y0 + 6, "NEXT", &f12, C_AMBER);
            else { k_snprintf(b, sizeof b, "%d", CLAMP(p->len, 1, NSTEP)); g_text_r(x1 - 7, y0 + 6, b, &f12, C_DIM); }
            if (playing) {   // where it is
                const int s = CLAMP((int) (ui.st.beat * 4.0f), 0, CLAMP(p->len, 1, NSTEP) - 1);
                const int px = x0 + 7 + s * 16 / CLAMP(p->len, 1, NSTEP) * 9;
                g_rect(px, y0 + 22, px + 7, y0 + 23, C_WHITE);
            }
        }
        if (playing) g_rframe(x0, y0, x1, y1, 4, C_GREEN, 2);
        else if (next) g_rframe(x0, y0, x1, y1, 4, C_AMBER, 2);
        if (n == ui.pat && !playing && !next) g_rect(x0 + 7, y1 - 4, x0 + 30, y1 - 3, C_TEXT);
        if (n == sel) g_rframe(x0 - 3, y0 - 3, x1 + 3, y1 + 3, 6, C_WHITE, 2);
    }
    static const char *const hints[] = { "A", "queue", "Y", "play now", "X", "edit", "R2+A", "copy", "R2+X", "paste", "R2+Y", "clear", "R1", "bank" };
    w_footer(hints, COUNT(hints));
}

// ------------------------------------------------------------------------------------------------- SONG

enum { SF_PATTERN, SF_REPEAT, SF_TEMPO, SF_MUTES, SF_LAST, SF_NEXT, SF_COUNT };
static const char *const sf_name[SF_COUNT] = { "PATTERN", "REPEAT", "TEMPO", "MUTES", "LAST BAR", "NEXT" };
static const char *const lb_name[] = { "AS IS", "FILL", "DROP" };
static const char *const sn_name[] = { "GO ON", "LOOP", "STOP" };
static int mute_cur;

static float entry_bars(const song_t *e) { return (float) CLAMP(doc.pat[e->pat % NPAT].len, 1, NSTEP) * (float) MAX(e->repeat, 1) / 16.0f; }

static void song_touch(void) { doc_touch(doc.song, sizeof doc.song); doc_touch(&doc.song_len, 1); }

static void song_insert(int at, song_t e)
{
    if (doc.song_len >= NSONG) { ui_toast("SONG FULL"); return; }
    for (int i = doc.song_len; i > at; i--) doc.song[i] = doc.song[i - 1];
    doc.song[at] = e;
    doc.song_len++;
    song_touch();
}

static void song_delete(int at)
{
    if (!doc.song_len) return;
    for (int i = at; i < doc.song_len - 1; i++) doc.song[i] = doc.song[i + 1];
    doc.song_len--;
    doc.song[doc.song_len] = (song_t) { 0, 1, 0, 0, 0, 0, 0 };
    song_touch();
}

static void entry_change(song_t *e, int field, int d, bool big)
{
    switch (field) {
    case SF_PATTERN: e->pat = (uint8_t) CLAMP((int) e->pat + d * (big ? NPERBANK : 1), 0, NPAT - 1); break;
    case SF_REPEAT: e->repeat = (uint8_t) CLAMP((int) e->repeat + d * (big ? 4 : 1), 1, 64); break;
    case SF_TEMPO:   // below 40: none of its own (SONG)
        if (e->tempo <= 0) { if (d > 0) e->tempo = m_round(doc.bpm); }
        else { const float t = e->tempo + (float) d * (big ? 5.0f : 1.0f); e->tempo = t < 40 ? 0 : MIN(t, 300.0f); }
        break;
    case SF_LAST: e->last_bar = (uint8_t) CLAMP((int) e->last_bar + d, 0, 2); break;
    case SF_NEXT: e->next = (uint8_t) CLAMP((int) e->next + d, 0, 2); break;
    default: break;
    }
    song_touch();
}

void song_button(int b, bool down, bool rep)
{
    (void) rep;
    int *sel = &ui.sel[NAV_SONG], *f = &ui.tab[NAV_SONG];
    const int n = doc.song_len;
    if (*sel >= n) *sel = MAX(n - 1, 0);
    if (!down) {   // A let go: play from here (on MUTES: that track's mute)
        if (b == BTN_A && !ui.used[BTN_A] && n) {
            if (*f == SF_MUTES) { doc.song[*sel].mutes ^= (uint16_t) (1u << mute_cur); song_touch(); }
            else { ui_msg(M_SONG, 1, *sel, 0, 0); if (!ui.st.playing) ui_msg(M_PLAY, 0, 0, 0, 0); }
        }
        return;
    }
    if (ui.held[BTN_R2] && (b == BTN_A || b == BTN_X || b == BTN_Y)) {
        ui.used[BTN_R2] = true;
        if (b == BTN_X) {   // a copy after this one, or the pattern being edited when there is none
            song_t e = n ? doc.song[*sel] : (song_t) { (uint8_t) ui.pat, 1, 0, 0, 0, 0, 0 };
            song_insert(n ? *sel + 1 : 0, e);
            if (n) (*sel)++;
        } else if (b == BTN_Y) { if (n) { song_delete(*sel); *sel = MAX(MIN(*sel, doc.song_len - 1), 0); } }
        else { song_insert(n ? *sel + 1 : 0, (song_t) { (uint8_t) ui.pat, 1, 0, 0, 0, 0, 0 }); if (n) (*sel)++; ui_toast("ADDED THE EDITED PATTERN"); }
        return;
    }
    if (ui.held[BTN_A] && b >= BTN_UP && b <= BTN_RIGHT && n) {
        ui.used[BTN_A] = true;
        if (*f == SF_MUTES) return;
        entry_change(&doc.song[*sel], *f, (b == BTN_RIGHT || b == BTN_UP) ? 1 : -1, b == BTN_UP || b == BTN_DOWN);
        return;
    }
    switch (b) {
    case BTN_LEFT: if (*f == SF_MUTES) mute_cur = MAX(mute_cur - 1, 0); else *sel = MAX(*sel - 1, 0); break;
    case BTN_RIGHT: if (*f == SF_MUTES) mute_cur = MIN(mute_cur + 1, NTRK - 1); else *sel = MIN(*sel + 1, MAX(n - 1, 0)); break;
    case BTN_UP: *f = MAX(*f - 1, 0); break;
    case BTN_DOWN: *f = MIN(*f + 1, SF_COUNT - 1); break;
    case BTN_X: if (n) { ui.pat = doc.song[*sel].pat; go(NAV_PATTERN); } break;
    case BTN_B: go(NAV_PATTERN); break;
    default: break;
    }
}

void draw_song(void)
{
    g_clear(C_BG);
    const col_t sc = nav_col(NAV_SONG);
    w_header("SONG", sc);
    const int n = doc.song_len, sel = MIN(ui.sel[NAV_SONG], MAX(n - 1, 0)), field = ui.tab[NAV_SONG];
    char b[48];
    float total = 0;
    for (int i = 0; i < n; i++) total += entry_bars(&doc.song[i]);
    {   // the length, in the header
        const float secs = total * 4.0f * 60.0f / MAX(doc.bpm, 20.0f);
        k_snprintf(b, sizeof b, "%d BARS  %d:%02d", (int) (total + 0.5f), (int) secs / 60, (int) secs % 60);
        g_text(330, 8, b, &f12, C_DIM);
    }
    // the timeline: every entry as long as it plays
    const float ppb = 12.0f;   // pixels a bar
    float start = 0, sel_x0 = 0, sel_x1 = 0;
    for (int i = 0; i < n; i++) { if (i == sel) { sel_x0 = start; sel_x1 = start + entry_bars(&doc.song[i]) * ppb; } start += entry_bars(&doc.song[i]) * ppb; }
    float scroll = 0;
    if (sel_x1 > 600) scroll = sel_x1 - 600;
    if (sel_x0 - scroll < 0) scroll = sel_x0;
    const int TX = 12, TY = 52, TH = 104;
    g_rrect(4, 34, 636, 192, 4, C_PANEL);
    g_clip(6, 34, 634, 192);
    for (int bar = 0; bar <= (int) total + 1; bar += 4) {
        const int x = TX + (int) ((float) bar * ppb - scroll);
        if (x < 0 || x > 640) continue;
        k_snprintf(b, sizeof b, "%d", bar + 1);
        g_text(x, 36, b, &f12, C_DIMMER);
        g_line(x, TY - 2, x, TY + TH + 2, C_PANEL2, 1);
    }
    float x = 0;
    int play_x = -1;
    for (int i = 0; i < n; i++) {
        const song_t *e = &doc.song[i];
        const float w = entry_bars(e) * ppb;
        const int x0 = TX + (int) (x - scroll), x1 = TX + (int) (x + w - scroll) - 2;
        x += w;
        if (x1 < 0 || x0 > 640) continue;
        const col_t c = pat_col(e->pat);
        g_rrect(x0, TY, x1, TY + TH, 3, c_mix(C_BG, c, i == sel ? 0.4f : 0.25f));
        g_rect(x0, TY, x1, TY + 2, c);
        pat_name(e->pat, b, sizeof b);
        if (x1 - x0 > 30) g_text(x0 + 4, TY + 5, b, &f12b, C_TEXT);
        if (e->repeat > 1 && x1 - x0 > 64) { k_snprintf(b, sizeof b, "\xc3\x97%d", e->repeat); g_text_r(x1 - 4, TY + 5, b, &f12, C_TEXT); }
        // the pattern once, stretched over one time round
        const pattern_t *p = &doc.pat[e->pat % NPAT];
        const int once = MAX((int) (w / (float) MAX(e->repeat, 1)), 4), cols = MAX(MIN(once / 3, CLAMP(p->len, 1, NSTEP)), 1);
        for (int r = 0; r < MAX(e->repeat, 1); r++) {
            const int rx = x0 + 3 + r * once;
            if (rx + once > x1 + 2) break;
            for (int cc = 0; cc < cols; cc++) {
                const int s0 = cc * CLAMP(p->len, 1, NSTEP) / cols, s1 = MAX((cc + 1) * CLAMP(p->len, 1, NSTEP) / cols, s0 + 1);
                for (int t = 0; t < NTRK; t++) {
                    bool on = false;
                    for (int s = s0; s < s1; s++) if (p->step[t][s].on) on = true;
                    if (on) { const int px = rx + cc * (once - 3) / cols; g_rect(px, TY + 24 + t * 6, px + MAX((once - 3) / cols - 1, 1), TY + 27 + t * 6, (e->mutes >> t & 1) ? C_DIMMER : trk_col[t]); }
                }
            }
        }
        if (x1 - x0 < 40) {}
        else if (e->last_bar == LB_FILL) g_text_r(x1 - 3, TY + TH - 14, "FILL", &f12, C_AMBER);
        else if (e->last_bar == LB_DROP) g_text_r(x1 - 3, TY + TH - 14, "DROP", &f12, C_RED);
        if (i == sel) g_rframe(x0 - 2, TY - 2, x1 + 2, TY + TH + 2, 4, C_WHITE, 2);
        if (ui.st.playing && ui.st.song_on && ui.st.song_pos == i) {
            const float into = ((float) ui.st.song_rep + ui.st.beat * 4.0f / (float) CLAMP(p->len, 1, NSTEP)) / (float) MAX(e->repeat, 1);
            play_x = x0 + (int) (into * (float) (x1 - x0));
        }
    }
    if (play_x >= 0) g_rect(play_x, TY - 4, play_x + 1, TY + TH + 4, C_WHITE);
    if (!n) g_text_c(320, 100, "EMPTY.  R2+X ADDS THE PATTERN YOU ARE EDITING", &f12, C_DIM);
    g_noclip();
    if (n) {
        const song_t *e = &doc.song[sel];
        if (e->next == SN_LOOP) g_text(TX + (int) (sel_x0 - scroll), TY + TH + 10, "\xe2\x97\x84 LOOPS HERE", &f12, C_SKY);
        else if (e->next == SN_STOP) g_text(TX + (int) (sel_x0 - scroll), TY + TH + 10, "\xe2\x96\xa0 STOPS HERE", &f12, C_RED);
    }

    // the chain, as a list
    g_rrect(4, 198, 316, 452, 4, C_PANEL);
    g_text(12, 204, "CHAIN", &f12b, C_DIM);
    const int rows = 12, first = CLAMP(sel - rows / 2, 0, MAX(n - rows, 0));
    float bar_at = 0;
    for (int i = 0; i < first; i++) bar_at += entry_bars(&doc.song[i]);
    for (int i = first; i < MIN(n, first + rows); i++) {
        const song_t *e = &doc.song[i];
        const int y = 222 + (i - first) * 19;
        if (i == sel) g_rect(6, y - 2, 314, y + 15, c_mix(C_PANEL, sc, 0.25f));
        const bool now = ui.st.playing && ui.st.song_on && ui.st.song_pos == i;
        k_snprintf(b, sizeof b, "%02d", i + 1);
        g_text(12, y, b, &f12, now ? C_GREEN : C_DIM);
        g_rect(36, y + 3, 44, y + 11, pat_col(e->pat));
        pat_name(e->pat, b, sizeof b);
        g_text(50, y, b, &f12b, C_TEXT);
        k_snprintf(b, sizeof b, "\xc3\x97%d", e->repeat);
        g_text(84, y, b, &f12, C_TEXT);
        const float eb = entry_bars(e);
        if (eb <= 1.0f) k_snprintf(b, sizeof b, "BAR %d", (int) bar_at + 1);
        else k_snprintf(b, sizeof b, "BARS %d-%d", (int) bar_at + 1, (int) (bar_at + eb + 0.999f));
        g_text(118, y, b, &f12, C_DIM);
        bar_at += eb;
        if (e->tempo > 0) { k_snprintf(b, sizeof b, "%d BPM", (int) e->tempo); g_text(210, y, b, &f12, C_SKY); }
        if (e->last_bar) g_text_r(310, y, lb_name[e->last_bar], &f12, e->last_bar == LB_FILL ? C_AMBER : C_RED);
        else if (e->next) g_text_r(310, y, sn_name[e->next], &f12, e->next == SN_LOOP ? C_SKY : C_RED);
    }

    // the entry
    g_rrect(322, 198, 636, 452, 4, C_PANEL);
    if (!n) {
        static const char *const hints[] = { "R2+X", "add", "B", "back" };
        w_footer(hints, COUNT(hints));
        return;
    }
    const song_t *e = &doc.song[sel];
    k_snprintf(b, sizeof b, "ENTRY %02d", sel + 1);
    g_text(332, 204, b, &f12b, sc);
    pat_name(e->pat, b, sizeof b);
    g_text(332, 220, b, &f24, pat_col(e->pat));
    k_snprintf(b, sizeof b, "%d BAR%s", (int) (entry_bars(e) + 0.5f), entry_bars(e) > 1.5f ? "S" : "");
    g_text(400, 230, b, &f12, C_DIM);
    for (int r = 0; r < SF_COUNT; r++) {
        const int y = 258 + r * 26;
        if (r == field) g_rect(324, y - 4, 634, y + 18, c_mix(C_PANEL, sc, 0.18f));
        if (r == field) g_rect(324, y - 4, 326, y + 18, sc);
        g_text(332, y, sf_name[r], &f12, r == field ? C_TEXT : C_DIM);
        switch (r) {
        case SF_PATTERN: pat_name(e->pat, b, sizeof b); g_text(420, y, b, &f12b, pat_col(e->pat)); break;
        case SF_REPEAT:
            w_bar(420, y + 4, 160, (float) e->repeat / 16.0f, sc, false, 6);
            k_snprintf(b, sizeof b, "\xc3\x97%d", e->repeat); g_text_r(628, y, b, &f12b, C_TEXT); break;
        case SF_TEMPO:
            if (e->tempo > 0) { w_bar(420, y + 4, 160, (e->tempo - 40) / 260.0f, C_SKY, false, 6); k_snprintf(b, sizeof b, "%d", (int) e->tempo); g_text_r(628, y, b, &f12b, C_TEXT); }
            else { w_bar(420, y + 4, 160, 0, C_SKY, false, 6); g_text_r(628, y, "SONG", &f12b, C_DIM); }
            break;
        case SF_MUTES:
            for (int t = 0; t < NTRK; t++) {
                const int mx = 420 + t * 17;
                const bool m = e->mutes >> t & 1;
                g_rect(mx, y, mx + 13, y + 13, m ? C_PANEL2 : trk_col[t]);
                if (m) g_text_c(mx + 7, y, "M", &f12b, C_AMBER);
                if (r == field && t == mute_cur) g_frame(mx - 2, y - 2, mx + 15, y + 15, C_WHITE, 1);
            }
            break;
        case SF_LAST: w_chips(420, y - 1, lb_name, 3, e->last_bar, e->last_bar == LB_DROP ? C_RED : C_AMBER, 16, 5, 4); break;
        case SF_NEXT: w_chips(420, y - 1, sn_name, 3, e->next, e->next == SN_STOP ? C_RED : C_SKY, 16, 5, 4); break;
        default: break;
        }
    }
    g_text(332, 418, field == SF_MUTES ? "\xe2\x86\x90\xe2\x86\x92 a track, A mutes it here." : "A plays from here. START plays the song", &f12, C_DIM);
    if (field != SF_MUTES) g_text(332, 432, "from the top.", &f12, C_DIM);
    static const char *const hints[] = { "\xe2\x86\x90\xe2\x86\x92", "entry", "A", "play here", "A+\xe2\x86\x90\xe2\x86\x92", "change", "X", "edit", "R2+X", "insert", "R2+Y", "delete", "R2+A", "add edited" };
    w_footer(hints, COUNT(hints));
}
