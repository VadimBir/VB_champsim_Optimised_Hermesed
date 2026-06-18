#!/usr/bin/env bash
# Merge all *.log from outputs/out*/coreN/ -> outputs/bulk_singleton_out/coreN/
# - keep only logs with "FINAL ROI CORE AVG IPC:"
# - on filename collision: latest mtime wins, log WARN with both paths
# - 8-way parallelism for grep + copy
set -u
ROOT="/home/cc/champsim_VB/outputs"
TGT="$ROOT/bulk_singleton_out"
mkdir -p "$TGT"
LOCKF="$TGT/.merge.lock"
exec 9>"$LOCKF"
flock -n 9 || { echo "another merge already running"; exit 1; }

LOG="$TGT/_merge.log"
WARN="$TGT/_merge_warnings.log"
: > "$LOG"; : > "$WARN"

ALL="$TGT/.all_logs.txt"
ROI="$TGT/.roi_logs.txt"
WIN="$TGT/.winners.txt"

echo "[1/4] enumerating source logs ..." | tee -a "$LOG"
# Source = outputs/out*/coreN/*.log  EXCEPT target itself, archives, merged, EVALS, cc_out
: > "$ALL"
for d in "$ROOT"/out*/; do
  base=$(basename "$d")
  case "$base" in
    bulk_singleton_out|archive_*|out_merged_*|outEVALS*|cc_out) continue ;;
  esac
  find "$d" -mindepth 2 -maxdepth 2 -type f -name '*.log' -print >> "$ALL"
done
total=$(wc -l < "$ALL")
echo "    total source logs: $total" | tee -a "$LOG"

echo "[2/4] filter by ROI line (8-way parallel grep -l) ..." | tee -a "$LOG"
xargs -a "$ALL" -d '\n' -n 200 -P 8 grep -l "FINAL ROI CORE AVG IPC:" > "$ROI" 2>/dev/null || true
have_roi=$(wc -l < "$ROI")
skipped=$((total - have_roi))
echo "    have_ROI=$have_roi  skipped_no_ROI=$skipped" | tee -a "$LOG"

echo "[3/4] selecting winners (latest mtime per filename) ..." | tee -a "$LOG"
# Emit: <mtime>\t<basename>\t<fullpath>
# Group by basename; latest mtime wins.
# Use python for safety w/ paths containing spaces / special chars.
python3 - "$ROI" "$WIN" "$WARN" <<'PY' | tee -a "$LOG"
import os, sys
roi_path, win_path, warn_path = sys.argv[1], sys.argv[2], sys.argv[3]
groups = {}  # basename -> list of (mtime, fullpath)
with open(roi_path) as f:
    for line in f:
        p = line.rstrip('\n')
        if not p: continue
        try:
            mt = os.path.getmtime(p)
        except OSError:
            continue
        bn = os.path.basename(p)
        groups.setdefault(bn, []).append((mt, p))

winners = []
conflicts = 0
with open(warn_path, 'w') as wf:
    for bn, lst in groups.items():
        lst.sort(key=lambda x: x[0], reverse=True)
        winner = lst[0]
        winners.append(winner[1])
        if len(lst) > 1:
            for loser in lst[1:]:
                conflicts += 1
                wf.write(f"WARN conflict {bn}: KEEP={winner[1]} (mt={int(winner[0])}) DROP={loser[1]} (mt={int(loser[0])})\n")

with open(win_path, 'w') as wf:
    for p in winners:
        wf.write(p + '\n')

print(f"    winners={len(winners)} conflicts={conflicts}")
PY

winners=$(wc -l < "$WIN")
echo "[4/4] copying $winners winners (8-way parallel cp) ..." | tee -a "$LOG"

# Determine target subdir from source path's parent dir name (coreN)
# Use python for robust path handling.
python3 - "$WIN" "$TGT" <<'PY' > "$TGT/.copy_cmds.txt"
import os, sys, shlex
win_path, tgt_root = sys.argv[1], sys.argv[2]
with open(win_path) as f:
    for line in f:
        src = line.rstrip('\n')
        if not src: continue
        parent = os.path.basename(os.path.dirname(src))   # e.g. core2
        bn = os.path.basename(src)
        dst_dir = os.path.join(tgt_root, parent)
        dst = os.path.join(dst_dir, bn)
        # Print: dst_dir\0src\0dst\0
        print(f"{shlex.quote(dst_dir)}\t{shlex.quote(src)}\t{shlex.quote(dst)}")
PY

# Parallel copy via xargs -P 8
awk -F'\t' '{print "mkdir -p "$1" && cp -p "$2" "$3}' "$TGT/.copy_cmds.txt" | xargs -d '\n' -P 8 -I{} bash -c '{}'

copied=$(find "$TGT" -mindepth 2 -type f -name '*.log' | wc -l)

{
  echo "=== merge_logs_v2.sh SUMMARY ==="
  echo "source_logs     : $total"
  echo "have_ROI        : $have_roi"
  echo "skipped_no_ROI  : $skipped"
  echo "winners         : $winners"
  echo "copied (verify) : $copied"
  echo "warnings        : $(wc -l <"$WARN")"
  echo "target          : $TGT"
} | tee -a "$LOG"
