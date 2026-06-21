# SHIP LLC Replacement Policy Comparison

## Question: Is the methodology basically the same?

**Answer:** Yes, the core SHIP methodology is identical. Both implementations use the same sampler-based prediction approach: a small sampler (256 sets per CPU) tracks reuse patterns via a Sampler History Counter Table (SHCT), and this prediction informs RRPV (Re-Reference Prediction Value) assignment on cache misses. The algorithms are methodologically equivalent.

## Meaningful Differences

1. **Extra function in Hermes:** `llc_replacement_print_config()` stub (lines 97-99) — not present in champsim_VB. This is a non-functional hook for config printing.

2. **Comments:** Hermes adds one extra comment ("// print config") before the new function; champsim_VB omits this entire function. No algorithmic impact.

3. **Code structure identical:** Both have identical sampler initialization, update logic, victim selection, and replacement state updates. Function signatures, macro defines (#define maxRRPV, SHCT_SIZE, etc.), class definitions (SAMPLER_class, SHCT_class), and all core logic match exactly.

**Conclusion:** The implementations are effectively identical in methodology and core function. The Hermes version includes one extra stub function, but this does not alter the SHIP algorithm itself.
