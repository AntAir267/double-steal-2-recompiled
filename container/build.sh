#!/usr/bin/env bash
# Runs inside the container (Containerfile; deck-build.sh starts it): configures
# and builds build/deck with the generated code already in src/game/recomp/gen-tkd
# (regen.sh runs on the host). The project and the runtime are mounted at
# their host paths, so the trees' own relative paths hold. The binary goes to
# build/deck/bin; package.sh DS2_BIN=... packages it.
set -euo pipefail
DS2="$(cd "$(dirname "$0")/.." && pwd)"
RT="${XBOXRECOMP_DIR:-$DS2/../xboxrecomp-takedawn}"
[ -d "$RT" ] || RT="$DS2/../antbox"
B="$DS2/build/deck"
# Every time, so an option added since (the settings overlay) reaches a tree
# configured before it; cheap once configured.
cmake -S "$DS2" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
      -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld \
      -DXBOXRECOMP_DIR="$RT" -DDS2_GEN_SUBDIR=gen-tkd -DXBOXRECOMP_OVERLAY=ON \
      -DDS2_BIN_DIR="$B/bin" > "$B.configure.log" 2>&1 \
    || { tail -30 "$B.configure.log"; exit 1; }
ninja -C "$B" > "$B.build.log" 2>&1 || { grep -E "error|FAILED" "$B.build.log" | head -20; exit 1; }
echo "glibc needed: $(objdump -T "$B/bin/doublesteal2" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)"
echo "libstdc++ needed: $(objdump -T "$B/bin/doublesteal2" | grep -o 'GLIBCXX_[0-9.]*' | sort -Vu | tail -1)"
