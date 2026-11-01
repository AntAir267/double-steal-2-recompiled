# Working notes

Everything found while bringing the game up, with the evidence. Paths under
`work/` and run names (perf1, ab1 ...) are the author's scratch folder, not in
this repository. Addresses are the game's own (`default.xbe`). Toolkit commit
hashes are from its development history and may not exist in its public copy.

## Layout

- `game/Double-S.T.E.A.L - The Second Clash/` — the disc's files, extracted
  with extract-xiso from a Redump image (7.8 GB, mostly the video partition;
  the game partition is 26 files, 368 MB). Not in git.
- `work/` — scratch, not in git.
- The toolkit, beside this checkout (section "Toolkit: antbox").

## What came from Wreckless

sp00nznet/wreckless is a thin per-title layer: `regen.sh` (pipeline order),
`config/seed_functions.json`, a CMake that adds `../xboxrecomp`, and a Win32
`main.c`. Carried over: the layout, the regen order, the seed discipline,
and its lessons:

- **Thread start routines need dispatch entries.** Wreckless seeded
  0x000E8A42 (XAPI's thread startup, handed to PsCreateSystemThreadEx) and
  0x000EB50F (the XAPI main-thread startup). In DSTEAL2 the same pair is
  0x00110846 (pushed by XAPI CreateThread at 0x001108DE) and 0x00113E96
  (handed to CreateThread by the entry point); toolkit v0.16's disassembler
  finds both itself (immediate-reference targets), so no seeds are needed for
  them.
- **A failed audio probe skips the whole engine init.** Wreckless reached
  its main loop with no renderer because `DirectSoundCreate` failed
  (toolkit docs/pipeline/06-debugging.md). `RECOMP_AC97_READY=1` brings up
  the emulated APU and is the first thing to try when nothing draws.
- **Wreckless's known stall is its WMV decoder** (LOGO1 intro). DSTEAL2 plays
  `LOGO1.XMV` with the XDK's XMV library instead, the same library Burnout 3
  uses; there the player paces on D3D's vblank count (Takedawn's main.c),
  so without a vblank the intro holds on its first frame.

Not carried over: Wreckless's `main.c` (hard-coded Wreckless addresses, Win32
only; the toolkit template is newer) and its build glue (Windows only).

## Differences from Wreckless

| | Wreckless (DSTEAL02) | Second Clash (DSTEAL2) |
|---|---|---|
| XDK | 1.0.4134 (launch) | 1.0.5849 (= Burnout 3, Half-Life 2) |
| built | 2002-01-08 | 2004-12-03 |
| .text | 947 KB | 1.10 MB |
| functions (toolkit v0.16) | ~3,470 | 9,031 |
| video | WMVDEC | XMV |
| audio | DSOUND | DSOUND + XACT engine |
| online | none | Xbox Live (stats, leaderboards) |

## Pipeline (regen.sh), 2026-10-24

- disasm 506 s (single-threaded Python): 593,772 instructions, 9,031
  functions (.text 7,015; D3D 283; D3DX 436; XGRPH 92; DSOUND 453; XACTENG
  309; XMV 73; WMADEC 138; XPP 232), 73.4 % reachable.
- recomp: 9,032/9,032 functions, 0 failed, 950,957 lines of C in 37 files.
- 5 unresolved call targets stubbed (called, never detected):
  0x000213F0, 0x00104095, 0x0011E817, 0x0014E28B, 0x0014E28F. Check each
  before seeding: a mid-function address must not be seeded.
- The toolkit's XDK name list applied 0 names (it has none for 5849 here).

## Runtime: the takedawn branch

Upstream's Linux path has no x86-64 MMIO traps (the APU crashed DirectSound)
and draws statically linked D3D only with a CPU rasteriser. The game runs on
xenophobentx/xboxrecomp's `takedawn` branch (from f84bd73), the toolkit
Takedawn ships Burnout 3 on: Vulkan NV2A renderer, MMIO traps, case folding,
APU, USB pads. Same XDK (5849).

What this game changed in that runtime, all in antbox now:

| Change | Why |
|---|---|
| PVIDEO_BUFFER (0x8700) busy-bit ack | XMV's overlay stop spun forever (0x00131F81) |
| NtQueryVolumeInformationFile class 1, NtQueryInformationFile class 6 | XAPI GetFileInformationByHandle failed, XFONT_OpenBitmapFont failed, the disc error screen |
| NtReadFileScatter (was a stub) | streaming reads stayed STATUS_PENDING; the queue at 0x000E8180 called it a damaged disc |
| semaphore DMA object base from RAMHT | D3D's fence is at 0x80002000 (DMA 8 has base 0x2000); releases went to 0x80000000 and the fence was never the executor's |
| D3D software methods (NOP with a parameter) run the title's dispatcher | method 13 applies RunPushBuffer fixups (re-points index-block exit jumps); without it the walker looped |
| kick ack while the dispatcher runs | the dispatcher's flush spun on 0x100410 with the ack thread busy |
| walker stop report: last hops; DS2_PB_STOP_DUMP=file | bring-up |
| depth formats 0x2E-0x31 decoded (D24S8/F24S8 and D16/F16 twins of 0x2A-0x2D) | undecoded, they came out magenta |
| depth-only passes (colour offset 0) draw into a scratch colour image | the shadow map's draws were all skipped |
| shadow buffers: a depth-format texture compares r/q with SET_SHADOW_COMPARE_FUNC (0x1E6C), 4-tap PCF, sampling the depth image the backend drew | the car and building shadows |
| texture modes DOT_ST, DOTPRODUCT (with the dot mappings), DPNDNT_AR/GB | the depth of field's lookup and its noise-offset blur |
| render targets: 64 slots, 8 depth buffers | 59 colour surfaces and 5 depth buffers a frame; the least recently used went every frame |
| a texture in the other layout than its target (swizzled over linear) reads guest memory; X8 formats sample alpha as 1; depth formats never sample a colour target | the glow layer covered the frame; the scene dropped out of the blend |
| a LIN_A8R8G8B8 texture over a Z24S8 depth buffer samples the depth image, packed as the bytes would be | the depth of field reads its depth buffer as colour; from stale memory the whole frame took the far blur (the haze) |
| the fence mirror's readable-address bound is the first page, not the calling thread's TIB | PGRAPH 0x400B10 froze once the ack thread had a TIB; D3D's wait for idle hung going into a mission |
| RECOMP_GPU_CPU_READS=WxH,...: those pitch targets are written to guest memory every flip | the CPU reads a 16x16 luminance target (exposure) and a 64x16 depth one (focus); unread, the exposure ran to its maximum (white-out) and the focus stayed put (car select all blur) |
| a shadowed KEVENT's SignalState is written to its header on every set and reset, and a header that reads clear over a signalled host twin clears it before a wait | XAPI rebuilds a stack event inline for each synchronous USB request; the twin stayed signalled from the last one, the wait returned early, and the request completed into a dead frame (a wild call from the USB interrupt) |
| the combiner state's debug byte moved last, never written to the shader key list; key list version 2 | the vk_fs_show byte had shifted tex_shadow, so key lists written before it rebuilt one shader with a garbage debug view that failed to compile |
| skipped draws by reason in the present report; unhandled methods per subchannel | diagnostics for the correctness sweep |
| graphics.aniso and graphics.aa read from enhance.toml | the settings overlay's keys, which this build has no overlay to set |
| RECOMP_LAUNCH_RESTART=<programs>: launching one restarts the title | the online score board hands off to stats.xbe, which a recompiled build cannot run; the game quit |
| a restart in place gets the real stdout/stderr back before the exec | the log capture's pipes had no reader in the new run, which died of SIGPIPE on its first line |
| XC_MAX_OS answered (the user section, built from the single indices); XC_TIMEZONE_BIAS in Win32's sign | XAPI takes its time zone from the user section, which was zeros: saves and replays were dated in UTC |
| Partition0.img's sector 4 written only when the image is new | it is XAPI's cache table; rewritten at every start, the cache was formatted at every boot |
| a FATX header written to cache partition 3-5 empties the save dir's Cache | Z:'s files live in that folder, not the image, so a format (the language switch) changed nothing |
| NtOpenSymbolicLinkObject / NtQuerySymbolicLinkObject answer from the links the title made | every link read as \Device\CdRom0, so XFormatUtilityDrive formatted the DVD drive |
| xbox_FileHandlePath on Windows (GetFinalPathNameByHandle) | the format detection, and the [READ] lines, need the handle's file |

And in this repo: native memmove (the fork cuts the CRT's at its jump table),
seeds (config/), the title's runtime defaults and hooks in main.c
(`RECOMP_GPU_CPU_READS=16x16,64x16` among them), bring-up wrappers in
recomp_debug.c.

## Toolkit: antbox

The game builds on antbox, a shared toolkit for recompiled Xbox games: the
`takedawn` branch of xenophobentx/xboxrecomp with the game-agnostic fixes of
this game, Wreckless and MechAssault 2 on top. `config/toolkit-commit` names
the commit this tree was last checked on. Title-specific code stays here.

The check (what proves the game still works on a new toolkit): regen
(`./regen.sh`, diff against a snapshot), build, then the headless drive into
mission 1, which `tools/check.sh` runs:
  `RECOMP_PAD_SCRIPT="7000:start,9000:start,11000:start,13000:a,16500:a,20000:a,22500:a,25000:a,27500:a,29500:a,64000:rt:9000,74000:rt+lleft:1500,76000:rt:7000,84000:rt+lright:1500,86000:rt:6000" EVERY=600 work/run.sh NAME 100`.
  It passes with no `[ICALL] Failed`, no stub hit and no crash in the log,
  the last frame dumps in mission 1 with the HUD, and the mission's TIME
  advancing as the dumps' file times do (10.00 s per 10 s). The windowed
  part, when the overlay or menus changed: no --headless, RECOMP_WINDOW_QUIT_AFTER=50,
  `RECOMP_PAD_SCRIPT="7000:start,9000:start,11000:start,13000:a,16500:down,18000:down,19500:a,22000:down,23500:down,25000:down,26500:down,28000:down,29500:down,31000:down,33000:a"`
  (OPTIONS is two downs on the main menu once a replay is saved, then seven
  to PC SETTING) and RECOMP_OVERLAY_TEST=<file> fed `state <path>` and
  `pad rb` lines from about 36 s. It passes with "[DS2] OPTIONS: PC SETTING
  row added", "[OVERLAY] open" and "closed" in the log, and the Display page
  listing Video mode, Picture area, Language and Skip the logo movie.

What antbox brought that DS2 did not have, checked on it: the frame
counter stands down once the title moves it ("moved by the title" at boot:
DS2's own vblank handler moves [device+0x1DE8]); before, the runtime's +1
doubled the count. DS2's timers are time-based (the mission's TIME and TIME
LIMIT kept real time either way) and LOGO1 took as long (the title loads
at 5.7-6.0 s in both runs), so nothing visible changed. The software-method
kick ack answers at once (Wreckless's executor went from 28-88 ms a frame
to about 9; not measured on DS2, ab2 overlapped another game's run).
Takedawn's own new commits (graphics.softbody, Input > First controller)
add no row DS2 shows.

## The colours, 2026-10-24

The frame is the scene (640x576, alpha 0) composited with a processed copy:
a 4-tap downsample to 256x256, ping-pong blurs, a depth-of-field pass that
picks between blur levels by a 256x4 lookup on the depth (read as
LIN_A8R8G8B8 bytes, DPNDNT_AR), a noise-offset blur scaled by the same
lookup (DOTPRODUCT + DOT_ST), and a bloom pyramid (256x128 down to 32x16).
Both the composite and the depth of field multiply the scene by 1.49
(combiner x4 of 0x5F), so the scene is meant to be stored dark: its world
shader multiplies the light by an exposure constant (c[34]) the CPU works
out from the 16x16 target, and a missing readback sent it to 2.82.
Checked against press screenshots of the Japanese release (everyeye.it):
the colours, contrast and focus now match. Frame rate unchanged (30 fps).

What looks odd but is the game: the replay demo's filters (old film with
scratches and grain, sepia, green night vision, black-and-white ink
outlines) and white flashes on crashes.

## Status, 2026-10-24

Boots through the first-run cache install (pkfiles0/1 into Z:), the LOGO1
intro (no picture: the PVIDEO overlay is not displayed), the animated title
and PRESS START, and the attract demo renders in 3D (cars, streets,
the logo in the corner), headless on the Radeon 890M.

Colours fixed (above). With a scripted pad (RECOMP_PAD_SCRIPT) the menus
work (main menu, game data, police station, mission select, car select)
and the first mission starts and plays: mission110.pk and the st110
cutscene load, "READY?", then driving with the HUD, minimap and timer.

## 720p, 2026-10-24

The game has its own HD mode, which the runtime offers through the
dashboard's video settings (display.hdtv = "720p", display.widescreen =
true; the kernel reports them through XC_VIDEO and the SMC's AV pack). It
then makes 1280x720 16-bit front buffers (X1R5G5B5, pitch 2560), renders
the 3D scene into a 768x672 buffer (640x576 in SD) and composites it, the
HUD on top, inside the TV-safe area: every front-buffer draw is clipped to
1152x648 at 64,36. Three things were wrong:

- The composite went past the far plane. XDK D3D's pass-through program
  scales screen z by 16777215 whatever the surface says, and a 16-bit
  colour surface carries the Z16 format even with no depth buffer; the
  backend divided by 65535, z came out near 128 and every pixel was
  clipped. The HD attract demo was black with the title's film-scratch
  sprites showing, and missions showed only HUD layers piling up. Draws
  without a depth buffer are now mapped by SET_CLIP_MIN/MAX (0x394/0x398,
  now kept) where that range is the wider: the console clips by it.
- A loading screen draws a 16x16 target at a front buffer's address; the
  1280x720 target was retired and came back from the next clip as
  1216x684. The presenter grows a front buffer to the display mode, and
  presents the mode, not the clip (columns as well as lines).
- On a monitor the safe-area border shows. present.crop = "clip" (new)
  shows only the part of each frame the title drew in: 1152x648, exactly
  16:9. SD frames are drawn edge to edge, so it changes nothing there.

## The intro movie, 2026-10-24

LOGO1.XMV (the Bunkasha Games logo) plays on the NV2A's video overlay:
XMV decodes 640x480 YUY2 frames into two contiguous buffers, and D3D's
UpdateOverlay points PVIDEO at one (BASE + OFFSET, SIZE_IN, POINT_OUT,
SIZE_OUT, DS_DX/DT_DY in 12.20, FORMAT with the pitch, the YUV order and
the colour-key bit) and sets its bit in NV_PVIDEO_BUFFER. On the console
the display composites it at scanout; nothing of it is in guest memory,
and the title flips its framebuffer once in the whole movie. Now the ack
thread records each buffer flip's registers (src/kernel/xbox_pvideo.h)
and the Vulkan presenter composites the latest frame as its first post
pass (BT.601 video range, the colour key when on), presenting again at
each new overlay frame. 720p: 864x648 at 208,36, the 4:3 picture centred
in the safe area.

## Distant trees, 2026-10-24

Trees past a few metres were bare trunks (and the chains between the
kerb posts were gone): their canopies are alpha-tested DXT1 cut-outs
whose combiner passes the test with the texture's alpha x 4/255 against
"alpha >= 2", a 50% cut on the texel. The runtime's coverage-keeping
mip rebuild (BO-171, alpha_mips, made for Burnout 3) took 2 as the
texel's threshold and scaled every level below the first down to land
on it, about 1% alpha: none of it passed the real cut. It now never
scales a level below its box-filtered alpha. Without mips (vk_no_mips)
or without the rebuild (alpha_mips=0) the trees were there, which is how
this was found. On the way: TEXTURE_CONTROL0's LOD clamps are 4.8 fixed
point (the title's MIN 0x100/0x200/0x400 are levels 1, 2, 4), and were
read as whole levels, which pinned those textures to their last level.

## Mission start hang, 2026-10-24

Going from car select into a mission hung at random (two runs in four):
the picture froze on the car select or went black, no file opened, and
after 15 s the hang watchdog wrote a report. The main thread was in XDK
D3D's wait for idle (0x0012F9B0), which spins until bits 2-6 of PGRAPH
0x400B10 equal the fence word at 0x80002000 shifted left by 2. The fork
keeps that register in step with the fence from its GPU ack thread
(pgraph_fence_tick), but the check that the fence's addresses are readable
(fence_readable) took the calling thread's TIB as the lowest mapped
address. That was a constant once (0x4000); it is per thread now, and our
software-method dispatch gives the ack thread a TIB of its own above the
D3D device (0x0013C2A8). From the first software method on, every check
failed and the register froze at the fence of that moment (5, so 0x14);
the wait passed only when the fence ended on the same five bits. The bound
is now the fixed first page (XBOX_TIB_MAIN).

## Correctness sweep, 2026-10-24

Scripted runs through the menus, Change Time, Free Drive and mission 1:

- Change Time works: the police station backdrop turns to dusk or to the
  lit-up night city, and Free Drive at night starts in the station's
  garage under its lamps. Missions keep their own time of day (mission 1
  is daytime whatever the setting). Outdoor night driving is not checked:
  the garage is hard to drive out of blind.
- The game steers with the left stick only (the d-pad does nothing while
  driving; the pad script's lleft/lright), and ignores a trigger held from
  the moment control starts until it is released. Both are the game's.
- One fragment shader failed to compile on every start (GLSL, an
  undeclared i_t189): the debug byte added for vk_fs_show sat in the middle
  of the combiner state, so key lists written before it put a shadow-mode
  byte (0xC2) into it. Fixed in the fork (above); the old list is discarded
  once.
- Skipped draws (about 1,200 in a mission) are all draws with too few
  vertices for one primitive, which draw nothing on the console either.
- The executor's "unhandled methods" survey is the CPU rasteriser's: it
  counts texture-stage and render-state methods the Vulkan backend reads
  from the register file. Of the other subchannels' methods, only object
  setup and one bookkeeping method on subchannel 5 (0x0310) appear, and no
  2D blits, so nothing there is missing.
- A wild call from the USB interrupt at the title screen: the URB of the
  title's XInputGetCapabilities pair completed after its stack frame was
  gone (call site 0x001BAD12, the URB completion in XAPI's OHCI driver).
  The cause was the kernel's event shadow (above); it is gone now.
  recomp_manual.c now logs a wild call's return address, the stack and the
  first argument's words.

Anisotropic filtering is on at 16x (config/enhance.toml): the road and
its markings stay sharp into the distance, and frames are otherwise the
same (a deterministic scripted run differs only in texture detail). FXAA
and SMAA work in the window; they are off, as render.scale 2 already
supersamples the 3D. The black-and-white striped frames in the attract
demo are one of its replay filters, not SMAA.

## The menus, 2026-10-24

Every main menu entry checked with scripted runs. Options (guide map,
speedometer, the four volumes, Controller Setting with its vibration,
camera panning and controller type, and the button diagram) and Game Stats
(scrolling) work; Replay Theater is greyed until a replay is saved; Back
on the main menu asks to return to the title, and Yes goes there.

- Controller Setting crashed the game. The Options screen's input handler
  (sub_0010A900, 0x0010A900-0x0010AA21) is a switch whose case blocks sit
  after its first ret. The detector took the code right after that ret
  (0x0010A95C) for a new function (gap_prologue), which ended sub_0010A900
  there, and two cases, 0x0010A9B8 and 0x0010A9E7, fell in a gap: lifted as
  stubs that pop a return address and return. A on Controller Setting ran
  one; its caller's esi became a return address and the next virtual call
  read [esi+0x24] (fault in sub_0010A3D0). Found with the ABI-check build
  (-DRECOMP_ABI_CHECK, build/linux-tkd-abi, binary in work/bin-abi), which
  names the first function to return with ebx/esi/edi changed or its return
  address still on the stack. Seeded both, and 0x0011E75A (the CRT's x87
  error path jumps to the second instruction of a 7-byte helper); the
  regeneration added those three and an entry at 0x0010A945 (the jump back
  into sub_0010A900) and changed only sub_0010A900 and sub_0011E758, now
  split with fall-through tail calls. The other 16 stubs are data decoded
  as code (switch tables, XACT and WMA tables), none ever reached.
- Online Score Board quit the game: it hands off to stats.xbe on the disc,
  Microsoft's Xbox Live stats program, which boots d:\default.xbe again
  when it is done. With Live gone it could only fail and hand back, so the
  game now starts again (RECOMP_LAUNCH_RESTART=stats.xbe in main.c, a
  restart in place with the arguments and environment it was started with).
- The ABI-check run also lists the CRT's own unusual conventions, which are
  right: the SEH prolog/epilog and the stack probe move esp by design, and
  _aulldvrm (0x00121EF0) returns its remainder in ebx:ecx.
- Scripting note: the second A after the title (13000 ms) already picks
  Play Game, so a third A opens Mission Select; a press while a screen is
  still fading in is lost.

## In a mission, 2026-10-24

- The pause menu (Continue, Restart, Retire, Options, Clear Condition, over
  the city map) did not appear: the picture dimmed and nothing more. Its
  texture, data/screen/PauseMenu.xpr on the cache drive, was all zeros.
  The first-run install unpacks pkfiles0/1 into Z: (the utility partition,
  work/save/Cache); that one file had been written by this morning's first
  bring-up run, when disc reads were still broken. A fresh install writes
  it right, and every other cached file matches the archives byte for byte
  (work/pk/pk.py lists and unpacks a .pk: LZSS, a 4 KB ring of spaces then
  zeros, written from 0xFF0). Fixed in work/save by hand. The game writes
  its installed.us marker before it unpacks anything, so an install cut
  short (it takes about 10 s) would leave a broken cache that it never
  redoes; worth knowing, not worth a guard.
- Retire asks "You will lose your progress in this mission" and goes back
  to the police station. Letting the time limit run out shows RESULT (max
  speed, jump, cars and objects destroyed) over the replay, then the
  replay menu (Hide Menu, Retry Mission, Exit Replay, View Result, Save
  Replay); the replay plays with its film filters. Save Replay writes a
  save of its own (U:\0DAE131983F0) and the Replay Theater then lists and
  plays it.
- The replay was dated 4:36 AM the next day: UTC. XAPI reads the time zone
  from the dashboard's user section (XC_MAX_OS), never XC_TIMEZONE_BIAS,
  and the kernel answered zeros. Fixed in the fork; the earlier "host time
  zone" change had used the wrong sign and changed nothing here, since the
  game never reads that index (the save it was checked on had been made at
  midday and showed UTC).

## Skipping the logo, 2026-10-24

game.skip_intro in enhance.toml ([game]; RECOMP_SKIP_INTRO) starts at
the title. The logo scene (vtable 0x001C9E1C) opens d:\LOGO1.xmv in its
start step (sub_00102B40: the disc error screen if that fails) and plays
it in sub_00102A20, which runs XMV on a thread of its own and waits on it
without reading the pad, then deletes the player. The hook in
recomp_hooks.c (the former recomp_debug.c; wrapped functions there need a
regeneration) deletes the player without playing. Making the file look
missing instead brings up the disc error screen.

## The cache install, 2026-10-24

The game keeps its data on the utility drive (Z:, the save dir's Cache),
unpacked from pkfiles0.<lang>.pk and pkfiles1.pk at first start
(sub_00012150): if z:\installed.<lang> is missing it formats the drive
(XFormatUtilityDrive), creates the marker, and an installer thread unpacks
every entry that is not there yet, one reader object and worker thread
per file. Five things were wrong:

- The runtime rewrote sector 4 of Partition0.img (a ****PARTINFO**** table)
  at every start. That sector is XAPI's cache table (signature 0x97315286,
  which title has which cache partition), so XMountUtilityDrive never found
  its entry and formatted the cache at every boot.
- The format only rewrote Partition5.img; the files are in the Cache folder.
  A FATX header written to a cache partition now empties it.
- XFormatUtilityDrive asks where \??\Z: points, and every link was
  answered \Device\CdRom0. Links now answer with what the title linked.
- With those, switching to Japanese formats the cache and installs the
  Japanese pack (menus, briefings and captions in Japanese; the date as
  2026/10/24 12:20); before, the English files stayed, behind both markers.
- In about half the installs the installer stopped (behind its LOADING
  bar, the file in progress full of zeros: this is what left
  PauseMenu.xpr zeroed this morning, not the disc-read bug). The reader
  (sub_00029850) starts its worker thread and only then creates the two
  events the worker signals; the worker reads its chunk from the host's
  file cache at once and signalled [this+0x88] while it was still 0, so
  the reader waited for ever. On the console the worker's disc read blocks
  long enough. A hook (recomp_hooks.c, sub_00029610) makes the worker wait
  for both events to exist; 16 installs out of 16 then completed, byte for
  byte equal to the archives (work/pk/pk.py). Found with gdb: the reader
  thread was waiting on an event in sub_000293E0 with no worker left.

## Builds for SteamOS, 2026-10-24

package.sh makes a folder to run from (binary, enhance.toml kept if
present, README, licenses, VERSION with both trees' commits and the glibc
the binary needs). A build on a newer system can need a newer glibc than the
Steam Deck's (SteamOS, 2.41), so deck-build.sh compiles the same generated
code in podman (localhost/ds2-build:24.04, container/: Ubuntu 24.04, Clang
20) into build/deck: glibc 2.38, GLIBCXX 3.4.32, the same five libraries
(SDL2, libepoxy, OpenSSL 3, libstdc++, glibc).

## Settings in OPTIONS, 2026-10-25

The runtime's settings overlay is built in now (XBOXRECOMP_OVERLAY on for
this title), and the game's own OPTIONS screen has a row that opens it:
PC SETTING, under CONTROLLER SETTING, as Burnout 3 has TAKEDAWN SETTINGS.
Back + Start and F1 open it too.

- The screen is data. Options.xpr holds a layout before its two DXT
  textures (Options.tga 1024x512: labels, values, arrows, the panel;
  Help.tga 512x512: the hint lines, English or Japanese, the labels being
  English in both): 32 pictures, one page of 46 elements and 28
  animations. The screen's code (vtable 0x001CA72C) finds elements by name
  and switches on an element's item number (sub_0010A900, A; sub_0010A530,
  left and right); the menu code moves the cursor by the up and down links
  each element carries (element index deltas). options_row.c has the
  format in its header; work/opt/lay2.py and anim.py dump one.
- When a screen resource is about to parse its layout (sub_000EB7A0, the
  XPR loaded and its textures registered), a copy with the row added is
  built and parsed instead: two pictures, the row and its hint (item 10,
  linked between CONTROLLER SETTING and RESET TO DEFAULT), Sel_, Flash_
  and UnSel_PcSetting (CONTROLLER SETTING's three on the new elements),
  and the intro's slide and fade for it, with RESET TO DEFAULT and EXIT
  one step later. Ten rows take 31 lines each where nine took 33.
- The label is cut from the screen's own lettering, in the player's copy at
  run time: the P of GUIDE MAP and the C of MUSIC VOLUME along the italic's
  slant (the letters share a one-pixel outline; per-row cut tables), then
  SETTING from CONTROLLER SETTING at that word space. The hint "PC version
  settings" comes from words of the other hints (P of Plays, C of Choose,
  v of voice, er of Controller, si of music, on, settings), 設定画面へ
  from the end of the controller setting's in Japanese. Decoded, put
  together and encoded back to DXT5 / DXT1 into empty rows of the
  textures; the new layout goes after them in Help.tga's empty rows, so it
  is freed with the screen. A registered texture's address is physical
  (D3D keeps VA & 0x0FFFFFFF for the GPU): the CPU reads it at 0x80000000
  up, which the first run missed (the check that the pictures are there
  refused, and the screen stayed as it was). The XPR loader keeps each
  resource's pointer at +0x3C, its size at +0x40 and its type at +0x44
  (0x80000000 for the layout).
- Scripted runs: RECOMP_DEBUG=pad_script= splits the script at its commas
  (RECOMP_DEBUG is a list), so only the first press reaches the pad; use
  RECOMP_PAD_SCRIPT, deprecated or not. With a replay saved, Replay Theater
  is a stop on the main menu: OPTIONS is two downs.
- A on the row (sub_0010A900, item 10) plays the menu's confirm and asks
  the overlay to open; the screen stays as it was under it.
- The overlay keeps what the player changes in settings.toml in the save
  folder. enhance.toml stays the build's defaults: settings.c gives its
  values to the overlay as the defaults (and sets them like env_default
  does, or the overlay shows its own), and settings.toml's values reach the
  environment first, which every reader ranks over the file. This title's
  own rows (the runtime's new xbox_SettingsAddTitle): video mode, picture
  area (live), language, skip the logo movie. Hidden (xbox_SettingsHide):
  the 2D and texture upscalers, the overscan crop, the button prompt sets.
- work/opt/test_row.c runs options_row.c on the extracted Options.xpr in a
  fake guest memory, for both languages and the other screens' files.

## The overlay in the game's look, 2026-10-27

The settings page the runtime brings looks like Burnout 3's front end, which
does not suit this game. The page PC SETTING opens is drawn in the game's own
look now, through a skin API added to antbox
(xbox_OverlaySetSkin, xbox_OverlaySetGameFontAtlas: d6a22f8, in antbox
since the 2026-10-29 batch). With no skin set the overlay is as before.
skin.c builds only when the toolkit has the API (XBOX_OVERLAY_HAS_SKIN), so
this tree still builds on antbox without it. skin.c builds the skin at start from the player's
files; ds2_pc_setting_open tries again if the cache was not there yet
(the first start unpacks it).

- The pieces come from the OPTIONS screen (save folder's
  Cache/data/screen/Options.xpr, first texture): its backdrop
  (384, 0)-(1024, 480) is the 640 x 480 page, so the header is rows 0-62,
  the footer 386-479 and a row plate (454, 77)-(954, 107); the arrows and
  the A / B prompts are the layout's pictures 1, 2, 30 and 31. The heading
  PC SETTING is cut along the headings' slant (a quarter texel a row): P
  from OPTIONS, C and SETTING from CONTROLLER SETTING
  (ControllerSetting.xpr, strip (0, 420)-(360, 452)). Each is checked by an
  FNV-1a of its texture blocks [verified 2026-10-27: the same in the
  English and Japanese packs, which differ only in Help.tga].
- Places and colours are the game's own [verified: the Options layout,
  work/opt/lay2.py and lay3.py]: the title at (40, 30), rows from 77 every
  33, hints centred at x 320 from y 388, prompts at (35, 420) and
  (105, 420). Items are drawn 0x808080 over their lettering (grey); the
  selected one flashes from white to 0x3CFF00 (the label) and 0xFFA800
  (its colon and value) over 15 frames, again and again (Flash_*).
- The words use the game's text font, data/fonts/arial32.bmf, an XFONT
  bitmap font (format in skin.c's header; work/skin/bmf.py decodes one).
  The anti-aliased escape (0 0 an empty run, 0 a one pixel of alpha a) is
  what xfont.bmf (binary) does not have; decoded that way all 94 printable
  glyphs give exactly width x height pixels [verified]. The file is regular
  Arial, the game's lettering bold Arial with a black outline, so the atlas
  is made a texel bolder and the overlay outlines it (6% of the size);
  beside the game's own "Set music volume" they match (work/skin/font_cmp2.png).
- Values are in capitals, a unit after a number as it is (720p, 16x).
  A hint fits the footer's band in one or two lines (down to 80%); a
  longer one goes above the footer and the page keeps it room by the
  page's longest hint, so the rows do not move with the selection.
- The menu sounds are the game's (skin.c, "the menu sounds"): System.xsb's
  cue 3 (the cursor) for a move, a value and a page, 1 (confirm) for A,
  2 (cancel) for back and close, none for opening (PC SETTING plays
  confirm itself). The waves are decoded from the cache's InMemory.xwb
  (Xbox ADPCM) and handed over with xbox_OverlaySetMenuSoundsPCM (antbox
  a43e467, in antbox since the 2026-10-29 batch). Played 1.4 dB under the
  bank's volume: the game's own cursor and confirm reach the output 1.8 and
  1.0 dB under it [measured: RMS in a RECOMP_AUDIO_WAV capture, run sk5;
  work/skin/find_snd.py finds a wave in a capture]. While the title is
  paused the capture only runs while a cue plays, so the overlay's cues sit
  back to back in it.
- Checked windowed (work/skin/wincheck.sh: OPTIONS > PC SETTING by the pad,
  window shots of every page): runs sk1-sk4 skinned; sk0 with
  RECOMP_OVERLAY_SKIN=0 RECOMP_OVERLAY_GAME_FONT=0 shows the overlay's own
  page as pc7 did (title, arc, boxes, colours; the footer named pad
  buttons instead of keys, which follows the last input, not the skin).

## Overhead, 2026-10-28

ROADMAP section 6. First numbers, from the
headless mission 1 drive (tools/check.sh's pad script, 100 s) with
RECOMP_DEBUG=pacing and tools/perf/threads.py (each thread's CPU time from
/proc once a second; perf is not allowed here, perf_event_paranoid 4).
Taken while the machine had other load (remote desktop and a browser, load
5-6 of 24 threads): CPU time per thread is little moved by that, the
flip percentiles a little; the windowed baseline, GPU use and load times
still want a quiet machine.

| run | pacing | process CPU | guest-main | nv2a-ack | nv2a-swm-kick | mcpx-apu | flips p5 / p50 / p95 / max |
|---|---|---|---|---|---|---|---|
| perf1 | spin (the runtime's stock) | 130% | 98% | 16% | 8% | 5% | 33.1 / 33.3 / 33.6 / 34.3 ms |
| perf2 | sleep, lead 8 (Balanced) | 104% | 49% | 39% | 9% | 6% | 33.3 / 33.3 / 33.3 / 33.6 ms |

(the mission's last 40 s; CPU as % of one core [measured]).

- The GPU side is light: the executor spends 5.1 ms of a 33.3 ms frame
  busy (draw 2.8-3.1, decode 1.7-1.9, present 0.3-0.4) and the rest
  holding for the vblank [measured: the [PACING] executor line].
- The game had the runtime's stock pacing, spin: its main thread
  busy-waited through every frame's wait. The runtime does not offer that
  mode on Linux or Windows (xbox_settings.c, k_stock_hidden) but uses it
  when a port names no pacing. DS2 now defaults to Balanced (main.c, unless
  enhance.toml names present.pacing; config/enhance.toml has it): a
  quarter of a core less, as steady. The game's own timing is untouched
  (it steps once a frame at 30 fps either way).
- With Balanced the nv2a-ack thread went from 16% to 39% of a core: the
  8 ms spin lead, by design (8 of 33.3 ms is 24%); RECOMP_FAST_KICK=0 did
  not lower it and sent the main thread back to 98% (run perf3). Auto and
  Battery saver (no lead) also left the main thread at 98%, 130-134% in
  all (perf4, perf5): Balanced stays.
- Where the main thread spun [verified: src/game/prof.c, DS2_PROFILE=<file>,
  a SIGPROF timer on the thread's CPU time, tools/perf/prof.py by function;
  run prof1]: 90% in sub_0012FA70, D3D's KickOff, at 0x0012FAB0
  (`test [reg 0x100410], 0x10000 / jne self`: the kick bit, which the
  runtime's ack thread clears once the executor has done the work, its
  vblank hold included). sub_0012FB20 has the same loop at 0x0012FB60.
  Both are lowered now (config/spin_waits.json, regen.sh passes it as
  --spin-waits): the back edge calls RECOMP_SPIN_WAIT, which sleeps until
  the runtime's xbox_SpinWake, and the ack calls that (kick_cleared). The
  regenerated code differs from before in those two lines only.
- Run perf6 (Balanced, the waits lowered): process 67% of a core, the main
  thread 10%, nv2a-ack 40%; no [ICALL] failures or stubs, the mission's
  TIME 10.00 s per 10 s, the HUD there. Flips p5-p95 33.3 ms, but 1 and 2
  late in the mission's windows (max 50 ms) where perf2 had none, with the
  machine at load 11 from other programs and
  the vblank timer's own gaps 14.5-18.9 ms (16.5-16.9 quiet): a woken
  thread waits for a core where a spinning one does not. The quiet A/B
  settled it (below): no late flips either way.

| run | change | process | guest-main | nv2a-ack | late flips (mission windows) |
|---|---|---|---|---|---|
| perf3 | Balanced, RECOMP_FAST_KICK=0 | 151% | 98% | 37% | 0 |
| perf4 | lead auto | 134% | 98% | 20% | 0 |
| perf5 | lead 0 (Battery saver) | 130% | 98% | 15% | 0 |
| perf6 | Balanced, KickOff waits lowered | 67% | 10% | 40% | 1, 2 |

- The A/B on a quiet machine (2026-10-28, work/perf/ab.sh, run ab1: the old
  code as work/ab/bin-old, the two interleaved, both Balanced; ~96% idle
  before, load 4-6 from the desktop) [measured]:

| run | code | process | guest-main | late flips | max interval (mission windows) |
|---|---|---|---|---|---|
| new1 | waits lowered | 63% | 9% | 0 | 33.4 / 34.4 ms |
| old1 | spinning | 146% | 95% | 0 | 33.4 / 33.6 ms |
| new2 | waits lowered | 65% | 10% | 0 | 34.4 / 34.4 ms |
| old2 | spinning | 150% | 98% | 0 | 33.4 / 33.4 ms |

  No [ICALL] failures, stubs or crashes in any. The lowered waits cost up
  to 1 ms of worst-case flip jitter and no late flip; they save most of a
  core. So it ships. Correction: the old code under Balanced is ~148%
  here, not perf2's 104%; perf2's main thread at 49% was the load of that
  hour, not Balanced. Balanced is still the default (it does not cost more
  than spin, and with the waits lowered it is what lets the main thread
  sleep: RECOMP_SPIN_WAIT only sleeps under sleep pacing).
- Next: the windowed baseline at the defaults (frame time, CPU, GPU, load
  times), and nv2a-ack's 40% (the spin lead), which is the toolkit's.
- The native renderer's first try (antbox e983301, RECOMP_PB_BACKEND=native,
  headless, the mission 1 drive, 2026-10-30, work/runs/gate_e983301): no
  failures, stubs or crash; menus, cutscene, mission and HUD draw; 0 skipped
  draws. The front buffer 0x004C6000 is remade 1280x720 -> 16x16 -> 1216x684
  -> 1280x720, no visible fault. Live frames differ by the moment (about
  0.2 s apart at f009). A capture of the drive on the old path, replayed
  through both by the toolkit's owner [its report, on an NVIDIA card]: 38 of 46
  frames identical, the other 8 within 3 levels of 255, cause not looked for.
  Not tried: windowed, the overlay, timing.

## 80-bit reals in the C library, 2026-10-30

antbox fe54d6c lifts `fld tbyte` and `fstp tbyte` properly (found by MA2: they
read the low 4 bytes as a float, and fstp was dropped). In DS2 that changes 14
functions, all in the CRT's maths at 0x0011A9C8-0x00122508 [verified: regen
diff against the pin, work/runs/gate_fe54d6c]. Counted through the mission 1
drive [measured, temporary counters in a scratch copy]: 2 of the 29 lines run,
2048-4095 times each in 100 s, both for one event:

- `acos` (sub_0011B671) with |x| > 1 loads its NaN from 0x3B904C, and
  sub_0011D8AB, which only raises FPU flags. The old code read those as 0.
- The one caller is D3DXQuaternionToAxisAngle (0x0013F43F: angle = 2 acos(w))
  from sub_00094820 at 0x00094C38, with w 1.0000001-1.0000008.
- That game code checks the angle at once: `_isnan` (0x0011A2EA), `_fpclass`
  (0x0011A318), and angle < 0.0034907 all go to 0x00094DA4, which replaces the
  angle. Old 0 and new NaN take that same branch [verified: disassembly and the
  lifted comparisons; fcompi and comiss send a NaN past the +-360 fold]. So no
  behaviour changes on this route, and frames and the mission's TIME are as
  before.
- asin(+-1), where pi/2 at 0x3B9056 read as 7.9e-19 (MA2's symptom is the same
  constant's kind), did not run on this route. Other routes are not measured.

## Rumble, 2026-10-25

The controller vibrated nearly the whole time the game was open. Two
causes.

- The runtime gave the pad the wrong bytes. The game's rumble manager
  (object 0x0041CD30: set sub_0002F520, the per-frame tick sub_0002F770,
  stop all sub_0002F8B0; XInputSetState is 0x001B945F in XPP) keeps its
  feedback blocks in the heap. XPP hands the OHCI the buffer's physical
  address, which for heap memory is its own number; those page numbers
  are also live contiguous pages, and the OHCI model's bus_resolve sent
  them to the contiguous window, so the pad played whatever the window held
  there: the first mission load's stop came out as the left motor at 90 %
  (59419), and nothing replaced it. The OHCI now asks the kernel's record
  of what each physical page was translated from (xbox_PhysicalToGuest,
  as the APU already did for the same collision in Burnout 3's sound
  banks) before its own rules. A drive with RECOMP_TRACE=input_diag then
  showed 54 changes in mission 1: knocks of 30 to 55 % on the big motor,
  collisions at full on the small one, each back to 0.
- The game hums the controller with the engine. The car's per-frame
  vibration (sub_00088F70) holds both motors at its engine object's load
  (sub_000809F0: 0.5 while changing gear, else 0..1 with the throttle):
  right = load x 0.35, left = (load - 0.5) x 0.7, so up to a third of full
  on both whenever you accelerate; collisions ([car+0x948] over 20, the
  flag at +0x950), rough road (a sine on the surface types 0x200/0x400)
  and the rev limiter are 3-frame pulses from the same function. On a
  modern pad the hold is a steady vibration. game.engine_vibration (off by
  default; the settings menu's Input page) keeps it: off, a hook on
  sub_0002F520 makes the held call (returning to 0x00089317) set 0, and
  the pulses end on 0.
- The game's own Options > Controller Setting > Vibration switches the
  manager off ([0x0041CD30+0x2DC]).

## Gamma ramp, 2026-10-26

The game sets a gamma ramp at start, and in 720p it is not the identity: 64
-> 59, 128 -> 130, 192 -> 199 on all three channels [verified: a run's
[DISPLAY] line]. sub_000316F0 builds it with sub_0002A6F0 (v = ((x·a −
0.5)·b + 0.5), optional sin, then pow(v, 1/c); a = b = c = 1.0 here, the
identity) and, when the game's mode number [0x00415784] is 4 or 5, passes
each step through 0x00012100, a mild S-curve. The mode number comes from
bit 2 of [0x001D2A9C], which sub_00030E00 sets for its 1280x720 mode
(bit 1 is 1920x1080) [verified: disassembly]. So the console's 720p
picture had slightly deeper shadows and brighter highlights than its
framebuffer, which the runtime showed as it was.

- The display part came from the Wreckless port (fork 399c282, here f136229):
  xbox_gamma.h, and a post pass in the Vulkan presenter (a 256x1 lookup,
  SDR frames, before the overlay and AA); an identity ramp is no pass.
- D3D's SetGammaRamp (0x0012B4D0) writes the DAC at once only with its
  flag 2; otherwise it marks the ramp for the vblank code at 0x00134818,
  which never wrote it here [verified: no writer call in 30 s while the
  ramp was set]. So the hook is on SetGammaRamp itself (the ramp the game
  asked for), with the writer (0x00132045) hooked as well; xbox_gamma keeps
  one copy of a repeated ramp.
- Checked in a window capture against the frame dump of the same moment:
  the title logo's gold comes out 3 to 9 levels brighter between 124 and
  188 [verified: medians over ~66,000 logo pixels]. Frame dumps
  (RECOMP_FB_DUMP) are the framebuffer before the ramp.

## Clean starts for immediates, 2026-10-26

The Wreckless port's disassembler change (fork 16eae77, here 3364bf7): an
immediate's target is taken for a function only where nothing runs into
it (a ret, jmp or int3 before it, padding, or a switch table's end). A
regeneration with --disasm against a snapshot (work/cmp_funcs.py): four
starts gone, none new, no generated body changed.

- Three were data the old pass took for code, never called [verified:
  disassembly]: 0x00176F2C (an XACT engine variable, read and written by
  mov at 0x001763E9 and 0x001763FB), 0x001B29E8 (one of three WMA decoder
  table pointers stored at 0x001A436F onwards: 0x001B25E8, 0x001B29E8,
  0x001B2BE8), 0x0043AB38 (in the DOLBY section, the DSP's program, stored
  by DirectSound at 0x0016BBBA). The stub 0x001B2A99, inside that table,
  went with it (16 stubs -> 15).
- One was a real function: 0x00142F99, libjpeg's error manager callback in
  D3DX, installed at 0x0014380F (mov [eax+0xC]). It follows error_exit
  (0x00142F7D), which ends in a call to longjmp (0x0011C884) and a dead
  pop esi, so the rule cannot see that nothing runs into it. Seeded
  (config/seed_functions.json), as the Wreckless port did for its own.
- A drive into mission 1 after it: no failed indirect call, no stub hit.

## 60 fps, 2026-10-24

The game asks for every second vblank (30 fps) and steps its logic once
per frame. With RECOMP_FLIP_PACING=0 it flipped about 200 times a second
(the window still showed ~27) and mission 1's time limit fell 111 units in
well under a minute of wall time. So no 60 fps option: it would need the
timestep found and halved, and every frame-counted timer with it.

Open:
- Outdoor night driving, the other missions (clearing mission 1 means
  catching five cars: not something to script blind).
