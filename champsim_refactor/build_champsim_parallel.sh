#!/bin/bash
set -euo pipefail

if [ "$#" -ne 5 ]; then
    echo "Illegal number of parameters"
    echo "Usage: ./build_champsim_parallel.sh [core_arch] [l1d_pref] [l2c_pref] [llc_pref] [num_core]"
    exit 1
fi

CORE_UARCH=$1
L1D_PREFETCHER=$2
L2C_PREFETCHER=$3
LLC_PREFETCHER=$4
NUM_CORE=$5

BRANCH="${BP:-perceptron}"
LLC_REPLACEMENT="${REPL:-lru}"
WIN_MODE_TAG="${WIN_MODE:-fixed}"
HERMES_TAG="${HERMES:-off}"
L1_BYP_TAG="${L1_BYP_MODEL:-no}"
L2_BYP_TAG="${L2_BYP_MODEL:-no}"
L3_BYP_TAG="${L3_BYP_MODEL:-no}"
PGO_FILE="profiles_v10/PGO_10_SPEC_LLM.profdata"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_BASE="/tmp/champsim_builds"
mkdir -p "$WORK_BASE"
WORKDIR="$(mktemp -d "$WORK_BASE/build.XXXXXX")"

# JOBS="${JOBS:-$(nproc --ignore=6)}"
JOBS=4

if command -v tput >/dev/null 2>&1; then
    BOLD=$(tput bold)
    NORMAL=$(tput sgr0)
else
    BOLD=""
    NORMAL=""
fi

cleanup() {
    status=$?
    if [ $status -eq 0 ]; then
        rm -rf "$WORKDIR"
    else
        echo ""
        echo "[ERROR] Build failed. Workdir kept for debug:"
        echo "        $WORKDIR"
    fi
    exit $status
}
trap cleanup EXIT INT TERM

re='^[0-9]+$'
if ! [[ "$NUM_CORE" =~ $re ]]; then
    echo "[ERROR] num_core is NOT a number <${NUM_CORE}>"
    exit 1
fi

if [ "$NUM_CORE" -lt 1 ]; then
    echo "[ERROR] Number of cores must be >= 1"
    exit 1
fi

if [ ! -f "$ROOT/inc/Arch/${CORE_UARCH}.h" ]; then
    echo "[ERROR] Cannot find core architecture definition ${CORE_UARCH}"
    find "$ROOT/inc/Arch" -name "*.h"
    exit 1
fi

if [ ! -f "$ROOT/branch/${BRANCH}.bpred" ]; then
    echo "[ERROR] Cannot find branch predictor ${BRANCH}"
    find "$ROOT/branch" -name "*.bpred"
    exit 1
fi

if [ ! -f "$ROOT/prefetcher/${L1D_PREFETCHER}.l1d_pref" ]; then
    echo "[ERROR] Cannot find L1D prefetcher ${L1D_PREFETCHER}"
    find "$ROOT/prefetcher" -name "*.l1d_pref"
    exit 1
fi

if [ ! -f "$ROOT/prefetcher/${L2C_PREFETCHER}.l2c_pref" ]; then
    echo "[ERROR] Cannot find L2C prefetcher ${L2C_PREFETCHER}"
    find "$ROOT/prefetcher" -name "*.l2c_pref"
    exit 1
fi

if [ ! -f "$ROOT/prefetcher/${LLC_PREFETCHER}.llc_pref" ]; then
    echo "[ERROR] Cannot find LLC prefetcher ${LLC_PREFETCHER}"
    find "$ROOT/prefetcher" -name "*.llc_pref"
    exit 1
fi

if [ ! -f "$ROOT/replacement/${LLC_REPLACEMENT}.llc_repl" ]; then
    echo "[ERROR] Cannot find LLC replacement policy ${LLC_REPLACEMENT}"
    find "$ROOT/replacement" -name "*.llc_repl"
    exit 1
fi

if [ ! -f "$ROOT/$PGO_FILE" ]; then
    echo "[ERROR] Missing required PGO file: $ROOT/$PGO_FILE"
    exit 1
fi

mkdir -p "$WORKDIR"

rsync -a --prune-empty-dirs \
    --include='/Makefile' \
    --include='/inc/***' \
    --include='/src/***' \
    --include='/branch/***' \
    --include='/prefetcher/***' \
    --include='/replacement/***' \
    --include='/profiles_v10/' \
    --include='/profiles_v10/PGO_10_SPEC_LLM.profdata' \
    --exclude='/obj/***' \
    --exclude='/bin/***' \
    --exclude='/tracer/***' \
    --exclude='*.o' \
    --exclude='*.d' \
    --exclude='*.bak' \
    --exclude='*' \
    "$ROOT/" "$WORKDIR/"

mkdir -p "$WORKDIR/bin"

cp "$ROOT/inc/Arch/${CORE_UARCH}.h" "$WORKDIR/inc/defs.h"
cp "$ROOT/branch/${BRANCH}.bpred" "$WORKDIR/branch/branch_predictor.cc"
cp "$ROOT/prefetcher/${L1D_PREFETCHER}.l1d_pref" "$WORKDIR/prefetcher/l1d_prefetcher.cc"
cp "$ROOT/prefetcher/${L2C_PREFETCHER}.l2c_pref" "$WORKDIR/prefetcher/l2c_prefetcher.cc"
cp "$ROOT/prefetcher/${LLC_PREFETCHER}.llc_pref" "$WORKDIR/prefetcher/llc_prefetcher.cc"
cp "$ROOT/replacement/${LLC_REPLACEMENT}.llc_repl" "$WORKDIR/replacement/llc_replacement.cc"

sed -i.bak "s/^#define NUM_CPUS [0-9][0-9]*/#define NUM_CPUS ${NUM_CORE}/" "$WORKDIR/inc/champsim.h"

echo "Building ChampSim in isolated workdir: $WORKDIR"
echo "Cores: ${NUM_CORE} | Core architecture: ${CORE_UARCH}"
echo "L1D: ${L1D_PREFETCHER} | L2C: ${L2C_PREFETCHER} | LLC: ${LLC_PREFETCHER}"
echo "Replacement: ${LLC_REPLACEMENT}"
echo "PGO: ${PGO_FILE}"
echo

case "$WIN_MODE_TAG" in
  dyn)     EXTRA_DEFS="-DLPM_WIN_DYN" ;;
  overlap) EXTRA_DEFS="-DLPM_WIN_OVERLAP" ;;
  *)       EXTRA_DEFS="-DLPM_WIN_FIXED" ;;
esac
case "$HERMES_TAG" in
  on|hermes) EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES" ;;
  ttp)       EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES -DUSE_HERMES_TTP" ;;
  hmp)       EXTRA_DEFS="$EXTRA_DEFS -DUSE_HERMES -DUSE_HERMES_HMP" ;;
  *)         EXTRA_DEFS="$EXTRA_DEFS -DNO_HERMES" ;;
esac
# EXTRA_DEFS="$EXTRA_DEFS -DDEBUG_PRINT"  # gated via inc/champsim.h instead

## Build (or skip via LTO cache) happens below

## LTO binary cache — hash all source inputs, skip entire build on hit
LTO_CACHE_DIR="$ROOT/.lto_cache"
mkdir -p "$LTO_CACHE_DIR"
SRC_HASH=$(cd "$WORKDIR" && { find src inc branch prefetcher replacement -type f \( -name '*.cc' -o -name '*.h' -o -name '*.bpred' \) -print0 | sort -z | xargs -0 md5sum; md5sum Makefile 2>/dev/null || true; md5sum "$ROOT/build_champsim_parallel.sh" 2>/dev/null || true; md5sum merged.profdata 2>/dev/null || true; } | md5sum | cut -d' ' -f1)
# Include EXTRA_DEFS in hash (affects codegen)
FULL_HASH=$(echo "${SRC_HASH}_${EXTRA_DEFS}" | md5sum | cut -d' ' -f1)

ts() { date +%H:%M:%S.%3N; }
echo "[$(ts)] [LTO-CACHE] Lookup: SRC_HASH=$SRC_HASH EXTRA_DEFS='$EXTRA_DEFS' FULL_HASH=$FULL_HASH"
echo "[$(ts)] [LTO-CACHE] Checking: $LTO_CACHE_DIR/$FULL_HASH exists? $([ -f "$LTO_CACHE_DIR/$FULL_HASH" ] && echo YES || echo NO)"
if [ -f "$LTO_CACHE_DIR/$FULL_HASH" ]; then
    echo "[$(ts)] [LTO-CACHE] HIT — skipping build ($FULL_HASH)"
    mkdir -p "$WORKDIR/bin"
    cp -p "$LTO_CACHE_DIR/$FULL_HASH" "$WORKDIR/bin/champsim"
    echo "[$(ts)] [LTO-CACHE] HIT done — copied binary (mtime preserved)"
else
    echo "[$(ts)] [LTO-CACHE] MISS — running full build"
    (
        cd "$WORKDIR"
        export CCACHE_BASEDIR="$WORKDIR"
        export CCACHE_SLOPPINESS="time_macros,include_file_ctime,include_file_mtime,locale,pch_defines"
        export CCACHE_NOHASHDIR=1
        export CCACHE_COMPILERCHECK=content
        make -j"$JOBS" run_clang EXTRA_CFLAGS="$EXTRA_DEFS" 2>&1
    )
    if [ ! -f "$WORKDIR/bin/champsim" ]; then
        echo "${BOLD}ChampSim build FAILED!${NORMAL}"
        exit 1
    fi
    cp -p "$WORKDIR/bin/champsim" "$LTO_CACHE_DIR/$FULL_HASH"
    echo "[$(ts)] [LTO-CACHE] Stored ($FULL_HASH, mtime=build time)"
fi
echo "[$(ts)] [LTO-CACHE] Block complete, continuing to copy binary"

mkdir -p "$ROOT/bin"

if [ "$L1_BYP_TAG" = "$L2_BYP_TAG" ] && [ "$L2_BYP_TAG" = "$L3_BYP_TAG" ]; then
    BYP_SEGMENT="ByP-${L1_BYP_TAG}"
else
    BYP_SEGMENT="ByP-${L1_BYP_TAG}-${L2_BYP_TAG}-${L3_BYP_TAG}"
fi
BINARY_NAME="${BRANCH}-${L1D_PREFETCHER}-${L2C_PREFETCHER}-${LLC_PREFETCHER}-${LLC_REPLACEMENT}-${NUM_CORE}core-${BYP_SEGMENT}-${WIN_MODE_TAG}-${HERMES_TAG}"
rm -f "$ROOT/bin/${BINARY_NAME}"
cp -p "$WORKDIR/bin/champsim" "$ROOT/bin/${BINARY_NAME}"

echo ""
echo "${BOLD}ChampSim is successfully built${NORMAL}"
echo "Branch Predictor: ${BRANCH}"
echo "L1D Prefetcher: ${L1D_PREFETCHER}"
echo "L2C Prefetcher: ${L2C_PREFETCHER}"
echo "LLC Prefetcher: ${LLC_PREFETCHER}"
echo "LLC Replacement: ${LLC_REPLACEMENT}"
echo "Cores: ${NUM_CORE}"
echo "Binary: $ROOT/bin/${BINARY_NAME}"