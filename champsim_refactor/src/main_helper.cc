#pragma once
// FinalStatsCollector — offloaded from main.cc (same-TU #include; CACHE, the
// ankerl map, <sstream>/<string> must be visible at the include site in main.cc).
// Collects per-core/cache/channel stats into a dict for a JSON-like dump.
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

FinalStatsCollector statsCollector;  // global or local inside main

alignas(64) rob_events_soa rob_events = {};
alignas(64) MemIndexRing mem_index_ring = {};

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

inline void print_end_of_sim_report() {
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
}
