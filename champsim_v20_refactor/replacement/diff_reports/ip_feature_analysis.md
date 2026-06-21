# IP (PC) Feature Feasibility Analysis for Model 520a

## Context

Model 520a (`l1d_bypass_operate`) uses two aggregate, per-CPU-level metrics:
- Gate 1: `κ_short > κ_long` (kappa from LPM_L1D)
- Gate 2: `κ_reduce% >= C-AMAT(L2)/CPA(L2)`

Both signals are **cache-level aggregates** — no per-access or per-PC granularity.
The bypass decision point is `handle_read_miss_bypass()` in cache.cc, called with the RQ entry at index.
The packet (`RQ.entry[index]`) carries `ip`, `instr_id`, `l1_bypassed`, `l2_bypassed`, `llc_bypassed`.
**IP is available at decision time** — `RQ.entry[index].ip` is set before bypass check.

## Key Constraint: No MSHR on Bypass

When bypass fires, `lower_level->add_rq()` is called and the function returns `true`, skipping MSHR insertion entirely. Training must happen on the **fill-return path** (`handle_fill_processed_and_bypass_return`), where the returning packet still carries the bypass flags and `ip`.

---

## Option A: PC-Indexed Bypass History Counter

### Mechanism
- Table: `uint8_t byp_hist[1<<IP_BITS]` — saturating counter per PC bucket (All cache are CPU core specific obj)
- At bypass decision: look up `byp_hist[ip_hash(ip)]`
  - Counter high → this PC historically misses after bypass → suppress or weight bypass
  - Counter low → this PC benefits from bypass → allow
- On fill-return (bypass path): increment or decrement counter based on whether the fill was useful (check `l1_bypassed` flag on returning packet)

### Feasibility: YES
- IP is available at both decision point and fill-return point
- No MSHR required — side table indexed by IP hash
- Hardware cost: 256-entry × 2-bit saturating counter = 64 bytes. Negligible.
- Integration: add as Gate 0 (pre-filter) or multiplicative modifier to `k_reduce_pct`
- Training signal: on fill-return in `handle_fill_processed_and_bypass_return`, check `pkt.l1_bypassed` → if bypass was taken, update counter based on fill latency vs threshold

### Risk
- Hash collisions across PCs (aliasing) — mitigated by 8–12 bit hash
- Cold start: counters initialize neutral, converge after ~256 misses per PC
- Does NOT become SDBP: SDBP uses PC-indexed dead-block prediction at LLC fill time; this is a bypass-history counter at bypass-decision time, orthogonal

---

## Option B: PC-Indexed Latency Predictor

### Mechanism
- Table: `uint16_t lat_pred[NUM_CPUS][1<<IP_BITS]` — exponential moving average of fill latency per PC
- At bypass decision: `predicted_lat[ip_hash(ip)] > threshold → bypass`
- On fill-return: update EMA with `(current_cycle - issue_cycle)` — but issue_cycle must be stored somewhere since MSHR is skipped

### MSHR Problem
Without MSHR, `issue_cycle` is lost. The bypassed packet's timestamp is not tracked after `add_rq()`. Two solutions:
1. **Bypass side-table** (~256 entries, 12 bytes each): store `{ip_hash, issue_cycle}` indexed by address tag. On fill-return, match by address → recover issue_cycle → compute latency → update predictor.
2. **Approximate**: use `RQ.entry[index].event_cycle` as issue proxy (set when packet enters RQ). Fill-return cycle is `current_core_cycle[cpu]`. Delta is valid approximation.

### Feasibility: YES, with side-table
- Side-table: 256 × 12 bytes = 3KB. Acceptable HW cost.
- Latency predictor table: 256 × 2 bytes = 512 bytes.
- Total: ~3.5KB per CPU.
- This IS the B2 perceptron design (latency predictor as one feature). Can be a standalone lightweight version without perceptron.

### Risk
- Side-table eviction policy needed (FIFO or LRU on 256 entries)
- Bypassed LLC misses go to DRAM — latency ~100–200 cycles, high variance. EMA smoothing needed (α ≈ 0.125).
- Does NOT become SDBP: SDBP predicts dead blocks at LLC; this predicts fetch latency at L1/L2 decision time.

---

## Option C: CARE-Style Miss Contribution per PC

### Mechanism
CARE (see `care.llc_repl`) tracks per-line reuse via `SHCT[cpu][sig]` where `sig = ip % SHCT_SIZE`. The contribution score is:
- `SHCT[cpu][sig].counter` — saturating up/down counter: increments on reuse, decrements on eviction-without-reuse
- `SHCT[cpu][sig].pmcl` — PMC-level counter: tracks memory cost pressure per PC signature

CARE's bypass decision equivalent: if `SHCT[cpu][sig].counter == 0` → install with maxRRPV (effectively dead on arrival = bypass candidate).

### Adapting to Bypass Decision (L1/L2)
- At bypass decision, `ip` is known → compute `sig = ip % SHCT_SIZE` (or smaller table)
- Look up `SHCT[cpu][sig].counter`: if 0 → high bypass confidence; if 7 → suppress bypass
- SHCT is already maintained by LLC replacement — **no additional training infrastructure needed** if we share the table
- If L1/L2 bypass is separate from LLC replacement: maintain a smaller L1-specific SHCT (~1K entries = 2KB)

### MSHR dependency: NONE
CARE's SHCT is updated in `llc_update_replacement_state()` (on fill + eviction), not in MSHR. Fill-return path updates it. Bypass path does not need MSHR for this.

### Feasibility: YES — lowest implementation cost of the three
- Can read from existing LLC SHCT table (zero new HW if sharing)
- Or add small L1-level SHCT (2KB per CPU)
- Training: piggyback on existing `llc_update_replacement_state` updates
- Integration into 520a: add Gate 0: `if (SHCT[cpu][ip%SHCT_SIZE].counter == 0) → force bypass`; `if counter == 7 → suppress bypass`

### Risk
- SHCT is LLC-level trained; may not reflect L1 reuse patterns accurately for all workloads
- `pmcl` field in CARE tracks PMC level (memory cost) — directly usable as bypass cost signal

---

## Summary Table

| Option | MSHR needed | HW cost | Training path | Risk | Recommended |
|--------|-------------|---------|---------------|------|-------------|
| A: Bypass history counter | No | 64 bytes | fill-return bypass flag | Aliasing | Yes — start here |
| B: Latency predictor | Side-table only | ~3.5KB | fill-return cycle delta | Side-table mgmt | Yes — with B2 perceptron |
| C: CARE SHCT reuse | No | 0 (shared) or 2KB | LLC replacement update | LLC-level proxy for L1 | Yes — immediate win |

---

## Recommended Integration Order

1. **C first**: Read `SHCT[cpu][ip%SHCT_SIZE].counter` at L1 bypass decision — if 0, boost bypass confidence (modify Gate 2 threshold); if 7, suppress. Zero new infrastructure.
2. **A second**: Add `byp_hist[cpu][ip_hash]` 2-bit counter, train on fill-return bypass flag. Gate 0 pre-filter.
3. **B third**: Add latency predictor with side-table (feeds into B2 perceptron design).

None of these require MSHR. All train on fill-return path where `l1_bypassed`/`l2_bypassed` flags are preserved.

## IP Hash Recommendation

`ip_hash = ((ip>>1) ^ (ip>>4)) & 0xFF` (8-bit = 256 buckets) for A/B tables.
For CARE SHCT sharing: `ip % SHCT_SIZE` (14-bit, 16K entries) — already defined.
Test full IP first for best-case ceiling, then reduce to find minimum viable bits.
