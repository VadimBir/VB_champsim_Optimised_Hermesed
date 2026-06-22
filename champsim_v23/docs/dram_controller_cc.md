# CONTENT PAGE
**FORMAT: <CLASS>:<METHOD>:<BLOCK>**
- print_dram_config [method]
- print_dram_config:print_config_params [block]
- MEMORY_CONTROLLER:reset_remain_requests [method]
- MEMORY_CONTROLLER:reset_remain_requests:scan_scheduled_entries [block]
- MEMORY_CONTROLLER:reset_remain_requests:scan_scheduled_entries:open_row_update [block]
- MEMORY_CONTROLLER:reset_remain_requests:scan_scheduled_entries:bank_request_clear [block]
- MEMORY_CONTROLLER:reset_remain_requests:scan_scheduled_entries:write_read_counter_decrement [block]
- MEMORY_CONTROLLER:reset_remain_requests:scan_scheduled_entries:entry_cycle_reset [block]
- MEMORY_CONTROLLER:reset_remain_requests:update_schedule_process_cycles [block]
- MEMORY_CONTROLLER:operate [method]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:warmup_skip [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:rq_active_check [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:wq_active_check [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:bank_working_check [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:alpha_compute [block]
- MEMORY_CONTROLLER:operate:lpm_tick_per_cpu:lpm_update [block]
- MEMORY_CONTROLLER:operate:write_mode_control_per_channel [block]
- MEMORY_CONTROLLER:operate:write_mode_control_per_channel:enter_write_mode [block]
- MEMORY_CONTROLLER:operate:write_mode_control_per_channel:exit_write_mode [block]
- MEMORY_CONTROLLER:operate:wq_schedule_dispatch [block]
- MEMORY_CONTROLLER:operate:wq_process_dispatch [block]
- MEMORY_CONTROLLER:operate:rq_schedule_dispatch [block]
- MEMORY_CONTROLLER:operate:rq_process_dispatch [block]
- MEMORY_CONTROLLER:schedule [method]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan [block]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan:skip_scheduled [block]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan:skip_empty [block]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan:skip_busy_bank [block]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan:skip_row_mismatch [block]
- MEMORY_CONTROLLER:schedule:row_buffer_hit_scan:oldest_hit_select [block]
- MEMORY_CONTROLLER:schedule:row_buffer_miss_scan [block]
- MEMORY_CONTROLLER:schedule:row_buffer_miss_scan:skip_scheduled [block]
- MEMORY_CONTROLLER:schedule:row_buffer_miss_scan:skip_empty [block]
- MEMORY_CONTROLLER:schedule:row_buffer_miss_scan:skip_busy_bank [block]
- MEMORY_CONTROLLER:schedule:row_buffer_miss_scan:oldest_miss_select [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:latency_compute [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:bank_mark_working [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:write_read_counter_increment [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:open_row_update [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:entry_scheduled_set [block]
- MEMORY_CONTROLLER:schedule:bank_request_commit:update_cycles [block]
- MEMORY_CONTROLLER:process [method]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:wq_writeback_complete [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:rq_dbus_update [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:llc_bypass_return_or_normal_return [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:row_buffer_stat_update [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:bank_request_clear [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_available_branch:queue_remove_update [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_congested_branch [block]
- MEMORY_CONTROLLER:process:bank_cycle_ready_check:dbus_congested_branch:congestion_accounting [block]
- MEMORY_CONTROLLER:add_rq [method]
- MEMORY_CONTROLLER:add_rq:pre_warmup_return [block]
- MEMORY_CONTROLLER:add_rq:pre_warmup_return:instruction_icache_return [block]
- MEMORY_CONTROLLER:add_rq:pre_warmup_return:data_llc_bypass_or_normal_return [block]
- MEMORY_CONTROLLER:add_rq:wq_forward_check [block]
- MEMORY_CONTROLLER:add_rq:wq_forward_check:wq_hit_return_data [block]
- MEMORY_CONTROLLER:add_rq:wq_forward_check:wq_stats_update [block]
- MEMORY_CONTROLLER:add_rq:rq_duplicate_check [block]
- MEMORY_CONTROLLER:add_rq:rq_empty_slot_insert [block]
- MEMORY_CONTROLLER:add_rq:rq_empty_slot_insert:slot_fill [block]
- MEMORY_CONTROLLER:add_rq:rq_empty_slot_insert:schedule_cycle_update [block]
- MEMORY_CONTROLLER:add_rq:rq_full_return [block]
- MEMORY_CONTROLLER:add_wq [method]
- MEMORY_CONTROLLER:add_wq:pre_warmup_drop [block]
- MEMORY_CONTROLLER:add_wq:wq_duplicate_check [block]
- MEMORY_CONTROLLER:add_wq:wq_empty_slot_insert [block]
- MEMORY_CONTROLLER:add_wq:wq_empty_slot_insert:slot_fill [block]
- MEMORY_CONTROLLER:add_wq:wq_empty_slot_insert:schedule_cycle_update [block]
- MEMORY_CONTROLLER:add_wq:wq_full_return [block]
- MEMORY_CONTROLLER:add_pq [method]
- MEMORY_CONTROLLER:add_pq:return_not_supported [block]
- MEMORY_CONTROLLER:return_data [method]
- MEMORY_CONTROLLER:return_data:empty_stub [block]
- MEMORY_CONTROLLER:update_schedule_cycle [method]
- MEMORY_CONTROLLER:update_schedule_cycle:unscheduled_min_scan [block]
- MEMORY_CONTROLLER:update_schedule_cycle:next_schedule_fields_update [block]
- MEMORY_CONTROLLER:update_process_cycle [method]
- MEMORY_CONTROLLER:update_process_cycle:scheduled_min_scan [block]
- MEMORY_CONTROLLER:update_process_cycle:next_process_fields_update [block]
- MEMORY_CONTROLLER:check_dram_queue [method]
- MEMORY_CONTROLLER:check_dram_queue:address_match_scan [block]
- MEMORY_CONTROLLER:check_dram_queue:return_not_found [block]
- MEMORY_CONTROLLER:dram_get_channel [method]
- MEMORY_CONTROLLER:dram_get_channel:zero_log2_check [block]
- MEMORY_CONTROLLER:dram_get_channel:address_extract_channel [block]
- MEMORY_CONTROLLER:dram_get_bank [method]
- MEMORY_CONTROLLER:dram_get_bank:zero_log2_check [block]
- MEMORY_CONTROLLER:dram_get_bank:address_extract_bank [block]
- MEMORY_CONTROLLER:dram_get_column [method]
- MEMORY_CONTROLLER:dram_get_column:zero_log2_check [block]
- MEMORY_CONTROLLER:dram_get_column:address_extract_column [block]
- MEMORY_CONTROLLER:dram_get_rank [method]
- MEMORY_CONTROLLER:dram_get_rank:zero_log2_check [block]
- MEMORY_CONTROLLER:dram_get_rank:address_extract_rank [block]
- MEMORY_CONTROLLER:dram_get_row [method]
- MEMORY_CONTROLLER:dram_get_row:zero_log2_check [block]
- MEMORY_CONTROLLER:dram_get_row:address_extract_row [block]
- MEMORY_CONTROLLER:get_occupancy [method]
- MEMORY_CONTROLLER:get_occupancy:channel_resolve [block]
- MEMORY_CONTROLLER:get_occupancy:rq_or_wq_occupancy_return [block]
- MEMORY_CONTROLLER:get_size [method]
- MEMORY_CONTROLLER:get_size:channel_resolve [block]
- MEMORY_CONTROLLER:get_size:rq_or_wq_size_return [block]
- MEMORY_CONTROLLER:increment_WQ_FULL [method]
- MEMORY_CONTROLLER:increment_WQ_FULL:channel_resolve_and_increment [block]

---

# print_dram_config

## print_dram_config

### print_config_params

---

# MEMORY_CONTROLLER

## reset_remain_requests

### scan_scheduled_entries

#### open_row_update

#### bank_request_clear

#### write_read_counter_decrement

#### entry_cycle_reset

### update_schedule_process_cycles

---

## operate

### lpm_tick_per_cpu

#### warmup_skip

#### rq_active_check

#### wq_active_check

#### bank_working_check

#### alpha_compute

#### lpm_update

### write_mode_control_per_channel

#### enter_write_mode

#### exit_write_mode

### wq_schedule_dispatch

### wq_process_dispatch

### rq_schedule_dispatch

### rq_process_dispatch

---

## schedule

### row_buffer_hit_scan

#### skip_scheduled

#### skip_empty

#### skip_busy_bank

#### skip_row_mismatch

#### oldest_hit_select

### row_buffer_miss_scan

#### skip_scheduled

#### skip_empty

#### skip_busy_bank

#### oldest_miss_select

### bank_request_commit

#### latency_compute

#### bank_mark_working

#### write_read_counter_increment

#### open_row_update

#### entry_scheduled_set

#### update_cycles

---

## process

### bank_cycle_ready_check

#### dbus_available_branch

##### wq_writeback_complete

##### rq_dbus_update

##### llc_bypass_return_or_normal_return

##### row_buffer_stat_update

##### bank_request_clear

##### queue_remove_update

#### dbus_congested_branch

##### congestion_accounting

---

## add_rq

### pre_warmup_return

#### instruction_icache_return

#### data_llc_bypass_or_normal_return

### wq_forward_check

#### wq_hit_return_data

#### wq_stats_update

### rq_duplicate_check

### rq_empty_slot_insert

#### slot_fill

#### schedule_cycle_update

### rq_full_return

---

## add_wq

### pre_warmup_drop

### wq_duplicate_check

### wq_empty_slot_insert

#### slot_fill

#### schedule_cycle_update

### wq_full_return

---

## add_pq

### return_not_supported

---

## return_data

### empty_stub

---

## update_schedule_cycle

### unscheduled_min_scan

### next_schedule_fields_update

---

## update_process_cycle

### scheduled_min_scan

### next_process_fields_update

---

## check_dram_queue

### address_match_scan

### return_not_found

---

## dram_get_channel

### zero_log2_check

### address_extract_channel

---

## dram_get_bank

### zero_log2_check

### address_extract_bank

---

## dram_get_column

### zero_log2_check

### address_extract_column

---

## dram_get_rank

### zero_log2_check

### address_extract_rank

---

## dram_get_row

### zero_log2_check

### address_extract_row

---

## get_occupancy

### channel_resolve

### rq_or_wq_occupancy_return

---

## get_size

### channel_resolve

### rq_or_wq_size_return

---

## increment_WQ_FULL

### channel_resolve_and_increment
