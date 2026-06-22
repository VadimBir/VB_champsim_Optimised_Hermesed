
# CONTENT PAGE
**FORMAT: <CLASS>:<METHOD>:<BLOCK>**
- CACHE:handle_read [method]
- CACHE:handle_read:early_exit [block]
- CACHE:handle_read:hit_path_processed_queue [block]
- CACHE:handle_read:hit_path_bypass_l1_l2c_processed [block]
- CACHE:handle_read:hit_path_bypass_l2_llc_return [block]
- CACHE:handle_read:hit_path_prefetcher_update [block]
- CACHE:handle_read:hit_path_replacement_update [block]
- CACHE:handle_read:hit_path_stats [block]
- CACHE:handle_read:hit_path_return_to_upper [block]
- CACHE:handle_read:hit_path_prefetch_useful [block]
- CACHE:handle_read:miss_bypass_counter [block]
- CACHE:handle_read:miss_bypass_l1d [block]
- CACHE:handle_read:miss_bypass_l2c [block]
- CACHE:handle_read:miss_bypass_llc [block]
- CACHE:handle_read:miss_new_llc [block]
- CACHE:handle_read:miss_new_other [block]
- CACHE:handle_read:miss_mshr_full [block]
- CACHE:handle_read:miss_mshr_merge_rfo [block]
- CACHE:handle_read:miss_mshr_merge_instr [block]
- CACHE:handle_read:miss_mshr_merge_data [block]
- CACHE:handle_read:miss_mshr_fill_level_update [block]
- CACHE:handle_read:miss_bypass_l1_mismatch [block]
- CACHE:handle_read:miss_bypass_l2_mismatch [block]
- CACHE:handle_read:miss_prefetch_takeover [block]
- CACHE:handle_read:miss_non_prefetch_merge [block]
- CACHE:handle_read:miss_handled_prefetcher [block]
- CACHE:handle_read:miss_handled_stats_remove [block]
- CACHE:set_force_all_hits [method]
- CACHE:set_force_all_hits:set_force_all_hits_flag [block]
- CACHE:handle_fill [method]
- CACHE:handle_fill:early_exit [block]
- CACHE:handle_fill:find_victim [block]
- CACHE:handle_fill:llc_bypass_path [block]
- CACHE:handle_fill:dirty_writeback [block]
- CACHE:handle_fill:prefetcher_fill_notify [block]
- CACHE:handle_fill:update_replacement [block]
- CACHE:handle_fill:collect_miss_stats [block]
- CACHE:handle_fill:fill_cache [block]
- CACHE:handle_fill:rfo_dirty_mark [block]
- CACHE:handle_fill:return_to_upper [block]
- CACHE:handle_fill:processed_queue_update [block]
- CACHE:handle_fill:latency_accounting [block]
- CACHE:handle_fill:mshr_remove
- CACHE:handle_writeback [method]
- CACHE:handle_writeback:early_exit [block]
- CACHE:handle_writeback:hit_path [block]
- CACHE:handle_writeback:rfo_miss_l1d [block]
- CACHE:handle_writeback:writeback_miss_find_victim [block]
- CACHE:handle_writeback:dirty_eviction_writeback [block]
- CACHE:handle_writeback:fill_and_return [block]
- CACHE:merge_with_prefetch [method]
- CACHE:merge_with_prefetch:save_prior [block]
- CACHE:merge_with_prefetch:overwrite [block]
- CACHE:merge_with_prefetch:restore_prior [block]
- CACHE:handle_prefetch [method]
- CACHE:handle_prefetch:early_exit [block]
- CACHE:handle_prefetch:hit_path [block]
- CACHE:handle_prefetch:miss_new_llc [block]
- CACHE:handle_prefetch:miss_new_other [block]
- CACHE:handle_prefetch:miss_mshr_full [block]
- CACHE:handle_prefetch:miss_mshr_merge_fill_update [block]
- CACHE:handle_prefetch:miss_handled_stats_remove [block]
- CACHE:operate [method]
- CACHE:operate:lpm_hit_miss_counters [block]
- CACHE:operate:lpm_bypass_detection [block]
- CACHE:operate:lpm_tick [block]
- CACHE:operate:dispatch [block]
- CACHE:get_set [method]
- CACHE:get_way [method]
- CACHE:fill_cache [method]
- CACHE:fill_cache:fill_body [block]
- CACHE:check_hit [method]
- CACHE:check_hit:force_all_hits_path [block]
- CACHE:check_hit:hit_scan [block]
- CACHE:invalidate_entry [method]
- CACHE:invalidate_entry:invalidate_scan [block]
- CACHE:add_rq [method]
- CACHE:add_rq:wq_hit_data_return [block]
- CACHE:add_rq:wq_hit_l1d_processed [block]
- CACHE:add_rq:wq_hit_bypass_l1_processed [block]
- CACHE:add_rq:wq_hit_bypass_l2_return [block]
- CACHE:add_rq:wq_hit_stats [block]
- CACHE:add_rq:rq_merge_instr [block]
- CACHE:add_rq:rq_merge_rfo [block]
- CACHE:add_rq:rq_merge_load [block]
- CACHE:add_rq:rq_merge_stats [block]
- CACHE:add_rq:rq_full_check [block]
- CACHE:add_rq:rq_enqueue [block]
- CACHE:add_wq [method]
- CACHE:add_wq:wq_duplicate_check [block]
- CACHE:add_wq:wq_enqueue [block]
- CACHE:prefetch_line [method]
- CACHE:add_pq [method]
- CACHE:add_pq:wq_forward [block]
- CACHE:add_pq:pq_duplicate_check [block]
- CACHE:add_pq:pq_full_check [block]
- CACHE:add_pq:pq_enqueue [block]
- CACHE:return_data [method]
- CACHE:return_data:mshr_lookup [block]
- CACHE:return_data:mshr_update [block]
- CACHE:return_data:bypass_l1_dep_merge [block]
- CACHE:return_data:update_fill_cycle_call [block]
- CACHE:update_fill_cycle [method]
- CACHE:probe_mshr [method]
- CACHE:check_mshr [method]
- CACHE:check_mshr:address_match [block]
- CACHE:check_mshr:not_found_dp [block]
- CACHE:add_mshr [method]
- CACHE:add_mshr:mshr_insert [block]
- CACHE:get_occupancy [method]
- CACHE:get_size [method]
- CACHE:increment_WQ_FULL [method]
- CACHE:prefetcher_feedback [method]
# CACHE
## set_force_all_hits
    ### set_force_all_hits_flag
#### set_force_all_hits_flag
## handle_fill
    Lines 177–441. Processes the next ready MSHR entry: finds a victim, optionally bypasses LLC fill, handles dirty eviction writeback, fills the cache line, returns data upstream, and updates PROCESSED queue.
    ### early_exit
            Lines 180–182. Reads `MSHR.next_fill_index`; if `== MSHR_SIZE` sets `fill_cpu = NUM_CPUS` and returns immediately. Guards all subsequent logic.
            Lines 186–189. `#ifdef TRUE_SANITY_CHECK`. Asserts `MSHR.next_fill_index < MSHR.SIZE`. Dead-code guard, not reachable in normal runs.
    ### find_victim
            Lines 193–199. Computes `set = get_set(address)`. Calls `llc_find_victim(...)` for IS_LLC, else `find_victim(...)`. Returns `way` index (may be `LLC_WAY` sentinel for bypass).
    ### llc_bypass_path
            Lines 201–241. `#ifdef LLC_BYPASS`. Active when `cache_type == IS_LLC && way == LLC_WAY`. Calls `llc_update_replacement_state` (no fill). Increments `sim_miss` / `sim_access` / `sim_miss_wByP` / `sim_access_wByP`. If `fill_level < fill_level` returns data to `upper_level_icache` or `upper_level_dcache`. Records miss latency. Removes MSHR entry, decrements `num_returned`, calls `update_fill_cycle()`, then `return` — skips normal fill path entirely.
    ### dirty_writeback
            Lines 244–290. Sets `do_fill = 1`. If `block[set][way].dirty`: checks `lower_level` WQ occupancy. If WQ full: sets `do_fill = 0`, increments `WQ_FULL`, increments `STALL`. If WQ has room: constructs `PACKET writeback_packet` with `fill_level << 1`, `type = WRITEBACK`, calls `lower_level->add_wq(...)`. `#ifdef BYPASS_DEBUG` logs if MSHR packet had `l1_bypassed == 1`. `#ifdef TRUE_SANITY_CHECK` asserts non-STLB has a `lower_level`.
    ### prefetcher_fill_notify
            Lines 292–305. Inside `do_fill` block. Calls per-type prefetcher fill hook: `l1d_prefetcher_cache_fill` for IS_L1D, `l2c_prefetcher_cache_fill` for IS_L2C (updates `pf_metadata`), `llc_prefetcher_cache_fill` for IS_LLC (sets `cpu = fill_cpu` temporarily).
    ### update_replacement
            Lines 307–312. Calls `llc_update_replacement_state` for IS_LLC, else `update_replacement_state`. Passes victim's `full_addr` as the evicted address argument.
    ### collect_miss_stats
            Lines 314–320. Increments `sim_miss[fill_cpu][type]`, `sim_access[fill_cpu][type]`. If `type == LOAD` also increments `sim_miss_wByP[fill_cpu]` and `sim_access_wByP[fill_cpu]`.
    ### fill_cache
            Line 322. Calls `fill_cache(set, way, &MSHR.entry[mshr_index])`. Writes data into cache block array.
    ### rfo_dirty_mark
            Lines 324–328. If `cache_type == IS_L1D && type == RFO`: sets `block[set][way].dirty = 1`. L2C RFO handling is commented out with assertion.
    ### return_to_upper
            Lines 334–341. If `MSHR.entry[mshr_index].fill_level < fill_level`: calls `upper_level_icache[fill_cpu]->return_data(...)` for instruction packets, else `upper_level_dcache[fill_cpu]->return_data(...)` for data packets.
    ### processed_queue_update
            Lines 344–424. Adds packet to `PROCESSED` queue based on cache type:
- IS_ITLB: sets `instruction_pa`, adds to PROCESSED; asserts on overflow.
- IS_DTLB: sets `data_pa`, adds to PROCESSED; asserts on overflow.
- IS_L1I: adds to PROCESSED; asserts on overflow.
- IS_L1D (non-PREFETCH): adds to PROCESSED with DP debug print; asserts on overflow.
- `#ifdef BYPASS_L1_LOGIC` IS_L2C + LOAD + `l1_bypassed==1`: adds to PROCESSED (bypass completion path).
- `#ifdef BYPASS_L2_LOGIC` IS_LLC + LOAD + `l2_bypassed==1`: calls `upper_level_dcache[fill_cpu]->upper_level_dcache[fill_cpu]->return_data(...)` — double-hop to skip L2 and return directly to L1D. Does NOT add to PROCESSED.
    ### latency_accounting
            Lines 426–429. If `warmup_complete[fill_cpu]`: computes `current_miss_latency = current_cycle - cycle_enqueued`, adds to `total_miss_latency`.
    ### mshr_remove
            Lines 435–438. Calls `MSHR.remove_queue(&MSHR.entry[mshr_index])`, decrements `MSHR.num_returned`, calls `update_fill_cycle()`.
## handle_writeback
    Lines 502–732. Processes the oldest WQ entry: handles writeback hits (marks dirty, optionally returns data upstream), RFO misses at L1D (MSHR insertion or merge), and writeback misses at other levels (find victim, evict dirty line, fill cache).
    ### early_exit
            Lines 504–506. Reads `WQ.entry[WQ.head].cpu`; if `== NUM_CPUS` returns immediately. Guards all subsequent logic.
    ### hit_path
            Lines 509–553. Active when `way >= 0` (writeback hit or RFO hit at L1D). Calls `llc_update_replacement_state` for IS_LLC, else `update_replacement_state`. Increments `sim_hit` and `sim_access`. Sets `block[set][way].dirty = 1`. For TLB types: copies block data into `instruction_pa` / `data_pa` / `data` on the WQ entry. If `fill_level < this->fill_level`: returns data via `upper_level_icache` or `upper_level_dcache`. Increments `HIT` and `ACCESS`. Calls `WQ.remove_queue`.
    ### rfo_miss_l1d
            Lines 554–625. Active when `cache_type == IS_L1D` and `way < 0` (RFO miss). Sets `miss_handled = 1`. Calls `check_mshr`. If new miss and MSHR has room: checks lower-level RQ occupancy; if full sets `miss_handled = 0`; else calls `add_mshr` + `lower_level->add_rq`. If MSHR full: sets `miss_handled = 0`, increments `STALL`. If already in MSHR: updates `fill_level` if lower; if MSHR entry is PREFETCH calls `merge_with_prefetch`; increments `MSHR_MERGED`. If `miss_handled`: increments `MISS` and `ACCESS`, calls `WQ.remove_queue`.
    ### writeback_miss_find_victim
            Lines 628–641. Active for non-L1D writeback miss. Calls `llc_find_victim` for IS_LLC, else `find_victim`. `#ifdef LLC_BYPASS`: if IS_LLC and `way == LLC_WAY` prints error and asserts — bypassing writebacks is not allowed.
    ### dirty_eviction_writeback
            Lines 643–681. Sets `do_fill = 1`. If `block[set][way].dirty` and `lower_level` exists: checks lower-level WQ occupancy. If WQ full: sets `do_fill = 0`, calls `increment_WQ_FULL`, increments `STALL`. If WQ has room: constructs `PACKET writeback_packet` with `fill_level << 1`, `type = WRITEBACK`, calls `lower_level->add_wq`. `#ifdef TRUE_SANITY_CHECK`: if no `lower_level` and not IS_STLB, asserts.
    ### fill_and_return
            Lines 683–728. Inside `do_fill` block. Calls per-type prefetcher fill hook: `l1d_prefetcher_cache_fill` for IS_L1D, `l2c_prefetcher_cache_fill` for IS_L2C (updates `pf_metadata`), `llc_prefetcher_cache_fill` for IS_LLC (sets `cpu = writeback_cpu` temporarily). Calls `llc_update_replacement_state` for IS_LLC, else `update_replacement_state`. Increments `sim_miss` and `sim_access`. Calls `fill_cache`. Sets `block[set][way].dirty = 1`. If `fill_level < this->fill_level`: returns data via `upper_level_icache` or `upper_level_dcache`. Increments `MISS` and `ACCESS`. Calls `WQ.remove_queue`.
## handle_read
    Lines 733–1338. Processes up to MAX_READ read queue entries per cycle. For each entry: checks cache hit/miss, routes hits to PROCESSED or returns data upstream, applies bypass decisions on new misses, inserts MSHR or merges with existing MSHR, and calls downstream `add_rq` as needed.
    ### early_exit
            Lines 738–742. Reads `RQ.entry[RQ.head].cpu`; if `== NUM_CPUS` returns immediately. Also exits at lines 1330–1333 if `event_cycle > current_core_cycle` or `RQ.occupancy == 0`. At line 1334–1336 exits loop early if `reads_available_this_cycle == 0`.
    ### hit_path_processed_queue
            Lines 756–815. Active when `way >= 0` (read hit). Dispatches to PROCESSED queue per cache type: IS_ITLB sets `instruction_pa` then adds to PROCESSED (asserts on overflow); IS_DTLB sets `data_pa` then adds; IS_STLB sets `data`; IS_L1I adds to PROCESSED; IS_L1D (non-PREFETCH) adds to PROCESSED with DP debug print.
    ### hit_path_bypass_l1_l2c_processed
            Lines 816–843. `#ifdef BYPASS_L1_LOGIC`. Active when `cache_type == IS_L2C && type == LOAD && l1_bypassed == 1 && !instruction`. Adds packet to L2C's own PROCESSED queue — bypass hit return path: L1D was bypassed, L2C services as the fill level, PROCESSED will route back to CPU.
    ### hit_path_bypass_l2_llc_return
            Lines 845–853. `#ifdef BYPASS_L2_LOGIC`. Active when `cache_type == IS_LLC && type == LOAD && l2_bypassed == 1 && !instruction`. Calls `upper_level_dcache[read_cpu]->upper_level_dcache[read_cpu]->return_data(...)` — double-hop: LLC's upper is L2, L2's upper is L1D; skips L2 fill entirely and returns directly to L1D.
    ### hit_path_prefetcher_update
            Lines 856–867. If `type == LOAD`: calls `l1d_prefetcher_operate` for IS_L1D, `l2c_prefetcher_operate` for IS_L2C, or `llc_prefetcher_operate` for IS_LLC (temporarily sets `cpu = read_cpu`). Notifies prefetcher of a hit.
    ### hit_path_replacement_update
            Lines 869–874. Calls `llc_update_replacement_state` for IS_LLC, else `update_replacement_state`. Marks the way as recently used.
    ### hit_path_stats
            Lines 876–882. Increments `sim_hit[read_cpu][type]` and `sim_access[read_cpu][type]`. If `type == LOAD`: also increments `sim_hit_wByP[read_cpu]` and `sim_access_wByP[read_cpu]`.
    ### hit_path_return_to_upper
            Lines 884–891. If `RQ.entry[index].fill_level < fill_level`: calls `upper_level_icache[read_cpu]->return_data(...)` for instruction packets, else `upper_level_dcache[read_cpu]->return_data(...)`. Propagates hit data to requesting upper cache.
    ### hit_path_prefetch_useful
            Lines 893–904. If `block[set][way].prefetch`: increments `pf_useful`, clears `prefetch` bit, sets `used = 1`. Increments `HIT[type]` and `ACCESS[type]`. Calls `RQ.remove_queue`, decrements `reads_available_this_cycle`.
    ### miss_bypass_counter
            Lines 910–915. `#if defined(BYPASS_L1D_OnNewMiss) || defined(BYPASS_L2_LOGIC) || defined(BYPASS_LLC_LOGIC)`. Active when `warmup_complete && (IS_L1D || IS_L2C || IS_LLC) && type == LOAD && mshr_index == -1 && lower_level->RQ has room`. Increments `total_ByP_req[read_cpu]` and `ByP_req[read_cpu]`. Counts opportunities regardless of which bypass fires.
    ### miss_bypass_l1d
            Lines 916–933. `#ifdef BYPASS_L1D_OnNewMiss`. Active when `warmup_complete && IS_L1D && type == LOAD && mshr_index == -1 && lower_level->RQ has room && l1d_bypass_operate(...)`. Sets `l1_bypassed = 1`, `fill_level = FILL_L2`, increments bypass stats, calls `lower_level->add_rq(...)`. Skips L1D MSHR insertion entirely.
    ### miss_bypass_l2c
            Lines 935–953. `#ifdef BYPASS_L2_LOGIC`. Active when `warmup_complete && IS_L2C && type == LOAD && !instruction && mshr_index == -1 && l1_bypassed == 0 && lower_level->RQ has room && l2c_bypass_operate(...)`. Sets `l2_bypassed = 1`, `fill_level = FILL_LLC`, increments bypass stats, calls `lower_level->add_rq(...)`. Skips L2C MSHR insertion.
    ### miss_bypass_llc
            Lines 955–973. `#ifdef BYPASS_LLC_LOGIC`. Active when `warmup_complete && IS_LLC && type == LOAD && mshr_index == -1 && l2_bypassed == 0 && lower_level->RQ has room && llc_bypass_operate(...)`. Sets `llc_bypassed = 1`, `fill_level = FILL_DRAM`, increments bypass stats, calls `lower_level->add_rq(...)`. Skips LLC MSHR insertion.
    ### miss_new_llc
            Lines 975–995. Active when `mshr_index == -1 && MSHR.occupancy < MSHR_SIZE && cache_type == IS_LLC`. Checks DRAM RQ occupancy; if full sets `miss_handled = 0`. Else calls `add_mshr` then `lower_level->add_rq(...)`.
    ### miss_new_other
            Lines 996–1035. Active when `mshr_index == -1 && MSHR.occupancy < MSHR_SIZE` and not IS_LLC. Checks lower-level RQ occupancy; if full sets `miss_handled = 0`. Else calls `add_mshr` then `lower_level->add_rq(...)`. Edge case: if no `lower_level` and `cache_type == IS_STLB`, emulates page table walk via `va_to_pa`, sets `data`, and calls `return_data` in-place.
    ### miss_mshr_full
            Lines 1038–1042. Active when `mshr_index == -1 && MSHR.occupancy == MSHR_SIZE`. Sets `miss_handled = 0`, increments `STALL[type]`.
    ### miss_mshr_merge_rfo
            Lines 1062–1076. Active when `mshr_index != -1 && type == RFO`. Asserts `l1_bypassed == 0` on bypass RFO (not expected). If `tlb_access`: sets `store_merged = 1`, inserts/joins `sq_index_depend_on_me`. If `load_merged`: sets `load_merged = 1`, joins `lq_index_depend_on_me`.
    ### miss_mshr_merge_instr
            Lines 1078–1091. Active when `mshr_index != -1 && instruction`. Inserts `rob_index` into `rob_index_depend_on_me`, sets `instr_merged = 1`. If already `instr_merged`: joins `rob_index_depend_on_me`.
    ### miss_mshr_merge_data
            Lines 1092–1115. Active when `mshr_index != -1 && !instruction && !RFO`. Inserts `lq_index` into `lq_index_depend_on_me`, sets `load_merged = 1`. Joins accumulated `lq_index_depend_on_me`. If `store_merged`: propagates `sq_index_depend_on_me` into MSHR.
    ### miss_mshr_fill_level_update
            Lines 1124–1132. Active when `mshr_index != -1`. If `RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level`: updates `MSHR.entry[mshr_index].fill_level`.
    ### miss_bypass_l1_mismatch
            Lines 1134–1218. `#ifdef BYPASS_L1_LOGIC`. Active when `cache_type == IS_L2C && RQ.entry[index].l1_bypassed != MSHR.entry[mshr_index].l1_bypassed`. If MSHR type is not PREFETCH: searches L1D MSHR for matching address; if found, inserts LQ deps into L1D MSHR, clears `l1_bypassed` on both RQ and MSHR entries, resets `fill_level = 1`. If MSHR is PREFETCH and its `fill_level < fill_level`: clears `l1_bypassed` on RQ entry (normal return path takes over).
    ### miss_bypass_l2_mismatch
            Lines 1219–1263. `#ifdef BYPASS_L2_LOGIC`. Active when `cache_type == IS_LLC && RQ.entry[index].l2_bypassed != MSHR.entry[mshr_index].l2_bypassed`. Mirrors the L1 mismatch logic at the LLC level: searches L2C MSHR, inserts LQ deps, clears `l2_bypassed` + resets `fill_level = FILL_L2` if L2C MSHR found. If MSHR is PREFETCH with lower fill_level: clears `l2_bypassed`.
    ### miss_prefetch_takeover
            Lines 1266–1283. Active when `MSHR.entry[mshr_index].type == PREFETCH`. Increments `pf_late`. Calls `merge_with_prefetch(MSHR.entry[mshr_index], RQ.entry[index])`. Then under respective `#ifdef` guards: restores `l1_bypassed`, `l2_bypassed`, `llc_bypassed` from RQ entry onto MSHR entry (overrides what `merge_with_prefetch` would have set from the demand packet).
    ### miss_non_prefetch_merge
            Lines 1290–1301. Active when `MSHR.entry[mshr_index].type != PREFETCH` and already-in-MSHR path. Inserts `rob_index` (instruction), `lq_index` (LOAD), or `sq_index` (RFO) into the corresponding MSHR dependency set; sets merge flag. Increments `MSHR_MERGED[type]`.
    ### miss_handled_prefetcher
            Lines 1310–1322. Active when `miss_handled == 1`. If `type == LOAD`: calls `l1d_prefetcher_operate`, `l2c_prefetcher_operate`, or `llc_prefetcher_operate` per cache type. Notifies prefetcher of a miss.
    ### miss_handled_stats_remove
            Lines 1323–1328. Active when `miss_handled == 1`. Increments `MISS[type]` and `ACCESS[type]`. Calls `RQ.remove_queue`, decrements `reads_available_this_cycle`.
## merge_with_prefetch
    Lines 443–500. Merges a demand packet (`queue_packet`) into an existing MSHR prefetch entry (`mshr_packet`), preserving routing/bypass fields that must survive the overwrite.
    ### save_prior
            Lines 445–457. Saves `mshr_packet.returned` and `mshr_packet.event_cycle` unconditionally. `#ifdef BYPASS_L1_LOGIC`: also saves `fill_level` and `l1_bypassed`. `#ifdef BYPASS_L2_LOGIC`: saves `l2_bypassed`. `#ifdef BYPASS_LLC_LOGIC`: saves `llc_bypassed`.
    ### overwrite
            Line 462. `mshr_packet = queue_packet`. Full struct copy; all fields from the demand packet replace the prefetch entry.
    ### restore_prior
            Lines 464–477. Restores `returned` and `event_cycle` unconditionally. Under respective `#ifdef` guards: restores `fill_level`, `l1_bypassed`, `l2_bypassed`, `llc_bypassed`. Ensures bypass routing and timing state from the original MSHR entry survive the overwrite.
## handle_prefetch
    Lines 1340–1528. Processes up to MAX_READ prefetch queue entries per cycle. For each entry: checks cache hit/miss, on hit updates replacement and returns data upstream, on miss inserts MSHR or merges with existing (including bypass fill_level adjustment), calls downstream add_rq/add_pq.
    ### early_exit
            Lines 1342–1346, 1521–1526. Reads `PQ.entry[PQ.head].cpu`; if `== NUM_CPUS` returns immediately. Also returns if `event_cycle > current_core_cycle` or `PQ.occupancy == 0`. Returns at end of loop if `reads_available_this_cycle == 0`.
    ### hit_path
            Lines 1355–1390. Active when `way >= 0` (prefetch hit). Calls `llc_update_replacement_state` for IS_LLC, else `update_replacement_state`. Increments `sim_hit` and `sim_access`. If `pf_origin_level < fill_level`: calls per-type prefetcher hit hook (`l1d_prefetcher_operate`, `l2c_prefetcher_operate`, or `llc_prefetcher_operate` with temporary `cpu` set). If `fill_level < fill_level`: returns data via `upper_level_icache` or `upper_level_dcache`. Increments `HIT` and `ACCESS`. Calls `PQ.remove_queue`, decrements `reads_available_this_cycle`.
    ### miss_new_llc
            Lines 1402–1429. Active when `mshr_index == -1 && MSHR.occupancy < MSHR_SIZE && cache_type == IS_LLC`. Checks DRAM RQ (`get_occupancy(1,...)`) occupancy; if full sets `miss_handled = 0`. Else: if `pf_origin_level < fill_level` runs `llc_prefetcher_operate` (with temporary `cpu` set). If `fill_level <= fill_level` calls `add_mshr`. Calls `lower_level->add_rq(...)` (DRAM RQ).
    ### miss_new_other
            Lines 1430–1450. Active when `mshr_index == -1 && MSHR.occupancy < MSHR_SIZE` and not IS_LLC. Checks lower-level PQ occupancy (`get_occupancy(3,...)`); if full sets `miss_handled = 0`. Else: if `pf_origin_level < fill_level` runs `l1d_prefetcher_operate` or `l2c_prefetcher_operate`. If `fill_level <= fill_level` calls `add_mshr`. Calls `lower_level->add_pq(...)`. Asserts on `-2` return (MSHR added but add_pq failed).
    ### miss_mshr_full
            Lines 1453–1457. Active when `mshr_index == -1 && MSHR.occupancy == MSHR_SIZE`. Sets `miss_handled = 0`, increments `STALL[type]`.
    ### miss_mshr_merge_fill_update
            Lines 1458–1505. Active when `mshr_index != -1` (already-in-flight miss). If `RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level`: updates `fill_level` on MSHR entry. `#ifdef BYPASS_L1_LOGIC`: if MSHR entry has `l1_bypassed == 1`, calls `upper_level_dcache[cpu]->probe_mshr(...)` on L1D; if L1D MSHR found, downgrades `fill_level` and clears `l1_bypassed`. `#ifdef BYPASS_L2_LOGIC`: mirrors same logic for `l2_bypassed` using L2C `probe_mshr`. Increments `MSHR_MERGED[type]` and logs DP debug info. Asserts on unexpected MSHR state.
    ### miss_handled_stats_remove
            Lines 1507–1519. Active when `miss_handled == 1`. Increments `MISS[type]` and `ACCESS[type]`. Calls `PQ.remove_queue`, decrements `reads_available_this_cycle`.
## CACHE:operate
    Lines 1531–1612. Top-level per-cycle dispatch. Conditionally accumulates LPM metrics then dispatches fill, writeback, read, and prefetch handlers.
    ### lpm_hit_miss_counters
            Lines 1538–1566. Active only when `warmup_complete[cpu]`. Computes: `α` = sum of `sim_access[cpu][t]` over all NUM_TYPES; `hit_active` = any occupancy in RQ/WQ/PQ; `miss_active` = `#ifdef LPM_STRICT_MISS`: any MSHR entry with `address != 0 && returned != COMPLETED`; else simply `MSHR.occupancy > 0`.
    ### lpm_bypass_detection
            Lines 1568–1599. Active only when `warmup_complete[cpu]`. Sets `has_byp = false`, then: `#ifdef BYPASS_L1_LOGIC`: if IS_L2C, scans MSHR for any entry with `l1_bypassed > 0`; `#ifdef BYPASS_L2_LOGIC`: if IS_LLC, scans MSHR for `l2_bypassed > 0`; `#ifdef BYPASS_LLC_LOGIC`: if IS_LLC, scans MSHR for `llc_bypassed > 0`. Sets `has_byp = true` on first match found in any scan.
    ### lpm_tick
            Lines 1601–1603. Calls `lpm_operate(cpu, cache_type, hit_active, miss_active, α, has_byp)` to advance LPM state machine for this cache level.
    ### dispatch
            Lines 1606–1611. Unconditional (outside warmup guard). Calls `handle_fill()`, `handle_writeback()`, resets `reads_available_this_cycle = MAX_READ`, calls `handle_read()`, then calls `handle_prefetch()` if `PQ.occupancy > 0 && reads_available_this_cycle > 0`.
## CACHE:get_set
    Line 1614–1616. Extracts the set index from `address` using `(address & ((1 << lg2(NUM_SET)) - 1))`. No sub-blocks.
## CACHE:get_way
    Lines 1618–1625. Scans all ways for `block[set][way].valid && tag == address`; returns matching `way` index or `NUM_WAY` on miss. No sub-blocks.
## CACHE:fill_cache
    Lines 1627–1672. Writes packet fields into `block[set][way]`: valid, dirty, prefetch, used, tag, address, full_addr, data, cpu, instr_id. Increments `pf_useless` if a prefetch block with `used==0` is being replaced. Increments `pf_fill` if new line is a prefetch.
    ### fill_body
            Lines 1644–1667. Main fill: increments `pf_useless` if evicting unused prefetch; sets `valid=1`, `dirty=0`, `prefetch` (from type), `used=0`; increments `pf_fill` if prefetch; writes tag, address, full_addr, data, cpu, instr_id from packet.
## CACHE:check_hit
    Lines 1674–1711. Returns the matching way index (≥0) on hit, or `-1` on miss. Optionally forces a hit via `FORCE_ALL_HITS`.
    ### force_all_hits_path
            Lines 1681–1687. Active when `FORCE_ALL_HITS`. Sets `block[set][0].valid=1` and `tag=packet->address`, returns `0` unconditionally.
    ### hit_scan
            Lines 1697–1708. Iterates all ways; on `valid && tag == address` sets `match_way = way`, emits DP print, breaks. Returns `match_way` (-1 if no hit found).
## CACHE:invalidate_entry
    Lines 1713–1736. Finds the way matching `inval_addr`, sets `block[set][way].valid=0`, returns matching way index or `-1`.
    ### invalidate_scan
            Lines 1723–1734. Iterates all ways; on `valid && tag == inval_addr`: sets `valid=0`, records `match_way`, emits DP print, breaks.
## CACHE:add_rq
    Lines 1738–1989. Adds a packet to the read queue. First checks WQ for a recent matching writeback (WQ forward); then checks for RQ duplicate (merge); then checks occupancy; finally enqueues at tail.
    ### wq_hit_data_return
            Lines 1751–1761. Active when `wq_index != -1` and `packet->fill_level < fill_level`. Copies `data` from WQ entry; calls `upper_level_icache` or `upper_level_dcache` `return_data`. Asserts `l1_bypassed` consistency between WQ and new packet.
    ### wq_hit_l1d_processed
            Lines 1771–1779. Active when `cache_type == IS_L1D && type != PREFETCH`. Adds packet to PROCESSED queue if room. Emits DP log.
    ### wq_hit_bypass_l1_processed
            Lines 1784–1797. `#ifdef BYPASS_L1_LOGIC`. Active when `cache_type == IS_L2C && type == LOAD && l1_bypassed == 1 && !instruction`. Adds to PROCESSED (bypass WQ-forward completion path). Emits DP log.
    ### wq_hit_bypass_l2_return
            Lines 1799–1810. `#ifdef BYPASS_L2_LOGIC`. Active when `cache_type == IS_LLC && type == LOAD && l2_bypassed == 1 && !instruction`. Calls `upper_level_dcache[cpu]->upper_level_dcache[cpu]->return_data(packet)` — double-hop to L1D, skipping L2 fill. Emits DP log.
    ### wq_hit_stats
            Lines 1811–1817. Increments `HIT[type]`, `ACCESS[type]`, `WQ.FORWARD`, `RQ.ACCESS`. Returns `-1`.
    ### rq_merge_instr
            Lines 1834–1840. Active when `index != -1 && packet->instruction`. Inserts `rob_index` into `RQ.entry[index].rob_index_depend_on_me`, sets `instr_merged=1`.
    ### rq_merge_rfo
            Lines 1843–1884. Active when `index != -1 && type == RFO`. Inserts `sq_index`, joins `sq_index_depend_on_me`, sets `store_merged=1`. `#ifdef BYPASS_L1_LOGIC`: if IS_L2C and `l1_bypassed` mismatch, clears `l1_bypassed` and updates `fill_level`. `#ifdef BYPASS_L2_LOGIC`: same for IS_LLC / `l2_bypassed`. `#ifdef BYPASS_LLC_LOGIC`: same for `llc_bypassed`. Emits DP dump.
    ### rq_merge_load
            Lines 1886–1937. Active when `index != -1 && !instruction && type != RFO`. Inserts `lq_index`, joins `lq_index_depend_on_me`, sets `load_merged=1`. `#ifdef BYPASS_L2_LOGIC`: IS_LLC `l2_bypassed` mismatch → clear both sides, update fill_level. `#ifdef BYPASS_LLC_LOGIC`: `llc_bypassed` mismatch → same. `#ifdef BYPASS_L1_LOGIC`: IS_L2C `l1_bypassed` mismatch → clear both, update fill_level, emit DP.
    ### rq_merge_stats
            Lines 1940–1942. Increments `RQ.MERGED` and `RQ.ACCESS`. Returns `index`.
    ### rq_full_check
            Lines 1945–1949. Active when `RQ.occupancy == RQ_SIZE`. Increments `RQ.FULL`, returns `-2`.
    ### rq_enqueue
            Lines 1951–1988. Finds tail slot; `#ifdef TRUE_SANITY_CHECK` asserts slot is empty. Moves `*packet` into `RQ.entry[index]`. Applies latency. Increments `occupancy`, advances `tail` (wraps). Emits DP log. `#ifdef TRUE_SANITY_CHECK` asserts `address != 0`. Increments `RQ.TO_CACHE` and `RQ.ACCESS`. Returns `-1`.
## CACHE:add_wq
    Lines 1991–2036. Adds a packet to the write queue. Checks for duplicate; if none, enqueues at tail.
    ### wq_duplicate_check
            Lines 1993–1998. Calls `WQ.check_queue(packet)`; if found increments `WQ.MERGED` and `WQ.ACCESS`, returns merged index.
            Lines 1999–2013. `#ifdef TRUE_SANITY_CHECK`. Asserts `WQ.occupancy < WQ.SIZE`. Asserts tail slot has `address == 0 && l1_bypassed == 0`.
    ### wq_enqueue
            Lines 2015–2035. Moves `*packet` into `WQ.entry[index]`. Applies latency. Increments `occupancy`, advances `tail` (wraps). Emits DP log. Increments `WQ.TO_CACHE` and `WQ.ACCESS`. Returns `-1`.
## CACHE:prefetch_line
    Lines 2038–2064. Creates a `PACKET` with type=PREFETCH from `ip`, `pf_addr`, `pf_fill_level`, `prefetch_metadata`. Only enqueues if `PQ.occupancy < PQ.SIZE` and `pf_addr` is on the same page as `base_addr`. Calls `add_pq`; increments `pf_issued`. Returns `1` on success, `0` otherwise. Increments `pf_requested` unconditionally. No sub-blocks.
## CACHE:add_pq
    Lines 2066–2147. Adds a packet to the prefetch queue. Checks WQ for recent writeback (forward); checks PQ for duplicate; checks occupancy; enqueues at tail.
    ### wq_forward
            Lines 2068–2085. WQ hit path: copies data, calls `return_data` if `fill_level < fill_level`. Increments `HIT[type]`, `ACCESS[type]`, `WQ.FORWARD`, `PQ.ACCESS`. Returns `-1`.
    ### pq_duplicate_check
            Lines 2088–2097. Calls `PQ.check_queue(packet)`; if found updates `fill_level` if lower, increments `PQ.MERGED` and `PQ.ACCESS`, returns merged index.
    ### pq_full_check
            Lines 2099–2105. Active when `PQ.occupancy == PQ_SIZE`. Increments `PQ.FULL`, emits DP log. Returns `-2`.
    ### pq_enqueue
            Lines 2107–2146. `#ifdef TRUE_SANITY_CHECK` asserts tail slot empty. Moves `*packet` into `PQ.entry[index]`. Applies latency. Increments `occupancy`, advances `tail` (wraps). Emits DP log. `#ifdef TRUE_SANITY_CHECK` asserts `address != 0` (unless `FORCE_ALL_HITS`). Increments `PQ.TO_CACHE` and `PQ.ACCESS`. Returns `-1`.
## CACHE:return_data
    Lines 2149–2283. Called by lower cache level to signal a completed miss. Finds the matching MSHR entry, marks it COMPLETED, updates data and latency, merges bypass-accumulated dependencies, then calls `update_fill_cycle()`.
    ### mshr_lookup
            Line 2155. Calls `check_mshr(packet)` to find `mshr_index`. Two `#ifdef BYPASS_DEBUG` prints (before/after) are filtered.
            Lines 2161–2170. `#ifdef TRUE_SANITY_CHECK`. Asserts `mshr_index != -1`; logs cache name, addresses, cycles on failure.
    ### mshr_update
            Lines 2183–2192. Increments `MSHR.num_returned`. Sets `MSHR.entry[mshr_index].returned = COMPLETED`, copies `data` and `pf_metadata` from packet. Applies `LATENCY` to `event_cycle`.
    ### bypass_l1_dep_merge
            Lines 2193–2249. `#ifdef BYPASS_L1_LOGIC`. Four cases at IS_L1D:
- Prefetch-promotion: if MSHR type is PREFETCH and arriving type is LOAD, overwrites MSHR type/instr_id/lq_index/rob_index/full_addr/ip; merges `lq_index_depend_on_me`; removes self from dep set.
- LOAD→LOAD deps: if both MSHR and packet are LOAD, joins `lq_index_depend_on_me`; inserts `packet->lq_index` if instr_ids differ.
- LOAD→RFO deps: if MSHR is RFO and packet is LOAD, sets `load_merged=1`, inserts lq_index; propagates load/store deps from packet.
- RFO→RFO deps: if both RFO, propagates `load_merged`/`store_merged` dep sets from packet.
    ### update_fill_cycle_call
            Line 2270. Calls `update_fill_cycle()` to recompute next ready MSHR entry.
## CACHE:update_fill_cycle
    Lines 2285–2312. Scans all MSHR entries for `returned == COMPLETED`; finds the one with minimum `event_cycle`; sets `MSHR.next_fill_cycle` and `MSHR.next_fill_index`. No sub-blocks.
## CACHE:probe_mshr
    Lines 2314–2320. Scans MSHR for `address == packet->address`; returns index or `-1`. Does not check bypass flags or type. No sub-blocks.
## CACHE:check_mshr
    Lines 2322–2375. Searches MSHR for a matching address. Optionally also requires `l1_bypassed` match under `#ifdef BYPASS_L1_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS`. On match, applies bypass downgrade (clears `l1_bypassed`, `l2_bypassed`, or `llc_bypassed` if incoming packet has lower bypass state and type==LOAD). Returns matched index or `-1`.
    ### address_match
            Lines 2325–2360. Inner match block: conditional on address (and optionally `l1_bypassed`). Under respective `#ifdef` guards: if MSHR entry has bypass flag set but packet does not (and packet is LOAD), clears the flag and updates `fill_level`. Emits DP dump. Returns matched index.
    ### not_found_dp
            Lines 2362–2374. Active when no match found. Emits DP log noting address not in MSHR. Returns `-1`.
## CACHE:add_mshr
    Lines 2377–2412. Finds an empty MSHR slot (`address == 0`), moves `*packet` in, sets `returned = INFLIGHT`, increments `MSHR.occupancy`. A `#ifdef BYPASS_DEBUG` stall-detection print inside the scan loop is filtered.
    ### mshr_insert
            Lines 2381–2395. Scans for empty slot; moves packet; sets INFLIGHT; increments occupancy; emits DP log; breaks.
## CACHE:get_occupancy
    Lines 2414–2422. Switch on `queue_type`: 0→`MSHR.occupancy`, 1→`RQ.occupancy`, 2→`WQ.occupancy`, 3→`PQ.occupancy`, default→0. No sub-blocks.
## CACHE:get_size
    Lines 2424–2433. Switch on `queue_type`: 0→`MSHR.SIZE`, 1→`RQ.SIZE`, 2→`WQ.SIZE`, 3→`PQ.SIZE`, default→0. No sub-blocks.
## CACHE:increment_WQ_FULL
    Lines 2435–2438. Increments `WQ.FULL`. No sub-blocks.
## CACHE:prefetcher_feedback
    Lines 2440–2446. Assigns out-parameters: `pref_gen = pf_issued`, `pref_fill = pf_fill`, `pref_used = pf_useful`, `pref_late = pf_late`. No sub-blocks.