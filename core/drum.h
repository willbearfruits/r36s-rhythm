// The synthesized drum, thump's engine in C: a pitched body with a two-stage sweep, a sub, a click, filtered noise,
// struck modes, 808 metal, clap bursts, then drive, EQ and a transient shaper. One drum_t per track, up to six
// voices so hits overlap. Oscillators and drive run at 2x or 4x and are decimated with half-band filters.
#pragma once
#include "dsp.h"

enum { DT_KICK, DT_SNARE, DT_CLAP, DT_RIM, DT_HAT, DT_TOM, DT_PERC, DT_COUNT };
enum { DS_MAIN, DS_BODY, DS_SUB, DS_CLICK, DS_NOISE, DS_MODES, DS_METAL, DS_CLAP, DS_DRIVE, DS_EQ, DS_SHAPE, DS_COUNT };
enum { DU_NONE, DU_HZ, DU_MS, DU_DB, DU_SEMI, DU_PCT, DU_RATIO, DU_CHOICE };

typedef struct {
    const char *key, *label;
    uint8_t section;
    float min, max, def;
    bool log;                 // exponential travel
    uint8_t unit;
    const char *choices;      // DU_CHOICE: labels separated by '|'
} drum_param_t;

// Same order as thump's, which is also the saved order: add new ones at the end.
enum {
    P_TYPE, P_LEVEL, P_PAN, P_TUNE, P_PUNCH, P_LENGTH, P_TONE, P_GRIT, P_VELSENS, P_CHOKE,
    P_BODY_LEVEL, P_BODY_START, P_BODY_END, P_BODY_SWEEP, P_BODY_CURVE, P_BODY_DROP, P_BODY_DROPTIME,
    P_BODY_SHAPE, P_BODY_PHASE, P_BODY_ATTACK, P_BODY_HOLD, P_BODY_DECAY, P_BODY_DECAYCURVE,
    P_SUB_LEVEL, P_SUB_OCTAVE, P_SUB_TRACK, P_SUB_DECAY,
    P_CLICK_LEVEL, P_CLICK_TYPE, P_CLICK_FREQ, P_CLICK_LEN,
    P_NOISE_LEVEL, P_NOISE_MODE, P_NOISE_FREQ, P_NOISE_RESO, P_NOISE_ATTACK, P_NOISE_DECAY, P_NOISE_CURVE, P_NOISE_COLOR,
    P_MODES_LEVEL, P_MODES_SET, P_MODES_FREQ, P_MODES_COUNT, P_MODES_DECAY, P_MODES_DAMP, P_MODES_BEND, P_MODES_STRIKE, P_MODES_SPREAD,
    P_METAL_LEVEL, P_METAL_TYPE, P_METAL_TUNE, P_METAL_BP1, P_METAL_BP2, P_METAL_HP, P_METAL_ATTACK, P_METAL_DECAY, P_METAL_CURVE,
    P_CLAP_LEVEL, P_CLAP_COUNT, P_CLAP_SPACING, P_CLAP_SPREAD, P_CLAP_FREQ, P_CLAP_Q, P_CLAP_BURST, P_CLAP_DECAY,
    P_DRIVE_AMOUNT, P_DRIVE_TYPE, P_DRIVE_TONE, P_DRIVE_MIX,
    P_EQ_LOW, P_EQ_LOWFREQ, P_EQ_MID, P_EQ_MIDFREQ, P_EQ_HIGH, P_EQ_HP, P_EQ_LP,
    P_SHAPE_ATTACK, P_SHAPE_SUSTAIN, P_SHAPE_GATE,
    DRUM_NP
};

extern const drum_param_t drum_params[DRUM_NP];
const char *drum_type_name(int t);
const char *drum_section_name(int s);
bool  drum_shown(int section, int type);          // is this section part of the model's panel
float drum_default(int p, int type);              // a sensible starting value per model
void  drum_defaults(float *v, int type);
void  drum_range(int p, int type, float *lo, float *hi);   // the musical range per model (dice, morph)
float drum_to_norm(int p, float v);               // value -> 0..1 knob travel
float drum_from_norm(int p, float n);
int   drum_choices(int p);
void  drum_format(int p, float v, char *out, int cap);

#define DRUM_VOICES 6
#define DRUM_MAXOS 4
#define DRUM_MAXBLOCK 128

typedef struct {
    int type;
    float tune_ratio, length, punch, tone, grit, vel_sens;
    float body_level, body_start, body_end, sweep_tau, sweep_oct, body_curve, drop, drop_tau;
    float shape_k, shape_norm, phase, attack, hold, decay, decay_curve;
    float sub_level, sub_mul, sub_track, sub_decay;
    float click_level, click_freq, click_len; int click_type;
    float noise_level, noise_freq, noise_q, noise_attack, noise_decay, noise_curve, noise_color; int noise_mode;
    float modes_level, modes_freq, modes_decay, modes_damp, modes_bend, modes_strike, modes_spread; int modes_set, modes_count;
    float metal_level, metal_tune, metal_bp1, metal_bp2, metal_hp, metal_attack, metal_decay, metal_curve; int metal_type;
    float clap_level, clap_spacing, clap_spread, clap_freq, clap_q, clap_burst, clap_decay; int clap_count;
    float drive_amount, drive_tone, drive_mix; int drive_type;
    float gate;
} drum_derived_t;

typedef struct {
    bool on;
    uint64_t age;
    int t, end;                       // samples at the internal rate
    float vel, gain, fade, fade_step;
    drum_derived_t d;                 // this hit's sound (a step can lock its own)
    float body_ph, pitch_e, drop_e, pitch_m, drop_m;
    int attack_n, hold_n;
    fdecay_t body_env, sub_env, noise_env, metal_env;
    float sub_ph;
    svf_t click_f; float click_e, click_m, pop_ph;
    rng_t rng;
    svf_t noise_f; float noise_lp; int noise_attack_n;
    float m_re[8], m_im[8], m_c[8], m_s[8], m_r[8], m_amp[8], m_freq[8], modes_norm;
    int modes;
    float bend_e, bend_m, strike_e, strike_m, strike_lp, strike_c;
    float mt_ph[6], mt_freq[6];
    svf_t mt_bp1, mt_bp2, mt_hp1, mt_hp2;
    int metal_attack_n;
    float clap_t[8]; int claps;
    float clap_e[8], clap_m, clap_tail, clap_tail_m;   // each burst's decay, the last one's tail: running products
    int clap_n[8];                                      // the sample each burst starts at
    int body_end, sub_end, noise_end, metal_end, modes_end, clap_end;   // where each section has gone quiet
    svf_t clap_f;
    int gate_n;
} drum_voice_t;

typedef struct {
    float vals[DRUM_NP];
    bool dirty;
    int os;                           // 1, 2 or 4
    float sr, fsi;
    float noise_k, noise_lp_c;        // keep 2x and 1x sounding like thump's 4x
    drum_voice_t voice[DRUM_VOICES];
    uint64_t counter;
    int active, tail;
    drum_derived_t d;
    svf_t drive_lp; float crush_hold; int crush_count;
    halfband_t hb1, hb2;
    float buf_i[DRUM_MAXBLOCK * DRUM_MAXOS + 16], buf_h[DRUM_MAXBLOCK * 2 + 8];
    svf_t hp, lp; biquad_t low_shelf, mid_peak, high_shelf;
    float env_fast, env_slow, shape_gain, shape_attack, shape_sustain;
    float gain_l, gain_r, peak;
} drum_t;

void drum_init(drum_t *s, float sr, int os);
void drum_set(drum_t *s, const float *vals);
void drum_derive(const float *vals, drum_derived_t *d);
// A hit. locked: a whole set of values for this hit alone (a step's locks), or NULL.
void drum_trigger(drum_t *s, float vel, uint32_t seed, const float *locked);
void drum_choke(drum_t *s);
void drum_render(drum_t *s, float *L, float *R, int n);   // adds into L and R; any n
static inline bool drum_active(const drum_t *s) { return s->active > 0 || s->tail > 0; }
