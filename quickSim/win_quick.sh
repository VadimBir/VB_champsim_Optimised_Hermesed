#!/bin/sh
# win_quick.sh — native-Windows orchestrator, parity with Linux quick_v14.sh.
# Flow: resolve prefetchers + bypass models -> CONFIG bypass src/defines ->
#       win_qbuild.sh (build via Makefile.win) -> win_qrun.sh (RT+affinity run).
# Drops Linux-only: malloc/LD_PRELOAD, perf, drain, multi-process fan-out.
#
# BANNED per project rule: no 2>/dev/null, no 2>&1, no ||true, no ||echo 0.
#
# Usage:
#   sh win_quick.sh --dir DIR [-1 L1pref] [-2 L2pref] [-3 L3pref] \
#       [--bp BP] [--repl R] [--win-mode M] [--ocp off|hermes|ttp|hmp] \
#       [--l1byp M|--l2byp M|--l3byp M|--allByp M] \
#       [--arch A] [-c N] [--trace SUBSTR] [--warmup N] [--sim N] \
#       [--rt on|off] [--affinity HEX] [--runs N]
# Defaults: pf=no/no/no bp=perceptron repl=lru win=fixed ocp=off byp=no
#           arch=glc cores=1 trace=LLM256.Pythia-70M rt=on affinity=F0 runs=1
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)   # repo root = parent of quickSim/
cd "$ROOT" || { echo "FATAL: cannot cd to repo root $ROOT"; exit 1; }

champsimDirName=""; L1="no"; L2="no"; L3="no"
BP="perceptron"; REPL="lru"; WIN_MODE="fixed"; OCP="off"; arch="glc"; NUM_CORES=1
trace="LLM256.Pythia-70M"; WARMUP=""; SIM=""; RT="on"; AFFINITY="F0"; RUNS=1
L1_BYP_MODEL="no"; L2_BYP_MODEL="no"; L3_BYP_MODEL="no"; BYPASS_MODEL="no"

# inject "[model: FNAME]" into the binary's startup cout (idempotent), like quick_v14 byp_cp
byp_cp() { fn=$(basename "$1"); sed "/\[model:/!s|cout << \"|cout << \"[model: ${fn}] |" "$1" > "$2"; }

# partial-match resolver (no 2>/dev/null per project rule); echoes resolved basename
resolve_partial() {
  rp_dir="$1"; rp_suf="$2"; rp_in="$3"; rp_lbl="${4:-file}"
  [ -f "$rp_dir/${rp_in}${rp_suf}" ] && { echo "$rp_in"; return 0; }
  rp_hits=$(find "$rp_dir" -maxdepth 1 -type f -name "*${rp_in}*${rp_suf}" -not -path '*/archive/*')
  if [ -z "$rp_hits" ]; then rp_n=0; else rp_n=$(printf '%s\n' "$rp_hits" | wc -l | tr -d ' '); fi
  if [ "$rp_n" -eq 0 ]; then
    echo "ERROR: no ${rp_lbl} matching *${rp_in}*${rp_suf} in $rp_dir" >&2
    find "$rp_dir" -maxdepth 1 -type f -name "*${rp_suf}" >&2; exit 1
  elif [ "$rp_n" -gt 1 ]; then
    echo "ERROR: ambiguous ${rp_lbl} '${rp_in}':" >&2; printf '%s\n' "$rp_hits" >&2; exit 1
  fi
  basename "$rp_hits" "$rp_suf"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --dir)        champsimDirName="$2"; shift 2 ;;
    -1|--L1)      L1="$2"; shift 2 ;;
    -2|--L2)      L2="$2"; shift 2 ;;
    -3|--L3)      L3="$2"; shift 2 ;;
    --bp)         BP="$2"; shift 2 ;;
    --repl)       REPL="$2"; shift 2 ;;
    --win-mode)   WIN_MODE="$2"; shift 2 ;;
    --ocp|--hermes) OCP="$2"; shift 2 ;;
    --arch)       arch="$2"; shift 2 ;;
    -c|--cores)   NUM_CORES="$2"; shift 2 ;;
    --trace)      trace="$2"; shift 2 ;;
    --warmup)     WARMUP="$2"; shift 2 ;;
    --sim)        SIM="$2"; shift 2 ;;
    --rt)         RT="$2"; shift 2 ;;
    --affinity)   AFFINITY="$2"; shift 2 ;;
    --runs)       RUNS="$2"; shift 2 ;;
    --l1byp)      BYP_L1_IN="$2"; shift 2 ;;
    --l2byp)      BYP_L2_IN="$2"; shift 2 ;;
    --l3byp)      BYP_L3_IN="$2"; shift 2 ;;
    --allByp|--allbyp) BYP_ALL_IN="$2"; shift 2 ;;
    -h|--help)    grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

[ -z "$champsimDirName" ] && { echo "ERROR: --dir DIR required"; exit 1; }
[ ! -d "./$champsimDirName" ] && { echo "ERROR: ./$champsimDirName not found"; exit 1; }
BYP_DIR="$champsimDirName/src/ByP_Models"
CHAMP_H="$champsimDirName/inc/champsim.h"

# resolve prefetcher names (partial -> full) when not 'no'
[ "$L1" != "no" ] && L1=$(resolve_partial "$champsimDirName/prefetcher" ".l1d_pref" "$L1" "L1 prefetcher")
[ "$L2" != "no" ] && L2=$(resolve_partial "$champsimDirName/prefetcher" ".l2c_pref" "$L2" "L2 prefetcher")
[ "$L3" != "no" ] && L3=$(resolve_partial "$champsimDirName/prefetcher" ".llc_pref" "$L3" "LLC prefetcher")

# --- bypass model CONFIG (copy .l*_bypass -> ooo_l*_byp_model.cc + toggle defines) ---
enable_def()  { sed -i "s|^[[:space:]]*//[[:space:]]*#define $1|#define $1|" "$CHAMP_H"; }
disable_def() { sed -i "s|^[[:space:]]*#define $1|// #define $1|" "$CHAMP_H"; }

if [ -n "$BYP_ALL_IN" ]; then
  m=$(resolve_partial "$BYP_DIR" ".l1_bypass" "$BYP_ALL_IN" "bypass model")
  byp_cp "$BYP_DIR/${m}.l1_bypass"  "$champsimDirName/src/ooo_l1_byp_model.cc"
  byp_cp "$BYP_DIR/${m}.l2_bypass"  "$champsimDirName/src/ooo_l2_byp_model.cc"
  byp_cp "$BYP_DIR/${m}.llc_bypass" "$champsimDirName/src/ooo_llc_byp_model.cc"
  L1_BYP_MODEL="$m"; L2_BYP_MODEL="$m"; L3_BYP_MODEL="$m"; BYPASS_MODEL="$m"
  enable_def BYPASS_L2_LOGIC; enable_def BYPASS_LLC_LOGIC
  echo "Loaded all-ByP model: $m"
fi
if [ -n "$BYP_L1_IN" ]; then
  m=$(resolve_partial "$BYP_DIR" ".l1_bypass" "$BYP_L1_IN" "L1 bypass model")
  byp_cp "$BYP_DIR/${m}.l1_bypass" "$champsimDirName/src/ooo_l1_byp_model.cc"
  L1_BYP_MODEL="$m"; BYPASS_MODEL="$m"; echo "Loaded L1 bypass model: $m"
fi
if [ -n "$BYP_L2_IN" ]; then
  m=$(resolve_partial "$BYP_DIR" ".l2_bypass" "$BYP_L2_IN" "L2 bypass model")
  byp_cp "$BYP_DIR/${m}.l2_bypass" "$champsimDirName/src/ooo_l2_byp_model.cc"
  L2_BYP_MODEL="$m"; enable_def BYPASS_L2_LOGIC; echo "Loaded L2 bypass model: $m"
fi
if [ -n "$BYP_L3_IN" ]; then
  m=$(resolve_partial "$BYP_DIR" ".llc_bypass" "$BYP_L3_IN" "LLC bypass model")
  byp_cp "$BYP_DIR/${m}.llc_bypass" "$champsimDirName/src/ooo_llc_byp_model.cc"
  L3_BYP_MODEL="$m"; enable_def BYPASS_LLC_LOGIC; echo "Loaded LLC bypass model: $m"
fi

echo "L1=$L1 L2=$L2 L3=$L3 | bp=$BP repl=$REPL arch=$arch cores=$NUM_CORES win=$WIN_MODE ocp=$OCP"
echo "byp: L1=$L1_BYP_MODEL L2=$L2_BYP_MODEL L3=$L3_BYP_MODEL (bypass=$BYPASS_MODEL)"

# --- BUILD ---
echo "    *** BUILDING (win_qbuild.sh) ***"
sh "$HERE/win_qbuild.sh" --dir "$champsimDirName" --repl "$REPL" --cores "$NUM_CORES" \
   --win-mode "$WIN_MODE" --hermes "$OCP" --arch "$arch" --bp "$BP" \
   --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" "$L1" "$L2" "$L3"

# --- RUN ---
echo "    *** RUNNING (win_qrun.sh) ***"
BYPASS_MODEL="$BYPASS_MODEL" sh "$HERE/win_qrun.sh" --dir "$champsimDirName" --bp "$BP" --repl "$REPL" \
   --win-mode "$WIN_MODE" --hermes "$OCP" --arch "$arch" --cores "$NUM_CORES" \
   --l1byp "$L1_BYP_MODEL" --l2byp "$L2_BYP_MODEL" --l3byp "$L3_BYP_MODEL" \
   --bypass "${BYPASS_MODEL:-none}" --rt "$RT" --affinity "$AFFINITY" --runs "$RUNS" \
   ${WARMUP:+--warmup "$WARMUP"} ${SIM:+--sim "$SIM"} \
   "$trace" "$L1" "$L2" "$L3"
