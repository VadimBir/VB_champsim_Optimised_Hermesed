#ifndef OOO_CPU_H
#define OOO_CPU_H

#include "cache.h"
#include "instruction.h"
#if defined(__has_feature)
#if __has_feature(memory_sanitizer)
#include <sanitizer/msan_interface.h>
#endif
#endif
// CORE PROCESSOR 
#include "defs.h"
#ifdef USE_HERMES
#include "hermes/offchip_pred_base.h"
#include "hermes/ddrp_monitor.h"
#include "hermes/offchip_tracer.h"
#endif
// #include "globals.h"

// Buffer Size is used in custom XZ decoding  
#define BUF_SZ 8


// #define PRINT_STATS_EVERY 1000000
#define PRINT_STATS_EVERY 5000000
#ifdef CRC2_COMPILE
#define STAT_PRINTING_PERIOD PRINT_STATS_EVERY // print stats every X instructions
#else
#define STAT_PRINTING_PERIOD PRINT_STATS_EVERY // print stats every X instructions
#endif
// DEADLOCK_CYCLE now in champsim.h → auto-derives CYCLE_PACK_WIDTH in cycle_pack.h

// Enable threaded trace helper: comment out to use default single-threaded handle_branch
#define USE_TRACE_HELPER

#define PROBLEM_INSTR_ID 1074715

using namespace std;

#define STA_SIZE (ROB_SIZE*NUM_INSTR_DESTINATIONS_SPARC)    // Unchanged FLAG_CONFIG same as Hermes
extern uint32_t SCHEDULING_LATENCY, EXEC_LATENCY;

void print_core_config();

#include "unordered_dense.h"
#include "svector.h"

// for every new ROB entry, we store ADDR:ROB_INDEX mapping to later O(1) get RAW for new
class  AddrDependencyTracker {
private:
    // Map: register -> ALL ROB indices that write to it (in order)
    // std::unordered_map<uint32_t, std::vector<uint16_t>> reg_producers;
    // C3A: per-register FRONT-CURSOR. `front` is the index of the first element
    //      not yet known to be a leading tombstone. Advancing `front` past leading
    //      tombstones amortizes the remove scan / reverse iteration toward O(1)
    //      without changing observable order or results: leading entries below the
    //      cursor are always TOMBSTONE, which both the remove-scan (value compare)
    //      and get_latest_producer (explicit TOMBSTONE skip) ignore anyway.
    struct ProducerList {
        uint32_t front = 0;            // front-cursor: first non-leading-tombstone slot
        std::vector<uint16_t> vec;     // ROB indices in insertion (program) order
    };
    // B2': reg-producer container hash-map -> flat array, direct index.
    // Register ids are dense uint8_t (0..255); index DIRECTLY by reg id.
    // Absent-key (map MISS) == empty default-constructed ProducerList slot.
    ProducerList reg_producers[256];
    uint16_t rob_size;
    // HOTFN-DIRTYREG: registers that received a tombstone since last compact().
    // compact() only needs to scrub these; the other ~254 slots are untouched and
    // contain zero tombstones, so erasing them is a guaranteed no-op. Visiting only
    // dirty regs yields a bit-identical result while cutting compact()'s 256-slot
    // sweep down to the handful of regs actually written this window.
    // REGBIT: dirty-register tracking as a 256-bit bitset (4x uint64).
    // bit r set == reg r received a tombstone since last compact(). Mirrors the
    // prior reg_is_dirty[256]/dirty_regs[256]/dirty_count trio exactly:
    // is-dirty == test bit, mark-dirty == set bit, "iterate dirty regs" == scan
    // the 4 words via __builtin_ctzll. Membership identical -> IPC-safe.
    uint64_t dirty_bits[4] = {};
public:
    void init() {
        // Flat-array equivalent of map.clear(): reset every slot to empty.
        for (uint32_t r = 0; r < 256; r++) {
            reg_producers[r].vec.clear();
            reg_producers[r].front = 0;
        }
        dirty_bits[0] = dirty_bits[1] = dirty_bits[2] = dirty_bits[3] = 0;
        tombstone_count = 0;
        rob_size = ROB_SIZE;

        constexpr uint8_t MAX_REGISTER_NUM = NUM_INSTR_DESTINATIONS;
        for (uint8_t reg = 0; reg < MAX_REGISTER_NUM; reg++) {
            reg_producers[reg].vec.reserve(ROB_SIZE);
        }

    }

    // Add when instruction enters ROB
    void add_producer(uint16_t rob_index, const uint8_t (&destination_registers)[NUM_INSTR_DESTINATIONS_SPARC]) {
        for (uint32_t i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            if (destination_registers[i] != 0) {
                reg_producers[destination_registers[i]].vec.emplace_back(rob_index);
            }
        }
    }

    static constexpr uint16_t TOMBSTONE = UINT16_MAX;
    uint16_t tombstone_count = 0;

    // Get latest valid producer in ROB window (early return, skips tombstones)
    int get_latest_producer(uint32_t reg, uint32_t current_rob, uint32_t rob_head) {
        auto& pl = reg_producers[reg];
        auto& vec = pl.vec;
        if (vec.empty()) return -1;

        // Reverse-iterate down to the front-cursor only. Everything below `front`
        // is a leading TOMBSTONE (which the original loop would `continue` past),
        // so bounding here yields the identical result.
        for (auto rit = vec.rbegin(); rit != vec.rend() - pl.front; ++rit) {
            if (*rit == TOMBSTONE) continue;
            if ((rob_head <= current_rob) ?
                ((*rit >= rob_head) && (*rit < current_rob)) :
                ((*rit >= rob_head) || (*rit < current_rob))) return *rit;
        }
        return -1;
    }

    // Remove when instruction RETIRES — tombstone O(1)
    void remove_producer(uint16_t rob_index, const ooo_model_instr* instr) {
        for (uint8_t i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            if (uint8_t reg = instr->destination_registers[i]) {
                // Direct index: absent-key map MISS == empty slot, which makes the
                // scan below a no-op (vec.size()==0), identical to the find()==end() skip.
                auto& pl = reg_producers[reg];
                auto& vec = pl.vec;
                const uint32_t sz = vec.size();
                uint16_t* const data = vec.data();
                // Scan from the front-cursor: slots below it are leading
                // TOMBSTONEs that can never equal a real rob_index, so skipping
                // them changes nothing about which element gets tombstoned.
                for (uint32_t k = pl.front; k < sz; ++k) {
                    if (data[k] == rob_index) {
                        data[k] = TOMBSTONE; ++tombstone_count;
                        // Mark this register dirty so compact() visits it.
                        dirty_bits[reg >> 6] |= (1ULL << (reg & 63));
                        // Advance the cursor past any now-leading tombstones.
                        uint32_t f = pl.front;
                        while (f < sz && data[f] == TOMBSTONE) ++f;
                        pl.front = f;
                        break;
                    }
                }
            }
        }
        if (tombstone_count > 32) compact();
    }

    void compact() {
        // Only dirty regs hold tombstones; all others would be no-op erases.
        // Scan the 256-bit dirty bitset word-by-word (ctz) — same dirty set as
        // the prior dirty_regs[] list, just iterated bit-by-bit. Identical effect.
        for (uint32_t w = 0; w < 4; w++) {
            uint64_t bits = dirty_bits[w];
            while (bits) {
                const uint8_t reg = (uint8_t)((w << 6) + __builtin_ctzll(bits));
                bits &= bits - 1;   // clear lowest set bit
                auto& pl = reg_producers[reg];
                auto& vec = pl.vec;
                vec.erase(std::remove(vec.begin(), vec.end(), TOMBSTONE), vec.end());
                pl.front = 0;   // all tombstones removed; reset cursor
            }
            dirty_bits[w] = 0;
        }
        tombstone_count = 0;
    }
};


class  MemDependencyTracker {
private:
    // Map: memory_address -> vector of (rob_index, dest_index) pairs
    // std::unordered_map<uint64_t, std::vector<std::pair<uint16_t, uint8_t>>> mem_producers;
    ankerl::unordered_dense::map<uint64_t, std::vector<std::pair<uint16_t, uint8_t>>> mem_producers;
public:
    void init() {
        mem_producers.clear();
        mem_producers.reserve(64);
        
        constexpr uint8_t MAX_REGISTER_NUM = NUM_INSTR_DESTINATIONS;
        for (uint8_t reg = 0; reg < MAX_REGISTER_NUM; reg++) {
            mem_producers[reg].reserve(ROB_SIZE);
        }
    }
    
    // Add when instruction with memory destinations enters ROB
    void add_producer(uint16_t rob_index, const uint64_t (&destination_memory)[NUM_INSTR_DESTINATIONS_SPARC]) {
        for (uint8_t i = 0; i < NUM_INSTR_DESTINATIONS_SPARC; i++) {
            if (destination_memory[i] != 0) {
                mem_producers[destination_memory[i]].emplace_back(rob_index, i);
            }
        }
    }
    
    // Get latest producer for a memory address - returns {rob_index, dest_index}
    std::pair<int, int> get_latest_producer(uint64_t addr, uint32_t current_rob, uint32_t rob_head) {
        auto it = mem_producers.find(addr);
        if (it == mem_producers.end()) return {-1, -1};
        
        // Check from newest to oldest
        for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit) {
            uint32_t idx = rit->first;
            
            // Check if in valid ROB window
            if ((rob_head <= current_rob) ?
                ((idx >= rob_head) && (idx < current_rob)) :
                ((idx >= rob_head) || (idx < current_rob))) {
                return {idx, rit->second};
            }
        }
        return {-1, -1};
    }
    
    // Remove when instruction completes
    void remove_producer(uint16_t rob_index, const ooo_model_instr* instr) {
        for (uint8_t i = 0; i < NUM_INSTR_DESTINATIONS_SPARC; i++) {
            if (uint64_t addr = instr->destination_memory[i]) {
                auto it = mem_producers.find(addr);
                if (it != mem_producers.end()) {
                    auto& vec = it->second;
                    for (auto iter = vec.begin(); iter != vec.end(); )
                        // *iter == rob_index ? iter = vec.erase(iter) : ++iter;
                        (iter->first == rob_index) ? iter = vec.erase(iter) : ++iter;
                    if (vec.empty()) mem_producers.erase(it);
                }
            }
        }
    }
};

#include <fstream>
#include <string>
#include <cstring>
#include <lzma.h>

class  XZReader {
private:
    uint8_t* inbuf;   // Heap pointer
    uint8_t* outbuf;  // Heap pointer
    size_t outbuf_pos;
    size_t outbuf_available;
    int consecutive_no_output = 0;
    const int MAX_NO_OUTPUT_ATTEMPTS = 5;
    std::string filename;
    // Debug counters
    size_t total_bytes_decoded;
    size_t total_items_read;
    lzma_action action;
    std::ifstream input_file;
    lzma_stream strm;
    lzma_ret ret;
    bool decoder_initialized;
    
    bool init_decoder() {
        strm = LZMA_STREAM_INIT;
        lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
        if (ret != LZMA_OK) {
            return false;
        }
        decoder_initialized = true;
        return true;
    }
    
    // This function aggressively extracts all data from the decoder
    bool fill_buffer() {
        // If we already have data, return it
        if (outbuf_pos < outbuf_available) {
            return true;
        }
        
        // Reset our output position
        outbuf_pos = 0;
        outbuf_available = 0;
        
        // State tracking for aggressive flushing
        bool input_exhausted = input_file.eof();
        bool seen_stream_end = false;
        
        
        // Keep trying until we're absolutely sure there's no more data
        while (outbuf_available == 0) {
            // Set up output buffer
            strm.next_out = outbuf;
            strm.avail_out = BUFSIZ * BUF_SZ;
            
            // Try to read more input if we haven't hit EOF and need it
            if (strm.avail_in == 0 && !input_exhausted) {
                input_file.read((char*)inbuf, BUFSIZ * BUF_SZ);
                size_t bytes_read = input_file.gcount();
                
                if (bytes_read > 0) {
                    strm.next_in = inbuf;
                    strm.avail_in = bytes_read;
                    consecutive_no_output = 0;  // Reset counter since we got new input
                } else {
                    input_exhausted = true;
                }
            }
            
            // Determine action - use FINISH when input is exhausted
            action = (input_exhausted && strm.avail_in == 0) ? LZMA_FINISH : LZMA_RUN;
            
            // Decompress
            size_t avail_out_before = strm.avail_out;
            ret = lzma_code(&strm, action);
            size_t bytes_produced = avail_out_before - strm.avail_out;
            
            // Update our total for debugging
            total_bytes_decoded += bytes_produced;
            
            if (bytes_produced > 0) {
                // Success! We got data
                outbuf_available = bytes_produced;
                return true;
            }
            
            // No output produced - increment counter
            consecutive_no_output++;
            
            // Handle return codes
            if (ret == LZMA_STREAM_END) {
                seen_stream_end = true;
                // Even with STREAM_END, keep trying a few more times
                // The decoder might still have data in its pipeline
                if (consecutive_no_output >= MAX_NO_OUTPUT_ATTEMPTS) {
                    return false;  // Really done now
                }
                // Otherwise, keep looping to ensure we've flushed everything
            } else if (ret == LZMA_OK || ret == LZMA_BUF_ERROR) {
                // These are normal - decoder just needs more input or output space
                // But if we've exhausted input and seen no output for a while, stop
                if (input_exhausted && seen_stream_end && 
                    consecutive_no_output >= MAX_NO_OUTPUT_ATTEMPTS) {
                    return false;
                }
                // Special case: LZMA_BUF_ERROR with FINISH action and no input means we're done
                if (ret == LZMA_BUF_ERROR && action == LZMA_FINISH && 
                    strm.avail_in == 0 && input_exhausted) {
                    return false;
                }
            } else {
                // Actual error
                fprintf(stderr, "XZReader: LZMA decode error %d\n", ret);
                return false;
            }
        }
        
        return false;
    }
    
public:
    // XZReader() : strm(LZMA_STREAM_INIT), outbuf_pos(0), outbuf_available(0), 
    //              decoder_initialized(false), total_bytes_decoded(0), total_items_read(0) {
    // }
    
    // ~XZReader() {
    //     close();
    // }
    XZReader() : outbuf_pos(0), outbuf_available(0),
                 total_bytes_decoded(0), total_items_read(0), decoder_initialized(false) {
        // Allocate aligned buffers
#ifdef _WIN32
        // MinGW has no posix_memalign; 64B alignment here is a cache-line perf
        // hint, not correctness. Plain malloc (16B aligned) stays free()-compatible.
        inbuf  = (uint8_t*)malloc(BUFSIZ * BUF_SZ);
        outbuf = (uint8_t*)malloc(BUFSIZ * BUF_SZ);
#else
        posix_memalign((void**)&inbuf, 64, BUFSIZ * BUF_SZ);
        posix_memalign((void**)&outbuf, 64, BUFSIZ * BUF_SZ);
#endif
        
        strm = LZMA_STREAM_INIT;
    }
    ~XZReader() {
        if (inbuf) free(inbuf);    // CHECK BEFORE FREE
        if (outbuf) free(outbuf);  // CHECK BEFORE FREE
        close();
    }
    // bool open(const char* filename) {
    //     if (!filename) return false;
        
    //     // #if defined(__has_feature)
    //     // #if __has_feature(memory_sanitizer)
    //     // // Unpoison first 4096 bytes to allow strlen to work
    //     // __msan_unpoison(filename, 4096);
    //     // size_t len = strlen(filename);
    //     // __msan_unpoison(filename, len + 1);  // Now unpoison exact length
    //     // #endif
    //     // #endif
        
    //     this->filename = filename;
    //     input_file.open(filename, std::ios::binary);
    //     if (!input_file.good()) {
    //         return false;
    //     }
        
    //     if (!init_decoder()) {
    //         input_file.close();
    //         return false;
    //     }
        
    //     strm.next_in = nullptr;
    //     strm.avail_in = 0;
        
    //     outbuf_pos = 0;
    //     outbuf_available = 0;
    //     total_bytes_decoded = 0;
    //     total_items_read = 0;
        
    //     return true;
    // }
    bool open(const char* filename) {
        if (!filename) return false;
        this->filename = filename;
        input_file.open(filename, std::ios::binary);
        if (!input_file.good()) {
            return false;
        }
        
        if (!init_decoder()) {
            input_file.close();
            return false;
        }
        
        strm.next_in = nullptr;
        strm.avail_in = 0;
        
        outbuf_pos = 0;
        outbuf_available = 0;
        total_bytes_decoded = 0;
        total_items_read = 0;
        
        return true;
    }
    
    size_t read(void* ptr, size_t size, size_t count) {
        size_t total_bytes_needed = size * count;
        size_t bytes_copied = 0;
        uint8_t* dest = (uint8_t*)ptr;
        
        // Keep trying to read until we have enough bytes or truly hit EOF
        while (bytes_copied < total_bytes_needed) {
            // Make sure we have data in our buffer
            if (outbuf_pos >= outbuf_available) {
                if (!fill_buffer()) {
                    // No more data available
                    break;
                }
            }
            
            // Copy what we can from our buffer
            size_t available = outbuf_available - outbuf_pos;
            size_t needed = total_bytes_needed - bytes_copied;
            size_t to_copy = (available < needed) ? available : needed;
            
            memcpy(dest + bytes_copied, outbuf + outbuf_pos, to_copy);
            // cache_line_usage_tracker += to_copy;
            bytes_copied += to_copy;
            outbuf_pos += to_copy;
        }
        
        // Calculate complete items (matching fread behavior)
        size_t items = bytes_copied / size;
        total_items_read += items;
        
        // Debug output for partial reads
        // if (bytes_copied > 0 && bytes_copied < total_bytes_needed && eof()) {
        //     fprintf(stderr, "XZReader: Partial read at EOF - got %zu bytes, wanted %zu bytes\n",
        //             bytes_copied, total_bytes_needed);
        //     fprintf(stderr, "XZReader: Total items read: %zu, Total bytes decoded: %zu\n",
        //             total_items_read, total_bytes_decoded);
        //     if (bytes_copied % size != 0) {
        //         fprintf(stderr, "XZReader WARNING: %zu leftover bytes (not a complete item)\n",
        //                 bytes_copied % size);
        //     }
        // }
        
        return items;
    }
    
    bool reopen() {
        close();
        return open(filename.c_str());
    }
    
    void close() {
        if (decoder_initialized) {
            lzma_end(&strm);
            decoder_initialized = false;
        }
        if (input_file.is_open()) {
            input_file.close();
        }
        strm = LZMA_STREAM_INIT;
        outbuf_pos = 0;
        outbuf_available = 0;
        total_bytes_decoded = 0;
        total_items_read = 0;
    }
    
    bool eof() const {
        // EOF means we can't get any more data from fill_buffer
        // This is checked by seeing if we have no buffered data and can't get more
        if (outbuf_pos < outbuf_available) {
            return false;  // Still have buffered data
        }
        // Try to fill buffer (const_cast is ugly but necessary here)
        return !const_cast<XZReader*>(this)->fill_buffer();
    }
};

#include <vector>
#include <list>
#include <set>

// cpu
class alignas(64) O3_CPU {
  public:

    // CUSTOM VB
    AddrDependencyTracker addr_dependencies; // Keep Hashmap to keep track of all ADDR:ROB_indx. Gets all ROB for given addr at once (15% imprvnt)
    MemDependencyTracker mem_dependencies;
    RegDepReleaseTracker reg_dep_tracker; // Replaces per-instruction fastset dependency fields
    // std::array<std::vector<uint32_t>, 2> sig_list; // ROB TRAVERSAL REDUCTION + REDUCE cmp
        
    // END CUSTOM VB
    int  cpu;

    // trace
    FILE *trace_file;
    char trace_string[1024];
    char gunzip_command[1024];

    // instruction
    input_instr current_instr;
    // cloudsuite_instr current_cloudsuite_instr;
    uint64_t instr_unique_id, completed_executions, 
             begin_sim_cycle, begin_sim_instr, 
             last_sim_cycle, last_sim_instr,
             finish_sim_cycle, finish_sim_instr,
             warmup_instructions, simulation_instructions, instrs_to_read_this_cycle, /* DEAD-2026-05-25: instrs_to_fetch_this_cycle, */
             next_print_instruction, num_retired;
    // DEAD-2026-05-25: num_searched — zero reads on field (local in ooo_cpu.cc:1025 is separate)
    uint32_t inflight_reg_executions, inflight_mem_executions /*, num_searched */;
    uint32_t next_ITLB_fetch;

    // reorder buffer, load/store queue, register file
    CORE_BUFFER ROB{"ROB", ROB_SIZE};
    LOAD_STORE_QUEUE LQ{"LQ", LQ_SIZE}, SQ{"SQ", SQ_SIZE};
    
    // store array, this structure is required to properly handle store instructions
    uint64_t STA[STA_SIZE], STA_head, STA_tail; 

    // Ready-To-Execute
    uint32_t RTE0[ROB_SIZE], RTE0_head, RTE0_tail, 
             RTE1[ROB_SIZE], RTE1_head, RTE1_tail;  

    // Ready-To-Load
    uint32_t RTL0[LQ_SIZE], RTL0_head, RTL0_tail, 
             RTL1[LQ_SIZE], RTL1_head, RTL1_tail;  

    // Ready-To-Store
    uint32_t RTS0[SQ_SIZE], RTS0_head, RTS0_tail,
             RTS1[SQ_SIZE], RTS1_head, RTS1_tail;

    // branch
    int branch_mispredict_stall_fetch; // flag that says that we should stall because a branch prediction was wrong
    int mispredicted_branch_iw_index; // index in the instruction window of the mispredicted branch.  fetch resumes after the instruction at this index executes
    uint8_t  fetch_stall;
    uint64_t fetch_resume_cycle;
    uint64_t num_branch, branch_mispredictions;
    uint64_t total_rob_occupancy_at_branch_mispredict;

    // TLBs and caches
    CACHE ITLB{"ITLB", ITLB_SET, ITLB_WAY, ITLB_SET*ITLB_WAY, ITLB_WQ_SIZE, ITLB_RQ_SIZE, ITLB_PQ_SIZE, ITLB_MSHR_SIZE},
          DTLB{"DTLB", DTLB_SET, DTLB_WAY, DTLB_SET*DTLB_WAY, DTLB_WQ_SIZE, DTLB_RQ_SIZE, DTLB_PQ_SIZE, DTLB_MSHR_SIZE},
          STLB{"STLB", STLB_SET, STLB_WAY, STLB_SET*STLB_WAY, STLB_WQ_SIZE, STLB_RQ_SIZE, STLB_PQ_SIZE, STLB_MSHR_SIZE},
          L1I{"L1I", L1I_SET, L1I_WAY, L1I_SET*L1I_WAY, L1I_WQ_SIZE, L1I_RQ_SIZE, L1I_PQ_SIZE, L1I_MSHR_SIZE},
          L1D{"L1D", L1D_SET, L1D_WAY, L1D_SET*L1D_WAY, L1D_WQ_SIZE, L1D_RQ_SIZE, L1D_PQ_SIZE, L1D_MSHR_SIZE},
          L2C{"L2C", L2C_SET, L2C_WAY, L2C_SET*L2C_WAY, L2C_WQ_SIZE, L2C_RQ_SIZE, L2C_PQ_SIZE, L2C_MSHR_SIZE};

    bool is_xz_trace; // flag to indicate if the trace is in xz format
    XZReader xz_reader;
    int progressive_completion_count = 0;
    uint32_t unretired_timeout = 0;
    void verify_xz_trace() {
        if (!is_xz_trace) return;
        
        input_instr test_instr;
        
        size_t items_read = xz_reader.read(&test_instr, sizeof(input_instr), 1);
        
        if (items_read != 1) {
            std::fprintf(stderr, "[TRACE_ERROR] read failed\n");
            assert(false);
        }
        
        // xz library is not MSan-instrumented - manually mark buffer as initialized
        volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(&test_instr);
        for (size_t i = 0; i < sizeof(test_instr); i++) {
            (void)p[i];  // Force MSan to see the memory
        }
        #ifdef __has_feature
        #if __has_feature(memory_sanitizer)
        __msan_unpoison(&test_instr, sizeof(test_instr));
        #endif
        #endif
        if (test_instr.ip == 0 || test_instr.ip == 0xFFFFFFFFFFFFFFFF) {
            std::fprintf(stderr, "[TRACE_ERROR] Invalid ip=0x%lx\n", test_instr.ip);
            assert(false);
        }
        
        if (!xz_reader.reopen()) {
            std::fprintf(stderr, "[TRACE_ERROR] reopen failed\n");
            assert(false);
        }
    }


    // constructor
    O3_CPU() {

        is_xz_trace = false;
        addr_dependencies.init();
        mem_dependencies.init();
        reg_dep_tracker.init();


        cpu = 0;
        // trace
        trace_file = NULL;

        // instruction
        instr_unique_id = 0;
        completed_executions = 0;
        begin_sim_cycle = 0;
        begin_sim_instr = 0;
        last_sim_cycle = 0;
        last_sim_instr = 0;
        finish_sim_cycle = 0;
        finish_sim_instr = 0;
        warmup_instructions = 0;
        simulation_instructions = 0;
        instrs_to_read_this_cycle = 0;
        // DEAD-2026-05-25: instrs_to_fetch_this_cycle init
        // instrs_to_fetch_this_cycle = 0;

        next_print_instruction = STAT_PRINTING_PERIOD;
        num_retired = 0;

        inflight_reg_executions = 0;
        inflight_mem_executions = 0;
        // DEAD-2026-05-25: num_searched (class field) init
        // num_searched = 0;

        next_ITLB_fetch = 0;

        // branch
        branch_mispredict_stall_fetch = 0;
        mispredicted_branch_iw_index = 0;
        fetch_stall = 0;
	fetch_resume_cycle = 0;
        num_branch = 0;
        branch_mispredictions = 0;

        for (uint32_t i=0; i<STA_SIZE; i++)
            STA[i] = UINT64_MAX;
        STA_head = 0;
        STA_tail = 0;

        for (uint32_t i=0; i<ROB_SIZE; i++) {
            RTE0[i] = ROB_SIZE;
            RTE1[i] = ROB_SIZE;
        }
        RTE0_head = 0;
        RTE1_head = 0;
        RTE0_tail = 0;
        RTE1_tail = 0;

        for (uint32_t i=0; i<LQ_SIZE; i++) {
            RTL0[i] = LQ_SIZE;
            RTL1[i] = LQ_SIZE;
        }
        RTL0_head = 0;
        RTL1_head = 0;
        RTL0_tail = 0;
        RTL1_tail = 0;

        for (uint32_t i=0; i<SQ_SIZE; i++) {
            RTS0[i] = SQ_SIZE;
            RTS1[i] = SQ_SIZE;
        }
        RTS0_head = 0;
        RTS1_head = 0;
        RTS0_tail = 0;
        RTS1_tail = 0;
    }
    
    ~O3_CPU() {
    }

    void handle_branch(),
        fetch_instruction(),
        schedule_instruction(), 
        execute_instruction(),
        schedule_memory_instruction();
    void execute_memory_instruction();
    void do_scheduling(uint16_t rob_index);

    void reg_dependency_simd_v2(uint16_t rob_index),
        apply_raw_dependency(uint32_t producer_idx, uint32_t consumer_idx, uint32_t source_idx),
        operate_lsq();
    void reg_dependency(uint16_t rob_index);
    void do_execution(uint16_t rob_index);
    void do_memory_scheduling(uint16_t rob_index);
    void complete_execution(uint16_t rob_index);
    void reg_RAW_dependency(uint16_t prior, uint16_t current, uint8_t source_index),
        reg_RAW_release(uint16_t rob_index),
        handle_o3_fetch(PACKET *current_packet, uint32_t cache_type),
        handle_merged_translation(PACKET *provider),
        handle_merged_load(PACKET *provider),
        complete_instr_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb),
        complete_data_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb);
    void release_load_queue(uint16_t lq_index);
        
        void initialize_core();
        void add_load_queue(uint16_t rob_index, uint32_t data_index),
        add_store_queue(uint16_t rob_index, uint32_t data_index),
        execute_store(uint16_t rob_index, uint32_t sq_index, uint32_t data_index);
        int  execute_load(uint16_t rob_index, uint32_t sq_index, uint32_t data_index);
        void check_dependency(int prior, int current);
        void operate_cache();
        void update_rob();
        void retire_rob();
        void progressive_memory_completion(uint16_t rob_index);
    
    
        uint32_t  add_to_rob(ooo_model_instr *arch_instr),
        check_rob(uint64_t instr_id);
        
        uint32_t check_and_add_lsq(uint16_t rob_index);
        
        // branch predictor
        uint8_t predict_branch(uint64_t ip);
        void    initialize_branch_predictor(),
        last_branch_result(uint64_t ip, uint8_t taken);

#ifdef USE_HERMES
        OffchipPredBase *offchip_pred;
        DDRPMonitor *ddrp_monitor;
        OffchipTracer tracer;
        struct {
            struct { uint64_t called, rob_non_head, rob_head, went_offchip, went_offchip_rob_head, went_offchip_rob_non_head; } bubble;
            struct { uint64_t pred_called, true_pos, false_pos, false_neg, true_neg; } offchip_pred;
            struct { uint64_t total, issued[2], dram_rq_full, dram_mshr_full; } ddrp;
        } stats;

        inline int32_t compute_rob_position(uint32_t rob_index, uint32_t rob_head) {
            return (rob_head <= rob_index) ? (rob_index - rob_head) : (ROB_SIZE - rob_head + rob_index);
        }
        void monitor_loads(uint32_t lq_index);
        bool rob_pos_is_frontal(int32_t rob_pos);
        bool rob_pos_is_dorsal(int32_t rob_pos);
        int8_t rob_part_id_get_part_type(uint32_t rob_part_id);
        int8_t rob_pos_get_part_type(int32_t rob_pos);
        uint32_t get_rob_parition_id(int32_t rob_pos);
        bool rob_part_id_is_frontal(uint32_t rob_part_id);
        bool rob_part_id_is_dorsal(uint32_t rob_part_id);
        void initialize_offchip_predictor(uint64_t seed);
        void dump_stats_offchip_predictor();
        void print_config_offchip_predictor();
        void offchip_predictor_update_dram_bw(uint8_t dram_bw);
        void offchip_predictor_track_llc_eviction(uint32_t set, uint32_t way, uint64_t address);
        void offchip_pred_stats_and_train(uint32_t lq_index);
        void issue_ddrp_request(uint32_t lq_index, uint32_t call_type);
        void initialize_ddrp_monitor();
        void print_config_ddrp_monitor();
#endif

};

extern O3_CPU ooo_cpu[NUM_CPUS];

#endif




