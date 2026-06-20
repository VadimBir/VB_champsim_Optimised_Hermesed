# 16-core timing — v17_DRAM vs v18_TEST_OPT (2026-06-19)

**Config:** NUM_CPUS=16 · warmup 4,000,000 · sim 10,000,000 · trace `LLM256.Pythia-70M_21M` ×16
**Method:** sequential (never parallel), RealTime priority, affinity `0xF0` (cores 4–7), median of 3 runs
**Binaries:** native Windows (w64devkit g++, `Makefile.win`)

| tree | run1 | run2 | run3 | MEDIAN | checksum | IPC |
|------|------|------|------|--------|----------|-----|
| champsim_v17_DRAM     | 469.5 | 473.3 | 473.5 | **473.3 s** | 140038081834206 | 0.35503 |
| champsim_v18_TEST_OPT | 435.7 | 433.0 | 439.0 | **435.7 s** | 140038081834206 | 0.35109 |

**Δ wall-clock: v18 is −7.94% (faster) vs v17_DRAM.**

## Honest caveat (do not overclaim)
- Same `execution_checksum` (140038081834206) ⇒ identical committed-instruction stream (functionally equivalent run).
- **IPC differs** (0.35503 vs 0.35109) ⇒ the two trees are **different codebases/microarchitecture**. `v18_TEST_OPT` = the `champsim_v17` **fork** (already divergent from DRAM) + 14 opts.
- Therefore −7.94% = **fork-divergence + 14 opts combined**, NOT the 14 opts in isolation.
- **Clean opt-isolation requires timing `champsim_v17` (fork, same lineage, no opts) vs `v18_TEST_OPT`.** Not yet run (fork still NUM_CPUS=2).

## Windows build shims applied to champsim_v17_DRAM (all `#ifdef _WIN32`, Linux byte-identical)
- `inc/block.h`: `execinfo.h`/`dlfcn.h` guarded (backtrace calls already commented out)
- `inc/ooo_cpu.h`: `posix_memalign` → `malloc` (64B alignment was perf-hint only)
- `src/main.cc`: `sigaction` → `signal(SIGINT,…)`
- `src/trace_helper.cc`: `pthread_sigmask` → spawn thread without mask
- added `win_deps/` + `Makefile.win` (Linux `Makefile` untouched)
