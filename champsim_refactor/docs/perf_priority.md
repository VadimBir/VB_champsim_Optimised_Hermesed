# Performance Optimization Priority Book
Generated: 2026-03-24
Source: profiling data + constexpr_priority.md + ooo_cpu_cc.md + cache_cc.md

---

## Profiling Hotspot Map (input data)

| % CPU | Function | Notes |
|-------|----------|-------|
| 25.77 | O3_CPU::update_rob | Top hotspot. Drives complete_execution_scan |
| 21.04 | O3_CPU::complete_execution | Called from update_rob scan |
|  0.59 | O3_CPU::complete_data_fetch | Called from update_rob processed queues |
| 17.83 | O3_CPU::execute_memory_instruction | Delegates to operate_lsq + operate_cache |
| 13.28 | CACHE::operate | Called from operate_cache inside exe_mem_instr |
|  4.43 | CACHE::handle_read | Called from CACHE::operate |
|  1.43 | lpm_operate | Called from CACHE::operate |
|  1.12 | CACHE::handle_writeback | Called from CACHE::operate |
|  0.87 | CACHE::handle_fill | Called from CACHE::operate |
|  2.17 | CACHE::add_rq | Called from execute_load → L1D/L2C |
| 13.23 | O3_CPU::handle_branch | Mostly XZReader::fill_buffer (I/O bound) |
| 11.23 | XZReader::fill_buffer | lzma_code (decompression, external) |
| 10.73 | MEMORY_CONTROLLER::operate | DRAM scheduling |
| 10.65 | O3_CPU::schedule_instruction | ROB scan + bitset dispatch |
|  1.44 | O3_CPU::reg_dependency | Called from do_scheduling |
|  8.50 | O3_CPU::schedule_memory_instruction | ROB scan + bitset dispatch |
|  1.89 | O3_CPU::check_and_add_lsq | Called from do_memory_scheduling |
|  4.67 | CACHE::operate | (second callsite, same function) |
|  2.93 | O3_CPU::fetch_instruction | |
|  1.41 | O3_CPU::complete_execution | (second callsite) |
|  1.40 | O3_CPU::retire_rob | |
|  0.81 | O3_CPU::execute_instruction | |

Combined: update_rob + complete_execution = ~47% CPU. This is the primary target.

---

## Priority Ranking (optimization opportunity × hotspot weight)

### RANK 1 — update_rob::complete_execution_scan (25.77% → 21.04% child)
**Block:** `O3_CPU:update_rob:complete_execution_scan`
**Profile share:** ~47% combined
**What it does:** Scans ROB head→tail calling `complete_execution` per entry; breaks early when both `inflight_reg_executions` and `inflight_mem_executions` reach zero.
**Opportunity:**
- `complete_execution` has a 2-condition guard (is_mem && num_mem_ops!=0; executed!=INFLIGHT || event_cycle>current). In the common case MOST ROB entries are already COMPLETED — guard fires immediately.
- The early-break condition checks TWO counters. If inflight counters are zero, the scan still starts and iterates until it checks. The scan can be **hoisted**: check both counters BEFORE entering the loop entirely — skip scan when zero.
- `complete_execution` is NOT marked `inline`. At 21% of total runtime this is a concrete inline win.
**Task for worker:** Read `update_rob` source lines, read `complete_execution` source lines. Confirm: (a) inflight counter check is inside loop not before it; (b) `complete_execution` is not `inline`; (c) no side effects prevent hoisting the zero-counter guard before the scan.

---

### RANK 2 — schedule_instruction + schedule_memory_instruction (10.65% + 8.50%)
**Blocks:** `linear_scan_head_lt_limit`, `linear_scan_wrap`, `bitset_dispatch` (both methods share same pattern)
**Profile share:** ~19%
**What they do:** Tiled ROB scan building a `valid_bits` bitset, then `__builtin_ctzll` dispatch on set bits. Two separate methods doing nearly identical scan logic.
**Opportunity:**
- Both methods do a tiled scan with TILE_SIZE (already `constexpr`). The tile iteration itself is identical boilerplate. If the scan condition lambda/functor were passed in, one scan kernel handles both — eliminates one full ROB traversal.
- `do_scheduling` and `do_memory_scheduling` (the per-entry callbacks) are NOT marked `inline`. Both are called inside the `__builtin_ctzll` dispatch hot loop.
- `reg_dependency` (1.44%) is called from `do_scheduling` and is also not `inline`.
**Task for worker:** Read both methods. Confirm: (a) do_scheduling and do_memory_scheduling are not inline; (b) the scan logic (guard/tile/wrap/bitset build) is duplicated vs shared; (c) what exactly differs between the two scans (so a shared kernel is feasible without logic change).

---

### RANK 3 — execute_memory_instruction → operate_lsq → CACHE::operate chain (17.83% + 13.28%)
**Blocks:** `execute_memory_instruction:delegate`, `operate_lsq:rtl1_execute_load_loop`, `CACHE:operate`
**Profile share:** ~31% (includes CACHE::operate at two callsites)
**What it does:** `execute_memory_instruction` calls `operate_lsq()` then `operate_cache()`. `operate_lsq` drains RTL1 via `execute_load`, which calls `L1D.add_rq` or `L2C.add_rq`. `operate_cache` calls 6 `.operate()` on TLBs+caches.
**Opportunity:**
- `execute_memory_instruction` is a 2-line delegate — NOT marked `inline`. At 17.83% it should be.
- `operate_cache` (6 `.operate()` calls) is NOT marked `inline`. Eliminating call overhead for a 6-call delegate is trivial.
- `CACHE::operate` calls `handle_fill`, `handle_writeback`, `handle_read`, `handle_prefetch` in fixed order. These are NOT reorderable without logic change. But `handle_read` at 4.43% drills into `check_hit` then `add_mshr` or `add_rq`. The `get_set` address decode called at start of `check_hit` and `fill_cache` is pure arithmetic and NOT `constexpr inline` — adding that eliminates a call on every cache access.
**Task for worker:** Read `execute_memory_instruction` and `operate_cache` source. Confirm: (a) both are simple delegates not marked inline; (b) CACHE::get_set is not constexpr/inline in current source; (c) check whether handle_read's first call to check_hit/get_set is a hot inner path with no side effects preventing inline.

---

## NOT actionable (for completeness)

| Function | Reason |
|----------|--------|
| XZReader::fill_buffer (11.23%) | lzma decompression, external library. Cannot optimize. |
| MEMORY_CONTROLLER::operate (10.73%) | DRAM scheduling, inherently runtime state machine. |
| handle_branch (13.23%) | Dominated by XZReader I/O. Remaining 2% is field copy (instr_field_copy) — trivial. |
| lpm_operate (1.43%) | Statistics accounting, not on critical simulation path. |

---

## Worker Task (single worker, read-only, report only)

Worker reads the 3 ranked targets and confirms or refutes each opportunity. Reports back per block:

```
RANK|FUNCTION:BLOCK|CONFIRMED(Y/N)|REASON|EXACT_CHANGE_IF_YES
```

Worker reads:
1. `src/ooo_cpu.cc` lines around `update_rob` and `complete_execution`
2. `src/ooo_cpu.cc` lines around `schedule_instruction`, `schedule_memory_instruction`, `do_scheduling`, `do_memory_scheduling`
3. `src/ooo_cpu.cc` lines around `execute_memory_instruction`, `operate_cache`
4. `src/cache.cc` lines around `CACHE::get_set`, `CACHE::operate`, `CACHE::handle_read`

Worker does NOT edit anything. Reports findings for manager approval before any change.

After manager approves top 3 confirmed opportunities, a second pass implements ONLY those 3 — no logic changes.
