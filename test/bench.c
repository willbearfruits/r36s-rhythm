// How much of a core the engine takes, part by part: the default pattern, then a stress pattern (every track on
// every step, ratchets, the sample track sliced and pitched). Run it here and on the R36S to compare.
// Usage: build/host/bench [os] [seconds] [workers]
#include "../core/eng.h"
#include "../core/plat.h"
#include "../core/presets.h"
#include "../core/wav.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR 48000
int workers_start(int n);

static void msg(int type, int a, int b, float f) { msg_t m = { (uint8_t) type, (uint8_t) a, (uint8_t) b, 0, f, 0 }; ring_put(&to_eng, &m, sizeof m); }
static void run(int frames) { static float L[512], R[512]; while (frames > 0) { const int n = frames < 512 ? frames : 512; eng_render(L, R, n); frames -= n; } }

static void send_doc(const doc_t *d) { uint32_t off = 0; while (off < sizeof *d) { off += link_send_doc(d, off, (uint32_t) sizeof *d - off); run(64); } run(64); }

static void load_break(void)
{
    const char *path = "BUNDLED/BREAKS/DUSTY_94.WAV";
    const int size = plat_file_size(path);
    if (size <= 0) { printf("(no break sample: run build/host/render break)\n"); return; }
    uint8_t *f = malloc((size_t) size); plat_file_read(path, f, size);
    wav_info_t w; if (!wav_parse(f, size, &w)) return;
    int16_t *pcm = malloc((size_t) w.frames * w.chans * 2); wav_decode(f, &w, pcm, w.frames);
    uint8_t m[4 + sizeof(sample_t)] = { M_SAMPLE, 0 };
    sample_t s = { pcm, w.frames, w.rate, (uint8_t) w.chans, { 0, 0, 0 } };
    memcpy(m + 4, &s, sizeof s); ring_put(&to_eng, m, sizeof m);
    msg(M_BIND, 10, 0, 0); run(64);
}

static void measure(const char *name, const doc_t *d, int os, double secs)
{
    link_init();
    eng_drum_os = os;
    eng_init(SR);
    send_doc(d);
    load_break();
    msg(M_PLAY, 0, 0, 0);
    run(SR);   // settle
    const uint64_t t0 = plat_us();
    float trk[NTRK] = { 0 }, seq = 0, mix = 0, dly = 0, rev = 0, mst = 0; int n = 0;
    status_t st;
    for (int i = 0; i < (int) (secs * 4); i++) {
        run(SR / 4);
        status_read(&st);
        for (int t = 0; t < NTRK; t++) trk[t] += st.cpu_trk[t];
        seq += st.cpu_seq; mix += st.cpu_mix; dly += st.cpu_dly; rev += st.cpu_rev; mst += st.cpu_mst; n++;
    }
    const double total = (double) (plat_us() - t0) / 1e6 / secs;
    printf("%-9s %dx  %5.1f%% of a core:", name, os, total * 100);
    float drums = 0, smp = 0;
    for (int t = 0; t < NTRK; t++) { if (d->trk[t].engine == ENG_SYNTH) drums += trk[t] / n; else smp += trk[t] / n; }
    printf("  drums %.1f  samples %.1f  seq %.1f  mix %.1f  delay %.1f  reverb %.1f  master %.1f\n",
           drums * 100, smp * 100, seq / n * 100, mix / n * 100, dly / n * 100, rev / n * 100, mst / n * 100);
}

int main(int argc, char **argv)
{
    const int os = argc > 1 ? atoi(argv[1]) : 2;
    const double secs = argc > 2 ? atof(argv[2]) : 10;
    const int nw = argc > 3 ? atoi(argv[3]) : 0;
    if (nw) printf("%d workers besides the audio thread\n", workers_start(nw));
    static doc_t d;
    doc_defaults(&d);
    measure("default", &d, os, secs);
    // stress: every track on every step, ratchets on the hats, the sample track with every slice
    for (int t = 0; t < NTRK; t++) {
        d.pat[0].trk[t].len = 16; d.pat[0].trk[t].speed = SPD_1; d.trk[t].mute = 0;
        for (int s = 0; s < 16; s++) {
            step_t *st = &d.pat[0].step[t][s];
            st->on = 1; st->vel = 110; st->chance = 100; st->cond = 0;
            st->ratchet = (t == 4 || t == 8) ? 3 : 0;
            st->note = t == 10 ? (int8_t) (s % 8 + 1) : 0;
        }
    }
    d.trk[10].smp.choke_self = 0;
    measure("stress", &d, os, secs);
    return 0;
}
