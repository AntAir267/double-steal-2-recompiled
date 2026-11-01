/*
 * prof.h - a sampling profile of the guest's main thread (prof.c).
 */
#ifndef DS2_PROF_H
#define DS2_PROF_H

/* On the guest's main thread, before the title starts: with DS2_PROFILE set,
 * samples its host PC every millisecond of its CPU time into that file. */
void ds2_prof_start(void);

#endif
