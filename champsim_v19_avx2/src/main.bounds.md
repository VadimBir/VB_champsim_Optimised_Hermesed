# main.cc — function bounds

## collectROIStats  L114-147
  block_00  L124-124  `// Use the same printed keys:  "Core_{cpu}_{cache->NAME}_tot`
  block_01  L125-125  `std::string coreCachePrefix = "Core_" + std::to_string(cpu) `
  block_02  L126-126  `data[coreCachePrefix + "total_access"]        = (double)TOTA`
  block_03  L127-127  `data[coreCachePrefix + "total_hit"]           = (double)TOTA`
  block_04  L128-128  `data[coreCachePrefix + "total_miss"]          = (double)TOTA`
  block_05  L129-129  `data[coreCachePrefix + "loads"]               = (double)load`
  block_06  L130-130  `data[coreCachePrefix + "load_hit"]            = (double)load`
  block_07  L131-131  `data[coreCachePrefix + "load_miss"]           = (double)load`
  block_08  L132-132  `data[coreCachePrefix + "RFOs"]                = (double)RFOs`
  block_09  L133-133  `data[coreCachePrefix + "RFO_hit"]             = (double)RFO_`
  block_10  L134-134  `data[coreCachePrefix + "RFO_miss"]            = (double)RFO_`
  block_11  L135-135  `data[coreCachePrefix + "prefetches"]          = (double)pref`
  block_12  L136-136  `data[coreCachePrefix + "prefetch_hit"]        = (double)pref`
  block_13  L137-137  `data[coreCachePrefix + "prefetch_miss"]       = (double)pref`
  block_14  L138-138  `data[coreCachePrefix + "writebacks"]          = (double)writ`
  block_15  L139-139  `data[coreCachePrefix + "writeback_hit"]       = (double)writ`
  block_16  L140-140  `data[coreCachePrefix + "writeback_miss"]      = (double)writ`
  block_17  L141-141  `data[coreCachePrefix + "prefetch_requested"]  = (double)pf_r`
  block_18  L142-142  `data[coreCachePrefix + "prefetch_issued"]     = (double)pf_i`
  block_19  L143-143  `data[coreCachePrefix + "prefetch_useful"]     = (double)pf_u`
  block_20  L144-144  `data[coreCachePrefix + "prefetch_useless"]    = (double)pf_u`
  block_21  L145-145  `data[coreCachePrefix + "prefetch_late"]       = (double)pf_l`
  block_22  L146-146  `data[coreCachePrefix + "average_miss_latency"] = average_mis`

## collectSimStats  L151-174
  block_00  L158-158  `std::string coreCachePrefix = "Core_" + std::to_string(cpu) `
  block_01  L159-159  `data[coreCachePrefix + "total_access"]   = (double)TOTAL_ACC`
  block_02  L160-160  `data[coreCachePrefix + "total_hit"]      = (double)TOTAL_HIT`
  block_03  L161-161  `data[coreCachePrefix + "total_miss"]     = (double)TOTAL_MIS`
  block_04  L162-162  `data[coreCachePrefix + "loads"]          = (double)loads;`
  block_05  L163-163  `data[coreCachePrefix + "load_hit"]       = (double)load_hit;`
  block_06  L164-164  `data[coreCachePrefix + "load_miss"]      = (double)load_miss`
  block_07  L165-165  `data[coreCachePrefix + "RFOs"]           = (double)RFOs;`
  block_08  L166-166  `data[coreCachePrefix + "RFO_hit"]        = (double)RFO_hit;`
  block_09  L167-167  `data[coreCachePrefix + "RFO_miss"]       = (double)RFO_miss;`
  block_10  L168-168  `data[coreCachePrefix + "prefetches"]     = (double)prefetche`
  block_11  L169-169  `data[coreCachePrefix + "prefetch_hit"]   = (double)prefetch_`
  block_12  L170-170  `data[coreCachePrefix + "prefetch_miss"]  = (double)prefetch_`
  block_13  L171-171  `data[coreCachePrefix + "writebacks"]     = (double)writeback`
  block_14  L172-172  `data[coreCachePrefix + "writeback_hit"]  = (double)writeback`
  block_15  L173-173  `data[coreCachePrefix + "writeback_miss"] = (double)writeback`

## collectBranchStats  L177-184
  block_00  L180-180  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_"`
  block_01  L181-181  `data[corePrefix + "branch_prediction_accuracy"]          = p`
  block_02  L182-182  `data[corePrefix + "branch_MPKI"]                         = b`
  block_03  L183-183  `data[corePrefix + "average_ROB_occupancy_at_mispredict"] = a`

## collectDRAMStats  L188-203
  block_00  L196-196  `std::string chPrefix = "Channel_" + std::to_string(channel) `
  block_01  L197-197  `data[chPrefix + "RQ_row_buffer_hit"]   = (double)RQ_row_buff`
  block_02  L198-198  `data[chPrefix + "RQ_row_buffer_miss"]  = (double)RQ_row_buff`
  block_03  L199-199  `data[chPrefix + "WQ_row_buffer_hit"]   = (double)WQ_row_buff`
  block_04  L200-200  `data[chPrefix + "WQ_row_buffer_miss"]  = (double)WQ_row_buff`
  block_05  L201-201  `data[chPrefix + "WQ_full"]             = (double)WQ_full;`
  block_06  L202-202  `data[chPrefix + "dbus_congested"]      = (double)dbus_conges`

## collectPageFaultStats  L207-211
  block_00  L208-208  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_"`
  block_01  L209-209  `data[corePrefix + "major_page_fault"] = (double)major_fault_`
  block_02  L210-210  `data[corePrefix + "minor_page_fault"] = (double)minor_fault_`

## collectCoreROIStats  L215-224
  block_00  L220-220  `std::string corePrefix = "Core_" + std::to_string(cpu) + "_"`
  block_01  L221-221  `data[corePrefix + "instructions"] = (double)finish_sim_instr`
  block_02  L222-222  `data[corePrefix + "cycles"]       = (double)finish_sim_cycle`
  block_03  L223-223  `data[corePrefix + "IPC"]          = finalIPC;`

## dumpAllAsString  L229-240
  block_00  L230-230  `std::ostringstream oss;`
  block_01  L231-231  `oss << std::fixed << std::setprecision(5) << "{";`
  block_02  L232-232  `bool first = true;`
  block_03  L233-237  `for (const auto& kv : data) {`
  block_04  L238-238  `oss << "}";`
  block_05  L239-239  `return oss.str();`

## record_roi_stats  L278-288
  block_00  L279-283  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_01  L284-284  `cache->roi_access_wByP[cpu] = cache->sim_access_wByP[cpu];`
  block_02  L285-285  `cache->roi_hit_wByP[cpu]    = cache->sim_hit_wByP[cpu];`
  block_03  L286-286  `cache->roi_miss_wByP[cpu]   = cache->sim_miss_wByP[cpu];`
  block_04  L287-287  `cache->roi_byp_wByP[cpu]    = cache->sim_byp_wByP[cpu];`

## print_pf_hitRatio  L289-295
  block_00  L290-290  `//cout<< "Core_" << cpu << "_" << cache->NAME << "_prefetch_`
  block_01  L292-292  `//(double)cache->roi_hit[cpu][2]/(double)cache->roi_access[c`
  block_02  L293-293  `//cout<<"PfHitRatio: "<<(double)cache->pf_useful/(double)cac`
  block_03  L294-294  `return ((double)cache->sim_hit[cpu][2]/(double)cache->sim_ac`

## print_L2_hitRatio  L296-303
  block_00  L297-297  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0;`
  block_01  L298-301  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_02  L302-302  `return ((double)TOTAL_HIT/(double)TOTAL_ACCESS);`

## print_L2_usefulRatio  L304-306
  block_00  L305-305  `return ((double)cache->pf_useful/(double)cache->pf_issued);`

## print_roi_stats  L309-401
  block_00  L310-310  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;`
  block_01  L312-316  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_02  L317-375  `cout << "Core_;" << cpu << ";" << std::right << setw(4) << c`
  block_03  L376-400  `statsCollector.collectROIStats(`

## print_sim_stats  L403-447
  block_00  L404-404  `uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;`
  block_01  L406-410  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_02  L412-427  `cout<< "Core_;" << cpu << ";_;" << cache->NAME << ";_total_a`
  block_03  L428-446  `statsCollector.collectSimStats(`

## print_branch_stats  L449-461
  block_00  L450-450  `// for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_01  L451-454  `cout << "Core_;" << cpu << ";_;branch_prediction_accuracy;" `
  block_02  L455-455  `// }`
  block_03  L456-460  `statsCollector.collectBranchStats(    cpu,`

## print_dram_stats  L463-502
  block_00  L464-464  `// cout << endl;`
  block_01  L465-465  `// cout << "DRAM Statistics" << endl;`
  block_02  L466-483  `for (uint32_t i=0; i<DRAM_CHANNELS; i++)`
  block_03  L486-486  `uint64_t total_congested_cycle = 0;`
  block_04  L487-488  `for (uint32_t i=0; i<DRAM_CHANNELS; i++)`
  block_05  L489-500  `if (uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]){`

## reset_cache_stats  L504-539
  block_00  L506-516  `for (uint32_t i=0; i<NUM_TYPES; i++) {`
  block_01  L518-518  `cache->total_miss_latency = 0;`
  block_02  L520-520  `// reset bypass counters at warmup (like sim_miss)`
  block_03  L521-521  `cache->total_ByP_issued[cpu] = 0;`
  block_04  L522-522  `cache->total_ByP_req[cpu] = 0;`
  block_05  L523-523  `cache->ByP_issued[cpu] = 0;`
  block_06  L524-524  `cache->ByP_req[cpu] = 0;`
  block_07  L525-525  `cache->sim_access_wByP[cpu] = 0;`
  block_08  L526-526  `cache->sim_hit_wByP[cpu] = 0;`
  block_09  L527-527  `cache->sim_miss_wByP[cpu] = 0;`
  block_10  L528-528  `cache->sim_byp_wByP[cpu] = 0;`
  block_11  L530-530  `cache->RQ.ACCESS = 0;`
  block_12  L531-531  `cache->RQ.MERGED = 0;`
  block_13  L532-532  `cache->RQ.TO_CACHE = 0;`
  block_14  L534-534  `cache->WQ.ACCESS = 0;`
  block_15  L535-535  `cache->WQ.MERGED = 0;`
  block_16  L536-536  `cache->WQ.TO_CACHE = 0;`
  block_17  L537-537  `cache->WQ.FORWARD = 0;`
  block_18  L538-538  `cache->WQ.FULL = 0;`

## finish_warmup  L541-594
  block_00  L543-545  `uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time`
  block_01  L546-546  `elapsed_minute -= elapsed_hour*60;`
  block_02  L547-547  `elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);`
  block_03  L549-549  `// reset core latency`
  block_04  L550-550  `SCHEDULING_LATENCY = 6;`
  block_05  L551-551  `EXEC_LATENCY = 1;`
  block_06  L552-552  `PAGE_TABLE_LATENCY = 100;`
  block_07  L553-553  `SWAP_LATENCY = 100000;`
  block_08  L555-555  `cout << endl;`
  block_09  L556-573  `for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_10  L574-574  `cout << endl;`
  block_11  L576-576  `// reset DRAM stats`
  block_12  L577-582  `for (uint32_t i=0; i<DRAM_CHANNELS; i++) {`
  block_13  L584-584  `// set actual cache latency`
  block_14  L585-592  `for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_15  L593-593  `uncore.LLC.LATENCY = LLC_LATENCY;`

## signal_handler  L893-897
  block_00  L895-895  `cout << "Caught signal: " << signal << endl;`
  block_01  L896-896  `exit(1);`

## va_to_pa  L902-1081
  block_00  L903-906  `#ifdef SANITY_CHECK`
  block_01  L908-908  `uint8_t  swap = 0;`
  block_02  L909-910  `uint64_t high_bit_mask = rotr64(cpu, lg2(NUM_CPUS)),`
  block_03  L911-911  `//uint64_t vpage = unique_va >> LOG2_PAGE_SIZE,`
  block_04  L912-913  `uint64_t vpage = unique_vpage | high_bit_mask,`
  block_05  L915-915  `// smart random number generator`
  block_06  L916-916  `uint64_t random_ppage;`
  block_07  L918-918  `// map <uint64_t, uint64_t>::iterator pr = page_table.begin(`
  block_08  L919-919  `// map <uint64_t, uint64_t>::iterator ppage_check = inverse_`
  block_09  L921-921  `// // check unique cache line footprint`
  block_10  L922-922  `// map <uint64_t, uint64_t>::iterator cl_check = unique_cl[c`
  block_11  L923-923  `ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator `
  block_12  L924-924  `ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator `
  block_13  L926-926  `// check unique cache line footprint`
  block_14  L927-927  `ankerl::unordered_dense::map <uint64_t, uint64_t>::iterator `
  block_15  L928-933  `if (cl_check == unique_cl[cpu].end()) { // we've never seen `
  block_16  L935-935  `pr = page_table.find(vpage);`
  block_17  L936-1057  `if (pr == page_table.end()) { // no VA => PA translation fou`
  block_18  L1059-1059  `pr = page_table.find(vpage);`
  block_19  L1060-1063  `#ifdef SANITY_CHECK`
  block_20  L1064-1064  `uint64_t ppage = pr->second;`
  block_21  L1066-1066  `uint64_t pa = ppage << LOG2_PAGE_SIZE;`
  block_22  L1067-1067  `pa |= voffset;`
  block_23  L1069-1069  `// DP ( if (warmup_complete[cpu]) {`
  block_24  L1070-1070  `// cout << "[PAGE_TABLE] instr_id: " << instr_id << " vpage:`
  block_25  L1071-1071  `// cout << " => ppage: " << (pa >> LOG2_PAGE_SIZE) << " vadr`
  block_26  L1073-1076  `if (swap)`
  block_27  L1078-1078  `//cout << "cpu: " << cpu << " allocated unique_vpage: " << h`
  block_28  L1080-1080  `return pa;`

## print_knobs  L1083-1111
  block_00  L1085-1091  `cout << "warmup_instructions " << warmup_instructions << end`
  block_01  L1092-1107  `cout << "num_cpus\t;" << NUM_CPUS<<";" << endl`
  block_02  L1108-1108  `print_core_config();`
  block_03  L1109-1109  `print_dram_config();`
  block_04  L1110-1110  `cout << endl;`

## main  L1114-1833
  block_00  L1116-1116  `// #if defined(__has_feature)`
  block_01  L1117-1117  `// #if __has_feature(memory_sanitizer)`
  block_02  L1118-1118  `// // Unpoison argv`
  block_03  L1119-1119  `// for (int i = 0; i < argc; i++) {`
  block_04  L1120-1120  `//     if (argv[i]) {`
  block_05  L1121-1121  `//         __msan_unpoison(argv[i], 32768);`
  block_06  L1122-1122  `//     }`
  block_07  L1123-1123  `// }`
  block_08  L1124-1124  `// #endif`
  block_09  L1125-1125  `// #endif`
  block_10  L1126-1126  `// memset(&ooo_cpu, 0, sizeof(ooo_cpu));`
  block_11  L1127-1127  `// memset(&uncore, 0, sizeof(uncore));`
  block_12  L1129-1129  `ChampsimDBConfig db_cfg = {};`
  block_13  L1131-1131  `int num_instr_dest = NUM_INSTR_DESTINATIONS;`
  block_14  L1132-1132  `int num_instr_dest_sparc = NUM_INSTR_DESTINATIONS_SPARC;`
  block_15  L1133-1138  `#ifdef SANITY_CHECK`
  block_16  L1140-1152  `if (num_instr_dest == 2) {`
  block_17  L1155-1155  `// pf_stat_num_retired = 0;`
  block_18  L1156-1156  `// interrupt signal hanlder`
  block_19  L1157-1157  `struct sigaction sigIntHandler;`
  block_20  L1158-1158  `sigIntHandler.sa_handler = signal_handler;`
  block_21  L1159-1159  `sigemptyset(&sigIntHandler.sa_mask);`
  block_22  L1160-1160  `sigIntHandler.sa_flags = 0;`
  block_23  L1161-1161  `sigaction(SIGINT, &sigIntHandler, NULL);`
  block_24  L1163-1163  `// cout << "************************************************`
  block_25  L1164-1164  `//      << "   ChampSim Multicore Out-of-Order Simulator" <<`
  block_26  L1165-1165  `//      << "   Last compiled: " << __DATE__ << " " << __TIME`
  block_27  L1166-1166  `//      << "************************************************`
  block_28  L1168-1169  `#include <cstdio>`
  block_29  L1169-1169  `fprintf(stdout, "*******************************************`
  block_30  L1170-1170  `fprintf(stdout, "   ChampSim Multicore Out-of-Order Simulato`
  block_31  L1171-1171  `fprintf(stdout, "   Last compiled: %s %s\n", __DATE__, __TIM`
  block_32  L1172-1172  `fprintf(stdout, "*******************************************`
  block_33  L1175-1175  `// initialize knobs`
  block_34  L1176-1176  `uint8_t show_heartbeat = 1;`
  block_35  L1178-1178  `uint32_t seed_number = 0;`
  block_36  L1180-1180  `// check to see if knobs changed using getopt_long()`
  block_37  L1181-1181  `int c;`
  block_38  L1182-1260  `while (1) {`
  block_39  L1262-1265  `if (knob_low_bandwidth)`
  block_40  L1267-1267  `// DRAM access latency`
  block_41  L1268-1268  `tRP  = (uint32_t)((1.0 * tRP_DRAM_NANOSECONDS  * CPU_FREQ) /`
  block_42  L1269-1269  `tRCD = (uint32_t)((1.0 * tRCD_DRAM_NANOSECONDS * CPU_FREQ) /`
  block_43  L1270-1270  `tCAS = (uint32_t)((1.0 * tCAS_DRAM_NANOSECONDS * CPU_FREQ) /`
  block_44  L1272-1272  `// default: 16 = (64 / 8) * (3200 / 1600)`
  block_45  L1273-1273  `// it takes 16 CPU cycles to tranfser 64B cache block on a 8`
  block_46  L1274-1274  `// note that dram burst length = BLOCK_SIZE/DRAM_CHANNEL_WID`
  block_47  L1275-1275  `DRAM_DBUS_RETURN_TIME = (BLOCK_SIZE / DRAM_CHANNEL_WIDTH) * `
  block_48  L1277-1277  `// end consequence of knobs`
  block_49  L1279-1279  `// search through the argv for "-traces"`
  block_50  L1280-1280  `// search through the argv for "-traces"`
  block_51  L1281-1281  `int found_traces = 0;`
  block_52  L1282-1282  `int count_traces = 0;`
  block_53  L1283-1283  `cout << endl;`
  block_54  L1284-1384  `for (int i=0; i<argc; i++) {`
  block_55  L1386-1386  `// cout << "NUM_CPUS " << NUM_CPUS << endl;`
  block_56  L1387-1387  `fprintf(stdout, "NUM_CPUS %d\n", NUM_CPUS);`
  block_57  L1389-1392  `if (count_traces != NUM_CPUS) {`
  block_58  L1393-1393  `// end trace file setup`
  block_59  L1395-1395  `// TODO: can we initialize these variables from the class co`
  block_60  L1396-1396  `srand(seed_number);`
  block_61  L1397-1397  `champsim_seed = seed_number;`
  block_62  L1399-1399  `lpm_init(); // INITIALIZE THE STATS COLLECTOR FOR THE Layer `
  block_63  L1399-1399  `lpm_init(); // INITIALIZE THE STATS COLLECTOR FOR THE Layer `
  block_64  L1400-1501  `for (int i=0; i<NUM_CPUS; i++) {`
  block_65  L1502-1514  `#ifdef BYPASS_DEBUG`
  block_66  L1516-1516  `uncore.LLC.llc_initialize_replacement();`
  block_67  L1517-1517  `uncore.LLC.llc_prefetcher_initialize();`
  block_68  L1518-1520  `#ifdef BYPASS_LLC_LOGIC`
  block_69  L1522-1527  `if (false){`
  block_70  L1531-1531  `// simulation entry point`
  block_71  L1532-1532  `start_time = time(NULL);`
  block_72  L1534-1546  `#ifdef USE_TRACE_HELPER`
  block_73  L1548-1548  `uint8_t run_simulation = 1;`
  block_74  L1549-1734  `while (run_simulation) {`
  block_75  L1736-1743  `#ifdef USE_TRACE_HELPER`
  block_76  L1745-1747  `uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time`
  block_77  L1748-1748  `elapsed_minute -= elapsed_hour*60;`
  block_78  L1749-1749  `elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);`
  block_79  L1751-1751  `cout << endl << "ChampSim completed all CPUs" << endl;`
  block_80  L1752-1752  `//     if (NUM_CPUS > 1) {`
  block_81  L1753-1753  `//         cout << endl << "Total Simulation Statistics (inc`
  block_82  L1754-1754  `//         for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_83  L1755-1755  `//             cout << endl << "CPU ;" << i << "; cumulative`
  block_84  L1756-1756  `//             cout << " instructions: " << ooo_cpu[i].num_r`
  block_85  L1757-1757  `// #ifndef CRC2_COMPILE`
  block_86  L1758-1758  `//             print_sim_stats(i, &ooo_cpu[i].L1D);`
  block_87  L1759-1759  `//             print_sim_stats(i, &ooo_cpu[i].L1I);`
  block_88  L1760-1760  `//             print_sim_stats(i, &ooo_cpu[i].L2C);`
  block_89  L1761-1761  `//             ooo_cpu[i].L1D.l1d_prefetcher_final_stats();`
  block_90  L1762-1762  `//             ooo_cpu[i].L2C.l2c_prefetcher_final_stats();`
  block_91  L1763-1763  `// #endif`
  block_92  L1764-1764  `//             print_sim_stats(i, &uncore.LLC);`
  block_93  L1765-1765  `//         }`
  block_94  L1766-1766  `//         uncore.LLC.llc_prefetcher_final_stats();`
  block_95  L1767-1767  `//     }`
  block_96  L1769-1769  `cout << endl << "[ROI Statistics]" << endl;`
  block_97  L1770-1803  `for (uint32_t i=0; i<NUM_CPUS; i++)`
  block_98  L1805-1808  `for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_99  L1810-1810  `uncore.LLC.llc_prefetcher_final_stats();`
  block_100  L1812-1815  `#ifndef CRC2_COMPILE`
  block_101  L1816-1816  `// cout << "STAT_ROI_DICT|"<<statsCollector.dumpAllAsString(`
  block_102  L1817-1817  `// print execution_checksum`
  block_103  L1818-1820  `for (uint32_t i=0; i<NUM_CPUS; i++) {`
  block_104  L1822-1826  `#ifdef USE_HERMES`
  block_105  L1827-1827  `cout << "FINAL ROI CORE AVG IPC: ;" << (TOTAL_SUM_FINAL_SIM_`
  block_106  L1829-1830  `champsim_db_store(db_cfg, TOTAL_SUM_FINAL_SIM_IPC / NUM_CPUS`
  block_107  L1832-1832  `return 0;`
