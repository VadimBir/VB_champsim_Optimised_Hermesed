#include "cache.h"
#include "lpm_tracker.h"

#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif

// MODEL 4000fix — KappaPhiL1L2 at L1
// L1 Bypass [4000fix: F6 sign-deriv: kappa_rising AND phi_falling + LATTRACK]

// #define DBG_4000fix 1

inline bool l1_bypass_init_4000fix[NUM_CPUS] = {};
inline uint64_t l1_dbg_counter_4000fix[NUM_CPUS] = {};

#define SHALL_L1D_BYPASS_DEFINED
inline void l1d_bypass_initialize(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (l1_bypass_init_4000fix[cpu]) return;
    cout << "[model: 4000fix-KappaPhiL1L2.l1_bypass] L1 Bypass [4000fix-KappaPhiL1L2]: L1 Bypass [4000fix: F6 sign-deriv: kappa_rising AND phi_falling + LATTRACK]" << endl;
    l1_bypass_init_4000fix[cpu] = true;
}

inline bool l1d_bypass_operate(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    l1d_bypass_initialize(cpu, L1D, L2C, LLC);

double k_s = get_kappa_short(cpu, LPM_L1D);
 double k_l = get_kappa_long(cpu, LPM_L1D);
 double p_s = get_phi_short(cpu, LPM_L1D);
 double p_l = get_phi_long(cpu, LPM_L1D);
 // bool kp = ((k_s > k_l) && (p_s < p_l)) || L1D->MSHR.occupancy == L1D_MSHR_SIZE;
 bool kp = ((k_s > k_l) && (p_s < p_l));
 if (!kp) return false;
 uint64_t addr = L1D->RQ.entry[L1D->RQ.head].address;
 uint64_t blk = addr >> LOG2_BLOCK_SIZE;
 g_l1_byplat[cpu].on_issue(blk, L1D->MSHR.occupancy);
 return true;
}
#define SHALL_L1D_BYPASS_FILL_DEFINED
inline void l1d_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {
 uint64_t blk = pkt.address >> LOG2_BLOCK_SIZE;
 g_l1_byplat[cpu].on_fill(blk);
}
