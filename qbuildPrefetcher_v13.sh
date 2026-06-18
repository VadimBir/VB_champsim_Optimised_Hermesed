#!/bin/sh
arch="${arch:-glc}"
NUM_CORES="${NUM_CORES:-2}"
isDebug="${isDebug:--1}"

champsimDirName=""
prefetcher_L1=""; prefetcher_L2=""; prefetcher_L3=""
REPL="${REPL:-lru}"
WIN_MODE="${WIN_MODE:-fixed}"
HERMES="${HERMES:-off}"
L1_BYP_MODEL="${L1_BYP_MODEL:-no}"
L2_BYP_MODEL="${L2_BYP_MODEL:-no}"
L3_BYP_MODEL="${L3_BYP_MODEL:-no}"

while [ $# -gt 0 ]; do
  case "$1" in
    --dir)      champsimDirName="$2"; shift 2 ;;
    --repl)     REPL="$2"; shift 2 ;;
    --win-mode) WIN_MODE="$2"; shift 2 ;;
    --hermes)   HERMES="$2"; shift 2 ;;
    --l1byp)    L1_BYP_MODEL="$2"; shift 2 ;;
    --l2byp)    L2_BYP_MODEL="$2"; shift 2 ;;
    --l3byp)    L3_BYP_MODEL="$2"; shift 2 ;;
    *) [ -z "$prefetcher_L1" ] && { prefetcher_L1="$1"; shift; continue; }
       [ -z "$prefetcher_L2" ] && { prefetcher_L2="$1"; shift; continue; }
       [ -z "$prefetcher_L3" ] && { prefetcher_L3="$1"; shift; continue; }
       shift ;;
  esac
done

[ -z "$champsimDirName" ] && { echo "ERROR: --dir required"; exit 1; }
[ ! -d "./$champsimDirName" ] && { echo "ERROR: ./$champsimDirName not found"; exit 1; }

export REPL WIN_MODE HERMES L1_BYP_MODEL L2_BYP_MODEL L3_BYP_MODEL
echo "=== BUILD [$champsimDirName] pf=${prefetcher_L1}-${prefetcher_L2}-${prefetcher_L3} repl=${REPL} win=${WIN_MODE} hermes=${HERMES} byp=${L1_BYP_MODEL}-${L2_BYP_MODEL}-${L3_BYP_MODEL} cores=${NUM_CORES} ==="
cd "./${champsimDirName}" || exit 1
./build_champsim_parallel.sh "${arch}_${NUM_CORES}c" "${prefetcher_L1}" "${prefetcher_L2}" "${prefetcher_L3}" "${NUM_CORES}" || exit 1
cd ..
