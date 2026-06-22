# Comparison: drrip.llc_repl (champsim_VB vs Hermes)

## Answer
Yes, the methodology is basically the same. Both implement Dynamic RRIP (DRRIP) using identical core logic: sampler sets to track BIP vs SRRIP performance via a policy selector (PSEL), adaptive RRIP value assignment based on leader policies, and victim selection favoring high RRIP values. The algorithms are functionally equivalent.

## Meaningful Differences

**Hermes has an additional function:** `llc_replacement_print_config()` (lines 118–120) at the end, which is absent in champsim_VB. This is a utility stub (currently commented out) for printing configuration and has no impact on the core DRRIP algorithm.

All other code—initialization, sampler set selection, PSEL updates, RRIP adjustments, and victim finding—is identical in both files.
