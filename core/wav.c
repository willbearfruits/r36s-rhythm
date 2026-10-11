#include "wav.h"

static uint32_t le32(const uint8_t *p) { return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t) (p[0] | p[1] << 8); }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24); }
static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); }

bool wav_parse(const uint8_t *f, int size, wav_info_t *w)
{
    m_set(w, 0, sizeof *w);
    if (size < 12 || m_cmp(f, "RIFF", 4) || m_cmp(f + 8, "WAVE", 4)) return false;
    bool fmt = false;
    for (int p = 12; p + 8 <= size;) {
        const uint32_t len = le32(f + p + 4);
        const uint8_t *c = f + p + 8;
        if (!m_cmp(f + p, "fmt ", 4) && len >= 16 && p + 8 + 16 <= size) {
            w->format = le16(c);
            w->file_chans = le16(c + 2);
            w->rate = le32(c + 4);
            w->bits = le16(c + 14);
            if (w->format == 0xFFFE && len >= 26) w->format = le16(c + 24);   // extensible: the sub-format
            fmt = true;
        } else if (!m_cmp(f + p, "data", 4)) {
            w->data_off = (uint32_t) (p + 8);
            w->data_len = MIN(len, (uint32_t) (size - p - 8));
            break;
        }
        p += 8 + (int) len + (int) (len & 1);
    }
    if (!fmt || !w->data_off || !w->file_chans || !w->rate) return false;
    if (!((w->format == 1 && (w->bits == 8 || w->bits == 16 || w->bits == 24 || w->bits == 32)) || (w->format == 3 && w->bits == 32))) return false;
    w->chans = MIN(w->file_chans, 2);
    w->frames = w->data_len / (uint32_t) (w->file_chans * (w->bits / 8));
    return w->frames > 0;
}

static int16_t sample(const uint8_t *p, const wav_info_t *w)
{
    switch (w->bits) {
    case 8: return (int16_t) (((int) p[0] - 128) << 8);
    case 16: return (int16_t) le16(p);
    case 24: return (int16_t) (p[1] | p[2] << 8);
    default:
        if (w->format == 3) {
            union { uint32_t u; float f; } v = { le32(p) };
            const float x = m_clampf(v.f, -1.0f, 1.0f) * 32767.0f;
            return (int16_t) (x < 0 ? x - 0.5f : x + 0.5f);
        }
        return (int16_t) (le32(p) >> 16);
    }
}

uint32_t wav_decode(const uint8_t *f, const wav_info_t *w, int16_t *out, uint32_t max)
{
    const uint32_t n = MIN(w->frames, max), bps = w->bits / 8, frame = bps * w->file_chans;
    const uint8_t *d = f + w->data_off;
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t c = 0; c < w->chans; c++) out[i * w->chans + c] = sample(d + i * frame + c * bps, w);
    return n;
}

void wav_header(uint8_t *h, uint32_t frames, int chans, uint32_t rate)
{
    const uint32_t data = frames * (uint32_t) chans * 2;
    m_copy(h, "RIFF", 4); put32(h + 4, 36 + data); m_copy(h + 8, "WAVEfmt ", 8);
    put32(h + 16, 16); put16(h + 20, 1); put16(h + 22, (uint16_t) chans); put32(h + 24, rate);
    put32(h + 28, rate * (uint32_t) chans * 2); put16(h + 32, (uint16_t) (chans * 2)); put16(h + 34, 16);
    m_copy(h + 36, "data", 4); put32(h + 40, data);
}
