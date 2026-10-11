// The effects: a tape delay after the PT2399 pedal (repeats darken, the time wobbles and glides, dirt in the loop),
// a reverb of eight delay lines mixed through a Hadamard matrix, and the master chain: compressor, tone, drive,
// width, a look-ahead limiter and the output stage (a lower rate and fewer bits, for the codec's grit).
#pragma once
#include "doc.h"

#define DLY_MAX (48000 * 5 / 2)
typedef struct {
    float buf[2][DLY_MAX];
    int w, len;
    float sr, time, lfo, drift, drift_to;
    int drift_n;
    rng_t rng;
    svf_t tone[2], hp[2];
} delay_fx_t;
void  dly_init(delay_fx_t *d, float sr);
float dly_time_beats(int sync);    // a sync setting as beats
// Adds the wet signal (the return) into out.
void  dly_process(delay_fx_t *d, const delay_t *p, float bpm, const float *inL, const float *inR, float *outL, float *outR, int n);

#define REV_LINES 8
#define REV_MAX 9000
#define REV_PRE 4800
typedef struct {
    float line[REV_LINES][REV_MAX];
    int len[REV_LINES], pos[REV_LINES];
    float lp[REV_LINES];
    float ap[4][800];
    int aplen[4], appos[4];
    float pre[REV_PRE];
    int prepos;
    float sr, lfo, energy;
    int quiet;
} reverb_fx_t;
void rev_init(reverb_fx_t *r, float sr);
void rev_process(reverb_fx_t *r, const reverb_t *p, const float *inL, const float *inR, float *outL, float *outR, int n);

#define LIM_LOOK 96
typedef struct {
    float sr;
    float env_db, gr_db;
    svf_t lc[2], hc[2];
    float dl[2][LIM_LOOK];
    int dpos;
    float mq_v[LIM_LOOK]; uint32_t mq_i[LIM_LOOK];   // the loudest frames ahead, a falling queue (sliding maximum)
    int mq_head, mq_n; uint32_t lim_n;
    float lim_gain, lim_gr;
    float hold_l, hold_r, out_ph;
    float dc_x[2], dc_y[2];
    rng_t rng;
} master_fx_t;
void mst_init(master_fx_t *m, float sr);
void mst_process(master_fx_t *m, const master_t *p, float *L, float *R, int n);
extern const float out_rates[OUT_COUNT];
extern const char *const out_names[OUT_COUNT];
