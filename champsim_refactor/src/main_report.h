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
