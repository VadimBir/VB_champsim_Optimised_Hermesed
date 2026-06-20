# ChampSim_VB Version Evolution History

Reconstructed 2026-06-11 from recovered local artifacts (OPT_REPORT copy, session scrollback,
117-session index). Repo: `/home/cc/champsim_VB` (remote `ssh-remote+cc`, lost 2026-06-10).
All performance numbers are 16-core LLM256.Pythiapre30Phase15xpost20-21M_14M gate runs unless noted.

**The invariant across the entire campaign:** every accepted change must be **bit-exact** —
`FINAL ROI CORE AVG IPC: ;0.51803;` (16c) / `0.55145` (1c), all 16 per-core
`execution_checksum = 140039767200773`, and LPM ROI tables digit-identical to the
`champsim_v17_DRAM_P2/P2_stage2_sim.log` reference. Speed without bit-exactness = FAIL.

---

## Era 0 — pre-v17 lineage (inferred from session titles, ~May 11 – June 8)

- **~May 11–14:** style-fix forks (`4000fix` / `4001` model variants); `cp` of inc/+src/ into
  `_BG_fixed_S…` dirs. The `4000fix-KappaPhiL1L2` bypass model becomes the standard test MODEL.
- **v14 (~May 15–23):** the workhorse. `quick_v14.sh` batch runner built (parallel ChampSim
  batch runner with dump features, job-queue distribution across remote hosts). "Work with v14
  version only" directive (May 22). Space-complexity optimization passes with Opus agents.
- **v15 (~May 25):** `v15_DBG` promoted to v15 after backup. Debug era: MSHR return handling,
  L2→L1 data return, prefetch fill-level merge bugs.
- **The deadlock war (~May 23–29):** recurring theme — deadlock detection false positives,
  L1D/L2/DRAM-controller deadlocks, **cycle packing removed entirely to fix an invisible
  deadlock class** (May 28). This left the documented PCYCLE wrap-deadlock scar: `cycle_pack.h`
  disabled, and later (v17 era) the event_cycle u64→u32 probe was rejected for exactly this
  reason.
- **v16 (`champsim_v16_task2_dirty_flag`, ~June 1–8):** dirty-flag task line. Hermes
  integration/analysis happened alongside (separate `/home/cc/Hermes` tree).
- **v17 (early June):** consolidation into `champsim_v17_DRAM`. This becomes the optimization
  baseline: **315.541s** (user, uncontended) / **327.909s** (serialized same-host anchor).

---

## Era 1 — v17_DRAM Wave-1: standalone probes P1–P15 (June 9)

Method: duplicate the whole dir per idea (`champsim_v17_DRAM_P<n>`), implement ONE optimization,
gate bit-exact, time it. Opus agents implement; per-branch WORKLOG.md; honest skips allowed.

**Winners (PASS, standalone, contended times):**

| Opt | What | Time |
|---|---|---|
| P1 | LPM class-run accumulator (batch constant-class cycles), lpm_tracker.h | 311.458s — strongest base |
| P2 | DRAM sched num_unsched early-out + scan shadow arrays | 324.449s |
| P6 | check_queue reads fallback_value direct (proxy machinery proven dead) | 316.814s |
| P7/P7ext/P7c | emhash7 map+set swaps (rob_maps, address_to_lq, address_to_entries, pending_lqs…) | 314–318s |
| P8 | LPM_Tracker hot-field colocation (704→688 B, 9→6 cache lines) | 320.968s |
| P9D | cache.h `auto& re` ref hoist | 326.853s |
| P10 | **AddressProxy REMOVED** → plain uint64 (PACKET 256→192 B, QUEUE 672→144 B) | 314.382s |
| P14 | ooo_model_instr reorder 136→128 B (3→2 cache lines) | 289.647s |
| P15 | schedule dirty-check skip-guards (zero-superset OR → return) | 292.255s |

**Casualties / lessons:**
- P3 (get_way tagv shadow), P4 (LPM transpose), P5 (lazy fused_scan — 393s, suspect), P13
  (event_cycle mirror arrays) — gated PASS but slower; excluded.
- P12 — honest skip: premise false (one dep-array, not two).
- **Key discovery:** AddressProxy was dead machinery all along (vec_ptr nullptr project-wide,
  ctor value-copy bug never bound). Found via the honest FAIL of the original P6 vector-scan
  (IPC 0.44141 → abort → investigate → P10). Biggest single win of the wave.
- LPM accumulator (P1) was initially INERT due to an extra guard; fixing it made it the
  strongest standalone base.
- Profiling after P1: DRAM controller collapsed 23.79% → 4.04% of host CPU; O3 back-end
  (exec_mem / sched_mem / sched_inst) became the dominant ~37%.

**STACK proof:** winners merged cumulatively, full gate after EACH stage (stages 0–9:
P1→P2→P6→P7→P9D→P8→P10→P11→P7ext→P7c), 311.458s → 296.824s. Combination branches P1X
(P1+P10, RSS −5.8 MB) and P11 (P1X + O3 hoists/prefetch) folded in as stage 7.

**Serialized retime (idle host):** baseline 327.909s; STACK 293.054s (−10.63%).

---

## v18 (created 2026-06-10) — first official merged tree

- `champsim_v18_DRAM` = copy of `champsim_v17_DRAM_STACK` (stages 0–9), clean rebuild, gate PASS.
- Serialized: **290.264s (−11.48% vs baseline)**.
- Stages 10–11 merged same day: +P14 (290.574s) and +P15 (287.400s), both gate-PASS.
- Final v18 content = **11 opts**: P1, P2, P6, P7+P7ext+P7c, P8, P9D, P10, P11, P14, P15.
- Serial 16c anchor re-measured 06-10 09:07: **286.216s** — the Wave-2 baseline.

---

## Era 2 — Wave-2 on v18: P16–P28 (June 10, strictly sequential, idle host)

New rigor: all runs serial-foreground, idle host verified by pgrep before/after. New agent
fleet (table recovered): P16–P23 Opus implementers + FALCON-1/2 Fable explorers (perf-data
high-win mining + fewer-instr/fewer-branch code reading) which spawned P24–P28.

**Ranking vs v18 286.216s:**

| Rank | Opt | Δ | What |
|---|---|---|---|
| 1 | **P24** | **−12.145s (−4.24%)** | retire/commit_rob explicit `break` when head not ready (5 spin sites) |
| 2 | **P21** | **−3.986s (−1.39%)** | scan_and_schedule word-skip: `stall==0 && (ready[w]&rmask)==0` |
| 3 | P19B | −2.043s | TRUE_SANITY_CHECK guards around debug rob_maps blocks |
| 4 | P25 | −0.821s | Makefile: drop `-fno-omit-frame-pointer` |
| 5 | P28 | −0.717s | fetch_instruction PACKET template hoist (13/17 fields) |
| — | P16 | +0.30% | inline SmallLqSet — bit-exact but slower |
| — | P23 | +0.59% | DRAM next-event gate — slower (pre-Wave2 contended measurement) |
| — | P26 | +0.97% | lazy DTLB PACKET copy — slower |
| — | P17 | +2.41% | Murmur3 mixer + map reserve — slower |
| — | P27 | **+23.73%** | ctz-iterate PCYCLE_LE — bit-exact but massive REGRESSION |

**Failures:** P20 (persistent dirty flags) — deadlock at 2.206s, restored from .P20_BK.
P22 — rejected on proof: dirty-flag in fetch violates the has_work invariant.
P18/P19 — rejected pre-wave (premise false / reader dead).

Lesson of the wave: clever ≠ faster. Hash-quality (P17), small-set (P16), and sparse-iteration
(P27) ideas all gated bit-exact yet LOST time; the dumb early-breaks (P24) won by far.

---

## v19 (2026-06-10) — v18 + P24 + P21

- Per master directive only the top-2 merged (P19B/P25/P28 deliberately held back).
- Stage gates: +P24 → 271.962s; +P21 → 269.573s; both bit-exact.
- **Official idle 16c run: 272.586s** (`/tmp/v19_official_16c.log`) = **−4.76% vs v18,
  −13.6% vs the original 315.541s** (−16.9% vs same-host 327.909s anchor).
- P24 and P21 touch disjoint functions → additive stack, no conflict.
- Docs on remote: OPT_REPORT §10, v19 WORKLOG, vault note `v19-merged-optimizations.md`.

---

## Era 3 — Wave-3 on v19: parallel slices A/B/C/D + siblings (June 10, evening)

Four sibling agents on v19 base, each owning a slice of ideas (contended times, retime pending):

| Slice | Time | Applied | Parked/Deferred |
|---|---|---|---|
| W3-B | 269.656s* | B8 compact threshold 128→32; B1 lambda `[=, this]` → `__restrict__` scalar locals in fused_scan; B3 per-word dirty pre-skip `fc & ~sinf & ~scmp & rmask` | B2 linked-list (spec misses ROB-slot reuse invalidation) |
| W3-C | 270.277s | C4 tombstone-compact amortize; C5 RegDep direct-index array; C3A ADT vec front-cursor | C7 (bit-exact obligation) |
| W3-D | 271.432s | D2 lpm_operate(LPM_Tracker&) overload (kills per-call imul); D8 port of P28; D5 skip WQ.check_queue for instruction packets; D1 run_clsbyp u16 shadow fusing 5-way cmp chain | — |
| W3-A | 275.129s | A8 __builtin_expect on operate_lsq guards | A6/A3/A1 parked (need signature refactor / absorbed / multi-hour) |

(*two W3-B figures exist in the recovered report: 269.656s and 271.529s — different runs/contention; retime was to settle it.)

Plus sibling branches **P-BLOOM** and **P-FASTSET** (both editing cache.cc/cache.h) — contents
not in recovered material; results were pending the retime.

**New front opened — DRAM on GAP workloads:** GAP pr-10 (1c, IPC 0.05313) showed DRAM
controller = 20.3% of host CPU (vs ~4% on LLM); schedule() O(SIZE) HIT+MISS scans alone 15.13%.
Ranked plan for v20+: A bank-bucketed pending lists (5–10% absolute est.) → B open-row
fast-gate → C (min,2nd-min) cache → D check_dram_queue hash → E free-idx bitmap.
New gate addition: GAP reference IPC 0.05313 must match.

---

## v20 — IN FLIGHT at server loss (2026-06-10 ~12:51 server time)

The running agent (checkpointed, resumable via `V20_BUILD_STATUS.md`):
1. Idle-wait → **sequential retime** of v19 + W3-A/B/C/D + P-BLOOM (6 clean runs) for honest ranking.
2. Stack winners one-by-one into `champsim_v20_DRAM` (winner = faster than fresh v19 baseline,
   ±0.5s neutral-excluded), both gates after every add, rollback on regression.
3. Official idle v20 number → OPT_REPORT §11 + vault. ETA was ~2h.

Queued after v20:
- **DRAM slice implementer** (GAP plan A–E) off v20.
- **Cache split refactor** (`cc_perf_specs/REFACTOR_CACHE_SPLIT_PLAN.md` + Addendum 2):
  cache.cc → core (~lines 300–1700 true logic) + `cache_helper.cc` (getters/print/stat bloat)
  + `cache_macro.h`; same for `dram_helper.cc`; ooo_cpu.cc → `ooo_cpu_fetch/_decode/_schedule/
  _execute.cc` with core keeping handle_branch/add_rob/update_rob/retire. DESIGN ONLY until
  v20 lands; frozen-structure / inline-split / const-audit / gate rules apply.

---

## Scoreboard

| Tree | 16c time (idle/serial) | vs v17 315.541s | Opts |
|---|---|---|---|
| v17_DRAM | 327.909s (same-host anchor) | — | baseline |
| v17_STACK | 293.054s | −10.6%* | P1..P7c (9 stages) |
| **v18** | 286.216s (re-anchor) / 290.264s (first) | −11.5%* | 11 opts |
| **v19** | **272.586s** | **−13.6% (vs 315.541s)** | 13 opts (+P24, +P21) |
| W3 best (contended) | ~269.7s | ~−14.5% (unofficial) | v19 + slice B |
| v20 | unknown — agent died with the server | target: retimed W3+BLOOM winners | — |

*vs 327.909s same-host anchor: STACK −10.63%, v18 −11.48%.

Held-back proven winners still on the table for v20+: **P19B (−0.71%), P25 (−0.29%), P28 (−0.25%)**.
