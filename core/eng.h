// The engine: the audio thread's side. It keeps its own copy of the document (patched through the link), runs the
// sequencer sample-accurately, plays the twelve tracks, mixes, runs the effects and the master, sends MIDI and the
// sync pulse, and publishes its status.
#pragma once
#include "link.h"

void     eng_init(float sample_rate);                 // before the audio starts
extern int eng_drum_os;                                // drum oversampling, 4 (default) or 2 for a slow CPU: set it first
void     eng_render(float *L, float *R, int n);       // any n
uint64_t eng_frames(void);

// MIDI from the platform (bytes, any port, any thread: a ring) and to it (each message with the frame it is due at,
// counted like eng_frames()).
void     eng_midi_in(const uint8_t *bytes, int n);
int      eng_midi_out(uint8_t *bytes, int max, uint64_t *frame);   // one message, 0 when there is none

// More cores: if set, the engine hands it the twelve tracks of each pass as jobs (job(0) .. job(count - 1)), to run
// in any order on any cores; it returns when all are done. Unset, they run one after the other.
extern void (*eng_parallel)(void (*job)(int index), int count);

// For the checks: called with every hit the engine plays (track, the frame it starts at, velocity).
extern void (*eng_hit_hook)(int trk, uint64_t frame, float vel);
