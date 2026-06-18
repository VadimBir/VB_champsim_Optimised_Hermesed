#!/bin/bash
# job_pull.sh — Pull all finished logs from remote hosts
# Usage: ./distro/job_pull.sh [host_ip|all]
# Pulls from outputs/TTT01/ on remote → local outputs/TTT01/

set -e

SSH="ssh -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no -o ConnectTimeout=10"
LOCAL_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOCAL_OUT="$LOCAL_DIR/outputs/TTT01"
REMOTE_OUT="/home/cc/champsim_VB/outputs/TTT01"
HOSTS_FILE="$LOCAL_DIR/distro/hosts.txt"

mkdir -p "$LOCAL_OUT"

if [[ "$1" == "all" || -z "$1" ]]; then
    HOSTS=$(grep -v '^#' "$HOSTS_FILE" | grep -v '^$')
else
    HOSTS="$1"
fi

for HOST in $HOSTS; do
    echo "[PULL] $HOST..."
    rsync -avz -e "ssh -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no -o ConnectTimeout=10" \
        cc@$HOST:$REMOTE_OUT/ "$LOCAL_OUT/" 2>/dev/null && \
        echo "  Synced from $HOST" || \
        echo "  SKIP $HOST (unreachable or no logs)"
done
echo "[PULL] Done. Results in: $LOCAL_OUT/"
