#!/bin/bash
# tools/release/linux.sh OUT_DIR: the Linux download (an AppImage), with no part of the game in it.
#
# It needs the translated code once, as every build does (./regen.sh on your own copy): the runtime's
# hooks are compiled against the declarations the translator writes, and the players' translation must
# come to the same thing (GEN_SHA256).
#
# The runtime is the SteamOS build's (./deck-build.sh, Ubuntu 24.04's Clang and static libraries), so
# the download needs glibc 2.38. The players' Zig compiles their translation for the same glibc.
#
# third_party/ holds what the download carries from elsewhere, each as its project releases it:
# zig-x86_64-linux-0.17.0, python-standalone (with bin/python3.14), pack-venv (capstone in
# lib/python3.14/site-packages), sdl3-rel + SDL3-*/ (the launcher: static, for glibc 2.31),
# appimagetool-x86_64.AppImage and appimage-runtime-x86_64. The game's own SDL2 is SDL2-2.32.10/ built
# in the same container into sdl2-rel/ (SDL_SHARED=ON, SDL_STATIC=OFF, SDL_TEST=OFF, the X11, Wayland,
# PulseAudio, ALSA and PipeWire options _SHARED=ON, so it needs only libc and libm to load and finds
# the rest at run time; SDL_SNDIO, SDL_JACK, SDL_LIBSAMPLERATE, SDL_KMSDRM off).
set -euo pipefail
export LC_ALL=C
cd "$(dirname "$0")/../.."
ROOT=$PWD
OUT=$(realpath -m "${1:?usage: linux.sh OUT_DIR}")
TP=$ROOT/third_party
ZDIR=$(readlink -f "$TP/zig-x86_64-linux-0.17.0")
Z=$ZDIR/zig
SDL3=$(readlink -f "$TP/sdl3-rel")
PYS=$(readlink -f "$TP/python-standalone")/python
CAP=$(readlink -f "$TP/pack-venv")/lib/python3.14/site-packages
GEN=src/game/recomp/gen-tkd
XR=$(readlink -f "${XBOXRECOMP_DIR:-$ROOT/../xboxrecomp-takedawn}")
[ -d "$XR" ] || XR=$(readlink -f "$ROOT/../antbox")
REL=$(cat tools/release/VERSION)
NAME=double-steal-2-recompiled
IMAGE=localhost/ds2-build:24.04
for need in "$Z" "$SDL3/lib/libSDL3.a" "$PYS/bin/python3.14" "$CAP/capstone" "$TP/appimagetool-x86_64.AppImage" \
            "$GEN/recomp_funcs.h"; do
    [ -e "$need" ] || { echo "missing $need"; exit 1; }
done
mkdir -p "$OUT"

echo "== the runtime, in the SteamOS build's container, at folder names that say nothing about this computer"
podman image exists "$IMAGE" || podman build -t "$IMAGE" -f container/Containerfile container
podman run --rm -v "$ROOT:/build/ds2" -v "$XR:/build/antbox" -w /build/ds2 -e XBOXRECOMP_DIR=/build/antbox \
    -e DS2_BUILD_DIR=/build/ds2/build/rel "$IMAGE" bash /build/ds2/container/build.sh
D=build/rel/src/game/CMakeFiles/doublesteal2.dir
RT=build/rel/xboxrecomp/src

A=$OUT/AppDir
B=$A/usr/lib/$NAME
rm -rf "$A"
mkdir -p "$B/lib/game" "$B/lib/stubs" "$B/include" "$B/tools/config" "$B/tools/toolkit" "$A/usr/bin"

echo "== the prebuilt parts"
for o in main settings options_row skin prof recomp/recomp_manual recomp/recomp_native recomp/recomp_hooks; do
    cp "$D/$o.c.o" "$B/lib/game/$(basename "$o").o"
done
GROUP=()
for a in audio/libxbox_dsound nv2a/libxbox_nv2a kernel/libxbox_kernel video/libxbox_video enhance/libxbox_enhance \
         overlay/libxbox_overlay usb/libxbox_usb d3d/libxbox_d3d8 input/libxbox_input apu/libxbox_apu \
         install/libxbox_install soundtrack/libxbox_soundtrack platform/libplatform platform/librecomp_env; do
    cp "$RT/$a.a" "$B/lib/"
    GROUP+=("$(basename "$a").a")
done
strip --strip-debug "$B"/lib/*.a "$B"/lib/game/*.o
cp "$XR/src/platform/xr_log.h" "$B/include/"

echo "== the static libraries, and the licences that go with them, from the build image"
L=$B/licenses
mkdir -p "$L"
podman run --rm -v "$B/lib:/o" -v "$L:/l" "$IMAGE" sh -c '
    cd /usr/lib/x86_64-linux-gnu
    cp libcrypto.a libglslang.a libglslang-default-resource-limits.a libSPIRV-Tools-opt.a libSPIRV-Tools.a /o/
    cp -L libepoxy.so.0 /o/
    cp /usr/lib/gcc/x86_64-linux-gnu/13/libstdc++.a /usr/lib/gcc/x86_64-linux-gnu/13/libgcc_eh.a /o/
    nm -D --defined-only libSDL2-2.0.so.0 | awk "\$2 ~ /^[TWi]\$/ {print \$3}" | sed "s/@.*//" | sort -u > /o/stubs/SDL2.names
    for x in openssl:libssl-dev glslang:glslang-dev spirv-tools:spirv-tools libepoxy:libepoxy-dev libstdc++:libstdc++-13-dev; do
        cp "/usr/share/doc/${x#*:}/copyright" "/l/${x%%:*}-copyright.txt"
    done'
strip --strip-debug "$B"/lib/lib*.a
sed 's/.*/void &(void) {}/' "$B/lib/stubs/SDL2.names" > build/stub_SDL2.c
"$Z" cc -target x86_64-linux-gnu.2.38 -shared -fPIC -o "$B/lib/stubs/libSDL2.so" -Wl,-soname,libSDL2-2.0.so.0 build/stub_SDL2.c
rm "$B/lib/stubs/SDL2.names"
ln -s ../libepoxy.so.0 "$B/lib/stubs/libepoxy.so"
if [ -f "$TP/sdl2-rel/lib/libSDL2-2.0.so.0" ]; then
    cp -L "$TP/sdl2-rel/lib/libSDL2-2.0.so.0" "$B/lib/"
    cp "$TP"/SDL2-*/LICENSE.txt "$L/SDL2-LICENSE.txt"
else
    echo "   (no third_party/sdl2-rel: the download will use the player's SDL2)"
fi
{
    for o in "$B"/lib/game/*.o; do echo "\$LIB/game/$(basename "$o")"; done
    echo "-Wl,--start-group"
    for a in "${GROUP[@]}"; do echo "\$LIB/$a"; done
    echo "-Wl,--end-group"
    for a in libcrypto libglslang libglslang-default-resource-limits libSPIRV-Tools-opt libSPIRV-Tools libstdc++ libgcc_eh; do
        echo "\$LIB/$a.a"
    done
    echo "-L"
    echo "\$LIB/stubs"
    printf '%s\n' -lSDL2 -lepoxy -ldl -lpthread -lm "-Wl,--allow-shlib-undefined" '-Wl,-rpath,$ORIGIN/lib'
} > "$B/lib/LINK"

echo "== the translator and installer on a standalone Python (glibc 2.17) with Capstone"
mkdir -p "$B/python/bin" "$B/python/lib"
cp -a "$PYS/bin/python3.14" "$B/python/bin/"
tar -C "$PYS/lib" --exclude=test --exclude=idlelib --exclude=tkinter --exclude=turtledemo --exclude=ensurepip \
    --exclude=pydoc_data --exclude='config-3.14-*' --exclude='site-packages/*' --exclude='_tkinter.*' \
    --exclude='_dbm.*' -cf - python3.14 | tar -C "$B/python/lib" -xf -
cp -r "$CAP/capstone" "$B/python/lib/python3.14/site-packages/"
find "$B/python/lib/python3.14/site-packages/capstone" -name __pycache__ -prune -exec rm -r {} +
git -C "$XR" archive HEAD tools templates | tar -x -C "$B/tools/toolkit" --exclude=tools/nvr_replay --exclude=tools/conformance
cp tools/install.py tools/translate.py tools/disc.manifest "$B/tools/"
cp config/seed_functions.json config/seeds_upstream_starts.json config/manual_functions.json config/spin_waits.json \
   config/enhance.toml src/game/recomp/recomp_hooks.c "$B/tools/config/"
"$B/python/bin/python3.14" -E -s -B tools/translate.py --hash "$GEN" > "$B/tools/GEN_SHA256"
echo "doublesteal2 $(date -u +%Y-%m-%d) $(git rev-parse --short HEAD) toolkit $(git -C "$XR" rev-parse --short HEAD)|$REL" > "$B/VERSION"

echo "== the launcher"
"$Z" cc -target x86_64-linux-gnu.2.31 -O2 -fPIE -pie -I"$SDL3/include" -idirafter /usr/include \
    -idirafter /usr/include/x86_64-linux-gnu -o "$A/usr/bin/$NAME" launcher/launcher.c "$SDL3/lib/libSDL3.a" -lm -ldl -pthread

# (SDL3's prebuilt library and Zig's output carry the folder they were built in, for messages only)
PARENT=$(dirname "$ROOT")
sed -i "s|$PARENT|$(printf '%s' "$PARENT" | sed 's/[^/]/x/g')|g" "$A/usr/bin/$NAME" "$B/lib/stubs/libSDL2.so"

echo "== Zig"
cp -a "$ZDIR" "$B/zig"
rm -rf "$B/zig/doc"

echo "== the licences (every part the download carries)"
cp LICENSE LICENSE.wreckless "$L/"
cp "$XR/LICENSE" "$L/antbox-LICENSE"
cp "$SDL3"/../SDL3-*/LICENSE.txt "$L/SDL3-LICENSE.txt"
cp "$ZDIR/LICENSE" "$L/Zig-LICENSE.txt"
for x in libcxx libcxxabi libunwind; do cp "$ZDIR/lib/$x/LICENSE.TXT" "$L/LLVM-$x-LICENSE.txt"; done
cp "$CAP"/capstone-*.dist-info/LICENSE.TXT "$L/Capstone-LICENSE.txt"
cp "$PYS/lib/python3.14/LICENSE.txt" "$L/Python-LICENSE.txt"
cp /usr/share/doc/libpython3.14/copyright "$L/Python-copyright-details.txt"
cp tools/release/python-builtins-NOTICE.txt "$L/Python-builtins-NOTICE.txt"
for x in expat:libexpat1 zlib:zlib1g bzip2:libbz2-1.0 xz:liblzma5 zstd:libzstd1 sqlite:libsqlite3-0 \
         libedit:libedit2 ncurses:libncurses6 util-linux:libuuid1; do
    cp "/usr/share/doc/${x#*:}/copyright" "$L/${x%%:*}-copyright.txt"
done

echo "== nothing of the game, no folder or address of this computer's"
if grep -rlF -e "$HOME" -e "$(git config user.email)" "$B/tools" "$B/include" "$B/lib" "$L" "$A/usr/bin"; then
    echo "refused: the files above name something private"; exit 1
fi
if find "$A" -type f \( -iname '*.xbe' -o -iname '*.xpr' -o -iname '*.pk' -o -iname '*.xwb' -o -iname '*.xmv' \) | grep .; then
    echo "refused: game files in the download"; exit 1
fi

echo "== AppImage"
cat > "$A/AppRun" <<EOF
#!/bin/sh
HERE=\$(dirname "\$(readlink -f "\$0")")
exec "\$HERE/usr/bin/$NAME" "\$@"
EOF
chmod +x "$A/AppRun"
cat > "$A/$NAME.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Double-S.T.E.A.L: The Second Clash
Comment=The 2004 Xbox racer, recompiled from your own copy
Exec=$NAME
Icon=$NAME
Categories=Game;
EOF
cp assets/icon.png "$A/$NAME.png"
RUNTIME=()
[ -f "$TP/appimage-runtime-x86_64" ] && RUNTIME=(--runtime-file "$TP/appimage-runtime-x86_64")
ARCH=x86_64 "$TP/appimagetool-x86_64.AppImage" --no-appstream "${RUNTIME[@]}" "$A" \
    "$OUT/Double-S.T.E.A.L_Recompiled-$REL-x86_64.AppImage" >/dev/null
ls -la "$OUT/Double-S.T.E.A.L_Recompiled-$REL-x86_64.AppImage"
