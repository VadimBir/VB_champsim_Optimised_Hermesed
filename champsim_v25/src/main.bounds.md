# champsim_refactor/src/main.cc — function bounds

## FinalStatsCollector::collectROIStats  L126-L159
  block_01  L135-L159  `std::string coreCachePrefix = "Core_" + std::to_string(cpu) + "_" + cache->NAME + "_";`

## FinalStatsCollector::collectSimStats  L163-L186
  block_01  L170-L186  `std::string coreCachePrefix = "Core_" + std::to_string(cpu) + "_" + cache->NAME + "_";`

## FinalStatsCollector::collectBranchStats  L189-L196
  block_01  L192-L196  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_";`

## FinalStatsCollector::collectDRAMStats  L200-L215
  block_01  L208-L215  `std::string chPrefix = "Channel_" + std::to_string(channel) + "_";`

## FinalStatsCollector::collectPageFaultStats  L219-L223
  block_01  L220-L223  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_";`

## FinalStatsCollector::collectCoreROIStats  L227-L236
  block_01  L232-L236  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_";`

## FinalStatsCollector::dumpAllAsString  L241-L252
  block_01  L243-L244  `std::ostringstream oss;`
  block_02  L245-L249  `for (const auto& kv : data) {`
  block_03  L250-L252  `oss << "}"; return oss.str();`

## record_roi_stats  L290-L300
  block_01  L291-L295  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_02  L296-L299  `cache->roi_access_wByP[cpu] = cache->sim_access_wByP[cpu];`

## print_pf_hitRatio  L301-L307
  block_01  L306-L306  `return ((double)cache->sim_hit[cpu][2]/(double)cache->sim_access[cpu][2]);`

## print_L2_hitRatio  L308-L315
  block_01  L309-L313  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0;`
  block_02  L314-L314  `return ((double)TOTAL_HIT/(double)TOTAL_ACCESS);`

## print_L2_usefulRatio  L316-L318
  block_01  L317-L317  `return ((double)cache->pf_useful/(double)cache->pf_issued);`

## print_roi_stats  L321-L417
  block_01  L322-L328  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;`
  block_02  L324-L328  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_03  L329-L391  `cout << "Core_;" << cpu << ";" << std::right << setw(4) << cache->NAME`
  block_04  L392-L416  `statsCollector.collectROIStats(`

## print_sim_stats  L419-L463
  block_01  L420-L426  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;`
  block_02  L422-L426  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_03  L428-L443  `cout<< "Core_;" << cpu << ";_;" << cache->NAME`
  block_04  L444-L462  `statsCollector.collectSimStats(`

## print_branch_stats  L465-L477
  block_01  L467-L470  `cout << "Core_;" << cpu << ";_;branch_prediction_accuracy;"`
  block_02  L472-L476  `statsCollector.collectBranchStats(`

## print_dram_stats  L479-L519
  block_01  L482-L499  `for (uint32_t i=0; i<DRAM_CHANNELS; i++) {`
  block_02  L502-L504  `uint64_t total_congested_cycle = 0;`
  block_03  L503-L504  `for (uint32_t i=0; i<DRAM_CHANNELS; i++)`
  block_04  L505-L516  `if (uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]) {`
  block_05  L517-L518  `cout << "OffChipPred\t;" << HERMES_LABEL << ";" << endl;`

## reset_cache_stats  L521-L557
  block_01  L523-L534  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_02  L536-L546  `cache->total_miss_latency = 0;`
  block_03  L548-L556  `cache->RQ.ACCESS = 0;`

## finish_warmup  L559-L612
  block_01  L561-L565  `uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),`
  block_02  L567-L571  `SCHEDULING_LATENCY = 6;`
  block_03  L573-L591  `cout << endl;`
  block_04  L595-L600  `for (uint32_t i=0; i<DRAM_CHANNELS; i++) {`
  block_05  L603-L611  `for (uint32_t i=0; i<NUM_CPUS; i++) {`

## print_deadlock  L615-L909
  block_01  L667-L668  `{ uint16_t _hd = ooo_cpu[i].ROB.head;`
  block_02  L669-L679  `if (PCYCLE_LE(...) && _dd%DEADLOCK_CYCLE==0) {`
  block_03  L683-L724  `PACKET_QUEUE *queue; queue = &ooo_cpu[i].L1D.MSHR;`
  block_04  L726-L765  `queue = &ooo_cpu[i].L2C.MSHR;`
  block_05  L768-L781  `cout << endl << "Load Queue Entry" << endl;`
  block_06  L783-L797  `cout << endl << "Store Queue Entry" << endl;`
  block_07  L859-L863  `int num_store = 0;`
  block_08  L870-L871  `cout << " num_store: " << num_store << endl;`
  block_09  L906-L908  `} } assert(0);`

## signal_handler  L911-L918
  block_01  L913-L917  `cout << "Caught signal: " << signal << endl;`

## va_to_pa  L923-L1102
  block_01  L924-L927  `#ifdef SANITY_CHECK if (va == 0) assert(0...`
  block_02  L929-L934  `uint8_t  swap = 0; uint64_t high_bit_mask = rotr64(...`
  block_03  L944-L954  `ankerl::unordered_dense::map ... cl_check = unique_cl[cpu].find(...`
  block_04  L956-L957  `pr = page_table.find(vpage);`
  block_05  L958-L1075  `if (pr == page_table.end()) {`
  block_06  L1080-L1084  `pr = page_table.find(vpage); #ifdef SANITY_CHECK`
  block_07  L1085-L1088  `uint64_t ppage = pr->second; uint64_t pa = ppage << LOG2_PAGE_SIZE;`
  block_08  L1094-L1097  `if (swap) stall_cycle[cpu] = ...; else stall_cycle[cpu] = ...;`
  block_09  L1101-L1101  `return pa;`

## print_knobs  L1104-L1133
  block_01  L1106-L1112  `cout << "warmup_instructions " << warmup_instructions << endl`
  block_02  L1113-L1129  `cout << "num_cpus\t;" << NUM_CPUS<<";" << endl`
  block_03  L1130-L1132  `print_core_config(); print_dram_config();`

## main  L1136-L1866
  block_01  L1151-L1174  `ChampsimDBConfig db_cfg = {};`
  block_02  L1179-L1188  `signal(SIGINT, signal_handler); / sigaction(SIGINT, ...`
  block_03  L1196-L1199  `fprintf(stdout, "***...");`
  block_04  L1203-L1287  `uint8_t show_heartbeat = 1; while (1) { getopt_long_only`
  block_05  L1289-L1303  `if (knob_low_bandwidth) DRAM_MTPS = ...; tRP = ...; tRCD = ...; tCAS = ...;`
  block_06  L1308-L1411  `int found_traces = 0; for (int i=0; i<argc; i++) {`
  block_07  L1416-L1419  `if (count_traces != NUM_CPUS) assert(0...`
  block_08  L1423-L1530  `srand(seed_number); for (int i=0; i<NUM_CPUS; i++) { ooo_cpu init`
  block_09  L1531-L1549  `#ifdef BYPASS_DEBUG ... uncore.LLC.llc_initialize_replacement();`
  block_10  L1551-L1556  `if (false) { ... } else { print_knobs(); }`
  block_11  L1561-L1575  `start_time = time(NULL); #ifdef USE_TRACE_HELPER trace_helper.start`
  block_12  L1577-L1765  `uint8_t run_simulation = 1; while (run_simulation) {`
  block_13  L1776-L1835  `uint64_t elapsed_second = ...; cout << "ChampSim completed"; [ROI stats loop]`
  block_14  L1837-L1864  `prefetcher_final_stats; print_dram_stats(); execution_checksum; db_store`
  block_15  L1865-L1865  `return 0;`
