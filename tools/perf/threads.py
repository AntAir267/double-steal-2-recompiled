"""tools/perf/threads.py OUT.csv -- CMD... (STOP_AFTER=s sends SIGINT then): run CMD (the game) as a child and
once a second record each of its threads' CPU time (utime + stime from
/proc/<pid>/task/<tid>/stat, readable for one's own processes) by name.
At the end, a summary: average CPU % per thread name over the run, and over
the last 40 s (the mission, for the check's drive)."""
import csv, os, subprocess, sys, time

out = sys.argv[1]
cmd = sys.argv[sys.argv.index('--') + 1:]
hz = os.sysconf('SC_CLK_TCK')
p = subprocess.Popen(cmd)
rows, last = [], {}
t0 = time.time()

def sample():
    now = time.time() - t0
    try:
        tids = os.listdir(f'/proc/{p.pid}/task')
    except FileNotFoundError:
        return
    for tid in tids:
        try:
            with open(f'/proc/{p.pid}/task/{tid}/stat') as f:
                s = f.read()
        except OSError:
            continue
        name = s[s.index('(') + 1:s.rindex(')')]
        f2 = s[s.rindex(')') + 2:].split()
        cpu = (int(f2[11]) + int(f2[12])) / hz          # utime, stime
        prev = last.get(tid, (cpu, now))
        rows.append((round(now, 2), tid, name, round(cpu - prev[0], 3)))
        last[tid] = (cpu, now)

stop = float(os.environ.get('STOP_AFTER', '0'))
while p.poll() is None:
    sample()
    if stop and time.time() - t0 > stop and p.poll() is None:
        p.send_signal(2)                                  # SIGINT, as timeout -s INT
        stop = 0
    time.sleep(1.0)
with open(out, 'w', newline='') as f:
    w = csv.writer(f)
    w.writerow(['t', 'tid', 'name', 'cpu_s'])
    w.writerows(rows)

end = max((r[0] for r in rows), default=0)
for label, since in (('whole run', 0), ('last 40 s', end - 40)):
    by = {}
    for t, tid, name, d in rows:
        if t >= since:
            by.setdefault(name, 0.0)
            by[name] += d
    span = max(1e-9, end - max(0, since))
    tot = sum(by.values())
    print(f'{label}: {100 * tot / span:.0f}% of one core in all ({os.cpu_count()} here)')
    for name, v in sorted(by.items(), key=lambda kv: -kv[1])[:14]:
        print(f'   {100 * v / span:6.1f}%  {name}')
sys.exit(p.returncode or 0)
