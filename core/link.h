// The screen side and the engine meet only here: two single-producer single-consumer rings of whole messages
// (screen -> engine, engine -> screen) and a status block the engine publishes under a sequence lock. A message that
// depends on document changes must go after them: link_sync() sends the pending patches first.
#pragma once
#include "doc.h"
#include <stdatomic.h>

typedef struct {
    uint8_t *buf;
    uint32_t size;                 // a power of two
    _Atomic uint32_t w, r;
} ring_t;
void     ring_init(ring_t *q, void *mem, uint32_t size);
bool     ring_put(ring_t *q, const void *msg, uint32_t len);   // the whole message or nothing
uint32_t ring_get(ring_t *q, void *out, uint32_t cap);         // one message, 0 when there is none
uint32_t ring_free(ring_t *q);

// A sample in memory, ready to play: 16-bit, interleaved when stereo. The screen side loads it; the engine only reads.
typedef struct { const int16_t *data; uint32_t frames, rate; uint8_t chans, pad[3]; } sample_t;
#define NSMPSLOT 64

enum {
    M_PATCH = 1,     // bytes of the document: { off, len } then the bytes
    M_PLAY,          // a: 0 from the top of the pattern, 1 continue
    M_STOP,
    M_QUEUE,         // a: pattern, b: 1 = now (restart), 0 = at the end of this one
    M_SONG,          // a: 1 on, 0 off; b: entry to start from
    M_HIT,           // a: track, f: velocity (pads, auditions); b: slice or note + 64
    M_LIVE,          // a: track, b: param, f: value; c: 1 = back to the document's; 255 track = all back
    M_FILL,          // a: 0/1
    M_SAMPLE,        // a: slot, then a sample_t (data NULL = empty the slot)
    M_BIND,          // a: track, b: slot (255 none)
    M_PERF,          // a: a perform effect (perf.h), f: how much, 0 = off (FILTER: -1..1); 255 = all off
    M_TEMPO,         // f: bpm (tap, MIDI): the document keeps its own
    M_QUALITY,       // a: drum oversampling 1, 2 or 4 (0: the platform's default)
    M_PREVIEW,       // a: slot, f: velocity: plays a sample by itself (the browser); b: 1 = a factory drum, a = preset
    M_CAPTURE,       // then a capture_t: records the output (or one track) into memory; M_ACK a = M_CAPTURE when full
    M_ACK = 100,     // engine -> screen: a: what was dropped, b: slot
    M_MON,           // engine -> screen: a MIDI message in (a 0) or out (a 1): b status, c data 1, f data 2
};
typedef struct { uint8_t type, a, b, c; float f, g; } msg_t;
// What M_CAPTURE records: src 0 = the output (after the master), 1..12 = that track alone; start 1 = play the
// pattern (or song) from the top as it begins, 2 = begin with the next bar.
typedef struct { int16_t *buf; uint32_t frames; uint8_t src, start, pad[2]; } capture_t;
typedef struct { uint8_t type, pad[3]; uint32_t off, len; } msg_patch_t;

typedef struct {
    _Atomic uint32_t seq;
    uint8_t playing, pat, next_pat, song_on;
    uint8_t step[NTRK];            // the step each track is on
    uint8_t song_pos, song_rep, pad[2];
    uint32_t bar, frames;          // bars since play, frames rendered
    float beat;                    // position in the bar, 0..4
    uint32_t hits[NTRK];           // counts up with every hit: the screen flashes when it changes
    float level[NTRK], out_l, out_r, comp_gr, limit_gr;
    float cpu;                     // share of the block time spent rendering
    // where it goes, as shares of real time (1.0 = a whole core): each track, then the sequencer, the mix, the
    // delay, the reverb and the master
    float cpu_trk[NTRK], cpu_seq, cpu_mix, cpu_dly, cpu_rev, cpu_mst;
    float bpm;
    uint32_t cap_pos, cap_frames;  // a capture's progress
} status_t;

extern ring_t to_eng, to_ui;
void link_init(void);              // before either side starts
// Sends [off, off + len) of a document as patches, as far as the ring has room: the bytes it sent.
uint32_t link_send_doc(const doc_t *d, uint32_t off, uint32_t len);
#define PATCH_MAX 4096
extern status_t eng_status;        // written by the engine, read with status_read()
// The last moments of the output, for the scope and the spectrum: written by the engine, read as it is.
#define SCOPE_LEN 2048
extern float eng_scope_l[SCOPE_LEN], eng_scope_r[SCOPE_LEN];
extern _Atomic uint32_t eng_scope_head;
void status_read(status_t *out);
void status_begin(void);
void status_end(void);
