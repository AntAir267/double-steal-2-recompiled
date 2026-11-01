/*
 * recomp_env_game.h - Double-S.T.E.A.L 2's defaults for runtime keys.
 *
 * Read by the runtime's recomp_env.h (RECOMP_ENV_HAVE_GAME_KEYS, set on the
 * recomp_env target in CMakeLists.txt). A default here applies when the
 * environment sets neither the key's new nor its old spelling. The config
 * keys that only the Vulkan build wants stay in main.c's runtime_defaults.
 */
#ifndef DS2_RECOMP_ENV_GAME_H
#define DS2_RECOMP_ENV_GAME_H

/* The title's own settings, also in enhance.toml's [game] (main.c binds
 * them): SKIP_INTRO 1 starts at the title screen, without the logo movie;
 * ENGINE_VIBRATION 1 keeps the game's engine hum on the controller. */
#define RECOMP_ENV_GAME_KEYS(X) \
    X(SKIP_INTRO,        CONFIG, "RECOMP_SKIP_INTRO",        NULL, "1: no Bunkasha logo movie at start (game.skip_intro)") \
    X(ENGINE_VIBRATION,  CONFIG, "RECOMP_ENGINE_VIBRATION",  NULL, "1: the controller hums with the engine's load as on the console; 0 (this build's default) only the knocks (game.engine_vibration)")

/* APU_DSP_ACK: DirectSound waits on the DSP's command doorbell; the runtime
 *   acknowledges it where it finds it (GPSADDR).
 * USB: the title's own XPP driver runs the pads on the emulated OHCI
 *   controller, so a pad only exists with it. */
#define RECOMP_ENV_GAME_DEFAULTS(D) \
    D(APU_DSP_ACK, "auto") \
    D(USB, "1")

#endif
