#!/bin/sh

# Load config
arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-16}"
TRACE_PERCENT="${TRACE_PERCENT:-20}"
tracesDirName="${tracesDirName:-traces}"
isDebug="${isDebug:--1}"
doMinSim="${doMinSim:-0}"
isProfile="${isProfile:-0}"
isTimeBin="${isTimeBin:-0}"
DB_FNAME="${DB_FNAME:-./champsim_results/champsim_results.db}"

champsimDirName="${champsimDirName:-champsim_v14}"
PfBuilder=qbuildPrefetcher_v14
PfRunner=qrun_champsim_v14

BP="${BP:-perceptron}"
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
OCP="${OCP:-off}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"

FAST_WARMUP=20000000
FAST_SIM=5000000

# Parse named flags, collect positional args
POSITIONAL=""
while [ $# -gt 0 ]; do
  case "$1" in
    --dir)      champsimDirName="$2"; shift 2 ;;
    --bp)       BP="$2"; shift 2 ;;
    --repl)     REPL="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   OCP="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    --profile)
      if [ -n "$2" ] && echo "$2" | grep -qE '^[0-9]+$'; then
        PERF_DELAY="$2"; shift 2
      elif [ -n "$2" ] && ! echo "$2" | grep -qE '^-'; then
        PERF_FNAME="$2"; shift 2
        if [ -n "$2" ] && echo "$2" | grep -qE '^[0-9]+$'; then
          PERF_DELAY="$2"; shift 2
        fi
      else
        shift
      fi
      isProfile=1
      ;;
    *)          POSITIONAL="$POSITIONAL $1"; shift ;;
  esac
done
set -- $POSITIONAL

if [ $# -ne 1 ] && [ $# -ne 4 ] && [ $# -ne 5 ]; then
  echo "Usage: $0 [--dir DIR] [--bp BP] [--repl REPL] [--win-mode MODE] [--hermes OCP] [--l1byp M] [--l2byp M] [--l3byp M] <trace_name_substring> [<L1> <L2> <L3>]"
  exit 1
fi
if [ $# -eq 4 ]; then
  prefetcher_L1="$2"
  prefetcher_L2="$3"
  prefetcher_L3="$4"
fi
 if [ $# -eq 5 ]; then
   prefetcher_L1="$2"
   prefetcher_L2="$3"
   prefetcher_L3="$4"
   SHOW_BIN_RUN=1
 fi



# Match trace based on user arg
trace_file=$(find "${tracesDirName}" -maxdepth 1 -name "*$1*.xz" | head -n 1)
trace_name=$(basename "$trace_file" .champsimtrace.xz)

case "$trace_name" in
    [0-9][0-9][0-9].*)
        # SPEC traces: fixed values
        WARMUP="50000000"; SIM="200000000"
        echo "SPEC trace: $trace_name"
        ;;
    LLM*)
        # Extract number before _*M (331, 125, etc.)
        trace_count=$(echo "$trace_name" | sed 's/.*_\([0-9]\+\)M.*/\1/')
        
        # Convert to simTraces_num and use your existing calculation
        simTraces_num=$((trace_count * 1000000))
        percent=$(( (simTraces_num * TRACE_PERCENT) / 100 ))
        traceBoundround_up=$(( (percent / 1000000) * 1000000 ))
        toWarmUp=$(( (percent / 1000000) * 1000000 ))
        # toSim=$((simTraces_num - 5 * toWarmUp / 2))
        toSim=$((simTraces_num - toWarmUp - toWarmUp))
        
        WARMUP="$toWarmUp"
        SIM="$toSim"
        
        echo "LLM trace: ${trace_count}M → Warmup=$WARMUP, Sim=$SIM"
        ;;
    *)
        WARMUP="50000000"; SIM="200000000"
        ;;
esac


# simTraces_num=$(echo "$trace_file" | grep -oP 'memTraces\[\K[0-9]+(?=\]_FE)')

# # Compute warmup and simulation instructions from TRACE_PERCENT
# percent=$(( (simTraces_num * $TRACE_PERCENT) / 100 ))
# traceBoundround_up=$(( (percent / 1000000) * 1000000 ))
# toWarmUp=$(( (percent / 1000000) * 1000000 ))
# toSim=$((simTraces_num-toWarmUp-toWarmUp))

echo "Total instructions: $simTraces_num"
echo "Warmup instructions: $toWarmUp"
echo "Simulation instructions: $toSim"

if [ -z "$trace_file" ]; then
  echo "No trace file found containing: $1"
  exit 1
fi

if [ "$L1_BYP_MODEL" = "$L2_BYP_MODEL" ] && [ "$L2_BYP_MODEL" = "$L3_BYP_MODEL" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}"
else
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL}"
fi
BIN="./${champsimDirName}/bin/${BP}-${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3}-${REPL}-${NUM_CORES}core-${BYP_SEGMENT}-${WIN_MODE}-${OCP}"
# perf: no sudo, fp call-graph, readable output
# PERF_EVENTS overrides default event (cycles:u). E.g. "branch-misses,cache-misses"
PERF_DATA_FILE="${PERF_FNAME:-perf}.data"
PERF_EVENT_SPEC="${PERF_EVENTS:-cycles:u}"
# PERF="perf record -e ${PERF_EVENT_SPEC} --call-graph=fp -F 7489 --mmap-pages=65536 --buildid-all -o ./${PERF_DATA_FILE}"
PERF="perf record -e ${PERF_EVENT_SPEC} --call-graph=fp -F 7489 --mmap-pages=65536 --buildid-all -o ./${PERF_DATA_FILE}"
echo "Perf config: events=${PERF_EVENT_SPEC} output=${PERF_DATA_FILE}"

PERF_COLLECT_INTERVAL=200 # seconds (15 minutes)
PERF_BEGIN_AT_TIME=120000 # ms
# PERF="perf record -e cycles -F 7019 --call-graph=dwarf,512 --delay=$PERF_BEGIN_AT_TIME --mmap-pages=32768 --buildid-all -o ./$PERF_DATA_FILE"
# PERF="perf record -e cycles -F 7019 --call-graph=dwarf,512 --delay=1800000 --mmap-pages=32768 --buildid-all -o ./perf_v4.data -- sleep 900"

# PERF="AMDuProfCLI collect --config ibs --cpu 48-63 --affinity 48-63 -o amdBin.perf"
# PERF="sudo perf mem record  -a  --ldlat=1  --data  --phys-data  --sample-cpu  -g  --call-graph dwarf,65528  -T  -P  -v  -o champsim_memory.data"

# $PERF ${BINARY} -warmup_instructions ${N_WARM}000000 -simulation_instructions ${N_SIM}000000 ${OPTION} -traces ${TRACE}

tmpSim=$toSim
tmpWarm=$toWarmUp
#if [ "$doMinSim" -eq 1 ]; then
#    toWarmUp=10000
#    toSim=20000000
#    # if [ "$isProfile" -eq 1 ]; then
#    #     toWarmUp=$tmpWarm
#    #     toSim=$tmpSim
#    # fi
#fi
echo "DB: $toSim | $toWarmUp | $tmpSim | $tmpWarm | $doMinSim | $isProfile"
# Run binary on matched trace
echo "=== Running on trace: $(basename "$trace_file") === Warmup: $WARMUP | Sim: $SIM ==="
echo "Binary: $BIN"
args=$(yes "$trace_file" | head -n "$NUM_CORES" | paste -sd' ')

# echo "$BIN -warmup $WARMUP -simulation_instructions $SIM -traces $args"
# exit 0


# Set warmup and simulation values
if [ "$doMinSim" -eq 1 ]; then
    WARMUP=$FAST_WARMUP
    SIM=$FAST_SIM
    echo "Doing Min Run"
else
    # Use the calculated WARMUP and SIM from trace parsing above
    echo "Using calculated values: Warmup=$WARMUP, Sim=$SIM"
fi

 if [ "$SHOW_BIN_RUN" -eq 1 ]; then
     echo "BINARY RUN:"
     echo "$BIN -warmup $WARMUP -simulation_instructions $SIM -traces $args"
    #  gdb --args "$BIN" -warmup "$WARMUP" -simulation_instructions "$SIM" -traces "$args" -ex run
    gdb "$BIN" \
    -ex "r -warmup $WARMUP -simulation_instructions $SIM -traces $args"
    
    # gdb --batch \
    # -ex "run -warmup $WARMUP -simulation_instructions $SIM -traces $args" \
    # -ex "quit" \
    # --args "$BIN"
#     gdbserver localhost:1234 ./champsim_v6_Edge/bin/perceptron-no-no-no-lru-1core \
# -warmup 50000000 -simulation_instructions 200000000 -traces traces/649.fotonik3d_s-10881B.champsimtrace.xz
# gdbserver :1234 ./champsim_v6_Edge/bin/perceptron-no-no-no-lru-1core -warmup 50000000 -simulation_instructions 200000000 -traces traces/649.fotonik3d_s-10881B.champsimtrace.xz
# gdb -ex "cd $(pwd)"     -ex "file ./champsim_v6_Edge/bin/perceptron-no-no-no-lru-1core"     -ex "break main.cc:578"     -ex 'run -warmup 50000000 -simulation_instructions 200000000 -traces traces/649.fotonik3d_s-10881B.champsimtrace.xz'

     exit 0

 fi


# Build the common binary args
BIN_ARGS="-warmup $WARMUP -simulation_instructions $SIM \
    --db $DB_FNAME --arch ${arch:-glc} --bypass ${BYPASS_MODEL:-none} \
    --pf_l1 $prefetcher_L1 --pf_l2 $prefetcher_L2 --pf_l3 $prefetcher_L3 \
    -traces $args"

# Timing wrapper
time_start() { [ "$isTimeBin" -eq 1 ] && TIME_BIN_START=$(date +%s%N); }
time_end() {
    if [ "$isTimeBin" -eq 1 ]; then
        TIME_BIN_END=$(date +%s%N)
        ELAPSED_NS=$((TIME_BIN_END - TIME_BIN_START))
        ELAPSED_S=$(awk "BEGIN { printf \"%.3f\", $ELAPSED_NS / 1000000000 }")
        ELAPSED_M=$(awk "BEGIN { printf \"%.2f\", $ELAPSED_NS / 60000000000 }")
        echo "=== BINARY TIME: ${ELAPSED_S}s (${ELAPSED_M}m) ==="
    fi
}

# Run simulation
if [ "$isProfile" -eq 1 ]; then
    echo "Profiling enabled"
    time_start
    "$BIN" $BIN_ARGS &
    BIN_PID=$!
    PERF_DELAY="${PERF_DELAY:-900}"
    echo "Binary PID: $BIN_PID — waiting ${PERF_DELAY}s before attaching perf..."
    sleep "$PERF_DELAY"
    $PERF -p $BIN_PID &
    PERF_PID=$!
    wait $BIN_PID
    time_end
    kill -INT $PERF_PID 2>/dev/null
    for i in $(seq 1 10); do
        kill -0 $PERF_PID 2>/dev/null || break
        sleep 1
    done
    kill -KILL $PERF_PID 2>/dev/null
    wait $PERF_PID 2>/dev/null
    chmod 644 "./${PERF_DATA_FILE}" 2>/dev/null
    echo "Perf data: ./${PERF_DATA_FILE}"
    # gdb -ex "cd $(pwd)" \
    # -ex "set non-stop on" \
    # -ex "set pagination off" \
    # -ex "file ./champsim_v6_Edge/bin/perceptron-no-no-no-lru-1core" \
    # -ex "break main.cc:578" \
    # -ex "run -warmup 50000000 -simulation_instructions 200000000 -traces traces/649.fotonik3d_s-10881B.champsimtrace.xz" \
    # -ex "continue &"

else
    echo "Profiling disabled"
    time_start
    "$BIN" $BIN_ARGS
    time_end
fi


# if [ "$isProfile" -eq 1 ]; then
#   echo "Profiling enabled"
#   echo "$PERF $BIN   -warmup 100000   -simulation_instructions 10000000   -traces $args" 
#   $PERF "$BIN"   -warmup 1000000   -simulation_instructions 5000000   -traces $args # "$trace_file" "${trace_file}" # "${trace_file}" "${trace_file}" "$trace_file" "${trace_file}" "${trace_file}" "${trace_file}" 
# fi
# if [ "$doMinSim" -eq 1 ]; then
#   echo "Doing Min Run"
#   "$BIN"   -warmup 4000000  -simulation_instructions 13000000 -traces $args # "$trace_file" "${trace_file}" # "${trace_file}" "${trace_file}" "$trace_file" "${trace_file}" "${trace_file}" "${trace_file}" 
# fi
# if [ "$isProfile" -eq 0 ]; then
#   echo "Profiling disabled"
#   if [ "$doMinSim" -eq 0 ]; then
#   "$BIN"   -warmup 50000000   -simulation_instructions 200000000   -traces $args # "$trace_file" "${trace_file}" # "${trace_file}" "${trace_file}" "$trace_file" "${trace_file}" "${trace_file}" "${trace_file}"
#   # "$BIN"   -warmup 50000000   -simulation_instructions 200000000   -traces $args # "$trace_file" "${trace_file}" # "${trace_file}" "${trace_file}" "$trace_file" "${trace_file}" "${trace_file}" "${trace_file}"
#   # "$BIN"   -warmup 0   -simulation_instructions 17000000   -traces $args # "$trace_file" "${trace_file}" # "${trace_file}" "${trace_file}" "$trace_file" "${trace_file}" "${trace_file}" "${trace_file}"
#   fi
# fi

# "$BIN" \
#   -warmup 100000 \
#   -simulation_instructions 19000000 \
#   -traces "${trace_file}" "${trace_file}" "${trace_file}" "${trace_file}"
