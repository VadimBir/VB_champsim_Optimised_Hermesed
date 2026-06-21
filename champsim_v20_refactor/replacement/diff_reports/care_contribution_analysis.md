# CARE Contribution Methodology — Bypass Decision Integration Analysis

## Source: care.llc_repl

CARE is a SHiP++ variant with two extensions:
1. `SHCT[cpu][sig].counter` — standard SHiP reuse counter (3-bit saturating, 0=dead, 7=live)
2. `SHCT[cpu][sig].pmcl` — PMC-level counter (3-bit saturating, tracks memory cost of evicted lines)
3. `block[set][way].pmc` — per-cache-line PMC value (memory cost at insertion time)
4. `thresh` — dynamic threshold separating low/high PMC lines (adapts every `miss_period` accesses)

---

## How CARE Computes Contribution Score

### Per-line PMC
`block[set][way].pmc` is a **pre-existing field** on BLOCK. In CARE it is read at eviction time to classify the line's memory cost into `pmc_level` (0, 1, or 3):
- `pmc_level = 0` → low memory cost (< thresh/7 range) → SHCT pmcl decremented
- `pmc_level = 3` → high memory cost (> thresh range) → SHCT pmcl incremented

The `pmc` field represents **how much memory pressure this line's miss contributed** — it IS the contribution score.

### SHCT Signature Update (miss-contribution training)
On eviction from a leader set:
```
if line_reuse[set][way] == FALSE:
    SHCT[cpu][sig].counter = SAT_DEC(counter)   // penalise: dead block, no contribution
else:
    SHCT[cpu][sig].counter = SAT_INC(counter)   // reward: reused, contributed

if pmc_level == 0:
    SHCT[cpu][sig].pmcl = SAT_DEC(pmcl)         // low cost PC → less pressure
elif pmc_level == 3:
    SHCT[cpu][sig].pmcl = SAT_INC(pmcl)         // high cost PC → more pressure
```

### Insertion Policy (bypass-equivalent in replacement domain)
```
if SHCT[cpu][new_sig].counter == 0:
    line_rrpv = maxRRPV   // dead-on-arrival → evict soonest (≡ bypass in repl domain)
elif SHCT[cpu][new_sig].counter == 7:
    line_rrpv = 0 or 1    // highly reused → keep
else:
    pmcl == 0 → rrpv=6    // low pressure + uncertain reuse → near-dead
    pmcl == 7 → rrpv=2    // high pressure + uncertain reuse → keep longer
    else       → rrpv=4   // middle
```

**The pmcl dual-signal is the key CARE contribution**: reuse alone doesn't determine insertion priority — memory cost (pressure contribution) also matters.

---

## Adapting CARE Contribution to Bypass Decision

### What We Need at Bypass Decision Time
At `handle_read_miss_bypass()`, we have:
- `RQ.entry[index].ip` → compute `sig = ip % SHCT_SIZE` (or smaller table)
- `cpu` index
- Cache type (IS_L1D / IS_L2C / IS_LLC)

SHCT is already populated by LLC replacement updates. We can **read it at L1/L2 bypass decision time** — this is the CARE contribution shortcut: zero additional training infrastructure.

### Bypass Predicate Using CARE Signals

```cpp
uint32_t sig = RQ.entry[index].ip % SHCT_SIZE;

// Contribution score interpretation:
// counter=0 + pmcl=0 → dead AND cheap → strong bypass candidate
// counter=0 + pmcl=7 → dead AND expensive → bypass (save pressure on upper cache)
// counter=7           → live → suppress bypass
// counter mid, pmcl=0 → uncertain reuse, low cost → lean bypass
// counter mid, pmcl=7 → uncertain reuse, high cost → lean no-bypass (costly to re-fetch)

bool care_bypass = false;
if (SHCT[cpu][sig].counter == 0) {
    care_bypass = true;    // dead-on-arrival per historical PC behaviour
} else if (SHCT[cpu][sig].counter < 4 && SHCT[cpu][sig].pmcl <= 4) {
    care_bypass = true;    // weakly reused, moderate cost → bypass saves pressure
}
bool care_suppress = (SHCT[cpu][sig].counter >= 6);  // strongly live → never bypass
```

This can be integrated into 520a as:
- **Gate 0** (pre-filter): if `care_suppress` → return false (override κ decision)
- **Gate 3** (boost): if `care_bypass` → lower the `next_pressure` threshold by 20%, making bypass easier to trigger

---

## MSHR Dependency: NONE

CARE's SHCT is updated in `llc_update_replacement_state()` — called on every LLC fill and hit. This is **not MSHR-dependent**. The bypass path also reaches LLC (bypassed packets go to lower_level → LLC → DRAM), so SHCT will eventually be trained even for bypassed accesses when they fill into LLC.

For L1-bypassed accesses:
- Packet goes to L2 → misses → LLC → fills into LLC → `llc_update_replacement_state` called → SHCT[sig] updated
- The `ip` field is preserved through the fill chain
- Training signal is valid even for bypassed packets

---

## PMC Field Availability

`block[set][way].pmc` is a BLOCK field. To use PMC **at bypass decision time** (before insertion), we need the PMC of the **line being evicted to make room** — i.e., the victim's PMC, which requires `llc_find_victim()` to be called first. This is NOT available at L1/L2 bypass decision time without a dummy find-victim call.

**Resolution**: Do NOT use per-line PMC at bypass decision. Use only SHCT counters (counter + pmcl), which are PC-indexed and available immediately from IP.

---

## HW Cost

| Component | Size | Note |
|-----------|------|-------|
| Shared LLC SHCT (existing) | 16K × 2 × 3-bit = 12KB/CPU | Already exists in care.llc_repl |
| L1-specific mini-SHCT | 256 × 2 × 3-bit = 192 bytes/CPU | If separate table needed |
| PMC threshold (thresh) | 1 × uint32_t = 4 bytes | Global |
| Dynamic level bounds | 14 × uint32_t = 56 bytes | Already in care.llc_repl |

**If sharing the LLC SHCT**: 0 additional bytes. Read-only access from bypass decision code.
**If L1/L2 separate mini-SHCT**: ~200 bytes per CPU. Negligible.

Recommended: share LLC SHCT initially. If aliasing is a problem (different PC access patterns at L1 vs LLC), add mini-SHCT trained separately on L1 bypass returns.

---

## M-CARE MLP Extension

M-CARE adds an MLP cost model on top of CARE's `pmcl`. The MLP predicts fill cost (memory pressure contribution) more accurately using multiple features. For bypass:
- M-CARE's cost prediction = estimated contribution if line is kept in cache
- High predicted cost → keeping line is expensive → strong bypass candidate
- This is exactly the latency/cost predictor in B2

M-CARE's MLP output can replace `pmcl` in the bypass predicate above, at higher HW cost (MLP weights ~several KB).

---

## Summary

| Question | Answer |
|----------|--------|
| Can we compute per-access contribution score at bypass decision? | **Yes** — read `SHCT[cpu][ip%SHCT_SIZE]` (counter + pmcl) |
| HW cost | **0 bytes** if sharing LLC SHCT; ~200 bytes for mini-SHCT |
| Needs MSHR? | **No** — SHCT trained on replacement path, not MSHR |
| Training path for bypass | LLC fill/eviction updates SHCT with ip; bypassed packets still reach LLC |
| Best integration into 520a | Gate 0 suppress (counter≥6) + Gate 3 threshold reduction (counter=0) |
| PMC per-line usable at decision time? | **No** — requires victim lookup before decision |
| M-CARE MLP usable? | Yes, as richer cost signal replacing pmcl; higher HW cost |
