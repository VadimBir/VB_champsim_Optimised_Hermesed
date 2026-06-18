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

// Dual 64-bit bloom: two independent filters, both must agree → FPR² reduction
struct DualBloom128 {
    uint64_t bits_a;
    uint64_t bits_b;

    static inline uint64_t hash_a(uint64_t addr) { return 1ULL << (((addr << 7) ^ addr) & 63); }
    static inline uint64_t hash_b(uint64_t addr) { return 1ULL << (((addr << 17) ^ (addr >> 3)) & 63); }

    inline void reset()               { bits_a = 0; bits_b = 0; }
    inline void insert(uint64_t addr) { bits_a |= hash_a(addr); bits_b |= hash_b(addr); }
    inline bool maybe_contains(uint64_t addr) const {
        return (bits_a & hash_a(addr)) && (bits_b & hash_b(addr));
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
    const uint32_t set_mask;
    uint32_t LATENCY;
    BLOCK **block;
    int fill_level;
    uint32_t MAX_READ, MAX_FILL;
    uint64_t reads_available_this_cycle;
    uint8_t cache_type;
    uint8_t has_work = 0;

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
        : NAME(v1), NUM_SET(v2), NUM_WAY(v3), NUM_LINE(v4), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8), set_mask((1u << lg2(v2)) - 1),
        FORCE_ALL_HITS(false) { // VB CUSTOM NOT FORCE ALL HITS

        LATENCY = 0;

        for (int q = 0; q < BLOOM_QTYPE_COUNT; q++)
            bloom[q].reset();

        // cache block
        block = new BLOCK* [NUM_SET];
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

    int  check_hit(const PACKET *packet),
         invalidate_entry(uint64_t inval_addr),
         check_mshr(const PACKET *packet),
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
    
    inline void return_to_upper_level(PACKET& packet) {
        DP_RET_M("RETURN_UP_ENTRY", &packet);
        if (packet.instruction) {
            DP_RET_M("RETURN_UP_to_icache", &packet);
            upper_level_icache[packet.cpu]->return_data(&packet);
        } else {
            DP_RET_M("RETURN_UP_to_dcache", &packet);
            upper_level_dcache[packet.cpu]->return_data(&packet);
        }
        DP_RET_M("RETURN_UP_DONE", &packet);
    }

    // ---------------------------------------------------------------
    // handle_read hit-path inline helpers
    // ---------------------------------------------------------------

    // Routes hit packet to PROCESSED queue for TLB/L1I/L1D cache types.
    // Sets instruction_pa / data_pa / data fields for TLBs before queuing.
    inline void handle_read_hit_processed(int index, uint32_t set, uint32_t way) {
        if (cache_type == IS_ITLB) {
            RQ.entry[index].instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "ITLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_DTLB) {
            RQ.entry[index].data_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "DTLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_STLB) {
            RQ.entry[index].data = block[set][way].data;
        }
        else if (cache_type == IS_L1I) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L1I PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if ((cache_type == IS_L1D) && (RQ.entry[index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L1D PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
    }

    // Handles bypass hit returns:
    //   L1 bypass: packet hit at L2C with l1_bypassed==1 → route via L2C PROCESSED (→CPU).
    //   L2 bypass: packet hit at LLC with l2_bypassed==1 → double-hop return directly to L1D.
    // Returns true if a bypass return was performed (caller skips normal return path).
    inline bool handle_read_hit_bypass_return(uint16_t read_cpu, int index) {
#ifdef BYPASS_L1_LOGIC
        if ((cache_type == IS_L2C) && (RQ.entry[index].type == LOAD
                && RQ.entry[index].l1_bypassed == 1 && !RQ.entry[index].instruction)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L2C PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
            g_l1_byplat[read_cpu].on_fill(RQ.entry[index].address >> LOG2_BLOCK_SIZE);
            DP_RET(&RQ.entry[index],"L1BYP_HIT");
            return true;
        }
#endif
#ifdef BYPASS_L2_LOGIC
        if ((cache_type == IS_LLC) && (RQ.entry[index].type == LOAD
                && RQ.entry[index].l2_bypassed == 1 && !RQ.entry[index].instruction)) {
            upper_level_dcache[read_cpu]->upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
            g_l2_byplat[read_cpu].on_fill(RQ.entry[index].address >> LOG2_BLOCK_SIZE);
            DP_RET(&RQ.entry[index],"L2BYP_HIT");
            return true;
        }
#endif
        return false;
    }

    // Notifies prefetcher of a read hit on LOAD type.
    // Dispatches to l1d / l2c / llc prefetcher_operate per cache level.
    inline void handle_read_hit_pf_operate(uint16_t read_cpu, int index, uint32_t set, uint32_t way) {
        if (RQ.entry[index].type == LOAD) {
            if (cache_type == IS_L1D)
                l1d_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 1, RQ.entry[index].type);
            else if (cache_type == IS_L2C)
                l2c_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
            else if (cache_type == IS_LLC) {
                cpu = read_cpu;
                llc_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
                cpu = 0;
            }
        }
    }

    // Updates replacement state (LRU) for the hit way.
    inline void handle_read_hit_replacement(uint16_t read_cpu, int index, uint32_t set, uint32_t way) {
        if (cache_type == IS_LLC)
            llc_update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1);
        else
            update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1);
    }

    // Increments sim_hit / sim_access counters, and wByP load counters.
    inline void handle_read_hit_stats(uint16_t read_cpu, int index) {
        sim_hit[read_cpu][RQ.entry[index].type]++;
        sim_access[read_cpu][RQ.entry[index].type]++;
        lpm_shadow_inc(read_cpu, RQ.entry[index].type, false);
        if (RQ.entry[index].type == LOAD) {
            sim_hit_wByP[read_cpu]++;
            sim_access_wByP[read_cpu]++;
        }
    }

    // If fill_level < this cache's fill_level, returns data to the upper level.
    inline void handle_read_hit_return(int index) {
        bool fire = (RQ.entry[index].fill_level < fill_level);
        DP_FILL_M(("HIT_RET_CHK rq.fill=" + std::to_string(RQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level) + " fire=" + (fire?"Y":"N")).c_str(), &RQ.entry[index]);
        if (fire) {
            DP_FWD(&RQ.entry[index],"upper");
            return_to_upper_level(RQ.entry[index]);
        }
    }

    // Increments pf_useful on prefetch hit, clears prefetch bit, sets used=1.
    // Increments HIT/ACCESS counters and removes entry from RQ.
    inline void handle_read_hit_pf_useful_and_remove(int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("HIT_REMOVE pf_used=" + std::string(block[set][way].prefetch?"Y":"N") + " set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &RQ.entry[index]);
        if (block[set][way].prefetch) {
            pf_useful++;
            block[set][way].prefetch = 0;
        }
        block[set][way].used = 1;
        HIT[RQ.entry[index].type]++;
        ACCESS[RQ.entry[index].type]++;
        RQ.remove_queue(&RQ.entry[index]);
        reads_available_this_cycle--;
    }

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
    inline void handle_read_miss_new_llc(int index, uint8_t& miss_handled) {
        DP_FILL_M("NEW_LLC_ENTRY", &RQ.entry[index]);
        if (lower_level->get_occupancy(1, RQ.entry[index].address) == lower_level->get_size(1, RQ.entry[index].address)) {
            DP_FILL_M("NEW_LLC_DRAM_FULL_STALL", &RQ.entry[index]);
            miss_handled = 0;
        } else {
            DP_FILL_M("NEW_LLC_ADD_MSHR", &RQ.entry[index]);
            add_mshr(&RQ.entry[index]);
            DP_MSHR_ADD(&RQ.entry[index], MSHR.occupancy-1);
            if (lower_level) {
                DP_FWD(&RQ.entry[index],"DRAM");
                lower_level->add_rq(&RQ.entry[index]);
            }
        }
    }

    // Handles new non-LLC miss: checks lower-level RQ capacity, calls add_mshr + lower_level->add_rq.
    // Special case: IS_STLB with no lower_level emulates page table walk via va_to_pa.
    // Sets miss_handled=0 if lower RQ is full.
    inline void handle_read_miss_new_other(uint16_t read_cpu, int index, uint8_t& miss_handled) {
        DP_FILL_M("NEW_OTHER_ENTRY", &RQ.entry[index]);
        if (lower_level && lower_level->get_occupancy(1, RQ.entry[index].address) == lower_level->get_size(1, RQ.entry[index].address)) {
            DP_FILL_M("NEW_OTHER_LOWER_FULL_STALL", &RQ.entry[index]);
            miss_handled = 0;
        } else {
            DP_FILL_M("NEW_OTHER_ADD_MSHR", &RQ.entry[index]);
            add_mshr(&RQ.entry[index]);
            DP_MSHR_ADD(&RQ.entry[index], MSHR.occupancy-1);
            if (lower_level) {
                DP_FWD(&RQ.entry[index],"lower");
                lower_level->add_rq(&RQ.entry[index]);
            } else {
                if (cache_type == IS_STLB) {
                    DP_FILL_M("NEW_OTHER_STLB_VA2PA_SELFRETURN", &RQ.entry[index]);
                    uint64_t pa = va_to_pa(read_cpu, RQ.entry[index].instr_id, RQ.entry[index].full_addr, RQ.entry[index].address);
                    RQ.entry[index].data = pa >> LOG2_PAGE_SIZE;
                    RQ.entry[index].event_cycle = PACK_CYCLE(current_core_cycle[read_cpu]);
                    return_data(&RQ.entry[index]);
                }
            }
        }
    }

    // Handles MSHR-full stall: sets miss_handled=0, increments STALL counter.
    inline void handle_read_miss_mshr_full(int index, uint8_t& miss_handled) {
        DP_FILL_M(("MSHR_FULL_STALL type=" + std::to_string(RQ.entry[index].type)).c_str(), &RQ.entry[index]);
        miss_handled = 0;
        STALL[RQ.entry[index].type]++;
        pure_MSHR_Admission_STALL[RQ.entry[index].type]++;
    }

    // Merges dependency indices from in-flight RQ entry into existing MSHR entry.
    // Handles RFO (sq/lq), instruction (rob), and data load (lq+sq) cases.
    inline void handle_read_miss_inflight_merge_deps(int index, int mshr_index) {
        DP_FILL_M(("MERGE_DEPS idx=" + std::to_string(mshr_index) + " rq.type=" + std::to_string(RQ.entry[index].type) + " rq.instr=" + std::to_string(RQ.entry[index].instruction) + " rq.lq=" + std::to_string(RQ.entry[index].lq_index) + " rq.sq=" + std::to_string(RQ.entry[index].sq_index)).c_str(), &RQ.entry[index]);
        if (RQ.entry[index].type == RFO) {
            if (RQ.entry[index].l1_bypassed)
                assert(0&&"RFO BYPASS NOT EXPECTED ... ");
            if (RQ.entry[index].tlb_access) {
                uint16_t sq_index = RQ.entry[index].sq_index;
                MSHR.entry[mshr_index].store_merged = 1;
                MSHR.entry[mshr_index].sq_index_depend_on_me.insert(sq_index);
                MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
            }
            if (RQ.entry[index].load_merged) {
                MSHR.entry[mshr_index].load_merged = 1;
                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
            }
        } else {
            if (RQ.entry[index].instruction) {
                uint16_t rob_index = RQ.entry[index].rob_index;
                MSHR.entry[mshr_index].instr_merged = 1;
                MSHR.entry[mshr_index].rob_index_depend_on_me.insert(rob_index);
                if (RQ.entry[index].instr_merged)
                    MSHR.entry[mshr_index].rob_index_depend_on_me.join(RQ.entry[index].rob_index_depend_on_me, ROB_SIZE);
            } else {
                uint16_t lq_index = RQ.entry[index].lq_index;
                MSHR.entry[mshr_index].load_merged = 1;
                MSHR.entry[mshr_index].lq_index_depend_on_me.insert(lq_index);
                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                if (RQ.entry[index].store_merged) {
                    MSHR.entry[mshr_index].store_merged = 1;
                    MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                }
            }
        }
    }

    // Updates MSHR fill_level if incoming RQ entry has a shallower fill target.
    inline void handle_read_miss_inflight_fill_level(int index, int mshr_index) {
        uint8_t pre_fl = MSHR.entry[mshr_index].fill_level;
        if (RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
            MSHR.entry[mshr_index].fill_level = RQ.entry[index].fill_level;
        DP_FILL_M(("INFLIGHT_FILL_LVL idx=" + std::to_string(mshr_index) + " pre=" + std::to_string(pre_fl) + " rq=" + std::to_string(RQ.entry[index].fill_level) + " post=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " promoted=" + ((pre_fl != MSHR.entry[mshr_index].fill_level)?"Y":"N")).c_str(), &MSHR.entry[mshr_index]);
    }

    // Resolves L1 bypass mismatch at L2C: if l1_bypassed differs between RQ and MSHR,
    // injects LQ deps into L1D MSHR and clears l1_bypassed to use normal fill path.
    inline void handle_read_miss_inflight_bypass_l1_mismatch(int index, int mshr_index) {
#ifdef BYPASS_L1_LOGIC
        if (cache_type == IS_L2C) {
            if (RQ.entry[index].l1_bypassed != MSHR.entry[mshr_index].l1_bypassed) {
                if (MSHR.entry[mshr_index].type != PREFETCH) {
                    auto *l1d = (CACHE *) this->upper_level_dcache[cpu];
                    bool found_l1d_mshr = false;
                    for (uint16_t m = 0; m < l1d->MSHR_SIZE; m++) {
                        if (l1d->MSHR.entry[m].address == MSHR.entry[mshr_index].address) {
                            found_l1d_mshr = true;
                            if (l1d->MSHR.entry[m].type == PREFETCH) {
                                auto& __m = l1d->MSHR.entry[m];
                                if (__m.lq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.lq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (__m.sq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.sq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (!__m.rob_index_depend_on_me.empty())
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.rob_index_depend_on_me.entries.size()
                                              << " **\033[0m\n" << std::flush;
                            }
                            // Only inject load deps for LOAD packets — RFO lq_index is garbage
                            if (RQ.entry[index].type != RFO) {
                                l1d->MSHR.entry[m].load_merged = 1;
                                l1d->MSHR.entry[m].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
                            }
                            if (RQ.entry[index].load_merged) {
                                l1d->MSHR.entry[m].load_merged = 1;
                                ITERATE_SET(dep, RQ.entry[index].lq_index_depend_on_me, LQ_SIZE) {
                                    l1d->MSHR.entry[m].lq_index_depend_on_me.insert(dep);
                                }
                            }
                            if (RQ.entry[index].store_merged) {
                                l1d->MSHR.entry[m].store_merged = 1;
                                l1d->MSHR.entry[m].sq_index_depend_on_me.join(
                                    RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }
                            if (l1d->MSHR.entry[m].type == LOAD)
                                l1d->MSHR.entry[m].lq_index_depend_on_me.remove(l1d->MSHR.entry[m].lq_index);
                            break;
                        }
                    }
                    if (found_l1d_mshr) {
                        RQ.entry[index].l1_bypassed = 0;
                        MSHR.entry[mshr_index].l1_bypassed = 0;
                        MSHR.entry[mshr_index].fill_level = 1;
                    }
                } else if (MSHR.entry[mshr_index].fill_level < fill_level) {
                    RQ.entry[index].l1_bypassed = 0;
                }
            }
        }
#endif
    }

    // Resolves L2 bypass mismatch at LLC: if l2_bypassed differs between RQ and MSHR,
    // injects LQ deps into L2C MSHR and clears l2_bypassed to use normal fill path.
    inline void handle_read_miss_inflight_bypass_l2_mismatch(int index, int mshr_index) {
#ifdef BYPASS_L2_LOGIC
        if (cache_type == IS_LLC) {
            if (RQ.entry[index].l2_bypassed != MSHR.entry[mshr_index].l2_bypassed) {
                if (MSHR.entry[mshr_index].type != PREFETCH) {
                    // [FIX] LLC shared: class member 'cpu' is stale; use packet's cpu for correct L2C target
                    auto *l2c = (CACHE *) this->upper_level_dcache[RQ.entry[index].cpu];
                    bool found_l2c_mshr = false;
                    for (uint16_t m = 0; m < l2c->MSHR_SIZE; m++) {
                        if (l2c->MSHR.entry[m].address == MSHR.entry[mshr_index].address) {
                            found_l2c_mshr = true;
                            if (l2c->MSHR.entry[m].type == PREFETCH) {
                                auto& __m = l2c->MSHR.entry[m];
                                if (__m.lq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.lq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (__m.sq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.sq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (!__m.rob_index_depend_on_me.empty())
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.rob_index_depend_on_me.entries.size()
                                              << " **\033[0m\n" << std::flush;
                            }
                            // Only inject load deps for LOAD packets — RFO lq_index is garbage
                            if (RQ.entry[index].type != RFO) {
                                l2c->MSHR.entry[m].load_merged = 1;
                                l2c->MSHR.entry[m].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
                            }
                            if (RQ.entry[index].load_merged) {
                                l2c->MSHR.entry[m].load_merged = 1;
                                ITERATE_SET(dep, RQ.entry[index].lq_index_depend_on_me, LQ_SIZE) {
                                    l2c->MSHR.entry[m].lq_index_depend_on_me.insert(dep);
                                }
                            }
                            if (RQ.entry[index].store_merged) {
                                l2c->MSHR.entry[m].store_merged = 1;
                                l2c->MSHR.entry[m].sq_index_depend_on_me.join(
                                    RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }
                            if (l2c->MSHR.entry[m].type == LOAD)
                                l2c->MSHR.entry[m].lq_index_depend_on_me.remove(l2c->MSHR.entry[m].lq_index);
                            break;
                        }
                    }
                    if (found_l2c_mshr) {
                        RQ.entry[index].l2_bypassed = 0;
                        MSHR.entry[mshr_index].l2_bypassed = 0;
                        MSHR.entry[mshr_index].fill_level = FILL_L2;
                    } else {
                        std::cout << "\033[1;31m\n** RARE UNHANDLED CASE line=" << __LINE__
                                  << " func=" << __func__
                                  << " cache=[" << NAME << "]"
                                  << " ISSUE: L2C_MSHR_MISSING_BYPASS_MISMATCH"
                                  << " **\033[0m\n" << std::flush;
                    }
                } else if (MSHR.entry[mshr_index].fill_level < fill_level) {
                    RQ.entry[index].l2_bypassed = 0;
                }
            }
        }
#endif
    }

    // Handles late prefetch takeover: increments pf_late, calls merge_with_prefetch,
    // then restores bypass flags from RQ entry onto MSHR (merge_with_prefetch overwrites them).
    inline void handle_read_miss_inflight_prefetch_takeover(int index, int mshr_index) {
        DP_FILL_M(("PF_TAKEOVER_PRE idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " rq.type=" + std::to_string(RQ.entry[index].type) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned)).c_str(), &MSHR.entry[mshr_index]);
        pf_late++;
        merge_with_prefetch(MSHR.entry[mshr_index], RQ.entry[index]);
#ifdef BYPASS_L1_LOGIC
        if (RQ.entry[index].l1_bypassed)
            MSHR.entry[mshr_index].l1_bypassed = 1;
#endif
#ifdef BYPASS_L2_LOGIC
        if (RQ.entry[index].l2_bypassed)
            MSHR.entry[mshr_index].l2_bypassed = 1;
#endif
#ifdef BYPASS_LLC_LOGIC
        if (RQ.entry[index].llc_bypassed)
            MSHR.entry[mshr_index].llc_bypassed = 1;
#endif
        DP_FILL_M(("PF_TAKEOVER_POST idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned)).c_str(), &MSHR.entry[mshr_index]);
    }

    // For non-prefetch in-flight MSHR: inserts rob/lq/sq index into MSHR dep set per packet type.
    inline void handle_read_miss_inflight_non_prefetch_merge(int index, int mshr_index) {
        DP_FILL_M(("NONPF_MERGE idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " rq.type=" + std::to_string(RQ.entry[index].type) + " rq.lq=" + std::to_string(RQ.entry[index].lq_index)).c_str(), &MSHR.entry[mshr_index]);
        if (RQ.entry[index].instruction) {
            MSHR.entry[mshr_index].rob_index_depend_on_me.insert(RQ.entry[index].rob_index);
            MSHR.entry[mshr_index].instr_merged = 1;
        } else if (RQ.entry[index].type == LOAD) {
            MSHR.entry[mshr_index].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
            MSHR.entry[mshr_index].load_merged = 1;
        } else if (RQ.entry[index].type == RFO) {
            MSHR.entry[mshr_index].sq_index_depend_on_me.insert(RQ.entry[index].sq_index);
            MSHR.entry[mshr_index].store_merged = 1;
        }
    }

    // Notifies prefetcher of a read miss on LOAD type.
    inline void handle_read_miss_handled_pf_operate(uint16_t read_cpu, int index) {
        if (RQ.entry[index].type == LOAD) {
            DP_FILL_M("MISS_PF_OPERATE", &RQ.entry[index]);
            if (cache_type == IS_L1D)
                l1d_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type);
            if (cache_type == IS_L2C)
                l2c_prefetcher_operate(RQ.entry[index].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
            if (cache_type == IS_LLC) {
                cpu = read_cpu;
                llc_prefetcher_operate(RQ.entry[index].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
                cpu = 0;
            }
        }
    }

    // Increments MISS/ACCESS counters and removes entry from RQ.
    inline void handle_read_miss_handled_stats_remove(int index) {
        DP_FILL_M("MISS_REMOVE_RQ", &RQ.entry[index]);
        MISS[RQ.entry[index].type]++;
        ACCESS[RQ.entry[index].type]++;
        RQ.remove_queue(&RQ.entry[index]);
        reads_available_this_cycle--;
    }

    // ---------------------------------------------------------------
    // handle_writeback hit-path inline helpers
    // ---------------------------------------------------------------

    // Updates replacement state, collects sim_hit/sim_access, marks block dirty,
    // copies TLB data fields, checks fill_level and returns to upper level, removes from WQ.
    inline void handle_writeback_hit(uint16_t writeback_cpu, int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("WB_HIT set=" + std::to_string(set) + " way=" + std::to_string(way) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &WQ.entry[index]);
        if (cache_type == IS_LLC)
            llc_update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1);
        else
            update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1);
        sim_hit[writeback_cpu][WQ.entry[index].type]++;
        sim_access[writeback_cpu][WQ.entry[index].type]++;
        lpm_shadow_inc(writeback_cpu, WQ.entry[index].type, false);
        block[set][way].dirty = 1;
        if (cache_type == IS_ITLB)
            WQ.entry[index].instruction_pa = block[set][way].data;
        else if (cache_type == IS_DTLB)
            WQ.entry[index].data_pa = block[set][way].data;
        else if (cache_type == IS_STLB)
            WQ.entry[index].data = block[set][way].data;
        if (WQ.entry[index].fill_level < fill_level) {
            DP_FILL_M("WB_HIT_RET_UP", &WQ.entry[index]);
            return_to_upper_level(WQ.entry[index]);
        }
        HIT[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

    // ---------------------------------------------------------------
    // handle_writeback miss-path inline helpers (L1D RFO path)
    // ---------------------------------------------------------------

    // New RFO miss: checks lower RQ capacity, calls add_mshr + lower->add_rq.
    // Sets miss_handled=0 if lower RQ is full.
    inline void handle_writeback_miss_new(int index, uint8_t& miss_handled) {
        DP_FILL_M("WB_MISS_NEW_ENTRY", &WQ.entry[index]);
        if (cache_type == IS_LLC) {
            if (lower_level->get_occupancy(1, WQ.entry[index].address) == lower_level->get_size(1, WQ.entry[index].address)) {
                DP_FILL_M("WB_MISS_NEW_LOWER_FULL", &WQ.entry[index]);
                miss_handled = 0;
            } else {
                DP_FILL_M("WB_MISS_NEW_ADD_MSHR_RQ", &WQ.entry[index]);
                add_mshr(&WQ.entry[index]);
                lower_level->add_rq(&WQ.entry[index]);
            }
        } else {
            if (lower_level && lower_level->get_occupancy(1, WQ.entry[index].address) == lower_level->get_size(1, WQ.entry[index].address)) {
                DP_FILL_M("WB_MISS_NEW_LOWER_FULL", &WQ.entry[index]);
                miss_handled = 0;
            } else {
                DP_FILL_M("WB_MISS_NEW_ADD_MSHR_RQ", &WQ.entry[index]);
                add_mshr(&WQ.entry[index]);
                lower_level->add_rq(&WQ.entry[index]);
            }
        }
    }

    // MSHR full stall for writeback path.
    inline void handle_writeback_miss_mshr_full(int index, uint8_t& miss_handled) {
        miss_handled = 0;
        STALL[WQ.entry[index].type]++;
    }

    // In-flight MSHR merge for writeback: updates fill_level, merges prefetch if needed.
    inline void handle_writeback_miss_inflight(int index, int mshr_index) {
        DP_FILL_M(("WB_INFLIGHT idx=" + std::to_string(mshr_index) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type)).c_str(), &WQ.entry[index]);
        if (WQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
            MSHR.entry[mshr_index].fill_level = WQ.entry[index].fill_level;
        if (MSHR.entry[mshr_index].type == PREFETCH)
            merge_with_prefetch(MSHR.entry[mshr_index], WQ.entry[index]);
        MSHR_MERGED[WQ.entry[index].type]++;
        DP_FILL_M(("WB_INFLIGHT_POST idx=" + std::to_string(mshr_index) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level)).c_str(), &MSHR.entry[mshr_index]);
    }

    // Increments MISS/ACCESS and removes entry from WQ.
    inline void handle_writeback_miss_handled_stats_remove(int index) {
        MISS[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

    // ---------------------------------------------------------------
    // handle_writeback non-L1D miss path (writeback eviction into lower level)
    // ---------------------------------------------------------------

    // Finds victim way for a writeback miss (non-L1D). Returns the way index.
    inline uint32_t handle_writeback_find_victim(uint32_t writeback_cpu, int index, uint32_t set) {
        if (cache_type == IS_LLC)
            return llc_find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);
        return find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);
    }

    // Checks dirty eviction: if block dirty and lower WQ has room, constructs and sends writeback packet.
    // Sets do_fill=0 and increments STALL if lower WQ is full.
    inline void handle_writeback_evict_dirty(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way, uint8_t& do_fill) {
        if (block[set][way].dirty && lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)) {
                do_fill = 0;
                lower_level->increment_WQ_FULL(block[set][way].address);
                STALL[WQ.entry[index].type]++;
            } else {
                PACKET writeback_packet;
                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = writeback_cpu;
                writeback_packet.address = block[set][way].address;
                writeback_packet.full_addr = block[set][way].full_addr;
                writeback_packet.data = block[set][way].data;
                writeback_packet.instr_id = WQ.entry[index].instr_id;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = PACK_CYCLE(current_core_cycle[writeback_cpu]);
                lower_level->add_wq(&writeback_packet);
            }
        }
    }

    // Calls per-type prefetcher fill hook, updates replacement, collects stats,
    // calls fill_cache, marks dirty, checks fill_level, increments MISS/ACCESS, removes from WQ.
    inline void handle_writeback_do_fill(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("WB_DO_FILL set=" + std::to_string(set) + " way=" + std::to_string(way) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &WQ.entry[index]);
        if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
        else if (cache_type == IS_L2C)
            WQ.entry[index].pf_metadata = l2c_prefetcher_cache_fill(WQ.entry[index].address<<LOG2_BLOCK_SIZE, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
        if (cache_type == IS_LLC) {
            cpu = writeback_cpu;
            WQ.entry[index].pf_metadata = llc_prefetcher_cache_fill(WQ.entry[index].address<<LOG2_BLOCK_SIZE, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
            cpu = 0;
        }
        if (cache_type == IS_LLC)
            llc_update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0);
        else
            update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0);
        sim_miss[writeback_cpu][WQ.entry[index].type]++;
        sim_access[writeback_cpu][WQ.entry[index].type]++;
        lpm_shadow_inc(writeback_cpu, WQ.entry[index].type, true);
        fill_cache(set, way, &WQ.entry[index]);
        block[set][way].dirty = 1;
        if (WQ.entry[index].fill_level < fill_level) {
            DP_FILL_M("WB_DO_FILL_RET_UP", &WQ.entry[index]);
            return_to_upper_level(WQ.entry[index]);
        }
        MISS[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

    // ---------------------------------------------------------------
    // handle_fill inline helpers
    // ---------------------------------------------------------------

    // Finds victim way for a fill. Returns the way index.
    inline uint32_t handle_fill_find_victim(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set) {
        if (cache_type == IS_LLC)
            return llc_find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);
        return find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);
    }

    // Checks dirty eviction on fill: constructs writeback packet to lower WQ if dirty block evicted.
    // Sets do_fill=0 and increments STALL if lower WQ is full.
    inline void handle_fill_evict_dirty(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way, uint8_t& do_fill) {
        if (block[set][way].dirty && lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)) {
                DP_FILL_M(("FILL_EVICT_STALL set=" + std::to_string(set) + " way=" + std::to_string(way) + " idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
                do_fill = 0;
                DP( if (DP_GATE_WW(current_core_cycle[fill_cpu], fill_cpu, MSHR.entry[mshr_index].address, MSHR.entry[mshr_index].instr_id) && cache_type == IS_LLC) {
                    cout << "[LLC_handle_fill_ENTRY] do_fill=0 WQ_FULL mshr_idx: " << mshr_index
                         << " addr: 0x" << hex << MSHR.entry[mshr_index].address << dec
                         << " evict_addr: 0x" << hex << block[set][way].address << dec
                         << " type: " << (int)MSHR.entry[mshr_index].type
                         << " cpu: " << (int)MSHR.entry[mshr_index].cpu
                         << " cy: " << current_core_cycle[fill_cpu] << endl;
                });
                lower_level->increment_WQ_FULL(block[set][way].address);
                STALL[MSHR.entry[mshr_index].type]++;
            } else {
                DP_FILL_M(("FILL_EVICT_WB set=" + std::to_string(set) + " way=" + std::to_string(way) + " idx=" + std::to_string(mshr_index) + " evict_addr=0x" + std::to_string(block[set][way].address)).c_str(), &MSHR.entry[mshr_index]);
                PACKET writeback_packet;
                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = fill_cpu;
                writeback_packet.address = block[set][way].address;
                writeback_packet.full_addr = block[set][way].full_addr;
                writeback_packet.data = block[set][way].data;
                writeback_packet.instr_id = MSHR.entry[mshr_index].instr_id;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                lower_level->add_wq(&writeback_packet);
            }
        }
    }

    // Calls per-type prefetcher cache fill hook for L1D/L2C/LLC.
    inline void handle_fill_pf_fill(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(MSHR.entry[mshr_index].full_addr, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
        if (cache_type == IS_L2C)
            MSHR.entry[mshr_index].pf_metadata = l2c_prefetcher_cache_fill(MSHR.entry[mshr_index].address<<LOG2_BLOCK_SIZE, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
        if (cache_type == IS_LLC) {
            cpu = fill_cpu;
            MSHR.entry[mshr_index].pf_metadata = llc_prefetcher_cache_fill(MSHR.entry[mshr_index].address<<LOG2_BLOCK_SIZE, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
            cpu = 0;
        }
    }

    // Updates replacement state for the filled way.
    inline void handle_fill_replacement(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_LLC)
            llc_update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0);
        else
            update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0);
    }

    // Increments sim_miss/sim_access and wByP counters.
    // wByP skipped for llc_bypassed: already counted at bypass-decision time in handle_read_miss_bypass.
    inline void handle_fill_stats(uint16_t fill_cpu, uint16_t mshr_index) {
        sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;
        lpm_shadow_inc(fill_cpu, MSHR.entry[mshr_index].type, true);
        if (MSHR.entry[mshr_index].type == LOAD
#ifdef BYPASS_LLC_LOGIC
            && !MSHR.entry[mshr_index].llc_bypassed
#endif
        ) {
            sim_miss_wByP[fill_cpu]++;
            sim_access_wByP[fill_cpu]++;
        }
    } 

    // Calls fill_cache, marks dirty for L1D RFO.
    inline void handle_fill_cache_and_dirty(uint16_t mshr_index, uint32_t set, uint32_t way) {
        fill_cache(set, way, &MSHR.entry[mshr_index]);
        DP_FILL(MSHR.entry[mshr_index], set, way);
        if (cache_type == IS_L1D && MSHR.entry[mshr_index].type == RFO)
            block[set][way].dirty = 1;
    }

    // If fill_level < this cache's fill_level, returns data to upper level.
    inline void handle_fill_return(uint16_t mshr_index) {
        bool fire = (MSHR.entry[mshr_index].fill_level < fill_level);
        DP( if (DP_GATE_WW(current_core_cycle[MSHR.entry[mshr_index].cpu], MSHR.entry[mshr_index].cpu, MSHR.entry[mshr_index].address, MSHR.entry[mshr_index].instr_id)) {
            cout << "[FILL_RETURN] " << NAME
                 << " mshr.fill_level: " << (int)MSHR.entry[mshr_index].fill_level
                 << " vs cache.fill_level: " << (int)fill_level
                 << " return_to_upper: " << (fire ? "YES" : "NO")
                 << " addr: 0x" << hex << MSHR.entry[mshr_index].address << dec
                 << " type: " << (int)MSHR.entry[mshr_index].type
                 << " cpu: " << (int)MSHR.entry[mshr_index].cpu
                 << " cy: " << current_core_cycle[MSHR.entry[mshr_index].cpu] << endl;
        });
        DP_FILL_M(("FILL_RET_CHK mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " self.fill=" + std::to_string(fill_level) + " fire=" + (fire?"Y":"N") + " idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
        if (fire) {
            DP_FILL_M(("FILL_RET_FIRE idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
            return_to_upper_level(MSHR.entry[mshr_index]);
        } else {
            DP_FILL_M(("FILL_RET_SKIP idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
        }
    }

    // Routes filled packet to PROCESSED queue for TLB/L1I/L1D types,
    // and handles bypass return paths for L2C (l1_bypassed→PROCESSED) and LLC (l2_bypassed→L1D).
    inline void handle_fill_processed_and_bypass_return(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_ITLB) {
            MSHR.entry[mshr_index].instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "ITLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_DTLB) {
            MSHR.entry[mshr_index].data_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "DTLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_L1I) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L1I PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if ((cache_type == IS_L1D) && (MSHR.entry[mshr_index].type != PREFETCH)) {
#ifdef DEBUG_PRINT
            if (DP_GATE_WW(current_core_cycle[fill_cpu], fill_cpu, MSHR.entry[mshr_index].address, MSHR.entry[mshr_index].instr_id)) {
                cout << "[L1D_PROC_ADD] iid:" << MSHR.entry[mshr_index].instr_id
                     << " addr:0x" << hex << MSHR.entry[mshr_index].address << dec
                     << " lq:" << MSHR.entry[mshr_index].lq_index
                     << " card:" << MSHR.entry[mshr_index].lq_index_depend_on_me.card
                     << " bits[0]:0x" << hex << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[0]
                     << " bits[1]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[1]
                     << " bits[2]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[2]
                     << " bits[3]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[3] << dec
                     << endl;
            }
#endif
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L1D PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
#ifdef BYPASS_L1_LOGIC
        else if ((cache_type == IS_L2C) && (MSHR.entry[mshr_index].type == LOAD) && MSHR.entry[mshr_index].l1_bypassed == 1) {
            // L1 bypass still active (no prefetch cleared it) — CPU gets data via L2C PROCESSED
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L2C PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
#ifdef BYP_DERFILL_ACTIVE
            // DERIVATIVE FILL: L2C filled, L1D was bypassed. Fill L1D directly.
            {
                CACHE* l1d = (CACHE*)upper_level_dcache[fill_cpu];
                PACKET df_pkt = MSHR.entry[mshr_index];
                df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                {
                    uint32_t df_set = l1d->get_set(df_pkt.address);
                    uint32_t df_way = l1d->find_victim(fill_cpu, df_pkt.instr_id, df_set, l1d->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                    if (l1d->block[df_set][df_way].valid && l1d->block[df_set][df_way].dirty) {
                        if (l1d->lower_level->get_occupancy(2, l1d->block[df_set][df_way].address) < l1d->lower_level->get_size(2, l1d->block[df_set][df_way].address)) {
                            PACKET wb_pkt;
                            wb_pkt.fill_level = FILL_L2;
                            wb_pkt.cpu = fill_cpu;
                            wb_pkt.address = l1d->block[df_set][df_way].address;
                            wb_pkt.full_addr = l1d->block[df_set][df_way].full_addr;
                            wb_pkt.data = l1d->block[df_set][df_way].data;
                            wb_pkt.instr_id = df_pkt.instr_id;
                            wb_pkt.ip = 0;
                            wb_pkt.type = WRITEBACK;
                            wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                            l1d->lower_level->add_wq(&wb_pkt);
                        }
                    }
                    l1d_prefetcher_cache_fill(df_pkt.full_addr, df_set, df_way, (df_pkt.type == PREFETCH) ? 1 : 0, l1d->block[df_set][df_way].address<<LOG2_BLOCK_SIZE, df_pkt.pf_metadata);
                    l1d->fill_cache(df_set, df_way, &df_pkt);
                    l1d->update_replacement_state(fill_cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, l1d->block[df_set][df_way].full_addr, df_pkt.type, 0);
                    if (df_pkt.type == RFO)
                        l1d->block[df_set][df_way].dirty = 1;
                }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                // Inject completed MSHR at L1D — fills in L1D_LATENCY cycles
                {
                    df_pkt.returned = COMPLETED;
                    df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu] + l1d->LATENCY);
                    df_pkt.fill_level = FILL_L1;
                    df_pkt.type = PREFETCH;
                    int mshr_idx = l1d->check_mshr(&df_pkt);
                    if (mshr_idx == -1
                        && l1d->MSHR.occupancy < l1d->MSHR.SIZE
                        && !(l1d->MSHR.occupancy && (l1d->MSHR.head == l1d->MSHR.tail))) {
                        if (df_pkt.lq_index_depend_on_me.card != 0
                            || df_pkt.sq_index_depend_on_me.card != 0
                            || !df_pkt.rob_index_depend_on_me.empty()) {
                            auto& __m = df_pkt;
                            if (__m.lq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.lq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (__m.sq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.sq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (!__m.rob_index_depend_on_me.empty())
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.rob_index_depend_on_me.size()
                                          << " **\033[0m\n" << std::flush;
                        }
                        l1d->MSHR.add_queue(&df_pkt);
                        l1d->MSHR.num_returned++;
                        l1d->update_fill_cycle();
                    }
                    // else: MSHR full or duplicate — skip (best-effort)
                }
#endif
            }
#endif
        }
#endif
#ifdef BYPASS_L2_LOGIC
        else if ((cache_type == IS_LLC) && (MSHR.entry[mshr_index].type == LOAD) && MSHR.entry[mshr_index].l2_bypassed == 1) {
            // L2 bypass active — skip L2C, return directly to L1D
            upper_level_dcache[fill_cpu]->upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
            // PF from L2C merged into this bypassed MSHR — complete L2C's prefetch MSHR.
            if (MSHR.entry[mshr_index].pf_merged_from_upper
                    && ((CACHE *)upper_level_dcache[fill_cpu])->probe_mshr(&MSHR.entry[mshr_index]) != -1) {
                return_to_upper_level(MSHR.entry[mshr_index]);
            }
#ifdef BYP_DERFILL_ACTIVE
            // DERIVATIVE FILL: LLC filled, L2C was bypassed. Fill L2C directly.
            {
                CACHE* l2c = (CACHE*)upper_level_dcache[fill_cpu];
                PACKET df_pkt = MSHR.entry[mshr_index];
                df_pkt.l2_bypassed = 0;
                df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                {
                    uint32_t df_set = l2c->get_set(df_pkt.address);
                    uint32_t df_way = l2c->find_victim(fill_cpu, df_pkt.instr_id, df_set, l2c->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                    if (l2c->block[df_set][df_way].valid && l2c->block[df_set][df_way].dirty) {
                        if (l2c->lower_level->get_occupancy(2, l2c->block[df_set][df_way].address) < l2c->lower_level->get_size(2, l2c->block[df_set][df_way].address)) {
                            PACKET wb_pkt;
                            wb_pkt.fill_level = FILL_LLC;
                            wb_pkt.cpu = fill_cpu;
                            wb_pkt.address = l2c->block[df_set][df_way].address;
                            wb_pkt.full_addr = l2c->block[df_set][df_way].full_addr;
                            wb_pkt.data = l2c->block[df_set][df_way].data;
                            wb_pkt.instr_id = df_pkt.instr_id;
                            wb_pkt.ip = 0;
                            wb_pkt.type = WRITEBACK;
                            wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                            l2c->lower_level->add_wq(&wb_pkt);
                        }
                    }
                    l2c_prefetcher_cache_fill(df_pkt.address<<LOG2_BLOCK_SIZE, df_set, df_way, (df_pkt.type == PREFETCH) ? 1 : 0, l2c->block[df_set][df_way].address<<LOG2_BLOCK_SIZE, df_pkt.pf_metadata);
                    l2c->fill_cache(df_set, df_way, &df_pkt);
                    l2c->update_replacement_state(fill_cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, l2c->block[df_set][df_way].full_addr, df_pkt.type, 0);
                }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                // Inject completed MSHR at L2C — fills in L2C_LATENCY cycles
                {
                    df_pkt.returned = COMPLETED;
                    df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu] + l2c->LATENCY);
                    df_pkt.fill_level = FILL_L2;
                    df_pkt.type = PREFETCH;
                    int mshr_idx = l2c->check_mshr(&df_pkt);
                    if (mshr_idx == -1
                        && l2c->MSHR.occupancy < l2c->MSHR.SIZE
                        && !(l2c->MSHR.occupancy && (l2c->MSHR.head == l2c->MSHR.tail))) {
                        if (df_pkt.lq_index_depend_on_me.card != 0
                            || df_pkt.sq_index_depend_on_me.card != 0
                            || !df_pkt.rob_index_depend_on_me.empty()) {
                            auto& __m = df_pkt;
                            if (__m.lq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.lq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (__m.sq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.sq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (!__m.rob_index_depend_on_me.empty())
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.rob_index_depend_on_me.size()
                                          << " **\033[0m\n" << std::flush;
                        }
                        l2c->MSHR.add_queue(&df_pkt);
                        l2c->MSHR.num_returned++;
                        l2c->update_fill_cycle();
                    }
                }
#endif
            }
#endif
        }
#endif
    }

    // Updates miss latency stats, removes from MSHR, decrements num_returned, calls update_fill_cycle.
    inline void handle_fill_remove(uint16_t fill_cpu, uint16_t mshr_index) {
        DP_FILL_M(("REMOVE idx=" + std::to_string(mshr_index) + " nret=" + std::to_string(MSHR.num_returned)).c_str(), &MSHR.entry[mshr_index]);
        // LPM counter: MSHR removal (normal fill path)
        {
            bool _byp = false;
#ifdef BYPASS_L1_LOGIC
            if (cache_type == IS_L2C && MSHR.entry[mshr_index].l1_bypassed) _byp = true;
#endif
#ifdef BYPASS_L2_LOGIC
            if (cache_type == IS_LLC && MSHR.entry[mshr_index].l2_bypassed) _byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
            if (cache_type == IS_LLC && MSHR.entry[mshr_index].llc_bypassed) _byp = true;
#endif
            if (_byp) { lpm_mshr_byp_count--; lpm_mshr_byp_per_cpu[fill_cpu]--; }
            if (MSHR.entry[mshr_index].returned == INFLIGHT) {
                lpm_mshr_inflight_count--;
                lpm_mshr_inflight_per_cpu[fill_cpu]--;
            }
            lpm_mshr_occ_per_cpu[fill_cpu]--;
        }
        if (warmup_complete[fill_cpu]) {
            uint64_t current_miss_latency = current_core_cycle[fill_cpu] - MSHR.entry[mshr_index].cycle_enqueued;
            total_miss_latency += current_miss_latency;
        }
        uint64_t removed_addr = MSHR.entry[mshr_index].address;
        MSHR.remove_queue(&MSHR.entry[mshr_index]);
        MSHR.num_returned--;
#ifdef USE_LLC_HASHMAP_MSHR
        if (cache_type == IS_LLC)
            mshr_map.erase(removed_addr);
        else
#endif
            bloom_rebuild_mshr();
        update_fill_cycle();
    }

    uint32_t get_set(uint64_t address) const {
        return (uint32_t)(address & set_mask);
    }
    uint32_t get_way(uint64_t address, uint32_t set) const,
             find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type),
             llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type),
             lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type);
};

#endif
