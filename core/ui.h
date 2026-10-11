// The screen side, run from the platform's main loop: buttons and sticks in, a 640x480 picture out. It owns the
// document and talks to the engine only through the link.
#pragma once
#include "lib.h"

// The R36S's buttons. The PC hosts map keys and pads onto these by position.
enum { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_A, BTN_B, BTN_X, BTN_Y,
       BTN_L1, BTN_L2, BTN_R1, BTN_R2, BTN_SELECT, BTN_START, BTN_L3, BTN_R3, BTN_COUNT };
extern const char *const btn_name[BTN_COUNT];

void ui_init(void *pool, size_t pool_bytes, float sample_rate);   // pool: memory for samples
void ui_button(int btn, bool down);
void ui_sticks(float lx, float ly, float rx, float ry);           // -1..1, y down
const uint32_t *ui_frame(uint32_t ms);                             // the picture, 0xRRGGBB, 640x480
void ui_power(int battery_pct, bool charging);                     // -1: no battery (a PC)
void ui_midi_ports(const char *list);                               // the MIDI devices connected, a line each
void ui_host_info(const char *text);                                // audio driver, buffer, workers, dropouts: for DIAGNOSTICS
int  ui_audio_buffer(void);                                         // after core_init: frames the user asked for, 0 = host's choice
void ui_quit(void);                                                // before the platform stops: saves
