# ChampSim TU Split Design — behavior-preserving (PURE code motion)

Scope: split the 4 oversized translation units (`cache.cc`, `dram_controller.cc`,
`ooo_cpu.cc`, `main.cc`) into cohesive, smaller files.

Hard invariant: a clean rebuild must produce **bit-identical `execution_checksum`
and IPC**. Every move below is **pure code motion** — the exact same source text,
compiled with the exact same flags (`g++ -O3 -std=c++17 -mavx2 -mbmi2 ...`),
with no inlining/devirtualization/ODR/template-instantiation change.

---

## 0. Ground rules that make these splits safe

### 0.1 The build auto-discovers `src/*.cc`
`Makefile.win` (and `Makefile`) build the object list with
`SRCS := $(wildcard src/*.cc) ...`. Therefore **every new `.cc` placed in
`src/` is automatically compiled and linked** — no Makefile edit is needed for a
new TU, and (critically) we must **not** create a new `.cc` that is *only* meant
to be `#include`d, unless it is safe to also be compiled standalone (see 0.3).

### 0.2 Definitions of namespace-scope objects must stay single (ODR)
`cache.cc`, `ooo_cpu.cc`, `main.cc`, `dram_controller.cc` each **define**
(not just declare) file-scope globals that the rest of the program links against:

- `ooo_cpu.cc`: `execution_checksum[]`, `ooo_cpu[]`, `current_core_cycle[]`,
  `stall_cycle[]`, `rob_memory_count[]`, `next_mem_sched_start[]`,
  `SCHEDULING_LATENCY`, `EXEC_LATENCY`, `INSTR_TEMPLATE`, `instr_size`.
- `cache.cc`: `l2pf_access`, `FORCE_ALL_HITS`, `lpm[][]`, `g_l1_byplat[]`,
  `g_l2_byplat[]`, `g_llc_byplat[]`, `g_crossmlp_*` (guarded), `pending_requests`,
  `ALL_CACHES` (guarded by `BYPASS_DEBUG`).
- `main.cc`: `TOTAL_SUM_FINAL_SIM_IPC`, `warmup_complete[]`,
  `simulation_complete[]`, `all_warmup_complete`, `all_simulation_complete`,
  `MAX_INSTR_DESTINATIONS`, knob flags, `warmup_instructions`,
  `simulation_instructions`, `champsim_seed`, `start_time`, `last_print_time[]`,
  page-table globals (`PAGE_TABLE_LATENCY`, `SWAP_LATENCY`, `page_queue`,
  `page_table`, `inverse_table`, `recent_page`, `unique_cl[]`, `previous_ppage`,
  `num_adjacent_page`, `num_cl[]`, `allocated_pages`, `num_page[]`,
  `minor_fault[]`, `major_fault[]`), `statsCollector`, `champsim_rand`,
  `rob_events`, `mem_index_ring`.
- `dram_controller.cc`: `DRAM_MTPS`, `DRAM_DBUS_RETURN_TIME`, `tRP`, `tRCD`, `tCAS`.

**Rule:** each such *definition* stays in exactly **one** of the resulting TUs.
The sibling TU(s) that reference it get an `extern` declaration (in a new shared
internal header — see per-file plans). No definition is ever duplicated.

### 0.3 File-local macros must travel with (or be shared by) the code that uses them
Three of the four files open with a large block of **file-local `#define`s** that
expand inside the functions:

- `cache.cc`: `SANITY_*` block, `CACHE_LVL_BASE`/`get_cache_lvl_bit`/
  `set_this_lvl_existance`/`check_upper_has_entry`, and the `_DRMIN_*` dump macros.
- `dram_controller.cc`: `SANITY_DRAM_REQUEST_IDX_BOUND`, `DRAM_FULL_DP`,
  `DRAM_CH_DP`.
- `ooo_cpu.cc`: the `SANITY_*` block.

These are **not** preprocessor-trivial: e.g. `DRAM_FULL_DP` references
`bank_hot`, `bank_request`, `RQ`, `WQ`, `write_mode`, etc. by name. If a function
that uses such a macro moves to a new TU, the macro definition **must be visible
in that new TU**. Solution: extract each block into a private header
(`*_internal.h`) `#include`d by **all** resulting fragments of that original file.
Because the macros are textually identical everywhere they expand, codegen is
unchanged.

### 0.4 The bypass-model `.cc` files are already `#include`-safe AND standalone-safe
`ooo_l1_byp_model.cc` / `ooo_l2_byp_model.cc` / `ooo_llc_byp_model.cc` define all
their symbols as **`inline` functions and `inline` variables (C++17)**. They are
`#include`d into `cache.cc` and `main.cc`, *and* compiled standalone by the
wildcard (yielding effectively empty objects). Any new fragment TU that needs
these symbols simply repeats the same `#include "ooo_l1_byp_model.cc"` block —
`inline` guarantees no ODR violation. **Do not** convert them.

### 0.5 Conditionally-compiled twin definitions must not be separated
`O3_CPU::handle_branch()` exists as **two mutually-exclusive bodies** guarded by
`#ifdef USE_TRACE_HELPER … #else … #endif` (ooo_cpu.cc L455–650). Both bodies, the
`#include "trace_helper.h"` inside the `#ifdef`, and the guard must move **as one
unit** to the same destination TU. Splitting them apart would risk emitting zero
or two definitions.

### 0.6 What "pure code motion" is verified against
After each split: `make -f Makefile.win clean && make -f Makefile.win`, then run
the fixed trace and diff `execution_checksum` and IPC against the pre-split
baseline. Identity is the acceptance gate; any diff means a move was not pure.

---

## 1. `main.cc` (1867 lines, 22 fns) — **SAFEST, do first**

The print/stats block is the textbook first cut: the stats/print functions are
leaf reporters that touch only globals + `CACHE*`/`O3_CPU*` arguments and the
`statsCollector` object — none of them is on the simulation hot path that mutates
`execution_checksum`, so they cannot perturb the checksum even if codegen shifted
(it won't).

### New files

| New file | Responsibility | Functions moved |
|---|---|---|
| `src/main_stats.cc` | All ROI / sim / dram / branch stat collection + printing | `record_roi_stats`, `print_pf_hitRatio`, `print_L2_hitRatio`, `print_L2_usefulRatio`, `print_roi_stats`, `print_sim_stats`, `print_branch_stats`, `print_dram_stats`, `reset_cache_stats`, `print_knobs`, and **all `FinalStatsCollector` member functions** (`collectROIStats`, `collectSimStats`, `collectDRAMStats`, `collectBranchStats`, `collectCoreROIStats`, `collectPageFaultStats`, `dumpAllAsString`) |
| `src/main_paging.cc` | Virtual→physical translation + page table | `va_to_pa` |
| `src/main.cc` (shrunk) | Program entry, arg parse, warmup, deadlock detect, signal | `main`, `finish_warmup`, `print_deadlock`, `signal_handler` |
| `src/main_internal.h` (new header, NOT a `.cc`) | Shared decls/macros for the three fragments | — |

### Shared declarations to hoist into `src/main_internal.h`
- The macro block: `FIXED_FLOAT*`, `STR_*`, `ITLB_type…DRAM_type`, and the
  `HERMES_LABEL` selection `#ifdef` (used by both `print_*` and `main`).
- `class FinalStatsCollector { … };` (the **class definition**; used by
  `print_*` and `main`).
- `extern` declarations for the shared globals consumed across the fragments:
  `statsCollector`, `champsim_rand`, `rob_events`, `mem_index_ring`,
  `warmup_complete[]`, `simulation_complete[]`, the page-table globals,
  `start_time`, `last_print_time[]`, `TOTAL_SUM_FINAL_SIM_IPC`, knob flags,
  `warmup_instructions`, `simulation_instructions`, `champsim_seed`,
  `PAGE_TABLE_LATENCY`, `SWAP_LATENCY`.
- Prototypes for the moved free functions (`print_roi_stats`, `print_sim_stats`,
  `print_dram_stats`, `print_branch_stats`, `reset_cache_stats`, `print_knobs`,
  `record_roi_stats`, `print_*Ratio`, `va_to_pa`) so `main.cc` can still call
  them across TUs. (Several already have prototypes in `inc/*.h`; reuse those
  where they exist rather than re-declaring.)

### Definition ownership after the split (each defined exactly once)
- `statsCollector`, `champsim_rand`, `rob_events`, `mem_index_ring`, all the
  page-table globals, warmup/sim/knob globals, `start_time`, `last_print_time[]`,
  `TOTAL_SUM_FINAL_SIM_IPC`: **keep their definitions in the shrunk `main.cc`**
  (lowest churn — `main` is the heaviest user). `main_stats.cc` and
  `main_paging.cc` see them via `extern` from `main_internal.h`.
  - Exception worth noting: `champsim_rand` is *defined* immediately above
    `va_to_pa`. To keep `va_to_pa` self-contained in `main_paging.cc` we may move
    its **definition** there and `extern` it into `main.cc`. Either placement is
    pure code motion; pick one, define once.
- Each fragment that uses bypass-model symbols repeats the
  `#include "lpm_tracker.h"` + `#include "ooo_l1_byp_model.cc"` (+ L2/LLC guarded)
  prologue, per rule 0.4.

### Cannot move cleanly
- `main` itself: it is the definition owner of most globals and orchestrates
  everything; it stays in `main.cc`.
- `print_deadlock` and `finish_warmup` read/write `rob_events`, `mem_index_ring`,
  `page_*` and the per-core arrays heavily and are entangled with `main`'s loop
  state conventions — keep them with `main.cc` (moving them buys little and raises
  the surface for an accidental decl mismatch). They are **not** part of the
  high-value first cut.

### Risk: **LOW** (stats/print are leaf reporters). Highest value/lowest risk → ship first.
### Confirmation: pure code motion — function bodies copied verbatim; only `extern`/prototype/class-decl plumbing added in a header.

---

## 2. `dram_controller.cc` (1449 lines, 20 fns) — do second

All functions are `MEMORY_CONTROLLER::` members or the two free helpers
(`print_dram_config`, and globals). The big-DP macros (`DRAM_FULL_DP`,
`DRAM_CH_DP`) and the sanity macro are file-local and referenced by the datapath
functions.

### New files

| New file | Responsibility | Functions moved |
|---|---|---|
| `src/dram_controller.cc` (shrunk) | Top-level tick + request admission datapath | `operate`, `process`, `schedule`, `reset_remain_requests`, `add_rq`, `add_wq`, `add_pq`, `return_data` |
| `src/dram_queue.cc` | Queue-cycle bookkeeping + occupancy/size + WQ-full | `update_schedule_cycle`, `update_process_cycle`, `check_dram_queue`, `get_occupancy`, `get_size`, `increment_WQ_FULL` |
| `src/dram_addr.cc` | Address decode (channel/bank/column/rank/row) + config print | `dram_get_channel`, `dram_get_bank`, `dram_get_column`, `dram_get_rank`, `dram_get_row`, `print_dram_config` |
| `src/dram_controller_internal.h` (new header) | Shared macros + extern decls | — |

### Shared declarations to hoist into `src/dram_controller_internal.h`
- `SANITY_DRAM_REQUEST_IDX_BOUND`, `DRAM_FULL_DP`, `DRAM_CH_DP`, and the
  `DRAM_SANITY_CHECK`/`DEBUG_PRINT` `#ifdef` scaffolding (rule 0.3). These expand
  against `MEMORY_CONTROLLER` members (`RQ`, `WQ`, `bank_*`, `write_mode`,
  `scheduled_*`, `dbus_cycle_available`) so they must be visible wherever those
  functions land.
- The `#ifdef USE_HERMES namespace knob { extern … }` block (needed by datapath).
- `#ifdef BYP_DERFILL_ACTIVE #include "cache.h"` (needed only by the datapath
  fragment — keep it in the fragment that contains the derivative-fill code,
  i.e. the shrunk `dram_controller.cc`).

### Definition ownership
- `DRAM_MTPS`, `DRAM_DBUS_RETURN_TIME`, `tRP`, `tRCD`, `tCAS` are **defined** at
  the top of the original file (initialized in main.cc). Keep the **definitions**
  in the shrunk `dram_controller.cc`; the `dram_addr.cc`/`dram_queue.cc` fragments
  reference them through `extern` decls (these are simple scalars — put the
  `extern`s in `dram_controller_internal.h`). `print_dram_config` reads
  `DRAM_MTPS`, `DRAM_DBUS_RETURN_TIME` → it gets them via `extern`.

### Cannot move cleanly
- None are hard-blocked. `add_rq`/`process`/`schedule`/`operate` are mutually
  call-coupled but that is fine across TUs (they're already non-`inline`
  out-of-line member definitions — calls go through normal symbols, no codegen
  change). Keeping them together in the shrunk file is the cohesive choice, not a
  hard requirement.

### Risk: **LOW–MEDIUM.** The macros are the only subtlety; once they live in the
shared header, codegen is identical. The graph drifted here, so confirm the macro
set is complete before moving any datapath fn.
### Confirmation: pure code motion — bodies verbatim, macros relocated to a shared header that every fragment includes.

---

## 3. `cache.cc` (2122 lines, 41 fns) — do third (largest, most macros)

Heaviest macro surface (`SANITY_*`, `CACHE_LVL_*`, `_DRMIN_*`) plus file-local
globals and the `pending_requests` vector defined mid-file (L307).

### New files

| New file | Responsibility | Functions moved |
|---|---|---|
| `src/cache.cc` (shrunk) | Core operate/fill/MSHR datapath | `operate`, `handle_fill`, `handle_read`, `handle_writeback`, `handle_read_miss_bypass`, `merge_with_prefetch`, `handle_prefetch`, `fill_cache`, `update_fill_cycle`, `check_hit`, `get_way`, `invalidate_entry`, `return_data`, `probe_mshr`, `check_mshr`, `check_mshr_hashmap`, `add_mshr`, `set_force_all_hits` |
| `src/cache_queues.cc` | Request/write/prefetch queue admission + occupancy/size | `add_rq`, `add_wq`, `add_pq`, `prefetch_line`, `get_size`, `get_occupancy`, `increment_WQ_FULL`, `prefetcher_feedback` |
| `src/cache_debug.cc` | All diagnostic dumps + config print | `print_cache_config`, `dump_req(PACKET&)`, `dump_req(PACKET*)`, and the 12 `dump_req_min_*` one-liners |
| `src/cache_internal.h` (new header) | Shared macros + extern decls | — |

### Shared declarations to hoist into `src/cache_internal.h`
- The **entire `SANITY_*` macro block** + `TRUE_SANITY_CHECK` scaffolding
  (used by datapath and queue functions).
- `CACHE_LVL_BASE`, `get_cache_lvl_bit`, `set_this_lvl_existance`,
  `check_upper_has_entry` (used in fill/prefetch path).
- The `_DRMIN_*` dump macros (`_DRMIN_SET_ROB/LQ/SQ`, `_DRMIN_CORE`,
  `_DRMIN_TAIL`) — **needed by `cache_debug.cc`** since all `dump_req_min_*`
  expand them. Keep their definition adjacent to the dump functions OR in the
  shared header; either is pure motion.
- `#ifdef USE_HERMES namespace knob { extern bool offchip_pred_mark_merged_load; }`
  + `#include "ooo_cpu.h"` (datapath fragment needs it).
- `extern` decls for the file-local globals that any fragment other than their
  definition-owner references.

### Definition ownership (define once)
- `l2pf_access`, `FORCE_ALL_HITS`, `lpm[][]`, `g_l1_byplat[]`, `g_l2_byplat[]`,
  `g_llc_byplat[]`, `g_crossmlp_*` (guarded), `LPM_Tracker` instance: **keep
  definitions in the shrunk `cache.cc`** (the datapath is their primary user;
  `set_force_all_hits` writes `FORCE_ALL_HITS`, so keep that setter with the
  definition or `extern` it — it currently lives at L103, so it co-locates
  naturally with the datapath fragment).
- `pending_requests` (defined at L307, just above `handle_fill`): keep with the
  shrunk `cache.cc` since `handle_fill` stays there.
- `ALL_CACHES` (`#ifdef BYPASS_DEBUG`, defined L237): keep in `cache_debug.cc`
  (it's a debug symbol) and `extern` it where referenced, OR keep in `cache.cc`
  and `extern` into debug — pick one; `cache.cc` (datapath) is the lower-risk
  owner because main.cc already `extern`s it from there today.
- The three `#include "ooo_l1/l2/llc_byp_model.cc"` lines: each fragment that
  references bypass symbols repeats them (rule 0.4 — `inline`, ODR-safe).

### Cannot move cleanly
- `set_force_all_hits` is trivial but writes the file-local `FORCE_ALL_HITS`;
  it must stay in the same TU as the `FORCE_ALL_HITS` **definition** (or that
  definition must be `extern`'d to it). Treat as bound to the datapath fragment.
- Functions in the datapath set (`operate` ↔ `handle_fill` ↔ `check_mshr` ↔
  `return_data` …) are heavily mutually recursive; not hard-blocked from
  splitting, but keeping them in one fragment is the cohesive low-risk choice.

### Risk: **MEDIUM.** Most macros + most file-local globals of any file, and this
file drifted most from the graph. Verify the full macro list compiles in each
fragment before trusting the line tables.
### Confirmation: pure code motion — every body verbatim; only macro relocation + `extern`/prototype plumbing.

---

## 4. `ooo_cpu.cc` (2247 lines, 31 fns) — do last (most cross-coupled)

Defines the program's `ooo_cpu[]`, `current_core_cycle[]`, and
**`execution_checksum[]` itself** (mutated by `retire_rob`). Pipeline stages are
tightly chained. Lowest priority because value/risk is worst.

### New files

| New file | Responsibility | Functions moved |
|---|---|---|
| `src/ooo_cpu.cc` (shrunk) | Front-end + ROB lifecycle + config/init | `print_core_config`, `initialize_core`, `fetch_instruction`, `complete_instr_fetch`, `complete_data_fetch`, `add_to_rob`, `check_rob`, `update_rob`, `retire_rob`, `handle_branch` (**both `#ifdef USE_TRACE_HELPER` twins together**) |
| `src/ooo_cpu_schedule.cc` | Scheduling + register dependency + execution issue | `schedule_instruction`, `do_scheduling`, `reg_dependency`, `reg_RAW_dependency`, `reg_RAW_release`, `schedule_memory_instruction`, `do_memory_scheduling`, `execute_instruction`, `do_execution`, `complete_execution`, `operate_cache` |
| `src/ooo_cpu_lsq.cc` | Load/store queue + memory execution | `operate_lsq`, `check_and_add_lsq`, `add_load_queue`, `add_store_queue`, `execute_load`, `execute_store`, `handle_merged_load`, `handle_merged_translation`, `release_load_queue` |
| `src/ooo_cpu_internal.h` (new header) | Shared `SANITY_*` macros + extern decls | — |

### Shared declarations to hoist into `src/ooo_cpu_internal.h`
- The whole `SANITY_*` macro block (`SANITY_LQ_*`, `SANITY_ROB_*`,
  `SANITY_CHECK_ROB_*`, …) + `TRUE_SANITY_CHECK` scaffolding — referenced across
  fetch/schedule/lsq, so it must be visible in all three fragments (rule 0.3).
  Note `SANITY_CHECK_ROB_MATCH` calls `check_rob(...)` → `check_rob`'s prototype
  must be reachable in any fragment that expands that macro (it's a member, so
  `ooo_cpu.h` already declares it — fine).
- `#ifdef USE_HERMES namespace knob { extern … }` block (schedule/lsq use it).
- The include prologue (`set.h`, `instr_event.h`, `cycle_pack.h`,
  `hash_table7.hpp`, `hash_set3.hpp`, `instruction.h`, `cache.h`) — repeat the
  same includes in each fragment so identical types/templates are instantiated.

### Definition ownership (define once — CRITICAL, checksum lives here)
- `execution_checksum[]`, `ooo_cpu[]`, `current_core_cycle[]`, `stall_cycle[]`,
  `rob_memory_count[]`, `next_mem_sched_start[]`, `SCHEDULING_LATENCY`,
  `EXEC_LATENCY`, `INSTR_TEMPLATE`, `instr_size`: **keep all definitions in the
  shrunk `ooo_cpu.cc`**. `execution_checksum` is `extern`'d by main.cc already, so
  this preserves the existing linkage exactly. The schedule/lsq fragments see
  these via `extern` from `ooo_cpu_internal.h` (matching the existing `extern` in
  `ooo_cpu.h` where present — reuse, don't duplicate).

### Cannot move cleanly
- `handle_branch` twins: must move together with their `#ifdef/#else/#endif`
  (rule 0.5). Kept in shrunk `ooo_cpu.cc`.
- `retire_rob`: it is the sole mutator of `execution_checksum`. Keeping it in the
  same TU as the `execution_checksum` **definition** avoids any cross-TU
  decl/def mismatch on the one symbol the acceptance gate checks. Bind it to
  `ooo_cpu.cc`. (It *could* move with an `extern`, but there is no upside and
  maximal downside — leave it home.)
- The pipeline stages call each other densely; splitting is allowed (out-of-line
  members link via normal symbols) but the grouping above keeps each call cluster
  mostly intra-TU.

### Risk: **MEDIUM–HIGH.** Most globals incl. the checksum, densely chained
stages, two-twin `handle_branch`. Do this only after the other three are proven
identical, so a checksum diff here is unambiguously localized.
### Confirmation: pure code motion — all bodies verbatim; the checksum-defining and checksum-mutating code stay co-located; only `extern`/macro plumbing added.

---

## 5. Recommended global ordering (safest / highest-value first)

1. **`main.cc` → `main_stats.cc`** (the stats/print block). Leaf reporters, cannot
   touch the checksum. **Ship and verify identity first.**
2. `main.cc` → `main_paging.cc` (`va_to_pa`).
3. **`dram_controller.cc`** split (LOW–MEDIUM).
4. **`cache.cc`** split (MEDIUM).
5. **`ooo_cpu.cc`** split (MEDIUM–HIGH) — last, because it owns and mutates
   `execution_checksum`.

After **each** step: `make -f Makefile.win clean && make -f Makefile.win`, run the
fixed trace, and require **bit-identical `execution_checksum` and IPC** before
proceeding. Any diff ⇒ revert that one step and re-inspect the moved macros/decls.

---

## 6. Per-move pure-code-motion confirmation (summary)

| File | New TUs | New header | Shared macros relocated | Definition-owner kept | Hard-bound fns |
|---|---|---|---|---|---|
| main.cc | main_stats.cc, main_paging.cc | main_internal.h | FIXED_FLOAT*, STR_*, *_type, HERMES_LABEL | main.cc (+champsim_rand optional in main_paging.cc) | main, print_deadlock, finish_warmup |
| dram_controller.cc | dram_queue.cc, dram_addr.cc | dram_controller_internal.h | DRAM_FULL_DP, DRAM_CH_DP, SANITY_DRAM_* | dram_controller.cc | none hard |
| cache.cc | cache_queues.cc, cache_debug.cc | cache_internal.h | SANITY_*, CACHE_LVL_*, _DRMIN_* | cache.cc (debug owns ALL_CACHES option) | set_force_all_hits↔FORCE_ALL_HITS |
| ooo_cpu.cc | ooo_cpu_schedule.cc, ooo_cpu_lsq.cc | ooo_cpu_internal.h | SANITY_* (cpu block) | ooo_cpu.cc | handle_branch twins, retire_rob (checksum) |

Every function body is moved **verbatim**. The only added text is `extern`
declarations, free-function prototypes, the `FinalStatsCollector` class decl, and
the relocated macro blocks — all placed in `*_internal.h` headers that each
fragment `#include`s. No symbol is defined twice; no `inline`/template/virtual
boundary is crossed; compiler flags are unchanged. Therefore codegen — and thus
`execution_checksum` and IPC — is bit-identical by construction, and verified by
the rebuild-and-diff gate after every step.
