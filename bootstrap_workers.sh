#!/usr/bin/env bash
# Bootstrap 8 Chameleon worker hosts:
#  1. Push NOPASSWD sudoers (one-time, needs cc pwd)
#  2. apt install sqlite3 libsqlite3-dev xz-utils
#  3. rsync /home/cc/champsim_VB/ -> remote /home/cc/champsim_VB/  (parallel)
#  4. Smoke quick_v14.sh on each host (parallel), save log
set -u

HOSTS=(10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96)
KEY=/home/cc/.ssh/id_rsa
SSH_OPTS="-i $KEY -o StrictHostKeyChecking=no -o ConnectTimeout=15 -o BatchMode=no"
SRC=/home/cc/champsim_VB/
DST=/home/cc/champsim_VB/
LOGDIR=/home/cc/champsim_VB/bootstrap_logs
mkdir -p "$LOGDIR"

PHASE="${1:-all}"   # all | sudoers | apt | rsync | smoke

if [[ -z "${CC_PWD:-}" ]]; then
  read -rsp "Enter cc sudo password (will be used once per host for NOPASSWD setup): " CC_PWD
  echo
fi

run_remote_pwd() {  # passes pwd via sshpass; used only for initial sudoers push
  local host=$1; shift
  sshpass -p "$CC_PWD" ssh $SSH_OPTS cc@$host "$@"
}
run_remote() {       # passwordless ssh (after sudoers set or for non-sudo cmds)
  local host=$1; shift
  ssh $SSH_OPTS cc@$host "$@"
}

phase_sudoers() {
  echo "=== Phase 1: NOPASSWD sudoers on 8 hosts ==="
  for h in "${HOSTS[@]}"; do
    (
      log="$LOGDIR/sudoers_$h.log"
      out=$(run_remote_pwd "$h" "echo '$CC_PWD' | sudo -S bash -c 'echo \"cc ALL=(ALL) NOPASSWD:ALL\" > /etc/sudoers.d/cc && chmod 440 /etc/sudoers.d/cc'" 2>&1)
      echo "$out" > "$log"
      verify=$(run_remote "$h" "sudo -n true && echo OK || echo FAIL" 2>&1)
      echo "[$h] sudoers: $verify"
    ) &
  done; wait
}

phase_apt() {
  echo "=== Phase 2: apt install (parallel, passwordless) ==="
  for h in "${HOSTS[@]}"; do
    (
      log="$LOGDIR/apt_$h.log"
      run_remote "$h" "sudo apt-get update -qq && sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq sqlite3 libsqlite3-dev xz-utils rsync" > "$log" 2>&1
      rc=$?
      echo "[$h] apt rc=$rc"
    ) &
  done; wait
}

phase_rsync() {
  echo "=== Phase 3: rsync champsim_VB/ in parallel ==="
  for h in "${HOSTS[@]}"; do
    (
      log="$LOGDIR/rsync_$h.log"
      rsync -a --partial -e "ssh $SSH_OPTS" "$SRC" cc@$h:$DST > "$log" 2>&1
      rc=$?
      echo "[$h] rsync rc=$rc"
    ) &
  done; wait
}

phase_smoke() {
  echo "=== Phase 4: smoke quick_v14.sh parallel ==="
  local CMD='cd /home/cc/champsim_VB && MODEL="no" && ./quick_v14.sh --dir champsim_v14 -p 1 --L1 no --L2 no --L3 no --trace LLM256.Py -d 2 -c 2 -bypca --l1byp $MODEL --l2byp $MODEL --l3byp $MODEL --ocp none --repl lru'
  for h in "${HOSTS[@]}"; do
    (
      log="$LOGDIR/smoke_$h.log"
      run_remote "$h" "$CMD" > "$log" 2>&1
      rc=$?
      echo "[$h] smoke rc=$rc  log=$log"
    ) &
  done; wait
}

case "$PHASE" in
  sudoers) phase_sudoers ;;
  apt)     phase_apt ;;
  rsync)   phase_rsync ;;
  smoke)   phase_smoke ;;
  all)     phase_sudoers && phase_apt && phase_rsync && phase_smoke ;;
  *) echo "Usage: $0 [all|sudoers|apt|rsync|smoke]"; exit 2 ;;
esac

echo "=== Done. Logs: $LOGDIR ==="
