# cache.cc Block Extraction Priority Book
Generated: 2026-03-24
Goal: extract cohesive blocks into named inline helpers — no logic changes, no call overhead

All extracted functions MUST be `inline`. They live in cache.h or a new cache_helpers.h included by cache.cc.
EQUIVALENCY RULE: every extraction = same inputs → same outputs → same side effects. Manager must approve each before implementation.

---

## RANK 1 — return_to_upper_level (7 call sites, hits handle_read 4.43%)

**Proposed:** `inline void return_to_upper_level(PACKET* pkt, uint16_t cpu_idx)`
**What it does:** `if (pkt->instruction) upper_level_icache[cpu_idx]->return_data(pkt); else upper_level_dcache[cpu_idx]->return_data(pkt);`
**Duplicate sites:** handle_fill, handle_writeback, handle_read, handle_prefetch, add_rq, add_pq — 7 total
**Profile weight:** covers every function that returns data upward; handle_read alone = 4.43%
**Params:** 2 — clean contract
**Equivalency:** identical conditional dispatch extracted verbatim; no state change in the helper itself

---

## RANK 2 — update_replacement_dispatch (6 call sites)

**Proposed:** `inline void update_replacement_dispatch(uint16_t cpu_idx, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t evicted_addr, uint8_t type, uint8_t hit)`
**What it does:** `if IS_LLC → llc_update_replacement_state(...) else update_replacement_state(...)`
**Duplicate sites:** handle_fill ×2, handle_writeback, handle_read, handle_prefetch — 6 total
**Profile weight:** inside every cache operation path
**Params:** 8 (all scalar) — same args already at each call site
**Equivalency:** same dispatch logic, same arguments passed through; no new computation

---

## RANK 3 — emit_dirty_writeback (2 call sites, largest structural dup)

**Proposed:** `inline uint8_t emit_dirty_writeback(uint32_t set, uint32_t way, uint16_t cpu_idx, uint64_t instr_id, uint8_t type)`
**What it does:** check lower WQ occupancy → construct PACKET from block[set][way] → call `lower_level->add_wq`; returns `do_fill` flag
**Duplicate sites:** handle_fill (~lines 244-290), handle_writeback (~lines 643-681) — ~20 filtered lines each
**Profile weight:** largest code duplication in the file; handle_writeback = 1.12%
**Params:** 5
**Equivalency:** only difference between the two sites is source of `instr_id` — passed as param; all other logic identical

---

## RANK 4 — notify_prefetcher_operate (4 call sites, inside handle_read hot path)

**Proposed:** `inline void notify_prefetcher_operate(PACKET& pkt, uint16_t cpu_idx, uint8_t hit)`
**What it does:** 3-branch dispatch: IS_L1D → l1d_prefetcher_operate; IS_L2C → l2c_prefetcher_operate; IS_LLC → (cpu=pkt.cpu; llc_prefetcher_operate; cpu=0)
**Duplicate sites:** handle_read hit path (~856-867), handle_prefetch hit path (~1367-1377), handle_read miss path (~1310-1322), handle_prefetch miss path — 4 total
**Profile weight:** handle_read = 4.43%; prefetcher notify on every hit and miss
**Params:** 3
**Equivalency:** identical 3-branch dispatch; `hit` param passed to prefetcher_operate already

---

## RANK 5 — resolve_bypass_mismatch_l1 + resolve_bypass_mismatch_l2 (inside handle_read 4.43%)

**Proposed L1:** `inline void resolve_bypass_mismatch_l1(int index, int mshr_index)`
**Proposed L2:** `inline void resolve_bypass_mismatch_l2(int index, int mshr_index)`
**What they do:** scan upper-level MSHR, inject LQ deps, clear bypass flag, update fill_level — structurally parallel between L1 and L2 mismatch cases
**Duplicate sites:** handle_read lines ~1134-1218 (L1) and ~1219-1263 (L2)
**Profile weight:** inside handle_read hot path (4.43%); also strategic — makes bypass Stage 2/3 logic legible
**Params:** 2 (this/MSHR/RQ accessed as members)
**Equivalency:** pure extraction, `this` provides all member access; no new computation

---

## RANK 6 — record_miss_stats (3 call sites, trivial but clean)

**Proposed:** `inline void record_miss_stats(uint16_t cpu_idx, uint8_t type)`
**What it does:** increment sim_miss[cpu][type], sim_access[cpu][type], conditional wByP counter
**Duplicate sites:** handle_fill ×2, handle_writeback — 3 call sites, 5 lines each
**Params:** 2
**Equivalency:** identical stat-increment block extracted verbatim

---

## NOT RECOMMENDED (kept for completeness)

| Block | Reason not extracted |
|-------|---------------------|
| add_to_processed_safe | 25+ lines but uses assert + queue-specific overflow check — passing queue object adds param burden, caller context matters |
| merge_bypass_deps_l1d | 50+ lines, 4 sub-cases — valuable clarity but high param count; defer to bypass Stage 2 work |
| detect_mshr_bypass (operate) | 30+ lines; called once per cycle from operate(); extraction adds overhead on every cycle; keep inline only if moved to header |
| merge_mshr_load_deps | Useful but tightly coupled to MSHR slot index and RQ state; 4+ implicit context deps |

---

## Implementation Order (proposed)

1. RANK 1 — `return_to_upper_level` — highest dup count, cleanest contract, 2 params
2. RANK 4 — `notify_prefetcher_operate` — 4 sites inside handle_read hot path
3. RANK 3 — `emit_dirty_writeback` — eliminates largest structural dup
4. RANK 2 — `update_replacement_dispatch` — 6 sites but 8 params; do after simpler ones confirmed
5. RANK 5 — `resolve_bypass_mismatch_l1/l2` — strategic for bypass Stage 2; do last

Each rank = one worker pass. Manager approves before next rank proceeds.
Worker must state equivalency proof for each site before editing.
