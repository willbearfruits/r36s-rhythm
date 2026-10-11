// The factory library: thump's single drums and its eight-drum kits (each with a pattern), plus the twelve-track
// kits built from them.
#pragma once
#include "drum.h"

typedef struct { int16_t p; float v; } drum_pv_t;
typedef struct { const char *name, *genre; int type; const drum_pv_t *set; } drum_preset_t;
typedef struct { const char *name, *genre; const char *drums[8]; const char *rows[8]; float tempo, swing; } kit8_t;

extern const drum_preset_t drum_presets[];
extern const int drum_preset_count;
extern const kit8_t kit8_presets[];
extern const int kit8_count;
// The level that brings each drum to its model's loudness (kicks loudest, hats lower), measured over several hits
// the way thump does and baked in by `build/host/check levels` (core/drum_levels.c).
extern const float drum_preset_level[];

int  drum_preset_find(const char *name);           // -1 if there is none
void drum_preset_values(int i, float *v, bool levelled);
