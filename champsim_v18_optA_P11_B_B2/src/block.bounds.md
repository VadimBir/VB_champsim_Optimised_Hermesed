# block.cc — function bounds

## check_queue  L2-47
  block_00  L3-4  `if ((head == tail) && occupancy == 0)`
  block_01  L6-6  `// Hoist string compare out of loop — NAME is invariant`
  block_02  L7-7  `const bool use_full_addr = is_WQ;`
  block_03  L9-9  `// Unified scan lambda — one branch on use_full_addr per cal`
  block_04  L10-10  `// Branchless scan: arithmetic mask select + arithmetic earl`
  block_05  L11-39  `auto scan = [&](uint16_t start, uint16_t end) -> int {`
  block_06  L41-46  `if (head < tail) {`

## add_queue  L49-80
  block_00  L50-53  `#ifdef TRUE_SANITY_CHECK`
  block_01  L54-54  `// Find next free slot starting from tail`
  block_02  L55-55  `// Prevents overwriting occupied entries in sparse arrays`
  block_03  L56-56  `uint16_t add_index = tail;`
  block_04  L57-57  `uint16_t checked = 0;`
  block_05  L59-64  `while (entry[add_index].address != 0 && checked < SIZE) {`
  block_06  L66-72  `#ifdef TRUE_SANITY_CHECK`
  block_07  L74-74  `entry[add_index].fast_copy_packet(entry[add_index], *packet)`
  block_08  L76-76  `occupancy++;`
  block_09  L77-77  `tail = add_index + 1;`
  block_10  L78-79  `if (tail >= SIZE)`

## remove_queue  L81-98
  block_00  L83-86  `#ifdef SANITY_CHECK`
  block_01  L87-87  `packet->quickReset();`
  block_02  L88-88  `occupancy--;`
  block_03  L89-89  `// Only increment head if removing the head entry`
  block_04  L90-96  `if (packet == &entry[head]) {`
  block_05  L97-97  `// For arbitrary removal, just leave hole - scheduling logic`
