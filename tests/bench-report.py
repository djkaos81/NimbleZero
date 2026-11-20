#!/usr/bin/env python3
"""Collect the nz-perf JSON blocks recorded by tests/qemu-boot.py --script bench-*.script
into one table.  usage: bench-report.py label=results/dir/script.log ... > table.md
(also writes bench.json next to the first log's parent)"""
import sys, re, json, os
rows = {}
for a in sys.argv[1:]:
    label, path = a.split("=", 1)
    txt = open(path, errors="replace").read()
    for m in re.finditer(r"### BENCH (\S+) \n\$.*?\n(\{.*?\n\})", txt, re.S):
        try: rows[f"{label}/{m.group(1)}"] = json.loads(m.group(2))
        except Exception as e: print("bad json in", a, e, file=sys.stderr)
keys = [("mem_used_kb", "RAM used (MemTotal-MemAvailable), kB"), ("userspace_pss_kb", "userspace PSS, kB"), ("mem_slab_kb", "kernel slab, kB"),
        ("processes_user", "userspace processes"), ("kernel_modules", "loaded modules"), ("cpu_idle_pct", "CPU idle %"),
        ("context_switches_per_s", "context switches/s"), ("interrupts_per_s", "interrupts/s"), ("forks_per_s", "forks/s"),
        ("sd_write_kb_per_s", "SD writes kB/s (30 s window)"), ("sd_written_since_boot_kb", "SD written since boot, kB"),
        ("boot_kernel_to_init_s", "kernel start -> init, s (emulated)"), ("boot_init_done_s", "init scripts done, s (emulated)"),
        ("rootfs_used_kb", "rootfs used, kB"), ("exec_latency_ms", "fork+exec /bin/true, ms (emulated)")]
print("| metric | " + " | ".join(rows) + " |"); print("|---|" + "---|" * len(rows))
for k, name in keys:
    print(f"| {name} | " + " | ".join(str(r.get(k, "")) for r in rows.values()) + " |")
json.dump(rows, open("bench.json", "w"), indent=1)
