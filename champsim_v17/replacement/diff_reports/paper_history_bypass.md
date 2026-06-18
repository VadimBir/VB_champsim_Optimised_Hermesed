# HBPB — History-Based Preemptive Bypassing
**Paper:** Krause, Santos, Navaux — SBAC-PAD 2022 (34th IEEE International Symposium on Computer Architecture and High Performance Computing)

---

## 1. What is the design?

HBPB bypasses **ALL cache levels** (L1, L2, L3) preemptively for instructions that are not known to be cache-friendly. The core philosophy is inverted from most approaches: **assume every instruction is cache-unfriendly unless it proves otherwise.** This is the opposite of traditional caches that assume everything is worth caching.

The mechanism has three hardware structures:
- **CIT (Classified Instructions Table):** A hash table indexed by PC (Program Counter). Each entry has a saturating counter that tracks how cache-friendly an instruction is. As few as 128 entries per core is sufficient (the paper found this indistinguishable from an infinite table).
- **AHT (Access History Table):** A clone of the LLC tag array — same size, same replacement policy. Stores the memory address AND the PC of the instruction that last accessed it. This tells the system "address X was last brought into the caches by instruction Y." When an address is reused, both the current instruction and the original instruction get credit as cache-friendly.
- **NTB (Non-Temporal Buffer):** A 32KB 8-way cache that sits parallel to the entire cache hierarchy. Bypassed data goes here instead of the caches. It provides short-term spatial/temporal reuse for bypassed data without polluting L1/L2/L3.

---

## 2. Do they actually bypass or pseudo-bypass?

**HBPB creates parallel paths — it does NOT skip MSHR entirely.**

When a bypass decision is made:
1. A new request is created and sent to the NTB
2. **Simultaneously**, the lower-level caches are still checked in parallel (the request still enters L2, L3)
3. If a cache hit is found during the parallel check → use the cached data, discard NTB result
4. If all caches miss → data comes from DRAM, fills into the NTB (NOT the caches), and pending MSHR entries on the caches are **deleted**
5. The NTB serves the data directly to the core's Load-Store Queue (LSQ)

**This is NOT MSHR-skip.** MSHRs are created in the caches during the parallel check. They are cleaned up after resolution. This is more complex than your true bypass (which simply skips MSHR creation) but provides a safety net: if the bypass prediction was wrong and the data IS in a cache, the parallel check catches it.

**Three variants tested:**
- **HBPB-R (Request bypass only):** Sends request to NTB in parallel, but cache fills still happen normally. Saves latency when NTB hits.
- **HBPB-F (Fill bypass only):** No parallel NTB requests. But when data returns from DRAM, it fills into NTB instead of caches. Saves cache pollution.
- **HBPB-RF (Both request and fill bypass):** Full bypass — parallel NTB requests AND fills go to NTB instead of caches. Best overall.

**HBPB-RF performed best** across all metrics (cycles, energy, ED²P).

---

## 3. What is the NTB (Non-Temporal Buffer)?

The NTB is a **32KB, 8-way set-associative cache** that sits physically parallel to L1 data cache. Key properties:

- **Same access latency as L1** (4 cycles) — because it's small and close to the core
- **Serves bypassed data only** — cache-unfriendly data that would pollute L1/L2/L3 goes here instead
- **Has its own prefetcher** — an IP-stride prefetcher that operates only on bypassed accesses, bringing non-temporal data closer without polluting caches
- **Write handling:** Bypassed writes go to NTB's write queue. They can only commit when the parallel cache check confirms miss on all caches (to maintain write-invalidate coherence).
- **Acts as a parallel L1 for non-temporal data** — the core checks L1 and NTB simultaneously

**Why it's needed:** Without NTB, bypassed data would have to come from DRAM every time (hundreds of cycles). NTB captures short-term reuse of non-temporal data at L1-like latency, giving bypassing its performance advantage.

---

## 4. How does the design work end-to-end?

### Scenario A: Cache-UNFRIENDLY instruction (unknown PC, first time seen)

1. **L1 miss** occurs for instruction at PC=0x401000
2. **HBPB decision:** Look up PC in CIT. Not found → **default = bypass** (assume unfriendly)
3. **Parallel paths launched:**
   - Path A: Request sent to NTB (check if this address was recently bypassed)
   - Path B: Request sent to L2 normally (parallel cache hierarchy check)
4. **NTB miss, L2 miss, L3 miss** → DRAM request issued
5. Data returns from DRAM → **fills into NTB** (not into L1/L2/L3)
6. **MSHR entries in L1/L2/L3 are deleted** (cleanup)
7. NTB serves data to core LSQ
8. **Training:** Address is inserted into AHT with PC=0x401000

### Scenario B: Cache-FRIENDLY instruction (proven reuse)

1. **L1 miss** for instruction at PC=0x402000
2. **HBPB decision:** Look up PC in CIT. Counter is above threshold → **do not bypass**
3. **Normal cache hierarchy operation:** L2 check → L3 check → DRAM if needed
4. Fills happen normally into L1/L2/L3
5. **Training:** Address checked in AHT → found! Both current PC and the PC that originally brought the data get their CIT counters incremented (reuse = cache-friendly)

### Training mechanism in detail:

On every L1 miss:
- Check if the memory address is in the AHT
- **AHT HIT** (address was seen before): Data was reused → good! Increment CIT counter for BOTH the current PC AND the PC stored in the AHT entry. Both instructions demonstrated cache-friendly behavior.
- **AHT MISS:** Insert the address+PC into AHT. If AHT is full, evict LRU entry. The evicted entry's PC gets its CIT counter **decremented** (its data died without reuse → cache-unfriendly).
- **Special case:** If address misses AHT AND the PC's CIT counter is already saturated negative (strongly unfriendly), the address is added to AHT with only 1/32 probability. This saves AHT space for potentially more useful entries.

---

## 5. What is the secret sauce?

**"Guilty until proven innocent"** — the inverted default.

Most cache systems assume data is worth caching. HBPB assumes the opposite: every instruction is cache-unfriendly until it demonstrates otherwise. This works because:

1. **Most instructions either always cache-friendly or always not.** The paper's reuse distance analysis (Fig 11-13) shows that the top instructions in mcf, xalancbmk, omnetpp have very consistent behavior — an instruction that bypasses once will usually bypass most of the time.

2. **Training is asymmetric:** It takes multiple reuses to become "cache-friendly" (counter must climb above threshold), but the default state is already "bypass." So new/unknown instructions immediately bypass instead of polluting caches during a learning period.

3. **The NTB catches short-term reuse** that would otherwise be lost. If bypassed data IS needed again quickly, the NTB serves it at L1 latency. This removes the main risk of aggressive bypassing.

4. **All-level bypass eliminates cascading pollution.** Previous work bypassed only LLC. But when LLC is bypassed, evictions from L1→L2→L3 still fill the caches with non-temporal data. HBPB blocks it at the source (L1 miss decision).

---

## 6. When is bypass activated?

Bypass is activated when an L1 data cache miss occurs AND:
- The instruction's PC is **NOT in the CIT** (unknown instruction → default bypass), OR
- The instruction's PC IS in the CIT but the **counter is BELOW the threshold** (learned to be unfriendly)

If the counter is **at or above threshold** → normal cache operation (no bypass).

New PCs start with counter **above threshold** (cache-friendly by default when first inserted into CIT). But CIT insertion only happens when the AHT confirms reuse. So a PC that has never had its data reused never gets a CIT entry → stays in bypass mode.

---

## 7. How does training work?

**CIT (Classified Instructions Table):**
- PC-indexed hash table with saturating counters
- Counter UP: when AHT confirms the PC's data was reused (both current and original PC get credit)
- Counter DOWN: when AHT evicts the PC's entry (data died without reuse)
- Decrement value < increment value: This asymmetry means instructions aren't penalized for short bursts of AHT evictions

**AHT (Access History Table):**
- LLC-sized tag array (same sets, same associativity, same replacement policy)
- Stores: memory address (tag) + PC of last instruction to access it
- On reuse detection (AHT hit): both PCs trained as cache-friendly
- On eviction without reuse: evicted PC trained as cache-unfriendly
- 1/32 probabilistic insertion for strongly unfriendly PCs (saves capacity)

---

## 8. Hardware cost

| Component | Size | Notes |
|-----------|------|-------|
| CIT | ~128 entries × (16-bit PC hash + counter) ≈ small | 128 entries sufficient per core |
| AHT | Same as LLC (1MB config: 16-way, tag+PC) = **131 KB** | 11.4% of LLC area. 64 bits per cache line (16-bit PC + 48-bit tag) |
| NTB | **32 KB** data + 3264 B tags = ~35 KB | 8-way, same latency as L1 |
| **Total** | **~166 KB** | Dominated by AHT |

The AHT is the major cost. It's essentially a shadow copy of the LLC tag array with an extra 16-bit PC field per entry. The paper acknowledges this is significant but argues the energy savings from bypassing offset it.

---

## 9. Architectural constraints

HBPB requires hardware features that simpler bypass systems don't:

1. **Parallel cache + NTB lookup:** The core must issue requests to both the NTB and the cache hierarchy simultaneously. This requires dual port access or an extra pipeline stage.
2. **MSHR cleanup logic:** After bypass resolution, MSHR entries in L1/L2/L3 must be deleted. This adds complexity to the miss-handling pipeline.
3. **Write coherence with NTB:** The LLC must confirm an address is NOT in the NTB before accepting a new DRAM fill (to prevent stale data). This adds a coherence check.
4. **NTB write queue:** Bypassed writes must buffer in the NTB's write queue and only commit after the parallel cache check confirms a miss. Adds latency for bypassed stores.
5. **AHT at LLC size:** The AHT must be at least LLC-sized to properly track which addresses are live. This is the biggest cost.

---

## 10. Critical insights for TRUE bypass (MSHR-skip)

Your system skips MSHR creation entirely — no parallel paths, no cleanup. Compared to HBPB:

**Advantages of your approach:**
- Simpler hardware: no NTB, no MSHR cleanup, no dual paths
- Lower latency for bypassed loads (straight to lower level, no parallel speculation)
- No AHT overhead

**What HBPB has that you don't:**
- Safety net: if prediction is wrong and data IS in cache, the parallel check catches it. Your MSHR-skip is irreversible.
- NTB captures short-term reuse of bypassed data. Your bypassed data has no local buffer.
- All-level bypass from a single decision point (L1 miss). Your system decides per-level.

**Borrowable ideas:**
- **Default-bypass philosophy:** Assume bypass unless proven otherwise. Could invert your predicate.
- **PC-based classification with asymmetric training:** The CIT's approach (fast to classify as unfriendly, slow to classify as friendly) could feed into your bypass predicate as a feature.
- **AHT-inspired reuse detection:** A much smaller structure (not LLC-sized) tracking recent bypassed addresses could tell your system "this bypassed address was accessed again" → train the predictor.
- **Per-instruction consistency:** The reuse distance analysis confirms that individual instructions have very stable cache behavior. This validates using PC as a bypass predictor feature in your system.

---

## 11. HBPB-R vs HBPB-F vs HBPB-RF

| Variant | What it bypasses | Best for | Mean cycle reduction | Mean ED²P reduction |
|---------|-----------------|----------|---------------------|-------------------|
| **HBPB-R** | Requests only (parallel NTB request, fills still happen to caches) | Latency reduction | 5.9% | — |
| **HBPB-F** | Fills only (no parallel requests, but DRAM data fills NTB not caches) | Energy/pollution | 2.1% | 7.7% |
| **HBPB-RF** | Both requests AND fills | Best overall (both latency + pollution) | 5.1% | 7.3% |

HBPB-RF was the best configuration across all applications and metrics. The best overall config (Table IV) was **HBPB+Pref** for ED²P metric.

---

## 12. Results

| Metric | Value |
|--------|-------|
| Max cycle reduction (single app) | **19.5%** (mcf with HBPB-RF) |
| Mean cycle reduction | **5.1%** (HBPB-RF) |
| Energy savings | Up to **28.6%** for a single app |
| ED²P reduction (mean) | **16.6%** (HBPB-RF) |
| With prefetchers | HBPB still improves over baseline+prefetch: **4.5% ED²P** reduction |
| Works with different replacement policies | Yes — LRU (16.5%), SHiP (12.5%), SRRIP (15%), DRRIP (12.1%) |
| Bigger LLC (8MB) | Still helpful for most apps, less for xalancbmk (working set fits in 8MB) |
| Never hurts performance | True — no app was negatively affected by >1% |

Key: HBPB's biggest wins are on apps with a mix of cache-friendly and cache-unfriendly instructions (mcf, xalancbmk). When most instructions are cache-friendly (omnetpp), HBPB correctly does not bypass and causes no harm.

---

## Flowchart

```
=== BYPASS DECISION (on L1d new miss) ===

(START: L1d miss for address A, from instruction PC)
  =>
<PC in CIT?>
  => YES =>
    <CIT[PC].counter >= threshold?>
      => YES => [DO NOT bypass — cache-friendly] => (REGULAR CACHE ACCESS)
      => NO  => [BYPASS — counter too low]
  => NO => [BYPASS — unknown instruction, default=unfriendly]
  =>

=== BYPASS PATH ===

[send parallel request to NTB] + [send request to L2 (normal hierarchy)]
  =>
<NTB hit?>
  => YES => [serve from NTB to core LSQ] => (END)
  => NO  =>
    <L2 hit?>
      => YES => [serve from L2, discard NTB path] => (END)
      => NO  =>
        <L3 hit?>
          => YES => [serve from L3, update NTB replacement state] => (END)
          => NO  =>
            [request DRAM]
            [fill data into NTB (NOT into caches)]
            [delete MSHR entries on L1/L2/L3]
            [NTB serves data to core LSQ]
            => (END)

=== TRAINING (on every L1d miss) ===

(START: L1d miss for address A, from instruction PC_current)
  =>
<address A in AHT?>
  => YES (reuse detected!) =>
    [PC_original = AHT[A].stored_PC]
    /CIT[PC_current].counter++/   (current instruction is cache-friendly)
    /CIT[PC_original].counter++/  (original instruction is cache-friendly)
    [update AHT[A].PC = PC_current]
  => NO (first time or evicted) =>
    <PC_current in CIT AND counter saturated negative?>
      => YES => [insert into AHT with 1/32 probability only]
      => NO  =>
        [insert (address A, PC_current) into AHT]
        <AHT full? need eviction?>
          => YES =>
            [evict LRU AHT entry]
            [evicted_PC = evicted_entry.PC]
            /CIT[evicted_PC].counter--/  (data died unused — unfriendly)
    <PC_current in CIT?>
      => NO => /CIT[PC_current] = above_threshold/  (new PCs start as cache-friendly)
  =>
(END)
```
