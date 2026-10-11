// Sample playback for one track: up to four voices with 4-point interpolation, slices, reverse, loop, an
// attack/hold/decay envelope, a state-variable filter, drive, and bits and rate for the grit.
#pragma once
#include "doc.h"
#include "link.h"

#define SMP_VOICES 4

typedef struct {
    bool on;
    double pos, inc;           // frames into the sample, per output frame (negative: reverse)
    double a, b, loop_a;       // the region, in frames
    bool loop;
    float gain, fade, fade_step, env;
    int t, att_n, hold_n, dec_n;
    svf_t flt[2];
    uint8_t filter_mode;
    float cutoff, reso;
    float step_q, rate_inc, rate_ph, hold_l, hold_r;   // bits and rate
    float drive;
} smp_voice_t;

typedef struct {
    float sr;
    smp_voice_t v[SMP_VOICES];
    const sample_t *s;
    float peak;
} smp_track_t;

void smp_init(smp_track_t *t, float sr);
// slice: 0 = the whole region, 1..N a slice. p: the track's settings, already with this step's locks.
void smp_trigger(smp_track_t *t, const smp_t *p, float vel, int slice);
void smp_choke(smp_track_t *t);
void smp_render(smp_track_t *t, float *L, float *R, int n);   // adds
bool smp_active(const smp_track_t *t);
// Where slice k (1..N) starts and ends, 0..1 of the whole file.
void smp_slice_range(const smp_t *p, int k, float *a, float *b);
