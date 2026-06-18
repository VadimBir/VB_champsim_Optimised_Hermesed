#!/bin/bash
# Get IPC for a specific config
# Usage: ./get_ipc.sh <CORES> <REPL> <L2_PF> <OCP> <TRACE>
# Example: ./get_ipc.sh 16 ship++ berti_c off LLM2048.GPTpre30Phase15xpost20-1979M_814M

CORES=$1
REPL=$2
L2_PF=$3
OCP=$4
TRACE=$5

BASE_DIR="/home/cc/champsim_VB/outputs/Hx_outHOMO_LLM-shortened-4096-2048-1024-512-256-01"
BK_DIR="$BASE_DIR/bk"

if [ -z "$TRACE" ]; then
  echo "Usage: $0 <CORES> <REPL> <L2_PF> <OCP> <TRACE>"
  echo "Example: $0 16 ship++ berti_c off LLM2048.GPTpre30Phase15xpost20-1979M_814M"
  exit 1
fi

# Handle 4000fix OCP
if [[ "$OCP" == "4000fix"* ]]; then
  PATTERN="glc..${CORES}c..${TRACE}..no..${L2_PF}..no..perceptron..${REPL}..ByP..4000fix-KappaPhiL1L2..4000fix-KappaPhiL1L2..4000fix-KappaPhiL1L2..fixed..off.log"
else
  PATTERN="glc..${CORES}c..${TRACE}..no..${L2_PF}..no..perceptron..${REPL}..ByP..no..no..no..fixed..${OCP}.log"
fi

# Search locations
LOCS=(
  "$BASE_DIR/core${CORES}/${PATTERN}"
  "$BASE_DIR/core${CORES}/000-${PATTERN}.INV_DDL_FIX"
  "$BK_DIR/${PATTERN}"
)

for f in "${LOCS[@]}"; do
  if [ -f "$f" ]; then
    if grep -q "Final ROI" "$f"; then
      grep "Final ROI" "$f" | grep -oP 'IPC: \K[0-9.]+'
      exit 0
    elif grep -q "INVISIBLE DEADLOCK" "$f"; then
      grep "INVISIBLE DEADLOCK" "$f" | grep -oP 'AVG IPC: \K[0-9.]+'
      exit 0
    else
      cnt=$(grep -c "Finished CPU" "$f")
      if [ "$cnt" -gt 0 ]; then
        avg=$(grep "Finished CPU" "$f" | grep -oP 'AVG IPC: ;\K[0-9.]+' | awk '{sum+=$1; count++} END {printf "%.5f", sum/count}')
        echo "$avg (${cnt}/${CORES})"
        exit 0
      fi
    fi
  fi
done

echo "—"
