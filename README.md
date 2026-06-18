# ChampSim v10 — Accelerated and Speed up architecture including a Bypass modeling Framework

ChampSim out-of-order CPU simulator with L1D cache bypass design space exploration (DSE).

---

## Quick Start

```bash
# Single run: 2 cores, no prefetch, LLM trace, debug on, bypass+cAMAT enabled
./quick_v10.sh -p 1 --L1 no --L2 no --trace LLM256.Pythia-70M_21M -d 1 -c 2 -bypca -ByPModel no

# See all options
./quick_v10.sh -h
```

---

## Active Scripts (root)

| Script | Purpose |
|--------|---------|
| `quick_v10.sh` | Main entry: build + run one config |
| `qbuildPrefetcher_v10.sh` | Build ChampSim binary with selected prefetcher |
| `qrun_champsim_v10.sh` | Run simulation (called by quick_v10) |
| `quickByP_v10.sh` | Parallel bypass model DSE runner |

### Key flags (`quick_v10.sh`)
| Flag | Description |
|------|-------------|
| `-p <N>` | Number of parallel runs |
| `--L1 <pf>` | L1 prefetcher (`no`, `nextline`, `visio`, …) |
| `--L2 <pf>` | L2 prefetcher |
| `--trace <name>` | Trace short name (see Traces below) |
| `-d <0/1>` | Debug output (1=verbose periodic stats) |
| `-c <N>` | CPU cores (1 or 2) |
| `-bypca` | Enable bypass + C-AMAT model |
| `-ByPModel <name>` | Bypass model name (`no` = baseline) |

---

## Traces

Traces live in `traces/`.

---


## Directory Structure

```
champsim_v10/          Active simulator (v10 = was v7_ByP_fast)
outputs/               Simulation output directories
sim_db/                Results DB + query scripts (CHAMPSIM_RESULTS.db)
docs/                  Design reports, agent guides, DSE analysis
traces/                Trace files
IIT_C-AMAT.pdf         Reference paper (Liu, Espina, Sun 2021)
```

---

All active scripts embed these as `${VAR:-default}` — ini file is optional override.

---

## Simulation Output

Periodic line every ~25K instructions:
```
C:0 I# 21000001 cy 24370345 IPC 1.91 IPCavg 0.83810 ByP 0.00 0.08 0.28
     APC 0.25 0.04 0.00 LPM 0.99 0.68 0.55 cAMT 0.25 0.04 0.00
     MSHR% 43 0 0 LoadH% 90 90 43
```
- `IPCavg` = cumulative IPC (primary metric, maximize)
- `MSHR%` = MSHR utilization per level — watch L2C > 0 (cascade risk)
- `ByP` = bypass rate per level

