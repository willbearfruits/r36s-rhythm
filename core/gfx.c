#include "gfx.h"

uint32_t gfx_fb[SCR_W * SCR_H];
static int cx0 = 0, cy0 = 0, cx1 = SCR_W - 1, cy1 = SCR_H - 1;

col_t c_mix(col_t a, col_t b, float t)
{
    const int ar = (int) (a >> 16) & 255, ag = (int) (a >> 8) & 255, ab = (int) a & 255;
    const int br = (int) (b >> 16) & 255, bg = (int) (b >> 8) & 255, bb = (int) b & 255;
    return RGB(m_lround((float) ar + (float) (br - ar) * t), m_lround((float) ag + (float) (bg - ag) * t), m_lround((float) ab + (float) (bb - ab) * t));
}

col_t c_scale(col_t c, float k)
{
    const int r = CLAMP((int) ((float) ((c >> 16) & 255) * k), 0, 255), g = CLAMP((int) ((float) ((c >> 8) & 255) * k), 0, 255);
    const int b = CLAMP((int) ((float) (c & 255) * k), 0, 255);
    return RGB(r, g, b);
}

col_t c_grey(col_t c)
{
    const int v = (int) ((float) (((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255)) / 3.0f * 0.55f);
    return RGB(v, v, v);
}

void g_clip(int x0, int y0, int x1, int y1)
{
    cx0 = MAX(0, x0); cy0 = MAX(0, y0); cx1 = MIN(SCR_W - 1, x1); cy1 = MIN(SCR_H - 1, y1);
}
void g_noclip(void) { cx0 = 0; cy0 = 0; cx1 = SCR_W - 1; cy1 = SCR_H - 1; }

void g_clear(col_t c) { for (int i = 0; i < SCR_W * SCR_H; i++) gfx_fb[i] = c; }

void g_pixel(int x, int y, col_t c)
{
    if (x >= cx0 && x <= cx1 && y >= cy0 && y <= cy1) gfx_fb[y * SCR_W + x] = c;
}

static void span(int y, int x0, int x1, col_t c)
{
    if (y < cy0 || y > cy1) return;
    if (x0 < cx0) x0 = cx0;
    if (x1 > cx1) x1 = cx1;
    uint32_t *p = &gfx_fb[y * SCR_W];
    for (int x = x0; x <= x1; x++) p[x] = c;
}

void g_rect(int x0, int y0, int x1, int y1, col_t c) { for (int y = y0; y <= y1; y++) span(y, x0, x1, c); }

void g_frame(int x0, int y0, int x1, int y1, col_t c, int w)
{
    g_rect(x0, y0, x1, y0 + w - 1, c);
    g_rect(x0, y1 - w + 1, x1, y1, c);
    g_rect(x0, y0 + w, x0 + w - 1, y1 - w, c);
    g_rect(x1 - w + 1, y0 + w, x1, y1 - w, c);
}

// The span of a rounded rectangle on row y: false if the row is outside it.
static bool rr_span(int x0, int y0, int x1, int y1, int r, int y, int *a, int *b)
{
    if (y < y0 || y > y1 || x1 < x0) return false;
    r = MIN(r, MIN((x1 - x0 + 1) / 2, (y1 - y0 + 1) / 2));
    int in = 0;
    if (r > 0) {
        float dy = -1;
        if (y < y0 + r) dy = (float) (y0 + r) - (float) y - 0.5f;
        else if (y > y1 - r) dy = (float) y - (float) (y1 - r) + 0.5f;
        if (dy > 0) in = r - m_lround(m_sqrt(MAX(0.0f, (float) (r * r) - dy * dy)));
    }
    *a = x0 + in; *b = x1 - in;
    return *a <= *b;
}

void g_rrect(int x0, int y0, int x1, int y1, int r, col_t c)
{
    for (int y = y0; y <= y1; y++) { int a, b; if (rr_span(x0, y0, x1, y1, r, y, &a, &b)) span(y, a, b, c); }
}

void g_rframe(int x0, int y0, int x1, int y1, int r, col_t c, int w)
{
    for (int y = y0; y <= y1; y++) {
        int a, b, ia, ib;
        if (!rr_span(x0, y0, x1, y1, r, y, &a, &b)) continue;
        if (rr_span(x0 + w, y0 + w, x1 - w, y1 - w, MAX(r - w, 0), y, &ia, &ib)) { span(y, a, ia - 1, c); span(y, ib + 1, b, c); }
        else span(y, a, b, c);
    }
}

void g_line(int x0, int y0, int x1, int y1, col_t c, int w)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    const int h = (w - 1) / 2;
    for (;;) {
        if (w <= 1) g_pixel(x0, y0, c); else g_rect(x0 - h, y0 - h, x0 - h + w - 1, y0 - h + w - 1, c);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static bool el_span(int x0, int y0, int x1, int y1, int y, int *a, int *b)
{
    if (y < y0 || y > y1 || x1 < x0) return false;
    const float rx = (float) (x1 - x0 + 1) * 0.5f, ry = (float) (y1 - y0 + 1) * 0.5f;
    const float cxf = (float) x0 + rx, cyf = (float) y0 + ry;
    const float t = ((float) y + 0.5f - cyf) / ry;
    if (t * t > 1) return false;
    const float hw = rx * m_sqrt(1 - t * t);
    *a = m_lround(cxf - hw); *b = m_lround(cxf + hw) - 1;
    return *a <= *b;
}

void g_ellipse(int x0, int y0, int x1, int y1, col_t c)
{
    for (int y = y0; y <= y1; y++) { int a, b; if (el_span(x0, y0, x1, y1, y, &a, &b)) span(y, a, b, c); }
}

void g_eframe(int x0, int y0, int x1, int y1, col_t c, int w)
{
    for (int y = y0; y <= y1; y++) {
        int a, b, ia, ib;
        if (!el_span(x0, y0, x1, y1, y, &a, &b)) continue;
        if (el_span(x0 + w, y0 + w, x1 - w, y1 - w, y, &ia, &ib)) { span(y, a, ia - 1, c); span(y, ib + 1, b, c); }
        else span(y, a, b, c);
    }
}

void g_arc(int cx, int cy, int r, float a0, float a1, col_t c, int w)
{
    if (a1 < a0) { const float t = a0; a0 = a1; a1 = t; }
    for (int y = cy - r; y <= cy + r; y++)
        for (int x = cx - r; x <= cx + r; x++) {
            const float dx = (float) x - (float) cx, dy = (float) y - (float) cy;
            const float d2 = dx * dx + dy * dy;
            if (d2 > ((float) r + 0.5f) * ((float) r + 0.5f) || d2 < ((float) (r - w) + 0.5f) * ((float) (r - w) + 0.5f)) continue;
            float a = m_atan2(dy, dx) * (180.0f / K_PI);
            while (a < a0) a += 360.0f;
            if (a <= a1) g_pixel(x, y, c);
        }
}

void g_poly(const int *xy, int n, col_t c)
{
    int ymin = xy[1], ymax = xy[1];
    for (int i = 1; i < n; i++) { ymin = MIN(ymin, xy[2 * i + 1]); ymax = MAX(ymax, xy[2 * i + 1]); }
    for (int y = ymin; y <= ymax; y++) {
        float xs[16]; int k = 0;
        const float fy = (float) y + 0.5f;
        for (int i = 0; i < n && k < 16; i++) {
            const int j = (i + 1) % n;
            const float ya = (float) xy[2 * i + 1], yb = (float) xy[2 * j + 1];
            if ((ya <= fy && yb > fy) || (yb <= fy && ya > fy)) {
                const float xa = (float) xy[2 * i], xb = (float) xy[2 * j];
                xs[k++] = xa + (fy - ya) / (yb - ya) * (xb - xa);
            }
        }
        for (int i = 1; i < k; i++) { const float t = xs[i]; int j = i - 1; while (j >= 0 && xs[j] > t) { xs[j + 1] = xs[j]; j--; } xs[j + 1] = t; }
        for (int i = 0; i + 1 < k; i += 2) span(y, m_lround(xs[i]), m_lround(xs[i + 1]) - 1, c);
    }
    // the outline too, so thin shapes do not vanish
    for (int i = 0; i < n; i++) { const int j = (i + 1) % n; g_line(xy[2 * i], xy[2 * i + 1], xy[2 * j], xy[2 * j + 1], c, 1); }
}

void g_tri(int x0, int y0, int x1, int y1, int x2, int y2, col_t c)
{
    const int p[6] = { x0, y0, x1, y1, x2, y2 };
    g_poly(p, 3, c);
}

void g_checker(int x0, int y0, int x1, int y1, col_t c)
{
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) if (((x >> 1) + (y >> 1)) & 1) g_pixel(x, y, c);
}

void g_dim(float k)
{
    for (int i = 0; i < SCR_W * SCR_H; i++) gfx_fb[i] = c_scale(gfx_fb[i], 1.0f - k);
}

// ---------------------------------------------------------------------------------------------------- text

int font_glyph(uint32_t code)
{
    if (code >= 32 && code < 127) return (int) code - 32;
    for (int i = 95; i < font_ncodes; i++) if (font_codes[i] == code) return i;
    return '?' - 32;
}

static uint32_t utf8(const char **s)
{
    const uint8_t *p = (const uint8_t *) *s;
    uint32_t c = p[0];
    int n = 1;
    if (c >= 0xF0 && p[1] && p[2] && p[3]) { c = (c & 7) << 18 | (uint32_t) (p[1] & 63) << 12 | (uint32_t) (p[2] & 63) << 6 | (p[3] & 63); n = 4; }
    else if (c >= 0xE0 && p[1] && p[2]) { c = (c & 15) << 12 | (uint32_t) (p[1] & 63) << 6 | (p[2] & 63); n = 3; }
    else if (c >= 0xC0 && p[1]) { c = (c & 31) << 6 | (p[1] & 63); n = 2; }
    *s += n;
    return c;
}

static void glyph(int x, int y, int g, const font_t *f, col_t c, int k)
{
    const uint8_t *b = f->bits + (size_t) g * f->h * f->bpr;
    for (int r = 0; r < f->h; r++)
        for (int col = 0; col < f->w; col++)
            if (b[r * f->bpr + col / 8] & (0x80 >> (col & 7))) {
                if (k == 1) g_pixel(x + col, y + r, c);
                else g_rect(x + col * k, y + r * k, x + col * k + k - 1, y + r * k + k - 1, c);
            }
}

int g_text(int x, int y, const char *s, const font_t *f, col_t c)
{
    while (*s) { glyph(x, y, font_glyph(utf8(&s)), f, c, 1); x += f->w; }
    return x;
}

void g_text_big(int x, int y, const char *s, const font_t *f, col_t c, int k)
{
    while (*s) { glyph(x, y, font_glyph(utf8(&s)), f, c, k); x += f->w * k; }
}

int g_tw(const char *s, const font_t *f)
{
    int n = 0;
    while (*s) { utf8(&s); n++; }
    return n * f->w;
}

void g_text_c(int cx, int y, const char *s, const font_t *f, col_t c) { g_text(cx - g_tw(s, f) / 2, y, s, f, c); }
void g_text_r(int rx, int y, const char *s, const font_t *f, col_t c) { g_text(rx - g_tw(s, f), y, s, f, c); }
