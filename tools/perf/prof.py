"""tools/perf/prof.py PROFILE BIN [FROM_S [TO_S]]: the DS2_PROFILE samples by
function of BIN (nm), hottest first, between FROM_S and TO_S seconds."""
import bisect, struct, subprocess, sys
d = open(sys.argv[1], 'rb').read()
binp = sys.argv[2]
t0 = float(sys.argv[3]) * 1000 if len(sys.argv) > 3 else 0
t1 = float(sys.argv[4]) * 1000 if len(sys.argv) > 4 else 1e12
recs = [struct.unpack_from('<QI', d, o) for o in range(0, len(d) - 11, 12)]
anchor = recs[0][0]
syms = []
for line in subprocess.run(['nm', '-n', '--defined-only', binp], capture_output=True, text=True).stdout.splitlines():
    p = line.split()
    if len(p) >= 3 and p[1] in 'tTwW':
        syms.append((int(p[0], 16), p[2]))
base = dict((n, a) for a, n in syms)['ds2_prof_start']
slide = anchor - base
addrs = [a for a, _ in syms]
hist, n = {}, 0
for pc, ms in recs[1:]:
    if not (t0 <= ms < t1):
        continue
    i = bisect.bisect_right(addrs, pc - slide) - 1
    name = syms[i][1] if i >= 0 else '?'
    hist[name] = hist.get(name, 0) + 1
    n += 1
print(f'{n} samples ({n / 1000:.1f} s of the thread\'s CPU) from {t0/1000:.0f} s')
for name, c in sorted(hist.items(), key=lambda kv: -kv[1])[:25]:
    print(f'  {100 * c / max(1, n):5.1f}%  {name}')
