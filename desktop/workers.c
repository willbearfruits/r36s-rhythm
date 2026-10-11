// A small pool of threads for the engine's track jobs (eng_parallel). The caller works too; workers spin briefly
// for the next pass (it comes every few milliseconds) and sleep if it does not. They ask for real-time priority and
// carry on without it if the system says no.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>
#include "../core/eng.h"

#define MAXW 7
static pthread_t th[MAXW];
static int nworkers;
static void (*job_fn)(int);
static int job_count;
static _Atomic int job_next, job_done, gen;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

static inline void relax(void)
{
#if defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield");
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#endif
}

static void work(void)
{
    int i;
    while ((i = atomic_fetch_add_explicit(&job_next, 1, memory_order_acq_rel)) < job_count) {
        job_fn(i);
        atomic_fetch_add_explicit(&job_done, 1, memory_order_acq_rel);
    }
}

static void *worker(void *arg)
{
    (void) arg;
    int seen = atomic_load(&gen);
    for (;;) {
        int g = atomic_load_explicit(&gen, memory_order_acquire);
        for (int spin = 0; spin < 4000 && g == seen; spin++) { relax(); g = atomic_load_explicit(&gen, memory_order_acquire); }
        if (g == seen) {
            pthread_mutex_lock(&mu);
            while ((g = atomic_load(&gen)) == seen) pthread_cond_wait(&cv, &mu);
            pthread_mutex_unlock(&mu);
        }
        seen = g;
        work();
    }
    return NULL;
}

static void run(void (*fn)(int), int count)
{
    job_fn = fn;
    job_count = count;
    atomic_store_explicit(&job_done, 0, memory_order_relaxed);
    atomic_store_explicit(&job_next, 0, memory_order_release);
    pthread_mutex_lock(&mu);
    atomic_fetch_add_explicit(&gen, 1, memory_order_acq_rel);
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mu);
    work();
    while (atomic_load_explicit(&job_done, memory_order_acquire) < count) relax();
}

void realtime_thread(int prio)
{
    struct sched_param sp = { .sched_priority = prio };
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);   // allowed or not: either way it goes on
}

static void *start(void *arg) { realtime_thread(70); return worker(arg); }

// n workers besides the audio thread (0: everything on the audio thread). Returns how many started.
int workers_start(int n)
{
    if (n > MAXW) n = MAXW;
    for (int i = 0; i < n; i++) if (pthread_create(&th[nworkers], NULL, start, NULL) == 0) nworkers++;
    if (nworkers) eng_parallel = run;
    return nworkers;
}
