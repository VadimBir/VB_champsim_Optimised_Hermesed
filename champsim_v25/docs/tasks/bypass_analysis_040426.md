# Bypass Analysis — 2026-04-04 Session
## Context: IIT C-AMAT Framework Applied to Per-Load Bypass Decision

---

## FRAMEWORK RECAP (IIT Paper — Liu, Espina, Sun 2021)

Four cycle types per cache level:
```
e = idle (no memory activity)
h = pure hit cycles  
m = pure miss cycles  ← ONLY THESE CAUSE STALLS
x = mixed hit/miss cycles  ← miss is HIDDEN by concurrent hits
```

Key equations:
```
MST       = m/α = µ × κ × C-AMAT        (eq. 35)
phi (φ)   = (h+x)/ω  ← hit cycle ratio
mu  (µ)   = (m+x)/ω  ← miss cycle ratio
kappa (κ) = m/(m+x)  ← pure miss fraction of miss cycles
1 - φ     = µ × κ    = m/ω  ← pure miss density
IPC       = 1 / (CPIexe + fmem × MST)
LPMR(1)   = IPCexe × fmem × C-AMAT(1)  ← 100% correlated with IPC within a workload
IPC       = IPCexe / (1 + LPMR(1))
```

---

## USER'S TECHNICAL POINTS — ADDRESSED

### Q1: MSHR proxy doesn't work. The issue is predicting NEXT bypass moment.

**Status**: Confirmed by user. MSHR==0 bypass already tried. Works in some cases.

**Root issue**: We don't know if the NEXT miss will be pure or overlapping.
The MSHR occupancy is a lagging indicator — it tells you the CURRENT state, not what the NEW entry will experience.

**The real question**: "Will this new MSHR entry overlap with existing hits, or be isolated (pure miss)?"

---

### Q2: RQ occupancy = Hit cycles definition

**User's insight** (correct from the paper):
```
RQ occupied (at level l+1)  →  those cycles = hit cycles at level l
MSHR occupied (at level l)  →  those cycles = miss cycles at level l
Both RQ and MSHR occupied   →  mixed (x) cycles
```

This is literally the paper's definition:
- `h` = cycles where only RQ activity (hits visiting cache)
- `m` = cycles where only MSHR activity (pure miss stalls)
- `x` = cycles where both RQ hits AND MSHR misses are active simultaneously

**Implication**: To know if a new miss will be pure or overlapped, you need to know if RQ has concurrent hits during the MSHR's service time.

---

### Q3: Probability of overlap — the "Golden Ratio"

**User's formulation** (correct):
> "Over APC accesses that occur over MST time, I will have Latency number of Hit cycles"

**Formalizing this:**
```
Over one stall window (MST cycles), number of memory accesses = APC × MST
Of those, fraction φ are hit-type accesses
→ Expected hit accesses during stall = φ × APC × MST
```

**The overlap criterion:**
```
IF  φ(l) × APC(l) × MST(l) >= 1
    → At least one hit will overlap this miss during its stall window
    → Miss becomes NON-PURE → MST impact = 0 for this access
    → DON'T BYPASS (it's already free)

IF  φ(l) × APC(l) × MST(l) < 1
    → No hits expected to overlap this miss
    → Miss will be PURE → adds directly to total MST
    → BYPASS CANDIDATE
```

This is the stateless, per-load, real-time formula you need.

**Can compute at runtime:**
- `φ` = tracked per-level in your simulator (phi output column)
- `APC` = tracked per-level (APC output column)
- `MST` = tracked per-level (MST output column)
- Product `φ × APC × MST` = dimensionless overlap probability

**Caveat — adjacent bypass restriction:**
- If L1 bypass → L2 MSHR absorbs load
- L2 MSHR can be 10x worse (user confirmed)
- Cannot bypass L2 if L1 bypassed (user's constraint)
- So bypass at L1 only if L2's `φ(2) × APC(2) × MST(2)` indicates it can absorb the miss cleanly

---

### Q4: Bypass "restarts" the process at worse latency

**User's observation** (correct):
> "Bypass restarts the whole process by doing the same on worse latency level"

From the paper's C-AMAT recursion (Eq. 41):
```
C-AMAT(l) = H(l)/CH(l) + ρm(l) × κ(l) × C-AMAT(l+1)
```

Bypassing level l means the miss goes directly to level l+1 with full latency H(l+1).
**Net gain only if:**
```
κ(l) × C-AMAT(l+1) < C-AMAT(l)
```
i.e., the pure miss fraction at level l, multiplied by next-level latency, is less than current level's total C-AMAT.

This is the bypass worth-it condition derived directly from the paper.

---

### Q5: Adjacent level correlation — "pure miss at L2 affects L3 pure miss"

From the paper's observation 3:
```
Pure hit cycles at level l → become INACTIVE cycles at level l+1
ω(l+1) = µ(l) × ω(l)
```

**This means:** The miss concurrency at level l+1 is determined by µ(l). If L1 bypass sends more misses to L2, L2's ω increases, its C-AMAT changes, which propagates to L3.

**The full chain is coupled.** This is why the models need to look at LPMR at each level recursively (paper section 7, eq. 56-58).

---

### Q6: L2 MSHR cap stalls are 10x worse than L1

**Why this happens:**
- L1 MSHR: ~16-32 entries, low latency hits
- L2 MSHR: when full → accesses stall at L2 → κ(2) → 1 → MST(2) spikes
- Since C-AMAT(1) includes ρm(1) × κ(1) × C-AMAT(2), L2 degradation directly hurts L1 C-AMAT

**Implication for bypass:**
- Bypass L1 at most until L2 MSHR reaches some occupancy threshold
- The threshold itself comes from `φ(2) × APC(2) × MST(2) < 1` → stop bypassing when L2 is saturated

---

## BYPASS DECISION FORMULA (PROPOSED)

At the moment of a load miss at level `l`, bypass if:
```
φ(l) × APC(l) × MST(l) < OVERLAP_THRESHOLD
AND
φ(l+1) × APC(l+1) × MST(l+1) > ABSORPTION_THRESHOLD
AND
NOT (l == bypassing_adjacent_level)
```

Where:
- First condition: current miss will likely be pure (not overlapped) → worth bypassing
- Second condition: next level can absorb it without becoming pure there too
- Third condition: adjacent bypass restriction

**All metrics are available at runtime per level in your simulator.**

---

## CORE RESEARCH QUESTION (answered)

**Q: Is it possible to design a bypass policy that adapts to ALL traces naturally via runtime stats?**

**A: Yes.** φ × APC × MST is inherently trace-adaptive:
- Memory-heavy trace (lbm): φ low, MST high → product < 1.0 → aggressive bypass auto-triggered
- Compute-heavy trace (wrf): φ high, APC high → product ≥ 1.0 → minimal bypass auto-triggered
- Threshold = 1.0 is theory-grounded (not hardcoded): "at least one hit access during stall window"

**Remaining hard problems:**
1. Formula uses windowed averages — phase transitions can shift product suddenly
2. L2 MSHR saturation is reactive not predictive (caught by absorption check φ(L2)×APC(L2)×MST(L2) < 1.0)

**Oracle role:** Determines whether the formula matches per-trace oracle labels per phase.
If formula predicts bypass where oracle says bypass → formula is sufficient.
If they diverge → shows exactly where formula fails and why.

## OPEN QUESTIONS (for next model iteration)

1. Should `φ × APC × MST` use instantaneous or windowed values?
2. Window size for phase-adaptive metrics (1M inst? 100K?)
3. Can inter-level bypass (L1+L3, skipping L2) be modeled?
4. How to handle phase transitions when `φ` shifts suddenly?

---

## PARALLEL TASK (see: oracle_upperbnd_parallel.md)

SEPARATE FILE — Oracle upper bound approximation.

---

## MISSING ITEMS (added post sanity check)

### L1 Fill on Bypass Packet — Artifact Check

User notes: currently L1 may NOT be filling on a bypass packet return (possible older artifact).
**Action needed**: Verify in `cache.cc` whether `fill_cache()` is called on bypass packet return path.
If NOT filling → bypass is truly non-caching. This is correct behavior.
If filling → it's a bug (defeats bypass purpose).

**Implication for "pseudo-prefetch" idea:**
- Return data to CPU (upper level)
- THEN issue a pseudo-prefetch to fill L1 with the data anyway
- This replicates original non-bypass behavior but with a different timing model
- Net effect: data gets to CPU at L2 latency, L1 filled async → future accesses hit L1
- This may actually HELP for workloads where reuse exists but first access latency hurts

**Whether to implement**: depends on whether current bypass is purely non-filling.
Check: `ooo_l1_byp_model.cc` return path.

### Absorption Threshold — Quantified

The second condition in bypass formula:
```
φ(l+1) × APC(l+1) × MST(l+1) > 1.0
```
Threshold = 1.0 (not arbitrary). Means: at least ONE hit access will occur during the downstream stall window.
If > 1.0 → next level can absorb the bypass miss as a non-pure miss.
If < 1.0 → bypassing just pushes a pure miss to next level → worse.
