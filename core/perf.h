// The performance effects the right stick plays (after BARE!'s perf page): REPEAT loops the last slice in time,
// REVERSE plays each last beat backwards (half a beat, pushed far), TAPE slows to a stop, GATE chops in time, FILTER is a DJ
// filter (left low-pass, right high-pass), CRUSH takes bits and rate away, DUB throws everything into the delay,
// FREEZE into the reverb and holds it. The first three play back a ring of the last four seconds.
#pragma once
#include "dsp.h"

enum { PF_REPEAT, PF_REVERSE, PF_TAPE, PF_GATE, PF_FILTER, PF_CRUSH, PF_DUB, PF_FREEZE, PF_COUNT };
extern const char *const perf_name[PF_COUNT];

#define PERF_RING (48000 * 4)
typedef struct {
    float sr;
    float ring_l[PERF_RING], ring_r[PERF_RING];
    uint32_t w;
    float target[PF_COUNT], amt[PF_COUNT];
    int mode;                     // which time effect plays the ring, or -1
    double rd, rate;              // reading the ring
    uint32_t rep_start, rep_len, rep_pos, rev_n;
    float mix;                    // 0 live .. 1 the ring
    double gate_ph;
    svf_t flt[2];
    float hold_l, hold_r, crush_ph;
} perf_t;

void perf_init(perf_t *p, float sr);
void perf_set(perf_t *p, int fx, float amount);   // 0 = off; FILTER takes -1..1
bool perf_any(const perf_t *p);
// Before the effects: DUB and FREEZE feed the sends. Returns how much the delay should repeat more, and whether the
// reverb freezes.
void perf_sends(perf_t *p, const float *L, const float *R, float *dL, float *dR, float *rL, float *rR, int n, float *dub, bool *freeze);
// After them: the rest, on what is heard.
void perf_main(perf_t *p, float *L, float *R, int n, float bpm);
