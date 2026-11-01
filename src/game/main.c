/**
 * Double-S.T.E.A.L: The Second Clash - recompiled game entry point
 *
 * The native program that hosts the recompiled game code. Boot order is the
 * toolkit template's (templates/new-game/src/main.c) and the one
 * sp00nznet/wreckless uses for the first game, on a POSIX host:
 *
 *   1. load default.xbe
 *   2. map the Xbox address space (data sections at their original VAs)
 *   3. the emulated APU, USB host controllers, kernel, paths, kernel bridge
 *   4. guest stack, flat dispatch table, hang watchdog
 *   5. call the recompiled entry point
 *
 * XBE (xbe_parser):
 *   Title:       DOUBLE S.T.E.A.L SECOND CLASH (Japan, English + Japanese)
 *   Title ID:    0x42550004
 *   Built:       2004-12-03, XDK 1.0.5849
 *   Debug path:  e:\Projects\DSTEAL2\xbox\DSTEAL2\Retail\default.exe
 *   Base addr:   0x00010000
 *   Entry point: 0x00113F0A
 *   Code:        .text 1.10 MB, plus D3D, D3DX, XGRPH, DSOUND, XACTENG, XMV,
 *                WMADEC, XPP and DOLBY library sections
 *   Kernel imports: 118
 *
 * Usage: doublesteal2 [--game-dir DIR] [--save-dir DIR] [--headless]
 *   --game-dir  folder holding default.xbe and the disc files
 *               (default: "game/Double-S.T.E.A.L - The Second Clash")
 *   --save-dir  the Xbox hard disk's save folders
 *               (default: $XDG_DATA_HOME/doublesteal2)
 */

#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>
#include <dlfcn.h>
#include <execinfo.h>

#include <xbox/xboxrecomp.h>
#include "video/video_player.h"     /* xbox_HostWindowMain */
#include "usb/ohci.h"               /* xbox_OhciInit */
#include "platform/recomp_env.h"    /* recomp_env */
/* The takedawn branch's crash and hang reports: <save dir>/crash, with every
 * registered thread sampled when no frame has flipped for a while
 * (RECOMP_HANG_WATCHDOG, default 15 s). */
#if defined(__has_include)
#if __has_include("xbox_crash.h")
#include "xbox_crash.h"
#define DS2_HAVE_CRASH_REPORTS 1
#endif
#endif
/* The opt-in enhancements layer (CMakeLists.txt): enhance.toml beside the
 * executable. */
#if defined(DS2_HAVE_ENHANCE)
#include "enhance.h"                /* xbox_enhance_init */
#include "enhance_cfg.h"            /* enhance_cfg_report_unused */
#include "recomp_cfg.h"             /* enhance.toml read early, for one key */
#include "platform/recomp_exe_dir.h"
#endif
#include "platform/recomp_restart.h"   /* recomp_restart_init */
#include "settings.h"                  /* the settings overlay's side of this title */
#include "skin.h"                      /* the overlay in the game's own look */
#include "prof.h"                      /* DS2_PROFILE: where the main thread spends its time */

/* RECOMP_TLS is required: the runtime defines the guest registers
 * thread-local, and a plain extern resolves to the TLS template rather than
 * this thread's copy (wreckless found this the hard way). */
extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;

#define DS2_ENTRY_POINT     0x00113F0Au
#define DS2_GAME_DIR        "game/Double-S.T.E.A.L - The Second Clash"
/* D3D's global device pointer (XDK 5849 D3D8, in the D3D section's BSS). */
#define DS2_D3D_DEVICE_PTR  0x0013C2A8u

extern void xbe_entry_point(void);
int recomp_dispatch_init(void);

typedef struct MCPXAPUState MCPXAPUState;
extern MCPXAPUState *mcpx_apu_init_standalone(uint8_t *ram_ptr);
extern MCPXAPUState *g_apu_state;

static char s_game_dir[PATH_MAX];
static char s_save_dir[PATH_MAX];
static char s_xbe_path[PATH_MAX + 16];

/* ── crash report ──────────────────────────────────────────── */

/* The guest's call chain: every lifted call pushes its guest return address
 * onto the guest stack, so scanning up from esp for values inside the code
 * sections recovers it (over-reports; these are FPO frames). */
static void print_guest_stack(void)
{
    const uint32_t *sp;
    int i, shown = 0;

    if (!g_xbox_mem_offset || !g_esp)
        return;
    sp = (const uint32_t *)((uintptr_t)g_xbox_mem_offset + g_esp);
    fprintf(stderr, "  guest stack (return addresses, innermost first):\n");
    for (i = 0; i < 256 && shown < 24; i++) {
        uint32_t v = sp[i];
        if (v > g_xbox_code_lo && v < g_xbox_code_hi) {
            fprintf(stderr, "    [esp+%-4d] 0x%08X\n", i * 4, v);
            shown++;
        }
    }
    if (!shown)
        fprintf(stderr, "    (no return addresses in range)\n");
    fprintf(stderr, "  recent ICALL targets:");
    for (i = 0; i < 16; i++)
        fprintf(stderr, " %08X", g_icall_trace[(g_icall_trace_idx + i) & 15]);
    fprintf(stderr, "\n");
}

static void on_fault(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = ucv;
    uintptr_t fault = (uintptr_t)si->si_addr;
    void *rip = NULL;
    Dl_info di;

    /* Trapped hardware registers (APU, AC'97, USB OHCI): serviced, and the
     * program counter stepped past the access. */
    if ((sig == SIGSEGV || sig == SIGBUS) && xbox_PosixMmioFault(ucv, fault))
        return;

#if defined(__x86_64__)
    rip = (void *)(uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
#endif
    fprintf(stderr, "\n[CRASH] signal %d at host pc %p, fault addr %p\n",
            sig, rip, si->si_addr);
    if (rip && dladdr(rip, &di) && di.dli_sname)
        fprintf(stderr, "  in %s+0x%lx\n", di.dli_sname,
                (unsigned long)((uintptr_t)rip - (uintptr_t)di.dli_saddr));
    fprintf(stderr, "  guest regs: eax=%08X ecx=%08X edx=%08X esp=%08X\n",
            g_eax, g_ecx, g_edx, g_esp);
    fprintf(stderr, "              ebx=%08X esi=%08X edi=%08X\n",
            g_ebx, g_esi, g_edi);
    if (g_xbox_mem_offset && fault >= (uintptr_t)g_xbox_mem_offset)
        fprintf(stderr, "  guest VA of fault: 0x%08llX\n",
                (unsigned long long)(fault - (uintptr_t)g_xbox_mem_offset));
    print_guest_stack();
    {
        void *bt[32];
        int n = backtrace(bt, 32);
        fprintf(stderr, "  host backtrace:\n");
        backtrace_symbols_fd(bt, n, 2);
    }
    fflush(stderr);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_fault_handler(void)
{
    /* Its own stack: a guest stack overflow or a runaway copy can leave the
     * host stack unusable at exactly the moment the report is needed. */
    static uint8_t alt[256 * 1024];
    stack_t ss = { .ss_sp = alt, .ss_size = sizeof alt };
    struct sigaction sa;

    sigaltstack(&ss, NULL);
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
}

/* ── boot ──────────────────────────────────────────────────── */

static bool load_xbe(const char *path, void **out_data, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    long size;
    void *data;

    if (!f) {
        fprintf(stderr, "Cannot open XBE: %s (%s)\n", path, strerror(errno));
        return false;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || !(data = malloc((size_t)size))) {
        fclose(f);
        return false;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return false;
    }
    fclose(f);
    *out_data = data;
    *out_size = (size_t)size;
    return true;
}

static int boot_and_run(void)
{
    void *xbe_data = NULL;
    size_t xbe_size = 0;

    /* Here, not in main: the alternate signal stack belongs to the thread
     * that runs the guest (xbox_HostWindowMain may start a new one). */
    install_fault_handler();
#if defined(DS2_HAVE_CRASH_REPORTS)
    xbox_crash_init(s_save_dir);
    xbox_crash_thread_register("host-main");
#endif

    printf("=== Double-S.T.E.A.L: The Second Clash - Static Recompilation ===\n");
    printf("game dir: %s\nsave dir: %s\n", s_game_dir, s_save_dir);

    if (!load_xbe(s_xbe_path, &xbe_data, &xbe_size))
        return 1;
    printf("XBE loaded: %zu bytes\n", xbe_size);

    if (!xbox_MemoryLayoutInit(xbe_data, xbe_size)) {
        fprintf(stderr, "Xbox memory layout init failed "
                        "(required VA range unavailable?)\n");
        free(xbe_data);
        return 1;
    }
    g_xbox_mem_offset = xbox_GetMemoryOffset();
    printf("Xbox memory mapped at host offset 0x%llX\n",
           (unsigned long long)g_xbox_mem_offset);

    /* The emulated APU walks Xbox physical RAM for voice buffers. Gated on
     * the variable that unmaps its registers: the two halves are useless
     * apart. Wreckless skipped its whole engine init when DirectSoundCreate
     * failed (docs/pipeline/06-debugging.md, "Runs, Never Draws a Frame"),
     * so this one matters for more than sound. */
    if (recomp_env(RENV_AC97_READY) && strcmp(recomp_env(RENV_AC97_READY), "0")) {
        g_apu_state = mcpx_apu_init_standalone((uint8_t *)xbox_GetMemoryBase());
        fprintf(stderr, "[BOOT] emulated APU %s\n",
                g_apu_state ? "up" : "FAILED to initialise");
    }

    /* XAPI is statically linked and drives the OHCI registers itself, so a
     * pad only exists if the host controllers do. */
    xbox_OhciInit();

    xbox_kernel_init();
    mkdir(s_save_dir, 0755);
    xbox_path_init(s_game_dir, s_save_dir);
    xbox_kernel_bridge_init();

    /* The GPU fence D3D waits on. sub_0012FC50 reserves pushbuffer space and
     * spins until the GPU has consumed enough of it:
     *
     *     mov esi, [0x13C2A8]   ; the D3D device
     *     mov eax, [esi+0x30]   ; -> what the GPU has consumed
     *     mov ecx, [eax]
     *     mov eax, [esi+0x2C]   ;    what the title has submitted
     *
     * The same layout as Burnout 3 and Half-Life 2 (XDK 5849). Without it the
     * first boot stalled in D3D device creation. Re-read and bounds-checked
     * on every poll, so registering before the device exists is harmless. */
    xbox_Nv2aMirrorFence(DS2_D3D_DEVICE_PTR, 0x2Cu, 0x30u);

    /* The vblank count the XMV player paces on: sub_0012C3C0 (D3D's
     * GetDisplayFieldStatus, called from the XMV section at 0x00177D43)
     * hands back [device+0x1DE8], which only D3D's vblank handler moves
     * (0x001301FB). Without a vblank the LOGO1.XMV intro would hold on its
     * first frame. The runtime stops bumping it once the title moves it. */
    xbox_Nv2aFrameCounter(DS2_D3D_DEVICE_PTR, 0x1DE8u);

#if defined(XBOXRECOMP_HAVE_VULKAN)
    /* D3D software methods (NO_OPERATION with a parameter): D3D's PGRAPH
     * service, sub_00134E00, hands the parameter of a trapped 0x100 to
     * sub_00134B90 (thiscall; the miniport object, the GPU interrupt's
     * context 0x0013DED8). Method 13 runs an inserted callback, which is
     * how the title re-points the exit jumps of its index blocks between
     * uses; the takedawn branch runs the dispatcher where the GPU would
     * stall (patches/xboxrecomp-takedawn.patch). */
    xbox_Nv2aSetSoftwareMethodHandler(0x00134B90u, 0x0013DED8u);
#endif

    g_esp = XBOX_STACK_TOP;

    if (!recomp_dispatch_init())
        fprintf(stderr, "[BOOT] flat dispatch unavailable; "
                        "indirect calls will use the binary search\n");

    /* From this thread: the guest registers it samples are thread-local.
     * Inert unless RECOMP_WATCHDOG_SECS is set. */
    xbox_WatchdogStart();
#if defined(DS2_HAVE_CRASH_REPORTS)
    xbox_crash_watchdog_start();
#endif

    printf("\nStarting game at entry point 0x%08X...\n", DS2_ENTRY_POINT);
    fflush(stdout);
    ds2_prof_start();
    xbe_entry_point();

    printf("\nGame returned. Cleaning up...\n");
    xbox_kernel_shutdown();
    xbox_MemoryLayoutShutdown();
    free(xbe_data);
    return 0;
}

/* A runtime setting unless the environment (or settings.toml) already
 * names one; also the overlay's default for it. */
static void env_default(const char *name, const char *value)
{
    setenv(name, value, 0);
    ds2_settings_default(name, value);
}

/* What this title needs from the runtime, found in bring-up (NOTES.md).
 * Set before anything in the runtime reads its settings. */
static void runtime_defaults(void)
{
    /* DirectSound checks the AC'97 codec and then drives the APU itself;
     * a failed probe skips the engine's whole init (Wreckless). */
    env_default("RECOMP_AC97_READY", "1");
    /* D3D's vblank interrupt: its handler moves the field count the XMV
     * player paces on and completes the flips. */
    env_default("RECOMP_VBLANK", "1");
    /* The NV2A pushbuffer executor: without it nothing is drawn. */
    env_default("RECOMP_PB_EXEC", "1");
#if defined(XBOXRECOMP_HAVE_VULKAN)
    /* The takedawn branch's Vulkan NV2A renderer, as Takedawn uses it on
     * Linux and the Steam Deck; flips held to the emulated vblank. */
    env_default("RECOMP_PB_BACKEND", "vulkan");
    env_default("RECOMP_FLIP_PACING", "vblank");
    /* The CPU reads two GPU targets back each frame: the 16x16 luminance
     * one the exposure comes from, and the 64x16 depth one the depth of
     * field focuses by. Unread, the picture went white. */
    env_default("RECOMP_GPU_CPU_READS", "16x16,64x16");
    /* The online score board is stats.xbe, Microsoft's Xbox Live stats UI;
     * with Live gone it could only fail and boot default.xbe again, so the
     * game starts again instead of quitting (kernel_bridge.c). */
    env_default("RECOMP_LAUNCH_RESTART", "stats.xbe");
    /* Driving reads the d-pad (up: change camera, down: rear camera), so
     * the arrow keys are only the d-pad; W A S D drive (keyboard.c). */
    env_default("RECOMP_KEYBOARD_ARROWS", "dpad");
#else
    /* Upstream: the title builds its own KEVENTs (D3D's at 0x0013E06C has
     * no KeInitializeEvent behind it); give them host events. */
    env_default("RECOMP_TITLE_KEVENTS", "1");
#endif
#if defined(DS2_HAVE_ENHANCE)
    /* Power mode Balanced: a held flip sleeps, then spins its last 8 ms
     * (the runtime's choice where it does not offer Maximum, Linux and
     * Windows). The game had the runtime's stock spin, its main thread
     * busy-waiting through every frame: with Balanced the mission drive
     * used 104% of a core instead of 130%, the main thread 49% instead of
     * 98%, and the flips were as steady (p5-p95 33.3 ms, max 33.6) [measured
     * 2026-10-28, runs perf1 and perf2, NOTES.md "Overhead"]. Unless the
     * player's enhance.toml names a pacing (older copies have no such key;
     * the environment would win over the file). */
    {
        char path[PATH_MAX], err[128];
        recomp_cfg *c;
        int named = 0;
        snprintf(path, sizeof path, "%s/enhance.toml", recomp_exe_dir());
        if ((c = recomp_cfg_load_file(path, err, sizeof err))) {
            named = recomp_cfg_type_of(c, "present.pacing") != RCFG_NONE;
            recomp_cfg_free(c);
        }
        if (!named) {
            env_default("RECOMP_PRESENT_PACING", "sleep");
            env_default("RECOMP_PRESENT_SPIN_LEAD", "8");
            /* The enhancements layer reads these through the runtime's
             * table, taken before: again, with them. */
            recomp_env_reload();
        }
    }
#endif
    /* The DSP doorbell ack and the emulated USB controller are debug-tier
     * keys: their defaults are in env/recomp_env_game.h. */
}

static void absolute(char *out, size_t n, const char *path)
{
    char cwd[PATH_MAX];

    if (path[0] == '/' || !getcwd(cwd, sizeof cwd))
        snprintf(out, n, "%s", path);
    else
        snprintf(out, n, "%s/%s", cwd, path);
}

/* game.skip_intro: the logo scene's play step deletes the player without
 * playing (recomp_hooks.c). */
int ds2_skip_intro;
/* game.engine_vibration: the car's engine hum on the controller, as the
 * console had it; off, only its knocks (recomp_hooks.c). */
int ds2_engine_vibration;

int main(int argc, char **argv)
{
    const char *game = DS2_GAME_DIR, *save = NULL;
    int i;

    /* First: the arguments and the environment as given, so a restart (the
     * online score board's hand-off, below) starts the program as it was. */
    recomp_restart_init(argc, argv);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--game-dir") && i + 1 < argc)
            game = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc)
            save = argv[++i];
        else if (!strcmp(argv[i], "--headless"))
            setenv("RECOMP_HEADLESS", "1", 1);
        else {
            fprintf(stderr, "usage: %s [--game-dir DIR] [--save-dir DIR] [--headless]\n",
                    argv[0]);
            return 2;
        }
    }
    absolute(s_game_dir, sizeof s_game_dir, game);
    if (save) {
        absolute(s_save_dir, sizeof s_save_dir, save);
    } else {
        const char *xdg = getenv("XDG_DATA_HOME"), *home = getenv("HOME");
        if (xdg && *xdg)
            snprintf(s_save_dir, sizeof s_save_dir, "%s/doublesteal2", xdg);
        else
            snprintf(s_save_dir, sizeof s_save_dir, "%s/.local/share/doublesteal2",
                     home ? home : ".");
    }
    snprintf(s_xbe_path, sizeof s_xbe_path, "%s/default.xbe", s_game_dir);

    /* The player's settings.toml (the overlay's, in the save folder) first:
     * what it sets goes into the environment, over every default below. */
    ds2_settings_init(s_save_dir);
    runtime_defaults();
#if defined(DS2_HAVE_ENHANCE)
    /* After the defaults (the environment wins over the file, the file over
     * the stock values), before anything starts the guest or a backend. */
    xbox_enhance_init(recomp_exe_dir(), NULL);
    /* The title's own keys ([game]); the environment wins over the file. */
    enhance_cfg_bind_env("game.skip_intro", RENV_SKIP_INTRO);
    enhance_cfg_bind_env("game.engine_vibration", RENV_ENGINE_VIBRATION);
    /* enhance.toml's values as the overlay's defaults. */
    ds2_settings_finish();
    ds2_skip_intro = enhance_cfg_bool("game.skip_intro", 0);
    ds2_engine_vibration = enhance_cfg_bool("game.engine_vibration", 0);
    enhance_cfg_report_unused();
#else
    ds2_skip_intro = recomp_env(RENV_SKIP_INTRO) && strcmp(recomp_env(RENV_SKIP_INTRO), "0");
    ds2_engine_vibration = recomp_env(RENV_ENGINE_VIBRATION) && strcmp(recomp_env(RENV_ENGINE_VIBRATION), "0");
#endif

    /* The overlay in the game's own look, from the player's files. */
    ds2_skin_init(s_game_dir, s_save_dir);

    /* With RECOMP_FB_WINDOW set this opens the framebuffer window on the
     * main thread and runs the boot on a second one; otherwise it just
     * calls boot_and_run. */
    return xbox_HostWindowMain(boot_and_run);
}
