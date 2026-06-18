#!/bin/bash
# job_push.sh — Push binary + job list to a remote host
# Usage: ./distro/job_push.sh <host_ip> <binary_path> <jobs_file>
#
# jobs_file format (one job per line):
#   <job_name> <run_command>
#
# Remote runs N jobs in parallel (controlled by PARALLEL var below)

set -e

PARALLEL=2  # jobs per host in parallel

HOST="$1"
BIN_PATH="$2"
JOBS_FILE="$3"

if [[ -z "$HOST" || -z "$BIN_PATH" || -z "$JOBS_FILE" ]]; then
    echo "Usage: $0 <host_ip> <binary_path> <jobs_file>"
    exit 1
fi

SSH="ssh -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no -o ConnectTimeout=10"
SCP="scp -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no"
REMOTE_BASE="/home/cc/champsim_VB"
REMOTE_BIN="$REMOTE_BASE/distro_bin/champsim"
REMOTE_JOBS="$REMOTE_BASE/distro_jobs"
REMOTE_OUT="$REMOTE_BASE/outputs/TTT01"

# 1) Send binary
echo "[PUSH] Sending binary to $HOST..."
$SSH cc@$HOST "mkdir -p $REMOTE_BASE/distro_bin $REMOTE_JOBS"
$SCP "$BIN_PATH" cc@$HOST:$REMOTE_BIN
$SSH cc@$HOST "chmod +x $REMOTE_BIN"

# 2) Send jobs file
$SCP "$JOBS_FILE" cc@$HOST:$REMOTE_JOBS/jobs.txt

# 3) Create worker script on remote
$SSH cc@$HOST "cat > $REMOTE_JOBS/worker.sh << 'WEOF'
#!/bin/bash
PARALLEL=$PARALLEL
REMOTE_BIN=$REMOTE_BIN
REMOTE_OUT=$REMOTE_OUT
JOBS_FILE=$REMOTE_JOBS/jobs.txt

mkdir -p \"\$REMOTE_OUT\"

run_job() {
    local JOB_NAME=\"\$1\"
    shift
    local CMD=\"\$@\"
    local LOG=\"\$REMOTE_OUT/\${JOB_NAME}.log\"
    echo \"[START] \$JOB_NAME \$(date)\" > \"\$LOG\"
    echo \"[CMD] \$CMD\" >> \"\$LOG\"
    echo \"---\" >> \"\$LOG\"
    eval \"\$CMD\" >> \"\$LOG\" 2>&1
    echo \"---\" >> \"\$LOG\"
    echo \"[END] \$JOB_NAME \$(date)\" >> \"\$LOG\"
}

# Run jobs with parallelism control
RUNNING=0
while IFS= read -r line; do
    [[ -z \"\$line\" || \"\$line\" == \"#\"* ]] && continue
    JOB_NAME=\$(echo \"\$line\" | awk '{print \$1}')
    CMD=\$(echo \"\$line\" | cut -d' ' -f2-)

    run_job \"\$JOB_NAME\" \"\$CMD\" &
    RUNNING=\$((RUNNING + 1))

    if [[ \$RUNNING -ge $PARALLEL ]]; then
        wait -n
        RUNNING=\$((RUNNING - 1))
    fi
done < \"\$JOBS_FILE\"

wait
echo \"[WORKER] All jobs complete on \$(hostname) at \$(date)\"
WEOF
chmod +x $REMOTE_JOBS/worker.sh"

# 4) Launch worker detached
echo "[PUSH] Launching $PARALLEL-parallel worker on $HOST..."
$SSH cc@$HOST "nohup $REMOTE_JOBS/worker.sh > $REMOTE_JOBS/worker.out 2>&1 &"

echo "[DONE] Jobs dispatched to $HOST. Logs will be at: $REMOTE_OUT/"
