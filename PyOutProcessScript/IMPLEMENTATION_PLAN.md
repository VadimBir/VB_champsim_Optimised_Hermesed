# IPC Restructure — Implementation Plan (Phase 1)

## Overview of Changes

Current: per-repl sheets, groups by (arch, cores, model_combo), rows=PFs, cols=traces (1 col/trace)
Target: single IPC sheet, groups by (arch, cores, repl), rows=PFs, cols=traces x N sub-columns (dynamic model combos), 5 gap cols between trace groups

---

## Step 0: Constants and Sort Keys

### 0a. Add `REPL_ORDER` dict for deterministic replacement policy ordering
```python
REPL_ORDER = {"lru": 0, "hawkeye": 1, "care": 2, "sdbp_2010": 3}
```
Location: near `NO_PF_COMBO` (L882)

### 0b. Add `_subcol_sort_key(model_combo)` function
Sorts (model, hermes) combos: Base first, then OCP modes, then forward models.
```python
def _subcol_sort_key(mc):
    """Order: no(off)=0, no|hermes=1, no|hmp=2, no|ttp=3, forward_models=4+"""
    if mc == NO_MODEL_TOKEN:
        return (0, "")
    if mc.startswith(NO_MODEL_TOKEN + "|"):
        ocp = mc.split("|", 1)[1]
        ocp_order = {"hermes": 1, "hmp": 2, "ttp": 3}
        return (ocp_order.get(ocp, 9), ocp)
    return (10, mc)  # forward models sort last, alpha tiebreak
```

### 0c. Add `_pf_row_sort_key(pf_combo)` function
no-pf FIRST (not last as current `base_sort_key`), then spp, Zion1, bingo_dpc3, berti_c, rest alpha.
```python
PF_PREFERRED_ORDER = {"L1:no L2:no L3:no": 0}  # dynamic fill for others
def _pf_row_sort_key(pf_combo):
    if pf_combo == NO_PF_COMBO:
        return (0, "")
    # Extract L2 prefetcher name as primary sort key (L2 is the differentiator)
    # PF format: "L1:X L2:Y L3:Z"
    parts = pf_combo.split()
    l2 = parts[1].split(":")[1] if len(parts) > 1 else pf_combo
    known = {"spp": 1, "Zion1": 2, "bingo_dpc3": 3, "berti_c": 4}
    return (known.get(l2, 5), pf_combo)
```

---

## Step 1: New `build_sheets_transposed(data, metric_key)` function

Current `build_sheets()` drops `repl` because data is pre-filtered. New function takes **full** data (all repls).

**Signature:**
```python
def build_sheets_transposed(data, metric_key):
    """Build transposed sheet data. Groups by (arch, cores, repl, pf_combo).
    Returns dict with keys: combined, traces, subcols, repls, pf_combos_per_group."""
```

**Data structure:**
```python
combined[(arch, cores, repl, pf_combo)][(model_combo)][trace] = value
```

**Returns:**
```python
{
    "combined": combined,           # nested dict
    "traces": sorted_trace_list,    # deterministic order
    "subcols": sorted_subcol_list,  # sorted by _subcol_sort_key, dynamic
    "groups": [                     # ordered list of (arch, cores, repl) groups
        ((arch, cores, repl), [pf_combo_ordered_list]),
        ...
    ],
}
```

**Key logic:**
- Iterate all data keys (11-tuple), do NOT filter by repl
- `model_combo = _model_hermes(l1m, l2m, llcm, hermes)` — same as now
- Collect all unique (model_combo) across entire dataset → sort by `_subcol_sort_key` → `subcols`
- Collect all unique traces → sort → `traces`
- Group by (arch, cores, repl) → per group collect PF combos → sort by `_pf_row_sort_key`
- Sort groups: (arch, cores, REPL_ORDER.get(repl, 99))
- Skip groups where no-PF base is missing

**Depends on:** Step 0

---

## Step 2: New `_write_transposed_header(ws, row, traces, subcols, first_trace_col, gap_width=5)` function

**Two-row header:**
- Row 1: label cols + merged trace name spanning N subcols, then 5 gap cols, repeat per trace. After all traces: "GM" merged + "AVG" merged.
- Row 2: label cols + subcol labels (short names like "Base", "Hermes", "HMP", "TTP", "4000fix", "4001") repeated per trace. Same labels under GM and AVG.

**Key logic:**
- `n_sub = len(subcols)` — dynamic
- Per trace: columns `[first_trace_col + t*(n_sub + gap_width) .. first_trace_col + t*(n_sub + gap_width) + n_sub - 1]`
- Gap columns are left empty (5 cols)
- After all traces: GM group (n_sub cols) + AVG group (n_sub cols)
- Merge cells for trace names spanning n_sub cols
- Sub-column labels: derive short name from model_combo string

**Helper:** `_subcol_short_name(model_combo)` — "no" → "Base", "no|hermes" → "Hermes", "no|hmp" → "HMP", "no|ttp" → "TTP", "4000fix-KappaPhiL1L2" → "4000fix", etc.

**Returns:** `(n_sub, gap_width, total_width_per_trace, gm_start_col, avg_start_col, last_col)` — a named tuple or dict for downstream use

**Depends on:** Step 0

---

## Step 3: New `_transposed_gm_avg_formulas(ws, row, traces, subcols, first_trace_col, gap_width, gm_start_col, avg_start_col)` function

**Per sub-column index `si`:** build non-contiguous cell references spanning all traces for that sub-column.

**Logic:**
```python
for si in range(n_sub):
    cols_for_si = [first_trace_col + t * (n_sub + gap_width) + si for t in range(n_traces)]
    # GM formula: =EXP(SUM(IF(valid, LN(cell), 0)) / SUM(IF(valid, 1, 0)))
    # with explicit cell refs (non-contiguous)
    # AVG formula: =SUM(IF(valid, cell, 0)) / SUM(IF(valid, 1, 0))
    gm_col = gm_start_col + si
    avg_col = avg_start_col + si
```

Uses same pattern as `_write_triple_avg_gm()` L791-826: explicit cell ref lists joined by `+`, not contiguous ranges.

**Depends on:** Step 2 (layout info)

---

## Step 4: New `_write_transposed_ipc_block(ws, sh_transposed, start_row)` function

This is the core writer. Replaces `_write_single_block` for IPC.

**Signature:**
```python
def _write_transposed_ipc_block(ws, sh, start_row):
    """Transposed IPC block: all repls in one sheet. Returns next_row."""
```

**Layout algorithm:**
```
row = start_row
write section title "IPC: all replacement policies (transposed)"
row += 1

for (arch, cores, repl), pf_combos in sh["groups"]:
    # Write CORE N — REPL title row
    ws.cell(row=row, column=1, value=f"CORE {cores} — {repl.upper()}")
    row += 1

    # Write 2-row header
    layout = _write_transposed_header(ws, row, ...)
    row += 2

    base_row_for_group = None  # row of no-PF, Base subcol

    for pf_combo in pf_combos:
        # Label cols
        ws.cell(row=row, column=1, value=arch)
        ws.cell(row=row, column=2, value=cores)
        ws.cell(row=row, column=3, value=repl)
        ws.cell(row=row, column=4, value=pf_combo)

        # Data cells: for each trace, for each subcol
        for ti, trace in enumerate(sh["traces"]):
            for si, mc in enumerate(sh["subcols"]):
                col = first_trace_col + ti * (n_sub + gap_width) + si
                val = sh["combined"].get((arch,cores,repl,pf_combo), {}).get(mc, {}).get(trace, SENTINEL_RUNNING)
                if val == SENTINEL_RUNNING or val == SENTINEL_ERRORED or val == 0:
                    ws.cell(row=row, column=col, value="")
                else:
                    _set_ipc_cell(ws, row, col, val)

        # GM/AVG formulas
        _transposed_gm_avg_formulas(ws, row, ...)

        # Track rows for speedup
        main_row_of[(arch, cores, repl, pf_combo)] = row
        if pf_combo == NO_PF_COMBO:
            base_row_for_group = row
            base_row_of[(arch, cores, repl)] = row
        row += 1

    row += 1  # gap between repl groups

    # Larger gap between core count sections (detect: next group has different cores)
    ...

row += 2  # gap before speedup mirror
```

**Speedup mirror** (second pass):
```
for (arch, cores, repl), pf_combos in sh["groups"]:
    # header rows again
    base_r = base_row_of.get((arch, cores, repl))

    for pf_combo in pf_combos:
        src_row = main_row_of[(arch, cores, repl, pf_combo)]
        for ti in range(n_traces):
            for si, mc in enumerate(sh["subcols"]):
                data_col = first_trace_col + ti * (n_sub + gap_width) + si
                cl = get_column_letter(data_col)
                # CRITICAL: speedup denominator = ALWAYS base_r row, Base subcol (si=0)
                base_col_idx = first_trace_col + ti * (n_sub + gap_width) + 0  # si=0 = Base
                base_cl = get_column_letter(base_col_idx)
                formula = _speedup_formula_transposed(cl, src_row, base_cl, base_r)
                cell = ws.cell(row=row, column=data_col, value=formula)
                cell.number_format = "0.0000"

        # GM/AVG for speedup rows too
        _transposed_gm_avg_formulas(ws, row, ...)
        row += 1
    row += 1
```

**Depends on:** Steps 1, 2, 3

---

## Step 5: New `_speedup_formula_transposed(src_cl, src_row, base_cl, base_row)` function

Current `_speedup_formula` assumes same column for numerator and denominator (`cl, src_row / cl, base_row`).

New version: numerator col may differ from denominator col.
```python
def _speedup_formula_transposed(src_cl, src_row, base_cl, base_row):
    return (
        f'=IF(AND({src_cl}{src_row}>=0,{base_cl}{base_row}>0),'
        f'{src_cl}{src_row}/{base_cl}{base_row},"")'
    )
```

This is the **key change** from the requirements: speedup denominator is ALWAYS no-PF row, Base sub-column (model="no", hermes="off"), regardless of which sub-column the numerator is in.

**Depends on:** nothing

---

## Step 6: Modify `HEADER_COLS_COMBINED`

Current: `["Arch", "Cores", "Model", "PF \\ Traces"]`
New (for transposed): `["Arch", "Cores", "Repl", "PF \\ Traces"]` — Model is no longer a row label; it's a sub-column.

Add as separate constant:
```python
HEADER_COLS_TRANSPOSED = ["Arch", "Cores", "Repl", "PF \\ Traces"]
```

**Depends on:** nothing

---

## Step 7: New `_write_transposed_ipc_sheet(wb, data, sheet_name)` function

Replaces `_write_metric_sheets` for IPC kind.
```python
def _write_transposed_ipc_sheet(wb, data, sheet_name):
    ws = wb.create_sheet(title=sheet_name[:31])
    sh = build_sheets_transposed(data, "ipc")
    _write_transposed_ipc_block(ws, sh, start_row=1)
```

**Depends on:** Steps 1, 4

---

## Step 8: Modify `_sheet_job()` (L1416-1437)

Add new kind `"ipc_transposed"`.
```python
elif kind == "ipc_transposed":
    _write_transposed_ipc_sheet(wb, data, sheet_name)
```

**Depends on:** Step 7

---

## Step 9: Modify `write_xlsx()` (L1445-1523)

### 9a. IPC metric: single job with FULL data (not per-repl)
Replace the IPC entry in `metric_specs` loop:
```python
# Instead of iterating per-repl for IPC:
# Old: for repl in repls: jobs.append(... kind="ipc", data=data_by_repl[repl] ...)
# New: single job
jobs.append((order, "IPC", "ipc_transposed", data, sheets_dir))
order += 1
```

### 9b. All other metrics: keep per-repl as-is
The `for base_name, kind, _ in metric_specs:` loop stays, but IPC is excluded from the per-repl iteration and handled separately above.

### 9c. Alternative: keep `metric_specs` but add a flag
```python
metric_specs.append(("IPC", "ipc_transposed", "full"))  # "full" = use unfiltered data
```
In the loop:
```python
for base_name, kind, scope in metric_specs:
    if scope == "full":
        jobs.append((order, base_name, kind, data, sheets_dir))
        order += 1
    else:
        for repl in repls:
            sname = _sname(base_name, repl)
            jobs.append((order, sname, kind, data_by_repl[repl], sheets_dir))
            order += 1
```

**Depends on:** Steps 7, 8

---

## Step 10: Modify `_apply_group_conditional_formatting()` (L1347-1413)

### Issue: wider layout with gap columns
Current group detection scans cols 5-9 for numeric data. With the new layout:
- Gap columns (5 empty cols between traces) will break contiguous group detection — this is FINE because the row-based detection still works (gap cols are empty but the row has data in other cols).
- Current probe range `range(5, min(max_col+1, 10))` may miss data if sub-columns start later.

### Fix:
Expand probe range: instead of `range(5, min(max_col+1, 10))`, use `range(5, min(max_col+1, 20))` — scan more columns to find at least one data cell per row.

### Color direction:
IPC sheet = higher-is-better = current default (red-low, green-high). Already correct.

**Depends on:** nothing (independent)

---

## Implementation Order (dependency graph)

```
Step 0  (constants, sort keys)          — no deps
Step 5  (transposed speedup formula)    — no deps
Step 6  (header constant)               — no deps
Step 10 (cond formatting fix)           — no deps
  |
Step 1  (build_sheets_transposed)       — depends on Step 0
Step 2  (header writer)                 — depends on Step 0
  |
Step 3  (GM/AVG formulas)              — depends on Step 2
  |
Step 4  (core block writer)            — depends on Steps 1, 2, 3, 5, 6
  |
Step 7  (sheet-level writer)           — depends on Steps 1, 4
  |
Step 8  (_sheet_job modification)      — depends on Step 7
  |
Step 9  (write_xlsx modification)      — depends on Steps 7, 8
```

**Recommended implementation order:** 0 → 5 → 6 → 10 → 1 → 2 → 3 → 4 → 7 → 8 → 9

---

## Risks and Edge Cases

### R1: Column count explosion
With N_traces=10, N_subcols=6, gap=5: `4 + 10*(6+5) + 2*6 = 4 + 110 + 12 = 126 cols`.
With N_traces=20: `4 + 20*11 + 12 = 236 cols`. Excel max = 16384. Safe but sheets get wide.

### R2: Missing sub-column data for some (repl, pf) combos
Not all (repl, pf, model) combos may have data. Writer must handle missing gracefully (empty cell). The `.get()` chain in Step 4 handles this.

### R3: `_write_triple_avg_gm` pattern with many explicit cell refs
Non-contiguous GM/AVG formulas with 10+ traces × 6 subcols = formulas with 60+ terms. Excel handles this but formulas get long. No functional issue, just string length.

### R4: Parallel execution and pickling
`_write_transposed_ipc_sheet` uses FULL data dict (not per-repl slice). This data is larger. `ProcessPoolExecutor` pickles it. For typical datasets (few hundred keys) this is fine. If data grows huge, consider passing data path and loading inside worker. Not a concern for Phase 1.

### R5: Speedup denominator alignment
Critical correctness issue: the Base sub-column MUST be at `si=0` in `subcols`. The `_subcol_sort_key` guarantees `NO_MODEL_TOKEN` (="no", hermes="off") sorts first. Verify this with an assertion in `build_sheets_transposed`:
```python
assert sh["subcols"][0] == NO_MODEL_TOKEN, f"Base must be first subcol, got {sh['subcols'][0]}"
```

### R6: Backward compatibility
Old `_write_single_block`, `_write_metric_sheets`, `_iter_groups`, `build_sheets` — KEEP ALL. They're used by other metric sheets (MPKI, LPM, etc. still use per-repl layout). Only IPC changes in Phase 1.

### R7: `_model_hermes` return value for Base
`_model_hermes("no","no","no","off")` returns `"no"` (the base string = `NO_MODEL_TOKEN`). This is what gets stored in `subcols[0]`. The short name mapping ("no" → "Base") must handle this exact string.

### R8: Repl not in `build_sheets()` output
Current `build_sheets()` discards `repl` from keys (because data is pre-filtered). `build_sheets_transposed()` must INCLUDE `repl` in the combined dict key. Cannot reuse `build_sheets()` — need a new function.

### R9: Core count section separators
Requirements say "larger gap — section separator" between different core counts. Implementation: detect when next group's `cores` differs from current, insert 3 blank rows instead of 1.

### R10: Sheet name collision
Old code creates `IPC_lru`, `IPC_hawkeye`, etc. New code creates single `IPC` sheet. If other code references sheet names by pattern, update accordingly. The `_sname` helper won't be used for IPC anymore.

---

## Functions Summary Table

| # | Function | Action | Location |
|---|----------|--------|----------|
| 0a | `REPL_ORDER` | ADD constant | near L882 |
| 0b | `_subcol_sort_key(mc)` | ADD | near L885 |
| 0c | `_pf_row_sort_key(pf)` | ADD | near L885 |
| 1 | `build_sheets_transposed(data, metric_key)` | ADD | after `build_sheets` L676 |
| 2 | `_write_transposed_header(ws, row, ...)` | ADD | near L838 |
| 2h | `_subcol_short_name(mc)` | ADD helper | near L838 |
| 3 | `_transposed_gm_avg_formulas(ws, row, ...)` | ADD | near L765 |
| 4 | `_write_transposed_ipc_block(ws, sh, start_row)` | ADD | after L1001 |
| 5 | `_speedup_formula_transposed(src_cl, src_row, base_cl, base_row)` | ADD | near L853 |
| 6 | `HEADER_COLS_TRANSPOSED` | ADD constant | near L692 |
| 7 | `_write_transposed_ipc_sheet(wb, data, sheet_name)` | ADD | after L1014 |
| 8 | `_sheet_job()` | MODIFY — add ipc_transposed branch | L1416 |
| 9 | `write_xlsx()` | MODIFY — IPC uses full data, single sheet | L1445 |
| 10 | `_apply_group_conditional_formatting()` | MODIFY — widen probe range | L1374 |

**No existing functions are removed or renamed.** All changes are additive + 2 small modifications.

---

## Gap / Contradiction Analysis vs Requirements Doc

1. **Req says "5 gap columns between trace groups"** — Confirmed, accounted for in layout math.
2. **Req says PF order: no-pf FIRST** — Current code puts no-pf LAST (`base_sort_key`). New `_pf_row_sort_key` reverses this. Old sort key preserved for backward compat.
3. **Req says "Dynamic sub-column discovery"** — Covered by `_subcol_sort_key` + collecting unique model_combos.
4. **Req says GM/AVG per sub-column across traces** — Covered by Step 3 (non-contiguous refs).
5. **Req mentions HOSC/Load Coverage (Phases 2-3)** — Not in scope but `build_sheets_transposed` pattern is reusable. Step 1 is designed to be generic enough.
6. **Req mentions conditional formatting "per column, per group"** — Step 10 adjusts probe range. The existing per-column ColorScale approach works for the new layout since groups are still contiguous row ranges.
7. **No contradictions found** between requirements and current codebase.
8. **Missing detail in req**: what happens when a (repl, pf, model_combo) has data for some traces but not all? Answer: empty cell (same as current behavior with SENTINEL_RUNNING → ""). Handled.
9. **Missing detail in req**: exact short name mapping for sub-column headers. Added `_subcol_short_name` helper with sensible defaults.
