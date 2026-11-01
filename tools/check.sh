#!/usr/bin/env bash
# tools/check.sh [--configure] [--toolkit DIR]: the project's check (NOTES.md,
# "Toolkit: antbox"): build, then the headless drive into mission 1 (100 s, a
# dump every 600 flips) and a summary. The build goes to build/check, the run
# to work/runs/chk, the build's output to work/check-build.log. Needs the
# generated code (regen.sh) and the game folder; saves go to work/save. Build
# libraries come from the system, or from work/sysroot if that folder exists.
# The toolkit: --toolkit DIR, else the one build/check was configured with,
# else ../xboxrecomp-takedawn or ../antbox.
set -u
cd "$(dirname "$0")/.."
root=$PWD
configure=0
toolkit=$root/../xboxrecomp-takedawn
[[ -d $toolkit ]] || toolkit=$root/../antbox
while [[ $# -gt 0 ]]; do
    case $1 in
        --configure) configure=1; shift ;;
        --toolkit) toolkit=$(cd "$2" && pwd); configure=1; shift 2 ;;
        *) echo "usage: tools/check.sh [--configure] [--toolkit DIR]" >&2; exit 2 ;;
    esac
done
mkdir -p work
sys=$root/work/sysroot/usr
sysroot=()
if [[ -d $sys ]]; then
    sysroot=("-DCMAKE_C_FLAGS=-isystem $sys/include/x86_64-linux-gnu"
             "-DCMAKE_CXX_FLAGS=-isystem $sys/include/x86_64-linux-gnu"
             "-DCMAKE_PREFIX_PATH=$sys" "-DSDL2_DIR=$sys/lib/x86_64-linux-gnu/cmake/SDL2")
fi
if (( configure )) || [[ ! -f build/check/CMakeCache.txt ]]; then
    cmake -S . -B build/check -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
        -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld "${sysroot[@]}" \
        -DDS2_GEN_SUBDIR=gen-tkd -DXBOXRECOMP_DIR="$toolkit" -DXBOXRECOMP_OVERLAY=ON \
        > work/check-configure.log 2>&1 || { echo "configure failed: work/check-configure.log"; exit 1; }
fi
cmake --build build/check -j"$(nproc)" > work/check-build.log 2>&1 || { echo "build failed: work/check-build.log"; exit 1; }
echo "build: ok ($(grep -m1 -o 'XBOXRECOMP_DIR:PATH=.*' build/check/CMakeCache.txt | cut -d= -f2))"

out=work/runs/chk; rm -rf "$out"; mkdir -p "$out"
RECOMP_WATCHDOG_SECS=160 RECOMP_STUB_LOG=1 RECOMP_KERNEL_LOG_BUDGET=60 \
RECOMP_FB_DUMP=$out/f RECOMP_FB_DUMP_FLIPS=600 \
RECOMP_PAD_SCRIPT="7000:start,9000:start,11000:start,13000:a,16500:a,20000:a,22500:a,25000:a,27500:a,29500:a,64000:rt:9000,74000:rt+lleft:1500,76000:rt:7000,84000:rt+lright:1500,86000:rt:6000" \
    timeout -s INT 100 bin/doublesteal2 --headless --save-dir work/save > "$out/run.log" 2>&1
rc=$?
dumps=$(ls "$out" | grep -c '\.bmp$')
fails=$(grep -c '\[ICALL\] Failed' "$out/run.log")
stubs=$(grep -c '\[STUB\]' "$out/run.log")
crash=$(grep -c -i 'crash report\|SIGSEGV\|fatal' "$out/run.log")
echo "run: exit $rc (124/130 = stopped at 100 s), dumps $dumps, icall failures $fails, stub hits $stubs, crash lines $crash"
grep -m3 '\[APU-IRQ\]' "$out/run.log" | tail -1
ls -l --time-style=+%s "$out"/*.bmp 2>/dev/null | awk '{print $6, $7}' > "$out/dump_times.txt"
echo "dump times: $out/dump_times.txt"
