# Clang build+run, all dirs (2M warmup / 14M sim, 8-core)

Toolchain: llvm-mingw clang++ (no LTO, no -mavx2), -O3. Same trace.
The three corrupted 4-core/DRAM dirs (v17_4c, v19_lpm_4c, v17_DRAM) were removed.

| dir | cores | build_s | run_s | checksum | IPC |
|---|---|---|---|---|---|
| champsim_v17 | 8 | n/a* | 264 | 140038115682937 | 0.62589 |
| champsim_v18_TEST_OPT | 8 | 39 | 253 | 140038115682937 | 0.62589 |
| champsim_v19 | 8 | 48 | 250 | 140038115682937 | 0.62589 |
| champsim_v19_avx2 | 8 | 47 | 247 | 140038115682937 | 0.62589 |
| champsim_v19_avx2_lpm | 8 | 66 | 276 | 140038115682937 | 0.62589 |
| champsim_v18_optA_P11_B | 8 | 66 | 276 | 140038115682937 | 0.62589 |
| champsim_v18_optA_P11_B_B2 | 8 | 68 | 262 | 140038115682937 | 0.62589 |
| champsim_refactor | 8 | 60 | 248 | 140038115682937 | 0.62589 |

*v17 build time lost to a stray-process collision; its checksum+IPC are valid (match all others).

## Conclusion
- Under clang, all 8 dirs are bit-identical to each other: same checksum (140038115682937) AND same IPC (0.62589). Fully equivalent — the refactor preserves behavior under clang.
- clang IPC 0.62589 != GCC IPC 0.62178 — that gap is the latent inc/set.h join() OOB UB; clang exploits it consistently (identical value across every dir), GCC masks it via -fno-aggressive-loop-optimizations.
- clang build ~26-68s (vs GCC minutes); clang run ~247-276s @ 8-core.
