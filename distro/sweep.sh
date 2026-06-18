#!/bin/bash
# sweep.sh — Build all configs locally, distribute jobs across hosts
# Usage: ./distro/sweep.sh <jobs_file>
#
# Jobs file format (one per line):
#   <job_name> <champsim_binary_args>
#
# This script:
#   1. Builds binary locally (or uses existing)
#   2. Splits jobs across available hosts
#   3. Pushes binary + job slice to each host
#   4. Hosts run independently

set -e
cd "$(dirname "$0")/.."

JOBS_FILE="$1"
HOSTS_FILE="distro/hosts.txt"
BIN="champsim_v14/bin/champsim"

if [[ -z "$JOBS_FILE" ]]; then
    echo "Usage: $0 <jobs_file>"
    echo "Jobs file format: <job_name> <binary args...>"
    exit 1
fi

if [[ ! -f "$BIN" ]]; then
    echo "ERROR: Binary not found at $BIN. Build first."
    exit 1
fi

if [[ ! -f "$JOBS_FILE" ]]; then
    echo "ERROR: Jobs file not found: $JOBS_FILE"
    exit 1
fi

# Count hosts and jobs
HOSTS=($(grep -v '^#' "$HOSTS_FILE" | grep -v '^$'))
NUM_HOSTS=${#HOSTS[@]}
NUM_JOBS=$(grep -vc '^#\|^$' "$JOBS_FILE" || true)

echo "[SWEEP] $NUM_JOBS jobs → $NUM_HOSTS hosts"

# Split jobs file into per-host slices
SLICE_DIR=$(mktemp -d)
split -n r/$NUM_HOSTS -d --additional-suffix=.txt "$JOBS_FILE" "$SLICE_DIR/slice_"

# Push each slice to corresponding host
i=0
for HOST in "${HOSTS[@]}"; do
    SLICE="$SLICE_DIR/slice_$(printf '%02d' $i).txt"
    if [[ -s "$SLICE" ]]; then
        SLICE_COUNT=$(wc -l < "$SLICE")
        echo "[SWEEP] $HOST ← $SLICE_COUNT jobs"
        ./distro/job_push.sh "$HOST" "$BIN" "$SLICE" &
    fi
    i=$((i + 1))
done

wait
rm -rf "$SLICE_DIR"
echo "[SWEEP] All hosts dispatched. Run './distro/job_pull.sh all' to collect results."
