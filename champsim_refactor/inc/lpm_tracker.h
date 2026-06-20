/*
 * lpm_tracker.h — Layered Performance Matching (LPM) cycle tracker
 *
 * Based on: Liu, Espina, Sun. "A Study on Modeling and Optimization
 * of Memory Systems." JCST 36(1), 2021. DOI:10.1007/s11390-021-0771-8
 *
 * 3 tiers: Global (g), Windowed (w), Double-Overlap-Windowed (w2d)
 * Each tier tracks: h/m/x/e cycle counters, α access count, load access/miss
 * Each tier derives: C-AMAT, APC, LPMR, MST, CPA metrics
 */

#ifndef LPM_TRACKER_H
#define LPM_TRACKER_H

#include "memory_class.h"
#include "cache.h"
#include "defs.h"
#include <cstdint>
#include <cstdio>

#define LPM_STRICT_MISS

#define LPM_NUM_TYPES 8
#define LPM_L1D       4
#define LPM_L2C       5
#define LPM_LLC       6
#define LPM_DRAM      7

#define LPM_CLASS_E 0
#define LPM_CLASS_H 1
#define LPM_CLASS_M 2
#define LPM_CLASS_X 3

/* ── Cycle-class indices — match cls = (ha<<1)|ma encoding ── */
#define CY_E 0   /* idle:    ha=0 ma=0 */
#define CY_M 1   /* miss:    ha=0 ma=1 */
#define CY_H 2   /* hit:     ha=1 ma=0 */
#define CY_X 3   /* overlap: ha=1 ma=1 */

/* ── Bucket indices ── */
#define BKT_G    0
#define BKT_W    1
#define BKT_W2DA 2
#define BKT_W2DB 3
#ifdef LPM_LONG_SHORT_WIN
#define BKT_WS   4
#define BKT_WL   5
#define BKT_COUNT 6
#else
#define BKT_COUNT 4
#endif

/* ── Metric indices ── */
#define MET_G    0
#define MET_W    1
#define MET_W2D  2
#ifdef LPM_LONG_SHORT_WIN
#define MET_WS   3
#define MET_WL   4
#define MET_COUNT 5
#else
#define MET_COUNT 3
#endif

/* ── Reusable counter bucket (one instance per tier) ── */
struct LPM_BucketCy {
    uint64_t cy[4];
    uint64_t α_accessCount;
    uint64_t load_accessCount, load_missCount;

    void reset() {
        cy[0] = cy[1] = cy[2] = cy[3] = 0;
        α_accessCount = load_accessCount = load_missCount = 0;
    }
    void halve() {
        cy[0] >>= 1; cy[1] >>= 1; cy[2] >>= 1; cy[3] >>= 1;
        α_accessCount >>= 1; load_accessCount >>= 1; load_missCount >>= 1;
    }
    inline void tick_class(uint8_t cls) {
        cy[cls]++;
    }
    inline void tick_α(uint64_t dα)                        { α_accessCount += dα; }
    inline void tick_load(uint64_t dAcc, uint64_t dMiss)   { load_accessCount += dAcc; load_missCount += dMiss; }

    inline uint64_t ω_activeMemCy()  const { return cy[CY_H] + cy[CY_M] + cy[CY_X]; }
    inline uint64_t totalCy()        const { return cy[CY_H] + cy[CY_M] + cy[CY_X] + cy[CY_E]; }
    inline uint64_t idealCy()        const { return cy[CY_E] + cy[CY_H] + cy[CY_X]; }

    inline double µ_missCyFracOfActiveCy()     const { uint64_t w = ω_activeMemCy(); return w ? (double)(cy[CY_M] + cy[CY_X]) / w : 0.0; }
    inline double κ_pureMissFracOfMissCy()     const { uint64_t d = cy[CY_M] + cy[CY_X]; return d ? (double)cy[CY_M] / d : 0.0; }
    inline double φ_hitCyFracOfActiveCy()      const { uint64_t w = ω_activeMemCy(); return w ? (double)(cy[CY_H] + cy[CY_X]) / w : 0.0; }
    inline double load_missRate()              const { return load_accessCount ? (double)load_missCount / load_accessCount : 0.0; }
};

/* ── Derived metrics (one instance per tier) ── */
struct LPM_Metrics {
    double camat_activeMemCyDivAccesses;     /* ω/α   [eq 30] */
    double apc_accessesDivActiveMemCy;        /* α/ω   [eq 32] */
    double lpmr_activeMemCyDivIdealCy;       /* ω/ideal [eq 52] */
    double mst_pureMissCyDivAccesses;         /* m/α   [eq 18] */
    double cpa_totalCyDivAccesses;            /* n/α   (incl idle) */

    void reset() {
        camat_activeMemCyDivAccesses = apc_accessesDivActiveMemCy = 0.0;
        lpmr_activeMemCyDivIdealCy = mst_pureMissCyDivAccesses = 0.0;
        cpa_totalCyDivAccesses = 9999.0;
    }
    inline void compute(const LPM_BucketCy& b, uint64_t ideal) {
        uint64_t ω = b.ω_activeMemCy();
        uint64_t α = b.α_accessCount;
        double inv_α = α ? 1.0 / (double)α : 0.0;
        double inv_ω = ω ? 1.0 / (double)ω : 0.0;
        double inv_ideal = ideal ? 1.0 / (double)ideal : 0.0;
        camat_activeMemCyDivAccesses  = (double)ω * inv_α;
        apc_accessesDivActiveMemCy     = (double)α * inv_ω;
        lpmr_activeMemCyDivIdealCy    = (double)ω * inv_ideal;
        mst_pureMissCyDivAccesses      = (double)b.cy[CY_M] * inv_α;
        cpa_totalCyDivAccesses         = α ? (double)b.totalCy() * inv_α : 9999.0;
    }
    inline void compute_pre(const LPM_BucketCy& b, double inv_α, double inv_ω, double inv_ideal) {
        camat_activeMemCyDivAccesses  = (double)b.ω_activeMemCy() * inv_α;
        apc_accessesDivActiveMemCy     = (double)b.α_accessCount * inv_ω;
        lpmr_activeMemCyDivIdealCy    = (double)b.ω_activeMemCy() * inv_ideal;
        mst_pureMissCyDivAccesses      = (double)b.cy[CY_M] * inv_α;
        cpa_totalCyDivAccesses         = inv_α > 0.0 ? (double)b.totalCy() * inv_α : 9999.0;
    }
    inline void average(const LPM_Metrics& a, const LPM_Metrics& b) {
        camat_activeMemCyDivAccesses  = (a.camat_activeMemCyDivAccesses  + b.camat_activeMemCyDivAccesses)  * 0.5;
        apc_accessesDivActiveMemCy     = (a.apc_accessesDivActiveMemCy     + b.apc_accessesDivActiveMemCy)     * 0.5;
        lpmr_activeMemCyDivIdealCy    = (a.lpmr_activeMemCyDivIdealCy    + b.lpmr_activeMemCyDivIdealCy)    * 0.5;
        mst_pureMissCyDivAccesses      = (a.mst_pureMissCyDivAccesses      + b.mst_pureMissCyDivAccesses)      * 0.5;
        cpa_totalCyDivAccesses         = (a.cpa_totalCyDivAccesses         + b.cpa_totalCyDivAccesses)         * 0.5;
    }
};

/* ── 16B-packed prev pair: forces compiler to emit a single 16B store
 *    (movdqa on x86_64 / stp on aarch64) and eliminates the SF stall
 *    where a 16B load tried to forward from two adjacent 8B stores. ── */
struct alignas(16) LPM_PrevAlphaPair {
    uint64_t α;
    uint64_t load_α;
};

/* ── Main tracker ── */
struct LPM_Tracker {
    LPM_BucketCy bkt[BKT_COUNT];
    LPM_Metrics  met[MET_COUNT];

    LPM_PrevAlphaPair prev_pair;   /* {α, load_α} — 16B aligned, single-store friendly */
    uint64_t prev_load_miss;

    /* Bypass correction (global only) */
    uint64_t m_byp_l1d_pureMissCy;
    // DEAD-2026-05-25: m_byp_llc_pureMissCy — zero reads
    // uint64_t m_byp_llc_pureMissCy;

    /* ROI snapshot */
    LPM_BucketCy roi;
    LPM_Metrics  roi_m;
    uint64_t roi_m_byp_l1d_pureMissCy /* DEAD-2026-05-25: , roi_m_byp_llc_pureMissCy — zero reads */;

    /* Window control */
#ifdef LPM_DYNAMIC_WIN
    static constexpr uint32_t W_SIZE_MIN = 64;
    static constexpr uint32_t W_SIZE_MAX = 4096;
    static constexpr uint32_t W_SIZE_DEFAULT = 512;
    static constexpr uint32_t W_SIZE_IN_CAMATS = 12;
    uint32_t w_size;
    uint32_t w_tick;
    uint32_t w2d_a_half;
    uint32_t w2d_b_half;

    inline void recompute_halve_points() {
        w2d_a_half = w_size * 3 / 4;
        w2d_b_half = w_size / 4;
    }
#else
    static constexpr uint32_t W_SIZE = 512;
    uint32_t w_tick;
    static constexpr uint32_t W2D_A_HALF = W_SIZE * 3 / 4;
    static constexpr uint32_t W2D_B_HALF = W_SIZE / 4;
#endif

#ifdef LPM_LONG_SHORT_WIN
    static constexpr uint32_t SHORT_SIZE = 512;
    static constexpr uint32_t LONG_MULT  = 32;
    static constexpr uint32_t LONG_SIZE  = SHORT_SIZE * LONG_MULT;
#ifdef LPM_DYNAMIC_WIN
    static constexpr uint32_t W_SHORT_IN_CAMATS = 256;
    uint32_t short_size;
    uint32_t long_size;
#endif
    uint32_t short_tick;
    uint32_t long_tick;
#endif

    uint8_t last_class;

    void init() {
        for (int i = 0; i < BKT_COUNT; i++) bkt[i].reset();
        for (int i = 0; i < MET_COUNT; i++) met[i].reset();
        prev_pair = {0, 0}; prev_load_miss = 0;
        // DEAD-2026-05-25: m_byp_llc_pureMissCy removed from chain
        m_byp_l1d_pureMissCy = 0;
        roi.reset(); roi_m.reset();
        // DEAD-2026-05-25: roi_m_byp_llc_pureMissCy removed from chain
        roi_m_byp_l1d_pureMissCy = 0;
#ifdef LPM_DYNAMIC_WIN
        w_size = W_SIZE_DEFAULT;
        w_tick = 0;
        recompute_halve_points();
#else
        w_tick = 0;
#endif
#ifdef LPM_LONG_SHORT_WIN
        short_tick = long_tick = 0;
#ifdef LPM_DYNAMIC_WIN
        short_size = SHORT_SIZE;
        long_size  = LONG_SIZE;
#endif
#endif
        last_class = LPM_CLASS_E;
    }

    void reset_sim() {
        bkt[BKT_G].reset(); met[MET_G].reset();
        prev_pair = {0, 0}; prev_load_miss = 0;
        // DEAD-2026-05-25: m_byp_llc_pureMissCy removed from chain
        m_byp_l1d_pureMissCy = 0;
    }

    void record_roi() {
        roi = bkt[BKT_G]; roi_m = met[MET_G];
        roi_m_byp_l1d_pureMissCy = m_byp_l1d_pureMissCy;
        // DEAD-2026-05-25: roi_m_byp_llc_pureMissCy write removed
        // roi_m_byp_llc_pureMissCy = m_byp_llc_pureMissCy;
    }

    /* ── Core tick — updates all tiers' cycle counters ── */
    inline void tick(bool ha, bool ma) {
        uint8_t cls = ((uint8_t)ha << 1) | (uint8_t)ma;
        static const uint8_t map[4] = { LPM_CLASS_E, LPM_CLASS_M, LPM_CLASS_H, LPM_CLASS_X };
        last_class = map[cls];

        for (int i = 0; i < BKT_COUNT; i++)
            bkt[i].tick_class(cls);

        /* windowed halve + tick advance */
#ifdef LPM_DYNAMIC_WIN
        if (++w_tick >= w_size) {
            uint64_t w_α = bkt[BKT_W].α_accessCount;
            uint64_t w_ω = bkt[BKT_W].ω_activeMemCy();
            if (w_α > 0 && w_ω > 0) {
                double camat_est = (double)w_ω / (double)w_α;
                uint32_t new_size = (uint32_t)(camat_est * W_SIZE_IN_CAMATS + 0.5);
                if (new_size < W_SIZE_MIN) new_size = W_SIZE_MIN;
                if (new_size > W_SIZE_MAX) new_size = W_SIZE_MAX;
                w_size = new_size;
                recompute_halve_points();
            }
            bkt[BKT_W].halve();
            w_tick = 0;
        }
        if (w_tick == w2d_a_half) bkt[BKT_W2DA].halve();
        if (w_tick == w2d_b_half) bkt[BKT_W2DB].halve();
#else
        if (++w_tick >= W_SIZE) { bkt[BKT_W].halve(); w_tick = 0; }
        if (w_tick == W2D_A_HALF) bkt[BKT_W2DA].halve();
        if (w_tick == W2D_B_HALF) bkt[BKT_W2DB].halve();
#endif
#ifdef LPM_LONG_SHORT_WIN
#ifdef LPM_DYNAMIC_WIN
        if (++short_tick >= short_size) {
            uint64_t s_α = bkt[BKT_WS].α_accessCount;
            uint64_t s_ω = bkt[BKT_WS].ω_activeMemCy();
            if (s_α > 0 && s_ω > 0) {
                double camat_s = (double)s_ω / (double)s_α;
                short_size = (uint32_t)(camat_s * W_SHORT_IN_CAMATS + 0.5);
            }
            bkt[BKT_WS].halve(); short_tick = 0;
        }
        if (++long_tick >= long_size) {
            uint64_t l_α = bkt[BKT_WL].α_accessCount;
            uint64_t l_ω = bkt[BKT_WL].ω_activeMemCy();
            if (l_α > 0 && l_ω > 0) {
                double camat_l = (double)l_ω / (double)l_α;
                long_size = (uint32_t)(camat_l * W_SHORT_IN_CAMATS * LONG_MULT + 0.5);
            }
            bkt[BKT_WL].halve(); long_tick = 0;
        }
#else
        if (++short_tick >= SHORT_SIZE) { bkt[BKT_WS].halve(); short_tick = 0; }
        if (++long_tick  >= LONG_SIZE)  { bkt[BKT_WL].halve(); long_tick  = 0; }
#endif
#endif
    }

    /* ── Batch idle advance: fast-forward N cycles where ha=0,ma=0 (cls=CY_E).
     *    Processes halve boundaries chronologically so bucket state matches
     *    N individual tick(false,false) calls. ── */
    inline void advance_idle(uint32_t N) {
        if (N == 0) return;
        last_class = LPM_CLASS_E;

        /* --- Helper lambda: advance a single bucket's halve window --- */
        /* For a window of `size` with tick at `*tick_p`, advance by `total` cycles,
         * calling halve on `bkt_idx` at each wrap boundary.
         * All cycles are CY_E class. Returns nothing; modifies tick in place. */
        auto advance_window = [&](uint32_t& tick_ref, uint32_t size, int bkt_idx, uint32_t total) {
            uint32_t remaining = total;
            /* Distance to next wrap from current tick */
            uint32_t to_wrap = size - tick_ref;
            if (remaining < to_wrap) {
                /* No wrap crossing — fast path */
                bkt[bkt_idx].cy[CY_E] += remaining;
                tick_ref += remaining;
                return;
            }
            /* First partial segment to the wrap boundary */
            bkt[bkt_idx].cy[CY_E] += to_wrap;
            bkt[bkt_idx].halve();
            remaining -= to_wrap;
            /* Full wraps */
            uint32_t full_wraps = remaining / size;
            uint32_t leftover  = remaining % size;
            for (uint32_t w = 0; w < full_wraps; w++) {
                bkt[bkt_idx].cy[CY_E] += size;
                bkt[bkt_idx].halve();
            }
            bkt[bkt_idx].cy[CY_E] += leftover;
            tick_ref = leftover;
        };

        /* --- BKT_G: global, never halved --- */
        bkt[BKT_G].cy[CY_E] += N;

        /* --- BKT_W + sub-halves W2DA, W2DB --- */
        /* W2DA and W2DB halve at specific tick positions within the W window.
         * We must process the W window chronologically, firing sub-halves
         * at W2D_A_HALF and W2D_B_HALF as w_tick crosses them, and
         * firing BKT_W halve when w_tick wraps. */
        {
            uint32_t remaining = N;
            while (remaining > 0) {
#ifdef LPM_DYNAMIC_WIN
                uint32_t cur_size = w_size;
                uint32_t cur_w2da = w2d_a_half;
                uint32_t cur_w2db = w2d_b_half;
#else
                const uint32_t cur_size = W_SIZE;
                const uint32_t cur_w2da = W2D_A_HALF;
                const uint32_t cur_w2db = W2D_B_HALF;
#endif
                /* Find next event: w_tick reaches w2da, w2db, or cur_size */
                /* Collect upcoming boundaries in sorted order */
                uint32_t next_event = cur_size; /* wrap always an event */
                if (w_tick < cur_w2da && cur_w2da < next_event) next_event = cur_w2da;
                if (w_tick < cur_w2db && cur_w2db < next_event) next_event = cur_w2db;

                uint32_t step = next_event - w_tick;
                if (step > remaining) step = remaining;

                /* Accumulate E-class cycles for W, W2DA, W2DB */
                bkt[BKT_W].cy[CY_E]    += step;
                bkt[BKT_W2DA].cy[CY_E] += step;
                bkt[BKT_W2DB].cy[CY_E] += step;

                w_tick += step;
                remaining -= step;

                /* Fire halves at boundaries */
                if (w_tick == cur_w2da) bkt[BKT_W2DA].halve();
                if (w_tick == cur_w2db) bkt[BKT_W2DB].halve();
                if (w_tick >= cur_size) {
                    bkt[BKT_W].halve();
                    w_tick = 0;
#ifdef LPM_DYNAMIC_WIN
                    /* Recompute dynamic window size at wrap */
                    uint64_t w_alpha = bkt[BKT_W].alpha_accessCount;
                    uint64_t w_omega = bkt[BKT_W].omega_activeMemCy();
                    if (w_alpha > 0 && w_omega > 0) {
                        double camat_est = (double)w_omega / (double)w_alpha;
                        uint32_t new_size = (uint32_t)(camat_est * W_SIZE_IN_CAMATS + 0.5);
                        if (new_size < W_SIZE_MIN) new_size = W_SIZE_MIN;
                        if (new_size > W_SIZE_MAX) new_size = W_SIZE_MAX;
                        w_size = new_size;
                        recompute_halve_points();
                    }
#endif
                }
            }
        }

#ifdef LPM_LONG_SHORT_WIN
        /* --- BKT_WS (short window) --- */
#ifdef LPM_DYNAMIC_WIN
        advance_window(short_tick, short_size, BKT_WS, N);
#else
        advance_window(short_tick, SHORT_SIZE, BKT_WS, N);
#endif
        /* --- BKT_WL (long window) --- */
#ifdef LPM_DYNAMIC_WIN
        advance_window(long_tick, long_size, BKT_WL, N);
#else
        advance_window(long_tick, LONG_SIZE, BKT_WL, N);
#endif
#endif
    }

    inline void tick_byp(bool ha, bool ma, bool has_byp_miss, bool upper_pure_miss) {
        tick(ha, ma);
        m_byp_l1d_pureMissCy += (last_class == LPM_CLASS_M) & has_byp_miss & (!upper_pure_miss);
    }

    inline void tick_llc_byp(bool ha, bool ma, bool has_byp_miss) {
        tick(ha, ma);
        // DEAD-2026-05-25: m_byp_llc_pureMissCy write — field never read
        // m_byp_llc_pureMissCy += (last_class == LPM_CLASS_M) & has_byp_miss;
        (void)has_byp_miss;
    }

    /* ── HOT: per-cycle counter tick only (no divs) ──
     * F1: prev_pair {α,load_α} = single 16B store → kills SF stall.
     * F2: zero-delta early return → skips 6-bucket RMW chain on most cycles. */
    inline void tick_alphas(uint64_t accum_α, uint64_t accum_load_α, uint64_t accum_load_miss) {
        uint64_t dα  = accum_α - prev_pair.α;
        uint64_t dL  = accum_load_α - prev_pair.load_α;
        uint64_t dLM = accum_load_miss - prev_load_miss;
        if ((dα | dL | dLM) == 0) return;
        LPM_PrevAlphaPair cur = { accum_α, accum_load_α };
        prev_pair = cur;                  /* compiler emits ONE 16B store */
        prev_load_miss = accum_load_miss;

        for (int i = 0; i < BKT_COUNT; i++) {
            bkt[i].tick_α(dα);
            bkt[i].tick_load(dL, dLM);
        }
    }

    /* ── COLD: on-demand metric compute (divs) ── */
    inline void demandCompute(const uint64_t ideals[BKT_COUNT]) {
        double inv_α[BKT_COUNT], inv_ω[BKT_COUNT], inv_id[BKT_COUNT];
        for (int i = 0; i < BKT_COUNT; i++) {
            uint64_t α = bkt[i].α_accessCount;
            uint64_t ω = bkt[i].ω_activeMemCy();
            inv_α[i] = α ? 1.0 / (double)α : 0.0;
            inv_ω[i] = ω ? 1.0 / (double)ω : 0.0;
            inv_id[i] = ideals[i] ? 1.0 / (double)ideals[i] : 0.0;
        }

        met[MET_G].compute_pre(bkt[BKT_G], inv_α[BKT_G], inv_ω[BKT_G], inv_id[BKT_G]);
        met[MET_W].compute_pre(bkt[BKT_W], inv_α[BKT_W], inv_ω[BKT_W], inv_id[BKT_W]);

        LPM_Metrics ma, mb;
        ma.compute_pre(bkt[BKT_W2DA], inv_α[BKT_W2DA], inv_ω[BKT_W2DA], inv_id[BKT_W2DA]);
        mb.compute_pre(bkt[BKT_W2DB], inv_α[BKT_W2DB], inv_ω[BKT_W2DB], inv_id[BKT_W2DB]);
        met[MET_W2D].average(ma, mb);

#ifdef LPM_LONG_SHORT_WIN
        met[MET_WS].compute_pre(bkt[BKT_WS], inv_α[BKT_WS], inv_ω[BKT_WS], inv_id[BKT_WS]);
        met[MET_WL].compute_pre(bkt[BKT_WL], inv_α[BKT_WL], inv_ω[BKT_WL], inv_id[BKT_WL]);
#endif
    }

    /* ── DEPRECATED wrapper kept for legacy callers ── */
    inline void update_all_metrics(uint64_t accum_α, const uint64_t ideals[BKT_COUNT],
                                   uint64_t accum_load_α, uint64_t accum_load_miss) {
        tick_alphas(accum_α, accum_load_α, accum_load_miss);
        demandCompute(ideals);
    }
};

/* Global storage: lpm[cpu][cache_type] */
extern LPM_Tracker lpm[NUM_CPUS][LPM_NUM_TYPES];

inline void lpm_init() {
    for (int c = 0; c < NUM_CPUS; c++)
        for (int t = 0; t < LPM_NUM_TYPES; t++)
            lpm[c][t].init();
}

inline void lpm_reset_sim(int cpu) {
    for (int t = 0; t < LPM_NUM_TYPES; t++)
        lpm[cpu][t].reset_sim();
}

inline void lpm_record_roi(int cpu) {
    for (int t = 0; t < LPM_NUM_TYPES; t++)
        lpm[cpu][t].record_roi();
}

/*-----------------------------------------------------------------
 * lpm_operate() — call from CACHE::operate() BEFORE handle_*
 *-----------------------------------------------------------------*/
/* ── D2: overload taking the already-resolved tracker reference T.
 *    Eliminates the `lpm[cpu][cache_type]` recompute (imul 0x2c0) at each use.
 *    T MUST be bound to exactly lpm[cpu][cache_type] by the caller; cpu/cache_type
 *    are still required because the body uses cache_type for branch selection and
 *    cpu to reach the independent cross-tracker lpm[cpu][LPM_L1D]. Logic is
 *    byte-identical to the index-based version below. ── */
inline void lpm_operate(LPM_Tracker& T, int cpu, uint8_t cache_type,
                        bool hit_active, bool miss_active,
                        uint64_t α, bool has_byp_miss,
                        uint64_t load_α, uint64_t load_miss) {
    (void)cpu;
#ifdef BYPASS_LLC_LOGIC
    if (cache_type == IS_LLC) {
        T.tick_llc_byp(hit_active, miss_active, has_byp_miss);
    } else
#endif
#ifdef BYPASS_L1_LOGIC
    if (cache_type == IS_L2C) {
        bool l1d_pm = (lpm[cpu][LPM_L1D].last_class == LPM_CLASS_M);
        T.tick_byp(hit_active, miss_active, has_byp_miss, l1d_pm);
    } else
#endif
    {
        T.tick(hit_active, miss_active);
    }

    /* HOT path: only tick α/load deltas. Divs deferred to demandCompute. */
    T.tick_alphas(α, load_α, load_miss);
}

inline void lpm_operate(int cpu, uint8_t cache_type,
                        bool hit_active, bool miss_active,
                        uint64_t α, bool has_byp_miss,
                        uint64_t load_α, uint64_t load_miss) {
    lpm_operate(lpm[cpu][cache_type], cpu, cache_type,
                hit_active, miss_active, α, has_byp_miss, load_α, load_miss);
}

/*-----------------------------------------------------------------
 * lpm_demand_compute() — invoke before reading met[] fields
 * (bypass predicate entry, heartbeat, db_writer end-of-sim)
 *-----------------------------------------------------------------*/
inline void lpm_demand_compute(int cpu, uint8_t cache_type) {
    const LPM_Tracker& l1d = lpm[cpu][LPM_L1D];
    uint64_t ideals[BKT_COUNT];
    for (int i = 0; i < BKT_COUNT; i++)
        ideals[i] = l1d.bkt[i].idealCy();
    lpm[cpu][cache_type].demandCompute(ideals);
}

inline void lpm_demand_compute_all(int cpu) {
    const LPM_Tracker& l1d = lpm[cpu][LPM_L1D];
    uint64_t ideals[BKT_COUNT];
    for (int i = 0; i < BKT_COUNT; i++)
        ideals[i] = l1d.bkt[i].idealCy();
    for (int t = 0; t < LPM_NUM_TYPES; t++)
        lpm[cpu][t].demandCompute(ideals);
}

/*-----------------------------------------------------------------
 * Standalone accessors
 *-----------------------------------------------------------------*/
inline double lpm_c_amat(int cpu, uint8_t ct, uint64_t α) {
    return α ? (double)lpm[cpu][ct].bkt[BKT_G].ω_activeMemCy() / α : 0.0;
}
inline double lpm_apc(int cpu, uint8_t ct, uint64_t α) {
    uint64_t w = lpm[cpu][ct].bkt[BKT_G].ω_activeMemCy();
    return w ? (double)α / w : 0.0;
}
inline double lpm_mst(int cpu, uint8_t ct, uint64_t α) {
    return α ? (double)lpm[cpu][ct].bkt[BKT_G].cy[CY_M] / α : 0.0;
}

#ifdef LPM_LONG_SHORT_WIN
inline double get_kappa_short(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WS].κ_pureMissFracOfMissCy();
}
inline double get_kappa_long(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WL].κ_pureMissFracOfMissCy();
}
inline double get_phi_short(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WS].φ_hitCyFracOfActiveCy();
}
inline double get_phi_long(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WL].φ_hitCyFracOfActiveCy();
}
inline double get_mu_short(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WS].µ_missCyFracOfActiveCy();
}
inline double get_mu_long(int cpu, uint8_t ct) {
    return lpm[cpu][ct].bkt[BKT_WL].µ_missCyFracOfActiveCy();
}
inline double get_mst_short(int cpu, uint8_t ct) {
    return lpm[cpu][ct].met[MET_WS].mst_pureMissCyDivAccesses;
}
inline double get_mst_long(int cpu, uint8_t ct) {
    return lpm[cpu][ct].met[MET_WL].mst_pureMissCyDivAccesses;
}
inline bool kappa_durable(int cpu, uint8_t ct, double epsilon = 0.02) {
    return (get_kappa_long(cpu, ct) + epsilon) >= get_kappa_short(cpu, ct);
}
#endif

/*-----------------------------------------------------------------
 * Recursive C-AMAT [eq 31]
 *-----------------------------------------------------------------*/
inline double get_recursive_camat(int cpu) {
    const auto& l1 = lpm[cpu][LPM_L1D]; const auto& l2 = lpm[cpu][LPM_L2C];
    const auto& llc = lpm[cpu][LPM_LLC]; const auto& dram = lpm[cpu][LPM_DRAM];
    double mk_l1 = l1.bkt[BKT_G].µ_missCyFracOfActiveCy() * l1.bkt[BKT_G].κ_pureMissFracOfMissCy();
    double mk_l2 = l2.bkt[BKT_G].µ_missCyFracOfActiveCy() * l2.bkt[BKT_G].κ_pureMissFracOfMissCy();
    double mk_llc = llc.bkt[BKT_G].µ_missCyFracOfActiveCy() * llc.bkt[BKT_G].κ_pureMissFracOfMissCy();
    return l1.met[MET_G].camat_activeMemCyDivAccesses + mk_l1 * l2.met[MET_G].camat_activeMemCyDivAccesses
         + mk_l1 * mk_l2 * llc.met[MET_G].camat_activeMemCyDivAccesses
         + mk_l1 * mk_l2 * mk_llc * dram.met[MET_G].camat_activeMemCyDivAccesses;
}

inline double get_recursive_camat_roi(int cpu) {
    const auto& l1 = lpm[cpu][LPM_L1D]; const auto& l2 = lpm[cpu][LPM_L2C];
    const auto& llc = lpm[cpu][LPM_LLC]; const auto& dram = lpm[cpu][LPM_DRAM];
    double mk_l1 = l1.roi.µ_missCyFracOfActiveCy() * l1.roi.κ_pureMissFracOfMissCy();
    double mk_l2 = l2.roi.µ_missCyFracOfActiveCy() * l2.roi.κ_pureMissFracOfMissCy();
    double mk_llc = llc.roi.µ_missCyFracOfActiveCy() * llc.roi.κ_pureMissFracOfMissCy();
    return l1.roi_m.camat_activeMemCyDivAccesses + mk_l1 * l2.roi_m.camat_activeMemCyDivAccesses
         + mk_l1 * mk_l2 * llc.roi_m.camat_activeMemCyDivAccesses
         + mk_l1 * mk_l2 * mk_llc * dram.roi_m.camat_activeMemCyDivAccesses;
}

inline double get_recursive_apc(int cpu) {
    double rc = get_recursive_camat(cpu); return rc > 0.0 ? 1.0 / rc : 0.0;
}
inline double get_recursive_apc_roi(int cpu) {
    double rc = get_recursive_camat_roi(cpu); return rc > 0.0 ? 1.0 / rc : 0.0;
}

/* Standard LPMR [eq 52] */
inline double get_LPMR_std(int cpu, uint8_t ct) {
    uint64_t ideal = lpm[cpu][LPM_L1D].bkt[BKT_G].idealCy();
    return ideal ? (double)lpm[cpu][ct].bkt[BKT_G].ω_activeMemCy() / ideal : 0.0;
}
inline double get_LPMR_byp(int cpu, uint8_t ct) {
    const auto& ref = lpm[cpu][LPM_L1D].bkt[BKT_G];
    uint64_t app = ref.idealCy();
    uint64_t cor = lpm[cpu][LPM_L1D].m_byp_l1d_pureMissCy;
    uint64_t ideal = (app > cor) ? (app - cor) : 1;
    return (double)lpm[cpu][ct].bkt[BKT_G].ω_activeMemCy() / ideal;
}
inline double get_LPMR_roi_std(int cpu, uint8_t ct) {
    uint64_t ideal = lpm[cpu][LPM_L1D].roi.idealCy();
    return ideal ? (double)lpm[cpu][ct].roi.ω_activeMemCy() / ideal : 0.0;
}
inline double get_LPMR_roi_byp(int cpu, uint8_t ct) {
    const auto& ref = lpm[cpu][LPM_L1D].roi;
    uint64_t app = ref.idealCy();
    uint64_t cor = lpm[cpu][LPM_L1D].roi_m_byp_l1d_pureMissCy;
    uint64_t ideal = (app > cor) ? (app - cor) : 1;
    return (double)lpm[cpu][ct].roi.ω_activeMemCy() / ideal;
}

inline double get_LPMR_global_std(int cpu) {
    return get_LPMR_std(cpu,LPM_L1D) * get_LPMR_std(cpu,LPM_L2C) * get_LPMR_std(cpu,LPM_LLC) * get_LPMR_std(cpu,LPM_DRAM);
}
inline double get_LPMR_global_byp(int cpu) {
    return get_LPMR_byp(cpu,LPM_L1D) * get_LPMR_byp(cpu,LPM_L2C) * get_LPMR_byp(cpu,LPM_LLC) * get_LPMR_byp(cpu,LPM_DRAM);
}
inline double get_LPMR_global_roi_std(int cpu) {
    return get_LPMR_roi_std(cpu,LPM_L1D) * get_LPMR_roi_std(cpu,LPM_L2C) * get_LPMR_roi_std(cpu,LPM_LLC) * get_LPMR_roi_std(cpu,LPM_DRAM);
}
inline double get_LPMR_global_roi_byp(int cpu) {
    return get_LPMR_roi_byp(cpu,LPM_L1D) * get_LPMR_roi_byp(cpu,LPM_L2C) * get_LPMR_roi_byp(cpu,LPM_LLC) * get_LPMR_roi_byp(cpu,LPM_DRAM);
}

/* Final stats print — ROI snapshot */
inline void lpm_print(int cpu) {
    static const char* nm[] = {"ITLB","DTLB","STLB","L1I ","L1D ","L2C ","LLC ","DRAM"};
    printf("\n=== LPM Cycle Classification  CPU %d (ROI) ===\n", cpu);
    printf("%-4s %12s %12s %12s %12s | %12s %8s %8s %8s | %10s %10s | %8s %8s %8s %8s\n",
           "Lvl", "h(hit)", "m(miss)", "x(mixed)", "e(idle)",
           "omega", "mu", "kappa", "phi", "LPMR_std", "LPMR_byp",
           "C-AMAT", "APC", "MST", "CPA");
    for (int t = 0; t < LPM_NUM_TYPES; t++) {
        const auto& s = lpm[cpu][t];
        if (s.roi.totalCy() == 0) continue;
        printf("%-4s %12lu %12lu %12lu %12lu | %12lu %8.4f %8.4f %8.4f",
               nm[t], s.roi.cy[CY_H], s.roi.cy[CY_M], s.roi.cy[CY_X], s.roi.cy[CY_E],
               s.roi.ω_activeMemCy(),
               s.roi.µ_missCyFracOfActiveCy(), s.roi.κ_pureMissFracOfMissCy(), s.roi.φ_hitCyFracOfActiveCy());
        if (t >= LPM_L1D)
            printf(" | %10.6f %10.6f | %8.4f %8.4f %8.4f %8.4f",
                   get_LPMR_roi_std(cpu, t), get_LPMR_roi_byp(cpu, t),
                   s.roi_m.camat_activeMemCyDivAccesses, s.roi_m.apc_accessesDivActiveMemCy,
                   s.roi_m.mst_pureMissCyDivAccesses, s.roi_m.cpa_totalCyDivAccesses);
        else
            printf(" | %10s %10s | %8s %8s %8s %8s", "n/a","n/a","n/a","n/a","n/a","n/a");
        printf("\n");
    }
    printf("Global LPMR_std: %.6f  LPMR_byp: %.6f\n",
           get_LPMR_global_roi_std(cpu), get_LPMR_global_roi_byp(cpu));
}


/* ByPLatTracker class + g_*_byplat externs moved to byplat_tracker.h
 * so cache.h can include it without circular dep (lpm_tracker.h includes
 * cache.h, so cache.h can't pull from lpm_tracker.h). */
#include "byplat_tracker.h"

#ifdef LPM_CROSS_LEVEL_MLP
extern uint32_t g_crossmlp_load_peak[NUM_CPUS];
extern uint32_t g_crossmlp_all_peak[NUM_CPUS];
inline void byplat_cross_sample(int cpu,
                                uint32_t l1_mshr_load, uint32_t l1_mshr_all,
                                uint32_t l2_mshr_load, uint32_t l2_mshr_all,
                                uint32_t llc_mshr_load, uint32_t llc_mshr_all) {
  uint32_t load_p = l1_mshr_load + l2_mshr_load + llc_mshr_load
                  + g_l1_byplat[cpu].inflight_now + g_l2_byplat[cpu].inflight_now + g_llc_byplat[cpu].inflight_now;
  if (load_p > g_crossmlp_load_peak[cpu]) g_crossmlp_load_peak[cpu] = load_p;
  uint32_t all_p = l1_mshr_all + l2_mshr_all + llc_mshr_all
                 + g_l1_byplat[cpu].inflight_now + g_l2_byplat[cpu].inflight_now + g_llc_byplat[cpu].inflight_now;
  if (all_p > g_crossmlp_all_peak[cpu]) g_crossmlp_all_peak[cpu] = all_p;
}
#endif

#endif /* LPM_TRACKER_H */
