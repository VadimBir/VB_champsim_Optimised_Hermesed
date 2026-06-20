#ifndef BYPLAT_TRACKER_H
#define BYPLAT_TRACKER_H

/*-----------------------------------------------------------------
 * ByPLatTracker — per-level inflight-BYP + MLP tracker (hashmap-based)
 *   - LOADs only. Per-cpu, per-level. No cross-level dedup.
 *   - on_issue(blk, mshr_occ): single call from model when bypass issued.
 *       inserts blk into hashmap, increments inflight_now,
 *       samples mshr/inflight/MLP (acc + peak), increments samples.
 *   - on_fill(blk): lookup blk; if present erase + decrement inflight_now.
 *       NO BYP-flag check — catches merged-prefetch / BYP-reset leak.
 *   - Stats: MSHR avg/peak, BYP inflight avg/peak, MLP avg/peak.
 *
 * Header is self-contained (no cache.h dep) so cache.h can include it
 * and call g_*_byplat[cpu].on_fill(blk) directly from inline fill paths.
 *-----------------------------------------------------------------*/

#include "defs.h"
#include "unordered_dense.h"
#include <cstdint>
#include <cstdio>

struct ByPLatTracker {
  ankerl::unordered_dense::map<uint64_t, uint8_t> inflight_blk;
  uint32_t inflight_now;
  uint32_t occupancy; /* mirror of inflight_now for legacy callers (LPM_CROSS_LEVEL_MLP) */

  uint64_t mshr_acc, inflight_acc, mlp_acc;
  uint64_t samples;

  uint64_t mshr_peak, inflight_peak, mlp_peak;

  uint16_t upper_lat_sat16; /* legacy; preserved for any external readers (0xFFFF = unset) */

  void init() {
    inflight_blk.clear();
    inflight_now = 0;
    occupancy = 0;
    mshr_acc = inflight_acc = mlp_acc = 0;
    samples = 0;
    mshr_peak = inflight_peak = mlp_peak = 0;
    upper_lat_sat16 = 0xFFFF;
  }

  inline void on_issue(uint64_t blk, uint32_t mshr_occ) {
    auto ins = inflight_blk.emplace(blk, (uint8_t)1);
    if (ins.second) {
      ++inflight_now;
      occupancy = inflight_now;
    }
    uint32_t infl  = inflight_now;
    uint64_t mlp_n = (uint64_t)mshr_occ + (uint64_t)infl;

    mshr_acc     += mshr_occ;
    inflight_acc += infl;
    mlp_acc      += mlp_n;
    ++samples;

    if (mshr_occ  > mshr_peak)     mshr_peak     = mshr_occ;
    if (infl      > inflight_peak) inflight_peak = infl;
    if (mlp_n     > mlp_peak)      mlp_peak      = mlp_n;
  }

  inline void on_fill(uint64_t blk) {
    auto it = inflight_blk.find(blk);
    if (it != inflight_blk.end()) {
      inflight_blk.erase(it);
      inflight_now -= (inflight_now > 0);
      occupancy = inflight_now;
    }
  }

  inline void on_fill(uint64_t blk, uint64_t /*cur_cyc*/) { on_fill(blk); }

  inline double mshr_avg()     const { return samples ? (double)mshr_acc     / samples : 0.0; }
  inline double inflight_avg() const { return samples ? (double)inflight_acc / samples : 0.0; }
  inline double mlp_avg()      const { return samples ? (double)mlp_acc      / samples : 0.0; }
};

extern ByPLatTracker g_l1_byplat[NUM_CPUS];
extern ByPLatTracker g_l2_byplat[NUM_CPUS];
extern ByPLatTracker g_llc_byplat[NUM_CPUS];

inline void byplat_init_all() {
  for (int c = 0; c < NUM_CPUS; c++) {
    g_l1_byplat[c].init(); g_l2_byplat[c].init(); g_llc_byplat[c].init();
  }
}
inline void byplat_print(int cpu) {
  printf("\n=== ByPLatTracker CPU %d ===\n", cpu);
  printf("%-4s %12s %10s %12s %10s %10s %10s %12s\n",
         "Lvl","mshr_avg","mshr_pk","byp_infl_avg","byp_pk","mlp_avg","mlp_pk","samples");
  ByPLatTracker* lvls[3] = { &g_l1_byplat[cpu], &g_l2_byplat[cpu], &g_llc_byplat[cpu] };
  const char* lnm[3] = { "L1D","L2C","LLC" };
  for (int i=0;i<3;i++) {
    printf("%-4s %12.3f %10llu %12.3f %10llu %10.3f %10llu %12llu\n",
           lnm[i],
           lvls[i]->mshr_avg(),     (unsigned long long)lvls[i]->mshr_peak,
           lvls[i]->inflight_avg(), (unsigned long long)lvls[i]->inflight_peak,
           lvls[i]->mlp_avg(),      (unsigned long long)lvls[i]->mlp_peak,
           (unsigned long long)lvls[i]->samples);
  }
}

#endif /* BYPLAT_TRACKER_H */
