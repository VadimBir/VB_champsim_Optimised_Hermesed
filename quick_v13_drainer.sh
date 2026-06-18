#!/bin/bash
# quick_v13_drainer.sh — persistent drain-file consumer for quick_v13.sh --drain queues.
# Consumption strategy: sidecar <drain>.cursor holding the next unread line number (1-based).
# Lines already consumed are never deleted; the cursor simply advances past them.
# Robust to the drain file growing while the drainer is running and to missing EOF newline.

set -euo pipefail

# Resolve REPO_ROOT from the drainer's own location (not from pwd).
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# === CONFIG ===
DEFAULT_DRAIN_FILE="./cc_runs/drain.txt"
DEFAULT_OUT_DIR="./cc_runs/drain_logs"
POLL_INTERVAL=0.5   # seconds

# ---------------------------------------------------------------------------
# Argument parsing — positional OR flag form
# ---------------------------------------------------------------------------
_usage() {
  cat <<EOF
Usage: $0 <input.txt> <output_dir>
       $0 --in <input.txt> --out <output_dir>

Polls <input.txt> every POLL_INTERVAL seconds, runs each new line as a sim CLI,
and pipes its stdout+stderr to <output_dir>/<JOBID>__<bin-name>__<trace-short>__<YYMMDDHHMM>.log.

Log filename format: <JOBID>__<bp>-<l1byp>-<l2byp>-<l3byp>-<repl>-<cores>core-ByP-<L1pf>-<L2pf>-<L3pf>-<win>-<hermes>__<trace-short>__<YYMMDDHHMM>.log
Example: 0042__perceptron-no-no-no-lru-2core-ByP-no-no-no-fixed-off__LLM256.Pythia-70M_21M__2604291205.log

Job IDs are 4-digit zero-padded monotonic integers (0001, 0042, 9999, 10000, …).
Counter starts at 1 each drainer session (in-memory; no persistence).
All logs share the same timestamp suffix = drainer start time.
Each dispatched process receives BYP_JOB_ID=<id> in its environment.
After each job completes, cc_runs/_append_log.py is invoked automatically.

Defaults (used if arg not given):
  input  = $DEFAULT_DRAIN_FILE
  output = $DEFAULT_OUT_DIR
  poll   = $POLL_INTERVAL s
EOF
}

DRAIN_FILE=""
OUT_DIR=""

# Parse flags or positional args
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      _usage
      exit 0
      ;;
    --in)
      DRAIN_FILE="${2:-}"
      shift 2
      ;;
    --out)
      OUT_DIR="${2:-}"
      shift 2
      ;;
    -*)
      echo "Unknown flag: $1" >&2
      _usage >&2
      exit 1
      ;;
    *)
      # Positional: first non-flag = input, second = output
      if [[ -z "$DRAIN_FILE" ]]; then
        DRAIN_FILE="$1"
      elif [[ -z "$OUT_DIR" ]]; then
        OUT_DIR="$1"
      else
        echo "Unexpected extra argument: $1" >&2
        _usage >&2
        exit 1
      fi
      shift
      ;;
  esac
done

# Apply defaults
DRAIN_FILE="${DRAIN_FILE:-$DEFAULT_DRAIN_FILE}"
OUT_DIR="${OUT_DIR:-$DEFAULT_OUT_DIR}"

# ---------------------------------------------------------------------------
# Setup
# ---------------------------------------------------------------------------
CURSOR_FILE="${DRAIN_FILE}.cursor"
DONE=0

# Session-scoped: start time (shared suffix for all logs) + in-memory job counter.
START_TIME="$(date +%y%m%d%H%M)"
JOB_COUNTER=1

mkdir -p "$OUT_DIR"

_cleanup() {
  DONE=1
  echo "[drainer] $(date +%H:%M:%S) caught signal — shutting down cleanly"
}
trap '_cleanup' INT TERM

# Read or initialise cursor (line number of next line to consume, 1-based)
_read_cursor() {
  if [[ -f "$CURSOR_FILE" ]]; then
    local v
    v=$(< "$CURSOR_FILE")
    [[ "$v" =~ ^[0-9]+$ ]] && echo "$v" || echo "1"
  else
    echo "1"
  fi
}

_write_cursor() {
  printf '%s\n' "$1" > "$CURSOR_FILE"
}

# Allocate the next job ID: print current counter (no padding), then increment.
# Counter is in-memory (resets to 1 each drainer session).
_next_jobid() {
  printf '%s' "$JOB_COUNTER"
  JOB_COUNTER=$(( JOB_COUNTER + 1 ))
}

# Derive a log filename from the CLI string and a pre-allocated job ID.
#
# New format: <JOBID>__<bp>-<l1byp>-<l2byp>-<l3byp>-<repl>-<cores>core-ByP-<L1pf>-<L2pf>-<L3pf>-<win>-<hermes>__<trace-short>__<YYMMDDHHMM>.log
#
# Mirrors the bin-name-building logic in quick_v13.sh / qbuildPrefetcher_v13.sh.
# Falls back to "unknown" for any field that cannot be parsed from the CLI.
_jobname() {
  local jobid="$1"
  local cmd="$2"

  # ---- extract named flags ----
  local bp="perceptron"
  local l1byp="no" l2byp="no" l3byp="no"
  local repl="lru"
  local cores="2"
  local win="fixed"
  local hermes="off"
  local l1pf="no" l2pf="no" l3pf="no"
  local trace_short="notrace"

  [[ "$cmd" =~ --bp[[:space:]]+([^[:space:]]+)         ]] && bp="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ --repl[[:space:]]+([^[:space:]]+)       ]] && repl="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ --win-mode[[:space:]]+([^[:space:]]+)   ]] && win="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ --hermes[[:space:]]+([^[:space:]]+)     ]] && hermes="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ -c[[:space:]]+([0-9]+)                  ]] && cores="${BASH_REMATCH[1]}"

  # Bypass models: --allByP sets all three; individual flags override per-level.
  if [[ "$cmd" =~ --allByP[[:space:]]+([^[:space:]]+) ]]; then
    local m="${BASH_REMATCH[1]}"
    l1byp="$m"; l2byp="$m"; l3byp="$m"
  fi
  [[ "$cmd" =~ --l1byp[[:space:]]+([^[:space:]]+)      ]] && l1byp="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ --l2byp[[:space:]]+([^[:space:]]+)      ]] && l2byp="${BASH_REMATCH[1]}"
  [[ "$cmd" =~ --l3byp[[:space:]]+([^[:space:]]+)      ]] && l3byp="${BASH_REMATCH[1]}"

  # Positional args: strip all --flag val pairs and script name, leaving: <trace> <L1pf> <L2pf> <L3pf>
  local positional
  # shellcheck disable=SC2001
  positional="$(echo "$cmd" \
    | sed 's/--[a-zA-Z0-9_-]*[[:space:]]\+[^[:space:]]\+//g' \
    | sed 's/-c[[:space:]]\+[0-9]\+//g' \
    | sed 's/\.[\/]*qrun_champsim_v13\.sh\|\.\/quick_v13\.sh//g')"
  local -a toks=()
  for tok in $positional; do
    [[ -z "$tok" ]] && continue
    toks+=("$tok")
  done
  # Layout: toks[0]=trace toks[1]=L1pf toks[2]=L2pf toks[3]=L3pf
  local ntoks=${#toks[@]}
  if [[ "$ntoks" -ge 4 ]]; then
    trace_short="$(basename "${toks[0]}")"
    trace_short="${trace_short%%.*}"
    l1pf="${toks[$((ntoks-3))]}"
    l2pf="${toks[$((ntoks-2))]}"
    l3pf="${toks[$((ntoks-1))]}"
  elif [[ "$ntoks" -ge 1 ]]; then
    trace_short="$(basename "${toks[0]}")"
    trace_short="${trace_short%%.*}"
    if [[ "$ntoks" -ge 3 ]]; then
      l1pf="${toks[$((ntoks-3))]}"
      l2pf="${toks[$((ntoks-2))]}"
      l3pf="${toks[$((ntoks-1))]}"
    fi
  fi

  if [ "$l1byp" = "$l2byp" ] && [ "$l2byp" = "$l3byp" ]; then
    local byp_tag="ByP-${l1byp}"
  else
    local byp_tag="ByP-${l1byp}-${l2byp}-${l3byp}"
  fi
  local bin_name="${bp}-${l1pf}-${l2pf}-${l3pf}-${repl}-${cores}core-${byp_tag}-${win}-${hermes}"
  echo "${START_TIME}_${jobid}_${bin_name}_${trace_short}"
}

echo "[drainer] $(date +%H:%M:%S) watching $DRAIN_FILE → logs to $OUT_DIR @ ${POLL_INTERVAL}s (session ts=${START_TIME})"

# ---------------------------------------------------------------------------
# Main poll loop
# ---------------------------------------------------------------------------
while [[ "$DONE" -eq 0 ]]; do
  # Wait for drain file to appear
  if [[ ! -f "$DRAIN_FILE" ]]; then
    sleep "$POLL_INTERVAL"
    continue
  fi

  CURSOR=$(_read_cursor)

  # Count total lines available (handles missing trailing newline via mapfile)
  mapfile -t ALL_LINES < "$DRAIN_FILE" || true
  TOTAL=${#ALL_LINES[@]}

  if [[ "$CURSOR" -le "$TOTAL" ]]; then
    # Lines are 1-based; array is 0-based
    IDX=$(( CURSOR - 1 ))
    CMD="${ALL_LINES[$IDX]}"

    # Skip blank lines
    if [[ -z "${CMD// /}" ]]; then
      _write_cursor $(( CURSOR + 1 ))
      continue
    fi

    JOBID="$JOB_COUNTER"
    JOB_COUNTER=$(( JOB_COUNTER + 1 ))
    JOBNAME="$(_jobname "$JOBID" "$CMD")"
    LOG_FILE="${OUT_DIR}/${JOBNAME}.log"
    DISPLAY="${CMD:0:100}"
    echo "[drainer] $(date +%H:%M:%S) [job ${JOBID}] running ${JOBNAME}: ${DISPLAY}"

    # Advance cursor BEFORE running so a crash doesn't re-run the same job
    _write_cursor $(( CURSOR + 1 ))

    # Concurrency gate: cap parallel sims at MAX_CONCURRENT (default 5)
    : "${MAX_CONCURRENT:=5}"
    while [[ "$(pgrep -c -f 'simulation_instructions ')" -ge "$MAX_CONCURRENT" ]]; do
      sleep 1
    done

    # Dispatch in background so the drainer can pick the next line.
    # Auto-fold completed log into cc_runs/GLOBAL.md after the sim exits.
    (
      BYP_JOB_ID="${JOBID}" bash -c "$CMD" >> "$LOG_FILE" 2>&1 \
        || echo "[drainer] $(date +%H:%M:%S) WARNING: job ${JOBID} exited non-zero (line $CURSOR) — log: $LOG_FILE"
      # Extract real trace name from log and rename file
      _trace="$(grep -m1 '^trace_0 ' "$LOG_FILE" | sed 's|.*/||' | sed 's|\.champsimtrace.*||')"
      if [[ -n "$_trace" && "$_trace" != "notrace" ]]; then
        _newlog="${LOG_FILE%_notrace.log}_${_trace}.log"
        if [[ "$LOG_FILE" != "$_newlog" ]]; then
          mv "$LOG_FILE" "$_newlog"
          LOG_FILE="$_newlog"
        fi
      fi
      if [[ -f "$REPO_ROOT/cc_runs/_log_to_global.py" ]]; then
        python3 "$REPO_ROOT/cc_runs/_log_to_global.py" "$LOG_FILE" \
          || echo "[drainer] WARN: _log_to_global.py failed for $JOBNAME (continuing)"
      fi
    ) &
  else
    # Nothing new — poll
    sleep "$POLL_INTERVAL"
  fi
done

echo "[drainer] $(date +%H:%M:%S) exited"
