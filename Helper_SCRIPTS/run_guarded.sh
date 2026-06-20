#!/bin/bash
# Size-guarded command runner.
# Launches command, finds the actual binary writing to the log via lsof,
# SIGSTOPs that binary when file exceeds cap.
#
# Usage: ./run_guarded.sh <MAX_GB> <LOG_FILE> <command...>

MAX_GB="${1:?Usage: $0 <MAX_GB> <LOG_FILE> <command...>}"
LOGFILE="${2:?Usage: $0 <MAX_GB> <LOG_FILE> <command...>}"
shift 2

MAX_BYTES=$((MAX_GB * 1024*1024*1024))
touch "$LOGFILE"

# Launch command, stdout → log
"$@" > "$LOGFILE" &
WRAPPER_PID=$!

echo "WRAPPER PID=$WRAPPER_PID  CAP=${MAX_GB}G  LOG=$LOGFILE"

# Wait for writers to appear (build phase first)
echo "Waiting for binary to start writing..."
while kill -0 $WRAPPER_PID; do
    PIDS=$(lsof -t "$LOGFILE")
    COUNT=$(echo "$PIDS" | grep -c .)
    if [ "$COUNT" -gt 1 ]; then
        echo "Found $COUNT writers on $LOGFILE:"
        for p in $PIDS; do
            echo "  PID=$p  $(ps -o comm= -p $p)"
        done
        break
    fi
    sleep 2
done

if [ -z "$PIDS" ]; then
    echo "Command exited before binary started."
    wait $WRAPPER_PID
    echo "DONE (exit=$?)"
    exit
fi

# Monitor loop — check any pid still alive
echo "---"
while lsof -t "$LOGFILE" > /dev/null; do
    sz=$(stat -c%s "$LOGFILE")
    hr=$(numfmt --to=iec "$sz")
    printf "\r[$(date +%H:%M:%S)] %s = %s   " "$LOGFILE" "$hr"

    if [ "$sz" -gt "$MAX_BYTES" ]; then
        PIDS=$(lsof -t "$LOGFILE")
        kill -STOP $PIDS
        sleep 1

        echo ""
        echo ""
        echo "========================================"
        echo "  CAPACITY REACHED: $hr > ${MAX_GB}G"
        echo "  STOPPED ALL:"
        for p in $PIDS; do
            STATE=$(ps -o stat= -p $p)
            NAME=$(ps -o comm= -p $p)
            echo "    PID=$p ($NAME) [state=$STATE]"
        done
        echo ""
        echo "  TO RESUME:  kill -CONT $PIDS"
        echo "  TO KILL:    kill $PIDS"
        echo "========================================"
        exit
    fi

    sleep 5
done

wait $WRAPPER_PID
echo ""
echo "DONE (exit=$?)"
