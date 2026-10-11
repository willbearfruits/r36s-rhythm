#include "smp.h"

void smp_init(smp_track_t *t, float sr)
{
    m_set(t, 0, sizeof *t);
    t->sr = sr;
}

bool smp_active(const smp_track_t *t)
{
    for (int i = 0; i < SMP_VOICES; i++) if (t->v[i].on) return true;
    return false;
}

void smp_slice_range(const smp_t *p, int k, float *a, float *b)
{
    const float s = m_clampf(p->start, 0, 1), e = m_clampf(p->end, s, 1);
    const int n = p->slices;
    if (n <= 0 || k <= 0) { *a = s; *b = e; return; }
    k = CLAMP(k, 1, n);
    bool manual = false;
    for (int i = 1; i < n && i < NSLICE; i++) if (p->slice_pos[i] > 0) manual = true;
    float u0, u1;
    if (manual) { u0 = k == 1 ? 0 : p->slice_pos[k - 1]; u1 = k == n ? 1 : p->slice_pos[k]; }
    else { u0 = (float) (k - 1) / (float) n; u1 = (float) k / (float) n; }
    *a = s + (e - s) * u0;
    *b = s + (e - s) * u1;
}

void smp_choke(smp_track_t *t)
{
    for (int i = 0; i < SMP_VOICES; i++)
        if (t->v[i].on && t->v[i].fade_step == 0) t->v[i].fade_step = 1.0f / (0.004f * t->sr);
}

void smp_trigger(smp_track_t *t, const smp_t *p, float vel, int slice)
{
    const sample_t *s = t->s;
    if (!s || !s->data || s->frames < 4) return;
    if (p->choke_self) smp_choke(t);
    smp_voice_t *v = NULL;
    for (int i = 0; i < SMP_VOICES; i++) if (!t->v[i].on) { v = &t->v[i]; break; }
    if (!v) {   // all busy: the quietest goes
        v = &t->v[0];
        for (int i = 1; i < SMP_VOICES; i++) if (t->v[i].fade * t->v[i].env < v->fade * v->env) v = &t->v[i];
    }
    m_set(v, 0, sizeof *v);
    float u0, u1;
    smp_slice_range(p, slice, &u0, &u1);
    const double n = (double) s->frames;
    v->a = u0 * n; v->b = MAX(u1 * n, v->a + 2);
    v->loop = p->mode == SMP_LOOP;
    v->loop_a = v->a + (v->b - v->a) * m_clampf(p->loop, 0, 0.98f);
    const double inc = (double) s->rate / t->sr * m_semi(m_clampf(p->pitch, -48, 48));
    if (p->reverse) { v->pos = v->b - 1; v->inc = -inc; }
    else { v->pos = v->a; v->inc = inc; }
    v->on = true;
    v->fade = 1;
    v->gain = m_pow(m_clampf(vel, 0, 1), 1.3f) * m_db(p->gain);
    v->att_n = (int) (p->attack * 0.001f * t->sr);
    v->hold_n = (int) (p->hold * 0.001f * t->sr);
    v->dec_n = p->decay > 0 ? MAX(1, (int) (p->decay * 0.001f * t->sr)) : 0;
    v->filter_mode = p->filter_mode;
    v->cutoff = m_clampf(p->filter, 20, 20000);
    v->reso = m_clampf(p->reso, 0, 1);
    for (int c = 0; c < 2; c++) svf_set(&v->flt[c], v->cutoff, 0.5f + v->reso * v->reso * 12.0f, t->sr);
    const float bits = m_clampf(p->bits, 2, 16);
    v->step_q = bits >= 15.99f ? 0 : m_exp2(bits - 1);
    v->rate_inc = m_clampf(p->rate, 500, t->sr) / t->sr;
    v->rate_ph = 1;
    v->drive = m_clampf(p->drive, 0, 1);
}

// 4-point, 3rd-order Hermite.
static inline float herm(float xm1, float x0, float x1, float x2, float f)
{
    const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

static inline float at(const sample_t *s, int64_t i, int c)
{
    if (i < 0) i = 0;
    if (i >= (int64_t) s->frames) i = (int64_t) s->frames - 1;
    return (float) s->data[(size_t) i * s->chans + (size_t) (s->chans > 1 ? c : 0)] * (1.0f / 32768.0f);
}

void smp_render(smp_track_t *t, float *L, float *R, int n)
{
    const sample_t *s = t->s;
    float pk = t->peak * 0.9f;
    if (!s || !s->data) { for (int i = 0; i < SMP_VOICES; i++) t->v[i].on = false; t->peak = pk; return; }
    const float edge = 0.002f * t->sr;   // fade at the region's ends, against clicks
    for (int k = 0; k < SMP_VOICES; k++) {
        smp_voice_t *v = &t->v[k];
        if (!v->on) continue;
        const bool filt = !(v->filter_mode == FLT_LP && v->cutoff >= 19999.0f && v->reso < 0.01f);
        for (int i = 0; i < n; i++) {
            // envelope
            float e;
            if (v->t < v->att_n) e = (float) v->t / (float) v->att_n;
            else if (v->dec_n == 0 || v->t < v->att_n + v->hold_n) e = 1;
            else {
                const float u = (float) (v->t - v->att_n - v->hold_n) / (float) v->dec_n;
                if (u >= 1) { v->on = false; break; }
                e = (1 - u) * (1 - u);
            }
            v->env = e;
            // read
            const double p = v->pos;
            const int64_t ip = (int64_t) p;              // p >= 0: truncation is the floor
            const float f = (float) (p - (double) ip);
            float xl = herm(at(s, ip - 1, 0), at(s, ip, 0), at(s, ip + 1, 0), at(s, ip + 2, 0), f);
            float xr = s->chans > 1 ? herm(at(s, ip - 1, 1), at(s, ip, 1), at(s, ip + 1, 1), at(s, ip + 2, 1), f) : xl;
            // bits and rate: hold each value for a while, then fewer levels
            if (v->rate_inc < 0.999f || v->step_q > 0) {
                v->rate_ph += v->rate_inc;
                if (v->rate_ph >= 1.0f) {
                    v->rate_ph -= m_floor(v->rate_ph);
                    v->hold_l = xl; v->hold_r = xr;
                    if (v->step_q > 0) { v->hold_l = m_round(xl * v->step_q) / v->step_q; v->hold_r = m_round(xr * v->step_q) / v->step_q; }
                }
                xl = v->hold_l; xr = v->hold_r;
            }
            if (filt) {
                svf_tick(&v->flt[0], xl); svf_tick(&v->flt[1], xr);
                xl = v->filter_mode == FLT_LP ? v->flt[0].lp : v->filter_mode == FLT_BP ? v->flt[0].bp : v->flt[0].hp;
                xr = v->filter_mode == FLT_LP ? v->flt[1].lp : v->filter_mode == FLT_BP ? v->flt[1].bp : v->flt[1].hp;
            }
            if (v->drive > 0.001f) {
                const float g = 1 + v->drive * v->drive * 12;
                xl = soft_clip(xl * g) / (1 + v->drive * 1.5f); xr = soft_clip(xr * g) / (1 + v->drive * 1.5f);
            }
            // the ends of the region
            float ef = 1;
            if (!v->loop) {
                const double left = v->inc > 0 ? v->b - p : p - v->a;
                if (left < edge) ef = (float) (left / edge);
            }
            const float g = v->gain * e * v->fade * ef;
            L[i] += xl * g; R[i] += xr * g;
            pk = MAX(pk, m_fabs(xl * g));
            // advance
            v->pos += v->inc;
            if (v->inc > 0 && v->pos >= v->b) { if (v->loop) v->pos = v->loop_a + (v->pos - v->b); else v->on = false; }
            if (v->inc < 0 && v->pos < v->a) { if (v->loop) v->pos = v->b - 1; else v->on = false; }
            v->t++;
            if (v->fade_step > 0) { v->fade -= v->fade_step; if (v->fade <= 0) v->on = false; }
            if (!v->on) break;
        }
    }
    t->peak = pk;
}
