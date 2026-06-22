# champsim_refactor/src/block.cc — function bounds

## PACKET_QUEUE::check_queue  L18-63
  block_01  L19-20  `if ((head == tail) && occupancy == 0)`
  block_02  L23-23  `const bool use_full_addr = is_WQ;`
  block_03  L27-55  `auto scan = [&](uint16_t start, uint16_t end) -> int {`
  block_04  L57-62  `if (head < tail) {`

## PACKET_QUEUE::add_queue  L65-87
  block_01  L66-66  `SANITY_PQ_NOT_FULL_OR_EMPTY();`
  block_02  L69-70  `uint16_t add_index = tail;`
  block_03  L72-77  `while (entry[add_index].address != 0 && checked < SIZE) {`
  block_04  L79-79  `SANITY_PQ_FREE_SLOT_FOUND(checked);`
  block_05  L81-86  `entry[add_index].fast_copy_packet(entry[add_index], *packet);`

## PACKET_QUEUE::remove_queue  L88-105
  block_01  L90-93  `#ifdef SANITY_CHECK`
  block_02  L94-95  `packet->quickReset();`
  block_03  L97-103  `if (packet == &entry[head]) {`
