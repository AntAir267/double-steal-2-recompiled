/*
 * prof.c - a sampling profile of the guest's main thread, for the overhead
 * work (usable where perf is not allowed, as under perf_event_paranoid 4).
 * DS2_PROFILE=<file> starts it: a timer on the thread's own CPU time
 * sends it SIGPROF every millisecond it runs, the handler keeps the
 * interrupted host PC, and a thread writes them to the file once a second
 * (8-byte PC, 4-byte milliseconds since the start). tools/perf/prof.py maps
 * them to the binary's functions (sub_XXXXXXXX is the guest's code).
 */
#include "prof.h"

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>

#if defined(__linux__) && defined(__x86_64__)

#define PROF_MAX (1u << 22)              /* 70 minutes at 1 kHz */

static struct { uint64_t pc; uint32_t ms; } *s_buf;
static volatile uint32_t s_n;
static struct timespec s_t0;
static FILE *s_out;

static void on_prof(int sig, siginfo_t *si, void *ucv)
{
    const ucontext_t *uc = ucv;
    struct timespec t;
    uint32_t i;

    (void)sig;
    (void)si;
    i = __atomic_fetch_add(&s_n, 1, __ATOMIC_RELAXED);
    if (i >= PROF_MAX)
        return;
    clock_gettime(CLOCK_MONOTONIC, &t);
    s_buf[i].pc = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
    s_buf[i].ms = (uint32_t)((t.tv_sec - s_t0.tv_sec) * 1000 + (t.tv_nsec - s_t0.tv_nsec) / 1000000);
}

static void *writer(void *arg)
{
    uint32_t done = 0;

    (void)arg;
    for (;;) {
        uint32_t n = __atomic_load_n(&s_n, __ATOMIC_ACQUIRE), i;
        if (n > PROF_MAX)
            n = PROF_MAX;
        for (i = done; i < n; i++) {
            fwrite(&s_buf[i].pc, 8, 1, s_out);
            fwrite(&s_buf[i].ms, 4, 1, s_out);
        }
        done = n;
        fflush(s_out);
        sleep(1);
    }
    return NULL;
}

void ds2_prof_start(void)
{
    const char *path = getenv("DS2_PROFILE");
    struct sigaction sa;
    struct sigevent sev;
    struct itimerspec its;
    timer_t timer;
    pthread_t th;
    uint64_t anchor = (uint64_t)(uintptr_t)&ds2_prof_start;

    if (!path || !*path || !(s_buf = calloc(PROF_MAX, sizeof *s_buf)) || !(s_out = fopen(path, "wb")))
        return;
    /* The first record: this function's address, for the binary's load
     * offset, and 0xFFFFFFFF. */
    fwrite(&anchor, 8, 1, s_out);
    fwrite(&(uint32_t){ 0xFFFFFFFFu }, 4, 1, s_out);
    clock_gettime(CLOCK_MONOTONIC, &s_t0);
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_prof;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);
    memset(&sev, 0, sizeof sev);
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    if (timer_create(CLOCK_THREAD_CPUTIME_ID, &sev, &timer) != 0) {
        fprintf(stderr, "[DS2] profile: no timer\n");
        return;
    }
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 1000000;
    its.it_interval = its.it_value;
    timer_settime(timer, 0, &its, NULL);
    pthread_create(&th, NULL, writer, NULL);
    pthread_detach(th);
    fprintf(stderr, "[DS2] profile: the guest's main thread, 1 kHz of its CPU time, to %s\n", path);
}

#else

void ds2_prof_start(void) {}

#endif
