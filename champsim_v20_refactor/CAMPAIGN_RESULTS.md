# Overnight Performance Campaign — ChampSim

**Started (autonomous, user AFK).** All wall numbers are ACTUAL measured `date +%s` deltas. Checksum gate = `140038115682937` for every run.

## Header / pre-measured facts
- `max_reg_id = 138`
- `max_pl_depth = 159`
- Trace: `traces/LLM256.Pythia-70M_21M.champsimtrace.xz`
- Run params: Cores 8, Warmup 2000000, Sim 14000000, Bypass `4000fix-KappaPhiL1L2`, PfL2 spp, Affinity F0
- Build: clang/libc++ via `quickSim/win_qbuild.sh --dir <DIR> no spp no --cores 8 --hermes ttp --repl lru --bp perceptron --arch glc`
- Non-refactored dir: `champsim_v19_avx2_lpm`; Refactored dir: `champsim_refactor`
- NOTE: `act` pre-action logging command from CLAUDE.md is NOT installed in this Windows environment (`act: command not found`; it referenced a Linux path `/home/cc/...`). No hook fired. Proceeding with the actual campaign work; documenting this honestly.

---

## STEP 0 — clean slate
- `git checkout -- champsim_refactor/{inc/ooo_cpu.h,src/ooo_cpu.cc,src/main.cc}` -> OK, tree clean for those files.

---

## ENV NOTE — sim-kill resolved (2026-06-21)
Earlier in the session, sims were being killed at ~30-40s producing empty checksums. ROOT CAUSE = OOM from
stalled agents running OVERLAPPING parallel sims (an agent backgrounded-and-waited, orphaning sims that piled up).
After killing all orphans: clean env (10+ GB free, 0 leftover procs, no watchdog). A single clean foreground run
completed in **208.2s**, checksum **140038115682937** exact, IPC **0.62589** — environment fully healthy.
All campaign timing thereafter uses ONE background driver running sims STRICTLY SERIAL (no parallelism, no OOM).

---

## STEP P1 — refactored vs non-refactored (clang/libc++, 5x interleaved, strictly serial)
Driver: `%TEMP%/p1_timing_driver.ps1` — alternated `champsim_refactor` and `champsim_v19_avx2_lpm`, 5 iters each,
1 sim at a time. Every run gated on checksum + FINAL ROI CORE AVG IPC.

| iter | refactor wall (s) | nonrefactor wall (s) |
|------|-------------------|----------------------|
| 1    | 208.9             | 209.4                |
| 2    | 210.0             | 210.6                |
| 3    | 209.4             | 209.1                |
| 4    | 209.3             | 210.9                |
| 5    | 209.4             | 209.8                |
| **mean**   | **209.40**  | **209.96**           |
| median     | 209.4       | 209.8                |

- **Checksum: 140038115682937 EXACT on ALL 10 runs** (both dirs, all 8 cores).
- **FINAL ROI CORE AVG IPC: 0.62589 EXACT on ALL 10 runs** (both dirs).
- Wall delta: refactor 0.56s (0.27%) FASTER — well within ~4% wall noise ⇒ statistically EQUAL.
- **VERDICT: the 6-stage `#include`-fragment refactor is architecturally IDENTICAL (checksum+IPC bit-exact)
  AND host-speed-neutral (no regression).** Refactor confirmed safe to keep as the working dir.

---

## STEP P2 — fold flat_branch_mispredicted into rob_events SoA bitset — REJECTED (measured regression)
Change: added `branch_mispredicted[ROB_WORDS]` to `rob_events_cpu` (instr_event.h) + `&= mask` in clear_entry;
deleted the standalone `uint8_t flat_branch_mispredicted.branch_mispredicted[NUM_CPUS][ROB_SIZE]` (4KB) array;
4 call sites → BS_SET (x2) / BS_TST / removed the redundant retire-clear (clear_entry covers it).
Built clang (bin 10419712, 512B smaller than prefold 10420224). A/B = fold-bin vs prefold-bin, 5x interleaved, serial.

| iter | fold wall (s) | prefold wall (s) | paired Δ |
|------|---------------|------------------|----------|
| 1    | 211.1         | 210.1            | prefold −1.0 |
| 2    | 210.3         | 204.0            | prefold −6.3 |
| 3    | 205.7         | 203.8            | prefold −1.9 |
| 4    | 206.2         | 204.2            | prefold −2.0 |
| 5    | 205.6         | 203.9            | prefold −1.7 |
| **mean**       | **207.78** | **205.20**  | **prefold +1.26% faster** |
| drop-warmup (2-5) | 206.95 | 203.98          | prefold +1.46% faster |

- **Checksum 140038115682937 + IPC 0.62589 EXACT on all 10 runs** (architecturally neutral — the fold is correct).
- **prefold faster 5/5 paired.** P1 (same driver shape) had the FIRST slot win 4/5, so this is NOT a second-slot
  ordering artifact — it is a real ~1.3% host-speed REGRESSION from the fold.
- ROOT CAUSE: `rob_events_cpu` is touched every cycle (bitset scans). Growing it +64B/cpu and relocating the hot
  `event_cycle[]` further out hurts cache locality MORE than deleting the cold 4KB `flat_branch_mispredicted` helps.
- **VERDICT: keep-if-wins ⇒ REJECT.** Reverted the 2 files to the committed prefold state; canonical bin rebuilt.
  (Consistent with prior rejects where growing a hot struct for co-location regressed: SoA-tag, B2″ svector.)

---

## STEP P3 — sanity-wrap write-only rob_maps — KEPT (measured win, +1.09%)
Change (`src/ooo_cpu.cc`): wrapped both `rob_hash_table` writes under `#ifdef TRUE_SANITY_CHECK`:
`add_rob_idx` (add_to_rob, :547) and `retire_rob_idx` (retire, :2082). `rob_maps` is WRITE-ONLY in production —
its only reader is `check_rob()` → `get_rob_idx()`, called solely from `SANITY_CHECK_ROB_MATCH/_RFO`, both
`#ifdef TRUE_SANITY_CHECK` (= `((void)0)` when off, and it IS off @ champsim.h:37). Register-dep tracking uses
the flat `reg_producers[256]` (B2'); rob_index is carried directly in packets/LQ/SQ. So the per-instruction
emhash insert+erase was pure dead work. Built clang (bin 10411008, **9216 B smaller** than base 10420224).
A/B = p3-bin vs base-bin (prefold), 5x interleaved, serial.

| iter | p3 wall (s) | base wall (s) | paired Δ |
|------|-------------|---------------|----------|
| 1    | 201.9       | 204.2         | p3 −2.3  |
| 2    | 204.8       | 207.8         | p3 −3.0  |
| 3    | 201.7       | 204.3         | p3 −2.6  |
| 4    | 204.1       | 204.8         | p3 −0.7  |
| 5    | 202.3       | 204.9         | p3 −2.6  |
| **mean**          | **202.96** | **205.20** | **p3 +1.09% faster** |
| drop-warmup (2-5) | 203.23     | 205.45     | p3 +1.08% faster |

- **Checksum 140038115682937 + IPC 0.62589 EXACT on all 10 runs** (write-only removal ⇒ provably arch-neutral).
- **p3 faster 5/5 paired.** CROSS-VALIDATION: base mean here = **205.20**, IDENTICAL to the prefold mean in the
  P2 A/B (205.20, an independent 5-run session) ⇒ baseline is rock-stable, so the ~2.24s gain is REAL, not slot/
  thermal noise. Mechanism: removes one emhash insert (add_to_rob) + one erase (retire) PER instruction.
- **VERDICT: keep-if-wins ⇒ KEEP.** Canonical `bin/champsim.exe` is the p3 build. Committing the 2-file edit.

---

## CAMPAIGN SUMMARY (so far)
- P1: refactor == non-refactor (checksum+IPC exact, wall-neutral). Refactor validated.
- P2: flat_branch_mispredicted→rob_events fold = REJECTED (−1.3% host regression, arch-neutral).
- P3: rob_maps sanity-wrap = KEPT (+1.09% host win, arch-neutral). ← committed winner.
