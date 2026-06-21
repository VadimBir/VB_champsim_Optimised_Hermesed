#!/usr/bin/env bash
# Merge all *.log files from outputs/*/core*/ into outputs/bulk_singleton_out/core*/
# Rule:
#   - Skip src logs lacking "FINAL ROI CORE AVG IPC:"
#   - If only one candidate per filename -> copy
#   - If multiple: latest mtime wins + WARN both paths
# Single-instance guard via flock.
set -u
ROOT="/home/cc/champsim_VB/outputs"
TGT="$ROOT/bulk_singleton_out"
mkdir -p "$TGT"
LOCKF="$TGT/.merge.lock"
exec 9>"$LOCKF"
if ! flock -n 9; then
  echo "another merge already running, abort"
  exit 1
fi
LOG="$TGT/_merge.log"
WARN="$TGT/_merge_warnings.log"
LIST="$TGT/.candidates.tsv"
: > "$LOG"; : > "$WARN"

echo "[1/4] finding logs ..." | tee -a "$LOG"
find "$ROOT" -mindepth 3 -maxdepth 3 -type f -name '*.log' -not -path "$TGT/*" > "$TGT/.all_logs.txt"
total=$(wc -l < "$TGT/.all_logs.txt")
echo "    total logs: $total" | tee -a "$LOG"

echo "[2/4] filtering by ROI line (parallel grep -l) ..." | tee -a "$LOG"
# Use xargs -P for parallel grep, output only matching paths
xargs -a "$TGT/.all_logs.txt" -d '\n' -n 200 -P 16 grep -l "FINAL ROI CORE AVG IPC:" > "$TGT/.roi_logs.txt" 2>/dev/null
have_roi=$(wc -l < "$TGT/.roi_logs.txt")
skipped_no_roi=$((total - have_roi))
echo "    have_ROI=$have_roi  skipped_no_ROI=$skipped_no_roi" | tee -a "$LOG"

echo "[3/4] building winner table ..." | tee -a "$LOG"
# stat each + emit "mtime<TAB>relpath_under_outputs<TAB>fullpath"
# relpath = strip $ROOT/ and the first component (outNN/...) -> coreX/foo.log key
while IFS= read -r f; do
  rel="${f#$ROOT/}"
  key="${rel#*/}"
  mt=$(stat -c %Y "$f")
  printf '%s\t%s\t%s\n' "$mt" "$key" "$f"
done < "$TGT/.roi_logs.txt" > "$LIST"

# Sort by key asc, mtime desc -> first row per key wins
sort -t$'\t' -k2,2 -k1,1nr "$LIST" -o "$LIST.sorted"

# Emit winners + warnings
awk -F'\t' -v WARN="$WARN" '
{
  key=$2
  if (key != prev_key) {
    if (NR>1) {
      # finalize prev group: prev_winner already chosen
    }
    print $3       # winner full path
    n_winners++
    prev_key=key
    prev_winner_mt=$1
    prev_winner_p=$3
  } else {
    conflicts++
    print "WARN conflict " key ": KEEP=" prev_winner_p " (mt=" prev_winner_mt ") DROP=" $3 " (mt=" $1 ")" >> WARN
  }
}
END {
  print "WINNERS=" n_winners " CONFLICTS=" conflicts > "/dev/stderr"
}
' "$LIST.sorted" > "$TGT/.winners.txt" 2> "$TGT/.awk_stats.txt"

winners=$(wc -l < "$TGT/.winners.txt")
echo "    $(cat "$TGT/.awk_stats.txt")" | tee -a "$LOG"

echo "[4/4] copying winners ..." | tee -a "$LOG"
copied=0
while IFS= read -r src; do
  rel="${src#$ROOT/}"
  key="${rel#*/}"
  dst="$TGT/$key"
  mkdir -p "$(dirname "$dst")"
  cp -p "$src" "$dst"
  copied=$((copied+1))
done < "$TGT/.winners.txt"

{
  echo "=== merge_logs_singleton.sh SUMMARY ==="
  echo "scanned_logs    : $total"
  echo "have_ROI        : $have_roi"
  echo "skipped_no_ROI  : $skipped_no_roi"
  echo "winners         : $winners"
  echo "copied          : $copied"
  echo "target          : $TGT"
  echo "warnings file   : $WARN ($(wc -l <"$WARN") lines)"
} | tee -a "$LOG"
