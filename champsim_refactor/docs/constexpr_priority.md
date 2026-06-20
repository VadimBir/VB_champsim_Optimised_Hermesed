# constexpr / inline Opportunity Report
Generated: 2026-03-24
Sources: cache.cc, dram_controller.cc, ooo_cpu.cc, ooo_cpu.h

Format: PRIORITY | FILE | FUNCTION | TASK | CHANGE_NEEDED

---

## H — High Priority (clear wins, call-frequency impact)

H | cache.cc          | CACHE::get_set                        | CONSTEXPR+INLINE | Pure address→set-index: `(addr >> LOG2_BLOCK_SIZE) & (NUM_SET-1)`. No side effects. Mark `constexpr inline`.
H | cache.cc          | CACHE::get_size                       | INLINE+CONSTEXPR-check | Returns SIZE fields. If SIZE is a compile-time constant member (not runtime-assigned), mark `constexpr inline`. Mark `inline` regardless.
H | cache.cc          | CACHE::get_occupancy                  | INLINE | Switch on queue_type returning runtime occupancy. Mark `inline`. NOTE: `address` param is unused — cleanup opportunity.
H | cache.cc          | CACHE::probe_mshr                     | INLINE | 3-line MSHR index scan. Mark `inline`. Not constexpr (reads runtime MSHR array).
H | dram_controller.cc| MEMORY_CONTROLLER::dram_get_channel   | CONSTEXPR+INLINE | Pure bit-shift/mask using compile-time constants LOG2_DRAM_CHANNELS/DRAM_CHANNELS. Mark `constexpr inline` after confirming those are `constexpr` (not extern runtime vars).
H | dram_controller.cc| MEMORY_CONTROLLER::dram_get_bank      | CONSTEXPR+INLINE | Same pattern as dram_get_channel with LOG2_DRAM_BANKS/DRAM_BANKS.
H | dram_controller.cc| MEMORY_CONTROLLER::dram_get_rank      | CONSTEXPR+INLINE | Same pattern.
H | dram_controller.cc| MEMORY_CONTROLLER::dram_get_row       | CONSTEXPR+INLINE | Same pattern.
H | dram_controller.cc| MEMORY_CONTROLLER::dram_get_column    | CONSTEXPR+INLINE | Same pattern.
H | ooo_cpu.cc        | SPLIT: build_pa (free function)       | SPLIT+CONSTEXPR | Extract `constexpr uint64_t build_pa(uint64_t data_pa, uint64_t va, uint32_t log2_page_size)` from 4+ identical expressions: `(data_pa << LOG2_PAGE_SIZE) | (va & PAGE_OFFSET_MASK)` in complete_data_fetch (lines ~1694,1706) and handle_merged_translation (lines ~1827,1840).
H | ooo_cpu.cc        | SPLIT: blend_pa (free function)       | SPLIT+CONSTEXPR | Extract `constexpr uint64_t blend_pa(uint64_t phys_base, uint64_t va, uint32_t log2_block_size)` from `(sq_phys_base | (lq_va & offset_mask))` pattern in add_load_queue (~line 1096) and execute_store (~line 1376).

---

## M — Medium Priority (inline markings on hot-path helpers)

M | ooo_cpu.cc | O3_CPU::complete_execution  | INLINE | Called in tight ROB scan loop inside update_rob. Currently not marked inline. Mark `inline`.
M | ooo_cpu.cc | O3_CPU::release_load_queue  | INLINE | 3-line body. Called from 4 hot completion paths (add_load_queue, execute_store, complete_data_fetch, handle_merged_load). Mark `inline`.
M | ooo_cpu.cc | O3_CPU::reg_dependency      | INLINE | ~8 lines. Called per instruction from do_scheduling (hot path). Not currently marked inline. Mark `inline`.
M | ooo_cpu.cc | O3_CPU::check_rob           | INLINE | 3 lines after guard. Short hashmap lookup. Mark `inline`.
M | ooo_cpu.cc | O3_CPU::operate_cache       | INLINE | 6 .operate() calls, very short delegate. Mark `inline`.
M | cache.cc   | SPLIT: same_page (free fn)  | SPLIT+CONSTEXPR | Extract `constexpr bool same_page(uint64_t a, uint64_t b)` → `(a>>LOG2_PAGE_SIZE)==(b>>LOG2_PAGE_SIZE)`. Appears in prefetch_line guard. Low value but clean.
M | dram_controller.cc | MEMORY_CONTROLLER::get_occupancy | INLINE | Calls dram_get_channel then returns RQ/WQ occupancy. Short. Mark `inline`.
M | dram_controller.cc | MEMORY_CONTROLLER::get_size      | INLINE | Same pattern. Mark `inline`.

---

## L — Low Priority / Already Correct

L | cache.cc   | CACHE::set_force_all_hits    | INLINE | 1-line body, sets global. Mark `inline`. Not constexpr.
L | cache.cc   | CACHE::increment_WQ_FULL     | INLINE | 1-line body. Mark `inline`.
L | cache.cc   | CACHE::prefetcher_feedback   | INLINE | 4 assignments from member fields. Mark `inline`.
L | cache.cc   | CACHE::merge_with_prefetch   | NONE   | Already `inline`. Correct as-is.
L | cache.cc   | CACHE::add_mshr              | NONE   | Already `inline`. Correct as-is.
L | ooo_cpu.cc | O3_CPU::do_scheduling        | INLINE | 8 lines. Mark `inline` for documentation; compiler likely already inlines.
L | ooo_cpu.cc | O3_CPU::reg_RAW_dependency   | INLINE | 6 lines. Mark `inline`.
L | ooo_cpu.cc | O3_CPU::do_execution         | INLINE | 5 lines. Mark `inline`.
L | ooo_cpu.cc | O3_CPU::execute_memory_instruction | INLINE | 2-line delegate. Mark `inline`.
L | dram_controller.cc | MEMORY_CONTROLLER::increment_WQ_FULL | INLINE | 2-line body. Mark `inline`.

---

## NO ACTION — Inherently Runtime

cache.cc:    handle_fill, handle_writeback, handle_read, handle_prefetch, operate, fill_cache,
             check_hit, invalidate_entry, add_rq, add_wq, add_pq, prefetch_line, return_data,
             update_fill_cycle, check_mshr

ooo_cpu.cc:  initialize_core, handle_branch, add_to_rob, fetch_instruction, schedule_instruction,
             execute_instruction, do_memory_scheduling (already inline), check_and_add_lsq,
             add_store_queue, operate_lsq, execute_load, reg_RAW_release (already inline),
             update_rob, complete_instr_fetch, complete_data_fetch (body), handle_merged_load,
             retire_rob, execute_store (body), add_load_queue (body)

ooo_cpu.h:   XZReader::eof (already inline), XZReader::reopen (already inline),
             O3_CPU::verify_xz_trace, O3_CPU::O3_CPU(),
             AddrDependencyTracker::get_latest_producer, MemDependencyTracker::get_latest_producer

---

## Prerequisites / Verification Steps (before any code change)

1. Confirm `LOG2_DRAM_CHANNELS`, `DRAM_CHANNELS`, `LOG2_DRAM_BANKS`, etc. are declared `constexpr` (not extern runtime vars) — required for dram_get_* to be constexpr.
2. Confirm CACHE queue SIZE fields are compile-time constants — required for get_size constexpr.
3. For build_pa / blend_pa: confirm LOG2_PAGE_SIZE and BLOCK_MASK are constexpr — required for free function to be constexpr.
4. For get_set: confirm NUM_SET is constexpr — likely yes (fixed cache geometry).
