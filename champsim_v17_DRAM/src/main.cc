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




void record_roi_stats(uint16_t cpu, CACHE *cache) {
    for (uint32_t i=0; i<NUM_TYPES; i++) {
        cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
        cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
        cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
    }
    cache->roi_access_wByP[cpu] = cache->sim_access_wByP[cpu];
    cache->roi_hit_wByP[cpu]    = cache->sim_hit_wByP[cpu];
    cache->roi_miss_wByP[cpu]   = cache->sim_miss_wByP[cpu];
    cache->roi_byp_wByP[cpu]    = cache->sim_byp_wByP[cpu];
}
double print_pf_hitRatio(uint16_t cpu, CACHE *cache) {
    //cout<< "Core_" << cpu << "_" << cache->NAME << "_prefetch_useful " << cache->pf_useful << " "<<cache->NAME<< "_Total_Hit Ratio: " << (double)cache->pf_useful/(double)cache->pf_issued << endl;
    
    //(double)cache->roi_hit[cpu][2]/(double)cache->roi_access[cpu][2]
    //cout<<"PfHitRatio: "<<(double)cache->pf_useful/(double)cache->pf_issued;
    return ((double)cache->sim_hit[cpu][2]/(double)cache->sim_access[cpu][2]);
}
double print_L2_hitRatio(uint16_t cpu, CACHE *cache) {
    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0;
    for (uint32_t i=0; i<NUM_TYPES; i++) {
        TOTAL_ACCESS += cache->sim_access[cpu][i];
        TOTAL_HIT += cache->sim_hit[cpu][i];
    }
    return ((double)TOTAL_HIT/(double)TOTAL_ACCESS);
}
double print_L2_usefulRatio(uint16_t cpu, CACHE *cache){
    return ((double)cache->pf_useful/(double)cache->pf_issued);
}


void print_roi_stats(uint16_t cpu, CACHE *cache) {
    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

    for (uint32_t i=0; i<NUM_TYPES; i++) {
        TOTAL_ACCESS += cache->roi_access[cpu][i];
        TOTAL_HIT += cache->roi_hit[cpu][i];
        TOTAL_MISS += cache->roi_miss[cpu][i];
    }
    cout << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";MPKI" << ";" <<std::right << setw(10) << (TOTAL_MISS * 1000.0 / ooo_cpu[cpu].finish_sim_instr) << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";LOAD_MSHR_cap" << ";" << std::right << setw(10) << cache->STALL[0] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";RFO_cap" << ";" << std::right << setw(10) << cache->STALL[1] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";Pf_MSHR_cap" << ";" << std::right << setw(10) << cache->STALL[2] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";WrBk_MSHR_cap" << ";" << std::right << setw(10) << cache->STALL[3] << ";" << endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pureAdm_LOAD" << ";" << std::right << setw(10) << cache->pure_MSHR_Admission_STALL[0] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pureAdm_RFO" << ";" << std::right << setw(10) << cache->pure_MSHR_Admission_STALL[1] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pureAdm_Pf" << ";" << std::right << setw(10) << cache->pure_MSHR_Admission_STALL[2] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pureAdm_WrBk" << ";" << std::right << setw(10) << cache->pure_MSHR_Admission_STALL[3] << ";" << endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";byp_req" << ";" << std::right << setw(10) << cache->total_ByP_req[cpu] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";byp_issued" << ";" << std::right << setw(10) << cache->total_ByP_issued[cpu] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";APC" << ";" << std::right << setw(10) << lpm[cpu][cache->cache_type].met[MET_G].apc_accessesDivActiveMemCy << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";LPM" << ";" << std::right << setw(10) << lpm[cpu][cache->cache_type].met[MET_G].lpmr_activeMemCyDivIdealCy << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";C-AMAT" << ";" << std::right << setw(10) << lpm[cpu][cache->cache_type].met[MET_G].camat_activeMemCyDivAccesses << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";MST" << ";" << std::right << setw(10) << lpm[cpu][cache->cache_type].met[MET_G].mst_pureMissCyDivAccesses << ";" << endl
    
    // #ifdef BYPASS_L1_LOGIC
    //     if (cache->cache_type == IS_L1D)
    //         cout << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";L1_miss_byp" << ";" << std::right << setw(10) << cache->total_L1_ByP_cnt << ";" << endl;
    // #endif
    // #ifdef BYPASS_LLC_LOGIC
    //     if (cache->cache_type == IS_LLC)
    //         cout << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";LLC_miss_byp" << ";" << std::right << setw(10) << cache->total_LLC_ByP_cnt << ";" << endl;
    // #endif
    // << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";APC" << ";" << std::right << setw(10) << lpm[cpu, L1D_type]->apc_accessesDivActiveMemCy_ratio << ";"
    // << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";LPM" << ";" << std::right << setw(10) << lpm[cpu, L1D_type]->lpmr_activeMemCyDivIdealCy_ratio << ";"
    // << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";C-AMAT" << ";" << std::right << setw(10) << lpm[cpu, L1D_type]->camat_activeMemCyDivAccesses_ratio << ";" << endl
    //      << " " STR_LPM
                    //  << setw(3) << right <<  FIXED_FLOAT2(get_LPMR_level(i, L1D_type))
                    //  << setw(3) << right <<  FIXED_FLOAT2(get_LPMR_level(i, L2C_type))
                    //  << setw(3) << right <<  FIXED_FLOAT2(get_LPMR_level(i, LLC_type)) 
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";total_access" << ";" <<std::right << setw(10) << TOTAL_ACCESS << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";total_hit" << ";" <<std::right << setw(10) << TOTAL_HIT << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";total_miss" << ";" <<std::right << setw(10) << TOTAL_MISS << ";" 
    << cache->NAME<< "_total_HitR: " << (double)TOTAL_HIT/(double)TOTAL_ACCESS << endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";loads" << ";" <<std::right << setw(10) << cache->roi_access[cpu][0] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_hit" << ";" <<std::right << setw(10) << cache->roi_hit[cpu][0] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_miss" << ";" <<std::right << setw(10) << cache->roi_miss[cpu][0] << ";" 
    << cache->NAME<< "_load_HitR: " << (double)cache->roi_hit[cpu][0]/(double)cache->roi_access[cpu][0]<< ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_wByP_acc" << ";" <<std::right << setw(10) << cache->roi_access_wByP[cpu] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_wByP_hit" << ";" <<std::right << setw(10) << cache->roi_hit_wByP[cpu] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_wByP_byp" << ";" <<std::right << setw(10) << cache->roi_byp_wByP[cpu] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";load_wByP_miss" << ";" <<std::right << setw(10) << cache->roi_miss_wByP[cpu] << ";"
    << cache->NAME<< "_load_wByP_HitR: " << (cache->roi_access_wByP[cpu] ? (double)cache->roi_hit_wByP[cpu]/(double)cache->roi_access_wByP[cpu] : 0.0) << ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";RFOs" << ";" <<std::right << setw(10) << cache->roi_access[cpu][1] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";RFO_hit" << ";" <<std::right << setw(10) << cache->roi_hit[cpu][1] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";RFO_miss" << ";" <<std::right << setw(10) << cache->roi_miss[cpu][1] << ";" 
    << cache->NAME<< "_RFO_HitR: " << (double)cache->roi_hit[cpu][1]/(double)cache->roi_access[cpu][1]<< ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";prefetches" << ";" <<std::right << setw(10) << cache->roi_access[cpu][2] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";prefetch_hit" << ";" <<std::right << setw(10) << cache->roi_hit[cpu][2] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";prefetch_miss" << ";" <<std::right << setw(10) << cache->roi_miss[cpu][2] << ";" 
    << cache->NAME<< "_Pf_HitR: " << (double)cache->roi_hit[cpu][2]/(double)cache->roi_access[cpu][2]<< ";" << endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";writebacks" << ";" <<std::right << setw(10) << cache->roi_access[cpu][3] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";writeback_hit" << ";" <<std::right << setw(10) << cache->roi_hit[cpu][3] << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";writeback_miss" << ";" <<std::right << setw(10) << cache->roi_miss[cpu][3] << ";" 
    << cache->NAME<< "_writeback_HitR: " << (double)cache->roi_hit[cpu][3]/(double)cache->roi_access[cpu][3]<< ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pf_requested" << ";" <<std::right << setw(10) << cache->pf_requested << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pf_issued" << ";" <<std::right << setw(10) << cache->pf_issued << ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pf_useful" << ";" <<std::right << setw(10) << cache->pf_useful << ";"
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pf_useless" << ";" <<std::right << setw(10) << cache->pf_useless << ";"<< endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";pf_late" << ";" <<std::right << setw(10) << cache->pf_late << ";" << cache->NAME<< "_Useful Ratio: " << (double)cache->pf_useful/(double)cache->pf_issued << ";" <<endl
    << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME << ";_" << setw(14) << ";avg_miss_lat" << ";" <<std::right << setw(10) << (1.0*(cache->total_miss_latency))/TOTAL_MISS <<";"<< endl
    << endl;
        statsCollector.collectROIStats(
        cpu, 
        cache,
        TOTAL_ACCESS,                // sum of cache->roi_access[cpu][i]
        TOTAL_HIT,                   // sum of cache->roi_hit[cpu][i]
        TOTAL_MISS,                  // sum of cache->roi_miss[cpu][i]
        cache->roi_access[cpu][0],   // loads
        cache->roi_hit[cpu][0],      // load_hit
        cache->roi_miss[cpu][0],     // load_miss
        cache->roi_access[cpu][1],   // RFOs
        cache->roi_hit[cpu][1],      // RFO_hit
        cache->roi_miss[cpu][1],     // RFO_miss
        cache->roi_access[cpu][2],   // prefetches
        cache->roi_hit[cpu][2],      // prefetch_hit
        cache->roi_miss[cpu][2],     // prefetch_miss
        cache->roi_access[cpu][3],   // writebacks
        cache->roi_hit[cpu][3],      // writeback_hit
        cache->roi_miss[cpu][3],     // writeback_miss
        cache->pf_requested,
        cache->pf_issued,
        cache->pf_useful,
        cache->pf_useless,
        cache->pf_late,
        (1.0 * (cache->total_miss_latency)) / TOTAL_MISS
    );
}

void print_sim_stats(uint16_t cpu, CACHE *cache) {
    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

    for (uint32_t i=0; i<NUM_TYPES; i++) {
        TOTAL_ACCESS += cache->sim_access[cpu][i];
        TOTAL_HIT += cache->sim_hit[cpu][i];
        TOTAL_MISS += cache->sim_miss[cpu][i];
    }

    cout<< "Core_;" << cpu << ";_;" << cache->NAME << ";_total_access\t;" << TOTAL_ACCESS <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_total_hit\t;" << TOTAL_HIT <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_total_miss\t;" << TOTAL_MISS <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_loads\t;" << cache->sim_access[cpu][0] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_load_hit\t;" << cache->sim_hit[cpu][0] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_load_miss\t;" << cache->sim_miss[cpu][0] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_RFOs\t;" << cache->sim_access[cpu][1] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_RFO_hit\t;" << cache->sim_hit[cpu][1] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_RFO_miss\t;" << cache->sim_miss[cpu][1] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_prefetches\t;" << cache->sim_access[cpu][2] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_prefetch_hit\t;" << cache->sim_hit[cpu][2] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_prefetch_miss\t;" << cache->sim_miss[cpu][2] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_writebacks\t;" << cache->sim_access[cpu][3] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_writeback_hit\t;" << cache->sim_hit[cpu][3] <<";"<< endl
        << "Core_;" << cpu << ";_;" << cache->NAME << ";_writeback_miss\t;" << cache->sim_miss[cpu][3] <<";"<< endl
        << endl;
    statsCollector.collectSimStats(
        cpu,
        cache,
        TOTAL_ACCESS,               // sum of cache->sim_access[cpu][i]
        TOTAL_HIT,                  // sum of cache->sim_hit[cpu][i]
        TOTAL_MISS,                 // sum of cache->sim_miss[cpu][i]
        cache->sim_access[cpu][0],  // loads
        cache->sim_hit[cpu][0],     // load_hit
        cache->sim_miss[cpu][0],    // load_miss
        cache->sim_access[cpu][1],  // RFOs
        cache->sim_hit[cpu][1],     // RFO_hit
        cache->sim_miss[cpu][1],    // RFO_miss
        cache->sim_access[cpu][2],  // prefetches
        cache->sim_hit[cpu][2],     // prefetch_hit
        cache->sim_miss[cpu][2],    // prefetch_miss
        cache->sim_access[cpu][3],  // writebacks
        cache->sim_hit[cpu][3],     // writeback_hit
        cache->sim_miss[cpu][3]     // writeback_miss
    );
}

void print_branch_stats(uint16_t cpu) {
    // for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << "Core_;" << cpu << ";_;branch_prediction_accuracy;" << (100.0*(ooo_cpu[cpu].num_branch - ooo_cpu[cpu].branch_mispredictions)) / ooo_cpu[cpu].num_branch <<";"<< endl
            << "Core_;" << cpu << ";_;branch_MPKI;" << (1000.0*ooo_cpu[cpu].branch_mispredictions)/(ooo_cpu[cpu].num_retired - ooo_cpu[cpu].warmup_instructions) <<";"<< endl
            << "Core_;" << cpu << ";_;average_ROB_occupancy_at_mispredict;" << (1.0*ooo_cpu[cpu].total_rob_occupancy_at_branch_mispredict)/ooo_cpu[cpu].branch_mispredictions <<";"<< endl
            << endl;
    // }
    statsCollector.collectBranchStats(    cpu,
    (100.0 * (ooo_cpu[cpu].num_branch - ooo_cpu[cpu].branch_mispredictions)) / ooo_cpu[cpu].num_branch,
    (1000.0 * ooo_cpu[cpu].branch_mispredictions) / (ooo_cpu[cpu].num_retired - ooo_cpu[cpu].warmup_instructions),
    (1.0 * ooo_cpu[cpu].total_rob_occupancy_at_branch_mispredict) / ooo_cpu[cpu].branch_mispredictions
    );
}

void print_dram_stats() {
    // cout << endl;
    // cout << "DRAM Statistics" << endl;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) 
    {
        cout << "Channel_;" << i << ";_;RQ_row_buffer_hit\t;" << uncore.DRAM.RQ[i].ROW_BUFFER_HIT <<";"
            << "Channel_;" << i << ";_;RQ_row_buffer_miss\t;" << uncore.DRAM.RQ[i].ROW_BUFFER_MISS <<";"
            << "Channel_;" << i << ";_;WQ_row_buffer_hit\t;" << uncore.DRAM.WQ[i].ROW_BUFFER_HIT <<";"
            << "Channel_;" << i << ";_;WQ_row_buffer_miss\t;" << uncore.DRAM.WQ[i].ROW_BUFFER_MISS <<";"
            << "Channel_;" << i << ";_;WQ_full\t;" << uncore.DRAM.WQ[i].FULL <<";"
            << "Channel_;" << i << ";_;dbus_congested\t;" << uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES] <<";"<< endl
            << endl;
            statsCollector.collectDRAMStats(    i,
            uncore.DRAM.RQ[i].ROW_BUFFER_HIT,
            uncore.DRAM.RQ[i].ROW_BUFFER_MISS,
            uncore.DRAM.WQ[i].ROW_BUFFER_HIT,
            uncore.DRAM.WQ[i].ROW_BUFFER_MISS,
            uncore.DRAM.WQ[i].FULL,
            uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]
        );
    }
    

    uint64_t total_congested_cycle = 0;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++)
        total_congested_cycle += (uint64_t)uncore.DRAM.dbus_cycle_congested[i] | ((uint64_t)uncore.DRAM.dbus_cycle_congested_ovf[i] << 32);
    if (uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]){
            cout << "avg_congested_cycle " << (total_congested_cycle / uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]) <<endl<< endl;
            for (int i=0; i<NUM_CPUS; i++) {
                cout << "EzSearch AVG IPC Core\t:"<<i <<":"<< ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle)<<":" << " L2-Pf-HR :" << (double)ooo_cpu[i].L2C.roi_hit[i][2]/(double)ooo_cpu[i].L2C.roi_access[i][2] << " : L2-HR :" <<print_L2_hitRatio(i,&ooo_cpu[i].L2C) << " Uful:"<< print_L2_usefulRatio(i,&ooo_cpu[i].L2C)<<endl;
            }
        }
    else{
        cout << "avg_congested_cycle 0\n" << endl;
        for (int i=0; i<NUM_CPUS; i++) {
                cout << "EzSearch AVG IPC Core\t:"<<i <<":"<< ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle)<<":" << " L2-Pf-HR :" << (double)ooo_cpu[i].L2C.roi_hit[i][2]/(double)ooo_cpu[i].L2C.roi_access[i][2] << " : L2-HR :" <<print_L2_hitRatio(i,&ooo_cpu[i].L2C) << " Uful:"<< print_L2_usefulRatio(i,&ooo_cpu[i].L2C)<<endl;
        }
    }
    cout << "OffChipPred\t;" << HERMES_LABEL << ";" << endl;

}

void reset_cache_stats(uint16_t cpu, CACHE *cache)
{
    for (uint32_t i=0; i<NUM_TYPES; i++) {
        cache->ACCESS[i] = 0;
        cache->HIT[i] = 0;
        cache->MISS[i] = 0;
        cache->MSHR_MERGED[i] = 0;
        cache->STALL[i] = 0;
        cache->pure_MSHR_Admission_STALL[i] = 0;

        cache->sim_access[cpu][i] = 0;
        cache->sim_hit[cpu][i] = 0;
        cache->sim_miss[cpu][i] = 0;
    }

    cache->total_miss_latency = 0;

    // reset bypass counters at warmup (like sim_miss)
    cache->total_ByP_issued[cpu] = 0;
    cache->total_ByP_req[cpu] = 0;
    cache->ByP_issued[cpu] = 0;
    cache->ByP_req[cpu] = 0;
    cache->sim_access_wByP[cpu] = 0;
    cache->sim_hit_wByP[cpu] = 0;
    cache->sim_miss_wByP[cpu] = 0;
    cache->sim_byp_wByP[cpu] = 0;

    cache->RQ.ACCESS = 0;
    cache->RQ.MERGED = 0;
    cache->RQ.TO_CACHE = 0;

    cache->WQ.ACCESS = 0;
    cache->WQ.MERGED = 0;
    cache->WQ.TO_CACHE = 0;
    cache->WQ.FORWARD = 0;
    cache->WQ.FULL = 0;
}

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
void print_deadlock(uint32_t i)
{

    // float l1d_mshr = (float)ooo_cpu[i].L1D.MSHR.occupancy / ooo_cpu[i].L1D.MSHR.SIZE * 100.0;
    // float l1d_rq = (float)ooo_cpu[i].L1D.RQ.occupancy / ooo_cpu[i].L1D.RQ.SIZE * 100.0;
    // float l1d_wq = (float)ooo_cpu[i].L1D.WQ.occupancy / ooo_cpu[i].L1D.WQ.SIZE * 100.0;
    // float l1d_pq = (float)ooo_cpu[i].L1D.PQ.occupancy / ooo_cpu[i].L1D.PQ.SIZE * 100.0;
    // float l1d_proc = (float)ooo_cpu[i].L1D.PROCESSED.occupancy / ooo_cpu[i].L1D.PROCESSED.SIZE * 100.0;
    
    // float l2c_mshr = (float)ooo_cpu[i].L2C.MSHR.occupancy / ooo_cpu[i].L2C.MSHR.SIZE * 100.0;
    // float l2c_rq = (float)ooo_cpu[i].L2C.RQ.occupancy / ooo_cpu[i].L2C.RQ.SIZE * 100.0;
    // float l2c_wq = (float)ooo_cpu[i].L2C.WQ.occupancy / ooo_cpu[i].L2C.WQ.SIZE * 100.0;
    // float l2c_pq = (float)ooo_cpu[i].L2C.PQ.occupancy / ooo_cpu[i].L2C.PQ.SIZE * 100.0;
    
    // float dtlb_mshr = (float)ooo_cpu[i].DTLB.MSHR.occupancy / ooo_cpu[i].DTLB.MSHR.SIZE * 100.0;
    // float dtlb_rq = (float)ooo_cpu[i].DTLB.RQ.occupancy / ooo_cpu[i].DTLB.RQ.SIZE * 100.0;
    // float dtlb_proc = (float)ooo_cpu[i].DTLB.PROCESSED.occupancy / ooo_cpu[i].DTLB.PROCESSED.SIZE * 100.0;
    
    // float itlb_mshr = (float)ooo_cpu[i].ITLB.MSHR.occupancy / ooo_cpu[i].ITLB.MSHR.SIZE * 100.0;
    // float itlb_rq = (float)ooo_cpu[i].ITLB.RQ.occupancy / ooo_cpu[i].ITLB.RQ.SIZE * 100.0;
    // float itlb_proc = (float)ooo_cpu[i].ITLB.PROCESSED.occupancy / ooo_cpu[i].ITLB.PROCESSED.SIZE * 100.0;
    
    // float stlb_mshr = (float)ooo_cpu[i].STLB.MSHR.occupancy / ooo_cpu[i].STLB.MSHR.SIZE * 100.0;
    // float stlb_rq = (float)ooo_cpu[i].STLB.RQ.occupancy / ooo_cpu[i].STLB.RQ.SIZE * 100.0;
    
    // float l1i_mshr = (float)ooo_cpu[i].L1I.MSHR.occupancy / ooo_cpu[i].L1I.MSHR.SIZE * 100.0;
    // float l1i_rq = (float)ooo_cpu[i].L1I.RQ.occupancy / ooo_cpu[i].L1I.RQ.SIZE * 100.0;
    // float l1i_proc = (float)ooo_cpu[i].L1I.PROCESSED.occupancy / ooo_cpu[i].L1I.PROCESSED.SIZE * 100.0;

    // // LLC
    // float llc_mshr = (float)uncore.LLC.MSHR.occupancy / uncore.LLC.MSHR.SIZE * 100.0;
    // float llc_rq = (float)uncore.LLC.RQ.occupancy / uncore.LLC.RQ.SIZE * 100.0;
    // float llc_wq = (float)uncore.LLC.WQ.occupancy / uncore.LLC.WQ.SIZE * 100.0;
    // float llc_pq = (float)uncore.LLC.PQ.occupancy / uncore.LLC.PQ.SIZE * 100.0;
    // cout << "\n=== CORE " << i << " CYCLE " << current_core_cycle[i] << " ===" << endl;
    //     cout << "L1D: MSHR=" << l1d_mshr << "% RQ=" << l1d_rq << "% WQ=" << l1d_wq << "% PQ=" << l1d_pq << "% PROC=" << l1d_proc << "%" << endl;
    //     cout << "L2C: MSHR=" << l2c_mshr << "% RQ=" << l2c_rq << "% WQ=" << l2c_wq << "% PQ=" << l2c_pq << "%" << endl;
    //     cout << "DTLB: MSHR=" << dtlb_mshr << "% RQ=" << dtlb_rq << "% PROC=" << dtlb_proc << "%" << endl;
    //     cout << "ITLB: MSHR=" << itlb_mshr << "% RQ=" << itlb_rq << "% PROC=" << itlb_proc << "%" << endl;
    //     cout << "STLB: MSHR=" << stlb_mshr << "% RQ=" << stlb_rq << "%" << endl;
    //     cout << "L1I: MSHR=" << l1i_mshr << "% RQ=" << l1i_rq << "% PROC=" << l1i_proc << "%" << endl;
    // cout << "\n=== LLC ===" << endl;
    // cout << "MSHR=" << llc_mshr << "% RQ=" << llc_rq << "% WQ=" << llc_wq << "% PQ=" << llc_pq << "%" << endl;
    // // DRAM
    // cout << "\n=== DRAM ===" << endl;
    // for (uint32_t ch = 0; ch < DRAM_CHANNELS; ch++) {
    //     float dram_rq = (float)uncore.DRAM.RQ[ch].occupancy / uncore.DRAM.RQ[ch].SIZE * 100.0;
    //     float dram_wq = (float)uncore.DRAM.WQ[ch].occupancy / uncore.DRAM.WQ[ch].SIZE * 100.0;
    //     cout << "CH[" << ch << "] RQ=" << dram_rq << "% WQ=" << dram_wq << "% WrMode=" << (int)uncore.DRAM.write_mode[ch] << endl;
    // }
    // assert(0);

    { uint16_t _hd = ooo_cpu[i].ROB.head;
      uint64_t _dd = PCYCLE_DIFF(PACK_CYCLE(current_core_cycle[i]), rob_events.per_cpu[i].event_cycle[_hd]);
    if (PCYCLE_LE(rob_events.per_cpu[i].event_cycle[_hd], PACK_CYCLE(current_core_cycle[i])) && _dd%DEADLOCK_CYCLE==0){
        cout << " *** WARNING : DEADLOCK DETECTED! (RUN IS CONTINUED!!!) *** \n";
        cout << "DEADLOCK! CPU " << i << " instr_id: " << ooo_cpu[i].ROB.entry[_hd].instr_id << endl;
        cout << " translated: " << +ooo_cpu[i].ROB.entry[_hd].translated;
        cout << " fetched: " << (BS_TST(rob_events.per_cpu[i].fetched_complete, _hd) ? 2 : BS_TST(rob_events.per_cpu[i].fetched_inflight, _hd) ? 1 : 0);
        cout << " scheduled: " << (BS_TST(rob_events.per_cpu[i].sched_complete, _hd) ? 2 : BS_TST(rob_events.per_cpu[i].sched_inflight, _hd) ? 1 : 0);
        cout << " executed: " << +ooo_cpu[i].ROB.entry[_hd].executed;
        cout << " is_memory: " << BS_TST(rob_events.per_cpu[i].is_mem, _hd);
        cout << " event: " << rob_events.per_cpu[i].event_cycle[_hd];
        cout << " current: " << current_core_cycle[i] << endl;



        // print L1D MSHR entry
        PACKET_QUEUE *queue;
        queue = &ooo_cpu[i].L1D.MSHR;
        cout << endl << queue->NAME << " Entry" << endl;
        for (uint32_t j = 0; j < queue->SIZE; j += 2) {
            // first entry
            cout << "[" << queue->NAME << "] entry: " << setw(2) << j
                << " instr: " << setw(8) << queue->entry[j].instr_id
                << " rob: " << setw(3) << queue->entry[j].rob_index
                << " addr: " << hex << setw(12) << queue->entry[j].address
                << " fuAddr: " << setw(12) << queue->entry[j].full_addr << dec
                << " type: " << +queue->entry[j].type
                << " fillLVL: " << setw(2) << +queue->entry[j].fill_level
                #ifdef BYPASS_L1_LOGIC
                << " ByP: " << +queue->entry[j].l1_bypassed
#endif
#ifdef BYPASS_LLC_LOGIC
                << " LByP: " << +queue->entry[j].llc_bypassed
#endif
                << " LQ: " << setw(3) << queue->entry[j].lq_index
                << " SQ: " << setw(3) << queue->entry[j].sq_index;

            // second entry, if exists
            if (j + 1 < queue->SIZE) {
                cout << "   [" << queue->NAME << "] ent: " << setw(2) << j + 1
                    << " instr: " << setw(8) << queue->entry[j+1].instr_id
                    << " rob: " << setw(3) << queue->entry[j+1].rob_index
                    << " addr: " << hex << setw(12) << queue->entry[j+1].address
                    << " fuAddr: " << setw(12) << queue->entry[j+1].full_addr << dec
                    << " type: " << +queue->entry[j+1].type
                    << " fillLVL: " << setw(2) << +queue->entry[j+1].fill_level
                    #ifdef BYPASS_L1_LOGIC
                << " ByP: " << +queue->entry[j].l1_bypassed
#endif
#ifdef BYPASS_LLC_LOGIC
                << " LByP: " << +queue->entry[j].llc_bypassed
#endif
                    << " LQ: " << setw(3) << queue->entry[j+1].lq_index
                    << " SQ: " << setw(3) << queue->entry[j+1].sq_index;
            }

            cout << endl;
        }

        // L2C MSHR
        queue = &ooo_cpu[i].L2C.MSHR;
        cout << endl << queue->NAME << " Entry" << endl;
        for (uint32_t j = 0; j < queue->SIZE; j += 2) {
            cout << "[" << queue->NAME << "] entry: " << setw(2) << j
                << " instr: " << setw(8) << queue->entry[j].instr_id
                << " rob: " << setw(3) << queue->entry[j].rob_index
                << " addr: " << hex << setw(12) << queue->entry[j].address
                << " fuAddr: " << setw(12) << queue->entry[j].full_addr << dec
                << " type: " << +queue->entry[j].type
                << " fillLVL: " << setw(2) << +queue->entry[j].fill_level
                #ifdef BYPASS_L1_LOGIC
                << " ByP: " << +queue->entry[j].l1_bypassed
#endif
#ifdef BYPASS_LLC_LOGIC
                << " LByP: " << +queue->entry[j].llc_bypassed
#endif
                << " LQ: " << setw(3) << queue->entry[j].lq_index
                << " SQ: " << setw(3) << queue->entry[j].sq_index;

            if (j + 1 < queue->SIZE) {
                cout << "   [" << queue->NAME << "] ent: " << setw(2) << j + 1
                    << " instr: " << setw(8) << queue->entry[j+1].instr_id
                    << " rob: " << setw(3) << queue->entry[j+1].rob_index
                    << " addr: " << hex << setw(12) << queue->entry[j+1].address
                    << " fuAddr: " << setw(12) << queue->entry[j+1].full_addr << dec
                    << " type: " << +queue->entry[j+1].type
                    << " fillLVL: " << setw(2) << +queue->entry[j+1].fill_level
                    #ifdef BYPASS_L1_LOGIC
                << " ByP: " << +queue->entry[j].l1_bypassed
#endif
#ifdef BYPASS_LLC_LOGIC
                << " LByP: " << +queue->entry[j].llc_bypassed
#endif
                    << " LQ: " << setw(3) << queue->entry[j+1].lq_index
                    << " SQ: " << setw(3) << queue->entry[j+1].sq_index;
            }

            cout << endl;
        }

        // Print Load Queue
        cout << endl << "Load Queue Entry" << endl;
        for (uint32_t j = 0; j < LQ_SIZE; j += 4) {
            for (uint32_t k = 0; k < 4 && j + k < LQ_SIZE; k++) {
                auto &e = ooo_cpu[i].LQ.entry[j + k];
                cout << "[LQ] "
                     << "ent:" << setw(3) << j + k
                     << " instr:" << setw(8) << e.instr_id
                     << " addr:" << hex << setw(12) << e.physical_address << dec
                     << " trans:" << +e.translated
                     << " fetch:" << +e.fetched
                     << "    "; // spacing between columns
            }
            cout << endl;
        }

        // Print Store Queue
        cout << endl << "Store Queue Entry" << endl;
        for (uint32_t j = 0; j < SQ_SIZE; j += 4) {
            for (uint32_t k = 0; k < 4 && j + k < SQ_SIZE; k++) {
                auto &e = ooo_cpu[i].SQ.entry[j + k];
                cout << "[SQ] "
                     << "ent:" << setw(3) << j + k
                     << " instr:" << setw(8) << e.instr_id
                     << " addr:" << hex << setw(12) << e.physical_address << dec
                     << " trans:" << +e.translated
                     << " fetch:" << +e.fetched
                     << "    ";
            }
            cout << endl;
        }

        // // print LQ entry
        // cout << endl << "Load Queue Entry" << endl;
        // for (uint32_t j=0; j<LQ_SIZE; j++) {
        //     cout << "[LQ] entry: " << j << " instr_id: " << ooo_cpu[i].LQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].LQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].LQ.entry[j].translated << " fetched: " << +ooo_cpu[i].LQ.entry[i].fetched << endl;
        // }
        //
        // // print SQ entry
        // cout << endl << "Store Queue Entry" << endl;
        // for (uint32_t j=0; j<SQ_SIZE; j++) {
        //     cout << "[SQ] entry: " << j << " instr_id: " << ooo_cpu[i].SQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].SQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].SQ.entry[j].translated << " fetched: " << +ooo_cpu[i].SQ.entry[i].fetched << endl;
        // }

        // queue = &ooo_cpu[i].L1D.MSHR;
        // cout << endl << queue->NAME << " Entry" << endl;
        // for (uint32_t j=0; j<queue->SIZE; j++) {
        //     cout << "[" << queue->NAME << "] entry: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        //     cout << " address: " << hex << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << dec << " type: " << +queue->entry[j].type;
        //     cout << " fill_level: " << queue->entry[j].fill_level << " lq_index: " << queue->entry[j].lq_index << " sq_index: " << queue->entry[j].sq_index << endl;
        // }
        // // print L2C mshr entries
        // queue = &ooo_cpu[i].L2C.MSHR;
        // cout << endl << queue->NAME << " Entry" << endl;
        // for (uint32_t j=0; j<queue->SIZE; j++) {
        //     cout << "[" << queue->NAME << "] entry: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
        //     cout << " address: " << hex << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << dec << " type: " << +queue->entry[j].type;
        //     cout << " fill_level: " << queue->entry[j].fill_level << " lq_index: " << queue->entry[j].lq_index << " sq_index: " << queue->entry[j].sq_index << endl;
        // }

        // // RQ head, WQ head, MSHR head
        //
        // // SAME ADDR IN hex
        // // cout << " xAddr: " << hex << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->address << " xFullAddr: " << hex << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->full_address  << dec << endl;
        // cout << "ROB EVENTS: ";
        // for (int rob = 0; rob < ROB_SIZE; ++rob) {
        //     if (rob == ooo_cpu[i].ROB.head) {
        //         // highlight exactly the matching two-character output in red
        //         cout << "\033[31m" << setw(2) << +rob_events.raw[i][rob] << "\033[0m";
        //     } else {
        //         cout << setw(2) << +rob_events.raw[i][rob];
        //     }
        // }
        // cout << endl;
        // cout << " rob_index: " << ooo_cpu[i].ROB.head;
        // cout << " translated: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->translated;
        // cout << " executed: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->executed;
        // cout << " fetched: " << +rob_events.entries[i][ooo_cpu[i].ROB.head].fetched;
        // cout << " scheduled: " << +(rob_events.raw[i][ooo_cpu[i].ROB.head] || COMPLETE_schedule_t);
        // cout << " ismem: " << +(rob_events.raw[i][ooo_cpu[i].ROB.head] || IS_MEMORY_t);
        // cout << " event: " << +rob_events.entries[i][ooo_cpu[i].ROB.head].event_cycle;
        // cout << " current: " << +current_core_cycle[i] << endl;
        // cout << " PhyAddr: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->physical_address << endl;
        // cout << " VAddr: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->virtual_address << endl;
        // cout << " IP: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->ip << endl;
        // cout << " head: " << ooo_cpu[i].ROB.head << " tail: " << ooo_cpu[i].ROB.tail << endl;
        // cout << " vAddr: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->virtual_address << endl;
        // cout << " phyAddr: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]-> << endl;
        // for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        //     if (ROB.entry[ROB.head].destination_memory[i])
        //         num_store++;
        // }
        int num_store = 0;
        for (uint32_t j=0; j<MAX_INSTR_DESTINATIONS; j++) {
            if (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].destination_memory[j])
                num_store++;
        }
        // ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->ip = 1;
        // ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->executed = COMPLETED;
        // memset(ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->destination_memory, 0,
        //     sizeof(ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head]->destination_memory));
        //

        cout << " num_store: " << num_store << endl;
        cout << "Wanted Retire BUT: (ooo_cpu[i].L1D.WQ.occupancy("<< +ooo_cpu[i].L1D.WQ.occupancy <<") + num_store(" << +num_store << ")) <= ooo_cpu[i].L1D.WQ.SIZE)(" << +ooo_cpu[i].L1D.WQ.SIZE <<  ") =>" << ((ooo_cpu[i].L1D.WQ.occupancy + num_store) <= ooo_cpu[i].L1D.WQ.SIZE) << endl;


#ifdef BYPASS_SANITY_CHECK
        cout << "BYPASS DEADLOCK DBG: \n";

#endif

        // // print LQ entry
    // cout << endl << "Load Queue Entry" << endl;
    // for (uint32_t j=0; j<LQ_SIZE; j++) {
        //     cout << "[LQ] entry: " << j << " instr_id: " << ooo_cpu[i].LQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].LQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].LQ.entry[j].translated << " fetched: " << +ooo_cpu[i].LQ.entry[i].fetched << endl;
        // }
        
        // // print SQ entry
        // cout << endl << "Store Queue Entry" << endl;
        // for (uint32_t j=0; j<SQ_SIZE; j++) {
            //     cout << "[SQ] entry: " << j << " instr_id: " << ooo_cpu[i].SQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].SQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].SQ.entry[j].translated << " fetched: " << +ooo_cpu[i].SQ.entry[i].fetched << endl;
    // }
    
    // print L1D MSHR entry
    // PACKET_QUEUE *queue;
    // queue = &ooo_cpu[i].L1D.MSHR;
    // cout << endl << queue->NAME << " Entry" << endl;
    // for (uint32_t j=0; j<queue->SIZE; j++) {
    //     cout << "[" << queue->NAME << "] entry: " << j << " instr_id: " << queue->entry[j].instr_id << " rob_index: " << queue->entry[j].rob_index;
    //     cout << " address: " << hex << queue->entry[j].address << " full_addr: " << queue->entry[j].full_addr << dec << " type: " << +queue->entry[j].type;
    //     cout << " fill_level: " << queue->entry[j].fill_level << " lq_index: " << queue->entry[j].lq_index << " sq_index: " << queue->entry[j].sq_index << endl; 
    // }

    // ooo_cpu[i].reset_rob_and_queues();
    // for (uint32_t ch = 0; ch < DRAM_CHANNELS; ch++) {
    //     uncore.DRAM.RQ[ch].quick_reset();
    //     uncore.DRAM.WQ[ch].quick_reset();
    // }
}
}
    assert(0);
}

void signal_handler(int signal)
{
	cout << "Caught signal: " << signal << endl;
#ifdef USE_TRACE_HELPER
	trace_helper.stop();
#endif
	exit(1);
}



RANDOM champsim_rand(champsim_seed);
uint64_t va_to_pa(uint32_t cpu, uint64_t instr_id, uint64_t va, uint64_t unique_vpage) {
#ifdef SANITY_CHECK
    if (va == 0) 
        assert(0&& "va should never be 0");
#endif

    uint8_t  swap = 0;
    uint64_t high_bit_mask = rotr64(cpu, lg2(NUM_CPUS)),
             unique_va = va | high_bit_mask;
    //uint64_t vpage = unique_va >> LOG2_PAGE_SIZE,
    uint64_t vpage = unique_vpage | high_bit_mask,
             voffset = unique_va & ((1<<LOG2_PAGE_SIZE) - 1);

    // smart random number generator
    uint64_t random_ppage;

    // map <uint64_t, uint64_t>::iterator pr = page_table.begin();
    // map <uint64_t, uint64_t>::iterator ppage_check = inverse_table.begin();

    // // check unique cache line footprint
    // map <uint64_t, uint64_t>::iterator cl_check = unique_cl[cpu].find(unique_va >> LOG2_BLOCK_SIZE);
    ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator pr = page_table.begin();
    ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator ppage_check = inverse_table.begin();

    // check unique cache line footprint
    ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator cl_check = unique_cl[cpu].find(unique_va >> LOG2_BLOCK_SIZE);
    if (cl_check == unique_cl[cpu].end()) { // we've never seen this cache line before
        unique_cl[cpu].insert(make_pair(unique_va >> LOG2_BLOCK_SIZE, 0));
        num_cl[cpu]++;
    }
    else
        cl_check->second++;

    pr = page_table.find(vpage);
    if (pr == page_table.end()) { // no VA => PA translation found 

        if (allocated_pages >= DRAM_PAGES) { // not enough memory

            // TODO: elaborate page replacement algorithm
            // here, ChampSim randomly selects a page that is not recently used and we only track 32K recently accessed pages
            uint8_t  found_NRU = 0;
            uint64_t NRU_vpage = 0; // implement it
            // map <uint64_t, uint64_t>::iterator pr2 = recent_page.begin();
            for (pr = page_table.begin(); pr != page_table.end(); pr++) {
                NRU_vpage = pr->first;
                if (recent_page.find(NRU_vpage) == recent_page.end()) {
                    found_NRU = 1;
                    break;
                }
            }
#ifdef SANITY_CHECK
            if (found_NRU == 0)
                assert(0);

            if (pr == page_table.end())
                assert(0);
#endif
            // DP ( if (warmup_complete[cpu]) {
            // cout << "[SWAP] update page table NRU_vpage: " << hex << pr->first << " new_vpage: " << vpage << " ppage: " << pr->second << dec << endl; });

            // update page table with new VA => PA mapping
            // since we cannot change the key value already inserted in a map structure, we need to erase the old node and add a new node
            uint64_t mapped_ppage = pr->second;
            page_table.erase(pr);
            page_table.insert(make_pair(vpage, mapped_ppage));

            // update inverse table with new PA => VA mapping
            ppage_check = inverse_table.find(mapped_ppage);
#ifdef SANITY_CHECK
            if (ppage_check == inverse_table.end())
                assert(0);
#endif
            ppage_check->second = vpage;

            // DP ( if (warmup_complete[cpu]) {
            // cout << "[SWAP] update inverse table NRU_vpage: " << hex << NRU_vpage << " new_vpage: ";
            // cout << ppage_check->second << " ppage: " << ppage_check->first << dec << endl; });

            // update page_queue
            page_queue.pop();
            page_queue.push(vpage);

            // invalidate corresponding vpage and ppage from the cache hierarchy
            ooo_cpu[cpu].ITLB.invalidate_entry(NRU_vpage);
            ooo_cpu[cpu].DTLB.invalidate_entry(NRU_vpage);
            ooo_cpu[cpu].STLB.invalidate_entry(NRU_vpage);
            for (uint32_t i=0; i<BLOCK_SIZE; i++) {
                uint64_t cl_addr = (mapped_ppage << 6) | i;
                ooo_cpu[cpu].L1I.invalidate_entry(cl_addr);
                ooo_cpu[cpu].L1D.invalidate_entry(cl_addr);
                ooo_cpu[cpu].L2C.invalidate_entry(cl_addr);
                uncore.LLC.invalidate_entry(cl_addr);
            }

            // swap complete
            swap = 1;
        } else {
            uint8_t fragmented = 0;
            if (num_adjacent_page > 0)
                random_ppage = ++previous_ppage;
            else {
                random_ppage = champsim_rand.draw_rand();
                fragmented = 1;
            }
            // encoding cpu number 
            // this allows ChampSim to run homogeneous multi-programmed workloads without VA => PA aliasing
            // (e.g., cpu0: astar  cpu1: astar  cpu2: astar  cpu3: astar...)
            //random_ppage &= (~((NUM_CPUS-1)<< (32-LOG2_PAGE_SIZE)));
            //random_ppage |= (cpu<<(32-LOG2_PAGE_SIZE)); 

            while (1) { // try to find an empty physical page number
                ppage_check = inverse_table.find(random_ppage); // check if this page can be allocated 
                if (ppage_check != inverse_table.end()) { // random_ppage is not available
                    // DP ( if (warmup_complete[cpu]) {
                    // cout << "vpage: " << hex << ppage_check->first << " is already mapped to ppage: " << random_ppage << dec << endl; }); 
                    
                    if (num_adjacent_page > 0)
                        fragmented = 1;

                    // try one more time
                    random_ppage = champsim_rand.draw_rand();
                    
                    // encoding cpu number 
                    //random_ppage &= (~((NUM_CPUS-1)<<(32-LOG2_PAGE_SIZE)));
                    //random_ppage |= (cpu<<(32-LOG2_PAGE_SIZE)); 
                }
                else
                    break;
            }

            // insert translation to page tables
            //printf("Insert  num_adjacent_page: %u  vpage: %lx  ppage: %lx\n", num_adjacent_page, vpage, random_ppage);
            page_table.insert(make_pair(vpage, random_ppage));
            inverse_table.insert(make_pair(random_ppage, vpage));
            page_queue.push(vpage);
            previous_ppage = random_ppage;
            num_adjacent_page--;
            num_page[cpu]++;
            allocated_pages++;

            // try to allocate pages contiguously
            if (fragmented) {
                num_adjacent_page = 1 << (rand() % 10);
                // DP ( if (warmup_complete[cpu]) {
                // cout << "Recalculate num_adjacent_page: " << num_adjacent_page << endl; });
            }
        }

        if (swap)
            major_fault[cpu]++;
        else
            minor_fault[cpu]++;
    }
    else {
        //printf("Found  vpage: %lx  random_ppage: %lx\n", vpage, pr->second);
    }

    pr = page_table.find(vpage);
#ifdef SANITY_CHECK
    if (pr == page_table.end())
        assert(0);
#endif
    uint64_t ppage = pr->second;

    uint64_t pa = ppage << LOG2_PAGE_SIZE;
    pa |= voffset;

    // DP ( if (warmup_complete[cpu]) {
    // cout << "[PAGE_TABLE] instr_id: " << instr_id << " vpage: " << hex << vpage;
    // cout << " => ppage: " << (pa >> LOG2_PAGE_SIZE) << " vadress: " << unique_va << " paddress: " << pa << dec << endl; });

    if (swap)
        stall_cycle[cpu] = PACK_CYCLE(current_core_cycle[cpu] + SWAP_LATENCY);
    else
        stall_cycle[cpu] = PACK_CYCLE(current_core_cycle[cpu] + PAGE_TABLE_LATENCY);

    //cout << "cpu: " << cpu << " allocated unique_vpage: " << hex << unique_vpage << " to ppage: " << ppage << dec << endl;

    return pa;
}

void print_knobs()
{
    cout << "warmup_instructions " << warmup_instructions << endl
        << "simulation_instructions " << simulation_instructions << endl
        << "champsim_seed " << champsim_seed << endl
        // << "low_bandwidth " << knob_low_bandwidth << endl
        // << "scramble_loads " << knob_scramble_loads << endl
        // << "cloudsuite " << knob_cloudsuite << endl
        << endl;
    cout << "num_cpus\t;" << NUM_CPUS<<";" << endl
        << "cpu_freq\t;" << CPU_FREQ<<";\t" 
        << "dram_io_freq\t;" << DRAM_IO_FREQ<<";" << endl
        << "page_size\t;" << PAGE_SIZE<<";\t"
        << "block_size\t;" << BLOCK_SIZE<<";" << endl
        << "max_read_per_cycle\t;" << MAX_READ_PER_CYCLE<<";\t"
        << "max_fill_per_cycle\t;" << MAX_FILL_PER_CYCLE<<";" << endl
        << "dram_channels\t;" << DRAM_CHANNELS<<"; "
        << "dram_ranks\t;" << DRAM_RANKS<<"; "
        << "dram_banks\t;" << DRAM_BANKS<<"; "
        << "dram_rows\t;" << DRAM_ROWS<<"; "
        << "dram_columns\t;" << DRAM_COLUMNS<<"; "
        << "dram_row_size\t;" << DRAM_ROW_SIZE<<"; "
        << "dram_size\t;" << DRAM_SIZE<<"; "
        << "dram_pages\t;" << DRAM_PAGES<<"; " << endl
        << "OffChipPred\t;" << HERMES_LABEL << ";" << endl
        << endl;
    print_core_config();
    print_dram_config();
    cout << endl;
}
#define ROB_MASK (ROB_SIZE - 1)
// static char zero_arena[1024*1024*1024] = {0};
int main(int argc, char** argv)
{
    // #if defined(__has_feature)
    // #if __has_feature(memory_sanitizer)
    // // Unpoison argv
    // for (int i = 0; i < argc; i++) {
    //     if (argv[i]) {
    //         __msan_unpoison(argv[i], 32768);
    //     }
    // }
    // #endif
    // #endif
    // memset(&ooo_cpu, 0, sizeof(ooo_cpu));
    // memset(&uncore, 0, sizeof(uncore));
    
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
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

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

    uint8_t run_simulation = 1;
    while (run_simulation) {

        uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
                 elapsed_minute = elapsed_second / 60,
                 elapsed_hour = elapsed_minute / 60;
        elapsed_minute -= elapsed_hour*60;
        elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);
        
        for (int i=0; i<NUM_CPUS; i++) {
            
            // proceed one cycle
            current_core_cycle[i]++;
            if (PCYCLE_LE(stall_cycle[i], PACK_CYCLE(current_core_cycle[i]))) {

                // fetch unit
                if (ooo_cpu[i].ROB.occupancy < ooo_cpu[i].ROB.SIZE) {
                    // handle branch
                    if (ooo_cpu[i].fetch_stall == 0) 
                    ooo_cpu[i].handle_branch();
                }

                    // fetch
                __builtin_prefetch(&rob_events.per_cpu[i].fetched_complete[0], 0, 3);
                __builtin_prefetch(&rob_events.per_cpu[i].sched_inflight[0], 0, 3);
                __builtin_prefetch(&rob_events.per_cpu[i].sched_complete[0], 0, 3);
                ooo_cpu[i].fetch_instruction();


                // schedule (including decode latency)
                uint32_t schedule_index = ooo_cpu[i].ROB.next_schedule;

                if (!BS_TST(rob_events.per_cpu[i].sched_inflight, schedule_index) && !BS_TST(rob_events.per_cpu[i].sched_complete, schedule_index) && PCYCLE_LE(rob_events.per_cpu[i].event_cycle[schedule_index], PACK_CYCLE(current_core_cycle[i])))
                    ooo_cpu[i].schedule_instruction();
                // memory operation
                ooo_cpu[i].schedule_memory_instruction();

                // execute
                ooo_cpu[i].execute_instruction();

                // memory operation
                ooo_cpu[i].execute_memory_instruction();

                // complete
                ooo_cpu[i].update_rob();

                // retire
                if ((ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].executed == COMPLETED) && PCYCLE_LE(rob_events.per_cpu[i].event_cycle[ooo_cpu[i].ROB.head], PACK_CYCLE(current_core_cycle[i])))
                    ooo_cpu[i].retire_rob();

            }

            // heartbeat information
            if (show_heartbeat && (ooo_cpu[i].num_retired >= ooo_cpu[i].next_print_instruction)) {
                lpm_demand_compute_all(i); // Lazy: refresh met[] before printing
                float cumulative_ipc;
                if (warmup_complete[i])
                    cumulative_ipc = (1.0*(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle);
                else
                    cumulative_ipc = (1.0*ooo_cpu[i].num_retired) / current_core_cycle[i];
                float heartbeat_ipc = (1.0*ooo_cpu[i].num_retired - ooo_cpu[i].last_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].last_sim_cycle);

                cout << STR_CORE_NUM << setw(2) << i
                     << STR_INSTR << " " << setw(10) << ooo_cpu[i].num_retired
                     << " " << STR_CYCLES << " " << setw(10) << current_core_cycle[i];

                // cout << " now IPC :" << FIXED_FLOAT(heartbeat_ipc) << ": AVG IPC :" << FIXED_FLOAT(cumulative_ipc)<< ": L2-Pf-HR :" << print_pf_hitRatio(i,&ooo_cpu[i].L2C)<< ": L2-HR :"<<print_L2_hitRatio(i,&ooo_cpu[i].L2C);
                cout << " " << STR_IPC_NOW << FIXED_FLOAT2(heartbeat_ipc)
                     << " " << STR_IPC_AVG << FIXED_FLOAT(cumulative_ipc)
                     << " " << STR_BYPASS
                     << dec
                     << setw(3) << right << (int)(ooo_cpu[i].L1D.ByP_req[i] ? ((float)ooo_cpu[i].L1D.ByP_issued[i] / (float)ooo_cpu[i].L1D.ByP_req[i]) * 100 : 0)
                     << setw(3) << right << (int)(ooo_cpu[i].L2C.ByP_req[i] ? ((float)ooo_cpu[i].L2C.ByP_issued[i] / (float)ooo_cpu[i].L2C.ByP_req[i]) * 100 : 0)
                     << setw(3) << right << (int)(uncore.LLC.ByP_req[i] ? ((float)uncore.LLC.ByP_issued[i] / (float)uncore.LLC.ByP_req[i]) * 100 : 0)
                     << left 
                     << " " STR_APC
                     << "①" << setw(6) << FIXED_FLOAT2(lpm[i][L1D_type].met[MET_G].cpa_totalCyDivAccesses)
                     << "②" << setw(6) << FIXED_FLOAT2(lpm[i][L2C_type].met[MET_G].cpa_totalCyDivAccesses)
                     << "③" << setw(6) << FIXED_FLOAT2(lpm[i][LLC_type].met[MET_G].cpa_totalCyDivAccesses)
                     << "Ⓜ" << setw(6) << FIXED_FLOAT2(lpm[i][DRAM_type].met[MET_G].cpa_totalCyDivAccesses)
                     << left
                     << " " STR_LPM
                     << "⚙" << setw(5) << FIXED_FLOAT2(get_LPMR_global_std(i))
                     << "①" << setw(6) << FIXED_FLOAT2(lpm[i][L1D_type].met[MET_G].lpmr_activeMemCyDivIdealCy)
                     << "②" << setw(6) << FIXED_FLOAT2(lpm[i][L2C_type].met[MET_G].lpmr_activeMemCyDivIdealCy)
                     << "③" << setw(6) << FIXED_FLOAT2(lpm[i][LLC_type].met[MET_G].lpmr_activeMemCyDivIdealCy)
                     << "Ⓜ" << setw(6) << FIXED_FLOAT2(lpm[i][DRAM_type].met[MET_G].lpmr_activeMemCyDivIdealCy)
                     << left
                     << " " STR_cAMT
                     << "①" << setw(6) << FIXED_FLOAT2(lpm[i][L1D_type].met[MET_G].camat_activeMemCyDivAccesses)
                     << "②" << setw(6) << FIXED_FLOAT2(lpm[i][L2C_type].met[MET_G].camat_activeMemCyDivAccesses)
                     << "③" << setw(6) << FIXED_FLOAT2(lpm[i][LLC_type].met[MET_G].camat_activeMemCyDivAccesses)
                     << "Ⓜ" << setw(6) << FIXED_FLOAT2(lpm[i][DRAM_type].met[MET_G].camat_activeMemCyDivAccesses)
                     << left
                     << " " STR_MST
                     << "①" << setw(6) << FIXED_FLOAT2(lpm[i][L1D_type].met[MET_G].mst_pureMissCyDivAccesses)
                     << "②" << setw(6) << FIXED_FLOAT2(lpm[i][L2C_type].met[MET_G].mst_pureMissCyDivAccesses)
                     << "③" << setw(6) << FIXED_FLOAT2(lpm[i][LLC_type].met[MET_G].mst_pureMissCyDivAccesses)
                     << "Ⓜ" << setw(6) << FIXED_FLOAT2(lpm[i][DRAM_type].met[MET_G].mst_pureMissCyDivAccesses)
                     << left
                     << " " << STR_MSHR_OCCUPANCY_PERCENT
                     << setw(3) << right << (int)(((float)ooo_cpu[i].L1D.MSHR.occupancy/(float)ooo_cpu[i].L1D.MSHR.SIZE)*100)
                     << setw(3) << right << (int)(((float)ooo_cpu[i].L2C.MSHR.occupancy/(float)ooo_cpu[i].L2C.MSHR.SIZE)*100)
                     << setw(3) << right << (int)(((float)uncore.LLC.MSHR.occupancy/(float)uncore.LLC.MSHR.SIZE)*100)
                    //  << left << " " << STR_LOAD_HIT_RATE
                    //  << setw(3) << right << (int)(((float)ooo_cpu[i].L1D.sim_hit[i][0]/(float)ooo_cpu[i].L1D.sim_access[i][0])*100)
                    //  << setw(3) << right << (int)(((float)ooo_cpu[i].L2C.sim_hit[i][0]/(float)ooo_cpu[i].L2C.sim_access[i][0])*100)
                    //  << setw(3) << right << (int)(((float)uncore.LLC.sim_hit[i][0]/(float)uncore.LLC.sim_access[i][0])*100)
                     << " LoadByPH%"
                     << setw(3) << right << (int)(ooo_cpu[i].L1D.sim_access_wByP[i] ? ((float)ooo_cpu[i].L1D.sim_hit_wByP[i]/(float)ooo_cpu[i].L1D.sim_access_wByP[i])*100 : 0)
                     << setw(3) << right << (int)(ooo_cpu[i].L2C.sim_access_wByP[i] ? ((float)ooo_cpu[i].L2C.sim_hit_wByP[i]/(float)ooo_cpu[i].L2C.sim_access_wByP[i])*100 : 0)
                     << setw(3) << right << (int)(uncore.LLC.sim_access_wByP[i] ? ((float)uncore.LLC.sim_hit_wByP[i]/(float)uncore.LLC.sim_access_wByP[i])*100 : 0)
                     << " " << STR_TIME
                     << setw(2) << right << elapsed_hour << "h"
                     << setw(2) << right << elapsed_minute << "m"
                     << setw(2) << right << elapsed_second << "s "
                     << STR_TimePerMillionInstr
                     << FIXED_FLOAT2(((double)(elapsed_hour*3600ULL + elapsed_minute*60ULL + elapsed_second) /
                        (ooo_cpu[i].num_retired / 1000000.0)))
                     << left << endl;
                ooo_cpu[i].next_print_instruction += STAT_PRINTING_PERIOD;

                ooo_cpu[i].last_sim_instr = ooo_cpu[i].num_retired;
                ooo_cpu[i].last_sim_cycle = current_core_cycle[i];

                // reset interval bypass counters after heartbeat print
                ooo_cpu[i].L1D.ByP_issued[i] = 0;
                ooo_cpu[i].L1D.ByP_req[i] = 0;
                ooo_cpu[i].L2C.ByP_issued[i] = 0;
                ooo_cpu[i].L2C.ByP_req[i] = 0;
                uncore.LLC.ByP_issued[i] = 0;
                uncore.LLC.ByP_req[i] = 0;
            }

            // check for deadlock: event must be in the past AND distance >= DEADLOCK_CYCLE
            { uint64_t _ec = rob_events.per_cpu[i].event_cycle[ooo_cpu[i].ROB.head];
              if (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].ip && PCYCLE_LE(_ec, PACK_CYCLE(current_core_cycle[i])) && PCYCLE_DIFF(PACK_CYCLE(current_core_cycle[i]), _ec) >= DEADLOCK_CYCLE)
                print_deadlock(i); }

            // check for warmup
            // warmup complete
            if ((warmup_complete[i] == 0) && (ooo_cpu[i].num_retired > warmup_instructions)) {
                warmup_complete[i] = 1;
                all_warmup_complete++;
            }
            if (all_warmup_complete == NUM_CPUS) { // this part is called only once when all cores are warmed up
                all_warmup_complete++;
                finish_warmup();
            }

            /*
            if (all_warmup_complete == 0) { 
                all_warmup_complete = 1;
                finish_warmup();
            }
            if (ooo_cpu[1].num_retired > 0)
                warmup_complete[1] = 1;
            */
            
            // simulation complete
            if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0) && (ooo_cpu[i].num_retired >= (ooo_cpu[i].begin_sim_instr + ooo_cpu[i].simulation_instructions))) {
                simulation_complete[i] = 1;
                ooo_cpu[i].finish_sim_instr = ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr;
                ooo_cpu[i].finish_sim_cycle = current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle;

                cout << setprecision(5) << "Finished CPU ;" << i << "; instr: ;" << ooo_cpu[i].finish_sim_instr << "; cyc: " << ooo_cpu[i].finish_sim_cycle;
                cout << "; AVG IPC: ;" << ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle)<<";";
                cout << " (Sim time: " << elapsed_hour << "h" << elapsed_minute << "m" << elapsed_second << "s)" <<";TPMI;"<<round(((double)(elapsed_hour*3600ULL + elapsed_minute*60ULL + elapsed_second) / (ooo_cpu[i].num_retired / 1000000.0)) * 100) / 100<< endl;
                
                
                record_roi_stats(i, &ooo_cpu[i].L1D);
                record_roi_stats(i, &ooo_cpu[i].L1I);
                record_roi_stats(i, &ooo_cpu[i].L2C);
                record_roi_stats(i, &uncore.LLC);
                lpm_demand_compute_all(i); // Lazy: refresh met[] before ROI snapshot
                lpm_record_roi(i);

                all_simulation_complete++;
            }

            if (all_simulation_complete == NUM_CPUS)
                run_simulation = 0;
        
    }

        // TODO: should it be backward?
        uncore.LLC.operate();
        uncore.DRAM.operate();
    }

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

    cout << endl << "[ROI Statistics]" << endl;
    for (uint32_t i=0; i<NUM_CPUS; i++)
    {
        cout << "Core_" << i << "_instructions " << ooo_cpu[i].finish_sim_instr << endl
            << "Core_" << i << "_cycles " << ooo_cpu[i].finish_sim_cycle << endl
            << "Core_" << i << "_IPC " << ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle) << endl
            << endl;
        TOTAL_SUM_FINAL_SIM_IPC += ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle);
        statsCollector.collectCoreROIStats(
            i,
            ooo_cpu[i].finish_sim_instr,
            ooo_cpu[i].finish_sim_cycle,
            ( (double)ooo_cpu[i].finish_sim_instr / (double)ooo_cpu[i].finish_sim_cycle )
        );
#ifndef CRC2_COMPILE
        print_branch_stats(i);
        print_roi_stats(i, &ooo_cpu[i].L1D);
        print_roi_stats(i, &ooo_cpu[i].L1I);
        print_roi_stats(i, &ooo_cpu[i].L2C);
#endif
        print_roi_stats(i, &uncore.LLC);
        cout << "Core_" << i << "_major_page_fault " << major_fault[i] << endl
            << "Core_" << i << "_minor_page_fault " << minor_fault[i] << endl
            << endl;
        statsCollector.collectPageFaultStats(
            i,
            major_fault[i],
            minor_fault[i]
        );
        lpm_print(i);
        byplat_print(i);
        cout << "Core_;" << i << ";DRAM;_;APC;" << lpm[i][DRAM_type].met[MET_G].apc_accessesDivActiveMemCy << ";"
             << "Core_;" << i << ";DRAM;_;LPM;" << lpm[i][DRAM_type].met[MET_G].lpmr_activeMemCyDivIdealCy << ";"
             << "Core_;" << i << ";DRAM;_;C-AMAT;" << lpm[i][DRAM_type].met[MET_G].camat_activeMemCyDivAccesses << ";"
             << "Core_;" << i << ";DRAM;_;MST;" << lpm[i][DRAM_type].met[MET_G].mst_pureMissCyDivAccesses << ";" << endl;
    }

    for (uint32_t i=0; i<NUM_CPUS; i++) {
        ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
        ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
    }

    uncore.LLC.llc_prefetcher_final_stats();

#ifndef CRC2_COMPILE
    uncore.LLC.llc_replacement_final_stats();
    print_dram_stats();
#endif
    // cout << "STAT_ROI_DICT|"<<statsCollector.dumpAllAsString()<<"|" << endl;
    // print execution_checksum
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << "Core_" << i << "_execution_checksum " << execution_checksum[i] << endl;
    }

#ifdef USE_HERMES
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
        ooo_cpu[i].dump_stats_offchip_predictor();
    }
#endif
    cout << "FINAL ROI CORE AVG IPC: ;" << (TOTAL_SUM_FINAL_SIM_IPC / NUM_CPUS)<<";" << endl;

    for (uint32_t i = 0; i < NUM_CPUS; i++) lpm_demand_compute_all(i); // Lazy: refresh met[] before db write
    champsim_db_store(db_cfg, TOTAL_SUM_FINAL_SIM_IPC / NUM_CPUS,
                      major_fault, minor_fault);

    return 0;
}
