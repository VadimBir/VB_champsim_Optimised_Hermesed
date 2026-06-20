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
