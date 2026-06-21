#!/bin/bash

# Self-locate: run from anywhere. Resources are repo-root-relative; siblings live in $HERE.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"   # repo root = parent of quickSim/
cd "$ROOT" || { echo "FATAL: cannot cd to repo root $ROOT"; exit 1; }

export arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-2}"
TRACE_PERCENT="${TRACE_PERCENT:-20}"
tracesDirName="${tracesDirName:-traces}"
isDebug="${isDebug:--1}"
doMinSim="${doMinSim:-0}"
isProfile="${isProfile:-0}"
# trace="Pythi"
# trace="OPT"
tracesDirName="${tracesDirName:-traces}"
# trace="LLM512.GPT"
# trace="654.roms_s-1007B"
# trace="410.bwaves-2097B"
# trace="429.mcf-22B"
# trace="459.GemsFDTD-1169B"
# trace="429.mcf-217B"
# trace="473.astar-359B"
trace="LLM256.Pythia-70M"
# trace="LLM256.OPT"
# trace="mcf-22B"
# trace="403.gcc-17B"
# trace="649.fotonik3d_s-10881"
# trace="605.mcf_s-1644B"
# trace="445.gobmk-30B"
# trace="483.xalancbmk-127B.champsimtrace"


PfBuilder=qbuildPrefetcher
PfRunner=qrun_champsim
BP="${BP:-perceptron}"
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
OCP="${OCP:-off}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"
# trace="623.xalancbmk_s-700B" # PATT is very very bad
L1="no"
L2="no"
L3="no"
L1=$prefetcher_L1
L2=$prefetcher_L2
L3=$prefetcher_L3

ByP_Type="Pf Based ByP"
DB_FNAME="./CHAMPSIM_RESULTS.db"
MALLOC_LIB=""
BYPASS_MODEL="no"
BYPASS_L2=0
BYPASS_LLC=0
PROCESSES_NUM=1

ByP_Model_BASE=""

L1_BYP_MODEL="no"
L2_BYP_MODEL="no"


mkdir -p $ARCHIVE_PATH
# Inject [model: FILENAME] prefix into startup cout — idempotent build-time injection
byp_cp() { local s="$1" d="$2" fn; fn=$(basename "$s"); sed "/\[model:/!s|cout << \"|cout << \"[model: ${fn}] |" "$s" > "$d"; }

# Partial-match resolver for bypass models and prefetchers.
# Usage: resolved=$(resolve_partial <search_dir> <suffix> <user_input>)
# suffix examples: ".l1_bypass", ".l2c_pref"
# Returns the basename (without suffix) on stdout; exits on 0 or ambiguous matches.
resolve_partial() {
  local dir="$1" suffix="$2" input="$3" label="${4:-file}"
  # Exact match — skip search
  [[ -f "$dir/${input}${suffix}" ]] && { echo "$input"; return 0; }
  # Partial: find all files whose basename (minus suffix) contains input (case-insensitive via nocasematch)
  local -a hits=()
  while IFS= read -r f; do
    [[ -n "$f" ]] && hits+=("$f")
  done < <(find "$dir" -maxdepth 1 -type f -iname "*${input}*${suffix}" ! -path "*/archive/*" 2>/dev/null)
  if (( ${#hits[@]} == 0 )); then
    echo -e "\033[1;91mERROR: No ${label} matching '*${input}*${suffix}' found\033[0m" >&2
    echo -e "\033[0;33mAvailable:\033[0m" >&2
    find "$dir" -maxdepth 1 -type f -name "*${suffix}" ! -path "*/archive/*" -printf "  %f\n" 2>/dev/null | sort >&2
    exit 1
  elif (( ${#hits[@]} > 1 )); then
    echo -e "\033[1;93mAmbiguous ${label} '${input}' — multiple matches:\033[0m" >&2
    for h in "${hits[@]}"; do echo "  $(basename "$h")" >&2; done
    exit 1
  fi
  # Single match — extract basename without suffix
  local base
  base=$(basename "${hits[0]}" "$suffix")
  [[ "$base" != "$input" ]] && echo -e "\033[1;92m${label} '${input}' → resolved to '${base}'\033[0m" >&2
  echo "$base"
}

parse_args() {
  echo "Function called with $# arguments: $@"
  shopt -s nocasematch
  # PRE-PASS: resolve --dir first so path-touching flags see the user-specified dir
  local _pp=("$@")
  for ((_i=0; _i<${#_pp[@]}; _i++)); do
    if [[ "${_pp[_i]}" == "--dir" ]]; then
      [[ -z "${_pp[_i+1]}" || "${_pp[_i+1]}" =~ ^- ]] && { echo "Error: --dir requires a value"; exit 1; }
      champsimDirName="${_pp[_i+1]}"
      ByP_NO_BASE="$champsimDirName/src/ByP_Models/no.l1_bypass"
      ARCHIVE_PATH="$champsimDirName/src/ByP_Models/archive"
      echo "[pre-pass] champsimDirName=$champsimDirName"
      break
    fi
  done
  while [[ $# -gt 0 ]]; do
    
    case $1 in
    --dir)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --dir requires a value"; exit 1; }
      champsimDirName="$2"; shift 2 ;;
    --bp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --bp requires a value"; exit 1; }
      BP="$2"; shift 2 ;;
    --repl)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --repl requires a value"; exit 1; }
      REPL="$2"; shift 2 ;;
    --win-mode)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --win-mode requires a value"; exit 1; }
      WIN_MODE="$2"; shift 2 ;;
    --ocp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --ocp requires a value (none|hermes|ttp|hmp)"; exit 1; }
      OCP="$2"; shift 2 ;;
    --malloc)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --malloc requires a value (mimalloc|jemalloc|none)"; exit 1; }
      case "$2" in
        mimalloc)
          MALLOC_LIB="$(ldconfig -p 2>/dev/null | grep -oP '/\S+libmimalloc\S*\.so[.0-9]*' | head -1)"
          [[ -n "$MALLOC_LIB" ]] || { echo -e "\033[1;91mERROR: mimalloc not found. Install: sudo apt install libmimalloc2.0\033[0m"; exit 1; }
          ;;
        jemalloc)
          MALLOC_LIB="$(ldconfig -p 2>/dev/null | grep -oP '/\S+libjemalloc\S*\.so[.0-9]*' | head -1)"
          [[ -n "$MALLOC_LIB" ]] || { echo -e "\033[1;91mERROR: jemalloc not found. Install: sudo apt install libjemalloc2\033[0m"; exit 1; }
          ;;
        none) MALLOC_LIB="" ;;
        *) echo "Error: --malloc accepts mimalloc|jemalloc|none"; exit 1 ;;
      esac
      shift 2 ;;
    --allByP|--allbyp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --allByP requires a value"; exit 1; }
      model=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l1_bypass" "$2" "bypass model")
      byp_cp "$champsimDirName/src/ByP_Models/${model}.l1_bypass" "$champsimDirName/src/ooo_l1_byp_model.cc"
      byp_cp "$champsimDirName/src/ByP_Models/${model}.l2_bypass" "$champsimDirName/src/ooo_l2_byp_model.cc"
      byp_cp "$champsimDirName/src/ByP_Models/${model}.llc_bypass" "$champsimDirName/src/ooo_llc_byp_model.cc"
      ByP_Type="Cache Based ByP"
      L1_BYP_MODEL="$model"; L2_BYP_MODEL="$model"; L3_BYP_MODEL="$model"
      BYPASS_MODEL="$model"; BYPASS_L2=1; BYPASS_LLC=1
      sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_L2_LOGIC|#define BYPASS_L2_LOGIC|' "./$champsimDirName/inc/champsim.h"
      sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_LLC_LOGIC|#define BYPASS_LLC_LOGIC|' "./$champsimDirName/inc/champsim.h"
      echo -e "\033[1;91mLoaded all-ByP model: $model\033[0m"
      shift 2 ;;
    -1|--L1)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      L1=$(resolve_partial "$champsimDirName/prefetcher" ".l1d_pref" "$2" "L1 prefetcher")
      shift 2 ;;
    -2|--L2)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      L2=$(resolve_partial "$champsimDirName/prefetcher" ".l2c_pref" "$2" "L2 prefetcher")
      shift 2 ;;
    -3|--L3)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      L3=$(resolve_partial "$champsimDirName/prefetcher" ".llc_pref" "$2" "LLC prefetcher")
      shift 2 ;;
    -bypca|-ByPca|-ByPcache|-CaByP)
        ByP_Type=$(grep -Eq '^[[:space:]]*#define[[:space:]]+BYPASS_L1D_OnNewMiss' "./$champsimDirName/inc/champsim.h" && echo "Cache Based ByP" || echo "Pf Based ByP"); shift ;;
    -ByPpf|-ByPpref|-PfByP)
        sed -i 's|^[[:space:]]*//\?[[:space:]]*#define BYPASS_L1D_OnNewMiss|// #define BYPASS_L1D_OnNewMiss|' "./$champsimDirName/inc/champsim.h"; shift ;;
    --l1byp|--L1byp|-l1byp)
        model=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l1_bypass" "$2" "L1 bypass model")
        src="$champsimDirName/src/ByP_Models/${model}.l1_bypass"
        dst="$champsimDirName/src/ooo_l1_byp_model.cc"
        ByP_NO_BASE=$dst
        byp_cp "$src" "$dst"
        ByP_Type="Cache Based ByP"
        L1_BYP_MODEL="$model"
        BYPASS_MODEL="$model"
        echo -e "\033[1;91mLoaded L1 bypass model: $model\033[0m"
        shift 2 ;;
    --l2byp|--L2byp|-l2byp)
        model=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l2_bypass" "$2" "L2 bypass model")
        src="$champsimDirName/src/ByP_Models/${model}.l2_bypass"
        dst="$champsimDirName/src/ooo_l2_byp_model.cc"
        byp_cp "$src" "$dst"
        BYPASS_L2=1
        L2_BYP_MODEL="$model"
        sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_L2_LOGIC|#define BYPASS_L2_LOGIC|' "./$champsimDirName/inc/champsim.h"
        echo -e "\033[1;91mLoaded L2 bypass model: $model\033[0m"
        shift 2 ;;
    --noL2byp|--nol2byp)
        BYPASS_L2=0
        sed -i 's|^[[:space:]]*#define BYPASS_L2_LOGIC|// #define BYPASS_L2_LOGIC|' "./$champsimDirName/inc/champsim.h"; shift ;;
    --L3byp|--l3byp|-L3byp)
        if [[ -z "$2" || "$2" =~ ^- ]]; then
            # No model name: just enable the define
            BYPASS_LLC=1
            sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_LLC_LOGIC|#define BYPASS_LLC_LOGIC|' "./$champsimDirName/inc/champsim.h"
            shift
        else
            model=$(resolve_partial "$champsimDirName/src/ByP_Models" ".llc_bypass" "$2" "LLC bypass model")
            src="$champsimDirName/src/ByP_Models/${model}.llc_bypass"
            dst="$champsimDirName/src/ooo_llc_byp_model.cc"
            byp_cp "$src" "$dst"
            BYPASS_LLC=1
            sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_LLC_LOGIC|#define BYPASS_LLC_LOGIC|' "./$champsimDirName/inc/champsim.h"
            echo -e "\033[1;91mLoaded LLC bypass model: $model\033[0m"
            shift 2
        fi
        ;;
    --noL3byp|--nol3byp)
        BYPASS_LLC=0
        sed -i 's|^[[:space:]]*#define BYPASS_LLC_LOGIC|// #define BYPASS_LLC_LOGIC|' "./$champsimDirName/inc/champsim.h"; shift ;;
    -ByPModel|-ByPmodel|-bypmodel)
        echo -e "\033[1;93mWARN: -ByPModel is deprecated, use --l1byp ModelName instead\033[0m"
        model=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l1_bypass" "$2" "bypass model")
        dst="$champsimDirName/src/ooo_l1_byp_model.cc"
        ByP_NO_BASE=$dst
        byp_cp "$champsimDirName/src/ByP_Models/${model}.l1_bypass" "$dst"
        BYPASS_MODEL="$model"
        L1_BYP_MODEL="$model"
        echo -e "\033[1;91mLoaded L1 bypass model: $model\033[0m"
        shift 2 ;;
    -d|--debug)
      [[ -z "$2" || ( "$2" =~ ^- && ! "$2" =~ ^-[0-9]+$ ) ]] && { echo "Error: --debug requires a value"; exit 1; }
      isDebug="$2"
      shift 2 ;;
    -f|--fast)
      doMinSim=1
      shift ;;
    -c|--cores)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --cores requires a value"; exit 1; }
      NUM_CORES="$2"
      shift 2 ;;
    --profile)
      isProfile=1
      # --profile                        → cycles:u, perf.data
      # --profile <name>                 → cycles:u, <name>.data
      # --profile <name> "<events>"      → custom events, <name>.data
      # --profile <name> "<events>" <delay>
      # Capture lookahead before any shifting (shift changes positional params)
      _p2="$2"; _p3="$3"; _p4="$4"
      shift 1  # consume --profile
      if [[ -n "$_p2" && ! "$_p2" =~ ^- ]]; then
        if [[ "$_p2" =~ ^[0-9]+$ ]]; then
          PERF_DELAY="$_p2"; shift 1
        else
          PERF_FNAME="$_p2"; shift 1
          if [[ -n "$_p3" && ! "$_p3" =~ ^- && ! "$_p3" =~ ^[0-9]+$ ]]; then
            PERF_EVENTS="$_p3"; shift 1
            if [[ -n "$_p4" && "$_p4" =~ ^[0-9]+$ ]]; then
              PERF_DELAY="$_p4"; shift 1
            fi
          elif [[ -n "$_p3" && "$_p3" =~ ^[0-9]+$ ]]; then
            PERF_DELAY="$_p3"; shift 1
          fi
        fi
      fi
      ;;
    --arch|--glc|--a14)
      case $1 in
        --glc) arch="glc" ;;
        --a14) arch="A14" ;;
        --arch) arch="$2"; shift ;;
      esac
      shift ;;
    --trace)
      trace="$2"
      shift 2;;
    -p|--processes_num)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --processes_num requires a value"; exit 1; }
      PROCESSES_NUM="$2"; shift 2 ;;
    --db)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --db requires a value"; exit 1; }
      DB_FNAME="$2"; shift 2 ;;
    --time)
      isTimeBin=1; shift ;;
    -h|--help) usage ;;
    *) echo "Error: Unknown option: $1"; echo ""; usage ;;
  esac
  
  done
}
champsimDirName="${champsimDirName:-champsim_v14}"
ByP_NO_BASE=$champsimDirName/src/ByP_Models/no.l1_bypass
ARCHIVE_PATH="$champsimDirName/src/ByP_Models/archive"
# # print all vars 
# echo "Initial vars:"
# echo "L1: $L1"
# echo "L2: $L2"
# echo "L3: $L3"
# echo "trace: $trace"
# echo "PROCESSES_NUM: $PROCESSES_NUM"
# echo "isDebug: $(grep isDebug config_fast.ini)"
# echo "doMinSim: $(grep doMinSim config_fast.ini)"
# echo "NUM_CORES: $(grep NUM_CORES config_fast.ini)"
# echo "isProfile: $(grep isProfile config_fast.ini)"
# echo ""

# exit 0

usage() {
  echo ""
  echo "Usage:"
  echo "  $0 [OPTIONS]"
  echo ""
  echo "Prefetcher selection:"
  echo "  -1, --L1 PREF             Select L1 prefetcher"
  echo "  -2, --L2 PREF             Select L2 prefetcher"
  echo "  -3, --L3 PREF             Select LLC prefetcher"
  echo ""
  echo "Architecture:"
  echo "  --glc                     Use GLC arch (default)"
  echo "  --a14                     Use A14 arch"
  echo "  --arch NAME               Use custom arch NAME"
  echo ""
  echo "Bypass control (choose one):"
  echo "  --ByPca, --ByPcache       Cache performs bypass"
  echo "  --ByPpf, --ByPpref        Prefetcher performs bypass"
  echo ""
  echo "Bypass model (requires --ByPca / -bypca):"
  echo "  --l1byp MODEL             L1 bypass model file (src/ByP_Models/<MODEL>.l1_bypass)"
  echo "  --l2byp MODEL             L2 bypass model file (src/ByP_Models/<MODEL>.l2_bypass)"
  echo "  --l3byp MODEL             LLC bypass model file (src/ByP_Models/<MODEL>.llc_bypass)"
  echo "    MODEL=no → no bypass    MODEL=<name> → load model"
  echo "  --L2byp                   Enable L2 bypass (no model)"
  echo "  --noL2byp                 Disable L2 bypass"
  echo "  --L3byp                   Enable LLC bypass (no model)"
  echo "  --noL3byp                 Disable LLC bypass"
  echo "  -ByPModel NAME            [DEPRECATED] use --l1byp instead"
  echo ""
  echo "Replacement policy:"
  echo "  --repl POLICY             Cache replacement policy (lru|srrip|drrip|...)"
  echo ""
  echo "OCP / Hermes:"
  echo "  --ocp MODE                Off-chip predictor mode (none|hermes|ttp|hmp)"
  echo ""
  echo "Simulation configuration:"
  echo "  --trace FILE              Trace file to run"
  echo "  -c, --cores N             Number of cores"
  echo "  -p, --processes_num N     Number of parallel processes"
  echo ""
  echo "Debug levels:"
  echo "  -d, --debug LEVEL"
  echo "        2   Full output: build + simulation"
  echo "        1   Full simulation output"
  echo "        0   Print IPC and result"
  echo "       -1   Print IPC only"
  echo "       -2   Time only"
  echo ""
  echo "Execution modes:"
  echo "  -f, --fast                Enable minimum simulation"
  echo "  --profile [name] [events] [START]  Enable profiling (perf, default event cycles:u → perf.data)"
  echo "                            START (seconds): 0=profile all, N=delay then profile to end"
  echo "                            (window START END is supported only via qrun_champsim.sh directly)"
  echo "  --time                    Time the binary execution (wall + user + sys)"
  echo ""
  echo "Other:"
  echo "  --dir DIR                 ChampSim directory to use (default: champsim_v14)"
  echo "  --malloc TYPE             Use custom allocator (mimalloc|jemalloc|none)"
  echo "  -h, --help                Show this help"
  echo ""
  echo "Examples:"
  echo "  $0 -1 no -2 no -3 ipcp -c 8 -p 4 --trace trace.champsimtrace"
  echo "  $0 --L1 next_line --L2 spp --L3 no --ByPca --debug 1"
  echo "  $0 -1 no -2 no -3 no --ByPpf -f --trace trace.champsimtrace"
  echo "  $0 -1 no -2 no -3 no -bypca --l1byp mymodel --l2byp mymodel --l3byp mymodel --ocp hermes --repl srrip --trace 256.Py"
  echo ""
  exit 1
}

parse_args "$@"
# Track L3 byp model from --l3byp branch above (BYPASS_LLC=1 set there)
[[ -z "${L3_BYP_MODEL:-}" || "$L3_BYP_MODEL" == "no" ]] && [[ "$BYPASS_LLC" == "1" && -n "${model:-}" ]] && L3_BYP_MODEL="$model"
if [[ -n "$MALLOC_LIB" ]]; then
  export LD_PRELOAD="$MALLOC_LIB"
  echo -e "\033[1;92mMalloc: LD_PRELOAD=$MALLOC_LIB\033[0m"
fi
export DB_FNAME BYPASS_MODEL doMinSim isDebug NUM_CORES isProfile PERF_DELAY PERF_FNAME PERF_EVENTS isTimeBin BYPASS_L2 BYPASS_LLC L1_BYP_MODEL L2_BYP_MODEL L3_BYP_MODEL BP REPL WIN_MODE OCP champsimDirName
echo "L1: $L1, L2: $L2, L3: $L3"
echo "isDebug=$isDebug doMinSim=$doMinSim NUM_CORES=$NUM_CORES isProfile=$isProfile"
echo -e "\e[31m$ByP_Type\e[0m"
echo "L1byp_model=$L1_BYP_MODEL L2byp_model=$L2_BYP_MODEL L2byp=$BYPASS_L2 L3byp=$BYPASS_LLC"
status=0
# arr of BUF_SZ values 
BUF_SZ=(512)
# echo "BUF_SZ: ${BUF_SZ[*]}"
for buf in "${BUF_SZ[@]}"; do
  # sed -i "s/^#define[ \t]\+BUF_SZ[ \t]\+[0-9]\+/#define BUF_SZ $buf/" ./pin_champsim/inc/ooo_cpu.h
  echo "=== BUF_SZ = $buf === TRACE: $trace"
  # sed -i 's/^#define[ \t]\+BUF_SZ[ \t]\+[0-9]\+/#define BUF_SZ '${buf}'/' ./pin_champsim/inc/ooo_cpu.h
  for n in $PROCESSES_NUM; do
    START=$(date +%s%3N)
    # mkdir -p profiles
    # export LLVM_PROFILE_FILE="profiles/PGO_${trace}_champsim_%p.profraw"
    # export LLVM_PROFILE_FILE="profiles/test_profile.profraw"
    # echo "LLVM_PROFILE_FILE: $LLVM_PROFILE_FILE"
    mid=$((n - 1))
    pids=()
    # if debug is 2 we do verbose build else void output 
    status=0
    echo "    *** BUILDING ***"
    if [ "$isDebug" -eq 2 ]; then
      echo "DEBUG: Building prefetcher with L1: $L1, L2: $L2, L3: $L3"
      output=$("$HERE/$PfBuilder".sh --dir "$champsimDirName" --repl "$REPL" --win-mode "$WIN_MODE" --hermes "$OCP" --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" "$L1" "$L2" "$L3")
      status=$?
      echo "$output"
      if [ $status -ne 0 ]; then
        redColour="\033[1;91m"; resetColour="\033[0m"
        echo -e "${redColour}*** BUILD FAIL (debug mode, exit=$status) ***${resetColour}"
        exit 1
      fi
    else
      output=$("$HERE/$PfBuilder".sh --dir "$champsimDirName" --repl "$REPL" --win-mode "$WIN_MODE" --hermes "$OCP" --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" "$L1" "$L2" "$L3" 2>&1)
      status=$?
      # set output colour red and use it in the error output 
      redColour="\033[1;91m"
      resetColour="\033[0m"
      greenColour="\033[1;92m" 
      if [ $status -ne 0 ]; then
        # echo "Build failed (exit code $status)"
        echo -e "${redColour}*** BUILD FAIL ***"
        # echo "$output" | grep -A 3 -E "error"
        echo -e "$output" | grep -B 4 -A 4 -E "error|Error" # | sed "s/^/\t/;s/$//"
        echo -e "${resetColour}"
        exit 1
      fi
      if [ $status -eq 0 ]; then
        echo -e "${greenColour}Build successful${resetColour}"
      fi
    fi

    sync

    if [ "$isDebug" -eq 10 ]; then
          # ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" # | grep -E "Finished CPU|now IPC :" # # > /dev/null 2>&1
          echo "$HERE/$PfRunner.sh --dir $champsimDirName $trace $L1 $L2 $L3 1"
          "$HERE/$PfRunner".sh --dir "$champsimDirName" --bp "$BP" --repl "$REPL" --win-mode "$WIN_MODE" --hermes "$OCP" --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" "$trace" "$L1" "$L2" "$L3" "1"
          exit 0
    fi
  
    # exit 1
    
    echo "=== ;$n; processes ==="
    for ((i = 0; i < n; i++)); do
      {
        # Extract context lines for visibility
        # Detect fatal condition
        # if echo "$output" | grep -qi "error:"; then
        #     echo "*** BUILD FAIL ***"
        #     exit 1
        # fi
        # START=$(date +%s%3N)
        # OUT=$(./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" | grep "Finished" | head -n 1)
        # perf record -F 13500 -g -o perf.data ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" #| grep "Finished" | head -n 1)
        #      perf record -F 13499 --call-graph dwarf -o perf.data ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" #| grep -E "Finished CPU|now IPC :"
        {
        src=$(find "$champsimDirName/src/ByP_Models" -type f -name "${BYPASS_MODEL}.l1_bypass" -print -quit)
        tmp_snapshot="${ARCHIVE_PATH}/.${ts}-${BYPASS_MODEL}.l1_bypass.pre"
        cp -- "$src" "$tmp_snapshot"

        echo "[job_id: ${BYP_JOB_ID:-NONE}] [trace: $trace] [model: $L1_BYP_MODEL]"
        "$HERE/$PfRunner".sh --dir "$champsimDirName" --bp "$BP" --repl "$REPL" --win-mode "$WIN_MODE" --hermes "$OCP" --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" "$trace" "$L1" "$L2" "$L3"
        # ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" 2>&1 \
        # | tee "Epoch10-$BYPASS_MODEL-$(date '+%Y%m%d-%H%M%S').log" \
        # | cat \
        # | {
        #     if [ "$isDebug" -gt 0 ]; then
        #         cat
        #     elif [ "$isDebug" -eq -2 ]; then
        #         grep -E "Finished CPU|FINAL ROI CORE AVG IPC:|Degree/Access Ratio|Global Hit Rate|DEADLOCK|SANITY|failed|Aborted"
        #     else
        #         grep -E "Finished CPU|FINAL ROI CORE AVG IPC:"
        #     fi
        # }
        # } 3>&1

        }

        # Now extract only the FINAL ROI CORE AVG IPC: lines
        ipc_result=$(echo "$output" | grep "FINAL ROI CORE AVG IPC:")
        ipc_val=$(echo "$ipc_result" | grep -oP '(?<=FINAL ROI CORE AVG IPC: ;)[^;]+' | tail -n1 | tr '.' '_')
        echo "$ipc_val"
        mv -- "$tmp_snapshot" "${ARCHIVE_PATH}/$(date +"%Y%m%d-%H%M%S")-${ipc_val}-${BYPASS_MODEL}.l1_bypass"
        # if [ "$isDebug" -gt 0 ]; then
        #   ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" # | grep -E "Finished CPU|now IPC :" # # > /dev/null 2>&1 
        # else
        #   if [ "$isDebug" -lt 0 ]; then
        #     if [ "$isDebug" -eq -2 ]; then
        #       ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" | grep -E "Finished CPU|FINAL ROI CORE AVG IPC:|Degree/Access Ratio|Global Hit Rate|DEADLOCK|SANITY|failed|Aborted" # > /dev/null 2>&1 
        #     else
        #       ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" | grep -E "Finished CPU|FINAL ROI CORE AVG IPC:|CFG|Degree/Access Ratio|Global Hit Rate|DEADLOCK|SANITY|trace_|_instructions |failed|Aborted" # # > /dev/null 2>&1 
        #     fi
        #   else 
        #     ./"$PfRunner".sh "$trace" "$L1" "$L2" "$L3" | grep -E "Finished CPU|FINAL ROI CORE AVG IPC:|CFG|Degree/Access Ratio|Global Hit Rate|DEADLOCK|SANITY|trace_|_instructions |now IPC:|failed|Aborted" # # > /dev/null 2>&1 
        #   fi
        # fi 
        #>&2; # | grep -E "Finished CPU" #|now IPC :"
        # echo "$n; $((ELAPSED / 1000)) "
        # echo "$full_output" #
      } & 
      pids+=($!)
      sleep 0.1
      # if [ $status -ne 0 ]; then
      #     echo "FAIL:q run"$PfRunner".sh exited with code $status"
      #     exit $status
      # fi
    done
    for pid in "${pids[@]}"; do wait "$pid"; done
    END=$(date +%s%3N)
    ELAPSED=$((END - START))
    echo "[DONE] BUF_SZ = $buf | $n processes | time: ${ELAPSED} ms | Effectiveness: $(awk "BEGIN { printf \"%.6f\", ($n * 1000) / $ELAPSED }") s"

  done
done
