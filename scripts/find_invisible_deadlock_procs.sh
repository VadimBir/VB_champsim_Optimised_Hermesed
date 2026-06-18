#!/bin/bash
# Find running processes that match invisible deadlock configs
# Usage: ./find_invisible_deadlock_procs.sh [CORES] [--kill]
# Example: ./find_invisible_deadlock_procs.sh 16
#          ./find_invisible_deadlock_procs.sh 16 --kill

CORES=${1:-16}
KILL_MODE=${2:-""}
HOSTS="10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96"
SSH_OPTS="-n -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no -o ConnectTimeout=5"

# Pattern matches configs known to cause invisible deadlocks
DEADLOCK_PATTERN="(berti_c|spp.*lru|bingo_dpc3.*(care|ship)|no_no_no.*hawkeye|Zion1.*(ship|lru|hawkeye))"

echo "=== Scanning for invisible deadlock processes (${CORES}c) ==="
echo ""

# Local
echo "=== LOCAL ==="
LOCAL_PIDS=()
while read line; do
  [ -z "$line" ] && continue
  pid=$(echo "$line" | awk '{print $1}')
  trace=$(echo "$line" | grep -oP 'LLM\d+' | head -1)
  model=$(echo "$line" | grep -oP '(GPT|OPT|Pythia)pre' | head -1 | sed 's/pre//')
  pf=$(echo "$line" | grep -oP 'pf_l2 (\S+)' | sed 's/pf_l2 //')
  repl=$(echo "$line" | grep -oP 'perceptron_([^_]+)_' | sed 's/perceptron_//' | sed 's/_$//')
  ocp=$(echo "$line" | grep -oP 'fixed_(\w+)/' | sed 's/fixed_//' | sed 's/\///')
  echo "PID:$pid | $trace $model | pf:$pf | repl:$repl | ocp:$ocp"
  LOCAL_PIDS+=($pid)
done < <(pgrep -f "/champsim " -a | grep "glc_${CORES}" | grep -v "zsh -c" | grep -E "LLM(256|512|1024|2048)" | grep -E "$DEADLOCK_PATTERN")

[ ${#LOCAL_PIDS[@]} -eq 0 ] && echo "(none)"

# Remotes
declare -A REMOTE_PIDS
for h in $HOSTS; do
  if result=$(ssh $SSH_OPTS cc@$h "pgrep -f '/champsim ' -a | grep 'glc_${CORES}' | grep -v 'zsh -c' | grep -E 'LLM(256|512|1024|2048)' | grep -E '$DEADLOCK_PATTERN'"); then
    echo ""
    echo "=== $h ==="
    echo "$result" | while read line; do
      [ -z "$line" ] && continue
      pid=$(echo "$line" | awk '{print $1}')
      trace=$(echo "$line" | grep -oP 'LLM\d+' | head -1)
      model=$(echo "$line" | grep -oP '(GPT|OPT|Pythia)pre' | head -1 | sed 's/pre//')
      pf=$(echo "$line" | grep -oP 'pf_l2 (\S+)' | sed 's/pf_l2 //')
      repl=$(echo "$line" | grep -oP 'perceptron_([^_]+)_' | sed 's/perceptron_//' | sed 's/_$//')
      ocp=$(echo "$line" | grep -oP 'fixed_(\w+)/' | sed 's/fixed_//' | sed 's/\///')
      echo "PID:$pid | $trace $model | pf:$pf | repl:$repl | ocp:$ocp"
    done
    REMOTE_PIDS[$h]=$(echo "$result" | awk '{print $1}' | tr '\n' ' ')
  fi
done

echo ""
echo "=== SUMMARY ==="
echo "Local: ${#LOCAL_PIDS[@]} processes"
for h in $HOSTS; do
  cnt=$(echo "${REMOTE_PIDS[$h]}" | wc -w)
  [ "$cnt" -gt 0 ] && echo "$h: $cnt processes"
done

# Kill mode
if [ "$KILL_MODE" == "--kill" ]; then
  echo ""
  echo "=== KILLING ==="
  for pid in "${LOCAL_PIDS[@]}"; do
    echo "kill $pid (local)"
    kill $pid
  done
  for h in $HOSTS; do
    pids="${REMOTE_PIDS[$h]}"
    [ -z "$pids" ] && continue
    for pid in $pids; do
      echo "kill $pid ($h)"
      ssh $SSH_OPTS cc@$h "kill $pid"
    done
  done
fi
