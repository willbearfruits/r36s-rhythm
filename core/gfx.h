// Drawing into the 640x480 screen: the shapes the mockups were drawn with (PIL's), and text in the Terminus fonts.
// Rectangles are inclusive (x0..x1, y0..y1), like PIL. Colours are 0xRRGGBB.
#pragma once
#include "font.h"

#define SCR_W 640
#define SCR_H 480
typedef uint32_t col_t;
#define RGB(r, g, b) ((col_t) (((uint32_t) (r) << 16) | ((uint32_t) (g) << 8) | (uint32_t) (b)))

extern uint32_t gfx_fb[SCR_W * SCR_H];

col_t c_mix(col_t a, col_t b, float t);
col_t c_scale(col_t c, float k);
col_t c_grey(col_t c);

void g_clear(col_t c);
void g_clip(int x0, int y0, int x1, int y1);
void g_noclip(void);
void g_pixel(int x, int y, col_t c);
void g_rect(int x0, int y0, int x1, int y1, col_t c);
void g_frame(int x0, int y0, int x1, int y1, col_t c, int w);
void g_rrect(int x0, int y0, int x1, int y1, int r, col_t c);
void g_rframe(int x0, int y0, int x1, int y1, int r, col_t c, int w);
void g_line(int x0, int y0, int x1, int y1, col_t c, int w);
void g_ellipse(int x0, int y0, int x1, int y1, col_t c);
void g_eframe(int x0, int y0, int x1, int y1, col_t c, int w);
void g_arc(int cx, int cy, int r, float a0, float a1, col_t c, int w);   // degrees, clockwise from 3 o'clock
void g_poly(const int *xy, int n, col_t c);                               // filled, n points
void g_tri(int x0, int y0, int x1, int y1, int x2, int y2, col_t c);
void g_checker(int x0, int y0, int x1, int y1, col_t c);
void g_dim(float k);                                                      // darken everything by k

int  g_text(int x, int y, const char *s, const font_t *f, col_t c);     // UTF-8; returns the x after it
int  g_tw(const char *s, const font_t *f);
void g_text_c(int cx, int y, const char *s, const font_t *f, col_t c);
void g_text_r(int rx, int y, const char *s, const font_t *f, col_t c);
void g_text_big(int x, int y, const char *s, const font_t *f, col_t c, int k);   // every pixel k x k
