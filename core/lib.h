// The only thing between the core and a C library: types, a few helpers and our own maths, so the core builds
// hosted (the PC, Linux on the R36S) and freestanding (bare metal) and gives the same numbers everywhere.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))
#define COUNT(a) ((int) (sizeof(a) / sizeof((a)[0])))

#define m_copy __builtin_memcpy
#define m_set __builtin_memset
#define m_move __builtin_memmove
#define m_cmp __builtin_memcmp

int    k_strlen(const char *s);
int    k_strcmp(const char *a, const char *b);
int    k_strncmp(const char *a, const char *b, int n);
void   k_strcpy(char *dst, const char *src, int cap);       // always terminates
void   k_strcat(char *dst, const char *src, int cap);
// A small printf: %d %i %u %x %X %c %s %f (%.Nf, width and '-', '0', '+' flags), %%.
int    k_snprintf(char *out, int cap, const char *fmt, ...);

// ---- maths --------------------------------------------------------------------------------------------------
#define K_PI 3.14159265358979f
#define K_TWO_PI 6.28318530717959f

static inline float m_fabs(float x) { return __builtin_fabsf(x); }
static inline float m_sqrt(float x) { return __builtin_sqrtf(x); }
static inline float m_floor(float x)
{
    if (!(x > -2147483520.0f && x < 2147483520.0f)) return x;   // already integral (or NaN)
    float t = (float) (int32_t) x;
    return t > x ? t - 1.0f : t;
}
static inline float m_frac(float x) { return x - m_floor(x); }
static inline float m_round(float x) { return m_floor(x + 0.5f); }
static inline int m_lround(float x) { return (int) m_floor(x + 0.5f); }
static inline float m_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline float m_copysign(float a, float s) { return __builtin_copysignf(a, s); }

// Two tiers, like thump: exact versions (a couple of ulp) for coefficients, where a per-sample multiplier such as
// exp(-1 / (tau fs)) = 0.9993 needs every bit, and thump's fast ones inside the per-sample loops.

// sin(2 pi p), p in cycles, any range. Fast (thump's sin2pi), error below 1e-4: oscillators.
static inline float m_sin2pi(float p)
{
    p -= m_floor(p + 0.5f);
    const float a = m_fabs(p);
    p = a > 0.25f ? m_copysign(0.5f, p) - p : p;
    const float x = p * K_TWO_PI, x2 = x * x;
    return x * (1.f + x2 * (-0.16666648f + x2 * (0.0083321609f + x2 * -0.00019515296f)));
}
// The exact one: Taylor to x^13 on [-pi/2, pi/2].
static inline float m_sin2pi_hq(float p)
{
    p -= m_floor(p + 0.5f);
    const float a = m_fabs(p);
    p = a > 0.25f ? m_copysign(0.5f, p) - p : p;
    const float x = p * K_TWO_PI, x2 = x * x;
    return x * (1.f + x2 * (-1.6666667e-1f + x2 * (8.3333333e-3f + x2 * (-1.9841270e-4f + x2 * (2.7557319e-6f
                + x2 * (-2.5052108e-8f + x2 * 1.6059044e-10f))))));
}
static inline float m_sin(float x) { return m_sin2pi_hq(x * (1.0f / K_TWO_PI)); }
static inline float m_cos(float x) { return m_sin2pi_hq(x * (1.0f / K_TWO_PI) + 0.25f); }

static inline float m_pow2i(int n)           // 2^n, -126 <= n <= 127
{
    union { float f; uint32_t u; } e;
    e.u = (uint32_t) (n + 127) << 23;
    return e.f;
}
// 2^x, fast (thump's exp2f_fast), relative error below 1e-4: per-sample pitch sweeps.
static inline float m_exp2_fast(float x)
{
    x = m_clampf(x, -60.0f, 60.0f);
    const float fi = m_floor(x), f = x - fi;
    const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
    return p * m_pow2i((int) fi);
}
// 2^x, exact: Taylor to x^7 on [-0.5, 0.5].
static inline float m_exp2(float x)
{
    x = m_clampf(x, -100.0f, 100.0f);
    const float n = m_floor(x + 0.5f), f = x - n;
    const float p = 1.0f + f * (6.9314718e-1f + f * (2.4022651e-1f + f * (5.5504109e-2f + f * (9.6181291e-3f
                    + f * (1.3333558e-3f + f * (1.5403530e-4f + f * 1.5252734e-5f))))));
    return p * m_pow2i((int) n);
}

// log2(x) for x > 0, absolute error about 1e-6.
static inline float m_log2(float x)
{
    if (!(x > 0)) return -60.0f;
    union { float f; uint32_t u; } v = { x };
    int e = (int) ((v.u >> 23) & 255) - 127;
    v.u = (v.u & 0x007fffffu) | 0x3f800000u;                 // mantissa in [1, 2)
    float m = v.f;
    if (m > 1.41421356f) { m *= 0.5f; e += 1; }              // now in [0.707, 1.414)
    const float t = (m - 1.0f) / (m + 1.0f), t2 = t * t;    // atanh series, |t| < 0.172
    const float ln = 2.0f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * (0.14285714f + t2 * 0.11111111f))));
    return ln * 1.44269504f + (float) e;
}

static inline float m_exp(float x) { return m_exp2(x * 1.44269504f); }
static inline float m_log(float x) { return m_log2(x) * 0.69314718f; }
static inline float m_log10(float x) { return m_log2(x) * 0.30103f; }
static inline float m_pow(float a, float b) { return a > 0 ? m_exp2(b * m_log2(a)) : 0.0f; }
static inline float m_tan(float x) { return m_sin(x) / m_cos(x); }
static inline float m_tanh(float x)
{
    if (x > 9.0f) return 1.0f;
    if (x < -9.0f) return -1.0f;
    const float e = m_exp2(x * 2.88539008f);                 // e^(2x)
    return (e - 1.0f) / (e + 1.0f);
}
// atan2, error below 1e-4 rad (enough for pixels and knobs).
static inline float m_atan2(float y, float x)
{
    const float ax = m_fabs(x), ay = m_fabs(y);
    if (ax == 0 && ay == 0) return 0;
    const float a = MIN(ax, ay) / MAX(ax, ay), s = a * a;
    float r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    return y < 0 ? -r : r;
}
static inline float m_db(float db) { return m_exp2(db * 0.16609640f); }              // dB to gain
static inline float m_todb(float g) { return 20.0f * m_log10(MAX(g, 1e-9f)); }
static inline float m_semi(float st) { return m_exp2(st * (1.0f / 12.0f)); }        // semitones to ratio

// Deterministic noise. xorshift32.
typedef struct { uint32_t s; } rng_t;
static inline void rng_seed(rng_t *r, uint32_t v) { r->s = v ? v : 0x9E3779B9u; }
static inline uint32_t rng_next(rng_t *r) { uint32_t s = r->s; s ^= s << 13; s ^= s >> 17; s ^= s << 5; r->s = s; return s; }
static inline float rng_bi(rng_t *r) { return (float) (rng_next(r) >> 8) * (2.0f / 16777216.0f) - 1.0f; }
static inline float rng_uni(rng_t *r) { return (float) (rng_next(r) >> 8) * (1.0f / 16777216.0f); }
static inline uint32_t hash32(uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
    return x;
}
