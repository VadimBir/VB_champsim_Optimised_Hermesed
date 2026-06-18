# ooo_cpu.cc — function bounds

## print_core_config  L36-53
  block_00  L38-52  `cout << "fetch_width ;" << FETCH_WIDTH << "; "`

## initialize_core  L55-58

## alloc_lq  L151-182
  block_00  L152-152  `static bool setup[NUM_CPUS] = {false};`
  block_01  L154-154  `// Lazy initialization on first call per CPU`
  block_02  L155-170  `if (!setup[cpu]) {`
  block_03  L172-172  `// Find first free slot`
  block_04  L173-173  `alignas(64)  uint8_t words = (LQ_SIZE + 63) / 64;`
  block_05  L174-180  `for (uint8_t i = 0; i < words; i++) {`
  block_06  L181-181  `return LQ_SIZE;  // No free slots`
  block_07  L181-181  `return LQ_SIZE;  // No free slots`

## free_lq  L184-190
  block_00  L185-185  `uint16_t w = idx >> 6, b = idx & 63;`
  block_01  L186-188  `#ifdef TRUE_SANITY_CHECK`
  block_02  L189-189  `lq_bits[cpu][w] |= 1ULL << b;`

## add_pending_load  L197-199
  block_00  L198-198  `address_to_lq[cpu][virtual_address].insert(lq_index);`

## get_pending_loads  L201-204
  block_00  L202-202  `auto it = address_to_lq[cpu].find(virtual_address);`
  block_01  L203-203  `return (it != address_to_lq[cpu].end()) ? &it->second : null`

## remove_pending_load  L206-213
  block_00  L207-207  `auto it = address_to_lq[cpu].find(virtual_address);`
  block_01  L208-212  `if (it != address_to_lq[cpu].end()) {`

## clear_cpu  L215-217
  block_00  L216-216  `address_to_lq[cpu].clear();`

## set_branch_mispredicted  L225-227
  block_00  L226-226  `branch_mispredicted[cpu][rob_idx] = value;`

## get_branch_mispredicted  L229-231
  block_00  L230-230  `return branch_mispredicted[cpu][rob_idx];`

## clear_rob  L233-235
  block_00  L234-234  `branch_mispredicted[cpu][rob_idx] = 0;`

## clear_cpu  L237-239
  block_00  L238-238  `memset(branch_mispredicted[cpu], 0, ROB_SIZE);`

## add_rob_idx  L250-252
  block_00  L251-251  `rob_maps[cpu][instr_id] = rob_index;`

## get_rob_idx  L255-267
  block_00  L256-256  `auto it = rob_maps[cpu].find(instr_id);`
  block_01  L257-259  `if (it != rob_maps[cpu].end()) {`
  block_02  L260-260  `// SANITY CHECK - Same error handling as original check_rob`
  block_03  L261-261  `cerr << "[ROB_ERROR] " << __func__ << " does not have any ma`
  block_04  L262-262  `cerr << " instr_id: " << instr_id << endl;`
  block_05  L263-263  `int siblingROB = get_rob_idx(0, 1074969, ROB_SIZE);`
  block_06  L264-264  `cerr << " EXIST ISNTR ID: 1074969 ROB: " << siblingROB;`
  block_07  L265-265  `assert(0);`
  block_08  L266-266  `return rob_size;`

## retire_rob_idx  L270-272
  block_00  L271-271  `rob_maps[cpu].erase(instr_id);`

## clear_cpu  L275-277
  block_00  L276-276  `rob_maps[cpu].clear();`

## add_sq_entry  L286-290
  block_00  L287-289  `if (virtual_address != 0) {`

## get_matching_indices  L291-297
  block_00  L292-292  `auto it = address_to_indices[cpu].find(virtual_address);`
  block_01  L293-295  `if (it != address_to_indices[cpu].end() && !it->second.empty`
  block_02  L296-296  `return nullptr;`

## remove_sq_entry  L298-307
  block_00  L299-299  `if (virtual_address == 0) return;`
  block_01  L300-300  `auto it = address_to_indices[cpu].find(virtual_address);`
  block_02  L301-306  `if (it != address_to_indices[cpu].end()) {`

## handle_branch  L318-402
  block_00  L320-320  `uint8_t continue_reading = 1;`
  block_01  L321-321  `uint32_t num_reads = 0;`
  block_02  L322-322  `instrs_to_read_this_cycle = FETCH_WIDTH;`
  block_03  L324-324  `TraceBuffer& tbuf = trace_helper.buffers[cpu];`
  block_04  L326-401  `while (continue_reading) {`

## handle_branch  L406-520
  block_00  L408-408  `// actual processors do not work like this but for easier im`
  block_01  L409-409  `// we read instruction traces and virtually add them in the `
  block_02  L410-410  `// note that these traces are not yet translated and fetched`
  block_03  L412-412  `uint8_t continue_reading = 1;`
  block_04  L413-413  `uint32_t num_reads = 0;`
  block_05  L414-414  `instrs_to_read_this_cycle = FETCH_WIDTH;`
  block_06  L417-417  `// first, read PIN trace`
  block_07  L418-519  `while (continue_reading) {`

## add_to_rob  L525-568
  block_00  L527-527  `const uint32_t index = ROB.tail;`
  block_01  L528-528  `// flush out cout`
  block_02  L529-529  `// sanity check`
  block_03  L530-534  `if (previousNotEmpty != 0) {`
  block_04  L536-536  `ROB.entry[index] = arch_instr;`
  block_05  L540-540  `rob_hash_table.add_rob_idx(cpu, instr_unique_id, index);`
  block_06  L541-541  `ROB.entry[index]->instr_id = instr_unique_id;`
  block_07  L542-542  `ROB.entry[index]->rob_index = index;  // Set correct index`
  block_08  L542-542  `ROB.entry[index]->rob_index = index;  // Set correct index`
  block_09  L543-543  `BS_CLR(rob_events.per_cpu[cpu].fetched_inflight, index);`
  block_10  L544-544  `BS_CLR(rob_events.per_cpu[cpu].fetched_complete, index);`
  block_11  L545-545  `BS_CLR(rob_events.per_cpu[cpu].sched_inflight, index);`
  block_12  L546-546  `BS_CLR(rob_events.per_cpu[cpu].sched_complete, index);`
  block_13  L547-547  `rob_events.per_cpu[cpu].event_cycle[index] = PACK_CYCLE(curr`
  block_14  L549-549  `addr_dependencies.add_producer(index, arch_instr->destinatio`
  block_15  L550-550  `mem_dependencies.add_producer(index, arch_instr->destination`
  block_16  L551-551  `// (int) // DEBUGGING TRAVERSAL OVER ROB_SIZE FOR FETCHED_GE`
  block_17  L553-553  `ROB.occupancy++;`
  block_18  L554-554  `ROB.tail++;`
  block_19  L555-556  `if (ROB.tail >= ROB.SIZE)`
  block_20  L559-565  `#ifdef TRUE_SANITY_CHECK`
  block_21  L567-567  `return index;`

## check_rob  L570-586
  block_00  L572-573  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_01  L575-575  `// rob_hash_table.rob_maps[cpu].prefetch_mapped_value(instr_`
  block_02  L577-577  `// REPLACE ALL LINEAR SEARCH LOOPS WITH THIS SINGLE LINE:`
  block_03  L578-578  `return rob_hash_table.get_rob_idx(cpu, instr_id, ROB.SIZE);`
  block_04  L581-581  `cerr << "[ROB_ERROR] " << __func__ << " does not have any ma`
  block_05  L582-582  `cerr << " instr_id: " << instr_id << endl;`
  block_06  L583-583  `assert(0);`
  block_07  L585-585  `return ROB.SIZE;`

## fetch_instruction  L604-704
  block_00  L606-609  `if ((fetch_stall == 1) && (CYC_GE(CYC(current_core_cycle[cpu`
  block_01  L611-611  `uint32_t read_index = (ROB.last_read == (ROB.SIZE-1)) ? 0 : `
  block_02  L612-659  `for (uint8_t i = 0; i < FETCH_WIDTH; i++) {`
  block_03  L661-661  `uint32_t fetch_index = (ROB.last_fetch == (ROB.SIZE-1)) ? 0 `
  block_04  L662-703  `for (uint32_t i = 0; i < FETCH_WIDTH; i++) {`

## schedule_instruction  L719-799
  block_00  L721-722  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_01  L724-724  `const uint64_t* fc_ptr = rob_events.per_cpu[cpu].fetched_com`
  block_02  L725-725  `const uint64_t* sinf_p = rob_events.per_cpu[cpu].sched_infli`
  block_03  L726-726  `const uint64_t* scmp_p = rob_events.per_cpu[cpu].sched_compl`
  block_04  L727-727  `const uint32_t* ec_ptr = rob_events.per_cpu[cpu].event_cycle`
  block_05  L728-728  `const uint32_t ccp = PACK_CYCLE(current_core_cycle[cpu]);`
  block_06  L729-729  `const uint32_t head = ROB.head;`
  block_07  L730-730  `const uint32_t limit = ROB.next_fetch[1];`
  block_08  L732-732  `// Pass 1: build cycle_ready sequentially — tile scan like b`
  block_09  L733-740  `for (uint32_t w = 0; w < ROB_WORDS; w++) {`
  block_10  L742-742  `// Pass 2: AND pipeline (SIMD-friendly 8-word loop)`
  block_11  L743-744  `for (uint32_t w = 0; w < ROB_WORDS; w++)`
  block_12  L746-746  `// Pass 3: range mask + break-point in one pass`
  block_13  L747-768  `auto apply_range_and_break = [&](uint32_t start, uint32_t en`
  block_14  L770-788  `if (head < limit) {`
  block_15  L790-798  `for (uint32_t w = 0; w < ROB_WORDS; w++) {`

## do_scheduling  L801-828
  block_00  L803-803  `BS_SET(rob_events.per_cpu[cpu].reg_ready, rob_index);`
  block_01  L804-804  `reg_dependency(rob_index);`
  block_02  L805-805  `ROB.next_schedule = (rob_index == (ROB.SIZE - 1)) ? 0 : (rob`
  block_03  L807-807  `const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, r`
  block_04  L808-827  `if (is_mem){`

## reg_dependency  L830-845
  block_00  L831-831  `if (rob_index == ROB.head) return;`
  block_01  L833-833  `// Direct lookup - NO TRAVERSAL`
  block_02  L834-844  `for (uint32_t j = 0; j < NUM_INSTR_SOURCES; j++) {`

## reg_RAW_dependency  L847-856
  block_00  L849-849  `reg_dep_tracker.add(prior, current, source_index);`
  block_01  L850-850  `ROB.entry[prior]->reg_RAW_producer = 1;`
  block_02  L852-852  `BS_CLR(rob_events.per_cpu[cpu].reg_ready, current);`
  block_03  L853-853  `ROB.entry[current]->producer_id = ROB.entry[prior]->instr_id`
  block_04  L854-854  `ROB.entry[current]->num_reg_dependent++;`
  block_05  L855-855  `ROB.entry[current]->reg_RAW_checked[source_index] = 1;`

## execute_instruction  L858-921
  block_00  L860-861  `if ((ROB.head == ROB.tail) && ROB.occupancy == 0)`
  block_01  L863-863  `// out-of-order execution for non-memory instructions`
  block_02  L864-864  `// memory instructions are handled by memory_instruction()`
  block_03  L865-865  `uint32_t exec_issued = 0, num_iteration = 0;`
  block_04  L867-892  `while (exec_issued < EXEC_WIDTH) {`
  block_05  L894-894  `num_iteration = 0;`
  block_06  L895-920  `while (exec_issued < EXEC_WIDTH) {`

## do_execution  L923-933
  block_00  L925-932  `if (BS_TST(rob_events.per_cpu[cpu].reg_ready, rob_index) && `

## schedule_memory_instruction  L990-1061
  block_00  L992-993  `if (rob_memory_count[cpu] == 0 || (ROB.head == ROB.tail && R`
  block_01  L995-995  `const uint64_t* im_ptr = rob_events.per_cpu[cpu].is_mem;`
  block_02  L996-996  `const uint64_t* rr_ptr = rob_events.per_cpu[cpu].reg_ready;`
  block_03  L997-997  `const uint64_t* fc_ptr = rob_events.per_cpu[cpu].fetched_com`
  block_04  L998-998  `const uint64_t* sinf_p = rob_events.per_cpu[cpu].sched_infli`
  block_05  L999-999  `const uint64_t* scmp_p = rob_events.per_cpu[cpu].sched_compl`
  block_06  L1001-1001  `// Bitwise pipeline: is_mem & reg_ready & fetched_complete &`
  block_07  L1002-1002  `uint64_t ready[ROB_WORDS];`
  block_08  L1003-1004  `for (uint32_t w = 0; w < ROB_WORDS; w++)`
  block_09  L1006-1006  `// Linear scan from next_mem_sched_start, same as baseline`
  block_10  L1007-1007  `const uint32_t sched_start = next_mem_sched_start[cpu];`
  block_11  L1008-1010  `uint32_t scan_end = (ROB.head + ROB.occupancy <= ROB.SIZE)`
  block_12  L1011-1011  `const uint32_t limit = ROB.next_schedule;`
  block_13  L1012-1012  `const uint32_t effective_limit = (scan_end < limit) ? scan_e`
  block_14  L1013-1013  `uint16_t mem_remaining = rob_memory_count[cpu];`
  block_15  L1015-1052  `auto scan_and_schedule = [&](uint32_t start, uint32_t end) -`
  block_16  L1054-1060  `if (sched_start < effective_limit) {`

## execute_memory_instruction  L1063-1067
  block_00  L1065-1065  `operate_lsq();`
  block_01  L1066-1066  `operate_cache();`

## do_memory_scheduling  L1069-1083
  block_00  L1071-1071  `uint16_t not_available = check_and_add_lsq(rob_index);`
  block_01  L1072-1082  `if (not_available == 0) {`

## check_and_add_lsq  L1085-1131
  block_00  L1087-1087  `uint32_t num_mem_ops = 0, num_added = 0;`
  block_01  L1089-1089  `// load`
  block_02  L1090-1102  `for (uint8_t i=0; i<NUM_INSTR_SOURCES; i++) {`
  block_03  L1104-1104  `// store`
  block_04  L1105-1117  `for (uint8_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {`
  block_05  L1119-1120  `if (num_added == num_mem_ops)`
  block_06  L1122-1122  `const uint32_t not_available = num_mem_ops - num_added;`
  block_07  L1123-1128  `#ifdef TRUE_SANITY_CHECK`
  block_08  L1130-1130  `return not_available;`

## add_load_queue  L1132-1233
  block_00  L1134-1134  `const uint32_t lq_index = free_LQueue.alloc_lq(cpu);`
  block_01  L1136-1141  `#ifdef TRUE_SANITY_CHECK`
  block_02  L1143-1143  `ROB.entry[rob_index]->lq_index[data_index] = lq_index;`
  block_03  L1144-1144  `LQ.entry[lq_index].instr_id = ROB.entry[rob_index]->instr_id`
  block_04  L1145-1145  `LQ.entry[lq_index].virtual_address = ROB.entry[rob_index]->s`
  block_05  L1146-1146  `LQ.entry[lq_index].ip = ROB.entry[rob_index]->ip;`
  block_06  L1147-1147  `LQ.entry[lq_index].data_index = data_index;`
  block_07  L1148-1148  `LQ.entry[lq_index].rob_index = rob_index;`
  block_08  L1149-1149  `LQ.entry[lq_index].asid[0] = ROB.entry[rob_index]->asid[0];`
  block_09  L1150-1150  `LQ.entry[lq_index].asid[1] = ROB.entry[rob_index]->asid[1];`
  block_10  L1151-1151  `LQ.entry[lq_index].event_cycle = CYC(current_core_cycle[cpu]`
  block_11  L1152-1152  `LQ.occupancy++;`
  block_12  L1154-1159  `#ifdef USE_HERMES`
  block_13  L1161-1161  `// RAW dependency check`
  block_14  L1162-1171  `if (rob_index != ROB.head) {`
  block_15  L1173-1173  `// Store forwarding check`
  block_16  L1174-1174  `uint32_t forwarding_index = SQ.SIZE;`
  block_17  L1175-1175  `const ankerl::unordered_dense::set<uint16_t>* matching_sq_in`
  block_18  L1177-1192  `if (matching_sq_indices != nullptr) {`
  block_19  L1194-1215  `if (forwarding_index != SQ.SIZE) {`
  block_20  L1217-1217  `ROB.entry[rob_index]->source_added[data_index] = 1;`
  block_21  L1219-1225  `if (LQ.entry[lq_index].virtual_address && (LQ.entry[lq_index`
  block_22  L1227-1232  `if (LQ.entry[lq_index].virtual_address && (LQ.entry[lq_index`

## add_store_queue  L1235-1275
  block_00  L1237-1237  `uint16_t sq_index = SQ.tail;`
  block_01  L1238-1241  `#ifdef TRUE_SANITY_CHECK`
  block_02  L1243-1243  `// add it to the store queue`
  block_03  L1244-1244  `ROB.entry[rob_index]->sq_index[data_index] = sq_index;`
  block_04  L1245-1245  `SQ.entry[sq_index].instr_id = ROB.entry[rob_index]->instr_id`
  block_05  L1246-1246  `SQ.entry[sq_index].virtual_address = ROB.entry[rob_index]->d`
  block_06  L1248-1248  `sq_address_map.add_sq_entry(cpu, SQ.entry[sq_index].virtual_`
  block_07  L1250-1250  `SQ.entry[sq_index].ip = ROB.entry[rob_index]->ip;`
  block_08  L1251-1251  `SQ.entry[sq_index].data_index = data_index;`
  block_09  L1252-1252  `SQ.entry[sq_index].rob_index = rob_index;`
  block_10  L1253-1253  `SQ.entry[sq_index].asid[0] = ROB.entry[rob_index]->asid[0];`
  block_11  L1254-1254  `SQ.entry[sq_index].asid[1] = ROB.entry[rob_index]->asid[1];`
  block_12  L1255-1255  `SQ.entry[sq_index].event_cycle = CYC(current_core_cycle[cpu]`
  block_13  L1257-1257  `SQ.occupancy++;`
  block_14  L1258-1258  `SQ.tail++;`
  block_15  L1259-1260  `if (SQ.tail == SQ.SIZE)`
  block_16  L1262-1262  `// succesfully added to the store queue`
  block_17  L1263-1263  `ROB.entry[rob_index]->destination_added[data_index] = 1;`
  block_18  L1265-1265  `STA[STA_head] = UINT64_MAX;`
  block_19  L1266-1266  `STA_head++;`
  block_20  L1267-1268  `if (STA_head == STA_SIZE)`
  block_21  L1270-1270  `RTS0[RTS0_tail] = sq_index;`
  block_22  L1271-1271  `RTS0_tail++;`
  block_23  L1272-1273  `if (RTS0_tail == SQ_SIZE)`

## operate_lsq  L1290-1430
  block_00  L1292-1292  `// handle store`
  block_01  L1293-1293  `uint32_t store_issued = 0, num_iteration = 0;`
  block_02  L1295-1295  `PACKET sq_data_packet = SQ_TLB_DATA_TEMPLATE;`
  block_03  L1296-1335  `while (store_issued < SQ_WIDTH) {`
  block_04  L1337-1337  `num_iteration = 0;`
  block_05  L1338-1359  `while (store_issued < SQ_WIDTH) {`
  block_06  L1361-1361  `unsigned load_issued = 0;`
  block_07  L1362-1362  `num_iteration = 0;`
  block_08  L1363-1363  `PACKET lq_data_packet = LQ_TLB_DATA_TEMPLATE;`
  block_09  L1364-1403  `while (load_issued < LQ_WIDTH) {`
  block_10  L1405-1405  `num_iteration = 0;`
  block_11  L1406-1429  `while (load_issued < LQ_WIDTH) {`

## execute_store  L1433-1498
  block_00  L1435-1435  `auto* sq_entry = &SQ.entry[sq_index];`
  block_01  L1436-1436  `auto* rob_entry = ROB.entry[rob_index];`
  block_02  L1438-1438  `sq_entry->fetched = COMPLETED;`
  block_03  L1439-1439  `sq_entry->event_cycle = CYC(current_core_cycle[cpu]);`
  block_04  L1441-1441  `rob_entry->num_mem_ops--;`
  block_05  L1442-1442  `// TRACK_MEMOPS(rob_index, " ROB IDX STORE_FWD");`
  block_06  L1443-1443  `rob_events.per_cpu[cpu].event_cycle[rob_index] = PACK_CYCLE(`
  block_07  L1444-1449  `#ifdef TRUE_SANITY_CHECK`
  block_08  L1450-1450  `inflight_mem_executions += (rob_entry->num_mem_ops == 0);  /`
  block_09  L1450-1450  `inflight_mem_executions += (rob_entry->num_mem_ops == 0);  /`
  block_10  L1452-1497  `if (rob_entry->is_producer) {`

## execute_load  L1506-1605
  block_00  L1509-1509  `// add it to L1D`
  block_01  L1510-1510  `// Copy from template (avoids constructor cost)`
  block_02  L1511-1511  `PACKET data_packet = EXEC_LOAD_DATA_TEMPLATE;`
  block_03  L1512-1512  `data_packet.cpu = cpu;`
  block_04  L1513-1513  `data_packet.data_index = LQ.entry[lq_index].data_index;`
  block_05  L1514-1514  `data_packet.lq_index = lq_index;`
  block_06  L1515-1515  `data_packet.address = LQ.entry[lq_index].physical_address >>`
  block_07  L1516-1516  `data_packet.full_addr = LQ.entry[lq_index].physical_address;`
  block_08  L1517-1517  `data_packet.instr_id = LQ.entry[lq_index].instr_id;`
  block_09  L1518-1518  `data_packet.rob_index = LQ.entry[lq_index].rob_index;`
  block_10  L1519-1519  `data_packet.ip = LQ.entry[lq_index].ip;`
  block_11  L1520-1520  `data_packet.type = LOAD;`
  block_12  L1521-1521  `data_packet.asid[0] = LQ.entry[lq_index].asid[0];`
  block_13  L1522-1522  `data_packet.asid[1] = LQ.entry[lq_index].asid[1];`
  block_14  L1523-1523  `data_packet.event_cycle = LQ.entry[lq_index].event_cycle;`
  block_15  L1524-1524  `int rq_index = 0;`
  block_16  L1526-1526  `// if (LQ.entry[lq_index].instr_id == 1074723) {`
  block_17  L1527-1527  `//     if (check_rob(LQ.entry[lq_index].instr_id)) {`
  block_18  L1528-1528  `//         int buggROB = rob_hash_table.get_rob_idx(0,LQ.ent`
  block_19  L1529-1529  `//         cout << " instr " << LQ.entry[lq_index].instr_id `
  block_20  L1530-1530  `//     } else {`
  block_21  L1531-1531  `//         cout << " ENTRY NOT!!! EXIST" << endl;`
  block_22  L1532-1532  `//     }`
  block_23  L1533-1533  `// }`
  block_24  L1535-1535  `// #ifdef BYPASS_DEBUG`
  block_25  L1536-1536  `//     uint64_t aAddr = data_packet.address;`
  block_26  L1537-1537  `//     if ((data_packet.rob_index == 27 && (int)data_packet.`
  block_27  L1538-1538  `//         cerr << __func__ << " CAUGHT DEADLOCK ROB!!!" << `
  block_28  L1539-1539  `//     cout << " Before execLoad LQ: " << lq_index << "instr`
  block_29  L1540-1540  `// #endif`
  block_30  L1542-1589  `#ifdef BYPASS_L1_LOGIC`
  block_31  L1593-1596  `if (rq_index == -2)`
  block_32  L1597-1597  `// #ifdef BYPASS_DEBUG`
  block_33  L1598-1598  `//     aAddr = data_packet.address;`
  block_34  L1599-1599  `//     if ((data_packet.rob_index == 27 && (int)data_packet.`
  block_35  L1600-1600  `//         cerr << __func__ << " CAUGHT DEADLOCK ROB!!!" << `
  block_36  L1601-1601  `//     cout << " After execLoad LQ: " << lq_index << "instrI`
  block_37  L1602-1602  `// #endif`
  block_38  L1604-1604  `return rq_index;`

## complete_execution  L1607-1627
  block_00  L1609-1609  `const bool is_mem = BS_TST(rob_events.per_cpu[cpu].is_mem, r`
  block_01  L1610-1610  `if (is_mem && ROB.entry[rob_index]->num_mem_ops != 0) return`
  block_02  L1612-1613  `if (ROB.entry[rob_index]->executed != INFLIGHT ||`
  block_03  L1615-1615  `ROB.entry[rob_index]->executed = COMPLETED;`
  block_04  L1616-1616  `BS_CLR(rob_events.per_cpu[cpu].exec_inflight, rob_index);`
  block_05  L1617-1617  `inflight_mem_executions -= is_mem;`
  block_06  L1618-1618  `inflight_reg_executions -= !is_mem;`
  block_07  L1619-1619  `completed_executions++;`
  block_08  L1621-1621  `addr_dependencies.remove_producer(rob_index, ROB.entry[rob_i`
  block_09  L1622-1623  `if (ROB.entry[rob_index]->reg_RAW_producer)`
  block_10  L1625-1626  `if (flat_branch_mispredicted.branch_mispredicted[cpu][rob_in`

## reg_RAW_release  L1629-1657
  block_00  L1632-1632  `auto* dep_vec = reg_dep_tracker.get(rob_index);`
  block_01  L1633-1656  `if (dep_vec) {`

## operate_cache  L1659-1667
  block_00  L1661-1661  `ITLB.operate();`
  block_01  L1662-1662  `DTLB.operate();`
  block_02  L1663-1663  `STLB.operate();`
  block_03  L1664-1664  `L1I.operate();`
  block_04  L1665-1665  `L1D.operate();`
  block_05  L1666-1666  `L2C.operate();`

## update_rob  L1669-1728
  block_00  L1671-1672  `if (ITLB.PROCESSED.occupancy && (CYC_LE(ITLB.PROCESSED.entry`
  block_01  L1674-1675  `if (L1I.PROCESSED.occupancy && (CYC_LE(L1I.PROCESSED.entry[L`
  block_02  L1677-1678  `if (DTLB.PROCESSED.occupancy && (CYC_LE(DTLB.PROCESSED.entry`
  block_03  L1680-1681  `if (L1D.PROCESSED.occupancy && (CYC_LE(L1D.PROCESSED.entry[L`
  block_04  L1682-1694  `#ifdef BYPASS_L1_LOGIC`
  block_05  L1698-1698  `// update ROB entries with completed executions — bitset ski`
  block_06  L1699-1727  `if ((inflight_reg_executions > 0) || (inflight_mem_execution`

## complete_instr_fetch  L1730-1777
  block_00  L1732-1734  `uint32_t index = queue->head,`
  block_01  L1736-1744  `#ifdef TRUE_SANITY_CHECK`
  block_02  L1746-1746  `// update ROB entry`
  block_03  L1747-1754  `if (is_it_tlb) {`
  block_04  L1755-1755  `rob_events.per_cpu[cpu].event_cycle[rob_index] = PACK_CYCLE(`
  block_05  L1756-1756  `num_fetched++;`
  block_06  L1758-1758  `// check if other instructions were merged`
  block_07  L1759-1773  `if (queue->entry[index].instr_merged) {`
  block_08  L1775-1775  `// remove this entry`
  block_09  L1776-1776  `queue->remove_queue(&queue->entry[index]);`

## complete_data_fetch  L1779-1869
  block_00  L1781-1781  `uint32_t index = queue->head;`
  block_01  L1782-1782  `uint32_t rob_index = queue->entry[index].rob_index;`
  block_02  L1783-1783  `uint32_t sq_index = queue->entry[index].sq_index;`
  block_03  L1784-1784  `uint32_t lq_index = queue->entry[index].lq_index;`
  block_04  L1786-1796  `#ifdef TRUE_SANITY_CHECK`
  block_05  L1798-1798  `// update ROB entry`
  block_06  L1799-1866  `if (is_it_tlb) { // DTLB`
  block_07  L1867-1867  `// remove this entry`
  block_08  L1868-1868  `queue->remove_queue(&queue->entry[index]);`

## handle_merged_translation  L1934-1966
  block_00  L1936-1948  `if (provider->store_merged) {`
  block_01  L1949-1965  `if (provider->load_merged) {`

## handle_merged_load  L1968-2006
  block_00  L1970-1970  `ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE`
  block_01  L1970-2005  `ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE`

## release_load_queue  L2008-2023
  block_00  L2009-2015  `#ifdef USE_HERMES`
  block_01  L2016-2016  `// release LQ entries`
  block_02  L2017-2019  `if (LQ.entry[lq_index].producer_id != UINT64_MAX) {`
  block_03  L2020-2020  `LQ.entry[lq_index].quickReset();`
  block_04  L2021-2021  `free_LQueue.free_lq(cpu, lq_index);`
  block_05  L2022-2022  `LQ.occupancy--;`

## retire_rob  L2034-2129
  block_00  L2036-2128  `for (uint32_t n=0; n<RETIRE_WIDTH; n++) {`
