# Double-S.T.E.A.L: The Second Clash — Static Recompilation

An unofficial native Linux version of the original Xbox game
**Double-S.T.E.A.L: The Second Clash** (Bunkasha, 2004, Japan; English
and Japanese). The game's own x86 code is translated to C on your computer and
compiled for x86-64. Nothing is emulated at run time except the hardware the
game drives directly; drawing goes through Vulkan.

> **You must provide your own copy of the game.** No game code or data is in
> this repository, and none is generated into it: the translated code and
> everything taken from the disc stay on your machine.

This project has no connection to the game's makers or to Microsoft.

## Status

**Alpha: a work in progress.** Nobody has finished the game on it yet.

- Tested: the menus, the attract demo, mission 1 (its cutscene, driving, the
  pause menu, retiring, the results, replays), saves, and English and Japanese.
- Not tested: most missions, and playing start to finish. Clearing a mission
  takes a person; automated runs can only drive into one.
- Tested only on the author's computer: a GPD Win Max 2 (Ryzen AI 9 HX 370,
  Radeon 890M, Mesa RADV) on Kubuntu, plus a build-and-boot check on an NVIDIA
  RTX 4070. Expect problems elsewhere, and please report them in Issues.

[ROADMAP.md](ROADMAP.md) has what is done and what is next, and
[NOTES.md](NOTES.md) every fix and why.

## What it adds

All optional, in the game's own OPTIONS menu under PC SETTING (see Settings):

- The game's own 720p mode and widescreen, at twice the internal resolution
- Anisotropic filtering and FXAA/SMAA
- Controller or keyboard, and the game's Japanese or English text and speech
- It runs at the game's own 30 frames a second. Faster is not an option: its
  logic steps once a frame.

## Download (Linux)

The AppImage on the [releases page](https://github.com/AntAir267/double-steal-2-recompiled/releases)
holds no part of the game. The first time you run it, it asks for your copy (a disc
image, or a folder of the extracted game), checks every file against the known release,
then translates the game and compiles it on your computer. That takes a few minutes, once,
and about 430 MB in `~/.local/share/double-steal-2-recompiled`, with your saves. After
that it starts the game at once.

It needs a 64-bit Linux with glibc 2.38 or newer (Ubuntu 24.04, Fedora 39 or later) and a
Vulkan driver. There is no Windows version yet.

## Building (Linux)

Needs Clang 20 and lld, CMake, Ninja, Python 3.10+, and development files for
SDL2, libepoxy, OpenSSL, Vulkan, glslang and SPIRV-Tools. On Ubuntu or Debian:

```bash
sudo apt install clang-20 lld-20 cmake ninja-build python3-venv pkg-config libsdl2-dev libepoxy-dev libssl-dev libvulkan-dev glslang-dev glslang-tools spirv-tools
```

The game files go in `game/Double-S.T.E.A.L - The Second Clash/`: the Redump
image of *Double-S.T.E.A.L - The Second Clash (Japan) (En,Ja)*, extracted with
[extract-xiso](https://github.com/XboxDev/extract-xiso) (368 MB).

The toolkit, [antbox](https://github.com/AntAir267/antbox), goes beside this
folder, at the commit this tree was checked on:

```bash
git clone https://github.com/AntAir267/antbox ../antbox
git -C ../antbox checkout "$(tail -1 config/toolkit-commit)"
```

Then translate the game and build it:

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
./regen.sh
cmake -S . -B build/game -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld
ninja -C build/game
bin/doublesteal2
```

`--game-dir` and `--save-dir` name other folders; `--headless` runs without a
window. `tools/check.sh` builds and drives into mission 1 unattended as a check.
On a new save the game installs its data first, so that first run stops at car
select; run it again to reach the mission.

For a binary that also runs on SteamOS, `./deck-build.sh` compiles the same
generated code inside an Ubuntu 24.04 container (podman; it needs glibc 2.38)
into `build/deck`; then `DS2_BIN=build/deck/bin/doublesteal2 ./package.sh DEST`
makes a folder to run from. `./package.sh` alone packages the host build.

## Playing

The game is in English unless you ask for Japanese.

The first start unpacks the game's data archives to the emulated hard
disk's cache, as the console does; it takes about ten seconds, behind the
logo. Let it finish: the game marks the cache installed before it starts,
so an install cut short is not redone (delete the save folder's `Cache` to
start it again).

### Controls

A controller works as an Xbox one (its button prompts are the Xbox's). Any
connected controller drives the game, including one plugged in while it runs.
That was tested with virtual controllers only; rumble with several connected,
and real hardware beyond one pad, are untested.
This is the game's own layout, Type A in Options > Controller Setting:

| Pad | In a mission | Keyboard |
|---|---|---|
| Right trigger | Accelerate | W |
| Left trigger | Brake | S |
| Left stick | Steering | A, D |
| Y | Action (dash) | Ctrl |
| B | Reverse / sidebrake; back in menus | Backspace |
| A | Gear up (manual); OK in menus | Space |
| X | Gear down (manual) | Shift |
| White | Zoom camera | Q |
| Black | Change map scale | E |
| Right stick | Rotate camera | |
| D-pad up / down | Change camera / rear camera; menus | Up / Down |
| D-pad left / right | menus | Left / Right |
| Start | Pause | Enter |
| Back | | Esc |

The keyboard drives the first pad, with or without a controller there,
while the game's window has the focus. `RECOMP_KEYBOARD=0` turns it off.
The arrows are only the d-pad here, since the game's d-pad works the
camera while you drive (`RECOMP_KEYBOARD_ARROWS=both` makes Up and Down
the triggers as well, as in the runtime's other games).

### Settings

In the game: OPTIONS > PC SETTING (in the main menu's Options and the
pause menu's), or Back + Start, or F1, anywhere. That opens the settings
menu over the game, which pauses: Display (fullscreen, aspect ratio, how
the window scales, the video mode, the picture area, the language, the logo
movie), Graphics (render scale, anisotropic filtering, anti-aliasing,
sharpening, texture sharpness), Audio, Input (deadzones, rumble strength,
the engine's vibration, Nintendo layout), Power and Advanced (the performance overlay, bug reports). A
setting tagged RESTART takes effect at the next start, and the menu offers
the restart. B or Esc closes it. The menu is drawn like the game's own
screens: its frame, row plates, prompts and colours, a PC SETTING heading
cut from its own lettering and its text font, all read from your copy of
the game (the screens from the cache it unpacks on its first start, so on
that very first start the menu looks plainer until then).

What you change there is kept in `settings.toml` beside the saves. The
defaults are `bin/enhance.toml` (copied from `config/enhance.toml` at the
first build; yours is never overwritten), which you can also edit; the
in-game menu's Reset goes back to it. The variable beside each sets it for
one run instead, over both files (`RECOMP_RENDER_SCALE=1 bin/doublesteal2`);
the menu shows such a setting locked.

| `enhance.toml` key | Default | What it does | `settings.toml` key | Variable |
|---|---|---|---|---|
| `display.hdtv` | `"720p"` | the dashboard's HDTV setting: the game's own 1280x720 mode; `"off"` for the console's 480-line picture | `display.hdtv` | `RECOMP_DISPLAY_HDTV` |
| `display.widescreen` | `true` | the dashboard's 16:9 setting | `display.aspect` (`"4:3"`, `"16:9"`) | `RECOMP_DISPLAY_WIDESCREEN` |
| `render.scale` | `2` | internal resolution: 2 draws everything at twice the size and filters it down | `graphics.render_scale` | `RECOMP_RENDER_SCALE` |
| `graphics.aniso` | `"16"` | anisotropic filtering: `"off"`, `"2"` to `"16"` | `graphics.aniso` (`"16x"`) | `RECOMP_GFX_ANISO` |
| `graphics.aa` | `"off"` | anti-aliasing of the finished frame: `"fxaa"`, `"smaa"` | `graphics.aa` | `RECOMP_GFX_AA` |
| `present.crop` | `"clip"` | show only the part of the HD picture the game draws in (it keeps a TV-safe border) | `display.crop` | `RECOMP_PRESENT_CROP` |
| `present.filter` | `"linear"` | how the window scales: `"nearest"`, `"linear"`, `"integer"` | `display.filter` | `RECOMP_PRESENT_FILTER` |
| `present.fullscreen` | `false` | | `display.fullscreen` | `RECOMP_PRESENT_FULLSCREEN` |
| `system.language` | `"english"` | `"japanese"` for the Japanese data on the disc | `system.language` | `RECOMP_LANGUAGE` |
| `game.skip_intro` | `false` | start at the title screen, without the logo movie | `game.skip_intro` | `RECOMP_SKIP_INTRO` |
| `game.engine_vibration` | `false` | the controller hums with the engine's load whenever you accelerate, as on the console; off keeps only the knocks (collisions, rough road, the rev limiter) | `game.engine_vibration` | `RECOMP_ENGINE_VIBRATION` |

The menu's other settings are the runtime's (its `docs/runtime/overlay.md`
lists them all).

Saves, replays and the hard-disk cache go to `~/.local/share/doublesteal2`
(`$XDG_DATA_HOME`) unless `--save-dir` names another folder; the compiled
shaders to `~/.cache/doublesteal2`.

### What is different from the console

- Online Score Board hands off to `stats.xbe`, the disc's Xbox Live
  program, which no recompiled build can run (and Xbox Live is gone): the
  game starts again, as that program would have handed back.
- The engine's hum on the controller is off unless you turn it on
  (Engine vibration, on the settings menu's Input page): the game holds
  both motors at up to a third of full whenever you accelerate, which the
  console's heavy motors made a faint buzz and a modern pad plays as a
  steady vibration. Collisions and rough road still knock. The game's own
  Options > Controller Setting > Vibration turns all of it off.
- Dates on saves and replays are your computer's local time, and the
  dashboard settings the game reads (language, video, audio) come from the
  table above rather than from a console.

## How it fits together

| Property | Value |
|----------|-------|
| Title | DOUBLE S.T.E.A.L SECOND CLASH |
| Title ID | 0x42550004 |
| Region | Japan (English and Japanese text) |
| Built | 2004-12-03, XDK 1.0.5849 |
| Debug path | `e:\Projects\DSTEAL2\xbox\DSTEAL2\Retail\default.exe` |
| Base / entry | 0x00010000 / 0x00113F0A |
| Kernel imports | 118 |

```
0x00011000  .text     1.10 MB   game code
0x0012B4A0  D3D       78 KB     Direct3D (statically linked)
0x0013ECC0  D3DX      115 KB
0x0015BA00  XGRPH     23 KB
0x00161560  DSOUND    46 KB
0x0016CBA0  XACTENG   42 KB     XACT audio engine (Wreckless had none)
0x001771A0  XMV       160 KB    XMV video (Wreckless used WMVDEC)
0x0019F1C0  WMADEC    100 KB
0x001B8240  XPP       30 KB     controllers
0x001BFBE0  .rdata    74 KB
0x001D21E0  .data     2.39 MB   data + BSS
0x00435B80  DOLBY     28 KB
```

Differences from Wreckless that matter: a later XDK (5849, the same as
Burnout 3 and Half-Life 2, which both run on the toolkit), XMV video instead
of WMV (Wreckless's known stall is in its WMV decoder), the XACT audio
engine, and Xbox Live code (leaderboards, `stats.xbe`).

The translator and runtime are the toolkit; this repository holds only what
belongs to this title: settings, addresses the translator needs told, hooks, and
the code that draws the settings page in the game's look from your own files.

## Credits

- [sp00nznet/wreckless](https://github.com/sp00nznet/wreckless) and
  [sp00nznet/xboxrecomp](https://github.com/sp00nznet/xboxrecomp): the
  recompilation of the first game, whose layout and pipeline this started from,
  and the toolkit it is built on.
- [xenophobentx/xboxrecomp](https://github.com/xenophobentx/xboxrecomp/tree/takedawn):
  the `takedawn` branch, the runtime of the Burnout 3 port Takedawn, which antbox
  grew from (Vulkan renderer, Linux device traps, the settings overlay).
- Bunkasha, for the game.

## License

MIT, like sp00nznet/wreckless, whose layout and scripts this started from
(its license is kept as `LICENSE.wreckless`). It covers this repository's own
code only, not the game.
