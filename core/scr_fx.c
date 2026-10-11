// FX: the tape delay and the reverb, and every track's sends (docs/mockups/screens/17).
// MASTER: the bus compressor, tone, drive, the output's rate and bits, the limiter; the spectrum and the scope (18).
#include "ui_int.h"

static int delay_prms(prm_t *p)
{
    int n = 0;
    p[n++] = prm_choice("Time", &doc.dly.sync, "1/16|1/8t|1/8|3/16|1/4t|1/4|3/8|1/2|free");
    if (doc.dly.sync >= DLY_SYNC_COUNT) p[n++] = prm_f("Free time", &doc.dly.time, 0.01f, 2.4f, 0.01f, UN_SEC);
    p[n++] = prm_f("Repeats", &doc.dly.repeats, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Tone", &doc.dly.tone, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Return", &doc.dly.mix, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Wobble", &doc.dly.wobble, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Depth", &doc.dly.depth, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Dirt", &doc.dly.dirt, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_choice("Ping pong", &doc.dly.pingpong, "off|on");
    return n;
}

static int reverb_prms(prm_t *p)
{
    int n = 0;
    p[n++] = prm_f("Size", &doc.rev.size, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Decay", &doc.rev.decay, 0.1f, 20, 0.1f, UN_SEC);
    p[n++] = prm_f("Damp", &doc.rev.damp, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Pre", &doc.rev.pre, 0, 100, 1, UN_MS);
    p[n++] = prm_f("Width", &doc.rev.width, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Move", &doc.rev.move, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_f("Return", &doc.rev.ret, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_choice("Freeze", &doc.rev.freeze, "off|on");
    return n;
}

// FX: R1/R2 choose DELAY, REVERB or SENDS; in SENDS left/right is the track, up/down the send.
void fx_button(int b, bool down, bool rep)
{
    (void) rep;
    if (!down) return;
    int *tab = &ui.tab[NAV_FX];
    if (b == BTN_R1) { *tab = (*tab + 2) % 3; ui.sel[NAV_FX] = 0; return; }
    if (b == BTN_R2) { *tab = (*tab + 1) % 3; ui.sel[NAV_FX] = 0; return; }
    if (b == BTN_B) { go(NAV_PATTERN); return; }
    prm_t p[24];
    int n = 0;
    if (*tab == 0) n = delay_prms(p);
    else if (*tab == 1) n = reverb_prms(p);
    else {
        for (int t = 0; t < NTRK; t++) { p[n++] = prm_f("Delay", &doc.trk[t].send_dly, 0, 1, 0.02f, UN_PCT); }
        for (int t = 0; t < NTRK; t++) { p[n++] = prm_f("Reverb", &doc.trk[t].send_rev, 0, 1, 0.02f, UN_PCT); }
        prm_nav(p, n, &ui.sel[NAV_FX], NTRK, b, down);
        return;
    }
    prm_nav(p, n, &ui.sel[NAV_FX], 4, b, down);
}

void draw_fx(void)
{
    g_clear(C_BG);
    w_header("FX", nav_col(NAV_FX));
    const int tab = ui.tab[NAV_FX];
    char b[32];
    // delay
    w_box(4, 36, 316, 300, NULL, C_DIM, tab == 0 ? c_mix(C_PANEL, C_AMBER, 0.08f) : C_PANEL, NULL);
    w_chip(12, 42, "DELAY \xc2\xb7 TAPE", &f14, C_INK, C_AMBER, 5, 18);
    g_text(12, 64, "Modelled on the PT2399 pedal.", &f12, C_DIM);
    g_rect(12, 82, 308, 150, RGB(12, 12, 20)); g_frame(12, 82, 308, 150, C_LINE, 1);
    const float beats = doc.dly.sync < DLY_SYNC_COUNT ? dly_time_beats(doc.dly.sync) : doc.dly.time * doc.bpm / 60.0f;
    const float sec = beats * 60.0f / doc.bpm, step = MAX(8.0f, MIN(64.0f, sec * 120.0f));
    float h = 60;
    for (int k = 0; k < 12 && 20 + (int) (k * step) < 300; k++) {
        const int x = 20 + (int) ((float) k * step) + (int) (3 * m_sin((float) k * 1.7f) * doc.dly.wobble);
        g_rect(x, 148 - (int) h, x + 6, 148, c_mix(C_AMBER, RGB(110, 50, 20), (float) k / 10.0f * (1 - doc.dly.tone)));
        h *= doc.dly.repeats * 1.05f;
        if (h < 1) break;
    }
    k_snprintf(b, sizeof b, "%.0f ms", (double) (sec * 1000));
    g_text_r(300, 86, b, &f12b, C_AMBER);
    prm_t p[24];
    int n = delay_prms(p);
    w_knobs(p, n, tab == 0 ? ui.sel[NAV_FX] : -1, 46, 188, 4, 74, 66, 20, C_AMBER);
    // reverb
    w_box(324, 36, 636, 300, NULL, C_DIM, tab == 1 ? c_mix(C_PANEL, C_SKY, 0.08f) : C_PANEL, NULL);
    w_chip(332, 42, "REVERB", &f14, C_INK, C_SKY, 5, 18);
    g_text(332, 64, "Eight delay lines feeding each other.", &f12, C_DIM);
    g_rect(332, 82, 628, 150, RGB(12, 12, 20)); g_frame(332, 82, 628, 150, C_LINE, 1);
    rng_t r; rng_seed(&r, 2);
    const float len = MAX(0.2f, doc.rev.decay / 4.0f);
    for (int k = 0; k < 294; k++) {
        const float a = (doc.rev.freeze ? 0.7f : m_exp(-(float) k / (294.0f * len) * 3.0f)) * (1 - m_exp(-(float) k / 6.0f)) * (0.5f + 0.5f * rng_uni(&r));
        g_line(334 + k, 148 - (int) (60 * a), 334 + k, 148, c_mix(C_SKY, RGB(20, 40, 80), (float) k / 294.0f * (0.5f + doc.rev.damp * 0.5f)), 1);
    }
    k_snprintf(b, sizeof b, "%.1f s", (double) doc.rev.decay);
    g_text_r(620, 86, b, &f12b, C_SKY);
    n = reverb_prms(p);
    w_knobs(p, n, tab == 1 ? ui.sel[NAV_FX] : -1, 366, 188, 4, 74, 66, 20, C_SKY);
    // the sends
    w_box(4, 306, 636, 452, "SENDS", tab == 2 ? C_TEXT : C_DIM, tab == 2 ? c_mix(C_PANEL, C_WHITE, 0.04f) : C_PANEL, "DELAY / REVERB");
    for (int t = 0; t < NTRK; t++) {
        const int x = 18 + t * 51;
        const float v[2] = { doc.trk[t].send_dly, doc.trk[t].send_rev };
        for (int k = 0; k < 2; k++) {
            g_rect(x + k * 14, 330, x + k * 14 + 9, 420, RGB(26, 26, 38));
            g_rect(x + k * 14, 420 - (int) (90 * v[k]), x + k * 14 + 9, 420, k ? C_SKY : C_AMBER);
            if (tab == 2 && ui.sel[NAV_FX] == k * NTRK + t) g_frame(x + k * 14 - 2, 328, x + k * 14 + 11, 422, C_WHITE, 1);
        }
        char nm[6]; k_strcpy(nm, doc.trk[t].name, sizeof nm);
        g_text_c(x + 12, 426, nm, &f12b, trk_col[t]);
    }
    static const char *const hints[] = { "R1/R2", "delay/reverb/sends", "D-PAD", "move", "A+\xe2\x86\x90\xe2\x86\x92", "change", "B", "back" };
    w_footer(hints, COUNT(hints));
}

// ----------------------------------------------------------------------------------------------- MASTER

static int master_prms(prm_t *p)
{
    int n = 0;
    p[n++] = prm_f("Thresh", &doc.mst.thresh, -40, 0, 0.5f, UN_DB);
    p[n++] = prm_f("Ratio", &doc.mst.ratio, 1, 20, 0.25f, UN_X);
    p[n++] = prm_f("Attack", &doc.mst.attack, 0.1f, 100, 0.5f, UN_MS);
    p[n++] = prm_f("Release", &doc.mst.release, 10, 1000, 5, UN_MS);
    p[n++] = prm_f("Makeup", &doc.mst.makeup, 0, 18, 0.5f, UN_DB);
    p[n++] = prm_flog("Low cut", &doc.mst.low_cut, 20, 500, UN_HZ);
    p[n++] = prm_flog("High cut", &doc.mst.high_cut, 1000, 20000, UN_HZ);
    p[n++] = prm_f("Drive", &doc.mst.drive, 0, 1, 0.02f, UN_PCT);
    p[n++] = prm_choice("Drive type", &doc.mst.drive_type, "warm|tape|fold");
    p[n++] = prm_f("Width", &doc.mst.width, 0, 2, 0.05f, UN_PCT);
    p[n++] = prm_choice("Rate", &doc.mst.out_rate, "clean|32k|26k|22k|12k|8k");
    p[n++] = prm_u8("Bits", &doc.mst.out_bits, 4, 16, UN_INT);
    p[n++] = prm_f("Limit", &doc.mst.limit, -12, 0, 0.1f, UN_DB);
    p[n++] = prm_f("Volume", &doc.mst.volume, -40, 6, 0.5f, UN_DB);
    return n;
}

void master_button(int b, bool down, bool rep)
{
    (void) rep;
    if (down && b == BTN_B) { go(NAV_PATTERN); return; }
    prm_t p[16];
    const int n = master_prms(p);
    prm_nav(p, n, &ui.sel[NAV_MASTER], 5, b, down);
}

// A 512-point FFT of the scope, for the spectrum.
static void fft(float *re, float *im, int n)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const float a = -K_TWO_PI / (float) len, wr0 = m_cos(a), wi0 = m_sin(a);
        for (int i = 0; i < n; i += len) {
            float wr = 1, wi = 0;
            for (int j = 0; j < len / 2; j++) {
                const int u = i + j, v = i + j + len / 2;
                const float xr = re[v] * wr - im[v] * wi, xi = re[v] * wi + im[v] * wr;
                re[v] = re[u] - xr; im[v] = im[u] - xi; re[u] += xr; im[u] += xi;
                const float t = wr * wr0 - wi * wi0; wi = wr * wi0 + wi * wr0; wr = t;
            }
        }
    }
}

void draw_master(void)
{
    g_clear(C_BG);
    const col_t col = nav_col(NAV_MASTER);
    w_header("MASTER", col);
    prm_t p[16];
    const int n = master_prms(p);
    w_box(4, 36, 636, 254, NULL, C_DIM, C_PANEL, NULL);
    static const char *const groups[3] = { "COMPRESSOR", "TONE \xc2\xb7 DRIVE", "OUTPUT \xc2\xb7 LIMITER" };
    for (int k = 0; k < 3; k++) g_text(14 + k * 212, 42, groups[k], &f12b, k == 2 ? C_AMBER : col);
    w_knobs(p, n, ui.sel[NAV_MASTER], 58, 78, 5, 126, 58, 17, col);
    char b[32];
    k_snprintf(b, sizeof b, "GR %.1f dB", (double) ui.st.comp_gr);
    g_text(14, 236, b, &f12b, ui.st.comp_gr > 0.5f ? C_AMBER : C_DIM);
    w_meter_h(100, 238, 300, MIN(ui.st.comp_gr / 12.0f, 1.0f), C_AMBER, 6);
    k_snprintf(b, sizeof b, "LIMIT %.1f dB", (double) ui.st.limit_gr);
    g_text(330, 236, b, &f12b, ui.st.limit_gr > 0.5f ? C_AMBER : C_DIM);
    g_text_r(628, 236, doc.mst.out_rate ? "THE OUTPUT IS GRITTY" : "THE OUTPUT IS CLEAN", &f12, doc.mst.out_rate ? C_AMBER : C_DIM);
    // spectrum
    w_box(4, 260, 424, 452, "SPECTRUM", C_DIM, C_PANEL, NULL);
    static float re[512], im[512], smooth[80];
    const uint32_t head = atomic_load(&eng_scope_head);
    for (int i = 0; i < 512; i++) {
        const uint32_t k = (head - 512 + (uint32_t) i) & (SCOPE_LEN - 1);
        const float w = 0.5f - 0.5f * m_cos(K_TWO_PI * (float) i / 511.0f);
        re[i] = (eng_scope_l[k] + eng_scope_r[k]) * 0.5f * w; im[i] = 0;
    }
    fft(re, im, 512);
    for (int k = 0; k < 80; k++) {
        const float f0 = 40.0f * m_pow(400.0f, (float) k / 80.0f), f1 = 40.0f * m_pow(400.0f, (float) (k + 1) / 80.0f);
        const int a = CLAMP((int) (f0 / 48000.0f * 512.0f), 1, 255), e = CLAMP((int) (f1 / 48000.0f * 512.0f), a + 1, 256);
        float mag = 0;
        for (int i = a; i < e; i++) mag = MAX(mag, re[i] * re[i] + im[i] * im[i]);
        const float db = 10.0f * m_log10(mag + 1e-12f) - 42.1f;   // the transform's gain (a Hann window, 512 points)
        const float v = m_clampf((db + 72.0f) / 72.0f, 0, 1);
        smooth[k] = MAX(v, smooth[k] * 0.85f);
        const int x = 12 + k * 5, yt = 432 - (int) (150.0f * smooth[k]);
        const col_t c = trk_col[MIN(11, k * 12 / 80)];
        g_rect(x, yt, x + 3, 432, c_scale(c, 0.85f));
        g_rect(x, yt, x + 3, yt + 1, c_mix(c, C_WHITE, 0.5f));
    }
    static const struct { const char *s; int f; } marks[] = { { "50", 50 }, { "200", 200 }, { "1k", 1000 }, { "5k", 5000 }, { "15k", 15000 } };
    for (int k = 0; k < 5; k++) { const int x = 12 + (int) (m_log2((float) marks[k].f / 40.0f) / m_log2(400.0f) * 400.0f); g_text(x, 436, marks[k].s, &f12, C_DIMMER); }
    // scope (left against right)
    w_box(428, 260, 530, 452, "SCOPE", C_DIM, C_PANEL, NULL);
    for (int i = 0; i < 512; i++) {
        const uint32_t k = (head - 512 + (uint32_t) i) & (SCOPE_LEN - 1);
        const float l = eng_scope_l[k], r = eng_scope_r[k];
        g_pixel(479 + (int) ((l - r) * 0.7f * 40), 360 - (int) ((l + r) * 0.7f * 40), C_GREEN);
    }
    // the output
    w_box(534, 260, 636, 452, "OUT", C_DIM, C_PANEL, NULL);
    w_meter_v(556, 282, 420, MIN(ui.st.out_l, 1.0f), C_GREEN, 16);
    w_meter_v(588, 282, 420, MIN(ui.st.out_r, 1.0f), C_GREEN, 16);
    k_snprintf(b, sizeof b, "%.1f", (double) m_todb(ui.st.out_l));
    g_text(546, 426, b, &f12, C_TEXT);
    k_snprintf(b, sizeof b, "%.1f", (double) m_todb(ui.st.out_r));
    g_text(584, 426, b, &f12, C_TEXT);
    static const char *const hints[] = { "D-PAD", "move", "A+\xe2\x86\x90\xe2\x86\x92", "change", "B", "back" };
    w_footer(hints, COUNT(hints));
}
