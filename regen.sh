#!/usr/bin/env bash
# Regenerate the recompiled C from default.xbe with the xboxrecomp toolkit.
#
# Same shape as sp00nznet/wreckless's regen.sh (the first Double S.T.E.A.L.),
# run on Linux with the project's .venv instead of the Windows py launcher.
#
# Order matters: disasm rewrites functions.json from scratch, so any naming
# pass has to run after it, and recomp last.
#
#   xbe_parser    section layout -> build/xr/analysis.json
#   disasm        every code section, never --text-only: without the XDK
#                 sections (D3D, DSOUND, XMV, XPP...) there are no function
#                 boundaries there and each function is glued to the next.
#   xdk names     the toolkit's XDK name list, by signature
#   func_id       library-function identification (CRT, XDK)
#   abi_analysis  calling convention / params / return type
#   recomp        emits the C into src/game/recomp/gen-tkd (gitignored)
#
# Usage: ./regen.sh [--disasm]     (--disasm forces the disassembly again)
#
# The toolkit is antbox, beside this checkout (../xboxrecomp-takedawn if that
# is there, else ../antbox; XBOXRECOMP_DIR names another). It writes build/xr-tkd
# and recomp/gen-tkd. Generated code only builds against the toolkit that
# generated it.

set -uo pipefail

DS2="$(cd "$(dirname "$0")" && pwd)"
RECOMP="${XBOXRECOMP_DIR:-$DS2/../xboxrecomp-takedawn}"
[[ -d "$RECOMP" ]] || RECOMP="$DS2/../antbox"
if [[ ! -d "$RECOMP/tools/recomp" ]]; then
    echo "no toolkit at $RECOMP -- see README.md, Building"
    exit 1
fi
SUFFIX=-tkd
PY="$DS2/.venv/bin/python3"
XBE="$DS2/game/Double-S.T.E.A.L - The Second Clash/default.xbe"
OUT="$DS2/build/xr$SUFFIX"
GEN="$DS2/src/game/recomp/gen$SUFFIX"

if [[ ! -f "$XBE" ]]; then
    echo "missing $XBE -- extract it from your own disc image"
    exit 1
fi
mkdir -p "$OUT"

# Seeds: entry points nothing in the code calls, so the detector cannot find
# them (Wreckless needed its game thread's start routine here). Add them one
# at a time and verify each: an address that is not a function start
# truncates the function containing it.
#
# config/seed_functions.json is the hand list (each entry says how it was
# found). config/seeds_upstream_starts.json is generated: function starts
# upstream v0.16 finds and the takedawn branch misses, kept to those right
# after a ret or padding (clean starts); harmless for upstream, which has them.
SEEDS=()
for f in seed_functions.json seeds_upstream_starts.json; do
    [[ -f "$DS2/config/$f" ]] && SEEDS+=(--seed-functions "$DS2/config/$f")
done

if [[ "${1:-}" == "--disasm" || ! -f "$OUT/disasm/functions.json" ]]; then
    echo "==> xbe_parser"
    (cd "$RECOMP" && "$PY" -m tools.xbe_parser "$XBE" \
        --json "$OUT/analysis.json" --quiet) || exit 1

    echo "==> disasm"
    (cd "$RECOMP" && "$PY" -m tools.disasm "$XBE" \
        --analysis-json "$OUT/analysis.json" "${SEEDS[@]}" \
        -o "$OUT/disasm" -v) > "$OUT/disasm.log" 2>&1 || { tail -20 "$OUT/disasm.log"; exit 1; }
    tr '\r' '\n' < "$OUT/disasm.log" | grep -E "Total functions|Reachable|Seeded" || true

    echo "==> xdk names"
    (cd "$RECOMP" && "$PY" tools/ghidra_naming/merge_names.py --apply \
        --names-json     tools/ghidra_naming/xdk_names.json \
        --functions-json "$OUT/disasm/functions.json") | tail -1
fi

echo "==> func_id"
(cd "$RECOMP" && "$PY" -m tools.func_id "$XBE" \
    --functions "$OUT/disasm/functions.json" \
    --strings   "$OUT/disasm/strings.json" \
    --xrefs     "$OUT/disasm/xrefs.json" \
    -o "$OUT/func_id") > "$OUT/func_id.log" 2>&1 || { tail -20 "$OUT/func_id.log"; exit 1; }
tail -1 "$OUT/func_id.log"

# ABI recovery. Without it every function is lifted as cdecl / 0 params /
# int-or-void, which is a guess the generated signatures then carry.
echo "==> abi_analysis"
(cd "$RECOMP" && "$PY" -m tools.abi_analysis "$XBE" \
    --functions  "$OUT/disasm/functions.json" \
    --identified "$OUT/func_id/identified_functions.json" \
    --output-dir "$OUT/abi") > "$OUT/abi.log" 2>&1 || { tail -20 "$OUT/abi.log"; exit 1; }
tail -1 "$OUT/abi.log"

# --split 250: the toolkit's advice; 1000 functions per file does not build
# in 15 GB.
# Guest busy-waits lowered to sleeping waits (tools.recomp --spin-waits):
# the D3D KickOff waits, NOTES.md "Overhead".
SPIN=()
[[ -f "$DS2/config/spin_waits.json" ]] && SPIN=(--spin-waits "$DS2/config/spin_waits.json")
echo "==> recomp"
(cd "$RECOMP" && "$PY" -m tools.recomp "$XBE" --all --split 250 \
    --disasm-dir  "$OUT/disasm" \
    --func-id-dir "$OUT/func_id" \
    --abi-dir     "$OUT/abi" \
    --gen-dir     "$GEN" \
    --manual-functions "$DS2/config/manual_functions.json" \
    --exclude-manual "$DS2/src/game/recomp/recomp_hooks.c" \
    "${SPIN[@]}" \
    -o "$OUT/recomp") > "$OUT/recomp.log" 2>&1 || { tail -30 "$OUT/recomp.log"; exit 1; }
grep -E "unresolved|Complete|functions|failed" "$OUT/recomp.log" | tail -12

echo "==> done"
