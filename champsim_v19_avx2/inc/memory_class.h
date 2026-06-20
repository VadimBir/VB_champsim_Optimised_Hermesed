#ifndef MEMORY_CLASS_H
#define MEMORY_CLASS_H

#include "champsim.h"
#include "block.h"

// CACHE ACCESS TYPE
#define LOAD      0
#define RFO       1
#define PREFETCH  2
#define WRITEBACK 3
#define NUM_TYPES 4

extern uint32_t tRP,  // Row Precharge (RP) latency
                tRCD, // Row address to Column address (RCD) latency
                tCAS; // Column Address Strobe (CAS) latency

extern uint64_t l2pf_access;

class MEMORY {
  public:
    // memory interface
    MEMORY *upper_level_icache[NUM_CPUS], *upper_level_dcache[NUM_CPUS], *lower_level, *extra_interface;

    // empty queues
    PACKET_QUEUE WQ{"EMPTY", 1}, RQ{"EMPTY", 1}, PQ{"EMPTY", 1}, MSHR{"EMPTY", 1};

    // functions
    virtual int  add_rq(PACKET *packet) = 0;
    virtual int  add_wq(PACKET *packet) = 0;
    virtual int  add_pq(PACKET *packet) = 0;
    virtual void return_data(PACKET *packet) = 0;
    virtual void operate() = 0;
    virtual void increment_WQ_FULL(uint64_t address) = 0;
    virtual uint32_t get_occupancy(uint8_t queue_type, uint64_t address) const = 0;
    virtual uint32_t get_size(uint8_t queue_type, uint64_t address) const = 0;

    // stats
    uint64_t ACCESS[NUM_TYPES], HIT[NUM_TYPES], MISS[NUM_TYPES], MSHR_MERGED[NUM_TYPES], STALL[NUM_TYPES];
    // Pure MSHR-admission stall: counts ONLY the RQ-side MSHR-table-full refusal
    // (handle_read_miss_mshr_full). Excludes the dirty-victim/lower-WQ-full fill stall
    // that also lands in STALL[]. At L2 this isolates true admission pressure (~0 when MSHR>upstream).
    uint64_t pure_MSHR_Admission_STALL[NUM_TYPES];

    MEMORY() {
        for (uint32_t i=0; i<NUM_TYPES; i++) {
            ACCESS[i] = 0;
            HIT[i] = 0;
            MISS[i] = 0;
            MSHR_MERGED[i] = 0;
            STALL[i] = 0;
            pure_MSHR_Admission_STALL[i] = 0;
        }
    }
};

class BANK_REQUEST {
  public:
    // NOTE: .working in MEMORY_CONTROLLER::bank_hot union (entries[ch][rk][bk].working).
    //       .cycle_available in MEMORY_CONTROLLER::bank_cycle_available[ch][rk][bk] (uint64 — widened
    //       from 32-bit to prevent 4.3B-cycle wrap-deadlock).
    // DEAD-2026-05-25: address, full_addr — zero direct reads via bank_request[ch][rk][bk].address/full_addr
    // uint64_t address,
    //          full_addr;

    uint32_t open_row;

    uint8_t working_type,
            row_buffer_hit,
            // DEAD-2026-05-25: drc_hit — zero reads via bank_request[ch][rk][bk].drc_hit
            // drc_hit,
            is_write,
            is_read;

    int request_index;

    BANK_REQUEST() {
        // DEAD-2026-05-25: address/full_addr inits
        // address = 0;
        // full_addr = 0;

        open_row = UINT32_MAX;

        working_type = 0;
        row_buffer_hit = 0;
        // DEAD-2026-05-25: drc_hit init
        // drc_hit = 0;
        is_write = 0;
        is_read = 0;

        request_index = -1;
    };
};

#endif
