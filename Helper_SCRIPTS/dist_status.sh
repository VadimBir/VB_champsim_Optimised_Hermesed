#!/usr/bin/env bash
# Live status of distributed sims. Reads job files + ssh probes hosts in parallel.
# Usage: ./dist_status.sh [jobs_dir]
set -u
JOBS_DIR="${1:-$(ls -td /tmp/champsim_dist_jobs_* 2>/dev/null | head -1)}"
[[ -z "$JOBS_DIR" || ! -d "$JOBS_DIR" ]] && { echo "no jobs dir found"; exit 1; }
DIST_KEY="${DIST_KEY:-/home/cc/.ssh/id_rsa}"
SSH_OPTS="-i $DIST_KEY -o StrictHostKeyChecking=no -o ConnectTimeout=5 -o BatchMode=yes"
HOSTS=(10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96)

echo "=== jobs_dir: $JOBS_DIR ==="
total=$(find "$JOBS_DIR" -maxdepth 1 -name '*.job' 2>/dev/null | wc -l)
done_=$(find "$JOBS_DIR" -maxdepth 1 -name '*.job.done' 2>/dev/null | wc -l)
echo "in-flight (.job): $total | finalized (.job.done): $done_"
echo

printf "%-15s %6s %6s %6s %6s %s\n" "HOST" "JOBS" "REM_PR" "MEM%" "CPU%" "STATE"
for h in "${HOSTS[@]}"; do
  jobs_h=$(grep -l "^host=$h$" "$JOBS_DIR"/*.job 2>/dev/null | wc -l)
  probe=$(timeout 8 ssh $SSH_OPTS "cc@$h" \
    'echo $(pgrep -fc "simulation_instructions ") $(free | awk "/Mem:/{printf \"%d\", \$3/\$2*100}") $(top -bn1 | awk "/Cpu\(s\)/{print \$2+\$4}")' \
    2>/dev/null) || probe="? ? ?"
  read -r rem_proc mem_pct cpu_pct <<< "$probe"
  state="OK"
  [[ "$rem_proc" == "?" ]] && state="UNREACHABLE"
  printf "%-15s %6d %6s %6s %6s %s\n" "$h" "$jobs_h" "$rem_proc" "$mem_pct" "$cpu_pct" "$state"
done
echo
echo "=== oldest 5 in-flight ==="
ls -t "$JOBS_DIR"/*.job 2>/dev/null | tail -5 | while read jf; do
  age=$(( $(date +%s) - $(awk -F= '/^started=/{print $2}' "$jf") ))
  h=$(awk -F= '/^host=/{print $2}' "$jf")
  tr=$(awk -F= '/^trace_hint=/{print $2}' "$jf")
  md=$(awk -F= '/^model_tag=/{print $2}' "$jf")
  printf "  age=%6ds  host=%-15s  tr=%s  md=%s\n" "$age" "$h" "$tr" "$md"
done
