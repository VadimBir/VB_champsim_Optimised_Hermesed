# CONTENT PAGE

- AddrDependencyTracker [class]
- AddrDependencyTracker::init [method]
- AddrDependencyTracker::add_producer [method]
- AddrDependencyTracker::get_latest_producer [method]
- AddrDependencyTracker::remove_producer [method]
- MemDependencyTracker [class]
- MemDependencyTracker::init [method]
- MemDependencyTracker::add_producer [method]
- MemDependencyTracker::get_latest_producer [method]
- MemDependencyTracker::remove_producer [method]
- XZReader [class]
- XZReader::init_decoder [method]
- XZReader::fill_buffer [method]
- XZReader::XZReader [method]
- XZReader::~XZReader [method]
- XZReader::open [method]
- XZReader::read [method]
- XZReader::reopen [method]
- XZReader::close [method]
- XZReader::eof [method]
- O3_CPU [class]
- O3_CPU::verify_xz_trace [method]
- O3_CPU::O3_CPU [method]
- O3_CPU::~O3_CPU [method]
- O3_CPU::handle_branch [method]
- O3_CPU::fetch_instruction [method]
- O3_CPU::schedule_instruction [method]
- O3_CPU::execute_instruction [method]
- O3_CPU::schedule_memory_instruction [method]
- O3_CPU::execute_memory_instruction [method]
- O3_CPU::do_scheduling [method]
- O3_CPU::reg_dependency_simd_v2 [method]
- O3_CPU::apply_raw_dependency [method]
- O3_CPU::reg_dependency [method]
- O3_CPU::do_execution [method]
- O3_CPU::do_memory_scheduling [method]
- O3_CPU::operate_lsq [method]
- O3_CPU::complete_execution [method]
- O3_CPU::reg_RAW_dependency [method]
- O3_CPU::reg_RAW_release [method]
- O3_CPU::handle_o3_fetch [method]
- O3_CPU::handle_merged_translation [method]
- O3_CPU::handle_merged_load [method]
- O3_CPU::release_load_queue [method]
- O3_CPU::complete_instr_fetch [method]
- O3_CPU::complete_data_fetch [method]
- O3_CPU::initialize_core [method]
- O3_CPU::add_load_queue [method]
- O3_CPU::add_store_queue [method]
- O3_CPU::execute_store [method]
- O3_CPU::execute_load [method]
- O3_CPU::check_dependency [method]
- O3_CPU::operate_cache [method]
- O3_CPU::update_rob [method]
- O3_CPU::retire_rob [method]
- O3_CPU::progressive_memory_completion [method]
- O3_CPU::add_to_rob [method]
- O3_CPU::check_rob [method]
- O3_CPU::check_and_add_lsq [method]
- O3_CPU::predict_branch [method]
- O3_CPU::initialize_branch_predictor [method]
- O3_CPU::last_branch_result [method]

# AddrDependencyTracker

## init

### register_map_setup

## add_producer

### register_insert

## get_latest_producer

### rob_window_search

## remove_producer

### register_retire

# MemDependencyTracker

## init

### memory_map_setup

## add_producer

### memory_insert

## get_latest_producer

### memory_window_search

## remove_producer

### memory_retire

# XZReader

## init_decoder

### decoder_init

## fill_buffer

### eof_detection

#### input_buffer_refill

#### decompression_loop

#### output_extraction

## XZReader

### buffer_allocation

### stream_init

## ~XZReader

### buffer_cleanup

### decoder_teardown

## open

### file_open

### decoder_setup

### buffer_reset

## read

### buffer_availability_check

#### buffer_fill_attempt

### buffer_copy_loop

### item_count_calculation

## reopen

### file_reopen

## close

### decoder_end

### file_close

### state_reset

## eof

### eof_check

# O3_CPU

## verify_xz_trace

### trace_read

### memory_unpoison

### validation_check

### reopen_trace

## O3_CPU

### custom_tracker_init

### processor_id_init

### trace_init

### instruction_state_init

### simulation_counter_init

### fetch_config_init

### branch_state_init

### store_array_init

### ready_queues_init

## ~O3_CPU

### destructor_empty

## handle_branch

### branch_prediction

## fetch_instruction

### instruction_fetch

## schedule_instruction

### instruction_scheduling

## execute_instruction

### instruction_execution

## schedule_memory_instruction

### memory_scheduling

## execute_memory_instruction

### memory_execution

## do_scheduling

### scheduling_logic

## reg_dependency_simd_v2

### simd_dependency_analysis

## apply_raw_dependency

### raw_apply

## reg_dependency

### register_dependency

## do_execution

### execution_logic

## do_memory_scheduling

### memory_scheduling_logic

## operate_lsq

### lsq_operation

## complete_execution

### execution_completion

## reg_RAW_dependency

### raw_dependency_check

## reg_RAW_release

### raw_release

## handle_o3_fetch

### fetch_handling

## handle_merged_translation

### merged_translation_handling

## handle_merged_load

### merged_load_handling

## release_load_queue

### lq_release

## complete_instr_fetch

### instr_fetch_completion

## complete_data_fetch

### data_fetch_completion

## initialize_core

### core_init

## add_load_queue

### lq_add

## add_store_queue

### sq_add

## execute_store

### store_exec

## execute_load

### load_exec

## check_dependency

### dependency_check

## operate_cache

### cache_operation

## update_rob

### rob_update

## retire_rob

### rob_retirement

## progressive_memory_completion

### progressive_completion

## add_to_rob

### rob_insertion

## check_rob

### rob_lookup

## check_and_add_lsq

### lsq_check_add

## predict_branch

### branch_prediction_logic

## initialize_branch_predictor

### predictor_init

## last_branch_result

### branch_result_update
