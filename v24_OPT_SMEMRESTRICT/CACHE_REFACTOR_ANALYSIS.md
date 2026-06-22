# cache.cc Refactor Decision Doc

Verified against `src/cache.cc` (1866 lines) and `inc/cache.h` (1552 lines). All cited line numbers confirmed by direct read.

---

# 1. User's 4-file split (RQ_WQ | PQ_Fill | op | thin operator)

**VERDICT: NEEDS-TWEAK** (works only with a 5th MSHR fragment + an extracted `lpm_tick()`).

- B1 (RQ/WQ): `handle_writeback` 242-319, `handle_read` 383-470, `add_rq` 935-1146, `add_wq` 1148-1180.
- B2 (PQ/Fill): `handle_fill` 53-208, `handle_prefetch` 472-646, `add_pq` 1210-1275, `prefetch_line` 1182-1208.
- B4 (operator): `operate` 649-826. B3 (getters): `check_hit`, `get_way`, `get_occupancy`, etc.
- MSHR has NO home: `check_mshr` 1530-1670, `add_mshr` 1785-1833, `update_fill_cycle` 1445-1520, `return_data` 1277-1443.
- Both B1 AND B2 call MSHR: B1 at L270, L415; B2 at L531, L554, L570, L145 — forces a cross-bucket leak wherever MSHR lands.
- `update_fill_cycle` called from B2 `handle_fill` L145 and from `return_data` L1440 — shared.
- `check_hit` (B3) called from B1 (L258, L395) and B2 (L488) — downward, acceptable low coupling.
- `handle_prefetch` L555 and `handle_read_miss_bypass` L342/360/376 call `lower_level->add_rq` (other instance) — not a leak.
- **thin operator NOT achievable as-is**: `operate` L655-812 is a 163-line inline LPM tick reading raw `MSHR.occupancy`, `lpm_mshr_*` counters; only L815-825 (11 lines) are dispatch.
- Minimal tweak: add 5th fragment `cache_mshr.h` (MSHR fns) + extract `lpm_tick()` to `cache_lpm_tick.h`; then operate = 11-line dispatch.
- Cleaner boundary: adopt the 5-file split (B1, B2, mshr-kernel, operate+lpm, getters), not the user's 4.

---

# 2. Never-nester (<=2 nesting depth)

**VERDICT: PARTIAL** — 10 functions exceed depth 2; most are cleanly flattenable by pure extraction.

- Depth 5: `handle_prefetch` (472-646, deepest ~L562) and `operate` (deepest only in `#ifdef SANITY_CHECK` per-CPU scan L774-795; production depth 3).
- Depth 4: `check_mshr` (~L1654), `check_mshr_hashmap` (~L1770), `handle_writeback` (~L274), `add_mshr` (~L1809), `add_rq` (~L1039).
- Depth 3: `handle_fill` (~L119), `handle_read` (~L442), `return_data` (~L1354), `update_fill_cycle` (for→if→if).
- Depth <=2 (16 fns): accessors, `check_hit`, `invalidate_entry`, `add_wq`, `add_pq`, `fill_cache`, `merge_with_prefetch`, `probe_mshr`, etc.
- Clean extractions (low risk): `lpm_mshr_adjust_counters` from check_mshr L1637-1663; `lpm_mshr_track_new_entry` from add_mshr L1809-1821.
- Clean extractions: `sanity_check_lpm_per_cpu` from operate L768-797 (assert-only, very low risk).
- `handle_prefetch` non-LLC arm L557-577 extractable to drop outer depth 5→3; inflight arm L585-627 medium risk (bypass ifdefs).
- `handle_writeback` RFO-miss ladder L266-292 already calls named helpers; extract dispatch to reach depth 2.
- Medium risk: `add_rq` RFO/LOAD bypass-reconcile blocks L1033-1112 (touch RQ.entry fields, no aliasing).
- All extractions are member-fn code motion, same TU, no ABI/behavior change.

---

# 3. ~1K-line target

**VERDICT: PARTIAL** — ~380 behavior-neutral lines removable → ~1480; reaching 1000 needs sub-helper fragments to absorb ~480 more.

- Cluster 1: `check_mshr` (1530-1670) vs `check_mshr_hashmap` (1673-1782) near-clone → shared `mshr_merge_found()`, ~100 lines.
- Cluster 2: PF→demand prior/restore in `merge_with_prefetch` 210-240, dup at 1571-1614 and 1691-1730 → ~65 lines.
- Cluster 3: bypass-flag-clear triple (handle_prefetch 597-621, add_rq 1038-1065/1075-1103) → macro, ~35 lines.
- Cluster 5: `add_rq`/`add_wq`/`add_pq` enqueue tails (1121-1145, 1159-1179, 1249-1274) near-identical → ~30 lines.
- Cluster 6: `handle_prefetch` inflight arm 585-627 duplicates `check_mshr` merge → reroute via CHECK_MSHR, ~40 lines (medium risk: PF-only fill_level guard L594 must survive).
- Cluster 7: `operate` TRACKER_LPM_SHARED L673-682 — both branches byte-identical, dead `#ifdef` → 6 lines, zero risk.
- Offload DP/SANITY/commented-dead blocks to `cache_debug.h` → ~100 lines.
- Realistic floor: ~810 lines of genuinely non-redundant datapath logic in this TU.
- 1866 − 380 ≈ **1480 lines** conservative; 1000 only if `handle_*` sub-helper fragments (currently in cache.h) absorb ≥480.

---

# 4. Bring back cache.h methods

**VERDICT: YES** — ~960 lines of datapath logic are wrongly inlined in the header; move the 34 stage helpers + constructor + bloom_rebuild.

- Constructor `CACHE(...)` 460-524 (~65 lines): full allocation/init body, no inline reason → move to cache.cc.
- `bloom_rebuild_mshr()` 240-256: MSHR_SIZE loop, two paths → move to cache.cc.
- 34 stage helpers, all called ONLY from cache.cc's `handle_read/writeback/fill/prefetch`: move out, drop `inline`.
- Largest offender: `handle_fill_processed_and_bypass_return` 1308-1503 (196 lines) inlined → move.
- Others: `handle_read_miss_inflight_bypass_l1_mismatch` 850-910 (61), `..._l2_mismatch` 914-981 (68), etc.
- No template/cross-TU use of these helpers exists; moving is behavior-identical (compiler unlikely to inline 60-196 line bodies anyway).
- MUST stay in header: `get_set()` 1543 (constexpr, constant-folds at call sites).
- MUST stay: `lpm_shadow_inc()` 274 (3-field hot increment) and `l1d/l2c/llc_bypass_fill` stubs 322-328 (`#ifndef`-guarded defaults).
- MAY stay (low risk): `~CACHE()` 527 (5 lines); `return_to_upper_level()` 591 moves with the helper batch if its callers move.

---

# 5. #ifdef → DP()-style macros + unite repeats

**VERDICT: YES** — ~89 live `#ifdef` sites; four repeated-body families (36 sites) collapse to ~13 macro calls, token-identical.

- Families: BYPASS_L1_LOGIC (22), BYPASS_L2_LOGIC (24), BYPASS_LLC_LOGIC (22), USE_HERMES (3), LLC_BYPASS (2), USE_LLC_HASHMAP_MSHR (3), LPM_STRICT_MISS (6), SANITY_CHECK (2), TRACKER_LPM_SHARED (2 — dead).
- Core macros: `IF_BYP_L1/L2/LLC(s)` wrappers; composites `BYP_PRIOR_SAVE/RESTORE`, `BYP_DEMAND_CLEAR`, `BYP_READ_STATE`, `byplat_on_fill()`.
- `byplat_on_fill`: 3-ifdef triplet at fill_cache L842-853 (verified) + add_rq → 9 sites → 3 calls.
- PF-takeover prior save/restore: 12 sites across check_mshr (1576-1612) + check_mshr_hashmap → 4 calls.
- demand-clear: 6 sites (1617-1634 + hashmap) → 2 calls. `BYP_READ_STATE`: 15 sites → 5 calls.
- Hermes: `HERMES_MARK_OFFCHIP`/`HERMES_TRACK_EVICT` (L168, L855); MSHR: `MSHR_INSERT_ADDR`/`MSHR_ERASE_ADDR` (L138, L1795).
- LPM_STRICT_MISS: `LPM_MISS_ACTIVE()`/`_PC(c)` collapse 6 sites at L658/703/719/761.
- Behavior-preserving: preprocessor substitutes before compile; `do{}while(0)` guards dangling-else; undefined-symbol arms expand to nothing.

before/after (fill_cache L842-853, BYPASS_L1/L2/LLC):
```c
// before: 3 separate #ifdef blocks, 12 lines
// after:  byplat_on_fill(cache_type, packet->cpu, packet->address >> LOG2_BLOCK_SIZE);
```

---

# RECOMMENDED PLAN

**Adopt a HYBRID: the 5-file MSHR-kernel split, not the user's 4-file split.** The user's model has no home for the MSHR subsystem, which both read (B1) and prefetch/fill (B2) call; a dedicated `cache_mshr.*` removes that mandatory cross-bucket leak, and `operate` cannot be thin until its 163-line LPM body is extracted.

PHASE 0 — PURE CODE MOTION (safe, bit-identical, do first, no gate):
- Move the 34 header stage helpers + constructor + `bloom_rebuild_mshr` from cache.h to cache.cc (Section 4). ~960 lines relocated.
- Delete dead `#ifdef TRACKER_LPM_SHARED` duplicate at L673-682 (both arms identical). 6 lines.
- Offload DP/SANITY/commented-dead blocks to cache_debug.h. ~100 lines.
- Extract assert-only `sanity_check_lpm_per_cpu` (operate L768-797).

PHASE 1 — MECHANICAL MACROS (safe, token-identical, light gate to confirm no diff):
- Introduce `IF_BYP_*` + composite bypass macros and `byplat_on_fill` (Section 5). Collapses 36 sites → 13.
- Extract `lpm_tick()` from operate L655-812 → makes operate a thin 11-line dispatcher.

PHASE 2 — STRUCTURAL SPLIT (gate: build all configs, diff a reference sim run):
- Split into cache_rq_wq.cc, cache_pq_fill.cc, cache_mshr.cc, cache_operate.cc (+lpm), cache_getters.cc.

PHASE 3 — BEHAVIOR-RISKY DEDUP (gate carefully, one cluster per commit, regression-check):
- Cluster 1/2: unify check_mshr / check_mshr_hashmap / merge_with_prefetch — the I3 LLC fix (L1602-1608) MUST be preserved.
- Cluster 6: reroute handle_prefetch inflight arm through CHECK_MSHR — PF-only fill_level guard (L594) MUST survive.
- Cluster 5: enqueue-tail template across add_rq/add_wq/add_pq.

Rationale: Phases 0-1 (~1100 lines moved/collapsed) are bit-identical and unlock the thin orchestrator with zero behavioral risk. Phase 3 carries the bypass/LPM semantic hazards and is where kids'-lives-at-stake correctness matters most — gate each cluster independently. The honest landing point is ~1480 lines in this TU after Phase 3; 1000 is only reached if the relocated sub-helper fragments are counted out of this file.
