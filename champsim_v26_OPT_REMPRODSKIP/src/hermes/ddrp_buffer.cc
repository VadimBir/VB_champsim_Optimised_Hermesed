#include "champsim.h"
#ifdef USE_HERMES
// Hermes DDRP buffer (fully-associative per-set) — implementation

#include "dram_controller.h"
#include <algorithm>

namespace knob
{
    extern uint32_t dram_cntlr_ddrp_buffer_sets;
    extern uint32_t dram_cntlr_ddrp_buffer_assoc;
    extern uint32_t dram_cntlr_ddrp_buffer_hash_type;
    extern bool     dram_cntlr_enable_ddrp_buffer;
}

void MEMORY_CONTROLLER::init_ddrp_buffer() {
    ddrp_buffer.clear();
    ddrp_buffer.resize(knob::dram_cntlr_ddrp_buffer_sets);
}

uint32_t MEMORY_CONTROLLER::get_ddrp_buffer_set_index(uint64_t address) {
    uint64_t line = address >> LOG2_BLOCK_SIZE;
    if (knob::dram_cntlr_ddrp_buffer_sets == 0) return 0;
    return (uint32_t)(line % knob::dram_cntlr_ddrp_buffer_sets);
}

void MEMORY_CONTROLLER::insert_ddrp_buffer(uint64_t address) {
    if (!knob::dram_cntlr_enable_ddrp_buffer) return;
    if (ddrp_buffer.empty()) init_ddrp_buffer();
    uint32_t idx = get_ddrp_buffer_set_index(address);
    auto &set_q = ddrp_buffer[idx];
    uint64_t line = address >> LOG2_BLOCK_SIZE;
    auto it = std::find(set_q.begin(), set_q.end(), line);
    if (it != set_q.end()) set_q.erase(it);
    set_q.push_front(line);
    if (set_q.size() > knob::dram_cntlr_ddrp_buffer_assoc) {
        set_q.pop_back();
        ddrp_buffer_stats.evict++;
    }
    ddrp_buffer_stats.insert++;
}

bool MEMORY_CONTROLLER::lookup_ddrp_buffer(uint64_t address) {
    if (!knob::dram_cntlr_enable_ddrp_buffer) return false;
    if (ddrp_buffer.empty()) return false;
    uint32_t idx = get_ddrp_buffer_set_index(address);
    auto &set_q = ddrp_buffer[idx];
    uint64_t line = address >> LOG2_BLOCK_SIZE;
    ddrp_buffer_stats.lookup++;
    auto it = std::find(set_q.begin(), set_q.end(), line);
    if (it != set_q.end()) { ddrp_buffer_stats.hit++; return true; }
    return false;
}


#endif // USE_HERMES
