#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
// MIDI on Linux through the ALSA sequencer, on its own thread (1 ms): two ports, "rhythm in" (it subscribes to every
// readable port there is, and to new ones as they appear) and "rhythm out" (notes, clock, start and stop, for a DAW
// or a synth to subscribe to). Adapted from keys' desktop/midi_alsa.c.
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../core/eng.h"

static snd_seq_t *seq;
static int my_client, my_in, my_out;
static pthread_t thread;
static volatile int running;

static pthread_mutex_t names_lock = PTHREAD_MUTEX_INITIALIZER;
static char names[160];

// In from everything that plays; out to every device (hardware: a DAW subscribes by itself, and the through port
// would come straight back in).
static bool connect_port(int client, int port, unsigned caps, unsigned type)
{
    if (client == my_client || client == SND_SEQ_CLIENT_SYSTEM || (caps & SND_SEQ_PORT_CAP_NO_EXPORT)) return false;
    const unsigned rd = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ, wr = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
    if ((caps & rd) == rd) snd_seq_connect_from(seq, my_in, client, port);
    if ((caps & wr) == wr && (type & SND_SEQ_PORT_TYPE_HARDWARE)) snd_seq_connect_to(seq, my_out, client, port);
    return (type & SND_SEQ_PORT_TYPE_HARDWARE) && (caps & (rd | wr));
}

// Connects every port there is, and remembers the devices' names for the MIDI page.
static void connect_all(void)
{
    char list[sizeof names] = "";
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;
    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    while (snd_seq_query_next_client(seq, ci) >= 0) {
        const int c = snd_seq_client_info_get_client(ci);
        bool dev = false;
        snd_seq_port_info_set_client(pi, c);
        snd_seq_port_info_set_port(pi, -1);
        while (snd_seq_query_next_port(seq, pi) >= 0)
            dev |= connect_port(c, snd_seq_port_info_get_port(pi), snd_seq_port_info_get_capability(pi), snd_seq_port_info_get_type(pi));
        if (dev && strlen(list) + strlen(snd_seq_client_info_get_name(ci)) + 2 < sizeof list) {
            strcat(list, snd_seq_client_info_get_name(ci));
            strcat(list, "\n");
        }
    }
    pthread_mutex_lock(&names_lock);
    memcpy(names, list, sizeof names);
    pthread_mutex_unlock(&names_lock);
}

void midi_ports(char *out, int cap)
{
    pthread_mutex_lock(&names_lock);
    snprintf(out, (size_t) cap, "%s", seq ? names : "NO MIDI");
    pthread_mutex_unlock(&names_lock);
}

static void out(const uint8_t *b, int n)
{
    snd_seq_event_t ev;
    snd_seq_ev_clear(&ev);
    snd_seq_ev_set_source(&ev, my_out);
    snd_seq_ev_set_subs(&ev);
    snd_seq_ev_set_direct(&ev);
    const int ch = b[0] & 15;
    switch (b[0] & 0xF0) {
    case 0x90: snd_seq_ev_set_noteon(&ev, ch, b[1], b[2]); break;
    case 0x80: snd_seq_ev_set_noteoff(&ev, ch, b[1], 0); break;
    case 0xB0: snd_seq_ev_set_controller(&ev, ch, b[1], b[2]); break;
    case 0xC0: snd_seq_ev_set_pgmchange(&ev, ch, b[1]); break;
    case 0xF0:
        if (b[0] == 0xF8) ev.type = SND_SEQ_EVENT_CLOCK;
        else if (b[0] == 0xFA) ev.type = SND_SEQ_EVENT_START;
        else if (b[0] == 0xFB) ev.type = SND_SEQ_EVENT_CONTINUE;
        else if (b[0] == 0xFC) ev.type = SND_SEQ_EVENT_STOP;
        else if (b[0] == 0xF2 && n >= 3) { ev.type = SND_SEQ_EVENT_SONGPOS; ev.data.control.value = b[1] | b[2] << 7; }
        else return;
        break;
    default: return;
    }
    snd_seq_event_output_direct(seq, &ev);
}

static void in(const snd_seq_event_t *ev)
{
    uint8_t b[3];
    switch (ev->type) {
    case SND_SEQ_EVENT_NOTEON: b[0] = 0x90 | (ev->data.note.channel & 15); b[1] = ev->data.note.note; b[2] = ev->data.note.velocity; eng_midi_in(b, 3); break;
    case SND_SEQ_EVENT_NOTEOFF: b[0] = 0x80 | (ev->data.note.channel & 15); b[1] = ev->data.note.note; b[2] = 0; eng_midi_in(b, 3); break;
    case SND_SEQ_EVENT_CONTROLLER: b[0] = 0xB0 | (ev->data.control.channel & 15); b[1] = (uint8_t) ev->data.control.param; b[2] = (uint8_t) ev->data.control.value; eng_midi_in(b, 3); break;
    case SND_SEQ_EVENT_PGMCHANGE: b[0] = 0xC0 | (ev->data.control.channel & 15); b[1] = (uint8_t) ev->data.control.value; eng_midi_in(b, 2); break;
    case SND_SEQ_EVENT_CLOCK: b[0] = 0xF8; eng_midi_in(b, 1); break;
    case SND_SEQ_EVENT_START: b[0] = 0xFA; eng_midi_in(b, 1); break;
    case SND_SEQ_EVENT_CONTINUE: b[0] = 0xFB; eng_midi_in(b, 1); break;
    case SND_SEQ_EVENT_STOP: b[0] = 0xFC; eng_midi_in(b, 1); break;
    case SND_SEQ_EVENT_PORT_START: case SND_SEQ_EVENT_PORT_EXIT: case SND_SEQ_EVENT_CLIENT_EXIT: connect_all(); break;
    default: break;
    }
}

static void *loop(void *arg)
{
    (void) arg;
    const struct timespec ms = { 0, 1000000 };
    while (running) {
        snd_seq_event_t *ev;
        while (snd_seq_event_input(seq, &ev) >= 0 && ev) in(ev);
        uint8_t b[3];
        uint64_t frame;
        int n;
        while ((n = eng_midi_out(b, 3, &frame)) > 0) out(b, n);
        nanosleep(&ms, NULL);
    }
    return NULL;
}

void midi_open(void)
{
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK) < 0) { seq = NULL; return; }
    snd_seq_set_client_name(seq, "rhythm");
    my_client = snd_seq_client_id(seq);
    my_in = snd_seq_create_simple_port(seq, "rhythm in", SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                       SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    my_out = snd_seq_create_simple_port(seq, "rhythm out", SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
                                        SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC);
    snd_seq_connect_from(seq, my_in, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE);
    connect_all();
    running = 1;
    pthread_create(&thread, NULL, loop, NULL);
}

void midi_close(void)
{
    if (!seq) return;
    running = 0;
    pthread_join(thread, NULL);
    snd_seq_close(seq);
    seq = NULL;
}
