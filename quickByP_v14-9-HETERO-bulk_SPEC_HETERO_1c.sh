#!/usr/bin/env bash
set -u
set -o pipefail

# =========================================================
# USER-EDITABLE CONFIG (top-of-file for visibility)
# =========================================================
MAX_CONCURRENT_PROCS=20

# =========================================================
# REMOTE DISPATCH CONFIG (v14-9)
# =========================================================
REMOTE_HOSTS=(10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96 10.52.0.214) # LOCAL: 10.52.0.214
# REMOTE_HOSTS=(10.52.3.1 10.52.0.126 10.52.3.96 10.52.2.153)
REMOTE_KEY="${REMOTE_KEY:-~/.ssh/id_rsa}"
REMOTE_SSH_CTRL_DIR="/tmp/champsim_ssh_mux_$$"
mkdir -p "$REMOTE_SSH_CTRL_DIR"
REMOTE_SSH="ssh -i $REMOTE_KEY -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes -o ControlMaster=auto -o ControlPath=${REMOTE_SSH_CTRL_DIR}/%r@%h:%p -o ControlPersist=600"
REMOTE_BASE="/home/cc/champsim_VB"
REMOTE_HOST_IDX=0
SWEEP_OUT_GREP_PERIOD=1800
OUT_DIR_SIGNATURE="Hx_outHOMO_SPEC-HETERO-1c-"
CHAMPSIM_DIR="champsim_v15"

# =========================================================
# HETERO MODE TOGGLE
#   HETERO_RUN=0 -> original homo: same trace replicated across cores
#   HETERO_RUN=1 -> heterogeneous: K unique traces per sim, C(NUM_TRACES,K) combos
#   K must match the single CORE_LIST entry. Set CORE_LIST=(4) when HETERO_RUN=1.
# =========================================================
HETERO_RUN=0
HETERO_four_cores=4
HETERO_DELIM="|"

if (( HETERO_RUN == 1 )); then
  if [[ "${#CORE_LIST[@]}" != 1 || "${CORE_LIST[0]}" != "$HETERO_four_cores" ]]; then
    echo "[FATAL] HETERO_RUN=1 requires CORE_LIST=(${HETERO_four_cores}); got CORE_LIST=(${CORE_LIST[*]})"
    exit 2
  fi
fi
# =========================================================

# CORE_LIST=(16 1 2  8 4)
# CORE_LIST=(2 16 8 4 1)
CORE_LIST=(1)

TRACE_LIST=( # FILTERED USELESS TO BYPASS AND SORTED BY TIME TO SIM
    
# HETERO GENIOUSLY WORKLOADS ------------------------------------------------
    "481.wrf-455B"
    "654.roms_s-293B"
    "482.sphinx3-1522B"
    "654.roms_s-294B"
    "482.sphinx3-234B"
    "603.bwaves_s-891B"
    "621.wrf_s-6673B"
    "603.bwaves_s-2609B"
    "623.xalancbmk_s-202B"
    "605.mcf_s-484B"
    "450.soplex-247B"
    "437.leslie3d-265B"
# HETERO GENIOUSLY WORKLOADS ------------------------------------------------
    # "LLM512.GPT-125M_125M" "LLM512.OPT-350M_167M" "LLM256.GPT-125M_32M" "LLM256.OPT-350M_42M" "LLM512.Pythia-70M_83M" "LLM256.Pythia-70M_21M" "437.leslie3d-273B" "437.leslie3d-265B" "437.leslie3d-271B" "623.xalancbmk_s-202B" "429.mcf-217B" "605.mcf_s-782B" "605.mcf_s-472B" "LLM1024.GPT-125M_496M" "LLM1024.OPT-350M_662M" "LLM1024.Pythia-70M_331M" "602.gcc_s-1850B" "605.mcf_s-1554B" "483.xalancbmk-127B" "605.mcf_s-484B" "607.cactuBSSN_s-2421B" "603.bwaves_s-891B" "623.xalancbmk_s-10B" "429.mcf-22B" "429.mcf-51B" "429.mcf-184B" "482.sphinx3-1522B" "433.milc-337B" "482.sphinx3-1100B" "482.sphinx3-1297B" "482.sphinx3-234B" "603.bwaves_s-2609B" "433.milc-127B" "459.GemsFDTD-1491B" "482.sphinx3-417B" "450.soplex-247B" "462.libquantum-1343B" "429.mcf-192B" "603.bwaves_s-1740B" "482.sphinx3-1395B" "481.wrf-196B" "403.gcc-17B" "471.omnetpp-188B" "481.wrf-1254B" "605.mcf_s-1152B" "481.wrf-455B" "437.leslie3d-134B" "649.fotonik3d_s-7084B" "654.roms_s-294B" "459.GemsFDTD-1320B" "473.astar-359B" "410.bwaves-2097B" "654.roms_s-1390B" "603.bwaves_s-2931B" "459.GemsFDTD-1169B" "654.roms_s-293B" "654.roms_s-523" "654.roms_s-1070B" "649.fotonik3d_s-1176B" "605.mcf_s-665B" "621.wrf_s-6673B" "620.omnetpp_s-141B" "605.mcf_s-994B" "620.omnetpp_s-874B" "462.libquantum-714B"
# LLM
    # "LLM4096.OPT-350M_10532M"  "LLM4096.GPT-125M_7899M" "LLM4096.Pythia-70M_5266M"
    # "LLM256.OPT-350M_42M" "LLM256.Pythia-70M_21M" "LLM256.GPT-125M_32M"
    # "LLM2048.GPT-125M_1979M" "LLM2048.OPT-350M_2639M" "LLM2048.Pythia-70M_1319M"
    # "LLM1024.GPT-125M_496M" "LLM1024.OPT-350M_662M" "LLM1024.Pythia-70M_331M"
    # "LLM512.GPT-125M_125M" "LLM512.OPT-350M_167M" "LLM512.Pythia-70M_83M" 
# SPEC
    # "473.astar-359B" "433.milc-127B" "605.mcf_s-472B" "429.mcf-22B" "605.mcf_s-1152B" "429.mcf-217B" "620.omnetpp_s-141B" "623.xalancbmk_s-10B" "450.soplex-247B" "605.mcf_s-484B" "459.GemsFDTD-1320B" "607.cactuBSSN_s-2421B" "459.GemsFDTD-1169B" "603.bwaves_s-1740B"
    # "621.wrf_s-6673B" "603.bwaves_s-2609B" "623.xalancbmk_s-202B" "649.fotonik3d_s-1176B" "482.sphinx3-417B" "654.roms_s-294B" "437.leslie3d-134B" "654.roms_s-293B" "481.wrf-455B" "603.bwaves_s-891B" "437.leslie3d-273B" "482.sphinx3-234B" "437.leslie3d-265B" "482.sphinx3-1100B" "482.sphinx3-1522B" "481.wrf-196B"

  )

PREFETCH_LIST=(
    "no no"
    "no bingo_dpc3"
    "no berti_c"
    "no spp"
    "no Zion1"
)

LLC_PREFETCHER_DEFAULT="no"

MODEL_LIST=(
"4000fix-KappaPhiL1L2 4000fix-KappaPhiL1L2 4000fix-KappaPhiL1L2"
"no no no"
)

REPL_LIST=(
  "lru" 
  # "ship++" "hawkeye" "care" 
  #    "sdbp_2010"
)

OCP_LIST=(
  "none" "hermes" #
   "hmp" "ttp"
)


# =========================================================
# UNLOCK ALL CORES (removes inherited 4-127 affinity restriction)
taskset -cp 0-255 $$ 2>/dev/null || true

# =========================================================
# PERFORMANCE TUNING — applied once at startup, no reboot needed
# Failures are warned but non-fatal (some need sudo)
# =========================================================
apply_perf_tuning() {
  local ok=0 warn=0

  _tune() {
    local val="$1" path="$2" desc="$3"
    if sudo sh -c "echo $val > $path" 2>/dev/null; then
      echo "[TUNE] OK  $desc = $val  ($path)"
      (( ok++ )) || true
    else
      echo "[TUNE] WARN  cannot set $desc  ($path)"
      (( warn++ )) || true
    fi
  }

  echo "--- Performance tuning ---"

  # 1. NUMA balancing: REALLY off (was showing 2, not 0 — kernel still migrating)
  _tune 0 /proc/sys/kernel/numa_balancing "numa_balancing"

  # 2. THP=always: large in-RAM trace working sets → fewer iTLB misses, less frontend stall
  _tune always /sys/kernel/mm/transparent_hugepage/enabled "THP"

  # 3. THP defrag=defer+madvise: async compaction, avoids stall on alloc
  _tune "defer+madvise" /sys/kernel/mm/transparent_hugepage/defrag "THP_defrag"

  # 4. Dirty writeback: avoid log-write storms from 100 sims
  #    background starts at 2% (early, small batches) → no big burst
  _tune 2  /proc/sys/vm/dirty_background_ratio  "dirty_background_ratio"
  #    hard cap at 40% (was 20%) — sims are reading, not writing bulk data
  _tune 40 /proc/sys/vm/dirty_ratio             "dirty_ratio"
  #    writeback age: 500cs = 5s (was default 3000cs=30s) — flush logs sooner, smaller batches
  _tune 500 /proc/sys/vm/dirty_expire_centisecs "dirty_expire_centisecs"
  _tune 100 /proc/sys/vm/dirty_writeback_centisecs "dirty_writeback_centisecs"

  # 5. CPU frequency governor → performance (no freq-scaling jitter during sim)
  local gcpu
  for gcpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    sudo sh -c "echo performance > $gcpu" 2>/dev/null || true
  done
  local gov; gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo "unknown")
  echo "[TUNE] CPU governor → $gov"

  # 6. CPU clock: lock min=max freq (no idle-clock dips), boost stays on, disable C2 (30us→1us max latency)
  local cpu maxf
  for cpu in /sys/devices/system/cpu/cpu[0-9]*/cpufreq; do
    maxf=$(cat "$cpu/cpuinfo_max_freq" 2>/dev/null) || continue
    sudo sh -c "echo $maxf > $cpu/scaling_min_freq" 2>/dev/null || true
  done
  echo "[TUNE] CPU scaling_min_freq locked to max on all cores"

  # Disable C2 (30us wakeup latency) — keep C1 (1us), POLL (0us)
  local cstate
  for cstate in /sys/devices/system/cpu/cpu*/cpuidle/state2/disable; do
    sudo sh -c "echo 1 > $cstate" 2>/dev/null || true
  done
  echo "[TUNE] C2 state disabled on all cores (max latency now C1=1us)"

  # Boost: ensure on
  sudo sh -c "echo 1 > /sys/devices/system/cpu/cpufreq/boost" 2>/dev/null || true
  echo "[TUNE] CPU boost = $(cat /sys/devices/system/cpu/cpufreq/boost 2>/dev/null)"

  # 7. Scheduler: reduce migration tendency for pinned workloads
  #    sched_migration_cost_ns: higher = kernel less eager to migrate tasks off their CPU
#   _tune 5000000 /proc/sys/kernel/sched_migration_cost_ns "sched_migration_cost_ns"
  #    sched_autogroup: off — one process group, fair scheduling across all sim procs
  _tune 0 /proc/sys/kernel/sched_autogroup_enabled "sched_autogroup"

  echo "[TUNE] Done: $ok applied, $warn skipped (no permission)"
  echo "---"
}

apply_perf_tuning

# =========================================================
# CCD ROUND-ROBIN AFFINITY
# EPYC 7763: 2 sockets x 8 CCDs x 8 cores (+8 SMT siblings each) = 16 CCDs
# Each sim pinned to one CCD to maximise local L3/memory bandwidth.
# Round-robin: sim1→CCD0, sim2→CCD1, ..., sim16→CCD15, sim17→CCD0, ...
# =========================================================
CCD_CPUS=(
  "0-7,128-135"      # CCD0  socket0
  "8-15,136-143"     # CCD1  socket0
  "16-23,144-151"    # CCD2  socket0
  "24-31,152-159"    # CCD3  socket0
  "32-39,160-167"    # CCD4  socket0
  "40-47,168-175"    # CCD5  socket0
  "48-55,176-183"    # CCD6  socket0
  "56-63,184-191"    # CCD7  socket0
  "64-71,192-199"    # CCD8  socket1
  "72-79,200-207"    # CCD9  socket1
  "80-87,208-215"    # CCD10 socket1
  "88-95,216-223"    # CCD11 socket1
  "96-103,224-231"   # CCD12 socket1
  "104-111,232-239"  # CCD13 socket1
  "112-119,240-247"  # CCD14 socket1
  "120-127,248-255"  # CCD15 socket1
)
CCD_COUNT=${#CCD_CPUS[@]}   # 16

# Atomic round-robin counter — shared across all subshells via flock
CCD_RR_FILE="/tmp/champsim_ccd_rr_$$"
echo 0 > "$CCD_RR_FILE"
CCD_RR_LOCK="${CCD_RR_FILE}.lock"
touch "$CCD_RR_LOCK"
# Cleanup on exit
# CCD temp cleanup now handled inside cleanup_all()

# Returns the CPU list for the next CCD in round-robin order
next_ccd_cpus() {
  local _idx _cpus
  {
    flock -x 9
    _idx=$(cat "$CCD_RR_FILE")
    _cpus="${CCD_CPUS[$_idx]}"
    echo $(( (_idx + 1) % CCD_COUNT )) > "$CCD_RR_FILE"
    echo "$_cpus"
  } 9>"$CCD_RR_LOCK"
}

# =========================================================
export arch="glc_Hetero4c"
NUM_CORES="${NUM_CORES:-2}"
prefetcher_L1="${prefetcher_L1:-no}"
prefetcher_L2="${prefetcher_L2:-no}"
prefetcher_L3="${prefetcher_L3:-no}"
TRACE_PERCENT="${TRACE_PERCENT:-20}"
champsimDirName="${champsimDirName:-$CHAMPSIM_DIR}"

# CLI arg tracking — detect single-run CLI mode vs sweep mode
CLI_L1BYP="" CLI_L2BYP="" CLI_L3BYP=""
CLI_TRACE="" CLI_CORES="" CLI_L1="" CLI_L2="" CLI_L3=""
CLI_OCP="" CLI_WIN_MODE="" CLI_DB="" CLI_DRAIN="" CLI_DIR=""
CLI_BP="" CLI_REPL="" CLI_MALLOC="" CLI_ALLBYP=""
CLI_PROCESSES="" CLI_FAST="" CLI_DEBUG=""
CLI_MODE=0  # 0=sweep, 1=single-run CLI
RESUME_DIR=""   # --resume-from <DIR>: skip sims whose log already has FINAL ROI
DRY_RUN=0       # --dry-run: print what would run, don't actually launch
CLI_FILTER_CSV=""  # --filter-csv <PATH>: only run sigs listed in this CSV
CLI_FILTER_WARN=0  # --filter-warn: warn (not error) on unknown sigs in filter CSV
RUNS_DUMP_PATH=""  # --runsDump <PATH>: dry-dump planned runs as CSV
PULL_DUMP_PATH=""  # --pullDump <PATH>: drain rows from a runsDump CSV

# Partial-match resolver for bypass models and prefetchers.
# Usage: resolved=$(resolve_partial <search_dir> <suffix> <user_input>)
resolve_partial() {
  local dir="$1" suffix="$2" input="$3" label="${4:-file}"
  [[ -f "$dir/${input}${suffix}" ]] && { echo "$input"; return 0; }
  local -a hits=()
  while IFS= read -r f; do
    [[ -n "$f" ]] && hits+=("$f")
  done < <(find "$dir" -maxdepth 1 -type f -iname "*${input}*${suffix}" ! -path "*/archive/*" 2>/dev/null)
  if (( ${#hits[@]} == 0 )); then
    echo -e "\033[1;91mERROR: No ${label} matching '*${input}*${suffix}' found\033[0m" >&2
    find "$dir" -maxdepth 1 -type f -name "*${suffix}" ! -path "*/archive/*" -printf "  %f\n" 2>/dev/null | sort >&2
    exit 1
  elif (( ${#hits[@]} > 1 )); then
    echo -e "\033[1;93mAmbiguous ${label} '${input}' — multiple matches:\033[0m" >&2
    for h in "${hits[@]}"; do echo "  $(basename "$h")" >&2; done
    exit 1
  fi
  local base
  base=$(basename "${hits[0]}" "$suffix")
  [[ "$base" != "$input" ]] && echo -e "\033[1;92m${label} '${input}' → resolved to '${base}'\033[0m" >&2
  echo "$base"
}

parse_args() {
  shopt -s nocasematch
  while [[ $# -gt 0 ]]; do
    case $1 in
    --dir)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --dir requires a value"; exit 1; }
      CLI_DIR="$2"; champsimDirName="$2"; shift 2 ;;
    --bp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --bp requires a value"; exit 1; }
      CLI_BP="$2"; shift 2 ;;
    --repl)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --repl requires a value"; exit 1; }
      CLI_REPL="$2"; shift 2 ;;
    --win-mode)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --win-mode requires a value"; exit 1; }
      CLI_WIN_MODE="$2"; shift 2 ;;
    --ocp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --ocp requires a value (none|hermes|ttp|hmp)"; exit 1; }
      CLI_OCP="$2"; shift 2 ;;
    --malloc)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --malloc requires a value (mimalloc|jemalloc|none)"; exit 1; }
      case "$2" in
        mimalloc)
          MALLOC_LIB="$(ldconfig -p 2>/dev/null | grep -oP '/\S+libmimalloc\S*\.so[.0-9]*' | head -1)"
          [[ -n "$MALLOC_LIB" ]] || { echo -e "\033[1;91mERROR: mimalloc not found\033[0m"; exit 1; }
          ;;
        jemalloc)
          MALLOC_LIB="$(ldconfig -p 2>/dev/null | grep -oP '/\S+libjemalloc\S*\.so[.0-9]*' | head -1)"
          [[ -n "$MALLOC_LIB" ]] || { echo -e "\033[1;91mERROR: jemalloc not found\033[0m"; exit 1; }
          ;;
        none) MALLOC_LIB="" ;;
        *) echo "Error: --malloc accepts mimalloc|jemalloc|none"; exit 1 ;;
      esac
      CLI_MALLOC="$2"; shift 2 ;;
    --allByP|--allbyp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --allByP requires a value"; exit 1; }
      CLI_ALLBYP="$2"; CLI_MODE=1; shift 2 ;;
    -1|--L1)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      CLI_L1="$2"; CLI_MODE=1; shift 2 ;;
    -2|--L2)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      CLI_L2="$2"; CLI_MODE=1; shift 2 ;;
    -3|--L3)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: $1 requires a value"; exit 1; }
      CLI_L3="$2"; CLI_MODE=1; shift 2 ;;
    -bypca|-ByPca|-ByPcache|-CaByP)
      CLI_MODE=1; shift ;;
    -ByPpf|-ByPpref|-PfByP)
      CLI_MODE=1; shift ;;
    --l1byp|--L1byp|-l1byp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --l1byp requires a value"; exit 1; }
      CLI_L1BYP="$2"; CLI_MODE=1; shift 2 ;;
    --l2byp|--L2byp|-l2byp)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --l2byp requires a value"; exit 1; }
      CLI_L2BYP="$2"; CLI_MODE=1; shift 2 ;;
    --noL2byp|--nol2byp)
      BYPASS_L2=0; CLI_MODE=1; shift ;;
    --L3byp|--l3byp|-L3byp)
      if [[ -z "$2" || "$2" =~ ^- ]]; then
        BYPASS_LLC=1; CLI_MODE=1; shift
      else
        CLI_L3BYP="$2"; CLI_MODE=1; shift 2
      fi
      ;;
    --noL3byp|--nol3byp)
      BYPASS_LLC=0; CLI_MODE=1; shift ;;
    -ByPModel|-ByPmodel|-bypmodel)
      echo -e "\033[1;93mWARN: -ByPModel deprecated, use --l1byp\033[0m"
      CLI_L1BYP="$2"; CLI_MODE=1; shift 2 ;;
    -d|--debug)
      [[ -z "$2" || ( "$2" =~ ^- && ! "$2" =~ ^-[0-9]+$ ) ]] && { echo "Error: --debug requires a value"; exit 1; }
      CLI_DEBUG="$2"; shift 2 ;;
    -f|--fast)
      CLI_FAST=1; shift ;;
    -c|--cores)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --cores requires a value"; exit 1; }
      CLI_CORES="$2"; CLI_MODE=1; shift 2 ;;
    --profile)
      # Accepted for CLI compat — consume up to 3 optional sub-args, no-op in bulk mode
      shift 1
      while [[ $# -gt 0 && ! "$1" =~ ^- ]]; do shift; done
      ;;
    --trace)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --trace requires a value"; exit 1; }
      CLI_TRACE="$2"; CLI_MODE=1; shift 2 ;;
    -p|--processes_num)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --processes_num requires a value"; exit 1; }
      CLI_PROCESSES="$2"; shift 2 ;;
    --db)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --db requires a value"; exit 1; }
      CLI_DB="$2"; shift 2 ;;
    --drain)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --drain requires a path"; exit 1; }
      CLI_DRAIN="$2"; shift 2 ;;
    --time)
      shift ;;  # accepted for CLI compat, no-op in bulk mode
    --resume-from)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --resume-from requires a directory path"; exit 1; }
      RESUME_DIR="$2"; shift 2 ;;
    --filter-csv)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --filter-csv requires a path"; exit 1; }
      CLI_FILTER_CSV="$2"; shift 2 ;;
    --filter-warn)
      CLI_FILTER_WARN=1; shift ;;
    --dry-run)
      DRY_RUN=1; shift ;;
    --runsDump|--runsdump)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --runsDump requires a path"; exit 1; }
      RUNS_DUMP_PATH="$2"; shift 2 ;;
    --pullDump|--pulldump)
      [[ -z "$2" || "$2" =~ ^- ]] && { echo "Error: --pullDump requires a path"; exit 1; }
      PULL_DUMP_PATH="$2"; shift 2 ;;
    -h|--help) echo "Usage: $0 [all quick_v14.sh flags] [--resume-from DIR] [--filter-csv PATH] [--filter-warn] [--dry-run] [--runsDump PATH] [--pullDump PATH]"; exit 0 ;;
    *)
      if [[ -z "$CLI_FILTER_CSV" && -f "$1" && "$1" == *.csv ]]; then
        CLI_FILTER_CSV="$1"; shift
      else
        echo "Error: Unknown option: $1"; exit 1
      fi ;;
    esac
  done
  shopt -u nocasematch
}

# Parse CLI args BEFORE array declarations so --dir is available
parse_args "$@"

# Validate --resume-from
if [[ -n "$RESUME_DIR" ]]; then
  # Resolve to absolute path
  if [[ "$RESUME_DIR" != /* ]]; then
    RESUME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/${RESUME_DIR}"
  fi
  [[ -d "$RESUME_DIR" ]] || { echo "[ERROR] --resume-from dir not found: $RESUME_DIR"; exit 1; }
  echo "[RESUME] Will skip sims with completed logs in: $RESUME_DIR"
fi

if (( DRY_RUN == 1 )); then
  echo "[DRY-RUN] Mode enabled — no sims will be launched"
  DRYRUN_WOULD=0
  DRYRUN_SKIP=0
fi

# FILTER-CSV: rerun-only mode — load failed signatures, only run those
declare -A FILTER_SET=()
FILTER_WARN_LOG=""

load_filter_csv() {
  local csv="$1"
  local warn_mode="$2"
  local row_count=0 sig_count=0 bad_count=0
  local line trace model l1 l2 l3 cores rc lf sig

  [[ -f "$csv" ]] || { echo "[FILTER] ERROR: --filter-csv path not found: $csv"; exit 1; }

  FILTER_WARN_LOG="${csv%.csv}_filter_warnings.log"
  echo "[FILTER] Loading filter from: $csv"
  : > "$FILTER_WARN_LOG"
  echo "# filter_warnings.log — generated $(date -u +%Y-%m-%dT%H:%M:%SZ)" >> "$FILTER_WARN_LOG"
  echo "# source: $csv" >> "$FILTER_WARN_LOG"

  local first_line
  first_line=$(head -n1 "$csv")
  local skip_header=0
  if [[ "$first_line" == "trace;model;l1_pf;l2_pf;l3_pf;cores;rc;logfile" ]]; then
    skip_header=1
  fi

  while IFS=';' read -r trace model l1 l2 l3 cores rc lf; do
    (( row_count++ ))
    (( skip_header && row_count == 1 )) && continue
    [[ -z "$trace" ]] && continue

    sig=$(basename "$lf" .log 2>/dev/null)
    if [[ -z "$sig" ]]; then
      (( bad_count++ ))
      echo "[FILTER] WARN: row $row_count has empty logfile field" >> "$FILTER_WARN_LOG"
      continue
    fi
    FILTER_SET["$sig"]=1
    (( sig_count++ ))
  done < "$csv"

  echo "[FILTER] Loaded $sig_count signatures from $csv ($bad_count bad rows)"
  if (( bad_count > 0 && warn_mode == 0 )); then
    echo "[FILTER] WARNING: $bad_count bad rows — use --filter-warn to suppress this"
  fi
}

if [[ -n "$CLI_FILTER_CSV" ]]; then
  load_filter_csv "$CLI_FILTER_CSV" "$CLI_FILTER_WARN"
  echo "[FILTER] ${#FILTER_SET[@]} unique signatures loaded"
fi

# USER SETTINGS
# =========================================================

if [[ -n "${OUT_ROOT:-}" && "$OUT_ROOT" != "./outputs/out" ]]; then
  mkdir -p "$OUT_ROOT"
  echo "Using directory: $OUT_ROOT (pre-set via env)"
else
  OUT_ROOT="./outputs/${OUT_DIR_SIGNATURE}"
  i=1
  while true; do
      dir=$(printf "%s%02d" "$OUT_ROOT" "$i")
      if [[ ! -d "$dir" ]]; then
          mkdir -p "$dir"
          echo "Using directory: $dir"
          OUT_ROOT=$dir
          break
      fi
      ((i++))
      if (( i > 99 )); then
          echo "ERROR: reached limit (99)"
          exit 1
      fi
  done
fi
# =========================================================
# RUNTIME-EDITABLE DURING EXECUTION
# Edit THIS SAME SCRIPT while it runs, save it, and new value
# will be used by wait_for_slot().
# =========================================================

# Poll interval for slot checking / stale temp cleanup
POLL_SEC=1

# Keep the launch pacing behaviour
LAUNCH_DELAY_SEC=0.6

# Isolated build temp root
WORK_BASE="/tmp/champsim_builds"
DB_FNAME="${OUT_ROOT:-./champsim_results}/champsim_results_GLOBAL_eval_ByP.db"
[[ -n "$CLI_DB" ]] && DB_FNAME="$CLI_DB"
DRAIN_FILE="${CLI_DRAIN:-${OUT_ROOT}/drain_queue.txt}"

# If temp build dir is older than this and not active anymore, delete it during run
BUILD_STALE_SEC=0

# If old run dir from crashed previous launch is older than this and owner pid is dead, delete it during run
RUN_STALE_SEC=0

# If free space on FS containing WORK_BASE drops below this, new builds wait
MIN_FREE_KB_WORK=0

# Keep failed build dirs for a while so you can inspect them before stale cleanup removes them
KEEP_FAILED_BUILD_DIRS=1

# Make parallelism
BUILD_JOBS="${BUILD_JOBS:-12}"
[[ "$BUILD_JOBS" =~ ^[0-9]+$ ]] || BUILD_JOBS=4
(( BUILD_JOBS > 0 )) || BUILD_JOBS=1

# Debug filtering for console output
#  2 = full log after each completed run
#  1 = broad filtered log
#  0 = key lines
# -1 = concise
# -2 = very concise
DEBUG_LEVEL=-1

# Fast / min sim mode
DO_MIN_SIM=0
FAST_WARMUP=500000
FAST_SIM=3000000

# L2/LLC bypass toggle (0=off, 1=on)
BYPASS_L2=${BYPASS_L2:-0}
BYPASS_LLC=${BYPASS_LLC:-0}

# =========================================================
# CLI OVERRIDE: single-run mode overrides arrays
# =========================================================
if (( CLI_MODE == 1 )); then
  echo "[CLI MODE] Single-run CLI mode detected — overriding sweep arrays"

  # Resolve bypass models via resolve_partial
  if [[ -n "$CLI_ALLBYP" ]]; then
    _m=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l1_bypass" "$CLI_ALLBYP" "bypass model")
    CLI_L1BYP="$_m"; CLI_L2BYP="$_m"; CLI_L3BYP="$_m"
  fi
  if [[ -n "$CLI_L1BYP" ]]; then
    _m=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l1_bypass" "$CLI_L1BYP" "L1 bypass model")
    CLI_L1BYP="$_m"
  fi
  if [[ -n "$CLI_L2BYP" ]]; then
    _m=$(resolve_partial "$champsimDirName/src/ByP_Models" ".l2_bypass" "$CLI_L2BYP" "L2 bypass model")
    CLI_L2BYP="$_m"; BYPASS_L2=1
  fi
  if [[ -n "$CLI_L3BYP" ]]; then
    _m=$(resolve_partial "$champsimDirName/src/ByP_Models" ".llc_bypass" "$CLI_L3BYP" "LLC bypass model")
    CLI_L3BYP="$_m"; BYPASS_LLC=1
  fi

  # Override MODEL_LIST with CLI bypass models
  if [[ -n "$CLI_L1BYP" ]]; then
    _l1m="$CLI_L1BYP"
    _l2m="${CLI_L2BYP:-$_l1m}"
    _l3m="${CLI_L3BYP:-$_l1m}"
    MODEL_LIST=("$_l1m $_l2m $_l3m")
    echo "[CLI MODE] MODEL_LIST overridden: ${MODEL_LIST[*]}"
  fi

  # Override TRACE_LIST
  if [[ -n "$CLI_TRACE" ]]; then
    TRACE_LIST=("$CLI_TRACE")
    echo "[CLI MODE] TRACE_LIST overridden: ${TRACE_LIST[*]}"
  fi

  # Override CORE_LIST
  if [[ -n "$CLI_CORES" ]]; then
    CORE_LIST=("$CLI_CORES")
    NUM_CORES="$CLI_CORES"
    echo "[CLI MODE] CORE_LIST overridden: ${CORE_LIST[*]}"
  fi

  # Override PREFETCH_LIST from --L1/--L2/--L3
  if [[ -n "$CLI_L1" || -n "$CLI_L2" || -n "$CLI_L3" ]]; then
    _pf_l1="${CLI_L1:-$prefetcher_L1}"
    _pf_l2="${CLI_L2:-$prefetcher_L2}"
    _pf_l3="${CLI_L3:-$prefetcher_L3}"
    # Resolve prefetcher names
    [[ "$_pf_l1" != "no" ]] && _pf_l1=$(resolve_partial "$champsimDirName/prefetcher" ".l1d_pref" "$_pf_l1" "L1 prefetcher")
    [[ "$_pf_l2" != "no" ]] && _pf_l2=$(resolve_partial "$champsimDirName/prefetcher" ".l2c_pref" "$_pf_l2" "L2 prefetcher")
    [[ "$_pf_l3" != "no" ]] && _pf_l3=$(resolve_partial "$champsimDirName/prefetcher" ".llc_pref" "$_pf_l3" "LLC prefetcher")
    PREFETCH_LIST=("$_pf_l1 $_pf_l2 $_pf_l3")
    echo "[CLI MODE] PREFETCH_LIST overridden: ${PREFETCH_LIST[*]}"
  fi

  # Override REPL_LIST from --repl
  if [[ -n "$CLI_REPL" ]]; then
    REPL_LIST=("$CLI_REPL")
    echo "[CLI MODE] REPL_LIST overridden: ${REPL_LIST[*]}"
  fi

  # Override OCP_LIST from --ocp
  if [[ -n "$CLI_OCP" ]]; then
    OCP_LIST=("$CLI_OCP")
    echo "[CLI MODE] OCP_LIST overridden: ${OCP_LIST[*]}"
  fi

  # Skip PHASE 1 rerun in CLI mode — go straight to sweep with overridden arrays
  RERUN_CONFIG_FILE="__NONE__"
  RESUME_INDEX=0
fi

# Apply CLI flag overrides to global vars
[[ -n "$CLI_DEBUG" ]]     && DEBUG_LEVEL="$CLI_DEBUG"
[[ -n "$CLI_FAST" ]]      && DO_MIN_SIM=1
[[ -n "$CLI_PROCESSES" ]] && MAX_CONCURRENT_PROCS="$CLI_PROCESSES"

# =========================================================
# FIXED BUILD SETTINGS
# =========================================================

BRANCH="perceptron"
LLC_REPLACEMENT="lru"
PGO_FILE="profiles_v10/PGO_10_SPEC_LLM.profdata"

# NO config_fast.ini
# If empty, script will auto-detect a UNIQUE file inc/Arch/*_<cores>c.h
# If there are multiple matches, set this manually.
ARCH_BASE="$arch"

TRACES_DIR="./traces"
# TRACE_PERCENT already set above (respects env override)

# =========================================================
# PATHS
# =========================================================

if command -v readlink >/dev/null 2>&1; then
  SCRIPT_FILE="$(readlink -f "${BASH_SOURCE[0]}")"
else
  SCRIPT_FILE="${BASH_SOURCE[0]}"
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHAMPSIM_DIR_NAME="${CHAMPSIM_DIR_NAME:-$CHAMPSIM_DIR}"
# Apply --dir from CLI (parse_args already set champsimDirName)
[[ -n "$CLI_DIR" ]] && CHAMPSIM_DIR_NAME="$CLI_DIR"
CHAMPSIM_DIR="${ROOT}/${CHAMPSIM_DIR_NAME}"

# Apply CLI overrides for tags
WIN_MODE_TAG="${WIN_MODE:-fixed}"
[[ -n "$CLI_WIN_MODE" ]] && WIN_MODE_TAG="$CLI_WIN_MODE"

HERMES_TAG="${HERMES:-off}"
if [[ -n "$CLI_OCP" ]]; then
  case "$CLI_OCP" in
    none)   HERMES_TAG="off" ;;
    hermes) HERMES_TAG="hermes" ;;
    ttp)    HERMES_TAG="ttp" ;;
    hmp)    HERMES_TAG="hmp" ;;
    *)      echo "Error: --ocp accepts none|hermes|ttp|hmp"; exit 1 ;;
  esac
fi

BP_TAG="${BP:-perceptron}"
[[ -n "$CLI_BP" ]] && { BP_TAG="$CLI_BP"; BRANCH="$CLI_BP"; }

REPL_TAG_OVERRIDE="${REPL:-lru}"
[[ -n "$CLI_REPL" ]] && { REPL_TAG_OVERRIDE="$CLI_REPL"; LLC_REPLACEMENT="$CLI_REPL"; }
TRACES_DIR_SRC="${ROOT}/${TRACES_DIR#./}"
TRACES_DIR_ABS="/dev/shm/traces"

OUT_ROOT_ABS="${ROOT}/${OUT_ROOT#./}"
RESULTS_FILE="${OUT_ROOT_ABS}/full_results2.csv"
FAILED_FILE="${OUT_ROOT_ABS}/failed_runs2.csv"
BIN_CACHE_ROOT="${OUT_ROOT_ABS}/bin_cache"
LOCK_ROOT="${OUT_ROOT_ABS}/locks"
RESULTS_LOCKDIR="${LOCK_ROOT}/results.lockdir"
FAILED_LOCKDIR="${LOCK_ROOT}/failed.lockdir"

mkdir -p "$OUT_ROOT_ABS" "$BIN_CACHE_ROOT" "$LOCK_ROOT" "$WORK_BASE"

RUN_ROOT="$(mktemp -d "${WORK_BASE}/run.$$.XXXXXX")"
echo "$$" > "${RUN_ROOT}/owner.pid"

if [[ ! -f "$RESULTS_FILE" ]]; then
  printf 'trace;model;l1_pf;l2_pf;l3_pf;ocp;repl;num_cores;avg_ipc;mpki_l1;mpki_l2;mpki_l3;hitr_l1;hitr_l2;hitr_l3;mshr_l1;mshr_l2;mshr_l3;apc_l1;apc_l2;apc_l3;lpm_l1;lpm_l2;lpm_l3;camat_l1;camat_l2;camat_l3\n' > "$RESULTS_FILE"
fi

if [[ ! -f "$FAILED_FILE" ]]; then
  printf 'trace;model;l1_pf;l2_pf;l3_pf;cores;rc;logfile;repl;ocp\n' > "$FAILED_FILE"
fi

# =========================================================
# HELPERS
# =========================================================

sanitize() {
  local s="$1"
  s="${s//\//_}"
  s="${s// /_}"
  s="${s//:/_}"
  s="${s//|/__}"
  echo "$s"
}

byp_cp() { local s="$1" d="$2" fn; fn=$(basename "$s"); sed "/\[model:/!s|cout << \"|cout << \"[model: ${fn}] |" "$s" > "$d"; }

current_max_procs() {
  local n
  n="$(
    grep -E '^MAX_CONCURRENT_PROCS=' "$SCRIPT_FILE" \
      | head -n1 \
      | cut -d= -f2 \
      | tr -d '[:space:]'
  )"

  [[ "$n" =~ ^[0-9]+$ ]] || n=1
  (( n > 0 )) || n=1
  echo "$n"
}

reload_remote_hosts() {
  local line
  line="$(grep -E '^REMOTE_HOSTS=' "$SCRIPT_FILE" | head -n1)"
  [[ -n "$line" ]] || return
  eval "$line"
}

count_running_jobs() {
  # Count sims running on current remote host
  local host="${REMOTE_HOSTS[$REMOTE_HOST_IDX]}"
  local n
  n=$($REMOTE_SSH cc@$host "pgrep -c champsim 2>/dev/null; exit 0" 2>/dev/null) || n=999
  [[ "$n" =~ ^[0-9]+$ ]] || n=999
  echo "$n"
}

count_running_on_host() {
  local host="$1"
  local n
  n=$($REMOTE_SSH cc@$host "pgrep -c champsim 2>/dev/null; exit 0" 2>/dev/null) || n=999
  [[ "$n" =~ ^[0-9]+$ ]] || n=999
  echo "$n"
}

count_total_remote_running() {
  local total=0
  for host in "${REMOTE_HOSTS[@]}"; do
    local n
    n=$(count_running_on_host "$host")
    (( total += n )) || true
  done
  echo "$total"
}

host_label() {
  local ip="$1" i
  for (( i = 0; i < ${#REMOTE_HOSTS[@]}; i++ )); do
    [[ "${REMOTE_HOSTS[$i]}" == "$ip" ]] && { echo "H$((i+1))"; return; }
  done
  echo "H?"
}

host_status_line() {
  local _proc_str="" _ram_str="" _load_str=""
  local -a _pids=() _tmpfiles=()
  local i
  for (( i = 0; i < ${#REMOTE_HOSTS[@]}; i++ )); do
    local _h="${REMOTE_HOSTS[$i]}" _lbl="H$((i+1))"
    local _tmp; _tmp="$(mktemp)"
    _tmpfiles+=("$_tmp")
    (
      local _info
      _info=$($REMOTE_SSH cc@$_h "echo \$(pgrep -c champsim 2>/dev/null || echo 0):\$(free -g|awk '/Mem/{print \$4}'):\$(cut -d' ' -f1 /proc/loadavg)" 2>/dev/null) || _info="?:?:?"
      echo "${_lbl}=${_info}" > "$_tmp"
    ) &
    _pids+=($!)
  done
  for _p in "${_pids[@]}"; do wait "$_p" 2>/dev/null || true; done
  for (( i = 0; i < ${#_tmpfiles[@]}; i++ )); do
    local _data; _data="$(<"${_tmpfiles[$i]}")"
    local _lbl="${_data%%=*}" _vals="${_data#*=}"
    IFS=':' read -r _pc _rf _ld <<< "$_vals"
    _proc_str+="${_lbl}=${_pc:-?} "
    _ram_str+="${_lbl}=${_rf:-?}G "
    _load_str+="${_lbl}=${_ld:-?} "
    rm -f "${_tmpfiles[$i]}"
  done
  echo "|> PROC: ${_proc_str}| RAM Free: ${_ram_str}| LOAD: ${_load_str}"
}

available_kb_on_path() {
  local p="$1"
  df -Pk "$p" | awk 'NR==2 {print $4}'
}

wait_for_work_space() {
  local free_kb
  while true; do
    free_kb="$(available_kb_on_path "$WORK_BASE")"
    [[ "$free_kb" =~ ^[0-9]+$ ]] || free_kb=0

    if (( free_kb >= MIN_FREE_KB_WORK )); then
      break
    fi

    echo "[WAIT] low free space on WORK_BASE fs: ${free_kb} KB < ${MIN_FREE_KB_WORK} KB"
    sleep "$POLL_SEC"
  done
}

acquire_lockdir() {
  local d="$1"
  while ! mkdir "$d" 2>/dev/null; do
    sleep 0.2
  done
}

release_lockdir() {
  local d="$1"
  rmdir "$d" 2>/dev/null || true
}

cleanup_stale_temp_during_run() {
  local stale_build_min stale_run_min
  local d owner_pid

  stale_build_min=$(( (BUILD_STALE_SEC + 59) / 60 ))
  stale_run_min=$(( (RUN_STALE_SEC + 59) / 60 ))
  (( stale_build_min >= 1 )) || stale_build_min=1
  (( stale_run_min >= 1 )) || stale_run_min=1

  # 1) Clean stale build dirs from THIS run if they are not active anymore
  while IFS= read -r d; do
    [[ -n "$d" ]] || continue
    [[ -e "$d/.active_build" ]] && continue
    rm -rf "$d" 2>/dev/null || true
  done < <(find "$RUN_ROOT" -maxdepth 1 -type d -name 'build.*' -mmin +"$stale_build_min" 2>/dev/null)

  # 2) Clean stale orphan run dirs from previous crashed launches if owner pid is dead
  while IFS= read -r d; do
    [[ -n "$d" ]] || continue
    [[ "$d" == "$RUN_ROOT" ]] && continue

    owner_pid=""
    if [[ -f "$d/owner.pid" ]]; then
      owner_pid="$(tr -d '[:space:]' < "$d/owner.pid" 2>/dev/null || true)"
    fi

    if [[ -n "$owner_pid" && "$owner_pid" =~ ^[0-9]+$ ]]; then
      if kill -0 "$owner_pid" 2>/dev/null; then
        continue
      fi
    fi

    rm -rf "$d" 2>/dev/null || true
  done < <(find "$WORK_BASE" -maxdepth 1 -type d -name 'run.*' -mmin +"$stale_run_min" 2>/dev/null)
}

wait_for_slot() {
  # Fill-first: stay on current host until full, then move to next.
  # Every 5s ping all hosts looking for one with room.
  local max
  max="$(current_max_procs)"
  [[ "$max" =~ ^[0-9]+$ ]] || max=1

  # Check current host first (fast path — no sleep)
  local running
  running=$(count_running_on_host "${REMOTE_HOSTS[$REMOTE_HOST_IDX]}")
  if (( running < max )); then
    return
  fi

  # Current host full — scan all hosts every 5s
  while true; do
    cleanup_stale_temp_during_run
    reload_remote_hosts
    max="$(current_max_procs)"
    [[ "$max" =~ ^[0-9]+$ ]] || max=1
    local i
    for (( i = 0; i < ${#REMOTE_HOSTS[@]}; i++ )); do
      local idx=$(( (REMOTE_HOST_IDX + i) % ${#REMOTE_HOSTS[@]} ))
      running=$(count_running_on_host "${REMOTE_HOSTS[$idx]}")
      if (( running < max )); then
        REMOTE_HOST_IDX=$idx
        return
      fi
    done
    sleep 5
  done
}

cleanup_all() {
  trap - EXIT INT TERM HUP QUIT

  jobs -pr | xargs -r kill 2>/dev/null || true
  wait 2>/dev/null || true

  # remove only THIS run root
  rm -rf "$RUN_ROOT" 2>/dev/null || true

  # Clean CCD round-robin temp files (was in a separate trap that got overridden)
  rm -f "$CCD_RR_FILE" "$CCD_RR_LOCK" 2>/dev/null || true

  # Tear down SSH ControlMaster mux sockets
  for _sock in "$REMOTE_SSH_CTRL_DIR"/*; do
    ssh -o ControlPath="$_sock" -O exit dummy 2>/dev/null || true
  done
  rm -rf "$REMOTE_SSH_CTRL_DIR" 2>/dev/null || true
}
trap cleanup_all EXIT INT TERM HUP QUIT

resolve_trace_file() {
  local trace_hint="$1"
  local found=""

  if [[ -f "$trace_hint" ]]; then
    echo "$trace_hint"
    return 0
  fi

  found="$(find "$TRACES_DIR_ABS" -maxdepth 1 -type f -name "*${trace_hint}*.xz" -print -quit 2>/dev/null || true)"
  [[ -n "$found" ]] || return 1

  echo "$found"
}

auto_detect_arch_base() {
  local cores="$1"
  local matches=()
  local f base

  while IFS= read -r f; do
    matches+=("$f")
  done < <(find "$CHAMPSIM_DIR/inc/Arch" -maxdepth 1 -type f -name "*_${cores}c.h" | sort)

  if (( ${#matches[@]} == 1 )); then
    base="$(basename "${matches[0]}")"
    base="${base%_${cores}c.h}"
    echo "$base"
    return 0
  fi

  return 1
}

resolve_arch_base() {
  local cores="$1"

  if [[ -n "$ARCH_BASE" ]]; then
    echo "$ARCH_BASE"
    return 0
  fi

  auto_detect_arch_base "$cores"
}

compute_trace_window() {
  local trace_file="$1"
  local base trace_name trace_count simTraces_num percent toWarmUp toSim

  if (( DO_MIN_SIM == 1 )); then
    echo "${FAST_WARMUP};${FAST_SIM}"
    return 0
  fi

  base="$(basename "$trace_file")"
  trace_name="${base%.champsimtrace.xz}"
  trace_name="${trace_name%.xz}"

  case "$trace_name" in
    [0-9][0-9][0-9].*)
      echo "50000000;200000000"
      return 0
      ;;
    LLM*)
      trace_count="$(echo "$trace_name" | sed -n 's/.*_\([0-9][0-9]*\)M.*/\1/p')"
      if [[ -z "$trace_count" || ! "$trace_count" =~ ^[0-9]+$ ]]; then
        echo "[WARN] compute_trace_window: LLM trace '${trace_name}' missing _NNM suffix → fallback 50M;200M" >&2
        echo "50000000;200000000"
        return 0
      fi

      simTraces_num=$((trace_count * 1000000))
      percent=$(( (simTraces_num * TRACE_PERCENT) / 100 ))
      toWarmUp=$(( (percent / 1000000) * 1000000 ))
      toSim=$(( simTraces_num - 5 * toWarmUp / 2 ))

      (( toWarmUp >= 0 )) || toWarmUp=0
      (( toSim >= 0 )) || toSim=0

      echo "${toWarmUp};${toSim}"
      return 0
      ;;
    *)
      echo "[WARN] compute_trace_window: trace '${trace_name}' matches no SPEC/LLM pattern → fallback 50M;200M" >&2
      echo "50000000;200000000"
      return 0
      ;;
  esac
}

copy_source_tree_for_build() {
  local dst="$1"

  # Broad source copy, but EXCLUDING heavy/runtime dirs.
  # This avoids missing source-side dirs while still avoiding trace copies.
  rsync -a \
    --exclude '/traces/***' \
    --exclude '/bin/***' \
    --exclude '/obj/***' \
    --exclude '/output*/***' \
    --exclude '/.git/***' \
    --exclude '*.o' \
    --exclude '*.d' \
    --exclude '*.bak' \
    --exclude '*.tmp' \
    "$CHAMPSIM_DIR/" "$dst/"
}

extract_metrics() {
gawk '
function add_avg(key, val) {
    SUM[key] += val + 0
    CNT[key] += 1
}
function avg(key) {
    return (CNT[key] ? SUM[key] / CNT[key] : 0)
}
function fmt(x) {
    return sprintf("%.5f", x)
}

/^NUM_CPUS[ \t]+[0-9]+/ {
    num_cores = $2 + 0
}

/^FINAL ROI CORE AVG IPC:/ {
    if (match($0, /;[ \t]*([0-9.]+)[ \t]*;/, m))
        avg_ipc = m[1] + 0
}

/^Core_[0-9]+_IPC[ \t]+/ {
    if (avg_ipc == 0)
        avg_ipc = $2 + 0
}

/L1D;_.*;MPKI;/ {
    if (match($0, /;MPKI;[ \t]*([0-9.]+);/, m)) add_avg("mpki_l1", m[1])
}
/L2C;_.*;MPKI;/ {
    if (match($0, /;MPKI;[ \t]*([0-9.]+);/, m)) add_avg("mpki_l2", m[1])
}
/LLC;_.*;MPKI;/ {
    if (match($0, /;MPKI;[ \t]*([0-9.]+);/, m)) add_avg("mpki_l3", m[1])
}

/L1D_total_HitR:/ {
    if (match($0, /L1D_total_HitR:[ \t]*([0-9.]+)/, m)) add_avg("hitr_l1", m[1])
}
/L2C_total_HitR:/ {
    if (match($0, /L2C_total_HitR:[ \t]*([0-9.]+)/, m)) add_avg("hitr_l2", m[1])
}
/LLC_total_HitR:/ {
    if (match($0, /LLC_total_HitR:[ \t]*([0-9.]+)/, m)) add_avg("hitr_l3", m[1])
}

/L1D;_.*;LOAD_MSHR_cap;/ {
    if (match($0, /;LOAD_MSHR_cap;[ \t]*([0-9.]+);/, m)) add_avg("mshr_l1", m[1])
}
/L2C;_.*;LOAD_MSHR_cap;/ {
    if (match($0, /;LOAD_MSHR_cap;[ \t]*([0-9.]+);/, m)) add_avg("mshr_l2", m[1])
}
/LLC;_.*;LOAD_MSHR_cap;/ {
    if (match($0, /;LOAD_MSHR_cap;[ \t]*([0-9.]+);/, m)) add_avg("mshr_l3", m[1])
}

/L1D;_.*;APC;/ {
    if (match($0, /;APC;[ \t]*([0-9.]+);/, m)) add_avg("apc_l1", m[1])
}
/L2C;_.*;APC;/ {
    if (match($0, /;APC;[ \t]*([0-9.]+);/, m)) add_avg("apc_l2", m[1])
}
/LLC;_.*;APC;/ {
    if (match($0, /;APC;[ \t]*([0-9.]+);/, m)) add_avg("apc_l3", m[1])
}

/L1D;_.*;LPM;/ {
    if (match($0, /;LPM;[ \t]*([0-9.]+);/, m)) add_avg("lpm_l1", m[1])
}
/L2C;_.*;LPM;/ {
    if (match($0, /;LPM;[ \t]*([0-9.]+);/, m)) add_avg("lpm_l2", m[1])
}
/LLC;_.*;LPM;/ {
    if (match($0, /;LPM;[ \t]*([0-9.]+);/, m)) add_avg("lpm_l3", m[1])
}

/L1D;_.*;C-AMAT;/ {
    if (match($0, /;C-AMAT;[ \t]*([0-9.]+);/, m)) add_avg("camat_l1", m[1])
}
/L2C;_.*;C-AMAT;/ {
    if (match($0, /;C-AMAT;[ \t]*([0-9.]+);/, m)) add_avg("camat_l2", m[1])
}
/LLC;_.*;C-AMAT;/ {
    if (match($0, /;C-AMAT;[ \t]*([0-9.]+);/, m)) add_avg("camat_l3", m[1])
}

END {
    printf "%d;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s;%s\n",
        num_cores,
        fmt(avg_ipc),
        fmt(avg("mpki_l1")),
        fmt(avg("mpki_l2")),
        fmt(avg("mpki_l3")),
        fmt(avg("hitr_l1")),
        fmt(avg("hitr_l2")),
        fmt(avg("hitr_l3")),
        fmt(avg("mshr_l1")),
        fmt(avg("mshr_l2")),
        fmt(avg("mshr_l3")),
        fmt(avg("apc_l1")),
        fmt(avg("apc_l2")),
        fmt(avg("apc_l3")),
        fmt(avg("lpm_l1")),
        fmt(avg("lpm_l2")),
        fmt(avg("lpm_l3")),
        fmt(avg("camat_l1")),
        fmt(avg("camat_l2")),
        fmt(avg("camat_l3"))
}
' "$1"
}

append_result_row() {
  local trace="$1"
  local model="$2"
  local l1="$3"
  local l2="$4"
  local l3="$5"
  local ocp="$6"
  local repl="$7"
  local logfile="$8"
  local row

  row="$(extract_metrics "$logfile")"

  acquire_lockdir "$RESULTS_LOCKDIR"
  printf '%s;%s;%s;%s;%s;%s;%s;%s\n' \
    "$trace" "$model" "$l1" "$l2" "$l3" "$ocp" "$repl" "$row" >> "$RESULTS_FILE"
  release_lockdir "$RESULTS_LOCKDIR"
}

append_failed_row() {
  local trace="$1"
  local model="$2"
  local l1="$3"
  local l2="$4"
  local l3="$5"
  local cores="$6"
  local rc="$7"
  local logfile="$8"
  local repl="${9:-${REPL_TAG_OVERRIDE:-$LLC_REPLACEMENT}}"
  local ocp="${10:-$HERMES_TAG}"

  acquire_lockdir "$FAILED_LOCKDIR"
  printf '%s;%s;%s;%s;%s;%s;%s;%s;%s;%s\n' \
    "$trace" "$model" "$l1" "$l2" "$l3" "$cores" "$rc" "$logfile" "$repl" "$ocp" >> "$FAILED_FILE"
  release_lockdir "$FAILED_LOCKDIR"
}


resolve_model_file() {
    local model="$1"
    local ext="${2:-.l1_bypass}"
    local normalized
    local found

    if [[ "$model" == *"$ext" ]]; then
        normalized="$model"
    else
        normalized="${model}${ext}"
    fi

    found="$(find "$CHAMPSIM_DIR/src/ByP_Models" -type f -name "$normalized" -print -quit 2>/dev/null || true)"
    [[ -n "$found" ]] || return 1
    echo "$found"
}

build_binary_for_cfg() {
  local arch_base="$1"
  local cores="$2"
  local l1_pf="$3"
  local l2_pf="$4"
  local l3_pf="$5"
  local l1_model_name="$6"
  local l2_model_name="$7"
  local llc_model_name="$8"
  local cached_bin="$9"
  local logfile="${10}"

  local core_uarch="${arch_base}_${cores}c"
  local build_key builddir rc=0

  local model_src="$(resolve_model_file "$l1_model_name" ".l1_bypass")" || {
        echo "[ERROR] missing L1 bypass model via find: $l1_model_name" >> "$logfile"
        return 207
    }
  local l2_model_src="$(resolve_model_file "$l2_model_name" ".l2_bypass")" || {
        echo "[WARN] no L2 bypass model for: $l2_model_name (using fallback)" >> "$logfile"
        l2_model_src=""
    }
  local llc_model_src="$(resolve_model_file "$llc_model_name" ".llc_bypass")" || {
        echo "[WARN] no LLC bypass model for: $llc_model_name (using fallback)" >> "$logfile"
        llc_model_src=""
    }
  echo "[MODEL PATH] L1=$l1_model_name L2=${l2_model_src:-fallback} LLC=${llc_model_src:-fallback}" >> "$logfile"
  local arch_src="$CHAMPSIM_DIR/inc/Arch/${core_uarch}.h"
  local branch_src="$CHAMPSIM_DIR/branch/${BRANCH}.bpred"
  local l1_src="$CHAMPSIM_DIR/prefetcher/${l1_pf}.l1d_pref"
  local l2_src="$CHAMPSIM_DIR/prefetcher/${l2_pf}.l2c_pref"
  local l3_src="$CHAMPSIM_DIR/prefetcher/${l3_pf}.llc_pref"
  local repl_src="$CHAMPSIM_DIR/replacement/${LLC_REPLACEMENT}.llc_repl"
  local pgo_src="$CHAMPSIM_DIR/${PGO_FILE}"

  [[ -f "$arch_src"  ]] || { echo "[ERROR] missing arch file: $arch_src" >> "$logfile"; return 201; }
  [[ -f "$branch_src" ]] || { echo "[ERROR] missing branch predictor: $branch_src" >> "$logfile"; return 202; }
  [[ -f "$l1_src"    ]] || { echo "[ERROR] missing L1 prefetcher: $l1_src" >> "$logfile"; return 203; }
  [[ -f "$l2_src"    ]] || { echo "[ERROR] missing L2 prefetcher: $l2_src" >> "$logfile"; return 204; }
  [[ -f "$l3_src"    ]] || { echo "[ERROR] missing LLC prefetcher: $l3_src" >> "$logfile"; return 205; }
  [[ -f "$repl_src"  ]] || { echo "[ERROR] missing replacement policy: $repl_src" >> "$logfile"; return 206; }
  [[ -f "$model_src" ]] || { echo "[ERROR] missing L1 bypass model: $model_src" >> "$logfile"; return 207; }
  [[ -f "$pgo_src"   ]] || { echo "[ERROR] missing PGO file: $pgo_src" >> "$logfile"; return 208; }

  wait_for_work_space

  build_key="$(sanitize "${arch_base}_${cores}_${l1_pf}_${l2_pf}_${l3_pf}_${l1_model_name}_${l2_model_name}_${llc_model_name}_${BRANCH}_${LLC_REPLACEMENT}_${PGO_FILE}_${WIN_MODE_TAG}_${HERMES_TAG}")"
  builddir="$(mktemp -d "${RUN_ROOT}/build.${build_key}.XXXXXX")" || {
    echo "[ERROR] mktemp failed — RUN_ROOT gone or /tmp full: ${RUN_ROOT}" >> "$logfile"
    return 230
  }
  [[ -d "$builddir" ]] || { echo "[ERROR] builddir empty/missing: ${builddir}" >> "$logfile"; return 231; }
  : > "${builddir}/.active_build"

  {
    echo "[BUILD] isolated build dir: $builddir"
    echo "[BUILD] arch=${core_uarch}"
    echo "[BUILD] L1=${l1_pf} L2=${l2_pf} L3=${l3_pf}"
    echo "[BUILD] l1_model=${l1_model_name} l2_model=${l2_model_name} llc_model=${llc_model_name}"
    echo "[BUILD] make -j${BUILD_JOBS} run_clang"
  } >> "$logfile"

    if [ "$DEBUG_LEVEL" -eq 2 ]; then
        copy_source_tree_for_build "$builddir"
    else
        copy_source_tree_for_build "$builddir" >/dev/null 2>&1
    fi

  cp "$arch_src"   "$builddir/inc/defs.h"                     || { rm -f "${builddir}/.active_build"; return 210; }
  cp "$branch_src" "$builddir/branch/branch_predictor.cc"     || { rm -f "${builddir}/.active_build"; return 211; }
  cp "$l1_src"     "$builddir/prefetcher/l1d_prefetcher.cc"   || { rm -f "${builddir}/.active_build"; return 212; }
  cp "$l2_src"     "$builddir/prefetcher/l2c_prefetcher.cc"   || { rm -f "${builddir}/.active_build"; return 213; }
  cp "$l3_src"     "$builddir/prefetcher/llc_prefetcher.cc"   || { rm -f "${builddir}/.active_build"; return 214; }
  cp "$repl_src"   "$builddir/replacement/llc_replacement.cc" || { rm -f "${builddir}/.active_build"; return 215; }
  byp_cp "$model_src"  "$builddir/src/ooo_l1_byp_model.cc"        || { rm -f "${builddir}/.active_build"; return 216; }
  if [[ -n "$l2_model_src" ]]; then
    byp_cp "$l2_model_src" "$builddir/src/ooo_l2_byp_model.cc"   || { rm -f "${builddir}/.active_build"; return 219; }
  fi
  if [[ -n "$llc_model_src" ]]; then
    byp_cp "$llc_model_src" "$builddir/src/ooo_llc_byp_model.cc" || { rm -f "${builddir}/.active_build"; return 222; }
  fi

  sed -i.bak "s/^#define NUM_CPUS [0-9][0-9]*/#define NUM_CPUS ${cores}/" "$builddir/inc/champsim.h" || { rm -f "${builddir}/.active_build"; return 217; }

  # Toggle L2 bypass — auto-enable when model file is present
  local _byp_l2=0; [[ -n "$l2_model_src" ]] && _byp_l2=1; (( BYPASS_L2 == 1 )) && _byp_l2=1
  if (( _byp_l2 == 1 )); then
    sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_L2_LOGIC|#define BYPASS_L2_LOGIC|' "$builddir/inc/champsim.h"
  else
    sed -i 's|^[[:space:]]*#define BYPASS_L2_LOGIC|// #define BYPASS_L2_LOGIC|' "$builddir/inc/champsim.h"
  fi

  # Toggle LLC bypass — auto-enable when model file is present
  local _byp_llc=0; [[ -n "$llc_model_src" ]] && _byp_llc=1; (( BYPASS_LLC == 1 )) && _byp_llc=1
  if (( _byp_llc == 1 )); then
    sed -i 's|^[[:space:]]*//[[:space:]]*#define BYPASS_LLC_LOGIC|#define BYPASS_LLC_LOGIC|' "$builddir/inc/champsim.h"
  else
    sed -i 's|^[[:space:]]*#define BYPASS_LLC_LOGIC|// #define BYPASS_LLC_LOGIC|' "$builddir/inc/champsim.h"
  fi

  echo "[BUILD] Loaded bypass models: L1=${l1_model_name} L2=${l2_model_name} LLC=${llc_model_name} | L2byp=${BYPASS_L2} L3byp=${BYPASS_LLC}" >> "$logfile"

  # Compute EXTRA_CFLAGS for WIN_MODE and HERMES defines (must match build_champsim_parallel.sh)
  local _extra_defs=""
  case "${WIN_MODE_TAG}" in
    dyn)     _extra_defs="-DLPM_WIN_DYN" ;;
    overlap) _extra_defs="-DLPM_WIN_OVERLAP" ;;
    *)       _extra_defs="-DLPM_WIN_FIXED" ;;
  esac
  case "${HERMES_TAG}" in
    on|hermes) _extra_defs="$_extra_defs -DUSE_HERMES" ;;
    ttp)       _extra_defs="$_extra_defs -DUSE_HERMES -DUSE_HERMES_TTP" ;;
    hmp)       _extra_defs="$_extra_defs -DUSE_HERMES -DUSE_HERMES_HMP" ;;
    *)         _extra_defs="$_extra_defs -DNO_HERMES" ;;
  esac
  echo "[BUILD] EXTRA_CFLAGS='${_extra_defs}'" >> "$logfile"

    echo "[BUILD] ---- make output BEGIN ----" >> "$logfile"
    (
        cd "$builddir" || exit 218
        make -j"$BUILD_JOBS" run_clang EXTRA_CFLAGS="$_extra_defs" 2>&1
    ) >> "$logfile"
    rc=$?
    echo "[BUILD] ---- make output END (rc=$rc) ----" >> "$logfile"

  rm -f "${builddir}/.active_build" 2>/dev/null || true

  if (( rc != 0 )); then
    if (( KEEP_FAILED_BUILD_DIRS == 1 )); then
      echo "[ERROR] build failed, kept temporarily for debug: $builddir" >> "$logfile"
    else
      rm -rf "$builddir" 2>/dev/null || true
    fi
    return "$rc"
  fi

  [[ -f "$builddir/bin/champsim" ]] || {
    echo "[SANITY-FAIL] binary missing after build: $builddir/bin/champsim" >> "$logfile"
    return 219
  }
  if [[ ! -s "$builddir/bin/champsim" ]]; then
    echo "[SANITY-FAIL] binary zero-size after build: $builddir/bin/champsim" >> "$logfile"
    return 219
  fi

  mkdir -p "$(dirname "$cached_bin")"
  # atomic replace: avoid ETXTBSY when $cached_bin is currently exec'd by parallel sims
  cp "$builddir/bin/champsim" "${cached_bin}.tmp.$$" || return 220
  chmod +x "${cached_bin}.tmp.$$" || true
  mv -f "${cached_bin}.tmp.$$" "$cached_bin" || return 220

  echo "[BUILD] cached binary: $cached_bin" >> "$logfile"

  rm -rf "$builddir" 2>/dev/null || true
  return 0
}

ensure_binary() {
  local arch_base="$1"
  local cores="$2"
  local l1_pf="$3"
  local l2_pf="$4"
  local l3_pf="$5"
  local l1_model_name="$6"
  local l2_model_name="$7"
  local llc_model_name="$8"
  local logfile="$9"

  local build_key cache_dir cached_bin lockdir rc=0

  build_key="$(sanitize "${arch_base}_${cores}_${l1_pf}_${l2_pf}_${l3_pf}_${l1_model_name}_${l2_model_name}_${llc_model_name}_${BRANCH}_${LLC_REPLACEMENT}_${PGO_FILE}_${WIN_MODE_TAG}_${HERMES_TAG}")"
  cache_dir="${BIN_CACHE_ROOT}/${build_key}"
  cached_bin="${cache_dir}/champsim"
  lockdir="${cache_dir}.lockdir"
  local src_hash_file="${cache_dir}/.src_hash"
  local cur_src_hash
  cur_src_hash="$(cd "$CHAMPSIM_DIR" && find src inc branch prefetcher replacement -type f \( -name '*.cc' -o -name '*.h' -o -name '*.bpred' -o -name '*.l1d_pref' -o -name '*.l2c_pref' -o -name '*.llc_pref' -o -name '*.llc_repl' -o -name '*.l1_bypass' -o -name '*.l2_bypass' -o -name '*.llc_bypass' \) -print0 2>/dev/null | sort -z | xargs -0 md5sum 2>/dev/null | md5sum | cut -d' ' -f1)"

  if [[ -x "$cached_bin" ]]; then
    local old_hash=""
    [[ -f "$src_hash_file" ]] && old_hash="$(<"$src_hash_file")"
    if [[ "$old_hash" == "$cur_src_hash" ]]; then
      echo "[BUILD CACHE HIT] $cached_bin (src_hash=$cur_src_hash)" >> "$logfile"
      echo "HIT::$cached_bin"
      return 0
    else
      echo "[BUILD CACHE STALE] src changed old=$old_hash new=$cur_src_hash — rebuilding (purging LTO cache)" >> "$logfile"
      rm -f "$cached_bin"
      rm -f "$CHAMPSIM_DIR/.lto_cache/"* 2>/dev/null || true
    fi
  fi

  mkdir -p "$cache_dir"
  # If no .src_hash yet (first build or purged), nuke LTO cache to prevent stale hits
  if [[ ! -f "$src_hash_file" ]]; then
    rm -f "$CHAMPSIM_DIR/.lto_cache/"* 2>/dev/null || true
  fi
  acquire_lockdir "$lockdir"

  if [[ -x "$cached_bin" ]]; then
    local old_hash=""
    [[ -f "$src_hash_file" ]] && old_hash="$(<"$src_hash_file")"
    if [[ "$old_hash" == "$cur_src_hash" ]]; then
      echo "[BUILD CACHE HIT after lock] $cached_bin (src_hash=$cur_src_hash)" >> "$logfile"
      release_lockdir "$lockdir"
      echo "HIT::$cached_bin"
      return 0
    else
      echo "[BUILD CACHE STALE after lock] rebuilding (purging LTO cache)" >> "$logfile"
      rm -f "$cached_bin"
      rm -f "$CHAMPSIM_DIR/.lto_cache/"* 2>/dev/null || true
    fi
  fi

  build_binary_for_cfg "$arch_base" "$cores" "$l1_pf" "$l2_pf" "$l3_pf" "$l1_model_name" "$l2_model_name" "$llc_model_name" "$cached_bin" "$logfile"
  rc=$?

  release_lockdir "$lockdir"

  (( rc == 0 )) || return "$rc"
  [[ -x "$cached_bin" ]] || return 221

  echo "$cur_src_hash" > "$src_hash_file"
  echo "[BUILD CACHE STORE] $cached_bin (src_hash=$cur_src_hash)" >> "$logfile"
  echo "BUILT::$cached_bin"
  return 0
}

run_one() {
  local cores="$1"
  local model_spec="$2"
  local trace_hint="$3"
  local l1_pf="$4"
  local l2_pf="$5"
  local l3_pf="$6"

  local l1_model l2_model llc_model model_tag trace_tag out_dir logfile
  local trace_file trace_base arch_base cached_bin
  local warmup sim rc build_rc
  local trace_args=()
  local i

  read -r l1_model l2_model llc_model <<< "$model_spec"
  [[ -n "${l1_model:-}" ]] || { echo "[ERROR] bad MODEL_LIST item (missing L1 model): '$model_spec'"; return 1; }
  [[ -n "${l2_model:-}" ]] || l2_model="$l1_model"
  [[ -n "${llc_model:-}" ]] || llc_model="$l1_model"

  model_tag="$(sanitize "${l1_model}_${l2_model}_${llc_model}")"
  trace_tag="$(sanitize "$trace_hint")"

  arch_base="$(resolve_arch_base "$cores")" || {
    echo "[ERROR] could not resolve ARCH_BASE for cores=${cores}. Set ARCH_BASE explicitly in script."
    return 302
  }

  out_dir="${OUT_ROOT_ABS}/core${cores}"
  mkdir -p "$out_dir"
  logfile="${out_dir}/${arch_base}..${cores}c..${trace_tag}..${l1_pf}..${l2_pf}..${l3_pf}..${BP_TAG}..${REPL_TAG_OVERRIDE}..ByP..${l1_model}..${l2_model}..${llc_model}..${WIN_MODE_TAG}..${HERMES_TAG}.log"

  {
    echo "[START] cores=${cores} l1_model=${l1_model} l2_model=${l2_model} llc_model=${llc_model} trace=${trace_hint} L1=${l1_pf} L2=${l2_pf} L3=${l3_pf}"
    echo "[CFG] DEBUG_LEVEL=${DEBUG_LEVEL} DO_MIN_SIM=${DO_MIN_SIM} BUILD_JOBS=${BUILD_JOBS}"
    echo "[CFG] current MAX_CONCURRENT_PROCS=$(current_max_procs)"
  } > "$logfile"

  local _first_hint="$trace_hint"
  if [[ "$trace_hint" == *"${HETERO_DELIM}"* ]]; then
    _first_hint="${trace_hint%%${HETERO_DELIM}*}"
  fi
  trace_file="$(resolve_trace_file "$_first_hint")" || {
    echo "[ERROR] trace not found for hint: ${_first_hint}" >> "$logfile"
    append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 301 "$logfile"
    echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | model=${model_tag} | pf=${l1_pf}/${l2_pf}/${l3_pf} | ocp=${HERMES_TAG} | repl=${REPL_TAG_OVERRIDE} | reason: trace_not_found (rc=301)"
    return 301
  }

  trace_base="$(basename "$trace_file")"
  # NOTE (hetero): warmup/sim derived from FIRST trace; all K traces share same window
  IFS=';' read -r warmup sim <<< "$(compute_trace_window "$trace_file")"

  {
    echo "[RUN] trace file: $trace_file"
    echo "[RUN] arch base: $arch_base"
    echo "[RUN] warmup=${warmup} sim=${sim}"
  } >> "$logfile"

  local _ensure_out
  _ensure_out="$(ensure_binary "$arch_base" "$cores" "$l1_pf" "$l2_pf" "$l3_pf" "$l1_model" "$l2_model" "$llc_model" "$logfile")"
  build_rc=$?
  if (( build_rc != 0 )); then
    append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" "$build_rc" "$logfile"
    echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | model=${model_tag} | pf=${l1_pf}/${l2_pf}/${l3_pf} | ocp=${HERMES_TAG} | repl=${REPL_TAG_OVERRIDE} | reason: build_error (rc=${build_rc})"
    return "$build_rc"
  fi

  local _build_status="${_ensure_out%%::*}"
  cached_bin="${_ensure_out#*::}"

  # ---- REMOTE DISPATCH (host assigned by caller via _ASSIGNED_HOST) ----
  local host="${_ASSIGNED_HOST:-${REMOTE_HOSTS[$REMOTE_HOST_IDX]}}"

  # Remote paths
  local remote_bin="${REMOTE_BASE}/${cached_bin#$ROOT/}"
  local remote_bin_dir; remote_bin_dir="$(dirname "$remote_bin")"
  local remote_logfile="${REMOTE_BASE}/${logfile#$ROOT/}"
  local remote_logdir; remote_logdir="$(dirname "$remote_logfile")"
  local remote_trace_dir="${REMOTE_BASE}/traces"
  local trace_basename; trace_basename="$(basename "$trace_file")"
  local remote_trace_file="${remote_trace_dir}/${trace_basename}"
  local remote_db="${REMOTE_BASE}/${DB_FNAME#$ROOT/}"

  local _rsync_ssh="ssh -i $REMOTE_KEY -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o ControlMaster=auto -o ControlPath=${REMOTE_SSH_CTRL_DIR}/%r@%h:%p -o ControlPersist=600"

  # Build trace args + (local, remote) file pairs for rsync.
  # HETERO: K unique traces; HOMO: single trace replicated cores times.
  local trace_args=""
  local -a _mix_local_files=() _mix_remote_files=()
  if [[ "$trace_hint" == *"${HETERO_DELIM}"* ]]; then
    local -a _mix_hints=()
    IFS="${HETERO_DELIM}" read -ra _mix_hints <<< "$trace_hint"
    if (( ${#_mix_hints[@]} != cores )); then
      echo "[ERROR] hetero mix has ${#_mix_hints[@]} hints but cores=${cores}" >> "$logfile"
      append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 312 "$logfile"
      echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | hetero K mismatch"
      return 312
    fi
    local _mh _mf _mbase _mremote
    for _mh in "${_mix_hints[@]}"; do
      _mf="$(resolve_trace_file "$_mh")" || {
        echo "[ERROR] mix trace not found: $_mh" >> "$logfile"
        append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 301 "$logfile"
        echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | mix trace_not_found: ${_mh}"
        return 301
      }
      _mbase="$(basename "$_mf")"
      _mremote="${remote_trace_dir}/${_mbase}"
      _mix_local_files+=("$_mf")
      _mix_remote_files+=("$_mremote")
      trace_args+="'${_mremote}' "
    done
  else
    _mix_local_files+=("$trace_file")
    _mix_remote_files+=("$remote_trace_file")
    for (( i = 0; i < cores; i++ )); do
      trace_args+="'${remote_trace_file}' "
    done
  fi

  local cmd="'${remote_bin}' -warmup ${warmup} -simulation_instructions ${sim} --db '${remote_db}' --arch '${arch_base}' --bypass '${l1_model}.l1_bypass' --pf_l1 '${l1_pf}' --pf_l2 '${l2_pf}' --pf_l3 '${l3_pf}' -traces ${trace_args} > '${remote_logfile}' 2>&1"

  if [[ "$_build_status" == "BUILT" ]]; then
    # Fresh build — mkdir + parallel rsync binary+trace, then launch
    $REMOTE_SSH cc@$host "mkdir -p '${remote_bin_dir}' '${remote_logdir}' '${remote_trace_dir}'" 2>/dev/null

    local _bin_rc=0 _trace_rc=0
    rsync -az -e "$_rsync_ssh" "$cached_bin" "cc@$host:${remote_bin}" 2>/dev/null &
    local _bin_pid=$!
    local -a _trace_pids=()
    local _ti
    for (( _ti = 0; _ti < ${#_mix_local_files[@]}; _ti++ )); do
      rsync -az --ignore-existing -e "$_rsync_ssh" "${_mix_local_files[$_ti]}" "cc@$host:${_mix_remote_files[$_ti]}" 2>/dev/null &
      _trace_pids+=($!)
    done
    wait $_bin_pid || _bin_rc=$?
    local _tpid
    for _tpid in "${_trace_pids[@]}"; do
      wait "$_tpid" || _trace_rc=$?
    done

    if (( _bin_rc != 0 )); then
      echo "[REMOTE-ERR] rsync binary failed → $host" >> "$logfile"
      append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 310 "$logfile"
      echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | rsync binary → $host"
      return 310
    fi
    if (( _trace_rc != 0 )); then
      echo "[REMOTE-ERR] rsync trace failed → $host" >> "$logfile"
      append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 311 "$logfile"
      echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | rsync trace → $host"
      return 311
    fi

    $REMOTE_SSH cc@$host "nohup bash -c \"$cmd\" >/dev/null 2>&1 &" 2>/dev/null
  else
    # CACHE HIT — single SSH: mkdir logdir, verify binary+all K traces, launch
    local _test_str="" _rf
    for _rf in "${_mix_remote_files[@]}"; do
      [[ -n "$_test_str" ]] && _test_str+=" && "
      _test_str+="test -f '${_rf}'"
    done
    $REMOTE_SSH cc@$host "mkdir -p '${remote_logdir}' '${remote_trace_dir}' && test -x '${remote_bin}' && ${_test_str} && nohup bash -c \"$cmd\" >/dev/null 2>&1 &" 2>/dev/null
    local _hit_rc=$?
    if (( _hit_rc != 0 )); then
      # binary or trace missing — parallel sync fallback + launch
      $REMOTE_SSH cc@$host "mkdir -p '${remote_bin_dir}' '${remote_logdir}' '${remote_trace_dir}'" 2>/dev/null
      local _bin_rc=0 _trace_rc=0
      rsync -az -e "$_rsync_ssh" "$cached_bin" "cc@$host:${remote_bin}" 2>/dev/null &
      local _bin_pid=$!
      local -a _trace_pids=()
      local _ti
      for (( _ti = 0; _ti < ${#_mix_local_files[@]}; _ti++ )); do
        rsync -az --ignore-existing -e "$_rsync_ssh" "${_mix_local_files[$_ti]}" "cc@$host:${_mix_remote_files[$_ti]}" 2>/dev/null &
        _trace_pids+=($!)
      done
      wait $_bin_pid || _bin_rc=$?
      local _tpid
      for _tpid in "${_trace_pids[@]}"; do
        wait "$_tpid" || _trace_rc=$?
      done

      if (( _bin_rc != 0 )); then
        echo "[REMOTE-ERR] rsync binary failed → $host" >> "$logfile"
        append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 310 "$logfile"
        echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | rsync binary → $host"
        return 310
      fi
      if (( _trace_rc != 0 )); then
        echo "[REMOTE-ERR] rsync trace failed → $host" >> "$logfile"
        append_failed_row "$trace_hint" "$model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" 311 "$logfile"
        echo "[FAIL] ${PHASE_TAG:-?} | ${cores}c | trace=${trace_hint} | rsync trace → $host"
        return 311
      fi
      $REMOTE_SSH cc@$host "nohup bash -c \"$cmd\" >/dev/null 2>&1 &" 2>/dev/null
    fi
  fi

  local _hlbl; _hlbl="$(host_label "$host")"
  local _mdl_sent="$l1_model"; [[ "$l1_model" == "$l2_model" && "$l2_model" == "$llc_model" ]] && _mdl_sent="${l1_model} x3" || _mdl_sent="${l1_model}/${l2_model}/${llc_model}"
  echo "[SENT-${_build_status}] ${PHASE_TAG:-?} | ${_hlbl} ${host} | ${cores}c | ${trace_hint} | ${l1_pf}/${l2_pf}/${l3_pf} | ${_mdl_sent} | ${REPL_TAG_OVERRIDE} | ${HERMES_TAG}"
  return 0
}

# =========================================================
# SANITY CHECKS
# =========================================================

[[ -d "$CHAMPSIM_DIR" ]] || { echo "[ERROR] champsim dir not found: $CHAMPSIM_DIR"; exit 1; }
[[ -f "$CHAMPSIM_DIR/$PGO_FILE" ]] || { echo "[ERROR] missing PGO file: $CHAMPSIM_DIR/$PGO_FILE"; exit 1; }

# Pre-run sanity: disk space
avail_gb=$(df --output=avail -BG /home/cc | tail -1 | tr -d ' G')
if (( avail_gb < 10 )); then
  echo "[SANITY-FAIL] Only ${avail_gb}GB free — need >=10GB. Aborting."
  exit 1
fi

# Pre-run sanity: RESUME_DIR exists (if specified)
if [[ -n "$RESUME_DIR" ]] && [[ ! -d "$RESUME_DIR" ]]; then
  echo "[SANITY-FAIL] --resume-from dir does not exist: $RESUME_DIR"
  exit 1
fi

# Auto-provision /dev/shm/traces: rsync only needed traces from source
if [[ ! -d "$TRACES_DIR_ABS" ]]; then
  echo "[PROVISION] $TRACES_DIR_ABS missing — creating and syncing needed traces"
  mkdir -p "$TRACES_DIR_ABS"
fi
if [[ -d "$TRACES_DIR_SRC" ]]; then
  _missing=0
  for _th in "${TRACE_LIST[@]}"; do
    _found="$(find "$TRACES_DIR_ABS" -maxdepth 1 -type f -name "*${_th}*.xz" -print -quit 2>/dev/null || true)"
    if [[ -z "$_found" ]]; then
      _src_f="$(find "$TRACES_DIR_SRC" -maxdepth 1 -type f -name "*${_th}*.xz" -print -quit 2>/dev/null || true)"
      if [[ -n "$_src_f" ]]; then
        echo "[PROVISION] rsync $(basename "$_src_f") → $TRACES_DIR_ABS/"
        numactl --interleave=all rsync -a "$_src_f" "$TRACES_DIR_ABS/"
        (( _missing++ )) || true
      else
        echo "[WARN] trace '*${_th}*.xz' not found in $TRACES_DIR_SRC either"
      fi
    fi
  done
  (( _missing > 0 )) && echo "[PROVISION] synced $_missing traces to $TRACES_DIR_ABS"
else
  echo "[WARN] source traces dir $TRACES_DIR_SRC not found — relying on $TRACES_DIR_ABS as-is"
fi
[[ -d "$TRACES_DIR_ABS" ]] || { echo "[ERROR] traces dir not found: $TRACES_DIR_ABS"; exit 1; }

if ! command -v rsync >/dev/null 2>&1; then
  echo "[ERROR] rsync not found"
  exit 1
fi

if ! command -v gawk >/dev/null 2>&1; then
  echo "[ERROR] gawk not found"
  exit 1
fi

# =========================================================
# HELPER: Parse "ModelA_ModelA_ModelA" tag -> "ModelA ModelA ModelA" spec
# (model tag in CSV is sanitize(L1_L2_LLC) = L1_L2_LLC with underscores)
# Split by underscore, group total/3 parts per model, rejoin with underscores
# =========================================================
parse_model_tag_to_spec() {
  local model_tag="$1"
  IFS='_' read -ra _parts <<< "$model_tag"
  local _total=${#_parts[@]}
  if (( _total % 3 != 0 )); then
    echo "ERROR: model tag not divisible by 3: '$model_tag' (${_total} parts)" >&2
    return 1
  fi
  local _n=$(( _total / 3 ))
  local _l1="" _l2="" _l3=""
  for (( _i=0; _i < _n; _i++ )); do
    [[ -n "$_l1" ]] && _l1+="_"; _l1+="${_parts[$_i]}"
    [[ -n "$_l2" ]] && _l2+="_"; _l2+="${_parts[$(( _i + _n ))]}"
    [[ -n "$_l3" ]] && _l3+="_"; _l3+="${_parts[$(( _i + 2*_n ))]}"
  done
  echo "$_l1 $_l2 $_l3"
}

# =========================================================
# HELPER: launch one run from a semicolon-delimited config line
# Input: "trace;model_tag;l1_pf;l2_pf;l3_pf;cores;rc"  (7 or 8 fields, rc/logfile ignored)
# =========================================================
launch_from_config_line() {
  local _line="$1"
  local _trace _model_tag _l1_pf _l2_pf _l3_pf _cores _rc _ignored _repl _ocp
  IFS=';' read -r _trace _model_tag _l1_pf _l2_pf _l3_pf _cores _rc _ignored _repl _ocp <<< "$_line"

  [[ -n "$_trace" && -n "$_model_tag" && -n "$_l1_pf" && -n "$_l2_pf" && -n "$_cores" ]] || {
    echo "[WARN] Skipping malformed line: '$_line'"
    return 1
  }

  local _model_name
  _model_name="$(parse_model_tag_to_spec "$_model_tag")" || {
    echo "[ERROR] Could not parse model tag: '$_model_tag' — skipping line"
    return 1
  }

  [[ -n "${_l3_pf:-}" ]] || _l3_pf="$LLC_PREFETCHER_DEFAULT"

  local _save_repl="$LLC_REPLACEMENT" _save_repl_tag="$REPL_TAG_OVERRIDE" _save_ocp="$HERMES_TAG"
  [[ -n "${_repl:-}" ]] && { LLC_REPLACEMENT="$_repl"; REPL_TAG_OVERRIDE="$_repl"; }
  [[ -n "${_ocp:-}" ]] && HERMES_TAG="$_ocp"

  # Purge cached binary + src_hash so ensure_binary() rebuilds from scratch.
  # This replaces the broken FORCE_REBUILD=1 approach (race condition: post-lock
  # cache check ignored FORCE_REBUILD, so concurrent procs with same build_key
  # got fake "CACHE HIT after lock" instead of rebuilding).
  {
    local _l1m_r _l2m_r _l3m_r
    read -r _l1m_r _l2m_r _l3m_r <<< "$_model_name"
    [[ -n "${_l2m_r:-}" ]] || _l2m_r="$_l1m_r"
    [[ -n "${_l3m_r:-}" ]] || _l3m_r="$_l1m_r"
    local _arch_base_r
    _arch_base_r="$(resolve_arch_base "$_cores")" || _arch_base_r="$ARCH_BASE"
    local _build_key_r
    _build_key_r="$(sanitize "${_arch_base_r}_${_cores}_${_l1_pf}_${_l2_pf}_${_l3_pf}_${_l1m_r}_${_l2m_r}_${_l3m_r}_${BRANCH}_${LLC_REPLACEMENT}_${PGO_FILE}_${WIN_MODE_TAG}_${HERMES_TAG}")"
    local _cache_dir_r="${BIN_CACHE_ROOT}/${_build_key_r}"
    local _cached_bin_r="${_cache_dir_r}/champsim"
    local _src_hash_r="${_cache_dir_r}/.src_hash"
    local _lockdir_r="${_cache_dir_r}.lockdir"
    rm -f "$_cached_bin_r" 2>/dev/null || true
    rm -f "$_src_hash_r" 2>/dev/null || true
    rm -rf "$_lockdir_r" 2>/dev/null || true
    echo "[RERUN-PURGE] removed cached bin+hash+lock for build_key=${_build_key_r}"
  }

  PHASE_TAG="${PHASE_TAG:-rerun}" run_one "$_cores" "$_model_name" "$_trace" "$_l1_pf" "$_l2_pf" "$_l3_pf"
  LLC_REPLACEMENT="$_save_repl"
  REPL_TAG_OVERRIDE="$_save_repl_tag"
  HERMES_TAG="$_save_ocp"
}

# =========================================================
# MAIN
# =========================================================

# Path to failed-runs config to rerun FIRST (before normal sweeps)
# Format per line: trace;model_tag;l1_pf;l2_pf;l3_pf;cores;rc  (7 or 8 fields)
# In CLI mode, RERUN_CONFIG_FILE is already set to __NONE__ to skip phase 1
if [[ "${RERUN_CONFIG_FILE:-}" != "__NONE__" ]]; then
  RERUN_CONFIG_FILE="${RERUN_CONFIG_FILE:-${OUT_ROOT_ABS}/rerun_failed_configs.csv}"
fi

# Skip launches whose currRunCnt < RESUME_INDEX (1-based). Override via env.
# In CLI mode, RESUME_INDEX is already set to 0
if [[ "${RESUME_INDEX:-unset}" == "unset" ]]; then
  RESUME_INDEX=0
fi

# Live queue: user can append lines here while script is running;
# script drains it before every new launch
PENDING_FILE="${PENDING_FILE:-${OUT_ROOT}/pending_rerun.csv}"

# Create empty pending file so user sees where to drop entries
touch "$PENDING_FILE"
echo "[INFO] Pending rerun file (drop lines here any time): $PENDING_FILE"

# --runsDump / --pullDump handling (mutually exclusive)
if [[ -n "${RUNS_DUMP_PATH:-}" && -n "${PULL_DUMP_PATH:-}" ]]; then
  echo "[err] --runsDump and --pullDump are mutually exclusive"; exit 2
fi
if [[ -n "${RUNS_DUMP_PATH:-}" ]]; then
  : > "$RUNS_DUMP_PATH"
fi
if [[ -n "${PULL_DUMP_PATH:-}" ]]; then
  [[ -f "$PULL_DUMP_PATH" ]] || { echo "[pullDump] file not found: $PULL_DUMP_PATH"; exit 2; }
  while IFS= read -r _line; do
    [[ -z "$_line" ]] && continue
    printf '%s\n' "$_line" >> "$PENDING_FILE"
  done < "$PULL_DUMP_PATH"
  MODEL_LIST=(); REPL_LIST=(); OCP_LIST=(); PREFETCH_LIST=(); TRACE_LIST=(); CORE_LIST=()
fi

# Priority tier for a (model, ocp, repl) combo (A/B/C phases — NO Fwd+OCP):
#   0 = Phase A: NoBypass + NoOCP   (model="no no no", ocp=none)
#   1 = Phase B: MyModel  + NoOCP   (model="4000fix-KappaPhiL1L2 x3", ocp=none)
#   2 = Phase C: NoBypass + OCP     (model="no no no", ocp in {hermes,hmp,ttp})
#  99 = filtered out (Fwd+OCP forbidden, unknown model, or unknown combo)
priority_tier() {
  local model="$1" ocp="$2" repl="$3"
  local has_ocp=0
  [[ "$ocp" != "none" ]] && has_ocp=1
  if [[ "$model" == "no no no" ]]; then
    if (( has_ocp == 0 )); then echo 0; return; else echo 2; return; fi
  elif [[ "$model" == "4000fix-KappaPhiL1L2 4000fix-KappaPhiL1L2 4000fix-KappaPhiL1L2" ]]; then
    if (( has_ocp == 0 )); then echo 1; return; fi
    # MyModel + OCP = forbidden combo, drop
  fi
  echo 99
}

# =========================================================
# HETERO: K=4 trace mix builder (iterative, 4 nested loops)
# =========================================================
build_trace_mix_list() {
  if (( HETERO_four_cores != 4 )); then
    echo "[FATAL] build_trace_mix_list hardcoded K=4; HETERO_four_cores=${HETERO_four_cores}" >&2
    exit 2
  fi
  local N="${#TRACE_LIST[@]}"
  TRACE_MIX_LIST=()
  if (( N < 4 )); then
    echo "[HETERO] WARN: TRACE_LIST N=${N} < K=4 → 0 mixes"
    return
  fi
  local i j k l
  for (( i = 0; i < N - 3; i++ )); do
    for (( j = i + 1; j < N - 2; j++ )); do
      for (( k = j + 1; k < N - 1; k++ )); do
        for (( l = k + 1; l < N; l++ )); do
          TRACE_MIX_LIST+=("${TRACE_LIST[$i]}${HETERO_DELIM}${TRACE_LIST[$j]}${HETERO_DELIM}${TRACE_LIST[$k]}${HETERO_DELIM}${TRACE_LIST[$l]}")
        done
      done
    done
  done
  echo "[HETERO] Built ${#TRACE_MIX_LIST[@]} unique K=4 mixes from ${N} traces"
}

# Build TRACE_ITER once: either raw TRACE_LIST (homo) or computed mixes (hetero)
if (( HETERO_RUN == 1 )); then
  build_trace_mix_list
  TRACE_ITER=("${TRACE_MIX_LIST[@]}")
else
  TRACE_ITER=("${TRACE_LIST[@]}")
fi

# ---- count total runs (reruns + normal sweeps) + per-phase totals ----
totRunCnt=0
rerunCntTotal=0
if [[ -f "$RERUN_CONFIG_FILE" ]]; then
  rerunCntTotal=$(wc -l < "$RERUN_CONFIG_FILE")
  totRunCnt=$rerunCntTotal
fi
phase1_total=0 phase2_total=0 phase3_total=0 phase4_total=0 phase5_total=0
for cores in "${CORE_LIST[@]}"; do
  if (( HETERO_RUN == 1 )) && (( cores != HETERO_four_cores )); then continue; fi
  for trace in "${TRACE_ITER[@]}"; do
    for ocp_item in "${OCP_LIST[@]}"; do
      for model_name in "${MODEL_LIST[@]}"; do
        for repl_item in "${REPL_LIST[@]}"; do
          for pf_spec in "${PREFETCH_LIST[@]}"; do
            _pt=$(priority_tier "$model_name" "$ocp_item" "$repl_item")
            case $_pt in
              0) ((phase1_total++)); ((totRunCnt++)) ;; 1) ((phase2_total++)); ((totRunCnt++)) ;; 2) ((phase3_total++)); ((totRunCnt++)) ;;
            esac
          done
        done
      done
    done
  done
done
echo "TOTAL RUNS: $totRunCnt  (rerun-first + normal sweep)  | HETERO=${HETERO_RUN} K=${HETERO_four_cores} mixes=${#TRACE_ITER[@]}"

currRunCnt=0

# Per-phase progress tracking (used to build multi-phase progress string)
# Index 0-4 = phase 1-5. Updated after each phase completes.
phase_processed=(0 0 0 0 0)   # how many combos processed (including skips)
phase_totals=("$phase1_total" "$phase2_total" "$phase3_total" "$phase4_total" "$phase5_total")

# Build progress string: #global/total (P1: x/y | P2: x/y | ... | P5: x/y)
# Args: current_phase_idx (0-based), current_phase_count
build_progress_str() {
  local cur_phase_idx="$1" cur_phase_cnt="$2"
  local parts="" i pcnt ptot
  for (( i = 0; i < 5; i++ )); do
    ptot="${phase_totals[$i]}"
    if (( i < cur_phase_idx )); then
      pcnt="${phase_processed[$i]}"
    elif (( i == cur_phase_idx )); then
      pcnt="$cur_phase_cnt"
    else
      pcnt=0
    fi
    parts+="P$((i+1)): ${pcnt}/${ptot}"
    (( i < 4 )) && parts+=" | "
  done
  echo "#${currRunCnt}/${totRunCnt} (${parts})"
}

# Drain pending file: atomically pull all current lines, clear it, launch each
drain_pending() {
  [[ -s "$PENDING_FILE" ]] || return 0
  local _tmpf
  _tmpf="$(mktemp)"
  mv "$PENDING_FILE" "$_tmpf" && touch "$PENDING_FILE"
  local _pline
  while IFS= read -r _pline; do
    [[ -z "$_pline" ]] && continue
    echo "[PENDING] Launching from pending queue: $_pline"
    launch_from_config_line "$_pline"
    (( currRunCnt++ )) || true
    sleep "$LAUNCH_DELAY_SEC"
  done < "$_tmpf"
  rm -f "$_tmpf"
}

# ---- PHASE 1: rerun failed configs FIRST ----
if [[ -f "$RERUN_CONFIG_FILE" ]]; then
  rerunCnt=$(wc -l < "$RERUN_CONFIG_FILE")
  echo "--- PHASE 1: rerunning $rerunCnt failed configs first ---"
  while IFS= read -r _cfg_line; do
    [[ -z "$_cfg_line" ]] && continue
    ((currRunCnt++))
    if (( currRunCnt < RESUME_INDEX )); then
      continue
    fi
    drain_pending
    if (( DRY_RUN == 1 )); then
      echo "[DRY-RUN] rerun #${currRunCnt}: $_cfg_line"
      (( DRYRUN_WOULD++ )) || true
    else
      echo "STARTED[rerun]: ${currRunCnt}/${totRunCnt} (rerun: ${currRunCnt}/${rerunCntTotal}) | $_cfg_line | run=$(count_running_jobs)/$(current_max_procs)"
      launch_from_config_line "$_cfg_line"
      sleep "$LAUNCH_DELAY_SEC"
    fi
  done < "$RERUN_CONFIG_FILE"
else
  echo "[INFO] No rerun config — skipping phase 1"
fi

# ---- PHASE 2: normal sweep ----
# ---- SWEEP REMOTE LOGS ----
sweep_remote_logs() {
  echo "[SWEEP] Pulling logs from ${#REMOTE_HOSTS[@]} hosts..."
  local _local_ips=" $(hostname -I 2>/dev/null) 127.0.0.1 localhost "
  for host in "${REMOTE_HOSTS[@]}"; do
    if [[ "$_local_ips" == *" $host "* ]]; then
      echo "[SWEEP] skip local $host (no rsync to self — same-path log corruption risk)"
      continue
    fi
    rsync -az -e "ssh -i $REMOTE_KEY -o StrictHostKeyChecking=no -o ConnectTimeout=10" \
      "cc@$host:${REMOTE_BASE}/${OUT_ROOT_ABS#$ROOT/}/" \
      "$OUT_ROOT_ABS/" 2>/dev/null || true
  done
  local done_count
  done_count=$(grep -rl "FINAL ROI CORE AVG IPC" "$OUT_ROOT_ABS"/core*/ 2>/dev/null | wc -l)
  echo "[SWEEP] $(date +%H:%M:%S) $done_count completed logs pulled."
}

sweep_pass() {
  local pass_name="$1" want_tier="$2" phase_num="$3" phase_total="$4" cores="$5"
  local phase_currCnt=0
  local _phase_skipped=0 _phase_launched=0 _phase_dryrun=0
  local _phase_idx=$(( phase_num - 1 ))
  local _progress_str
  local -a _dispatch_queue=()
  _progress_str="$(build_progress_str $_phase_idx $phase_currCnt)"
  echo "========== PHASE ${phase_num}/5 START: ${pass_name} — ${phase_total} runs — ${cores}c =========="
  for trace in "${TRACE_ITER[@]}"; do
    for repl_item in "${REPL_LIST[@]}"; do
      LLC_REPLACEMENT="$repl_item"
      REPL_TAG_OVERRIDE="$repl_item"
      for ocp_item in "${OCP_LIST[@]}"; do
        case "$ocp_item" in
          none)   HERMES_TAG="off" ;;
          hermes) HERMES_TAG="hermes" ;;
          ttp)    HERMES_TAG="ttp" ;;
          hmp)    HERMES_TAG="hmp" ;;
          *)      HERMES_TAG="off" ;;
        esac
        for model_name in "${MODEL_LIST[@]}"; do
          for pf_spec in "${PREFETCH_LIST[@]}"; do

            local tier
            tier=$(priority_tier "$model_name" "$ocp_item" "$repl_item")
            [[ "$tier" == "99" ]] && continue

            ((currRunCnt++))
            ((phase_currCnt++))
            _progress_str="$(build_progress_str $_phase_idx $phase_currCnt)"
            if (( currRunCnt < RESUME_INDEX )); then
              continue
            fi

            read -r l1_pf l2_pf l3_pf <<< "$pf_spec"
            [[ -n "${l1_pf:-}" ]] || { echo "[ERROR] bad PREFETCH_LIST item: '$pf_spec'"; exit 1; }
            [[ -n "${l2_pf:-}" ]] || { echo "[ERROR] bad PREFETCH_LIST item: '$pf_spec'"; exit 1; }
            [[ -n "${l3_pf:-}" ]] || l3_pf="$LLC_PREFETCHER_DEFAULT"

            # Compute log filename for resume-from check
            { read -r _l1m _l2m _l3m <<< "$model_name"; } 2>/dev/null || true
            [[ -n "${_l2m:-}" ]] || _l2m="$_l1m"
            [[ -n "${_l3m:-}" ]] || _l3m="$_l1m"
            local _trace_tag; _trace_tag="$(sanitize "$trace")"
            local _arch_tag; _arch_tag="$(resolve_arch_base "$cores" 2>/dev/null || echo "$ARCH_BASE")"
            local _logname="${_arch_tag}..${cores}c..${_trace_tag}..${l1_pf}..${l2_pf}..${l3_pf}..${BP_TAG}..${repl_item}..ByP..${_l1m}..${_l2m}..${_l3m}..${WIN_MODE_TAG}..${HERMES_TAG}.log"

            # --filter-csv: only run sigs in FILTER_SET (skip everything else)
            if (( ${#FILTER_SET[@]} > 0 )); then
              local _sig="${_logname%.log}"
              if [[ -z "${FILTER_SET[$_sig]:-}" ]]; then
                local _mdl_s="$_l1m"; [[ "$_l1m" == "$_l2m" && "$_l2m" == "$_l3m" ]] && _mdl_s="${_l1m} x3" || _mdl_s="${_l1m}/${_l2m}/${_l3m}"
                echo "[SKIP-FILTER] ${_progress_str} | ${cores}c | ${trace} | ${l1_pf}/${l2_pf}/${l3_pf} | ${_mdl_s} | ${repl_item} | ${ocp_item}"
                continue
              fi
            fi

            # --resume-from: skip if completed log exists in RESUME_DIR
            if [[ -n "$RESUME_DIR" ]]; then
              local _resume_log="$RESUME_DIR/core${cores}/${_logname}"
              if [[ -f "$_resume_log" ]]; then
                if grep -q "FINAL ROI CORE AVG IPC" "$_resume_log"; then
                  local _mdl_s2="$_l1m"; [[ "$_l1m" == "$_l2m" && "$_l2m" == "$_l3m" ]] && _mdl_s2="${_l1m} x3" || _mdl_s2="${_l1m}/${_l2m}/${_l3m}"
                  echo "[SKIP] ${_progress_str} | ${cores}c | ${trace} | ${l1_pf}/${l2_pf}/${l3_pf} | ${_mdl_s2} | ${repl_item} | ${ocp_item} | resume-hit"
                  (( _phase_skipped++ )) || true
                  (( DRY_RUN == 1 )) && (( DRYRUN_SKIP++ )) || true

                  # Copy prior log into new out_dir and append CSV row
                  if (( DRY_RUN == 0 )); then
                    local out_dir="${OUT_ROOT_ABS}/core${cores}"
                    mkdir -p "$out_dir"
                    local _dest_log="${out_dir}/${_logname}"
                    if cp "$_resume_log" "$_dest_log" 2>/dev/null; then
                      # Post-cp sanity: verify copied log still has FINAL IPC
                      if ! grep -q "FINAL ROI CORE AVG IPC" "$_dest_log"; then
                        echo "[WARN] copied log missing FINAL IPC — possible corruption: $_dest_log"
                      fi
                      local _model_tag
                      _model_tag="$(sanitize "${_l1m}_${_l2m}_${_l3m}")"
                      if ! append_result_row "$trace" "$_model_tag" "$l1_pf" "$l2_pf" "$l3_pf" "$HERMES_TAG" "$repl_item" "$_dest_log"; then
                        echo "[WARN] append_result_row failed for resumed log: ${_logname}"
                      fi
                    else
                      echo "[WARN] cp failed: $_resume_log -> $_dest_log"
                    fi
                  fi

                  continue
                else
                  echo "[RESUME-NOSKIP] ${_progress_str} | ${cores}c | trace=${trace} | log exists but no FINAL IPC — will rerun"
                fi
              fi
            fi

            # --dry-run: print what would run, don't launch
            if (( DRY_RUN == 1 )); then
              echo "[DRY-RUN] ${_progress_str} would run: ${_logname}"
              (( DRYRUN_WOULD++ )) || true
              (( _phase_dryrun++ )) || true
              continue
            fi

            # --runsDump: emit planned CSV row and skip launch
            if [[ -n "${RUNS_DUMP_PATH:-}" ]]; then
              local _model_csv; _model_csv="$(sanitize "${_l1m}_${_l2m}_${_l3m}")"
              local _log_csv="${OUT_ROOT_ABS}/core${cores}/${_logname}"
              printf '%s;%s;%s;%s;%s;%s;%s;%s;%s;%s\n' \
                "$trace" "$_model_csv" "$l1_pf" "$l2_pf" "$l3_pf" "$cores" "0" "$_log_csv" "$repl_item" "$ocp_item" \
                >> "$RUNS_DUMP_PATH"
              continue
            fi

            # Collect dispatch entry (tab-delimited; progress_str has pipes)
            _dispatch_queue+=("${cores}"$'\t'"${model_name}"$'\t'"${trace}"$'\t'"${l1_pf}"$'\t'"${l2_pf}"$'\t'"${l3_pf}"$'\t'"${_progress_str}"$'\t'"${repl_item}"$'\t'"${ocp_item}"$'\t'"${HERMES_TAG}")
          done
        done
      done
    done
  done

  # ================================================================
  # PHASE A: BUILD ALL — ensure_binary for each unique build key (parallel)
  # ================================================================
  declare -A _built_keys=()
  local -a _build_pids=()
  local _build_fail_file; _build_fail_file="$(mktemp)"

  echo "[BUILD-ALL] Collecting unique build keys..."
  for _dq_entry in "${_dispatch_queue[@]}"; do
    IFS=$'\t' read -r _dq_cores _dq_model _dq_trace _dq_l1 _dq_l2 _dq_l3 _dq_progress _dq_repl _dq_ocp _dq_hermes <<< "$_dq_entry"
    { read -r _bl1m _bl2m _bl3m <<< "$_dq_model"; } 2>/dev/null || true
    [[ -n "${_bl2m:-}" ]] || _bl2m="$_bl1m"
    [[ -n "${_bl3m:-}" ]] || _bl3m="$_bl1m"
    local _barch; _barch="$(resolve_arch_base "$_dq_cores" 2>/dev/null || echo "$ARCH_BASE")"
    local _bkey
    _bkey="$(sanitize "${_barch}_${_dq_cores}_${_dq_l1}_${_dq_l2}_${_dq_l3}_${_bl1m}_${_bl2m}_${_bl3m}_${BRANCH}_${_dq_repl}_${PGO_FILE}_${WIN_MODE_TAG}_${_dq_hermes}")"
    if [[ -z "${_built_keys[$_bkey]:-}" ]]; then
      _built_keys[$_bkey]="${_barch}|${_dq_cores}|${_dq_l1}|${_dq_l2}|${_dq_l3}|${_bl1m}|${_bl2m}|${_bl3m}|${_dq_repl}|${_dq_hermes}"
    fi
  done

  echo "[BUILD-ALL] ${#_built_keys[@]} unique binaries to build..."
  for _bkey in "${!_built_keys[@]}"; do
    IFS='|' read -r _ba _bc _bl1 _bl2 _bl3 _bm1 _bm2 _bm3 _brepl _bhermes <<< "${_built_keys[$_bkey]}"
    (
      LLC_REPLACEMENT="$_brepl"
      HERMES_TAG="$_bhermes"
      _blog="/dev/null"
      _out="$(ensure_binary "$_ba" "$_bc" "$_bl1" "$_bl2" "$_bl3" "$_bm1" "$_bm2" "$_bm3" "$_blog")"
      _rc=$?
      local _mdl_short="$_bm1"; [[ "$_bm1" == "$_bm2" && "$_bm2" == "$_bm3" ]] && _mdl_short="${_bm1} x3" || _mdl_short="${_bm1}/${_bm2}/${_bm3}"
      if (( _rc != 0 )); then
        echo "$_bkey" >> "$_build_fail_file"
        printf "[BUILD-FAIL] %3sc | %s/%s/%s | %-30s | %-8s | %-7s | rc=%d | src=%s\n" \
          "$_bc" "$_bl1" "$_bl2" "$_bl3" "$_mdl_short" "$_brepl" "$_bhermes" "$_rc" "$CHAMPSIM_DIR_NAME"
      else
        local _bin_path="${_out#*::}"
        local _bin_short="${_bin_path#$ROOT/}"
        printf "[BUILD-OK]   %3sc | %s/%s/%s | %-30s | %-8s | %-7s | src=%s | bin=%s\n" \
          "$_bc" "$_bl1" "$_bl2" "$_bl3" "$_mdl_short" "$_brepl" "$_bhermes" "$CHAMPSIM_DIR_NAME" "$_bin_short"
      fi
    ) &
    _build_pids+=($!)
    sleep 0.5
  done

  echo "[BUILD-ALL] Waiting for ${#_build_pids[@]} builds..."
  for _bpid in "${_build_pids[@]}"; do
    wait "$_bpid" 2>/dev/null || true
  done

  local -a _failed_keys=()
  if [[ -s "$_build_fail_file" ]]; then
    mapfile -t _failed_keys < "$_build_fail_file"
    echo "[BUILD-ALL] ${#_failed_keys[@]} builds FAILED"
  fi
  rm -f "$_build_fail_file"
  echo "[BUILD-ALL] Complete."

  # ================================================================
  # PHASE B: SYNC ALL — rsync all unique binaries + traces to all hosts
  # ================================================================
  echo "[SYNC-ALL] Syncing binaries and traces to ${#REMOTE_HOSTS[@]} hosts..."
  local -a _sync_pids=()
  local _rsync_ssh="ssh -i $REMOTE_KEY -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o ControlMaster=auto -o ControlPath=${REMOTE_SSH_CTRL_DIR}/%r@%h:%p -o ControlPersist=600"

  # Collect unique binaries
  declare -A _sync_bins=()
  for _bkey in "${!_built_keys[@]}"; do
    IFS='|' read -r _ba _bc _bl1 _bl2 _bl3 _bm1 _bm2 _bm3 _brepl _bhermes <<< "${_built_keys[$_bkey]}"
    local _cache_key
    _cache_key="$(sanitize "${_ba}_${_bc}_${_bl1}_${_bl2}_${_bl3}_${_bm1}_${_bm2}_${_bm3}_${BRANCH}_${_brepl}_${PGO_FILE}_${WIN_MODE_TAG}_${_bhermes}")"
    local _cbin="${BIN_CACHE_ROOT}/${_cache_key}/champsim"
    if [[ -x "$_cbin" ]]; then
      _sync_bins[$_cbin]=1
    fi
  done

  # Collect unique traces (handle hetero mix: split on HETERO_DELIM)
  declare -A _sync_traces=()
  for _dq_entry in "${_dispatch_queue[@]}"; do
    IFS=$'\t' read -r _dq_cores _dq_model _dq_trace _dq_l1 _dq_l2 _dq_l3 _dq_progress _dq_repl _dq_ocp _dq_hermes <<< "$_dq_entry"
    if [[ "$_dq_trace" == *"${HETERO_DELIM}"* ]]; then
      local -a _mix_hints=()
      IFS="${HETERO_DELIM}" read -ra _mix_hints <<< "$_dq_trace"
      local _mh _tfile
      for _mh in "${_mix_hints[@]}"; do
        _tfile="$(resolve_trace_file "$_mh" 2>/dev/null)" || continue
        _sync_traces[$_tfile]=1
      done
    else
      local _tfile
      _tfile="$(resolve_trace_file "$_dq_trace" 2>/dev/null)" || continue
      _sync_traces[$_tfile]=1
    fi
  done

  for host in "${REMOTE_HOSTS[@]}"; do
    (
      # Create all needed remote dirs
      $REMOTE_SSH cc@$host "mkdir -p '${REMOTE_BASE}/traces' '${REMOTE_BASE}/outputs'" 2>/dev/null

      # Sync all binaries
      for _sbin in "${!_sync_bins[@]}"; do
        local _rbin="${REMOTE_BASE}/${_sbin#$ROOT/}"
        local _rdir; _rdir="$(dirname "$_rbin")"
        $REMOTE_SSH cc@$host "mkdir -p '$_rdir'" 2>/dev/null
        rsync -az -e "$_rsync_ssh" "$_sbin" "cc@$host:${_rbin}" 2>/dev/null || echo "[SYNC-WARN] binary rsync failed: $_sbin → $host"
      done

      # Sync all traces
      local _rtrace_dir="${REMOTE_BASE}/traces"
      for _strace in "${!_sync_traces[@]}"; do
        local _tbase; _tbase="$(basename "$_strace")"
        rsync -az --ignore-existing -e "$_rsync_ssh" "$_strace" "cc@$host:${_rtrace_dir}/${_tbase}" 2>/dev/null || echo "[SYNC-WARN] trace rsync failed: $_strace → $host"
      done

      echo "[SYNC-ALL] $host done: ${#_sync_bins[@]} binaries, ${#_sync_traces[@]} traces"
    ) &
    _sync_pids+=($!)
  done

  echo "[SYNC-ALL] Waiting for ${#_sync_pids[@]} host syncs..."
  for _spid in "${_sync_pids[@]}"; do
    wait "$_spid" 2>/dev/null || true
  done
  echo "[SYNC-ALL] Complete."

  # ================================================================
  # BACKGROUND SWEEP THREAD — pulls logs from remotes every SWEEP_OUT_GREP_PERIOD
  # Runs during dispatch AND poll phases; killed at cleanup
  # ================================================================
  _bg_sweep_active=1
  _bg_sweep_pid=""
  (
    _last_bg_sweep=$(date +%s)
    while (( _bg_sweep_active )); do
      sleep 10
      _now_bg=$(date +%s)
      if (( _now_bg - _last_bg_sweep >= SWEEP_OUT_GREP_PERIOD )); then
        sweep_remote_logs
        _last_bg_sweep=$_now_bg
      fi
    done
  ) &
  _bg_sweep_pid=$!
  echo "[BG-SWEEP] Started background log puller (pid=${_bg_sweep_pid}, interval=${SWEEP_OUT_GREP_PERIOD}s)"

  # ================================================================
  # PHASE C: SEND ALL — get delta per host, blast that many, move to next host
  # ================================================================
  echo "[SEND-ALL] Dispatching ${#_dispatch_queue[@]} sims with ${LAUNCH_DELAY_SEC}s gap..."
  local _dq_idx=0
  local _dq_total=${#_dispatch_queue[@]}

  while (( _dq_idx < _dq_total )); do
    # Pick host and compute available slots
    local _host="${REMOTE_HOSTS[$REMOTE_HOST_IDX]}"
    local _max; _max="$(current_max_procs)"
    [[ "$_max" =~ ^[0-9]+$ ]] || _max=1
    local _running; _running=$(count_running_on_host "$_host")
    local _slots=$(( _max - _running ))
    (( _slots < 0 )) && _slots=0

    if (( _slots == 0 )); then
      # Current host full — rotate to next
      REMOTE_HOST_IDX=$(( (REMOTE_HOST_IDX + 1) % ${#REMOTE_HOSTS[@]} ))
      # Check if we've wrapped around (all hosts full) — wait and retry
      local _checked=0
      while (( _slots == 0 )); do
        (( _checked++ ))
        if (( _checked >= ${#REMOTE_HOSTS[@]} )); then
          echo "[WAIT] All hosts full — ${_dq_idx}/${_dq_total} dispatched — waiting "
          host_status_line
          sleep 43
          reload_remote_hosts
          _max="$(current_max_procs)"
          [[ "$_max" =~ ^[0-9]+$ ]] || _max=1
          _checked=0
        fi
        _host="${REMOTE_HOSTS[$REMOTE_HOST_IDX]}"
        _running=$(count_running_on_host "$_host")
        _slots=$(( _max - _running ))
        (( _slots < 0 )) && _slots=0
        if (( _slots == 0 )); then
          REMOTE_HOST_IDX=$(( (REMOTE_HOST_IDX + 1) % ${#REMOTE_HOSTS[@]} ))
        fi
      done
      echo ""
    fi

    local _slbl; _slbl="$(host_label "$_host")"
    echo "[SEND-ALL] ${_slbl} ${_host}: ${_running}/${_max} running, ${_slots} slots available"

    # Fire _slots jobs to this host
    local _sent=0
    while (( _sent < _slots && _dq_idx < _dq_total )); do
      local _dq_entry="${_dispatch_queue[$_dq_idx]}"
      IFS=$'\t' read -r _dq_cores _dq_model _dq_trace _dq_l1 _dq_l2 _dq_l3 _dq_progress _dq_repl _dq_ocp _dq_hermes <<< "$_dq_entry"

      LLC_REPLACEMENT="$_dq_repl"
      REPL_TAG_OVERRIDE="$_dq_repl"
      HERMES_TAG="$_dq_hermes"

      drain_pending

      _ASSIGNED_HOST="$_host" PHASE_TAG="$_dq_progress" run_one "$_dq_cores" "$_dq_model" "$_dq_trace" "$_dq_l1" "$_dq_l2" "$_dq_l3" &
      (( _phase_launched++ )) || true
      (( _sent++ )) || true
      (( _dq_idx++ )) || true
      sleep "$LAUNCH_DELAY_SEC"
    done

    echo "[SEND-ALL] Sent ${_sent} to ${_slbl} ${_host}"
    # Move to next host
    REMOTE_HOST_IDX=$(( (REMOTE_HOST_IDX + 1) % ${#REMOTE_HOSTS[@]} ))
  done

  # Update global phase tracking for progress string
  phase_processed[$_phase_idx]=$phase_currCnt
  echo "========== PHASE ${phase_num}/5 COMPLETE: ${phase_currCnt} processed, ${_phase_launched} launched, ${_phase_skipped} skipped, ${_phase_dryrun} dry-run =========="
}

echo "=== SWEEP BREAKDOWN: P1=${phase1_total} | P2=${phase2_total} | P3=${phase3_total} | TOTAL=$((phase1_total+phase2_total+phase3_total)) ==="

for cores in "${CORE_LIST[@]}"; do
  echo "====== ALL PHASES FOR ${cores}c ======"
  sweep_pass "ALL: trace-grouped" 0 1 "$((phase1_total+phase2_total+phase3_total))" "$cores"
done

if [[ -n "${RUNS_DUMP_PATH:-}" ]]; then
  echo "[runsDump] wrote planned runs to: $RUNS_DUMP_PATH"
  exit 0
fi


# ---- WAIT: poll remote hosts until all sims finish ----
if (( DRY_RUN == 0 )); then
  echo "[INFO] All ${currRunCnt} sims dispatched to remote hosts. Waiting for completion..."
  echo "[INFO] Background sweep thread pulling logs every ${SWEEP_OUT_GREP_PERIOD}s (pid=${_bg_sweep_pid:-?})"
  _last_health=$(date +%s)
  _health_interval=1800
  while true; do
    drain_pending
    reload_remote_hosts
    total_running=$(count_total_remote_running)
    pending_lines=$(wc -l < "$PENDING_FILE" 2>/dev/null || echo 0)
    echo "[POLL] $(date +%H:%M:%S) | running=${total_running} | pending=${pending_lines}"

    _now_health=$(date +%s)
    if (( _now_health - _last_health >= _health_interval )); then
      sweep_remote_logs
      _done_cnt=$(grep -rl "FINAL ROI CORE AVG IPC" "$OUT_ROOT_ABS"/core*/ 2>/dev/null | wc -l)
      _ddl_cnt=0; _asrt_cnt=0; _err_cnt=0
      while IFS= read -r _hlog; do
        grep -q "DEADLOCK" "$_hlog" 2>/dev/null && (( _ddl_cnt++ )) || true
        grep -q "assert" "$_hlog" 2>/dev/null && (( _asrt_cnt++ )) || true
        grep -qi "error\|fail" "$_hlog" 2>/dev/null && (( _err_cnt++ )) || true
      done < <(find "$OUT_ROOT_ABS"/core*/ -name '*.log' -newer "$OUT_ROOT_ABS" 2>/dev/null)
      echo "[HEALTH] $(date +%H:%M:%S) | done=${_done_cnt} | DDL=${_ddl_cnt} ASRT=${_asrt_cnt} ERR=${_err_cnt}"
      _last_health=$_now_health
    fi

    (( total_running == 0 && pending_lines == 0 )) && break
    sleep 43
  done
  echo ""

  # Kill background sweep thread
  if [[ -n "${_bg_sweep_pid:-}" ]]; then
    kill "$_bg_sweep_pid" 2>/dev/null; wait "$_bg_sweep_pid" 2>/dev/null || true
    echo "[BG-SWEEP] Stopped background log puller"
  fi

  # Final sweep to get all remaining logs
  sweep_remote_logs

  # Process pulled logs — append results for any that completed
  echo "[POST] Processing pulled logs..."
  while IFS= read -r _pulled_log; do
    # Check if already in results CSV (avoid duplicates)
    _pulled_base="$(basename "$_pulled_log")"
    if grep -qF "$_pulled_base" "$RESULTS_FILE" 2>/dev/null; then
      continue
    fi
    # Extract trace hint from log header
    _trace_hint=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*trace=\([^ ]*\).*/\1/p')
    _l1m=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*l1_model=\([^ ]*\).*/\1/p')
    _l2m=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*l2_model=\([^ ]*\).*/\1/p')
    _l3m=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*llc_model=\([^ ]*\).*/\1/p')
    [[ -n "${_l2m:-}" ]] || _l2m="$_l1m"
    [[ -n "${_l3m:-}" ]] || _l3m="$_l1m"
    _model_tag="$(sanitize "${_l1m}_${_l2m}_${_l3m}")"
    _l1_pf=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*L1=\([^ ]*\).*/\1/p')
    _l2_pf=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*L2=\([^ ]*\).*/\1/p')
    _l3_pf=$(grep -m1 '^\[START\]' "$_pulled_log" 2>/dev/null | sed -n 's/.*L3=\([^ ]*\).*/\1/p')
    # Parse ocp/repl from filename: ARCH..CORESc..TRACE..L1..L2..L3..BP..REPL..ByP..M1..M2..M3..WIN..OCP.log
    _fname="$(basename "$_pulled_log" .log)"
    _ocp_tag=$(echo "$_fname" | awk -F'\\.\\.' '{print $NF}')
    _repl_tag=$(echo "$_fname" | awk -F'\\.\\.' '{print $8}')
    if [[ -n "$_trace_hint" && -n "$_model_tag" ]]; then
      append_result_row "$_trace_hint" "$_model_tag" "${_l1_pf:-no}" "${_l2_pf:-no}" "${_l3_pf:-no}" "${_ocp_tag:-off}" "${_repl_tag:-lru}" "$_pulled_log"
    fi
  done < <(grep -rl "FINAL ROI CORE AVG IPC" "$OUT_ROOT_ABS"/core*/ 2>/dev/null)
fi

if (( DRY_RUN == 1 )); then
  echo "=== DRY-RUN COMPLETE ==="
  echo "Would run: ${DRYRUN_WOULD}"
  echo "Skipped (resume): ${DRYRUN_SKIP}"
  [[ -n "$RESUME_DIR" ]] && echo "Resume dir: ${RESUME_DIR}"
else
  echo "DONE — total scheduled: ${totRunCnt}, last dispatched: #${currRunCnt}"
  echo "Results : ${RESULTS_FILE}"
  echo "Failures: ${FAILED_FILE}"
  echo "Pending file: ${PENDING_FILE}"
  echo "Bin cache: ${BIN_CACHE_ROOT}"
  [[ -n "$RESUME_DIR" ]] && echo "Resumed from: ${RESUME_DIR}"
fi