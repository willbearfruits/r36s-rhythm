// WAV files in: PCM 8, 16, 24 and 32 bit, 32-bit float, mono or stereo (more channels: the first two), any rate.
// Out as 16-bit, interleaved when stereo, at the file's own rate (the sampler plays any rate).
#pragma once
#include "lib.h"

typedef struct { uint32_t rate, frames; uint16_t chans, bits, format; uint32_t data_off, data_len; uint16_t file_chans; } wav_info_t;
bool     wav_parse(const uint8_t *file, int size, wav_info_t *w);
uint32_t wav_decode(const uint8_t *file, const wav_info_t *w, int16_t *out, uint32_t max_frames);
// A 16-bit WAV header for frames x chans at rate, into 44 bytes.
void     wav_header(uint8_t *h, uint32_t frames, int chans, uint32_t rate);
