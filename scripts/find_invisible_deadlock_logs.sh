#!/bin/bash
# Find invisible deadlock logs (have Finished CPU but no Final ROI)
# Usage: ./find_invisible_deadlock_logs.sh <CORES> [LLM_SIZE] [--summary]
# Example: ./find_invisible_deadlock_logs.sh 16
#          ./find_invisible_deadlock_logs.sh 16 1024
#          ./find_invisible_deadlock_logs.sh 16 --summary

CORES=${1:-16}
LLM_SIZE=${2:-""}
SUMMARY_MODE=""

# Handle --summary flag
if [ "$2" == "--summary" ]; then
  SUMMARY_MODE="1"
  LLM_SIZE=""
elif [ "$3" == "--summary" ]; then
  SUMMARY_MODE="1"
fi

BASE_DIR="/home/cc/champsim_VB/outputs/Hx_outHOMO_LLM-shortened-4096-2048-1024-512-256-01"
LOG_DIR="$BASE_DIR/core${CORES}"

if [ -n "$LLM_SIZE" ]; then
  PATTERN="*LLM${LLM_SIZE}*.log"
else
  PATTERN="*.log"
fi

echo "=== Invisible Deadlock Logs (${CORES}c) ==="

declare -A BY_FINISHED
total=0

for f in "$LOG_DIR"/$PATTERN; do
  [ -f "$f" ] || continue
  grep -q "Final ROI" "$f" && continue
  grep -q "INVISIBLE DEADLOCK" "$f" && continue

  cnt=$(grep -c "Finished CPU" "$f")
  [ "$cnt" -eq 0 ] && continue
  [ "$cnt" -ge "$CORES" ] && continue

  if [ -n "$SUMMARY_MODE" ]; then
    BY_FINISHED[$cnt]=$((${BY_FINISHED[$cnt]:-0} + 1))
  else
    seqlen=$(basename "$f" | grep -oP 'LLM\K[0-9]+')
    trace=$(basename "$f" | grep -oP 'LLM[0-9]+\.\K[^.]+')
    echo "${cnt}/${CORES} | LLM${seqlen} | ${trace} | $(basename "$f")"
  fi
  ((total++))
done

if [ -n "$SUMMARY_MODE" ]; then
  echo ""
  echo "=== SUMMARY ==="
  for cnt in $(echo "${!BY_FINISHED[@]}" | tr ' ' '\n' | sort -n); do
    echo "${cnt}/${CORES}: ${BY_FINISHED[$cnt]} logs"
  done
fi

echo ""
echo "Total: $total invisible deadlock logs"
