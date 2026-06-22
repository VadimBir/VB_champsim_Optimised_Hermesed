#!/bin/sh
# qrun_v14_win.sh — native-Windows (w64devkit sh) port of qrun_champsim_v14.sh.
# Runs INSIDE champsim_v17 so ../traces and bin/ resolve like the Makefile.
#
# Flag names VERIFIED against src/main.cc long_options[] (getopt_long_only):
#   -warmup_instructions ('w')   -simulation_instructions ('i')
#   --db ('D')  --arch ('A')  --bypass ('B')  --pf_l1 ('1') --pf_l2 ('2') --pf_l3 ('3')
#   --traces ('t')
# NOTE: the Linux qrun used "-warmup"; main.cc only defines "warmup_instructions".
# getopt_long_only would accept "-warmup" as an unambiguous prefix, but we use the
# FULL canonical name -warmup_instructions to avoid any ambiguity. Same for traces.
#
# DROPPED vs Linux qrun (Linux-only / not requested): perf record, gdb, profiling
# attach, sleep/kill perf branches. These are no-op'd with a clear echo.
#
# BANNED per project rule: no 2>/dev/null, 2>&1, ||true, ||echo 0.
#
# Usage:
#   sh qrun_v14_win.sh <trace_substring> [<L1> <L2> <L3>]
#     [--bp BP] [--repl R] [--win-mode M] [--hermes H]
#     [--l1byp M] [--l2byp M] [--l3byp M] [--arch A]
# cores via NUM_CORES env (keep consistent with the build).

set -e

arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-1}"
TRACE_PERCENT="${TRACE_PERCENT:-20}"
tracesDirName="${tracesDirName:-../traces}"
isTimeBin="${isTimeBin:-0}"
DB_FNAME="${DB_FNAME:-./champsim_results/champsim_results.db}"

BP="${BP:-perceptron}"
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
OCP="${OCP:-off}"          # hermes tag in binary name
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"

POSITIONAL=""
while [ $# -gt 0 ]; do
  case "$1" in
    --bp)       BP="$2"; shift 2 ;;
    --repl)     REPL="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   OCP="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    --arch)     arch="$2"; shift 2 ;;
    --profile)  echo "[win] --profile (perf/gdb) is Linux-only; ignored on Windows." ; shift ;;
    *)          POSITIONAL="$POSITIONAL $1"; shift ;;
  esac
done
set -- $POSITIONAL

if [ $# -ne 1 ] && [ $# -ne 4 ] && [ $# -ne 5 ]; then
  echo "Usage: $0 [--bp BP] [--repl R] [--win-mode M] [--hermes H] [--l1byp M] [--l2byp M] [--l3byp M] <trace_substring> [<L1> <L2> <L3>]"
  exit 1
fi
if [ $# -eq 4 ] || [ $# -eq 5 ]; then
  prefetcher_L1="$2"
  prefetcher_L2="$3"
  prefetcher_L3="$4"
fi
[ $# -eq 5 ] && SHOW_BIN_RUN=1

# --- match trace by substring in ../traces ---
trace_file=$(find "${tracesDirName}" -maxdepth 1 -name "*$1*.xz" | head -n 1)
if [ -z "$trace_file" ]; then
  echo "No trace file found containing: $1"
  exit 1
fi
trace_name=$(basename "$trace_file" .champsimtrace.xz)

# --- WARMUP/SIM rules (identical to Linux qrun) ---
case "$trace_name" in
    [0-9][0-9][0-9].*)
        WARMUP="50000000"; SIM="200000000"
        echo "SPEC trace: $trace_name"
        ;;
    LLM*)
        trace_count=$(echo "$trace_name" | sed 's/.*_\([0-9]\+\)M.*/\1/')
        simTraces_num=$((trace_count * 1000000))
        percent=$(( (simTraces_num * TRACE_PERCENT) / 100 ))
        toWarmUp=$(( (percent / 1000000) * 1000000 ))
        toSim=$((simTraces_num - toWarmUp - toWarmUp))
        WARMUP="$toWarmUp"
        SIM="$toSim"
        echo "LLM trace: ${trace_count}M -> Warmup=$WARMUP, Sim=$SIM"
        ;;
    *)
        WARMUP="50000000"; SIM="200000000"
        ;;
esac

echo "Total instructions: $simTraces_num"
echo "Warmup instructions: $WARMUP"
echo "Simulation instructions: $SIM"

# --- descriptive binary name (mirror qrun BYP_SEGMENT) ---
if [ "$L1_BYP_MODEL" = "$L2_BYP_MODEL" ] && [ "$L2_BYP_MODEL" = "$L3_BYP_MODEL" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}"
else
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL}"
fi
BIN="./bin/${BP}-${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3}-${REPL}-${NUM_CORES}core-${BYP_SEGMENT}-${WIN_MODE}-${OCP}.exe"

if [ ! -f "$BIN" ]; then
  echo "[ERROR] Binary not found: $BIN"
  echo "        Build it first with qbuild_v14_win.sh using matching options."
  exit 1
fi

mkdir -p "$(dirname "$DB_FNAME")"

# --- repeat the trace NUM_CORES times for --traces ---
args=$(yes "$trace_file" | head -n "$NUM_CORES" | paste -sd' ')

echo "=== Running on trace: $(basename "$trace_file") === Warmup: $WARMUP | Sim: $SIM ==="
echo "Binary: $BIN"

if [ "${SHOW_BIN_RUN:-0}" -eq 1 ]; then
  echo "BINARY RUN (dry, gdb is Linux-only and skipped on Windows):"
  echo "$BIN -warmup_instructions $WARMUP -simulation_instructions $SIM --db $DB_FNAME --arch ${arch} --bypass ${BYPASS_MODEL:-none} --pf_l1 $prefetcher_L1 --pf_l2 $prefetcher_L2 --pf_l3 $prefetcher_L3 -traces $args"
  exit 0
fi

BIN_ARGS="-warmup_instructions $WARMUP -simulation_instructions $SIM \
    --db $DB_FNAME --arch ${arch} --bypass ${BYPASS_MODEL:-none} \
    --pf_l1 $prefetcher_L1 --pf_l2 $prefetcher_L2 --pf_l3 $prefetcher_L3 \
    -traces $args"

time_start() { [ "$isTimeBin" -eq 1 ] && TIME_BIN_START=$(date +%s%N); return 0; }
time_end() {
    if [ "$isTimeBin" -eq 1 ]; then
        TIME_BIN_END=$(date +%s%N)
        ELAPSED_NS=$((TIME_BIN_END - TIME_BIN_START))
        ELAPSED_S=$(awk "BEGIN { printf \"%.3f\", $ELAPSED_NS / 1000000000 }")
        ELAPSED_M=$(awk "BEGIN { printf \"%.2f\", $ELAPSED_NS / 60000000000 }")
        echo "=== BINARY TIME: ${ELAPSED_S}s (${ELAPSED_M}m) ==="
    fi
    return 0
}

echo "Profiling disabled (perf/gdb are Linux-only, removed on Windows)"
RUN_LOG="$(dirname "$DB_FNAME")/qrun_last.log"
time_start
# w64devkit sh is NOT bash (no PIPESTATUS). Run the binary directly (no pipe) so
# $? is its REAL exit code; capture output to a log AND echo it live afterward.
# set -e relaxed so a non-zero rc is reported, not silently aborted.
set +e
"$BIN" $BIN_ARGS > "$RUN_LOG"
RUN_RC=$?
set -e
cat "$RUN_LOG"
time_end

if [ "$RUN_RC" -ne 0 ]; then
  echo "[ERROR] Binary exited non-zero (rc=$RUN_RC)"
  exit "$RUN_RC"
fi

echo "================ RUN RESULT ================"
grep "FINAL ROI CORE AVG IPC" "$RUN_LOG" | tail -n 1
exit 0
