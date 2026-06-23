
# CONTENT PAGE
**FORMAT: <CLASS>:<METHOD>:<BLOCK>**
- O3_CPU:initialize_core [method]
- O3_CPU:handle_branch [method]
- O3_CPU:handle_branch:read_trace_loop [block]
- O3_CPU:handle_branch:instr_field_copy [block]
- O3_CPU:handle_branch:sta_update [block]
- O3_CPU:handle_branch:source_reg_mem_copy [block]
- O3_CPU:handle_branch:rob_add_branch_predict [block]
- O3_CPU:add_to_rob [method]
- O3_CPU:add_to_rob:rob_insert [block]
- O3_CPU:check_rob [method]
- O3_CPU:check_rob:empty_guard [block]
- O3_CPU:check_rob:hash_lookup [block]
- O3_CPU:fetch_instruction [method]
- O3_CPU:fetch_instruction:fetch_stall_reset [block]
- O3_CPU:fetch_instruction:tlb_read_loop [block]
- O3_CPU:fetch_instruction:icache_fetch_loop [block]
- O3_CPU:schedule_instruction [method]
- O3_CPU:schedule_instruction:empty_guard [block]
- O3_CPU:schedule_instruction:linear_scan_head_lt_limit [block]
- O3_CPU:schedule_instruction:linear_scan_wrap [block]
- O3_CPU:schedule_instruction:bitset_dispatch [block]
- O3_CPU:do_scheduling [method]
- O3_CPU:do_scheduling:reg_dep_check [block]
- O3_CPU:do_scheduling:mem_inflight [block]
- O3_CPU:do_scheduling:nonmem_rte1_enqueue [block]
- O3_CPU:reg_dependency [method]
- O3_CPU:reg_dependency:head_guard [block]
- O3_CPU:reg_dependency:source_reg_producer_lookup [block]
- O3_CPU:reg_RAW_dependency [method]
- O3_CPU:execute_instruction [method]
- O3_CPU:execute_instruction:rte0_drain [block]
- O3_CPU:execute_instruction:rte1_drain [block]
- O3_CPU:do_execution [method]
- O3_CPU:schedule_memory_instruction [method]
- O3_CPU:schedule_memory_instruction:empty_guard [block]
- O3_CPU:schedule_memory_instruction:scan_lambda [block]
- O3_CPU:schedule_memory_instruction:scan_dispatch [block]
- O3_CPU:schedule_memory_instruction:bitset_dispatch [block]
- O3_CPU:execute_memory_instruction [method]
- O3_CPU:do_memory_scheduling [method]
- O3_CPU:check_and_add_lsq [method]
- O3_CPU:check_and_add_lsq:load_loop [block]
- O3_CPU:check_and_add_lsq:store_loop [block]
- O3_CPU:add_load_queue [method]
- O3_CPU:add_load_queue:lq_alloc [block]
- O3_CPU:add_load_queue:raw_dep_check [block]
- O3_CPU:add_load_queue:store_forward_check [block]
- O3_CPU:add_load_queue:forward_complete [block]
- O3_CPU:add_load_queue:pending_load_register [block]
- O3_CPU:add_load_queue:rtl0_enqueue [block]
- O3_CPU:add_store_queue [method]
- O3_CPU:add_store_queue:sq_insert [block]
- O3_CPU:add_store_queue:sta_advance [block]
- O3_CPU:add_store_queue:rts0_enqueue [block]
- O3_CPU:operate_lsq [method]
- O3_CPU:operate_lsq:rts0_dtlb_loop [block]
- O3_CPU:operate_lsq:rts1_execute_store_loop [block]
- O3_CPU:operate_lsq:rtl0_dtlb_loop [block]
- O3_CPU:operate_lsq:rtl1_execute_load_loop [block]
- O3_CPU:execute_store [method]
- O3_CPU:execute_store:sq_complete [block]
- O3_CPU:execute_store:store_forward_notify [block]
- O3_CPU:execute_store:forward_lq_loop [block]
- O3_CPU:execute_load [method]
- O3_CPU:execute_load:packet_build [block]
- O3_CPU:execute_load:bypass_l1_branch [block]
- O3_CPU:execute_load:no_bypass_path [block]
- O3_CPU:execute_load:rq_result [block]
- O3_CPU:complete_execution [method]
- O3_CPU:complete_execution:guard [block]
- O3_CPU:complete_execution:mark_complete [block]
- O3_CPU:complete_execution:dep_release [block]
- O3_CPU:complete_execution:branch_resume [block]
- O3_CPU:reg_RAW_release [method]
- O3_CPU:reg_RAW_release:dep_vec_iter [block]
- O3_CPU:reg_RAW_release:rte0_enqueue [block]
- O3_CPU:operate_cache [method]
- O3_CPU:update_rob [method]
- O3_CPU:update_rob:itlb_processed [block]
- O3_CPU:update_rob:l1i_processed [block]
- O3_CPU:update_rob:dtlb_processed [block]
- O3_CPU:update_rob:l1d_processed [block]
- O3_CPU:update_rob:l2c_bypass_processed [block]
- O3_CPU:update_rob:complete_execution_scan [block]
- O3_CPU:complete_instr_fetch [method]
- O3_CPU:complete_instr_fetch:tlb_or_cache_update [block]
- O3_CPU:complete_instr_fetch:merged_update [block]
- O3_CPU:complete_data_fetch [method]
- O3_CPU:complete_data_fetch:dtlb_rfo_path [block]
- O3_CPU:complete_data_fetch:dtlb_load_path [block]
- O3_CPU:complete_data_fetch:l1d_rfo_path [block]
- O3_CPU:complete_data_fetch:l1d_load_path [block]
- O3_CPU:handle_merged_translation [method]
- O3_CPU:handle_merged_translation:store_merged [block]
- O3_CPU:handle_merged_translation:load_merged [block]
- O3_CPU:handle_merged_load [method]
- O3_CPU:handle_merged_load:lq_iter [block]
- O3_CPU:release_load_queue [method]
- O3_CPU:retire_rob [method]
- O3_CPU:retire_rob:retire_guard [block]
- O3_CPU:retire_rob:store_count [block]
- O3_CPU:retire_rob:store_wq_submit [block]
- O3_CPU:retire_rob:sq_release [block]
- O3_CPU:retire_rob:rob_release [block]

# O3_CPU

## initialize_core

## handle_branch

    ### read_trace_loop
            `while (continue_reading)` — xz_reader.reopen on eof; xz_reader.read into current_instr; get ROB.tail slot.

    ### instr_field_copy
            Copy ip, is_branch, branch_taken, asid, destination_registers, destination_memory, destination_virtual_address from current_instr; count num_reg_ops, num_mem_ops.

    ### sta_update
            On destination_memory hit: write instr_unique_id to STA[STA_tail], advance STA_tail with wrap.

    ### source_reg_mem_copy
            Copy source_registers, source_memory, source_virtual_address; accumulate num_reg_ops/num_mem_ops; set rob_events.is_mem flag.

    ### rob_add_branch_predict
            If ROB not full: call add_to_rob, increment num_reads; if is_branch: predict_branch, on mismatch set branch_mispredictions, fetch_stall=1, flat_branch_mispredicted; call last_branch_result; stop reading if num_reads >= instrs_to_read_this_cycle or ROB full. Increment instr_unique_id.


## add_to_rob
    Assert previousNotEmpty == 0 (slot must be empty).

    ### rob_insert
            Assign arch_instr to ROB.entry[index]; rob_hash_table.add_rob_idx; set instr_id, rob_index, rob_events init; addr_dependencies/mem_dependencies add_producer; advance ROB.tail, ROB.occupancy.


## check_rob

    ### empty_guard
            Return ROB.SIZE if ROB empty.

    ### hash_lookup
            Return rob_hash_table.get_rob_idx(cpu, instr_id, ROB.SIZE).


## fetch_instruction

    ### fetch_stall_reset
            Clear fetch_stall and fetch_resume_cycle when current_core_cycle >= fetch_resume_cycle.

    ### tlb_read_loop
            For FETCH_WIDTH entries from ROB.last_read+1: skip if ip==0 or already translated; build trace_packet (ITLB TLB access); call ITLB.add_rq; break on -2; advance ROB.last_read.

    ### icache_fetch_loop
            For FETCH_WIDTH entries from ROB.last_fetch+1: skip if not translated/COMPLETED or event_cycle future; build fetch_packet; call L1I.add_rq; break on -2; set fetched=INFLIGHT, rob_memory_count; advance ROB.last_fetch.


## schedule_instruction

    ### empty_guard
            Return if ROB empty.

    ### linear_scan_head_lt_limit
            Tiled scan ROB.head..limit: check COMPLETE_fetch_t, SCHEDULER_SIZE cap, event_cycle; set valid_bits for unscheduled entries; goto done on stop condition.

    ### linear_scan_wrap
            Tiled scan ROB.head..ROB.SIZE (phase1) then 0..limit (phase2) with same checks; goto done on stop.

    ### bitset_dispatch
            Iterate valid_bits words via __builtin_ctzll; call do_scheduling for each set bit.


## do_scheduling

    ### reg_dep_check
            Set reg_ready=1; call reg_dependency.

    ### mem_inflight
            If is_mem: set scheduled=INFLIGHT.

    ### nonmem_rte1_enqueue
            If not mem: set scheduled=COMPLETED, update event_cycle with SCHEDULING_LATENCY; enqueue rob_index into RTE1.


## reg_dependency

    ### head_guard
            Return if rob_index == ROB.head.

    ### source_reg_producer_lookup
            For each source_register not yet reg_RAW_checked: call addr_dependencies.get_latest_producer; if found call reg_RAW_dependency.


## reg_RAW_dependency

    ### reg_raw_dep_record
            reg_dep_tracker.add(prior, current, source_index); set reg_RAW_producer on prior; clear reg_ready on current; set producer_id, num_reg_dependent, reg_RAW_checked.


## execute_instruction

    ### rte0_drain
            While exec_issued < EXEC_WIDTH: dequeue RTE0[RTE0_head] if event_cycle ready; call do_execution; advance head; increment exec_issued; break on empty or ROB_SIZE-1 iterations.

    ### rte1_drain
            While exec_issued < EXEC_WIDTH: same pattern for RTE1 with prefetch hints.


## do_execution

    ### exec_guard_and_mark
            Guard: reg_ready && scheduled==COMPLETED && event_cycle<=current; set executed=INFLIGHT; compute eventCycle with EXEC_LATENCY; increment inflight_reg_executions.


## schedule_memory_instruction

    ### empty_guard
            Return if rob_memory_count==0 or ROB empty.

    ### scan_lambda
            Lambda scan(start,end): skip non-memory entries; stop on incomplete fetch, SCHEDULER_SIZE cap, or future event_cycle; set valid_bits on SCHED_READY_STATE entries.

    ### scan_dispatch
            Compute scan_end and effective_limit; dispatch linear or wrapped scan via scan lambda.

    ### bitset_dispatch
            Iterate valid_bits words via __builtin_ctzll; call do_memory_scheduling for each bit.


## execute_memory_instruction

    ### delegate
            Call operate_lsq(); call operate_cache().


## do_memory_scheduling

    ### lsq_add_and_mark
            Call check_and_add_lsq; if not_available==0: set scheduled=COMPLETED, set executed=INFLIGHT if not already set.


## check_and_add_lsq

    ### load_loop
            For each source_memory: count num_mem_ops; if not source_added and LQ has space, call add_load_queue, increment num_added.

    ### store_loop
            For each destination_memory: count num_mem_ops; if not destination_added, SQ has space, and STA[STA_head] matches instr_id, call add_store_queue, increment num_added.


## add_load_queue

    ### lq_alloc
            free_LQueue.alloc_lq(cpu); populate LQ entry fields from ROB and source_memory; increment LQ.occupancy.

    ### raw_dep_check
            If not ROB.head: mem_dependencies.get_latest_producer for load_addr; if found set is_producer on producer, set LQ.entry.producer_id and translated=INFLIGHT.

    ### store_forward_check
            sq_address_map.get_matching_indices; iterate matching sq entries; match producer_id to find forwarding_index; if instr ordering conflict clear translated/fetched.

    ### forward_complete
            If forwarding_index valid and sq fetched COMPLETED and event_cycle past: compute physical_address from sq; set translated=COMPLETED, fetched=COMPLETED; decrement num_mem_ops; if zero increment inflight_mem_executions; release_load_queue.

    ### pending_load_register
            If virtual_address set and producer_id != UINT64_MAX and not yet forwarded: lq_pending_loads.add_pending_load.

    ### rtl0_enqueue
            If virtual_address set and producer_id == UINT64_MAX: enqueue lq_index into RTL0.


## add_store_queue

    ### sq_insert
            Populate SQ entry fields from ROB and destination_memory; sq_address_map.add_sq_entry; increment SQ.occupancy; advance SQ.tail; set destination_added.

    ### sta_advance
            Clear STA[STA_head] to UINT64_MAX; advance STA_head with wrap.

    ### rts0_enqueue
            Enqueue sq_index into RTS0.


## operate_lsq

    ### rts0_dtlb_loop
            While store_issued < SQ_WIDTH: dequeue RTS0[RTS0_head] if event_cycle ready; build data_packet (RFO, tlb_access); DTLB.add_rq; on -2 break; set SQ.translated=INFLIGHT; advance head; increment store_issued.

    ### rts1_execute_store_loop
            While store_issued < SQ_WIDTH: dequeue RTS1[RTS1_head] if event_cycle ready; call execute_store; advance head; increment store_issued.

    ### rtl0_dtlb_loop
            While load_issued < LQ_WIDTH: dequeue RTL0[RTL0_head] if event_cycle ready; build data_packet (LOAD, tlb_access); DTLB.add_rq; on -2 break; set LQ.translated=INFLIGHT; advance head; increment load_issued.

    ### rtl1_execute_load_loop
            While load_issued < LQ_WIDTH: dequeue RTL1[RTL1_head] if event_cycle ready; call execute_load; on non -2 advance head, increment load_issued.


## execute_store

    ### sq_complete
            Set sq_entry.fetched=COMPLETED, event_cycle=current; decrement rob_entry.num_mem_ops; update rob event_cycle; branchless increment inflight_mem_executions if num_mem_ops==0.

    ### store_forward_notify
            If rob_entry.is_producer: get pending_lqs from lq_pending_loads; copy to lqs_to_process for iteration.

    ### forward_lq_loop
            For each lq_index in lqs_to_process: skip if producer_id mismatch; compute physical_address from sq; set lq translated=COMPLETED, fetched=COMPLETED, event_cycle; decrement fwr_rob.num_mem_ops; increment inflight_mem_executions if zero; remove_pending_load; release_load_queue.


## execute_load

    ### packet_build
            Copy EXEC_LOAD_DATA_TEMPLATE; populate cpu, data_index, lq_index, address, full_addr, instr_id, rob_index, ip, type, asid, event_cycle from LQ entry.

    ### bypass_l1_branch
            `#ifdef BYPASS_L1_LOGIC`: if !(L2C.is_bypassing & BYP_L1_BIT) call L1D.add_rq; else if (L2C.is_bypassing & BYP_L1_BIT) and warmup_complete: set l1_bypassed=1, fill_level=FILL_L2, call L2C.add_rq.

    ### no_bypass_path
            `#else`: call L1D.add_rq unconditionally.

    ### rq_result
            If rq_index==-2 return -2; else set LQ.entry.fetched=INFLIGHT; return rq_index.


## complete_execution

    ### guard
            Return if is_mem and num_mem_ops!=0; return if executed!=INFLIGHT or event_cycle future.

    ### mark_complete
            Set executed=COMPLETED; decrement inflight_mem_executions or inflight_reg_executions; increment completed_executions.

    ### dep_release
            addr_dependencies.remove_producer; if reg_RAW_producer call reg_RAW_release.

    ### branch_resume
            If flat_branch_mispredicted: set fetch_resume_cycle = current + BRANCH_MISPREDICT_PENALTY.


## reg_RAW_release

    ### dep_vec_iter
            Get dep_vec from reg_dep_tracker; for each [dep_idx, src_idx]: decrement num_reg_dependent; if zero set reg_ready=1; if IS_MEMORY set scheduled=INFLIGHT else set scheduled=COMPLETED.

    ### rte0_enqueue
            On non-memory dep with zero num_reg_dependent: enqueue dep_idx into RTE0.


## operate_cache

    ### cache_operate_calls
            Call operate() on ITLB, DTLB, STLB, L1I, L1D, L2C in order.


## update_rob

    ### itlb_processed
            If ITLB.PROCESSED has entry with event_cycle ready: call complete_instr_fetch(&ITLB.PROCESSED, 1).

    ### l1i_processed
            If L1I.PROCESSED ready: call complete_instr_fetch(&L1I.PROCESSED, 0).

    ### dtlb_processed
            If DTLB.PROCESSED ready: call complete_data_fetch(&DTLB.PROCESSED, 1).

    ### l1d_processed
            If L1D.PROCESSED ready: call complete_data_fetch(&L1D.PROCESSED, 0).

    ### l2c_bypass_processed
            `#ifdef BYPASS_L1_LOGIC`: if L2C.PROCESSED ready: call complete_data_fetch(&L2C.PROCESSED, 0).

    ### complete_execution_scan
            If inflight executions > 0: scan ROB head..tail (or wrapped) calling complete_execution per entry; break early if both inflight counts reach zero.


## complete_instr_fetch

    ### tlb_or_cache_update
            If is_it_tlb: set ROB.translated=COMPLETED and instruction_pa from data_pa; else set rob_events.fetched=COMPLETED. Set event_cycle=current; increment num_fetched.

    ### merged_update
            If instr_merged: for each rob_index_depend_on_me entry apply same tlb/cache update with staggered event_cycle; call queue->remove_queue.


## complete_data_fetch

    ### dtlb_rfo_path
            If is_it_tlb and type==RFO: set SQ.physical_address, SQ.translated=COMPLETED, event_cycle; enqueue into RTS1; call handle_merged_translation.

    ### dtlb_load_path
            If is_it_tlb and type!=RFO: set LQ.physical_address, LQ.translated=COMPLETED, event_cycle; enqueue into RTL1; call handle_merged_translation. Set rob event_cycle.

    ### l1d_rfo_path
            If not tlb and type==RFO: call handle_merged_load.

    ### l1d_load_path
            If not tlb and type!=RFO: set LQ.fetched=COMPLETED, event_cycle; decrement ROB.num_mem_ops; increment inflight_mem_executions if zero; release_load_queue; call handle_merged_load. Call queue->remove_queue.


## handle_merged_translation

    ### store_merged
            If store_merged: ITERATE_SET over sq_index_depend_on_me; set SQ.translated=COMPLETED, physical_address, event_cycle; enqueue into RTS1.

    ### load_merged
            If load_merged: ITERATE_SET over lq_index_depend_on_me; set LQ.translated=COMPLETED, physical_address, event_cycle; enqueue into RTL1.


## handle_merged_load

    ### lq_iter
            ITERATE_SET over lq_index_depend_on_me: skip if instr_id==0 or address mismatch; set LQ.fetched=COMPLETED, event_cycle; decrement ROB.num_mem_ops; increment inflight_mem_executions if zero; release_load_queue.


## release_load_queue

    ### release
            If producer_id != UINT64_MAX: lq_pending_loads.remove_pending_load; LQ.entry.quickReset; free_LQueue.free_lq; decrement LQ.occupancy.


## retire_rob

    ### retire_guard
            For each of RETIRE_WIDTH: return if ROB.head ip==0 or executed!=COMPLETED.

    ### store_count
            Count destination_memory entries as num_store.

    ### store_wq_submit
            If num_store and L1D.WQ has space: for each destination_memory build data_packet (RFO) from SQ entry; call L1D.add_wq. If WQ full: increment FULL/STALL, return.

    ### sq_release
            For each sq_index != UINT16_MAX: sq_address_map.remove_sq_entry; SQ.entry.quickReset; decrement SQ.occupancy; advance SQ.head.

    ### rob_release
            XOR execution_checksum with ip (in simulation window); rob_hash_table.retire_rob_idx; mem_dependencies.remove_producer; decrement rob_memory_count; clear rob_events.raw and flat_branch_mispredicted; ROB.entry.quickReset; advance ROB.head; decrement ROB.occupancy; decrement completed_executions; increment num_retired.
