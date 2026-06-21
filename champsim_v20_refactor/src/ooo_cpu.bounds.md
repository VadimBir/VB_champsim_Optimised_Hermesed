# champsim_refactor/src/ooo_cpu.cc — function bounds

## print_core_config  L142-159
  block_01  L144-158  `cout << "fetch_width ;" << FETCH_WIDTH << "; "`

## O3_CPU::initialize_core  L161-164
  block_01  L163-163  `(empty body)`

## O3_CPU::handle_branch [USE_TRACE_HELPER variant]  L458-537
  block_01  L460-463  `uint8_t continue_reading = 1;`
  block_02  L466-536  `while (continue_reading) {`
  block_03  L467-473  `  uint32_t t = tbuf.tail.load(std::memory_order_relaxed);`
  block_04  L476-500  `  TraceBufEntry& src = tbuf.entries[t]; *arch_instr = src.instr;`
  block_05  L506-534  `  if (ROB.occupancy < ROB.SIZE) {`

## O3_CPU::handle_branch [single-threaded variant]  L541-650
  block_01  L547-550  `uint8_t continue_reading = 1;`
  block_02  L553-649  `while (continue_reading) {`
  block_03  L557-563  `  if (xz_reader.eof()) {`
  block_04  L564-615  `  read_success = xz_reader.read(&current_instr, instr_size, 1);`
  block_05  L618-648  `  if (ROB.occupancy < ROB.SIZE) {`

## O3_CPU::add_to_rob  L655-692
  block_01  L657-664  `const uint32_t index = ROB.tail;`
  block_02  L666-680  `ROB.entry[index] = *arch_instr;`
  block_03  L683-691  `ROB.occupancy++;`

## O3_CPU::check_rob  L694-710
  block_01  L696-697  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_02  L702-709  `return rob_hash_table.get_rob_idx(cpu, instr_id, ROB.SIZE);`

## O3_CPU::fetch_instruction  L728-812
  block_01  L730-733  `if ((fetch_stall == 1) && (PCYCLE_GE(...)) && (fetch_resume_cycle != 0)) {`
  block_02  L735-768  `uint32_t read_index = (ROB.last_read == (ROB.SIZE-1)) ? 0 : (ROB.last_read + 1);`
  block_03  L770-811  `uint32_t fetch_index = (ROB.last_fetch == (ROB.SIZE-1)) ? 0 : (ROB.last_fetch + 1);`

## O3_CPU::schedule_instruction  L827-920
  block_01  L829-831  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_02  L832-836  `const uint64_t* fc_ptr = rob_events.per_cpu[cpu].fetched_complete;`
  block_03  L846-852  `{ uint64_t dirty = 0;`
  block_04  L860-865  `const uint64_t* __restrict__ fc_l   = fc_ptr;`
  block_05  L866-912  `auto fused_scan = [fc_l, sinf_l, scmp_l, ec_l, ccp_l, self](`
  block_06  L914-919  `if (head < limit) {`

## O3_CPU::do_scheduling  L922-946
  block_01  L924-926  `BS_SET(rob_events.per_cpu[cpu].reg_ready, rob_index);`
  block_02  L928-944  `const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, rob_index);`

## O3_CPU::reg_dependency  L948-963
  block_01  L949-949  `if (rob_index == ROB.head) return;`
  block_02  L952-961  `for (uint32_t j = 0; j < NUM_INSTR_SOURCES; j++) {`

## O3_CPU::reg_RAW_dependency  L965-974
  block_01  L967-973  `reg_dep_tracker.add(prior, current, source_index);`

## O3_CPU::execute_instruction  L976-1039
  block_01  L978-979  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_02  L985-1010  `while (exec_issued < EXEC_WIDTH) {`
  block_03  L1013-1038  `while (exec_issued < EXEC_WIDTH) {`

## O3_CPU::do_execution  L1041-1051
  block_01  L1043-1050  `if (BS_TST(rob_events.per_cpu[cpu].reg_ready, rob_index) &&`

## O3_CPU::schedule_memory_instruction  L1109-1182
  block_01  L1111-1112  `if (rob_memory_count[cpu] == 0 || (ROB.head == ROB.tail && ROB.occupancy == 0))`
  block_02  L1114-1123  `const uint64_t* im_ptr = rob_events.per_cpu[cpu].is_mem;`
  block_03  L1126-1132  `const uint32_t sched_start = next_mem_sched_start[cpu];`
  block_04  L1134-1173  `auto scan_and_schedule = [&](uint32_t start, uint32_t end) -> bool {`
  block_05  L1175-1181  `if (sched_start < effective_limit) {`

## O3_CPU::do_memory_scheduling  L1190-1204
  block_01  L1192-1203  `uint16_t not_available = check_and_add_lsq(rob_index);`

## O3_CPU::check_and_add_lsq  L1206-1247
  block_01  L1208-1223  `uint32_t num_mem_ops = 0, num_added = 0;`
  block_02  L1226-1238  `for (uint8_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {`
  block_03  L1240-1246  `if (num_added == num_mem_ops)`

## O3_CPU::add_load_queue  L1248-1341
  block_01  L1250-1266  `const uint32_t lq_index = free_LQueue.alloc_lq(cpu);`
  block_02  L1268-1285  `#ifdef USE_HERMES`
  block_03  L1287-1306  `uint32_t forwarding_index = SQ.SIZE;`
  block_04  L1308-1323  `if (forwarding_index != SQ.SIZE && fwd_info) {`
  block_05  L1325-1340  `robe.source_added[data_index] = 1;`

## O3_CPU::add_store_queue  L1343-1383
  block_01  L1345-1368  `uint16_t sq_index = SQ.tail;`
  block_02  L1370-1382  `robe.destination_added[data_index] = 1;`

## O3_CPU::operate_lsq  L1398-1559
  block_01  L1400-1402  `if (__builtin_expect( (RTS0[RTS0_head] >= SQ_SIZE && RTS1[RTS1_head] >= SQ_SIZE &&`
  block_02  L1407-1454  `uint32_t store_issued = 0, num_iteration = 0;`
  block_03  L1456-1481  `num_iteration = 0;`
  block_04  L1483-1529  `unsigned load_issued = 0;`
  block_05  L1531-1558  `num_iteration = 0;`

## O3_CPU::execute_store  L1562-1615
  block_01  L1564-1575  `auto* sq_entry = &SQ.entry[sq_index];`
  block_02  L1577-1613  `if (rob_entry->is_producer) {`

## O3_CPU::execute_load  L1623-1720
  block_01  L1628-1641  `PACKET data_packet = EXEC_LOAD_DATA_TEMPLATE;`
  block_02  L1660-1704  `#ifdef BYPASS_L1_LOGIC`
  block_03  L1708-1719  `if (rq_index == -2)`

## O3_CPU::complete_execution  L1722-1743
  block_01  L1724-1729  `const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, rob_index);`
  block_02  L1731-1742  `robe.executed = COMPLETED;`

## O3_CPU::reg_RAW_release  L1745-1770
  block_01  L1748-1768  `auto* dep_vec = reg_dep_tracker.get(rob_index);`
  block_02  L1769-1769  `reg_dep_tracker.remove(rob_index);`

## O3_CPU::operate_cache  L1772-1780
  block_01  L1774-1779  `ITLB.operate();`

## O3_CPU::update_rob  L1782-1841
  block_01  L1784-1807  `if (ITLB.PROCESSED.occupancy && (...)) complete_instr_fetch(...);`
  block_02  L1812-1840  `if ((inflight_reg_executions > 0) || (inflight_mem_executions > 0)) {`

## O3_CPU::complete_instr_fetch  L1843-1882
  block_01  L1845-1860  `uint32_t index = queue->head,`
  block_02  L1864-1878  `if (queue->entry[index].instr_merged) {`
  block_03  L1881-1881  `queue->remove_queue(&queue->entry[index]);`

## O3_CPU::complete_data_fetch  L1884-1971
  block_01  L1886-1890  `uint32_t index = queue->head;`
  block_02  L1894-1929  `if (is_it_tlb) { // DTLB`
  block_03  L1930-1967  `else { // L1D or ByP L2C`
  block_04  L1969-1970  `queue->remove_queue(&queue->entry[index]);`

## O3_CPU::handle_merged_translation  L2036-2071
  block_01  L2038-2051  `if (provider->store_merged) {`
  block_02  L2053-2070  `if (provider->load_merged) {`

## O3_CPU::handle_merged_load  L2073-2120
  block_01  L2088-2119  `ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE) {`

## O3_CPU::release_load_queue  L2122-2138
  block_01  L2123-2129  `auto& lqe = LQ.entry[lq_index];`
  block_02  L2132-2137  `if (lqe.producer_id != UINT64_MAX) {`

## O3_CPU::retire_rob  L2149-2247
  block_01  L2151-2154  `for (uint32_t n=0; n<RETIRE_WIDTH; n++) {`
  block_02  L2157-2159  `if (rhe.executed != COMPLETED) {`
  block_03  L2162-2203  `uint32_t num_store = 0;`
  block_04  L2207-2220  `if (num_store) for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {`
  block_05  L2221-2222  `if (num_retired >= warmup_instructions && num_retired < warmup_instructions + simulation_instructions)`
  block_06  L2230-2246  `rob_hash_table.retire_rob_idx(cpu, rhe.instr_id);`
