#!/bin/bash
# Fix invisible deadlock logs - calculate avg IPC and append summary line
# Usage: ./fix_invisible_deadlock.sh <LLM_SIZE> <CORES> [--dry-run]
# Example: ./fix_invisible_deadlock.sh 1024 16
#          ./fix_invisible_deadlock.sh 2048 16 --dry-run

LLM_SIZE=$1
CORES=$2
DRY_RUN=$3
BASE_DIR="/home/cc/champsim_VB/outputs/Hx_outHOMO_LLM-shortened-4096-2048-1024-512-256-01"

if [ -z "$LLM_SIZE" ] || [ -z "$CORES" ]; then
    echo "Usage: $0 <LLM_SIZE> <CORES> [--dry-run]"
    exit 1
fi

LOG_DIR="$BASE_DIR/core${CORES}"
PATTERN1="*LLM${LLM_SIZE}*.log"
PATTERN2="000-*LLM${LLM_SIZE}*.INV_DDL_FIX"

echo "=== Scanning LLM${LLM_SIZE} ${CORES}c for invisible deadlocks ==="

for f in "$LOG_DIR"/$PATTERN1 "$LOG_DIR"/$PATTERN2; do
    [ -f "$f" ] || continue
    grep -q "Final ROI" "$f" && continue
    grep -q "INVISIBLE DEADLOCK" "$f" && continue  # already fixed
    
    cnt=$(grep -c "Finished CPU" "$f")
    [ "$cnt" -eq 0 ] && continue
    [ "$cnt" -ge "$CORES" ] && continue
    
    avg=$(grep "Finished CPU" "$f" | grep -oP 'AVG IPC: ;\K[0-9.]+' | awk '{sum+=$1; count++} END {printf "%.5f", sum/count}')
    line="INVISIBLE DEADLOCK ${cnt}/${CORES} AVG IPC: ${avg}"
    
    echo "$line  $(basename "$f")"
    
    if [ "$DRY_RUN" != "--dry-run" ]; then
        echo "$line" >> "$f"
    fi
done

echo "=== Done ==="
