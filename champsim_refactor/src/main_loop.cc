inline void simulation_loop(uint8_t show_heartbeat) {
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
}
