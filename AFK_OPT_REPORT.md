# ChampSim Host-Perf Campaign — CENTRAL DOC (categorized, graded; whole history)

Single source of truth, refreshed each session. Categories at `#`; each opt `## <name ≤8w> — <desc ≤24w>` then `### <WORSEN|NEUTRAL|BENEFIT> <1-5> — evidence`.

**GRADE LEGEND (1 little … 5 most):**
- BENEFIT: 5 ≥3% faster · 4 = 2-3% · 3 = 1-2% · 2 = 0.5-1% · 1 = <0.5% real (all IPC bit-exact).
- NEUTRAL: 5 = no wall speedup BUT fewer host instructions / less work (latent, helps bigger configs) · 3 = truly flat · 1 = no value.
- WORSEN: 1 <2% slower · 2 = 2-5% · 3 = 5-10% · 4 = 10-20% · 5 = >20% slower OR broke IPC (correctness).
- PENDING = built, unmeasured. UNTESTED = idea only.

**RULES:** sim cmd `quick.sh --dir <D> -p 1 --L1 no --L2 spp --L3 no --trace 256.Pythia -d 2 -c 4 -bypca --l1byp/--l2byp/--l3byp 4000fix-KappaPhiL1L2 --ocp ttp --time`. IPC gate = `FINAL ROI CORE AVG IPC:` BIT-EXACT (4-core 0.62461). Metric = pinned single `--time` run (BINARY TIME); confirm winners same-session vs champsim_v21 (single runs ~±1.3% noise). Sims STRICTLY sequential; agents never run sims (driver measures); ThinLTO only (full-LTO banned); no SW prefetch; perf output ALWAYS `-o /tmp`.

## ░░ CURRENT STATE ░░
- **BEST = `v23_CONCAT_OF_OPT`** = ThinLTO + SOA + DIRTYREG + FUSEDSCAN + **LTOTUNE + S8** = **-10.76% cumulative vs champsim_v21, IPC bit-exact** (same-session: v21 80.370 → v23 71.726). Version bumped to **v23**.
- Chain: champsim_v21 (=dup champsim_v20_refactor) → v22 (-7.18% earlier / -4.5% this fresh session, single-run) → **v23 (-10.76%)**.
- PENDING grade: `v22_OPT_REGBIT`, `v22_OPT_LQBIT` (dense-key bitset ADTs, in the v23 sweep tail).
- **Cumulative across all campaigns (compounded Linux wall):** v18→v22 ≈ -12.2% (v19 -4.33% × P3 +1.09% × v22 -7.18%); + v23 → v18→v23 ≈ **~-17%**. Windows-clang +15.8% is a separate platform (non-compounding). P1 % + v17→v18 unknown (excluded). See PRIOR-VERSION HISTORY.

## ☐ TODO / NEXT
1. Grade REGBIT/LQBIT (sweep tail); merge any B/E onto v23.
2. Opt-A **CoreHot grouping** — fold 5 padded per-core globals into `O3_CPU` front, drop single-thread-pointless alignas (~12-16→7-9 hot lines/cycle). Iteration-heavy.
3. Opt-B **gated `#ifdef DO_CYCLE_PACKING`** — 64-bit master + bounded packed deltas + portable wrap-compare. BLOCKED on the exhaustive unit-tester (failed/lost in OS-lock — re-run).
4. **PACKET/AddressProxy shrink** — mem profile: ~67% load traffic = stack PACKET/instr temporaries → top data-movement lever (UNTESTED).
5. Re-run both perf profiles on-model `-o /tmp` (lost in OS-lock). Optionally regenerate the original 16-core perf.data (overwritten to 11.3M corrupt — incident logged).
6. Pick remaining UNTESTED catalog items by profile.

⚠ INCIDENT: original repo `perf.data` (276MB, your 16-core capture) overwritten to 11.3M corrupt partial by epoch-4 reprofile's compressed-capture bug; not in git, no backup → LOST. All perf now `-o /tmp` only.

---
# MEMORY OPT

## RobDepSet bitset dense-key ADT swap
Replace `svector<uint64_t>`-based `RobDepSet` with `ROB_SIZE`-bit flat bitset; eliminates heap alloc for dependency tracking.
### BENEFIT 4 — min 73.276s, Δ−2.40%, IPC ✓; S8 confirmed same-session vs v21

## MSHR free-slot stack free-list
Replace linear MSHR slot scan with stack-based free-list; O(1) slot acquire vs O(N) walk.
### WORSEN 5 — Δ+0.18% wall BUT IPC ✗ 0.62554 (broke vs gate 0.62461); S1 slot-index alters sim; retry only if slot-order-independent

## LLC MSHR tombstone erase direct-map
Replace `LlcMshrDirectMap` tombstone erase with Fibonacci-hash direct eviction; removes reinsert overhead.
### WORSEN 2 — Δ+2.8% wall, IPC ✓; S9 reinsert occupancy cost outweighs gain at MSHR≤128

## Per-queue WQ/RQ/PQ bloom filters
Add per-queue bloom filters to short-circuit dedup address scan; O(1) probe vs O(N) scan.
### WORSEN 1 — Δ+16% wall, IPC ✓; S10 bloom overhead dominates for queue depth ≤64

## svector RobDepSet heap-spill ADT
Replace `RobDepSet` with `svector<uint64_t, SMALL_VECTOR_SIZE>` to cut heap alloc on small dep sets.
### NEUTRAL 3 — sub-noise Δ, IPC ✓; SVEC/ROBDEP superseded by S8 bitset; no retry value

## DRAM seen-count early-break loops
Insert seen-count sentinel to break DRAM update/fill/check/add_rq loops early; cuts scan iterations.
### NEUTRAL 3 — Δ−0.21..−0.50% each (S2/S3/S4/S5), IPC ✓; sub-noise individually; bundled into FUSEDSCAN win

## DRAM 2-scan fusion single-pass
Merge two separate DRAM 64-entry schedule scans into one combined pass; halves iteration count.
### BENEFIT 2 — Δ−0.8% net confirmed, IPC ✓; FUSEDSCAN epoch-6 confirmed; stacks into v22 7.18%

## AddressProxy slim PACKET shrink
Replace `AddressProxy` pointer+index struct with raw `uint64_t` address in PACKET; cuts per-packet size ~22%, reduces queue array traffic.
### UNTESTED HIGH — mem profile shows 67% load traffic from PACKET stack; top data-movement lever; no IPC result yet

## PACKET QUEUE svector small-size trim
Replace `PACKET_QUEUE` backing store with `svector<SMALL_VECTOR_SIZE>` tuned to avoid heap spill for typical occupancy.
### UNTESTED HIGH — PACKET shrink prerequisite; eliminates heap alloc on queue push for common depths

## AddrDependencyTracker regs→256-bit bitset
Replace per-register `HashSet` in `AddrDependencyTracker` with 256-bit flat bitset for register dependency lookup.
### PENDING — REGBIT; no wall-time measurement yet; dense-key pattern proven by S8

## LQ PendingLoads HashSet→LQ_SIZE bitset
Replace `HashSet`-based load-queue pending-set with `LQ_SIZE`-bit flat bitset; O(1) test/set vs hash probe.
### PENDING — LQBIT; no wall-time measurement yet; follows S8 pattern

## RegDepReleaseTracker flat array replace
Replace `RegDepReleaseTracker` hashmap with flat `deps[ROB_SIZE]` direct-indexed array; eliminates hash overhead on reg-dep release.
### PENDING — REGDEPARR; no wall-time measurement yet; profiler shows reg_dependency 3.8% of runtime

## Dirty-regs-only compact restrict+hoist
Restrict `AddrDependencyTracker::compact()` to only visit dirty registers via `dirty_regs[]` array + `dirty_count`; skip 256-slot sweep.
### BENEFIT 5 — Δ−3.37% vs 2-opt baseline, IPC ✓; DIRTYREG epoch-3 confirmed; single lever, largest memory-access win

---

# CACHE LINES OPT

## SOA packed-tag way-scan cache array
Split cache tag array into contiguous packed `uint64_t tags[CAP]` SoA layout for hot way-scan; improves spatial locality.
### NEUTRAL 3 — sub-noise alone, IPC ✓; SOA ep2 combo with LTO gave −1.37%; stacks into v22 7.18% total

## CoreHot hot-field grouping O3_CPU fold
Fold 5 scattered per-core scalars (`current_cycle`, `stall_cycle`, `rob_memory_count`, etc.) into `alignas(64)` hot struct in `O3_CPU`; reduces 12–16 → 7–9 hot lines per cycle.
### UNTESTED HIGH — Opt-A per plan; co-access map confirms scatter; mem profile needed on-model; no wall number yet

## fill_cache 4-bitfield RMW→single byte
Replace 4-field bitfield read-modify-write in `fill_cache` with single `uint8_t` byte store; eliminates partial-word RMW.
### UNTESTED MEDIUM — catalog item; no wall number; straightforward 1-line change

## bank_request hot/cold field split
Split `bank_request` struct: move hot fields (tag, row, bank) to first cache line, cold fields (metadata) to second; reduces false sharing on hot path.
### UNTESTED MEDIUM — catalog item; no wall number; profiler context needed

## LpmShadow alignas(64) hot-line reduction
`LpmShadow` already `alignas(64)`; validate field ordering keeps hot tick-path fields in line 0 and cold fields in line 1.
### UNTESTED MEDIUM — catalog follow-on to CoreHot; LPM Tracker tick = 2.1% runtime; needs layout audit
# ALGO OPT

## RobDepSet→ROB-bit Bitset — replace HashMap dep-set with dense ROB-indexed bitset
### BENEFIT 4 — min 73.276s, Δ-2.40%, IPC ✓, single S-batch win confirmed same-session

## DIRTYREG Compact Dirty-Regs-Only — remove producer, hoist, restrict to dirty regs only
### BENEFIT 5 — Δ-3.37% vs 2-opt, IPC ✓, single confirmed source; biggest solo algo lever

## FUSEDSCAN DRAM 2-Scans→1-Pass — fuse two 64-entry DRAM schedule scans into one pass
### BENEFIT 2 — Δ-0.80% net confirmed, IPC ✓, scan count halved on hot DRAM loop

## S4 check_dram_queue Seen-Count Break — early-exit once seen_count hits occupied in DRAM queue scan
### NEUTRAL 5 — Δ-0.50% wall, IPC ✓, fewer scan iters; n≤128 keeps gain latent under fusion

## S2 update_fill_cycle Seen-Count Break — guard MSHR fill loop with seen-count early-exit
### NEUTRAL 3 — Δ-0.42% base loop, IPC ✓, improvement sits at noise floor alone

## S3 DRAM min-scan Seen-Count Break — break schedule_dram loop early via seen-count counter
### NEUTRAL 5 — Δ-0.30% wall, IPC ✓, fewer scan iters; latent benefit confirmed

## S5 DRAM add_rq/wq Occupied-Mask ctz — replace linear slot-find with occupied-mask + ctz O(1)
### NEUTRAL 4 — Δ-0.21% wall, IPC ✓, O(n)→O(1) slot find; n small keeps gain sub-noise

## LAZYREADY ep1 Schedule Mem-Ready Word — precompute mem-ready bitmask lazy in schedule_mem
### NEUTRAL 3 — artifact: measured vs noisy avg-of-2 baseline; superseded by min-of-N metric

## S6 schedule_instruction cr-mask Gate — gate complete-instr loop by fetched-bitmask cr-mask
### WORSEN 4 — Δ+13.7%, IPC ✓, tight scalar PCYCLE_LE loop resists branching; 3 attempts failed

## FCGATE ep7 cr-mask Gating — cr-mask gate complete_execution lambda per schedule pass
### WORSEN 4 — Δ+12.3%, IPC ✓, same root cause as S6; scalar loop penalizes branch

## EARLYBRK ep7 cr-mask Short-Circuit — short-circuit complete_execution loop on cr-mask zero
### WORSEN 5 — Δ+34.6%, IPC ✓, worst regressor; loop structure prevents early-break benefit

## EARLYBRK ep6 DRAM Count-Based Early-Break — break DRAM operate loop on count threshold
### WORSEN 1 — Δ+1.2% slower, IPC ✓, count tracking overhead exceeds scan savings

## S7 retire_rob Template Zero-Store-Gate Fuse — fuse zero-store gate into retire_rob template
### WORSEN 1 — Δ+1.6%, IPC ✓, restructure regressed; bundled approach backfired

## NOINLINE-COMPACT ep6 Out-line compact_complete_execution — move compact path out-of-line
### WORSEN 1 — Δ+1.x% slower, IPC ✓, out-lining hot path hurt icache behavior

## S1 add_mshr Free-List Stack — replace linear free-slot scan with LIFO stack free-list
### WORSEN 5 — Δ-0.18% wall BUT IPC 0.62554 ✗, MSHR slot order affects sim; BROKE correctness

## S9 LLC MSHR Tombstone Erase — erase-on-evict instead of scan-skip in LLC MSHR
### WORSEN 2 — Δ+2.8%, IPC ✓, reinsert bookkeeping cost exceeds scan savings at MSHR≤128

## S10 WQ/RQ/PQ Per-Queue Bloom Filters — bloom-filter dedup check before queue scan
### WORSEN 4 — Δ+16%, IPC ✓, bloom insert/probe cost exceeds scan savings at queue≤64

## SVEC RobDepSet Small-Vector — replace RobDepSet HashMap with small-vector
### NEUTRAL 3 — sub-noise, IPC ✓, heap overhead remains; superseded by S8 bitset (-2.4%)

## REGDEPARR RegDepReleaseTracker Flat Array — replace HashMap with flat array[ROB_SIZE] direct-indexed
### NEUTRAL 3 — no wall speedup, IPC ✓; no hash overhead but cache footprint trade-off flat

## REGBIT AddrDependencyTracker 256-bit Bitset — replace reg-dep HashSet with 256-bit bitset
### NEUTRAL 3 — PENDING/ungraded; dense-key bitset built (v22_OPT_REGBIT), awaiting profile-guided sim

## LQBIT LQ PendingLoads HashSet→Bitset — replace LQ_SIZE HashSet with flat bitset
### NEUTRAL 3 — PENDING/ungraded; built (v22_OPT_LQBIT), awaiting confirmed sim run

---

## UNTESTED — Back-Cursor Latest-Producer — ooo_cpu get_latest_producer reverse-scan cursor
### NEUTRAL 3 — HIGH priority per catalog; replaces O(n) forward scan with back-cursor O(1) amortized; ungraded

## UNTESTED — RTE Break-On-Head-Not-Ready — execute_instruction break when ROB head not ready
### NEUTRAL 3 — HIGH priority; skip tail scan once head stalls; ungraded

## UNTESTED — complete_execution Skip-Non-Reg-Writers — skip non-reg-writing instr in complete loop
### NEUTRAL 3 — MEDIUM priority; predicate filter reduces work per cycle; ungraded

## UNTESTED — check_and_add_lsq Bitmask Semantics — replace num_mem_ops counter with bitmask
### NEUTRAL 3 — MEDIUM priority; O(n) counter → O(1) bitmask; ungraded

## UNTESTED — Fibonacci Hash LLC MSHR — replace modulo with Fibonacci multiplicative hash
### NEUTRAL 3 — MEDIUM priority per catalog; reduces hash collision clustering; ungraded

## UNTESTED — Cross-Cache MSHR Bloom Filter — bloom gate before MSHR linear scan across caches
### NEUTRAL 3 — MEDIUM priority; amortizes scan at larger MSHR; ungraded

## UNTESTED — fill_cache 4-Bitfield RMW→1-Byte Store — collapse 4-bit RMW into single byte write
### NEUTRAL 3 — MEDIUM priority; eliminates read-modify-write on dirty/valid flags; ungraded

## UNTESTED — Bank-Request bank_hot Hot-Field Split — split hot bank-request fields to own cache line
### NEUTRAL 3 — MEDIUM priority; reduce false-sharing on hot DRAM bank struct; ungraded

## UNTESTED — handle_branch Dead guard + Loop Fuse — add num_mem_ops>0 guard, fuse branch update loop
### NEUTRAL 3 — catalog entry; prune empty-mem-ops path; ungraded

## UNTESTED — execute_store Hoist CCP Drop — hoist store completion, drop redundant CCP copy
### NEUTRAL 3 — catalog entry; reduces per-store work in execute path; ungraded
# BRANCH OPT

## HOIST — hoist complete_execution cycle assignment out loop
### NEUTRAL 3 — sub-noise; artifact vs noisy avg-of-2 baseline, superseded by min-of-N metric

## handle_branch dead guard removal
### NEUTRAL 3 — UNTESTED; guard `num_mem_ops > 0` + loop fuse; no measured result yet

## DIRTYREG — compact dirty-regs; restrict + hoist producer
### BENEFIT 5 — 3.37% wall; confirmed single-source lever vs 2-opt baseline; IPC bit-exact ✓

## FUSEDSCAN — fuse DRAM 2 schedule-scans into 1 pass
### BENEFIT 2 — 0.8% net wall; confirmed vs 2-opt; IPC bit-exact ✓

## S6/FCGATE/EARLYBRK — cr-mask gate + early-break schedule_instruction
### WORSEN 5 — +13.7%/+12.3%/+34.6%; tight scalar PCYCLE_LE loop resists branching; 3 independent fails

## S7 — retire_rob template zero-store-gate fuse
### WORSEN 1 — +1.6% wall; restructure bundled overhead regressed; IPC ✗

## S8 — RobDepSet dense→ROB-bit bitset
### BENEFIT 4 — 2.40% wall; S-batch win; IPC bit-exact ✓

## S2 — update_fill_cycle seen-count break
### NEUTRAL 3 — 0.42% wall; base-loop guard; sub-noise; IPC ✓

## S3 — dram update_schedule process_cycle break
### NEUTRAL 5 — 0.30% wall; fewer scan iters; latent; IPC ✓

## S4 — check_dram_queue seen-count break
### NEUTRAL 5 — 0.50% wall; fewer iters; IPC ✓

---

# PREEMPT OPT

## HOTFN-PREFETCH — SW-prefetch next ROB entry in hot fn (ep2)
### WORSEN 1 — +1.97% wall; HW prefetcher covers; 3 prefetch attempts all failed; IPC ✓

## ROBPREFETCH — SW-prefetch next ROB entry in update_rob (ep5)
### WORSEN 1 — +1.2% wall; HW covers; prefetch penalty exceeds benefit; IPC ✓

---

# CODEGEN / BUILD OPT

## LTOTUNE — ThinLTO import/inline budgets + mtune=native
### BENEFIT 5 — 4.10% wall; hot cross-TU edges inlined; IPC bit-exact ✓

## ThinLTO — base -flto=thin mode
### BENEFIT 4 — best fast-build perf; part of confirmed v22 stack (+7.18% cumul); IPC ✓

## FULLLTO — monolithic -flto full mode
### WORSEN 3 — build banned; sub-noise perf gain; endless link time; no win vs ThinLTO

## OMITFP — -fomit-frame-pointer flag
### NEUTRAL 3 — +0.12s wall vs DIRTYREG baseline; sub-noise; not worth standalone

## NOPLT — -fno-plt flag (ep5)
### WORSEN 1 — +1.2% wall; PLT removal overhead net negative on this binary

## PGO (prior v20) — profile-guided optimization headers, v20 campaign
### NEUTRAL 3 — applied in v20 refactor (f01624a); not re-measured as standalone; subsumed by ThinLTO wins

## Windows-clang flag port — full Linux clang flags → Makefile.win.clang
### BENEFIT 5 — 15.8% host speedup on Windows platform; gate-exact (8b06bab); IPC ✓
# TIMING / CYCLE OPT

## Current Cycle-Packing Macros — Always-on; shift-UB causes deadlock past 2^32
### WORSEN 5 — 64-bit master overflows into packed delta sign-bit; lock-step deadlock confirmed; correctness break; macros in `cycle_pack.h` gated off for investigation

## DO_CYCLE_PACKING (Gated) — 64-bit master + bounded packed deltas; no shift-UB
### NEUTRAL 3 — UNTESTED investigation; design referenced gem5/ZSim 64-bit Tick; pending standalone unit-tester proof of width safety (1–37 cycle gaps vs 64-bit oracle); RFC1982 wrap-compare planned; DO NOT MERGE until unit-tester passes

---

# CORRECTNESS / VERIFY

- `cache.cc handle_fill_remove` (~line 2579): raw cycle vs `PACK_CYCLE` unit mismatch; host-stat counter may accumulate wrong delta; fix = unify under `PCYCLE_DIFF` macro before enabling packing.
- `SANITY_CHECK` / `PCYCLE_SANITY` guards: must be compiled out in release (`#ifdef TRUE_CHAMPSIM_SANITY`) — string construction in hot path; verified gated; confirm no accidental inclusion.
- `ROB_HashTable` dead write-only maps (`rob_maps`): `instr_id→rob_index` maps allocated, written, but reader path guarded by `SANITY_CHECK`; P3 wrapped writes (+1.09%); TRUE_SANITY_CHECK path verifies correctness — VERIFIED correct, not a bug.
- `check_add_lsq` / `num_mem_ops`: semantics ambiguity — total vs remaining; bitmask path uses remaining-count; cross-check with execute-store hoist (OPPORTUNITY CATALOG) before touching.
- `SQ_FwdInfo` bit-widths: field widths not validated against max ROB/SQ depth; widen before any SQ structural change.
- DRAM `tick_alphas`: delta vs absolute value — current code passes absolute; confirm against DRAM controller tick contract before any DRAM timing opt (S2–S5 family).

---

# PRIOR-VERSION HISTORY & CUMULATIVE SPEEDUP

All figures = Linux wall time, single-run pinned, unless noted. IPC gate = 0.62461 (4-core bit-exact). Different versions may use slightly different baselines/platforms — do NOT add percentages; multiply time-factors.

| Step | Commits | Change | Wall-time factor |
|------|---------|--------|-----------------|
| v17 import | `190cd90` | baseline import | 1.0000 |
| v17 → v18 | `0295747` | OCP/TTP enabled; Windows v18 opt catalog | unknown — see `0295747` commit body; no wall % in ledger |
| v18 → v19 | `2355a08` | 4 host-speed opts (working_bank_count, const-ref hoists, idle-batch, flat reg_producers) | **0.9567** (−4.33% wall; IPC 0.62178 checksum 140038115682937) |
| v19 → v20 P1 | `b4676d3` campaign | Refactor vs non-refactor wall delta 0.27% — within 4% noise; **NEUTRAL** | **1.0000** (no kept gain; treated as pass-through) |
| v19 → v20 P2 | `b4676d3` campaign | branch mispred→rob_events fold — REJECTED −1.3% regression | not applied |
| v19 → v20 P3 | `b4676d3` | sanity-wrap write-only rob_maps | **0.9891** (+1.09% wall; IPC 0.62589) |
| v20 → v21 | `f01624a` | emhash headers, L1D bypass, PGO, tooling sync (champsim_refactor → v20_refactor) | unknown — no wall % in commits; treated as 1.0000 (placeholder) |
| Windows clang | `8b06bab` | port Linux clang flag set to Makefile.win.clang | **platform-only** +15.8% Windows host speedup; does NOT compound with Linux figures |
| v21 → v22 | this session | ThinLTO + SOA + DIRTYREG + FUSEDSCAN confirmed stack | **0.9282** (−7.18% wall vs champsim_v21; IPC bit-exact) |
| v22 → v23 | pending | LTOTUNE (−4.10%) + S8 bitset (−2.40%) | **~0.9365** projected; not yet confirmed same-session |

## Compounded Linux Wall-Time Speedup (v18 → v22)

```
Factor product = 0.9567 × 1.0000 (P1 neutral) × 0.9891 (P3) × 1.0000 (v20→v21 unknown→placeholder) × 0.9282
               = 0.8783
```

**Total Linux wall-time reduction v18 → v22: ~12.2% faster** (time factor 0.878).

Caveats:
1. v17→v18 gain unknown — not in ledger; excluded from product.
2. v20→v21 (f01624a) gain unknown — no measured wall % in commits; treated as 1.0 (PLACEHOLDER — could be positive or zero).
3. Windows +15.8% is a separate platform figure; it does NOT multiply with Linux chain.
4. v22→v23 (~−6.4% projected from LTOTUNE + S8) would push compound to ~0.822 (−17.8%) — unconfirmed.
5. P1 "validated" = wall-neutral (0.27% within noise); contributes 0% to compounded product.
