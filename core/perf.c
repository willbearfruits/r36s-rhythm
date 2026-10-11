#include "perf.h"

const char *const perf_name[PF_COUNT] = { "REPEAT", "REVERSE", "TAPE STOP", "GATE", "FILTER", "CRUSH", "DUB", "FREEZE" };

void perf_init(perf_t *p, float sr)
{
    m_set(p, 0, sizeof *p);
    p->sr = sr;
    p->mode = -1;
}

void perf_set(perf_t *p, int fx, float amount) { if (fx >= 0 && fx < PF_COUNT) p->target[fx] = amount; }

bool perf_any(const perf_t *p)
{
    for (int k = 0; k < PF_COUNT; k++) if (m_fabs(p->target[k]) > 0.01f || m_fabs(p->amt[k]) > 0.01f) return true;
    return p->mix > 0.001f;
}

static void glide(perf_t *p, int n)
{
    const float k = 1.0f - m_exp(-(float) n / (0.01f * p->sr));   // 10 ms
    for (int f = 0; f < PF_COUNT; f++) p->amt[f] += (p->target[f] - p->amt[f]) * k;
}

void perf_sends(perf_t *p, const float *L, const float *R, float *dL, float *dR, float *rL, float *rR, int n, float *dub, bool *freeze)
{
    glide(p, n);
    const float d = MAX(p->amt[PF_DUB], 0.0f), f = MAX(p->amt[PF_FREEZE], 0.0f);
    *dub = d;
    *freeze = f > 0.5f;
    if (d > 0.01f) for (int i = 0; i < n; i++) { dL[i] += L[i] * d; dR[i] += R[i] * d; }
    if (f > 0.01f && f <= 0.5f) for (int i = 0; i < n; i++) { rL[i] += L[i] * f * 2; rR[i] += R[i] * f * 2; }
}

static inline float ring_at(const float *r, double pos)
{
    while (pos < 0) pos += PERF_RING;
    uint32_t i = (uint32_t) pos;
    const float fr = (float) (pos - (double) i);
    i %= PERF_RING;
    const uint32_t j = i + 1 < PERF_RING ? i + 1 : 0;
    return r[i] + (r[j] - r[i]) * fr;
}

void perf_main(perf_t *p, float *L, float *R, int n, float bpm)
{
    if (!perf_any(p)) {   // only remember: REPEAT and REVERSE start from what came before
        for (int i = 0; i < n; i++) { p->ring_l[p->w] = L[i]; p->ring_r[p->w] = R[i]; if (++p->w >= PERF_RING) p->w = 0; }
        return;
    }
    const float beat = 60.0f / MAX(bpm, 20.0f) * p->sr;
    // which time effect: the strongest of the three
    int want = -1; float best = 0.15f;
    for (int f = PF_REPEAT; f <= PF_TAPE; f++) if (p->target[f] > best) { best = p->target[f]; want = f; }
    if (want != p->mode && want >= 0) {   // it starts: from now
        if (want == PF_REPEAT) {
            const float a = p->target[PF_REPEAT];
            const float div = a < 0.4f ? 0.25f : a < 0.6f ? 0.125f : a < 0.8f ? 0.0625f : 0.03125f;
            p->rep_len = MAX(64u, (uint32_t) (beat * 4.0f * div));
            p->rep_start = (p->w + PERF_RING - p->rep_len) % PERF_RING;
            p->rep_pos = 0;
        } else if (want == PF_REVERSE) { p->rd = (double) p->w; p->rev_n = 0; }
        else { p->rd = (double) p->w; p->rate = 1.0; }
        p->mode = want;
    } else if (want < 0 && p->mix < 0.001f) p->mode = -1;
    if (p->mode == PF_REPEAT && want == PF_REPEAT) {   // the slice follows the stick
        const float a = p->target[PF_REPEAT];
        const float div = a < 0.4f ? 0.25f : a < 0.6f ? 0.125f : a < 0.8f ? 0.0625f : 0.03125f;
        p->rep_len = MAX(64u, (uint32_t) (beat * 4.0f * div));
    }
    const float mix_to = want >= 0 ? 1.0f : 0.0f, mix_k = 1.0f / (0.005f * p->sr);
    // gate, filter, crush settings
    const float g = p->amt[PF_GATE];
    const double gate_rate = g > 0.01f ? (g < 0.33f ? 2.0 : g < 0.66f ? 4.0 : 8.0) / beat : 0;   // eighths .. 32nds
    const float fa = p->amt[PF_FILTER];
    if (m_fabs(fa) > 0.01f) {
        const float cut = fa < 0 ? 20000.0f * m_exp2(fa * 7.6f) : 20.0f * m_exp2(fa * 8.6f);
        for (int c = 0; c < 2; c++) svf_set(&p->flt[c], cut, 1.4f, p->sr);
    }
    const float cr = p->amt[PF_CRUSH];
    const float steps = cr > 0.01f ? m_exp2(15.0f - cr * 12.0f) : 0, crate = 1.0f - cr * 0.92f;
    for (int i = 0; i < n; i++) {
        p->ring_l[p->w] = L[i]; p->ring_r[p->w] = R[i];
        if (++p->w >= PERF_RING) p->w = 0;
        float l = L[i], r = R[i];
        if (p->mode >= 0 || p->mix > 0) {
            float ql = l, qr = r;
            if (p->mode == PF_REPEAT) {
                const uint32_t k = (p->rep_start + p->rep_pos) % PERF_RING;
                ql = p->ring_l[k]; qr = p->ring_r[k];
                const uint32_t edge = MIN(96u, p->rep_len / 4);   // round the loop's corners
                if (p->rep_pos < edge) { const float u = (float) p->rep_pos / (float) edge; ql *= u; qr *= u; }
                if (p->rep_pos >= p->rep_len - edge) { const float u = (float) (p->rep_len - p->rep_pos) / (float) edge; ql *= u; qr *= u; }
                if (++p->rep_pos >= p->rep_len) p->rep_pos = 0;
            } else if (p->mode == PF_REVERSE) {
                const uint32_t seg = (uint32_t) (p->target[PF_REVERSE] > 0.75f ? beat * 0.5f : beat), edge = 96;
                p->rd -= 1.0;
                if (++p->rev_n >= seg) { p->rev_n = 0; p->rd = (double) p->w; }   // the last beat backwards, then the next
                ql = ring_at(p->ring_l, p->rd); qr = ring_at(p->ring_r, p->rd);
                const float u = p->rev_n < edge ? (float) p->rev_n / edge : p->rev_n > seg - edge ? (float) (seg - p->rev_n) / edge : 1.0f;
                ql *= u; qr *= u;
            } else if (p->mode == PF_TAPE) {
                const double to = 1.0 - MIN(p->target[PF_TAPE], 1.0f);
                p->rate += (to - p->rate) * 0.00008;          // the tape slows down, not at once
                p->rd += p->rate;
                ql = ring_at(p->ring_l, p->rd) * (float) MIN(1.0, p->rate * 4); qr = ring_at(p->ring_r, p->rd) * (float) MIN(1.0, p->rate * 4);
            }
            p->mix += (mix_to - p->mix) * MIN(1.0f, mix_k * 4);
            if (p->mix < 0) p->mix = 0;
            l += (ql - l) * p->mix; r += (qr - r) * p->mix;
        }
        if (gate_rate > 0) {
            p->gate_ph += gate_rate;
            if (p->gate_ph >= 1) p->gate_ph -= 1;
            const float open = p->gate_ph < 0.5 ? 1.0f : 1.0f - MIN(1.0f, g * 1.5f);
            l *= open; r *= open;
        }
        if (m_fabs(fa) > 0.01f) {
            svf_tick(&p->flt[0], l); svf_tick(&p->flt[1], r);
            const float w = MIN(1.0f, m_fabs(fa) * 6);
            const float fl = fa < 0 ? p->flt[0].lp : p->flt[0].hp, fr = fa < 0 ? p->flt[1].lp : p->flt[1].hp;
            l += (fl - l) * w; r += (fr - r) * w;
        }
        if (steps > 0) {
            p->crush_ph += crate;
            if (p->crush_ph >= 1) { p->crush_ph -= 1; p->hold_l = m_round(l * steps) / steps; p->hold_r = m_round(r * steps) / steps; }
            l = p->hold_l; r = p->hold_r;
        }
        L[i] = l; R[i] = r;
    }
}
