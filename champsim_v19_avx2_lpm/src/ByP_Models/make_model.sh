#!/bin/bash
# Usage: ./make_model.sh <level> <num> <name> "<cout_msg>" "<code_body>"
#
# <level>     : l1 | l2 | llc
# <num>       : model number e.g. 574
# <name>      : model name   e.g. ByP_CamatRatioEquil
# <cout_msg>  : string inserted into cout << "..." << endl;
# <code_body> : C++ code inserted as the function body (before return)
#               Use these placeholders — script replaces them:
#                 <THIS_LVL>   → LPM_L1D / LPM_L2C / LPM_LLC
#                 <NEXT_LVL>   → LPM_L2C / LPM_LLC / LPM_DRAM
#                 <H_THIS>     → 4.0     / 10.0    / 50.0
#                 <H_NEXT>     → 10.0    / 50.0    / 200.0
#                 <RATIO>      → 0.4     / 0.2     / 0.25
#
# Call once per level (3 times total) or use same body for shared logic.
#
# Example — model 574, same ratio logic at all levels:
#   BODY='double camat_this = lpm[cpu][<THIS_LVL>].wsm.camat_activeMemCyDivAccesses;
#   double camat_next = lpm[cpu][<NEXT_LVL>].wsm.camat_activeMemCyDivAccesses;
#   bool byp = (camat_next > 1e-6) && (camat_this / camat_next < <H_THIS> / <H_NEXT>);'
#
#   ./make_model.sh l1  574 ByP_CamatRatioEquil "camat(this)/camat(next)<H(this)/H(next)" "$BODY"
#   ./make_model.sh l2  574 ByP_CamatRatioEquil "camat(this)/camat(next)<H(this)/H(next)" "$BODY"
#   ./make_model.sh llc 574 ByP_CamatRatioEquil "camat(this)/camat(next)<H(this)/H(next)" "$BODY"

set -e

LEVEL=$1
NUM=$2
NAME=$3
MSG=$4
BODY=$5

DIR="$(dirname "$0")"

case "$LEVEL" in
  l1)
    THIS_LVL="LPM_L1D"; NEXT_LVL="LPM_L2C"
    H_THIS="4.0";        H_NEXT="10.0";  RATIO="0.4"
    THIS_CACHE="L1D";    NEXT_CACHE="L2C"
    GUARD="SHALL_L1D_BYPASS_DEFINED"
    INIT_FN="l1d_bypass_initialize"; INIT_VAR="l1_bypass_init_${NUM}"
    OPER_FN="l1d_bypass_operate";    DBG_CTR="l1_dbg_counter_${NUM}"
    LVL_TAG="L1"; INDENT=""
    ;;
  l2)
    THIS_LVL="LPM_L2C"; NEXT_LVL="LPM_LLC"
    H_THIS="10.0";       H_NEXT="50.0";  RATIO="0.2"
    THIS_CACHE="L2C";    NEXT_CACHE="LLC"
    GUARD="SHALL_L2C_BYPASS_DEFINED"
    INIT_FN="l2c_bypass_initialize"; INIT_VAR="l2_bypass_init_${NUM}"
    OPER_FN="l2c_bypass_operate";    DBG_CTR="l2_dbg_counter_${NUM}"
    LVL_TAG="L2"; INDENT="  "
    ;;
  llc)
    THIS_LVL="LPM_LLC";  NEXT_LVL="LPM_DRAM"
    H_THIS="50.0";        H_NEXT="200.0"; RATIO="0.25"
    THIS_CACHE="LLC";    NEXT_CACHE="LLC"
    GUARD="SHALL_LLC_BYPASS_DEFINED"
    INIT_FN="llc_bypass_initialize"; INIT_VAR="llc_bypass_init_${NUM}"
    OPER_FN="llc_bypass_operate";    DBG_CTR="llc_dbg_counter_${NUM}"
    LVL_TAG="LLC"; INDENT="    "
    ;;
  *)
    echo "ERROR: level must be l1 | l2 | llc"; exit 1
    ;;
esac

# substitute placeholders in body
BODY="${BODY//<THIS_LVL>/$THIS_LVL}"
BODY="${BODY//<NEXT_LVL>/$NEXT_LVL}"
BODY="${BODY//<H_THIS>/$H_THIS}"
BODY="${BODY//<H_NEXT>/$H_NEXT}"
BODY="${BODY//<RATIO>/$RATIO}"
BODY="${BODY//<THIS_CACHE>/$THIS_CACHE}"
BODY="${BODY//<NEXT_CACHE>/$NEXT_CACHE}"

OUTFILE="${DIR}/${NUM}-${NAME}.${LEVEL}_bypass"

cat > "$OUTFILE" << HEREDOC
#include "cache.h"
#include "lpm_tracker.h"

#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif

// MODEL ${NUM} — ${NAME} at ${LVL_TAG}
// ${MSG}

#define DBG_${NUM} 1

inline bool ${INIT_VAR}[NUM_CPUS] = {};
inline uint64_t ${DBG_CTR}[NUM_CPUS] = {};

#define ${GUARD}
inline void ${INIT_FN}(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (${INIT_VAR}[cpu]) return;
    cout << "[model: ${NUM}-${NAME}.${LEVEL}_bypass] ${LVL_TAG} Bypass [${NUM}-${NAME}]: ${MSG}" << endl;
    ${INIT_VAR}[cpu] = true;
}

inline bool ${OPER_FN}(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    ${INIT_FN}(cpu, L1D, L2C, LLC);

${BODY}

#if DBG_${NUM}
    if ((++${DBG_CTR}[cpu] & 0xFFFF) == 0) {
        printf("${INDENT}[${NUM}-${LVL_TAG}] -> %s\n", byp ? "BYP" : "---");
    }
#endif

    return byp;
}
HEREDOC

echo "  → $OUTFILE"
