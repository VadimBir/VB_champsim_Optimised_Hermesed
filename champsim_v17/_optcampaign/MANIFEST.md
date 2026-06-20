# champsim_v17 Optimization Re-Implementation Campaign

## BASELINE (locked 2026-06-19)
- Tree: `champsim_v17` (already carries P1, P2, P7/P7c, P14, C4; AddressProxy LIVE)
- Gate: `bin/champsim.exe -warmup_instructions 1000000 -simulation_instructions 5000000 --arch glc --bypass none --pf_l1 no --pf_l2 no --pf_l3 no -traces <Pythia-70M> <Pythia-70M>` (2 core)
- Build: `PATH=w64devkit/bin make -C champsim_v17 -f Makefile.win -j4` (clean-rebuild after ANY header edit)
- **Reference checksum (both cores): 481009653**
- **Reference FINAL ROI CORE AVG IPC: 1.56796**
- **Reference wall time: 12.020 s** (single run; timing noise dominates sub-1% deltas at 2c/12s)

## OFFICIAL GATE (user spec 2026-06-19): 2M warmup / 14M sim, 2-core
- Gate: `bin/champsim.exe -warmup_instructions 2000000 -simulation_instructions 14000000 --arch glc --bypass none --pf_l1 no --pf_l2 no --pf_l3 no -traces <Pythia-70M> <Pythia-70M>`
- **Reference checksum (both cores): 140038115682937**
- **Reference FINAL ROI CORE AVG IPC: 0.56673**
- **Reference wall time: 43.286 s**
- (superseded 1M/5M ref: checksum 481009653 / IPC 1.56796 / 12.020s)

## METHOD (user-directed)
Per-opt FULL TREE COPY (champsim_v17_<opt>); one agent each, implemented IN PARALLEL; each agent
clean-builds + BIT-EXACT gates (checksum 140038115682937 AND IPC 0.56673). Then re-time each PASS
SEQUENTIALLY (quiet machine) -> retain same-or-better time. MERGE winners (multi-merge) -> rebuild ->
final IPC+time. Goal: reproduce the prior lost session's champsim.

## ORACLE
Bit-exact = checksum 140038115682937 AND IPC 0.56673. Any drift => NOT behavior-preserving => opt FAILS.
Timing: median of 3, sequential/quiet, reported honestly; sub-1% deltas likely below 2c/43s noise (remote wins were 16-core).

## IMPLEMENT QUEUE (proven wins, missing, never rolled back)
1. P24  - retire/RT-loop `else{break}` on head-not-ready (remote -4.24%, biggest)
2. P21  - scan_and_schedule per-word `stall==0 && (ready[w]&rmask)==0` skip (-1.39%)
3. P15  - dirty OR-fold predicate -> early return
4. P19B - `#ifdef TRUE_SANITY_CHECK` around rob_maps debug blocks (-0.71%)
5. P25  - drop `-fno-omit-frame-pointer` from Makefile.win (-0.29%)
6. B8   - AddrDependencyTracker compact threshold 128 -> 32
7. A8   - `__builtin_expect` on operate_lsq RTS/RTL empty guard
8. B1   - `__restrict__` scalar locals in fused_scan lambda
9. B3   - per-word dirty pre-skip (superset of P15)
10. C5  - reg_RAW_release hash -> direct array[ROB_SIZE]
11. C3A - AddrDependencyTracker remove_producer front-cursor
12. D2  - lpm_operate(LPM_Tracker&) overload
13. D5  - skip WQ.check_queue for instruction packets
14. D1  - run_clsbyp u16 shadow + fused 5-way compare
15. P28/D8 - fetch_instruction PACKET template hoist
16. P-BLOOM - blocked-512 k=4 one-cacheline bloom (replaces DualBloom128)

## DO NOT IMPLEMENT (verified poison)
P20 (deadlock), P22, P27 (+23.7%), P16/P17/P23/P26 (slower), P3/P4/P5/P13, P12/P18/P19 (premise false),
P-FASTSET (refuted), schedule-merge (drift), event_cycle-u32 (PCYCLE wrap), 06-08 data-packings (paper-only).
P6/P10 N/A here (AddressProxy is LIVE in this tree).

## LOG — Phase 1 (2-core 2M/14M equivalency). Times shown are CONTENDED (parallel) = NOT valid speed; real timing = Phase 2 (16-core sequential).
| Opt | checksum | IPC | 2c-equiv | change |
|-----|----------|-----|----------|--------|
| BASELINE | 140038115682937 | 0.56673 | ref (43.286s quiet) | P2+P7+P14 already in tree |
| B8  | 140038115682937 | 0.56673 | PASS | ooo_cpu.h compact 128->32 |
| P21 | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc scan empty-word skip |
| P25 | 140038115682937 | 0.56673 | PASS | Makefile.win drop -fno-omit-frame-pointer |
| P24 | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc operate_lsq else-break head-not-ready x4 |
| A8  | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc __builtin_expect operate_lsq guard |
| P15 | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc OR-fold early-return schedule_instruction |
| B3  | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc per-word dirty pre-skip |
| C5  | 140038115682937 | 0.56673 | PASS | instruction.h RegDepReleaseTracker hash->array[ROB_SIZE] |
| B1  | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc fused_scan __restrict__ locals |
| C3A | 140038115682937 | 0.56673 | PASS | ooo_cpu.h AddrDependencyTracker front-cursor |
| D2  | 140038115682937 | 0.56673 | PASS | lpm_tracker.h lpm_operate(LPM_Tracker&) overload + 3 callers |
| D5  | 140038115682937 | 0.56673 | PASS | cache.cc skip WQ.check_queue for instr packets (instruction==1 -> -1) |
| P28 | 140038115682937 | 0.56673 | PASS | ooo_cpu.cc fetch TRACE/FETCH_PACKET_TEMPLATE hoist |
| P-BLOOM | 140038115682937 | 0.56673 | PASS | cache.h DualBloom128 -> blocked-512 k=4 splitmix64 |
| P19B | n/a | n/a | ALREADY-REALIZED | dead rob_maps debug blocks already commented-out; nothing to wrap |
| D1  | n/a | n/a | ALREADY-REALIZED | LUT class + fused branchless byp + (dα|dL|dLM)==0 dz idiom already present |

## PHASE 1 RESULT: 14 implemented+bit-exact, 2 already-realized, 0 fail, 0 fake.
## PHASE 2 (16-core timing) — NUM_CPUS=16 rebuild + sequential median-3, baseline+each opt.
