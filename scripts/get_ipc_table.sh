#!/bin/bash
# Get IPC table for multiple configs
# Usage: ./get_ipc_table.sh <CORES> <TRACE> <REPL_LIST> <PF_LIST> <OCP_LIST>
# Example: ./get_ipc_table.sh 16 "LLM2048.GPTpre30Phase15xpost20-1979M_814M" "ship++ hawkeye" "no spp Zion1 bingo_dpc3 berti_c" "off hermes hmp ttp 4000fix"

CORES=$1
TRACE=$2
REPL_LIST=$3
PF_LIST=$4
OCP_LIST=$5

SCRIPT_DIR="$(dirname "$0")"

if [ -z "$OCP_LIST" ]; then
  echo "Usage: $0 <CORES> <TRACE> <REPL_LIST> <PF_LIST> <OCP_LIST>"
  exit 1
fi

echo "=== $TRACE | ${CORES}c ==="
echo ""
printf "%-10s %-12s" "REPL" "L2_PF"
for ocp in $OCP_LIST; do
  printf "%-10s" "$ocp"
done
echo ""
echo "-----------------------------------------------------------"

for repl in $REPL_LIST; do
  for pf in $PF_LIST; do
    printf "%-10s %-12s" "$repl" "$pf"
    for ocp in $OCP_LIST; do
      ipc=$("$SCRIPT_DIR/get_ipc.sh" "$CORES" "$repl" "$pf" "$ocp" "$TRACE")
      printf "%-10s" "$ipc"
    done
    echo ""
  done
done
