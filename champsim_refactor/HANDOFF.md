# CHAMPSIM SESSION — CANONICAL HANDOFF

> Single source of truth. Survives context compaction. Supersedes any chat "compact" message.

## 0. ONE-LINE
6-stage `#include`-fragment refactor of `champsim_refactor` DONE + committed + pushed; clang/libstdc++ toolchain solved; AFK perf-campaign (agent `af7f07c0`) running → `champsim_refactor/CAMPAIGN_RESULTS.md`.

## 1. SESSION ARC (chronological)
1. Committed+pushed entire repo first time (`97bd89e`, all dirs) → origin master.
2. Refreshed stale per-file `src/*.bounds.md` (function maps) via Sonnet fan-out.
3. Split-design study (4 Sonnets→Opus) → `CACHE_SPLIT_DESIGNS.md`, `SPLIT_DESIGN.md`, `CACHE_REFACTOR_ANALYSIS.md`. Chose **#include-fragment (1 TU, no extern)** over separate-`.cc` (which needs extern).
4. Refactor in gated stages (each build+run, checksum-exact). Phase-0 = bring 36 `handle_*` back from cache.h→cache.cc.
5. Clang thread: user insisted clang (Ubuntu/prod toolchain). Built ALL dirs clang → `CLANG_TIMING.md`: every dir checksum `140038115682937`, IPC **0.62589** (≠ gcc 0.62178). Diagnosed = **libc++ vs libstdc++ STL diff, not a bug**. Grafted w64devkit libstdc++ onto clang → **0.62178 EXACT** (`Makefile.win.clang.libstdcxx`).
6. Hardcoded `win_qbuild.sh`→clang; standardized clang BIN to `champsim.exe`; added Makefile filter-out.
7. User asked rm 3 corrupted dirs → deleted `v17_4c`/`v19_lpm_4c`/`v17_DRAM` (in git, recoverable). Removed 12 backup files (`.OPT*/.bk*`). Added `.gitignore` scratch + `.gitattributes` (LF pin).
8. Long opt-analysis Q&A (reg_producers, rob_maps, flat_branch_mispredicted, tombstone, register-id 138). Measured max_reg_id=138 / max_pl_depth=159.
9. Launched AFK timing+opt campaign.

## 2. GIT (master; origin github VadimBir/VB_champsim_Optimised_Hermesed)
`97bd89e` all-dirs · `499c68c` base+clang+libstdc+++equiv-sweep · `49e10c9` S1 stats · `cd33449` S2 ooo_cpu_helper · `84974cc` S3 dram_helper · `1f9bb7c` S4 cache DP→macros · `aa34cb9` inline-36-cache-helpers · `81a2af8` S5 .h→.cc rename · `1d0547d` S6 brace sweep. (AFK campaign will add commits for any kept winner + CAMPAIGN_RESULTS.md.)

## 3. TOOLCHAIN (CRITICAL — the 0.62178↔0.62589 thing)
- `win_qbuild.sh` HARDCODED clang (`Makefile.win.clang`, libc++) → `bin/champsim.exe`, IPC **0.62589**.
- **0.62178 (gcc/libstdc++) vs 0.62589 (clang/libc++) = STL difference** (libc++ vs libstdc++ → different page-alloc order in va_to_pa maps → diff IPC, SAME checksum). NOT a bug, NOT set.h UB. Proven.
- ★ `Makefile.win.clang.libstdcxx` = clang + w64devkit libstdc++ → IPC **0.62178 EXACT** (= gcc). Recipe: compile `-nostdinc++ -isystem C:/Users/vasum/w64devkit/lib/gcc/x86_64-w64-mingw32/16.1.0/include/c++{,/x86_64-w64-mingw32,/backward}`; link `-nostdlib++ -L.../16.1.0 -lstdc++`.
- GCC `Makefile.win` = libstdc++ → 0.62178. (set.h `join()` has documented OOB UB, masked by gcc `-fno-aggressive-loop-optimizations`; real but UNRELATED to the IPC gap.)

## 4. GATE
- checksum **140038115682937** (8-core; XOR of retired IPs) = equivalence metric, MUST be exact.
- IPC: **0.62589** clang/libc++ · **0.62178** gcc-or-clang+libstdc++ · 4-core: 0.65721 clang / 0.65907 gcc. Compare to SAME-toolchain baseline.
- Ignore in full-output diff (non-deterministic): ⏱ / TPMI / Last-compiled / `Core_*_total_produced` (async I/O thread).

## 5. BUILD / RUN
- BUILD: `sh quickSim/win_qbuild.sh --dir <DIR> no spp no --cores 8 --hermes ttp --repl lru --bp perceptron --arch glc` (PATH must incl `/c/Users/vasum/w64devkit/bin`) → `<DIR>/bin/champsim.exe`.
- RUN: `quickSim/run_one.ps1 -Bin <…\bin\champsim.exe> -Trace <…\traces\LLM256.Pythia-70M_21M.champsimtrace.xz> -Cores 8 -Log <log> -Warmup 2000000 -Sim 14000000 -Bypass 4000fix-KappaPhiL1L2 -PfL2 spp -Affinity F0`. run_one.ps1 = **repo-root** `quickSim/`, not per-dir.
- WALL = wrap run with `date +%s`. SERIAL only (1 sim at a time) for valid timing.

## 6. REFACTOR MAP (all bit-identical; `.cc` fragments #included, filter-out'd in all 4 Makefiles)
- `main.cc` 1866→**602** → `main_helper.cc` (FinalStatsCollector class + statsCollector + rob_events/mem_index_ring globals + print_roi/sim/branch/dram_stats + record_roi_stats + reset_cache_stats + print_knobs + finish_warmup + print_deadlock + print_end_of_sim_report) · `main_loop.cc` (`simulation_loop(show_heartbeat)` = per-cycle while-loop) · `main_paging.cc` (va_to_pa).
- `cache.cc` 2121→**2587** (36 `handle_*` brought back from header as `inline CACHE::`) → `cache_helper.cc` (was cache_debug: DP/SANITY/_DRMIN macros + dump_req*/print_cache_config + 15 named DP-block macros).
- `cache.h` 1552→**757** (36 `handle_read_hit_*/miss_*/writeback_*/fill_*` + return_to_upper_level moved out).
- `ooo_cpu.cc` 2246→**2127** → `ooo_cpu_helper.cc` (TRUE_SANITY_CHECK/SANITY_* block + print_core_config).
- `dram_controller.cc` 1446→**1366** → `dram_helper.cc` (DRAM_FULL_DP/DRAM_CH_DP/SANITY_DRAM + print_dram_config).
- Brace `)\n{`→`) {` ×429. `.gitattributes` pins LF (Linux-safe).

## 7. INLINE RULE
Offloaded methods originally **in-class** (implicitly inline) → mark `inline` when moved out-of-line (matches codegen). Done: 36 cache helpers. **Cross-TU** funcs (`print_core_config`, `print_dram_config`, `va_to_pa` — called from other `.cc`) MUST stay non-inline (else undefined-reference link error). FinalStatsCollector methods stay in-class.

## 8. OPT FINDINGS
- **rob_maps** (ROB_HashTable, instr_id→rob_index, ooo_cpu.cc): WRITE-ONLY in prod — add_rob_idx(`:551`)+retire_rob_idx(`:2111`) every instr; read only via get_rob_idx←check_rob←`SANITY_CHECK_ROB_MATCH/_RFO` = `#ifdef TRUE_SANITY_CHECK` (OFF @ champsim.h:37). Obsoleted by B2'. → wrap writes under TRUE_SANITY_CHECK = free host-speed (campaign P3).
- **Dep mechanism**: `reg_producers[256]` flat array (B2', uint8_t reg-id DIRECT index, branchless) inside misleadingly-named `addr_dependencies` (=REGISTER tracker). `mem_dependencies` = mem hashmap. rob_index carried directly in packets/LQ/SQ.
- **reg_producers[256] SETTLED → keep**: measured max_reg_id=**138** (SIMD/vector regs, AVX LLM trace, trace-dependent), max_pl_depth=**159**. 256 = full uint8_t range, safe-all-traces, branchless; can't shrink (138 needs ≥139). svector<16/32>=LOSS (depth 159 thrashes = B2″ rejection; current `vec.reserve(ROB_SIZE=512)` alloc-free). resizable-46 = not-worth (hot-path branch, trace-dep, ~KB, wall-flat). 256→#define OK as naming only.
- **flat_branch_mispredicted** (ooo_cpu.cc, `[cpu][512]` byte; write=1 `:404/:515`, clear `:2087`, read `:1601`): co-location candidate → fold into `rob_events` SoA bitset (instr_event.h BS_SET/TST/CLR like reg_ready/is_mem). Campaign P2, A/B paired-wall.
- `tombstone_count>32→compact()` = batch-amortization knob, behavior-neutral, tunable.
- Prior rejects (don't re-try): SoA-tag, base+delta, AddressProxy, B2″ svector — all dead/flat.

## 9. FACTS
`source_registers[4]`/`destination_registers[2]` = SLOT arrays (counts SRC=4 / DST=2 @ instruction.h:18-19,38-39); each slot = uint8_t reg-id 0–255. **138 = vector/SIMD reg** (it's a MAX over slot VALUES, not an index, not a sum). GP regs ~0-15.

## 10. DIRS
`champsim_refactor`=WORKING · `champsim_v19_avx2_lpm`=best checkpoint (= "non-refactored" baseline) · v17, v18_TEST_OPT, v19, v19_avx2, v18_optA_P11_B, v18_optA_P11_B_B2 = older. **DELETED** (corrupted, recoverable in git): v17_4c, v19_lpm_4c, v17_DRAM.

## 11. RULES
NEVER edit frozen/checkpoint dirs (dup only). NEVER rm dirs w/o explicit ask. NEVER >1 build/sim parallel (OOM); wall-timing serial+clean. All agents Opus. No `2>&1`/`2>/dev/null`/`||true`/`||echo`. Honest, never fake numbers (life-critical sim). `act` cmd absent here — don't fake ACKs. Scratch → `%TEMP%`, not repo.

## 12. AFK CAMPAIGN (agent `af7f07c0`) + NEXT
P1: time `v19_avx2_lpm` vs `champsim_refactor` (clang, 5× interleaved). P2: `flat_branch_mispredicted`→rob_events, gate, 5×. P3: `rob_maps` sanity-wrap, gate, 5× → `CAMPAIGN_RESULTS.md`; commits winners, reverts losers.
NEXT: read `CAMPAIGN_RESULTS.md` → register-id histogram (prove 138 + depth distribution, gate-neutral) → reg_producers resizable A/B (expect flat).
Memory files: `project_afk_opt_loop.md`, `project_win_v17_reconstruction.md`, `feedback_scratch_files_to_temp.md`.
