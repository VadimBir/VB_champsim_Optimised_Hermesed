# Hawkeye Methodology Comparison

## Core Question: Is the methodology basically the same?

**Yes, the core methodology is identical.** Both implementations use Hawkeye's three-pillar approach: a per-PC predictor trained via OPTgen, RRIP-based victim selection, and per-set occupancy tracking to distinguish cache-friendly from cache-adverse accesses.

## Key Findings

**Matching methodology:**
- Both use an OPTgen oracle to simulate optimal cache decisions and train a predictor based on whether the optimal policy would have hit
- Both use RRIP values (0 = cache-friendly, maxRRPV = cache-adverse) determined by predictor output
- Both scan for max-RRIP victims first, falling back to highest RRIP if no max exists
- Both track per-access history and occupancy vectors per cache set to detect reuse patterns

**Implementation differences (not methodological):**
1. **Code architecture:** HPCA_23 uses monolithic functions in cache.cc; Hermes uses object-oriented classes (HawkeyeRepl, HawkeyePred, OPTgen)
2. **Predictor training:** HPCA_23 manually manages per-PC history with separate demand/prefetch predictors; Hermes uses unified confidence counters with configurable hashing
3. **Prefetch handling:** HPCA_23 tracks prefetch status separately and trains different predictors; Hermes subsumes this into a single prediction mechanism
4. **Statistics tracking:** Both collect stats, but Hermes uses formal struct-based accounting while HPCA_23 uses local variables

The Hermes version is a cleaner refactor with better modularity, but implements the exact same Hawkeye algorithm.
