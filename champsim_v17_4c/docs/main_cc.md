# CONTENT PAGE
**FORMAT: <CLASS>:<METHOD>:<BLOCK>**
- FinalStatsCollector:collectROIStats [method]
- FinalStatsCollector:collectROIStats:prefix_build [block]
- FinalStatsCollector:collectROIStats:cache_stats_store [block]
- FinalStatsCollector:collectSimStats [method]
- FinalStatsCollector:collectSimStats:prefix_build [block]
- FinalStatsCollector:collectSimStats:cache_stats_store [block]
- FinalStatsCollector:collectBranchStats [method]
- FinalStatsCollector:collectBranchStats:prefix_build [block]
- FinalStatsCollector:collectBranchStats:branch_stats_store [block]
- FinalStatsCollector:collectDRAMStats [method]
- FinalStatsCollector:collectDRAMStats:prefix_build [block]
- FinalStatsCollector:collectDRAMStats:dram_channel_stats_store [block]
- FinalStatsCollector:collectPageFaultStats [method]
- FinalStatsCollector:collectPageFaultStats:prefix_build [block]
- FinalStatsCollector:collectPageFaultStats:fault_stats_store [block]
- FinalStatsCollector:collectCoreROIStats [method]
- FinalStatsCollector:collectCoreROIStats:prefix_build [block]
- FinalStatsCollector:collectCoreROIStats:instr_cycle_ipc_store [block]
- FinalStatsCollector:dumpAllAsString [method]
- FinalStatsCollector:dumpAllAsString:json_dict_serialize [block]
- record_roi_stats [method]
- record_roi_stats:copy_sim_to_roi [block]
- print_pf_hitRatio [method]
- print_pf_hitRatio:return_pf_hit_ratio [block]
- print_L2_hitRatio [method]
- print_L2_hitRatio:aggregate_totals [block]
- print_L2_hitRatio:return_hit_ratio [block]
- print_L2_usefulRatio [method]
- print_L2_usefulRatio:return_useful_ratio [block]
- print_roi_stats [method]
- print_roi_stats:aggregate_totals [block]
- print_roi_stats:print_stall_bypass_lpm_stats [block]
- print_roi_stats:print_access_hit_miss_stats [block]
- print_roi_stats:print_bypass_with_bypass_stats [block]
- print_roi_stats:print_rfo_prefetch_writeback_stats [block]
- print_roi_stats:print_prefetch_counters [block]
- print_roi_stats:print_miss_latency [block]
- print_roi_stats:collect_roi_stats_call [block]
- print_sim_stats [method]
- print_sim_stats:aggregate_totals [block]
- print_sim_stats:print_per_type_stats [block]
- print_sim_stats:collect_sim_stats_call [block]
- print_branch_stats [method]
- print_branch_stats:print_branch_metrics [block]
- print_branch_stats:collect_branch_stats_call [block]
- print_dram_stats [method]
- print_dram_stats:per_channel_print_and_collect [block]
- print_dram_stats:congested_cycle_print [block]
- print_dram_stats:ipc_summary_print [block]
- reset_cache_stats [method]
- reset_cache_stats:zero_access_hit_miss_stall [block]
- reset_cache_stats:zero_bypass_counters [block]
- reset_cache_stats:zero_queue_stats [block]
- finish_warmup [method]
- finish_warmup:elapsed_time_compute [block]
- finish_warmup:latency_reset [block]
- finish_warmup:per_cpu_warmup_complete_print [block]
- finish_warmup:per_cpu_warmup_complete_print:begin_sim_markers_set [block]
- finish_warmup:per_cpu_warmup_complete_print:branch_stats_reset [block]
- finish_warmup:per_cpu_warmup_complete_print:cache_stats_reset [block]
- finish_warmup:per_cpu_warmup_complete_print:lpm_reset [block]
- finish_warmup:dram_stats_reset [block]
- finish_warmup:cache_latency_set [block]
- print_deadlock [method]
- print_deadlock:deadlock_cycle_check [block]
- print_deadlock:deadlock_cycle_check:rob_head_state_print [block]
- print_deadlock:deadlock_cycle_check:l1d_mshr_print [block]
- print_deadlock:deadlock_cycle_check:l2c_mshr_print [block]
- print_deadlock:deadlock_cycle_check:load_queue_print [block]
- print_deadlock:deadlock_cycle_check:store_queue_print [block]
- print_deadlock:deadlock_cycle_check:num_store_count [block]
- print_deadlock:deadlock_cycle_check:deadlock_assert [block]
- signal_handler [method]
- signal_handler:signal_print_and_exit [block]
- lg2 [method]
- lg2:log2_compute_loop [block]
- rotl64 [method]
- rotl64:rotate_left_compute [block]
- rotr64 [method]
- rotr64:rotate_right_compute [block]
- va_to_pa [method]
- va_to_pa:unique_va_build [block]
- va_to_pa:cl_footprint_track [block]
- va_to_pa:page_table_lookup [block]
- va_to_pa:page_table_lookup:page_replacement_path [block]
- va_to_pa:page_table_lookup:page_replacement_path:nru_page_find [block]
- va_to_pa:page_table_lookup:page_replacement_path:page_table_remap [block]
- va_to_pa:page_table_lookup:page_replacement_path:inverse_table_update [block]
- va_to_pa:page_table_lookup:page_replacement_path:cache_invalidate [block]
- va_to_pa:page_table_lookup:page_replacement_path:swap_flag_set [block]
- va_to_pa:page_table_lookup:new_page_alloc_path [block]
- va_to_pa:page_table_lookup:new_page_alloc_path:ppage_search_loop [block]
- va_to_pa:page_table_lookup:new_page_alloc_path:page_table_insert [block]
- va_to_pa:page_table_lookup:new_page_alloc_path:fragmentation_recalc [block]
- va_to_pa:page_table_lookup:fault_count_increment [block]
- va_to_pa:pa_build [block]
- va_to_pa:stall_cycle_set [block]
- print_knobs [method]
- print_knobs:warmup_sim_seed_print [block]
- print_knobs:hardware_config_print [block]
- print_knobs:core_dram_config_print [block]
- main [method]
- main:db_cfg_init [block]
- main:instr_dest_type_warn [block]
- main:signal_handler_setup [block]
- main:banner_print [block]
- main:knob_defaults_init [block]
- main:getopt_loop [block]
- main:getopt_loop:warmup_instructions_set [block]
- main:getopt_loop:simulation_instructions_set [block]
- main:getopt_loop:show_heartbeat_set [block]
- main:getopt_loop:cloudsuite_set [block]
- main:getopt_loop:low_bandwidth_set [block]
- main:getopt_loop:traces_flag_set [block]
- main:getopt_loop:db_path_set [block]
- main:getopt_loop:arch_set [block]
- main:getopt_loop:bypass_set [block]
- main:getopt_loop:pf_l1_set [block]
- main:getopt_loop:pf_l2_set [block]
- main:getopt_loop:pf_l3_set [block]
- main:getopt_loop:epoch_set [block]
- main:dram_timing_compute [block]
- main:trace_file_open_loop [block]
- main:trace_file_open_loop:gz_trace_open [block]
- main:trace_file_open_loop:xz_trace_open [block]
- main:trace_file_open_loop:extension_validate [block]
- main:trace_file_open_loop:filename_tokenize_seed [block]
- main:trace_count_validate [block]
- main:seed_and_lpm_init [block]
- main:per_cpu_struct_init [block]
- main:per_cpu_struct_init:rob_cpu_set [block]
- main:per_cpu_struct_init:branch_predictor_init [block]
- main:per_cpu_struct_init:tlb_init [block]
- main:per_cpu_struct_init:l1i_l1d_init [block]
- main:per_cpu_struct_init:l2c_init [block]
- main:per_cpu_struct_init:llc_init [block]
- main:per_cpu_struct_init:dram_init [block]
- main:per_cpu_struct_init:state_vars_reset [block]
- main:llc_replacement_prefetcher_init [block]
- main:print_knobs_call [block]
- main:simulation_main_loop [block]
- main:simulation_main_loop:cycle_advance [block]
- main:simulation_main_loop:stall_skip_check [block]
- main:simulation_main_loop:stall_skip_check:handle_branch_call [block]
- main:simulation_main_loop:stall_skip_check:fetch_instruction_call [block]
- main:simulation_main_loop:stall_skip_check:schedule_instruction_call [block]
- main:simulation_main_loop:stall_skip_check:schedule_memory_instruction_call [block]
- main:simulation_main_loop:stall_skip_check:execute_instruction_call [block]
- main:simulation_main_loop:stall_skip_check:execute_memory_instruction_call [block]
- main:simulation_main_loop:stall_skip_check:update_rob_call [block]
- main:simulation_main_loop:stall_skip_check:retire_rob_call [block]
- main:simulation_main_loop:heartbeat_print [block]
- main:simulation_main_loop:heartbeat_print:bypass_pct_compute [block]
- main:simulation_main_loop:heartbeat_print:lpm_apc_lpm_camat_print [block]
- main:simulation_main_loop:heartbeat_print:mshr_load_hit_rate_print [block]
- main:simulation_main_loop:heartbeat_print:interval_bypass_reset [block]
- main:simulation_main_loop:deadlock_check [block]
- main:simulation_main_loop:warmup_complete_check [block]
- main:simulation_main_loop:simulation_complete_check [block]
- main:simulation_main_loop:uncore_operate [block]
- main:roi_stats_print_loop [block]
- main:roi_stats_print_loop:core_instr_cycle_ipc_print [block]
- main:roi_stats_print_loop:branch_roi_cache_stats_print [block]
- main:roi_stats_print_loop:page_fault_print [block]
- main:roi_stats_print_loop:lpm_print_call [block]
- main:roi_stats_print_loop:dram_lpm_print [block]
- main:prefetcher_final_stats [block]
- main:llc_final_stats [block]
- main:dram_stats_print [block]
- main:execution_checksum_print [block]
- main:final_ipc_print [block]
- main:db_store_call [block]

---

# FinalStatsCollector

## collectROIStats

### prefix_build

### cache_stats_store

---

## collectSimStats

### prefix_build

### cache_stats_store

---

## collectBranchStats

### prefix_build

### branch_stats_store

---

## collectDRAMStats

### prefix_build

### dram_channel_stats_store

---

## collectPageFaultStats

### prefix_build

### fault_stats_store

---

## collectCoreROIStats

### prefix_build

### instr_cycle_ipc_store

---

## dumpAllAsString

### json_dict_serialize

---

# record_roi_stats

## record_roi_stats

### copy_sim_to_roi

---

# print_pf_hitRatio

## print_pf_hitRatio

### return_pf_hit_ratio

---

# print_L2_hitRatio

## print_L2_hitRatio

### aggregate_totals

### return_hit_ratio

---

# print_L2_usefulRatio

## print_L2_usefulRatio

### return_useful_ratio

---

# print_roi_stats

## print_roi_stats

### aggregate_totals

### print_stall_bypass_lpm_stats

### print_access_hit_miss_stats

### print_bypass_with_bypass_stats

### print_rfo_prefetch_writeback_stats

### print_prefetch_counters

### print_miss_latency

### collect_roi_stats_call

---

# print_sim_stats

## print_sim_stats

### aggregate_totals

### print_per_type_stats

### collect_sim_stats_call

---

# print_branch_stats

## print_branch_stats

### print_branch_metrics

### collect_branch_stats_call

---

# print_dram_stats

## print_dram_stats

### per_channel_print_and_collect

### congested_cycle_print

### ipc_summary_print

---

# reset_cache_stats

## reset_cache_stats

### zero_access_hit_miss_stall

### zero_bypass_counters

### zero_queue_stats

---

# finish_warmup

## finish_warmup

### elapsed_time_compute

### latency_reset

### per_cpu_warmup_complete_print

#### begin_sim_markers_set

#### branch_stats_reset

#### cache_stats_reset

#### lpm_reset

### dram_stats_reset

### cache_latency_set

---

# print_deadlock

## print_deadlock

### deadlock_cycle_check

#### rob_head_state_print

#### l1d_mshr_print

#### l2c_mshr_print

#### load_queue_print

#### store_queue_print

#### num_store_count

#### deadlock_assert

---

# signal_handler

## signal_handler

### signal_print_and_exit

---

# lg2

## lg2

### log2_compute_loop

---

# rotl64

## rotl64

### rotate_left_compute

---

# rotr64

## rotr64

### rotate_right_compute

---

# va_to_pa

## va_to_pa

### unique_va_build

### cl_footprint_track

### page_table_lookup

#### page_replacement_path

##### nru_page_find

##### page_table_remap

##### inverse_table_update

##### cache_invalidate

##### swap_flag_set

#### new_page_alloc_path

##### ppage_search_loop

##### page_table_insert

##### fragmentation_recalc

#### fault_count_increment

### pa_build

### stall_cycle_set

---

# print_knobs

## print_knobs

### warmup_sim_seed_print

### hardware_config_print

### core_dram_config_print

---

# main

## main

### db_cfg_init

### instr_dest_type_warn

### signal_handler_setup

### banner_print

### knob_defaults_init

### getopt_loop

#### warmup_instructions_set

#### simulation_instructions_set

#### show_heartbeat_set

#### cloudsuite_set

#### low_bandwidth_set

#### traces_flag_set

#### db_path_set

#### arch_set

#### bypass_set

#### pf_l1_set

#### pf_l2_set

#### pf_l3_set

#### epoch_set

### dram_timing_compute

### trace_file_open_loop

#### gz_trace_open

#### xz_trace_open

#### extension_validate

#### filename_tokenize_seed

### trace_count_validate

### seed_and_lpm_init

### per_cpu_struct_init

#### rob_cpu_set

#### branch_predictor_init

#### tlb_init

#### l1i_l1d_init

#### l2c_init

#### llc_init

#### dram_init

#### state_vars_reset

### llc_replacement_prefetcher_init

### print_knobs_call

### simulation_main_loop

#### cycle_advance

#### stall_skip_check

##### handle_branch_call

##### fetch_instruction_call

##### schedule_instruction_call

##### schedule_memory_instruction_call

##### execute_instruction_call

##### execute_memory_instruction_call

##### update_rob_call

##### retire_rob_call

#### heartbeat_print

##### bypass_pct_compute

##### lpm_apc_lpm_camat_print

##### mshr_load_hit_rate_print

##### interval_bypass_reset

#### deadlock_check

#### warmup_complete_check

#### simulation_complete_check

#### uncore_operate

### roi_stats_print_loop

#### core_instr_cycle_ipc_print

#### branch_roi_cache_stats_print

#### page_fault_print

#### lpm_print_call

#### dram_lpm_print

### prefetcher_final_stats

### llc_final_stats

### dram_stats_print

### execution_checksum_print

### final_ipc_print

### db_store_call
