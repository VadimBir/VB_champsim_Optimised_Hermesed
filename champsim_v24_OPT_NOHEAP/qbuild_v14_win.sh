#!/bin/sh
# qbuild_v14_win.sh — native-Windows (w64devkit g++) port of qbuildPrefetcher_v14.sh.
# Runs INSIDE champsim_v17 (so relative inc/ prefetcher/ replacement/ branch/ bin/ work).
# Drives Makefile.win (NOT build_champsim_parallel.sh, which is Linux-only:
# rsync/ccache/clang-PGO/LTO/tmp). Does the CONFIG step itself (Makefile.win does not).
#
# BANNED per project rule: no 2>/dev/null, 2>&1, ||true, ||echo 0. Errors must surface.
#
# Usage:
#   sh qbuild_v14_win.sh <L1> <L2> <L3> [--repl R] [--cores N] [--win-mode M]
#                        [--hermes H] [--l1byp M] [--l2byp M] [--l3byp M] [--arch A] [--bp BP]
# Defaults: repl=lru cores=1 win=fixed hermes=off byp=no arch=glc BP=perceptron

set -e

arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-1}"

prefetcher_L1=""; prefetcher_L2=""; prefetcher_L3=""
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
HERMES="${HERMES:-off}"
BP="${BP:-perceptron}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"

while [ $# -gt 0 ]; do
  case "$1" in
    --repl)     REPL="$2"; shift 2 ;;
    --cores)    NUM_CORES="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   HERMES="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    --arch)     arch="$2"; shift 2 ;;
    --bp)       BP="$2"; shift 2 ;;
    *) [ -z "$prefetcher_L1" ] && { prefetcher_L1="$1"; shift; continue; }
       [ -z "$prefetcher_L2" ] && { prefetcher_L2="$1"; shift; continue; }
       [ -z "$prefetcher_L3" ] && { prefetcher_L3="$1"; shift; continue; }
       shift ;;
  esac
done

[ -z "$prefetcher_L1" ] && { echo "ERROR: L1 prefetcher (positional 1) required"; exit 1; }
[ -z "$prefetcher_L2" ] && { echo "ERROR: L2 prefetcher (positional 2) required"; exit 1; }
[ -z "$prefetcher_L3" ] && { echo "ERROR: L3 prefetcher (positional 3) required"; exit 1; }

CORE_UARCH="${arch}_${NUM_CORES}c"

# --- num_core sanity (mirror build_champsim_parallel.sh) ---
case "$NUM_CORES" in
  ''|*[!0-9]*) echo "[ERROR] num_core is NOT a number <${NUM_CORES}>"; exit 1 ;;
esac
[ "$NUM_CORES" -lt 1 ] && { echo "[ERROR] Number of cores must be >= 1"; exit 1; }

# --- source-file existence checks (mirror build script) ---
[ -f "inc/Arch/${CORE_UARCH}.h" ]            || { echo "[ERROR] Cannot find core architecture inc/Arch/${CORE_UARCH}.h"; find inc/Arch -name '*.h'; exit 1; }
[ -f "branch/${BP}.bpred" ]                  || { echo "[ERROR] Cannot find branch predictor branch/${BP}.bpred"; find branch -name '*.bpred'; exit 1; }
[ -f "prefetcher/${prefetcher_L1}.l1d_pref" ]|| { echo "[ERROR] Cannot find L1D prefetcher prefetcher/${prefetcher_L1}.l1d_pref"; find prefetcher -name '*.l1d_pref'; exit 1; }
[ -f "prefetcher/${prefetcher_L2}.l2c_pref" ]|| { echo "[ERROR] Cannot find L2C prefetcher prefetcher/${prefetcher_L2}.l2c_pref"; find prefetcher -name '*.l2c_pref'; exit 1; }
[ -f "prefetcher/${prefetcher_L3}.llc_pref" ]|| { echo "[ERROR] Cannot find LLC prefetcher prefetcher/${prefetcher_L3}.llc_pref"; find prefetcher -name '*.llc_pref'; exit 1; }
[ -f "replacement/${REPL}.llc_repl" ]        || { echo "[ERROR] Cannot find LLC replacement replacement/${REPL}.llc_repl"; find replacement -name '*.llc_repl'; exit 1; }

echo "=== CONFIG [champsim_v17] pf=${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3} repl=${REPL} bp=${BP} arch=${CORE_UARCH} win=${WIN_MODE} hermes=${HERMES} byp=${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL} cores=${NUM_CORES} ==="

# --- CONFIG step (exactly what build_champsim*.sh does) ---
cp "inc/Arch/${CORE_UARCH}.h"            "inc/defs.h"
cp "branch/${BP}.bpred"                  "branch/branch_predictor.cc"
cp "prefetcher/${prefetcher_L1}.l1d_pref" "prefetcher/l1d_prefetcher.cc"
cp "prefetcher/${prefetcher_L2}.l2c_pref" "prefetcher/l2c_prefetcher.cc"
cp "prefetcher/${prefetcher_L3}.llc_pref" "prefetcher/llc_prefetcher.cc"
cp "replacement/${REPL}.llc_repl"        "replacement/llc_replacement.cc"
sed -i "s/^#define NUM_CPUS [0-9][0-9]*/#define NUM_CPUS ${NUM_CORES}/" "inc/champsim.h"
echo "NUM_CPUS now: $(grep -m1 '^#define NUM_CPUS' inc/champsim.h)"

# --- EXTRA_DEFS (mirror build_champsim_parallel.sh win-mode + hermes codegen) ---
case "$WIN_MODE" in
  dyn)     EXTRA_DEFS="-DLPM_WIN_DYN" ;;
  overlap) EXTRA_DEFS="-DLPM_WIN_OVERLAP" ;;
  *)       EXTRA_DEFS="-DLPM_WIN_FIXED" ;;
esac
case "$HERMES" in
  on|hermes) EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES" ;;
  ttp)       EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES -DUSE_HERMES_TTP" ;;
  hmp)       EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES -DUSE_HERMES_HMP" ;;
  *)         EXTRA_DEFS="$EXTRA_DEFS -DNO_HERMES" ;;
esac

# --- BUILD via Makefile.win (g++) ---
rm -f bin/champsim.exe
echo "=== BUILD: make -f Makefile.win -j4 EXTRA_CFLAGS='$EXTRA_DEFS' ==="
make -f Makefile.win -j4 EXTRA_CFLAGS="$EXTRA_DEFS"
BUILD_RC=$?

if [ "$BUILD_RC" -ne 0 ] || [ ! -f bin/champsim.exe ]; then
  echo "ChampSim build FAILED! (rc=$BUILD_RC, bin/champsim.exe missing)"
  exit 1
fi

# --- descriptive rename so qrun finds it (mirror qrun BYP_SEGMENT logic) ---
if [ "$L1_BYP_MODEL" = "$L2_BYP_MODEL" ] && [ "$L2_BYP_MODEL" = "$L3_BYP_MODEL" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}"
else
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL}"
fi
BINARY_NAME="${BP}-${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3}-${REPL}-${NUM_CORES}core-${BYP_SEGMENT}-${WIN_MODE}-${HERMES}.exe"

rm -f "bin/${BINARY_NAME}"
cp -p "bin/champsim.exe" "bin/${BINARY_NAME}"

# liblzma-5.dll must sit beside the exe at runtime; make already placed it in bin/.
[ -f bin/liblzma-5.dll ] || { echo "[ERROR] bin/liblzma-5.dll missing (exe needs it at runtime)"; exit 1; }

echo ""
echo "ChampSim is successfully built (native Windows)"
echo "Branch Predictor: ${BP}"
echo "L1D Prefetcher:   ${prefetcher_L1}"
echo "L2C Prefetcher:   ${prefetcher_L2}"
echo "LLC Prefetcher:   ${prefetcher_L3}"
echo "LLC Replacement:  ${REPL}"
echo "Cores:            ${NUM_CORES}"
echo "Binary: bin/${BINARY_NAME}  (+ bin/liblzma-5.dll)"
exit 0
