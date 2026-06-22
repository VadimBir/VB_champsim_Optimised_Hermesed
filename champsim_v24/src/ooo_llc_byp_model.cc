#include "cache.h"
#include "lpm_tracker.h"

// USEFUL VARS:  usage example
//  * Cached metrics available without sim_access:
//  *   lpm[cpu][IS_L1D].gm.camat_activeMemCyDivAccesses   ← ω(L1D)/α(L1D)
//  *   lpm[cpu][IS_L1D].gm.apc_accessesDivActiveMemCy      ← α(L1D)/ω(L1D)
//  *   lpm[cpu][IS_L1D].gm.lpmr_activeMemCyDivIdealCy     ← ω(L1D)/(IC×CPIexe)
//  *   lpm[cpu][IS_L2C].g.µ_missCyFracOfActiveCy()         ← miss-cycle fraction
//  *   lpm[cpu][IS_L2C].g.κ_pureMissFracOfMissCy()      ← pure-miss fraction
//  *
//  * Also raw counters:
//  *   lpm[cpu][IS_L1D].h, .m, .x, .e
//  *   lpm[cpu][IS_L1D].g.ω_activeMemCy()

// L1D->MSHR.occupancy      L1D->MSHR.SIZE
// L1D->RQ.occupancy        L1D->RQ.SIZE
// L1D->PQ.occupancy        L1D->PQ.SIZE
// L2C->MSHR.occupancy      L2C->MSHR.SIZE
// L2C->RQ.occupancy        L2C->RQ.SIZE
// L2C->PQ.occupancy        L2C->PQ.SIZE

// CACHE TYPEs
#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif

inline bool llc_bypass_init[NUM_CPUS] = {};

inline void llc_bypass_initialize(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (llc_bypass_init[cpu]) return;
        cout << "[model: no.llc_bypass] Bypass: \nBASE" << endl;
    llc_bypass_init[cpu] = true;
}

#define SHALL_LLC_BYPASS_DEFINED
inline bool llc_bypass_operate(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    llc_bypass_initialize(cpu, L1D, L2C, LLC);
    return false;
}
