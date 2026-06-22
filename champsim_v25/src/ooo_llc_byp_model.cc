#include "cache.h"
#include "lpm_tracker.h"

#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif

// MODEL 4000fix — KappaPhiL1L2 at LLC
// LLC Bypass [4000fix: unconditional + LATTRACK]

// #define DBG_4000fix 1

inline bool llc_bypass_init_4000fix[NUM_CPUS] = {};
inline uint64_t llc_dbg_counter_4000fix[NUM_CPUS] = {};

#define SHALL_LLC_BYPASS_DEFINED
inline void llc_bypass_initialize(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (llc_bypass_init_4000fix[cpu]) return;
    cout << "[model: 4000fix-KappaPhiL1L2.llc_bypass] LLC Bypass [4000fix-KappaPhiL1L2]: LLC Bypass [4000fix: unconditional + LATTRACK]" << endl;
    llc_bypass_init_4000fix[cpu] = true;
}

inline bool llc_bypass_operate(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    llc_bypass_initialize(cpu, L1D, L2C, LLC);

uint64_t addr = LLC->RQ.entry[LLC->RQ.head].address;
 uint64_t blk = addr >> LOG2_BLOCK_SIZE;
 g_llc_byplat[cpu].on_issue(blk, LLC->MSHR.occupancy);
 return true;
}
#define SHALL_LLC_BYPASS_FILL_DEFINED
inline void llc_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {
 uint64_t blk = pkt.address >> LOG2_BLOCK_SIZE;
 g_llc_byplat[cpu].on_fill(blk);
}
