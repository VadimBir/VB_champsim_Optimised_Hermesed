#define _BSD_SOURCE


#include "lpm_tracker.h"
#include "ooo_l1_byp_model.cc"
#ifdef BYPASS_L2_LOGIC
#include "ooo_l2_byp_model.cc"
#endif
#ifdef BYPASS_LLC_LOGIC
#include "ooo_llc_byp_model.cc"
#endif

#include <getopt.h>
#include "ooo_cpu.h"
#ifdef USE_TRACE_HELPER
#include "trace_helper.h"
#endif


#include <unistd.h>   // getcwd
#include <limits.h>   // PATH_MAX

#include "uncore.h"
#include <fstream>

#include "instr_event.h"
#include "db_writer.h"

#include <iostream>
// #include <iomanip>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#ifdef USE_HERMES
namespace knob { extern std::string offchip_pred_type; }
#ifdef USE_HERMES_TTP
#define HERMES_LABEL "Hermes:TTP"
#elif defined(USE_HERMES_HMP)
#define HERMES_LABEL "Hermes:HMP"
#else
#define HERMES_LABEL "Hermes:std_active"
#endif
#else
#define HERMES_LABEL "Hermes:inactive"
#endif
// #include <map>
#include <string>

#include <stdint.h> // C-style

#define FIXED_FLOAT(x) std::fixed << std::setprecision(5) << (x)
#define FIXED_FLOAT4(x) std::fixed << std::setprecision(4) << (x)
#define FIXED_FLOAT2(x) std::fixed << std::setprecision(2) << (x)
#define FIXED_FLOAT1(x) std::fixed << std::setprecision(1) << (x)

#define STR_CORE_NUM       "C:"
#define STR_INSTR        "I#"
#define STR_CYCLES         "cy"
#define STR_IPC_NOW        "IPC "
#define STR_IPC_AVG        "IPCavg "
#define STR_BYPASS         "ByP% "
#define STR_APC        "CPA "
#define STR_LPM         "LPM "
#define STR_cAMT        "cAMT "
#define STR_MST         "MST "
#define STR_MSHR_OCCUPANCY_PERCENT       "MSHR%"
#define STR_LOAD_HIT_RATE       "LoadH%"
#define STR_TIME           "⏱"
#define STR_TimePerMillionInstr           "TPMI "

// CACHE TYPEs
#define ITLB_type 0
#define DTLB_type 1
#define STLB_type 2
#define L1I_type  3
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#define DRAM_type 7


float TOTAL_SUM_FINAL_SIM_IPC = 0;
uint8_t warmup_complete[NUM_CPUS], 
        simulation_complete[NUM_CPUS], 
        all_warmup_complete = 0, 
        all_simulation_complete = 0;
uint8_t MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS;
uint8_t knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000,
         champsim_seed;

extern int64_t execution_checksum[NUM_CPUS];

#ifdef BYPASS_DEBUG
extern std::vector<CACHE*> ALL_CACHES;
#endif


time_t start_time;
time_t last_print_time[NUM_CPUS] = {};

// PAGE TABLE
uint32_t PAGE_TABLE_LATENCY = 0, SWAP_LATENCY = 0;
queue <uint64_t > page_queue;
ankerl::unordered_dense::map <uint64_t, uint64_t> page_table, inverse_table, recent_page, unique_cl[NUM_CPUS];
uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS], allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS], major_fault[NUM_CPUS];

/******************************************************************************************
 * 1) DEFINE THE CLASS EXACTLY WITH THE SAME NAMES USED IN print_roi_stats / print_sim_stats
 ******************************************************************************************/

// Place these lines BEFORE int main(...), e.g. line ~25:
class FinalStatsCollector {
public:
    // Dictionary of all stats: string -> double
    // std::map<std::string, double> data;
    ankerl::unordered_dense::map<std::string, double> data;

    // Collect ROI stats (from print_roi_stats)
    // EXACT variable names: TOTAL_ACCESS, TOTAL_HIT, TOTAL_MISS, roi_access, roi_hit, roi_miss, pf_requested, pf_issued, pf_useful, pf_useless, pf_late, average_miss_latency
    void collectROIStats(uint16_t cpu, CACHE *cache,
                         uint64_t TOTAL_ACCESS, uint64_t TOTAL_HIT, uint64_t TOTAL_MISS,
                         uint64_t loads, uint64_t load_hit, uint64_t load_miss,
                         uint64_t RFOs, uint64_t RFO_hit, uint64_t RFO_miss,
                         uint64_t prefetches, uint64_t prefetch_hit, uint64_t prefetch_miss,
                         uint64_t writebacks, uint64_t writeback_hit, uint64_t writeback_miss,
                         uint64_t pf_requested, uint64_t pf_issued,
                         uint64_t pf_useful, uint64_t pf_useless, uint64_t pf_late,
                         double average_miss_latency)
    {
        // Use the same printed keys:  "Core_{cpu}_{cache->NAME}_total_access", etc.
        std::string coreCachePrefix = "Core_" + std::to_string(cpu) + "_" + cache->NAME + "_";
        data[coreCachePrefix + "total_access"]        = (double)TOTAL_ACCESS;
        data[coreCachePrefix + "total_hit"]           = (double)TOTAL_HIT;
        data[coreCachePrefix + "total_miss"]          = (double)TOTAL_MISS;
        data[coreCachePrefix + "loads"]               = (double)loads;
        data[coreCachePrefix + "load_hit"]            = (double)load_hit;
        data[coreCachePrefix + "load_miss"]           = (double)load_miss;
        data[coreCachePrefix + "RFOs"]                = (double)RFOs;
        data[coreCachePrefix + "RFO_hit"]             = (double)RFO_hit;
        data[coreCachePrefix + "RFO_miss"]            = (double)RFO_miss;
        data[coreCachePrefix + "prefetches"]          = (double)prefetches;
        data[coreCachePrefix + "prefetch_hit"]        = (double)prefetch_hit;
        data[coreCachePrefix + "prefetch_miss"]       = (double)prefetch_miss;
        data[coreCachePrefix + "writebacks"]          = (double)writebacks;
        data[coreCachePrefix + "writeback_hit"]       = (double)writeback_hit;
        data[coreCachePrefix + "writeback_miss"]      = (double)writeback_miss;
        data[coreCachePrefix + "prefetch_requested"]  = (double)pf_requested;
        data[coreCachePrefix + "prefetch_issued"]     = (double)pf_issued;
        data[coreCachePrefix + "prefetch_useful"]     = (double)pf_useful;
        data[coreCachePrefix + "prefetch_useless"]    = (double)pf_useless;
        data[coreCachePrefix + "prefetch_late"]       = (double)pf_late;
        data[coreCachePrefix + "average_miss_latency"] = average_miss_latency;
    }

    // Collect SIM stats (from print_sim_stats)
    // EXACT variable names: TOTAL_ACCESS, TOTAL_HIT, TOTAL_MISS, sim_access, sim_hit, sim_miss
    void collectSimStats(uint16_t cpu, CACHE *cache,
                         uint64_t TOTAL_ACCESS, uint64_t TOTAL_HIT, uint64_t TOTAL_MISS,
                         uint64_t loads, uint64_t load_hit, uint64_t load_miss,
                         uint64_t RFOs, uint64_t RFO_hit, uint64_t RFO_miss,
                         uint64_t prefetches, uint64_t prefetch_hit, uint64_t prefetch_miss,
                         uint64_t writebacks, uint64_t writeback_hit, uint64_t writeback_miss)
    {
        std::string coreCachePrefix = "Core_" + std::to_string(cpu) + "_" + cache->NAME + "_";
        data[coreCachePrefix + "total_access"]   = (double)TOTAL_ACCESS;
        data[coreCachePrefix + "total_hit"]      = (double)TOTAL_HIT;
        data[coreCachePrefix + "total_miss"]     = (double)TOTAL_MISS;
        data[coreCachePrefix + "loads"]          = (double)loads;
        data[coreCachePrefix + "load_hit"]       = (double)load_hit;
        data[coreCachePrefix + "load_miss"]      = (double)load_miss;
        data[coreCachePrefix + "RFOs"]           = (double)RFOs;
        data[coreCachePrefix + "RFO_hit"]        = (double)RFO_hit;
        data[coreCachePrefix + "RFO_miss"]       = (double)RFO_miss;
        data[coreCachePrefix + "prefetches"]     = (double)prefetches;
        data[coreCachePrefix + "prefetch_hit"]   = (double)prefetch_hit;
        data[coreCachePrefix + "prefetch_miss"]  = (double)prefetch_miss;
        data[coreCachePrefix + "writebacks"]     = (double)writebacks;
        data[coreCachePrefix + "writeback_hit"]  = (double)writeback_hit;
        data[coreCachePrefix + "writeback_miss"] = (double)writeback_miss;
    }

    // Collect branch stats (from print_branch_stats)
    void collectBranchStats(uint16_t cpu, double prediction_accuracy,
                            double branch_MPKI, double avg_ROB_occ_mispredict)
    {
        std::string corePrefix = "Core_" + std::to_string(cpu) + "_";
        data[corePrefix + "branch_prediction_accuracy"]          = prediction_accuracy;
        data[corePrefix + "branch_MPKI"]                         = branch_MPKI;
        data[corePrefix + "average_ROB_occupancy_at_mispredict"] = avg_ROB_occ_mispredict;
    }

    // Collect DRAM stats (from print_dram_stats)
    // Each channel uses: "Channel_i_RQ_row_buffer_hit", etc.
    void collectDRAMStats(uint32_t channel,
                          uint64_t RQ_row_buffer_hit,
                          uint64_t RQ_row_buffer_miss,
                          uint64_t WQ_row_buffer_hit,
                          uint64_t WQ_row_buffer_miss,
                          uint64_t WQ_full,
                          uint64_t dbus_congested)
    {
        std::string chPrefix = "Channel_" + std::to_string(channel) + "_";
        data[chPrefix + "RQ_row_buffer_hit"]   = (double)RQ_row_buffer_hit;
        data[chPrefix + "RQ_row_buffer_miss"]  = (double)RQ_row_buffer_miss;
        data[chPrefix + "WQ_row_buffer_hit"]   = (double)WQ_row_buffer_hit;
        data[chPrefix + "WQ_row_buffer_miss"]  = (double)WQ_row_buffer_miss;
        data[chPrefix + "WQ_full"]             = (double)WQ_full;
        data[chPrefix + "dbus_congested"]      = (double)dbus_congested;
    }

    // Collect final page faults or other CPU-level stats
    // e.g. "Core_i_major_page_fault", "Core_i_minor_page_fault"
    void collectPageFaultStats(uint16_t cpu, uint64_t major_fault_val, uint64_t minor_fault_val) {
        std::string corePrefix = "Core_" + std::to_string(cpu) + "_";
        data[corePrefix + "major_page_fault"] = (double)major_fault_val;
        data[corePrefix + "minor_page_fault"] = (double)minor_fault_val;
    }

    // Collect instructions, cycles, IPC from ROI block
    // "Core_i_instructions", "Core_i_cycles", "Core_i_IPC"
    void collectCoreROIStats(uint16_t cpu,
                             uint64_t finish_sim_instr,
                             uint64_t finish_sim_cycle,
                             double finalIPC)
    {
        std::string corePrefix = "Core_" + std::to_string(cpu) + "_";
        data[corePrefix + "instructions"] = (double)finish_sim_instr;
        data[corePrefix + "cycles"]       = (double)finish_sim_cycle;
        data[corePrefix + "IPC"]          = finalIPC;
    }

    // Print the entire dictionary as JSON-like string: {"key": value, "key2": value2, ...}


    std::string dumpAllAsString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(5) << "{";
        bool first = true;
        for (const auto& kv : data) {
            if (!first) oss << ", ";
            oss << "\"" << kv.first << "\": " << kv.second;
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};


/***************************************************************************************************
 * 2) CREATE THE OBJECT IN main(), THEN AFTER EACH PRINT_ FUNCTION, CALL THE COLLECTOR WITH MATCHING NAMES
 ***************************************************************************************************/

// Somewhere near line ~90, before int main returns:
FinalStatsCollector statsCollector;  // global or local inside main


/*
 * Example usage:
 *   - After "cout<< ..." lines in print_roi_stats or print_sim_stats,
 *     you can do something like:
 *
 *     statsCollector.collectROIStats(cpu, cache,
 *         TOTAL_ACCESS, TOTAL_HIT, TOTAL_MISS,
 *         cache->roi_access[cpu][0], cache->roi_hit[cpu][0], cache->roi_miss[cpu][0],
 *         cache->roi_access[cpu][1], cache->roi_hit[cpu][1], cache->roi_miss[cpu][1],
 *         ...
 *         cache->pf_requested, cache->pf_issued,
 *         cache->pf_useful, cache->pf_useless, cache->pf_late,
 *         (1.0*(cache->total_miss_latency))/TOTAL_MISS
 *     );
 *
 *   - Do similarly in print_sim_stats(...) calling collectSimStats(...).
 *   - Then at the END of main():
 *
 *       cout << statsCollector.dumpAllAsString() << endl;
 *
 * That gives a final JSON-like dict of all stats with the EXACT same names.
 */




#include "main_stats.h"

void finish_warmup()
{
    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

    // reset core latency
    SCHEDULING_LATENCY = 6;
    EXEC_LATENCY = 1;
    PAGE_TABLE_LATENCY = 100;
    SWAP_LATENCY = 100000;

    cout << endl;
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << "Warmup done CPU " << setw(2) << i << " instr: " << setw(10) << ooo_cpu[i].num_retired << " cycles: " << setw(10) << current_core_cycle[i];
        cout << " (Sim time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " <<"TPMI:"<<round(((double)(elapsed_hour*3600ULL + elapsed_minute*60ULL + elapsed_second) / (ooo_cpu[i].num_retired / 1000000.0)) * 100) / 100<< endl;

        ooo_cpu[i].begin_sim_cycle = current_core_cycle[i]; 
        ooo_cpu[i].begin_sim_instr = ooo_cpu[i].num_retired;

        // reset branch stats
        ooo_cpu[i].num_branch = 0;
        ooo_cpu[i].branch_mispredictions = 0;
	ooo_cpu[i].total_rob_occupancy_at_branch_mispredict = 0;

        reset_cache_stats(i, &ooo_cpu[i].L1I);
        reset_cache_stats(i, &ooo_cpu[i].L1D);
        reset_cache_stats(i, &ooo_cpu[i].L2C);
        reset_cache_stats(i, &uncore.LLC);
        lpm_reset_sim(i);
    }
    cout << endl;

    // reset DRAM stats
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        uncore.DRAM.RQ[i].ROW_BUFFER_HIT = 0;
        uncore.DRAM.RQ[i].ROW_BUFFER_MISS = 0;
        uncore.DRAM.WQ[i].ROW_BUFFER_HIT = 0;
        uncore.DRAM.WQ[i].ROW_BUFFER_MISS = 0;
    }

    // set actual cache latency
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        ooo_cpu[i].ITLB.LATENCY = ITLB_LATENCY;
        ooo_cpu[i].DTLB.LATENCY = DTLB_LATENCY;
        ooo_cpu[i].STLB.LATENCY = STLB_LATENCY;
        ooo_cpu[i].L1I.LATENCY  = L1I_LATENCY;
        ooo_cpu[i].L1D.LATENCY  = L1D_LATENCY;
        ooo_cpu[i].L2C.LATENCY  = L2C_LATENCY;
    }
    uncore.LLC.LATENCY = LLC_LATENCY;
}
alignas(64) rob_events_soa rob_events = {};
alignas(64) MemIndexRing mem_index_ring = {};
#include "main_deadlock.h"

void signal_handler(int signal)
{
	cout << "Caught signal: " << signal << endl;
#ifdef USE_TRACE_HELPER
	trace_helper.stop();
#endif
	exit(1);
}



RANDOM champsim_rand(champsim_seed);
#include "main_paging.h"
#include "main_loop.h"
#include "main_report.h"
#define ROB_MASK (ROB_SIZE - 1)
// static char zero_arena[1024*1024*1024] = {0};
int main(int argc, char** argv)
{
    
    ChampsimDBConfig db_cfg = {};

    int num_instr_dest = NUM_INSTR_DESTINATIONS;
    int num_instr_dest_sparc = NUM_INSTR_DESTINATIONS_SPARC;
#ifdef SANITY_CHECK
#else
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl;
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl;
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl << endl;
#endif

if (num_instr_dest == 2) {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;33m*** NON CLOUD SUITE NUM_INSTR_DESTINATIONS \033[1;31m(%d)\033[0m \033[1;33m***\033[0m\n",
                     num_instr_dest);
} else if (num_instr_dest_sparc != 4) {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;34m*** HYBRID NUM_INSTR_DESTINATIONS \033[1;31m%d\033[0m \033[1;34m***\033[0m\n",
                     num_instr_dest_sparc);
} else {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;31m*** CLOUD NUM_INSTR_DESTINATIONS \033[1;31m(%d)\033[0m \033[1;32m***\033[0m\n",
                     num_instr_dest);
}


        // pf_stat_num_retired = 0;
	// interrupt signal hanlder
#ifdef _WIN32
	// MinGW has no sigaction; plain signal() installs the same SIGINT handler.
	signal(SIGINT, signal_handler);
#else
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
#endif

    // cout << "*************************************************" << endl
    //      << "   ChampSim Multicore Out-of-Order Simulator" << endl
    //      << "   Last compiled: " << __DATE__ << " " << __TIME__ << endl
    //      << "*************************************************" << endl;

    #include <cstdio>
    fprintf(stdout, "*************************************************\n");
    fprintf(stdout, "   ChampSim Multicore Out-of-Order Simulator\n");
    fprintf(stdout, "   Last compiled: %s %s\n", __DATE__, __TIME__);
    fprintf(stdout, "*************************************************\n");


    // initialize knobs
    uint8_t show_heartbeat = 1;

    uint32_t seed_number = 0;

    // check to see if knobs changed using getopt_long()
    int c;
    while (1) {
        static struct option long_options[] =
        {
            {"warmup_instructions", required_argument, 0, 'w'},
            {"simulation_instructions", required_argument, 0, 'i'},
            {"hide_heartbeat", no_argument, 0, 'h'},
            {"cloudsuite", no_argument, 0, 'c'},
            {"low_bandwidth",  no_argument, 0, 'b'},
            {"traces",  no_argument, 0, 't'},
            // ── DB args ──
            {"db",           required_argument, 0, 'D'},
            {"arch",         required_argument, 0, 'A'},
            {"bypass",       required_argument, 0, 'B'},
            {"pf_l1",        required_argument, 0, '1'},
            {"pf_l2",        required_argument, 0, '2'},
            {"pf_l3",        required_argument, 0, '3'},
            {"unique_epoch", no_argument,       0, 'E'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long_only(argc, argv, "wihsb", long_options, &option_index);

        // no more option characters
        if (c == -1)
            break;

        int traces_encountered = 0;

        switch(c) {
            case 'w':
                warmup_instructions = atol(optarg);
                break;
            case 'i':
                simulation_instructions = atol(optarg);
                break;
            case 'h':
                show_heartbeat = 0;
                break;
            case 'c':
                knob_cloudsuite = 1;
                MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
                break;
            case 'b':
                knob_low_bandwidth = 1;
                break;
            case 't':
                traces_encountered = 1;
                break;
            // ── DB args ──
            case 'D':
                std::strncpy(db_cfg.db_path, optarg, sizeof(db_cfg.db_path) - 1);
                break;
            case 'A':
                std::strncpy(db_cfg.arch, optarg, sizeof(db_cfg.arch) - 1);
                break;
            case 'B':
                std::strncpy(db_cfg.bypass, optarg, sizeof(db_cfg.bypass) - 1);
                break;
            case '1':
                std::strncpy(db_cfg.pf_l1, optarg, sizeof(db_cfg.pf_l1) - 1);
                break;
            case '2':
                std::strncpy(db_cfg.pf_l2, optarg, sizeof(db_cfg.pf_l2) - 1);
                break;
            case '3':
                std::strncpy(db_cfg.pf_l3, optarg, sizeof(db_cfg.pf_l3) - 1);
                break;
            case 'E':
                db_cfg.use_epoch = true;
                break;
            default:
                abort();
        }

        if (traces_encountered == 1)
            break;
    }

    if (knob_low_bandwidth)
        DRAM_MTPS = DRAM_IO_FREQ/4;
    else
        DRAM_MTPS = DRAM_IO_FREQ;

    // DRAM access latency
    tRP  = (uint32_t)((1.0 * tRP_DRAM_NANOSECONDS  * CPU_FREQ) / 1000); 
    tRCD = (uint32_t)((1.0 * tRCD_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 
    tCAS = (uint32_t)((1.0 * tCAS_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 

    // default: 16 = (64 / 8) * (3200 / 1600)
    // it takes 16 CPU cycles to tranfser 64B cache block on a 8B (64-bit) bus 
    // note that dram burst length = BLOCK_SIZE/DRAM_CHANNEL_WIDTH
    DRAM_DBUS_RETURN_TIME = (BLOCK_SIZE / DRAM_CHANNEL_WIDTH) * (1.0 * CPU_FREQ / DRAM_MTPS);

    // end consequence of knobs

    // search through the argv for "-traces"
// search through the argv for "-traces"
    int found_traces = 0;
    int count_traces = 0;
    cout << endl;
    for (int i=0; i<argc; i++) {
        if (found_traces == 0) {
            if (strcmp(argv[i], "-traces") == 0) {
                found_traces = 1;
            }
            continue;
        }

        // Copy argv into trace_string buffer
        std::snprintf(ooo_cpu[count_traces].trace_string,
                    sizeof(ooo_cpu[count_traces].trace_string),
                    "%s",
                    argv[i]);
        ooo_cpu[count_traces].trace_string[sizeof(ooo_cpu[count_traces].trace_string)-1] = '\0';

        // Point to the buffer we just initialized
        const char *full_name = ooo_cpu[count_traces].trace_string;
        // cout << "" << full_name << endl;
        printf("trace_%d %s\n", count_traces, argv[i]);

        // Validate extension
        const char *last_dot = std::strrchr(full_name, '.');
        if (!last_dot || last_dot[1] == '\0') {
            std::cerr << "TRACE FILE HAS NO EXTENSION: " << full_name << '\n';
            assert(false && "TRACE FILE HAS NO EXTENSION");
        }

        char ext = last_dot[1];

        if (ext == 'g') {
            std::snprintf(ooo_cpu[count_traces].gunzip_command,
                        sizeof(ooo_cpu[count_traces].gunzip_command),
                        "gunzip -c %s", full_name);

            ooo_cpu[count_traces].trace_file = popen(ooo_cpu[count_traces].gunzip_command, "r");
            if (!ooo_cpu[count_traces].trace_file) {
                std::fprintf(stderr, "Unable to popen gunzip command: %s\n", ooo_cpu[count_traces].gunzip_command);
                assert(false && "Unable to popen gunzip command");
            }

            setvbuf(ooo_cpu[count_traces].trace_file, NULL, _IOFBF, 16 * 1024 * 1024);
            ooo_cpu[count_traces].is_xz_trace = false;
        }
        else if (ext == 'x') {
            // fprintf(stderr, "DEBUG: count_traces=%d, NUM_CPUS=%d\n", count_traces, NUM_CPUS);
            // fprintf(stderr, "DEBUG: full_name='%s'\n", full_name);
            // fprintf(stderr, "DEBUG: &ooo_cpu[count_traces]=%p\n", (void*)&ooo_cpu[count_traces]);
            // fprintf(stderr, "DEBUG: &xz_reader=%p\n", (void*)&ooo_cpu[count_traces].xz_reader);

            // if (!ooo_cpu[count_traces].xz_reader.open(full_name)) {
            //     std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s\n", full_name);
            //     assert(false && "TRACE FILE DOES NOT EXIST");
            // }
            if (!ooo_cpu[count_traces].xz_reader.open(full_name)) {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                    std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s/%s\n", cwd, full_name);
                } else {
                    std::perror("getcwd failed");
                    std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s\n", full_name);
                }
                assert(false && "TRACE FILE DOES NOT EXIST");
            }
            ooo_cpu[count_traces].is_xz_trace = true;
            ooo_cpu[count_traces].verify_xz_trace();
        }
        else {
            std::cerr << "ChampSim does not support traces other than gz or xz compression: " << full_name << '\n';
            assert(false && "ChampSim does not support traces other than gz or xz compression!");
        }

        char *pch[100] = {nullptr};
        int count_str = 0;
        pch[count_str] = std::strtok(argv[i], " /,.-");
        while (pch[count_str] != nullptr && count_str + 1 < (int)std::size(pch)) {
            ++count_str;
            pch[count_str] = std::strtok(nullptr, " /,.-");
        }

        if (count_str < 3 || pch[count_str - 3] == nullptr) {
            std::cerr << "TRACE FILENAME TOKENIZATION FAILED: " << argv[i] << '\n';
            assert(false && "TRACE FILENAME TOKENIZATION FAILED");
        }

        int j = 0;
        while (pch[count_str - 3][j] != '\0') {
            seed_number += static_cast<unsigned long>(pch[count_str - 3][j]);
            ++j;
        }

        if (!ooo_cpu[count_traces].is_xz_trace && ooo_cpu[count_traces].trace_file == NULL) {
            std::fprintf(stderr, "\n*** Trace file not found: %s ***\n\n", full_name);
            assert(false && "Trace file not found");
        }

        ++count_traces;
        if (count_traces > NUM_CPUS) {
            std::fprintf(stderr, "\n*** Too many traces for the configured number of cores ***\n\n");
            assert(false && "Too many traces for the configured number of cores");
        }
    }

    // cout << "NUM_CPUS " << NUM_CPUS << endl;
    fprintf(stdout, "NUM_CPUS %d\n", NUM_CPUS);

    if (count_traces != NUM_CPUS) {
        printf("\n*** Not enough traces for the configured number of cores ***\n\n");
        assert(0&&"Not enough traces for the configured number of cores");
    }
    // end trace file setup

    // TODO: can we initialize these variables from the class constructor?
    srand(seed_number);
    champsim_seed = seed_number;

    lpm_init(); // INITIALIZE THE STATS COLLECTOR FOR THE Layer Performance Metric (LPM)
    byplat_init_all(); // BYPASS LATENCY + MLP TRACKER
    for (int i=0; i<NUM_CPUS; i++) {

        ooo_cpu[i].cpu = i; 
        ooo_cpu[i].warmup_instructions = warmup_instructions;
        ooo_cpu[i].simulation_instructions = simulation_instructions;
        ooo_cpu[i].begin_sim_cycle = 0; 
        ooo_cpu[i].begin_sim_instr = warmup_instructions;

        // ROB
        ooo_cpu[i].ROB.cpu = i;

        // BRANCH PREDICTOR
        ooo_cpu[i].initialize_branch_predictor();
#ifdef USE_HERMES
        ooo_cpu[i].offchip_pred = NULL;
        ooo_cpu[i].ddrp_monitor = NULL;
        ooo_cpu[i].initialize_offchip_predictor(i);
        ooo_cpu[i].initialize_ddrp_monitor();
        memset(&ooo_cpu[i].stats, 0, sizeof(ooo_cpu[i].stats));
        if (i == 0) cout << HERMES_LABEL << endl;
#endif

        // TLBs
        ooo_cpu[i].ITLB.cpu = i;
        ooo_cpu[i].ITLB.cache_type = IS_ITLB;
        ooo_cpu[i].ITLB.fill_level = FILL_L1;
        ooo_cpu[i].ITLB.extra_interface = &ooo_cpu[i].L1I;
        ooo_cpu[i].ITLB.lower_level = &ooo_cpu[i].STLB; 

        ooo_cpu[i].DTLB.cpu = i;
        ooo_cpu[i].DTLB.cache_type = IS_DTLB;
        ooo_cpu[i].DTLB.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].DTLB.fill_level = FILL_L1;
        ooo_cpu[i].DTLB.extra_interface = &ooo_cpu[i].L1D;
        ooo_cpu[i].DTLB.lower_level = &ooo_cpu[i].STLB;

        ooo_cpu[i].STLB.cpu = i;
        ooo_cpu[i].STLB.cache_type = IS_STLB;
        ooo_cpu[i].STLB.fill_level = FILL_L2;
        ooo_cpu[i].STLB.upper_level_icache[i] = &ooo_cpu[i].ITLB;
        ooo_cpu[i].STLB.upper_level_dcache[i] = &ooo_cpu[i].DTLB;

        // PRIVATE CACHE
        ooo_cpu[i].L1I.cpu = i;
        ooo_cpu[i].L1I.cache_type = IS_L1I;
        ooo_cpu[i].L1I.MAX_READ = (FETCH_WIDTH > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : FETCH_WIDTH;
        ooo_cpu[i].L1I.fill_level = FILL_L1;
        ooo_cpu[i].L1I.lower_level = &ooo_cpu[i].L2C; 

        ooo_cpu[i].L1D.cpu = i;
        ooo_cpu[i].L1D.cache_type = IS_L1D;
        ooo_cpu[i].L1D.WQ.is_WQ = 1;
        ooo_cpu[i].L1D.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].L1D.fill_level = FILL_L1;
        ooo_cpu[i].L1D.lower_level = &ooo_cpu[i].L2C;
        ooo_cpu[i].L1D.l1d_prefetcher_initialize();
#ifdef BYPASS_L1_LOGIC
        l1d_bypass_initialize(i, &ooo_cpu[i].L1D, &ooo_cpu[i].L2C, &uncore.LLC);
#endif
        // ooo_cpu[i].L1D.MSHR.occupancy

        ooo_cpu[i].L2C.cpu = i;
        ooo_cpu[i].L2C.cache_type = IS_L2C;
        ooo_cpu[i].L2C.fill_level = FILL_L2;
        ooo_cpu[i].L2C.upper_level_icache[i] = &ooo_cpu[i].L1I;
        ooo_cpu[i].L2C.upper_level_dcache[i] = &ooo_cpu[i].L1D;
        ooo_cpu[i].L2C.lower_level = &uncore.LLC;
        ooo_cpu[i].L2C.l2c_prefetcher_initialize();
#ifdef BYPASS_L2_LOGIC
        l2c_bypass_initialize(i, &ooo_cpu[i].L1D, &ooo_cpu[i].L2C, &uncore.LLC);
#endif

        // SHARED CACHE
        uncore.LLC.cache_type = IS_LLC;
        uncore.LLC.fill_level = FILL_LLC;
        uncore.LLC.MAX_READ = NUM_CPUS;
        uncore.LLC.upper_level_icache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.upper_level_dcache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.lower_level = &uncore.DRAM;

        // OFF-CHIP DRAM
        uncore.DRAM.fill_level = FILL_DRAM;
        uncore.DRAM.upper_level_icache[i] = &uncore.LLC;
        uncore.DRAM.upper_level_dcache[i] = &uncore.LLC;
        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            uncore.DRAM.RQ[i].is_RQ = 1;
            uncore.DRAM.WQ[i].is_WQ = 1;
        }

        warmup_complete[i] = 0;
        //all_warmup_complete = NUM_CPUS;
        simulation_complete[i] = 0;
        current_core_cycle[i] = 0;
        stall_cycle[i] = 0;
        
        previous_ppage = 0;
        num_adjacent_page = 0;
        num_cl[i] = 0;
        allocated_pages = 0;
        num_page[i] = 0;
        minor_fault[i] = 0;
        major_fault[i] = 0;
    }
#ifdef BYPASS_DEBUG
    for (int i=0; i<NUM_CPUS; i++) {
        ALL_CACHES.push_back(&ooo_cpu[i].ITLB);
        ALL_CACHES.push_back(&ooo_cpu[i].DTLB);
        ALL_CACHES.push_back(&ooo_cpu[i].STLB);

        ALL_CACHES.push_back(&ooo_cpu[i].L1I);
        ALL_CACHES.push_back(&ooo_cpu[i].L1D);
        ALL_CACHES.push_back(&ooo_cpu[i].L2C);
    }
    ALL_CACHES.push_back(&uncore.LLC);
    // ALL_CACHES.push_back(&uncore.DRAM); // INCORECT!!!
#endif

    uncore.LLC.llc_initialize_replacement();
    uncore.LLC.llc_prefetcher_initialize();
#ifdef BYPASS_LLC_LOGIC
    llc_bypass_initialize(0, &ooo_cpu[0].L1D, &ooo_cpu[0].L2C, &uncore.LLC);
#endif

    if (false){
        cout << "MAIN.cc ln 788 stats print" << endl;
    } else {
        cout << "MAIN.cc ln 790 stats print to disable" << endl;
        print_knobs();
    }



    // simulation entry point
    start_time = time(NULL);

#ifdef USE_TRACE_HELPER
    trace_helper.start(ooo_cpu);
    // Prefill: wait until all core buffers are half-full before simulation
    for (int i = 0; i < NUM_CPUS; i++) {
        while (true) {
            uint32_t h = trace_helper.buffers[i].head.load(std::memory_order_acquire);
            uint32_t t = trace_helper.buffers[i].tail.load(std::memory_order_relaxed);
            uint32_t used = (h >= t) ? (h - t) : (TRACE_BUFFER_SIZE - t + h);
            if (used >= TRACE_BUFFER_SIZE / 2) break;
            std::this_thread::yield();
        }
    }
#endif

    simulation_loop(show_heartbeat);

#ifdef USE_TRACE_HELPER
    trace_helper.stop();
    cout << endl << "[Trace Helper Stats]" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
        cout << "Core_" << i << "_io_trace_idle " << trace_helper.io_trace_idle[i] << endl;
        cout << "Core_" << i << "_total_produced " << trace_helper.total_produced[i] << endl;
    }
#endif

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

    cout << endl << "ChampSim completed all CPUs" << endl;
//     if (NUM_CPUS > 1) {
//         cout << endl << "Total Simulation Statistics (including warmup)" << endl;
//         for (uint32_t i=0; i<NUM_CPUS; i++) {
//             cout << endl << "CPU ;" << i << "; cumulative IPC: ;" << (float) (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle); 
//             cout << " instructions: " << ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr << " cycles: " << current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle << endl;
// #ifndef CRC2_COMPILE
//             print_sim_stats(i, &ooo_cpu[i].L1D);
//             print_sim_stats(i, &ooo_cpu[i].L1I);
//             print_sim_stats(i, &ooo_cpu[i].L2C);
//             ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
//             ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
// #endif
//             print_sim_stats(i, &uncore.LLC);
//         }
//         uncore.LLC.llc_prefetcher_final_stats();
//     }

    print_end_of_sim_report();

    for (uint32_t i = 0; i < NUM_CPUS; i++) lpm_demand_compute_all(i); // Lazy: refresh met[] before db write
    champsim_db_store(db_cfg, TOTAL_SUM_FINAL_SIM_IPC / NUM_CPUS,
                      major_fault, minor_fault);

    return 0;
}
