#ifndef CACHE_H
#define CACHE_H

#include "memory_class.h"
#include "defs.h"
#include "byplat_tracker.h"

// PAGE
extern uint32_t PAGE_TABLE_LATENCY, SWAP_LATENCY;

// CACHE TYPE
#define IS_ITLB 0
#define IS_DTLB 1
#define IS_STLB 2
#define IS_L1I  3
#define IS_L1D  4
#define IS_L2C  5
#define IS_LLC  6

// normalize to bit index: 0,1,2
#define CACHE_LVL_BASE IS_L1D

void print_cache_config();

// ============ BLOOM FILTER FOR QUEUE EARLY-EXIT ============
#define BLOOM_QTYPE_MSHR 0
#define BLOOM_QTYPE_COUNT 1

// P-BLOOM: blocked-512 / k=4 one-cacheline bloom filter.
// 512 bits = 4 x uint64 (one 64-byte cache line). k=4 bit positions per key.
// Replaces the old DualBloom128 (m=128, k=2) keeping the SAME API
// (reset/insert/maybe_contains, hash_a/hash_b retained for source compatibility).
//
// ZERO FALSE NEGATIVE GUARANTEE:
//   The 4 bit positions are derived DETERMINISTICALLY from the key (no
//   randomness, no ordering dependence). insert() sets exactly those 4 bits;
//   maybe_contains() tests those same 4 bits and returns true iff ALL are set.
//   Because insert set all 4, the identical key always tests positive.
//   Therefore maybe_contains(k) is ALWAYS true for any k previously inserted
//   (until reset). A bloom that never false-negatives preserves bit-exactness
//   of the pre-filtered MSHR scan (a negative skips the exact scan; here a
//   true match can never be skipped). Distinct keys may collide on all 4 bits
//   (false POSITIVE) — that only triggers the exact scan, which is harmless.
struct DualBloom128 {
    // one cache line, 512 bits
    alignas(64) uint64_t bits[4];

    // 64-bit avalanche mix (splitmix64 finalizer) — deterministic, no state.
    static inline uint64_t mix(uint64_t x) {
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    // Retained for API/source compatibility (single-bit 64-wide hashes).
    static inline uint64_t hash_a(uint64_t addr) { return 1ULL << (((addr << 7) ^ addr) & 63); }
    static inline uint64_t hash_b(uint64_t addr) { return 1ULL << (((addr << 17) ^ (addr >> 3)) & 63); }

    // Compute the 4 (word,bit) positions for a key. p in [0,512):
    //   word = p >> 6  (0..3),  bit = p & 63.
    static inline void positions(uint64_t addr, uint32_t w[4], uint64_t m[4]) {
        uint64_t h = mix(addr);
        // four independent 9-bit slices (9 bits => 0..511)
        uint32_t p0 = (uint32_t)( h        & 0x1FF);
        uint32_t p1 = (uint32_t)((h >> 16) & 0x1FF);
        uint32_t p2 = (uint32_t)((h >> 32) & 0x1FF);
        uint32_t p3 = (uint32_t)((h >> 48) & 0x1FF);
        w[0] = p0 >> 6; m[0] = 1ULL << (p0 & 63);
        w[1] = p1 >> 6; m[1] = 1ULL << (p1 & 63);
        w[2] = p2 >> 6; m[2] = 1ULL << (p2 & 63);
        w[3] = p3 >> 6; m[3] = 1ULL << (p3 & 63);
    }

    inline void reset() { bits[0] = 0; bits[1] = 0; bits[2] = 0; bits[3] = 0; }

    inline void insert(uint64_t addr) {
        uint32_t w[4]; uint64_t m[4];
        positions(addr, w, m);
        bits[w[0]] |= m[0];
        bits[w[1]] |= m[1];
        bits[w[2]] |= m[2];
        bits[w[3]] |= m[3];
    }

    inline bool maybe_contains(uint64_t addr) const {
        uint32_t w[4]; uint64_t m[4];
        positions(addr, w, m);
        return (bits[w[0]] & m[0]) && (bits[w[1]] & m[1])
            && (bits[w[2]] & m[2]) && (bits[w[3]] & m[3]);
    }
};

// Define to enable LLC hashmap MSHR lookup (separate function, no pollution of bloom path)
#define USE_LLC_HASHMAP_MSHR
#ifdef USE_LLC_HASHMAP_MSHR
#include "unordered_dense.h"
#define CHECK_MSHR(pkt) (cache_type == IS_LLC ? check_mshr_hashmap(pkt) : check_mshr(pkt))

// ===== LLC MSHR direct-index hashmap (v15_HSH C1) =====
// Fixed-capacity open-addressing, key/val arrays, sized to next_pow2(2*LLC_MSHR_SIZE).
// Bit-exact functional replacement for ankerl::unordered_dense::map<uint64_t, uint16_t>.
// Used ONLY by LLC (non-LLC caches use bloom path via CHECK_MSHR dispatch, cache.h:48).
constexpr uint32_t llc_mshr_next_pow2(uint32_t v) {
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}
#define LLC_MSHR_DIRECT_MAP_CAP (llc_mshr_next_pow2(2u * (uint32_t)(LLC_MSHR_SIZE)))

template<uint32_t CAP>
struct LlcMshrDirectMap {
    static_assert((CAP & (CAP - 1)) == 0, "CAP must be power-of-2");
    static constexpr uint32_t MASK = CAP - 1;
    static constexpr uint16_t EMPTY = 0xFFFF;
    alignas(64) uint64_t keys[CAP];
    alignas(64) uint16_t vals[CAP];
    uint16_t size_;

    LlcMshrDirectMap() : size_(0) {
        for (uint32_t i = 0; i < CAP; i++) { keys[i] = 0; vals[i] = EMPTY; }
    }

    static inline uint32_t hash(uint64_t key) {
        // shift+XOR fold defends against sequential ppage runs (num_adjacent_page).
        uint64_t x = key;
        x ^= x >> 11;
        return (uint32_t)((x >> 6) & MASK);
    }

    // Returns mshr index or -1.
    inline int32_t find(uint64_t key) const {
        uint32_t i = hash(key);
        while (vals[i] != EMPTY) {
            if (keys[i] == key) return (int32_t)vals[i];
            i = (i + 1) & MASK;
        }
        return -1;
    }

    inline void insert(uint64_t key, uint16_t val) {
        uint32_t i = hash(key);
        while (vals[i] != EMPTY && keys[i] != key) {
            i = (i + 1) & MASK;
        }
        if (vals[i] == EMPTY) size_++;
        keys[i] = key;
        vals[i] = val;
    }

    inline void erase(uint64_t key) {
        uint32_t i = hash(key);
        while (vals[i] != EMPTY && keys[i] != key) {
            i = (i + 1) & MASK;
        }
        if (vals[i] == EMPTY) return;
        // Clear target slot then re-insert every key in the contiguous occupied run
        // starting at (i+1) until the next EMPTY slot. This preserves linear-probe
        // chain invariant without relying on Robin-Hood-style backshift math.
        vals[i] = EMPTY; keys[i] = 0; size_--;
        uint32_t j = (i + 1) & MASK;
        while (vals[j] != EMPTY) {
            uint64_t rk = keys[j];
            uint16_t rv = vals[j];
            vals[j] = EMPTY; keys[j] = 0; size_--;
            insert(rk, rv);
            j = (j + 1) & MASK;
        }
    }

    inline void clear() {
        for (uint32_t i = 0; i < CAP; i++) { keys[i] = 0; vals[i] = EMPTY; }
        size_ = 0;
    }
};
#else
#define CHECK_MSHR(pkt) check_mshr(pkt)
#endif

class CACHE : public MEMORY {
  public:
    void dump_req(PACKET& o);
    void dump_req(PACKET* o);
    void dump_req_min_read(PACKET& o);  void dump_req_min_read(PACKET* o);
    void dump_req_min_write(PACKET& o); void dump_req_min_write(PACKET* o);
    void dump_req_min_mshr(PACKET& o);  void dump_req_min_mshr(PACKET* o);
    void dump_req_min_ret(PACKET& o);   void dump_req_min_ret(PACKET* o);
    void dump_req_min_pq(PACKET& o);    void dump_req_min_pq(PACKET* o);
    void dump_req_min_fill(PACKET& o);  void dump_req_min_fill(PACKET* o);
    int cpu;
    const string NAME;
    const uint32_t NUM_SET, NUM_WAY, NUM_LINE, WQ_SIZE, RQ_SIZE, PQ_SIZE, MSHR_SIZE;
    uint32_t LATENCY;
    BLOCK **block;
    // SOA tag mirror: contiguous packed per-set tag array for the hot way-scan.
    // tag_check[set*NUM_WAY + way] == block[set][way].tag when that way is VALID,
    // else TAG_INVALID (a value no real block address can take). This reproduces
    // the (valid && tag==addr) test exactly while scanning ONE cache line of
    // packed uint64 tags instead of NUM_WAY scattered ~56B BLOCK objects.
    static constexpr uint64_t TAG_INVALID = ~0ULL;
    uint64_t *tag_check = nullptr;
    int fill_level;
    uint32_t MAX_READ, MAX_FILL;
    uint64_t reads_available_this_cycle;
    uint8_t cache_type;
    uint8_t has_work = 0;
    uint32_t cache_idle_accumulator = 0;

    // prefetch stats
    uint64_t pf_requested,
             pf_issued,
             pf_useful,
             pf_useless,
	         pf_late,
             pf_fill;

    // bloom/mshr stats
    uint64_t bloom_check_total = 0, bloom_reject = 0, bloom_pass_hit = 0, bloom_pass_miss = 0;

    // queues
    PACKET_QUEUE WQ{NAME + "_WQ", WQ_SIZE}, // write queue
                 RQ{NAME + "_RQ", RQ_SIZE}, // read queue
                 PQ{NAME + "_PQ", PQ_SIZE}, // prefetch queue
                 MSHR{NAME + "_MSHR", MSHR_SIZE}, // MSHR
                 PROCESSED{NAME + "_PROESSED", ROB_SIZE}; // processed queue
    // PACKET_QUEUE ByPassed_DATA{NAME + "_ByPassed_DATA", ROB_SIZE}; // VB CUSTOM BYPASS QUEUE

    // Bloom filters: [qtype] — index 0 = MSHR (future: 1=RQ, 2=WQ)
    DualBloom128 bloom[BLOOM_QTYPE_COUNT];

#ifdef USE_LLC_HASHMAP_MSHR
    // v15_HSH C1: direct-index open-addressing map replacing ankerl::unordered_dense::map.
    // Non-LLC caches never invoke mshr_map (CHECK_MSHR dispatches to check_mshr for them, cache.h:48).
    LlcMshrDirectMap<LLC_MSHR_DIRECT_MAP_CAP> mshr_map;
#endif

    // LPM MSHR O(1) counters — replace per-cycle O(MSHR_SIZE) scans in operate()
    // inflight = entries with returned==INFLIGHT (not yet COMPLETED)
    // byp = entries with the relevant bypass flag set for this cache level
    uint16_t lpm_mshr_inflight_count = 0;
    uint16_t lpm_mshr_byp_count = 0;
    uint16_t lpm_mshr_inflight_per_cpu[NUM_CPUS] = {};
    uint16_t lpm_mshr_byp_per_cpu[NUM_CPUS] = {};
    uint16_t lpm_mshr_occ_per_cpu[NUM_CPUS] = {};  // total MSHR entries per CPU (for non-strict miss_active)

    // Rebuild MSHR bloom/hashmap from scratch (call after any MSHR remove)
    inline void bloom_rebuild_mshr() {
#ifdef USE_LLC_HASHMAP_MSHR
        if (cache_type == IS_LLC) {
            mshr_map.clear();
            for (uint16_t i = 0; i < MSHR_SIZE; i++) {
                if (MSHR.entry[i].address != 0)
                    mshr_map.insert(MSHR.entry[i].address, i);
            }
            return;
        }
#endif
        bloom[BLOOM_QTYPE_MSHR].reset();
        for (uint16_t i = 0; i < MSHR_SIZE; i++) {
            if (MSHR.entry[i].address != 0)
                bloom[BLOOM_QTYPE_MSHR].insert(MSHR.entry[i].address);
        }
    }

    uint64_t sim_access[NUM_CPUS][NUM_TYPES],
             sim_hit[NUM_CPUS][NUM_TYPES],
             sim_miss[NUM_CPUS][NUM_TYPES],
             roi_access[NUM_CPUS][NUM_TYPES],
             roi_hit[NUM_CPUS][NUM_TYPES],
             roi_miss[NUM_CPUS][NUM_TYPES];

    // Tier A perf: shadow hot cacheline mirroring sim_access/sim_miss aggregates for LPM-tracker reads.
    // sim_*/roi_* arrays remain authoritative for stats; shadow is derived view only. IPC bit-exact.
    struct alignas(64) LpmShadow {
        uint64_t alpha_total;
        uint64_t load_alpha;
        uint64_t load_miss;
        uint64_t _pad[5];
    };
    LpmShadow lpm_shadow[NUM_CPUS];
    inline void lpm_shadow_inc(uint32_t cpu_, uint32_t type_, bool is_miss) {
        lpm_shadow[cpu_].alpha_total++;
        if (type_ == 0) {
            lpm_shadow[cpu_].load_alpha++;
            if (is_miss) lpm_shadow[cpu_].load_miss++;
        }
    }

    uint64_t total_miss_latency;

    // Bypass-aware load counters (parallel to sim_*, adds bypass as 3rd category)
    // wByP = "with Bypass": access = hit + miss + bypassed
    uint64_t sim_access_wByP[NUM_CPUS];   // sim_access[cpu][LOAD] + ByP_issued
    uint64_t sim_hit_wByP[NUM_CPUS];      // same as sim_hit[cpu][LOAD]
    uint64_t sim_miss_wByP[NUM_CPUS];     // same as sim_miss[cpu][LOAD]
    uint64_t sim_byp_wByP[NUM_CPUS];      // = total_ByP_issued (duplicated for clean accounting)
    uint64_t roi_access_wByP[NUM_CPUS];   // ROI snapshot of sim_access_wByP
    uint64_t roi_hit_wByP[NUM_CPUS];      // ROI snapshot of sim_hit_wByP
    uint64_t roi_miss_wByP[NUM_CPUS];     // ROI snapshot of sim_miss_wByP
    uint64_t roi_byp_wByP[NUM_CPUS];      // ROI snapshot of sim_byp_wByP

// VB: CCUSTOM CODE START
    // Cumulative post-warmup (for final stats, reset at warmup like sim_miss)
    uint64_t total_ByP_issued[NUM_CPUS];  // bypass model said yes
    uint64_t total_ByP_req[NUM_CPUS];     // bypass eligible (new miss + room)
    // Interval (for heartbeat, reset after each heartbeat print)
    uint64_t ByP_issued[NUM_CPUS];
    uint64_t ByP_req[NUM_CPUS];
    bool FORCE_ALL_HITS; // VB CUSTOM
    uint8_t is_bypassing = 0;   // bit0=L1 bit1=L2 bit2=LLC

// Bypass bit encoding for CACHE::is_bypassing config flag: L1=1, L2=2, LLC=4
#define BYP_L1_BIT   1u
#define BYP_L2_BIT   2u
#define BYP_LLC_BIT  4u

// Bypass per-level flags (separate variables, no cross-level interference)
#define BYPASS_L1(pkt)     ((pkt).l1_bypassed = 1)
#define BYPASS_L2(pkt)     ((pkt).l2_bypassed = 1)
#define BYPASS_LLC(pkt)    ((pkt).llc_bypassed = 1)
#define UNBYPASS_L1(pkt)   ((pkt).l1_bypassed = 0)
#define UNBYPASS_L2(pkt)   ((pkt).l2_bypassed = 0)
#define UNBYPASS_LLC(pkt)  ((pkt).llc_bypassed = 0)
#define isBYPASSING_L1(pkt)  ((pkt).l1_bypassed)
#define isBYPASSING_L2(pkt)  ((pkt).l2_bypassed)
#define isBYPASSING_LLC(pkt) ((pkt).llc_bypassed)

#ifndef SHALL_L1D_BYPASS_FILL_DEFINED
inline void l1d_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {}
#endif
#ifndef SHALL_L2C_BYPASS_FILL_DEFINED
inline void l2c_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {}
#endif
#ifndef SHALL_LLC_BYPASS_FILL_DEFINED
inline void llc_bypass_fill(int cpu, CACHE *L1D, CACHE *L2C, CACHE *LLC, PACKET &pkt) {}
#endif


// ============== STAGE-NAMED DP MACROS ==============
// Output matches dump_req format: [NAME_func] STAGE <dump_req output> cy=X
// Only fires for L1D/L2C/LLC (cache_type >= IS_L1D)

#ifdef DEBUG_PRINT
#define _DP_LVL_OK (cache_type >= IS_L1D)

// Common head: [NAME_func][TAG] msg-padded. Used by all stage macros.
#define _DPH(TAG, msg) std::cout << "[" << std::setw(20) << std::left << (std::string(NAME) + "_" + __func__) << "][" << std::setw(5) << TAG << "] " << std::setw(22) << std::left << msg << " "

// ---- RQ ----
#define DP_RQ(p, src)            DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { _DPH("RQ",     std::string("NEW src=") + (src));         dump_req_min_read(p);  std::cout << std::endl; } } while(0) )
#define DP_RQ_HIT(p, src)        DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { _DPH("RQ",     std::string("WQHIT src=") + (src));       dump_req_min_read(p);  std::cout << std::endl; } } while(0) )
#define DP_RQ_MERGE(p_in, e_ex, tag) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p_in)->cpu],(p_in)->cpu,(p_in)->address,(p_in)->instr_id)) { \
    _DPH("RQ", std::string("MERGE kind=") + (tag) + " IN"); dump_req_min_read(p_in); std::cout << std::endl; \
    _DPH("RQ", std::string("MERGE kind=") + (tag) + " EX"); dump_req_min_read(e_ex); std::cout << std::endl; } } while(0) )

// ---- WQ / PQ ----
#define DP_WQ(p)      DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { _DPH("WQ", "NEW");       dump_req_min_write(p); std::cout << std::endl; } } while(0) )
#define DP_PQ(p)      DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { _DPH("PQ", "NEW");       dump_req_min_pq(p);    std::cout << std::endl; } } while(0) )
#define DP_PQ_FULL(p) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { _DPH("PQ", "FULL");      dump_req_min_pq(p);    std::cout << std::endl; } } while(0) )

// ---- HIT / MISS / INVAL / BYPASS / FORWARD ----
#define DP_HIT(p, set_v, way_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("READ", std::string("RD_HIT set=") + std::to_string(set_v) + " way=" + std::to_string(way_v)); \
    dump_req_min_read(p); std::cout << std::endl; } } while(0) )

#define DP_MISS(p, mshr_idx) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("READ", std::string("RD_MISS mshr_idx=") + std::to_string(mshr_idx) + " occu=" + std::to_string(MSHR.occupancy)); \
    dump_req_min_read(p); std::cout << std::endl; } } while(0) )

#define DP_INVAL(addr_v, set_v, way_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[cpu],cpu,(addr_v),0)) { \
    _DPH("READ", std::string("INVAL set=") + std::to_string(set_v) + " way=" + std::to_string(way_v)); \
    std::cout << "Addr=0x" << std::hex << (addr_v) << std::dec << " cy=" << current_core_cycle[cpu] << std::endl; } } while(0) )

#define DP_BYP(p, byp_lvl) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("MSHR", std::string("BYP_TAKEN lvl=") + (byp_lvl)); \
    dump_req_min_mshr(p); std::cout << std::endl; } } while(0) )

#define DP_FWD(p, to_name) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("READ", std::string("FWD to=") + (to_name)); \
    dump_req_min_read(p); std::cout << std::endl; } } while(0) )

// ---- MSHR ----
#define DP_MSHR_ADD(p, idx_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("MSHR", std::string("ADD idx=") + std::to_string(idx_v) + " occu=" + std::to_string(MSHR.occupancy)); \
    dump_req_min_mshr(p); std::cout << std::endl; } } while(0) )

#define DP_MSHR_MERGE(p_in, e_mshr, idx_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p_in)->cpu],(p_in)->cpu,(p_in)->address,(p_in)->instr_id)) { \
    _DPH("MSHR", std::string("MERGE idx=") + std::to_string(idx_v) + " IN"); dump_req_min_mshr(p_in);  std::cout << std::endl; \
    _DPH("MSHR", std::string("MERGE idx=") + std::to_string(idx_v) + " EX"); dump_req_min_mshr(e_mshr); std::cout << std::endl; } } while(0) )

#define DP_MSHR_NEW_ADDR(p) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("MSHR", "NEW_ADDR"); dump_req_min_mshr(p); std::cout << std::endl; } } while(0) )

// ---- GUARDED add_rq: check RQ not full, call add_rq, on -2 undo mshr + set miss_handled=0 ----
// Usage: GUARDED_ADD_RQ(pkt_ptr, has_mshr, miss_handled_var)
//   has_mshr=1  → on -2: remove_mshr(pkt_ptr) + miss_handled=0
//   has_mshr=0  → on -2: miss_handled=0 only
// check lower RQ capacity; if full set mh=0, else call add_rq
#define GUARDED_ADD_RQ(pkt, mh) do { \
    if (lower_level->get_occupancy(1,(pkt)->address) >= lower_level->get_size(1,(pkt)->address)) { (mh) = 0; } \
    else { lower_level->add_rq(pkt); } \
} while(0)

// check MSHR capacity; if full set mh=0, else call add_mshr
#define GUARDED_ADD_MSHR(pkt, mh) do { \
    if (MSHR.occupancy >= MSHR_SIZE) { (mh) = 0; } \
    else { add_mshr(pkt); } \
} while(0)

// ---- RETURN / FILL ----
#define DP_RET(p, from_tag) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(p)->cpu],(p)->cpu,(p)->address,(p)->instr_id)) { \
    _DPH("RET", std::string("from=") + (from_tag)); dump_req_min_ret(p); std::cout << std::endl; } } while(0) )

#define DP_RET_AFTER(e_mshr, idx_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(e_mshr).cpu],(e_mshr).cpu,(e_mshr).address,(e_mshr).instr_id)) { \
    _DPH("RET", std::string("AFTER idx=") + std::to_string(idx_v)); dump_req_min_ret(e_mshr); std::cout << std::endl; } } while(0) )

#define DP_FILL(e_mshr, set_v, way_v) DP( do { if (_DP_LVL_OK && DP_GATE_WW(current_core_cycle[(e_mshr).cpu],(e_mshr).cpu,(e_mshr).address,(e_mshr).instr_id)) { \
    _DPH("FILL", std::string("set=") + std::to_string(set_v) + " way=" + std::to_string(way_v)); \
    dump_req_min_fill(e_mshr); std::cout << std::endl; } } while(0) )

#endif // DEBUG_PRINT
#ifndef DEBUG_PRINT
#define DP_RQ(p,s)
#define DP_RQ_HIT(p,s)
#define DP_RQ_MERGE(p,e,t)
#define DP_WQ(p)
#define DP_PQ(p)
#define DP_PQ_FULL(p)
#define DP_HIT(p,s,w)
#define DP_MISS(p,i)
#define DP_INVAL(a,s,w)
#define DP_BYP(p,l)
#define DP_FWD(p,t)
#define DP_MSHR_ADD(p,i)
#define DP_MSHR_MERGE(p,e,i)
#define DP_MSHR_NEW_ADDR(p)
#define DP_RET(p,f)
#define DP_RET_AFTER(e,i)
#define DP_FILL(e,s,w)
#endif

    // bool BYPASS_L1D_OnNewMiss = false; // VB CUSTOM
    bool BYPASS_L1D_on_MSHR_cap = false; // VB CUSTOM
    int probe_mshr(PACKET *packet) const;

    void set_force_all_hits(bool toEnable); // VB CUSTOM
// VB: CUSTOM CODE END 


    // uint8_t get_bypass_bit();
    uint8_t return_data_bypass_forward_destination(PACKET *packet);
    // uint8_t return_data_bypass_do_forward(PACKET *packet, uint8_t bypass_destination);
    // uint8_t return_data_to_cpu(PACKET *packet);
    int check_mshr_fully(PACKET *packet);
    // bool should_bypass_current_level(PACKET *packet);
    // uint8_t get_bypass_destination(PACKET *packet);
    // void forward_bypassed_data(PACKET *packet);

    // bool should_bypass_to_lower();
    // CACHE* get_cache_at_level(const char* level_name);
    // bool handle_bypass();
// VB: CUSTOM BYPASS FUNCS END
    // void handle_read_pf_on_load(uint16_t read_cpu, int index, uint32_t set, int way);
    

    // constructor
    CACHE(string v1, uint32_t v2, int v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8) 
        : NAME(v1), NUM_SET(v2), NUM_WAY(v3), NUM_LINE(v4), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8),
        FORCE_ALL_HITS(false) { // VB CUSTOM NOT FORCE ALL HITS

        LATENCY = 0;

        for (int q = 0; q < BLOOM_QTYPE_COUNT; q++)
            bloom[q].reset();

        // cache block
        block = new BLOCK* [NUM_SET];
        // SOA tag mirror: all ways start INVALID (matches BLOCK default valid=0).
        tag_check = new uint64_t[(size_t)NUM_SET * NUM_WAY];
        for (size_t t = 0; t < (size_t)NUM_SET * NUM_WAY; t++)
            tag_check[t] = TAG_INVALID;
        for (uint32_t i=0; i<NUM_SET; i++) {
            block[i] = new BLOCK[NUM_WAY];

            for (uint32_t j=0; j<NUM_WAY; j++) {
                block[i][j].lru = j;
            }
        }

        for (uint32_t i=0; i<NUM_CPUS; i++) {
            upper_level_icache[i] = NULL;
            upper_level_dcache[i] = NULL;

            for (uint32_t j=0; j<NUM_TYPES; j++) {
                sim_access[i][j] = 0;
                sim_hit[i][j] = 0;
                sim_miss[i][j] = 0;
                roi_access[i][j] = 0;
                roi_hit[i][j] = 0;
                roi_miss[i][j] = 0;
            }
            lpm_shadow[i].alpha_total = 0;
            lpm_shadow[i].load_alpha = 0;
            lpm_shadow[i].load_miss = 0;
            for (int p = 0; p < 5; p++) lpm_shadow[i]._pad[p] = 0;
        }

        total_miss_latency = 0;
        for (uint32_t i=0; i<NUM_CPUS; i++) {
            total_ByP_issued[i] = 0;
            total_ByP_req[i] = 0;
            ByP_issued[i] = 0;
            ByP_req[i] = 0;
            sim_access_wByP[i] = 0;
            sim_hit_wByP[i] = 0;
            sim_miss_wByP[i] = 0;
            sim_byp_wByP[i] = 0;
            roi_access_wByP[i] = 0;
            roi_hit_wByP[i] = 0;
            roi_miss_wByP[i] = 0;
            roi_byp_wByP[i] = 0;
        }
            lower_level = NULL;
            extra_interface = NULL;
            fill_level = -1;
            MAX_READ = 1;
            MAX_FILL = 1;

            pf_requested = 0;
            pf_issued = 0;
            pf_useful = 0;
            pf_useless = 0;
            pf_late = 0;
            pf_fill = 0;
    };

    // destructor
    ~CACHE() {
        for (uint32_t i=0; i<NUM_SET; i++)
            delete[] block[i];
        delete[] block;
        delete[] tag_check;
    };

    // functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);



    void return_data(PACKET *packet),
         operate(),
         increment_WQ_FULL(uint64_t address);

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address) const;
    uint32_t get_size(uint8_t queue_type, uint64_t address) const;

    int  check_hit(PACKET *packet),
         invalidate_entry(uint64_t inval_addr),
         check_mshr(PACKET *packet),
         check_mshr_hashmap(PACKET *packet),
         prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, uint32_t prefetch_metadata);
        //  kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, int delta, int depth, int signature, int confidence, uint32_t prefetch_metadata);

    void handle_fill(),
         handle_writeback(),
         handle_read(),
         handle_prefetch();

    // VB: JOB IS TO HANDLE MERGES FOR ANY QUEUE BUT ONLY FOR PREFETCH PACKETS
    void merge_with_prefetch(PACKET &mshr_packet, PACKET &queue_packet);

    void add_mshr(PACKET *packet),
         update_fill_cycle(),
         llc_initialize_replacement(),
         update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit),
         llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit),
         lru_update(uint32_t set, uint32_t way),
         fill_cache(uint32_t set, uint32_t way, PACKET *packet),
         replacement_final_stats(),
         llc_replacement_final_stats(),
         //prefetcher_initialize(),
         l1d_prefetcher_initialize(),
         l2c_prefetcher_initialize(),
         llc_prefetcher_initialize(),
         prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
         l1d_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
         prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
         l1d_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in),
         //prefetcher_final_stats(),
         l1d_prefetcher_final_stats(),
         l2c_prefetcher_final_stats(),
         llc_prefetcher_final_stats();

    uint32_t l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in),
         llc_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in),
         l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in),
         llc_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in);

    void prefetcher_feedback(uint64_t &pref_gen, uint64_t &pref_fill, uint64_t &pref_used, uint64_t &pref_late);
    
    void return_to_upper_level(PACKET& packet);

    // ---------------------------------------------------------------
    // handle_read hit-path inline helpers
    // ---------------------------------------------------------------

    // Routes hit packet to PROCESSED queue for TLB/L1I/L1D cache types.
    // Sets instruction_pa / data_pa / data fields for TLBs before queuing.
    void handle_read_hit_processed(int index, uint32_t set, uint32_t way);

    // Handles bypass hit returns:
    //   L1 bypass: packet hit at L2C with l1_bypassed==1 → route via L2C PROCESSED (→CPU).
    //   L2 bypass: packet hit at LLC with l2_bypassed==1 → double-hop return directly to L1D.
    // Returns true if a bypass return was performed (caller skips normal return path).
    bool handle_read_hit_bypass_return(uint16_t read_cpu, int index);

    // Notifies prefetcher of a read hit on LOAD type.
    // Dispatches to l1d / l2c / llc prefetcher_operate per cache level.
    void handle_read_hit_pf_operate(uint16_t read_cpu, int index, uint32_t set, uint32_t way);

    // Updates replacement state (LRU) for the hit way.
    void handle_read_hit_replacement(uint16_t read_cpu, int index, uint32_t set, uint32_t way);

    // Increments sim_hit / sim_access counters, and wByP load counters.
    void handle_read_hit_stats(uint16_t read_cpu, int index);

    // If fill_level < this cache's fill_level, returns data to the upper level.
    void handle_read_hit_return(int index);

    // Increments pf_useful on prefetch hit, clears prefetch bit, sets used=1.
    // Increments HIT/ACCESS counters and removes entry from RQ.
    void handle_read_hit_pf_useful_and_remove(int index, uint32_t set, uint32_t way);

    // ---------------------------------------------------------------
    // handle_read miss-path inline helpers
    // ---------------------------------------------------------------

    // Checks bypass conditions for L1D/L2C/LLC on a new miss.
    // Increments bypass opportunity counters, sets bypass flag on packet,
    // calls lower_level->add_rq(), and returns true if bypass was taken.
    // NOTE: defined out-of-line in cache.cc (bypass model funcs only visible there).
    bool handle_read_miss_bypass(uint16_t read_cpu, int index, int mshr_index);

    // Handles new LLC miss: checks DRAM RQ capacity, calls add_mshr + lower_level->add_rq.
    // Sets miss_handled=0 if DRAM RQ is full.
    void handle_read_miss_new_llc(int index, uint8_t& miss_handled);

    // Handles new non-LLC miss: checks lower-level RQ capacity, calls add_mshr + lower_level->add_rq.
    // Special case: IS_STLB with no lower_level emulates page table walk via va_to_pa.
    // Sets miss_handled=0 if lower RQ is full.
    void handle_read_miss_new_other(uint16_t read_cpu, int index, uint8_t& miss_handled);

    // Handles MSHR-full stall: sets miss_handled=0, increments STALL counter.
    void handle_read_miss_mshr_full(int index, uint8_t& miss_handled);

    // Merges dependency indices from in-flight RQ entry into existing MSHR entry.
    // Handles RFO (sq/lq), instruction (rob), and data load (lq+sq) cases.
    void handle_read_miss_inflight_merge_deps(int index, int mshr_index);

    // Updates MSHR fill_level if incoming RQ entry has a shallower fill target.
    void handle_read_miss_inflight_fill_level(int index, int mshr_index);

    // Resolves L1 bypass mismatch at L2C: if l1_bypassed differs between RQ and MSHR,
    // injects LQ deps into L1D MSHR and clears l1_bypassed to use normal fill path.
    void handle_read_miss_inflight_bypass_l1_mismatch(int index, int mshr_index);

    // Resolves L2 bypass mismatch at LLC: if l2_bypassed differs between RQ and MSHR,
    // injects LQ deps into L2C MSHR and clears l2_bypassed to use normal fill path.
    void handle_read_miss_inflight_bypass_l2_mismatch(int index, int mshr_index);

    // Handles late prefetch takeover: increments pf_late, calls merge_with_prefetch,
    // then restores bypass flags from RQ entry onto MSHR (merge_with_prefetch overwrites them).
    void handle_read_miss_inflight_prefetch_takeover(int index, int mshr_index);

    // For non-prefetch in-flight MSHR: inserts rob/lq/sq index into MSHR dep set per packet type.
    void handle_read_miss_inflight_non_prefetch_merge(int index, int mshr_index);

    // Notifies prefetcher of a read miss on LOAD type.
    void handle_read_miss_handled_pf_operate(uint16_t read_cpu, int index);

    // Increments MISS/ACCESS counters and removes entry from RQ.
    void handle_read_miss_handled_stats_remove(int index);

    // ---------------------------------------------------------------
    // handle_writeback hit-path inline helpers
    // ---------------------------------------------------------------

    // Updates replacement state, collects sim_hit/sim_access, marks block dirty,
    // copies TLB data fields, checks fill_level and returns to upper level, removes from WQ.
    void handle_writeback_hit(uint16_t writeback_cpu, int index, uint32_t set, uint32_t way);

    // ---------------------------------------------------------------
    // handle_writeback miss-path inline helpers (L1D RFO path)
    // ---------------------------------------------------------------

    // New RFO miss: checks lower RQ capacity, calls add_mshr + lower->add_rq.
    // Sets miss_handled=0 if lower RQ is full.
    void handle_writeback_miss_new(int index, uint8_t& miss_handled);

    // MSHR full stall for writeback path.
    void handle_writeback_miss_mshr_full(int index, uint8_t& miss_handled);

    // In-flight MSHR merge for writeback: updates fill_level, merges prefetch if needed.
    void handle_writeback_miss_inflight(int index, int mshr_index);

    // Increments MISS/ACCESS and removes entry from WQ.
    void handle_writeback_miss_handled_stats_remove(int index);

    // ---------------------------------------------------------------
    // handle_writeback non-L1D miss path (writeback eviction into lower level)
    // ---------------------------------------------------------------

    // Finds victim way for a writeback miss (non-L1D). Returns the way index.
    uint32_t handle_writeback_find_victim(uint32_t writeback_cpu, int index, uint32_t set);

    // Checks dirty eviction: if block dirty and lower WQ has room, constructs and sends writeback packet.
    // Sets do_fill=0 and increments STALL if lower WQ is full.
    void handle_writeback_evict_dirty(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way, uint8_t& do_fill);

    // Calls per-type prefetcher fill hook, updates replacement, collects stats,
    // calls fill_cache, marks dirty, checks fill_level, increments MISS/ACCESS, removes from WQ.
    void handle_writeback_do_fill(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way);

    // ---------------------------------------------------------------
    // handle_fill inline helpers
    // ---------------------------------------------------------------

    // Finds victim way for a fill. Returns the way index.
    uint32_t handle_fill_find_victim(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set);

    // Checks dirty eviction on fill: constructs writeback packet to lower WQ if dirty block evicted.
    // Sets do_fill=0 and increments STALL if lower WQ is full.
    void handle_fill_evict_dirty(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way, uint8_t& do_fill);

    // Calls per-type prefetcher cache fill hook for L1D/L2C/LLC.
    void handle_fill_pf_fill(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way);

    // Updates replacement state for the filled way.
    void handle_fill_replacement(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way);

    // Increments sim_miss/sim_access and wByP counters.
    // wByP skipped for llc_bypassed: already counted at bypass-decision time in handle_read_miss_bypass.
    void handle_fill_stats(uint16_t fill_cpu, uint16_t mshr_index);

    // Calls fill_cache, marks dirty for L1D RFO.
    void handle_fill_cache_and_dirty(uint16_t mshr_index, uint32_t set, uint32_t way);

    // If fill_level < this cache's fill_level, returns data to upper level.
    void handle_fill_return(uint16_t mshr_index);

    // Routes filled packet to PROCESSED queue for TLB/L1I/L1D types,
    // and handles bypass return paths for L2C (l1_bypassed→PROCESSED) and LLC (l2_bypassed→L1D).
    void handle_fill_processed_and_bypass_return(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way);

    // Updates miss latency stats, removes from MSHR, decrements num_returned, calls update_fill_cycle.
    void handle_fill_remove(uint16_t fill_cpu, uint16_t mshr_index);

    constexpr uint32_t get_set(uint64_t address) {
        return (uint32_t)(address & ((1 << lg2(NUM_SET)) - 1));
    }
    uint32_t get_way(uint64_t address, uint32_t set) const,
             find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type),
             llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type),
             lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type);
};

#endif
