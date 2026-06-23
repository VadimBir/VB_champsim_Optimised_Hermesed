# CONTENT PAGE

- MEMORY_CONTROLLER [class]
- MEMORY_CONTROLLER::MEMORY_CONTROLLER [method]
- MEMORY_CONTROLLER::quickReset [method]
- MEMORY_CONTROLLER::~MEMORY_CONTROLLER [method]
- MEMORY_CONTROLLER::add_rq [method]
- MEMORY_CONTROLLER::add_wq [method]
- MEMORY_CONTROLLER::add_pq [method]
- MEMORY_CONTROLLER::return_data [method]
- MEMORY_CONTROLLER::operate [method]
- MEMORY_CONTROLLER::increment_WQ_FULL [method]
- MEMORY_CONTROLLER::get_occupancy [method]
- MEMORY_CONTROLLER::get_size [method]
- MEMORY_CONTROLLER::schedule [method]
- MEMORY_CONTROLLER::process [method]
- MEMORY_CONTROLLER::update_schedule_cycle [method]
- MEMORY_CONTROLLER::update_process_cycle [method]
- MEMORY_CONTROLLER::reset_remain_requests [method]
- MEMORY_CONTROLLER::dram_get_channel [method]
- MEMORY_CONTROLLER::dram_get_rank [method]
- MEMORY_CONTROLLER::dram_get_bank [method]
- MEMORY_CONTROLLER::dram_get_row [method]
- MEMORY_CONTROLLER::dram_get_column [method]
- MEMORY_CONTROLLER::drc_check_hit [method]
- MEMORY_CONTROLLER::get_bank_earliest_cycle [method]
- MEMORY_CONTROLLER::check_dram_queue [method]

---

# MEMORY_CONTROLLER

## MEMORY_CONTROLLER

### dbus_init

#### dbus_init

### bank_cycle_init

#### bank_cycle_init

### write_mode_init

#### write_mode_init

### queue_init

#### queue_init

## quickReset

### dbus_reset

#### dbus_reset

### bank_cycle_reset

#### bank_cycle_reset

### write_mode_reset

#### write_mode_reset

### queue_reset

#### queue_reset

## ~MEMORY_CONTROLLER

### destructor_body

#### destructor_body

## add_rq

### add_read_queue

#### add_read_queue

## add_wq

### add_write_queue

#### add_write_queue

## add_pq

### add_prefetch_queue

#### add_prefetch_queue

## return_data

### return_to_upper_level

#### return_to_upper_level

## operate

### operation_cycle

#### operation_cycle

## increment_WQ_FULL

### increment_counter

#### increment_counter

## get_occupancy

### get_queue_occupancy

#### get_queue_occupancy

## get_size

### get_queue_size

#### get_queue_size

## schedule

### schedule_queue

#### schedule_queue

## process

### process_queue

#### process_queue

## update_schedule_cycle

### update_schedule

#### update_schedule

## update_process_cycle

### update_process

#### update_process

## reset_remain_requests

### reset_requests

#### reset_requests

## dram_get_channel

### extract_channel

#### extract_channel

## dram_get_rank

### extract_rank

#### extract_rank

## dram_get_bank

### extract_bank

#### extract_bank

## dram_get_row

### extract_row

#### extract_row

## dram_get_column

### extract_column

#### extract_column

## drc_check_hit

### check_hit_logic

#### check_hit_logic

## get_bank_earliest_cycle

### find_bank_cycle

#### find_bank_cycle

## check_dram_queue

### queue_check

#### queue_check
