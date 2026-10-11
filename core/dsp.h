// Small DSP building blocks, allocation-free. Ported from thump's Dsp.h.
#pragma once
#include "lib.h"

static inline float soft_clip(float x) { x = m_clampf(x, -3.0f, 3.0f); return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }   // tanh-like

static inline float poly_blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Topology-preserving state-variable filter (Simper). All three outputs at once.
typedef struct { float g, k, a1, a2, a3, ic1, ic2, lp, bp, hp; } svf_t;
static inline void svf_set(svf_t *f, float fc, float q, float fs)
{
    fc = m_clampf(fc, 5.0f, fs * 0.49f);
    f->g = m_tan(K_PI * fc / fs);
    f->k = 1.0f / MAX(q, 0.05f);
    f->a1 = 1.0f / (1.0f + f->g * (f->g + f->k));
    f->a2 = f->g * f->a1;
    f->a3 = f->g * f->a2;
}
static inline void svf_reset(svf_t *f) { f->ic1 = f->ic2 = 0; }
static inline void svf_tick(svf_t *f, float x)
{
    const float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2 * v1 - f->ic1;
    f->ic2 = 2 * v2 - f->ic2;
    f->lp = v2; f->bp = v1; f->hp = x - f->k * v1 - v2;
}

// RBJ biquad, transposed direct form II.
typedef struct { float b0, b1, b2, a1, a2, z1, z2; } biquad_t;
static inline void bq_reset(biquad_t *q) { q->z1 = q->z2 = 0; }
static inline float bq_tick(biquad_t *q, float x)
{
    const float y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}
static inline void bq_set(biquad_t *q, float B0, float B1, float B2, float A0, float A1, float A2)
{
    q->b0 = B0 / A0; q->b1 = B1 / A0; q->b2 = B2 / A0; q->a1 = A1 / A0; q->a2 = A2 / A0;
}
static inline void bq_low_shelf(biquad_t *q, float f, float db, float fs)
{
    const float A = m_pow(10.0f, db / 40.0f), w = K_TWO_PI * f / fs, c = m_cos(w), s = m_sin(w);
    const float al = s / 2 * 1.41421356f, sa = 2 * m_sqrt(A) * al;
    bq_set(q, A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
           (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
}
static inline void bq_high_shelf(biquad_t *q, float f, float db, float fs)
{
    const float A = m_pow(10.0f, db / 40.0f), w = K_TWO_PI * f / fs, c = m_cos(w), s = m_sin(w);
    const float al = s / 2 * 1.41421356f, sa = 2 * m_sqrt(A) * al;
    bq_set(q, A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
           (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
}
static inline void bq_peak(biquad_t *q, float f, float db, float Q, float fs)
{
    const float A = m_pow(10.0f, db / 40.0f), w = K_TWO_PI * f / fs, c = m_cos(w), s = m_sin(w), al = s / (2 * Q);
    bq_set(q, 1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
}

// Half-band FIR decimator by 2 (Kaiser-windowed sinc). Every other tap is zero and skipped.
#define HB_MAX 63
typedef struct {
    float h[HB_MAX], line[HB_MAX * 2];
    uint8_t nz[HB_MAX];      // taps that are not zero
    int taps, nnz, pos;
    int first, centre;       // the non-zero taps: first, first + 2, ... and the centre one
} halfband_t;
void hb_design(halfband_t *hb, int taps, float beta);
static inline void hb_reset(halfband_t *hb) { m_set(hb->line, 0, sizeof hb->line); hb->pos = 0; }
static inline void hb_push(halfband_t *hb, float v)
{
    hb->line[hb->pos] = v;
    hb->line[hb->pos + hb->taps] = v;      // mirrored, so the window is always contiguous
    if (++hb->pos == hb->taps) hb->pos = 0;
}
// Takes 2n samples, writes n.
static inline void hb_process(halfband_t *hb, const float *in, float *out, int n)
{
    for (int i = 0; i < n; ++i) {
        hb_push(hb, in[2 * i]);
        hb_push(hb, in[2 * i + 1]);
        const float *x = &hb->line[hb->pos];
        float acc = hb->h[hb->centre] * x[hb->centre];
        for (int k = hb->first; k < hb->taps; k += 2) acc += hb->h[k] * x[k];
        out[i] = acc;
    }
}

// Decay that reaches exactly zero at `length` seconds. curve 0: snappy exponential, 1: close to linear.
typedef struct { float e, m, floor_, norm; } fdecay_t;
static inline void fdecay_start(fdecay_t *d, float len, float curve, float fs)
{
    len = MAX(len, 1e-4f);
    const float tau = len * (0.06f + 0.94f * curve * curve);
    d->m = m_exp(-1.0f / (tau * fs));
    d->e = 1;
    d->floor_ = m_exp(-len / tau);
    d->norm = 1.0f / (1.0f - d->floor_);
}
static inline float fdecay_tick(fdecay_t *d)
{
    const float v = (d->e - d->floor_) * d->norm;
    d->e *= d->m;
    return v > 0 ? v : 0.0f;
}

// One-pole smoother.
typedef struct { float y, a; } smooth_t;
static inline void smooth_set(smooth_t *s, float ms, float fs) { s->a = 1.0f - m_exp(-1.0f / (MAX(ms, 0.01f) * 0.001f * fs)); }
static inline float smooth_tick(smooth_t *s, float x) { s->y += (x - s->y) * s->a; return s->y; }
