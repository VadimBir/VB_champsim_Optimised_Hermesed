

#include "ooo_cpu.h"
#include "set.h"
#include "instr_event.h"
#include "cycle_pack.h"
#include "hash_table7.hpp"   // P7:  emhash7::HashMap (swapped in for ankerl maps)
#include "hash_set3.hpp"     // P7c: emhash7::HashSet (swapped in for ankerl sets)
#ifdef USE_HERMES
namespace knob { extern bool enable_ddrp; extern bool offchip_pred_mark_merged_load; extern uint32_t ddrp_req_latency; extern bool enable_ddrp_monitor; }
#endif 

#include "ooo_cpu_helper.cc"   // PURE code motion: file-local SANITY macros + print_core_config moved here (same TU); placed at top so later datapath uses see the macros

// === PASTE THIS MACRO AT TOP OF ooo_cpu.cc ===
// #define TRACK_MEMOPS(rob_idx, source_label) \
// if ((current_core_cycle[cpu] > 58318781) && warmup_complete[cpu] && ROB.entry[rob_idx].instr_id >= 1074720 && ROB.entry[rob_idx].instr_id <= 1074730) { \
// cout << "[MEMOPS] " << source_label \
// << " instr:" << ROB.entry[rob_idx].instr_id \
// << " rob:" << rob_idx \
// << " before:" << (int)ROB.entry[rob_idx].num_mem_ops \
// << " cy:" << current_core_cycle[cpu] << endl; \
// }
// out-of-order core
int64_t execution_checksum[NUM_CPUS] = {0}; // the execution checksum to verify correctness, retire_ROB changes it. 

O3_CPU ooo_cpu[NUM_CPUS];
uint64_t current_core_cycle[NUM_CPUS];
uint64_t stall_cycle[NUM_CPUS];
alignas(64) uint16_t rob_memory_count[NUM_CPUS] = {0};
alignas(64) uint16_t next_mem_sched_start[NUM_CPUS] = {0};
uint32_t SCHEDULING_LATENCY = 0, EXEC_LATENCY = 0;

#include <cstdint>
#include "instruction.h"
#include "cache.h"


// alignas(64) int8_t is_executed_Arr[NUM_CPUS][ROB_SIZE] = {{0}};


void O3_CPU::initialize_core() {

}


const ooo_model_instr INSTR_TEMPLATE = []{
    return ooo_model_instr{};
}();

[[maybe_unused]] const size_t instr_size = sizeof(input_instr);

// USE OF REFERENCE MODIFIES ENTRIES DIRECTLY, WHICH MEANS instr_id IS NOT EMPTY.
// BEFORE OVERWRITING WE GET OLD VALUE USED TO ERROR CHECK.
bool previousNotEmpty;
bool read_success;


// struct alignas(64) MemoryROBLinkedList {
//     alignas(64) uint16_t next_mem_rob[NUM_CPUS][ROB_SIZE];
//     alignas(64) uint16_t first_mem_rob[NUM_CPUS];
//     alignas(64) uint16_t last_mem_rob[NUM_CPUS];

//     MemoryROBLinkedList() {
//         for (uint8_t cpu = 0; cpu < NUM_CPUS; cpu++) {
//             first_mem_rob[cpu] = ROB_SIZE;
//             last_mem_rob[cpu] = ROB_SIZE;
//             memset(next_mem_rob[cpu], 0xFF, ROB_SIZE * sizeof(uint16_t));
//         }
//     }

//     inline void append(const uint8_t cpu, const uint16_t rob_index) {
//         next_mem_rob[cpu][rob_index] = ROB_SIZE;
//         if (last_mem_rob[cpu] == ROB_SIZE) {
//             first_mem_rob[cpu] = rob_index;
//         } else {
//             next_mem_rob[cpu][last_mem_rob[cpu]] = rob_index;
//         }
//         last_mem_rob[cpu] = rob_index;
//     }

//     inline void remove_head(const uint8_t cpu, const uint16_t rob_head) {
//         if (first_mem_rob[cpu] == rob_head) {
//             first_mem_rob[cpu] = next_mem_rob[cpu][rob_head];
//             if (first_mem_rob[cpu] == ROB_SIZE) {
//                 last_mem_rob[cpu] = ROB_SIZE;
//             }
//         }
//         next_mem_rob[cpu][rob_head] = ROB_SIZE;
//     }

//     inline bool is_empty(const uint8_t cpu) const {
//         return first_mem_rob[cpu] == ROB_SIZE;
//     }

//     class Iterator {
//         const uint16_t* next_array;
//         uint16_t current;
        
//     public:
//         Iterator(const uint16_t* next_arr, uint16_t cur) 
//             : next_array(next_arr), current(cur) {}
        
//         bool operator!=(const Iterator& other) const { return current != other.current; }
        
//         Iterator& operator++() {
//             current = next_array[current];
//             return *this;
//         }
        
//         uint16_t operator*() const { return current; }
//     };

//     struct CPUView {
//         const MemoryROBLinkedList* list;
//         uint8_t cpu;
        
//         Iterator begin() const {
//             return Iterator(list->next_mem_rob[cpu], list->first_mem_rob[cpu]);
//         }
        
//         Iterator end() const {
//             return Iterator(list->next_mem_rob[cpu], ROB_SIZE);
//         }
//     };

//     CPUView cpu_view(uint8_t cpu) const {
//         return CPUView{this, cpu};
//     }
// };
// MemoryROBLinkedList rob_memory_list;

#include <cstdint>
struct alignas(64)  {
    alignas(64)  uint64_t lq_bits[NUM_CPUS][(LQ_SIZE + 63) / 64];

    uint16_t alloc_lq(const uint8_t cpu) {
        static bool setup[NUM_CPUS] = {false};

        // Lazy initialization on first call per CPU
        if (!setup[cpu]) {
        uint16_t words = (LQ_SIZE + 63) / 64;

        // Set all valid bits as free (1)
        for (uint16_t i = 0; i < words; i++) {
            lq_bits[cpu][i] = UINT64_MAX;
        }

        // Mask shadow bits in last word as occupied (0)
        uint8_t valid_bits = LQ_SIZE % 64;
        if (valid_bits) {
            lq_bits[cpu][words-1] &= (1ULL << valid_bits) - 1;
        }

        setup[cpu] = true;
        }

        // Find first free slot
        alignas(64)  uint8_t words = (LQ_SIZE + 63) / 64;
        for (uint8_t i = 0; i < words; i++) {
            if (lq_bits[cpu][i]) {
                uint8_t bit = __builtin_ctzll(lq_bits[cpu][i]);
                lq_bits[cpu][i] &= ~(1ULL << bit);
                return (i << 6) + bit;
            }
        }
            return LQ_SIZE;  // No free slots
    }

    void free_lq(const uint8_t cpu, const uint16_t idx) {
        uint16_t w = idx >> 6, b = idx & 63;
        SANITY_LQ_BIT_FREE(lq_bits, cpu, w, b);
        lq_bits[cpu][w] |= 1ULL << b;
    }
} free_LQueue;

// ROB_RegisterDeps rob_reg_deps;
struct LQ_PendingLoads {
    alignas(64) emhash7::HashMap<uint64_t, emhash7::HashSet<uint16_t>> address_to_lq[NUM_CPUS];
    
    inline void add_pending_load(uint16_t cpu, uint64_t virtual_address, uint16_t lq_index) {
        address_to_lq[cpu][virtual_address].insert(lq_index);
    }
    
    inline emhash7::HashSet<uint16_t>* get_pending_loads(uint16_t cpu, uint64_t virtual_address) {
        auto it = address_to_lq[cpu].find(virtual_address);
        return (it != address_to_lq[cpu].end()) ? &it->second : nullptr;
    }
    
    inline void remove_pending_load(uint16_t cpu, uint64_t virtual_address, uint16_t lq_index) {
        auto it = address_to_lq[cpu].find(virtual_address);
        if (it != address_to_lq[cpu].end()) {
            it->second.erase(lq_index);
            if (it->second.empty()) 
                address_to_lq[cpu].erase(it);
        }
    }
    
    inline void clear_cpu(uint16_t cpu) { 
        address_to_lq[cpu].clear(); 
    }
};

LQ_PendingLoads lq_pending_loads;

struct {
    alignas(64)  uint8_t branch_mispredicted[NUM_CPUS][ROB_SIZE];

    inline void set_branch_mispredicted(const uint16_t cpu, const uint32_t rob_idx, const uint8_t value) {
        branch_mispredicted[cpu][rob_idx] = value;
    }

    inline uint8_t get_branch_mispredicted(const uint16_t cpu, const uint32_t rob_idx) {
        return branch_mispredicted[cpu][rob_idx];
    }

    inline void clear_rob(const uint16_t cpu, const uint32_t rob_idx) {
        branch_mispredicted[cpu][rob_idx] = 0;
    }

    inline void clear_cpu(const uint16_t cpu) {
        memset(branch_mispredicted[cpu], 0, ROB_SIZE);
    }
} flat_branch_mispredicted;
// replaces the check ROB linear search with a hashmap search:
// 1. ROB add will add entry,
// 2. ROB check (find) will return the rob index
// 3. on ROB retire entry is removed
class ROB_HashTable {
public:
    // std::unordered_map<uint64_t, std::unordered_set<uint32_t>> address_to_indices[NUM_CPUS];
    alignas(64)  emhash7::HashMap<uint64_t, uint32_t> rob_maps[NUM_CPUS];
    // Add instruction ID to ROB index mapping
    void add_rob_idx(const uint16_t cpu, const uint64_t instr_id, const uint32_t rob_index) {
        rob_maps[cpu][instr_id] = rob_index;
    }

    // Get ROB index for given instruction ID
    uint32_t get_rob_idx(const uint16_t cpu, const uint64_t instr_id, const uint32_t rob_size) {
        auto it = rob_maps[cpu].find(instr_id);
        if (it != rob_maps[cpu].end()) {
            return it->second;
        }
        // SANITY CHECK - Same error handling as original check_rob
        cerr << "[ROB_ERROR] " << __func__ << " does not have any matching index! ";
        cerr << " instr_id: " << instr_id << endl;
        int siblingROB = get_rob_idx(0, 1074969, ROB_SIZE);
        cerr << " EXIST ISNTR ID: 1074969 ROB: " << siblingROB;
        assert(0);
        return rob_size;
    }

    // Remove instruction ID from hashmap when retiring
    void retire_rob_idx(const uint16_t cpu, const uint64_t instr_id) {
        rob_maps[cpu].erase(instr_id);
    }

    // Clear all entries for a CPU (for reset/initialization)
    void clear_cpu(const uint16_t cpu) {
        rob_maps[cpu].clear();
    }
};
#include <algorithm>

struct SQ_FwdInfo {
    uint16_t sq_index;
    uint64_t instr_id;
    uint8_t  fetched;
    uint64_t event_cycle;
    uint64_t physical_address;
};

class SQ_AddressMap {

public:
    alignas(64) emhash7::HashMap<uint64_t, ankerl::svector<SQ_FwdInfo, 2>> address_to_entries[NUM_CPUS];

    void add_sq_entry(const uint16_t cpu, const uint64_t virtual_address, const uint16_t sq_index, const uint64_t instr_id) {
        if (virtual_address != 0) {
            address_to_entries[cpu][virtual_address].push_back({sq_index, instr_id, 0, 0, 0});
        }
    }
    const ankerl::svector<SQ_FwdInfo, 2>* get_matching_entries(const uint16_t cpu, const uint64_t virtual_address) {
        auto it = address_to_entries[cpu].find(virtual_address);
        if (it != address_to_entries[cpu].end() && !it->second.empty()) {
            return &it->second;
        }
        return nullptr;
    }
    void remove_sq_entry(const uint16_t cpu, const uint64_t virtual_address, const uint16_t sq_index) {
        if (virtual_address == 0) return;
        auto it = address_to_entries[cpu].find(virtual_address);
        if (it != address_to_entries[cpu].end()) {
            auto& vec = it->second;
            for (size_t j = 0; j < vec.size(); j++) {
                if (vec[j].sq_index == sq_index) {
                    vec[j] = vec.back();
                    vec.pop_back();
                    break;
                }
            }
            if (vec.empty()) address_to_entries[cpu].erase(it);
        }
    }
    void update_fetched(const uint16_t cpu, const uint64_t virtual_address, const uint16_t sq_index, uint8_t fetched, uint64_t event_cycle) {
        auto it = address_to_entries[cpu].find(virtual_address);
        if (it != address_to_entries[cpu].end()) {
            for (auto& e : it->second) {
                if (e.sq_index == sq_index) {
                    e.fetched = fetched;
                    e.event_cycle = event_cycle;
                    return;
                }
            }
        }
    }
    void update_physical_address(const uint16_t cpu, const uint64_t virtual_address, const uint16_t sq_index, uint64_t pa) {
        auto it = address_to_entries[cpu].find(virtual_address);
        if (it != address_to_entries[cpu].end()) {
            for (auto& e : it->second) {
                if (e.sq_index == sq_index) {
                    e.physical_address = pa;
                    return;
                }
            }
        }
    }
};
// Global instance for check_ROB()
ROB_HashTable rob_hash_table;

SQ_AddressMap sq_address_map;


#ifdef USE_TRACE_HELPER
#include "trace_helper.h"

void O3_CPU::handle_branch() {
    uint8_t continue_reading = 1;
    uint32_t num_reads = 0;
    instrs_to_read_this_cycle = FETCH_WIDTH;

    TraceBuffer& tbuf = trace_helper.buffers[cpu];

    while (continue_reading) {
        // Spin-wait until helper has an entry ready
        uint32_t t = tbuf.tail.load(std::memory_order_relaxed);
        uint32_t h = tbuf.head.load(std::memory_order_acquire);
        if (t == h) {
            // Helper not ready — spin
            trace_helper.io_trace_idle[cpu]++;
            continue;
        }

        TraceBufEntry& src = tbuf.entries[t];

        previousNotEmpty = ROB.entry[ROB.tail].instr_id;
        ooo_model_instr* arch_instr = &ROB.entry[ROB.tail];

        // Copy fully prepared instruction from buffer into ROB slot
        *arch_instr = src.instr;

        // Overwrite instr_id with sequential main-thread id
        arch_instr->instr_id = instr_unique_id;

        // STA update — must stay on main thread
        int num_mem_ops = 0;
        for (uint32_t i = 0; i < MAX_INSTR_DESTINATIONS; i++) {
            if (arch_instr->destination_memory[i]) {
                num_mem_ops++;
                if (num_mem_ops > 0) {
                    SANITY_STA_TAIL_EMPTY();
                    STA[STA_tail] = instr_unique_id;
                    STA_tail++;
                    if (STA_tail == STA_SIZE)
                        STA_tail = 0;
                }
            }
        }

        if (arch_instr->num_mem_ops > 0) {
            BS_SET(rob_events.per_cpu[cpu].is_mem, ROB.tail);
        }

        // Consume: advance tail
        uint32_t next_t = t + 1;
        if (next_t >= TRACE_BUFFER_SIZE) next_t = 0;
        tbuf.tail.store(next_t, std::memory_order_release);
        trace_helper.maybe_wake(cpu);

        // ROB insertion — unchanged
        if (ROB.occupancy < ROB.SIZE) {
            uint32_t rob_index = add_to_rob(arch_instr);
            num_reads++;

            // Branch stats from pre-computed prediction
            if (arch_instr->is_branch) {
                num_branch++;
                if (src.branch_mispredicted) {
                    branch_mispredictions++;
                    total_rob_occupancy_at_branch_mispredict += ROB.occupancy;
                    instrs_to_read_this_cycle = 0;
                    fetch_stall = 1;
                    flat_branch_mispredicted.branch_mispredicted[cpu][rob_index] = 1;
                } else {
                    instrs_to_read_this_cycle = src.branch_prediction ? 0 : instrs_to_read_this_cycle;
                }
                // last_branch_result already called by helper
            }

            if ((num_reads >= instrs_to_read_this_cycle) || (ROB.occupancy == ROB.SIZE))
                continue_reading = 0;
        }
        instr_unique_id++;
    }
}

#else // !USE_TRACE_HELPER — original single-threaded handle_branch

void O3_CPU::handle_branch() {
    // actual processors do not work like this but for easier implementation,
    // we read instruction traces and virtually add them in the ROB
    // note that these traces are not yet translated and fetched

    uint8_t continue_reading = 1;
    uint32_t num_reads = 0;
    instrs_to_read_this_cycle = FETCH_WIDTH;


    // first, read PIN trace
    while (continue_reading) {

        // SINGLE THREADED WORKING CUSTOM XZ DECODE => READ => GET CURRENT INSTR

        if (xz_reader.eof()) {
            if (!xz_reader.reopen()) {
                cerr << "*** CANNOT REOPEN XZ TRACE FOR CPU: " << cpu << " ***" << endl;
                assert(0);
            }
            cout << "*** Restarted XZ trace for Core: " << cpu << " ***" << endl;
        }
        read_success = xz_reader.read(&current_instr, instr_size, 1);
        previousNotEmpty = ROB.entry[ROB.tail].instr_id;
        ooo_model_instr* arch_instr = &ROB.entry[ROB.tail];


        int num_reg_ops = 0, num_mem_ops = 0;
        arch_instr->instr_id = instr_unique_id;
        arch_instr->ip = current_instr.ip;
        arch_instr->is_branch = current_instr.is_branch;
        arch_instr->branch_taken = current_instr.branch_taken;

        arch_instr->asid[0] = cpu;
        arch_instr->asid[1] = cpu;

        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            arch_instr->destination_registers[i] = current_instr.destination_registers[i];
            arch_instr->destination_memory[i] = current_instr.destination_memory[i];
            // arch_instr->destination_virtual_address[i] = current_instr.destination_memory[i];  // field removed (dead)

            if (arch_instr->destination_registers[i])
            num_reg_ops++;
        if (arch_instr->destination_memory[i]) {
            num_mem_ops++;

            // update STA, this structure is required to execute store instructios properly without deadlock
            if (num_mem_ops > 0) {
            SANITY_STA_TAIL_EMPTY();
            STA[STA_tail] = instr_unique_id;
            STA_tail++;

            if (STA_tail == STA_SIZE)
            STA_tail = 0;
    }
            }
        }

        for (uint8_t i=0; i<NUM_INSTR_SOURCES; i++) {
            arch_instr->source_registers[i] = current_instr.source_registers[i];
            arch_instr->source_memory[i] = current_instr.source_memory[i];
            // arch_instr->source_virtual_address[i] = current_instr.source_memory[i];  // field removed (dead)

            if (arch_instr->source_registers[i])
                num_reg_ops++;
            if (arch_instr->source_memory[i])
                num_mem_ops++;
        }

        arch_instr->num_reg_ops = num_reg_ops;
        arch_instr->num_mem_ops = num_mem_ops;
        if (num_mem_ops > 0){
            BS_SET(rob_events.per_cpu[cpu].is_mem, ROB.tail);
        }

        // virtually add this instruction to the ROB
        if (ROB.occupancy < ROB.SIZE) {
            uint32_t rob_index = add_to_rob(arch_instr);
            num_reads++;

            // branch prediction
            if (arch_instr->is_branch) {
                num_branch++;
                uint8_t branch_prediction = predict_branch(arch_instr->ip);
                if (arch_instr->branch_taken != branch_prediction) {
                    branch_mispredictions++;
                    total_rob_occupancy_at_branch_mispredict += ROB.occupancy;

                    // halt any further fetch this cycle
                    instrs_to_read_this_cycle = 0;

                    // and stall any additional fetches until the branch is executed
                    fetch_stall = 1;

                    // ROB.entry[rob_index].branch_mispredicted = 1;
                    flat_branch_mispredicted.branch_mispredicted[cpu][rob_index] = 1;
                } else {
                    instrs_to_read_this_cycle = branch_prediction ? 0 : instrs_to_read_this_cycle;
                }
                last_branch_result(arch_instr->ip, arch_instr->branch_taken);
            }

            //if ((num_reads == FETCH_WIDTH) || (ROB.occupancy == ROB.SIZE))
            if ((num_reads >= instrs_to_read_this_cycle) || (ROB.occupancy == ROB.SIZE))
                continue_reading = 0;
        }
        instr_unique_id++;
    }
}

#endif // USE_TRACE_HELPER


uint32_t O3_CPU::add_to_rob(ooo_model_instr *arch_instr) {
    const uint32_t index = ROB.tail;
    // flush out cout
    // sanity check
    if (previousNotEmpty != 0) {
        cerr << "[ROB_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " instr_id: " << ROB.entry[index].instr_id << endl;
        assert(0);
    }

    ROB.entry[index] = *arch_instr;



#ifdef TRUE_SANITY_CHECK
    // rob_maps is WRITE-ONLY in production: its only reader is check_rob(), called solely from
    // SANITY_CHECK_ROB_MATCH/_RFO which are #ifdef TRUE_SANITY_CHECK. Register-dep tracking uses
    // the flat reg_producers[256] (B2'); rob_index is carried directly in packets/LQ/SQ. So this
    // per-instruction hashmap insert is dead work unless sanity checks are compiled in.
    rob_hash_table.add_rob_idx(cpu, instr_unique_id, index);
#endif
    ROB.entry[index].instr_id = instr_unique_id;
    ROB.entry[index].rob_index = index;  // Set correct index
    BS_CLR(rob_events.per_cpu[cpu].fetched_inflight, index);
    BS_CLR(rob_events.per_cpu[cpu].fetched_complete, index);
    BS_CLR(rob_events.per_cpu[cpu].sched_inflight, index);
    BS_CLR(rob_events.per_cpu[cpu].sched_complete, index);
    { uint64_t _now = PACK_CYCLE(current_core_cycle[cpu]);
      rob_events.per_cpu[cpu].ec_write(index, _now, _now); }

    addr_dependencies.add_producer(index, arch_instr->destination_registers);
    mem_dependencies.add_producer(index, arch_instr->destination_memory);
    // (int) // DEBUGGING TRAVERSAL OVER ROB_SIZE FOR FETCHED_GET

    ROB.occupancy++;
    ROB.tail++;
    if (ROB.tail >= ROB.SIZE)
        ROB.tail = 0;


    SANITY_ROB_IP_NZ(__func__, index);

    return index;
}

uint32_t O3_CPU::check_rob(const uint64_t instr_id) {
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return ROB.SIZE;

    // rob_hash_table.rob_maps[cpu].prefetch_mapped_value(instr_id);

    // REPLACE ALL LINEAR SEARCH LOOPS WITH THIS SINGLE LINE:
    return rob_hash_table.get_rob_idx(cpu, instr_id, ROB.SIZE);


    cerr << "[ROB_ERROR] " << __func__ << " does not have any matching index! ";
    cerr << " instr_id: " << instr_id << endl;
    assert(0);

    return ROB.SIZE;
}
const PACKET TRACE_TEMPLATE = []{
    PACKET p;
    p.instruction = 1;
    p.tlb_access = 1;
    p.fill_level = FILL_L1;
    p.type = LOAD;
    // p.producer = 0;  // TODO: check if this guy gets used or not
    return p;
}();
const PACKET FETCH_PACKET_TEMPLATE = []{
    PACKET p;
    p.instruction = 1;
    p.fill_level = FILL_L1;
    p.type = LOAD;
    // p.producer = 0;
    return p;
}();
void O3_CPU::fetch_instruction() {
    // core_cycle_packed-hoist: current_core_cycle[cpu] invariant here; callees (ITLB/L1I.add_rq)
    // do not write it (only writer is main_loop's per-tick increment).
    const uint64_t now = PACK_CYCLE(current_core_cycle[cpu]);
    if ((fetch_stall == 1) && (PCYCLE_GE(now, fetch_resume_cycle)) && (fetch_resume_cycle != 0)) {
        fetch_stall = 0;
        fetch_resume_cycle = 0;
    }

    uint32_t read_index = (ROB.last_read == (ROB.SIZE-1)) ? 0 : (ROB.last_read + 1);
    for (uint8_t i = 0; i < FETCH_WIDTH; i++) {
        if (ROB.entry[read_index].ip == 0)
            break;

        if (ROB.entry[read_index].translated) {
            if (read_index == ROB.head)
                break;
            else { SANITY_ROB_EVENTS_DUMP(i); }
        }

        // P28: constant fields (instruction, tlb_access, fill_level, type) come
        // from TRACE_TEMPLATE; every per-iteration field is still written below.
        PACKET trace_packet = TRACE_TEMPLATE;
        trace_packet.cpu = cpu;
        trace_packet.address = ROB.entry[read_index].ip >> LOG2_PAGE_SIZE;
        trace_packet.full_addr = ROB.entry[read_index].ip;
        trace_packet.instr_id = ROB.entry[read_index].instr_id;
        trace_packet.rob_index = read_index;
        trace_packet.ip = ROB.entry[read_index].ip;
        trace_packet.asid[0] = ROB.entry[read_index].asid[0];
        trace_packet.asid[1] = ROB.entry[read_index].asid[1];
        trace_packet.event_cycle = now;

        int rq_index = ITLB.add_rq(&trace_packet);

        if (rq_index == -2)
            break;

        ROB.last_read = read_index;
        read_index++;
        if (read_index == ROB.SIZE)
            read_index = 0;
    }

    uint32_t fetch_index = (ROB.last_fetch == (ROB.SIZE-1)) ? 0 : (ROB.last_fetch + 1);
    for (uint32_t i = 0; i < FETCH_WIDTH; i++) {
        if ((ROB.entry[fetch_index].translated != COMPLETED) || PCYCLE_GT(rob_events.per_cpu[cpu].event_cycle[fetch_index], now))
            break;

        if (BS_TST(rob_events.per_cpu[cpu].fetched_inflight, fetch_index) || BS_TST(rob_events.per_cpu[cpu].fetched_complete, fetch_index)) {
            if (fetch_index == ROB.head)
                break;
        }

        // P28: constant fields (instruction, fill_level, type) come from
        // FETCH_PACKET_TEMPLATE; every per-iteration field is still written below.
        PACKET fetch_packet = FETCH_PACKET_TEMPLATE;
        fetch_packet.cpu = cpu;
        fetch_packet.address = ROB.entry[fetch_index].instruction_pa >> 6;
        fetch_packet.instruction_pa = ROB.entry[fetch_index].instruction_pa;
        fetch_packet.full_addr = ROB.entry[fetch_index].instruction_pa;
        fetch_packet.instr_id = ROB.entry[fetch_index].instr_id;
        fetch_packet.rob_index = fetch_index;
        fetch_packet.ip = ROB.entry[fetch_index].ip;
        fetch_packet.asid[0] = ROB.entry[fetch_index].asid[0];
        fetch_packet.asid[1] = ROB.entry[fetch_index].asid[1];
        fetch_packet.event_cycle = now;

        int rq_index = L1I.add_rq(&fetch_packet);

        if (rq_index == -2)
            break;

        BS_SET(rob_events.per_cpu[cpu].fetched_inflight, fetch_index);
        if (BS_TST(rob_events.per_cpu[cpu].is_mem, fetch_index)) {
            rob_memory_count[cpu]++;
            mem_index_ring.push(cpu, fetch_index);
        }
        ROB.entry[fetch_index].rob_index = fetch_index;
        BS_CLR(rob_events.per_cpu[cpu].sched_inflight, fetch_index);
        BS_CLR(rob_events.per_cpu[cpu].sched_complete, fetch_index);
        ROB.last_fetch = fetch_index;
        fetch_index++;
        if (fetch_index == ROB.SIZE)
            fetch_index = 0;
    }
}


#define CACHE_LINE_SIZE 64
#define PREFETCH_LINE_DISTANCE 16

// TODO: When should we update ROB.schedule_event_cycle?
// I. Instruction is fetched
// II. Instruction is completed
// III. Instruction is retired
uint64_t result_bits[ROB_WORDS];
uint64_t cycle_ready[ROB_WORDS];
size_t bit_offset;
size_t i;

void O3_CPU::schedule_instruction() {
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // HEAP-SCHED (A): mature the timing min-heap -> cr_bitset for this cycle.
    // After this, cr_bitset[w] bit i == (event_cycle[w*64+i] <= now), exactly
    // the value the original per-word PCYCLE_LE inner loop computed.
    rob_events.per_cpu[cpu].mature(PACK_CYCLE(current_core_cycle[cpu]));

    const uint64_t* fetched_complete_ptr = rob_events.per_cpu[cpu].fetched_complete;
    const uint64_t* sched_inflight_ptr = rob_events.per_cpu[cpu].sched_inflight;
    const uint64_t* sched_complete_ptr = rob_events.per_cpu[cpu].sched_complete;
    const uint64_t* cycle_ready_ptr = rob_events.per_cpu[cpu].cr_bitset;
    const uint64_t core_cycle_packed = PACK_CYCLE(current_core_cycle[cpu]);
    const uint32_t head = ROB.head;
    const uint32_t limit = ROB.next_fetch[1];

    // P15: OR-fold dirty-check skip-guard. The fused scan can only schedule a
    // bit where (fetched_complete & cycle_ready & ~sched_inflight & ~sched_complete & rmask) is set. Since cycle_ready and rmask
    // only narrow, (fetched_complete & ~sched_inflight & ~sched_complete) is a strict per-word superset of any
    // schedulable bit. Folding it over ALL ROB_WORDS is a superset of the
    // scanned range, so if the fold is zero nothing can be scheduled this cycle
    // and the scan would have no side effects -> safe early return (true no-op).
    {
        uint64_t dirty = 0;
        for (uint32_t w = 0; w < ROB_WORDS; w++)
            dirty |= fetched_complete_ptr[w] & ~sched_inflight_ptr[w] & ~sched_complete_ptr[w];
        if (dirty == 0)
            return;
    }

    // Fused single-pass: cycle_ready + AND + range mask + stall + dispatch
    // Only touches words in the active range, not all ROB_WORDS
    // B1: bind explicit __restrict__ scalar locals so the compiler keeps the
    // four distinct (non-aliasing) array pointers + core_cycle_packed + self in registers
    // across the per-word loop instead of spilling/reloading them around the
    // opaque do_scheduling() call. Codegen-only; semantics identical.
    const uint64_t* __restrict__ fetched_complete_l   = fetched_complete_ptr;
    const uint64_t* __restrict__ sched_inflight_l = sched_inflight_ptr;
    const uint64_t* __restrict__ sched_complete_l = sched_complete_ptr;
    const uint64_t* __restrict__ cycle_ready_l   = cycle_ready_ptr;
    const uint64_t                core_cycle_packed_l = core_cycle_packed;
    (void)core_cycle_packed_l;
    O3_CPU* __restrict__          self  = this;
    auto fused_scan = [fetched_complete_l, sched_inflight_l, sched_complete_l, cycle_ready_l, core_cycle_packed_l, self](uint32_t start, uint32_t end) -> bool {
        const uint32_t ws = start >> 6;
        const uint32_t we = (end + 63) >> 6;
        for (uint32_t w = ws; w < we && w < ROB_WORDS; w++) {
            uint64_t rmask = ~0ULL;
            if (w == ws) rmask &= ~((1ULL << (start & 63)) - 1);
            if (w == we - 1 && (end & 63)) rmask &= (1ULL << (end & 63)) - 1;

            // B3: per-word dirty pre-skip (strict superset of P15 guard).
            // ready == cycle_ready & dirty_word, so dirty_word==0 => no schedules this word.
            // When also (~fetched_complete & rmask)!=0, stall is guaranteed nonzero, so the real
            // path schedules nothing then returns false. Provable pure no-op.
            uint64_t dirty_word = fetched_complete_l[w] & ~sched_inflight_l[w] & ~sched_complete_l[w] & rmask;
            if (dirty_word == 0 && (~fetched_complete_l[w] & rmask) != 0)
                return false;

            // HEAP-SCHED: cycle_ready is read once per word from the maintained cr_bitset
            // (== original PCYCLE_LE(event_cycle[idx], now) per bit). Snapshotting it here
            // BEFORE the dispatch loop reproduces the original "compute cycle_ready once,
            // then dispatch" ordering: do_scheduling's non-mem event_cycle bump
            // clears cr_bitset bits but does NOT affect this word's local cycle_ready.
            uint64_t cycle_ready = cycle_ready_l[w];

            uint64_t ready = fetched_complete_l[w] & cycle_ready & ~sched_inflight_l[w] & ~sched_complete_l[w] & rmask;

            uint64_t stall = ~(fetched_complete_l[w] & cycle_ready) & rmask;
            if (stall) {
                ready &= (1ULL << (__builtin_ctzll(stall))) - 1;
                while (ready) {
                    bit_offset = __builtin_ctzll(ready);
                    i = w * 64 + bit_offset;
                    self->do_scheduling(i);
                    ready &= ready - 1;
                }
                return false;
            }

            while (ready) {
                bit_offset = __builtin_ctzll(ready);
                i = w * 64 + bit_offset;
                self->do_scheduling(i);
                ready &= ready - 1;
            }
        }
        return true;
    };

    if (head < limit) {
        fused_scan(head, limit);
    } else {
        if (fused_scan(head, ROB_SIZE))
            fused_scan(0, limit);
    }
}

inline void O3_CPU::do_scheduling(const uint16_t rob_index) {
    // CCP-DOSCHED: current_core_cycle[cpu] is written ONLY at init (main.cc) and
    // once per tick (main_loop.cc ++), so it is invariant across this call. Read
    // and PACK it ONCE up front into a local; the intervening opaque reg_dependency()
    // call would otherwise force the compiler to reload it at the event_cycle store.
    // Bit-exact (same value either way).
    const uint64_t core_cycle_packed_ds = PACK_CYCLE(current_core_cycle[cpu]);
    BS_SET(rob_events.per_cpu[cpu].reg_ready, rob_index);
    reg_dependency(rob_index);
    ROB.next_schedule = (rob_index == (ROB.SIZE - 1)) ? 0 : (rob_index + 1);

    const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, rob_index);
    if (is_mem){
        BS_SET(rob_events.per_cpu[cpu].sched_inflight, rob_index);
    } else {
        BS_SET(rob_events.per_cpu[cpu].sched_complete, rob_index);
        { uint64_t _e = rob_events.per_cpu[cpu].event_cycle[rob_index], _c = core_cycle_packed_ds;
          rob_events.per_cpu[cpu].ec_write(rob_index, PACK_CYCLE((PCYCLE_GE(_e,_c) ? _e : _c) + SCHEDULING_LATENCY), _c); }
        if (BS_TST(rob_events.per_cpu[cpu].reg_ready, rob_index)) {

            SANITY_RTE_TAIL_INVALID(RTE1, RTE1_tail);
            // remember this rob_index in the Ready-To-Execute array 1
            RTE1[RTE1_tail] = rob_index;

            RTE1_tail++;
            if (RTE1_tail == ROB_SIZE)
                RTE1_tail = 0;
        }
    }
}

void O3_CPU::reg_dependency(const uint16_t rob_index) {
    if (rob_index == ROB.head) return;

    // REGDEP-ENTRYREF: hoist the ROB entry reference once; identical reads/order.
    auto& e = ROB.entry[rob_index];

    // REGDEP-EARLY-EMPTY-SOURCES: register 0 == "no source" — add_producer only
    // ever inserts producers for destination_registers != 0, so reg_producers[0]
    // is always empty and get_latest_producer(0,...) returns -1 (true no-op). The
    // per-element guard below already skips reg-0 sources; this outer short-circuit
    // skips the loop entirely when EVERY source is 0. Pure no-op superset, bit-exact.
    bool any_source = false;
    for (uint32_t j = 0; j < NUM_INSTR_SOURCES; j++)
        any_source |= (e.source_registers[j] != 0);
    if (!any_source) return;

    // Direct lookup - NO TRAVERSAL
    for (uint32_t j = 0; j < NUM_INSTR_SOURCES; j++) {
        if (!e.source_registers[j] ||
            e.reg_RAW_checked[j]) continue;
        int producer = addr_dependencies.get_latest_producer(
                e.source_registers[j],
                rob_index,
                ROB.head
        );
        if (producer >= 0)
            reg_RAW_dependency(producer, rob_index, j);
    }
}

void O3_CPU::reg_RAW_dependency(const uint16_t prior, const uint16_t current, const uint8_t source_index) {
reg_dep_tracker.add(prior, current, source_index);
ROB.entry[prior].reg_RAW_producer = 1;

BS_CLR(rob_events.per_cpu[cpu].reg_ready, current);
ROB.entry[current].producer_id = ROB.entry[prior].instr_id;
ROB.entry[current].num_reg_dependent++;
ROB.entry[current].reg_RAW_checked[source_index] = 1;
}

void O3_CPU::execute_instruction() {
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // out-of-order execution for non-memory instructions
    // memory instructions are handled by memory_instruction()
    uint32_t exec_issued = 0, num_iteration = 0;

    while (exec_issued < EXEC_WIDTH) {
        if (RTE0[RTE0_head] < ROB_SIZE) {
            uint32_t exec_index = RTE0[RTE0_head];
            uint32_t next_head = RTE0_head + 1;
            if (next_head == ROB_SIZE) next_head = 0;
            __builtin_prefetch(&RTE0[next_head], 0, 3);
            if (RTE0[next_head] < ROB_SIZE)
                __builtin_prefetch(&rob_events.per_cpu[cpu].event_cycle[RTE0[next_head]], 0, 1);
            if (PCYCLE_LE(rob_events.per_cpu[cpu].event_cycle[exec_index], PACK_CYCLE(current_core_cycle[cpu]))) {
                do_execution(exec_index);

                RTE0[RTE0_head] = ROB_SIZE;
                RTE0_head++;
                if (RTE0_head == ROB_SIZE)
                    RTE0_head = 0;
                exec_issued++;
            }
            else {
                // RTEBRK: the head is valid but NOT ready this cycle. The original
                // loop neither advances RTE0_head nor exec_issued in this case, so it
                // would re-test the IDENTICAL head every iteration until num_iteration
                // caps — pure wasted spins, never executing a later entry (head only
                // advances on execution). Breaking is provably equivalent: same head,
                // same cycle => PCYCLE_LE fails identically each spin. IPC bit-exact.
                break;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (exec_issued < EXEC_WIDTH) {
        if (RTE1[RTE1_head] < ROB_SIZE) {
            uint32_t exec_index = RTE1[RTE1_head];
            uint32_t next_head1 = RTE1_head + 1;
            if (next_head1 == ROB_SIZE) next_head1 = 0;
            __builtin_prefetch(&RTE1[next_head1], 0, 3);
            if (RTE1[next_head1] < ROB_SIZE)
                __builtin_prefetch(&rob_events.per_cpu[cpu].event_cycle[RTE1[next_head1]], 0, 1);
            if (PCYCLE_LE(rob_events.per_cpu[cpu].event_cycle[exec_index], PACK_CYCLE(current_core_cycle[cpu]))) {
                do_execution(exec_index);

                RTE1[RTE1_head] = ROB_SIZE;
                RTE1_head++;
                if (RTE1_head == ROB_SIZE)
                    RTE1_head = 0;
                exec_issued++;
            }
            else {
                // RTEBRK: same equivalence as the RTE0 loop above — a valid-but-not-
                // ready head neither advances RTE1_head nor exec_issued, so the loop
                // would re-test the identical head until num_iteration caps without
                // ever reaching a later entry. Break => identical execution set.
                break;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB_SIZE-1))
            break;
    }
}

void O3_CPU::do_execution(const uint16_t rob_index) {
    const uint64_t now = PACK_CYCLE(current_core_cycle[cpu]); // core_cycle_packed-hoist: invariant, no callees
    if (BS_TST(rob_events.per_cpu[cpu].reg_ready, rob_index) && BS_TST(rob_events.per_cpu[cpu].sched_complete, rob_index) && PCYCLE_LE(rob_events.per_cpu[cpu].event_cycle[rob_index], now)) {
        ROB.entry[rob_index].executed = INFLIGHT;
        BS_SET(rob_events.per_cpu[cpu].exec_inflight, rob_index);
        { uint64_t _e = rob_events.per_cpu[cpu].event_cycle[rob_index], _c = now;
          rob_events.per_cpu[cpu].ec_write(rob_index, PACK_CYCLE((PCYCLE_GE(_e,_c) ? _e : _c) + EXEC_LATENCY), _c); }

        inflight_reg_executions++;
    }
}

uint16_t scan_end;
uint16_t effective_limit;
uint16_t end1;
// DEAD-2026-05-25: file-scope num_searched — only referenced in commented-out old schedule_memory_instruction
// uint32_t num_searched;

// uint64_t flat_array[ROB_SIZE];

// void O3_CPU::schedule_memory_instruction()
// {
//    if (rob_memory_list.is_empty(cpu) || (ROB.head == ROB.tail && ROB.occupancy == 0))
//        return;

//    (removed: old AOS pointer)
//    valid_bits.reset();

//    const uint64_t current_cycle = current_core_cycle[cpu];
//    num_searched = 0;

//    uint16_t* next_ptr = rob_memory_list.next_mem_rob[cpu];

//    for (uint16_t i : rob_memory_list.cpu_view(cpu)) {
//        if (num_searched >= SCHEDULER_SIZE) break;

//        __builtin_prefetch(&rob_events_ptr[next_ptr[i]], 0, 1);

//        uint64_t entry = rob_events_ptr[i];
//        uint64_t ev_cycle = entry >> 8;

//        if (((entry & (INFLIGHT_fetch_t | COMPLETE_fetch_t)) != COMPLETE_fetch_t) ||
//            (ev_cycle > current_cycle))
//            break;

//        if ((entry & REG_READY_t) &&
//            ((entry & (INFLIGHT_schedule_t | COMPLETE_schedule_t)) == INFLIGHT_schedule_t))
//            valid_bits.set(i);

//        num_searched++;
//    }

//    constexpr size_t NUM_WORDS = (ROB_SIZE + 63) / 64;
//    bits_data = reinterpret_cast<uint64_t*>(&valid_bits);

//    for (word_idx = 0; word_idx < NUM_WORDS; word_idx++) {
//        word = bits_data[word_idx];
//        while (word) {
//            bit_offset = __builtin_ctzll(word);
//            i = word_idx * 64 + bit_offset;
//            if (i >= ROB_SIZE) break;

//            do_memory_scheduling(i);

//            word &= word - 1;
//        }
//    }
// }
void O3_CPU::schedule_memory_instruction() {
   if (rob_memory_count[cpu] == 0 || (ROB.head == ROB.tail && ROB.occupancy == 0))
       return;

   const uint64_t* im_ptr = rob_events.per_cpu[cpu].is_mem;
   const uint64_t* rr_ptr = rob_events.per_cpu[cpu].reg_ready;
   const uint64_t* fetched_complete_ptr = rob_events.per_cpu[cpu].fetched_complete;
   const uint64_t* sched_inflight_ptr = rob_events.per_cpu[cpu].sched_inflight;
   const uint64_t* sched_complete_ptr = rob_events.per_cpu[cpu].sched_complete;

   // SMEMSCHED-RESTRICT: the 5 read arrays are distinct members of
   // rob_events.per_cpu[cpu] (is_mem/reg_ready/fetched_complete/sched_inflight/
   // sched_complete) and ready[] is a separate local stack array — no legitimate
   // overlap. Bind __restrict__ locals (mirror of schedule_instruction's B1) so
   // the writable ready[] store cannot force reloads of the read pointers across
   // the per-word loop. Codegen-only; semantics identical.
   const uint64_t* __restrict__ im_l   = im_ptr;
   const uint64_t* __restrict__ rr_l   = rr_ptr;
   const uint64_t* __restrict__ fetched_complete_l   = fetched_complete_ptr;
   const uint64_t* __restrict__ sched_inflight_l = sched_inflight_ptr;
   const uint64_t* __restrict__ sched_complete_l = sched_complete_ptr;

   // Bitwise pipeline: is_mem & reg_ready & fetched_complete & sched_inflight & ~sched_complete
   uint64_t ready[ROB_WORDS];
   uint64_t* __restrict__ ready_l = ready;
   for (uint32_t w = 0; w < ROB_WORDS; w++)
       ready_l[w] = im_l[w] & rr_l[w] & fetched_complete_l[w] & sched_inflight_l[w] & ~sched_complete_l[w];

   // Linear scan from next_mem_sched_start, same as baseline
   const uint32_t sched_start = next_mem_sched_start[cpu];
   uint32_t scan_end = (ROB.head + ROB.occupancy <= ROB.SIZE)
                     ? (ROB.head + ROB.occupancy)
                     : (ROB.head + ROB.occupancy - ROB.SIZE);
   const uint32_t limit = ROB.next_schedule;
   const uint32_t effective_limit = (scan_end < limit) ? scan_end : limit;
   uint16_t mem_remaining = rob_memory_count[cpu];

   auto scan_and_schedule = [&](uint32_t start, uint32_t end) -> bool {
       const uint32_t ws = start >> 6;
       const uint32_t we = (end + 63) >> 6;
       for (uint32_t w = ws; w < we && w < ROB_WORDS; w++) {
           uint64_t rmask = ~0ULL;
           if (w == ws) rmask &= ~((1ULL << (start & 63)) - 1);
           if (w == we - 1 && (end & 63)) rmask &= (1ULL << (end & 63)) - 1;

           // Break on fetch incomplete: first 0 in (is_mem & fc) = stall
           uint64_t mem_fc = im_l[w] & fetched_complete_l[w] & rmask;
           uint64_t mem_bits = im_l[w] & rmask;
           uint64_t stall = mem_bits & ~mem_fc;
           if (stall) {
               uint32_t sb = __builtin_ctzll(stall);
               rmask &= (1ULL << sb) - 1;
           }

           uint64_t word = ready_l[w] & rmask;
           // P21: skip empty word — no-op when nothing ready and no stall
           if (stall == 0 && word == 0) continue;
           while (word) {
               uint32_t b = __builtin_ctzll(word);
               uint32_t idx = w * 64 + b;
               if (check_and_add_lsq(idx) == 0) {
                   BS_CLR(rob_events.per_cpu[cpu].sched_inflight, idx);
                   BS_SET(rob_events.per_cpu[cpu].sched_complete, idx);
                   if (idx == next_mem_sched_start[cpu])
                       next_mem_sched_start[cpu] = (idx + 1 >= ROB.SIZE) ? 0 : idx + 1;
                   if (ROB.entry[idx].executed == 0) {
                       ROB.entry[idx].executed = INFLIGHT;
                       BS_SET(rob_events.per_cpu[cpu].exec_inflight, idx);
                   }
               }
               word &= word - 1;
               if (--mem_remaining == 0) return false;
           }
           if (stall) return false;
       }
       return true;
   };

   if (sched_start < effective_limit) {
       scan_and_schedule(sched_start, effective_limit);
   } else {
       uint32_t end1 = (sched_start < limit) ? limit : ROB.SIZE;
       if (scan_and_schedule(sched_start, end1) && effective_limit > 0)
           scan_and_schedule(0, effective_limit);
   }
}

void O3_CPU::execute_memory_instruction() {
    operate_lsq();
    operate_cache();
}

inline void O3_CPU::do_memory_scheduling(const uint16_t rob_index) {
    uint16_t not_available = check_and_add_lsq(rob_index);
    if (not_available == 0) {
        BS_CLR(rob_events.per_cpu[cpu].sched_inflight, rob_index);
        BS_SET(rob_events.per_cpu[cpu].sched_complete, rob_index);
        if (rob_index == next_mem_sched_start[cpu])
            next_mem_sched_start[cpu] = (rob_index + 1 >= ROB.SIZE) ? 0 : rob_index + 1;
        if (ROB.entry[rob_index].executed == 0) {
            ROB.entry[rob_index].executed  = INFLIGHT;
            BS_SET(rob_events.per_cpu[cpu].exec_inflight, rob_index);
        }

    }
}

uint32_t O3_CPU::check_and_add_lsq(const uint16_t rob_index) {
    uint32_t num_mem_ops = 0, num_added = 0;

    // load
    for (uint8_t i=0; i<NUM_INSTR_SOURCES; i++) {
        if (ROB.entry[rob_index].source_memory[i]) {
            num_mem_ops++;
            if (ROB.entry[rob_index].source_added[i])
                num_added++;
            else {
                if (LQ.occupancy < LQ.SIZE) {
                    add_load_queue(rob_index, i);
                    num_added++;
                }
            }
        }
    }

    // store
    for (uint8_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (ROB.entry[rob_index].destination_memory[i]) {
            num_mem_ops++;
            if (ROB.entry[rob_index].destination_added[i])
                num_added++;
            else if (SQ.occupancy < SQ.SIZE) {
                if (STA[STA_head] == ROB.entry[rob_index].instr_id) {
                    add_store_queue(rob_index, i);
                    num_added++;
                }
            }
        }
    }

    if (num_added == num_mem_ops)
        return 0;

    const uint32_t not_available = num_mem_ops - num_added;
    SANITY_NOT_AVAILABLE_LE(not_available, num_mem_ops, rob_index);

    return not_available;
}
void O3_CPU::add_load_queue(const uint16_t rob_index, const uint32_t data_index) {
    const uint32_t lq_index = free_LQueue.alloc_lq(cpu);

    SANITY_LQ_FREE_SLOT(lq_index, rob_index);

    auto& robe = ROB.entry[rob_index];
    auto& lqe = LQ.entry[lq_index];

    robe.lq_index[data_index] = lq_index;
    lqe.instr_id = robe.instr_id;
    lqe.virtual_address = robe.source_memory[data_index];
    lqe.ip = robe.ip;
    lqe.data_index = data_index;
    lqe.rob_index = rob_index;
    lqe.asid[0] = robe.asid[0];
    lqe.asid[1] = robe.asid[1];
    lqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu] + SCHEDULING_LATENCY);
    LQ.occupancy++;

    IF_HERMES(
    lqe.rob_position = compute_rob_position(rob_index, ROB.head);
    lqe.rob_part_type = rob_pos_get_part_type(lqe.rob_position);
    if (offchip_pred && robe.source_memory[data_index] != 0)
        lqe.went_offchip_pred = offchip_pred->predict(&robe, data_index, &lqe);
    )

    // RAW dependency check
    if (rob_index != ROB.head) {
        const uint64_t load_addr = robe.source_memory[data_index];
        auto [producer_rob, producer_dest_idx] = mem_dependencies.get_latest_producer(load_addr, rob_index, ROB.head);

        if (producer_rob >= 0) {
            ROB.entry[producer_rob].is_producer = 1;
            lqe.producer_id = ROB.entry[producer_rob].instr_id;
            lqe.translated = INFLIGHT;
        }
    }

    // Store forwarding check — fat map (no SQ.entry[] access for decision)
    uint32_t forwarding_index = SQ.SIZE;
    const SQ_FwdInfo* fwd_info = nullptr;
    const auto* matching_entries = sq_address_map.get_matching_entries(cpu, lqe.virtual_address);

    if (matching_entries != nullptr) {
        for (const auto& e : *matching_entries) {
            if ((rob_index != ROB.head) && (lqe.producer_id == e.instr_id)) {
                forwarding_index = e.sq_index;
                fwd_info = &e;
                break;
            }

            if ((lqe.producer_id == UINT64_MAX) && (lqe.instr_id <= e.instr_id)) {
                lqe.physical_address = 0;
                lqe.translated = 0;
                lqe.fetched = 0;
            }
        }
    }

    if (forwarding_index != SQ.SIZE && fwd_info) {
        if ((fwd_info->fetched == COMPLETED) && (PCYCLE_LE(fwd_info->event_cycle, PACK_CYCLE(current_core_cycle[cpu])))) {
            lqe.physical_address = (fwd_info->physical_address & ~(uint64_t)((1 << LOG2_BLOCK_SIZE) - 1)) | (lqe.virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
            lqe.translated = COMPLETED;
            lqe.fetched = COMPLETED;

            uint32_t fwr_rob_index = lqe.rob_index;
            ROB.entry[fwr_rob_index].num_mem_ops--;
            { uint64_t _now = PACK_CYCLE(current_core_cycle[cpu]);
              rob_events.per_cpu[cpu].ec_write(fwr_rob_index, _now, _now); }
            SANITY_NUM_MEM_OPS_NONNEG(fwr_rob_index);
            if (ROB.entry[fwr_rob_index].num_mem_ops == 0)
                inflight_mem_executions++;

            release_load_queue(lq_index);
        }
    }

    robe.source_added[data_index] = 1;

    if (lqe.virtual_address && (lqe.producer_id != UINT64_MAX)) {
        if (forwarding_index == SQ.SIZE ||
            (fwd_info && fwd_info->fetched != COMPLETED) ||
            (fwd_info && PCYCLE_GT(fwd_info->event_cycle, PACK_CYCLE(current_core_cycle[cpu])))) {
            lq_pending_loads.add_pending_load(cpu, lqe.virtual_address, lq_index);
        }
    }

    if (lqe.virtual_address && (lqe.producer_id == UINT64_MAX)) {
        RTL0[RTL0_tail] = lq_index;
        RTL0_tail++;
        if (RTL0_tail == LQ_SIZE)
            RTL0_tail = 0;
    }
}

void O3_CPU::add_store_queue(const uint16_t rob_index, const uint32_t data_index) {
    uint16_t sq_index = SQ.tail;
    SANITY_SQ_ENTRY_EMPTY(sq_index);

    auto& robe = ROB.entry[rob_index];
    auto& sqe = SQ.entry[sq_index];

    // add it to the store queue
    robe.sq_index[data_index] = sq_index;
    sqe.instr_id = robe.instr_id;
    sqe.virtual_address = robe.destination_memory[data_index];

    sq_address_map.add_sq_entry(cpu, sqe.virtual_address, sq_index, sqe.instr_id);

    sqe.ip = robe.ip;
    sqe.data_index = data_index;
    sqe.rob_index = rob_index;
    sqe.asid[0] = robe.asid[0];
    sqe.asid[1] = robe.asid[1];
    sqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu] + SCHEDULING_LATENCY);

    SQ.occupancy++;
    SQ.tail++;
    if (SQ.tail == SQ.SIZE)
        SQ.tail = 0;

    // succesfully added to the store queue
    robe.destination_added[data_index] = 1;

    STA[STA_head] = UINT64_MAX;
    STA_head++;
    if (STA_head == STA_SIZE)
        STA_head = 0;

    RTS0[RTS0_tail] = sq_index;
    RTS0_tail++;
    if (RTS0_tail == SQ_SIZE)
        RTS0_tail = 0;

}
const PACKET SQ_TLB_DATA_TEMPLATE = []{
    PACKET p;
    p.fill_level = FILL_L1;
    p.tlb_access = 1;
    p.type = RFO;
    return p;
}();
// Template for operate_lsq load (LQ) packets - TLB access
const PACKET LQ_TLB_DATA_TEMPLATE = []{
    PACKET p;
    p.fill_level = FILL_L1;
    p.type = LOAD;
    return p;
}();
void O3_CPU::operate_lsq() {
    if (__builtin_expect( (RTS0[RTS0_head] >= SQ_SIZE && RTS1[RTS1_head] >= SQ_SIZE &&
        RTL0[RTL0_head] >= LQ_SIZE && RTL1[RTL1_head] >= LQ_SIZE), 0 ))
        return;

    const uint64_t curr_cy = current_core_cycle[cpu];
    const uint64_t core_cycle_packed     = PACK_CYCLE(curr_cy);

    // handle store
    uint32_t store_issued = 0, num_iteration = 0;

    PACKET sq_data_packet = SQ_TLB_DATA_TEMPLATE;
    while (store_issued < SQ_WIDTH) {
        if (RTS0[RTS0_head] < SQ_SIZE) {
            uint16_t sq_index = RTS0[RTS0_head];
            auto& sqe = SQ.entry[sq_index];
            const uint64_t sqe_ev = sqe.event_cycle;
            if (PCYCLE_LE(sqe_ev, core_cycle_packed)) {

                // add it to DTLB
                sq_data_packet.cpu = cpu;
                sq_data_packet.data_index = sqe.data_index;
                sq_data_packet.sq_index = sq_index;
                sq_data_packet.address = sqe.virtual_address >> LOG2_PAGE_SIZE;
                sq_data_packet.full_addr = sqe.virtual_address;
                sq_data_packet.instr_id = sqe.instr_id;
                sq_data_packet.rob_index = sqe.rob_index;
                sq_data_packet.ip = sqe.ip;
                sq_data_packet.asid[0] = sqe.asid[0];
                sq_data_packet.asid[1] = sqe.asid[1];
                sq_data_packet.event_cycle = sqe_ev;
                int rq_index = DTLB.add_rq(&sq_data_packet);

                if (rq_index == -2)
                    break;
                else
                    sqe.translated = INFLIGHT;

                RTS0[RTS0_head] = SQ_SIZE;
                RTS0_head++;
                if (RTS0_head == SQ_SIZE)
                    RTS0_head = 0;

                store_issued++;
            }
            else
                break; // P24: head not ready, no further progress this call
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (store_issued < SQ_WIDTH) {
        if (RTS1[RTS1_head] < SQ_SIZE) {
            uint16_t sq_index = RTS1[RTS1_head];
            auto& sqe = SQ.entry[sq_index];
            if (PCYCLE_LE(sqe.event_cycle, core_cycle_packed)) {
                execute_store(sqe.rob_index, sq_index, sqe.data_index);

                RTS1[RTS1_head] = SQ_SIZE;
                RTS1_head++;
                if (RTS1_head == SQ_SIZE)
                    RTS1_head = 0;

                store_issued++;
            }
            else
                break; // P24: head not ready, no further progress this call
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    unsigned load_issued = 0;
    num_iteration = 0;
    PACKET lq_data_packet = LQ_TLB_DATA_TEMPLATE;
    while (load_issued < LQ_WIDTH) {
        if (RTL0[RTL0_head] < LQ_SIZE) {
            const uint32_t lq_index = RTL0[RTL0_head];
            auto& lqe = LQ.entry[lq_index];
            const uint64_t lqe_ev = lqe.event_cycle;
            if (PCYCLE_LE(lqe_ev, core_cycle_packed)) {

                // add it to DTLB
                lq_data_packet.cpu = cpu;
                lq_data_packet.data_index = lqe.data_index;
                lq_data_packet.lq_index = lq_index;
                lq_data_packet.address = lqe.virtual_address >> LOG2_PAGE_SIZE;
                lq_data_packet.full_addr = lqe.virtual_address;
                lq_data_packet.instr_id = lqe.instr_id;
                lq_data_packet.rob_index = lqe.rob_index;
                lq_data_packet.ip = lqe.ip;
                lq_data_packet.asid[0] = lqe.asid[0];
                lq_data_packet.asid[1] = lqe.asid[1];
                lq_data_packet.event_cycle = lqe_ev;
                int rq_index = DTLB.add_rq(&lq_data_packet);

                if (rq_index == -2)
                    break; // break here
                else
                    lqe.translated = INFLIGHT;

                RTL0[RTL0_head] = LQ_SIZE;
                RTL0_head++;
                if (RTL0_head == LQ_SIZE)
                    RTL0_head = 0;

                load_issued++;
            }
            else
                break; // P24: head not ready, no further progress this call
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (RTL1[RTL1_head] < LQ_SIZE) {
            const uint32_t lq_index = RTL1[RTL1_head];
            auto& lqe = LQ.entry[lq_index];
            if (PCYCLE_LE(lqe.event_cycle, core_cycle_packed)) {
                int rq_index = execute_load(lqe.rob_index, lq_index, lqe.data_index);

                if (rq_index != -2) {
                    RTL1[RTL1_head] = LQ_SIZE;
                    RTL1_head++;
                    if (RTL1_head == LQ_SIZE)
                        RTL1_head = 0;

                    load_issued++;
                }
            }
            else
                break; // P24: head not ready, no further progress this call
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ_SIZE-1))
            break;
    }
}
emhash7::HashSet<uint16_t>* pending_lqs;
emhash7::HashSet<uint16_t> lqs_to_process;
void O3_CPU::execute_store(const uint16_t rob_index, uint32_t sq_index, const uint32_t data_index) {
    auto* sq_entry = &SQ.entry[sq_index];
    auto* rob_entry = &ROB.entry[rob_index];

    sq_entry->fetched = COMPLETED;
    sq_entry->event_cycle = PACK_CYCLE(current_core_cycle[cpu]);
    sq_address_map.update_fetched(cpu, sq_entry->virtual_address, sq_index, COMPLETED, sq_entry->event_cycle);

    rob_entry->num_mem_ops--;
    // TRACK_MEMOPS(rob_index, " ROB IDX STORE_FWD");
    { uint64_t _now = PACK_CYCLE(current_core_cycle[cpu]);
      rob_events.per_cpu[cpu].ec_write(rob_index, _now, _now); }
    SANITY_NUM_MEM_OPS_NONNEG_PTR(rob_entry);
    inflight_mem_executions += (rob_entry->num_mem_ops == 0);  // BRANCHLESS

    if (rob_entry->is_producer) {
        pending_lqs = lq_pending_loads.get_pending_loads(cpu, sq_entry->virtual_address);

        if (pending_lqs != nullptr) {
            lqs_to_process = *pending_lqs;

            // HOIST LOOP-INVARIANT LOADS
            uint64_t sq_instr_id = sq_entry->instr_id;
            uint64_t sq_phys_addr = sq_entry->physical_address;
            uint64_t addr_mask = ~(uint64_t)((1 << LOG2_BLOCK_SIZE) - 1);
            uint64_t offset_mask = (1 << LOG2_BLOCK_SIZE) - 1;
            uint64_t sq_phys_base = sq_phys_addr & addr_mask;

            for (uint16_t lq_index : lqs_to_process) {
                SANITY_LQ_IDX_BOUND(lq_index);
                auto* lq_entry = &LQ.entry[lq_index];

                if (lq_entry->producer_id != sq_instr_id)
                    continue;

                lq_entry->physical_address = sq_phys_base | (lq_entry->virtual_address & offset_mask);
                lq_entry->translated = COMPLETED;
                lq_entry->fetched = COMPLETED;
                lq_entry->event_cycle = PACK_CYCLE(current_core_cycle[cpu]);

                uint32_t fwr_rob_index = lq_entry->rob_index;
                auto* fwr_rob_entry = &ROB.entry[fwr_rob_index];
                fwr_rob_entry->num_mem_ops--;
                // TRACK_MEMOPS(fwr_rob_index, "STORE_FWD")
                { uint64_t _now = PACK_CYCLE(current_core_cycle[cpu]);
                  rob_events.per_cpu[cpu].ec_write(fwr_rob_index, _now, _now); }
                SANITY_NUM_MEM_OPS_NONNEG_PTR(fwr_rob_entry);
                inflight_mem_executions += (fwr_rob_entry->num_mem_ops == 0);  // BRANCHLESS

                lq_pending_loads.remove_pending_load(cpu, lq_entry->virtual_address, lq_index);
                release_load_queue(lq_index);
            }
        }
    }
}

const PACKET EXEC_LOAD_DATA_TEMPLATE = []{
    PACKET p;
    p.fill_level = FILL_L1;  // Common constant
    p.type = LOAD;           // Common constant
    return p;
}();
int O3_CPU::execute_load(const uint16_t rob_index, const uint32_t lq_index, const uint32_t data_index) {

    // add it to L1D
    // Copy from template (avoids constructor cost)
    PACKET data_packet = EXEC_LOAD_DATA_TEMPLATE;
    auto& lqe = LQ.entry[lq_index];
    data_packet.cpu = cpu;
    data_packet.data_index = lqe.data_index;
    data_packet.lq_index = lq_index;
    data_packet.address = lqe.physical_address >> LOG2_BLOCK_SIZE;
    data_packet.full_addr = lqe.physical_address;
    data_packet.instr_id = lqe.instr_id;
    data_packet.rob_index = lqe.rob_index;
    data_packet.ip = lqe.ip;
    data_packet.type = LOAD;
    data_packet.asid[0] = lqe.asid[0];
    data_packet.asid[1] = lqe.asid[1];
    data_packet.event_cycle = lqe.event_cycle;
    int rq_index = 0;

    // if (LQ.entry[lq_index].instr_id == 1074723) {
    //     if (check_rob(LQ.entry[lq_index].instr_id)) {
    //         int buggROB = rob_hash_table.get_rob_idx(0,LQ.entry[lq_index].instr_id, ROB_SIZE);
    //         cout << " instr " << LQ.entry[lq_index].instr_id << " ENTRY EXIST rob: " << buggROB  << endl;
    //     } else {
    //         cout << " ENTRY NOT!!! EXIST" << endl;
    //     }
    // }

// #ifdef BYPASS_DEBUG
//     uint64_t aAddr = data_packet.address;
//     if ((data_packet.rob_index == 27 && (int)data_packet.instr_id == 1074715) || aAddr == 1862882905264)
//         cerr << __func__ << " CAUGHT DEADLOCK ROB!!!" << endl;
//     cout << " Before execLoad LQ: " << lq_index << "instrID: " << data_packet.instr_id << " rob: " << rob_index << " addr:" << data_packet.address << endl;
// #endif

#ifdef BYPASS_L1_LOGIC
    // if (L1D.MSHR.occupancy >= L1D_MSHR_SIZE)
    //     (L2C.is_bypassing & BYP_L1_BIT) = true;
    // else
    //     (L2C.is_bypassing & BYP_L1_BIT) = false;
    // #ifdef BYPASS_L1D_OnNewMiss
    // int isMiss = L1D.check_hit(&data_packet) < 0;
    // int mshr_idx = L1D.probe_mshr(&data_packet);
    // if (isMiss && L1D.probe_mshr(&data_packet) != -1 && shall_L1D_Bypass_OnCacheMissedMSHRcap(L1D, L2C)) {
    //     data_packet.l1_bypassed = 1;
    //     data_packet.fill_level = FILL_L2;
    //     rq_index = L2C.add_rq(&data_packet);
    // } else 
    // #endif
    if (!(L2C.is_bypassing & BYP_L1_BIT)) {
// #ifdef BYPASS_DEBUG
//         cout << __func__ << " addL1D RQ ... " << endl;
// #endif

        rq_index = L1D.add_rq(&data_packet);

    } else {
        #ifdef BYPASS_L1D_OnNewMiss
        // SANITY CHECK 
        if ((L2C.is_bypassing & BYP_L1_BIT)){
            assert(0&&"BOTH L2 PREFETCHER TRIGGER BYPASS + CACHE LEVEL L1D RQ BASED BYPASS ARE ACTIVE");
        }
        #endif 
        if ((L2C.is_bypassing & BYP_L1_BIT)) {
            DP( do { if (DP_GATE_WW(current_core_cycle[cpu],cpu,data_packet.address,data_packet.instr_id)) {
                cout << __func__ << " L2C ByP Entry: instrID:" << data_packet.instr_id << " addr:" << hex << data_packet.address << dec << " rob:" << data_packet.rob_index << endl;
            } } while(0) );
            if (warmup_complete[cpu]){
                data_packet.l1_bypassed = 1;
                data_packet.fill_level = FILL_L2;
                rq_index = L2C.add_rq(&data_packet);
            }
// #ifdef BYPASS_DEBUG
//             cout << " rq_index: " << rq_index << endl;
// #endif
        }
    }
#else
    rq_index = L1D.add_rq(&data_packet);
#endif



    if (rq_index == -2)
        return rq_index;
    else
        lqe.fetched = INFLIGHT;
// #ifdef BYPASS_DEBUG
//     aAddr = data_packet.address;
//     if ((data_packet.rob_index == 27 && (int)data_packet.instr_id == 1074715) || aAddr == 1862882905264)
//         cerr << __func__ << " CAUGHT DEADLOCK ROB!!!" << endl;
//     cout << " After execLoad LQ: " << lq_index << "instrID: " << data_packet.instr_id << " rob: " << rob_index << " addr:" << data_packet.address << endl;
// #endif

    return rq_index;
}

inline void O3_CPU::complete_execution(const uint16_t rob_index) {
    const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, rob_index);
    auto& robe = ROB.entry[rob_index];
    if (is_mem && robe.num_mem_ops != 0) return;

    // LOAD-BEARING INVARIANT (BRANCH-RMGATE): this early-return is the EXACT negation of the
    // former per-bit PCYCLE_LE gate in update_rob's completion scan. It makes complete_execution
    // a no-op for not-ready entries, so update_rob calls it UNCONDITIONALLY. Removing this guard
    // would silently break that contract (re-scanned bits would complete early). Do not remove.
    if (robe.executed != INFLIGHT ||
        PCYCLE_GT(rob_events.per_cpu[cpu].event_cycle[rob_index], PACK_CYCLE(current_core_cycle[cpu]))) return;

    robe.executed = COMPLETED;
    BS_CLR(rob_events.per_cpu[cpu].exec_inflight, rob_index);
    inflight_mem_executions -= is_mem;
    inflight_reg_executions -= !is_mem;
    completed_executions++;

    addr_dependencies.remove_producer(rob_index, &robe);
    if (robe.reg_RAW_producer)
        reg_RAW_release(rob_index);

    if (flat_branch_mispredicted.branch_mispredicted[cpu][rob_index])
        fetch_resume_cycle = PACK_CYCLE(current_core_cycle[cpu] + BRANCH_MISPREDICT_PENALTY);
}

inline void O3_CPU::reg_RAW_release(const uint16_t rob_index) {

    auto* dep_vec = reg_dep_tracker.get(rob_index);
    if (dep_vec) {
        for (auto& [dep_idx, src_idx] : *dep_vec) {
            ROB.entry[dep_idx].num_reg_dependent--;

            if (ROB.entry[dep_idx].num_reg_dependent == 0) {
                BS_SET(rob_events.per_cpu[cpu].reg_ready, dep_idx);
                if (BS_TST(rob_events.per_cpu[cpu].is_mem, dep_idx))
                    BS_SET(rob_events.per_cpu[cpu].sched_inflight, dep_idx);
                else {
                    BS_SET(rob_events.per_cpu[cpu].sched_complete, dep_idx);

                    SANITY_RTE_TAIL_INVALID(RTE0, RTE0_tail);
                    RTE0[RTE0_tail] = dep_idx;
                    RTE0_tail++;
                    if (RTE0_tail == ROB_SIZE)
                        RTE0_tail = 0;
                }
            }
        }
        reg_dep_tracker.remove(rob_index);
    }
}

void O3_CPU::operate_cache() {
    ITLB.operate();
    DTLB.operate();
    STLB.operate();
    L1I.operate();
    L1D.operate();
    L2C.operate();
}

void O3_CPU::update_rob() {
    // core_cycle_packed-hoist: current_core_cycle[cpu] is invariant across this function and its
    // callees (only writer is main_loop's per-tick increment). Hoist the repeated read.
    const uint64_t now = PACK_CYCLE(current_core_cycle[cpu]);
    if (ITLB.PROCESSED.occupancy && (PCYCLE_LE(ITLB.PROCESSED.entry[ITLB.PROCESSED.head].event_cycle, now)))
        complete_instr_fetch(&ITLB.PROCESSED, 1);

    if (L1I.PROCESSED.occupancy && (PCYCLE_LE(L1I.PROCESSED.entry[L1I.PROCESSED.head].event_cycle, now)))
        complete_instr_fetch(&L1I.PROCESSED, 0);

    if (DTLB.PROCESSED.occupancy && (PCYCLE_LE(DTLB.PROCESSED.entry[DTLB.PROCESSED.head].event_cycle, now)))
        complete_data_fetch(&DTLB.PROCESSED, 1);

    if (L1D.PROCESSED.occupancy && (PCYCLE_LE(L1D.PROCESSED.entry[L1D.PROCESSED.head].event_cycle, now)))
        complete_data_fetch(&L1D.PROCESSED, 0);
#ifdef BYPASS_L1_LOGIC
    if (L2C.PROCESSED.occupancy && (PCYCLE_LE(L2C.PROCESSED.entry[L2C.PROCESSED.head].event_cycle, now))) {
        // cout << " \n L2C: updateROB complete_data_fetch " << " instrID: " << L2C.PROCESSED.entry[L2C.PROCESSED.head].instr_id << " addr: " << L2C.PROCESSED.entry[L2C.PROCESSED.head].address << " cy:" << L2C.PROCESSED.entry[L2C.PROCESSED.head].event_cycle << endl;
        // cerr << " updateROB() L2C: " <<  (int) L2C.PROCESSED.entry[L2C.PROCESSED.head].l1_bypassed << "/" << (int) L2C.PROCESSED.entry[L2C.PROCESSED.head].l2_bypassed << "/" << (int) L2C.PROCESSED.entry[L2C.PROCESSED.head].llc_bypassed << endl;
#ifdef BYPASS_SANITY_CHECK
        if (!L2C.PROCESSED.entry[L2C.PROCESSED.head].l1_bypassed && !L2C.PROCESSED.entry[L2C.PROCESSED.head].l2_bypassed) {
            cerr << "ooo_instrID: " << L2C.PROCESSED.entry[L2C.PROCESSED.head].instr_id << " addr: " << L2C.PROCESSED.entry[L2C.PROCESSED.head].address << " type: " << L2C.PROCESSED.entry[L2C.PROCESSED.head].type << endl;
            assert(0&&"Why L2 COMPLETES NON BYPASS???? ");
        }
#endif
        complete_data_fetch(&L2C.PROCESSED, 0);
    }
#endif



    // update ROB entries with completed executions — bitset skip scan
    if ((inflight_reg_executions > 0) || (inflight_mem_executions > 0)) {
        const uint64_t* exec_inflight_ptr = rob_events.per_cpu[cpu].exec_inflight;
        const uint64_t* event_cycle_ptr = rob_events.per_cpu[cpu].event_cycle;
        const uint64_t core_cycle_packed = now;

        auto scan_range = [&](uint32_t start, uint32_t end) {
            const uint32_t ws = start >> 6;
            const uint32_t we = (end + 63) >> 6;
            for (uint32_t w = ws; w < we && w < ROB_WORDS; w++) {
                uint64_t word = exec_inflight_ptr[w];
                if (w == ws) word &= ~((1ULL << (start & 63)) - 1);
                if (w == we - 1 && (end & 63)) word &= (1ULL << (end & 63)) - 1;
                while (word) {
                    uint32_t b = __builtin_ctzll(word);
                    uint32_t idx = w * 64 + b;
                    // BRANCH-RMGATE: per-bit PCYCLE_LE gate removed (redundant + unpredictable).
                    // complete_execution early-returns on not-ready entries (see invariant there),
                    // leaving the exec_inflight bit set for re-scan next cycle — bit-exact.
                    complete_execution(idx);
                    word &= word - 1;
                }
            }
        };

        if (ROB.head < ROB.tail) {
            scan_range(ROB.head, ROB.tail);
        } else {
            scan_range(ROB.head, ROB.SIZE);
            scan_range(0, ROB.tail);
        }
    }
}

void O3_CPU::complete_instr_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb) {
    uint32_t index = queue->head,
            rob_index = queue->entry[index].rob_index,
            num_fetched = 0;

    SANITY_CHECK_ROB_MATCH(queue, index, rob_index);

    // update ROB entry
    if (is_it_tlb) {
        ROB.entry[rob_index].translated = COMPLETED;
        ROB.entry[rob_index].instruction_pa = (queue->entry[index].instruction_pa << LOG2_PAGE_SIZE) | (ROB.entry[rob_index].ip & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
    }
    else {
        BS_CLR(rob_events.per_cpu[cpu].fetched_inflight, rob_index);
        BS_SET(rob_events.per_cpu[cpu].fetched_complete, rob_index);
    }
    rob_events.per_cpu[cpu].ec_write(rob_index, PACK_CYCLE(current_core_cycle[cpu]), PACK_CYCLE(current_core_cycle[cpu]));
    num_fetched++;

    // check if other instructions were merged
    if (queue->entry[index].instr_merged) {
        for (auto i : queue->entry[index].rob_index_depend_on_me) {
            // update ROB entry
            if (is_it_tlb) {
                ROB.entry[i].translated = COMPLETED;
                ROB.entry[i].instruction_pa = (queue->entry[index].instruction_pa << LOG2_PAGE_SIZE) | (ROB.entry[i].ip & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            }
            else {
                BS_CLR(rob_events.per_cpu[cpu].fetched_inflight, i);
                BS_SET(rob_events.per_cpu[cpu].fetched_complete, i);
            }
            rob_events.per_cpu[cpu].ec_write(i, PACK_CYCLE(current_core_cycle[cpu] + (num_fetched / FETCH_WIDTH)), PACK_CYCLE(current_core_cycle[cpu]));
            num_fetched++;
        }
    }

    // remove this entry
    queue->remove_queue(&queue->entry[index]);
}

void O3_CPU::complete_data_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb) {
    uint32_t index = queue->head;
    uint32_t rob_index = queue->entry[index].rob_index;
    uint32_t sq_index = queue->entry[index].sq_index;
    uint32_t lq_index = queue->entry[index].lq_index;

    SANITY_CHECK_ROB_RFO(queue, index, rob_index);

    // update ROB entry
    if (is_it_tlb) { // DTLB

        if (queue->entry[index].type == RFO) {
            auto& sqe = SQ.entry[sq_index];
            sqe.physical_address = (queue->entry[index].data_pa << LOG2_PAGE_SIZE) | (sqe.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            sqe.translated = COMPLETED;
            sqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);
            sq_address_map.update_physical_address(cpu, sqe.virtual_address, sq_index, sqe.physical_address);

            RTS1[RTS1_tail] = sq_index;
            RTS1_tail++;
            if (RTS1_tail == SQ_SIZE)
                RTS1_tail = 0;

            handle_merged_translation(&queue->entry[index]);
        }
        else {
            auto& lqe = LQ.entry[lq_index];
            lqe.physical_address = (queue->entry[index].data_pa << LOG2_PAGE_SIZE) | (lqe.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            lqe.translated = COMPLETED;
            lqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);

            RTL1[RTL1_tail] = lq_index;
            RTL1_tail++;
            if (RTL1_tail == LQ_SIZE)
                RTL1_tail = 0;
            IF_HERMES(
            if (lqe.went_offchip_pred && knob::enable_ddrp)
                issue_ddrp_request(lq_index, 0);
            )

            handle_merged_translation(&queue->entry[index]);
        }

        rob_events.per_cpu[cpu].ec_write(rob_index, PACK_CYCLE(queue->entry[index].event_cycle), PACK_CYCLE(current_core_cycle[cpu]));
    }
    else { // L1D or ByP L2C
        DP( do { if (queue->entry[index].l1_bypassed && (queue->entry[index].type != LOAD)
                && DP_GATE_WW(current_core_cycle[cpu],cpu,queue->entry[index].address,queue->entry[index].instr_id)) {
            cerr << "  ByP complete_data_fetch NOT LOAD! iid:" << queue->entry[index].instr_id << " type:" << (int)queue->entry[index].type << endl;
        } } while(0) );
        if (queue->entry[index].type == RFO)
            handle_merged_load(&queue->entry[index]);
        else {
            SANITY_NOT_STORE_MERGED(queue->entry[index]);
            auto& lqe = LQ.entry[lq_index];
            auto& robe = ROB.entry[rob_index];
            lqe.fetched = COMPLETED;
            lqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);
            robe.num_mem_ops--;
            // TRACK_MEMOPS(rob_index, "CDF");
            rob_events.per_cpu[cpu].ec_write(rob_index, PACK_CYCLE(queue->entry[index].event_cycle), PACK_CYCLE(current_core_cycle[cpu]));

            SANITY_ROB_NUM_MEM_OPS_DETAILED(rob_index, queue->entry[index]);
            if (robe.num_mem_ops == 0)
                inflight_mem_executions++;

            IF_DEBUG_PRINT(
            if (DP_GATE_WW(current_core_cycle[cpu], cpu, queue->entry[index].address, queue->entry[index].instr_id)) {
                cout << "[CDF_PRE_HML] cpu:" << cpu
                     << " iid:" << queue->entry[index].instr_id
                     << " addr:0x" << hex << queue->entry[index].address << dec
                     << " lq:" << lq_index
                     << " card:" << queue->entry[index].lq_index_depend_on_me.card
                     << " bits[0]:0x" << hex << queue->entry[index].lq_index_depend_on_me.data.bits[0]
                     << " bits[1]:0x" << queue->entry[index].lq_index_depend_on_me.data.bits[1]
                     << " bits[2]:0x" << queue->entry[index].lq_index_depend_on_me.data.bits[2]
                     << " bits[3]:0x" << queue->entry[index].lq_index_depend_on_me.data.bits[3] << dec
                     << endl;
            }
            )
            release_load_queue(lq_index);
            handle_merged_load(&queue->entry[index]);
        }
    }
    // remove this entry
    queue->remove_queue(&queue->entry[index]);
}

// void O3_CPU::handle_o3_fetch(PACKET *current_packet, uint32_t cache_type) {
//     const uint16_t rob_index = current_packet->rob_index,
//             sq_index  = current_packet->sq_index,
//             lq_index  = current_packet->lq_index;

//     // update ROB entry
//     if (cache_type == 0) { // DTLB

//         #ifdef TRUE_SANITY_CHECK
//             if (rob_index != check_rob(current_packet->instr_id))
//                 assert(0);
//         #endif
//         if (current_packet->type == RFO) {
//             SQ.entry[sq_index].physical_address = (current_packet->data_pa << LOG2_PAGE_SIZE) | (SQ.entry[sq_index].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
//             SQ.entry[sq_index].translated = COMPLETED;

//             RTS1[RTS1_tail] = sq_index;
//             RTS1_tail++;
//             if (RTS1_tail == SQ_SIZE)
//             RTS1_tail = 0;

//             handle_merged_translation(current_packet);
//         } else {
//             LQ.entry[lq_index].physical_address = (current_packet->data_pa << LOG2_PAGE_SIZE) | (LQ.entry[lq_index].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
//             LQ.entry[lq_index].translated = COMPLETED;

//             RTL1[RTL1_tail] = lq_index;
//             RTL1_tail++;
//             if (RTL1_tail == LQ_SIZE)
//             RTL1_tail = 0;

//             handle_merged_translation(current_packet);
//         }
//         rob_events.entries[cpu][rob_index].event_cycle = current_packet->event_cycle;
//     } else { // L1D
//         if (current_packet->type == RFO)
//             handle_merged_load(current_packet);
//         else { // do traditional things
//             #ifdef TRUE_SANITY_CHECK
//             if (rob_index != check_rob(current_packet->instr_id))
//                 assert(0);

//             if (current_packet->store_merged)
//                 assert(0);
//             #endif
//             LQ.entry[lq_index].fetched = COMPLETED;
//             ROB.entry[rob_index].num_mem_ops--;

//             #ifdef TRUE_SANITY_CHECK
//             if (ROB.entry[rob_index].num_mem_ops < 0) {
//                 cerr << "instr_id: " << ROB.entry[rob_index].instr_id << endl;
//                 assert(0);
//             }
//             #endif
//             if (ROB.entry[rob_index].num_mem_ops == 0)
//                 inflight_mem_executions++;
//             release_load_queue(lq_index);
//             handle_merged_load(current_packet);
//             rob_events.entries[cpu][rob_index].event_cycle = current_packet->event_cycle;
//         }
//     }
// }

void O3_CPU::handle_merged_translation(PACKET *provider) {
    if (provider->store_merged) {
        ITERATE_SET(merged, provider->sq_index_depend_on_me, SQ_SIZE) {
            auto& sqe = SQ.entry[merged];
            sqe.translated = COMPLETED;
            sqe.physical_address = (provider->data_pa << LOG2_PAGE_SIZE) | (sqe.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            sqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);
            sq_address_map.update_physical_address(cpu, sqe.virtual_address, merged, sqe.physical_address);

            RTS1[RTS1_tail] = merged;
            RTS1_tail++;
            if (RTS1_tail == SQ_SIZE)
                RTS1_tail = 0;

        }
    }
    if (provider->load_merged) {
        ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE) {
            auto& lqe = LQ.entry[merged];
            lqe.translated = COMPLETED;
            lqe.physical_address = (provider->data_pa << LOG2_PAGE_SIZE) | (lqe.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            lqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);

            RTL1[RTL1_tail] = merged;
            RTL1_tail++;
            if (RTL1_tail == LQ_SIZE)
                RTL1_tail = 0;
            IF_HERMES(
            if (lqe.went_offchip_pred && knob::enable_ddrp)
                issue_ddrp_request(merged, 1);
            )

        }
    }
}

void O3_CPU::handle_merged_load(PACKET *provider) {
    IF_DEBUG_PRINT(
    if (DP_GATE_WW(current_core_cycle[cpu], cpu, provider->address, provider->instr_id)) {
        cout << "[HML_ENTRY] cpu:" << cpu
             << " iid:" << provider->instr_id
             << " addr:0x" << hex << provider->address << dec
             << " card:" << provider->lq_index_depend_on_me.card
             << " bits[0]:0x" << hex << provider->lq_index_depend_on_me.data.bits[0]
             << " bits[1]:0x" << provider->lq_index_depend_on_me.data.bits[1]
             << " bits[2]:0x" << provider->lq_index_depend_on_me.data.bits[2]
             << " bits[3]:0x" << provider->lq_index_depend_on_me.data.bits[3] << dec
             << endl;
    }
    )
    ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE) {
        auto& lqe = LQ.entry[merged];
        // VB fix to prevent leak.
        if (lqe.instr_id == 0) continue;
        if ((lqe.physical_address >> LOG2_BLOCK_SIZE) != provider->address) continue;

        uint32_t merged_rob_index = lqe.rob_index;

        lqe.fetched = COMPLETED;
        lqe.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);
        ROB.entry[merged_rob_index].num_mem_ops--;
        IF_DEBUG_PRINT(
        if (DP_GATE_WW(current_core_cycle[cpu], cpu, provider->address, provider->instr_id)) {
            cout << "[MEMOP HML] lq:" << merged
                 << " lq_instr:" << lqe.instr_id
                 << " lq_vaddr:" << hex << lqe.virtual_address << dec
                 << " lq_rob:" << lqe.rob_index
                 << " rob_instr:" << ROB.entry[lqe.rob_index].instr_id
                 << " prov_instr:" << provider->instr_id
                 << " prov_addr:" << hex << provider->address << dec
                 << endl;
        }
        )
        { uint64_t _now = PACK_CYCLE(current_core_cycle[cpu]);
          rob_events.per_cpu[cpu].ec_write(merged_rob_index, _now, _now); }

        SANITY_ROB_NUM_MEM_OPS_AT_MERGE(merged_rob_index);

        if (ROB.entry[merged_rob_index].num_mem_ops == 0)
            inflight_mem_executions++;

        release_load_queue(merged);
    }
}

void O3_CPU::release_load_queue(uint16_t lq_index) {
    auto& lqe = LQ.entry[lq_index];
    IF_HERMES(
    offchip_pred_stats_and_train(lq_index);
    if (lqe.ocp_feature) {
        delete lqe.ocp_feature;
        lqe.ocp_feature = NULL;
    }
    )
    // release LQ entries
    if (lqe.producer_id != UINT64_MAX) {
        lq_pending_loads.remove_pending_load(cpu, lqe.virtual_address, lq_index);
    }
    lqe.quickReset();
    free_LQueue.free_lq(cpu, lq_index);
    LQ.occupancy--;
}

// Template for retire_rob store packets
const PACKET RETIRE_ROB_STORE_DATA_TEMPLATE = []{
    PACKET p;
    p.fill_level = FILL_L1;
    p.type = RFO;
    return p;
}();


void O3_CPU::retire_rob() {
    for (uint32_t n=0; n<RETIRE_WIDTH; n++) {
        auto& rhe = ROB.entry[ROB.head];
        if (rhe.ip == 0)
            return;

        // retire is in-order
        if (rhe.executed != COMPLETED) {
            return;
        }

        // check store instruction
        uint32_t num_store = 0;
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (rhe.destination_memory[i])
                num_store++;
        }

        if (num_store) {
            if ((L1D.WQ.occupancy + num_store) <= L1D.WQ.SIZE) {
                for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
                    if (rhe.destination_memory[i]) {

                        uint16_t sq_index = rhe.sq_index[i];
                        auto& sqe = SQ.entry[sq_index];
                        PACKET data_packet;

                        // sq_index and rob_index are no longer available after retirement
                        // but we pass this information to avoid segmentation fault
                        data_packet.fill_level = FILL_L1;
                        data_packet.cpu = cpu;
                        data_packet.data_index = sqe.data_index;
                        data_packet.sq_index = sq_index;
                        data_packet.address = sqe.physical_address >> LOG2_BLOCK_SIZE;
                        data_packet.full_addr = sqe.physical_address;
                        data_packet.instr_id = sqe.instr_id;
                        data_packet.rob_index = sqe.rob_index;
                        data_packet.ip = sqe.ip;
                        data_packet.type = RFO;
                        data_packet.asid[0] = sqe.asid[0];
                        data_packet.asid[1] = sqe.asid[1];
                        data_packet.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);

                        L1D.add_wq(&data_packet);
                    }
                }
            }
            else {

                L1D.WQ.FULL++;
                L1D.STALL[RFO]++;

                return;
            }
        }

        // release SQ entries
        if (num_store) for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (rhe.sq_index[i] != UINT16_MAX) {
                uint16_t sq_index = rhe.sq_index[i];
                auto& sqe = SQ.entry[sq_index];

                sq_address_map.remove_sq_entry(cpu, sqe.virtual_address, sq_index);

                sqe.quickReset();
                SQ.occupancy--;
                SQ.head++;
                if (SQ.head == SQ.SIZE)
                    SQ.head = 0;
            }
        }
        if (num_retired >= warmup_instructions && num_retired < warmup_instructions + simulation_instructions)
            execution_checksum[cpu] ^= rhe.ip;
        // execution_checksum[cpu] = (execution_checksum[cpu]) ^ ROB.entry[ROB.head].ip;
        // release ROB entry
#ifdef DEBUG_PRINT
        // if (ROB.entry[ROB.head].instr_id == 1074723 || ROB.head == 35)
        //     cout << " CATCH RETIRE OF BUG instrID: " << ROB.entry[ROB.head].instr_id << " head: " << ROB.head <<  endl;
#endif

#ifdef TRUE_SANITY_CHECK
        rob_hash_table.retire_rob_idx(cpu, rhe.instr_id);  // paired with add_rob_idx; sanity-only (see add)
#endif
        mem_dependencies.remove_producer(ROB.head, &rhe);

        rob_memory_count[cpu] -= BS_TST(rob_events.per_cpu[cpu].is_mem, ROB.head);
        rob_events.clear_entry(cpu, ROB.head);
        flat_branch_mispredicted.branch_mispredicted[cpu][ROB.head] = 0;

        rhe.quickReset();
        ROB.head++;
        if (ROB.head == ROB.SIZE)
            ROB.head = 0;
        if (next_mem_sched_start[cpu] == ROB.head - 1 || (ROB.head == 0 && next_mem_sched_start[cpu] == ROB.SIZE - 1))
            next_mem_sched_start[cpu] = ROB.head;
        ROB.occupancy--;
        completed_executions--;
        num_retired++;
    }
}