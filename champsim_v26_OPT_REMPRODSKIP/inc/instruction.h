/******************************************************************
 * This file defines all core+uncore configutation parameters
 * The core parameters are closely modeled after Intel Golden Cove architecture.
 * 
 * https://en.wikichip.org/wiki/intel/microarchitectures/golden_cove
 * https://www.anandtech.com/show/16881/a-deep-dive-into-intels-alder-lake-microarchitectures/3 
 *
 ******************************************************************/

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

// instruction format
#include "defs.h"
#include "instr_event.h"

#define NUM_INSTR_DESTINATIONS_SPARC 2
#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

#include "set.h"
#include <list>
#include "champsim.h"
#include <array>
#include "unordered_dense.h"
#include "svector.h"

class input_instr {
  public:

    // instruction pointer or PC (Program Counter)
    uint64_t ip;

    // branch info
    uint8_t is_branch;
    uint8_t branch_taken;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS]; // output registers
    uint8_t source_registers[NUM_INSTR_SOURCES]; // input registers

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS]; // output memory
    uint64_t source_memory[NUM_INSTR_SOURCES]; // input memory

    input_instr() {
        ip = 0;
        is_branch = 0;
        branch_taken = 0;

        for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
            source_registers[i] = 0;
            source_memory[i] = 0;
        }

        for (uint32_t i=0; i<NUM_INSTR_DESTINATIONS; i++) {
            destination_registers[i] = 0;
            destination_memory[i] = 0;
        }
    };
};


#ifdef knob_cloudsuite
    constexpr int NUM_INSTR_DESTINATIONS_X = NUM_INSTR_DESTINATIONS_SPARC; // this is used to determine the number of destination registers in the instruction
    //#define MAX_INSTR_DESTINATIONS NUM_INSTR_DESTINATIONS_X
#else
    constexpr int NUM_INSTR_DESTINATIONS_X = NUM_INSTR_DESTINATIONS; // this is used to determine the number of destination registers in the instruction
    //#define MAX_INSTR_DESTINATIONS NUM_INSTR_DESTINATIONS_X
#endif

extern uint64_t event_cycle_Arr[NUM_CPUS][ROB_SIZE];
extern int_fast8_t is_executed_Arr[NUM_CPUS][ROB_SIZE];

static const fastset template_fastset{};
static const LQ_fastset LQ_template_fastset{};
static const SQ_fastset SQ_template_fastset{};

// Centralized register dependency release tracker — replaces per-instruction fastset fields
// Key: producer rob_index → vector of (dependent_rob_idx, source_index) pairs
// C5: direct-indexed array replaces the hash map. Producer key == rob_index,
// bounded by ROB_SIZE, so it indexes deps[] directly (kills hash cost). An
// explicit `active[]` flag mirrors map presence exactly: set on add, cleared on
// remove. get() returns nullptr iff the key is absent — identical to map
// find()==end(). Each producer's own vector preserves insertion order, and the
// tracker is never iterated across producers (get/remove always take a specific
// key), so semantics are bit-exact with the map.
class RegDepReleaseTracker {
private:
    ankerl::svector<std::pair<uint16_t, uint8_t>, 8> deps[ROB_SIZE];
    bool active[ROB_SIZE];
public:
    void init() {
        for (uint32_t i = 0; i < ROB_SIZE; i++) {
            deps[i].clear();
            active[i] = false;
        }
    }

    void add(uint16_t producer, uint16_t dependent, uint8_t source_index) {
        active[producer] = true;
        deps[producer].emplace_back(dependent, source_index);
    }

    auto* get(uint16_t producer) {
        return active[producer] ? &deps[producer] : nullptr;
    }

    void remove(uint16_t producer) {
        deps[producer].clear();
        active[producer] = false;
    }
};

class alignas(8) ooo_model_instr {
  public:
    // === P14: members reordered largest-alignment-first to pack 136B -> 128B
    // (3 cache lines -> 2). Pure reorder: no field added/removed/resized and the
    // bitfield cluster's internal order is preserved, so every value and all
    // behavior is identical. Verified bit-exact via the local gate checksum. ===

    // --- 8-byte (uint64) region ---
    uint64_t instr_id : 34;
    uint64_t ip : 48;
    uint64_t producer_id;
    uint64_t instruction_pa /*, data_pa, virtual_address, physical_address — DEAD in ooo_model_instr (LQ/SQ have their own) */;
    uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC];
    uint64_t source_memory[NUM_INSTR_SOURCES];
    // destination_virtual_address/source_virtual_address: write-only, never read — removed

    // --- 4-byte (int) region ---
    int num_reg_ops, num_mem_ops, num_reg_dependent;

    // --- 2-byte (uint16) region ---
    uint16_t lq_index[NUM_INSTR_SOURCES],
             sq_index[NUM_INSTR_DESTINATIONS_SPARC];
    // DEAD-2026-05-25: forwarding_index[] — zero reads on field (local var at src/ooo_cpu.cc:1251 is distinct uint32_t scalar)

    // --- bitfield cluster (internal order preserved for layout stability) ---
    // DEAD-2026-05-25: data_translated, is_consumer, mem_ready — zero reads; bitfield decls retained for layout stability
    uint8_t is_branch :2,
            branch_taken :2,
            translated :2,
            data_translated :2,    // DEAD-2026-05-25
            is_producer :2,
            is_consumer :2,        // DEAD-2026-05-25
            reg_RAW_producer :2,
            mem_ready :2;          // DEAD-2026-05-25
    int8_t executed:4;
    uint8_t cpu:4;
    int16_t rob_index:12;

    // --- 1-byte (uint8) arrays ---
    uint8_t source_added[NUM_INSTR_SOURCES];
    uint8_t destination_added[NUM_INSTR_DESTINATIONS_SPARC];
    uint8_t asid[2];
    uint8_t reg_RAW_checked[NUM_INSTR_SOURCES];
    uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC];
    uint8_t source_registers[NUM_INSTR_SOURCES];

    inline operator int_fast8_t() const {
        return is_executed_Arr[this->cpu][this->rob_index];
    }
    inline bool operator==(int_fast8_t other) const {
        return is_executed_Arr[this->cpu][this->rob_index] == other;
    }
    inline bool operator!=(int_fast8_t other) const {
        return is_executed_Arr[this->cpu][this->rob_index] != other;
    }
    inline int_fast8_t* addr_is_executed(uint8_t cpu, uint16_t rob_index){
        return &is_executed_Arr[cpu][rob_index];
    }
    void quickReset(){
        instr_id = 0;
        ip = 0;
        producer_id = 0;

        is_branch = 0;
        branch_taken = 0;
        translated = 0;
        // DEAD-2026-05-25: data_translated write — field never read
        // data_translated = 0;
        is_producer = 0;
        // DEAD-2026-05-25: is_consumer write — field never read
        // is_consumer = 0;
        reg_RAW_producer = 0;
        rob_index = 0;
        // DEAD-2026-05-25: mem_ready write — field never read
        // mem_ready = 0;
        executed = 0;
        num_reg_ops = 0;
        num_mem_ops = 0;
        num_reg_dependent = 0;
        asid[0] = UINT8_MAX;
        asid[1] = UINT8_MAX;

        for (uint8_t i=0; i<NUM_INSTR_SOURCES; i++) {
            source_added[i] = 0;
            lq_index[i] = UINT16_MAX;
            reg_RAW_checked[i] = 0;
        }

        for (uint8_t i=0; i<NUM_INSTR_DESTINATIONS_SPARC; i++) {
            destination_memory[i] = 0;
            destination_registers[i] = 0;
            // destination_virtual_address[i] = 0;
            destination_added[i] = 0;
            sq_index[i] = UINT16_MAX;
            // DEAD-2026-05-25: forwarding_index[i] write — field never read (local var in ooo_cpu.cc:1251 is distinct)
            // forwarding_index[i] = 0;
        }
    }

    ooo_model_instr() {
        instr_id = 0;
        ip = 0;
        producer_id = 0;

        is_branch = 0;
        branch_taken = 0;
        translated = 0;
        // DEAD-2026-05-25: data_translated write — field never read
        // data_translated = 0;
        is_producer = 0;
        // DEAD-2026-05-25: is_consumer write — field never read
        // is_consumer = 0;
        reg_RAW_producer = 0;
        rob_index = 0;
        executed = 0;
        // DEAD-2026-05-25: mem_ready write — field never read
        // mem_ready = 0;
        asid[0] = UINT8_MAX;
        asid[1] = UINT8_MAX;

        num_reg_ops = 0;
        num_mem_ops = 0;
        num_reg_dependent = 0;

        for (uint8_t i=0; i<NUM_INSTR_SOURCES; i++) {
            source_registers[i] = 0;
            source_memory[i] = 0;
            // source_virtual_address[i] = 0;
            source_added[i] = 0;
            lq_index[i] = UINT16_MAX;
            reg_RAW_checked[i] = 0;
        }

        for (uint8_t i=0; i<NUM_INSTR_DESTINATIONS_SPARC; i++) {
            destination_memory[i] = 0;
            destination_registers[i] = 0;
            // destination_virtual_address[i] = 0;
            destination_added[i] = 0;
            sq_index[i] = UINT16_MAX;
            // DEAD-2026-05-25: forwarding_index[i] write — field never read
            // forwarding_index[i] = 0;
        }
    };
};

#endif
/*
*/