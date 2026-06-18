#!/bin/bash
# Test: push a simple lru sim to first host
# First build locally, then push

cd "$(dirname "$0")/.."

# Build (no bypass, lru repl, 2 cores)
echo "[TEST] Building binary..."
MODEL="no"
./quick_v14.sh --dir champsim_v14 -p 1 --L1 no --L2 no --L3 no --trace 256.Py -d 0 -c 2 -bypca --l1byp $MODEL --l2byp $MODEL --l3byp $MODEL --ocp none --repl lru --build-only 2>&1 | tail -5

# Figure out the actual run command the binary needs
# The binary after build is at champsim_v14/bin/champsim
# We need the trace path on remote — assumed same location
TRACE=$(find traces/ -name "*256*Py*" 2>/dev/null | head -1)
TRACE_BASENAME=$(basename "$TRACE")

HOST=$(head -1 distro/hosts.txt)

# Push with a simple run command
# Remote trace path assumed: /home/cc/champsim_VB/traces/<name>
./distro/job_push.sh "$HOST" "test_lru_$(date +%s)" \
    "./champsim --warmup_instructions 1000000 --simulation_instructions 5000000 -traces /home/cc/champsim_VB/traces/$TRACE_BASENAME"

echo "[TEST] Done. Check distro/results/ for output."
