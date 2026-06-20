# dram_controller.cc — function bounds

## print_dram_config  L12-27
  block_00  L14-26  `cout << std::right << setw(16) << "dram_chnl_width;" << std:`

## reset_remain_requests  L29-88
  block_00  L31-71  `for (uint32_t i=0; i<queue->SIZE; i++) {`
  block_01  L73-73  `update_schedule_cycle(&RQ[channel]);`
  block_02  L74-74  `update_schedule_cycle(&WQ[channel]);`
  block_03  L75-75  `update_process_cycle(&RQ[channel]);`
  block_04  L76-76  `update_process_cycle(&WQ[channel]);`
  block_05  L78-87  `#ifdef SANITY_CHECK`

## operate  L90-238
  block_00  L91-91  `{ //TIME_it("      MM Op");`
  block_01  L93-93  `/* LPM tick for DRAM — one tick per CPU per cycle */`
  block_02  L94-94  `// A1: once all_warmup_complete latches, skip per-cpu warmup`
  block_03  L95-166  `if (all_warmup_complete) {`
  block_04  L168-237  `for (uint32_t i=0; i<DRAM_CHANNELS; i++) {`

## schedule  L240-389
  block_00  L242-242  `uint64_t read_addr;`
  block_01  L243-243  `uint32_t read_channel, read_rank, read_bank, read_row;`
  block_02  L244-244  `uint8_t  row_buffer_hit = 0;`
  block_03  L246-246  `int oldest_index = -1;`
  block_04  L247-247  `uint32_t oldest_cycle = UINT32_MAX;`
  block_05  L249-249  `// first, search for the oldest open row hit`
  block_06  L250-300  `for (uint32_t i=0; i<queue->SIZE; i++) {`
  block_07  L302-332  `if (oldest_index == -1) { // no matching open_row (row buffe`
  block_08  L334-334  `// at this point, the scheduler knows which bank to access a`
  block_09  L335-388  `if (oldest_index != -1) { // scheduler might not find anythi`

## process  L391-610
  block_00  L393-393  `uint32_t request_index = queue->next_process_index;`
  block_01  L395-395  `// sanity check`
  block_02  L396-399  `#ifdef DRAM_SANITY_CHECK`
  block_03  L401-401  `uint8_t  op_type = queue->entry[request_index].type;`
  block_04  L402-402  `uint64_t op_addr = queue->entry[request_index].address;`
  block_05  L403-406  `uint32_t op_cpu = queue->entry[request_index].cpu,`
  block_06  L407-410  `#ifdef DEBUG_PRINT`
  block_07  L411-411  `// update_process_cycle picks the earliest-event-cycle sched`
  block_08  L412-412  `// but the bank may be servicing a different entry at the sa`
  block_09  L413-413  `// If mismatch: this entry cannot proceed now. Re-scan for a`
  block_10  L414-432  `if (bank_request[op_channel][op_rank][op_bank].request_index`
  block_11  L434-434  `// paid all DRAM access latency, data is ready to be process`
  block_12  L435-609  `if (CYC_LE(bank_hot.entries[op_channel][op_rank][op_bank].cy`

## add_rq  L612-838
  block_00  L614-617  `#ifdef BYPASS_DEBUG`
  block_01  L619-619  `// simply return read requests with dummy response before th`
  block_02  L620-680  `if (all_warmup_complete < NUM_CPUS) {`
  block_03  L682-682  `// check for the latest wirtebacks in the write queue`
  block_04  L683-683  `uint32_t channel = dram_get_channel(packet->address);`
  block_05  L684-684  `int wq_index = check_dram_queue(&WQ[channel], packet);`
  block_06  L686-766  `if (wq_index != -1) {`
  block_07  L768-768  `// check for duplicates in the read queue`
  block_08  L769-769  `int index = check_dram_queue(&RQ[channel], packet);`
  block_09  L770-776  `if (index != -1) {`
  block_10  L778-778  `//     // search for the empty index`
  block_11  L779-779  `//     for (index=0; index<DRAM_RQ_SIZE; index++) {`
  block_12  L780-780  `//         if (RQ[channel].entry[index].address == 0) {`
  block_13  L782-782  `//             RQ[channel].entry[index]= * packet;`
  block_14  L783-783  `//             RQ[channel].occupancy++;`
  block_15  L785-785  `// #ifdef DEBUG_PRINT`
  block_16  L786-786  `//             uint32_t channel = dram_get_channel(packet->a`
  block_17  L787-787  `//                      rank = dram_get_rank(packet->address`
  block_18  L788-788  `//                      bank = dram_get_bank(packet->address`
  block_19  L789-789  `//                      row = dram_get_row(packet->address),`
  block_20  L790-790  `//                      column = dram_get_column(packet->add`
  block_21  L791-791  `// #endif`
  block_22  L793-793  `//             // DP ( if(warmup_complete[packet->cpu]) {`
  block_23  L794-794  `//             // cout << "[" << NAME << "_RQ] " <<  __func_`
  block_24  L795-795  `//             // cout << " full_addr: " << packet->full_add`
  block_25  L796-796  `//             // cout << " rank: " << rank << " bank: " << `
  block_26  L797-797  `//             // cout << " occupancy: " << RQ[channel].occu`
  block_27  L799-799  `//             break;`
  block_28  L800-800  `//         }`
  block_29  L801-801  `//     }`
  block_30  L804-804  `//     update_schedule_cycle(&RQ[channel]);`
  block_31  L806-806  `//     return -1;`
  block_32  L808-808  `// search for the empty index`
  block_33  L809-833  `for (index=0; index<DRAM_RQ_SIZE; index++) {`
  block_34  L835-835  `// CRITICAL FIX: No empty slot found - queue full`
  block_35  L836-836  `RQ[channel].FULL++;`
  block_36  L837-837  `return -2;`

## add_wq  L840-912
  block_00  L842-842  `// simply drop write requests before the warmup`
  block_01  L843-844  `if (all_warmup_complete < NUM_CPUS)`
  block_02  L846-846  `// check for duplicates in the write queue`
  block_03  L847-847  `uint32_t channel = dram_get_channel(packet->address);`
  block_04  L848-848  `int index = check_dram_queue(&WQ[channel], packet);`
  block_05  L850-851  `if (index != -1)`
  block_06  L851-851  `return index; // merged index`
  block_07  L853-853  `//     // search for the empty index`
  block_08  L854-854  `//     for (index=0; index<DRAM_WQ_SIZE; index++) {`
  block_09  L855-855  `//         if (WQ[channel].entry[index].address == 0) {`
  block_10  L857-857  `//             WQ[channel].entry[index]= * packet;`
  block_11  L858-858  `//             WQ[channel].occupancy++;`
  block_12  L860-860  `// #ifdef DEBUG_PRINT`
  block_13  L861-861  `//             uint32_t channel = dram_get_channel(packet->a`
  block_14  L862-862  `//                      rank = dram_get_rank(packet->address`
  block_15  L863-863  `//                      bank = dram_get_bank(packet->address`
  block_16  L864-864  `//                      row = dram_get_row(packet->address),`
  block_17  L865-865  `//                      column = dram_get_column(packet->add`
  block_18  L866-866  `// #endif`
  block_19  L868-868  `//             // DP ( if(warmup_complete[packet->cpu]) {`
  block_20  L869-869  `//             // cout << "[" << NAME << "_WQ] " <<  __func_`
  block_21  L870-870  `//             // cout << " full_addr: " << packet->full_add`
  block_22  L871-871  `//             // cout << " rank: " << rank << " bank: " << `
  block_23  L872-872  `//             // cout << " occupancy: " << WQ[channel].occu`
  block_24  L874-874  `//             break;`
  block_25  L875-875  `//         }`
  block_26  L876-876  `//     }`
  block_27  L878-878  `//     update_schedule_cycle(&WQ[channel]);`
  block_28  L880-880  `//     return -1;`
  block_29  L882-882  `// search for the empty index`
  block_30  L883-907  `for (index=0; index<DRAM_WQ_SIZE; index++) {`
  block_31  L909-909  `// CRITICAL FIX: No empty slot found - queue full`
  block_32  L910-910  `WQ[channel].FULL++;`
  block_33  L911-911  `return -2;`

## add_pq  L914-917
  block_00  L916-916  `return -1;`

## return_data  L919-922

## update_schedule_cycle  L924-954
  block_00  L926-926  `// update next_schedule_cycle`
  block_01  L927-927  `uint32_t min_cycle = UINT32_MAX;`
  block_02  L928-928  `uint32_t min_index = queue->SIZE;`
  block_03  L929-942  `for (uint32_t i=0; i<queue->SIZE; i++) {`
  block_04  L944-944  `queue->next_schedule_cycle = min_cycle;`
  block_05  L945-945  `queue->next_schedule_index = min_index;`
  block_06  L946-953  `if (min_index < queue->SIZE) {`

## update_process_cycle  L956-978
  block_00  L958-958  `// update next_process_cycle`
  block_01  L959-959  `uint32_t min_cycle = UINT32_MAX;`
  block_02  L960-960  `uint32_t min_index = queue->SIZE;`
  block_03  L961-966  `for (uint32_t i=0; i<queue->SIZE; i++) {`
  block_04  L968-968  `queue->next_process_cycle = min_cycle;`
  block_05  L969-969  `queue->next_process_index = min_index;`
  block_06  L970-977  `if (min_index < queue->SIZE) {`

## check_dram_queue  L980-1093
  block_00  L982-982  `// search write queue`
  block_01  L983-1010  `for (uint32_t index=0; index<queue->SIZE; index++) {`
  block_02  L1012-1012  `// constexpr uint32_t CACHE::get_set(const uint64_t address)`
  block_03  L1013-1013  `//     return (uint32_t) (address & ((1 << lg2(NUM_SET)) - 1`
  block_04  L1014-1014  `// }`
  block_05  L1017-1025  `constexpr uint32_t MEMORY_CONTROLLER::dram_get_channel(uint6`
  block_06  L1027-1035  `constexpr uint32_t MEMORY_CONTROLLER::dram_get_bank(uint64_t`
  block_07  L1037-1045  `constexpr uint32_t MEMORY_CONTROLLER::dram_get_column(uint64`
  block_08  L1047-1055  `constexpr uint32_t MEMORY_CONTROLLER::dram_get_rank(uint64_t`
  block_09  L1057-1065  `constexpr uint32_t MEMORY_CONTROLLER::dram_get_row(uint64_t `
  block_10  L1067-1076  `uint32_t MEMORY_CONTROLLER::get_occupancy(uint8_t queue_type`
  block_11  L1078-1087  `uint32_t MEMORY_CONTROLLER::get_size(uint8_t queue_type, uin`
  block_12  L1089-1093  `void MEMORY_CONTROLLER::increment_WQ_FULL(uint64_t address)`

## dram_get_channel  L1017-1025
  block_00  L1019-1020  `if (LOG2_DRAM_CHANNELS == 0)`
  block_01  L1022-1022  `int shift = 0;`
  block_02  L1024-1024  `return (uint32_t) (address >> shift) & (DRAM_CHANNELS - 1);`

## dram_get_bank  L1027-1035
  block_00  L1029-1030  `if (LOG2_DRAM_BANKS == 0)`
  block_01  L1032-1032  `int shift = LOG2_DRAM_CHANNELS;`
  block_02  L1034-1034  `return (uint32_t) (address >> shift) & (DRAM_BANKS - 1);`

## dram_get_column  L1037-1045
  block_00  L1039-1040  `if (LOG2_DRAM_COLUMNS == 0)`
  block_01  L1042-1042  `int shift = LOG2_DRAM_BANKS + LOG2_DRAM_CHANNELS;`
  block_02  L1044-1044  `return (uint32_t) (address >> shift) & (DRAM_COLUMNS - 1);`

## dram_get_rank  L1047-1055
  block_00  L1049-1050  `if (LOG2_DRAM_RANKS == 0)`
  block_01  L1052-1052  `int shift = LOG2_DRAM_COLUMNS + LOG2_DRAM_BANKS + LOG2_DRAM_`
  block_02  L1054-1054  `return (uint32_t) (address >> shift) & (DRAM_RANKS - 1);`

## dram_get_row  L1057-1065
  block_00  L1059-1060  `if (LOG2_DRAM_ROWS == 0)`
  block_01  L1062-1062  `int shift = LOG2_DRAM_RANKS + LOG2_DRAM_COLUMNS + LOG2_DRAM_`
  block_02  L1064-1064  `return (uint32_t) (address >> shift) & (DRAM_ROWS - 1);`

## get_occupancy  L1067-1076
  block_00  L1069-1069  `uint32_t channel = dram_get_channel(address);`
  block_01  L1070-1073  `if (queue_type == 1)`
  block_02  L1075-1075  `return 0;`

## get_size  L1078-1087
  block_00  L1080-1080  `uint32_t channel = dram_get_channel(address);`
  block_01  L1081-1084  `if (queue_type == 1)`
  block_02  L1086-1086  `return 0;`

## increment_WQ_FULL  L1089-1093
  block_00  L1091-1091  `uint32_t channel = dram_get_channel(address);`
  block_01  L1092-1092  `WQ[channel].FULL++;`
