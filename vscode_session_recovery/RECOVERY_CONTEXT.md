# ChampSim Optimization Campaign — Recovered Context (server lost 2026-06-10 ~22:40)

Reconstructed from local VS Code caches/backups on the Windows machine. Use this to re-seed a
new Claude Code session. Remote workspace was `ssh-remote+cc:/home/cc/champsim_VB`.

## Lost sessions (transcripts only on remote: ~/.claude/projects/-home-cc-champsim_VB/<id>.jsonl)

| Session ID | Active | Title |
|---|---|---|
| 84c2f24d-8985-475e-b2be-6bcb84269fae | 06-10 05:33 → 21:52 | Reduce cycle stalls in CPU cache simulation (MAIN) |
| 8714165b-ae2c-4086-b024-08b8900018ab | 06-09 23:40 → 06-10 05:32 | Organize simulation results by workload type |
| 501683ae-449d-4c6a-b29d-cf3b83b9950a | 06-09 21:49 → 21:52 | Check script loop filtering for forwarding and OCP mixes |
| 09d51765-375d-48ed-b43d-a8654c7afb50 | 06-08 → 06-09 06:43 | HERMES_PATH_FILE_KNOWN |

Full 117-session index: `_session_index.txt`. If server disk returns:
`claude --resume 84c2f24d-8985-475e-b2be-6bcb84269fae` restores the main session.

## STATE AT DISCONNECT (last scrollback, ~12:51 server time 06-10)

- **v19 = official fastest merged tree**: `champsim_v19_DRAM`, 16c sequential **272.586s**,
  bit-exact (IPC ;0.51803; + 16x checksum 140039767200773 + LPM ROI diff 0).
  Composition: v18 (11 opts) + P24 (retire early-break, −4.24%) + P21 (sched word-skip, −1.39%).
  −4.76% vs v18 (286.216s); −13.6% vs original 315.541s baseline.
- **Wave-3 slices all PASS on v19 base** (contended times, retime pending):
  W3-B 269.656s (B8 compact 128→32 + B1 __restrict lambda locals + B3 per-word dirty pre-skip),
  W3-C 270.277s (C4 tombstone-compact, C5 RegDep direct-index, C3A ADT front-cursor),
  W3-D 271.432s (D2 lpm_operate(T&) overload, D8 P28 port, D5 WQ instruction-skip, D1 run_clsbyp u16 shadow),
  W3-A 275.129s (A8 builtin_expect only; A6/A3/A1 PARKED).
- **RUNNING WHEN LOST: v20 build agent** — checkpointed in `/home/cc/champsim_VB/V20_BUILD_STATUS.md`
  (resumable). Plan: idle-wait → sequential retime of v19 + W3-A/B/C/D + P-BLOOM (6 runs) →
  rank (winner = faster than fresh v19 baseline; ±0.5s neutral-excluded) → stack winners
  one-by-one, both gates each → official idle v20 number → OPT_REPORT §11 + vault. ETA was ~2h.
- **DRAM-GAP analysis done** (GAP pr-10, 1c, IPC 0.05313, 5.18 host-s/Minstr): DRAM controller
  = 20.3% host CPU on GAP (vs ~4% LLM). schedule() O(SIZE) HIT+MISS scans = 15.13%,
  add_rq dup-scan 1.68%, operate 2.16%. Ranked queue for v20+: A bank-bucketed pending lists
  (5-10% absolute est.) → B open-row fast-gate → C (min,2nd-min) cache → D check_dram_queue
  hash (1.5-2%) → E free-idx bitmap. Gates: LLM bit-exact + GAP reference IPC 0.05313.
- **Refactor plan extended (Addendum 2, appended to
  `/home/cc/champsim_VB/cc_perf_specs/REFACTOR_CACHE_SPLIT_PLAN.md`)**: cache_helper.cc for
  getters/print bloat (user estimates true cache.cc logic ≈ lines 300–1700); dram_helper.cc
  same; ooo_cpu split → ooo_cpu_fetch/_decode/_schedule/_execute.cc, core keeps
  handle_branch/add_rob/update_rob/retire. Executor fires on v20, on user's word.
  Plan copy recovered locally: `0610_2236_Untitled-12_73e72b60.txt`. DESIGN ONLY — gated on
  v20 merge of sibling branches P-BLOOM and P-FASTSET (both editing cache.cc/cache.h).

## Gate / test command (recovered verbatim)

```
MODEL="4000fix-KappaPhiL1L2" && ./quick_v14.sh --glc --dir <DIR> -p 1 --L1 no --L2 Zion1 --L3 no \
  --trace LLM256.Pythiapre30Phase15xpost20-21M_14M -d 1 -c {1,16} -bypca \
  --l1byp $MODEL --l2byp $MODEL --l3byp $MODEL --ocp none --repl lru --time
```
Gate A (1c) IPC=0.55145; Gate B (16c) IPC=0.51803 + 16x execution_checksum 140039767200773 +
LPM ROI diff vs `/home/cc/champsim_VB/champsim_v17_DRAM_P2/P2_stage2_sim.log` = 0 lines.

## Version lineage

- v17_DRAM baseline: 315.541s (user, uncontended); 327.909s same-host serialized.
- v18 = v17 + STACK stages 0–11 (P1, P2, P6, P7/P7ext/P7c, P8, P9D, P10, P11, P14, P15): 286.216s serial.
- v19 = v18 + P24 + P21: 272.586s official.
- v20 = (in flight) v19 + retimed winners of W3-A/B/C/D + P-BLOOM.
- Rejected/excluded: P3, P4, P5(suspect), P13, P16(+0.30%), P17(+2.41%), P18/P19(premise false),
  P20(FAIL deadlock, restored from src/ooo_cpu.cc.P20_BK), P22(FAIL invariant), P23(+0.59%),
  P25/P28/P19B (PASS-faster but excluded from v19 per master directive — candidates again for v20),
  P26(+0.97%), P27(+23.7% REGRESSION).

## Recovered local files (this folder)

- `0610_2238_Untitled-13_73e72b5f.txt` — FULL OPT_REPORT.md copy (§1–§10 + Wave-3 rows + agent table). MOST COMPLETE ARTIFACT.
- `0610_2236_Untitled-12_73e72b60.txt` — REFACTOR_CACHE_SPLIT_PLAN.md copy (pre-Addendum-2).
- `0610_2236_Untitled-11_73e72b61.txt` — 1047-line session scrollback (orphan-kill saga, v19 creation, DRAM-GAP analysis, v20 launch).
- `0610_2107` / `0610_2034` / `0610_0535` — perf profiles (16c/1c execute_memory_instruction breakdowns).
- `0610_0642_Untitled-20` — test-command + equivalency-results note.
- `0609_0607_Untitled-16` — hoist/const-param optimization checklist (execute_load, reg_dependency, add_rq/check_hit/check_mshr, add_load_queue).
- `0608_1850_Untitled-15` — earlier (06-08) report snapshot.

## Key remote artifacts to grab FIRST if disk returns

1. `~/.claude/projects/-home-cc-champsim_VB/` (all session transcripts)
2. `/home/cc/champsim_VB/OPT_REPORT.md` (newer than local copy: §11 may exist)
3. `/home/cc/champsim_VB/V20_BUILD_STATUS.md` (v20 agent checkpoint)
4. `/home/cc/champsim_VB/cc_perf_specs/` (REFACTOR_CACHE_SPLIT_PLAN.md w/ Addendum 2 + slice specs)
5. The trees: `champsim_v18_DRAM`, `champsim_v19_DRAM`, `champsim_v19_DRAM_W3_{A,B,C,D}`,
   P-BLOOM/P-FASTSET dirs, `champsim_v18_DRAM_P2x` branch dirs (each has WORKLOG.md + .BK backups)
   — exact diffs = `diff -ru champsim_v18_DRAM champsim_v18_DRAM_P24` etc.
6. Vault notes: v18 architecture page, v19-merged-optimizations.md.
