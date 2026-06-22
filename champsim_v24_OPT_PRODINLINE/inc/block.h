#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"
#include "instruction.h"
#include "set.h"
#ifdef USE_HERMES
#include "hermes/offchip_pred_base_helper.h"
#endif
 
// extra
#include <ostream>
#include <stdlib.h>

#ifndef _WIN32
#include <execinfo.h>   // glibc backtrace — Linux only (used only by commented-out code below)
#include <dlfcn.h>      // Linux only
#endif
#include <cxxabi.h>
#include <cstdio>
#include <cstdlib>

// CACHE BLOCK
class BLOCK
{
public:
    uint8_t valid : 2,
        prefetch : 2,
        dirty : 2,
        used : 2;
    uint8_t l1_bypassed : 1;
    uint8_t l2_bypassed  : 1;
    uint8_t llc_bypassed  : 1;
    // int delta,
    //     depth,
    //     signature,
    //     confidence;

    uint64_t address,
        full_addr,
        tag,
        data;
        // DEAD-2026-05-25: instr_id — zero reads in src/inc/replacement/prefetcher/branch/tracer
        // uint64_t instr_id;
    // DEAD-2026-05-25: cpu — zero reads (current_set[].cpu/block[][].cpu zero hits)
    // int cpu;

    // replacement state
    uint32_t lru;
    uint32_t pmc;

    BLOCK() {
        valid = 0;
        prefetch = 0;
        dirty = 0;
        used = 0;

        // delta = 0;
        // depth = 0;
        // signature = 0;
        // confidence = 0;

        l1_bypassed = 0;
        l2_bypassed = 0;
        llc_bypassed = 0;

        address = 0;
        full_addr = 0;
        tag = 0;
        data = 0;
        // DEAD-2026-05-25: cpu init
        // cpu = 0;
        // DEAD-2026-05-25: instr_id init
        // instr_id = 0;

        lru = 0;
        pmc = 0;
    };
};

// DRAM CACHE BLOCK
class DRAM_ARRAY
{
public:
    BLOCK **block;

    DRAM_ARRAY() {
        block = NULL;
    };
};

// message packet
#include "svector.h"
#define SMALL_VECTOR_SIZE 32

// S8: ROB dependency set — order-FREE, heap-FREE.
// Old impl: backing std::vector<uint16_t> with sorted insert (O(n) shift + per-PACKET heap alloc).
// The only readers iterate ALL entries regardless of order (ooo_cpu reg/instr merge,
// cache_helper dumps) and check empty()/size(). Order is irrelevant -> replace with a
// fixed-size ROB_SIZE-bit bitset stored inline (no heap). insert = set bit (O(1)),
// join = word-wise OR (O(words)), iterate = ctz over words (ascending indices, same
// observable order as old sorted vector). IPC-safe: identical membership semantics.
struct RobDepSet {
    static constexpr size_t NUM_WORDS = (ROB_SIZE + 63) / 64;
    uint64_t bits[NUM_WORDS];

    RobDepSet() { for (size_t w = 0; w < NUM_WORDS; ++w) bits[w] = 0; }

    void insert(uint16_t val) { bits[val >> 6] |= (uint64_t(1) << (val & 63)); }
    void join(const RobDepSet& other, int) {
        for (size_t w = 0; w < NUM_WORDS; ++w) bits[w] |= other.bits[w];
    }
    void clear() { for (size_t w = 0; w < NUM_WORDS; ++w) bits[w] = 0; }
    bool empty() const {
        for (size_t w = 0; w < NUM_WORDS; ++w) if (bits[w]) return false;
        return true;
    }
    size_t size() const {
        size_t n = 0;
        for (size_t w = 0; w < NUM_WORDS; ++w) n += __builtin_popcountll(bits[w]);
        return n;
    }

    // Forward iterator over set bit indices (ascending), yielding uint16_t by value.
    struct const_iterator {
        const uint64_t* bits;
        size_t word;
        uint64_t cur;   // remaining bits in current word
        void advance() {
            while (cur == 0) {
                if (++word >= NUM_WORDS) return;   // word == NUM_WORDS marks end
                cur = bits[word];
            }
        }
        const_iterator(const uint64_t* b, size_t w) : bits(b), word(w), cur(0) {
            if (word < NUM_WORDS) { cur = bits[word]; advance(); }
        }
        uint16_t operator*() const {
            return static_cast<uint16_t>(word * 64 + __builtin_ctzll(cur));
        }
        const_iterator& operator++() {
            cur &= (cur - 1);   // clear lowest set bit
            advance();
            return *this;
        }
        bool operator!=(const const_iterator& o) const {
            return word != o.word || cur != o.cur;
        }
    };
    const_iterator begin() const { return const_iterator(bits, 0); }
    const_iterator end()   const { return const_iterator(bits, NUM_WORDS); }
};

class alignas(64) PACKET
{
public:
    // AddressProxy declared first so AddressProxy-typed fields can be hoisted to line 0.
    // AddressProxy with null pointer safety
    struct AddressProxy
    {
        // PKTSLIM: removed write-only `tmpAddrProxy` mirror (8B). It was assigned
        // in operator= but never read for correctness (only a commented-out debug
        // line referenced it). Dropping it shrinks AddressProxy 32B->24B, saving
        // 16B per PACKET (address + full_addr) with identical read/write semantics
        // — the live value still lives in vec_ptr/fallback_value. IPC-safe.
        ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *vec_ptr;
        uint32_t index;
        mutable uint64_t fallback_value; // For when vec_ptr is null

        AddressProxy() : vec_ptr(nullptr), index(0), fallback_value(0) {}
        AddressProxy(ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *ptr, uint32_t idx) : vec_ptr(ptr), index(idx), fallback_value(0) {}

        // Safe access with null checks
        operator uint64_t &() {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value;
            return (*vec_ptr)[index];
        }
        operator const uint64_t &() const
        {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value;
            return (*vec_ptr)[index];
        }

        AddressProxy &operator=(uint64_t val) {
            if (vec_ptr && index < vec_ptr->size())
                (*vec_ptr)[index] = val;
            else
                fallback_value = val;
            return *this;
        }
        AddressProxy &operator=(const AddressProxy &other) {

            if (vec_ptr && index < vec_ptr->size() && other.vec_ptr && other.index < other.vec_ptr->size()) {
                (*vec_ptr)[index] = (*other.vec_ptr)[other.index];
            }
            else if (other.vec_ptr && other.index < other.vec_ptr->size()) {
                fallback_value = (*other.vec_ptr)[other.index];
            }
            else
            {
                fallback_value = other.fallback_value;
            }
            return *this;
        }

        bool operator==(uint64_t val) const
        {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value == val;
            return (*vec_ptr)[index] == val;
        }
        bool operator!=(uint64_t val) const
        {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value != val;
            return (*vec_ptr)[index] != val;
        }
        bool operator==(int val) const
        {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value == static_cast<uint64_t>(val);
            return (*vec_ptr)[index] == static_cast<uint64_t>(val);
        }
        bool operator!=(int val) const
        {
            if (!vec_ptr || index >= vec_ptr->size())
                return fallback_value != static_cast<uint64_t>(val);
            return (*vec_ptr)[index] != static_cast<uint64_t>(val);
        }
    }; //__attribute__((packed));

    // ========================================================
    // PACKET layout — Design 3 reorder (hot fields → line 0)
    //
    // Line 0 (bytes 0..63): hot subset touched on every CHECK_MSHR
    //   path. ordered: AddressProxy address (32B), event_cycle (8B),
    //   bitfield bursts incl. fill_level/type/returned/*_bypassed,
    //   cpu, lq/sq_index, pf_origin_level/rob_index, instr_id (8B).
    //
    // Line 1+ (bytes 64..): cold-ish or write-side-only fields.
    //   ip, cycle_enqueued, pf_metadata, asid, data_index,
    //   instruction_pa, data_pa, data, dep-sets, AddressProxy full_addr.
    //
    // INVARIANT: AddressProxy fields contain `vec_ptr` pointers; they
    // must NEVER be naively memcpy'd between PACKETs because the
    // pointer would survive across queues. fast_copy_packet below
    // explicitly preserves dest's AddressProxy state across the bulk
    // memcpy.
    // ========================================================

    // ---- HOT (line 0, ≤64B) ----
    AddressProxy address;                                                                                                  // 32B  (0..31)
    uint64_t event_cycle;                                                                                                  //  8B  (32..39)
    // uint8 bitfield burst 1 (kept grouped to preserve original packing):
    uint8_t instruction : 2, tlb_access : 2, scheduled : 2, translated : 2, fetched : 2, prefetched : 2, drc_tag_read : 2, fill_level : 5;
    // uint8 bitfield burst 2:
    uint8_t is_producer : 2, instr_merged : 2, load_merged : 2, store_merged : 2, returned : 2, type : 2;
    // uint8 bitfield burst 3 (bypass flags):
    uint8_t l1_bypassed : 1;
    uint8_t l2_bypassed : 1;
    uint8_t llc_bypassed : 1;
    uint8_t pf_merged_from_upper = 0;                                                                                      //  1B
    uint8_t cpu;                                                                                                           //  1B
    uint16_t lq_index : 12, sq_index : 12;                                                                                 //  4B (uint16 storage)
    int pf_origin_level : 4, rob_index : 12;                                                                               //  4B (int storage)
    uint64_t instr_id : 34;                                                                                                //  8B (uint64 storage; ip moved below)

    // ---- COLD / write-side (line 1+) ----
    uint64_t ip : 48;                                                                                                      //  8B (own uint64 storage)
    uint64_t cycle_enqueued;
    uint32_t pf_metadata;
    uint8_t asid[2];
    uint32_t data_index;
    uint64_t instruction_pa;
    uint64_t data_pa;
    uint64_t data;
#ifdef USE_HERMES
    uint16_t rob_position;
    uint8_t went_offchip_pred;
#endif

    // Original dep-sets — names/types unchanged.
    RobDepSet rob_index_depend_on_me;
    LQ_fastset lq_index_depend_on_me;
    SQ_fastset sq_index_depend_on_me;

    // full_addr was previously paired with address; moved to cold region.
    AddressProxy full_addr;

    void set_queue_vectors(ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *addr_vec, ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *full_addr_vec, uint32_t idx) {
        address = AddressProxy(addr_vec, idx);
        full_addr = AddressProxy(full_addr_vec, idx);
    }

    inline void fast_copy_packet(PACKET &dest, const PACKET &src) {
        // Save dest's AddressProxy queue bindings — bulk memcpy below
        // would otherwise clobber `vec_ptr`/`index` to point at src's
        // queue storage, corrupting subsequent address writes.
        ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *saved_addr_vec      = dest.address.vec_ptr;
        uint32_t                                      saved_addr_idx      = dest.address.index;
        uint64_t                                      saved_addr_fallback = dest.address.fallback_value;
        ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> *saved_full_vec      = dest.full_addr.vec_ptr;
        uint32_t                                      saved_full_idx      = dest.full_addr.index;
        uint64_t                                      saved_full_fallback = dest.full_addr.fallback_value;

        memcpy((void*)&dest, (void*)&src, offsetof(PACKET, rob_index_depend_on_me));

        // Restore dest's AddressProxy queue binding.
        dest.address.vec_ptr        = saved_addr_vec;
        dest.address.index          = saved_addr_idx;
        dest.address.fallback_value = saved_addr_fallback;
        dest.full_addr.vec_ptr      = saved_full_vec;
        dest.full_addr.index        = saved_full_idx;
        dest.full_addr.fallback_value = saved_full_fallback;

        dest.rob_index_depend_on_me = src.rob_index_depend_on_me;
        fast_copy_fastset(dest.lq_index_depend_on_me, src.lq_index_depend_on_me);
        fast_copy_fastset(dest.sq_index_depend_on_me, src.sq_index_depend_on_me);
        dest.address = src.address;
        dest.full_addr = src.full_addr;
        dest.l1_bypassed = src.l1_bypassed;
        dest.l2_bypassed = src.l2_bypassed;
        dest.llc_bypassed = src.llc_bypassed;
        dest.pf_merged_from_upper = src.pf_merged_from_upper;
        dest.fill_level = src.fill_level;
    }
    template <typename T>
    inline void fast_copy_fastset(T &dest, const T &src) {
        // inline void fast_copy_fastset(fastset& dest, const fastset& src) {
        if (src.card == 0)
            return;
        dest.card = src.card;
        // LQ_SMALL_SIZE=8 is the bitset transition threshold; SMALL_SIZE=13 is wrong here
        if (src.card < LQ_SMALL_SIZE) {
            memcpy(dest.data.values, src.data.values, sizeof(TYPE) * src.card);
        }
        else
        {
            memcpy(dest.data.bits, src.data.bits, sizeof(src.data.bits));
        }
    }

    void quickReset() {
        instruction = tlb_access = scheduled = translated = fetched = prefetched = drc_tag_read = returned = 0; // DEAD-2026-05-25 drc_tag_read kept for layout-stable init
        asid[0] = asid[1] = UINT8_MAX;
        type = 0;
        fill_level = rob_index = -1;
        pf_metadata = 0;
        // VB: used for bypass cache level
        l1_bypassed = 0;
        l2_bypassed = 0;
        llc_bypassed = 0;
        pf_merged_from_upper = 0;
        // DEAD-2026-05-25: exist_lvls write
        // exist_lvls = 0;

        rob_index_depend_on_me.clear();
        lq_index_depend_on_me = LQ_template_fastset;
        sq_index_depend_on_me = SQ_template_fastset;
        is_producer = instr_merged = load_merged = store_merged = 0;
        cpu = NUM_CPUS;
        data_index = lq_index = sq_index = 0;
        address = full_addr = instruction_pa = data_pa = data = instr_id = ip = cycle_enqueued = 0;
        event_cycle = CYC_PACKED_MAX;

    }

    PACKET() : address(), full_addr() {
        instruction = tlb_access = scheduled = translated = fetched = prefetched = drc_tag_read = returned = 0; // DEAD-2026-05-25 drc_tag_read kept for layout-stable init
        asid[0] = asid[1] = UINT8_MAX;
        type = 0;
        fill_level = rob_index = -1;
        l1_bypassed = l2_bypassed = llc_bypassed = 0;
        is_producer = instr_merged = load_merged = store_merged = 0;
        cpu = NUM_CPUS;
        data_index = lq_index = sq_index = 0;
        instruction_pa = data = instr_id = ip = 0;
        event_cycle = CYC_PACKED_MAX;
        cycle_enqueued = 0;
    }
    friend std::ostream& operator<<(std::ostream&, const PACKET&);
    // inline std::ostream& operator<<(std::ostream& os, const PACKET& p)
    // {
    //     os
    //         << " InstrID " << (uint64_t)p.instr_id
    //         << " cy "      << (uint64_t)p.event_cycle
    //         << " ROB "     << (int)p.rob_index
    //         << " LQ "      << (int)p.lq_index
    //         << " SQ "      << (int)p.sq_index
    //         << " fill "    << (int)p.fill_level
    //         << " addr "    << (uint64_t)p.address      // DECIMAL, NOT HEX
    //         << " ByP "     << (int)p.l1_bypassed << "/" << (int)p.l2_bypassed << "/" << (int)p.llc_bypassed
    //         << " type "    << (int)p.type
    //         << " ret "     << (int)p.returned
    //         << " currCy "  << (uint64_t)current_cycle[p.cpu];
    //
    //     return os;
    // }



    // static const char* demangle(const char* name)
    // {
    //     int status = 0;
    //     char* dem = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    //     return (status == 0 && dem) ? dem : name;
    // }
    // __attribute__((noinline))
    // PACKET* operator&()
    // {
    //     if (instr_id < 1074740 || instr_id > 10780367912) {
    //         return this;
    //     }
    //     uint64_t pAddr = address.tmpAddrProxy;
    //
    //     void* stack[8];
    //     int n = backtrace(stack, 8);
    //
    //     const char* caller = "unknown";
    //
    //     if (n > 2) {
    //         Dl_info info;
    //         if (dladdr(stack[2], &info) && info.dli_sname) {
    //             caller = demangle(info.dli_sname);
    //         }
    //     }
    //
    //     fprintf(stdout,
    //         "[%s]"
    //         " InstrID %lu"
    //         " cy %lu"
    //         "pAddr %lu"
    //         " ROB %d"
    //         " LQ %d"
    //         " SQ %d"
    //         " fill %d"
    //         " ByP %d"
    //         " type %d"
    //         " ret %d\n",
    //         caller,
    //         (unsigned long)instr_id,
    //         (unsigned long)event_cycle,
    //         (unsigned long) pAddr,
    //         (int)rob_index,
    //         (int)lq_index,
    //         (int)sq_index,
    //         (int)fill_level,
    //         (int)l1_bypassed, (int)l2_bypassed, (int)llc_bypassed,
    //         (int)type,
    //         (int)returned
    //     );
    //
    //     fflush(stdout);
    //     return this;
    // }

    // PACKET* operator&()
    // {
    //     void* stack[4];
    //     int n = backtrace(stack, 4);
    //     char** syms = backtrace_symbols(stack, n);
    //
    //     const char* caller =
    //         (n > 1 && syms && syms[1]) ? syms[1] : "unknown";
    //
    //     fprintf(stdout,
    //         "[%s]"
    //         " InstrID %lu"
    //         " cy %lu"
    //         " ROB %d"
    //         " LQ %d"
    //         " SQ %d"
    //         " fill %d"
    //         " ByP %d"
    //         " type %d"
    //         " ret %d\n",
    //         caller,
    //         (unsigned long)instr_id,
    //         (unsigned long)event_cycle,
    //         (int)rob_index,
    //         (int)lq_index,
    //         (int)sq_index,
    //         (int)fill_level,
    //         (int)l1_bypassed, (int)l2_bypassed, (int)llc_bypassed,
    //         (int)type,
    //         (int)returned
    //     );
    //
    //     free(syms);
    //     return this;
    // }


};
inline std::ostream& operator<<(std::ostream& os, const PACKET& p) {
    os
        << " InstrID " << (uint64_t)p.instr_id
        << " cy "      << (uint64_t)p.event_cycle
        << " ROB "     << (int)p.rob_index
        << " LQ "      << (int)p.lq_index
        << " SQ "      << (int)p.sq_index
        << " fill "    << (int)p.fill_level
        << " addr "    << (uint64_t)p.address
        << " ByP "     << (int)p.l1_bypassed << "/" << (int)p.l2_bypassed << "/" << (int)p.llc_bypassed
        << " type "    << (int)p.type
        << " ret "     << (int)p.returned;
        // << " currCy "  << (uint64_t)current_cycle[p.cpu];

    return os;
}


class PACKET_QUEUE
{
public:
    string NAME;
    const uint16_t SIZE;
    uint8_t is_RQ : 4, is_WQ : 4, write_mode : 4;
    uint8_t cpu;
    uint16_t head, tail, occupancy, num_returned, next_fill_index, next_schedule_index, next_process_index;
    uint64_t next_fill_cycle, next_schedule_cycle, next_process_cycle;
    uint64_t ACCESS, FORWARD, MERGED, TO_CACHE, ROW_BUFFER_HIT, ROW_BUFFER_MISS, FULL;
    PACKET *entry;
    // uint8_t cache_type
    // NEW: SOA vectors for cache-friendly address access
    ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> address_vector;
    ankerl::svector<uint64_t, SMALL_VECTOR_SIZE> full_addr_vector;

    // PACKET_QUEUE(string v1, uint32_t v2) : NAME(v1), SIZE(v2){
    //     is_RQ = is_WQ = write_mode = 0;
    //     cpu = head = tail = occupancy = num_returned = next_fill_index = next_schedule_index = next_process_index = 0;
    //     next_fill_cycle = next_schedule_cycle = next_process_cycle = UINT64_MAX;
    //     ACCESS = FORWARD = MERGED = TO_CACHE = ROW_BUFFER_HIT = ROW_BUFFER_MISS = FULL = 0;

    //     // Allocate and initialize vectors FIRST
    //     address_vector.reserve(SIZE);
    //     full_addr_vector.reserve(SIZE);
    //     for (size_t i = 0; i < SIZE; i++)
    //     {
    //         address_vector.emplace_back(0);
    //         full_addr_vector.emplace_back(0);
    //     }

    //     // THEN allocate PACKET array
    //     entry = new PACKET[SIZE];

    //     // FINALLY set queue vectors for each packet
    //     for (size_t i = 0; i < SIZE; i++)
    //     {
    //         entry[i].set_queue_vectors(&address_vector, &full_addr_vector, i);
    //     }
    // };
    // PACKET_QUEUE(const string& v1, uint32_t v2)
    // : NAME(v1.c_str()), SIZE(v2)
//     PACKET_QUEUE(const std::string& v1, uint32_t v2) : NAME(v1.c_str()), SIZE(v2)
// {
//     is_RQ = is_WQ = write_mode = 0;
//     cpu = head = tail = occupancy = num_returned = next_fill_index = next_schedule_index = next_process_index = 0;
//     next_fill_cycle = next_schedule_cycle = next_process_cycle = UINT64_MAX;
//     ACCESS = FORWARD = MERGED = TO_CACHE = ROW_BUFFER_HIT = ROW_BUFFER_MISS = FULL = 0;

//     address_vector.reserve(SIZE);
//     full_addr_vector.reserve(SIZE);
//     for (size_t i = 0; i < SIZE; i++) {
//         address_vector.emplace_back(0);
//         full_addr_vector.emplace_back(0);
//     }

//     entry = new PACKET[SIZE];

//     for (size_t i = 0; i < SIZE; i++)
//         entry[i].set_queue_vectors(&address_vector, &full_addr_vector, i);
// };
PACKET_QUEUE(const std::string& v1, uint32_t v2) : NAME(v1), SIZE(v2)  // CHANGE: v1 instead of v1.c_str()
{
    is_RQ = is_WQ = write_mode = 0;
    cpu = head = tail = occupancy = num_returned = next_fill_index = next_schedule_index = next_process_index = 0;
    next_fill_cycle = next_schedule_cycle = next_process_cycle = CYC_PACKED_MAX;
    ACCESS = FORWARD = MERGED = TO_CACHE = ROW_BUFFER_HIT = ROW_BUFFER_MISS = FULL = 0;

    address_vector.reserve(SIZE);
    full_addr_vector.reserve(SIZE);
    for (size_t i = 0; i < SIZE; i++) {
        address_vector.emplace_back(0);
        full_addr_vector.emplace_back(0);
    }

    entry = new PACKET[SIZE];

    for (size_t i = 0; i < SIZE; i++)
        entry[i].set_queue_vectors(&address_vector, &full_addr_vector, i);
}
PACKET_QUEUE() :
    NAME(""), SIZE(0),
    is_RQ(0), is_WQ(0), write_mode(0),
    cpu(0), head(0), tail(0), occupancy(0), num_returned(0),
    next_fill_index(0), next_schedule_index(0), next_process_index(0),
    next_fill_cycle(CYC_PACKED_MAX), next_schedule_cycle(CYC_PACKED_MAX), next_process_cycle(CYC_PACKED_MAX),
    ACCESS(0), FORWARD(0), MERGED(0), TO_CACHE(0), ROW_BUFFER_HIT(0), ROW_BUFFER_MISS(0), FULL(0),
    entry(nullptr)
{}

    // PACKET_QUEUE()
    // {
    //     is_RQ = is_WQ = 0;
    //     cpu = head = tail = occupancy = num_returned = next_fill_index = next_schedule_index = next_process_index = 0;
    //     next_fill_cycle = next_schedule_cycle = next_process_cycle = UINT64_MAX;
    //     ACCESS = FORWARD = MERGED = TO_CACHE = ROW_BUFFER_HIT = ROW_BUFFER_MISS = FULL = 0;
    //     entry = nullptr;
    // };

    ~PACKET_QUEUE() {
        delete[] entry;
    };
    void quick_reset() {
        // Reset ALL queue state
        head = tail = occupancy = 0;
        num_returned = 0;

        // CRITICAL: Reset scheduler indices and cycles
        next_fill_index = 0;
        next_schedule_index = 0;
        next_process_index = 0;
        next_fill_cycle = CYC_PACKED_MAX;
        next_schedule_cycle = CYC_PACKED_MAX;
        next_process_cycle = CYC_PACKED_MAX;

        // // Zero vectors
        // for(uint32_t i = 0; i < SIZE; i++) {
        //     address_vector[i] = 0;
        //     full_addr_vector[i] = 0;
        // }

        // Reset packets and rebind
        // for(uint32_t i = 0; i < SIZE; i++) {
        //     entry[i].quickReset();
        //     entry[i].set_queue_vectors(&address_vector, &full_addr_vector, i);
        // }
        for (uint32_t i = 0; i < SIZE; i++) {
            entry[i].quickReset();

            // DIRECT member access - no function call
            entry[i].address.vec_ptr = &address_vector;
            entry[i].address.index = i;
            entry[i].address.fallback_value = 0;
            entry[i].full_addr.vec_ptr = &full_addr_vector;
            entry[i].full_addr.index = i;
            entry[i].full_addr.fallback_value = 0;
        }

        // processed_packet removed — was never used
    }
    // void quick_reset(){
    //         head = 0;
    //     tail = 0;
    //     occupancy = 0;
    //     for(uint32_t i = 0; i < SIZE; i++) {
    //         entry[i].quickReset();
    //         entry[i].set_queue_vectors(&address_vector, &full_addr_vector, i);
    //     }
    //     for(uint32_t i = 0; i < 2*MAX_READ_PER_CYCLE; i++) {
    //         processed_packet[i].quickReset();
    //         // vec_ptr remains nullptr per quickReset() - correct for temporaries
    //     }

    // }

    int check_queue(PACKET *packet) const;
    void add_queue(PACKET *packet), remove_queue(PACKET *packet);
};

// reorder buffer
class CORE_BUFFER
{
public:
    const string NAME;
    const uint16_t SIZE;
    uint16_t cpu,
        head,
        tail,
        occupancy,
        last_read, last_fetch, last_scheduled,
        inorder_fetch[2],
        next_fetch[2],
        next_schedule;
    uint64_t event_cycle;
    // lsq_event_cycle,
    // retire_event_cycle;
    //  fetch_event_cycle,
    //  schedule_event_cycle,
    //  execute_event_cycle,

    ooo_model_instr *entry;

    // constructor
    CORE_BUFFER(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        head = 0;
        tail = 0;
        occupancy = 0;

        last_read = SIZE - 1;
        last_fetch = SIZE - 1;
        last_scheduled = 0;

        inorder_fetch[0] = 0;
        inorder_fetch[1] = 0;
        next_fetch[0] = 0;
        next_fetch[1] = 0;
        next_schedule = 0;

        event_cycle = 0;
        entry = new ooo_model_instr[SIZE]();
    };

    // destructor
    ~CORE_BUFFER() {
        delete[] entry;
    };
};

// load/store queue
class LSQ_ENTRY
{
public:
    uint64_t instr_id : 34;
    uint64_t producer_id;
    uint64_t virtual_address;
    uint64_t physical_address : 48;
    uint64_t ip : 48;
    uint64_t event_cycle;

    uint16_t rob_index, data_index, sq_index;

    uint8_t translated : 2;
    uint8_t fetched : 2;
    uint8_t asid[2];
#ifdef USE_HERMES
    int32_t rob_position;
    int8_t rob_part_type;
    uint8_t went_offchip;
    uint8_t went_offchip_pred;
    ocp_base_feature_t *ocp_feature;
#endif

    void quickReset() {
        instr_id = 0;
        producer_id = UINT64_MAX;
        virtual_address = 0;
        physical_address = 0;
        ip = 0;
        event_cycle = 0;

        rob_index = 0;
        data_index = 0;
        sq_index = UINT16_MAX;

        translated = 0;
        fetched = 0;
        asid[0] = UINT8_MAX;
        asid[1] = UINT8_MAX;
#ifdef USE_HERMES
        rob_position = -1;
        rob_part_type = -1;
        went_offchip = 0;
        went_offchip_pred = 0;
        ocp_feature = NULL;
#endif

    }

    // constructor
    LSQ_ENTRY() {
        instr_id = 0;
        producer_id = UINT64_MAX;
        virtual_address = 0;
        physical_address = 0;
        ip = 0;
        event_cycle = 0;

        rob_index = 0;
        data_index = 0;
        sq_index = UINT16_MAX;

        translated = 0;
        fetched = 0;
        asid[0] = UINT8_MAX;
        asid[1] = UINT8_MAX;
#ifdef USE_HERMES
        // mirror quickReset(): without this, freshly-constructed LQ entries have a garbage
        // ocp_feature pointer -> `delete ocp_feature` in release_load_queue SIGSEGVs at sim start.
        rob_position = -1;
        rob_part_type = -1;
        went_offchip = 0;
        went_offchip_pred = 0;
        ocp_feature = NULL;
#endif

#if 0
        for (uint32_t i=0; i<ROB_SIZE; i++)
            forwarding_depend_on_me[i] = 0;
#endif
    };
};

class LOAD_STORE_QUEUE
{
public:
    const string NAME;
    const uint32_t SIZE;
    uint32_t occupancy, head, tail;

    LSQ_ENTRY *entry;

    // constructor
    LOAD_STORE_QUEUE(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        occupancy = 0;
        head = 0;
        tail = 0;

        entry = new LSQ_ENTRY[SIZE];
    };

    // destructor
    ~LOAD_STORE_QUEUE() {
        delete[] entry;
    };
};
#endif
