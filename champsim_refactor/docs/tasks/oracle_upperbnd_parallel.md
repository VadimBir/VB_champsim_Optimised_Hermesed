# Oracle Upper Bound — Parallel Task
## Goal: Compute max possible IPC gain from bypass (theoretical ceiling)

---

## WHAT WE WANT

An **upper bound IPC** on bypass performance — without hardcoded thresholds — to know:
1. Is bypass even worth pursuing for a given trace?
2. What is the maximum achievable IPC delta?
3. Which misses SHOULD have been bypassed (oracle label for model training)?

---

## ORACLE DEFINITION

A miss at level l **should have been bypassed** if:
```
time_of_next_access_to_same_address - time_of_miss > eviction_time(l)
```
i.e., the block would be evicted before it's reused anyway.
If evicted before reuse → caching it was WRONG → bypass was correct.

Equivalently (from the paper):
A miss should bypass if it would become a **pure miss access** — i.e., caching it adds to `m` cycles
without reducing future `m` cycles (because the block won't be a hit before eviction).

---

## IMPLEMENTATION PLAN

### Stage 0: 
Dup the /home/cc/champsim_VB/champsim_v10 DIRECTORY RECURSIVELY, AND WORK INSIDE IT UNDER INC AND SRC FOLDERS
### Stage 1: Capture Access Timings

In `cache.cc`, for each load access to L1D/L2C/LLC:
```cpp
// On any access (hit or miss) to level l:
access_log[l].push_back({
    .addr     = block_addr,
    .cycle    = current_cycle,
    .is_hit   = (hit or miss),
    .mshr_occ = MSHR.occupancy,
    .rq_occ   = RQ.occupancy
});
```

### Stage 2: Post-Processing — Reuse Distance Analysis

After simulation completes, for each level-l miss:
```python
for each miss[i] at address A at cycle C:
    next_access = find_next_access(A, after=C)
    eviction_cycle = estimate_eviction_cycle(A, C, level=l)
    
    if next_access > eviction_cycle:
        should_bypass[i] = True   # block evicted before reuse
    else:
        should_bypass[i] = False  # block reused before eviction
```

### Stage 3: Oracle IPC Simulation

Re-run simulation with oracle bypass policy:
- Bypass any miss where `should_bypass[i] == True`
- This gives the **maximum achievable IPC** with perfect knowledge

### Stage 4: IPC Upper Bound Report

Output per trace:
```
Trace:          621.wrf_s-8100B
No bypass IPC:  0.70545
Oracle IPC:     X.XXXXX   ← computed
IPC headroom:   +X.X%
Bypass rate:    XX%        ← fraction of misses that should bypass
```

---

## ALTERNATIVE UPPER BOUND (Simpler, No Replay)

Instead of re-simulation, compute analytically:

```
Oracle_MST = µ × κ_oracle × C-AMAT

where κ_oracle = fraction of miss cycles that CANNOT be overlapped
               = 1 - (φ × APC × MST) clipped to [0,1]
```

Then:
```
IPC_upper_bound = 1 / (CPIexe + fmem × µ × κ_oracle × C-AMAT)
```

This gives the upper bound WITHOUT re-running the simulation.

---

## KEY QUESTION THE ORACLE ANSWERS

> "We do not need threshold — we just need to know if next level can retire given instruction faster than current level"

Oracle answers this per-miss:
- If oracle says bypass → next level WOULD have been faster (or equal, with less pollution)
- If oracle says keep → current level was correct

---

## PHASE STRETCHING NOTE (user observation)

> "Different trace variants can have stretched phases where it literally tanks L2 MSHR completely"

Oracle must track phase boundaries:
- Segment trace into phases (e.g., 1M instruction windows)
- Compute `φ × APC × MST` per phase
- Oracle bypass rate will vary per phase

Output: phase-level bypass-rate heatmap per trace.

---

## STATUS

- [ ] Stage 1: Access timing capture (needs code in cache.cc)
- [ ] Stage 2: Post-processing script (Python)
- [ ] Stage 3: Oracle replay or analytical upper bound
- [ ] Stage 4: Report generation

## APPROVAL GATE

**STOP — awaiting user approval before implementing code.**
