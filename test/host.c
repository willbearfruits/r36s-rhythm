#define _POSIX_C_SOURCE 200809L
// The headless host: runs the whole instrument from a script, at 60 frames a second with the audio in step, and
// writes screenshots (PNG) and sound (WAV). Opens no window and plays nothing.
// Usage: build/host/rhythm-host OUTDIR "script"
//   w:MS        let MS milliseconds pass
//   p:BTN       press and let go (BTN: UP DOWN LEFT RIGHT A B X Y L1 L2 R1 R2 SELECT START L3 R3)
//   d:BTN u:BTN hold down, let go
//   s:LX,LY,RX,RY  the sticks, -1..1
//   shot:NAME   OUTDIR/NAME.png
//   wav:NAME    start recording OUTDIR/NAME.wav; endwav writes it
#include "../core/core.h"
#include "../core/wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SR 48000
static const char *outdir;
static uint32_t now_ms;
static double audio_due;
static float *rec_l, *rec_r;
static int rec_n, rec_cap;
static char rec_name[128];
static const uint32_t *fb;

// ------------------------------------------------------------------------------------------------- PNG
static uint32_t crc_table[256];
static uint32_t crc(uint32_t c, const uint8_t *b, size_t n)
{
    if (!crc_table[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1; crc_table[i] = v; }
    c = ~c;
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void be32(uint8_t *p, uint32_t v) { p[0] = (uint8_t) (v >> 24); p[1] = (uint8_t) (v >> 16); p[2] = (uint8_t) (v >> 8); p[3] = (uint8_t) v; }
static void chunk(FILE *f, const char *type, const uint8_t *d, uint32_t n)
{
    uint8_t h[8]; be32(h, n); memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f); fwrite(d, 1, n, f);
    uint32_t c = crc(0, (const uint8_t *) type, 4); c = crc(c, d, n);
    uint8_t t[4]; be32(t, c); fwrite(t, 1, 4, f);
}
static void png(const char *path, const uint32_t *px, int w, int h)
{
    const size_t raw_n = (size_t) h * (1 + (size_t) w * 3);
    uint8_t *raw = malloc(raw_n);
    for (int y = 0; y < h; y++) {
        uint8_t *r = raw + (size_t) y * (1 + (size_t) w * 3);
        r[0] = 0;
        for (int x = 0; x < w; x++) { const uint32_t c = px[y * w + x]; r[1 + 3 * x] = (uint8_t) (c >> 16); r[2 + 3 * x] = (uint8_t) (c >> 8); r[3 + 3 * x] = (uint8_t) c; }
    }
    // zlib, stored blocks
    const size_t nb = (raw_n + 65534) / 65535;
    uint8_t *z = malloc(raw_n + nb * 5 + 6);
    size_t o = 0;
    z[o++] = 0x78; z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw_n; i += 65535) {
        const size_t n = raw_n - i < 65535 ? raw_n - i : 65535;
        z[o++] = i + n >= raw_n;
        z[o++] = (uint8_t) n; z[o++] = (uint8_t) (n >> 8); z[o++] = (uint8_t) ~n; z[o++] = (uint8_t) (~n >> 8);
        memcpy(z + o, raw + i, n); o += n;
    }
    for (size_t i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    be32(z + o, (b << 16) | a); o += 4;
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    uint8_t ih[13]; be32(ih, (uint32_t) w); be32(ih + 4, (uint32_t) h); ih[8] = 8; ih[9] = 2; ih[10] = ih[11] = ih[12] = 0;
    chunk(f, "IHDR", ih, 13);
    chunk(f, "IDAT", z, (uint32_t) o);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw); free(z);
}

// ------------------------------------------------------------------------------------------------- time
static void frame(void)
{
    now_ms += 16;
    audio_due += SR * 0.016;
    static float L[2048], R[2048];
    while (audio_due >= 256) {
        eng_render(L, R, 256);
        audio_due -= 256;
        if (rec_l) {
            if (rec_n + 256 > rec_cap) { rec_cap = rec_cap * 2 + 65536; rec_l = realloc(rec_l, sizeof(float) * (size_t) rec_cap); rec_r = realloc(rec_r, sizeof(float) * (size_t) rec_cap); }
            memcpy(rec_l + rec_n, L, sizeof(float) * 256); memcpy(rec_r + rec_n, R, sizeof(float) * 256); rec_n += 256;
        }
    }
    fb = ui_frame(now_ms);
}

static int button(const char *s)
{
    for (int b = 0; b < BTN_COUNT; b++) if (!strcmp(btn_name[b], s)) return b;
    fprintf(stderr, "no button %s\n", s);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s OUTDIR \"script\"\n", argv[0]); return 1; }
    outdir = argv[1];
    mkdir(outdir, 0755);
    const size_t pool = 64u << 20;
    core_init(SR, malloc(pool), pool);
    frame();
    char *script = strdup(argv[2]);
    for (char *tok = strtok(script, " \n"); tok; tok = strtok(NULL, " \n")) {
        char path[512];
        if (!strncmp(tok, "w:", 2)) { const int ms = atoi(tok + 2); for (int t = 0; t < ms; t += 16) frame(); }
        else if (!strncmp(tok, "p:", 2)) { const int b = button(tok + 2); ui_button(b, true); for (int i = 0; i < 3; i++) frame(); ui_button(b, false); frame(); }
        else if (!strncmp(tok, "d:", 2)) { ui_button(button(tok + 2), true); frame(); }
        else if (!strncmp(tok, "u:", 2)) { ui_button(button(tok + 2), false); frame(); }
        else if (!strncmp(tok, "s:", 2)) { float v[4] = { 0 }; sscanf(tok + 2, "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]); ui_sticks(v[0], v[1], v[2], v[3]); frame(); }
        else if (!strncmp(tok, "shot:", 5)) { frame(); snprintf(path, sizeof path, "%s/%s.png", outdir, tok + 5); png(path, fb, 640, 480); }
        else if (!strncmp(tok, "wav:", 4)) { snprintf(rec_name, sizeof rec_name, "%s", tok + 4); rec_n = 0; rec_cap = 65536; rec_l = malloc(sizeof(float) * (size_t) rec_cap); rec_r = malloc(sizeof(float) * (size_t) rec_cap); }
        else if (!strcmp(tok, "endwav") && rec_l) {
            snprintf(path, sizeof path, "%s/%s.wav", outdir, rec_name);
            int16_t *pcm = malloc((size_t) rec_n * 4);
            for (int i = 0; i < rec_n; i++) { pcm[2 * i] = (int16_t) (m_clampf(rec_l[i], -1, 1) * 32767); pcm[2 * i + 1] = (int16_t) (m_clampf(rec_r[i], -1, 1) * 32767); }
            uint8_t h[44]; wav_header(h, (uint32_t) rec_n, 2, SR);
            FILE *f = fopen(path, "wb"); fwrite(h, 1, 44, f); fwrite(pcm, 4, (size_t) rec_n, f); fclose(f);
            free(pcm); free(rec_l); free(rec_r); rec_l = rec_r = NULL;
        }
        else { fprintf(stderr, "what is \"%s\"?\n", tok); return 1; }
    }
    return 0;
}
