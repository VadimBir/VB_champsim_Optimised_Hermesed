#include "cache.h"
#include "lpm_tracker.h"

#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif



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

// CACHE TYPEs




inline bool l1_bypass_init[NUM_CPUS] = {};

#define SHALL_L1D_BYPASS_DEFINED
inline void l1d_bypass_initialize(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (l1_bypass_init[cpu]) return;
        cout << "[model: no.l1_bypass] Bypass on: \nNO BYBPASS LOGIC" << endl;
    l1_bypass_init[cpu] = true;
}

inline bool l1d_bypass_operate(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    l1d_bypass_initialize(cpu, L1D, L2C, LLC);
    return false;
}
