#!/bin/bash
# Move invisible deadlock logs to prevent remote sweep overwrite
# Adds 000- prefix and .INV_DDL_FIX suffix
# Usage: ./move_invisible_deadlock_logs.sh <CORES> [FINISHED_COUNT] [--dry-run]
# Example: ./move_invisible_deadlock_logs.sh 16 15          # only 15/16
#          ./move_invisible_deadlock_logs.sh 16             # all partial
#          ./move_invisible_deadlock_logs.sh 16 15 --dry-run

CORES=${1:-16}
FINISHED_FILTER=${2:-""}
DRY_RUN=""

# Handle flags
if [ "$2" == "--dry-run" ]; then
  DRY_RUN="1"
  FINISHED_FILTER=""
elif [ "$3" == "--dry-run" ]; then
  DRY_RUN="1"
fi

BASE_DIR="/home/cc/champsim_VB/outputs/Hx_outHOMO_LLM-shortened-4096-2048-1024-512-256-01"
LOG_DIR="$BASE_DIR/core${CORES}"

echo "=== Moving Invisible Deadlock Logs (${CORES}c) ==="
[ -n "$DRY_RUN" ] && echo "[DRY-RUN MODE]"
[ -n "$FINISHED_FILTER" ] && echo "[FILTER: ${FINISHED_FILTER}/${CORES} only]"
echo ""

moved=0

cd "$LOG_DIR" || exit 1

for f in *.log; do
  [ -f "$f" ] || continue
  [[ "$f" == 000-* ]] && continue  # already moved
  grep -q "Final ROI" "$f" && continue
  grep -q "INVISIBLE DEADLOCK" "$f" && continue

  cnt=$(grep -c "Finished CPU" "$f")
  [ "$cnt" -eq 0 ] && continue
  [ "$cnt" -ge "$CORES" ] && continue

  # Apply finished count filter if set
  if [ -n "$FINISHED_FILTER" ] && [ "$cnt" -ne "$FINISHED_FILTER" ]; then
    continue
  fi

  newname="000-${f}.INV_DDL_FIX"

  if [ -n "$DRY_RUN" ]; then
    echo "[DRY] $f -> $newname"
  else
    echo "MOVE: $f -> $newname"
    mv "$f" "$newname"
  fi
  ((moved++))
done

echo ""
echo "Total: $moved logs moved"
