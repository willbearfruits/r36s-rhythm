// thump's Slot, Params and model tables in C. Same parameters, same ranges, same maths.
#include "drum.h"

// Mode frequency ratios and levels per material.
static const float mode_ratios[7][8] = {
    { 1.000f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f },   // membrane (Bessel zeros)
    { 1.000f, 1.620f, 2.220f, 2.860f, 3.500f, 4.100f, 4.800f, 5.400f },   // two-tone shell
    { 1.000f, 2.350f, 3.860f, 5.330f, 7.000f, 8.900f, 10.80f, 13.00f },   // wood block
    { 1.000f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.81f, 31.87f },   // free bar
    { 0.500f, 1.000f, 1.183f, 1.506f, 2.000f, 2.514f, 2.662f, 3.011f },   // bell
    { 1.000f, 1.900f, 2.800f, 3.400f, 4.300f, 5.100f, 5.700f, 6.600f },   // plate
    { 1.000f, 2.000f, 3.000f, 4.000f, 5.000f, 6.000f, 7.000f, 8.000f },   // tube
};
static const float mode_amps[7][8] = {
    { 1.0f, 0.75f, 0.55f, 0.5f, 0.35f, 0.3f, 0.22f, 0.18f },
    { 1.0f, 0.85f, 0.4f, 0.3f, 0.2f, 0.15f, 0.1f, 0.08f },
    { 1.0f, 0.5f, 0.3f, 0.2f, 0.12f, 0.08f, 0.05f, 0.03f },
    { 1.0f, 0.45f, 0.25f, 0.15f, 0.1f, 0.06f, 0.04f, 0.02f },
    { 0.5f, 1.0f, 0.7f, 0.6f, 0.5f, 0.35f, 0.3f, 0.2f },
    { 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.45f, 0.4f, 0.35f },
    { 1.0f, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f, 0.14f, 0.12f },
};
// Roland TR-808 cymbal/hat oscillators, relative to 205.3 Hz.
static const float metal_ratios[6] = { 1.0f, 1.4827f, 1.8003f, 2.5460f, 2.6303f, 3.8967f };
#define METAL_BASE 205.3f

const drum_param_t drum_params[DRUM_NP] = {
    { "type", "Model", DS_MAIN, 0, 6, 0, false, DU_CHOICE, "Kick|Snare|Clap|Rim|Hat|Tom|Perc" },
    { "level", "Level", DS_MAIN, -48, 18, 0, false, DU_DB, "" },
    { "pan", "Pan", DS_MAIN, -1, 1, 0, false, DU_NONE, "" },
    { "tune", "Tune", DS_MAIN, -24, 24, 0, false, DU_SEMI, "" },
    { "punch", "Punch", DS_MAIN, 0, 1, 0.5f, false, DU_PCT, "" },
    { "length", "Length", DS_MAIN, 0.25f, 4, 1, true, DU_RATIO, "" },
    { "tone", "Tone", DS_MAIN, -1, 1, 0, false, DU_NONE, "" },
    { "grit", "Grit", DS_MAIN, 0, 1, 0, false, DU_PCT, "" },
    { "velSens", "Velocity", DS_MAIN, 0, 1, 0.6f, false, DU_PCT, "" },
    { "choke", "Choke", DS_MAIN, 0, 4, 0, false, DU_CHOICE, "off|A|B|C|D" },

    { "bodyLevel", "Level", DS_BODY, 0, 1, 0.9f, false, DU_PCT, "" },
    { "bodyStart", "Start", DS_BODY, 20, 8000, 300, true, DU_HZ, "" },
    { "bodyEnd", "End", DS_BODY, 20, 2000, 50, true, DU_HZ, "" },
    { "bodySweep", "Sweep", DS_BODY, 1, 1000, 40, true, DU_MS, "" },
    { "bodyCurve", "Curve", DS_BODY, 0, 1, 0.3f, false, DU_PCT, "" },
    { "bodyDrop", "Drop", DS_BODY, -24, 0, 0, false, DU_SEMI, "" },
    { "bodyDropTime", "Drop time", DS_BODY, 10, 4000, 500, true, DU_MS, "" },
    { "bodyShape", "Shape", DS_BODY, 0, 1, 0, false, DU_PCT, "" },
    { "bodyPhase", "Phase", DS_BODY, 0, 1, 0.25f, false, DU_PCT, "" },
    { "bodyAttack", "Attack", DS_BODY, 0, 50, 0.3f, false, DU_MS, "" },
    { "bodyHold", "Hold", DS_BODY, 0, 2000, 0, false, DU_MS, "" },
    { "bodyDecay", "Decay", DS_BODY, 10, 8000, 450, true, DU_MS, "" },
    { "bodyDecayCurve", "Decay shape", DS_BODY, 0, 1, 0.4f, false, DU_PCT, "" },

    { "subLevel", "Level", DS_SUB, 0, 1, 0, false, DU_PCT, "" },
    { "subOctave", "Octave", DS_SUB, 0, 1, 0, false, DU_CHOICE, "same|-1 oct" },
    { "subTrack", "Track", DS_SUB, 0, 1, 0, false, DU_PCT, "" },
    { "subDecay", "Decay", DS_SUB, 10, 8000, 600, true, DU_MS, "" },

    { "clickLevel", "Level", DS_CLICK, 0, 1, 0.3f, false, DU_PCT, "" },
    { "clickType", "Type", DS_CLICK, 0, 3, 3, false, DU_CHOICE, "noise|pop|tick|mix" },
    { "clickFreq", "Freq", DS_CLICK, 200, 16000, 3000, true, DU_HZ, "" },
    { "clickLen", "Length", DS_CLICK, 0.3f, 40, 4, true, DU_MS, "" },

    { "noiseLevel", "Level", DS_NOISE, 0, 1, 0, false, DU_PCT, "" },
    { "noiseMode", "Filter", DS_NOISE, 0, 2, 1, false, DU_CHOICE, "low|band|high" },
    { "noiseFreq", "Freq", DS_NOISE, 100, 16000, 4000, true, DU_HZ, "" },
    { "noiseReso", "Reso", DS_NOISE, 0, 1, 0.2f, false, DU_PCT, "" },
    { "noiseAttack", "Attack", DS_NOISE, 0, 50, 0, false, DU_MS, "" },
    { "noiseDecay", "Decay", DS_NOISE, 5, 4000, 180, true, DU_MS, "" },
    { "noiseCurve", "Decay shape", DS_NOISE, 0, 1, 0.3f, false, DU_PCT, "" },
    { "noiseColor", "Colour", DS_NOISE, 0, 1, 0, false, DU_PCT, "" },

    { "modesLevel", "Level", DS_MODES, 0, 1, 0, false, DU_PCT, "" },
    { "modesSet", "Material", DS_MODES, 0, 6, 0, false, DU_CHOICE, "membrane|two-tone|wood|bar|bell|plate|tube" },
    { "modesFreq", "Freq", DS_MODES, 30, 6000, 180, true, DU_HZ, "" },
    { "modesCount", "Modes", DS_MODES, 1, 8, 5, false, DU_NONE, "" },
    { "modesDecay", "Decay", DS_MODES, 5, 5000, 250, true, DU_MS, "" },
    { "modesDamp", "Damping", DS_MODES, 0, 1, 0.5f, false, DU_PCT, "" },
    { "modesBend", "Bend", DS_MODES, 0, 12, 0, false, DU_SEMI, "" },
    { "modesStrike", "Strike", DS_MODES, 0, 1, 0.4f, false, DU_PCT, "" },
    { "modesSpread", "Spread", DS_MODES, 0, 1, 0, false, DU_PCT, "" },

    { "metalLevel", "Level", DS_METAL, 0, 1, 0, false, DU_PCT, "" },
    { "metalType", "Type", DS_METAL, 0, 2, 0, false, DU_CHOICE, "808|ring|fm" },
    { "metalTune", "Tune", DS_METAL, 0.25f, 4, 1, true, DU_RATIO, "" },
    { "metalBp1", "Band 1", DS_METAL, 800, 16000, 3440, true, DU_HZ, "" },
    { "metalBp2", "Band 2", DS_METAL, 800, 16000, 7100, true, DU_HZ, "" },
    { "metalHp", "Highpass", DS_METAL, 300, 16000, 6000, true, DU_HZ, "" },
    { "metalAttack", "Attack", DS_METAL, 0, 20, 0, false, DU_MS, "" },
    { "metalDecay", "Decay", DS_METAL, 5, 6000, 90, true, DU_MS, "" },
    { "metalCurve", "Decay shape", DS_METAL, 0, 1, 0.2f, false, DU_PCT, "" },

    { "clapLevel", "Level", DS_CLAP, 0, 1, 0, false, DU_PCT, "" },
    { "clapCount", "Claps", DS_CLAP, 1, 8, 4, false, DU_NONE, "" },
    { "clapSpacing", "Spacing", DS_CLAP, 2, 40, 10, true, DU_MS, "" },
    { "clapSpread", "Spread", DS_CLAP, 0, 1, 0.3f, false, DU_PCT, "" },
    { "clapFreq", "Freq", DS_CLAP, 300, 8000, 1200, true, DU_HZ, "" },
    { "clapQ", "Reso", DS_CLAP, 0, 1, 0.4f, false, DU_PCT, "" },
    { "clapBurst", "Burst", DS_CLAP, 1, 30, 6, true, DU_MS, "" },
    { "clapDecay", "Tail", DS_CLAP, 20, 3000, 220, true, DU_MS, "" },

    { "driveAmount", "Drive", DS_DRIVE, 0, 1, 0, false, DU_PCT, "" },
    { "driveType", "Type", DS_DRIVE, 0, 3, 0, false, DU_CHOICE, "warm|clip|fold|crush" },
    { "driveTone", "Tone", DS_DRIVE, 200, 20000, 20000, true, DU_HZ, "" },
    { "driveMix", "Mix", DS_DRIVE, 0, 1, 1, false, DU_PCT, "" },

    { "eqLow", "Low", DS_EQ, -18, 18, 0, false, DU_DB, "" },
    { "eqLowFreq", "Low freq", DS_EQ, 30, 400, 80, true, DU_HZ, "" },
    { "eqMid", "Mid", DS_EQ, -18, 18, 0, false, DU_DB, "" },
    { "eqMidFreq", "Mid freq", DS_EQ, 100, 10000, 1000, true, DU_HZ, "" },
    { "eqHigh", "High", DS_EQ, -18, 18, 0, false, DU_DB, "" },
    { "eqHp", "Highpass", DS_EQ, 10, 2000, 10, true, DU_HZ, "" },
    { "eqLp", "Lowpass", DS_EQ, 500, 22000, 22000, true, DU_HZ, "" },

    { "shapeAttack", "Attack", DS_SHAPE, -1, 1, 0, false, DU_NONE, "" },
    { "shapeSustain", "Sustain", DS_SHAPE, -1, 1, 0, false, DU_NONE, "" },
    { "shapeGate", "Gate", DS_SHAPE, 20, 8000, 8000, true, DU_MS, "" },
};

const char *drum_type_name(int t)
{
    static const char *n[] = { "Kick", "Snare", "Clap", "Rim", "Hat", "Tom", "Perc" };
    return n[CLAMP(t, 0, DT_COUNT - 1)];
}

const char *drum_section_name(int s)
{
    static const char *n[] = { "Main", "Body", "Sub", "Click", "Noise", "Modes", "Metal", "Clap", "Drive", "EQ", "Shape" };
    return n[CLAMP(s, 0, DS_COUNT - 1)];
}

bool drum_shown(int s, int t)
{
    switch (s) {
    case DS_MAIN: case DS_DRIVE: case DS_EQ: case DS_SHAPE: return true;
    case DS_BODY: return t == DT_KICK || t == DT_SNARE || t == DT_TOM || t == DT_PERC || t == DT_RIM;
    case DS_SUB: return t == DT_KICK || t == DT_TOM;
    case DS_CLICK: return t != DT_CLAP;
    case DS_NOISE: return t != DT_RIM;
    case DS_MODES: return t == DT_SNARE || t == DT_RIM || t == DT_TOM || t == DT_PERC;
    case DS_METAL: return t == DT_HAT || t == DT_PERC;
    case DS_CLAP: return t == DT_CLAP || t == DT_SNARE;
    default: return false;
    }
}

typedef struct { int p; float v; } pv_t;
typedef struct { int p; float lo, hi; } pr_t;

static const pv_t def_kick[] = {
    { P_BODY_LEVEL, 0.95f }, { P_BODY_START, 320 }, { P_BODY_END, 48 }, { P_BODY_SWEEP, 32 }, { P_BODY_CURVE, 0.25f },
    { P_BODY_DECAY, 520 }, { P_BODY_DECAYCURVE, 0.45f }, { P_CLICK_LEVEL, 0.35f }, { P_CLICK_FREQ, 3500 }, { P_CLICK_LEN, 3 }, { -1, 0 } };
static const pv_t def_snare[] = {
    { P_BODY_LEVEL, 0.55f }, { P_BODY_START, 330 }, { P_BODY_END, 185 }, { P_BODY_SWEEP, 12 }, { P_BODY_DECAY, 170 }, { P_BODY_DECAYCURVE, 0.2f },
    { P_BODY_PHASE, 0 }, { P_NOISE_LEVEL, 0.75f }, { P_NOISE_MODE, 2 }, { P_NOISE_FREQ, 1800 }, { P_NOISE_DECAY, 230 }, { P_NOISE_CURVE, 0.25f },
    { P_MODES_LEVEL, 0.35f }, { P_MODES_FREQ, 195 }, { P_MODES_DECAY, 140 }, { P_MODES_SET, 0 }, { P_MODES_DAMP, 0.6f },
    { P_CLICK_LEVEL, 0.3f }, { P_CLICK_FREQ, 5000 }, { P_CLICK_LEN, 2 }, { -1, 0 } };
static const pv_t def_clap[] = {
    { P_BODY_LEVEL, 0 }, { P_CLICK_LEVEL, 0 }, { P_CLAP_LEVEL, 0.9f }, { P_CLAP_COUNT, 4 }, { P_CLAP_SPACING, 9 }, { P_CLAP_FREQ, 1250 },
    { P_CLAP_Q, 0.35f }, { P_CLAP_BURST, 6 }, { P_CLAP_DECAY, 260 }, { P_EQ_HP, 250 }, { -1, 0 } };
static const pv_t def_rim[] = {
    { P_BODY_LEVEL, 0.25f }, { P_BODY_START, 1800 }, { P_BODY_END, 900 }, { P_BODY_SWEEP, 3 }, { P_BODY_DECAY, 35 }, { P_BODY_PHASE, 0 },
    { P_MODES_LEVEL, 0.8f }, { P_MODES_SET, 2 }, { P_MODES_FREQ, 480 }, { P_MODES_COUNT, 3 }, { P_MODES_DECAY, 55 }, { P_MODES_STRIKE, 0.8f },
    { P_CLICK_LEVEL, 0.5f }, { P_CLICK_FREQ, 6000 }, { P_CLICK_LEN, 1 }, { P_EQ_HP, 180 }, { -1, 0 } };
static const pv_t def_hat[] = {
    { P_BODY_LEVEL, 0 }, { P_CLICK_LEVEL, 0.1f }, { P_CLICK_FREQ, 9000 }, { P_CLICK_LEN, 1 },
    { P_METAL_LEVEL, 0.8f }, { P_METAL_DECAY, 70 }, { P_NOISE_LEVEL, 0.3f }, { P_NOISE_MODE, 2 }, { P_NOISE_FREQ, 8000 },
    { P_NOISE_DECAY, 60 }, { P_EQ_HP, 400 }, { P_CHOKE, 1 }, { -1, 0 } };
static const pv_t def_tom[] = {
    { P_BODY_LEVEL, 0.9f }, { P_BODY_START, 260 }, { P_BODY_END, 120 }, { P_BODY_SWEEP, 45 }, { P_BODY_DECAY, 520 }, { P_BODY_DECAYCURVE, 0.35f },
    { P_MODES_LEVEL, 0.3f }, { P_MODES_FREQ, 125 }, { P_MODES_DECAY, 380 }, { P_CLICK_LEVEL, 0.25f }, { P_CLICK_FREQ, 2500 }, { -1, 0 } };
static const pv_t def_perc[] = {
    { P_BODY_LEVEL, 0.2f }, { P_BODY_START, 900 }, { P_BODY_END, 560 }, { P_BODY_SWEEP, 8 }, { P_BODY_DECAY, 120 },
    { P_MODES_LEVEL, 0.8f }, { P_MODES_SET, 4 }, { P_MODES_FREQ, 560 }, { P_MODES_COUNT, 5 }, { P_MODES_DECAY, 320 },
    { P_CLICK_LEVEL, 0.25f }, { P_CLICK_FREQ, 4000 }, { -1, 0 } };
static const pv_t *const type_defs[DT_COUNT] = { def_kick, def_snare, def_clap, def_rim, def_hat, def_tom, def_perc };

static const pr_t rng_kick[] = {
    { P_BODY_START, 90, 900 }, { P_BODY_END, 32, 85 }, { P_BODY_SWEEP, 6, 160 }, { P_BODY_CURVE, 0, 0.7f },
    { P_BODY_DROP, -7, 0 }, { P_BODY_SHAPE, 0, 0.55f }, { P_BODY_PHASE, 0, 0.25f }, { P_BODY_ATTACK, 0, 2 },
    { P_BODY_HOLD, 0, 120 }, { P_BODY_DECAY, 150, 2200 }, { P_BODY_DECAYCURVE, 0.15f, 0.8f },
    { P_SUB_LEVEL, 0, 0.6f }, { P_SUB_DECAY, 200, 2500 }, { P_CLICK_LEVEL, 0.05f, 0.8f }, { P_CLICK_FREQ, 1200, 9000 },
    { P_CLICK_LEN, 0.8f, 12 }, { P_NOISE_LEVEL, 0, 0.15f }, { P_NOISE_DECAY, 10, 80 },
    { P_DRIVE_AMOUNT, 0, 0.6f }, { P_DRIVE_TONE, 2000, 20000 }, { P_EQ_LOW, -3, 6 }, { P_EQ_MID, -6, 3 }, { P_EQ_HIGH, -6, 4 },
    { P_EQ_HP, 10, 40 }, { P_EQ_LP, 4000, 22000 }, { P_SHAPE_ATTACK, -0.2f, 0.6f }, { P_SHAPE_SUSTAIN, -0.4f, 0.4f }, { -1, 0, 0 } };
static const pr_t rng_snare[] = {
    { P_BODY_LEVEL, 0.2f, 0.8f }, { P_BODY_START, 180, 700 }, { P_BODY_END, 140, 320 }, { P_BODY_SWEEP, 3, 40 },
    { P_BODY_DECAY, 60, 380 }, { P_BODY_SHAPE, 0, 0.4f }, { P_NOISE_LEVEL, 0.35f, 1 }, { P_NOISE_FREQ, 900, 8000 },
    { P_NOISE_RESO, 0, 0.5f }, { P_NOISE_DECAY, 90, 600 }, { P_NOISE_CURVE, 0.1f, 0.6f }, { P_NOISE_COLOR, 0, 0.6f },
    { P_MODES_LEVEL, 0, 0.7f }, { P_MODES_FREQ, 130, 420 }, { P_MODES_DECAY, 40, 380 }, { P_MODES_BEND, 0, 4 },
    { P_CLICK_LEVEL, 0, 0.6f }, { P_CLICK_FREQ, 2500, 10000 }, { P_CLAP_LEVEL, 0, 0.4f },
    { P_DRIVE_AMOUNT, 0, 0.5f }, { P_EQ_LOW, -6, 4 }, { P_EQ_MID, -6, 6 }, { P_EQ_HIGH, -4, 6 }, { P_EQ_HP, 60, 220 },
    { P_SHAPE_ATTACK, -0.2f, 0.6f }, { P_SHAPE_SUSTAIN, -0.5f, 0.5f }, { -1, 0, 0 } };
static const pr_t rng_clap[] = {
    { P_CLAP_COUNT, 2, 6 }, { P_CLAP_SPACING, 5, 20 }, { P_CLAP_SPREAD, 0, 0.7f }, { P_CLAP_FREQ, 700, 2600 },
    { P_CLAP_Q, 0.1f, 0.7f }, { P_CLAP_BURST, 3, 12 }, { P_CLAP_DECAY, 80, 900 }, { P_NOISE_LEVEL, 0, 0.3f },
    { P_DRIVE_AMOUNT, 0, 0.4f }, { P_EQ_HP, 120, 500 }, { P_EQ_MID, -4, 6 }, { P_EQ_HIGH, -4, 5 }, { -1, 0, 0 } };
static const pr_t rng_rim[] = {
    { P_MODES_FREQ, 280, 2200 }, { P_MODES_COUNT, 2, 4 }, { P_MODES_DECAY, 15, 180 }, { P_MODES_SET, 1, 3 },
    { P_BODY_START, 800, 3000 }, { P_BODY_END, 400, 1500 }, { P_BODY_DECAY, 15, 80 }, { P_CLICK_LEVEL, 0.2f, 0.8f },
    { P_CLICK_FREQ, 2000, 9000 }, { P_DRIVE_AMOUNT, 0, 0.4f }, { P_EQ_HP, 100, 400 }, { -1, 0, 0 } };
static const pr_t rng_hat[] = {
    { P_METAL_TUNE, 0.6f, 1.8f }, { P_METAL_BP1, 2500, 6500 }, { P_METAL_BP2, 5000, 12000 }, { P_METAL_HP, 3000, 10000 },
    { P_METAL_DECAY, 25, 450 }, { P_METAL_CURVE, 0, 0.5f }, { P_NOISE_LEVEL, 0, 0.6f }, { P_NOISE_FREQ, 5000, 14000 },
    { P_NOISE_DECAY, 20, 300 }, { P_CLICK_LEVEL, 0, 0.3f }, { P_DRIVE_AMOUNT, 0, 0.3f }, { P_EQ_HP, 250, 2000 },
    { P_EQ_HIGH, -4, 5 }, { -1, 0, 0 } };
static const pr_t rng_tom[] = {
    { P_BODY_START, 110, 550 }, { P_BODY_END, 60, 320 }, { P_BODY_SWEEP, 10, 200 }, { P_BODY_DECAY, 180, 1400 },
    { P_BODY_SHAPE, 0, 0.3f }, { P_MODES_LEVEL, 0, 0.5f }, { P_MODES_FREQ, 60, 420 }, { P_MODES_DECAY, 100, 900 },
    { P_SUB_LEVEL, 0, 0.3f }, { P_CLICK_LEVEL, 0.05f, 0.5f }, { P_NOISE_LEVEL, 0, 0.2f }, { P_DRIVE_AMOUNT, 0, 0.4f }, { -1, 0, 0 } };
static const pr_t rng_perc[] = {
    { P_MODES_FREQ, 150, 3200 }, { P_MODES_DECAY, 30, 1400 }, { P_MODES_COUNT, 2, 8 }, { P_MODES_STRIKE, 0.1f, 0.9f },
    { P_BODY_LEVEL, 0, 0.6f }, { P_BODY_START, 200, 3000 }, { P_BODY_END, 150, 1500 }, { P_BODY_DECAY, 20, 400 },
    { P_METAL_LEVEL, 0, 0.5f }, { P_METAL_DECAY, 20, 800 }, { P_NOISE_LEVEL, 0, 0.3f }, { P_CLICK_LEVEL, 0, 0.5f },
    { P_DRIVE_AMOUNT, 0, 0.4f }, { -1, 0, 0 } };
static const pr_t *const type_ranges[DT_COUNT] = { rng_kick, rng_snare, rng_clap, rng_rim, rng_hat, rng_tom, rng_perc };

float drum_default(int p, int t)
{
    t = CLAMP(t, 0, DT_COUNT - 1);
    if (p == P_TYPE) return (float) t;
    for (const pv_t *o = type_defs[t]; o->p >= 0; o++) if (o->p == p) return o->v;
    return drum_params[p].def;
}

void drum_defaults(float *v, int t) { for (int p = 0; p < DRUM_NP; p++) v[p] = drum_default(p, t); }

void drum_range(int p, int t, float *lo, float *hi)
{
    t = CLAMP(t, 0, DT_COUNT - 1);
    for (const pr_t *r = type_ranges[t]; r->p >= 0; r++) if (r->p == p) { *lo = r->lo; *hi = r->hi; return; }
    *lo = drum_params[p].min; *hi = drum_params[p].max;
}

float drum_to_norm(int p, float v)
{
    const drum_param_t *d = &drum_params[p];
    v = m_clampf(v, d->min, d->max);
    if (d->log) return m_log2(v / d->min) / m_log2(d->max / d->min);
    return (v - d->min) / (d->max - d->min);
}

float drum_from_norm(int p, float n)
{
    const drum_param_t *d = &drum_params[p];
    n = m_clampf(n, 0.0f, 1.0f);
    float v = d->log ? d->min * m_exp2(n * m_log2(d->max / d->min)) : d->min + (d->max - d->min) * n;
    if (d->unit == DU_CHOICE || p == P_MODES_COUNT || p == P_CLAP_COUNT) v = m_round(v);
    return m_clampf(v, d->min, d->max);
}

int drum_choices(int p)
{
    const drum_param_t *d = &drum_params[p];
    if (d->unit != DU_CHOICE) return 0;
    int n = 1;
    for (const char *c = d->choices; *c; c++) if (*c == '|') n++;
    return n;
}

void drum_format(int p, float v, char *out, int cap)
{
    const drum_param_t *d = &drum_params[p];
    switch (d->unit) {
    case DU_CHOICE: {
        int idx = m_lround(v), k = 0, n = 0;
        for (const char *c = d->choices;; c++) {
            if (*c == '|' || !*c) { if (k == idx) { out[MIN(n, cap - 1)] = 0; return; } k++; n = 0; if (!*c) break; continue; }
            if (k == idx && n < cap - 1) out[n] = *c;
            n++;
        }
        k_strcpy(out, "?", cap);
        return;
    }
    case DU_HZ:
        if (v >= 10000) k_snprintf(out, cap, "%.0fk", v / 1000.0f);
        else if (v >= 1000) k_snprintf(out, cap, "%.1fk", v / 1000.0f);
        else k_snprintf(out, cap, "%.0f", v);
        return;
    case DU_MS:
        if (v >= 1000) k_snprintf(out, cap, "%.2fs", v / 1000.0f);
        else if (v < 10) k_snprintf(out, cap, "%.1fms", v);
        else k_snprintf(out, cap, "%.0fms", v);
        return;
    case DU_DB: k_snprintf(out, cap, "%+.1f", v); return;
    case DU_SEMI: k_snprintf(out, cap, "%+.1f", v); return;
    case DU_PCT: k_snprintf(out, cap, "%.0f", v * 100); return;
    case DU_RATIO: k_snprintf(out, cap, "x%.2f", v); return;
    default:
        if (d->max - d->min >= 4 && m_fabs(v - m_round(v)) < 1e-3f) k_snprintf(out, cap, "%.0f", v);
        else k_snprintf(out, cap, "%+.2f", v);
    }
}

// ---------------------------------------------------------------------------------------------------- engine

void drum_derive(const float *v, drum_derived_t *n)
{
    m_set(n, 0, sizeof *n);
    n->type = CLAMP(m_lround(v[P_TYPE]), 0, DT_COUNT - 1);
    n->tune_ratio = m_exp2(v[P_TUNE] / 12.0f);
    n->length = v[P_LENGTH];
    n->punch = v[P_PUNCH];
    n->tone = v[P_TONE];
    n->grit = v[P_GRIT];
    n->vel_sens = v[P_VELSENS];
    const float tone_mul = m_exp2(n->tone);          // bright/dark macro on filters
    const float L = n->length;

    n->body_level = v[P_BODY_LEVEL];
    n->body_start = v[P_BODY_START] * n->tune_ratio;
    n->body_end = v[P_BODY_END] * n->tune_ratio;
    n->sweep_tau = v[P_BODY_SWEEP] * 0.001f / 3.0f;
    n->sweep_oct = m_log2(n->body_start / n->body_end);
    n->body_curve = v[P_BODY_CURVE];
    n->drop = v[P_BODY_DROP];
    n->drop_tau = v[P_BODY_DROPTIME] * 0.001f * L / 3.0f;
    const float sh = v[P_BODY_SHAPE];
    n->shape_k = 1.0f + sh * sh * 9.0f;
    n->shape_norm = 1.0f / soft_clip(n->shape_k);
    n->phase = v[P_BODY_PHASE];
    n->attack = v[P_BODY_ATTACK] * 0.001f * (1.4f - 0.8f * n->punch);
    n->hold = v[P_BODY_HOLD] * 0.001f * L;
    n->decay = v[P_BODY_DECAY] * 0.001f * L;
    n->decay_curve = v[P_BODY_DECAYCURVE];

    n->sub_level = v[P_SUB_LEVEL];
    n->sub_mul = v[P_SUB_OCTAVE] > 0.5f ? 0.5f : 1.0f;
    n->sub_track = v[P_SUB_TRACK];
    n->sub_decay = v[P_SUB_DECAY] * 0.001f * L;

    n->click_level = v[P_CLICK_LEVEL] * (0.35f + 1.3f * n->punch);
    n->click_type = m_lround(v[P_CLICK_TYPE]);
    n->click_freq = v[P_CLICK_FREQ] * tone_mul;
    n->click_len = v[P_CLICK_LEN] * 0.001f;

    n->noise_level = v[P_NOISE_LEVEL];
    n->noise_mode = m_lround(v[P_NOISE_MODE]);
    n->noise_freq = v[P_NOISE_FREQ] * tone_mul * m_sqrt(n->tune_ratio);
    n->noise_q = 0.5f + v[P_NOISE_RESO] * v[P_NOISE_RESO] * 9.5f;
    n->noise_attack = v[P_NOISE_ATTACK] * 0.001f;
    n->noise_decay = v[P_NOISE_DECAY] * 0.001f * L;
    n->noise_curve = v[P_NOISE_CURVE];
    n->noise_color = v[P_NOISE_COLOR];

    n->modes_level = v[P_MODES_LEVEL];
    n->modes_set = CLAMP(m_lround(v[P_MODES_SET]), 0, 6);
    n->modes_freq = v[P_MODES_FREQ] * n->tune_ratio;
    n->modes_count = CLAMP(m_lround(v[P_MODES_COUNT]), 1, 8);
    n->modes_decay = v[P_MODES_DECAY] * 0.001f * L;
    n->modes_damp = v[P_MODES_DAMP];
    n->modes_bend = v[P_MODES_BEND];
    n->modes_strike = v[P_MODES_STRIKE];
    n->modes_spread = v[P_MODES_SPREAD];

    n->metal_level = v[P_METAL_LEVEL];
    n->metal_type = m_lround(v[P_METAL_TYPE]);
    n->metal_tune = v[P_METAL_TUNE] * n->tune_ratio;
    n->metal_bp1 = v[P_METAL_BP1] * tone_mul;
    n->metal_bp2 = v[P_METAL_BP2] * tone_mul;
    n->metal_hp = v[P_METAL_HP] * m_sqrt(tone_mul);
    n->metal_attack = v[P_METAL_ATTACK] * 0.001f;
    n->metal_decay = v[P_METAL_DECAY] * 0.001f * L;
    n->metal_curve = v[P_METAL_CURVE];

    n->clap_level = v[P_CLAP_LEVEL];
    n->clap_count = CLAMP(m_lround(v[P_CLAP_COUNT]), 1, 8);
    n->clap_spacing = v[P_CLAP_SPACING] * 0.001f;
    n->clap_spread = v[P_CLAP_SPREAD];
    n->clap_freq = v[P_CLAP_FREQ] * tone_mul;
    n->clap_q = 0.6f + v[P_CLAP_Q] * v[P_CLAP_Q] * 7.0f;
    n->clap_burst = v[P_CLAP_BURST] * 0.001f;
    n->clap_decay = v[P_CLAP_DECAY] * 0.001f * L;

    // Grit pushes the drive; the drive knob sets the floor.
    n->drive_amount = m_clampf(v[P_DRIVE_AMOUNT] + n->grit * (1.0f - v[P_DRIVE_AMOUNT]) * 0.85f, 0.0f, 1.0f);
    n->drive_type = m_lround(v[P_DRIVE_TYPE]);
    n->drive_tone = MIN(v[P_DRIVE_TONE] * m_exp2(n->tone * 1.5f), 20000.0f);
    n->drive_mix = v[P_DRIVE_MIX];
    n->gate = v[P_SHAPE_GATE] * 0.001f;
}

static void compute(drum_t *s)
{
    const float *v = s->vals;
    drum_derive(v, &s->d);
    const drum_derived_t *n = &s->d;
    const float fs = s->sr;
    svf_set(&s->hp, v[P_EQ_HP], 0.707f, fs);
    svf_set(&s->lp, MIN(v[P_EQ_LP] * m_exp2(n->tone * 0.5f), fs * 0.45f), 0.707f, fs);
    bq_low_shelf(&s->low_shelf, v[P_EQ_LOWFREQ], v[P_EQ_LOW], fs);
    bq_peak(&s->mid_peak, v[P_EQ_MIDFREQ], v[P_EQ_MID], 0.9f, fs);
    bq_high_shelf(&s->high_shelf, 8000.0f, v[P_EQ_HIGH] + n->tone * 5.0f, fs);
    svf_set(&s->drive_lp, MIN(n->drive_tone, s->fsi * 0.2f), 0.6f, s->fsi);
    s->shape_attack = m_clampf(v[P_SHAPE_ATTACK] + (n->punch - 0.5f) * 0.5f, -1.0f, 1.0f);
    s->shape_sustain = v[P_SHAPE_SUSTAIN];
    const float pan = v[P_PAN], lvl = m_db(v[P_LEVEL]);
    s->gain_l = lvl * m_cos((pan + 1) * K_PI * 0.25f) * 1.41421356f;
    s->gain_r = lvl * m_sin((pan + 1) * K_PI * 0.25f) * 1.41421356f;
    s->dirty = false;
}

void drum_init(drum_t *s, float sr, int os)
{
    m_set(s, 0, sizeof *s);
    s->os = os >= 4 ? 4 : os >= 2 ? 2 : 1;
    s->sr = sr;
    s->fsi = sr * (float) s->os;
    // thump runs at 4x: white noise at a lower internal rate packs the same power into less bandwidth, and its
    // per-sample smoothers were tuned at 192 kHz. Both are scaled so 2x and 1x sound like 4x (and 4x is unchanged).
    s->noise_k = m_sqrt((float) s->os / 4.0f);
    s->noise_lp_c = 1.0f - m_pow(1.0f - 0.05f, 4.0f / (float) s->os);
    hb_design(&s->hb1, 15, 7.0f);    // 4x -> 2x: only has to clear 3/4 of the band
    hb_design(&s->hb2, 47, 9.0f);    // 2x -> 1x: the steep one
    drum_defaults(s->vals, DT_KICK);
    s->shape_gain = 1;
    s->dirty = true;
}

void drum_set(drum_t *s, const float *vals)
{
    m_copy(s->vals, vals, sizeof s->vals);
    s->dirty = true;
}

static void start_voice(drum_t *s, drum_voice_t *v, float vel, uint32_t seed, const drum_derived_t *dd)
{
    const float fs = s->fsi;
    m_set(v, 0, sizeof *v);
    v->d = *dd;
    const drum_derived_t *d = &v->d;
    v->on = true;
    v->age = ++s->counter;
    rng_seed(&v->rng, seed * 2654435761u + 17);
    v->vel = vel;
    v->fade = 1;
    v->pitch_e = v->drop_e = v->click_e = v->bend_e = v->strike_e = 1;
    v->bend_m = v->strike_m = 1;
    const float vs = d->vel_sens;
    v->gain = 1.0f - vs * (1.0f - m_pow(m_clampf(vel, 0.0f, 1.0f), 1.3f));
    const float bright = m_exp2((vel - 1.0f) * vs * 0.8f);   // softer hits are darker

    // body
    v->body_ph = d->phase;
    v->pitch_m = m_exp(-1.0f / (MAX(d->sweep_tau, 1e-5f) * fs));
    v->drop_m = m_exp(-1.0f / (MAX(d->drop_tau, 1e-5f) * fs));
    v->attack_n = (int) (d->attack * fs);
    v->hold_n = (int) (d->hold * fs);
    fdecay_start(&v->body_env, d->decay, d->decay_curve, fs);
    fdecay_start(&v->sub_env, d->sub_decay, 0.55f, fs);
    // click
    svf_set(&v->click_f, MIN(d->click_freq * bright, fs * 0.2f), d->click_type == 2 ? 6.0f : 1.2f, fs);
    v->click_m = m_exp(-1.0f / (MAX(d->click_len, 1e-4f) / 3.0f * fs));
    // noise
    svf_set(&v->noise_f, MIN(d->noise_freq * bright, fs * 0.2f), d->noise_q, fs);
    v->noise_attack_n = (int) (d->noise_attack * fs);
    fdecay_start(&v->noise_env, d->noise_decay, d->noise_curve, fs);
    // modes
    v->modes = 0;
    if (d->modes_level > 0) {
        const int set = d->modes_set;
        for (int k = 0; k < d->modes_count; ++k) {
            const float f = d->modes_freq * mode_ratios[set][k] * (1.0f + d->modes_spread * 0.06f * rng_bi(&v->rng));
            if (f > s->sr * 0.45f) break;
            const float T = d->modes_decay / (1.0f + d->modes_damp * (float) k * 1.2f);
            v->m_freq[k] = f;
            v->m_r[k] = m_exp(-6.9f / (MAX(T, 1e-3f) * fs));
            v->m_amp[k] = mode_amps[set][k];
            v->modes_norm += mode_amps[set][k];
            v->m_c[k] = m_cos(K_TWO_PI * f / fs);
            v->m_s[k] = m_sin(K_TWO_PI * f / fs);
            ++v->modes;
        }
        v->modes_norm = v->modes_norm > 0 ? 0.9f / v->modes_norm : 0.0f;
        v->bend_m = m_exp(-1.0f / (0.035f * fs));
        v->strike_m = m_exp(-1.0f / ((0.0004f + 0.003f * d->modes_strike) * fs));
    }
    v->strike_c = 1.0f - m_pow(1.0f - (0.08f + 0.9f * d->modes_strike), 4.0f / (float) s->os);
    if (s->os == 4) v->strike_c = 0.08f + 0.9f * d->modes_strike;
    // metal
    for (int k = 0; k < 6; ++k) {
        v->mt_freq[k] = METAL_BASE * metal_ratios[k] * d->metal_tune;
        v->mt_ph[k] = m_frac(0.37f * (float) k + 0.04f * rng_bi(&v->rng));   // mostly fixed: hits stay alike
    }
    svf_set(&v->mt_bp1, MIN(d->metal_bp1 * bright, fs * 0.2f), 3.0f, fs);
    svf_set(&v->mt_bp2, MIN(d->metal_bp2 * bright, fs * 0.2f), 3.0f, fs);
    svf_set(&v->mt_hp1, MIN(d->metal_hp, fs * 0.2f), 0.7f, fs);
    svf_set(&v->mt_hp2, MIN(d->metal_hp, fs * 0.2f), 0.7f, fs);
    v->metal_attack_n = (int) (d->metal_attack * fs);
    fdecay_start(&v->metal_env, d->metal_decay, d->metal_curve, fs);
    // clap: burst times with a little human scatter
    v->claps = d->clap_count;
    float t = 0;
    for (int k = 0; k < v->claps; ++k) {
        v->clap_t[k] = t;
        t += d->clap_spacing * (1.0f + d->clap_spread * 0.7f * rng_bi(&v->rng));
    }
    svf_set(&v->clap_f, MIN(d->clap_freq * bright, fs * 0.2f), d->clap_q, fs);
    // the bursts decay by a constant factor a sample: start each at exp(-(its first sample - its time) / burst)
    v->clap_m = m_exp(-1.0f / (MAX(d->clap_burst, 1e-5f) * fs));
    v->clap_tail_m = m_exp(-1.0f / (MAX(d->clap_decay * 0.2f, 1e-5f) * fs));
    for (int k = 0; k < v->claps; ++k) {
        float n = v->clap_t[k] * fs;
        int ni = (int) n; if ((float) ni < n) ni++;
        v->clap_n[k] = ni;
        v->clap_e[k] = m_exp(-((float) ni / fs - v->clap_t[k]) / MAX(d->clap_burst, 1e-5f));
    }
    v->clap_tail = v->claps ? m_exp(-((float) v->clap_n[v->claps - 1] / fs - v->clap_t[v->claps - 1]) / MAX(d->clap_decay * 0.2f, 1e-5f)) : 0;

    // How long this voice can sound, at the internal rate.
    float len = 0.01f;
    if (d->body_level > 0) len = MAX(len, d->attack + d->hold + d->decay);
    if (d->sub_level > 0) len = MAX(len, d->sub_decay);
    if (d->click_level > 0) len = MAX(len, d->click_len * 3.0f + 0.01f);
    if (d->noise_level > 0) len = MAX(len, d->noise_attack + d->noise_decay);
    if (d->modes_level > 0) len = MAX(len, d->modes_decay * 1.4f);
    if (d->metal_level > 0) len = MAX(len, d->metal_attack + d->metal_decay);
    if (d->clap_level > 0) len = MAX(len, v->clap_t[v->claps - 1] + d->clap_decay);
    len = MIN(len, d->gate);
    v->end = (int) (len * fs) + 32;
    v->gate_n = (int) (d->gate * fs);
    // each section's own end: past it, it adds nothing (the decays reach zero exactly there), so it is skipped
    v->body_end = (int) ((d->attack + d->hold + d->decay) * fs) + 2;
    v->sub_end = (int) (d->sub_decay * fs) + 2;
    v->noise_end = (int) ((d->noise_attack + d->noise_decay) * fs) + 2;
    v->metal_end = (int) ((d->metal_attack + d->metal_decay) * fs) + 2;
    v->modes_end = (int) (d->modes_decay * 1.4f * fs) + 2;
    v->clap_end = v->claps ? (int) ((v->clap_t[v->claps - 1] + 2.0f * d->clap_decay) * fs) + 2 : 0;   // the tail is 0.45 e^-5 at one decay
    ++s->active;
}

void drum_trigger(drum_t *s, float vel, uint32_t seed, const float *locked)
{
    if (s->dirty) compute(s);
    drum_derived_t ld;
    const drum_derived_t *d = &s->d;
    if (locked) { drum_derive(locked, &ld); d = &ld; }
    // Keep at most four voices sounding: fade the oldest out quickly, then take a free one.
    int sounding = 0;
    drum_voice_t *oldest = NULL;
    for (int i = 0; i < DRUM_VOICES; i++) {
        drum_voice_t *v = &s->voice[i];
        if (v->on && v->fade_step == 0) { ++sounding; if (!oldest || v->age < oldest->age) oldest = v; }
    }
    if (sounding >= 4 && oldest) oldest->fade_step = 1.0f / (0.003f * s->fsi);
    drum_voice_t *slot = NULL;
    for (int i = 0; i < DRUM_VOICES; i++) if (!s->voice[i].on) { slot = &s->voice[i]; break; }
    if (!slot) {   // everything busy: take the quietest fade
        slot = &s->voice[0];
        for (int i = 0; i < DRUM_VOICES; i++) if (s->voice[i].fade < slot->fade) slot = &s->voice[i];
        --s->active;   // it is replaced, not ended
    }
    start_voice(s, slot, vel, seed, d);
}

void drum_choke(drum_t *s)
{
    for (int i = 0; i < DRUM_VOICES; i++) {
        drum_voice_t *v = &s->voice[i];
        if (v->on && v->fade_step == 0) v->fade_step = 1.0f / (0.006f * s->fsi);
    }
}

static float render_voice(drum_t *s, drum_voice_t *v)
{
    const drum_derived_t *d = &v->d;
    const float fs = s->fsi, inv_fs = 1.0f / fs;
    const float t_sec = (float) v->t * inv_fs;
    (void) t_sec;
    float out = 0;

    // body
    float f_body = 0;
    const bool body_on = d->body_level > 0 && v->t < v->body_end, sub_on = d->sub_level > 0 && v->t < v->sub_end;
    if (body_on || sub_on) {
        const float e = v->pitch_e;
        const float log_f = d->body_end * m_exp2_fast(d->sweep_oct * e);
        const float lin_f = d->body_end + (d->body_start - d->body_end) * e;
        f_body = log_f + (lin_f - log_f) * d->body_curve;
        if (d->drop != 0) f_body *= m_exp2_fast(d->drop / 12.0f * (1.0f - v->drop_e));
        v->pitch_e *= v->pitch_m;
        v->drop_e *= v->drop_m;
    }
    if (body_on) {
        float amp;
        if (v->t < v->attack_n) amp = (float) v->t / (float) MAX(1, v->attack_n);
        else if (v->t < v->attack_n + v->hold_n) amp = 1.0f;
        else amp = fdecay_tick(&v->body_env);
        float sv = m_sin2pi(v->body_ph);
        if (d->shape_k > 1.001f) sv = soft_clip(sv * d->shape_k) * d->shape_norm;
        v->body_ph += f_body * inv_fs;
        v->body_ph -= m_floor(v->body_ph);
        out += sv * amp * d->body_level;
    }
    if (sub_on) {
        const float fixed = d->body_end * (d->drop != 0 ? m_exp2_fast(d->drop / 12.0f * (1.0f - v->drop_e)) : 1.0f);
        const float f = (fixed + (f_body - fixed) * d->sub_track) * d->sub_mul;
        const float a = MIN(1.0f, t_sec / 0.004f) * fdecay_tick(&v->sub_env);
        out += m_sin2pi(v->sub_ph) * a * d->sub_level;
        v->sub_ph += f * inv_fs;
        v->sub_ph -= m_floor(v->sub_ph);
    }

    // click
    if (d->click_level > 0 && v->click_e > 1e-4f) {
        float c = 0;
        const float e = v->click_e;
        if (d->click_type == 0 || d->click_type == 3) {
            svf_tick(&v->click_f, rng_bi(&v->rng) * s->noise_k);
            c += v->click_f.bp * (d->click_type == 3 ? 0.9f : 1.6f);
        }
        if (d->click_type == 1 || d->click_type == 3) {
            c += m_sin2pi(v->pop_ph) * (d->click_type == 3 ? 0.7f : 1.0f);
            v->pop_ph += d->click_freq * (0.3f + 0.7f * e) * inv_fs;
        }
        if (d->click_type == 2) {
            svf_tick(&v->click_f, v->t == 0 ? 9.0f : 0.0f);
            c = v->click_f.bp;
        }
        out += c * (d->click_type == 2 ? 1.0f : e) * d->click_level;
        v->click_e *= v->click_m;
    }

    // noise
    if (d->noise_level > 0 && v->t < v->noise_end) {
        const float amp = v->t < v->noise_attack_n ? (float) v->t / (float) MAX(1, v->noise_attack_n) : fdecay_tick(&v->noise_env);
        float w = rng_bi(&v->rng) * s->noise_k;
        v->noise_lp += (w - v->noise_lp) * s->noise_lp_c;      // about 1.5 kHz
        w -= d->noise_color * (w - v->noise_lp * 2.0f);        // colour: toward a darker, fuller noise
        svf_tick(&v->noise_f, w);
        const float y = d->noise_mode == 0 ? v->noise_f.lp : d->noise_mode == 1 ? v->noise_f.bp * 1.5f : v->noise_f.hp * 0.7f;
        out += y * amp * d->noise_level;
    }

    // modes
    if (v->modes > 0 && v->t < v->modes_end) {
        // excitation: an impulse, softened by a one-pole, plus a short noise burst for hard strikes
        const float x = v->t == 0 ? 1.0f : 0.0f;
        v->strike_lp += (x - v->strike_lp) * v->strike_c;
        float ex = v->strike_lp;
        // the resonators add the burst up over their decay: its level goes with sqrt(4 / os), not sqrt(os / 4)
        if (v->strike_e > 1e-4f) { ex += rng_bi(&v->rng) / s->noise_k * v->strike_e * d->modes_strike * 0.08f; v->strike_e *= v->strike_m; }
        if (d->modes_bend > 0 && (v->t & 15) == 0) {
            const float mul = m_exp2_fast(d->modes_bend / 12.0f * v->bend_e);
            for (int k = 0; k < v->modes; ++k) {
                const float w = K_TWO_PI * v->m_freq[k] * mul * inv_fs;
                v->m_c[k] = m_cos(w);
                v->m_s[k] = m_sin(w);
            }
        }
        v->bend_e *= v->bend_m;
        float m = 0;
        for (int k = 0; k < v->modes; ++k) {
            const float re = v->m_re[k], im = v->m_im[k];
            v->m_re[k] = (re * v->m_c[k] - im * v->m_s[k]) * v->m_r[k] + ex;
            v->m_im[k] = (re * v->m_s[k] + im * v->m_c[k]) * v->m_r[k];
            m += v->m_im[k] * v->m_amp[k];
        }
        out += m * d->modes_level * v->modes_norm;
    }

    // metal
    if (d->metal_level > 0 && v->t < v->metal_end) {
        float sm = 0;
        if (d->metal_type == 2) {   // fm pairs
            for (int k = 0; k < 6; k += 2) {
                const float mod = m_sin2pi(v->mt_ph[k + 1]) * 2.2f;
                sm += m_sin2pi(v->mt_ph[k] + mod * 0.16f);
            }
        } else {
            float sq[6];
            for (int k = 0; k < 6; ++k) {
                const float dt = v->mt_freq[k] * inv_fs;
                sq[k] = (v->mt_ph[k] < 0.5f ? 1.0f : -1.0f) + poly_blep(v->mt_ph[k], dt) - poly_blep(m_frac(v->mt_ph[k] + 0.5f), dt);
            }
            if (d->metal_type == 1) sm = (sq[0] * sq[1] + sq[2] * sq[3] + sq[4] * sq[5]) * 0.45f;
            else sm = (sq[0] + sq[1] + sq[2] + sq[3] + sq[4] + sq[5]) * 0.5f;
        }
        for (int k = 0; k < 6; ++k) { v->mt_ph[k] += v->mt_freq[k] * inv_fs; v->mt_ph[k] -= m_floor(v->mt_ph[k]); }
        svf_tick(&v->mt_bp1, sm);
        svf_tick(&v->mt_bp2, sm);
        float y = v->mt_bp1.bp + v->mt_bp2.bp;
        svf_tick(&v->mt_hp1, y);
        svf_tick(&v->mt_hp2, v->mt_hp1.hp);
        y = v->mt_hp2.hp;
        const float amp = v->t < v->metal_attack_n ? (float) v->t / (float) MAX(1, v->metal_attack_n) : fdecay_tick(&v->metal_env);
        out += y * amp * d->metal_level * 1.6f;
    }

    // clap: each burst a running decay from its start; the last one keeps a longer tail
    if (d->clap_level > 0 && v->t < v->clap_end) {
        float env = 0;
        for (int k = 0; k < v->claps; ++k) {
            if (v->t < v->clap_n[k]) break;
            const float b = v->clap_e[k];
            v->clap_e[k] *= v->clap_m;
            if (k == v->claps - 1) { env = MAX(env, MAX(b, 0.45f * v->clap_tail)); v->clap_tail *= v->clap_tail_m; }
            else env = MAX(env, b);
        }
        if (env > 1e-5f) {
            svf_tick(&v->clap_f, rng_bi(&v->rng) * s->noise_k);
            out += v->clap_f.bp * env * d->clap_level * 2.2f;
        }
    }

    // gate, choke, steal
    if (v->t > v->gate_n - (int) (0.005f * fs)) v->fade_step = MAX(v->fade_step, 1.0f / (0.005f * fs));
    out *= v->gain * v->fade;
    if (v->fade_step > 0) { v->fade -= v->fade_step; if (v->fade <= 0) v->on = false; }
    if (++v->t >= v->end) v->on = false;
    if (!v->on) --s->active;
    return out;
}

static void render_chunk(drum_t *s, float *L, float *R, int n)
{
    const drum_derived_t *d = &s->d;
    const int ni = n * s->os;
    float *I = s->buf_i;
    const float amt = d->drive_amount;
    const float pre = 1.0f + amt * amt * 30.0f;
    const float post = 1.0f / (1.0f + amt * 1.8f);
    const bool drive = amt > 0.001f;
    for (int i = 0; i < ni; ++i) {
        float x = 0;
        if (s->active > 0)
            for (int k = 0; k < DRUM_VOICES; k++)
                if (s->voice[k].on) x += render_voice(s, &s->voice[k]);
        if (drive) {
            float y = x * pre;
            switch (d->drive_type) {
            case 1: y = m_clampf(y, -1.0f, 1.0f); break;
            case 2: y = m_sin2pi(m_clampf(x * (1.0f + amt * 5.0f), -8.0f, 8.0f) * 0.25f); break;
            case 3:   // crush: fewer bits and a lower rate
                if (--s->crush_count <= 0) {
                    const float steps = m_exp2(12.0f - amt * 9.0f);
                    s->crush_hold = m_round(m_clampf(x * (1.0f + amt * 3.0f), -1.0f, 1.0f) * steps) / steps;
                    s->crush_count = 1 + (int) (amt * amt * 24.0f);
                }
                y = s->crush_hold;
                break;
            default: y = soft_clip(y); break;
            }
            if (d->drive_type != 3) y *= post;
            x += (y - x) * d->drive_mix;
        }
        if (d->drive_tone < 19900.0f) { svf_tick(&s->drive_lp, x); x = s->drive_lp.lp; }
        I[i] = x;
    }
    float out[DRUM_MAXBLOCK];
    if (s->os == 4) { hb_process(&s->hb1, I, s->buf_h, n * 2); hb_process(&s->hb2, s->buf_h, out, n); }
    else if (s->os == 2) hb_process(&s->hb2, I, out, n);
    else m_copy(out, I, sizeof(float) * (size_t) n);

    float pk = 0;
    for (int i = 0; i < n; ++i) {
        float x = out[i];
        svf_tick(&s->hp, x); x = s->hp.hp;
        svf_tick(&s->lp, x); x = s->lp.lp;
        x = bq_tick(&s->low_shelf, x);
        x = bq_tick(&s->mid_peak, x);
        x = bq_tick(&s->high_shelf, x);
        // transient shaper: a fast and a slow follower; their difference marks attack and sustain
        const float a = m_fabs(x);
        s->env_fast += (a - s->env_fast) * (a > s->env_fast ? 0.3f : 0.0015f);
        s->env_slow += (a - s->env_slow) * (a > s->env_slow ? 0.004f : 0.0004f);
        if (s->shape_attack != 0 || s->shape_sustain != 0) {
            const float diff = (s->env_fast - s->env_slow) / (MAX(s->env_fast, s->env_slow) + 1e-6f);
            const float db = diff > 0 ? s->shape_attack * 14.0f * diff : s->shape_sustain * 14.0f * -diff;
            const float g = m_db(db);
            s->shape_gain += (g - s->shape_gain) * 0.2f;
            x *= s->shape_gain;
        }
        L[i] += x * s->gain_l;
        R[i] += x * s->gain_r;
        pk = MAX(pk, m_fabs(x));
    }
    s->peak = MAX(pk, s->peak * 0.9f);
    if (s->active > 0) s->tail = (int) (s->sr * 0.08f);
    else s->tail = MAX(0, s->tail - n);
}

void drum_render(drum_t *s, float *L, float *R, int n)
{
    if (s->dirty) compute(s);
    if (!drum_active(s)) { s->peak *= 0.9f; return; }
    while (n > 0) {
        const int c = MIN(n, DRUM_MAXBLOCK);
        render_chunk(s, L, R, c);
        L += c; R += c; n -= c;
    }
}
