#include "fx.h"

const float out_rates[OUT_COUNT] = { 0, 32000, 26040, 22050, 12000, 8000 };
const char *const out_names[OUT_COUNT] = { "CLEAN", "32k", "26k", "22k", "12k", "8k" };

// ---------------------------------------------------------------------------------------------------- delay

float dly_time_beats(int sync)
{
    static const float beats[DLY_SYNC_COUNT] = { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f };
    return beats[CLAMP(sync, 0, DLY_SYNC_COUNT - 1)];
}

void dly_init(delay_fx_t *d, float sr)
{
    m_set(d, 0, sizeof *d);
    d->sr = sr;
    d->len = MIN(DLY_MAX, (int) (sr * 2.5f));
    d->time = sr * 0.3f;
    rng_seed(&d->rng, 2399);
}

static inline float rd(const float *b, int len, float pos)
{
    while (pos < 0) pos += (float) len;
    int i = (int) pos;
    const float f = pos - (float) i;
    if (i >= len) i -= len;
    const int j = i + 1 < len ? i + 1 : 0;
    return b[i] + (b[j] - b[i]) * f;
}

void dly_process(delay_fx_t *d, const delay_t *p, float bpm, const float *inL, const float *inR, float *outL, float *outR, int n)
{
    const float target = p->sync < DLY_SYNC_COUNT ? dly_time_beats(p->sync) * 60.0f / MAX(bpm, 20.0f) * d->sr
                                                   : m_clampf(p->time, 0.01f, 2.4f) * d->sr;
    const float fb = m_clampf(p->repeats, 0, 1) * 1.05f;                       // a little past 1: it can run away
    const float cut = 500.0f * m_exp2(m_clampf(p->tone, 0, 1) * 5.0f);         // 500 Hz .. 16 kHz
    for (int c = 0; c < 2; c++) { svf_set(&d->tone[c], cut, 0.6f, d->sr); svf_set(&d->hp[c], 90.0f, 0.6f, d->sr); }
    const float depth = m_clampf(p->depth, 0, 1) * 0.004f * d->sr;             // up to +-4 ms
    const float rate = 0.05f + m_clampf(p->wobble, 0, 1) * 6.0f;
    const float dirt = m_clampf(p->dirt, 0, 1), mix = m_clampf(p->mix, 0, 1);
    const float maxd = (float) d->len - 4;
    for (int i = 0; i < n; i++) {
        d->time += (m_clampf(target, 16, maxd - depth * 2) - d->time) * 0.0004f;   // tape: a change glides
        if (--d->drift_n <= 0) { d->drift_to = rng_bi(&d->rng) * 0.3f; d->drift_n = (int) (d->sr * 0.2f); }
        d->drift += (d->drift_to - d->drift) * 0.0002f;
        d->lfo += rate / d->sr;
        if (d->lfo >= 1) d->lfo -= 1;
        const float t = d->time + depth * (m_sin2pi(d->lfo) + d->drift);
        float yl = rd(d->buf[0], d->len, (float) d->w - t), yr = rd(d->buf[1], d->len, (float) d->w - t);
        // the loop: darker every time round, no rumble, a little dirt
        svf_tick(&d->tone[0], yl); svf_tick(&d->tone[1], yr);
        float fl = d->tone[0].lp, fr = d->tone[1].lp;
        svf_tick(&d->hp[0], fl); svf_tick(&d->hp[1], fr);
        fl = d->hp[0].hp; fr = d->hp[1].hp;
        if (dirt > 0) {
            const float g = 1 + dirt * 3;
            fl = soft_clip(fl * g) / g * (1 + dirt * 0.4f) + rng_bi(&d->rng) * dirt * 2e-4f;
            fr = soft_clip(fr * g) / g * (1 + dirt * 0.4f) + rng_bi(&d->rng) * dirt * 2e-4f;
        }
        fl = soft_clip(fl * fb); fr = soft_clip(fr * fb);
        if (p->pingpong) {
            d->buf[0][d->w] = (inL[i] + inR[i]) * 0.5f + fr;
            d->buf[1][d->w] = fl;
        } else {
            d->buf[0][d->w] = inL[i] + fl;
            d->buf[1][d->w] = inR[i] + fr;
        }
        if (++d->w >= d->len) d->w = 0;
        outL[i] += yl * mix;
        outR[i] += yr * mix;
    }
}

// ---------------------------------------------------------------------------------------------------- reverb

static const int rev_base[REV_LINES] = { 1031, 1327, 1523, 1801, 2063, 2333, 2683, 3049 };
static const int ap_base[4] = { 142, 107, 379, 277 };

void rev_init(reverb_fx_t *r, float sr)
{
    m_set(r, 0, sizeof *r);
    r->sr = sr;
    for (int k = 0; k < 4; k++) r->aplen[k] = MIN(799, (int) (ap_base[k] * sr / 48000.0f));
    r->quiet = 1 << 30;
}

void rev_process(reverb_fx_t *r, const reverb_t *p, const float *inL, const float *inR, float *outL, float *outR, int n)
{
    float in_e = 0;
    for (int i = 0; i < n; i++) in_e += m_fabs(inL[i]) + m_fabs(inR[i]);
    if (in_e == 0 && !p->freeze && r->quiet > (int) (r->sr * 12)) return;   // nothing in, the tail long gone
    const float scale = (0.3f + m_clampf(p->size, 0, 1) * 2.5f) * r->sr / 48000.0f;
    for (int k = 0; k < REV_LINES; k++) r->len[k] = CLAMP((int) (rev_base[k] * scale), 64, REV_MAX - 32);
    const float rt60 = m_clampf(p->decay, 0.1f, 20.0f);
    float g[REV_LINES];
    for (int k = 0; k < REV_LINES; k++) g[k] = p->freeze ? 1.0f : m_exp2(-3.0f * 3.3219281f * (float) r->len[k] / (rt60 * r->sr));
    const float damp = p->freeze ? 1.0f : 0.08f + (1.0f - m_clampf(p->damp, 0, 1)) * 0.9f;
    const int pre = CLAMP((int) (m_clampf(p->pre, 0, 100) * 0.001f * r->sr), 0, REV_PRE - 1);
    const float mv = m_clampf(p->move, 0, 1) * 10.0f;
    const float width = m_clampf(p->width, 0, 1), ret = m_clampf(p->ret, 0, 1);
    const float in_g = p->freeze ? 0.0f : 0.35f;
    float e = 0;
    for (int i = 0; i < n; i++) {
        // pre-delay, then four allpasses to smear the attack
        r->pre[r->prepos] = (inL[i] + inR[i]) * in_g;
        int rp = r->prepos - pre; if (rp < 0) rp += REV_PRE;
        float x = r->pre[rp];
        if (++r->prepos >= REV_PRE) r->prepos = 0;
        for (int k = 0; k < 4; k++) {
            const float v = r->ap[k][r->appos[k]], w = x + v * 0.65f;
            r->ap[k][r->appos[k]] = w;
            x = v - w * 0.65f;
            if (++r->appos[k] >= r->aplen[k]) r->appos[k] = 0;
        }
        // the lines, two of them moving slowly so the tail does not ring metallic
        r->lfo += 0.7f / r->sr; if (r->lfo >= 1) r->lfo -= 1;
        float o[REV_LINES];
        for (int k = 0; k < REV_LINES; k++) {
            float rpos = (float) (r->pos[k] - r->len[k]);
            if (k == 2) rpos -= mv * (1 + m_sin2pi(r->lfo));
            if (k == 5) rpos -= mv * (1 + m_sin2pi(r->lfo + 0.37f));
            while (rpos < 0) rpos += REV_MAX;
            int a = (int) rpos; const float f = rpos - (float) a;
            if (a >= REV_MAX) a -= REV_MAX;
            const int a1 = a + 1 < REV_MAX ? a + 1 : 0;
            o[k] = r->line[k][a] * (1 - f) + r->line[k][a1] * f;
        }
        // Hadamard 8x8, scaled to keep the energy
        float h[REV_LINES];
        for (int k = 0; k < REV_LINES; k++) h[k] = o[k];
        for (int len = 1; len < REV_LINES; len <<= 1)
            for (int k = 0; k < REV_LINES; k += len << 1)
                for (int j = k; j < k + len; j++) { const float a = h[j], b = h[j + len]; h[j] = a + b; h[j + len] = a - b; }
        for (int k = 0; k < REV_LINES; k++) {
            float y = h[k] * 0.35355339f;
            r->lp[k] += (y - r->lp[k]) * damp;
            r->line[k][r->pos[k]] = r->lp[k] * g[k] + (k & 1 ? -x : x);
            if (++r->pos[k] >= REV_MAX) r->pos[k] = 0;
        }
        const float l = (o[0] + o[2] + o[4] + o[6]) * 0.5f, rr = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
        const float m = (l + rr) * 0.5f, s = (l - rr) * 0.5f * width;
        outL[i] += (m + s) * ret;
        outR[i] += (m - s) * ret;
        e += m_fabs(l) + m_fabs(rr);
    }
    r->energy = e;
    r->quiet = (in_e > 0 || e > 1e-4f) ? 0 : r->quiet + n;
}

// ---------------------------------------------------------------------------------------------------- master

void mst_init(master_fx_t *m, float sr)
{
    m_set(m, 0, sizeof *m);
    m->sr = sr;
    m->env_db = -120;
    m->lim_gain = 1;
    m->out_ph = 1;
    rng_seed(&m->rng, 7);
}

void mst_process(master_fx_t *m, const master_t *p, float *L, float *R, int n)
{
    // compressor: stereo-linked, a 6 dB soft knee, attack and release on the gain reduction
    const bool comp = p->ratio > 1.01f;
    const float th = p->thresh, ratio = MAX(p->ratio, 1.0f), knee = 6.0f;
    const float att = m_exp(-1.0f / (MAX(p->attack, 0.1f) * 0.001f * m->sr));
    const float rel = m_exp(-1.0f / (MAX(p->release, 1.0f) * 0.001f * m->sr));
    const float makeup = m_db(p->makeup);
    const bool lc = p->low_cut > 21, hc = p->high_cut < 19999;
    if (lc) for (int c = 0; c < 2; c++) svf_set(&m->lc[c], p->low_cut, 0.707f, m->sr);
    if (hc) for (int c = 0; c < 2; c++) svf_set(&m->hc[c], p->high_cut, 0.707f, m->sr);
    const float drive = m_clampf(p->drive, 0, 1), width = m_clampf(p->width, 0, 2);
    const float vol = m_db(p->volume), ceil = m_db(MIN(p->limit, 0.0f));
    const float lim_rel = 1.0f - m_exp(-1.0f / (0.08f * m->sr));
    const float orate = out_rates[CLAMP(p->out_rate, 0, OUT_COUNT - 1)];
    const float ostep = p->out_bits >= 16 || p->out_bits < 2 ? 0 : m_exp2((float) p->out_bits - 1);
    float peak_gr = 0;
    for (int i = 0; i < n; i++) {
        float l = L[i], r = R[i];
        // DC out
        for (int c = 0; c < 2; c++) {
            const float x = c ? r : l, y = x - m->dc_x[c] + 0.9995f * m->dc_y[c];
            m->dc_x[c] = x; m->dc_y[c] = y;
            if (c) r = y; else l = y;
        }
        if (comp) {
            const float lev = m_todb(MAX(m_fabs(l), m_fabs(r)));
            const float over = lev - th;
            float gr = 0;
            if (over > knee * 0.5f) gr = over * (1 - 1 / ratio);
            else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; gr = (1 - 1 / ratio) * k * k / (2 * knee); }
            m->gr_db = gr > m->gr_db ? att * m->gr_db + (1 - att) * gr : rel * m->gr_db + (1 - rel) * gr;
            const float g = m_db(-m->gr_db) * makeup;
            l *= g; r *= g;
            peak_gr = MAX(peak_gr, m->gr_db);
        }
        if (lc) { svf_tick(&m->lc[0], l); svf_tick(&m->lc[1], r); l = m->lc[0].hp; r = m->lc[1].hp; }
        if (hc) { svf_tick(&m->hc[0], l); svf_tick(&m->hc[1], r); l = m->hc[0].lp; r = m->hc[1].lp; }
        if (drive > 0.001f) {
            const float g = 1 + drive * drive * 8;
            float yl, yr;
            switch (p->drive_type) {
            case 1:   // tape: a little uneven, rounder top
                yl = soft_clip(l * g + 0.15f * l * l * g) / (1 + drive); yr = soft_clip(r * g + 0.15f * r * r * g) / (1 + drive); break;
            case 2:   // fold
                yl = m_sin2pi(m_clampf(l * g, -4, 4) * 0.25f) * 0.8f; yr = m_sin2pi(m_clampf(r * g, -4, 4) * 0.25f) * 0.8f; break;
            default:  // warm
                yl = soft_clip(l * g) / (1 + drive * 1.5f); yr = soft_clip(r * g) / (1 + drive * 1.5f); break;
            }
            l = yl; r = yr;
        }
        if (width < 0.999f || width > 1.001f) { const float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * width; l = mid + side; r = mid - side; }
        l *= vol; r *= vol;
        // limiter: the gain the loudest frame in the next 2 ms needs, reached before it arrives
        m->dl[0][m->dpos] = l; m->dl[1][m->dpos] = r;
        {
            const float a = MAX(m_fabs(l), m_fabs(r));
            const uint32_t now = m->lim_n++;
            while (m->mq_n && m->mq_v[(m->mq_head + m->mq_n - 1) % LIM_LOOK] <= a) m->mq_n--;   // quieter ones never matter again
            m->mq_v[(m->mq_head + m->mq_n) % LIM_LOOK] = a; m->mq_i[(m->mq_head + m->mq_n) % LIM_LOOK] = now; m->mq_n++;
            while (now - m->mq_i[m->mq_head] >= LIM_LOOK) { m->mq_head = (m->mq_head + 1) % LIM_LOOK; m->mq_n--; }
        }
        const float loud = m->mq_v[m->mq_head];
        const float need = loud > ceil ? ceil / loud : 1.0f;
        if (need < m->lim_gain) m->lim_gain += (need - m->lim_gain) * 0.25f;
        else m->lim_gain += (need - m->lim_gain) * lim_rel;
        int rp = m->dpos + 1; if (rp >= LIM_LOOK) rp = 0;
        l = m->dl[0][rp] * m->lim_gain; r = m->dl[1][rp] * m->lim_gain;
        l = m_clampf(l, -ceil, ceil); r = m_clampf(r, -ceil, ceil);
        if (++m->dpos >= LIM_LOOK) m->dpos = 0;
        // output stage: the codec's rate and bits
        if (orate > 0) {
            m->out_ph += orate / m->sr;
            if (m->out_ph >= 1) { m->out_ph -= m_floor(m->out_ph); m->hold_l = l; m->hold_r = r; }
            l = m->hold_l; r = m->hold_r;
        }
        if (ostep > 0) { l = m_round(l * ostep) / ostep; r = m_round(r * ostep) / ostep; }
        L[i] = l; R[i] = r;
    }
    m->lim_gr = -m_todb(m->lim_gain);
    m->env_db = peak_gr;
}
