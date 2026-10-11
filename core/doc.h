// The document: everything a project saves. One plain struct with no pointers, owned by the screen side; the engine
// keeps a byte-identical copy fed by patches through the link (core/link.h). Changing a struct changes the file
// format: bump DOC_VERSION.
#pragma once
#include "drum.h"

#define DOC_MAGIC   0x4b545352u     // "RSTK"
#define DOC_VERSION 1

#define NTRK      12
#define NSTEP     64
#define NBANK     8
#define NPERBANK  16
#define NPAT      (NBANK * NPERBANK)
#define NLOCK     256               // locked values per pattern
#define NSONG     128
#define NSLICE    32

enum { ENG_SYNTH, ENG_SAMPLE, ENG_COUNT };
enum { SEND_INT, SEND_MIDI, SEND_BOTH };
enum { STEP_ACCENT = 1, STEP_FLAM = 2 };
enum { RAT_FLAT, RAT_FADE, RAT_RISE };
// Conditions. A:B ("play on the A-th of every B loops") are COND_AB + (B - 2) * 8 + (A - 1), B 2..8.
enum { COND_ALWAYS, COND_FILL, COND_NOT_FILL, COND_FIRST, COND_NOT_FIRST, COND_PRE, COND_NOT_PRE, COND_AB = 16 };
#define COND_LAST (COND_AB + 6 * 8 + 7)
// Track speeds, as a multiple of the pattern's sixteenths.
enum { SPD_1_8, SPD_1_4, SPD_1_2, SPD_3_4, SPD_1, SPD_3_2, SPD_2, SPD_COUNT };
extern const float speed_mul[SPD_COUNT];
extern const char *const speed_name[SPD_COUNT];

typedef struct {
    uint8_t on;          // 0 off, 1 hit
    uint8_t vel;         // 1..127 (below 64 shows as a ghost)
    uint8_t flags;       // STEP_ACCENT, STEP_FLAM
    uint8_t ratchet;     // bits 0-2: hits - 1; bits 4-5: RAT_ shape
    uint8_t chance;      // 1..100 percent
    uint8_t cond;
    int8_t  nudge;       // -23..23, in 48ths of a step
    int8_t  note;        // synth: semitones; sample: slice 1..32 (0 = the track's own)
} step_t;

// Values a step can lock: the synth's own parameters (0..DRUM_NP-1), then these.
enum { LK_SMP_PITCH = 100, LK_SMP_START, LK_SMP_DECAY, LK_SMP_FILTER, LK_SMP_REVERSE, LK_LEVEL, LK_PAN, LK_SEND_DLY, LK_SEND_REV, LK_LAST };
// The perform lane: trk NTRK, param LK_PERF + a perform effect (core/perf.h): the right stick, recorded.
#define LK_PERF 120
typedef struct { uint8_t trk, step, param, pad; float val; } lock_t;

typedef struct { uint8_t len, speed, pad[2]; } ptrk_t;
typedef struct {
    step_t  step[NTRK][NSTEP];
    ptrk_t  trk[NTRK];
    uint8_t len;         // the pattern's own length in sixteenths: where it loops and where a queued one starts
    uint8_t swing;       // 50 (straight) .. 75
    uint8_t used, pad;
    uint16_t nlock, pad2;
    lock_t  lock[NLOCK];
} pattern_t;

enum { SMP_ONESHOT, SMP_GATE, SMP_LOOP };
enum { FLT_LP, FLT_BP, FLT_HP };
typedef struct {
    char    path[96];     // on the card, under SAMPLES/
    float   start, end;   // 0..1 of the file
    float   loop;         // loop start, 0..1 of [start, end]
    uint8_t mode, reverse, slices, pad;   // slices: 0 = the whole sample, else that many
    float   slice_pos[NSLICE];            // 0..1 of [start, end]; all zero = an even grid
    float   pitch;        // semitones -24..24
    float   attack, hold, decay;          // ms; decay 0 = to the end
    float   filter, reso; // cutoff Hz (20000 = open), 0..1
    uint8_t filter_mode, choke_self, pad2[2];
    float   bits;         // 16 = clean .. 2
    float   rate;         // Hz: 48000 = clean .. 2000
    float   drive, gain;  // 0..1, dB
} smp_t;

typedef struct {
    char    name[8];      // shown on the grid
    char    sound[24];    // the preset or file it came from
    uint8_t engine, choke, send, midi_ch, midi_note, mute, solo, pad;
    float   level, pan;   // dB, -1..1
    float   send_dly, send_rev;
    float   synth[DRUM_NP];
    smp_t   smp;
} track_t;

// A song entry: a pattern played repeat times. last_bar: what its last bar does on the last time round; next: what
// comes after it; mutes: tracks that stay quiet; tempo 0 = keep the tempo.
enum { LB_AS_IS, LB_FILL, LB_DROP };
enum { SN_GO_ON, SN_LOOP, SN_STOP };
typedef struct { uint8_t pat, repeat, last_bar, next; uint16_t mutes, pad; float tempo; } song_t;

enum { DLY_SYNC_16, DLY_SYNC_8T, DLY_SYNC_8, DLY_SYNC_3_16, DLY_SYNC_4T, DLY_SYNC_4, DLY_SYNC_3_8, DLY_SYNC_2, DLY_SYNC_COUNT };
typedef struct { uint8_t sync, pingpong, pad[2]; float time, repeats, tone, mix, wobble, depth, dirt; } delay_t;
typedef struct { float size, decay, damp, pre, width, move, ret; uint8_t freeze, pad[3]; } reverb_t;
enum { OUT_CLEAN, OUT_32K, OUT_26K, OUT_22K, OUT_12K, OUT_8K, OUT_COUNT };
typedef struct {
    float thresh, ratio, attack, release, makeup;   // bus compressor
    float low_cut, high_cut, drive, width;          // tone
    uint8_t drive_type, out_rate, out_bits, pad;    // OUT_ rate, 16/12/8
    float limit, volume;                            // dB
} master_t;

enum { CLK_INTERNAL, CLK_MIDI, CLK_AUTO };
enum { JACK_STEREO, JACK_SYNC_MONO, JACK_SPLIT };
typedef struct {
    uint8_t notes_out, notes_in, clock_out, clock_in, transport_out, spp_out, prog_in, cc_out;
    uint8_t jack, pulse_ppq, pulse_ms, pad;
    float pulse_level, pulse_offset_ms;
} io_t;

typedef struct {
    uint32_t magic, version, size, pad;
    char     name[32];
    char     kit[16];
    float    bpm;
    uint8_t  song_len, pad2[3];
    track_t  trk[NTRK];
    pattern_t pat[NPAT];
    song_t   song[NSONG];
    delay_t  dly;
    reverb_t rev;
    master_t mst;
    io_t     io;
} doc_t;

void doc_defaults(doc_t *d);                         // an empty project with the BASEMENT kit and pattern A01
void doc_pattern_clear(pattern_t *p);
void track_from_preset(track_t *t, int preset, const char *name);
void track_sample(track_t *t, const char *path, const char *name);

static inline int  step_hits(const step_t *s) { return (s->ratchet & 7) + 1; }
static inline int  step_rshape(const step_t *s) { return (s->ratchet >> 4) & 3; }
const char *cond_name(int cond, char *buf, int cap);
int  cond_next(int cond, int dir);                   // walks the list the screen shows
// Locked values: the index into p->lock, or -1.
int  lock_find(const pattern_t *p, int trk, int step, int param);
const char *lock_param_name(int param);
