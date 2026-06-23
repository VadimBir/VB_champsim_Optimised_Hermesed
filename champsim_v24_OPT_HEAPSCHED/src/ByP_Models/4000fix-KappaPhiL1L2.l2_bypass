#include "cache.h"
#include "lpm_tracker.h"

#ifndef L1D_type
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#endif

// MODEL 4000fix — KappaPhiL1L2 at L2
// L2 Bypass [4000fix: F6 sign-deriv: kappa_rising AND phi_falling + LATTRACK]

// #define DBG_4000fix 1

inline bool l2_bypass_init_4000fix[NUM_CPUS] = {};
inline uint64_t l2_dbg_counter_4000fix[NUM_CPUS] = {};

#define SHALL_L2C_BYPASS_DEFINED
inline void l2c_bypass_initialize(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    if (l2_bypass_init_4000fix[cpu]) return;
    cout << "[model: 4000fix-KappaPhiL1L2.l2_bypass] L2 Bypass [4000fix-KappaPhiL1L2]: L2 Bypass [4000fix: F6 sign-deriv: kappa_rising AND phi_falling + LATTRACK]" << endl;
    l2_bypass_init_4000fix[cpu] = true;
}

inline bool l2c_bypass_operate(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC) {
    l2c_bypass_initialize(cpu, L1D, L2C, LLC);

double k_s = get_kappa_short(cpu, LPM_L2C);
 double k_l = get_kappa_long(cpu, LPM_L2C);
 double p_s = get_phi_short(cpu, LPM_L2C);
 double p_l = get_phi_long(cpu, LPM_L2C);
 // bool kp = ((k_s > k_l) && (p_s < p_l)) || L2C->MSHR.occupancy == L2C_MSHR_SIZE;
 bool kp = ((k_s > k_l) && (p_s < p_l));
 if (!kp) return false;
 uint64_t addr = L2C->RQ.entry[L2C->RQ.head].address;
 uint64_t blk = addr >> LOG2_BLOCK_SIZE;
 g_l2_byplat[cpu].on_issue(blk, L2C->MSHR.occupancy);
 return true;
}
#define SHALL_L2C_BYPASS_FILL_DEFINED
inline void l2c_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {
 uint64_t blk = pkt.address >> LOG2_BLOCK_SIZE;
 g_l2_byplat[cpu].on_fill(blk);
}
