#!/bin/sh
# win_qrun.sh — native-Windows run, parity with Linux qrun_champsim_v14.sh.
# Drops Linux-only perf/gdb/LD_PRELOAD. Adds RealTime priority + CPU affinity
# (the thing the timing campaign needs). All scratch goes to $TMPDIR, never the repo.
#
# BANNED per project rule: no 2>/dev/null, no 2>&1, no ||true, no ||echo 0.
#
# Usage:
#   sh win_qrun.sh --dir DIR [--bp BP] [--repl R] [--win-mode M] [--hermes H] \
#        [--l1byp M] [--l2byp M] [--l3byp M] [--arch A] [--cores N] \
#        [--warmup N] [--sim N] [--bypass none|MODEL] \
#        [--rt on|off] [--affinity HEX] [--runs N] \
#        <trace_substring> [<L1pref> <L2pref> <L3pref>]
# Defaults: bp=perceptron repl=lru win-mode=fixed hermes=off arch=glc cores=1
#           rt=on affinity=F0 (cores 4-7) runs=1 ; warmup/sim auto-computed if omitted.
set -e

# Self-locate: run from anywhere. Resources are repo-root-relative.
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)   # repo root = parent of quickSim/
cd "$ROOT" || { echo "FATAL: cannot cd to repo root $ROOT"; exit 1; }

arch="${arch:-glc}"; NUM_CORES="${NUM_CORES:-1}"
BP="${BP:-perceptron}"; REPL="${REPL:-lru}"; WIN_MODE="${WIN_MODE:-fixed}"; OCP="${OCP:-off}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"; L2_BYP_MODEL="${L2_BYP_MODEL:-no}"; L3_BYP_MODEL="${L3_BYP_MODEL:-no}"
BYPASS="${BYPASS_MODEL:-none}"
tracesDirName="${tracesDirName:-traces}"; TRACE_PERCENT="${TRACE_PERCENT:-20}"
RT="on"; AFFINITY="F0"; RUNS=1; WARMUP=""; SIM=""
champsimDirName=""; POS=""

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
    --arch)     arch="$2"; shift 2 ;;
    --cores)    NUM_CORES="$2"; shift 2 ;;
    --warmup)   WARMUP="$2"; shift 2 ;;
    --sim)      SIM="$2"; shift 2 ;;
    --bypass)   BYPASS="$2"; shift 2 ;;
    --rt)       RT="$2"; shift 2 ;;
    --affinity) AFFINITY="$2"; shift 2 ;;
    --runs)     RUNS="$2"; shift 2 ;;
    *)          POS="$POS $1"; shift ;;
  esac
done
set -- $POS

[ -z "$champsimDirName" ] && { echo "ERROR: --dir DIR required"; exit 1; }
[ ! -d "./$champsimDirName" ] && { echo "ERROR: ./$champsimDirName not found"; exit 1; }
[ $# -lt 1 ] && { echo "ERROR: <trace_substring> required"; exit 1; }
TRACE_KEY="$1"
prefetcher_L1="${2:-no}"; prefetcher_L2="${3:-no}"; prefetcher_L3="${4:-no}"

# --- resolve trace from traces/ by substring ---
trace_file=$(find "$tracesDirName" -maxdepth 1 -name "*${TRACE_KEY}*.xz" | head -n 1)
[ -z "$trace_file" ] && { echo "ERROR: no trace in $tracesDirName matching *${TRACE_KEY}*.xz"; exit 1; }
trace_name=$(basename "$trace_file" .champsimtrace.xz)

# --- warmup/sim: explicit override wins; else mirror qrun LLM/SPEC logic ---
if [ -z "$WARMUP" ] || [ -z "$SIM" ]; then
  case "$trace_name" in
    [0-9][0-9][0-9].*) WARMUP="${WARMUP:-50000000}"; SIM="${SIM:-200000000}" ;;
    LLM*)
      tc=$(echo "$trace_name" | sed 's/.*_\([0-9]\+\)M.*/\1/')
      sn=$((tc * 1000000)); pc=$(( (sn * TRACE_PERCENT) / 100 ))
      tw=$(( (pc / 1000000) * 1000000 ))
      WARMUP="${WARMUP:-$tw}"; SIM="${SIM:-$((sn - tw - tw))}" ;;
    *) WARMUP="${WARMUP:-50000000}"; SIM="${SIM:-200000000}" ;;
  esac
fi

# --- descriptive binary (win_qbuild naming); fall back to plain champsim.exe ---
if [ "$L1_BYP_MODEL" = "$L2_BYP_MODEL" ] && [ "$L2_BYP_MODEL" = "$L3_BYP_MODEL" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}"
else
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL}"
fi
BIN_NAME="${BP}-${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3}-${REPL}-${NUM_CORES}core-${BYP_SEGMENT}-${WIN_MODE}-${OCP}.exe"
BIN_EXE="./${champsimDirName}/bin/${BIN_NAME}"
if [ ! -f "$BIN_EXE" ]; then
  echo "[warn] descriptive binary not found: $BIN_EXE — falling back to bin/champsim.exe"
  BIN_EXE="./${champsimDirName}/bin/champsim.exe"
fi
[ -f "$BIN_EXE" ] || { echo "ERROR: no binary at $BIN_EXE (build first with win_qbuild.sh)"; exit 1; }

PRIO="RealTime"; [ "$RT" = "off" ] && PRIO="High"
WORK="${TMPDIR:-/tmp}/champsim_scratch"; mkdir -p "$WORK"
BIN_WIN=$(cygpath -w "$BIN_EXE"); TRACE_WIN=$(cygpath -w "$trace_file")

# PowerShell array of NUM_CORES trace copies
TRACES_PS=""; i=0
while [ "$i" -lt "$NUM_CORES" ]; do TRACES_PS="${TRACES_PS}'${TRACE_WIN}',"; i=$((i+1)); done

echo "=== RUN $champsimDirName | trace=$trace_name | cores=$NUM_CORES | warmup=$WARMUP sim=$SIM | prio=$PRIO affinity=0x$AFFINITY | runs=$RUNS ==="
echo "Binary: $BIN_EXE"

median_of() { printf '%s\n' "$@" | sort -n | awk '{a[NR]=$0} END{print a[int((NR+1)/2)]}'; }

times=""; CHK=""; IPC=""
r=1
while [ "$r" -le "$RUNS" ]; do
  LOG="$WORK/${champsimDirName}_run${r}.out"
  PS1="$WORK/_runner_${champsimDirName}_${r}.ps1"
  cat > "$PS1" <<PSEOF
\$ErrorActionPreference='Stop'
\$traces = @(${TRACES_PS})
\$common = @('-warmup_instructions','${WARMUP}','-simulation_instructions','${SIM}','--arch','${arch}','--bypass','${BYPASS}','--pf_l1','${prefetcher_L1}','--pf_l2','${prefetcher_L2}','--pf_l3','${prefetcher_L3}','-traces') + \$traces
\$sw=[System.Diagnostics.Stopwatch]::StartNew()
\$p=Start-Process -FilePath '${BIN_WIN}' -ArgumentList \$common -NoNewWindow -PassThru -RedirectStandardOutput '${LOG}'
try { \$p.ProcessorAffinity=[IntPtr]0x${AFFINITY} } catch { Write-Output ('AFFINITY_FAIL: '+\$_.Exception.Message) }
try { \$p.PriorityClass=[System.Diagnostics.ProcessPriorityClass]::${PRIO} } catch { Write-Output ('PRIO_FAIL(fallback): '+\$_.Exception.Message) }
\$p.WaitForExit()
\$sw.Stop()
Write-Output ('WALL_SECONDS='+[math]::Round(\$sw.Elapsed.TotalSeconds,1))
PSEOF
  WALL=$(powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$(cygpath -w "$PS1")" | sed -n 's/^WALL_SECONDS=//p')
  CHK=$(grep -m1 'Core_0_execution_checksum' "$LOG" | awk '{print $NF}')
  IPC=$(grep -m1 'FINAL ROI CORE AVG IPC' "$LOG" | sed 's/^[[:space:]]*//')
  echo "  run${r}: ${WALL} s | checksum=${CHK}"
  times="$times $WALL"
  r=$((r+1))
done

MED=$(median_of $times)
echo "=== MEDIAN=${MED} s | checksum=${CHK} | ${IPC} ==="
echo "(logs in $WORK)"
