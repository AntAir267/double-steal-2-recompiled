#!/usr/bin/env python3
"""translate.py XBE OUT: default.xbe to C in OUT/gen, the same steps in the same order as regen.sh, for
the download's first run. The toolkit's tools are in toolkit/ beside this file and the title's lists
in config/. A file named GEN_SHA256 beside this file holds what the translation must come to: the
download's runtime was built against those very declarations. Exit codes: 0 done, 2 it came out
different, 1 a step failed. Prints each step's name; a failing step's own output goes to OUT/*.log."""
import glob
import hashlib
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLKIT = os.path.join(HERE, "toolkit")
CONFIG = os.path.join(HERE, "config")


def gen_hash(gen):
    h = hashlib.sha256()
    for p in sorted(glob.glob(os.path.join(gen, "*"))):
        with open(p, "rb") as f:
            h.update(f.read())
    return h.hexdigest()


def step(name, out, *args):
    print("==>", name, flush=True)
    log = os.path.join(out, name + ".log")
    with open(log, "w") as f:
        code = subprocess.call([sys.executable, "-E", "-s", "-B", *args], cwd=TOOLKIT, stdout=f, stderr=subprocess.STDOUT)
    if code:
        with open(log, errors="replace") as f:
            print(f.read()[-1500:])
        sys.exit(1)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--hash":
        print(gen_hash(sys.argv[2]))
        return 0
    xbe, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    gen = os.path.join(out, "gen")
    disasm, func_id, abi = (os.path.join(out, d) for d in ("disasm", "func_id", "abi"))
    os.makedirs(gen, exist_ok=True)
    analysis = os.path.join(out, "analysis.json")
    functions = os.path.join(disasm, "functions.json")
    step("xbe_parser", out, "-m", "tools.xbe_parser", xbe, "--json", analysis, "--quiet")
    step("disasm", out, "-m", "tools.disasm", xbe, "--analysis-json", analysis,
         "--seed-functions", os.path.join(CONFIG, "seed_functions.json"),
         "--seed-functions", os.path.join(CONFIG, "seeds_upstream_starts.json"), "-o", disasm, "-v")
    step("xdk_names", out, "tools/ghidra_naming/merge_names.py", "--apply",
         "--names-json", "tools/ghidra_naming/xdk_names.json", "--functions-json", functions,
         "--out", os.path.join(out, "names.json"))
    step("func_id", out, "-m", "tools.func_id", xbe, "--functions", functions,
         "--strings", os.path.join(disasm, "strings.json"), "--xrefs", os.path.join(disasm, "xrefs.json"), "-o", func_id)
    step("abi_analysis", out, "-m", "tools.abi_analysis", xbe, "--functions", functions,
         "--identified", os.path.join(func_id, "identified_functions.json"), "--output-dir", abi)
    step("recomp", out, "-m", "tools.recomp", xbe, "--all", "--split", "250", "--disasm-dir", disasm,
         "--func-id-dir", func_id, "--abi-dir", abi, "--gen-dir", gen,
         "--manual-functions", os.path.join(CONFIG, "manual_functions.json"),
         "--exclude-manual", os.path.join(CONFIG, "recomp_hooks.c"),
         "--spin-waits", os.path.join(CONFIG, "spin_waits.json"), "-o", os.path.join(out, "recomp"))
    want_file = os.path.join(HERE, "GEN_SHA256")
    if os.path.exists(want_file):
        with open(want_file) as f:
            want = f.read().strip()
        if gen_hash(gen) != want:
            print("the translation came out different from this download's")
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
