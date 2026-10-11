// SAMPLE: a sample track's file, where it starts and ends, its slices, how it plays and how gritty it sounds
// (docs/mockups/screens/14). Slices come as an even grid or at the hits (transients).
#include "ui_int.h"

static const char *const tabs[] = { "TRIM", "SLICE", "PLAY", "TONE" };
static uint8_t slice_mode[NTRK];        // 0 grid, 1 at the hits
static float sens[NTRK] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };
static int heard[NTRK];                 // the last slice X played

// Slices at the hits: an energy envelope in 5 ms windows, where it jumps the most.
static void find_hits(int t)
{
    track_t *tr = &doc.trk[t];
    const sample_t *s = ui_sample(t);
    const int n = tr->smp.slices;
    m_set(tr->smp.slice_pos, 0, sizeof tr->smp.slice_pos);
    if (!s || n < 2 || slice_mode[t] == 0) { doc_touch(tr->smp.slice_pos, sizeof tr->smp.slice_pos); return; }
    const uint32_t a = (uint32_t) (m_clampf(tr->smp.start, 0, 1) * (float) s->frames), e = (uint32_t) (m_clampf(tr->smp.end, 0, 1) * (float) s->frames);
    const uint32_t win = MAX(1u, s->rate / 200);
    static float env[4096], jump[4096];
    int nw = 0;
    for (uint32_t i = a; i + win <= e && nw < 4096; i += win, nw++) {
        float acc = 0;
        for (uint32_t k = i; k < i + win; k++) { const float x = (float) s->data[k * s->chans] / 32768.0f; acc += x * x; }
        env[nw] = m_sqrt(acc / (float) win);
    }
    for (int i = 0; i < nw; i++) jump[i] = i ? MAX(0.0f, env[i] - env[i - 1] * (1.2f + sens[t])) : 0;
    // the n - 1 biggest jumps, not closer than 40 ms to each other
    int picked[NSLICE]; int np = 0;
    for (int k = 0; k < n - 1 && np < NSLICE - 1; k++) {
        int best = -1;
        for (int i = 2; i < nw; i++) {
            bool near = false;
            for (int j = 0; j < np; j++) if (i > picked[j] - 8 && i < picked[j] + 8) near = true;
            if (!near && jump[i] > 1e-4f && (best < 0 || jump[i] > jump[best])) best = i;
        }
        if (best < 0) break;
        picked[np++] = best;
    }
    for (int i = 1; i < np; i++) for (int j = i; j > 0 && picked[j - 1] > picked[j]; j--) { const int x = picked[j]; picked[j] = picked[j - 1]; picked[j - 1] = x; }
    for (int i = 0; i < np; i++) tr->smp.slice_pos[i + 1] = (float) picked[i] / (float) MAX(nw, 1);
    tr->smp.slices = (uint8_t) (np + 1);
    doc_touch(&tr->smp, sizeof tr->smp);
}

static int params(int t, prm_t *p)
{
    smp_t *s = &doc.trk[t].smp;
    int n = 0;
    switch (ui.tab[NAV_SAMPLE]) {
    case 0:
        p[n++] = prm_f("Start", &s->start, 0, 1, 0.002f, UN_PCT);
        p[n++] = prm_f("End", &s->end, 0, 1, 0.002f, UN_PCT);
        p[n++] = prm_choice("Mode", &s->mode, "one shot|gate|loop");
        p[n++] = prm_f("Loop at", &s->loop, 0, 0.98f, 0.01f, UN_PCT);
        p[n++] = prm_choice("Reverse", &s->reverse, "off|on");
        break;
    case 1:
        p[n++] = prm_u8("Slices", &s->slices, 0, NSLICE, UN_INT);
        p[n++] = prm_choice("Slice by", &slice_mode[t], "grid|hits");
        p[n++] = prm_f("Feel", &sens[t], 0, 1, 0.05f, UN_PCT);
        p[n++] = prm_choice("Cut own", &s->choke_self, "off|on");
        break;
    case 2:
        p[n++] = prm_f("Pitch", &s->pitch, -24, 24, 1, UN_ST);
        p[n++] = prm_f("Gain", &s->gain, -24, 12, 0.5f, UN_DB);
        p[n++] = prm_f("Attack", &s->attack, 0, 200, 1, UN_MS);
        p[n++] = prm_f("Hold", &s->hold, 0, 2000, 10, UN_MS);
        p[n++] = prm_f("Decay", &s->decay, 0, 4000, 10, UN_MS);
        break;
    default:
        p[n++] = prm_choice("Filter", &s->filter_mode, "low|band|high");
        p[n++] = prm_flog("Cutoff", &s->filter, 20, 20000, UN_HZ);
        p[n++] = prm_f("Reso", &s->reso, 0, 1, 0.02f, UN_PCT);
        p[n++] = prm_f("Drive", &s->drive, 0, 1, 0.02f, UN_PCT);
        p[n++] = prm_f("Bits", &s->bits, 2, 16, 1, UN_INT);
        p[n++] = prm_flog("Rate", &s->rate, 1000, 48000, UN_HZ);
        break;
    }
    return n;
}

void sample_button(int b, bool down, bool rep)
{
    (void) rep;
    const int t = ui.cur_t;
    track_t *tr = &doc.trk[t];
    if (!down) return;
    if (tr->engine != ENG_SAMPLE) {
        if (b == BTN_B) go(NAV_PATTERN);
        if (b == BTN_Y) { ui.tab[NAV_BROWSER] = 0; go(NAV_BROWSER); }
        return;
    }
    switch (b) {
    case BTN_R1: ui.tab[NAV_SAMPLE] = (ui.tab[NAV_SAMPLE] + 3) % 4; ui.sel[NAV_SAMPLE] = 0; return;
    case BTN_R2: ui.tab[NAV_SAMPLE] = (ui.tab[NAV_SAMPLE] + 1) % 4; ui.sel[NAV_SAMPLE] = 0; return;
    case BTN_B: go(NAV_PATTERN); return;
    case BTN_Y: ui.tab[NAV_BROWSER] = 0; go(NAV_BROWSER); return;
    case BTN_X: {
        const int n = MAX(tr->smp.slices, 1);
        heard[t] = tr->smp.slices ? heard[t] % n + 1 : 0;
        ui_msg(M_HIT, t, heard[t] ? 64 + heard[t] : 0, 0, 0.85f);
        return;
    }
    default: break;
    }
    prm_t p[8];
    const int n = params(t, p);
    const uint8_t before_slices = tr->smp.slices, before_mode = slice_mode[t];
    const float before_sens = sens[t];
    if (prm_nav(p, n, &ui.sel[NAV_SAMPLE], 5, b, down)) {
        if (ui.tab[NAV_SAMPLE] == 1 && (tr->smp.slices != before_slices || slice_mode[t] != before_mode || sens[t] != before_sens)) find_hits(t);
        if (tr->smp.end < tr->smp.start + 0.002f) { tr->smp.end = MIN(1.0f, tr->smp.start + 0.002f); doc_touch(&tr->smp.end, 4); }
        if (!ui.st.playing) ui_msg(M_HIT, t, heard[t] ? 64 + heard[t] : 0, 0, 0.85f);
    }
}

void draw_sample(void)
{
    g_clear(C_BG);
    const int t = ui.cur_t;
    const track_t *tr = &doc.trk[t];
    const col_t col = trk_col[t];
    w_header("SAMPLE", nav_col(NAV_SAMPLE));
    char b[64];
    if (tr->engine != ENG_SAMPLE) {
        g_text_c(SCR_W / 2, 200, "THIS TRACK IS A SYNTH", &f16, C_DIM);
        g_text_c(SCR_W / 2, 224, "Y opens the browser to give it a sample.", &f12, C_DIM);
        static const char *const hints[] = { "Y", "browser", "B", "back" };
        w_footer(hints, COUNT(hints));
        return;
    }
    const sample_t *s = ui_sample(t);
    k_snprintf(b, sizeof b, "%02d %s", t + 1, tr->name);
    g_text(10, 36, b, &f16, col);
    k_strcpy(b, tr->sound, 40); w_upper(b);
    g_text(130, 36, b, &f16, C_TEXT);
    if (s) k_snprintf(b, sizeof b, "%.2f s  %d HZ  %s", (double) s->frames / s->rate, (int) s->rate, s->chans > 1 ? "STEREO" : "MONO");
    else k_strcpy(b, "NOT FOUND ON THE CARD", sizeof b);
    g_text_r(632, 39, b, &f12, s ? C_DIM : C_RED);
    w_tabs(8, 58, tabs, 4, ui.tab[NAV_SAMPLE], col);
    // the waveform: the region bright, the rest dim, slices numbered, the one last heard lit
    const int bx0 = 4, by0 = 84, bx1 = 636, by1 = 234, w = bx1 - bx0 - 2, mid = (by0 + by1) / 2 + 6, amp = (by1 - by0) / 2 - 10;
    g_rect(bx0, by0, bx1, by1, RGB(12, 12, 20));
    g_frame(bx0, by0, bx1, by1, C_LINE, 1);
    const int xa = bx0 + 1 + (int) (tr->smp.start * (float) w), xe = bx0 + 1 + (int) (tr->smp.end * (float) w);
    const int ns = tr->smp.slices;
    float u0 = 0, u1 = 0;
    if (heard[t] && ns) smp_slice_range(&tr->smp, heard[t], &u0, &u1);
    if (heard[t] && ns) g_rect(bx0 + 1 + (int) (u0 * (float) w), by0 + 1, bx0 + 1 + (int) (u1 * (float) w), by1 - 1, c_mix(C_BG, col, 0.22f));
    if (s) {
        for (int c = 0; c < w; c++) {
            const uint32_t a = (uint32_t) ((uint64_t) c * s->frames / (uint32_t) w), e = (uint32_t) ((uint64_t) (c + 1) * s->frames / (uint32_t) w);
            int hi = 0;
            for (uint32_t i = a; i < e; i += 2) { const int v = s->data[i * s->chans]; hi = MAX(hi, v < 0 ? -v : v); }
            const int h = (int) ((float) hi / 32768.0f * (float) amp), x = bx0 + 1 + c;
            const bool in = x >= xa && x <= xe;
            g_line(x, mid - h, x, mid + h, in ? c_scale(col, 0.9f) : C_DIMMER, 1);
        }
    }
    g_line(xa, by0 + 1, xa, by1 - 1, C_GREEN, 2);
    g_line(xe, by0 + 1, xe, by1 - 1, C_RED, 2);
    if (tr->smp.mode == SMP_LOOP) { const int xl = xa + (int) (tr->smp.loop * (float) (xe - xa)); g_line(xl, by0 + 1, xl, by1 - 1, C_AMBER, 1); g_text(xl + 3, by1 - 14, "LOOP", &f12, C_AMBER); }
    for (int k = 1; k <= ns; k++) {
        float a, e2;
        smp_slice_range(&tr->smp, k, &a, &e2);
        const int x = bx0 + 1 + (int) (a * (float) w);
        g_line(x, by0 + 1, x, by1 - 1, k == heard[t] ? C_WHITE : C_DIMMER, 1);
        k_snprintf(b, sizeof b, "%d", k);
        w_chip(x + 1, by0 + 1, b, &f12, k == heard[t] ? C_INK : C_TEXT, k == heard[t] ? C_WHITE : C_PANEL2, 2, 12);
    }
    // the knobs of this tab
    prm_t p[8];
    const int n = params(t, p);
    w_box(4, 242, 636, 362, NULL, C_DIM, C_PANEL, NULL);
    w_knobs(p, n, ui.sel[NAV_SAMPLE], 70, 286, 5, 125, 100, 26, col);
    // what is where
    w_box(4, 368, 636, 456, NULL, C_DIM, C_PANEL, NULL);
    static const char *const help[4] = {
        "Green: where it starts. Red: where it ends. In LOOP mode it goes round from the amber mark.",
        "BY HITS cuts where the drums hit; FEEL sets how small a hit counts. A step picks its slice.",
        "Pitch in semitones; DECAY 0 plays to the end. Each step can lock its own pitch and decay.",
        "BITS and RATE are the grit: 12 bits at 26 kHz is the SP-1200. The filter comes after them.",
    };
    g_text(14, 376, help[ui.tab[NAV_SAMPLE]], &f12, C_DIM);
    g_text(14, 396, "X plays the next slice (or the whole sample).  Y loads another file.", &f12, C_DIM);
    k_snprintf(b, sizeof b, "%d SLICES%s", ns, ns && slice_mode[t] ? " AT THE HITS" : ns ? " ON A GRID" : "");
    g_text(14, 420, b, &f14, ns ? col : C_DIMMER);
    static const char *const hints[] = { "R1/R2", "tab", "D-PAD", "move", "A+\xe2\x86\x90\xe2\x86\x92", "change", "X", "listen", "Y", "browser", "B", "back" };
    w_footer(hints, COUNT(hints));
}
