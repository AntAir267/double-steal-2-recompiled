/**
 * Hooks on the title's own functions: the generated body runs as sub_X_gen,
 * and calls, tail jumps and the dispatch entry reach the sub_X defined here
 * first. Found by the lifter's --exclude-manual scan of this file (regen.sh),
 * so a change to the set of wrapped functions needs a regeneration.
 */

#include <stdio.h>
#include <time.h>

#include "recomp_types.h"
#include "../options_row.h"
#include "../settings.h"

/* Return addresses on the guest stack, innermost first (FPO frames: a scan,
 * so it over-reports). */
static void guest_callers(const char *what)
{
    uint32_t i, shown = 0;

    fprintf(stderr, "[DS2] %s; guest callers:", what);
    for (i = 0; i < 128 && shown < 12; i++) {
        uint32_t v = MEM32(g_esp + i * 4);
        if (v >= 0x00011000u && v < 0x001BFBE0u) {
            fprintf(stderr, " %08X", v);
            shown++;
        }
    }
    fprintf(stderr, "\n");
}

/* The disc error screen (loads d:\data\Screen\DiscDamage.tga and never
 * returns). Every failed read or load path ends here, so the callers say
 * which one. */
extern void sub_000E4010_gen(void);
void sub_000E4010(void)
{
    guest_callers("disc error screen");
    sub_000E4010_gen();
}

/* The Bunkasha logo (LOGO1.xmv). The logo scene's play step (thiscall,
 * 0x00102A20) plays the movie to its end on a thread of its own (XMV,
 * sub_0008D950: no input is read meanwhile, so the console could not skip
 * it either), stops it, deletes the player at [this+0xC] and returns 0.
 * With game.skip_intro it deletes the player, which the scene's start
 * already opened the file into, without playing: the state after is the
 * same. */
extern int ds2_skip_intro;              /* main.c */
extern void sub_00102A20_gen(void);
extern void sub_0008D820(void);         /* the player's destructor */
extern void sub_0011ABB5(void);         /* operator delete */
void sub_00102A20(void)
{
    uint32_t self = g_ecx, player;

    if (!ds2_skip_intro) {
        sub_00102A20_gen();
        return;
    }
    player = MEM32(self + 0xC);
    if (player) {
        g_ecx = player;
        PUSH32(g_esp, 0x00102A4Fu); RECOMP_ABI_CALL(0x0008D820u, sub_0008D820);
        PUSH32(g_esp, player);
        PUSH32(g_esp, 0x00102A55u); RECOMP_ABI_CALL(0x0011ABB5u, sub_0011ABB5);
        g_esp += 4;
    }
    MEM32(self + 0xC) = 0;
    g_eax &= ~0xFFu;                    /* return 0 in al */
    g_esp += 4;                         /* ret */
}

/* The game's threaded file reader (vtable 0x001C0FF8): the first-run
 * install and its other streamed loads read through it. sub_00029850 makes
 * the reader's worker thread (0x00029840, this body) and only then the two
 * events the worker and the reader trade, [this+0x88] (a chunk is ready)
 * and [this+0x8C]. The worker reads a chunk, unpacks it and sets the first.
 * On the console it cannot get there first: its disc read blocks, and the
 * creator makes the events meanwhile. Here the read comes from the host's
 * file cache at once, and in about one install in two a worker set
 * [this+0x88] while it was still 0: the signal was lost, the reader waited
 * on the event for ever, and the install stopped behind its loading bar
 * (with the file it was writing left full of zeros). So the worker waits
 * for both events to exist, which is a few microseconds (2 s at most). */
extern void sub_00029610_gen(void);
void sub_00029610(void)
{
    uint32_t self = g_ecx;
    int i;

    for (i = 0; i < 20000 && (!MEM32(self + 0x88) || !MEM32(self + 0x8C)); i++) {
        struct timespec ts = { 0, 100000 };     /* 0.1 ms */
        nanosleep(&ts, NULL);
    }
    sub_00029610_gen();
}


/* A screen resource's finish (thiscall 0x000EB7A0): its XPR is in memory and
 * the layout is about to be parsed. For OPTIONS a copy with the PC SETTING
 * row is parsed instead (options_row.c). */
extern void sub_000EB7A0_gen(void);
void sub_000EB7A0(void)
{
    if (ds2_pc_setting_row())
        ds2_options_row_patch(g_ecx);
    sub_000EB7A0_gen();
}

/* The OPTIONS screen's A (thiscall 0x0010A900, the item first, ret 0xC):
 * items 6 to 8 are CONTROLLER SETTING, EXIT and RESET TO DEFAULT, any other
 * leaves the screen like EXIT. PC SETTING plays the menu's confirm, as
 * CONTROLLER SETTING does, and opens the overlay over the screen, which
 * stays as it was. */
extern void sub_0010A900_gen(void);
extern void sub_0008B960(void);         /* the menu sound player's play */
void sub_0010A900(void)
{
    uint32_t esp0 = g_esp, self = g_ecx;

    if (MEM16(g_esp + 4) != DS2_OPTIONS_PC_ITEM) {
        sub_0010A900_gen();
        return;
    }
    g_ecx = 0x0042D708u;
    PUSH32(g_esp, 0x2001);              /* confirm */
    PUSH32(g_esp, 0x0010A954u); RECOMP_ABI_CALL(0x0008B960u, sub_0008B960);
    g_ecx = self;
    ds2_pc_setting_open();
    g_esp = esp0 + 16;                  /* ret 0xC */
}

/* The rumble manager's set (thiscall 0x0002F520, ret 0x10: pad, left and
 * right as 0..1, a 3-frame pulse or held). The car's per-frame vibration
 * (sub_00088F70) holds both motors at the engine's load while it drives,
 * up to 0.35 each with the throttle open (the call returning to
 * 0x00089317); collisions, rough road and the rev limiter are pulses from
 * the same function. Without game.engine_vibration that held call sets 0,
 * so the motors rest between the knocks; a pulse under way still ends on
 * what was held, which is then 0. */
extern int ds2_engine_vibration;        /* main.c */
extern void sub_0002F520_gen(void);
void sub_0002F520(void)
{
    if (!ds2_engine_vibration && MEM32(g_esp) == 0x00089317u) {
        MEM32(g_esp + 8) = 0;           /* left 0.0f */
        MEM32(g_esp + 12) = 0;          /* right 0.0f */
    }
    sub_0002F520_gen();
}

/* The gamma ramp. D3D's SetGammaRamp (stdcall 0x0012B4D0: flags, ramp;
 * ret 8) keeps a new ramp and writes it to the DAC at once with flag 2, or
 * marks it for the vblank (0x00134832); its writer (thiscall 0x00132045, the
 * ramp on the stack, ret 4) puts 256 red, green and blue bytes into PRMDIO
 * 0x6813C9, the ramp red[256], green[256], blue[256]. The game sets one at
 * start (sub_000316F0 via sub_000317A0 from 0x00012D10), through a mild
 * S-curve (0x00012100) when its mode number [0x00415784] is 4 or 5, which
 * sub_0001B0D0 sets from bit 2 of [0x001D2A9C], the bit sub_00030E00 sets
 * for its 1280x720 mode [verified: disassembly]. On the console the display
 * applies it at scanout; here the port is plain RAM, so both hand it to the
 * presenter (xbox_gamma.h, which keeps one copy of a repeated ramp): the
 * request, so a ramp the vblank would write is not lost, and the writer. */
#include "xbox_gamma.h"
static void gamma_from(uint32_t ramp)
{
    uint8_t rgb[768];
    int i;

    for (i = 0; i < 768; i++)
        rgb[i] = MEM8(ramp + (uint32_t)i);
    xbox_GammaSetRamp(rgb);
}

extern void sub_0012B4D0_gen(void);
void sub_0012B4D0(void)
{
    gamma_from(MEM32(g_esp + 8));
    sub_0012B4D0_gen();
}

extern void sub_00132045_gen(void);
void sub_00132045(void)
{
    gamma_from(MEM32(g_esp + 4));
    sub_00132045_gen();
}
