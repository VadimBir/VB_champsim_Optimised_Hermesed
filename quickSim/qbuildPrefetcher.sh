#!/bin/sh
# Self-locate: run from anywhere. Resources ($champsimDirName) are repo-root-relative.
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"   # repo root = parent of quickSim/
cd "$ROOT" || { echo "FATAL: cannot cd to repo root $ROOT"; exit 1; }
arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-1}"
isDebug="${isDebug:--1}"

champsimDirName=""
prefetcher_L1=""; prefetcher_L2=""; prefetcher_L3=""
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
HERMES="${HERMES:-off}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"

PROFILE_BUILD=0
while [ $# -gt 0 ]; do
  case "$1" in
    --dir)      champsimDirName="$2"; shift 2 ;;
    --repl)     REPL="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   HERMES="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    --profile)  PROFILE_BUILD=1; shift ;;
    *) [ -z "$prefetcher_L1" ] && { prefetcher_L1="$1"; shift; continue; }
       [ -z "$prefetcher_L2" ] && { prefetcher_L2="$1"; shift; continue; }
       [ -z "$prefetcher_L3" ] && { prefetcher_L3="$1"; shift; continue; }
       shift ;;
  esac
done

[ -z "$champsimDirName" ] && { echo "ERROR: --dir required"; exit 1; }
[ ! -d "./$champsimDirName" ] && { echo "ERROR: ./$champsimDirName not found"; exit 1; }

export REPL WIN_MODE HERMES L1_BYP_MODEL L2_BYP_MODEL L3_BYP_MODEL

MAKEFILE="./${champsimDirName}/Makefile"
NEED_RESTORE=0
if [ "$PROFILE_BUILD" -eq 1 ]; then
  echo "=== PROFILE BUILD: enabling -g -fno-omit-frame-pointer ==="
  # Check if profile line is already active (uncommented)
  if grep -q '^CFlags.*-g.*-fno-omit-frame-pointer' "$MAKEFILE"; then
    echo "Profile CFlags already active, no swap needed"
  else
    # Profile line is commented, prod line is active — swap them
    PROF_LINE=$(grep -n '^# CFlags.*-g.*-fno-omit-frame-pointer' "$MAKEFILE" | head -1 | cut -d: -f1)
    PROD_LINE=$(grep -n '^CFlags.*-fomit-frame-pointer' "$MAKEFILE" | head -1 | cut -d: -f1)
    if [ -n "$PROF_LINE" ] && [ -n "$PROD_LINE" ]; then
      sed -i "${PROF_LINE}s/^# //" "$MAKEFILE"
      sed -i "${PROD_LINE}s/^CFlags/# CFlags/" "$MAKEFILE"
      NEED_RESTORE=1
    else
      echo "WARN: cannot find profile/prod CFlags lines, building as-is"
    fi
  fi
fi

echo "=== BUILD [$champsimDirName] pf=${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3} repl=${REPL} win=${WIN_MODE} hermes=${HERMES} byp=${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL} cores=${NUM_CORES} ==="
cd "./${champsimDirName}" || exit 1
./build_champsim_parallel.sh "${arch}_${NUM_CORES}c" "${prefetcher_L1}" "${prefetcher_L2}" "${prefetcher_L3}" "${NUM_CORES}"
BUILD_RC=$?
cd ..

if [ "$NEED_RESTORE" -eq 1 ]; then
  echo "=== Restoring Makefile to production CFlags ==="
  sed -i "${PROF_LINE}s/^/# /" "$MAKEFILE"
  sed -i "${PROD_LINE}s/^# //" "$MAKEFILE"
fi

exit $BUILD_RC
