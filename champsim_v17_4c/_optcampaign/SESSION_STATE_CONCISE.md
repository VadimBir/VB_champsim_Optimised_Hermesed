# SESSION STATE — concise (2026-06-20)

## 1. IPC "mismatch" — SOLVED (measured, not inferred)
4-core gate (2M warm / 10M sim, 4 trace copies, RT, aff 0xF0):

| tree | checksum | IPC |
|---|---|---|
| fork `champsim_v17` (0 of 14 opts) | 140038080281864 | 0.55277 |
| `champsim_v17_DRAM` | 140038080281864 | 0.53922 |
| `champsim_v18_TEST_OPT` (fork+14) | 140038080281864 | **0.55277** |

- **All 3 checksums identical → no corruption.** Architecturally equal; only timing model differs.
- **fork IPC == v18 IPC to the digit → the 14 opts add ZERO IPC change; merge is clean.**
- **DRAM ≠ fork (0.53922 vs 0.55277) with ZERO opts involved → gap is 100% base divergence (fork≠DRAM), not the opts.**
- Sign even flips by core count: 16c earlier had DRAM 0.35503 > v18 0.35109; 4c has DRAM < fork. = two different timing models.
- 16-core ref (4M/10M): v18 0.35109 / DRAM 0.35503, both checksum 140038081834206. v18 −7.94% wall vs DRAM (fork-divergence+opts combined).

## 2. The 14 opts
P24 P21 P15 A8 B1 B3 P28 B8 C3A C5 P-BLOOM D5 D2 P25 — **all PASS bit-exact vs the fork**, **zero reverts ever**. Patches: `champsim_v17/_optcampaign/patches/`. P19B & D1 were queued but NO-OP (target code absent) → final set 14, not 16. Proven on the FORK, **never gated vs DRAM**.

## 3. Roles (why "lost")
- **DRAM** = true base you want (IPC 0.53922/0.35503), un-optimized binary, none of the 14.
- **v18** = fork+14 = **right opts, WRONG base** → carries fork's IPC, not DRAM's.
- **GOAL (doesn't exist yet)** = DRAM + 14 opts = keeps DRAM's IPC, faster binary. → the DRAM-port task.

## 4. Recovered remote work NOT here (file: `RECOVERED_NOT_APPLIED_HERE.md`)
- **P8** (LPM 704→688): recoverable substance = removing 2 dead `*_byp_llc_pureMissCy` fields → **ALREADY DONE here** (DEAD-2026-05-25). `sizeof(LPM_Tracker)=704` measured; 704/688 don't map (this config has long/short buckets). No faithful reorder spec. **Nothing real left to apply.**
- **P9D** (`auto& re` hoist in cache.h): **site absent** here (grep nil) — like P19B/D1.
- **P11** (O3 hoists/prefetch in exec_mem/sched_mem/update_rob, ooo_cpu.cc): **real, missing, MED effort** (overlaps P15/P21/P24 region). Behavior-preserving.
- **P6**: remote KEPT it, but **locally rejected unsafe** (AddressProxy is LIVE here) — needs decision, not blind port.
- 06-08 data-packings = "VERIFIED" is v15-era **static claim, never gate-applied** — candidates only. **P-FASTSET** content unrecoverable. v19 cache-split = design-only (no speedup). **v20 never existed as code.**

## 5. DRAM controller: DRAM vs v18 (read-only diff)
`memory_class.h`/`uncore.*` identical. 3 real diffs in `dram_controller.cc`/`.h`:
- **P2 `num_unsched` early-out** (v18 only): `if(occupancy<=scheduled[ch]) return;` + channel arg. **Behavior-preserving** — returns only when all pending already dispatched (both scans would find nothing). Speedup: O(2·SIZE)→O(1) on idle queues.
- **Opt-A** (DRAM only): incremental `working_bank_count` counter for `miss_active` vs v18's per-cycle ch×rank×bank rescan.
- **Opt-B** (DRAM only): idle-batching (`dram_idle_accumulator++`/`advance_idle`) vs v18's unconditional per-cycle sweep.
- Opt-A/B feed **LPM tracker only**, not DRAM scheduling → don't change checksum/ordering. ⚠ Opt-A `miss_active(count>0)` vs v18 `working&&avail>now` are **not bit-identical LPM signals** (differ in post-latency-pre-process window) — LPM-stats only, not checksum.
- **Direction note:** the real DRAM speedups (Opt-A/B) live in the **DRAM tree; v18 LACKS them** → port into optimized-DRAM.

## 6. Current disk state ⚠
All 3 trees set **NUM_CPUS=4** (champsim.h:137); their `bin/champsim.exe` are **4-core**. DRAM's & v18's prior **16-core binaries were overwritten**. To restore 16c: set NUM_CPUS=16 + clean rebuild.

## 7. Build/run rules
- PATH: `export PATH="/c/Users/vasum/w64devkit/bin:$PATH"`. Build: `make -f Makefile.win -j4` per tree. **NEVER 2 builds in parallel → cc1plus OOM.**
- Run: PowerShell `Start-Process ... ProcessorAffinity=[IntPtr]0xF0; PriorityClass=RealTime`. RedirectStandardOutput needs a **`cygpath -w` Windows path** (a `/tmp/..` path silently writes to `C:\tmp`).
- Scratch → `/tmp/champsim_scratch` (=`%TEMP%`), never repo. **BAN: `2>/dev/null` `2>&1` `||true` `||echo 0`.** `act` not installed on this machine.
- Header-only edits leave a STALE binary (no dep tracking) → always clean-rebuild.
