#!/usr/bin/env bash
# package.sh [DEST] - the built game as a folder to run from.
#
# DEST (default dist/doublesteal2) gets the binary, the default settings
# (enhance.toml, only when DEST has none: a player's own is kept), the
# README, the licenses and a VERSION file naming both trees' commits. The
# game's files are not part of it: run it with --game-dir pointing at your
# extracted disc, and --save-dir where saves and the cache should go
# (~/.local/share/doublesteal2 by default).
#
# DS2_BIN names another build's binary (deck-build.sh's build/deck/bin for
# SteamOS). It needs the libraries the build links (SDL2, libepoxy,
# OpenSSL; Vulkan at run time), all of them ordinary system packages.
set -euo pipefail

DS2="$(cd "$(dirname "$0")" && pwd)"
RT="${XBOXRECOMP_DIR:-$DS2/../xboxrecomp-takedawn}"
[ -d "$RT" ] || RT="$DS2/../antbox"
DEST="${1:-$DS2/dist/doublesteal2}"

BIN="${DS2_BIN:-$DS2/bin/doublesteal2}"
[[ "$BIN" = /* ]] || BIN="$DS2/$BIN"
if [[ ! -x "$BIN" ]]; then
    echo "no $BIN: build first (README.md, Building)" >&2
    exit 1
fi

mkdir -p "$DEST"
cp "$BIN" "$DEST/doublesteal2.new"
mv -f "$DEST/doublesteal2.new" "$DEST/doublesteal2"   # a running copy keeps its file
[[ -f "$DEST/enhance.toml" ]] || cp "$DS2/config/enhance.toml" "$DEST/enhance.toml"
cp "$DS2/README.md" "$DS2/LICENSE" "$DS2/LICENSE.wreckless" "$DEST/"
{
    echo "doublesteal2-recomp $(git -C "$DS2" rev-parse --short HEAD)$(git -C "$DS2" diff --quiet || echo '+changes')"
    echo "runtime $(git -C "$RT" rev-parse --abbrev-ref HEAD) $(git -C "$RT" rev-parse --short HEAD)$(git -C "$RT" diff --quiet || echo '+changes')"
    echo "built $(date -u -r "$BIN" +%Y-%m-%dT%H:%MZ) ($(basename "$(dirname "$(dirname "$BIN")")")), needs $(objdump -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)"
} > "$DEST/VERSION"
echo "packaged in $DEST:"
cat "$DEST/VERSION"
