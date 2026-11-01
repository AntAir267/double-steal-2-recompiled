/**
 * Native replacements for library routines the lifter gets wrong or that are
 * worth running as host code. Each address is listed in
 * config/manual_functions.json, so its generated body is not emitted, and
 * recomp_lookup_manual() (recomp_manual.c) hands out the function here, which
 * is how direct calls reach it too.
 *
 * Calling convention as lifted: on entry g_esp points at the return address,
 * arguments follow it, and the function pops what its `ret` popped.
 */

#include <string.h>

#include "recomp_types.h"

/* memmove / memcpy (the XDK CRT's one routine for both), 0x0011C1B0.
 * cdecl (dst, src, n), returns dst.
 *
 * Lifted by the takedawn branch it is cut short at its inline jump table: the
 * arms after the table (0x0011C454...) belong to no function, so a copy whose
 * tail goes through them called "[ICALL] Failed to resolve VA 0x0011C454"
 * and left its last words uncopied. Upstream v0.13.1 lifts those arms
 * (#173); a host memmove is right on both and faster. */
void sub_0011C1B0(void)
{
    uint32_t dst = MEM32(g_esp + 4);
    uint32_t src = MEM32(g_esp + 8);
    uint32_t n   = MEM32(g_esp + 12);

    if (n)
        memmove((void *)XBOX_PTR(dst), (const void *)XBOX_PTR(src), n);
    g_eax = dst;
    g_esp += 4;                         /* ret */
}
