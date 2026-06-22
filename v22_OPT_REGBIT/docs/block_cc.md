# CONTENT PAGE
**FORMAT: <CLASS>:<METHOD>:<BLOCK>**
- PACKET_QUEUE:check_queue [method]
- PACKET_QUEUE:check_queue:empty_queue_check [block]
- PACKET_QUEUE:check_queue:head_lt_tail_scan [block]
- PACKET_QUEUE:check_queue:head_lt_tail_scan:l1d_wq_full_addr_match [block]
- PACKET_QUEUE:check_queue:head_lt_tail_scan:other_queue_address_match [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_upper [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_upper:l1d_wq_full_addr_match [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_upper:other_queue_address_match [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_lower [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_lower:l1d_wq_full_addr_match [block]
- PACKET_QUEUE:check_queue:head_gte_tail_scan_lower:other_queue_address_match [block]
- PACKET_QUEUE:check_queue:return_not_found [block]
- PACKET_QUEUE:add_queue [method]
- PACKET_QUEUE:add_queue:free_slot_search [block]
- PACKET_QUEUE:add_queue:fast_copy_insert [block]
- PACKET_QUEUE:add_queue:occupancy_tail_update [block]
- PACKET_QUEUE:remove_queue [method]
- PACKET_QUEUE:remove_queue:packet_reset [block]
- PACKET_QUEUE:remove_queue:occupancy_decrement [block]
- PACKET_QUEUE:remove_queue:head_advance_if_head_entry [block]

---

# PACKET_QUEUE

## check_queue

### empty_queue_check

### head_lt_tail_scan

#### l1d_wq_full_addr_match

#### other_queue_address_match

### head_gte_tail_scan_upper

#### l1d_wq_full_addr_match

#### other_queue_address_match

### head_gte_tail_scan_lower

#### l1d_wq_full_addr_match

#### other_queue_address_match

### return_not_found

---

## add_queue

### free_slot_search

### fast_copy_insert

### occupancy_tail_update

---

## remove_queue

### packet_reset

### occupancy_decrement

### head_advance_if_head_entry
