#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>

// w64devkit (native Windows) lacks the POSIX bzero; map to standard memset (string.h above
// provides it). Same platform-guard idiom as block.h's #ifndef _WIN32.
#ifdef _WIN32
#ifndef bzero
#define bzero(ptr, len) memset((ptr), 0, (len))
#endif
#endif

#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <iomanip>
#include "defs.h"

#include "unordered_dense.h"

// include ooo cpu h for pf_stat_num_retired var 


// USEFUL MACROS

// comment out disables it, uncomment enables it
// #define TRUE_SANITY_CHECK 1
// #define SANITY_CHECK 1
// #define SANITY_CYCLES_PACK_CHECK
// #define REPLACEMENT_SANITY_CHECK
// #define DRAM_SANITY_CHECK
// #define main_SANITY_CHECK
// #define BYPASS_SANITY_CHECK
 

#define BYPASS_L1D_OnNewMiss
#define BYPASS_LOGIC
#define BYPASS_L1_LOGIC
#define BYPASS_L2_LOGIC    // uncomment at Stage 2
#define BYPASS_LLC_LOGIC   // uncomment at Stage 3
// #define BYPASS_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS

// ---- Function-like guard wrappers (readability: replace inline #ifdef clutter) ----
// Each IF_*(...) expands to its argument when the guard is defined, to nothing
// otherwise. TOKEN-IDENTICAL to the old #ifdef GUARD ... #endif — zero codegen change.
#ifdef BYPASS_L1_LOGIC
  #define IF_BYP_L1(...) __VA_ARGS__
#else
  #define IF_BYP_L1(...)
#endif
#ifdef BYPASS_L2_LOGIC
  #define IF_BYP_L2(...) __VA_ARGS__
#else
  #define IF_BYP_L2(...)
#endif
#ifdef BYPASS_LLC_LOGIC
  #define IF_BYP_LLC(...) __VA_ARGS__
#else
  #define IF_BYP_LLC(...)
#endif
#ifdef BYPASS_L1D_OnNewMiss
  #define IF_BYP_L1_ONNEWMISS(...) __VA_ARGS__
#else
  #define IF_BYP_L1_ONNEWMISS(...)
#endif
#ifdef USE_HERMES
  #define IF_HERMES(...) __VA_ARGS__
#else
  #define IF_HERMES(...)
#endif
#ifdef USE_LLC_HASHMAP_MSHR
  #define IF_LLC_HASHMAP(...) __VA_ARGS__
#else
  #define IF_LLC_HASHMAP(...)
#endif
#ifdef LLC_BYPASS
  #define IF_LLC_BYPASS(...) __VA_ARGS__
#else
  #define IF_LLC_BYPASS(...)
#endif

// ---- Hermes (off-chip predictor + DDRP) ----
// #define USE_HERMES

/* ===== LPM Tracker Switches ===== */
#define TRACKER_LPM_SHARED       /* ON = LLC shared (orig); OFF = LLC per-core */
// #define LPM_DYNAMIC_WIN       /* ON = dynamic window LPM; OFF = fixed window */
#define LPM_LONG_SHORT_WIN       /* ON = add long(32x)/short window pair for bypass models */
/* ================================ */

// ---- Bypass Derivative Fill (BDF): fill bypassed level directly on lower-level fill ----
// Uncomment exactly ONE method (or none to disable derivative fill)
#define BYP_DERFILL_IMMEDIATE     // Method 1: directly fill upper cache same cycle (0 extra latency)
// #define BYP_DERFILL_SEQUENTIAL // Method 2: inject completed MSHR at upper, fills in upper_LATENCY cycles

#if defined(BYP_DERFILL_SEQUENTIAL) || defined(BYP_DERFILL_IMMEDIATE)
#define BYP_DERFILL_ACTIVE
#endif

#define ROB_FLAGS
// #define LLC_BYPASS // EITHER WAY IS NOT RUNNING
#define DRC_BYPASS
#define NO_CRC2_COMPILE


#define DEBUG_FROM_CY 22541000
#define DEBUG_FROM_INSTR 0
#define DEBUG_ADDR 0
// #define DEBUG_ADDR 11423564796656
// #define DEBUG_FROM_CY 863000000
// #define DEBUG_CPU 15
// #define DEBUG_FROM_CY 4295984663
#define DEBUG_CPU 4
// #define DEBUG_PRINT_WITH_WARMUP
// #define BYPASS_DEBUG
// #define DEBUG_PRINT

// Comment out next line to SUPPRESS DP during warmup (default behavior).
// Uncomment to PRINT DP during warmup too (useful for pre-warmup crash debug).
#ifdef DEBUG_PRINT
    #define DP(x) x
    #define _DP_CY_OK(cy)       ((uint64_t)(cy) >= (uint64_t)DEBUG_FROM_CY)
    #define _DP_INSTR_OK(iid)   ((DEBUG_FROM_INSTR == 0) || ((uint64_t)(iid) >= (uint64_t)DEBUG_FROM_INSTR))
    #define _DP_CPU_OK(cpu_val) ((DEBUG_CPU == -1) || ((int)(cpu_val) == (int)DEBUG_CPU))
    #define _DP_ADDR_OK(addr)   ((DEBUG_ADDR == 0) || ((uint64_t)(addr) == (uint64_t)DEBUG_ADDR) || ((uint64_t)(addr) == ((uint64_t)DEBUG_ADDR >> LOG2_BLOCK_SIZE)))
    #define DP_GATE(cy, cpu_val) \
        (_DP_CY_OK(cy) && _DP_CPU_OK(cpu_val))
    #define DP_GATE_I(cy, cpu_val, iid, addr) \
        ((_DP_CY_OK(cy) || _DP_INSTR_OK(iid)) && _DP_CPU_OK(cpu_val) && _DP_ADDR_OK(addr))
    #else
    #define DP(x)
    #define DP_GATE(cy, cpu_val) (false)
    #define DP_GATE_I(cy, cpu_val, iid, addr) (false)
#endif

// ============================================================
// Per-call-site DP macros — one line w/ context + compact dump.
// Prefix [NAME_func][TAG] msg | dump_req_min_* | endl
// Gated by DP_GATE(cycle, cpu).
// ============================================================
// Unified warmup-aware gate: respects DEBUG_PRINT_WITH_WARMUP toggle.
#ifdef DEBUG_PRINT_WITH_WARMUP
#define DP_WARM_OK(cpu_val) (true)
#else
#define DP_WARM_OK(cpu_val) (warmup_complete[(cpu_val)] != 0)
#endif

#define DP_GATE_WW(cy, cpu_val, addr, iid) (DP_GATE(cy, cpu_val) && DP_WARM_OK(cpu_val) && _DP_ADDR_OK(addr) && _DP_INSTR_OK(iid))

#ifdef DEBUG_PRINT
#define _DPM_HEAD(TAG, msg) std::cout << "[" << std::setw(20) << std::left << (std::string(NAME) + "_" + __func__) << "][" << std::setw(5) << TAG << "] " << std::setw(22) << std::left << msg << " "
#define DP_READ_M(msg, pkt)  do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("READ",  msg); dump_req_min_read(pkt);  std::cout << std::endl; } } while(0)
#define DP_WRITE_M(msg, pkt) do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("WRITE", msg); dump_req_min_write(pkt); std::cout << std::endl; } } while(0)
#define DP_MSHR_M(msg, pkt)  do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("MSHR",  msg); dump_req_min_mshr(pkt);  std::cout << std::endl; } } while(0)
#define DP_RET_M(msg, pkt)   do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("RET",   msg); dump_req_min_ret(pkt);   std::cout << std::endl; } } while(0)
#define DP_PQ_M(msg, pkt)    do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("PQ",    msg); dump_req_min_pq(pkt);    std::cout << std::endl; } } while(0)
#define DP_FILL_M(msg, pkt)  do { if (DP_GATE_WW(current_core_cycle[(pkt)->cpu], (pkt)->cpu, (pkt)->address, (pkt)->instr_id)) { \
    _DPM_HEAD("FILL",  msg); dump_req_min_fill(pkt);  std::cout << std::endl; } } while(0)
#else
#define DP_READ_M(msg, pkt)
#define DP_WRITE_M(msg, pkt)
#define DP_MSHR_M(msg, pkt)
#define DP_RET_M(msg, pkt)
#define DP_PQ_M(msg, pkt)
#define DP_FILL_M(msg, pkt)
#endif

// CPU
#define NUM_CPUS 8
#define CPU_FREQ 4000
#define DRAM_IO_FREQ 3200 // DDR4-2400
#define PAGE_SIZE 4096
#define LOG2_PAGE_SIZE 12

// CACHE
#define BLOCK_SIZE 64
#define LOG2_BLOCK_SIZE 6
#define MAX_READ_PER_CYCLE 16
#define MAX_FILL_PER_CYCLE 1
#define PACKET_DEBUG
#define INFLIGHT 1
#define COMPLETED 2

#define DEADLOCK_CYCLE 20000000

#define FILL_L1    1
#define FILL_L2    2
#define FILL_LLC   4
#define FILL_DRC   8
#define FILL_DRAM 16
#ifdef USE_HERMES
#define FILL_DDRP 24  // must fit in 5-bit fill_level bitfield (max 31); reference uses 9
#define NUM_PARTITION_TYPES 4
#define DRAM_BW_LEVELS 4
#endif

// DRAM 4GB 
// #define DRAM_CHANNELS 4 
// #define LOG2_DRAM_CHANNELS 2 
// #define DRAM_RANKS 2 
// #define LOG2_DRAM_RANKS 1 
// #define DRAM_BANKS 8 
// #define LOG2_DRAM_BANKS 3


// DRAM 2GB 
// #define DRAM_CHANNELS 2      // default: assuming one DIMM per one channel 4GB * 1 => 4GB off-chip memory
// #define LOG2_DRAM_CHANNELS 1
// #define DRAM_RANKS 2         // 512MB * 8 ranks => 4GB per DIMM
// #define LOG2_DRAM_RANKS 1
// #define DRAM_BANKS 8         // 64MB * 8 banks => 512MB per rank
// #define LOG2_DRAM_BANKS 3

// #define DRAM_ROWS 65536      // 2KB * 32K rows => 64MB per bank
// #define LOG2_DRAM_ROWS 16
// #define DRAM_COLUMNS 128      // 64B * 32 column chunks (Assuming 1B DRAM cell * 8 chips * 8 transactions = 64B size of column chunks) => 2KB per row
// #define LOG2_DRAM_COLUMNS 7
// #define DRAM_ROW_SIZE (BLOCK_SIZE*DRAM_COLUMNS/1024)

// #define DRAM_SIZE (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024) 
// #define DRAM_PAGES ((DRAM_SIZE<<10)>>2) 
//#define DRAM_PAGES 10

using namespace std;

extern uint8_t warmup_complete[NUM_CPUS], 
               simulation_complete[NUM_CPUS], 
               all_warmup_complete, 
               all_simulation_complete,
               MAX_INSTR_DESTINATIONS,
               knob_cloudsuite,
               knob_low_bandwidth;

extern uint64_t current_core_cycle[NUM_CPUS],
                last_drc_read_mode,
                last_drc_write_mode,
                drc_blocks;
extern uint64_t stall_cycle[NUM_CPUS];

extern queue <uint64_t> page_queue;
// extern map <uint64_t, uint64_t> page_table, inverse_table, recent_page, unique_cl[NUM_CPUS];
// In champsim.h line 107:
extern ankerl::unordered_dense::map<uint64_t, uint64_t> page_table, inverse_table, recent_page, unique_cl[NUM_CPUS];
extern uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS], allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS], major_fault[NUM_CPUS];

void print_stats();
constexpr uint64_t rotl64 (uint64_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT*sizeof(n)-1);
    c &= mask;
    return (n<<c) | (n>>( (-c)&mask ));
}
constexpr uint64_t rotr64 (uint64_t n, unsigned int c) {
    const unsigned int mask = (CHAR_BIT*sizeof(n)-1);
    c &= mask;
    return (n>>c) | (n<<( (-c)&mask ));
}
uint64_t va_to_pa(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage);

// log base 2 function from efectiu
constexpr int lg2(int n) {
    int m = n, c = -1;
    for (; m; m /= 2) { c++; }
    return c;
}

// smart random number generator
class RANDOM {
  public:
    std::random_device rd;
    std::mt19937_64 engine{rd()};
    std::uniform_int_distribution<uint64_t> dist{0, 0xFFFFFFFFF}; // used to generate random physical page numbers

    RANDOM (uint64_t seed) {
        engine.seed(seed);
    }

    uint64_t draw_rand() {
        return dist(engine);
    };
};
extern uint64_t champsim_seed;
#endif
