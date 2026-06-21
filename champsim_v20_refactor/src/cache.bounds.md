# champsim_refactor/src/cache.cc — function bounds

## CACHE::set_force_all_hits  L103-105
  block_01  L104-104  `FORCE_ALL_HITS = toEnable;`

## CACHE::dump_req(PACKET&)  L114-148
  block_01  L116-121  `cout << std::hex << " Addr: "  << o.address;`
  block_02  L122-128  `for (auto rob_set : o.rob_index_depend_on_me) {`
  block_03  L129-136  `cout << "LQ " << int(o.lq_index);`
  block_04  L138-146  `cout << "SQ " << int(o.sq_index);`
  block_05  L147-147  `cout << "ByP " << int(o.l1_bypassed) ...`

## CACHE::dump_req(PACKET*)  L149-184
  block_01  L151-156  `cout << std::hex << " Addr " << o->address;`
  block_02  L157-164  `for (auto rob_set : o->rob_index_depend_on_me) {`
  block_03  L165-173  `cout << "LQ " << int(o->lq_index);`
  block_04  L174-182  `cout << "SQ " << int(o->sq_index);`
  block_05  L183-183  `cout << "ByP " << int(o->l1_bypassed) ...`

## CACHE::dump_req_min_read(PACKET&)  L219-219
  block_01  L219-219  `_DRMIN_CORE(o); _DRMIN_SET_ROB(o); _DRMIN_SET_LQ(o); _DRMIN_SET_SQ(o); _DRMIN_TAIL();`

## CACHE::dump_req_min_read(PACKET*)  L220-220
  block_01  L220-220  `_DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL();`

## CACHE::dump_req_min_write(PACKET&)  L221-221
  block_01  L221-221  `_DRMIN_CORE(o); _DRMIN_SET_ROB(o); _DRMIN_SET_LQ(o); _DRMIN_SET_SQ(o); _DRMIN_TAIL();`

## CACHE::dump_req_min_write(PACKET*)  L222-222
  block_01  L222-222  `_DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL();`

## CACHE::dump_req_min_mshr(PACKET&)  L223-223
  block_01  L223-223  `_DRMIN_CORE(o); _DRMIN_SET_ROB(o); _DRMIN_SET_LQ(o); _DRMIN_SET_SQ(o); _DRMIN_TAIL();`

## CACHE::dump_req_min_mshr(PACKET*)  L224-224
  block_01  L224-224  `_DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL();`

## CACHE::dump_req_min_ret(PACKET&)  L225-225
  block_01  L225-225  `_DRMIN_CORE(o); _DRMIN_SET_ROB(o); _DRMIN_SET_LQ(o); _DRMIN_SET_SQ(o); _DRMIN_TAIL();`

## CACHE::dump_req_min_ret(PACKET*)  L226-226
  block_01  L226-226  `_DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL();`

## CACHE::dump_req_min_pq(PACKET&)  L227-227
  block_01  L227-227  `_DRMIN_CORE(o); ... cout << " pfmd=" << o.pf_metadata; _DRMIN_TAIL();`

## CACHE::dump_req_min_pq(PACKET*)  L228-228
  block_01  L228-228  `_DRMIN_CORE(*o); ... cout << " pfmd=" << o->pf_metadata; _DRMIN_TAIL();`

## CACHE::dump_req_min_fill(PACKET&)  L229-229
  block_01  L229-229  `_DRMIN_CORE(o); _DRMIN_SET_ROB(o); _DRMIN_SET_LQ(o); _DRMIN_SET_SQ(o); _DRMIN_TAIL();`

## CACHE::dump_req_min_fill(PACKET*)  L230-230
  block_01  L230-230  `_DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL();`

## print_cache_config  L240-304
  block_01  L241-303  `cout << std::right << setw(3) <<"itlb_sz ;"<<std::left << setw(4) ...`

## CACHE::handle_fill  L308-463
  block_01  L311-312  `const uint16_t mshr_nfi_ = MSHR.next_fill_index;`
  block_02  L315-317  `uint16_t fill_cpu = ...; if (fill_cpu == NUM_CPUS) return;`
  block_03  L319-320  `if (mshr_nfc_ != CYC_PACKED_MAX) PCYCLE_SANITY(...)`
  block_04  L322-403  `SANITY_MSHR_IDX_BOUND(...); #ifdef LLC_BYPASS bypass path ...`
  block_05  L406-411  `uint8_t do_fill = 1; handle_fill_evict_dirty(...)`
  block_06  L412-461  `if (do_fill){ handle_fill_pf_fill ... handle_fill_remove(...) }`

## CACHE::merge_with_prefetch  L465-495
  block_01  L467-479  `uint8_t prior_returned = mshr_packet.returned; snapshot bypass fields`
  block_02  L480-494  `mshr_packet = queue_packet; restore retained fields`

## CACHE::handle_writeback  L497-575
  block_01  L498-504  `if (WQ.occupancy == 0) return; uint32_t writeback_cpu = WQ.entry[wq_h_].cpu;`
  block_02  L507-514  `PCYCLE_SANITY(...); int index = wq_h_; uint32_t set = ...; int way = check_hit(...);`
  block_03  L517-519  `if (way >= 0) { handle_writeback_hit(...) }`
  block_04  L521-548  `else if (cache_type == IS_L1D) { RFO miss: check mshr, new/full/inflight paths }`
  block_05  L550-572  `else { find victim, evict dirty, do_fill path }`

## CACHE::handle_read_miss_bypass  L576-636
  block_01  L577-584  `#if BYPASS_L1D_OnNewMiss||L2||LLC total_ByP_req[read_cpu]++`
  block_02  L585-599  `#ifdef BYPASS_L1D_OnNewMiss l1d_bypass_operate → RQ.entry[index].l1_bypassed=1`
  block_03  L601-617  `#ifdef BYPASS_L2_LOGIC l2c_bypass_operate → RQ.entry[index].l2_bypassed=1`
  block_04  L619-634  `#ifdef BYPASS_LLC_LOGIC llc_bypass_operate → RQ.entry[index].llc_bypassed=1`
  block_05  L635-635  `return false;`

## CACHE::handle_read  L638-725
  block_01  L639-644  `for (uint16_t i=0; i<MAX_READ; i++) { const uint16_t rq_h_ = RQ.head;`
  block_02  L646-650  `PCYCLE_SANITY(...); int index = rq_h_; set/way lookup`
  block_03  L652-667  `if (way >= 0) { read hit: processed, bypass_return, pf_operate, replacement, stats, return }`
  block_04  L668-717  `else { read miss: bypass, new-miss-LLC, new-miss-other, mshr-full, inflight-merge paths }`
  block_05  L719-724  `} else { return; } if (reads_available_this_cycle == 0) return;`

## CACHE::handle_prefetch  L727-901
  block_01  L729-735  `for (uint16_t i=0; i<MAX_READ; i++) { const uint16_t pq_h_ = PQ.head;`
  block_02  L737-738  `PCYCLE_SANITY(...)`
  block_03  L745-779  `if (way >= 0) { prefetch hit: update replacement, stats, pf_operate, return_to_upper_level }`
  block_04  L781-883  `else { prefetch miss: new-miss LLC/other, mshr-full, inflight-merge paths }`
  block_05  L884-900  `if (miss_handled) { MISS/ACCESS++, PQ.remove_queue } else return;`

## CACHE::operate  L904-1081
  block_01  L905-921  `DP(...); _lpm_has_mshr check; if (has_work==0 && !_lpm_has_mshr) idle accumulate+return`
  block_02  L923-942  `if (cache_idle_accumulator > 0) flush idle cycles via advance_idle()`
  block_03  L944-1067  `if (warmup_complete[cpu]) { LPM tick: compute α, hit_active, miss_active, has_byp, lpm_operate }`
  block_04  L1070-1081 `if (has_work==0) return; has_work=0; handle_fill/writeback/read/prefetch; update has_work`

## CACHE::get_way  L1083-1090
  block_01  L1084-1089  `for (uint32_t way=0; way<NUM_WAY; way++) { if (block[set][way].valid && tag==address) return way;`

## CACHE::fill_cache  L1092-1144
  block_01  L1093-1095  `SANITY_FILL_TLB_DATA(...); if (prefetch && used==0) pf_useless++;`
  block_02  L1097-1108  `#ifdef BYPASS_L1/L2/LLC_LOGIC byplat.on_fill(...)`
  block_03  L1110-1113  `#ifdef USE_HERMES offchip_predictor_track_llc_eviction`
  block_04  L1115-1123  `valid/dirty/prefetch/used/pmc fields assignment`
  block_05  L1130-1133  `tag/address/full_addr/data fields assignment`

## CACHE::check_hit  L1146-1172
  block_01  L1149-1159  `uint32_t set = get_set(...); if (FORCE_ALL_HITS) { block[set][0] force hit; return 0; }`
  block_02  L1160-1160  `SANITY_SET_BOUND_PKT(...)`
  block_03  L1162-1169  `for way in NUM_WAY: if valid && tag==address → match_way = way; break`
  block_04  L1171-1171  `return match_way;`

## CACHE::invalidate_entry  L1174-1188
  block_01  L1175-1177  `uint32_t set = get_set(inval_addr); SANITY_SET_BOUND_INV(...)`
  block_02  L1179-1187  `for way in NUM_WAY: if valid && tag==inval_addr → valid=0; break`

## CACHE::add_rq  L1190-1401
  block_01  L1196-1203  `int wq_index = (instruction==1)?-1:WQ.check_queue(packet); DP(...)`
  block_02  L1204-1268  `if (wq_index != -1) { fill_level check, PROCESSED, HIT/ACCESS, WQ.FORWARD, return -1 }`
  block_03  L1271-1368  `int index = RQ.check_queue(packet); if (index != -1) { instruction/RFO/load merge, MERGED++ }`
  block_04  L1370-1374  `if (RQ.occupancy == RQ_SIZE) { RQ.FULL++; return -2; }`
  block_05  L1375-1400  `index=RQ.tail; SANITY; RQ.entry[index]=move(*packet); ADD LATENCY; occupancy/tail++; return -1`

## CACHE::add_wq  L1403-1435
  block_01  L1404-1410  `int index = WQ.check_queue(packet); if (index != -1) { WQ.MERGED++; return index; }`
  block_02  L1411-1434  `SANITY_WQ_OCCUPANCY; index=WQ.tail; WQ.entry[index]=move(*packet); ADD LATENCY; occupancy/tail++`

## CACHE::prefetch_line  L1437-1463
  block_01  L1438-1438  `pf_requested++;`
  block_02  L1439-1462  `if (PQ.occupancy < PQ.SIZE && same page) { build PACKET pf_packet; add_pq; pf_issued++; return 1; }`

## CACHE::add_pq  L1465-1530
  block_01  L1466-1482  `int wq_index = WQ.check_queue(packet); if (wq_index != -1) { return_to_upper_level; HIT++; return -1 }`
  block_02  L1484-1494  `int index = PQ.check_queue(packet); if (index != -1) { update fill_level; PQ.MERGED++; return index }`
  block_03  L1496-1501  `if (PQ.occupancy == PQ_SIZE) { PQ.FULL++; return -2; }`
  block_04  L1503-1529  `index=PQ.tail; SANITY; PQ.entry[index]=move(*packet); ADD LATENCY; occupancy/tail++; return -1`

## CACHE::return_data  L1532-1698
  block_01  L1533-1551  `DP_RET_M; int mshr_index = CHECK_MSHR(packet); DP LLC debug`
  block_02  L1552-1569  `SANITY_MSHR_FOUND; MSHR.num_returned++; LPM INFLIGHT->COMPLETED counters; returned=COMPLETED; data/pf_metadata`
  block_03  L1571-1575  `ADD LATENCY to MSHR.entry[mshr_index].event_cycle`
  block_04  L1576-1635  `#ifdef BYPASS_L1_LOGIC: L1D pf_promote / load_merge / load_into_rfo / rfo_merge cases`
  block_05  L1668-1693  `#ifdef BYPASS_L2_LOGIC: L2C pf_promote / load_merge`
  block_06  L1694-1697  `DP_RET_M("exit_pre_update"); update_fill_cycle();`

## CACHE::update_fill_cycle  L1700-1775
  block_01  L1701-1722  `DP ENTRY + DUMP all MSHR slots for debug`
  block_02  L1723-1728  `if (MSHR.num_returned == 0) { next_fill_cycle=MAX; next_fill_index=SIZE; return; }`
  block_03  L1729-1755  `linear scan MSHR for COMPLETED entries; track min event_cycle`
  block_04  L1756-1774  `MSHR.next_fill_cycle=min_cycle; MSHR.next_fill_index=min_index; DP WINNER`

## CACHE::probe_mshr  L1777-1783
  block_01  L1778-1782  `for index in MSHR_SIZE: if address==packet->address return index; return -1`

## CACHE::check_mshr  L1785-1925
  block_01  L1786-1791  `bloom_check_total++; if (!bloom.maybe_contains) { bloom_reject++; return -1; }`
  block_02  L1793-1805  `branchless MSHR scan: int32_t mask arithmetic to find matching entry`
  block_03  L1807-1919  `if (found>=0): LPM snapshot; fill_level promote; PF->demand takeover; bypass-flag clears; LPM adjust`
  block_04  L1920-1924  `} else { DP_MSHR_NEW_ADDR; } return found;`

## CACHE::check_mshr_hashmap  L1928-2037
  block_01  L1929-1930  `int32_t found = mshr_map.find(packet->address); if (found < 0) return -1;`
  block_02  L1932-1944  `LPM snapshot old bypass+cpu; fill_level promote`
  block_03  L1946-1985  `if PREFETCH->demand takeover: pf_late++; copy prior fields; restore; bypass flags`
  block_04  L1987-2030  `bypass-flag clears (#ifdef L1/L2/LLC); LPM adjust counters`
  block_05  L2032-2036  `DP_MSHR_MERGE; } else { DP_MSHR_NEW_ADDR; } return found;`

## CACHE::add_mshr  L2040-2088
  block_01  L2041-2042  `uint16_t index = 0; packet->cycle_enqueued = PACK_CYCLE(...)`
  block_02  L2043-2086  `for index in MSHR_SIZE: if entry.address==0 { move packet in; set INFLIGHT; bloom/hashmap insert; LPM counters; break; }`
  block_03  L2087-2087  `SANITY_MSHR_EMPTY_FOUND(...)`

## CACHE::get_occupancy  L2090-2098
  block_01  L2091-2097  `switch(queue_type) { 0:MSHR.occupancy; 1:RQ; 2:WQ; 3:PQ; default:0 }`

## CACHE::get_size  L2100-2109
  block_01  L2101-2108  `switch(queue_type) { 0:MSHR.SIZE; 1:RQ; 2:WQ; 3:PQ; default:0 }`

## CACHE::increment_WQ_FULL  L2111-2114
  block_01  L2113-2113  `WQ.FULL++;`

## CACHE::prefetcher_feedback  L2116-2122
  block_01  L2117-2121  `pref_gen=pf_issued; pref_fill=pf_fill; pref_used=pf_useful; pref_late=pf_late;`
