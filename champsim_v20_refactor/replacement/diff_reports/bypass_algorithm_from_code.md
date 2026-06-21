# ChampSim Cache Bypass — Algorithm Flowchart (from code)

**Source files:** `inc/cache.h`, `src/cache.cc`, `src/dram_controller.cc`
**Scope:** LOADs only. Stores / RFOs / prefetches / writebacks are never bypass-eligible.
**Compile guards:** `BYPASS_L1D_OnNewMiss`, `BYPASS_L2_LOGIC`, `BYPASS_LLC_LOGIC` (independent, never merged)
**Packet bypass fields (inc/block.h):** `l1_bypassed`, `l2_bypassed`, `llc_bypassed` — each `uint8_t`, zero on construction.

---

## SECTION 1 — REQUEST PATH (CPU → DRAM)

### Entry point: CACHE::handle_read() [cache.cc ~436]

```
(START: CPU issues LOAD)
  => [CPU → L1D.add_rq(pkt)]
  => [L1D.handle_read() fires per cycle]

  <L1D cache hit?>
    YES => [handle_read_hit_processed: push to L1D.PROCESSED]
           => [ooo_cpu drains L1D.PROCESSED → CPU retires] => (END: hit path)
    NO  => [check_mshr(pkt) → mshr_index]

  <mshr_index != -1? (in-flight merge)>
    YES => [handle_read_miss_inflight_merge_deps]
           [handle_read_miss_inflight_fill_level]
           => (no bypass decision — packet merged into existing MSHR)
           => (END: merged path)
    NO  => [try handle_read_miss_bypass(read_cpu, index, mshr_index)]

  ── L1 BYPASS DECISION [cache.cc ~389, guard: BYPASS_L1D_OnNewMiss] ──
  <cache_type==IS_L1D && type==LOAD && mshr_index==-1
   && warmup_complete && lower_level not full
   && l1d_bypass_operate(cpu, L1D*, L2C*, LLC*) returns true?>
    [LPM inputs to model: κ, CAMAT, μ, φ, LPMR, MST, APC from lpm[cpu][LPM_L1D]]
    YES => [pkt.l1_bypassed = 1]
           [pkt.fill_level = FILL_L2]
           [stats: ByP_issued++]
           [lower_level->add_rq(pkt)]   // lower_level of L1D = L2C
           => NO L1D MSHR CREATED
           => (packet now at L2C.RQ) => CONTINUE to L2C

  <L1 bypass not taken → new miss path>
    => [add_mshr(pkt) at L1D]
    => [lower_level->add_rq(pkt)]   // normal: L1D → L2C
    => (END: normal L1D miss — L2C creates MSHR, LLC, DRAM proceed normally)
```

### At L2C — either bypass packet from L1D, or normal L1D-missed packet

```
  [L2C.handle_read() fires]
  <L2C cache hit?>
    YES => [handle_read_hit_bypass_return: cache.h ~348]
           <pkt.l1_bypassed == 1?>
             YES => [push to L2C.PROCESSED] => (END: L2C hit, L1-bypassed — CPU gets via L2C.PROCESSED)
             NO  => [normal return_to_upper_level → L1D.return_data]
    NO  => [check_mshr → mshr_index]

  <mshr_index != -1?>
    YES => [handle_read_miss_inflight_merge_deps]
           [handle_read_miss_inflight_fill_level]
           [handle_read_miss_inflight_bypass_l1_mismatch ← see Section 3]
           [handle_read_miss_inflight_bypass_l2_mismatch ← see Section 3]
           <MSHR.type == PREFETCH?>
             YES => [handle_read_miss_inflight_prefetch_takeover ← see Section 3]
             NO  => [handle_read_miss_inflight_non_prefetch_merge]
           => (END: merged)
    NO  => [try handle_read_miss_bypass]

  ── L2 BYPASS DECISION [cache.cc ~402, guard: BYPASS_L2_LOGIC] ──
  <cache_type==IS_L2C && type==LOAD && !instruction
   && mshr_index==-1 && l1_bypassed==0          // ISOLATION: no L1+L2 adjacent bypass
   && warmup_complete && lower_level not full
   && l2c_bypass_operate(cpu, L1D*, L2C*, LLC*) returns true?>
    [LPM inputs to model: κ, CAMAT, μ, φ, LPMR, MST from lpm[cpu][LPM_L2C]]
    YES => [pkt.l2_bypassed = 1]
           [pkt.fill_level = FILL_LLC]
           [stats: ByP_issued++]
           [lower_level->add_rq(pkt)]   // lower_level of L2C = LLC
           => NO L2C MSHR CREATED
           => (packet now at LLC.RQ) => CONTINUE to LLC

  <L2 bypass not taken → new miss path>
    <cache_type == IS_L2C?>
      => [add_mshr(pkt) at L2C]
      => [lower_level->add_rq(pkt)]   // normal: L2C → LLC
    => (END: normal L2C miss)
```

### At LLC — either L2-bypass packet, or normal L2C-missed packet

```
  [LLC.handle_read() fires]
  <LLC cache hit?>
    YES => [handle_read_hit_bypass_return: cache.h ~361]
           <pkt.l2_bypassed == 1?>
             YES => [LLC.upper_dcache[cpu]->upper_dcache[cpu]->return_data(pkt)]
                    // = L2C.upper_dcache = L1D → L1D.return_data()
                    => (END: LLC hit, L2-bypassed — L1D gets data directly)
             NO  => [normal return_to_upper_level → L2C.return_data]
    NO  => [check_mshr → mshr_index]

  <mshr_index != -1?>
    YES => [merge deps, fill_level, mismatch handlers ← see Section 3]
           => (END: merged)
    NO  => [try handle_read_miss_bypass]

  ── LLC BYPASS DECISION [cache.cc ~418, guard: BYPASS_LLC_LOGIC] ──
  <cache_type==IS_LLC && type==LOAD && mshr_index==-1
   && l2_bypassed==0                             // ISOLATION: no L2+LLC adjacent bypass
   && warmup_complete && lower_level not full
   && llc_bypass_operate(cpu, DRAM*, LLC_lower*, LLC*) returns true?>
    [LPM inputs: κ, CAMAT, μ, φ, LPMR, MST from lpm[cpu][LPM_LLC]]
    YES => [pkt.llc_bypassed = 1]
           [pkt.fill_level = FILL_DRAM]
           [stats: ByP_issued++]
           [lower_level->add_rq(pkt)]   // lower_level of LLC = DRAM
           => NO LLC MSHR CREATED
           => (packet now at DRAM.RQ) => (END: request at DRAM)

  <LLC bypass not taken → new LLC miss>
    => [handle_read_miss_new_llc: add_mshr(pkt) at LLC, lower_level->add_rq(pkt)]
    => (END: normal LLC miss — DRAM processes)
```

### Isolation rules (enforced in bypass decision guards)
```
  L1 + L2 bypass FORBIDDEN  (adjacent): l1_bypassed==0 guard in L2 decision
  L2 + LLC bypass FORBIDDEN (adjacent): l2_bypassed==0 guard in LLC decision
  L1 + LLC bypass ALLOWED   (non-adjacent): L2C MSHR is the anchor between them
  Instructions (pkt.instruction==1): NEVER L2-bypassed (guard in L2 decision)
```

---

## SECTION 2 — RETURN PATH (DRAM → CPU)

### LLC bypass return path (in dram_controller.cc ~467)

```
(DRAM completes request)
  [dram_controller fires return]

  <pkt.llc_bypassed == 1?  [guard: BYPASS_LLC_LOGIC]>
    YES => [upper_level_dcache[op_cpu]->upper_level_dcache[op_cpu]->return_data(pkt)]
           // DRAM.upper = LLC; LLC.upper = L2C
           // So this calls: LLC(ptr)->upper_dcache[cpu] = L2C, then L2C.return_data(pkt)
           // i.e., DRAM skips LLC, returns to L2C directly

           ── BYP_DERFILL_ACTIVE: derivative fill into LLC ──
           <BYP_DERFILL_ACTIVE defined?>
             YES => [df_pkt = pkt; df_pkt.llc_bypassed = 0]
                    <BYP_DERFILL_IMMEDIATE defined?>
                      YES => [find_victim in LLC]
                             [handle dirty eviction to DRAM.WQ if needed]
                             [llc->fill_cache(set, way, &df_pkt)]  // IMMEDIATE fill
                             [llc->update_replacement_state(...)]
                    <BYP_DERFILL_SEQUENTIAL defined?>
                      YES => [df_pkt.returned=COMPLETED]
                             [df_pkt.event_cycle = now + LLC.LATENCY]
                             [df_pkt.fill_level = FILL_LLC]
                             [df_pkt.type = PREFETCH]
                             <LLC.MSHR not full && no duplicate?>
                               YES => //STORE// [llc->MSHR.add_queue(df_pkt)]  // re-inject
                                      [llc->MSHR.num_returned++]
                                      [llc->update_fill_cycle()]
                    <neither defined?>  => LLC never filled (pure skip)
             NO  => LLC never filled (pure skip)

    NO  => [upper_level_dcache[op_cpu]->return_data(pkt)]
           // Normal path: DRAM → LLC.return_data

(END LLC bypass DRAM return)
```

### Normal fill return path (L2C.handle_fill / LLC.handle_fill) [cache.h ~885]

```
(Lower level data returns → CACHE::handle_fill() fires)
  [handle_fill_find_victim → way]

  ── LLC_BYPASS check (way == LLC_WAY = bypass sentinel) ──
  <cache_type==IS_LLC && way==LLC_WAY?  [guard: LLC_BYPASS — older mechanism]>
    YES => [update replacement, stats]
           [handle_fill_return: if fill_level < this.fill_level → return_to_upper_level]
           [MSHR.remove_queue] => (END: LLC bypass no-fill path)

  [handle_fill_evict_dirty: if dirty block, send WB to lower WQ]
  [handle_fill_pf_fill: prefetcher cache fill hook]
  [handle_fill_replacement: update replacement state]
  [handle_fill_stats: sim_miss/sim_access++]
  [handle_fill_cache_and_dirty: fill_cache(), mark dirty if L1D+RFO]
  [handle_fill_return: if fill_level < this.fill_level → return_to_upper_level]

  ── [handle_fill_processed_and_bypass_return] [cache.h ~885] ──

  <cache_type == IS_L2C && pkt.type==LOAD && pkt.l1_bypassed==1?  [BYPASS_L1_LOGIC]>
    YES => [push to L2C.PROCESSED]   // CPU gets data via L2C.PROCESSED drain
           // ooo_cpu drains L2C.PROCESSED → complete_data_fetch → CPU retires

           ── BYP_DERFILL_ACTIVE: derivative fill into L1D ──
           <BYP_DERFILL_ACTIVE defined?>
             YES => [df_pkt = pkt; df_pkt.l1_bypassed = 0]
                    <BYP_DERFILL_IMMEDIATE defined?>
                      YES => [l1d = upper_dcache[fill_cpu]]
                             [find_victim in L1D]
                             [handle dirty eviction to L2C.WQ if room]
                             [l1d->fill_cache(set, way, &df_pkt)]
                             [l1d->update_replacement_state(...)]
                    <BYP_DERFILL_SEQUENTIAL defined?>
                      YES => [df_pkt.returned=COMPLETED]
                             [df_pkt.event_cycle = now + l1d->LATENCY]
                             [df_pkt.fill_level = FILL_L1]
                             [df_pkt.type = PREFETCH]
                             <L1D MSHR not full && no duplicate?>
                               YES => //STORE// [l1d->MSHR.add_queue(df_pkt)]
                                      [l1d->MSHR.num_returned++]
                                      [l1d->update_fill_cycle()]
                    <neither defined?>  => L1D never filled (pure skip)

  <cache_type == IS_LLC && pkt.type==LOAD && pkt.l2_bypassed==1?  [BYPASS_L2_LOGIC]>
    YES => [upper_dcache[fill_cpu]->upper_dcache[fill_cpu]->return_data(pkt)]
           // LLC.upper = L2C; L2C.upper = L1D → calls L1D.return_data(pkt)
           // L1D finds its own MSHR (L1D has MSHR since L2 was bypassed, not L1)

           <pkt.pf_merged_from_upper && L2C probe_mshr(pkt) != -1?>
             YES => [return_to_upper_level(pkt)]  // complete L2C's dangling prefetch MSHR

           ── BYP_DERFILL_ACTIVE: derivative fill into L2C ──
           <BYP_DERFILL_ACTIVE defined?>
             YES => [df_pkt = pkt; df_pkt.l2_bypassed=0; df_pkt.l1_bypassed=0]
                    <BYP_DERFILL_IMMEDIATE defined?>
                      YES => [l2c = upper_dcache[fill_cpu]]
                             [find_victim in L2C]
                             [handle dirty eviction to LLC.WQ if room]
                             [l2c->fill_cache(set, way, &df_pkt)]
                             [l2c->update_replacement_state(...)]
                    <BYP_DERFILL_SEQUENTIAL defined?>
                      YES => [df_pkt.returned=COMPLETED]
                             [df_pkt.event_cycle = now + l2c->LATENCY]
                             [df_pkt.fill_level = FILL_L2]
                             [df_pkt.type = PREFETCH]
                             <L2C MSHR not full && no duplicate?>
                               YES => //STORE// [l2c->MSHR.add_queue(df_pkt)]
                                      [l2c->MSHR.num_returned++]
                                      [l2c->update_fill_cycle()]
                    <neither defined?>  => L2C never filled (pure skip)

  <neither bypass case matched>
    => normal L1D/L1I/TLB PROCESSED push if applicable
    => (END: normal fill return)

  [handle_fill_remove: latency stats, MSHR.remove_queue, update_fill_cycle]
```

### Return path summary table

```
  Scenario                        | MSHR owner | CPU gets data via
  ──────────────────────────────────────────────────────────────────
  Normal L1D hit                  | none       | L1D.PROCESSED
  Normal L1D miss (filled at L1D) | L1D        | L1D.PROCESSED
  L1 bypassed, L2C fills          | L2C        | L2C.PROCESSED
  L1 bypassed, L2C hit            | none       | L2C.PROCESSED (hit path)
  L2 bypassed, LLC fills          | LLC        | LLC→L2C(ptr)→L1D.return_data → L1D.PROCESSED
  L2 bypassed, LLC hit            | none       | LLC→L2C(ptr)→L1D.return_data → L1D.PROCESSED
  LLC bypassed, DRAM returns      | none(DRAM) | DRAM→L2C(ptr via double-deref)→L2C.return_data
  LLC BYPASSED + L1 bypassed      | L2C        | L2C.PROCESSED (L2C is anchor MSHR)
```

---

## SECTION 3 — MISMATCH HANDLERS

### 3.1 L1 bypass mismatch at L2C [cache.h ~518, handle_read_miss_inflight_bypass_l1_mismatch]

```
  <cache_type==IS_L2C && RQ.l1_bypassed != MSHR.l1_bypassed?>
    <MSHR.type != PREFETCH?>
      YES => <=LOAD<= [scan L1D.MSHR for matching address]
             <found_l1d_mshr?>
               YES => [inject lq/sq deps from RQ into L1D.MSHR.entry]
                      [RQ.l1_bypassed = 0]
                      [MSHR.l1_bypassed = 0]
                      [MSHR.fill_level = 1 (FILL_L1)]  // redirect return to L1D
               NO  => (no redirect — leave bypass active, let it return via L2C.PROCESSED)
                      // KNOWN BUG scenario: after merge_with_prefetch, type!=PREFETCH
                      // but no L1D MSHR exists → found_l1d_mshr=false → correct: bypass preserved
    <MSHR.type == PREFETCH && MSHR.fill_level < this.fill_level?>
      YES => [RQ.l1_bypassed = 0]  // prefetch will be taken over, clear incoming bypass flag only
```

### 3.2 L2 bypass mismatch at LLC [cache.h ~564, handle_read_miss_inflight_bypass_l2_mismatch]

```
  <cache_type==IS_LLC && RQ.l2_bypassed != MSHR.l2_bypassed?>
    <MSHR.type != PREFETCH?>
      YES => <=LOAD<= [scan L2C.MSHR for matching address]
             <found_l2c_mshr?>
               YES => [inject lq/sq deps from RQ into L2C.MSHR.entry]
                      [RQ.l2_bypassed = 0]
                      [MSHR.l2_bypassed = 0]
                      [MSHR.fill_level = FILL_L2]  // redirect return: LLC fills → L2C normal path
               NO  => (leave bypass active)
    <MSHR.type == PREFETCH && MSHR.fill_level < this.fill_level?>
      YES => [RQ.l2_bypassed = 0]  // prefetch takeover pending
```

### 3.3 Prefetch takeover — bypass flag re-injection [cache.h ~610, handle_read_miss_inflight_prefetch_takeover]

```
  (LOAD merges with PREFETCH MSHR — LOAD's data overwrites MSHR via merge_with_prefetch)
  [merge_with_prefetch: SAVE {returned, event_cycle, fill_level, l1/l2/llc_bypassed, pf_merged}]
  [MSHR = queue_packet (LOAD data)]
  [RESTORE: returned, event_cycle, fill_level, l1/l2/llc_bypassed, pf_merged]
  // After restore, MSHR retains PREFETCH's original bypass state.

  // THEN in prefetch_takeover (called AFTER merge_with_prefetch):
  <RQ.l1_bypassed == 1?>  YES => [MSHR.l1_bypassed = 1]  // re-inject LOAD's bypass flag
  <RQ.l2_bypassed == 1?>  YES => [MSHR.l2_bypassed = 1]
  <RQ.llc_bypassed == 1?> YES => [MSHR.llc_bypassed = 1]

  // NOTE: merge_with_prefetch restores prefetch fill/bypass; prefetch_takeover then
  // overrides with the LOAD's bypass if the LOAD wants bypass. LOAD wins.
```

### 3.4 PQ merge with bypassed MSHR [cache.cc ~622, handle_prefetch]

```
  (PQ entry merges with existing MSHR: fill_level update path)
  <PQ.fill_level < MSHR.fill_level?>
    <MSHR.type == PREFETCH?>
      YES => [MSHR.fill_level = PQ.fill_level]  // normal prefetch update
    <MSHR.l1_bypassed == 1?>  [BYPASS_L1_LOGIC]
      YES => [MSHR.l1_bypassed = 0]
             [MSHR.fill_level = PQ.fill_level]
             // Prefetcher predicts L1 needed — clear L1 bypass, promote return to L1D
    <MSHR.l2_bypassed == 1?>  [BYPASS_L2_LOGIC]
      YES => [MSHR.pf_merged_from_upper = 1]
             // Cannot clear L2 bypass here (no L2C promotion path for LLC MSHR)
             // Tag for cleanup: on LLC→L1D direct return, check pf_merged and complete L2C PF MSHR
```

### 3.5 add_rq RQ-merge mismatch (bypass downgrade) [not fully shown — see add_rq flow]

```
  (New packet arrives via add_rq, merges with existing RQ entry with different bypass)
  => [clear bypass on both sides, adopt lower fill_level]
```

---

## LPM METRIC REFERENCE (for bypass model predicates)

```
  κ   (kappa)  = m / (m + x)  — pure-miss fraction of miss cycles. High κ → low concurrency benefit → bypass candidate.
  μ   (mu)     = (m + x) / ω  — fraction of active cycles with miss activity.
  φ   (phi)    = (h + x) / ω  — fraction of active cycles with hit activity.
  CAMAT        = ω / α         — concurrent avg memory access time (throughput metric).
  APC          = α / ω         — accesses per cycle (= CAMAT^-1).
  LPMR(l)      = λ(l) / ν(l)  — request rate / supply rate. LPMR > 1 → bottleneck at level l.
  MST          = m / α         — memory stall time per access (= μ × κ × CAMAT).
  CPA          = total_cy / α  — cycles per access (raw).

  Tier accessors: lpm[cpu][LPM_L1D/L2C/LLC/DRAM].{g, w, ws, wl}
  Helpers: get_recursive_camat(cpu, level), get_kappa_short/long(cpu),
           kappa_durable(cpu), get_LPMR_*(), get_apc(cpu, level)
  Bypass-corrected: m_byp_l1d_pureMissCy, m_byp_llc_pureMissCy

  Decision function signatures:
    l1d_bypass_operate(cpu, CACHE* l1d, CACHE* l2c, CACHE* llc)  → bool
    l2c_bypass_operate(cpu, CACHE* l1d, CACHE* l2c, CACHE* llc)  → bool
    llc_bypass_operate(cpu, CACHE* dram, CACHE* llc_lower, CACHE* llc) → bool
```

---

## KEY INVARIANTS

```
  I1: bypass packet NEVER creates MSHR at the bypassed level
  I2: l1_bypassed+l2_bypassed cannot both be 1 (adjacent-bypass forbidden)
  I3: l2_bypassed+llc_bypassed cannot both be 1 (adjacent-bypass forbidden)
  I4: l1_bypassed+llc_bypassed CAN both be 1 (non-adjacent, L2C is anchor)
  I5: pkt.instruction==1 → NEVER L2-bypassed
  I6: merge_with_prefetch saves/restores bypass fields BEFORE overwrite
  I7: prefetch_takeover re-injects LOAD's bypass AFTER merge_with_prefetch restore
  I8: BYP_DERFILL_ACTIVE — "bypass" = skip MSHR but back-fill on return (not pure skip)
  I9: BYP_DERFILL undefined — pure skip (bypassed level never sees the data)
  I10: LLC.PROCESSED is never drained by ooo_cpu — only L1D/L1I/L2C.PROCESSED feed CPU
  I11: fill_level < this.fill_level → return_to_upper_level (governs all return routing)
  I12: pf_merged_from_upper=1 tags LLC MSHR that absorbed L2C prefetch — cleaned on return
```
