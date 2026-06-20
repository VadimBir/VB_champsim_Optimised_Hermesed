# Evaluation Plan Matrix — ChampSim L1/L2/LLC Bypass

**Date**: 2026-04-30  
**Goal**: Beat 0.57 IPC on Pythia, 0.77 on LBM  
**Baselines (fixed)**:
| Label | IPC (Pythia) | IPC (LBM) |
|---|---|---|
| no-bypass (LRU, no prefetch) | 0.50778 | 0.75614 |
| AlwaysBypass | 0.53692 | 0.76354 |
| PMCHeadroom (1100) | 0.54110 | — |
| **SOTA 520a** | **0.55543** | — |

---

## Section 1 — Model Matrix

| Priority | Model ID | Description | Bypass Levels | Predictor Type | Hermes |
|---|---|---|---|---|---|
| P1 | 520a | KappaReduce + AlwaysLLC + CAMATPressure (current SOTA) | L1+L2+LLC | CAMAT pressure gating | off |
| P2 | B-successor | 520a + IP-hash feature + perceptron-latency term + CARE contribution weight | L1+L2+LLC | Perceptron + CARE | off |
| P3 | 1200-SDBPstyle | SDBP dead-block predictor ported as bypass predicate (PC-indexed skewed predictor, predict-dead→bypass) | LLC only | Skewed PC table | off |
| P4 | 1210-SDBPplusLPM | 1200-SDBPstyle + LPM CAMAT gating on top | LLC only | Skewed PC + LPM | off |
| P5 | 1100-PMCHeadroom | Prior SOTA (PMC headroom gating) | L1+L2+LLC | PMC headroom | off |
| P6 | 520a + Hermes-TTP | SOTA bypass + Hermes TTP prefetch shortcut | L1+L2+LLC | CAMAT | TTP |
| P7 | 520a + Hermes-HMP | SOTA bypass + Hermes HMP | L1+L2+LLC | CAMAT | HMP |
| P8 | B-successor + Hermes-TTP | Successor model + Hermes TTP | L1+L2+LLC | Perceptron+CARE | TTP |
| P9 | B-successor + Hermes-HMP | Successor model + Hermes HMP | L1+L2+LLC | Perceptron+CARE | HMP |
| P10 | 1210-SDBPplusLPM + Hermes-TTP | SDBP+LPM + Hermes TTP | LLC | Skewed+LPM | TTP |
| P11 | no-bypass + Hermes-TTP | Hermes alone (control for Hermes contribution) | none | — | TTP |
| P12 | no-bypass + Hermes-HMP | Hermes alone control | none | — | HMP |

**Hermes variants**: off = standard hierarchy; TTP = time-to-prefetch shortcut; HMP = history-based multi-path  
**Top-3 bypass models for Hermes cross-product**: 520a, B-successor, 1210-SDBPplusLPM

---

## Section 2 — Prefetcher Cross-Test

Run top-2 models (520a, B-successor) × 4 SOTA prefetchers:

| Prefetcher | Rationale |
|---|---|
| **Pythia** | RL-based, already primary trace source — baseline comparison |
| **Bingo** | Spatial pattern, complements temporal bypass decisions |
| **SPP (Signature Path Prefetcher)** | Confidence-based, interacts with bypass via MSHR contention |
| **IPCP (IP-based Classified Prefetcher)** | IP-classified streams — directly tests IP-hash bypass synergy |

Matrix: 2 models × 4 prefetchers × 2 traces = 16 runs (lower priority, run after P1–P5)

---

## Section 3 — Traces

| Trace | Priority | Reason |
|---|---|---|
| Pythia suite | MUST | Primary benchmark, IPC target 0.57 |
| LBM | MUST | Memory-intensive, IPC target 0.77, AlwaysBypass wins here |
| SPEC CPU2017 memory-bound (mcf, omnetpp, xalancbmk) | IF TIME | Covers diverse access patterns |
| GAP benchmark (BFS, PageRank) | IF TIME | Irregular access — stress-tests LLC bypass |

---

## Section 4 — Priority Run Order (Today's Meeting)

**Phase 1 — Must run before meeting** (P1–P5, Pythia + LBM only):
1. **520a** on Pythia + LBM — confirm SOTA baseline reproduced
2. **1200-SDBPstyle** on Pythia + LBM — establishes SDBP-as-bypass lower bound
3. **1210-SDBPplusLPM** on Pythia + LBM — SDBP + gating, head-to-head vs 520a
4. **1100-PMCHeadroom** on Pythia + LBM — prior SOTA confirm
5. **B-successor** on Pythia + LBM — main new result

**Phase 2 — If time permits before meeting**:
6. 520a + Hermes-TTP on Pythia (Hermes contribution isolation)
7. B-successor + Hermes-TTP on Pythia

**Phase 3 — Post-meeting / async**:
8. Full Hermes cross-product (P6–P12)
9. Prefetcher cross-test (Section 2)
10. Additional traces (SPEC, GAP)

---

## Section 5 — Key Differentiators to Highlight

| Dimension | Yours (520a/successor) | SDBP | HBPB | Hermes |
|---|---|---|---|---|
| Bypass mechanism | MSHR-skip (before MSHR creation) | Fill-skip (MSHR created, fill skipped on return) | Parallel NTB path | L1→DRAM shortcut |
| Levels | L1+L2+LLC (non-adjacent) | LLC only | All levels | L1 only |
| Predictor | CAMAT pressure / perceptron+CARE | PC-indexed skewed table | Confidence counter | History-based |
| Overhead | Low (no extra storage path) | Low | High (32KB NTB) | Medium (DRAM-side buffer) |

**Novelty of yours vs SDBP**: More aggressive (MSHR-skip vs fill-skip), multi-level, dynamic threshold vs static table.

---

## Section 6 — Success Criteria

| Metric | Pass |
|---|---|
| B-successor IPC (Pythia) | > 0.55543 (beats 520a), target 0.57 |
| B-successor IPC (LBM) | > 0.75614, target 0.77 |
| 1210-SDBPplusLPM vs 520a | Determines if SDBP-style feature worth adding to successor |
| Hermes + bypass vs bypass alone | Quantifies Hermes contribution; if < 0.5% → deprioritize |
| Prefetcher cross-test | Confirms IP-hash synergy with IPCP |
