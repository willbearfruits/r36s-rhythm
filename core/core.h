// The whole instrument, for a platform: start it, then call eng_render from the audio callback and ui_frame from
// the main loop, feed ui_button / ui_sticks / eng_midi_in, and drain eng_midi_out.
#pragma once
#include "eng.h"
#include "ui.h"

void core_init(float sample_rate, void *pool, size_t pool_bytes);
