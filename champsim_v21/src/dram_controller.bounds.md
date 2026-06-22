# champsim_refactor/src/dram_controller.cc — function bounds

## print_dram_config  L80-95
  block_01  L82-94  `cout << std::right << setw(16) << "dram_chnl_width;"`

## MEMORY_CONTROLLER::reset_remain_requests  L97-158
  block_01  L99-141  `for (uint32_t i=0; i<queue->SIZE; i++) {`
  block_02  L143-146  `update_schedule_cycle(&RQ[channel]);`
  block_03  L148-157  `#ifdef SANITY_CHECK`

## MEMORY_CONTROLLER::operate  L160-350
  block_01  L165-221  `if (all_warmup_complete) {`
  block_02  L223-253  `for (uint32_t i=0; i<DRAM_CHANNELS; i++) {  // write-mode watermark flip`
  block_03  L257-271  `if (write_mode[i] && (WQ[i].next_schedule_index < WQ[i].SIZE)) {`
  block_04  L274-288  `if (write_mode[i] && (WQ[i].next_process_index < WQ[i].SIZE)) {`
  block_05  L299-321  `if ((write_mode[i] == 0) && (RQ[i].next_schedule_index < RQ[i].SIZE)) {`
  block_06  L328-348  `if ((write_mode[i] == 0) && (RQ[i].next_process_index < RQ[i].SIZE)) {`

## MEMORY_CONTROLLER::schedule  L352-524
  block_01  L360-362  `uint32_t _scheduled = queue->is_WQ ? scheduled_writes[channel] : scheduled_reads[channel];`
  block_02  L364-369  `uint64_t read_addr; ... int oldest_index = -1;`
  block_03  L372-424  `for (uint32_t i=0; i<queue->SIZE; i++) {  // open-row-hit scan`
  block_04  L426-463  `if (oldest_index == -1) {  // row-miss scan`
  block_05  L466-523  `if (oldest_index != -1) {  // dispatch selected entry to bank`

## MEMORY_CONTROLLER::process  L526-812
  block_01  L528-542  `uint32_t request_index = queue->next_process_index;`
  block_02  L551-583  `if (bank_request[op_channel][op_rank][op_bank].request_index != (int)request_index) {`
  block_03  L586-589  `if (CYC_LE(bank_cycle_available[op_channel][op_rank][op_bank], CYC(current_core_cycle[op_cpu]))) {`
  block_04  L591-612  `if (queue->is_WQ) {  // WQ write-complete path`
  block_05  L614-739  `} else {  // RQ read-complete: Hermes / LLC-bypass / normal paths`
  block_06  L742-747  `if (queue->is_WQ) rw_occ[op_cpu].wq--; else rw_occ[op_cpu].rq--;`
  block_07  L749-811  `else { // data bus busy or bank still working`

## MEMORY_CONTROLLER::add_rq  L814-1180
  block_01  L821-828  `{  // DP top-of-function address decompose + trace`
  block_02  L831-903  `if (all_warmup_complete < NUM_CPUS) {  // pre-warmup fast-return`
  block_03  L906-996  `uint32_t channel = dram_get_channel(packet->address);  // WQ-forward check`
  block_04  L998-1096  `int index = check_dram_queue(&RQ[channel], packet);  // RQ dup check / merge`
  block_05  L1129-1161  `for (index=0; index<DRAM_RQ_SIZE; index++) {  // find empty slot`
  block_06  L1163-1179  `RQ[channel].FULL++;  // queue full path`

## MEMORY_CONTROLLER::add_wq  L1182-1273
  block_01  L1184-1191  `{  // DP top-of-function trace`
  block_02  L1193-1195  `if (all_warmup_complete < NUM_CPUS)  // pre-warmup drop`
  block_03  L1197-1206  `uint32_t channel = dram_get_channel(packet->address);  // dup check`
  block_04  L1237-1261  `for (index=0; index<DRAM_WQ_SIZE; index++) {  // find empty slot`
  block_05  L1263-1272  `WQ[channel].FULL++;  // queue full path`

## MEMORY_CONTROLLER::add_pq  L1275-1278
  block_01  L1277-1277  `return -1;`

## MEMORY_CONTROLLER::return_data  L1280-1283
  block_01  L1282-1282  `(empty body)`

## MEMORY_CONTROLLER::update_schedule_cycle  L1285-1310
  block_01  L1288-1295  `uint64_t min_cycle = CYC_PACKED_MAX;  uint32_t min_index = queue->SIZE;`
  block_02  L1290-1295  `for (uint32_t i=0; i<queue->SIZE; i++) {  // find min unscheduled event_cycle`
  block_03  L1297-1309  `queue->next_schedule_cycle = min_cycle;  // write result + DP trace`

## MEMORY_CONTROLLER::update_process_cycle  L1312-1337
  block_01  L1315-1322  `uint64_t min_cycle = CYC_PACKED_MAX;  uint32_t min_index = queue->SIZE;`
  block_02  L1317-1322  `for (uint32_t i=0; i<queue->SIZE; i++) {  // find min scheduled event_cycle`
  block_03  L1324-1336  `queue->next_process_cycle = min_cycle;  // write result + DP trace`

## MEMORY_CONTROLLER::check_dram_queue  L1339-1366
  block_01  L1342-1359  `for (uint32_t index=0; index<queue->SIZE; index++) {  // address match scan`
  block_02  L1361-1365  `DP( if (...) { cout << ... } ); return -1;`

## MEMORY_CONTROLLER::dram_get_channel  L1373-1381
  block_01  L1375-1376  `if (LOG2_DRAM_CHANNELS == 0) return 0;`
  block_02  L1378-1380  `int shift = 0; return (address >> shift) & (DRAM_CHANNELS - 1);`

## MEMORY_CONTROLLER::dram_get_bank  L1383-1391
  block_01  L1385-1386  `if (LOG2_DRAM_BANKS == 0) return 0;`
  block_02  L1388-1390  `int shift = LOG2_DRAM_CHANNELS; return (address >> shift) & (DRAM_BANKS - 1);`

## MEMORY_CONTROLLER::dram_get_column  L1393-1401
  block_01  L1395-1396  `if (LOG2_DRAM_COLUMNS == 0) return 0;`
  block_02  L1398-1400  `int shift = LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;`

## MEMORY_CONTROLLER::dram_get_rank  L1403-1411
  block_01  L1405-1406  `if (LOG2_DRAM_RANKS == 0) return 0;`
  block_02  L1408-1410  `int shift = LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;`

## MEMORY_CONTROLLER::dram_get_row  L1413-1421
  block_01  L1415-1416  `if (LOG2_DRAM_ROWS == 0) return 0;`
  block_02  L1418-1420  `int shift = LOG2_DRAM_RANKS + LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;`

## MEMORY_CONTROLLER::get_occupancy  L1423-1432
  block_01  L1425-1431  `uint32_t channel = dram_get_channel(address); if/else return RQ/WQ.occupancy;`

## MEMORY_CONTROLLER::get_size  L1434-1443
  block_01  L1436-1442  `uint32_t channel = dram_get_channel(address); if/else return RQ/WQ.SIZE;`

## MEMORY_CONTROLLER::increment_WQ_FULL  L1445-1449
  block_01  L1447-1448  `uint32_t channel = dram_get_channel(address); WQ[channel].FULL++;`
