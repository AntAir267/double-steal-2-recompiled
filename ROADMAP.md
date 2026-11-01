# Roadmap

From "it boots and plays a mission" to a release a player would want. The
toolkit (antbox) has most of what a finished title needs: an enhancements
layer (`enhance.toml`), the in-game settings overlay, HDTV and widescreen
reporting, render scale, FXAA/SMAA, anisotropic filtering, keyboard, rumble,
per-title patch hooks. Work goes in this order; NOTES.md keeps the record of
each fix and why.

## 1. Play it the way a player would

- [x] Build with the enhancements layer; `enhance.toml` beside the binary
      with this title's defaults (config/enhance.toml, copied when missing).
- [x] Window run (Vulkan present), not only headless frame dumps: zero-copy
      swapchain, vsync, 1280x960 window.
- [x] Any controller drives player 1, in any order and plugged in late, merged
      (the toolkit's; this game has no local two-player mode). tools/pad_test.sh
      in a window: late pads, unplugging, a pad resting off-centre. Rumble on all
      and a hands-on check with several real controllers are untested.
- [ ] Controller: the host pad (SDL) through the emulated USB pad; keyboard.
      Both pads on the Win Max are detected; scripted input drives the
      game's own XPP driver; a hands-on check of the SDL mapping is left.
- [x] Sound: captured through title, menus and a mission (audio_wav): no
      dropouts during play, no clipping, the limiter on 0.01-0.07% of
      samples, crest 12 dB. Listening by ear still to do.
- [x] Saves: game data written to the HDD, found again after a restart.
- [x] Save and replay dates were UTC: XAPI reads the time zone from the
      dashboard's user section, which the kernel answered with zeros. It
      now carries the host's time zone (Win32's sign).
- [x] Clean exit from the window; no crash report on quit.

## 2. Correctness sweep

- [x] Every menu screen: options (Controller Setting crashed: two case
      blocks of its handler were never lifted; seeded), game stats, the
      online score board (hands off to stats.xbe; now restarts the game, as
      that program would have handed back). The replay theater needs a
      saved replay, still to see.
- [x] Free Drive and Change Time: dusk and night backdrops, Free Drive at
      night in the station garage. Missions keep their own time of day.
      Outdoor night driving left for a hands-on check (the garage is hard
      to drive out of blind).
- [ ] Several missions start to finish, their cutscenes (st*.pk with
      XACT audio), results screens. Mission 1: opening cutscene, driving,
      the pause menu (its cached texture was zeros from the first bring-up
      run), retire, time-out, RESULT, the replay menu, Save Replay and the
      Replay Theater all work. Clearing a mission needs a person: the
      chase cannot be scripted blind.
- [x] The LOGO1.XMV intro: the PVIDEO overlay is now composited by the
      window presenter, as the display did.
- [x] Draws the Vulkan backend skips: all degenerate (too few vertices
      for a primitive). Unhandled methods: the CPU rasteriser's survey,
      nothing missing for Vulkan. Fixed on the way: a shader that failed to
      compile from an old key list, and a USB request completing into a
      dead stack frame (the kernel's event shadow).
- [x] Frames against press screenshots of the release (everyeye.it
      gallery): day streets, the garage, replays with their filters,
      crashes, and the attract demo's dusk alleys look as the press shots
      do. Night streets with lamps are not reachable by script (hands-on).

## 3. Rendering enhancements, using the game's own features first

The project's rule: small enhancements are fine at any time and may be on by
default, as long as the original look stays reachable in the options and
the defaults hold the full frame rate on a GPD Win Max 2 (Radeon 890M). The
defaults fit: 720p, render scale 2, 16x anisotropic and the picture crop can each be
set back to the console's in PC SETTING. The serious, heavily involved
enhancements (the "Later" item below) wait until the game is fully
functional at its defaults (several missions start to finish, no known
crashes, the check passing; section 2 is not there yet), then come as
options of their own. Gameplay and timing never change with a graphics
option. The renderer is the toolkit's, so what those need (depth, normals,
motion, passes that can be toggled) belongs there.

- [x] Native 720p: the game's own HD mode (1280x720 front buffers, the 3D
      at 768x672). Fixed its composite (clipped past the far plane), the
      front buffers' size, and the safe-area border (present.crop = clip).
      The default now.
- [x] Widescreen 16:9 (`display.widescreen`): the game draws it in 720p.
- [x] Render scale 2 in 720p: post chain, exposure and focus readbacks
      right, 30 fps on the 890M. The default now. SD at scale 2 is fine too.
- [x] Distant trees were bare trunks: the runtime's cut-out mip rebuild
      scaled their alpha under the combiner's real cut; LOD clamps were
      read as whole levels (4.8 fixed point).
- [ ] Draw distance: whether the game's own LOD and visibility distances
      can be pushed out (as Burnout 3's scenery detail patch does).
- [x] Anisotropic filtering: 16x by default (graphics.aniso), the road
      sharp into the distance, no artifacts, no GPU waits. FXAA/SMAA in
      graphics.aa, off by default: the 3D is already supersampled, and
      SMAA softens the menu font a little.
- [ ] No AI-upscaled textures.
- [ ] Later, once fully functional (ideas only, nothing started): volumetric fog,
      smoke and light shafts; ray-traced shadows, reflections and global
      illumination (Vulkan ray tracing where the GPU has it, with a
      fallback); ambient occlusion (SSAO/GTAO); better shadows than the
      game's shadow map. Each its own option, on or off by default as
      suits; what is on must keep 30 fps on the Win Max's 890M, and the
      heavier ones may need the RTX 4070 PC.

## 4. Quality of life

- [x] Settings in the game: OPTIONS > PC SETTING (a row added to the
      game's own screen, its label and hint cut from the game's lettering)
      opens the runtime's settings overlay, as do Back + Start and F1. It
      has this title's own rows (video mode, picture area, language, the
      logo movie), not the upscalers, and keeps changes in settings.toml
      beside the saves with enhance.toml as the defaults. Without Burnout
      3's art it draws its own look; no updater row (not set up).
- [x] The settings in the game's own look, not Burnout's (2026-10-27): the
      PC SETTING page has the OPTIONS screen's frame, row
      plates, arrows and A / B prompts, a PC SETTING heading cut from the
      game's headings, its colours and selection flash, and its text font
      (arial32.bmf, an XFONT bitmap font), all from the player's files;
      antbox's skin API (xbox_OverlaySetSkin). NOTES.md, "The overlay in
      the game's look". Its menu sounds too (System.xsb's cursor,
      confirm and cancel, decoded from the InMemory wave bank).
- [x] The gamma ramp: the game's 720p ramp is a mild S-curve (64 -> 59,
      192 -> 199), now applied by the presenter (the Wreckless port's
      display pass; hooks on D3D's SetGammaRamp and its DAC writer).
      NOTES.md, "Gamma ramp".
- [x] The disassembler's clean-start rule for immediates (the Wreckless
      port's 16eae77): three data "functions" and a stub gone, one real
      callback after longjmp seeded, no generated body changed. NOTES.md,
      "Clean starts for immediates". The other fixes of theirs do not
      apply (no maskmovq, no 0x1E80/0x1E90 methods, the 4134-only ones).
- [x] Rumble: the pad was given the contiguous window's bytes for the
      game's heap reports (a stop played as 90 % on the left motor until
      the game quit); the OHCI resolves through the kernel's translation
      record now. The engine's steady hum while accelerating is a setting,
      off by default (game.engine_vibration).
- [x] Language: system.language = "japanese" installs and shows the
      Japanese data: menus, briefings, captions, the date format. Switching
      works both ways (the game formats its cache and installs the other
      pack; that took three runtime fixes and a hook against a lost-signal
      race in the installer, which also hung one first install in two).
- [x] Skip the intro logo: game.skip_intro (RECOMP_SKIP_INTRO). The logo
      is not skippable on the console (the movie plays on its own thread
      and nothing reads the pad); a hook deletes the player unplayed.
      Hiding LOGO1.xmv instead shows the disc error screen.
- [x] Button prompts and keyboard: the prompts are the Xbox's (they match
      an Xbox-style pad); the runtime's keyboard map drives pad 1, with the
      arrows as the d-pad only (the game's d-pad works the camera while
      driving); the README has the game's layout and the keys.
- [x] 60 fps: not safe. The logic steps once per frame: with the flip
      pacing off the game flipped ~200 times a second and the mission's
      time limit ran several times faster than real time. A 60 fps mode
      would mean halving its timestep and every frame-counted timer, a
      project of its own.

## 5. Project

- [x] The game's runtime changes as reviewable commits, all in antbox now.
- [x] Bring-up wrappers: recomp_debug.c is now recomp_hooks.c, the title's
      hooks (skip intro; the disc error screen's caller log, kept for bug
      reports); the XPR loader's debug wrapper is gone.
- [x] The runtime's own tests with our changes: 71 pass, the same 2 fail
      as on the takedawn branch (bug_report, usb_ohci_posix), 25 do not
      build on Linux. Two regressions found and fixed on the way.
- [x] README for players (what you need, controls, settings, saves, what
      differs from the console) and for building.
- [x] A packaged build: package.sh makes dist/doublesteal2 (the binary,
      enhance.toml, README, licenses, VERSION with both trees' commits).
- [x] A build for the Steam Deck too: deck-build.sh compiles in an Ubuntu
      24.04 container (glibc 2.38 needed; SteamOS has 2.41).
- [x] A download with no part of the game in it: an AppImage whose launcher
      takes the player's copy, checks it and builds the game on their computer
      (tools/release/linux.sh, NOTES.md "The download").
- [ ] A Windows download: the game has not been built for Windows yet.

## 6. Remove the overhead

The goal: the port should use a small fraction of a GPD Win Max 2's CPU and GPU, keep frame times
steady and load quickly, by cutting the layers between the game and the
hardware. The game's own timing stays exact (it steps once per frame at
30 fps; see "60 fps" in NOTES.md), and the check must still pass. Simple,
safe wins any time; big restructures once the game is fully functional.
Most of the layers (push-buffer interpretation, the kernel, D3D's MMIO,
DirectSound) are the toolkit's, so those wins belong there; this game's own
are its hooks and anything title-specific.

- [x] Balanced pacing by default instead of the runtime's stock spin
      (the toolkit's own choice on Linux; what lets the lowered waits
      sleep). Its own saving was the load of that hour: see "Overhead".
- [ ] Baseline, recorded in NOTES.md (CPU per thread and the executor's
      frame done headless, 2026-10-28): frame time (average and worst),
      CPU use per thread, GPU use, and load times (boot to title, menu to
      mission) on the Win Max, windowed at the defaults, on the mission 1
      route. Taken with nothing else running (the shared rules: no timing
      runs beside another game's).
- [x] The KickOff spin: 90% of the main thread (DS2_PROFILE, src/game/prof.c),
      lowered to a sleeping wait (config/spin_waits.json): the main thread
      98% -> 10%, the process ~148% -> 64% on a quiet machine, no late
      flips, up to 1 ms more worst-case jitter (NOTES.md "Overhead", run ab1).
- [ ] Profile that run (perf on the recompiled code and the runtime's
      threads) and list the costs by layer: the push-buffer walker and
      its waits, the kick ack, kernel calls, audio mixing, the frame dump
      and present path. Each finding goes to its owner (antbox or DS2).
- [ ] DS2's own: the hooks that run every frame (the rumble filter on
      sub_0002F520, the gamma hooks) and the CPU readback targets
      (RECOMP_GPU_CPU_READS, two small targets written to guest memory
      every flip): whether each still costs anything measurable.
- [ ] Re-measure after each change and record before and after.
- [ ] A native Vulkan renderer: built once in antbox for all its games,
      switchable (the Renderer row, Graphics page), the current path the
      default until it has been played through. No renderer of this game's
      own. First try 2026-10-30, headless on
      e983301 (RECOMP_PB_BACKEND=native): NOTES.md "Overhead".
