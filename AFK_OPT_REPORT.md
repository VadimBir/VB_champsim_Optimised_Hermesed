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
- **(2026-06-22 update) BEST is now `champsim_v24`** = v23 + **PKTSLIM + RTEBRK** (same-session min-of-3: PKTSLIM -2.17%, RTEBRK -1.88%, PKTRTE stack **-3.96% vs v23**, IPC bit-exact 0.62461) → ≈ **-14.3% cumulative vs v21**. ⚠ the v24-vs-v21 same-session confirm was LOST in an OS-lock /tmp wipe; -14.3% is derived from the v23-relative confirm, not re-measured direct.
- **`champsim_v25`** = v24 + **cycle packing re-enabled, correctly** (gated `DO_CYCLE_PACKING`, narrowed `rob_events.event_cycle` uint64→uint32). IPC bit-exact 0.62461 ✓. **NOT a host-speed win — a REGRESSION, and form-sensitive:** hand-rolled inline serial macro ≈ **+1.4%**; vendored WebRTC `AheadOrAt` template **call ≈ +31.5%** (clean min-of-4: v24 67.978s → v25 89.410s, uniform 89-91s every run) — the per-compare template call does NOT inline on the every-cycle `update_rob` hot path. Correctness identical either way. **Verdict: keep v25 as parked correctness/safety milestone (4B deadlock fixed+proven); if kept, revert the compare to the force-inlined macro (panel-proven == lib in-window) to avoid the -31%; do NOT put packing on the perf path.**
- **`v24_OPT_CCPHOIST`** = v24 + hoist redundant `current_core_cycle[cpu]` reloads (update_rob/do_execution/fetch/add_rq/wq). Built, IPC bit-exact ✓. Clean min-of-4: 67.505s vs v24 67.978s = -0.70% BUT `all` runs overlap v24 → **sub-noise / NEUTRAL, not a confirmed win.**

## ☐ TODO / NEXT
1. Grade REGBIT/LQBIT (sweep tail); merge any B/E onto v23.
2. Opt-A **CoreHot grouping** — fold 5 padded per-core globals into `O3_CPU` front, drop single-thread-pointless alignas (~12-16→7-9 hot lines/cycle). Iteration-heavy.
3. Opt-B **gated `#ifdef DO_CYCLE_PACKING`** — ✅ RESOLVED 2026-06-22 in `champsim_v25`: correct UB-free serial compare via vendored WebRTC `AheadOrAt`, proven safe at infinite run length (gap-bound + wrap test past 2^33). See TIMING/CYCLE OPT entry. Remaining: not a speedup; non-ROB-head gap-bound enumerated-not-formally-proven.
4. **PACKET/AddressProxy shrink** — PKTSLIM done (-2.17%, in v24). PACKET/queue `event_cycle` NOT yet narrowed (left wide in v25, layout-sensitive); mem profile still says ~67% load traffic = stack PACKET/instr temporaries → top remaining data-movement lever (UNTESTED).
5. ✅ DONE 2026-06-22: re-ran cy + mem + branch-miss/cache-miss perf profiles on-model `-o /tmp` (`/tmp/claude-afk-perf/{cy,mem,br}.data.zst`). Original 16-core perf.data still LOST.
6. Pick remaining UNTESTED catalog items by profile.

⚠ INCIDENT: original repo `perf.data` (276MB, your 16-core capture) overwritten to 11.3M corrupt partial by epoch-4 reprofile's compressed-capture bug; not in git, no backup → LOST. All perf now `-o /tmp` only.

## ▼ v23 UNTESTED-BATCH SWEEP (2026-06-22) — all 12 graded vs champsim_v23 (control 70.179s, single-run ~±1% noise, IPC gate 0.62461)
Dirs deleted after grading (no-garbage rule); grades retained here so none is retried.
| candidate | min | Δ vs v23 | IPC | GRADE |
|-----------|-----|----------|-----|-------|
| RTEBRK — execute_instruction head-not-ready break | 69.318 | **-1.23%** | ✓ | BENEFIT 3 (CONFIRMING min-of-3) |
| PKTSLIM — AddressProxy 32→24B (drop write-only tmp mirror) | 69.398 | **-1.11%** | ✓ | BENEFIT 3 (CONFIRMING min-of-3) |
| COREHOT — co-locate 5 per-core scalars, drop alignas | 70.099 | -0.11% | ✓ | NEUTRAL 3 (layout fold sub-noise here) |
| BRGUARD — handle_branch dead-guard + loop fuse | 70.496 | +0.45% | ✓ | NEUTRAL 3 |
| SKIPNONREG — skip remove_producer for non-reg writers | 70.621 | +0.63% | ✓ | NEUTRAL 3 |
| BANKSPLIT — hoist open_row to flat array | 70.656 | +0.68% | ✓ | NEUTRAL 3 |
| LQBIT — LQ pending HashSet→bitset (on v23) | 70.798 | +0.88% | ✓ | WORSEN 1 |
| STHOIST — execute_store hoist ccp | 70.817 | +0.91% | ✓ | WORSEN 1 |
| FIBHASH — Fibonacci hash LLC MSHR | 70.830 | +0.93% | ✓ | WORSEN 1 |
| FILLBYTE — fill_cache 4-bitfield→1-byte store | 71.257 | +1.54% | ✓ | WORSEN 1 |
| REGBIT — AddrDep dirty_regs→256-bit bitset (on v23) | 72.760 | +3.68% | ✓ | WORSEN 2 |
| BACKCURSOR — get_latest_producer back-cursor | 7.077 | n/a | **✗ no ROI** | **WORSEN 5 — CRASHED (sim aborted ~7s); back-cursor logic broke the run; debug-only** |
| LSQBITMASK — check_and_add_lsq bitmask | — | — | — | N/A — not built: num_mem_ops total-vs-remaining ambiguity = IPC-unsafe |
LESSON reinforced: structural/ADT/codegen micro-changes mostly N or W at these queue/ROB sizes; the only gains are RTEBRK (control-flow early-exit) + PKTSLIM (struct shrink) — both marginal, confirming. REGBIT/LQBIT bitsets SLOWER on v23 (opposite of S8 RobDepSet — bitset only wins where the set is hot+large).

## ▼ v24 ON-MODEL PERF RE-PROFILE (2026-06-22) — replaces the OS-lock-lost profiles; all IPC bit-exact 0.62461, repo perf.data untouched, output `-o /tmp`
- **cy** (`/tmp/claude-afk-perf/cy.data.zst`): self-time top — update_rob **10.5%** (now #1, was complete_execution pre-PKTSLIM/RTEBRK), schedule_memory_instruction λ 10.0%, schedule_instruction λ 8.4%, CACHE::operate 8.0%, handle_read 6.7%, reg_dependency 4.2%, add_rq 3.6%, check_queue 2.8%, OffchipPredTTP::predict 1.6%.
- **mem** (`/tmp/claude-afk-perf/mem.data.zst`): 67% load traffic = stack PACKET/ooo_model_instr temporaries; 22% = CACHE RQ/WQ/MSHR queue arrays (~589KB heap); __memmove_avx 3.95% (add_to_rob self-copy). Loads: 49% LFB/MAB, 28% L3, 14% L1.
- **br** (`/tmp/claude-afk-perf/br.data.zst`): branch-misses top = update_rob **15.9%**, then CACHE::operate/handle_read, AddrDependencyTracker::remove_producer 6.2%. cache-misses top = add_to_rob 13.9%, handle_branch 13.7%, handle_read 8.6%.

## ▼ v24 OPPORTUNITY CATALOG (2026-06-22) — UNTESTED proposals from 2 sweeps (static-source `wbskfemjy` + perf-mine `whre2jrw9`); all checked vs DO-NOT-RETRY ledger; full JSON in task outputs. None measured except CCPHOIST (see CURRENT STATE).
| ID | file:function | what | source | state |
|----|---------------|------|--------|-------|
| CCPHOIST (=UPDROB-CCPHOIST/CYCHOIST + wider) | ooo_cpu update_rob/do_execution/fetch + cache add_rq/wq | hoist redundant `current_core_cycle[cpu]` reloads on #1 hotspot | both | **BUILT v24_OPT_CCPHOIST, IPC ✓, clean timing pending** |
| ADDTOROB-SELFCOPY-GUARD | ooo_cpu add_to_rob:533 | guard the 3.95% `__memmove` self-copy (`if(arch_instr!=&entry)`) | perf-mem | UNTESTED (Tier-1) |
| BRANCH-MISPRED-BITSET | flat_branch_mispredicted | uint8[NUM_CPUS][ROB_SIZE] → bitset (128B); update_rob top branch-miss | static | UNTESTED (Tier-1, only H-rated) |
| TTPLAMBDA-REF | hermes lookup_catalog_cache:147 | find_if lambda by-value→const-ref, kills per-elem pair copy | perf-cy | UNTESTED (Tier-1) |
| CACHE-SETONCE | cache handle_read/wb/pf + check_hit | pass already-computed `set`, drop repeat get_set() | static | UNTESTED (Tier-1) |
| GLPWINDOW-HOIST | ooo_cpu get_latest_producer:108 | hoist wrap-selector; remove_producer 6.2% branch-miss | perf-br | UNTESTED (Tier-1) |
| REGDEPHOIST | ooo_cpu reg_dependency:827 | hoist ROB.entry ref, kill ~12 index recomputes/instr | static | UNTESTED (Tier-1) |
| DOEXECCSE | ooo_cpu do_execution:934 | CSE event_cycle + PACK_CYCLE double-read | static | UNTESTED |
| MISPREDINLINE | ooo_cpu complete_execution:1623 | move mispredict flag onto hot rob_events line | static | UNTESTED |
| PRODLISTSPLIT / ADDPRODUNROLL / CAALSQHOIST | ooo_cpu AddrDepTracker / check_and_add_lsq | ProducerList reorder; unroll fixed-2 loop; ref-hoist | static | UNTESTED (L) |
| CACHE-LOWQHOIST / CACHE-PROXYRAW / CACHE-PQ-COLDFIELDS / CACHE-OPERATE-LPMHOIST / CACHE-UFC-INCRMIN | cache.cc | compute lower occ/size once; raw addr-array scan; PQ hot-field-front; hoist LPM hit_active; O(1) fill-cycle min | static | UNTESTED |
| DRAMSOA / UPDMIN-EARLYOUT / SCHEDADDR-SAVE / BANK-WORKING-BITMAP / BANKREQ-PACK / PKTQ-HOTFIELD-FRONT / ADDRPROXY-FLAT-DRAM | dram_controller + block.h | raw addr-array DRAM scans; occupancy==0 early-out; bank bitmap; struct packs | static | UNTESTED (some ⚠ adjacent BANKSPLIT neutral) |
| MEMDEP-SVEC / MEMDEP-FLATARR / REGDEP-BOOL-ARRAY / ADDR-DEP-COMPACT-THRESH | ooo_cpu trackers | vector→svector; map→flat array; bool[]→bitset; raise compact threshold | static | UNTESTED |
| BUILTIN-EXPECT-RETIRE / -COMPLETE-EXEC / POLLY-RESTRICT / CLANG-FNO-EXCEPT | codegen | likely/unlikely hints; de-alias stack copies; `-fno-exceptions` (0 try/catch) | static | UNTESTED |
| ADDQ-RAWSLOT / FETCH-ENTRYREF / SMEMSCHED-RANGEBUILD / ADDRQ-CYCLOCAL / ADDRQ-WQSKIP-EXPECT / ADDQ-HOLESKIP-BRANCHLESS / CHECKQ-WQ-DISPATCH-LIFT | cache/ooo hot loops | raw-slot scans, ref/cycle hoists, branchless hole-skip, de-lambda check_queue | perf | UNTESTED |
| HBRANCH-DESTLOCAL / HBRANCH-MEMOP-GUARD-DROP | ooo_cpu handle_branch:456/463 | local dest test; drop dead num_mem_ops guard | perf | ⚠ HOLD — overlap tried-neutral BRGUARD |
| SMEMSCHED-RANGEBUILD | ooo_cpu schedule_memory_instr:1011 | lazy ready[] build | perf | ⚠ HOLD — abuts the 3×-failed cr-mask loop |

---
# MEMORY OPT

## RobDepSet bitset dense-key ADT swap                      — Replace `svector<uint64_t>`-based `RobDepSet` with `ROB_SIZE`-bit flat bitset; eliminates heap alloc for dependency tracking.
### BENEFIT 4 — min 73.276s, Δ−2.40%, IPC ✓; S8 confirmed same-session vs v21

## MSHR free-slot stack free-list                           — Replace linear MSHR slot scan with stack-based free-list; O(1) slot acquire vs O(N) walk.
### WORSEN 5 — Δ+0.18% wall BUT IPC ✗ 0.62554 (broke vs gate 0.62461); S1 slot-index alters sim; retry only if slot-order-independent

## LLC MSHR tombstone erase direct-map                      — Replace `LlcMshrDirectMap` tombstone erase with Fibonacci-hash direct eviction; removes reinsert overhead.
### WORSEN 2 — Δ+2.8% wall, IPC ✓; S9 reinsert occupancy cost outweighs gain at MSHR≤128

## Per-queue WQ/RQ/PQ bloom filters                         — Add per-queue bloom filters to short-circuit dedup address scan; O(1) probe vs O(N) scan.
### WORSEN 4 — Δ+16% wall, IPC ✓; S10 bloom overhead dominates for queue depth ≤64

## SOASCAN dense SoA-address queue scan                     — Replace PACKET-array `check_queue` with a parallel SoA `uint64` address-array scan (ep5).
### WORSEN 5 — Δ+21% wall AND IPC ✗ 0.50330 (BROKE: SoA mirror desynced sim results); reject outright

## svector RobDepSet heap-spill ADT                         — Replace `RobDepSet` with `svector<uint64_t, SMALL_VECTOR_SIZE>` to cut heap alloc on small dep sets.
### NEUTRAL 3 — sub-noise Δ, IPC ✓; SVEC/ROBDEP superseded by S8 bitset; no retry value

## DRAM seen-count early-break loops                        — Insert seen-count sentinel to break DRAM update/fill/check/add_rq loops early; cuts scan iterations.
### NEUTRAL 3 — Δ−0.21..−0.50% each (S2/S3/S4/S5), IPC ✓; sub-noise individually; bundled into FUSEDSCAN win

## DRAM 2-scan fusion single-pass                           — Merge two separate DRAM 64-entry schedule scans into one combined pass; halves iteration count.
### BENEFIT 2 — Δ−0.8% net confirmed, IPC ✓; FUSEDSCAN epoch-6 confirmed; stacks into v22 7.18%

## AddressProxy slim PACKET shrink                          — Replace `AddressProxy` pointer+index struct with raw `uint64_t` address in PACKET; cuts per-packet size ~22%, reduces queue array traffic.
### UNTESTED HIGH — mem profile shows 67% load traffic from PACKET stack; top data-movement lever; no IPC result yet

## PACKET QUEUE svector small-size trim                     — Replace `PACKET_QUEUE` backing store with `svector<SMALL_VECTOR_SIZE>` tuned to avoid heap spill for typical occupancy.
### UNTESTED HIGH — PACKET shrink prerequisite; eliminates heap alloc on queue push for common depths

## AddrDependencyTracker regs→256-bit bitset                — Replace per-register `HashSet` in `AddrDependencyTracker` with 256-bit flat bitset for register dependency lookup.
### PENDING — REGBIT; no wall-time measurement yet; dense-key pattern proven by S8

## LQ PendingLoads HashSet→LQ_SIZE bitset                   — Replace `HashSet`-based load-queue pending-set with `LQ_SIZE`-bit flat bitset; O(1) test/set vs hash probe.
### PENDING — LQBIT; no wall-time measurement yet; follows S8 pattern

## RegDepReleaseTracker flat array replace                  — Proposed hashmap→flat-array swap for reg-dep release tracker.
### N/A — NOT implemented: tracker is ALREADY a direct-indexed flat `deps[ROB_SIZE]` (no hashmap to replace); nothing to do

## Dirty-regs-only compact restrict+hoist                   — Restrict `AddrDependencyTracker::compact()` to only visit dirty registers via `dirty_regs[]` array + `dirty_count`; skip 256-slot sweep.
### BENEFIT 5 — Δ−3.37% vs 2-opt baseline, IPC ✓; DIRTYREG epoch-3 confirmed; single lever, largest memory-access win

---

# CACHE LINES OPT

## SOA packed-tag way-scan cache array                      — Split cache tag array into contiguous packed `uint64_t tags[CAP]` SoA layout for hot way-scan; improves spatial locality.
### NEUTRAL 3 — sub-noise alone, IPC ✓; SOA ep2 combo with LTO gave −1.37%; stacks into v22 7.18% total

## CoreHot hot-field grouping O3_CPU fold                   — Fold 5 scattered per-core scalars (`current_cycle`, `stall_cycle`, `rob_memory_count`, etc.) into `alignas(64)` hot struct in `O3_CPU`; reduces 12–16 → 7–9 hot lines per cycle.
### UNTESTED HIGH — Opt-A per plan; co-access map confirms scatter; mem profile needed on-model; no wall number yet

## fill_cache 4-bitfield RMW→single byte                    — Replace 4-field bitfield read-modify-write in `fill_cache` with single `uint8_t` byte store; eliminates partial-word RMW.
### UNTESTED MEDIUM — catalog item; no wall number; straightforward 1-line change

## bank_request hot/cold field split                        — Split `bank_request` struct: move hot fields (tag, row, bank) to first cache line, cold fields (metadata) to second; reduces false sharing on hot path.
### UNTESTED MEDIUM — catalog item; no wall number; profiler context needed

## LpmShadow alignas(64) hot-line reduction                 — `LpmShadow` already `alignas(64)`; validate field ordering keeps hot tick-path fields in line 0 and cold fields in line 1.
### UNTESTED MEDIUM — catalog follow-on to CoreHot; LPM Tracker tick = 2.1% runtime; needs layout audit
# ALGO OPT

## RobDepSet→ROB-bit Bitset                                 — replace HashMap dep-set with dense ROB-indexed bitset
### BENEFIT 4 — min 73.276s, Δ-2.40%, IPC ✓, single S-batch win confirmed same-session

## DIRTYREG Compact Dirty-Regs-Only                         — remove producer, hoist, restrict to dirty regs only
### BENEFIT 5 — Δ-3.37% vs 2-opt, IPC ✓, single confirmed source; biggest solo algo lever

## FUSEDSCAN DRAM 2-Scans→1-Pass                            — fuse two 64-entry DRAM schedule scans into one pass
### BENEFIT 2 — Δ-0.80% net confirmed, IPC ✓, scan count halved on hot DRAM loop

## S4 check_dram_queue Seen-Count Break                     — early-exit once seen_count hits occupied in DRAM queue scan
### NEUTRAL 5 — Δ-0.50% wall, IPC ✓, fewer scan iters; n≤128 keeps gain latent under fusion

## S2 update_fill_cycle Seen-Count Break                    — guard MSHR fill loop with seen-count early-exit
### NEUTRAL 3 — Δ-0.42% base loop, IPC ✓, improvement sits at noise floor alone

## S3 DRAM min-scan Seen-Count Break                        — break schedule_dram loop early via seen-count counter
### NEUTRAL 5 — Δ-0.30% wall, IPC ✓, fewer scan iters; latent benefit confirmed

## S5 DRAM add_rq/wq Occupied-Mask ctz                      — replace linear slot-find with occupied-mask + ctz O(1)
### NEUTRAL 4 — Δ-0.21% wall, IPC ✓, O(n)→O(1) slot find; n small keeps gain sub-noise

## LAZYREADY ep1 Schedule Mem-Ready Word                    — precompute mem-ready bitmask lazy in schedule_mem
### NEUTRAL 3 — artifact: measured vs noisy avg-of-2 baseline; superseded by min-of-N metric

## S6 schedule_instruction cr-mask Gate                     — gate complete-instr loop by fetched-bitmask cr-mask
### WORSEN 4 — Δ+13.7%, IPC ✓, tight scalar PCYCLE_LE loop resists branching; 3 attempts failed

## FCGATE ep7 cr-mask Gating                                — cr-mask gate complete_execution lambda per schedule pass
### WORSEN 4 — Δ+12.3%, IPC ✓, same root cause as S6; scalar loop penalizes branch

## EARLYBRK ep7 cr-mask Short-Circuit                       — short-circuit complete_execution loop on cr-mask zero
### WORSEN 5 — Δ+34.6%, IPC ✓, worst regressor; loop structure prevents early-break benefit

## EARLYBRK ep6 DRAM Count-Based Early-Break                — break DRAM operate loop on count threshold
### WORSEN 1 — Δ+1.2% slower, IPC ✓, count tracking overhead exceeds scan savings

## EARLYBRK ep5 schedule_instruction Short-Circuit          — early-exit cycle-ready scan on first not-ready entry
### WORSEN 4 — Δ+20% wall, IPC ✓, ep5; cr-mask-family regression (scalar loop resists early-break)

## S7 retire_rob Template Zero-Store-Gate Fuse              — fuse zero-store gate into retire_rob template
### WORSEN 1 — Δ+1.6%, IPC ✓, restructure regressed; bundled approach backfired

## NOINLINE-COMPACT ep6 Out-line compact_complete_execution — move compact path out-of-line
### WORSEN 1 — Δ+1.x% slower, IPC ✓, out-lining hot path hurt icache behavior

## S1 add_mshr Free-List Stack                              — replace linear free-slot scan with LIFO stack free-list
### WORSEN 5 — Δ-0.18% wall BUT IPC 0.62554 ✗, MSHR slot order affects sim; BROKE correctness

## S9 LLC MSHR Tombstone Erase                              — erase-on-evict instead of scan-skip in LLC MSHR
### WORSEN 2 — Δ+2.8%, IPC ✓, reinsert bookkeeping cost exceeds scan savings at MSHR≤128

## S10 WQ/RQ/PQ Per-Queue Bloom Filters                     — bloom-filter dedup check before queue scan
### WORSEN 4 — Δ+16%, IPC ✓, bloom insert/probe cost exceeds scan savings at queue≤64

## SVEC RobDepSet Small-Vector                              — replace RobDepSet HashMap with small-vector
### NEUTRAL 3 — sub-noise, IPC ✓, heap overhead remains; superseded by S8 bitset (-2.4%)

## REGDEPARR RegDepReleaseTracker Flat Array                — proposed HashMap→flat-array swap
### N/A — NOT implemented: tracker is ALREADY a direct-indexed flat deps[ROB_SIZE]; no hashmap to replace; nothing to do

## REGBIT AddrDependencyTracker 256-bit Bitset              — replace reg-dep HashSet with 256-bit bitset
### NEUTRAL 3 — PENDING/ungraded; dense-key bitset built (v22_OPT_REGBIT), awaiting profile-guided sim

## LQBIT LQ PendingLoads HashSet→Bitset                     — replace LQ_SIZE HashSet with flat bitset
### NEUTRAL 3 — PENDING/ungraded; built (v22_OPT_LQBIT), awaiting confirmed sim run

---

## UNTESTED                                                 — Back-Cursor Latest-Producer — ooo_cpu get_latest_producer reverse-scan cursor
### NEUTRAL 3 — HIGH priority per catalog; replaces O(n) forward scan with back-cursor O(1) amortized; ungraded

## UNTESTED                                                 — RTE Break-On-Head-Not-Ready — execute_instruction break when ROB head not ready
### NEUTRAL 3 — HIGH priority; skip tail scan once head stalls; ungraded

## UNTESTED                                                 — complete_execution Skip-Non-Reg-Writers — skip non-reg-writing instr in complete loop
### NEUTRAL 3 — MEDIUM priority; predicate filter reduces work per cycle; ungraded

## UNTESTED                                                 — check_and_add_lsq Bitmask Semantics — replace num_mem_ops counter with bitmask
### NEUTRAL 3 — MEDIUM priority; O(n) counter → O(1) bitmask; ungraded

## UNTESTED                                                 — Fibonacci Hash LLC MSHR — replace modulo with Fibonacci multiplicative hash
### NEUTRAL 3 — MEDIUM priority per catalog; reduces hash collision clustering; ungraded

## UNTESTED                                                 — Cross-Cache MSHR Bloom Filter — bloom gate before MSHR linear scan across caches
### NEUTRAL 3 — MEDIUM priority; amortizes scan at larger MSHR; ungraded

## UNTESTED                                                 — fill_cache 4-Bitfield RMW→1-Byte Store — collapse 4-bit RMW into single byte write
### NEUTRAL 3 — MEDIUM priority; eliminates read-modify-write on dirty/valid flags; ungraded

## UNTESTED                                                 — Bank-Request bank_hot Hot-Field Split — split hot bank-request fields to own cache line
### NEUTRAL 3 — MEDIUM priority; reduce false-sharing on hot DRAM bank struct; ungraded

## UNTESTED                                                 — handle_branch Dead guard + Loop Fuse — add num_mem_ops>0 guard, fuse branch update loop
### NEUTRAL 3 — catalog entry; prune empty-mem-ops path; ungraded

## UNTESTED                                                 — execute_store Hoist CCP Drop — hoist store completion, drop redundant CCP copy
### NEUTRAL 3 — catalog entry; reduces per-store work in execute path; ungraded
# BRANCH OPT

## HOIST                                                    — hoist complete_execution cycle assignment out loop
### NEUTRAL 3 — sub-noise; artifact vs noisy avg-of-2 baseline, superseded by min-of-N metric

## handle_branch dead guard removal
### NEUTRAL 3 — UNTESTED; guard `num_mem_ops > 0` + loop fuse; no measured result yet

## DIRTYREG                                                 — compact dirty-regs; restrict + hoist producer
### BENEFIT 5 — 3.37% wall; confirmed single-source lever vs 2-opt baseline; IPC bit-exact ✓

## FUSEDSCAN                                                — fuse DRAM 2 schedule-scans into 1 pass
### BENEFIT 2 — 0.8% net wall; confirmed vs 2-opt; IPC bit-exact ✓

## S6/FCGATE/EARLYBRK                                       — cr-mask gate + early-break schedule_instruction
### WORSEN 5 — +13.7%/+12.3%/+34.6%; tight scalar PCYCLE_LE loop resists branching; 3 independent fails

## S7                                                       — retire_rob template zero-store-gate fuse
### WORSEN 1 — +1.6% wall; restructure bundled overhead regressed; IPC ✗

## S8                                                       — RobDepSet dense→ROB-bit bitset
### BENEFIT 4 — 2.40% wall; S-batch win; IPC bit-exact ✓

## S2                                                       — update_fill_cycle seen-count break
### NEUTRAL 3 — 0.42% wall; base-loop guard; sub-noise; IPC ✓

## S3                                                       — dram update_schedule process_cycle break
### NEUTRAL 5 — 0.30% wall; fewer scan iters; latent; IPC ✓

## S4                                                       — check_dram_queue seen-count break
### NEUTRAL 5 — 0.50% wall; fewer iters; IPC ✓

---

# PREEMPT OPT

## HOTFN-PREFETCH                                           — SW-prefetch next ROB entry in hot fn (ep2)
### WORSEN 1 — +1.97% wall; HW prefetcher covers; 3 prefetch attempts all failed; IPC ✓

## ROBPREFETCH                                              — SW-prefetch next ROB entry in update_rob (ep5)
### WORSEN 1 — +1.2% wall; HW covers; prefetch penalty exceeds benefit; IPC ✓

---

# CODEGEN / BUILD OPT

## LTOTUNE                                                  — ThinLTO import/inline budgets + mtune=native
### BENEFIT 5 — 4.10% wall; hot cross-TU edges inlined; IPC bit-exact ✓

## ThinLTO                                                  — base -flto=thin mode
### BENEFIT 4 — best fast-build perf; part of confirmed v22 stack (+7.18% cumul); IPC ✓

## FULLLTO                                                  — monolithic -flto full mode
### WORSEN 3 — build banned; sub-noise perf gain; endless link time; no win vs ThinLTO

## OMITFP                                                   — -fomit-frame-pointer flag
### NEUTRAL 3 — +0.12s wall vs DIRTYREG baseline; sub-noise; not worth standalone

## NOPLT                                                    — -fno-plt flag (ep5)
### WORSEN 1 — +1.2% wall; PLT removal overhead net negative on this binary

## PGO (prior v20)                                          — profile-guided optimization headers, v20 campaign
### NEUTRAL 3 — applied in v20 refactor (f01624a); not re-measured as standalone; subsumed by ThinLTO wins

## Windows-clang flag port                                  — full Linux clang flags → Makefile.win.clang
### BENEFIT 5 — 15.8% host speedup on Windows platform; gate-exact (8b06bab); IPC ✓
# TIMING / CYCLE OPT

## Current Cycle-Packing Macros                             — Always-on; shift-UB causes deadlock past 2^32
### WORSEN 5 — 64-bit master overflows into packed delta sign-bit; lock-step deadlock confirmed; correctness break; macros in `cycle_pack.h` gated off for investigation

## DO_CYCLE_PACKING (Gated)                                 — 64-bit master + bounded packed deltas; no shift-UB
### NEUTRAL 3 — UNTESTED investigation; design referenced gem5/ZSim 64-bit Tick; pending standalone unit-tester proof of width safety (1–37 cycle gaps vs 64-bit oracle); RFC1982 wrap-compare planned; DO NOT MERGE until unit-tester passes
### ✅ RESOLVED 2026-06-22 (champsim_v25) — NEUTRAL 5 (correct + safe, no host-speed gain)
- **Root cause of old 4B deadlock**: old `PCYCLE_LE(a,b)=(int32_t)((a-b)<<(32-W))<=0` — (1) left-shift-into-sign signed-overflow UB pre-C++20; (2) inverts once true gap ≥ 2^(W-1); (3) 32-bit container truncation of the 64-bit master mixed operands once cycles crossed 2^32. PACK_BITS=27 (the `_clog2` `r+17` typo that gave 36 also FIXED → `r+1`).
- **Fix shipped in v25**: gated `#define DO_CYCLE_PACKING 1`; operands masked cleanly to 27 bits; compare delegated to **vendored WebRTC `rtc_base/numerics/sequence_number_util.h` `AheadOrAt<uint32_t,2^27>`** (BSD-3, byte-verified verbatim, `champsim_v25/inc/webrtc_seqnum.h`) — no shift, no UB, defined midpoint. `#else` = 64-bit pass-through fallback.
- **Proof (3 legs)**: (1) compare-correct — 4-Opus adversarial panel SHIP, all severity none; 8.59B in-window compares 0 divergences. (2) applicable — every ~45 packed-compare site bounded; max live gap = 20M (deadlock detector) < half-window 2^26=67.1M, 3.35× margin, no unbounded site; standalone wrap test swept master past 2^33 (~64 wraps), 70.5M compares, 0 in-window mismatches. (3) results-identical — sim IPC bit-exact 0.62461.
- **Why WORSEN not BENEFIT (clean min-of-4, 2026-06-22)**: only `rob_events.event_cycle` narrowed (uint64→uint32). Hand-rolled inline serial macro ≈+1.4% slower (compare overhead > 1-array footprint saving). **Vendored WebRTC `AheadOrAt` template call ≈+31.5%** (v24 67.978s → v25 89.410s, uniform) — the function-template call does NOT inline on the every-cycle `update_rob` path. → WORSEN 2 as shipped (vendored); WORSEN 1 if reverted to inline macro. PACKET/queue `event_cycle` left wide = remaining footprint lever. Correct + safe, but not a speedup in any form.
- **Residual risk**: non-ROB-head operand gap-bounds (DRAM/MSHR/queue) enumerated + argued, NOT machine-proven for all sites; no counterexample found. Ultimate empirical check available: `-i 2.6e9` forces a real >2^32-cycle run (trace loops on EOF), ~124× a normal sim — NOT run.

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
| v22 → v23 | this session | LTOTUNE (−4.10%) + S8 bitset (−2.40%) | **0.9345** CONFIRMED — v23 = −10.76% vs v21 same-session (80.370→71.726), IPC bit-exact |

## Compounded Linux Wall-Time Speedup (v18 → v23)

```
Factor product = 0.9567 × 1.0000 (P1 neutral) × 0.9891 (P3) × 1.0000 (v20→v21 unknown) × 0.9282 (v22) × 0.9345 (v23)
               = 0.8783 (v18→v22) × 0.9345 = 0.8208
```

**Total Linux wall-time reduction v18 → v23: ~17.9% faster** (time factor 0.821). [v18→v22 = ~12.2%; v23 adds confirmed −6.55% over v22.]

Caveats:
1. v17→v18 gain unknown — not in ledger; excluded from product.
2. v20→v21 (f01624a) gain unknown — no measured wall % in commits; treated as 1.0 (PLACEHOLDER — could be positive or zero).
3. Windows +15.8% is a separate platform figure; it does NOT multiply with Linux chain.
4. v22→v23 CONFIRMED this session (−6.55% over v22; v23 = −10.76% vs v21, IPC bit-exact).
5. P1 "validated" = wall-neutral (0.27% within noise); contributes 0% to compounded product.
