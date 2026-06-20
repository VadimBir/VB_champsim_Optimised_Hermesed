#!/usr/bin/env python3
"""Parse _merge_warnings.log; for each conflict, grep IPC from KEEP vs DROP.
Emit per-conflict: bn, keep_ipc, drop_ipc, abs_diff, rel_diff%.
Summarize: zero-diff, small-diff (<1%), large-diff (>=1%)."""
import re, sys, os, subprocess
from collections import Counter

WARN = "/home/cc/champsim_VB/outputs/bulk_singleton_out/_merge_warnings.log"
OUT  = "/home/cc/champsim_VB/outputs/bulk_singleton_out/_ipc_diff_report.csv"
LARGE_OUT = "/home/cc/champsim_VB/outputs/bulk_singleton_out/_ipc_diff_large.csv"

LINE_RE = re.compile(
    r"^WARN conflict (?P<bn>\S+?): KEEP=(?P<keep>.+?) \(mt=\d+\) DROP=(?P<drop>.+?) \(mt=\d+\)\s*$"
)
IPC_RE = re.compile(r"FINAL ROI CORE AVG IPC:\s*;\s*([0-9.]+)\s*;")

def get_ipc(path):
    try:
        with open(path, 'r', errors='ignore') as f:
            for line in f:
                m = IPC_RE.search(line)
                if m: return float(m.group(1))
    except OSError:
        return None
    return None

def main():
    total = 0; missing = 0
    rows = []
    with open(WARN) as f:
        for line in f:
            m = LINE_RE.match(line)
            if not m: continue
            total += 1
            bn, keep, drop = m.group('bn'), m.group('keep'), m.group('drop')
            k_ipc = get_ipc(keep)
            d_ipc = get_ipc(drop)
            if k_ipc is None or d_ipc is None:
                missing += 1
                continue
            abs_d = abs(k_ipc - d_ipc)
            rel = (abs_d / max(k_ipc, d_ipc) * 100) if max(k_ipc, d_ipc) > 0 else 0
            rows.append((bn, k_ipc, d_ipc, abs_d, rel, keep, drop))

    rows.sort(key=lambda r: r[4], reverse=True)

    zero=0; small=0; large=0
    with open(OUT, 'w') as f:
        f.write("basename;keep_ipc;drop_ipc;abs_diff;rel_pct;keep_path;drop_path\n")
        for r in rows:
            f.write(";".join([r[0], f"{r[1]:.5f}", f"{r[2]:.5f}", f"{r[3]:.5f}", f"{r[4]:.3f}", r[5], r[6]])+"\n")
            if r[3] < 1e-9: zero+=1
            elif r[4] < 1.0: small+=1
            else: large+=1

    with open(LARGE_OUT, 'w') as f:
        f.write("basename;keep_ipc;drop_ipc;abs_diff;rel_pct;keep_path;drop_path\n")
        for r in rows:
            if r[4] >= 1.0:
                f.write(";".join([r[0], f"{r[1]:.5f}", f"{r[2]:.5f}", f"{r[3]:.5f}", f"{r[4]:.3f}", r[5], r[6]])+"\n")

    print(f"=== IPC DIFF REPORT ===")
    print(f"warn lines parsed : {total}")
    print(f"both have IPC     : {len(rows)}")
    print(f"missing IPC       : {missing}")
    print(f"zero diff         : {zero}")
    print(f"small diff (<1%)  : {small}")
    print(f"large diff (>=1%) : {large}")
    print(f"full report       : {OUT}")
    print(f"large-only report : {LARGE_OUT}")
    print()
    print(f"--- TOP 20 LARGEST REL DIFFS ---")
    for r in rows[:20]:
        print(f"{r[4]:7.3f}%  abs={r[3]:.5f}  keep={r[1]:.5f}  drop={r[2]:.5f}  {r[0]}")

if __name__ == '__main__':
    main()
