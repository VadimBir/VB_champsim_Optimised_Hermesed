# IPC Sheet Restructure — Requirements Document
# v2 — 2026-05-14 — incorporates user clarifications

## Current State (What Exists)

### IPC Sheet Layout (current)
- **Separate SHEET per replacement policy** (IPC_lru, IPC_hawkeye, IPC_care, IPC_sdbp_2010)
- Within each sheet: **separate group per (bypass_model + OCP_mode) combo** per core count
- Each group: Cols = traces, Rows = prefetchers (Zion1, berti_c, bingo_dpc3, spp, no)
- Groups: "no" (base), "4000fix-KappaPhiL1L2" (forward), "4001-KappaPhiL1L2_OR" (forward2), "no|hermes", "no|hmp", "no|ttp"
- Each group has header row: `Cores | Model | PF \ Traces | trace1 | trace2 | ...`
- Groups separated by gap row(s)
- Geomean/AVG = full contiguous row
- Speedup = each PF row / no-PF base row within same group

### Current group explosion
- N replacement policies x M (model+OCP) combos x C core counts = separate groups
- Hard to compare across OCP modes or models for same trace

---

## Target State (What Is Needed)

### Core Concept: Partial Transpose
- What was **separate groups** (Base, Forward, OCP1, OCP2, OCP3) → becomes **sub-columns per trace**
- What was **separate sheets** per replacement policy → becomes **row groups under same core count**
- PF configs remain as rows within each replacement policy group

### Data Observed in out11
- **Replacement policies**: lru, hawkeye, care, sdbp_2010
- **Models**: no, 4000fix-KappaPhiL1L2, 4001-KappaPhiL1L2_OR (DYNAMIC — not hardcoded)
- **OCP modes**: off, hermes, hmp, ttp
- **L2 prefetchers**: Zion1, berti_c, bingo_dpc3, spp, no
- **Core counts**: 1, 2 (more may come: 4, 8, 16)
- **Arch**: perceptron (glc) — currently single, but script handles multiple

### Sub-columns per trace (DYNAMIC)
Derived from unique (model, hermes) combos in data. Expected order:
1. **Base** = model="no", hermes="off" — FIRST
2. **Hermes** = model="no", hermes="hermes" — SOTA OCP
3. **HMP** = model="no", hermes="hmp" — SOTA OCP
4. **TTP** = model="no", hermes="ttp" — SOTA OCP
5. **Forward Model 1** = model="4000fix-KappaPhiL1L2", hermes="off" — LAST section
6. **Forward Model 2** = model="4001-KappaPhiL1L2_OR", hermes="off" — LAST section
7. (any future models auto-discovered)

Order: **Base → SOTA/OCP → Forward models**

NOTE: Number of sub-columns is DYNAMIC. Script discovers unique combos from data.

### PF Row Order (within each replacement policy group)
1. **no-pf** (L1:no L2:no L3:no) — BASE ROW, FIRST
2. spp
3. Zion1 (or Zion-family)
4. bingo_dpc3
5. berti_c
NOTE: Generalized — script extracts unique PFs dynamically, order may need sort key

### 5 Gap Columns between trace groups (not 1)

### New Layout Per Core Count Section

```
                                    |<--- Trace: 429.mcf-184B --->|  gap x5  |<--- Trace: 429.mcf-217B --->|  gap x5  | ... | GM sub-cols | AVG sub-cols |
                                    | Base | Hermes|HMP|TTP|Fwd1|Fwd2|       | Base | Hermes|HMP|TTP|Fwd1|Fwd2|       |     |             |              |
CORE 1 — LRU
  no-pf (L1:no L2:no L3:no)        | 0.08 | 0.08  |0.09|0.09|0.08|0.08|    | 0.19 | 0.19  |0.20|0.21|0.22|0.22|    |     | GM-per-sub  | AVG-per-sub  |
  spp                               | 0.08 | 0.08  |0.09|0.09|0.09|0.09|    | 0.42 | 0.42  |0.42|0.41|0.43|0.43|    |     |             |              |
  Zion1                              | 0.11 | 0.10  |0.11|0.10|0.11|0.11|    | 0.57 | 0.54  |0.54|0.53|0.60|0.60|    |     |             |              |
  bingo_dpc3                         | 0.12 | 0.12  |0.12|0.12|0.12|0.12|    | 0.61 | 0.61  |0.61|0.59|0.63|0.63|    |     |             |              |
  berti_c                            | 0.08 | 0.08  |0.09|0.09|0.08|0.08|    | 0.43 | 0.43  |0.43|0.42|0.46|0.46|    |     |             |              |
(gap row)
CORE 1 — HAWKEYE
  no-pf                              | ...  | ...   |... |... |... |... |    | ...  | ...   |... |... |... |... |    |     |             |              |
  spp                                | ...  | ...   |... |... |... |... |    |      |       |    |    |    |    |    |     |             |              |
  ...
(gap row)
CORE 1 — CARE
  ...
(gap row)
CORE 1 — SDBP_2010
  ...
(larger gap — section separator)
CORE 2 — LRU
  ...
CORE 2 — HAWKEYE
  ...
...
```

### Speedup Formula (CHANGED — BIG)

**Base case for speedup** = same replacement policy, NO prefetcher (no-pf row), Base sub-column (model="no", hermes="off")

This means:
- **With-PF speedup**: PF row, Base sub-col / no-PF row, Base sub-col
- **With-PF+model speedup**: PF row, model sub-col / no-PF row, Base sub-col

The speedup denominator is ALWAYS the no-PF + Base sub-column cell for that trace in that repl group.

The speedup mirror block appears below the raw data block (same as current).

NOTE: This is a significant change from current behavior where speedup was within-group only.

### GM / AVG Calculation
- GM section has same number of sub-columns as each trace group
- GM-Base = geomean of all Base sub-col values across traces (skipping 5 gap cols)
- GM-Hermes = geomean of all Hermes sub-col values across traces
- etc.
- Same for AVG
- Formulas use non-contiguous cell references (like existing triple AVG/GM approach)

### Conditional Formatting
- **IPC: higher = GREEN, lower = RED** (higher-is-better)
- Style closer to MPKI sheet approach
- Applied per data group (per repl block within core count)
- Per column, per group — same mechanism as current `_apply_group_conditional_formatting()` but direction verified

### Style Preservation
- **PRESERVE existing formatting**: fonts, borders, header styles, number formats ("0.0000")
- Only change: layout structure + conditional formatting direction
- Section titles, group headers — keep style

---

## Scope — Implementation Phases

### Phase 1: IPC Sheet (this task)
- New `_write_transposed_ipc_block()` method
- New `_iter_repl_groups()` grouping iterator
- Dynamic sub-column discovery from data
- 5 gap columns between traces
- GM/AVG with non-contiguous references
- Speedup mirror with new base logic
- Conditional formatting
- All replacement policies in ONE sheet (not N separate sheets)
- Existing `_write_single_block()` preserved for backward compat if needed

### Phase 2: HOSC Sheet (NEW — Hit Overlap Stall Cycles)
Same transposed layout as IPC. New metrics:

**Metrics per cell (model x trace x PF):**
1. **HOSC_coverage** = `(HOSC_base - HOSC_model) / HOSC_base`
   - Coverage formula: how much of hidden overlap stall the model eliminates
   - Base = same repl, no-PF, Base sub-col (model="no", hermes="off")
   - Analog to prefetch coverage but on overlap axis
2. **HOSC_fraction** = `x_hidden / x` — diagnostic
   - Reported for BOTH base and model configs
   - x = overlap cycles (LPM classification)
   - Shows what fraction of C-AMAT's "benign overlap" is actually stalling

**Data source**: `LOAD_MSHR_cap` (LoadMSHRcap) from CP5_FAMILIES, already extracted per L1D/L2C/LLC
**LPM overlap**: `LPM_x` from LPM cycle classification (x column = overlap cycles)

**Denominator for HOSC ratio**: x (overlap cycles ONLY)
- NOT m (pure miss) — apples-to-oranges
- NOT m+x — dilutes, lower number
- x is C-AMAT's own bucket → honest + max impact

**Aggregation**: geomean across traces per (model x PF) cell

**Conditional formatting**: higher coverage = GREEN (higher-is-better)

**Story arc this enables**:
1. Headline: Forwarding gets X% IPC speedup
2. Mechanism: Y% HOSC coverage (OCPs get ~0% — don't touch MSHR admission)
3. Diagnostic: baseline HOSC_frac = Z% of x → C-AMAT's overlap bucket hiding real stall
4. Critique: prior SOTA optimizes m or kappa; we optimize hidden x_hidden axis they ignored

### Phase 3: Load Coverage Sheet
Same transposed layout. Coverage formula:
- **Load_coverage** = `(miss_no_pf - miss_with_pf) / miss_no_pf`
- Base = same repl, no-PF, Base sub-col
- Per level (L1D/L2C/LLC) — triple sub-columns per model sub-column
- Same aggregation (geomean)

### Phase 4: Other Sheets (MPKI, LPM, etc.)
- Same transposed approach
- Each gets sub-columns per trace (3-level x N-model = wider)
- Conditional formatting direction per metric type

---

## Functions to Modify (from agent analysis)

| Function | Lines | Change |
|---|---|---|
| `_iter_groups()` | 891-916 | New `_iter_repl_groups()`: group by `(arch, cores, repl)` instead of `(arch, cores, model_combo)` |
| `_write_single_block()` | 919-1001 | New `_write_transposed_ipc_block()`: sub-columns per trace, new speedup base |
| `_model_hermes()` | 579-582 | Keep but use differently — hermes becomes column index, not group key |
| `build_sheets()` | 652-676 | New pivot: `(arch, cores, repl, pf_combo)` → `(model, hermes)` → trace → value |
| `write_xlsx()` | 1445-1523 | IPC no longer creates per-repl sheets; creates single merged sheet |
| `_filter_data_by_repl()` | 1440-1442 | Removed for IPC; kept for other metrics until Phase 2 |
| `_apply_group_conditional_formatting()` | 1347-1413 | Adjust group detection for wider layout |
| `_gm_formula()` / `_avg_formula()` | 696-708 | New versions for non-contiguous sub-column references |

**Untouched**: collect, _process_log, all extractors, parse_log_filename, deduce_missing, validate_ipc_count

---

## Visual Target (from papers)
- Hermes Fig 16 (p12): Grouped bar chart, X=workload categories, bars=OCP configs
- CARE Fig 12 (p11): Scalability across cores, lines=repl policies
- CARE Fig 5 (p6): Per-benchmark distribution grid

Table layout enables creating these charts directly from sub-columns.

---

## Testing Strategy
- Opus test agent creates minimal standalone snippets per function
- Max 6 columns in test output (bounded)
- Uses copy of script as playground
- Verifies output before returning code
- Runs parallel to main implementation
