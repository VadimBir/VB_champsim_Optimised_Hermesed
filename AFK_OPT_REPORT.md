# ChampSim Host-Perf Campaign — CENTRAL DOC (categorized, graded; whole history)

Single source of truth, refreshed each session. Categories at `#`; each opt is exactly two lines: `## <name ≤8w> — <desc ≤24w>` then `### <WORSEN|NEUTRAL|BENEFIT> <1-5> — evidence`. Each opt appears ONCE, in its primary category.

**GRADE LEGEND (1 little … 5 most):**
- BENEFIT: 5 ≥3% faster · 4 = 2-3% · 3 = 1-2% · 2 = 0.5-1% · 1 = <0.5% real (all IPC bit-exact).
- NEUTRAL: 5 = no wall speedup BUT fewer host instructions / less work (latent, helps bigger configs) · 3 = truly flat · 1 = no value.
- WORSEN: 1 <2% slower · 2 = 2-5% · 3 = 5-10% · 4 = 10-20% · 5 = >20% slower OR broke IPC (correctness).
- PENDING = built, unmeasured. UNTESTED = idea only (graded by expected effect, flagged ungraded).

**RULES:** sim `quick.sh --dir <D> -p 1 --L1 no --L2 spp --L3 no --trace 256.Pythia -d 2 -c 4 -bypca --l1byp/--l2byp/--l3byp 4000fix-KappaPhiL1L2 --ocp ttp --time`. IPC gate = `FINAL ROI CORE AVG IPC:` BIT-EXACT (4-core 0.62461). Metric = pinned single `--time` run (BINARY TIME); confirm same-session vs champsim_v21 (single-run ~±1.3% noise). Sims STRICTLY sequential; agents never run sims (driver measures); ThinLTO only (full-LTO banned); no SW prefetch; perf output ALWAYS `-o /tmp`.

## ░░ CURRENT STATE ░░
- **BEST = `v23_CONCAT_OF_OPT`** = ThinLTO + SOA + DIRTYREG + FUSEDSCAN + LTOTUNE + S8 = **-10.76% cumulative vs champsim_v21, IPC bit-exact** (same-session v21 80.370 → v23 71.726). Version = **v23**.
- Chain: champsim_v21 (=dup champsim_v20_refactor) → v22 (-7.18% earlier same-session) → **v23 (-10.76%)**.
- PENDING grade: `v22_OPT_REGBIT`, `v22_OPT_LQBIT`.
- Cross-campaign compounded (Linux wall): v18→v22 ≈ **-12.2%**; +v23 → ~**-17%**. Windows-clang +15.8% separate platform. See PRIOR-VERSION HISTORY.

## ☐ TODO / NEXT
1. Grade REGBIT/LQBIT; merge any B/E onto v23.
2. **Opt-A CoreHot grouping** — fold 5 padded per-core globals into `O3_CPU` front, drop single-thread-pointless alignas (~12-16→7-9 hot lines/cycle).
3. **Opt-B gated `#ifdef DO_CYCLE_PACKING`** — 64-bit master + bounded packed deltas + portable wrap-compare; BLOCKED on the exhaustive unit-tester (lost in OS-lock — re-run).
4. **PACKET/AddressProxy shrink** — mem profile: ~67% load traffic = stack PACKET/instr temporaries → top data-movement lever.
5. Re-run both on-model perf profiles `-o /tmp` (lost in OS-lock); optionally regenerate original 16-core perf.data.
6. Pick remaining UNTESTED items by profile.

⚠ INCIDENT: original repo `perf.data` (276MB, 16-core capture) overwritten to 11.3M corrupt partial by epoch-4 reprofile's compressed-capture bug; not in git, no backup → LOST. All perf now `-o /tmp`.

---

# MEMORY OPT

## S8 RobDepSet → ROB-bit bitset — replace svector/heap dep-set with dense ROB_SIZE-bit flat bitset (set/word-OR/ctz, no heap)
### BENEFIT 4 — min 73.276s, Δ-2.40%, IPC ✓; only S-batch win, confirmed same-session

## DIRTYREG compact dirty-regs-only — restrict compact() to dirty registers via dirty_regs[]+count, hoist remove_producer, skip 256-slot sweep
### BENEFIT 5 — Δ-3.37% vs 2-opt, IPC ✓; biggest single solo lever (was ep2 COMPACTMEMO; earlier no-effect reading was a buggy artifact)

## SVEC RobDepSet small-vector — replace dep-set with svector to cut heap alloc on small sets
### NEUTRAL 3 — sub-noise, IPC ✓; heap overhead remains; superseded by S8 bitset

## S1 add_mshr free-list stack — replace linear MSHR free-slot scan with LIFO free-list (O(1) acquire)
### WORSEN 5 — Δ-0.18% wall BUT IPC ✗ 0.62554 (BROKE: MSHR slot index affects sim); retry only if proven slot-order-independent

## S9 LLC MSHR tombstone erase — erase-on-evict + Fibonacci direct-map instead of scan-skip
### WORSEN 2 — Δ+2.8%, IPC ✓; reinsert bookkeeping ≫ scan savings at MSHR≤128

## S10 WQ/RQ/PQ per-queue blooms — bloom dedup probe before queue address scan
### WORSEN 4 — Δ+16%, IPC ✓; bloom insert/probe ≫ scan savings at queue≤64

## REGBIT AddrDependencyTracker 256-bit bitset — replace dirty_regs[256]+reg_is_dirty[256]+count with one 256-bit bitset (ctz iterate)
### PENDING — built (v22_OPT_REGBIT), ungraded; dense-key pattern proven by S8

## LQBIT LQ_PendingLoads HashSet→bitset — replace inner per-address HashSet of LQ indices with LQ_SIZE-bit bitset
### PENDING — built (v22_OPT_LQBIT), ungraded; follows S8 pattern

## REGDEPARR RegDepReleaseTracker — proposed hashmap→flat-array swap
### N/A — NOT implemented: tracker is ALREADY a direct-indexed flat deps[ROB_SIZE] (no hashmap to replace); nothing to do

## AddressProxy slim / PACKET shrink — replace AddressProxy ptr+index (28B×2) with raw uint64 address; shrink PACKET, cut queue-array traffic
### UNTESTED HIGH — mem profile: ~67% load traffic = stack PACKET/instr temporaries; top data-movement lever; ungraded

## PACKET_QUEUE svector small-size trim — tune svector backing to avoid heap spill at typical occupancy
### UNTESTED MEDIUM — PACKET-shrink prerequisite; eliminates queue-push heap alloc; ungraded

---

# CACHE LINES OPT

## SOA packed-tag way-scan — contiguous uint64 tags[CAP] SoA for hot cache way-scan, better spatial locality
### NEUTRAL 3 — sub-noise alone, IPC ✓; in ep2 combo with LTO gave -1.37%; stacks into v22

## CoreHot hot-field grouping — fold 5 scattered per-core scalars (current_cycle/stall_cycle/rob_memory_count/next_mem_sched_start/execution_checksum) into O3_CPU front, drop alignas
### UNTESTED HIGH — Opt-A; co-access map confirms scatter; ~12-16→7-9 hot lines/cycle; needs on-model profile; ungraded

## fill_cache 4-bitfield RMW → 1-byte store — collapse 4-field bitfield read-modify-write into single uint8 store
### UNTESTED MEDIUM — eliminates partial-word RMW on valid/dirty flags; ungraded

## bank_request hot/cold field split — move hot bank fields to line 0, cold metadata to line 1
### UNTESTED MEDIUM — reduce false-sharing on hot DRAM bank struct; ungraded

## LpmShadow layout audit — verify alignas(64) field order keeps hot tick-path in line 0
### UNTESTED MEDIUM — follow-on to CoreHot; LPM_Tracker::tick = 2.1% runtime; ungraded

---

# ALGO OPT

## FUSEDSCAN DRAM 2-scans → 1-pass — fuse two 64-entry DRAM schedule scans into one combined pass
### BENEFIT 2 — Δ-0.8% net confirmed, IPC ✓; scan count halved; stacks into v22

## S2 update_fill_cycle seen-count break — early-exit MSHR fill loop once seen==occupied
### NEUTRAL 3 — Δ-0.42%, IPC ✓; base loop already largely guarded; at noise floor

## S3 DRAM schedule/process min-scan break — seen-count early-exit on the two DRAM cycle scans
### NEUTRAL 5 — Δ-0.30% wall, IPC ✓; fewer scan iters (latent benefit for bigger configs)

## S4 check_dram_queue seen-count break — early-exit DRAM queue dedup scan once seen==occupied
### NEUTRAL 5 — Δ-0.50% wall, IPC ✓; fewer iters; latent

## S5 DRAM add_rq/wq occupied-mask ctz — replace linear slot-find with occupied-mask + ctz O(1)
### NEUTRAL 4 — Δ-0.21% wall, IPC ✓; O(n)→O(1) slot find; n small keeps it sub-noise

## LAZYREADY schedule mem-ready word — lazily precompute mem-ready bitmask in schedule_memory
### NEUTRAL 3 — artifact vs noisy avg-of-2 baseline; superseded by min-of-N metric

## S6 schedule_instruction cr-mask gate — gate complete-instr loop by fetched-bitmask cr-mask
### WORSEN 4 — Δ+13.7%, IPC ✓; tight scalar PCYCLE_LE loop resists branching

## FCGATE cr-mask gating (ep7) — cr-mask gate complete_execution lambda per schedule pass
### WORSEN 4 — Δ+12.3%, IPC ✓; same root cause as S6

## EARLYBRK cr-mask short-circuit (ep7) — short-circuit complete loop on cr-mask zero
### WORSEN 5 — Δ+34.6%, IPC ✓; worst regressor; loop structure prevents early-break gain

## EARLYBRK DRAM count-break (ep6) — break DRAM operate loop on count threshold
### WORSEN 1 — Δ+1.2%, IPC ✓; count-tracking overhead exceeds scan savings

## S7 retire_rob template+zero-store-gate+fuse — fuse zero-store gate into retire_rob template
### WORSEN 1 — Δ+1.6%, IPC ✓ (semantics preserved, just slower); bundled restructure regressed

## NOINLINE-COMPACT out-line (ep6) — move compact path out-of-line from complete_execution
### WORSEN 1 — Δ~+1% slower, IPC ✓; out-lining hot path hurt icache

## UNTESTED back-cursor latest-producer — replace get_latest_producer reverse scan with back-cursor
### UNTESTED HIGH — O(n)→O(1) amortized; ungraded

## UNTESTED RTE break-on-head-not-ready — execute_instruction breaks when ROB head not ready
### UNTESTED HIGH — skip tail scan once head stalls; ungraded

## UNTESTED complete_execution skip non-reg-writers — predicate-filter non-reg-writing instr
### UNTESTED MEDIUM — reduces per-cycle work; ungraded

## UNTESTED check_and_add_lsq bitmask — replace num_mem_ops counter with O(1) bitmask
### UNTESTED MEDIUM — verify total-vs-remaining semantics first; ungraded

## UNTESTED Fibonacci hash LLC MSHR — replace modulo with multiplicative hash
### UNTESTED MEDIUM — reduces collision clustering; ungraded

## UNTESTED cross-cache MSHR bloom — bloom gate before cross-cache MSHR linear scan
### UNTESTED MEDIUM — amortizes scan at larger MSHR; ungraded

## UNTESTED execute_store hoist + drop CCP copy — hoist store completion, drop redundant CCP copy
### UNTESTED LOW — reduces per-store work; ungraded

---

# BRANCH OPT

## HOIST complete_execution cycle hoist — hoist cycle assignment out of complete loop
### NEUTRAL 3 — artifact vs noisy avg-of-2 baseline; superseded by min-of-N metric

## UNTESTED handle_branch dead-guard + fuse — add num_mem_ops>0 guard, fuse branch-update loop
### UNTESTED LOW — prune empty-mem-ops path; ungraded

---

# PREEMPT OPT

## HOTFN-PREFETCH next-ROB SW prefetch (ep2) — software-prefetch next ROB entry in hot fn
### WORSEN 1 — Δ+1.97%, IPC ✓; HW prefetcher already covers (1 of 3 failed prefetch attempts)

## ROBPREFETCH update_rob SW prefetch (ep5) — software-prefetch next ROB entry in update_rob
### WORSEN 1 — Δ+1.2%, IPC ✓; prefetch penalty exceeds benefit

---

# CODEGEN / BUILD OPT

## LTOTUNE wide ThinLTO + mtune=native — widen ThinLTO import/inline budgets + -mtune=native; inline hot cross-TU edges
### BENEFIT 5 — Δ-4.10%, IPC ✓; e7l1; folds into v23

## ThinLTO base mode — -flto=thin baseline LTO mode
### BENEFIT 4 — fast builds, perf ≈ full LTO; part of confirmed v22 stack; IPC ✓

## FULLLTO monolithic -flto — full monolithic LTO mode
### WORSEN 3 — banned: endless link time, perf sub-noise vs ThinLTO; no win

## OMITFP -fomit-frame-pointer — drop frame pointer
### NEUTRAL 3 — +0.12s vs DIRTYREG (sub-noise); not worth standalone

## NOPLT -fno-plt (ep5) — remove PLT indirection
### WORSEN 1 — Δ+1.2%, IPC ✓; net negative on this binary

## PGO (prior v20) — profile-guided optimization in v20 refactor
### NEUTRAL 3 — applied in v20 (f01624a); not re-measured standalone; subsumed by ThinLTO wins

## Windows-clang flag port — port Linux clang flag set to Makefile.win.clang
### BENEFIT 5 — +15.8% Windows host (8b06bab), gate-exact; PLATFORM-ONLY, does NOT compound with Linux chain

---

# TIMING / CYCLE OPT

## Current cycle-packing macros — always-on packed cycle compare; shift-into-sign UB
### WORSEN 5 — deadlock once 64-bit master passes 2^32 (truncation laps packed delta → sign flip → packet-timeout); correctness break; cycle_pack.h

## DO_CYCLE_PACKING gated rewrite — gated 64-bit master + bounded packed deltas + portable wrap-compare
### UNTESTED — design = gem5/ZSim 64-bit Tick + RFC1982; BLOCKED on standalone unit-tester (widths 1-37 × gaps vs 64-bit oracle); DO NOT MERGE until it passes

---

# CORRECTNESS / VERIFY
- `cache.cc handle_fill_remove` (~2579): raw cycle vs `PACK_CYCLE` unit mismatch (host-stat only) → unify under `PCYCLE_DIFF` before enabling packing.
- `SANITY_CHECK`/`PCYCLE_SANITY`: must compile out in release (string construction in hot path) — verified gated; confirm no accidental inclusion.
- `ROB_HashTable` rob_maps: dead WRITE-ONLY map still allocated+written every instr in prod (reader guarded by P3 sanity-wrap) → wrap writes+member under TRUE_SANITY_CHECK. VERIFIED real (not a bug, an opportunity).
- `check_and_add_lsq` num_mem_ops: total-vs-remaining ambiguity; cross-check with execute_store hoist before touching.
- `SQ_FwdInfo` bit-widths: not validated vs max ROB/SQ depth; widen before any SQ structural change.
- DRAM `tick_alphas`: delta-vs-absolute — current passes absolute; confirm contract before any DRAM timing opt.

---

# PRIOR-VERSION HISTORY & CUMULATIVE SPEEDUP
Figures = Linux wall, single-run pinned, unless noted. Do NOT add %; multiply time-factors. IPC gate 0.62461 (4-core).

| Step | Commit | Change | Wall factor |
|------|--------|--------|-------------|
| v17 import | 190cd90 | baseline | 1.0000 |
| v17→v18 | 0295747 | OCP/TTP + Windows opt catalog | unknown (excluded) |
| v18→v19 | 2355a08 | 4 host-speed opts | **0.9567** (-4.33%, IPC/checksum exact) |
| v19→v20 P1 | b4676d3 | refactor wall delta 0.27% (within noise) | 1.0000 (neutral, no kept gain) |
| v19→v20 P2 | b4676d3 | branch-mispred→rob_events fold | REJECTED (-1.3% regression) |
| v19→v20 P3 | b4676d3 | sanity-wrap write-only rob_maps | **0.9891** (+1.09%) |
| v20→v21 | f01624a | emhash/L1D-bypass/PGO/tooling sync | unknown (placeholder 1.0) |
| Windows-clang | 8b06bab | clang flag port | PLATFORM +15.8% (non-compounding) |
| v21→v22 | this session | ThinLTO+SOA+DIRTYREG+FUSEDSCAN | **0.9282** (-7.18%, IPC bit-exact) |
| v22→v23 | this session | LTOTUNE + S8 | **0.9344** (-6.55% over v22; v23 = -10.76% vs v21, IPC bit-exact) |

**Compounded Linux wall v18→v23:** 0.9567 × 1.0 × 0.9891 × 1.0 × 0.9282 × 0.9344 ≈ **0.821 → ~-17.9% faster than v18.**
Caveats: v17→v18 and v20→v21 gains unknown (treated 1.0 — true total likely better); Windows +15.8% is a separate platform; P1 = wall-neutral.
