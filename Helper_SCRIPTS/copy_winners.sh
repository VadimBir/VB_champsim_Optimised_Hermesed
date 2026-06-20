#!/usr/bin/env bash
# Copy winners from .winners.txt to bulk_singleton_out/coreN/
set -u
ROOT="/home/cc/champsim_VB/outputs"
TGT="$ROOT/bulk_singleton_out"
WIN="$TGT/.winners.txt"
LOG="$TGT/_merge.log"

[[ ! -s "$WIN" ]] && { echo "no winners file"; exit 1; }
total=$(wc -l < "$WIN")
echo "copying $total winners ..." | tee -a "$LOG"

copied=0
while IFS= read -r src; do
  rel="${src#$ROOT/}"
  key="${rel#*/}"           # strip outXX/ -> coreN/foo.log
  dst="$TGT/$key"
  mkdir -p "$(dirname "$dst")"
  cp -p "$src" "$dst"
  copied=$((copied+1))
  if (( copied % 2000 == 0 )); then
    echo "  $copied / $total" | tee -a "$LOG"
  fi
done < "$WIN"

{
  echo "=== copy phase DONE ==="
  echo "copied: $copied / $total"
  echo "target: $TGT"
} | tee -a "$LOG"
