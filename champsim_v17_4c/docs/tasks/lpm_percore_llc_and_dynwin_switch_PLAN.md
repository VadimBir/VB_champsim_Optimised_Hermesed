# LPM Per-Core LLC + Dynamic-Window Switch — Execution Plan

**Date:** 2026-04-18
**Target dir:** `/home/cc/champsim_VB/champsim_v10_ByP_L1Fille/`
**Sibling reference:** `/home/cc/champsim_VB/champsim_v10_ByP_L1Fille_DYNAMIC_WIN/`
**Runner to modify:** `/home/cc/champsim_VB/quickByP_rerun_failed_v10.WIP2.sh`

---

## 🚫 HARD RULE — NO FAKING (×4 BY USER DEMAND)

> ### 1️⃣ **NO FAKE DATA. NO FAKE METRICS. NO FAKE LOGS. NO PLACEHOLDER NUMBERS.**
> If a value cannot be measured, **leave it unmeasured and say so**. Do NOT fabricate, interpolate, or guess a value that looks "reasonable." Corrupted simulation output = destroyed research.

> ### 2️⃣ **NO FAKE DATA. NO FAKE METRICS. NO FAKE LOGS. NO PLACEHOLDER NUMBERS.**
> If a build fails, report the real error. Do NOT stub out a function to make it compile. Do NOT return 0 from a tracker that hasn't been implemented. Do NOT comment out a broken block to "get past it."

> ### 3️⃣ **NO FAKE DATA. NO FAKE METRICS. NO FAKE LOGS. NO PLACEHOLDER NUMBERS.**
> Equivalency gate (`./qcheck_consistency.sh`) results must be **real**. Do not report PASS if it did not run or did not pass. Do not skip it. Do not alter thresholds to make it pass.

> ### 4️⃣ **NO FAKE DATA. NO FAKE METRICS. NO FAKE LOGS. NO PLACEHOLDER NUMBERS.**
> Every run in the bulk runner must produce a **real log from a real binary**. Do NOT skip runs silently, do NOT copy prior logs under new names, do NOT emit summary rows not backed by completed sims. If a binary fails to build, the runner must halt and report — not fabricate output for it.

**Violation of any of the above invalidates the entire simulation batch. Zero tolerance.**

---

## Context Recall — Bypass Fundamentals (verified)

1. Bypass does NOT add L2 traffic — alpha(2) identical w/ or w/o bypass.
2. Bypass only affects L1 (skip MSHR alloc, skip fill on return).
3. Reservation guards TIMING burst, not volume.
4. L2 MSHR cascade is the catastrophic failure mode.
5. Wrong bypass decisions are free (overlap miss bypass = zero downside).
6. **NO hardcoded thresholds** — all decisions derive from MSHR state + paper metrics.

---

## Task Breakdown

### Task A — Per-core LLC LPM tracker (gated by `TRACKER_LPM_SHARED`)

**Current state** (`src/cache.cc:715-725`):
```cpp
if (cache_type == IS_LLC) {
    /* LLC is shared — tick lpm for every CPU that completed warmup */
    for (uint32_t c = 0; c < NUM_CPUS; c++) { ... lpm_operate(c, ...); }
} else {
    lpm_operate(cpu, ...);
}
```
Storage already per-core: `LPM_Tracker lpm[NUM_CPUS][LPM_NUM_TYPES]` (cache.cc:16).

**Define flag — `TRACKER_LPM_SHARED`** (declared in `inc/champsim.h`, **co-located adjacent to `LPM_DYNAMIC_WIN`**):
- **Defined (default ON for back-compat):** preserve current shared LLC behavior — tick all CPUs with global flags. This is the existing code path.
- **Undefined (per-core mode):** new path — split `hit_active`/`miss_active`/`has_byp` per CPU based on each MSHR entry's `cpu` field; tick `lpm_operate(c, ..., hit_active[c], miss_active[c], α_c, has_byp[c], ...)`.

**Wrap in `cache.cc` LLC branch:**
```cpp
if (cache_type == IS_LLC) {
#ifdef TRACKER_LPM_SHARED
    /* shared-LLC path — original behavior */
    for (uint32_t c = 0; c < NUM_CPUS; c++) { ... lpm_operate(c, hit_active, miss_active, α_c, has_byp, ...); }
#else
    /* per-core LLC path — new behavior */
    bool hit_active_pc[NUM_CPUS]  = {false};
    bool miss_active_pc[NUM_CPUS] = {false};
    bool has_byp_pc[NUM_CPUS]     = {false};
    /* second MSHR walk OR refactored single walk that buckets by entry.cpu */
    for (uint32_t c = 0; c < NUM_CPUS; c++) {
        if (!warmup_complete[c]) continue;
        lpm_operate(c, cache_type, hit_active_pc[c], miss_active_pc[c], α_c, has_byp_pc[c], load_α_c, load_miss_c);
    }
#endif
} else {
    lpm_operate(cpu, ...);   /* L1/L2 unchanged */
}
```

**Change required:** LLC must tick a **per-core** LPM using per-core `α`, `load_α`, `load_miss`, **and** per-core `hit_active`/`miss_active`/`has_byp` — but ONLY when `TRACKER_LPM_SHARED` is undefined.

**Verify first (before editing):**
- Q-A1: Do MSHR entries carry `cpu` field reliably at LLC? (Grep `MSHR.entry[i].cpu` in LLC path.)
- Q-A2: Are `hit_active` / `miss_active` / `has_byp` currently computed globally across all MSHR entries regardless of CPU? If yes, we must split them per-CPU at LLC.

**Implementation sketch (only after Q-A1/A2 answered):**
- Replace the scalar `miss_active`/`hit_active`/`has_byp` with arrays `[NUM_CPUS]` in the LLC branch.
- Walk MSHR once, bucket each entry's liveness/bypass flag into the entry's `cpu` index.
- In the tick loop, pass the per-core flags: `lpm_operate(c, cache_type, hit_active[c], miss_active[c], α_c, has_byp[c], load_α_c, load_miss_c)`.
- Leave non-LLC path untouched.

**Invariants (enforce):**
- No cross-core contamination in LLC LPM.
- Non-warmup cores still skipped.
- Existing L1/L2 path UNCHANGED (separate `if` branches, no shared mutation).

---

### Task B — Dynamic-Window LPM via `#define` switch

**Goal:** Single source tree `champsim_v10_ByP_L1Fille` produces:
- `champsim_v10_ByP_L1Fille` (fixed-win, default)
- `champsim_v10_ByP_L1Fille_DYNAMIC_WIN` (dynamic-win, when define active)

**Define name/location:** `LPM_DYNAMIC_WIN` in `inc/champsim.h` — **placed adjacent to `TRACKER_LPM_SHARED` (Task A)** so both LPM-tracker switches sit in one block in the header for easy review.
- Default: commented out.
- **Toggle mechanism: HEADER-ONLY.** No `-D` compiler flag, no env var, no sed of .cc files. The runner edits `inc/champsim.h` to comment/uncomment the define, then rebuilds.
- Source of truth is `inc/champsim.h`. Period.

**Header block layout (in `inc/champsim.h`):**
```cpp
/* ===== LPM Tracker Switches ===== */
#define TRACKER_LPM_SHARED       /* ON = LLC shared (orig); OFF = LLC per-core */
// #define LPM_DYNAMIC_WIN       /* ON = dynamic window LPM; OFF = fixed window */
/* ================================ */
```

**Source of dynamic-window tracker:** `champsim_v10_ByP_L1Fille_DYNAMIC_WIN/inc/lpm_tracker.h` (17.1K vs current 15.7K).

**Discovery required BEFORE merge:**
- Q-B1: Diff both `lpm_tracker.h` files. What are the additions for dynamic window? (Bucket sizing? Window adapt function? State fields?)
- Q-B2: Does dynamic version keep the same public API (`lpm_operate`, fields read by cache.cc)? If API diverges, `#ifdef` must wrap call-sites in `cache.cc`, not just the header.
- Q-B3: Any `.cc` support file for dynamic-win? (grep for `lpm_tracker.cc` in sibling.)

**Merge strategy:**
- Option 1 (preferred): fold both versions into a **single `lpm_tracker.h`** with `#ifdef LPM_DYNAMIC_WIN ... #else ... #endif` around divergent blocks.
- Option 2 (fallback): two headers, `#include` selected by define.

**Binary suffix wiring:** Check if existing Makefile/build script already produces suffix via define. If not, build script must emit `_DYNAMIC_WIN` suffix when `LPM_DYNAMIC_WIN` is set.

**NO FAKING:** If dynamic-win code references symbols not present in current tree, **DO NOT STUB**. Report and ask.

---

### Task C — Bulk runner `quickByP_rerun_failed_v10.WIP2.sh`

**Constraint:** Edits go to `.WIP2.sh` copy; do NOT touch the live runner.

**Required change:** Outer loop (or wrap around innermost existing loop) iterating over:
```
for WIN_MODE in fixed dynamic; do
    # Header-only toggle: sed the define in inc/champsim.h in-place, then build.
    if [[ $WIN_MODE == dynamic ]]; then
        BIN_SUFFIX="_DYNAMIC_WIN"
        sed -i 's|^// *#define LPM_DYNAMIC_WIN|#define LPM_DYNAMIC_WIN|' inc/champsim.h
    else
        BIN_SUFFIX=""
        sed -i 's|^#define LPM_DYNAMIC_WIN|// #define LPM_DYNAMIC_WIN|' inc/champsim.h
    fi
    # build_key MUST include $BIN_SUFFIX so bin_cache does not collide.
    # OPTIONAL second axis (per-core LLC):
    # toggle TRACKER_LPM_SHARED in inc/champsim.h the same way; add to BIN_SUFFIX.
done
```

**Delegation to Haiku (token efficiency — user explicit demand):**
- User confirmed: the WIP2 script is huge. A **Haiku subagent** must read it end-to-end, map its existing loop structure, and return a **minimal diff proposal**: where the WIN_MODE loop best fits, how the build step keys on the define, and how output dirs are disambiguated to avoid overwrite.
- The Haiku proposal is **advisory**. Main agent reviews, then applies edits to `.WIP2.sh` (NOT live).
- **NO FAKING:** Haiku must not invent script structure. If uncertain, it reports "unclear" rather than guessing.

**Output path disambiguation:** Every run's log/output dir must include the WIN_MODE tag. Otherwise dynamic-win logs overwrite fixed-win logs → corrupted batch.

---

## Stage Gates

| Stage | Gate |
|-------|------|
| A done | `./qcheck_consistency.sh` — both tests PASS. Real run, real output. |
| B done | Both binaries build cleanly. Fixed-win binary byte-reproduces prior results on smoke trace (or documents delta with reason). Dynamic-win binary produces plausibly-different LPM stats (sanity check; do NOT fake equivalence). |
| C done | Dry-run of `.WIP2.sh` with `set -x` prints 2× the invocation count of the prior version, with distinct output paths. No live batch started until user reviews dry-run. |

---

## Deliverables

1. This plan file (you are reading it).
2. Haiku subagent report on `quickByP_rerun_failed_v10.WIP2.sh` structure + proposed loop injection point.
3. Answers to Q-A1, Q-A2, Q-B1, Q-B2, Q-B3 before any code edit.
4. Patch to `src/cache.cc` for per-core LLC LPM (Task A).
5. Unified `inc/lpm_tracker.h` with `LPM_DYNAMIC_WIN` switch + define in `inc/champsim.h` (Task B).
6. Edited `quickByP_rerun_failed_v10.WIP2.sh` with WIN_MODE loop (Task C).
7. Real qcheck PASS logs for fixed-win. Real build+smoke logs for dynamic-win.

---

## Runner Findings (from Haiku, read-only map of `quickByP_rerun_failed_v10.WIP2.sh`)

**Top-level loops:**
- 1177: PHASE 1 `while IFS= read` over RERUN_CONFIG_FILE
- 1197: PHASE 2 `for _arch in ARCH_LIST`
- 1199: `for _cs_dir in CHAMPSIM_DIRS`  ← **injection point**
- 1209: `for cores in CORE_LIST`
- 1210: `for pf_spec in PREFETCH_LIST`
- 1211: `for trace in TRACE_LIST`
- 1212: `for model_name in MODEL_LIST`
- 1213: `for _mult in _mult_list`
- 1249: PHASE 3 drain loop

**Build step:**
- 862 / 868: `make -j"$BUILD_JOBS" run_clang`
- 921: `build_key="$(sanitize ...)"` — **does NOT include WIN_MODE / CFLAGS** → collision risk
- 922: `cache_dir="${BIN_CACHE_ROOT}/${build_key}"`
- 923: `cached_bin="${cache_dir}/champsim"`
- 801: inner `build_binary_for_cfg` build_key DOES include extras (discrepancy)

**Run invocation:**
- 1028-1031: `"$cached_bin" -warmup ... --bypass ... -traces ...`
- 984: `logfile="${out_dir}/${_arch}-${CURRENT_CHAMPSIM_IDX}-..."` — safe per-idx

**Recommended injection:** wrap lines **1199–1244** with `for WIN_MODE in fixed dynamic; do ... done`.

**Risks to fix:**
1. **Line 921 `build_key`** MUST be patched to include WIN_MODE suffix, else `-DLPM_DYNAMIC_WIN` and non-dynamic builds cache to the same key → silent binary reuse → corrupted results. **This is a NO-FAKING critical item.**
2. `BIN_CACHE_ROOT` shared across modes — acceptable once build_key is patched.
3. **CFLAGS injection — RESOLVED:** NO CFLAGS. Header-only toggle via sed on `inc/champsim.h` before each build. Single source of truth.

---

## STOP Conditions

- Any Q-* unresolvable from code → stop, report, await.
- qcheck FAIL → stop, report exact diff, do NOT advance.
- Haiku returns guesswork instead of grounded mapping → re-prompt, do not build on it.
- Any temptation to fabricate a value, log, or PASS → **STOP**. Re-read the four-times rule above.
