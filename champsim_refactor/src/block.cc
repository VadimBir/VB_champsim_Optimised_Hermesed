#include "block.h"

// ==== TRUE_SANITY_CHECK macros (block.cc local) ====
#ifdef TRUE_SANITY_CHECK
  #define SANITY_PQ_NOT_FULL_OR_EMPTY() \
      do { if (occupancy && (head == tail)) assert(0); } while(0)

  #define SANITY_PQ_FREE_SLOT_FOUND(checked) \
      do { if ((checked) >= SIZE) { \
          cerr << "[" << NAME << "] add_queue failed: no free slot despite occupancy=" \
               << occupancy << "/" << SIZE << endl; assert(0); } } while(0)
#else
  #define SANITY_PQ_NOT_FULL_OR_EMPTY()          ((void)0)
  #define SANITY_PQ_FREE_SLOT_FOUND(checked)     ((void)0)
#endif
// ==== END TRUE_SANITY_CHECK macros (block.cc local) ====

int PACKET_QUEUE::check_queue(PACKET *packet) const {
    if ((head == tail) && occupancy == 0)
        return -1;

    // Hoist string compare out of loop — NAME is invariant
    const bool use_full_addr = is_WQ;

    // Unified scan lambda — one branch on use_full_addr per call, not per iteration
    // Branchless scan: arithmetic mask select + arithmetic early exit
    auto scan = [&](uint16_t start, uint16_t end) -> int {
        int32_t found = -1;
        if (use_full_addr) {
            const uint64_t target = packet->full_addr;
            for (uint16_t i = start; i < end; i++) {
                __builtin_prefetch(&entry[i + 8], 0, 3);
#ifdef BYPASS_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS
                int32_t mask = -((entry[i].full_addr == target) & (entry[i].l1_bypassed == packet->l1_bypassed) & (entry[i].l2_bypassed == packet->l2_bypassed) & (entry[i].llc_bypassed == packet->llc_bypassed));
#else
                int32_t mask = -(entry[i].full_addr == target);
#endif
                found = ((int32_t)i & mask) | (found & ~mask);
                i += ((end - i - 1) & mask);
            }
        } else {
            const uint64_t target = packet->address;
            for (uint16_t i = start; i < end; i++) {
                __builtin_prefetch(&entry[i + 8], 0, 3);
#ifdef BYPASS_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS
                int32_t mask = -((entry[i].address == target) & (entry[i].l1_bypassed == packet->l1_bypassed) & (entry[i].l2_bypassed == packet->l2_bypassed) & (entry[i].llc_bypassed == packet->llc_bypassed));
#else
                int32_t mask = -(entry[i].address == target);
#endif
                found = ((int32_t)i & mask) | (found & ~mask);
                i += ((end - i - 1) & mask);
            }
        }
        return found;
    };

    if (head < tail) {
        return scan(head, tail);
    } else {
        int r = scan(head, SIZE);
        return (r >= 0) ? r : scan(0, tail);
    }
}

void PACKET_QUEUE::add_queue(PACKET *packet) {
    SANITY_PQ_NOT_FULL_OR_EMPTY();
    // Find next free slot starting from tail
    // Prevents overwriting occupied entries in sparse arrays
    uint16_t add_index = tail;
    uint16_t checked = 0;

    while (entry[add_index].address != 0 && checked < SIZE) {
        add_index++;
        if (add_index >= SIZE)
            add_index = 0;
        checked++;
    }
    
    SANITY_PQ_FREE_SLOT_FOUND(checked);

    entry[add_index].fast_copy_packet(entry[add_index], *packet);

    occupancy++;
    tail = add_index + 1;
    if (tail >= SIZE)
        tail = 0;
}
void PACKET_QUEUE::remove_queue(PACKET *packet) {

#ifdef SANITY_CHECK
    if ((occupancy == 0) && (head == tail))
        assert(0);
#endif
    packet->quickReset();
    occupancy--;
    // Only increment head if removing the head entry
    if (packet == &entry[head]) {
        head++;
        if (head >= SIZE)
            head = 0;
        // if (head == tail)
        //     assert(0&&"SANITY FAIL: PACKET_QUEUE::remove_queue => QUEUE HEAD TAIL EQUAL ");
    }
    // For arbitrary removal, just leave hole - scheduling logic handles it
}