#include "dram_controller.h"
#include "lpm_tracker.h"
#include "cycle_pack.h"
#ifdef BYP_DERFILL_ACTIVE
#include "cache.h"  // needed for CACHE* cast in derivative fill
#endif
IF_HERMES( namespace knob { extern bool enable_ddrp; extern bool dram_cntlr_enable_ddrp_buffer; } )

#include "dram_helper.h"

// initialized in main.cc
uint32_t DRAM_MTPS, DRAM_DBUS_RETURN_TIME,
         tRP, tRCD, tCAS;

void MEMORY_CONTROLLER::reset_remain_requests(PACKET_QUEUE *queue, uint32_t channel)
{
    for (uint32_t i=0; i<queue->SIZE; i++) {
        if (queue->entry[i].scheduled) {

            uint64_t op_addr = queue->entry[i].address;
            uint32_t op_cpu = queue->entry[i].cpu,
                     op_channel = dram_get_channel(op_addr), 
                     op_rank = dram_get_rank(op_addr), 
                     op_bank = dram_get_bank(op_addr), 
                     op_row = dram_get_row(op_addr);

#ifdef DEBUG_PRINT
            //uint32_t op_column = dram_get_column(op_addr);
#endif

            // update open row
            if (CYC_LE(bank_cycle_available[op_channel][op_rank][op_bank] - tCAS, CYC(current_core_cycle[op_cpu])))
                bank_request[op_channel][op_rank][op_bank].open_row = op_row;
            else
                bank_request[op_channel][op_rank][op_bank].open_row = UINT32_MAX;

            // this bank is ready for another DRAM request
            bank_request[op_channel][op_rank][op_bank].request_index = -1;
            bank_request[op_channel][op_rank][op_bank].row_buffer_hit = 0;
            if (bank_hot.entries[op_channel][op_rank][op_bank].working) working_bank_count--; // Opt-A: 1->0 transition only
            bank_hot.entries[op_channel][op_rank][op_bank].working = 0; // working=0
            bank_cycle_available[op_channel][op_rank][op_bank] = current_core_cycle[op_cpu]; // full 64-bit cycle
            if (bank_request[op_channel][op_rank][op_bank].is_write) {
                scheduled_writes[channel]--;
                bank_request[op_channel][op_rank][op_bank].is_write = 0;
            }
            else if (bank_request[op_channel][op_rank][op_bank].is_read) {
                scheduled_reads[channel]--;
                bank_request[op_channel][op_rank][op_bank].is_read = 0;
            }

            queue->entry[i].scheduled = 0;
            queue->entry[i].event_cycle = PACK_CYCLE(current_core_cycle[op_cpu]);

            // DP ( if (warmup_complete[op_cpu]) {
            // cout << queue->NAME << " instr_id: " << queue->entry[i].instr_id << " swrites: " << scheduled_writes[channel] << " sreads: " << scheduled_reads[channel] << endl; });

        }
    }
    
    update_schedule_cycle(&RQ[channel]);
    update_schedule_cycle(&WQ[channel]);
    update_process_cycle(&RQ[channel]);
    update_process_cycle(&WQ[channel]);

    IF_SANITY_CHECK(
    if (queue->is_WQ) {
        if (scheduled_writes[channel] != 0)
            assert(0);
    }
    else {
        if (scheduled_reads[channel] != 0)
            assert(0);
    }
    )
}

void MEMORY_CONTROLLER::operate()
{ //TIME_it("      MM Op");

    /* LPM tick for DRAM — one tick per CPU per cycle */
    // A1: once all_warmup_complete latches, skip per-cpu warmup check (kills 1 branch/cpu/cycle)
    if (all_warmup_complete) {
        // Opt-B (DRAM idle-batch, stats-only): when the controller is FULLY idle
        // (no working banks AND every cpu's RQ+WQ empty), every cpu would receive
        // tick(false,false) i.e. a CY_E (idle) cycle and tick_alphas() is a no-op
        // (sim_read_access only advances inside process(), which cannot run while
        // idle). Batch those identical idle cycles into dram_idle_accumulator and
        // SKIP the per-cpu LPM work this cycle. On the first non-idle cycle FLUSH:
        // advance_idle(N) per cpu produces byte-identical LPM state to N separate
        // tick(false,false) calls (verified exhaustively for this build config),
        // so the execution_checksum is unaffected.
        bool fully_idle = (working_bank_count == 0);
        if (fully_idle) {
            for (uint32_t cpu = 0; cpu < NUM_CPUS; cpu++) {
                if (rw_occ[cpu].both != 0) { fully_idle = false; break; }
            }
        }

        if (fully_idle) {
            dram_idle_accumulator++;
        } else {
            // Non-idle cycle: flush any accumulated idle cycles first (transition out of idle).
            if (dram_idle_accumulator != 0) {
                uint32_t N = (uint32_t)dram_idle_accumulator;
                for (uint32_t cpu = 0; cpu < NUM_CPUS; cpu++)
                    lpm[cpu][LPM_DRAM].advance_idle(N);
                dram_idle_accumulator = 0;
            }

        for (uint32_t cpu = 0; cpu < NUM_CPUS; cpu++) {

        // Branchless: rw_occ.both==0 iff (RQ empty AND WQ empty) for this cpu.
        bool hit_active = (rw_occ[cpu].both != 0);

        // miss_active — Opt-A: O(1) counter replaces O(banks) working-flag scan (stats-only signal).
        bool miss_active = (working_bank_count > 0);

        // alpha: LOAD completions served to this CPU (analogous to sim_access[cpu][LOAD] in CACHE)
        uint64_t alpha = sim_read_access[cpu];

        lpm[cpu][LPM_DRAM].tick(hit_active, miss_active);
        lpm[cpu][LPM_DRAM].tick_alphas(alpha, 0, 0);
        } // for cpu (all_warmup_complete fast path)
        } // else (non-idle: flushed accumulator + ticked this cycle)
    } else {
        // Pre-warmup slow path: per-cpu check retained. Runs for at most warmup duration.
        for (uint32_t cpu = 0; cpu < NUM_CPUS; cpu++) {
            if (!warmup_complete[cpu]) continue;

            bool hit_active = (rw_occ[cpu].both != 0);
            // miss_active — Opt-A: O(1) counter replaces O(banks) working-flag scan (stats-only signal).
            bool miss_active = (working_bank_count > 0);

            uint64_t alpha = sim_read_access[cpu];
            lpm[cpu][LPM_DRAM].tick(hit_active, miss_active);
            lpm[cpu][LPM_DRAM].tick_alphas(alpha, 0, 0);
        }
    }

    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        //if ((write_mode[i] == 0) && (WQ[i].occupancy >= DRAM_WRITE_HIGH_WM)) {
      if ((write_mode[i] == 0) && ((WQ[i].occupancy >= DRAM_WRITE_HIGH_WM) || ((RQ[i].occupancy == 0) && (WQ[i].occupancy > 0)))) { // use idle cycles to perform writes
            DRAM_CH_DP("OP_WM_FLIP_0to1", current_core_cycle[DEBUG_CPU], i,
                       (WQ[i].occupancy >= DRAM_WRITE_HIGH_WM ? "WQ_HI_WM" : "RQ_EMPTY_WQ_HAS"));
            write_mode[i] = 1;

            // reset scheduled RQ requests
            reset_remain_requests(&RQ[i], i);
            // add data bus turn-around time
            dbus_cycle_available[i] += DRAM_DBUS_TURN_AROUND_TIME;
            DRAM_CH_DP("OP_WM_POST_0to1", current_core_cycle[DEBUG_CPU], i, "RQ_RESET_DBUS_TAT_ADDED");
        } else if (write_mode[i]) {

            if (WQ[i].occupancy == 0) {
                DRAM_CH_DP("OP_WM_FLIP_1to0", current_core_cycle[DEBUG_CPU], i, "WQ_EMPTY");
                write_mode[i] = 0;
            }
            else if (RQ[i].occupancy && (WQ[i].occupancy < DRAM_WRITE_LOW_WM)) {
                DRAM_CH_DP("OP_WM_FLIP_1to0", current_core_cycle[DEBUG_CPU], i, "RQ_PRESSURE_WQ_LO_WM");
                write_mode[i] = 0;
            }

            if (write_mode[i] == 0) {
                // reset scheduled WQ requests
                reset_remain_requests(&WQ[i], i);
                // add data bus turnaround time
                dbus_cycle_available[i] += DRAM_DBUS_TURN_AROUND_TIME;
                DRAM_CH_DP("OP_WM_POST_1to0", current_core_cycle[DEBUG_CPU], i, "WQ_RESET_DBUS_TAT_ADDED");
            }
        }

        // handle write
        // schedule new entry
        if (write_mode[i] && (WQ[i].next_schedule_index < WQ[i].SIZE)) {
            uint32_t _nsi = WQ[i].next_schedule_index;
            uint64_t _ev_cy = current_core_cycle[WQ[i].entry[_nsi].cpu];
            if ((WQ[i].next_schedule_cycle != CYC_PACKED_MAX) && PCYCLE_LE(WQ[i].next_schedule_cycle, PACK_CYCLE(_ev_cy))) {
                DRAM_FULL_DP("OP_WQ_SCH_CALL", _ev_cy, WQ[i].entry[_nsi].cpu, i,
                             dram_get_rank(WQ[i].entry[_nsi].address), dram_get_bank(WQ[i].entry[_nsi].address),
                             WQ[i].entry[_nsi], "GATE_OK");
                schedule(&WQ[i], i);
            } else {
                DRAM_FULL_DP("OP_WQ_SCH_SKIP", _ev_cy, WQ[i].entry[_nsi].cpu, i,
                             dram_get_rank(WQ[i].entry[_nsi].address), dram_get_bank(WQ[i].entry[_nsi].address),
                             WQ[i].entry[_nsi],
                             (WQ[i].next_schedule_cycle == CYC_PACKED_MAX ? "NFC_PACKED_MAX" : "NFC_FUTURE"));
            }
        }

        // process DRAM requests
        if (write_mode[i] && (WQ[i].next_process_index < WQ[i].SIZE)) {
            uint32_t _npi = WQ[i].next_process_index;
            uint64_t _ev_cy = current_core_cycle[WQ[i].entry[_npi].cpu];
            if ((WQ[i].next_process_cycle != CYC_PACKED_MAX) && PCYCLE_LE(WQ[i].next_process_cycle, PACK_CYCLE(_ev_cy))) {
                DRAM_FULL_DP("OP_WQ_PROC_CALL", _ev_cy, WQ[i].entry[_npi].cpu, i,
                             dram_get_rank(WQ[i].entry[_npi].address), dram_get_bank(WQ[i].entry[_npi].address),
                             WQ[i].entry[_npi], "GATE_OK");
                process(&WQ[i]);
            } else {
                DRAM_FULL_DP("OP_WQ_PROC_SKIP", _ev_cy, WQ[i].entry[_npi].cpu, i,
                             dram_get_rank(WQ[i].entry[_npi].address), dram_get_bank(WQ[i].entry[_npi].address),
                             WQ[i].entry[_npi],
                             (WQ[i].next_process_cycle == CYC_PACKED_MAX ? "NPC_PACKED_MAX" : "NPC_FUTURE"));
            }
        }

        // handle read
        // schedule new entry
        // cout << "GBG: " << i << " WrMod:" << write_mode[i] << " NxtSched:" << RQ[i].next_schedule_index << " ROB_sz:" << RQ[i].SIZE << " NxtSched:" << RQ[i].next_schedule_cycle << " cpu:" << RQ[i].entry[RQ[i].next_schedule_index].cpu << "currCy:" << current_core_cycle[RQ[i].entry[RQ[i].next_schedule_index].cpu] << endl;
        // if ((write_mode[i] == 0) && (RQ[i].next_schedule_index < RQ[i].SIZE)) {
        //     // check bounds of access
        //     if (){}
        //     if (RQ[i].next_schedule_cycle <= current_core_cycle[RQ[i].entry[RQ[i].next_schedule_index].cpu])
        //         schedule(&RQ[i]);
        // }
        if ((write_mode[i] == 0) && (RQ[i].next_schedule_index < RQ[i].SIZE)) {
            // check bounds of access
            if (i < RQ->SIZE && // Ensure i is within RQ array bounds
                RQ[i].next_schedule_index < RQ[i].SIZE && // Ensure next_schedule_index is within entry array bounds
                RQ[i].entry[RQ[i].next_schedule_index].cpu < NUM_CPUS) { // Ensure cpu index is within current_core_cycle bounds
                uint32_t _nsi = RQ[i].next_schedule_index;
                uint64_t _ev_cy = current_core_cycle[RQ[i].entry[_nsi].cpu];
                if ((RQ[i].next_schedule_cycle != CYC_PACKED_MAX) && PCYCLE_LE(RQ[i].next_schedule_cycle, PACK_CYCLE(_ev_cy))) {
                    DRAM_FULL_DP("OP_RQ_SCH_CALL", _ev_cy, RQ[i].entry[_nsi].cpu, i,
                                 dram_get_rank(RQ[i].entry[_nsi].address), dram_get_bank(RQ[i].entry[_nsi].address),
                                 RQ[i].entry[_nsi], "GATE_OK");
                    schedule(&RQ[i], i);
                } else {
                    DRAM_FULL_DP("OP_RQ_SCH_SKIP", _ev_cy, RQ[i].entry[_nsi].cpu, i,
                                 dram_get_rank(RQ[i].entry[_nsi].address), dram_get_bank(RQ[i].entry[_nsi].address),
                                 RQ[i].entry[_nsi],
                                 (RQ[i].next_schedule_cycle == CYC_PACKED_MAX ? "NFC_PACKED_MAX" : "NFC_FUTURE"));
                }
            }
        } else if (write_mode[i] != 0) {
            // wm=1 — RQ schedule disabled this cycle (write_mode high)
            DRAM_CH_DP("OP_RQ_SCH_BLOCK_WM", current_core_cycle[DEBUG_CPU], i, "WM_EQ_1");
        }

        // process DRAM requests
        // if ((write_mode[i] == 0) && (RQ[i].next_process_index < RQ[i].SIZE)) {
        //     if (RQ[i].next_process_cycle <= current_core_cycle[RQ[i].entry[RQ[i].next_process_index].cpu])
        //         process(&RQ[i]);
        // }
        if ((write_mode[i] == 0) && (RQ[i].next_process_index < RQ[i].SIZE)) {
            if (i < RQ->SIZE && // Ensure i is within RQ array bounds
                RQ[i].next_process_index < RQ[i].SIZE && // Ensure next_process_index is within entry array bounds
                RQ[i].entry[RQ[i].next_process_index].cpu < NUM_CPUS) { // Ensure cpu index is within current_core_cycle bounds
                uint32_t _npi = RQ[i].next_process_index;
                uint64_t _ev_cy = current_core_cycle[RQ[i].entry[_npi].cpu];
                if ((RQ[i].next_process_cycle != CYC_PACKED_MAX) && PCYCLE_LE(RQ[i].next_process_cycle, PACK_CYCLE(_ev_cy))) {
                    DRAM_FULL_DP("OP_RQ_PROC_CALL", _ev_cy, RQ[i].entry[_npi].cpu, i,
                                 dram_get_rank(RQ[i].entry[_npi].address), dram_get_bank(RQ[i].entry[_npi].address),
                                 RQ[i].entry[_npi], "GATE_OK");
                    process(&RQ[i]);
                } else {
                    DRAM_FULL_DP("OP_RQ_PROC_SKIP", _ev_cy, RQ[i].entry[_npi].cpu, i,
                                 dram_get_rank(RQ[i].entry[_npi].address), dram_get_bank(RQ[i].entry[_npi].address),
                                 RQ[i].entry[_npi],
                                 (RQ[i].next_process_cycle == CYC_PACKED_MAX ? "NPC_PACKED_MAX" : "NPC_FUTURE"));
                }
            }
        } else if (write_mode[i] != 0) {
            DRAM_CH_DP("OP_RQ_PROC_BLOCK_WM", current_core_cycle[DEBUG_CPU], i, "WM_EQ_1");
        }
    }
}

void MEMORY_CONTROLLER::schedule(PACKET_QUEUE *queue, uint32_t channel)
{
    // P2: num_unsched early-out. occupancy counts non-empty entries; the
    // per-channel scheduled_{reads,writes} count the scheduled subset. When all
    // non-empty entries are already scheduled there is no unscheduled candidate,
    // so both the hit-scan and miss-scan below would hit `continue` on every
    // entry and leave oldest_index == -1. Returning here is exactly equivalent
    // (the skipped DRAM_FULL_DP traces do not affect simulation state/checksum).
    uint32_t _scheduled = queue->is_WQ ? scheduled_writes[channel] : scheduled_reads[channel];
    if (queue->occupancy <= _scheduled)
        return;

    uint64_t read_addr;
    uint32_t read_channel, read_rank, read_bank, read_row;
    uint8_t  row_buffer_hit = 0;

    int oldest_index = -1;
    uint64_t oldest_cycle = CYC_PACKED_MAX;

    // first, search for the oldest open row hit
    for (uint32_t i=0; i<queue->SIZE; i++) {

        // already scheduled
        if (queue->entry[i].scheduled) {
            if (queue->entry[i].address != 0) {
                DRAM_FULL_DP("SCH_HIT_SKIP_ALREADY_SCH", current_core_cycle[queue->entry[i].cpu],
                             queue->entry[i].cpu,
                             dram_get_channel(queue->entry[i].address),
                             dram_get_rank(queue->entry[i].address),
                             dram_get_bank(queue->entry[i].address),
                             queue->entry[i], "ALREADY_SCHEDULED");
            }
            continue;
        }

        // empty entry
        read_addr = queue->entry[i].address;
        if (read_addr == 0)
            continue;

        read_channel = dram_get_channel(read_addr);
        read_rank = dram_get_rank(read_addr);
        read_bank = dram_get_bank(read_addr);

        // bank is busy
        if (bank_hot.entries[read_channel][read_rank][read_bank].working) {
            DRAM_FULL_DP("SCH_HIT_SKIP_BANK_BUSY", current_core_cycle[queue->entry[i].cpu],
                         queue->entry[i].cpu, read_channel, read_rank, read_bank,
                         queue->entry[i], "BANK_WORKING_1");
            continue;
        }

        read_row = dram_get_row(read_addr);
        //read_column = dram_get_column(read_addr);

        // check open row
        if (bank_request[read_channel][read_rank][read_bank].open_row != read_row) {
            DRAM_FULL_DP("SCH_HIT_SKIP_ROW_INACTIVE", current_core_cycle[queue->entry[i].cpu],
                         queue->entry[i].cpu, read_channel, read_rank, read_bank,
                         queue->entry[i], "ROW_MISMATCH_FOR_HIT_SCAN");
            continue;
        }

        // select the oldest entry
        if ((oldest_index == -1) || PCYCLE_LT(queue->entry[i].event_cycle, oldest_cycle)) {
            DRAM_FULL_DP("SCH_HIT_CAND", current_core_cycle[queue->entry[i].cpu],
                         queue->entry[i].cpu, read_channel, read_rank, read_bank,
                         queue->entry[i], "ROW_HIT_CAPTURED");
            oldest_cycle = queue->entry[i].event_cycle;
            oldest_index = i;
            row_buffer_hit = 1;
        }
    }

    if (oldest_index == -1) { // no matching open_row (row buffer miss)

        oldest_cycle = CYC_PACKED_MAX;
        for (uint32_t i=0; i<queue->SIZE; i++) {

            // already scheduled
            if (queue->entry[i].scheduled)
                continue;

            // empty entry
            read_addr = queue->entry[i].address;
            if (read_addr == 0)
                continue;

            // bank is busy
            read_channel = dram_get_channel(read_addr);
            read_rank = dram_get_rank(read_addr);
            read_bank = dram_get_bank(read_addr);
            if (bank_hot.entries[read_channel][read_rank][read_bank].working) {
                DRAM_FULL_DP("SCH_MISS_SKIP_BANK_BUSY", current_core_cycle[queue->entry[i].cpu],
                             queue->entry[i].cpu, read_channel, read_rank, read_bank,
                             queue->entry[i], "BANK_WORKING_1_MISS_SCAN");
                continue;
            }

            //read_row = dram_get_row(read_addr);
            //read_column = dram_get_column(read_addr);

            // select the oldest entry
            if ((oldest_index == -1) || PCYCLE_LT(queue->entry[i].event_cycle, oldest_cycle)) {
                DRAM_FULL_DP("SCH_MISS_CAND", current_core_cycle[queue->entry[i].cpu],
                             queue->entry[i].cpu, read_channel, read_rank, read_bank,
                             queue->entry[i], "ROW_MISS_CAPTURED");
                oldest_cycle = queue->entry[i].event_cycle;
                oldest_index = i;
            }
        }
    }

    // at this point, the scheduler knows which bank to access and if the request is a row buffer hit or miss
    if (oldest_index != -1) { // scheduler might not find anything if all requests are already scheduled or all banks are busy

        uint64_t LATENCY = 0;
        if (row_buffer_hit)
            LATENCY = tCAS;
        else
            LATENCY = tRP + tRCD + tCAS;

        uint64_t op_addr = queue->entry[oldest_index].address;
        uint32_t op_cpu = queue->entry[oldest_index].cpu,
                 op_channel = dram_get_channel(op_addr),
                 op_rank = dram_get_rank(op_addr),
                 op_bank = dram_get_bank(op_addr),
                 op_row = dram_get_row(op_addr);
        IF_DEBUG_PRINT(
        uint32_t op_column = dram_get_column(op_addr);
        (void)op_column;
        )

        DRAM_FULL_DP("SCH_DISPATCH_PRE", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                     queue->entry[oldest_index],
                     (row_buffer_hit ? "HIT_PATH_tCAS" : "MISS_PATH_tRP_tRCD_tCAS"));

        // this bank is now busy — separate stores (cycle_available is uint64, full width)
        if (bank_hot.entries[op_channel][op_rank][op_bank].working == 0) working_bank_count++; // Opt-A: 0->1 transition only
        bank_hot.entries[op_channel][op_rank][op_bank].working = 1;
        bank_cycle_available[op_channel][op_rank][op_bank] = current_core_cycle[op_cpu] + LATENCY;
        bank_request[op_channel][op_rank][op_bank].working_type = queue->entry[oldest_index].type;

        bank_request[op_channel][op_rank][op_bank].request_index = oldest_index;
        bank_request[op_channel][op_rank][op_bank].row_buffer_hit = row_buffer_hit;
        if (queue->is_WQ) {
            bank_request[op_channel][op_rank][op_bank].is_write = 1;
            bank_request[op_channel][op_rank][op_bank].is_read = 0;
            scheduled_writes[op_channel]++;
        }
        else {
            bank_request[op_channel][op_rank][op_bank].is_write = 0;
            bank_request[op_channel][op_rank][op_bank].is_read = 1;
            scheduled_reads[op_channel]++;
        }

        // update open row
        bank_request[op_channel][op_rank][op_bank].open_row = op_row;

        queue->entry[oldest_index].scheduled = 1;
        queue->entry[oldest_index].event_cycle = PACK_CYCLE(current_core_cycle[op_cpu] + LATENCY);

        update_schedule_cycle(queue);
        update_process_cycle(queue);

        DRAM_FULL_DP("SCH_DISPATCH_POST", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                     queue->entry[oldest_index], "BANK_SET_WORKING_AND_CYC_AV");
    } else {
        // schedule() entered but no candidate found in either hit or miss scan
        DRAM_CH_DP("SCH_NO_CAND", current_core_cycle[DEBUG_CPU], 0,
                   (queue->is_WQ ? "WQ_SCAN_EMPTY" : "RQ_SCAN_EMPTY"));
    }
}

void MEMORY_CONTROLLER::process(PACKET_QUEUE *queue)
{
    uint32_t request_index = queue->next_process_index;

    SANITY_DRAM_REQUEST_IDX_BOUND(queue, request_index);

    uint8_t  op_type = queue->entry[request_index].type;
    uint64_t op_addr = queue->entry[request_index].address;
    uint32_t op_cpu = queue->entry[request_index].cpu,
             op_channel = dram_get_channel(op_addr),
             op_rank = dram_get_rank(op_addr),
             op_bank = dram_get_bank(op_addr);
    IF_DEBUG_PRINT(
    uint32_t op_row = dram_get_row(op_addr),
             op_column = dram_get_column(op_addr);
    (void)op_row; (void)op_column;
    )

    DRAM_FULL_DP("PROC_TOP", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                 queue->entry[request_index],
                 (queue->is_WQ ? "PROC_ENTER_WQ" : "PROC_ENTER_RQ"));

    // update_process_cycle picks the earliest-event-cycle scheduled entry globally,
    // but the bank may be servicing a different entry at the same channel/rank/bank.
    // If mismatch: this entry cannot proceed now. Re-scan for a processable entry.
    if (bank_request[op_channel][op_rank][op_bank].request_index != (int)request_index) {
        DRAM_FULL_DP("PROC_MISMATCH_PRE", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                     queue->entry[request_index], "REQ_IDX_NEQ_BANK_OWNER");

        // Enumerate bank slots on this channel — each holds at most one request_index.
        uint64_t alt_cycle = CYC_PACKED_MAX;
        uint32_t alt_index = queue->SIZE;
        for (uint32_t rk = 0; rk < DRAM_RANKS; rk++) {
            for (uint32_t bk = 0; bk < DRAM_BANKS; bk++) {
                int ri = bank_request[op_channel][rk][bk].request_index;
                if (ri < 0 || (uint32_t)ri >= queue->SIZE) continue;
                if (!queue->entry[ri].scheduled) continue;
                DRAM_FULL_DP("PROC_MISMATCH_ALT_CAND", current_core_cycle[op_cpu], op_cpu, op_channel, rk, bk,
                             queue->entry[ri], "ALT_CANDIDATE_FOUND");
                if (PCYCLE_LT(queue->entry[ri].event_cycle, alt_cycle)) {
                    alt_cycle = queue->entry[ri].event_cycle;
                    alt_index = (uint32_t)ri;
                }
            }
        }
        if (alt_index < queue->SIZE) {
            queue->next_process_cycle = alt_cycle;
            queue->next_process_index = alt_index;
            DRAM_CH_DP("PROC_MISMATCH_POST", current_core_cycle[op_cpu], op_channel, "ALT_SET_NPC_NPI_UPDATED");
        } else {
            // No channel bank-owner found. Do NOT poison next_process with MAX —
            // that fails the operate() guard and silently deadlocks the queue.
            // Redrive to the global next-min scheduled entry instead.
            update_process_cycle(queue);
            DRAM_CH_DP("PROC_MISMATCH_POST", current_core_cycle[op_cpu], op_channel, "NO_ALT_GLOBAL_REDRIVE");
        }
        return;
    }

    // paid all DRAM access latency, data is ready to be processed
    if (CYC_LE(bank_cycle_available[op_channel][op_rank][op_bank], CYC(current_core_cycle[op_cpu]))) {

        // check if data bus is available
        if (CYC_LE(dbus_cycle_available[op_channel], CYC(current_core_cycle[op_cpu]))) {

            if (queue->is_WQ) {
                DRAM_FULL_DP("PROC_WQ_RETURN", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                             queue->entry[request_index], "WQ_WRITE_COMPLETE_DBUS_UPDATE");

                // update data bus cycle time
                dbus_cycle_available[op_channel] = CYC(current_core_cycle[op_cpu] + DRAM_DBUS_RETURN_TIME);

                if (bank_request[op_channel][op_rank][op_bank].row_buffer_hit)
                    queue->ROW_BUFFER_HIT++;
                else
                    queue->ROW_BUFFER_MISS++;

                // this bank is ready for another DRAM request
                bank_request[op_channel][op_rank][op_bank].request_index = -1;
                bank_request[op_channel][op_rank][op_bank].row_buffer_hit = 0;
                if (bank_hot.entries[op_channel][op_rank][op_bank].working) working_bank_count--; // Opt-A: 1->0 transition only
                bank_hot.entries[op_channel][op_rank][op_bank].working = 0; // C2
                bank_request[op_channel][op_rank][op_bank].is_write = 0;
                bank_request[op_channel][op_rank][op_bank].is_read = 0;

                scheduled_writes[op_channel]--;
                DRAM_CH_DP("PROC_WQ_BANK_RESET", current_core_cycle[op_cpu], op_channel, "WQ_BANK_WORKING_0_SR_DEC");
            } else {
                // update data bus cycle time
                dbus_cycle_available[op_channel] = CYC(current_core_cycle[op_cpu] + DRAM_DBUS_RETURN_TIME);
                queue->entry[request_index].event_cycle = PACK_CYCLE(dbus_cycle_available[op_channel]);

                // DP ( if (warmup_complete[op_cpu]) {
                // cout << "[" << queue->NAME << "] " <<  __func__ << " return data" << hex;
                // cout << " address: " << queue->entry[request_index].address << " full_addr: " << queue->entry[request_index].full_addr << dec;
                // cout << " occupancy: " << queue->occupancy << " channel: " << op_channel << " rank: " << op_rank << " bank: " << op_bank;
                // cout << " row: " << op_row << " column: " << op_column;
                // cout << " current_cycle: " << current_core_cycle[op_cpu] << " event_cycle: " << queue->entry[request_index].event_cycle << endl; });

                // send data back to the core cache hierarchy
                if (warmup_complete[op_cpu])
                    sim_read_access[op_cpu]++;
#ifdef USE_HERMES
                if (queue->entry[request_index].fill_level >= FILL_DDRP) {
                    assert(queue->entry[request_index].type == PREFETCH);
                    DRAM_FULL_DP("PROC_RQ_DDRP_PATH", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                                 queue->entry[request_index],
                                 (knob::dram_cntlr_enable_ddrp_buffer ? "DDRP_INSERT_BUF" : "DDRP_NO_BUF_NO_LLC_RETURN"));
                    DP( if (DP_GATE_WW(current_core_cycle[op_cpu], op_cpu, queue->entry[request_index].address, queue->entry[request_index].instr_id)) {
                        cout << "[DRAM_DDRP_PROCESS] addr: 0x" << hex << queue->entry[request_index].address << dec
                             << " fill_level: " << (int)queue->entry[request_index].fill_level
                             << " type: " << (int)queue->entry[request_index].type
                             << " cpu: " << (int)op_cpu
                             << " cy: " << current_core_cycle[op_cpu] << endl;
                    });
                    if (knob::dram_cntlr_enable_ddrp_buffer) {
                        insert_ddrp_buffer(queue->entry[request_index].address);
                    }
                } else
#endif
#ifdef BYPASS_LLC_LOGIC
                if (queue->entry[request_index].llc_bypassed) {
                    // LLC was bypassed: skip LLC->return_data(), call LLC's upper (L2C) directly
                    DRAM_FULL_DP("PROC_RQ_LLCBYP_PATH_PRE", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                                 queue->entry[request_index], "LLC_BYP_DIRECT_L2C_RETURN");
                    DP( if (DP_GATE_WW(current_core_cycle[op_cpu], op_cpu, 0, 0)) {
                    cout << "[DRAM_G1] [cpu " << (int)op_cpu << "] process() llc_bypassed RETURN"
                         << " addr: " << hex << queue->entry[request_index].address << dec
                         << " instr: " << queue->entry[request_index].instr_id
                         << " type: " << (int)queue->entry[request_index].type
                         << " ByP " << (int)queue->entry[request_index].l1_bypassed << " " << (int)queue->entry[request_index].l2_bypassed << " " << (int)queue->entry[request_index].llc_bypassed
                         << " cy: " << current_core_cycle[op_cpu] << endl; });
                    upper_level_dcache[op_cpu]->upper_level_dcache[op_cpu]->return_data(&queue->entry[request_index]);
#ifdef BYP_DERFILL_ACTIVE
                    // DERIVATIVE FILL: DRAM returned, LLC was bypassed. Fill LLC directly.
                    {
                        CACHE* llc = (CACHE*)upper_level_dcache[op_cpu];
                        PACKET df_pkt = queue->entry[request_index];
                        df_pkt.llc_bypassed = 0;
                        df_pkt.l2_bypassed = 0;
                        df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                        {
                            uint32_t df_set = llc->get_set(df_pkt.address);
                            uint32_t df_way = llc->llc_find_victim(op_cpu, df_pkt.instr_id, df_set, llc->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                            if (llc->block[df_set][df_way].valid && llc->block[df_set][df_way].dirty) {
                                if (llc->lower_level->get_occupancy(2, llc->block[df_set][df_way].address) < llc->lower_level->get_size(2, llc->block[df_set][df_way].address)) {
                                    PACKET wb_pkt;
                                    wb_pkt.fill_level = FILL_DRAM;
                                    wb_pkt.cpu = op_cpu;
                                    wb_pkt.address = llc->block[df_set][df_way].address;
                                    wb_pkt.full_addr = llc->block[df_set][df_way].full_addr;
                                    wb_pkt.data = llc->block[df_set][df_way].data;
                                    wb_pkt.instr_id = df_pkt.instr_id;
                                    wb_pkt.ip = 0;
                                    wb_pkt.type = WRITEBACK;
                                    wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[op_cpu]);
                                    llc->lower_level->add_wq(&wb_pkt);
                                }
                            }
                            llc->fill_cache(df_set, df_way, &df_pkt);
                            llc->llc_update_replacement_state(op_cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, llc->block[df_set][df_way].full_addr, df_pkt.type, 0);
                        }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                        // Inject completed MSHR at LLC — fills in LLC_LATENCY cycles
                        {
                            df_pkt.returned = COMPLETED;
                            df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[op_cpu] + llc->LATENCY);
                            df_pkt.fill_level = FILL_LLC;
                            df_pkt.type = PREFETCH;
                            int mshr_idx = llc->check_mshr(&df_pkt);
                            if (mshr_idx == -1
                                && llc->MSHR.occupancy < llc->MSHR.SIZE
                                && !(llc->MSHR.occupancy && (llc->MSHR.head == llc->MSHR.tail))) {
                                llc->MSHR.add_queue(&df_pkt);
                                llc->MSHR.num_returned++;
                                llc->update_fill_cycle();
                            }
                        }
#endif
                    }
#endif
                } else
#endif
                {
                DRAM_FULL_DP("PROC_RQ_NORMAL_PRE", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                             queue->entry[request_index], "NORMAL_LLC_RETURN");
                DP( if (DP_GATE_WW(current_core_cycle[op_cpu], op_cpu, 0, 0)) {
                cout << "[DRAM_OUT] [cpu " << (int)op_cpu << "] process() NORMAL return to LLC"
                     << " addr: " << hex << queue->entry[request_index].address << dec
                     << " instr: " << queue->entry[request_index].instr_id
                     << " type: " << (int)queue->entry[request_index].type
                     << " ByP " << (int)queue->entry[request_index].l1_bypassed << " " << (int)queue->entry[request_index].l2_bypassed << " " << (int)queue->entry[request_index].llc_bypassed
                     << " cy: " << current_core_cycle[op_cpu] << endl; });
                upper_level_dcache[op_cpu]->return_data(&queue->entry[request_index]);
                DRAM_FULL_DP("PROC_RQ_NORMAL_POST", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                             queue->entry[request_index], "POST_LLC_RETURN_DATA");
                }

                if (bank_request[op_channel][op_rank][op_bank].row_buffer_hit)
                    queue->ROW_BUFFER_HIT++;
                else
                    queue->ROW_BUFFER_MISS++;

                // this bank is ready for another DRAM request
                bank_request[op_channel][op_rank][op_bank].request_index = -1;
                bank_request[op_channel][op_rank][op_bank].row_buffer_hit = 0;
                if (bank_hot.entries[op_channel][op_rank][op_bank].working) working_bank_count--; // Opt-A: 1->0 transition only
                bank_hot.entries[op_channel][op_rank][op_bank].working = 0; // C2
                bank_request[op_channel][op_rank][op_bank].is_write = 0;
                bank_request[op_channel][op_rank][op_bank].is_read = 0;

                scheduled_reads[op_channel]--;
                DRAM_CH_DP("PROC_RQ_BANK_RESET", current_core_cycle[op_cpu], op_channel, "RQ_BANK_WORKING_0_SR_DEC");
            }

            // remove the oldest entry
            if (queue->is_WQ) rw_occ[op_cpu].wq--; else rw_occ[op_cpu].rq--;
            queue->remove_queue(&queue->entry[request_index]);
            update_process_cycle(queue);
            DRAM_CH_DP("PROC_REMOVE_DONE", current_core_cycle[op_cpu], op_channel,
                       (queue->is_WQ ? "WQ_ENTRY_REMOVED" : "RQ_ENTRY_REMOVED"));
        }
        else { // data bus is busy, the available bank cycle time is fast-forwarded for faster simulation
            DRAM_FULL_DP("PROC_DBUS_BUSY", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                         queue->entry[request_index], "DBUS_AV_GT_NOW_BANK_CYAV_FF");

#if 0
            // TODO: what if we can service prefetching request without dbus congestion?
            // can we have more timely prefetches and improve performance?
            if ((op_type == PREFETCH) || (op_type == LOAD)) {
                // just magically return prefetch request (no need to update data bus cycle time)
                
                dbus_cycle_available[op_channel] = current_core_cycle[op_cpu] + DRAM_DBUS_RETURN_TIME;
                queue->entry[request_index].event_cycle = PACK_CYCLE(dbus_cycle_available[op_channel]); 

                // DP ( if (warmup_complete[op_cpu]) {
                // cout << "[" << queue->NAME << "] " <<  __func__ << " return data" << hex;
                // cout << " address: " << queue->entry[request_index].address << " full_addr: " << queue->entry[request_index].full_addr << dec;
                // cout << " occupancy: " << queue->occupancy << " channel: " << op_channel << " rank: " << op_rank << " bank: " << op_bank;
                // cout << " row: " << op_row << " column: " << op_column;
                // cout << " current_cycle: " << current_core_cycle[op_cpu] << " event_cycle: " << queue->entry[request_index].event_cycle << endl; });
                

                // send data back to the core cache hierarchy
                upper_level_dcache[op_cpu]->return_data(&queue->entry[request_index]);

                if (bank_request[op_channel][op_rank][op_bank].row_buffer_hit)
                    queue->ROW_BUFFER_HIT++;
                else
                    queue->ROW_BUFFER_MISS++;

                // this bank is ready for another DRAM request
                bank_request[op_channel][op_rank][op_bank].request_index = -1;
                bank_request[op_channel][op_rank][op_bank].row_buffer_hit = 0;
                bank_hot.entries[op_channel][op_rank][op_bank].working = 0; // C2
                bank_request[op_channel][op_rank][op_bank].is_write = 0;
                bank_request[op_channel][op_rank][op_bank].is_read = 0;

                scheduled_reads[op_channel]--;

                // remove the oldest entry
                queue->remove_queue(&queue->entry[request_index]);
                update_process_cycle(queue);

                return;
            }
#endif

            { uint64_t _old = dbus_cycle_congested[op_channel];
              dbus_cycle_congested[op_channel] += CYC_DIFF(dbus_cycle_available[op_channel], CYC(current_core_cycle[op_cpu]));
              dbus_cycle_congested_ovf[op_channel] |= (dbus_cycle_congested[op_channel] < _old); }
            bank_cycle_available[op_channel][op_rank][op_bank] = dbus_cycle_available[op_channel]; // uint64
            dbus_congested[NUM_TYPES][NUM_TYPES]++;
            dbus_congested[NUM_TYPES][op_type]++;
            dbus_congested[bank_request[op_channel][op_rank][op_bank].working_type][NUM_TYPES]++;
            dbus_congested[bank_request[op_channel][op_rank][op_bank].working_type][op_type]++;

            DRAM_FULL_DP("PROC_DBUS_BUSY_POST", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                         queue->entry[request_index], "BANK_CYAV_BUMPED_TO_DBUS_AV");
        }
    } else {
        // Bank is still working — cycle_available > now → wait
        DRAM_FULL_DP("PROC_BANK_BUSY", current_core_cycle[op_cpu], op_cpu, op_channel, op_rank, op_bank,
                     queue->entry[request_index], "BANK_CYAV_GT_NOW_WAIT");
    }
}

int MEMORY_CONTROLLER::add_rq(PACKET *packet)
{
// #ifdef BYPASS_DEBUG
//     if (packet->instr_id >= 23230000 || (uint64_t) packet->cycle_enqueued > (uint16_t)12090000)
//         cout << " DRAM PROBLEM PACKET RETURNED!!!!";
// #endif

    {
        uint32_t _ch = dram_get_channel(packet->address);
        uint32_t _rk = dram_get_rank(packet->address);
        uint32_t _bk = dram_get_bank(packet->address);
        DRAM_FULL_DP("ADD_RQ_TOP", current_core_cycle[packet->cpu], packet->cpu, _ch, _rk, _bk,
                     (*packet),
                     (all_warmup_complete < NUM_CPUS ? "PRE_WARMUP_PATH" : "POST_WARMUP_PATH"));
    }

    // simply return read requests with dummy response before the warmup
    if (all_warmup_complete < NUM_CPUS) {
        IF_HERMES(
        if (knob::enable_ddrp && packet->fill_level >= FILL_DDRP)
            return -1; // silently drop DDRP during warmup
        )
#ifdef BYPASS_LLC_LOGIC
        if (packet->llc_bypassed) {
                DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
                cout << "[DRAM_G1] [cpu " << (int)packet->cpu << "] WQ-hit llc_bypassed RETURN"
                     << " addr: " << hex << packet->address << dec
                     << " instr: " << packet->instr_id
                     << " type: " << (int)packet->type
                     << " ByP " << (int)packet->l1_bypassed << " " << (int)packet->l2_bypassed << " " << (int)packet->llc_bypassed
                     << " cy: " << current_core_cycle[packet->cpu] << endl; });
                upper_level_dcache[packet->cpu]->upper_level_dcache[packet->cpu]->return_data(packet);
#ifdef BYP_DERFILL_ACTIVE
                {
                    CACHE* llc = (CACHE*)upper_level_dcache[packet->cpu];
                    PACKET df_pkt = *packet;
                    df_pkt.llc_bypassed = 0;
                    df_pkt.l2_bypassed = 0;
                    df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                    {
                        uint32_t df_set = llc->get_set(df_pkt.address);
                        uint32_t df_way = llc->llc_find_victim(packet->cpu, df_pkt.instr_id, df_set, llc->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                        if (llc->block[df_set][df_way].valid && llc->block[df_set][df_way].dirty) {
                            if (llc->lower_level->get_occupancy(2, llc->block[df_set][df_way].address) < llc->lower_level->get_size(2, llc->block[df_set][df_way].address)) {
                                PACKET wb_pkt;
                                wb_pkt.fill_level = FILL_DRAM;
                                wb_pkt.cpu = packet->cpu;
                                wb_pkt.address = llc->block[df_set][df_way].address;
                                wb_pkt.full_addr = llc->block[df_set][df_way].full_addr;
                                wb_pkt.data = llc->block[df_set][df_way].data;
                                wb_pkt.instr_id = df_pkt.instr_id;
                                wb_pkt.ip = 0;
                                wb_pkt.type = WRITEBACK;
                                wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu]);
                                llc->lower_level->add_wq(&wb_pkt);
                            }
                        }
                        llc->fill_cache(df_set, df_way, &df_pkt);
                        llc->llc_update_replacement_state(packet->cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, llc->block[df_set][df_way].full_addr, df_pkt.type, 0);
                    }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                    {
                        df_pkt.returned = COMPLETED;
                        df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + llc->LATENCY);
                        df_pkt.fill_level = FILL_LLC;
                        df_pkt.type = PREFETCH;
                        int mshr_idx = llc->check_mshr(&df_pkt);
                        if (mshr_idx == -1
                            && llc->MSHR.occupancy < llc->MSHR.SIZE
                            && !(llc->MSHR.occupancy && (llc->MSHR.head == llc->MSHR.tail))) {
                            llc->MSHR.add_queue(&df_pkt);
                            llc->MSHR.num_returned++;
                            llc->update_fill_cycle();
                        }
                    }
#endif
                }
#endif
            } else
#endif
            {
                if (packet->instruction)
                    upper_level_icache[packet->cpu]->return_data(packet);
                else
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }

        return -1;
    }

    // check for the latest wirtebacks in the write queue
    uint32_t channel = dram_get_channel(packet->address);
    int wq_index = check_dram_queue(&WQ[channel], packet);

    if (wq_index != -1) {

        // no need to check fill level
        //if (packet->fill_level < fill_level) {

            packet->data = WQ[channel].entry[wq_index].data;
            IF_HERMES(
            if (knob::enable_ddrp && packet->fill_level >= FILL_DDRP) {
                if (knob::dram_cntlr_enable_ddrp_buffer)
                    insert_ddrp_buffer(packet->address);
                return 1; // WQ hit for DDRP — don't return data up
            }
            )
#ifdef BYPASS_LLC_LOGIC
            if (packet->llc_bypassed) {
                    upper_level_dcache[packet->cpu]->upper_level_dcache[packet->cpu]->return_data(packet);
#ifdef BYP_DERFILL_ACTIVE
                    {
                        CACHE* llc = (CACHE*)upper_level_dcache[packet->cpu];
                        PACKET df_pkt = *packet;
                        df_pkt.llc_bypassed = 0;
                        df_pkt.l2_bypassed = 0;
                        df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                        {
                            uint32_t df_set = llc->get_set(df_pkt.address);
                            uint32_t df_way = llc->llc_find_victim(packet->cpu, df_pkt.instr_id, df_set, llc->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                            if (llc->block[df_set][df_way].valid && llc->block[df_set][df_way].dirty) {
                                if (llc->lower_level->get_occupancy(2, llc->block[df_set][df_way].address) < llc->lower_level->get_size(2, llc->block[df_set][df_way].address)) {
                                    PACKET wb_pkt;
                                    wb_pkt.fill_level = FILL_DRAM;
                                    wb_pkt.cpu = packet->cpu;
                                    wb_pkt.address = llc->block[df_set][df_way].address;
                                    wb_pkt.full_addr = llc->block[df_set][df_way].full_addr;
                                    wb_pkt.data = llc->block[df_set][df_way].data;
                                    wb_pkt.instr_id = df_pkt.instr_id;
                                    wb_pkt.ip = 0;
                                    wb_pkt.type = WRITEBACK;
                                    wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu]);
                                    llc->lower_level->add_wq(&wb_pkt);
                                }
                            }
                            llc->fill_cache(df_set, df_way, &df_pkt);
                            llc->llc_update_replacement_state(packet->cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, llc->block[df_set][df_way].full_addr, df_pkt.type, 0);
                        } 
#elif defined(BYP_DERFILL_SEQUENTIAL)
                        {
                            df_pkt.returned = COMPLETED;
                            df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + llc->LATENCY);
                            df_pkt.fill_level = FILL_LLC;
                            df_pkt.type = PREFETCH;
                            int mshr_idx = llc->check_mshr(&df_pkt);
                            if (mshr_idx == -1
                                && llc->MSHR.occupancy < llc->MSHR.SIZE
                                && !(llc->MSHR.occupancy && (llc->MSHR.head == llc->MSHR.tail))) {
                                llc->MSHR.add_queue(&df_pkt);
                                llc->MSHR.num_returned++;
                                llc->update_fill_cycle();
                            }
                        }
#endif
                    }
#endif
            } else
#endif
            {
                if (packet->instruction)
                    upper_level_icache[packet->cpu]->return_data(packet);
                else
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }

        // DP ( if (packet->cpu) {
        // cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " found recent writebacks";
        // cout.flush();
        // cout << hex << " read: " << packet->address << " writeback: " << WQ[channel].entry[wq_index].address << dec << endl; 
        // });

        ACCESS[1]++;
        HIT[1]++;

        WQ[channel].FORWARD++;
        RQ[channel].ACCESS++;

        // assert(0);

        return -1;
    }

    // check for duplicates in the read queue
    int index = check_dram_queue(&RQ[channel], packet);
    if (index != -1) {
        IF_HERMES(
        // [HERMES_CONFLICT_FIX] Drop incoming HMP probe when existing entry is a real demand.
        // HMP is redundant (demand already going to DRAM). Prevents fall-through into the
        // bypass-flag reconcile block below — that would clear existing demand's bypass flags
        // (HMP carries flags=0) and orphan upstream MSHRs → deadlock.
        if (packet->fill_level >= FILL_DDRP && RQ[channel].entry[index].fill_level < FILL_DDRP) {
            DRAM_FULL_DP("ADD_RQ_HMP_DROP", current_core_cycle[packet->cpu], packet->cpu,
                         channel, dram_get_rank(packet->address), dram_get_bank(packet->address),
                         (*packet), "INCOMING_HMP_DROP_INTO_DEMAND");
            DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
                cout << "[DRAM_RQ_HMP_DROP_EXIST] existing_iid=" << RQ[channel].entry[index].instr_id
                     << " existing_type=" << (int)RQ[channel].entry[index].type
                     << " existing_fill=" << (int)RQ[channel].entry[index].fill_level
                     << " existing_ByP=" << (int)RQ[channel].entry[index].l1_bypassed
                     << (int)RQ[channel].entry[index].l2_bypassed
                     << (int)RQ[channel].entry[index].llc_bypassed
                     << " sched=" << (int)RQ[channel].entry[index].scheduled
                     << " ev_cy=" << RQ[channel].entry[index].event_cycle << endl; });
            return index;
        }
        if (RQ[channel].entry[index].fill_level >= FILL_DDRP && packet->fill_level < FILL_DDRP) {
            DRAM_FULL_DP("ADD_RQ_DDRP_PROMOTE_PRE", current_core_cycle[packet->cpu], packet->cpu,
                         channel, dram_get_rank(packet->address), dram_get_bank(packet->address),
                         RQ[channel].entry[index], "EXISTING_DDRP_GETS_REPLACED_BY_DEMAND");
            // std::cout << "\033[1;31m\n** RARE UNHANDLED CASE line=" << __LINE__
            //           << " func=" << __func__
            //           << " cache=[DRAM]"
            //           << " ISSUE: DDRP_TO_DEMAND_PROMOTE"
            //           << " **\033[0m\n" << std::flush;
            uint8_t tmp_scheduled = RQ[channel].entry[index].scheduled;
            uint64_t tmp_event_cycle = RQ[channel].entry[index].event_cycle;
            uint64_t tmp_cycle_enqueued = RQ[channel].entry[index].cycle_enqueued;
            RQ[channel].entry[index] = *packet;
            RQ[channel].entry[index].scheduled = tmp_scheduled;
            RQ[channel].entry[index].event_cycle = tmp_event_cycle;
            RQ[channel].entry[index].cycle_enqueued = tmp_cycle_enqueued;
            DRAM_FULL_DP("ADD_RQ_DDRP_PROMOTE_POST", current_core_cycle[packet->cpu], packet->cpu,
                         channel, dram_get_rank(packet->address), dram_get_bank(packet->address),
                         RQ[channel].entry[index], "DEMAND_OVERWROTE_DDRP_SCHED_EVCY_PRESERVED");
        }
        )
#ifdef DO_LVL_BYPASS_ON_FULL_MSHR // ARTIFACT OF PAST???
        cerr << "AM I ARTIFACT ??? "
        RQ[channel].entry[index].bypassed_levels &= packet->bypassed_levels;
#endif
        IF_BYP_LLC(
        // I3-DRAM: non-bypassed PF merges into bypassed demand → clear bypass
        // so DRAM returns via normal LLC path → LLC handle_fill → MSHR cleaned
        if (RQ[channel].entry[index].llc_bypassed && !packet->llc_bypassed) {
            DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
            cout << "[DRAM_RQ] [cpu " << (int)packet->cpu << "] I3-DRAM merge CLEAR llc_bypassed"
                 << " addr: " << hex << packet->address << dec
                 << " existing instr: " << RQ[channel].entry[index].instr_id
                 << " incoming instr: " << packet->instr_id
                 << " existing type: " << (int)RQ[channel].entry[index].type
                 << " incoming type: " << (int)packet->type
                 << " ByP " << (int)RQ[channel].entry[index].l1_bypassed << " " << (int)RQ[channel].entry[index].l2_bypassed << " " << (int)RQ[channel].entry[index].llc_bypassed
                 << " -> 0"
                 << " fill: " << (int)RQ[channel].entry[index].fill_level << " -> " << (int)packet->fill_level
                 << " cy: " << current_core_cycle[packet->cpu] << endl; });
            RQ[channel].entry[index].llc_bypassed = 0;
            RQ[channel].entry[index].fill_level = (packet->type == PREFETCH) ? RQ[channel].entry[index].fill_level : packet->fill_level;
        }
        else if (!RQ[channel].entry[index].llc_bypassed && packet->llc_bypassed) {
            DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
            cout << "[DRAM_RQ] [cpu " << (int)packet->cpu << "] I3-DRAM merge CLEAR incoming llc_bypassed"
                 << " addr: " << hex << packet->address << dec
                 << " existing type: " << (int)RQ[channel].entry[index].type
                 << " incoming type: " << (int)packet->type
                 << " fill: " << (int)RQ[channel].entry[index].fill_level << " min " << (int)packet->fill_level
                 << " cy: " << current_core_cycle[packet->cpu] << endl; });
            RQ[channel].entry[index].fill_level = (packet->type == PREFETCH) ? RQ[channel].entry[index].fill_level : packet->fill_level;
        }
        DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
        cout << "[DRAM_RQ] [cpu " << (int)packet->cpu << "] add_rq MERGED"
             << " addr: " << hex << packet->address << dec
             << " existing instr: " << RQ[channel].entry[index].instr_id
             << " incoming instr: " << packet->instr_id
             << " existing type: " << (int)RQ[channel].entry[index].type << " incoming type: " << (int)packet->type
             << " existing ByP " << (int)RQ[channel].entry[index].l1_bypassed << " " << (int)RQ[channel].entry[index].l2_bypassed << " " << (int)RQ[channel].entry[index].llc_bypassed
             << " incoming ByP " << (int)packet->l1_bypassed << " " << (int)packet->l2_bypassed << " " << (int)packet->llc_bypassed
             << " cy: " << current_core_cycle[packet->cpu] << endl; });
        )
        DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
            cout << "[DRAM_RQ_MERGE] addr: 0x" << hex << packet->address << dec
                 << " existing_addr: 0x" << hex << RQ[channel].entry[index].address << dec
                 << " existing_fill: " << (int)RQ[channel].entry[index].fill_level
                 << " incoming_fill: " << (int)packet->fill_level
                 << " merged_fill: " << (int)RQ[channel].entry[index].fill_level
                 << " existing_type: " << (int)RQ[channel].entry[index].type
                 << " incoming_type: " << (int)packet->type
                 << " cpu: " << (int)packet->cpu
                 << " cy: " << current_core_cycle[packet->cpu] << endl;
        });
        return index; // merged index
    }

    //     // search for the empty index
//     for (index=0; index<DRAM_RQ_SIZE; index++) {
//         if (RQ[channel].entry[index].address == 0) {
            
//             RQ[channel].entry[index]= * packet;
//             RQ[channel].occupancy++;

// #ifdef DEBUG_PRINT
//             uint32_t channel = dram_get_channel(packet->address),
//                      rank = dram_get_rank(packet->address),
//                      bank = dram_get_bank(packet->address),
//                      row = dram_get_row(packet->address),
//                      column = dram_get_column(packet->address); 
// #endif

//             // DP ( if(warmup_complete[packet->cpu]) {
//             // cout << "[" << NAME << "_RQ] " <<  __func__ << " instr_id: " << packet->instr_id << " address: " << hex << packet->address;
//             // cout << " full_addr: " << packet->full_addr << dec << " ch: " << channel;
//             // cout << " rank: " << rank << " bank: " << bank << " row: " << row << " col: " << column;
//             // cout << " occupancy: " << RQ[channel].occupancy << " current: " << current_core_cycle[packet->cpu] << " event: " << packet->event_cycle << endl; });

//             break;
//         }
//     }

    
//     update_schedule_cycle(&RQ[channel]);

//     return -1;
    
    // search for the empty index
    for (index=0; index<DRAM_RQ_SIZE; index++) {
        if (RQ[channel].entry[index].address == 0) {
            
            RQ[channel].entry[index]= * packet;
            RQ[channel].occupancy++;
            rw_occ[packet->cpu].rq++;

            IF_DEBUG_PRINT(
            uint32_t channel = dram_get_channel(packet->address),
                     rank = dram_get_rank(packet->address),
                     bank = dram_get_bank(packet->address),
                     row = dram_get_row(packet->address),
                     column = dram_get_column(packet->address);
            )

            DRAM_FULL_DP("ADD_RQ_NEW_PRE_UPSCHED", current_core_cycle[packet->cpu], packet->cpu,
                         channel, rank, bank, RQ[channel].entry[index], "EMPTY_SLOT_FILLED");
            DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
            cout << "[DRAM_IN] [cpu " << (int)packet->cpu << "] add_rq NEW"
                 << " addr: " << hex << packet->address << dec
                 << " instr: " << packet->instr_id
                 << " type: " << (int)packet->type
                 << " ByP " << (int)packet->l1_bypassed << " " << (int)packet->l2_bypassed << " " << (int)packet->llc_bypassed
                 << " fill: " << (int)packet->fill_level
                 << " ch: " << channel << " rank: " << rank << " bank: " << bank << " row: " << row << " col: " << column
                 << " cy: " << current_core_cycle[packet->cpu] << endl; });

            update_schedule_cycle(&RQ[channel]);
            DRAM_FULL_DP("ADD_RQ_NEW_POST_UPSCHED", current_core_cycle[packet->cpu], packet->cpu,
                         channel, rank, bank, RQ[channel].entry[index], "POST_UPDATE_SCHEDULE_CYCLE");
            return -1;
        }
    }

    // CRITICAL FIX: No empty slot found - queue full
    RQ[channel].FULL++;
    {
        uint32_t _ch = dram_get_channel(packet->address);
        uint32_t _rk = dram_get_rank(packet->address);
        uint32_t _bk = dram_get_bank(packet->address);
        DRAM_FULL_DP("ADD_RQ_FULL_REJECT", current_core_cycle[packet->cpu], packet->cpu, _ch, _rk, _bk,
                     (*packet), "RQ_OCC_EQ_SIZE_REJECT");
    }
    DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
    cout << "[DRAM_RQ] [cpu " << (int)packet->cpu << "] add_rq FULL occ=" << RQ[channel].occupancy
         << " addr: " << hex << packet->address << dec
         << " instr: " << packet->instr_id
         << " type: " << (int)packet->type
         << " fill: " << (int)packet->fill_level
         << " cy: " << current_core_cycle[packet->cpu] << endl; });
    return -2;
}

int MEMORY_CONTROLLER::add_wq(PACKET *packet)
{
    {
        uint32_t _ch = dram_get_channel(packet->address);
        uint32_t _rk = dram_get_rank(packet->address);
        uint32_t _bk = dram_get_bank(packet->address);
        DRAM_FULL_DP("ADD_WQ_TOP", current_core_cycle[packet->cpu], packet->cpu, _ch, _rk, _bk,
                     (*packet),
                     (all_warmup_complete < NUM_CPUS ? "PRE_WARMUP_DROP" : "POST_WARMUP_PATH"));
    }

    // simply drop write requests before the warmup
    if (all_warmup_complete < NUM_CPUS)
        return -1;

    // check for duplicates in the write queue
    uint32_t channel = dram_get_channel(packet->address);
    int index = check_dram_queue(&WQ[channel], packet);

    if (index != -1) {
        DRAM_FULL_DP("ADD_WQ_MERGED", current_core_cycle[packet->cpu], packet->cpu,
                     channel, dram_get_rank(packet->address), dram_get_bank(packet->address),
                     WQ[channel].entry[index], "MERGED_INTO_EXISTING_WQ_ENTRY");
        return index; // merged index
    }

//     // search for the empty index
//     for (index=0; index<DRAM_WQ_SIZE; index++) {
//         if (WQ[channel].entry[index].address == 0) {
            
//             WQ[channel].entry[index]= * packet;
//             WQ[channel].occupancy++;

// #ifdef DEBUG_PRINT
//             uint32_t channel = dram_get_channel(packet->address),
//                      rank = dram_get_rank(packet->address),
//                      bank = dram_get_bank(packet->address),
//                      row = dram_get_row(packet->address),
//                      column = dram_get_column(packet->address); 
// #endif

//             // DP ( if(warmup_complete[packet->cpu]) {
//             // cout << "[" << NAME << "_WQ] " <<  __func__ << " instr_id: " << packet->instr_id << " address: " << hex << packet->address;
//             // cout << " full_addr: " << packet->full_addr << dec << " ch: " << channel;
//             // cout << " rank: " << rank << " bank: " << bank << " row: " << row << " col: " << column;
//             // cout << " occupancy: " << WQ[channel].occupancy << " current: " << current_core_cycle[packet->cpu] << " event: " << packet->event_cycle << endl; });

//             break;
//         }
//     }

//     update_schedule_cycle(&WQ[channel]);

//     return -1;

    // search for the empty index
    for (index=0; index<DRAM_WQ_SIZE; index++) {
        if (WQ[channel].entry[index].address == 0) {

            WQ[channel].entry[index]= * packet;
            WQ[channel].occupancy++;
            rw_occ[packet->cpu].wq++;

            IF_DEBUG_PRINT(
            uint32_t channel = dram_get_channel(packet->address),
                     rank = dram_get_rank(packet->address),
                     bank = dram_get_bank(packet->address),
                     row = dram_get_row(packet->address),
                     column = dram_get_column(packet->address);
            (void)row; (void)column;
            )

            DRAM_FULL_DP("ADD_WQ_NEW_PRE_UPSCHED", current_core_cycle[packet->cpu], packet->cpu,
                         channel, rank, bank, WQ[channel].entry[index], "EMPTY_WQ_SLOT_FILLED");
            update_schedule_cycle(&WQ[channel]);
            DRAM_FULL_DP("ADD_WQ_NEW_POST_UPSCHED", current_core_cycle[packet->cpu], packet->cpu,
                         channel, rank, bank, WQ[channel].entry[index], "POST_UPDATE_SCHEDULE_CYCLE");
            return -1;
        }
    }

    // CRITICAL FIX: No empty slot found - queue full
    WQ[channel].FULL++;
    {
        uint32_t _ch = dram_get_channel(packet->address);
        uint32_t _rk = dram_get_rank(packet->address);
        uint32_t _bk = dram_get_bank(packet->address);
        DRAM_FULL_DP("ADD_WQ_FULL_REJECT", current_core_cycle[packet->cpu], packet->cpu, _ch, _rk, _bk,
                     (*packet), "WQ_OCC_EQ_SIZE_REJECT");
    }
    return -2;
}

int MEMORY_CONTROLLER::add_pq(PACKET *packet)
{
    return -1;
}

void MEMORY_CONTROLLER::return_data(PACKET *packet)
{

}

void MEMORY_CONTROLLER::update_schedule_cycle(PACKET_QUEUE *queue)
{
    // update next_schedule_cycle
    uint64_t min_cycle = CYC_PACKED_MAX;
    uint32_t min_index = queue->SIZE;
    for (uint32_t i=0; i<queue->SIZE; i++) {
        if (queue->entry[i].address && (queue->entry[i].scheduled == 0) && ((min_index >= queue->SIZE) || PCYCLE_LT(queue->entry[i].event_cycle, min_cycle))) {
            min_cycle = queue->entry[i].event_cycle;
            min_index = i;
        }
    }

    queue->next_schedule_cycle = min_cycle;
    queue->next_schedule_index = min_index;
    if (min_index < queue->SIZE) {
        uint32_t _ch = dram_get_channel(queue->entry[min_index].address);
        uint32_t _rk = dram_get_rank(queue->entry[min_index].address);
        uint32_t _bk = dram_get_bank(queue->entry[min_index].address);
        DRAM_FULL_DP("USCH_RESULT", current_core_cycle[queue->entry[min_index].cpu],
                     queue->entry[min_index].cpu, _ch, _rk, _bk, queue->entry[min_index],
                     (queue->is_WQ ? "WQ_NEW_MIN_SET" : "RQ_NEW_MIN_SET"));
    } else {
        DRAM_CH_DP("USCH_RESULT_NONE", current_core_cycle[DEBUG_CPU], 0,
                   (queue->is_WQ ? "WQ_NO_PENDING_SCH" : "RQ_NO_PENDING_SCH"));
    }
}

void MEMORY_CONTROLLER::update_process_cycle(PACKET_QUEUE *queue)
{
    // update next_process_cycle
    uint64_t min_cycle = CYC_PACKED_MAX;
    uint32_t min_index = queue->SIZE;
    for (uint32_t i=0; i<queue->SIZE; i++) {
        if (queue->entry[i].scheduled && ((min_index >= queue->SIZE) || PCYCLE_LT(queue->entry[i].event_cycle, min_cycle))) {
            min_cycle = queue->entry[i].event_cycle;
            min_index = i;
        }
    }

    queue->next_process_cycle = min_cycle;
    queue->next_process_index = min_index;
    if (min_index < queue->SIZE) {
        uint32_t _ch = dram_get_channel(queue->entry[min_index].address);
        uint32_t _rk = dram_get_rank(queue->entry[min_index].address);
        uint32_t _bk = dram_get_bank(queue->entry[min_index].address);
        DRAM_FULL_DP("UPROC_RESULT", current_core_cycle[queue->entry[min_index].cpu],
                     queue->entry[min_index].cpu, _ch, _rk, _bk, queue->entry[min_index],
                     (queue->is_WQ ? "WQ_NEW_PROC_MIN_SET" : "RQ_NEW_PROC_MIN_SET"));
    } else {
        DRAM_CH_DP("UPROC_RESULT_NONE", current_core_cycle[DEBUG_CPU], 0,
                   (queue->is_WQ ? "WQ_NO_PENDING_PROC" : "RQ_NO_PENDING_PROC"));
    }
}

int MEMORY_CONTROLLER::check_dram_queue(PACKET_QUEUE *queue, PACKET *packet)
{
    // search write queue
    for (uint32_t index=0; index<queue->SIZE; index++) {
#ifdef BYPASS_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS
        if (queue->entry[index].address == packet->address && (queue->entry[index].bypassed_levels == packet->bypassed_levels)) {
#else
        if (queue->entry[index].address == packet->address) {
#endif
            DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
                cout << "[CHECK_DRAM_Q_HIT] queue=" << queue->NAME << " idx=" << index
                     << " incoming_iid=" << packet->instr_id << " existing_iid=" << queue->entry[index].instr_id
                     << " incoming_type=" << (int)packet->type << " existing_type=" << (int)queue->entry[index].type
                     << " incoming_fill=" << (int)packet->fill_level << " existing_fill=" << (int)queue->entry[index].fill_level
                     << " existing_sched=" << (int)queue->entry[index].scheduled
                     << " existing_ev_cy=" << queue->entry[index].event_cycle
                     << " addr=0x" << hex << packet->address << dec
                     << " cy=" << current_core_cycle[packet->cpu] << endl; });
            return index;
        }
    }

    DP ( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id)) {
    cout << "[" << queue->NAME << "] " << __func__ << " new address: " << hex << packet->address;
    cout << " full_addr: " << packet->full_addr << dec << endl; });

    return -1;
}

// constexpr uint32_t CACHE::get_set(const uint64_t address) {
//     return (uint32_t) (address & ((1 << lg2(NUM_SET)) - 1));
// }

 
constexpr uint32_t MEMORY_CONTROLLER::dram_get_channel(uint64_t address) const
{
    if (LOG2_DRAM_CHANNELS == 0)
        return 0;

    int shift = 0;

    return (uint32_t) (address >> shift) & (DRAM_CHANNELS - 1);
}

constexpr uint32_t MEMORY_CONTROLLER::dram_get_bank(uint64_t address) const
{
    if (LOG2_DRAM_BANKS == 0)
        return 0;

    int shift = LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_BANKS - 1);
}

constexpr uint32_t MEMORY_CONTROLLER::dram_get_column(uint64_t address) const
{
    if (LOG2_DRAM_COLUMNS == 0)
        return 0;

    int shift = LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_COLUMNS - 1);
}

constexpr uint32_t MEMORY_CONTROLLER::dram_get_rank(uint64_t address) const
{
    if (LOG2_DRAM_RANKS == 0)
        return 0;

    int shift = LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_RANKS - 1);
}

constexpr uint32_t MEMORY_CONTROLLER::dram_get_row(uint64_t address) const
{
    if (LOG2_DRAM_ROWS == 0)
        return 0;

    int shift = LOG2_DRAM_RANKS + LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;

    return (uint32_t) (address >> shift) & (DRAM_ROWS - 1);
}

uint32_t MEMORY_CONTROLLER::get_occupancy(uint8_t queue_type, uint64_t address) const
{
    uint32_t channel = dram_get_channel(address);
    if (queue_type == 1)
        return RQ[channel].occupancy;
    else if (queue_type == 2)
        return WQ[channel].occupancy;

    return 0;
}

uint32_t MEMORY_CONTROLLER::get_size(uint8_t queue_type, uint64_t address) const
{
    uint32_t channel = dram_get_channel(address);
    if (queue_type == 1)
        return RQ[channel].SIZE;
    else if (queue_type == 2)
        return WQ[channel].SIZE;

    return 0;
}

void MEMORY_CONTROLLER::increment_WQ_FULL(uint64_t address)
{
    uint32_t channel = dram_get_channel(address);
    WQ[channel].FULL++;
}