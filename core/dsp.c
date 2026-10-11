#include "dsp.h"

static float bessel0(float x)
{
    float s = 1, t = 1;
    for (int k = 1; k < 30; ++k) { const float q = x / (float) (2 * k); t *= q * q; s += t; }
    return s;
}

void hb_design(halfband_t *hb, int taps, float beta)
{
    taps |= 1;
    if (taps > HB_MAX) taps = HB_MAX;
    m_set(hb, 0, sizeof *hb);
    hb->taps = taps;
    const int m = taps / 2;
    const float den = bessel0(beta);
    float sum = 0;
    for (int i = 0; i < taps; ++i) {
        const int n = i - m;
        const float sinc = n == 0 ? 0.5f : m_sin2pi((float) n * 0.25f) / (K_PI * (float) n);   // sin(pi n / 2) / (pi n)
        const float r = (float) n / (float) m;
        const float w = bessel0(beta * m_sqrt(MAX(0.0f, 1.0f - r * r))) / den;
        hb->h[i] = sinc * w;
        sum += hb->h[i];
    }
    for (int i = 0; i < taps; ++i) {
        hb->h[i] /= sum;
        if (hb->h[i] != 0.0f) hb->nz[hb->nnz++] = (uint8_t) i;
    }
    hb->centre = m;
    hb->first = (m & 1) ? 0 : 1;               // n = i - m odd is where the taps live
    if (((hb->first - m) & 1) == 0) hb->first ^= 1;
}
