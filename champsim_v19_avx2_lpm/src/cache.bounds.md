# cache.cc — function bounds

## set_force_all_hits  L23-25
  block_00  L24-24  `FORCE_ALL_HITS = toEnable;`

## dump_req  L34-68
  block_00  L36-36  `cout << std::hex << " Addr: "  << o.address;`
  block_01  L37-37  `//<< " FAddr: " << o.full_addr`
  block_02  L38-40  `cout << std::dec`
  block_03  L41-41  `cout << " {";`
  block_04  L42-47  `for (auto rob_set : o.rob_index_depend_on_me) {`
  block_05  L48-48  `cout << "} ";`
  block_06  L49-49  `cout << "LQ " << int(o.lq_index);`
  block_07  L50-50  `cout << " {";`
  block_08  L51-51  `ITERATE_SET(lq_set, o.lq_index_depend_on_me, LQ_SIZE) {`
  block_09  L51-56  `ITERATE_SET(lq_set, o.lq_index_depend_on_me, LQ_SIZE) {`
  block_10  L57-57  `cout << "} ";`
  block_11  L58-58  `cout << "SQ " << int(o.sq_index);`
  block_12  L59-59  `cout << " {";`
  block_13  L60-60  `ITERATE_SET(sq_set, o.sq_index_depend_on_me, LQ_SIZE) {`
  block_14  L60-65  `ITERATE_SET(sq_set, o.sq_index_depend_on_me, LQ_SIZE) {`
  block_15  L66-66  `cout << "} ";`
  block_16  L67-67  `cout << "ByP " << int(o.l1_bypassed) << " " << int(o.l2_bypa`

## dump_req  L69-104
  block_00  L71-71  `cout << std::hex << " Addr " << o->address;`
  block_01  L72-72  `//<< " FAddr " << o->full_addr`
  block_02  L73-76  `cout << std::dec`
  block_03  L77-77  `cout << " {";`
  block_04  L78-83  `for (auto rob_set : o->rob_index_depend_on_me) {`
  block_05  L84-84  `cout << "} ";`
  block_06  L85-85  `cout << "LQ " << int(o->lq_index);`
  block_07  L86-86  `cout << " {";`
  block_08  L87-87  `ITERATE_SET(lq_set, o->lq_index_depend_on_me, LQ_SIZE) {`
  block_09  L87-92  `ITERATE_SET(lq_set, o->lq_index_depend_on_me, LQ_SIZE) {`
  block_10  L93-93  `cout << "} ";`
  block_11  L94-94  `cout << "SQ " << int(o->sq_index);`
  block_12  L95-95  `cout << " {";`
  block_13  L96-96  `ITERATE_SET(sq_set, o->sq_index_depend_on_me, LQ_SIZE) {`
  block_14  L96-101  `ITERATE_SET(sq_set, o->sq_index_depend_on_me, LQ_SIZE) {`
  block_15  L102-102  `cout << "} ";`
  block_16  L103-103  `cout << "ByP " << int(o->l1_bypassed) << " " << int(o->l2_by`

## print_cache_config  L115-179
  block_00  L116-178  `cout << std::right << setw(3) <<"itlb_sz ;"<<std::left << se`

## handle_fill  L183-279
  block_00  L185-185  `// handle fill`
  block_01  L186-186  `uint16_t fill_cpu = (MSHR.next_fill_index == MSHR_SIZE) ? NU`
  block_02  L187-188  `if (fill_cpu == NUM_CPUS)`
  block_03  L190-278  `if ((MSHR.next_fill_cycle != UINT32_MAX) && CYC_LE(MSHR.next`

## merge_with_prefetch  L281-311
  block_00  L282-282  `// ORIGINAL info retainer`
  block_01  L283-283  `uint8_t prior_returned = mshr_packet.returned;`
  block_02  L284-284  `uint32_t prior_event_cycle = mshr_packet.event_cycle;`
  block_03  L285-288  `#ifdef BYPASS_L1_LOGIC`
  block_04  L289-291  `#ifdef BYPASS_L2_LOGIC`
  block_05  L292-294  `#ifdef BYPASS_LLC_LOGIC`
  block_06  L295-295  `uint8_t prior_pf_merged = mshr_packet.pf_merged_from_upper;`
  block_07  L296-296  `mshr_packet = queue_packet;`
  block_08  L297-297  `// restore original retained data`
  block_09  L298-298  `mshr_packet.returned = prior_returned;`
  block_10  L299-299  `mshr_packet.event_cycle = prior_event_cycle;`
  block_11  L300-303  `#ifdef BYPASS_L1_LOGIC`
  block_12  L304-306  `#ifdef BYPASS_L2_LOGIC`
  block_13  L307-309  `#ifdef BYPASS_LLC_LOGIC`
  block_14  L310-310  `mshr_packet.pf_merged_from_upper = prior_pf_merged;`

## handle_writeback  L313-378
  block_00  L314-314  `// handle write`
  block_01  L315-315  `uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;`
  block_02  L316-317  `if (writeback_cpu == NUM_CPUS)`
  block_03  L319-319  `// handle the oldest entry`
  block_04  L320-377  `if ((CYC_LE(WQ.entry[WQ.head].event_cycle, CYC(current_core_`

## handle_read_miss_bypass  L379-434
  block_00  L380-387  `#if defined(BYPASS_L1D_OnNewMiss) || defined(BYPASS_L2_LOGIC`
  block_01  L388-401  `#ifdef BYPASS_L1D_OnNewMiss`
  block_02  L402-417  `#ifdef BYPASS_L2_LOGIC`
  block_03  L418-432  `#ifdef BYPASS_LLC_LOGIC`
  block_04  L433-433  `return false;`

## handle_read  L436-494
  block_00  L437-493  `for (uint16_t i=0; i<MAX_READ; i++) {`

## handle_prefetch  L496-682
  block_00  L497-497  `// handle prefetch`
  block_01  L498-681  `for (uint16_t i=0; i<MAX_READ; i++) {`

## operate  L685-796
  block_00  L686-686  `// print RQ MSHR head index instruction id`
  block_01  L687-691  `#ifdef BYPASS_DEBUG`
  block_02  L692-788  `if (warmup_complete[cpu]) {`
  block_03  L789-789  `/* >>> end LPM <<< */`
  block_04  L790-790  `handle_fill();`
  block_05  L791-791  `handle_writeback();`
  block_06  L792-792  `reads_available_this_cycle = MAX_READ;`
  block_07  L793-793  `handle_read();`
  block_08  L794-795  `if (PQ.occupancy && (reads_available_this_cycle > 0))`

## get_way  L798-805
  block_00  L799-802  `for (uint32_t way=0; way<NUM_WAY; way++) {`
  block_01  L804-804  `return NUM_WAY;`

## fill_cache  L807-858
  block_00  L808-823  `#ifdef TRUE_SANITY_CHECK`
  block_01  L824-825  `if (block[set][way].prefetch && (block[set][way].used == 0))`
  block_02  L827-830  `#ifdef USE_HERMES`
  block_03  L832-833  `if (block[set][way].valid == 0)`
  block_04  L834-834  `block[set][way].dirty = 0;`
  block_05  L835-835  `block[set][way].prefetch = (packet->type == PREFETCH) ? 1 : `
  block_06  L836-836  `block[set][way].used = 0;`
  block_07  L838-839  `if (block[set][way].prefetch)`
  block_08  L841-841  `// block[set][way].delta = packet->delta;`
  block_09  L842-842  `// block[set][way].depth = packet->depth;`
  block_10  L843-843  `// block[set][way].signature = packet->signature;`
  block_11  L844-844  `// block[set][way].confidence = packet->confidence;`
  block_12  L846-846  `block[set][way].tag = packet->address;`
  block_13  L847-847  `block[set][way].address = packet->address;`
  block_14  L848-848  `block[set][way].full_addr = packet->full_addr;`
  block_15  L849-849  `block[set][way].data = packet->data;`
  block_16  L850-850  `block[set][way].cpu = packet->cpu;`
  block_17  L851-851  `block[set][way].instr_id = packet->instr_id;`
  block_18  L853-853  `// DP( if ((current_core_cycle[cpu] > 58318781) && warmup_co`
  block_19  L854-854  `// DP( if (warmup_complete[packet->cpu] && (NAME == "L1D" ||`
  block_20  L855-855  `// cout << "[" << NAME << "] " << __func__ << " set: " << se`
  block_21  L856-856  `// cout << " lru: " << block[set][way].lru << " tag: " << he`
  block_22  L857-857  `// cout << " data: " << block[set][way].data << dec << endl;`

## check_hit  L860-899
  block_00  L863-863  `uint32_t set = get_set(packet->address);`
  block_01  L864-864  `int match_way = -1;`
  block_02  L866-866  `//pf_issued=pf_issued+1024;`
  block_03  L867-873  `if (FORCE_ALL_HITS) { //(cache_type == IS_L1D || cache_type `
  block_04  L874-881  `#ifdef TRUE_SANITY_CHECK`
  block_05  L883-883  `// hit`
  block_06  L884-896  `for (uint32_t way=0; way<NUM_WAY; way++) {`
  block_07  L898-898  `return match_way;`

## invalidate_entry  L901-926
  block_00  L902-902  `uint32_t set = get_set(inval_addr);`
  block_01  L903-903  `int match_way = -1;`
  block_02  L904-910  `#ifdef TRUE_SANITY_CHECK`
  block_03  L911-911  `// invalidate`
  block_04  L912-924  `for (uint32_t way=0; way<NUM_WAY; way++) {`
  block_05  L925-925  `return match_way;`

## add_rq  L928-1190
  block_00  L930-930  `// check for the latest wirtebacks in the write queue`
  block_01  L931-931  `int wq_index = WQ.check_queue(packet);`
  block_02  L932-932  `// check if WQ packet has ByPass and new packet not`
  block_03  L935-939  `#ifdef BYPASS_DEBUG`
  block_04  L941-1011  `if (wq_index != -1) {`
  block_05  L1014-1014  `// check for duplicates in the read queue`
  block_06  L1015-1015  `int index = RQ.check_queue(packet);`
  block_07  L1016-1025  `#ifdef DEBUG_PRINT`
  block_08  L1026-1142  `if (index != -1) {`
  block_09  L1144-1144  `// check occupancy`
  block_10  L1145-1148  `if (RQ.occupancy == RQ_SIZE) {`
  block_11  L1149-1149  `// if there is no duplicate, add it to RQ`
  block_12  L1150-1150  `index = RQ.tail;`
  block_13  L1152-1159  `#ifdef TRUE_SANITY_CHECK`
  block_14  L1161-1161  `RQ.entry[index]= std::move(*packet);`
  block_15  L1163-1163  `// ADD LATENCY`
  block_16  L1164-1167  `if (CYC_LT(RQ.entry[index].event_cycle, CYC(current_core_cyc`
  block_17  L1169-1169  `RQ.occupancy++;`
  block_18  L1170-1170  `RQ.tail++;`
  block_19  L1171-1172  `if (RQ.tail >= RQ.SIZE)`
  block_20  L1173-1173  `// DP( if (warmup_complete[RQ.entry[index].cpu] && (NAME == `
  block_21  L1174-1174  `// DP( if ((NAME == "L1D" || NAME == "L2C" || NAME == "LLC")`
  block_22  L1175-1180  `DP( if ((current_core_cycle[cpu] > 58318781) && warmup_compl`
  block_23  L1181-1181  `//`
  block_24  L1182-1185  `#ifdef TRUE_SANITY_CHECK`
  block_25  L1186-1186  `RQ.TO_CACHE++;`
  block_26  L1187-1187  `RQ.ACCESS++;`
  block_27  L1189-1189  `return -1;`

## add_wq  L1192-1238
  block_00  L1193-1193  `// check for duplicates in the write queue`
  block_01  L1194-1194  `int index = WQ.check_queue(packet);`
  block_02  L1195-1199  `if (index != -1) {`
  block_03  L1200-1204  `#ifdef TRUE_SANITY_CHECK`
  block_04  L1206-1206  `// if there is no duplicate, add it to the write queue`
  block_05  L1207-1207  `index = WQ.tail;`
  block_06  L1208-1215  `#ifdef TRUE_SANITY_CHECK`
  block_07  L1216-1216  `WQ.entry[index]= std::move(*packet);`
  block_08  L1217-1217  `// WQ.entry[index].quickReset();`
  block_09  L1218-1218  `// ADD LATENCY`
  block_10  L1219-1222  `if (CYC_LT(WQ.entry[index].event_cycle, CYC(current_core_cyc`
  block_11  L1224-1224  `WQ.occupancy++;`
  block_12  L1225-1225  `WQ.tail++;`
  block_13  L1226-1227  `if (WQ.tail >= WQ.SIZE)`
  block_14  L1228-1228  `// DP( if ((current_core_cycle[cpu] > 58318781) && warmup_co`
  block_15  L1229-1234  `DP( if (warmup_complete[WQ.entry[index].cpu]) {`
  block_16  L1234-1234  `cout << " event: " << WQ.entry[index].event_cycle << " curr:`
  block_17  L1234-1234  `cout << " event: " << WQ.entry[index].event_cycle << " curr:`
  block_18  L1235-1235  `WQ.TO_CACHE++;`
  block_19  L1236-1236  `WQ.ACCESS++;`
  block_20  L1237-1237  `return -1;`

## ( if (warmup_complete[WQ.entry[index].cp  L1229-1234
  block_00  L1230-1230  `cout << "[" << NAME << "_WQ] " <<  __func__ << " instr_id: "`
  block_01  L1231-1231  `cout << " full_addr: " << WQ.entry[index].full_addr << dec;`
  block_02  L1232-1232  `cout << " head: " << WQ.head << " tail: " << WQ.tail << " oc`
  block_03  L1233-1233  `cout << " data: " << hex << WQ.entry[index].data << dec;`
  block_04  L1234-1234  `cout << " event: " << WQ.entry[index].event_cycle << " curr:`

## prefetch_line  L1240-1266
  block_00  L1241-1241  `pf_requested++;`
  block_01  L1242-1264  `if (PQ.occupancy < PQ.SIZE) {`
  block_02  L1265-1265  `return 0;`

## add_pq  L1268-1349
  block_00  L1269-1269  `// check for the latest wirtebacks in the write queue`
  block_01  L1270-1270  `int wq_index = WQ.check_queue(packet);`
  block_02  L1271-1284  `if (wq_index != -1) {`
  block_03  L1286-1286  `// check for duplicates in the PQ`
  block_04  L1287-1287  `int index = PQ.check_queue(packet);`
  block_05  L1288-1296  `if (index != -1) {`
  block_06  L1298-1298  `// check occupancy`
  block_07  L1299-1305  `if (PQ.occupancy == PQ_SIZE) {`
  block_08  L1307-1307  `// if there is no duplicate, add it to PQ`
  block_09  L1308-1308  `index = PQ.tail;`
  block_10  L1310-1317  `#ifdef TRUE_SANITY_CHECK`
  block_11  L1319-1319  `PQ.entry[index]= std::move(*packet);`
  block_12  L1321-1321  `// ADD LATENCY`
  block_13  L1322-1325  `if (CYC_LT(PQ.entry[index].event_cycle, CYC(current_core_cyc`
  block_14  L1327-1327  `PQ.occupancy++;`
  block_15  L1328-1328  `PQ.tail++;`
  block_16  L1329-1330  `if (PQ.tail >= PQ.SIZE)`
  block_17  L1332-1332  `// DP( if (warmup_complete[PQ.entry[index].cpu] && (NAME == `
  block_18  L1333-1333  `// DP( if ((NAME == "L1D" || NAME == "L2C" || NAME == "LLC")`
  block_19  L1334-1340  `DP( if ((current_core_cycle[cpu] > 58318781) && warmup_compl`
  block_20  L1341-1344  `#ifdef TRUE_SANITY_CHECK`
  block_21  L1345-1345  `PQ.TO_CACHE++;`
  block_22  L1346-1346  `PQ.ACCESS++;`
  block_23  L1348-1348  `return -1;`

## return_data  L1351-1479
  block_00  L1352-1352  `// check MSHR information`
  block_01  L1353-1353  `int mshr_index = check_mshr(packet);`
  block_02  L1355-1364  `#ifdef TRUE_SANITY_CHECK`
  block_03  L1365-1365  `// DP( if ((current_core_cycle[cpu] > 58318781) && warmup_co`
  block_04  L1366-1371  `DP( if (warmup_complete[packet->cpu]) {`
  block_05  L1371-1371  `cout << endl; });`
  block_06  L1371-1371  `cout << endl; });`
  block_07  L1372-1372  `// if (packet->instr_id == MSHR.entry[mshr_index].instr_id) `
  block_08  L1373-1373  `//     MSHR.entry[mshr_index].lq_index_depend_on_me.join(pac`
  block_09  L1374-1374  `// }`
  block_10  L1376-1376  `// MSHR holds the most updated information about this reques`
  block_11  L1377-1377  `// no need to do memcpy`
  block_12  L1378-1378  `MSHR.num_returned++;`
  block_13  L1379-1379  `MSHR.entry[mshr_index].returned = COMPLETED;`
  block_14  L1380-1380  `MSHR.entry[mshr_index].data = packet->data;`
  block_15  L1381-1381  `MSHR.entry[mshr_index].pf_metadata = packet->pf_metadata;`
  block_16  L1383-1383  `// ADD LATENCY`
  block_17  L1384-1387  `if (CYC_LT(MSHR.entry[mshr_index].event_cycle, CYC(current_c`
  block_18  L1388-1445  `#ifdef BYPASS_L1_LOGIC`
  block_19  L1446-1446  `//     // VB: ===== NEW: propagate bypass-accumulated deps f`
  block_20  L1447-1447  `// #ifdef BYPASS_L1_LOGIC`
  block_21  L1448-1448  `//     if (cache_type == IS_L1D) {`
  block_22  L1449-1449  `//         if (packet->load_merged) {`
  block_23  L1450-1450  `//             MSHR.entry[mshr_index].load_merged = 1;`
  block_24  L1451-1451  `//             MSHR.entry[mshr_index].lq_index_depend_on_me.`
  block_25  L1452-1452  `//                 packet->lq_index_depend_on_me, LQ_SIZE);`
  block_26  L1453-1453  `//             // MSHR.entry[mshr_index].lq_index_depend_on_`
  block_27  L1454-1454  `//`
  block_28  L1455-1455  `//         }`
  block_29  L1456-1456  `//         if (packet->store_merged) {`
  block_30  L1457-1457  `//             MSHR.entry[mshr_index].store_merged = 1;`
  block_31  L1458-1458  `//             MSHR.entry[mshr_index].sq_index_depend_on_me.`
  block_32  L1459-1459  `//                 packet->sq_index_depend_on_me, SQ_SIZE);`
  block_33  L1460-1460  `//         }`
  block_34  L1461-1461  `//     }`
  block_35  L1462-1462  `// #endif`
  block_36  L1463-1463  `//     // ===== END NEW =====`
  block_37  L1465-1465  `update_fill_cycle();`
  block_38  L1467-1467  `// DP( if ((current_core_cycle[cpu] > 58318781) && warmup_co`
  block_39  L1468-1478  `DP( if (warmup_complete[packet->cpu]) {`
  block_40  L1478-1478  `cout << endl; });`
  block_41  L1478-1478  `cout << endl; });`

## ( if (warmup_complete[packet->cpu])  L1366-1371
  block_00  L1367-1367  `cout << "[" << NAME << "MSHR_ret_before] " <<  __func__;`
  block_01  L1368-1368  `dump_req(packet);`
  block_02  L1369-1369  `cout << " MSHR: ";`
  block_03  L1370-1370  `dump_req(MSHR.entry[mshr_index]);`
  block_04  L1371-1371  `cout << endl; });`

## ( if (warmup_complete[packet->cpu])  L1468-1478
  block_00  L1469-1469  `cout << "[" << NAME << "_MSHR_ret_after] " <<  __func__;`
  block_01  L1470-1470  `cout << " MSHR";`
  block_02  L1471-1471  `dump_req(MSHR.entry[mshr_index]);`
  block_03  L1472-1472  `//     << " recieved instr_id: " << MSHR.entry[mshr_index].i`
  block_04  L1473-1473  `// cout << " addr: " << hex << MSHR.entry[mshr_index].addres`
  block_05  L1474-1474  `// cout << " data: " << MSHR.entry[mshr_index].data << dec <`
  block_06  L1475-1475  `// cout << " idx: " << mshr_index << " occup: " << MSHR.occu`
  block_07  L1476-1476  `// cout << " event: " << MSHR.entry[mshr_index].event_cycle `
  block_08  L1477-1477  `// cout << " MSHR ByP: " << (int) MSHR.entry[mshr_index].l1_`
  block_09  L1478-1478  `cout << endl; });`

## update_fill_cycle  L1481-1510
  block_00  L1482-1482  `// update next_fill_cycle`
  block_01  L1483-1483  `uint32_t min_cycle = UINT32_MAX;`
  block_02  L1484-1484  `uint16_t min_index = MSHR.SIZE;`
  block_03  L1485-1498  `for (uint16_t i=0; i<MSHR.SIZE; i++) {`
  block_04  L1499-1499  `MSHR.next_fill_cycle = min_cycle;`
  block_05  L1500-1500  `MSHR.next_fill_index = min_index;`
  block_06  L1501-1509  `if (min_index < MSHR.SIZE) {`

## probe_mshr  L1512-1518
  block_00  L1513-1516  `for (uint16_t index = 0; index < MSHR_SIZE; index++) {`
  block_01  L1517-1517  `return -1;`

## check_mshr  L1520-1571
  block_00  L1521-1521  `// Bloom filter early exit: guaranteed no false negatives`
  block_01  L1522-1523  `if (!bloom[BLOOM_QTYPE_MSHR].maybe_contains(packet->address)`
  block_02  L1525-1525  `// Branchless MSHR scan: no branches in hot loop, arithmetic`
  block_03  L1526-1526  `const uint64_t target = packet->address;`
  block_04  L1527-1527  `int32_t found = -1;`
  block_05  L1528-1536  `for (uint16_t index = 0; index < MSHR_SIZE; index++) {`
  block_06  L1538-1569  `if (found >= 0) {`
  block_07  L1570-1570  `return found;`

## add_mshr  L1573-1611
  block_00  L1574-1574  `uint16_t index = 0;`
  block_01  L1575-1575  `packet->cycle_enqueued = CYC(current_core_cycle[packet->cpu]`
  block_02  L1576-1576  `// search mshr`
  block_03  L1577-1602  `for (index=0; index<MSHR_SIZE; index++) {`
  block_04  L1603-1610  `#ifdef TRUE_SANITY_CHECK`

## get_occupancy  L1613-1621
  block_00  L1614-1620  `switch (queue_type) {`

## get_size  L1623-1632
  block_00  L1625-1631  `switch (queue_type) {`

## increment_WQ_FULL  L1634-1637
  block_00  L1636-1636  `WQ.FULL++;`

## prefetcher_feedback  L1639-1645
  block_00  L1641-1641  `pref_gen = pf_issued;`
  block_01  L1642-1642  `pref_fill = pf_fill;`
  block_02  L1643-1643  `pref_used = pf_useful;`
  block_03  L1644-1644  `pref_late = pf_late;`
