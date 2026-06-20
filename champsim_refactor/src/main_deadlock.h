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
