# Graph Report - champsim_VB  (2026-04-24)

## Corpus Check
- 544 files · ~31,667,216 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 1795 nodes · 5095 edges · 56 communities detected
- Extraction: 89% EXTRACTED · 11% INFERRED · 0% AMBIGUOUS · INFERRED: 544 edges (avg confidence: 0.79)
- Token cost: 0 input · 0 output

## Community Hubs (Navigation)
- [[_COMMUNITY_Community 0|Community 0]]
- [[_COMMUNITY_Community 1|Community 1]]
- [[_COMMUNITY_Community 2|Community 2]]
- [[_COMMUNITY_Community 3|Community 3]]
- [[_COMMUNITY_Community 4|Community 4]]
- [[_COMMUNITY_Community 5|Community 5]]
- [[_COMMUNITY_Community 6|Community 6]]
- [[_COMMUNITY_Community 7|Community 7]]
- [[_COMMUNITY_Community 8|Community 8]]
- [[_COMMUNITY_Community 9|Community 9]]
- [[_COMMUNITY_Community 10|Community 10]]
- [[_COMMUNITY_Community 11|Community 11]]
- [[_COMMUNITY_Community 12|Community 12]]
- [[_COMMUNITY_Community 13|Community 13]]
- [[_COMMUNITY_Community 14|Community 14]]
- [[_COMMUNITY_Community 15|Community 15]]
- [[_COMMUNITY_Community 16|Community 16]]
- [[_COMMUNITY_Community 17|Community 17]]
- [[_COMMUNITY_Community 18|Community 18]]
- [[_COMMUNITY_Community 19|Community 19]]
- [[_COMMUNITY_Community 20|Community 20]]
- [[_COMMUNITY_Community 21|Community 21]]
- [[_COMMUNITY_Community 22|Community 22]]
- [[_COMMUNITY_Community 23|Community 23]]
- [[_COMMUNITY_Community 24|Community 24]]
- [[_COMMUNITY_Community 25|Community 25]]
- [[_COMMUNITY_Community 26|Community 26]]
- [[_COMMUNITY_Community 27|Community 27]]
- [[_COMMUNITY_Community 28|Community 28]]
- [[_COMMUNITY_Community 29|Community 29]]
- [[_COMMUNITY_Community 30|Community 30]]
- [[_COMMUNITY_Community 31|Community 31]]
- [[_COMMUNITY_Community 32|Community 32]]
- [[_COMMUNITY_Community 33|Community 33]]
- [[_COMMUNITY_Community 34|Community 34]]
- [[_COMMUNITY_Community 35|Community 35]]
- [[_COMMUNITY_Community 36|Community 36]]
- [[_COMMUNITY_Community 37|Community 37]]
- [[_COMMUNITY_Community 38|Community 38]]
- [[_COMMUNITY_Community 39|Community 39]]
- [[_COMMUNITY_Community 40|Community 40]]
- [[_COMMUNITY_Community 41|Community 41]]
- [[_COMMUNITY_Community 42|Community 42]]
- [[_COMMUNITY_Community 43|Community 43]]
- [[_COMMUNITY_Community 44|Community 44]]
- [[_COMMUNITY_Community 45|Community 45]]
- [[_COMMUNITY_Community 46|Community 46]]
- [[_COMMUNITY_Community 47|Community 47]]
- [[_COMMUNITY_Community 48|Community 48]]
- [[_COMMUNITY_Community 49|Community 49]]
- [[_COMMUNITY_Community 50|Community 50]]
- [[_COMMUNITY_Community 51|Community 51]]
- [[_COMMUNITY_Community 52|Community 52]]
- [[_COMMUNITY_Community 53|Community 53]]
- [[_COMMUNITY_Community 174|Community 174]]
- [[_COMMUNITY_Community 175|Community 175]]

## God Nodes (most connected - your core abstractions)
1. `getHash()` - 82 edges
2. `main()` - 51 edges
3. `getHash()` - 38 edges
4. `folded_xor()` - 38 edges
5. `handle_read()` - 31 edges
6. `process()` - 29 edges
7. `add()` - 29 edges
8. `add_rq()` - 27 edges
9. `handle_prefetch()` - 26 edges
10. `max()` - 26 edges

## Surprising Connections (you probably didn't know these)
- `resize()` --calls--> `init_ddrp_buffer()`  [INFERRED]
  champsim_v10_ByP_L1Fille_incHermess/inc/unordered_dense.h → champsim_v10_HERMES/src/hermes/dram_hermes.cc
- `add()` --calls--> `noteSent()`  [INFERRED]
  champsim_v10_ByP_L1Fille_incHermess/inc/hermes/xxhash32.h → _claude_backup/plugins/marketplaces/claude-plugins-official/external_plugins/discord/server.ts
- `run_h2o()` --calls--> `init()`  [INFERRED]
  find_ipc_automl.py → champsim_v10_ByP_L1Fille_incHermess/inc/lpm_tracker.h
- `run_h2o()` --calls--> `shutdown()`  [INFERRED]
  find_ipc_automl.py → _claude_backup/plugins/marketplaces/claude-plugins-official/external_plugins/imessage/server.ts
- `main()` --calls--> `max()`  [INFERRED]
  find_ipc_automl.py → champsim_v10_ByP_L1Fille_incHermess/inc/hermes/util.h

## Communities

### Community 0 - "Community 0"
Cohesion: 0.06
Nodes (106): base_sort_key(), build_sheets(), build_sheets_multilevel(), build_sheets_triple(), build_sheets_triple_covbyp(), _col_letter(), collect(), _coverage_formula() (+98 more)

### Community 1 - "Community 1"
Cohesion: 0.07
Nodes (96): process_address(), process_delta(), process_delta_path(), process_Delta_Path_PC(), process_offset(), process_offset_path(), process_Offset_Path_PC(), process_Page() (+88 more)

### Community 2 - "Community 2"
Cohesion: 0.15
Nodes (72): add_queue(), check_queue(), remove_queue(), add_mshr(), add_pq(), add_rq(), add_wq(), bloom_rebuild_mshr() (+64 more)

### Community 3 - "Community 3"
Cohesion: 0.04
Nodes (84): _apply_group_conditional_formatting(), _avg_formula(), base_sort_key(), build_sheets(), build_sheets_multilevel(), build_sheets_triple(), build_sheets_triple_covbyp(), collect() (+76 more)

### Community 4 - "Community 4"
Cohesion: 0.03
Nodes (75): extract_lpm_table(), extract_stats_from_file(), main(), process_directory(), Extract LPM Cycle Classification table data., Extract all stats from a single log file., Process all log files in directory., build_dataframe() (+67 more)

### Community 5 - "Community 5"
Cohesion: 0.04
Nodes (75): _apply_group_conditional_formatting(), _avg_formula(), base_sort_key(), build_sheets(), build_sheets_multilevel(), build_sheets_triple(), build_sheets_triple_covbyp(), collect() (+67 more)

### Community 6 - "Community 6"
Cohesion: 0.04
Nodes (69): aggregate_results(), calculate_stats(), generate_benchmark(), generate_markdown(), load_run_results(), main(), Aggregate run results into summary statistics.      Returns run_summary with sta, Generate complete benchmark.json from run results. (+61 more)

### Community 7 - "Community 7"
Cohesion: 0.1
Nodes (59): AddrHitTracker(), build_feature_groups(), classify_pf_config(), load_and_filter_data(), main(), Only keep features that are in our desired categories, Run symbolic regression and return results., IPC CORRELATION FORMULA FINDER - Symbolic Regression Finds formula: IPC = f(cach (+51 more)

### Community 8 - "Community 8"
Cohesion: 0.12
Nodes (34): draw_rand(), lg2(), rotl64(), rotr64(), l1d_prefetcher_cache_fill(), l1d_prefetcher_final_stats(), l1d_prefetcher_initialize(), l1d_prefetcher_operate() (+26 more)

### Community 9 - "Community 9"
Cohesion: 0.12
Nodes (47): four_hybrid1(), four_hybrid10(), four_hybrid11(), four_hybrid12(), four_hybrid2(), four_hybrid3(), four_hybrid4(), four_hybrid5() (+39 more)

### Community 10 - "Community 10"
Cohesion: 0.06
Nodes (39): BaseHTTPRequestHandler, build_run(), embed_file(), find_runs(), _find_runs_recursive(), generate_html(), get_mime_type(), _kill_port() (+31 more)

### Community 11 - "Community 11"
Cohesion: 0.2
Nodes (45): compute(), get_kappa_long(), get_kappa_short(), get_LPMR_byp(), get_LPMR_global_byp(), get_LPMR_global_roi_byp(), get_LPMR_global_roi_std(), get_LPMR_global_std() (+37 more)

### Community 12 - "Community 12"
Cohesion: 0.22
Nodes (37): initialize_branch_predictor(), initialize_perceptron(), last_branch_result(), predict_branch(), issue_ddrp_request(), add_load_queue(), add_store_queue(), add_to_rob() (+29 more)

### Community 13 - "Community 13"
Cohesion: 0.08
Nodes (29): champsim_db_store(), _db_exec(), _db_extract_trace_name(), get_ddrp_buffer_set_index(), init_ddrp_buffer(), insert_ddrp_buffer(), lookup_ddrp_buffer(), get_ddrp_buffer_set_index() (+21 more)

### Community 14 - "Community 14"
Cohesion: 0.15
Nodes (36): get_array_float(), get_array_int(), hermes_handler(), hermes_parse_config(), hermes_parse_configs(), hermes_parse_knobs(), check_and_update_act_thresh(), dump_stats() (+28 more)

### Community 15 - "Community 15"
Cohesion: 0.07
Nodes (20): decompress(), DDRPMonitor(), dump_stats(), get_winner_config(), monitor_instr(), print_config(), reset_cycles(), reset_stats() (+12 more)

### Community 16 - "Community 16"
Cohesion: 0.18
Nodes (23): BLOCK(), clear(), CORE_BUFFER(), DRAM_ARRAY(), fast_copy_packet(), LOAD_STORE_QUEUE(), LSQ_ENTRY(), operator() (+15 more)

### Community 17 - "Community 17"
Cohesion: 0.11
Nodes (33): allowedChatGuids(), appleDate(), assertAllowedChat(), assertSendable(), checkApprovals(), chunk(), consumeEcho(), conversationHeader() (+25 more)

### Community 18 - "Community 18"
Cohesion: 0.1
Nodes (29): Condition, extract_frontmatter(), from_dict(), load_rule_file(), load_rules(), A single condition for matching., Load all hookify rules from .claude directory.      Args:         event: Optiona, Load a single rule file.      Returns:         Rule object or None if file is in (+21 more)

### Community 19 - "Community 19"
Cohesion: 0.26
Nodes (24): add_pq(), add_rq(), add_wq(), check_dram_queue(), dram_get_bank(), dram_get_channel(), dram_get_column(), dram_get_rank() (+16 more)

### Community 20 - "Community 20"
Cohesion: 0.3
Nodes (20): value(), BeginInstruction(), BranchOrNot(), dcount(), EndInstruction(), EnterROI(), ExitROI(), Fini() (+12 more)

### Community 21 - "Community 21"
Cohesion: 0.15
Nodes (21): addUsage(), buildByDay(), bumpSkill(), classifyFile(), fmt(), handleUser(), hrs(), inferAgentTypeFromFilename() (+13 more)

### Community 22 - "Community 22"
Cohesion: 0.56
Nodes (10): const_iterator(), else(), exec_limbs(), fbitset(), get_limb(), get_limb_lidx(), get_mask(), is_inplace() (+2 more)

### Community 23 - "Community 23"
Cohesion: 0.12
Nodes (2): DRAM(), UNCORE()

### Community 24 - "Community 24"
Cohesion: 0.52
Nodes (10): find_victim(), GetVictimInSet(), InitReplacementState(), lru_update(), lru_victim(), PrintStats(), PrintStats_Heartbeat(), replacement_final_stats() (+2 more)

### Community 25 - "Community 25"
Cohesion: 0.4
Nodes (4): GLOBAL_REGISTER(), PATTERN_TABLE(), PREFETCH_FILTER(), SIGNATURE_TABLE()

### Community 26 - "Community 26"
Cohesion: 0.2
Nodes (15): check_patterns(), cleanup_old_state_files(), debug_log(), extract_content_from_input(), get_state_file(), load_state(), main(), Get session-specific state file path. (+7 more)

### Community 27 - "Community 27"
Cohesion: 0.17
Nodes (1): STAT_PATTERNS — grep/regex definitions for aggregator.py.  CP1 scope: ONLY ipc_f

### Community 28 - "Community 28"
Cohesion: 0.55
Nodes (5): GLOBAL_REGISTER(), PATTERN_TABLE(), PERCEPTRON(), PREFETCH_FILTER(), SIGNATURE_TABLE()

### Community 29 - "Community 29"
Cohesion: 0.33
Nodes (7): dump_stats(), get_index(), OffchipPredHMPLocal(), predict(), print_config(), reset_stats(), train()

### Community 30 - "Community 30"
Cohesion: 0.33
Nodes (7): dump_stats(), get_hash(), OffchipPredHMPGshare(), predict(), print_config(), reset_stats(), train()

### Community 31 - "Community 31"
Cohesion: 0.53
Nodes (4): l2c_prefetcher_cache_fill(), l2c_prefetcher_final_stats(), l2c_prefetcher_initialize(), l2c_prefetcher_operate()

### Community 32 - "Community 32"
Cohesion: 0.53
Nodes (4): llc_prefetcher_cache_fill(), llc_prefetcher_final_stats(), llc_prefetcher_initialize(), llc_prefetcher_operate()

### Community 33 - "Community 33"
Cohesion: 0.47
Nodes (3): fastset(), LQ_fastset(), SQ_fastset()

### Community 34 - "Community 34"
Cohesion: 0.33
Nodes (2): l1d_bypass_initialize(), l1d_bypass_operate()

### Community 35 - "Community 35"
Cohesion: 0.33
Nodes (2): l2c_bypass_initialize(), l2c_bypass_operate()

### Community 36 - "Community 36"
Cohesion: 0.33
Nodes (2): llc_bypass_initialize(), llc_bypass_operate()

### Community 37 - "Community 37"
Cohesion: 0.31
Nodes (6): dump_stats(), OffchipPredRandom(), predict(), print_config(), reset_stats(), train()

### Community 38 - "Community 38"
Cohesion: 0.31
Nodes (6): dump_stats(), OffchipPredHMPEnsemble(), predict(), print_config(), reset_stats(), train()

### Community 39 - "Community 39"
Cohesion: 0.5
Nodes (3): maybe_wake(), TraceBufEntry(), TraceBuffer()

### Community 40 - "Community 40"
Cohesion: 0.39
Nodes (2): BANK_REQUEST(), WQ()

### Community 41 - "Community 41"
Cohesion: 0.28
Nodes (7): main(), package_skill(), Check if a path should be excluded from packaging., Package a skill folder into a .skill file.      Args:         skill_path: Path t, should_exclude(), Basic validation of a skill, validate_skill()

### Community 42 - "Community 42"
Cohesion: 0.25
Nodes (1): MemoryAccessVis

### Community 43 - "Community 43"
Cohesion: 0.25
Nodes (1): MEMORY_CONTROLLER()

### Community 44 - "Community 44"
Cohesion: 0.25
Nodes (1): MemSchedHeap()

### Community 45 - "Community 45"
Cohesion: 0.29
Nodes (1): reset()

### Community 46 - "Community 46"
Cohesion: 0.4
Nodes (1): XZReader

### Community 47 - "Community 47"
Cohesion: 0.83
Nodes (3): extract_two_col(), flatten_paragraphs(), main()

### Community 48 - "Community 48"
Cohesion: 0.67
Nodes (2): pearson_r(), Calculate Pearson correlation coefficient.

### Community 49 - "Community 49"
Cohesion: 0.67
Nodes (1): OffchipTracer()

### Community 50 - "Community 50"
Cohesion: 0.67
Nodes (1): FNV()

### Community 51 - "Community 51"
Cohesion: 0.67
Nodes (1): BitmapHelper()

### Community 52 - "Community 52"
Cohesion: 0.67
Nodes (1): FeatureKnowledge()

### Community 53 - "Community 53"
Cohesion: 0.67
Nodes (1): ocp_base_feature_t()

### Community 174 - "Community 174"
Cohesion: 1.0
Nodes (1): Create Condition from dict.

### Community 175 - "Community 175"
Cohesion: 1.0
Nodes (1): Create Rule from frontmatter dict and message body.

## Knowledge Gaps
- **215 isolated node(s):** `Safely convert to float.`, `Parse the BULK TSV file - handles variable row lengths.`, `Build pandas DataFrame from parsed rows.`, `Parse the trace data into a dict: benchmark_name -> {model: value}`, `Check if all values are close to 1.0 (unchanged from baseline)` (+210 more)
  These have ≤1 connection - possible missing edges or undocumented components.
- **Thin community `Community 23`** (17 nodes): `uncore.cc`, `uncore.h`, `uncore.cc`, `uncore.h`, `uncore.cc`, `uncore.h`, `uncore.h`, `uncore.cc`, `uncore.h`, `uncore.cc`, `uncore.cc`, `uncore.h`, `uncore.cc`, `uncore.h`, `uncore.cc`, `DRAM()`, `UNCORE()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 27`** (12 nodes): `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `stat_patterns.py`, `STAT_PATTERNS — grep/regex definitions for aggregator.py.  CP1 scope: ONLY ipc_f`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 34`** (10 nodes): `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `ooo_l1_byp_model.cc`, `l1d_bypass_initialize()`, `l1d_bypass_operate()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 35`** (10 nodes): `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `ooo_l2_byp_model.cc`, `l2c_bypass_initialize()`, `l2c_bypass_operate()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 36`** (10 nodes): `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `ooo_llc_byp_model.cc`, `llc_bypass_initialize()`, `llc_bypass_operate()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 40`** (9 nodes): `memory_class.h`, `memory_class.h`, `memory_class.h`, `memory_class.h`, `memory_class.h`, `memory_class.h`, `memory_class.h`, `BANK_REQUEST()`, `WQ()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 42`** (8 nodes): `MemoryAccessVis.cpp`, `MemoryAccessVis.cpp`, `MemoryAccessVis.cpp`, `MemoryAccessVis.cpp`, `MemoryAccessVis.cpp`, `MemoryAccessVis.cpp`, `MemoryAccessVis`, `.MemoryAccessVis()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 43`** (8 nodes): `dram_controller.h`, `dram_controller.h`, `dram_controller.h`, `dram_controller.h`, `dram_controller.h`, `dram_controller.h`, `dram_controller.h`, `MEMORY_CONTROLLER()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 44`** (8 nodes): `instr_event.h`, `instr_event.h`, `instr_event.h`, `instr_event.h`, `instr_event.h`, `instr_event.h`, `instr_event.h`, `MemSchedHeap()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 45`** (7 nodes): `training_unit.h`, `training_unit.h`, `training_unit.h`, `training_unit.h`, `training_unit.h`, `training_unit.h`, `reset()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 46`** (5 nodes): `lzstd_encoder.cc`, `main()`, `worker()`, `XZReader`, `.XZReader()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 48`** (3 nodes): `pearson_r()`, `correlation_analysis.py`, `Calculate Pearson correlation coefficient.`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 49`** (3 nodes): `offchip_tracer.h`, `offchip_tracer.h`, `OffchipTracer()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 50`** (3 nodes): `fnv.h`, `fnv.h`, `FNV()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 51`** (3 nodes): `BitmapHelper()`, `bitmap.h`, `bitmap.h`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 52`** (3 nodes): `feature_knowledge.h`, `feature_knowledge.h`, `FeatureKnowledge()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 53`** (3 nodes): `offchip_pred_base_helper.h`, `offchip_pred_base_helper.h`, `ocp_base_feature_t()`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 174`** (1 nodes): `Create Condition from dict.`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.
- **Thin community `Community 175`** (1 nodes): `Create Rule from frontmatter dict and message body.`
  Too small to be a meaningful cluster - may be noise or needs more connections extracted.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `add()` connect `Community 0` to `Community 3`, `Community 5`, `Community 9`, `Community 12`, `Community 17`, `Community 19`, `Community 21`, `Community 26`?**
  _High betweenness centrality (0.222) - this node is a cross-community bridge._
- **Why does `max()` connect `Community 6` to `Community 0`, `Community 3`, `Community 4`, `Community 5`, `Community 10`, `Community 14`, `Community 20`, `Community 21`?**
  _High betweenness centrality (0.144) - this node is a cross-community bridge._
- **Why does `hash()` connect `Community 9` to `Community 0`, `Community 7`?**
  _High betweenness centrality (0.117) - this node is a cross-community bridge._
- **Are the 44 inferred relationships involving `getHash()` (e.g. with `get_partial_tag()` and `get_index()`) actually correct?**
  _`getHash()` has 44 INFERRED edges - model-reasoned connections that need verification._
- **Are the 33 inferred relationships involving `main()` (e.g. with `initialize_branch_predictor()` and `l1d_prefetcher_initialize()`) actually correct?**
  _`main()` has 33 INFERRED edges - model-reasoned connections that need verification._
- **Are the 37 inferred relationships involving `folded_xor()` (e.g. with `get_partial_tag()` and `get_index()`) actually correct?**
  _`folded_xor()` has 37 INFERRED edges - model-reasoned connections that need verification._
- **Are the 18 inferred relationships involving `handle_read()` (e.g. with `handle_read_hit_processed()` and `handle_read_hit_bypass_return()`) actually correct?**
  _`handle_read()` has 18 INFERRED edges - model-reasoned connections that need verification._