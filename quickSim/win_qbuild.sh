#!/bin/sh
# win_qbuild.sh — native-Windows (w64devkit g++) build, full parity with the Linux
#   qbuildPrefetcher_v14.sh + build_champsim_parallel.sh CONFIG+BUILD pipeline.
# Drives Makefile.win (NOT build_champsim_parallel.sh: that is Linux-only —
#   rsync to /tmp, ccache, clang-PGO, LTO cache). Does the CONFIG step itself.
#
# BANNED per project rule: no 2>/dev/null, no 2>&1, no ||true, no ||echo 0. Errors must surface.
#
# Usage:
#   sh win_qbuild.sh --dir DIR <L1pref> <L2pref> <L3pref> \
#        [--repl R] [--cores N] [--win-mode fixed|dyn|overlap] [--hermes off|on|ttp|hmp] \
#        [--l1byp M] [--l2byp M] [--l3byp M] [--arch A] [--bp BP] [--jobs N]
# Defaults: repl=lru cores=1 win-mode=fixed hermes=off byp=no arch=glc bp=perceptron jobs=4
set -e

# Self-locate: run from anywhere. Resources are repo-root-relative.
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)   # repo root = parent of quickSim/
cd "$ROOT" || { echo "FATAL: cannot cd to repo root $ROOT"; exit 1; }

arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-1}"
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
HERMES="${HERMES:-off}"
BP="${BP:-perceptron}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"
JOBS="${JOBS:-4}"
champsimDirName=""
prefetcher_L1=""; prefetcher_L2=""; prefetcher_L3=""

while [ $# -gt 0 ]; do
  case "$1" in
    --dir)      champsimDirName="$2"; shift 2 ;;
    --repl)     REPL="$2"; shift 2 ;;
    --cores)    NUM_CORES="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   HERMES="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    --arch)     arch="$2"; shift 2 ;;
    --bp)       BP="$2"; shift 2 ;;
    --jobs)     JOBS="$2"; shift 2 ;;
    *) [ -z "$prefetcher_L1" ] && { prefetcher_L1="$1"; shift; continue; }
       [ -z "$prefetcher_L2" ] && { prefetcher_L2="$1"; shift; continue; }
       [ -z "$prefetcher_L3" ] && { prefetcher_L3="$1"; shift; continue; }
       shift ;;
  esac
done

[ -z "$champsimDirName" ] && { echo "ERROR: --dir DIR required"; exit 1; }
[ ! -d "./$champsimDirName" ] && { echo "ERROR: ./$champsimDirName not found"; exit 1; }
[ -z "$prefetcher_L1" ] && { echo "ERROR: L1 prefetcher (positional 1) required"; exit 1; }
[ -z "$prefetcher_L2" ] && { echo "ERROR: L2 prefetcher (positional 2) required"; exit 1; }
[ -z "$prefetcher_L3" ] && { echo "ERROR: L3 prefetcher (positional 3) required"; exit 1; }

case "$NUM_CORES" in ''|*[!0-9]*) echo "[ERROR] cores is NOT a number <${NUM_CORES}>"; exit 1 ;; esac
[ "$NUM_CORES" -lt 1 ] && { echo "[ERROR] cores must be >= 1"; exit 1; }

cd "./$champsimDirName"
[ -f Makefile.win ] || { echo "[ERROR] $champsimDirName/Makefile.win missing (run on a Windows-ported tree)"; exit 1; }
CORE_UARCH="${arch}_${NUM_CORES}c"

# --- source-file existence checks (mirror build_champsim_parallel.sh) ---
[ -f "inc/Arch/${CORE_UARCH}.h" ]             || { echo "[ERROR] no inc/Arch/${CORE_UARCH}.h"; find inc/Arch -name '*.h'; exit 1; }
[ -f "branch/${BP}.bpred" ]                   || { echo "[ERROR] no branch/${BP}.bpred"; find branch -name '*.bpred'; exit 1; }
[ -f "prefetcher/${prefetcher_L1}.l1d_pref" ] || { echo "[ERROR] no prefetcher/${prefetcher_L1}.l1d_pref"; find prefetcher -name '*.l1d_pref'; exit 1; }
[ -f "prefetcher/${prefetcher_L2}.l2c_pref" ] || { echo "[ERROR] no prefetcher/${prefetcher_L2}.l2c_pref"; find prefetcher -name '*.l2c_pref'; exit 1; }
[ -f "prefetcher/${prefetcher_L3}.llc_pref" ] || { echo "[ERROR] no prefetcher/${prefetcher_L3}.llc_pref"; find prefetcher -name '*.llc_pref'; exit 1; }
[ -f "replacement/${REPL}.llc_repl" ]         || { echo "[ERROR] no replacement/${REPL}.llc_repl"; find replacement -name '*.llc_repl'; exit 1; }

echo "=== CONFIG [$champsimDirName] pf=${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3} repl=${REPL} bp=${BP} arch=${CORE_UARCH} win=${WIN_MODE} hermes=${HERMES} byp=${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL} cores=${NUM_CORES} ==="

# --- CONFIG step (exactly what build_champsim*.sh does, in place) ---
cp "inc/Arch/${CORE_UARCH}.h"             "inc/defs.h"
cp "branch/${BP}.bpred"                   "branch/branch_predictor.cc"
cp "prefetcher/${prefetcher_L1}.l1d_pref" "prefetcher/l1d_prefetcher.cc"
cp "prefetcher/${prefetcher_L2}.l2c_pref" "prefetcher/l2c_prefetcher.cc"
cp "prefetcher/${prefetcher_L3}.llc_pref" "prefetcher/llc_prefetcher.cc"
cp "replacement/${REPL}.llc_repl"         "replacement/llc_replacement.cc"
sed -i "s/^#define NUM_CPUS [0-9][0-9]*/#define NUM_CPUS ${NUM_CORES}/" "inc/champsim.h"
echo "NUM_CPUS now: $(grep -m1 '^#define NUM_CPUS' inc/champsim.h)"

# --- EXTRA_DEFS (win-mode + hermes codegen, mirror Linux build) ---
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

# --- clean rebuild via Makefile.win (no header-dep tracking => must clean) ---
rm -rf obj_win bin
echo "=== BUILD: make -f Makefile.win -j${JOBS} EXTRA_CFLAGS='$EXTRA_DEFS' ==="
make -f Makefile.win -j"$JOBS" EXTRA_CFLAGS="$EXTRA_DEFS"

[ -f bin/champsim.exe ] || { echo "ChampSim build FAILED! (bin/champsim.exe missing)"; exit 1; }

# --- descriptive rename so win_qrun.sh finds it (mirror Linux BYP_SEGMENT naming) ---
if [ "$L1_BYP_MODEL" = "$L2_BYP_MODEL" ] && [ "$L2_BYP_MODEL" = "$L3_BYP_MODEL" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}"
else
    BYP_SEGMENT="ByP-${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL}"
fi
BINARY_NAME="${BP}-${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3}-${REPL}-${NUM_CORES}core-${BYP_SEGMENT}-${WIN_MODE}-${HERMES}.exe"
rm -f "bin/${BINARY_NAME}"
cp -p "bin/champsim.exe" "bin/${BINARY_NAME}"
[ -f bin/liblzma-5.dll ] || { echo "[ERROR] bin/liblzma-5.dll missing (exe needs it at runtime)"; exit 1; }

echo ""
echo "ChampSim built (native Windows)"
echo "  bp=${BP} L1=${prefetcher_L1} L2=${prefetcher_L2} L3=${prefetcher_L3} repl=${REPL} cores=${NUM_CORES}"
echo "  Binary: ${champsimDirName}/bin/${BINARY_NAME}  (+ bin/liblzma-5.dll)"
exit 0
