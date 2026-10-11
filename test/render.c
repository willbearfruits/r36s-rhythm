// Renders WAV files to listen to (it plays nothing itself), into build/out/:
//   drums/NN-name.wav   every factory drum, one hit at full quality
//   basement.wav        the default project's pattern A01, four bars, through the whole engine
//   kits/NN-name.wav    thump's twelve kits, two bars each
// and the bundled break the CHOP track plays: build/card/BUNDLED/BREAKS/DUSTY_94.WAV (made by the engine itself).
// Usage: build/host/render [drums] [break] [basement] [kits]   (nothing: all)
#include "../core/eng.h"
#include "../core/presets.h"
#include "../core/wav.h"
#include "../core/plat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SR 48000

static void save_wav(const char *path, const float *L, const float *R, int n)
{
    int16_t *pcm = malloc((size_t) n * 4);
    for (int i = 0; i < n; i++) {
        pcm[2 * i] = (int16_t) (m_clampf(L[i], -1, 1) * 32767.0f);
        pcm[2 * i + 1] = (int16_t) (m_clampf(R[i], -1, 1) * 32767.0f);
    }
    uint8_t h[44];
    wav_header(h, (uint32_t) n, 2, SR);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(h, 1, 44, f);
    fwrite(pcm, 4, (size_t) n, f);
    fclose(f);
    free(pcm);
}

static void slug(char *out, const char *name)
{
    int j = 0;
    for (int i = 0; name[i] && j < 40; i++) {
        const char c = name[i];
        out[j++] = (c >= 'A' && c <= 'Z') ? (char) (c + 32) : ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ? c : '-';
    }
    out[j] = 0;
}

static void msg(int type, int a, int b, float f)
{
    msg_t m = { (uint8_t) type, (uint8_t) a, (uint8_t) b, 0, f, 0 };
    ring_put(&to_eng, &m, sizeof m);
}

static void idle(int frames) { float L[256], R[256]; while (frames > 0) { const int n = MIN(frames, 256); eng_render(L, R, n); frames -= n; } }

static void send_doc(const doc_t *d)
{
    uint32_t off = 0;
    while (off < sizeof *d) { off += link_send_doc(d, off, (uint32_t) sizeof *d - off); idle(64); }
    idle(64);
}

static void fresh(void) { link_init(); eng_init(SR); }

// A sample from the card into memory and into slot s, bound to track t. Returns the frames (0: none).
static uint32_t load_sample(const char *path, int s, int t)
{
    const int size = plat_file_size(path);
    if (size <= 0) return 0;
    uint8_t *file = malloc((size_t) size);
    plat_file_read(path, file, size);
    wav_info_t w;
    if (!wav_parse(file, size, &w)) { free(file); return 0; }
    int16_t *pcm = malloc((size_t) w.frames * w.chans * 2);
    wav_decode(file, &w, pcm, w.frames);
    free(file);
    uint8_t m[4 + sizeof(sample_t)] = { M_SAMPLE, (uint8_t) s };
    sample_t smp = { pcm, w.frames, w.rate, (uint8_t) w.chans, { 0, 0, 0 } };
    memcpy(m + 4, &smp, sizeof smp);
    ring_put(&to_eng, m, sizeof m);
    msg(M_BIND, t, s, 0);
    idle(64);
    return w.frames;
}

static void render(const char *path, int frames)
{
    float *L = calloc((size_t) frames, 4), *R = calloc((size_t) frames, 4);
    for (int i = 0; i < frames; i += 256) eng_render(L + i, R + i, MIN(256, frames - i));
    save_wav(path, L, R, frames);
    free(L); free(R);
}

static void drums(void)
{
    mkdir("build/out/drums", 0755);
    static drum_t d;
    static float L[SR * 5], R[SR * 5];
    for (int i = 0; i < drum_preset_count; i++) {
        float v[DRUM_NP];
        drum_preset_values(i, v, true);
        drum_init(&d, SR, 4);
        drum_set(&d, v);
        drum_trigger(&d, 1.0f, 1, NULL);
        memset(L, 0, sizeof L); memset(R, 0, sizeof R);
        int n = 0;
        while (n + 256 <= SR * 5) { drum_render(&d, L + n, R + n, 256); n += 256; if (!drum_active(&d)) break; }
        char s[64], path[128];
        slug(s, drum_presets[i].name);
        snprintf(path, sizeof path, "build/out/drums/%02d-%s.wav", i + 1, s);
        save_wav(path, L, R, n);
    }
    printf("drums: %d files in build/out/drums/\n", drum_preset_count);
}

// thump's eight-drum kit into the first eight tracks, its pattern into A01; tracks 9-12 silent.
static void kit_doc(doc_t *d, const kit8_t *k)
{
    doc_defaults(d);
    d->bpm = k->tempo;
    pattern_t *p = &d->pat[0];
    doc_pattern_clear(p);
    p->used = 1;
    p->swing = (uint8_t) (50 + k->swing * 50 + 0.5f);
    for (int t = 0; t < 8; t++) {
        track_from_preset(&d->trk[t], drum_preset_find(k->drums[t]), drum_presets[drum_preset_find(k->drums[t])].name);
        for (int s = 0; k->rows[t][s] && s < NSTEP; s++) {
            const char c = k->rows[t][s];
            step_t *st = &p->step[t][s];
            if (c == '.') continue;
            st->on = 1;
            st->vel = c == 'X' ? 127 : c == 'x' ? 100 : 57;
            if (c == 'X') st->flags |= STEP_ACCENT;
        }
    }
    for (int t = 8; t < NTRK; t++) d->trk[t].mute = 1;
    d->trk[5].choke = d->trk[4].choke = 1;
}

static void make_break(void)
{
    static doc_t d;
    fresh();
    for (int k = 0; k < kit8_count; k++) if (!strcmp(kit8_presets[k].name, "Lo-Fi Beat")) kit_doc(&d, &kit8_presets[k]);
    d.bpm = 94;
    d.mst.out_rate = OUT_26K; d.mst.out_bits = 12; d.mst.drive = 0.35f; d.mst.drive_type = 1; d.mst.high_cut = 11000;
    for (int t = 0; t < NTRK; t++) { d.trk[t].send_dly = 0; d.trk[t].send_rev = t == 1 ? 0.15f : 0; }
    send_doc(&d);
    msg(M_PLAY, 0, 0, 0);
    const int bar = (int) (SR * 60.0 / 94.0 * 4.0 + 0.5);
    mkdir("build/card", 0755); mkdir("build/card/BUNDLED", 0755); mkdir("build/card/BUNDLED/BREAKS", 0755);
    render("build/card/BUNDLED/BREAKS/DUSTY_94.WAV", bar);
    printf("break: build/card/BUNDLED/BREAKS/DUSTY_94.WAV (one bar at 94 BPM, from the engine)\n");
}

static void basement(void)
{
    static doc_t d;
    fresh();
    doc_defaults(&d);
    send_doc(&d);
    const uint32_t got = load_sample(d.trk[10].smp.path, 0, 10);
    msg(M_PLAY, 0, 0, 0);
    const int bar = (int) (SR * 60.0 / d.bpm * 4.0 + 0.5);
    render("build/out/basement.wav", bar * 4 + SR * 3 / 2);
    printf("basement: build/out/basement.wav (A01, four bars at %.0f BPM; the CHOP sample %s)\n", d.bpm, got ? "loaded" : "missing: run with break first");
}

static void kits(void)
{
    static doc_t d;
    mkdir("build/out/kits", 0755);
    for (int k = 0; k < kit8_count; k++) {
        fresh();
        kit_doc(&d, &kit8_presets[k]);
        send_doc(&d);
        msg(M_PLAY, 0, 0, 0);
        const int bar = (int) (SR * 60.0 / d.bpm * 4.0 + 0.5);
        char s[64], path[128];
        slug(s, kit8_presets[k].name);
        snprintf(path, sizeof path, "build/out/kits/%02d-%s.wav", k + 1, s);
        render(path, bar * 2 + SR);
    }
    printf("kits: %d files in build/out/kits/\n", kit8_count);
}

int main(int argc, char **argv)
{
    mkdir("build", 0755); mkdir("build/out", 0755);
    const bool all = argc < 2;
    for (int a = all ? 0 : 1; a < (all ? 1 : argc); a++) {
        const char *w = all ? "" : argv[a];
        if (all || !strcmp(w, "drums")) drums();
        if (all || !strcmp(w, "break")) make_break();
        if (all || !strcmp(w, "basement")) basement();
        if (all || !strcmp(w, "kits")) kits();
    }
    return 0;
}
