# 0. RECOMMENDED — MSHR-Kernel + Block-Array Leaf + Cycle-Drivers + Queue-Ingress + Operate/LPM

Verified against cache.cc (1866 lines): handlers are already broken into named sub-helpers; MSHR is the one true coupling hub.
Five `.inc.cc` fragments, single TU via ordered `#include`, so all calls are intra-TU (no extern).
- `cache_mshr_kernel.inc.cc`: check_mshr, check_mshr_hashmap, add_mshr, probe_mshr, update_fill_cycle, return_data, merge_with_prefetch. Sole writer of MSHR.entry[], bloom, mshr_map, lpm_mshr_* counters, bypass MSHR fields.
- `cache_block_array.inc.cc`: check_hit, get_way, fill_cache, invalidate_entry, FORCE_ALL_HITS. Pure block[set][way] leaf; calls nothing back. Verified zero MSHR/queue contact.
- `cache_cycle_drivers.inc.cc`: handle_fill, handle_writeback, handle_read, handle_prefetch, handle_read_miss_bypass and ALL their handle_*_* sub-helpers. The hot path; calls kernel + array only.
- `cache_queue_ingress.inc.cc`: add_rq, add_wq, add_pq, prefetch_line, get_occupancy, get_size, increment_WQ_FULL, prefetcher_feedback. External API; owns RQ/WQ/PQ tails. (add_rq/add_pq read WQ — kept here.)
- `cache_operate_lpm.inc.cc`: operate() with its ~150-line LPM tick block, set_force_all_hits, print_cache_config, dump_req*, lpm[]/g_byplat[] globals.
Unavoidable coupling: cycle_drivers→kernel (CHECK_MSHR/add_mshr/update_fill_cycle) and →array (fill_cache); all one-directional, intra-TU.
Decoupled cleanly: block_array is a true leaf; kernel is the only MSHR writer so bypass+LPM counter churn stays contained inside it.
Include order: array → kernel → cycle_drivers → queue_ingress → operate_lpm.

# 1. MSHR-Core + Cycle-Drivers + Queue-Ingress + Array-Primitives + Operate-Glue

Proposer L4#1 (design 16). Best of the consensus "MSHR-as-hub" cluster; the only one that names the real sub-helpers, which cache.cc confirms exist.
- `cache_mshr_core.cc`: the six MSHR ops; sole owner of entry[], bloom, hashmap, lpm_mshr_* counters. Callers only need int/void returns.
- `cache_cycle_drivers.cc`: all four handlers plus every handle_fill_*/handle_read_*/handle_writeback_* sub-helper; calls into core + array + ingress.
- `cache_queue_ingress.cc`: add_rq/wq/pq, prefetch_line, return_data, sizing/feedback API.
- `cache_array_primitives.cc`: check_hit, get_way, fill_cache, invalidate_entry; pure block[] leaf, zero reverse coupling.
- `cache_operate_lpm.cc`: operate(), LPM tick, dump/print diag.
Coupling note: array_primitives has zero MSHR/queue contact (verified). cycle_drivers→core calls are frequent but intra-TU.
Slight overstatement: it places return_data in ingress, but return_data is pure MSHR mutation — better grouped with core (the recommendation fixes this).
Otherwise structurally near-identical to the recommendation; loses #0 only on the return_data placement.

# 2. MSHR-lifecycle vs BlockArray-Datapath vs Scheduler

Proposer L2#2 (design 7); merges L2#1 (6), L1#1/#2 (1,2), L3#3 (13) — the dominant "lifecycle + array + dispatch" family.
- `cache_mshr_lifecycle.inc.cc`: add/check/probe/update/return/merge; owns entry[], bloom, map, counters.
- `cache_blockarray.inc.cc`: fill_cache, check_hit, get_way, invalidate_entry PLUS handle_fill + handle_writeback (block[] writers).
- `cache_miss_path.inc.cc`: handle_read, handle_read_miss_bypass, handle_prefetch + queue admission.
- `cache_scheduler.inc.cc`: operate() with LPM tick, sizing/feedback API.
- `cache_util.inc.cc`: print/dump, globals, bypass-model includes.
Coupling note: handle_fill lives with block-array but must call update_fill_cycle (lifecycle) — the one unavoidable cross-fragment call, verified at line 1440/205.
Weaker than #0/#1: putting handle_fill in blockarray splits the hot path across two files and forces a blockarray→lifecycle back-call; #0 keeps all handlers together.
Still strong; clean MSHR/array invariants.

# 3. UpstreamAdmit vs DownstreamFill vs MSHRKernel

Proposer L2#3 (design 8); also covers L3#13's data-flow-direction idea (return vs issue path).
- `cache_mshr_kernel.inc.cc`: shared MSHR contract between the two halves; owns all MSHR state.
- `cache_upstream.inc.cc`: add_rq/wq/pq, prefetch_line, handle_read, handle_read_miss_bypass, handle_prefetch (CPU→cache).
- `cache_downstream.inc.cc`: handle_fill, handle_writeback, fill_cache, check_hit, get_way, invalidate_entry (fill→block[]).
- `cache_operate_lpm.inc.cc`: operate(), LPM tick, sizing API.
- `cache_globals_util.inc.cc`: globals, dump/print, bypass includes, macros.
Coupling note: directional split is elegant, but verified flaw — upstream's handle_read calls check_hit which sits in downstream, creating an upstream→downstream edge the kernel-split was meant to avoid.
The block-array leaf is split across the upstream/downstream boundary instead of isolated; #0/#1 isolate it cleanly. Lower cohesion as a result.

# 4. HotPath vs ColdPath vs Infrastructure

Proposer L2#5 (design 10); merges L1#5 (5) and L3#11 (11) — the frequency/criticality family.
- `cache_hot_handlers.inc.cc`: handle_fill/read/prefetch/writeback/read_miss_bypass + operate(); the per-cycle path together for profiler locality.
- `cache_mshr_ops.inc.cc`: the six MSHR ops PLUS check_hit/fill_cache/get_way/invalidate_entry.
- `cache_queue_interface.inc.cc`: add_*, prefetch_line, sizing/feedback API.
- `cache_lpm_analytics.inc.cc`: extracted lpm_tick() helper, lpm[]/g_byplat[] globals, print.
- `cache_debug_globals.inc.cc`: FORCE_ALL_HITS, dump_req*, bypass includes, macros.
Coupling note: highest cross-fragment call density of the surviving designs — hot_handlers calls into mshr_ops for check_mshr/add_mshr/fill_cache on every miss. Lumps block-array INTO mshr_ops, muddying the clean leaf that #0 isolates.
Genuine win: the LPM tick truly extracts to a named function (verified ~150-line self-contained block, lines 655-813). Ranks here for readability, not coupling.

# 5. Per-Queue Handlers + Shared-Services (naive corrected)

Proposer L4#5 (design 20); represents the naive read/write/pf/fill family (also L3#15, L2/L1 queue-owner variants 3) — kept as the reference anti-pattern.
- `cache_shared_services.cc`: ALL MSHR ops + block[] ops + return_to_upper_level + sizing/diag/globals (the "stdlib").
- `cache_fill_handler.cc`: handle_fill + sub-helpers + return_data + update_fill_cycle.
- `cache_writeback_handler.cc`: handle_writeback + sub-helpers + add_wq.
- `cache_read_handler.cc`: handle_read, handle_read_miss_bypass + sub-helpers + add_rq.
- `cache_prefetch_handler.cc`: handle_prefetch + add_pq + prefetch_line.
Coupling note: verified worst — every handler calls CHECK_MSHR/add_mshr into shared_services (~20+ hot-path cross-fragment calls). Verified bleed: add_rq and add_pq both read WQ (lines 941, 1212), so PQ/RQ handlers reach into WQ territory.
Included only to show why the queue-type axis loses; shared_services becomes a 600+ line omnibus.

# Ranking rationale

Verified cache.cc directly: handlers ARE pre-split into handle_*_* sub-helpers, so hit/miss separability claims hold and L4's designs are most accurate.
Merged families: {1,2,6,7,13}→lifecycle cluster; {5,10,11}→hot/cold; {3,15,20}→queue-owner; {8}→directional; {4,14,18}→bypass/LPM-axis variants folded into notes.
Consensus #1 was the MSHR-hub pipeline (designs 1/6/16). It IS essentially correct, so a hub design stays at top — but the recommendation refines L4#16 by moving return_data into the MSHR kernel.
Moved UP: L4#16 (was proposer-internal #1, now overall #1) for naming real sub-helpers; L2#10 hot/cold up to #4 because its LPM extraction is genuinely clean (verified).
Moved DOWN: bypass-axis designs (L1#4, L4#18) dropped out — verified check_mshr interleaves #ifdef bypass + LPM counters inline (line 1542+), so bypass cannot be a clean fragment.
Moved DOWN: queue-owner designs to #5 — verified add_rq/add_pq read WQ, so the axis structurally bleeds across queue owners.
Directional split (L2#8) ranked #3 not higher: verified check_hit sits on the wrong side of its upstream/downstream cut, re-coupling the halves.
Key invariant driving the order: MSHR kernel is the sole hub; block-array is a true leaf; LPM tick is cleanly extractable; bypass is irreducibly cross-cutting.
Lower real coupling, not vote count, decided ties — that is why the block-array-leaf isolation (present in #0/#1, absent in #4/#5) is the deciding factor.
