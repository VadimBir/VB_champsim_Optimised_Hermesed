#!/bin/bash
# Watcher: keep all champsim procs at nice=-5 (counters numad re-renice).
# Run: nohup bash /home/cc/champsim_VB/champsim_renice_watcher.sh > /tmp/renice_watcher.log 2>/dev/null &
TARGET_NICE=-5
SLEEP_SEC=5
LOG="/tmp/champsim_renice_watcher.log"
echo "[$(date)] watcher start, target_nice=$TARGET_NICE, sleep=$SLEEP_SEC" >> "$LOG"
while true; do
  pids=$(pgrep champsim)
  if [[ -n "$pids" ]]; then
    fixed=0
    for pid in $pids; do
      cur=$(ps -o ni= -p "$pid" 2>/dev/null | tr -d ' ')
      if [[ -n "$cur" && "$cur" != "$TARGET_NICE" ]]; then
        sudo renice -n "$TARGET_NICE" -p "$pid" >/dev/null && ((fixed++))
      fi
    done
    if (( fixed > 0 )); then
      echo "[$(date)] renice'd $fixed champsim procs to $TARGET_NICE" >> "$LOG"
    fi
  fi
  sleep "$SLEEP_SEC"
done
