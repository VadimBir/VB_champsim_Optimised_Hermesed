#ifndef DRAM_H
#define DRAM_H

#include "memory_class.h"

// DRAM configuration
#include "defs.h"

extern uint32_t DRAM_MTPS, DRAM_DBUS_RETURN_TIME;

 
void print_dram_config();
 
// DRAM

// C2: hot-field union mirroring rob_events pattern.
// .working (1B) stays in 8B raw entry for fast OR-reduction. cycle_available is now
// 64-bit in a SEPARATE array (bank_cycle_available) — wider storage, no truncation wrap.
union bank_hot_t {
    uint64_t raw[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];
    struct {
        uint8_t  working;          // 1B — 0 or 1 (bank administratively held)
        uint8_t  _pad[7];          // 7B — pad to 8B (cycle_available moved to bank_cycle_available[])
    } entries[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];
};

// DRAM
class MEMORY_CONTROLLER : public MEMORY {
  public:
    const string NAME;

    DRAM_ARRAY dram_array[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];
    alignas(64) bank_hot_t bank_hot;
    uint64_t dbus_cycle_available[DRAM_CHANNELS]; // widened: full cycle (no 4.3B wrap)
    uint64_t dbus_cycle_congested[DRAM_CHANNELS];
    uint8_t  dbus_cycle_congested_ovf[DRAM_CHANNELS]; // branchless overflow flag
    uint64_t dbus_congested[NUM_TYPES+1][NUM_TYPES+1];
    uint64_t bank_cycle_available[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS]; // widened: full cycle
    uint8_t  do_write, write_mode[DRAM_CHANNELS]; 
    uint32_t processed_writes, scheduled_reads[DRAM_CHANNELS], scheduled_writes[DRAM_CHANNELS];
    int fill_level;

    BANK_REQUEST bank_request[DRAM_CHANNELS][DRAM_RANKS][DRAM_BANKS];

    uint64_t sim_read_access[NUM_CPUS];  // post-warmup LOAD completions per CPU (analogous to sim_access[cpu][LOAD] in CACHE)

    // Per-CPU DRAM queue/bank occupancy counters — replaces O(CH*(RQ+WQ)+CH*RANKS*BANKS) scan in operate().
    // rw_occ.both==0 iff both RQ and WQ are empty for this cpu (single 16-bit test).
    union DramRwOcc {
        uint16_t both;
        struct { uint8_t rq; uint8_t wq; };
    };
    alignas(64) DramRwOcc rw_occ[NUM_CPUS];         // 2B/cpu — keeps RQ and WQ independent but OR-testable

    // Opt-A: incremental bank working counter — replaces O(32) scan in operate()
    uint32_t working_bank_count = 0;

    // Opt-B: batch idle accumulator — buffers idle cycles for bulk LPM advance
    uint32_t dram_idle_accumulator = 0;
    uint8_t  has_work = 0;  // 1 when any RQ/WQ has entries OR banks working

    // queues
    PACKET_QUEUE WQ[DRAM_CHANNELS];
    PACKET_QUEUE RQ[DRAM_CHANNELS];

    // // constructor
    // MEMORY_CONTROLLER(const std::string &v1) : NAME(v1.c_str())
    // {
    //     for (uint32_t i = 0; i < NUM_TYPES + 1; i++)
    //         for (uint32_t j = 0; j < NUM_TYPES + 1; j++)
    //             dbus_congested[i][j] = 0;

    //     do_write = 0;
    //     processed_writes = 0;

    //     for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    //         dbus_cycle_available[i] = 0;
    //         dbus_cycle_congested[i] = 0;
    //         write_mode[i] = 0;
    //         scheduled_reads[i] = 0;
    //         scheduled_writes[i] = 0;

    //         for (uint32_t j = 0; j < DRAM_RANKS; j++)
    //             for (uint32_t k = 0; k < DRAM_BANKS; k++)
    //                 bank_cycle_available[i][j][k] = 0;

    //         // WQ[i] = PACKET_QUEUE("DRAM_WQ" + std::to_string(i), DRAM_WQ_SIZE);
    //         // RQ[i] = PACKET_QUEUE("DRAM_RQ" + std::to_string(i), DRAM_RQ_SIZE);
    //         new (&WQ[i]) PACKET_QUEUE(
    //             std::string("DRAM_WQ") + std::to_string(i),
    //             DRAM_WQ_SIZE
    //         );

    //         new (&RQ[i]) PACKET_QUEUE(
    //             std::string("DRAM_RQ") + std::to_string(i),
    //             DRAM_RQ_SIZE
    //         );

    //     }

    //     fill_level = FILL_DRAM;
    // }

    // constructor
    MEMORY_CONTROLLER(const std::string &v1) : NAME(v1.c_str())
    {
        for (uint32_t i = 0; i < NUM_TYPES + 1; i++)
            for (uint32_t j = 0; j < NUM_TYPES + 1; j++)
                dbus_congested[i][j] = 0;

        do_write = 0;
        processed_writes = 0;

        for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
            dbus_cycle_available[i] = 0;
            dbus_cycle_congested[i] = 0;
            dbus_cycle_congested_ovf[i] = 0;
            write_mode[i] = 0;
            scheduled_reads[i] = 0;
            scheduled_writes[i] = 0;

            for (uint32_t j = 0; j < DRAM_RANKS; j++)
                for (uint32_t k = 0; k < DRAM_BANKS; k++) {
                    bank_cycle_available[i][j][k] = 0;
                    bank_hot.raw[i][j][k] = 0;  // C2: zero working + cycle_available
                }

            {
                char buf[32];
                int n = std::snprintf(buf, sizeof(buf), "DRAM_WQ%u", i);
                new (&WQ[i]) PACKET_QUEUE(std::string(buf, (n>0 ? n : 0)), DRAM_WQ_SIZE);

                n = std::snprintf(buf, sizeof(buf), "DRAM_RQ%u", i);
                new (&RQ[i]) PACKET_QUEUE(std::string(buf, (n>0 ? n : 0)), DRAM_RQ_SIZE);
            }

        }

        fill_level = FILL_DRAM;
        for (uint32_t c = 0; c < NUM_CPUS; c++) {
            sim_read_access[c] = 0;
            rw_occ[c].both = 0;
        }
        working_bank_count = 0;
        dram_idle_accumulator = 0;
        has_work = 0;
    };


    // // queues
    // PACKET_QUEUE WQ[DRAM_CHANNELS], RQ[DRAM_CHANNELS];

    
    // // constructor
    // MEMORY_CONTROLLER(string v1) : NAME (v1) {
    //     for (uint32_t i=0; i<NUM_TYPES+1; i++) {
    //         for (uint32_t j=0; j<NUM_TYPES+1; j++) {
    //             dbus_congested[i][j] = 0;
    //         }
    //     }
    //     do_write = 0;
    //     processed_writes = 0;
    //     for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
    //         dbus_cycle_available[i] = 0;
    //         dbus_cycle_congested[i] = 0;
    //         write_mode[i] = 0;
    //         scheduled_reads[i] = 0;
    //         scheduled_writes[i] = 0;

    //         for (uint32_t j=0; j<DRAM_RANKS; j++) {
    //             for (uint32_t k=0; k<DRAM_BANKS; k++)
    //                 bank_cycle_available[i][j][k] = 0;
    //         }

    //         WQ[i].NAME = "DRAM_WQ" + to_string(i);
    //         WQ[i].SIZE = DRAM_WQ_SIZE;
    //         WQ[i].entry = new PACKET [DRAM_WQ_SIZE];

    //         RQ[i].NAME = "DRAM_RQ" + to_string(i);
    //         RQ[i].SIZE = DRAM_RQ_SIZE;
    //         RQ[i].entry = new PACKET [DRAM_RQ_SIZE];
    //     }

    //     fill_level = FILL_DRAM;
    // };
    void quickReset(){
        for (uint32_t i=0; i<NUM_TYPES+1; i++) {
            for (uint32_t j=0; j<NUM_TYPES+1; j++) {
                dbus_congested[i][j] = 0;
            }
        }
        do_write = 0;
        processed_writes = 0;
        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            dbus_cycle_available[i] = 0;
            dbus_cycle_congested[i] = 0;
            dbus_cycle_congested_ovf[i] = 0;
            write_mode[i] = 0;
            scheduled_reads[i] = 0;
            scheduled_writes[i] = 0;

            for (uint32_t j=0; j<DRAM_RANKS; j++) {
                for (uint32_t k=0; k<DRAM_BANKS; k++) {
                    bank_cycle_available[i][j][k] = 0;
                    bank_hot.raw[i][j][k] = 0;  // C2
                }
            }

            WQ[i].quick_reset();
            RQ[i].quick_reset();
        }
        for (uint32_t c = 0; c < NUM_CPUS; c++) {
            sim_read_access[c] = 0;
            rw_occ[c].both = 0;
        }
        working_bank_count = 0;
        dram_idle_accumulator = 0;
        has_work = 0;
    }

    // destructor
    ~MEMORY_CONTROLLER() {

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

    void schedule(PACKET_QUEUE *queue), process(PACKET_QUEUE *queue),
         update_schedule_cycle(PACKET_QUEUE *queue),
         update_process_cycle(PACKET_QUEUE *queue),
         reset_remain_requests(PACKET_QUEUE *queue, uint32_t channel);

    constexpr uint32_t dram_get_channel(uint64_t address) const;
    constexpr uint32_t dram_get_rank   (uint64_t address) const;
    constexpr uint32_t dram_get_bank   (uint64_t address) const;
    constexpr uint32_t dram_get_row    (uint64_t address) const;
    constexpr uint32_t dram_get_column (uint64_t address) const;
    uint32_t           drc_check_hit (uint64_t address, uint32_t cpu, uint32_t channel, uint32_t rank, uint32_t bank, uint32_t row);

    uint64_t get_bank_earliest_cycle();

    int check_dram_queue(PACKET_QUEUE *queue, PACKET *packet);

#ifdef USE_HERMES
    vector<deque<uint64_t>> ddrp_buffer;
    struct { uint64_t insert, evict, lookup, hit; } ddrp_buffer_stats;
    void init_ddrp_buffer();
    void insert_ddrp_buffer(uint64_t address);
    bool lookup_ddrp_buffer(uint64_t address);
    uint32_t get_ddrp_buffer_set_index(uint64_t address);
#endif
};

#endif