#!/usr/bin/env bash
# deck-build.sh - a build that also runs on SteamOS (glibc 2.41): the game
# compiled inside an Ubuntu 24.04 container (container/Containerfile) into
# build/deck. Needs podman; the image is made the first time.
#   ./deck-build.sh && DS2_BIN=build/deck/bin/doublesteal2 ./package.sh DEST
set -euo pipefail
DS2="$(cd "$(dirname "$0")" && pwd)"
T="${XBOXRECOMP_DIR:-$DS2/../xboxrecomp-takedawn}"
[ -d "$T" ] || T="$DS2/../antbox"
RT="$(cd "$T" && pwd)"
IMAGE=localhost/ds2-build:24.04
podman image exists "$IMAGE" || podman build -t "$IMAGE" -f "$DS2/container/Containerfile" "$DS2/container"
mkdir -p "$DS2/build"
podman run --rm -v "$DS2:$DS2" -v "$RT:$RT" -w "$DS2" -e XBOXRECOMP_DIR="$RT" \
    "$IMAGE" bash "$DS2/container/build.sh"
