# SDBP — Sampling Dead Block Prediction for Last-Level Caches
**Paper:** Khan, Tian, Jiménez — MICRO 2010 (43rd IEEE/ACM International Symposium on Microarchitecture)

---

## 1. What is the design?

SDBP predicts whether a cache block in the LLC (Last-Level Cache) is "dead" — meaning it will never be accessed again before it gets evicted. On average, **86% of blocks in a 2MB LRU-managed LLC are dead** at any given time. That's massive waste.

The key idea: instead of tracking every single cache set (expensive), SDBP **samples only 32 out of 2048 sets** (1.6%) and learns patterns from those. The patterns learned from the sampled sets generalize to the entire cache. This is the "sampling" in the name.

What it tracks in those sampled sets: the **Program Counter (PC) of the last instruction that touched each block**. That single PC is enough to predict future behavior. If a PC's blocks tend to die without reuse, future blocks from that PC are predicted dead too.

The prediction drives two optimizations:
- **Replacement:** When the cache needs to evict something, prefer blocks predicted dead over LRU/random victims
- **Bypass:** When a new block arrives and is predicted "dead on arrival," don't place it in the cache at all

---

## 2. What is a "sampler set"?

A **sampler set** is a miniature replica of a cache set, kept in a separate structure outside the main cache. Think of it as a spy camera pointed at a small fraction of the cache.

**Why sample instead of tracking everything?** Previous predictors (like "reftrace") needed metadata for EVERY cache block — 16 bits per block × 32K blocks = 64KB of extra storage, plus read/modify/write on every access. SDBP's sampler needs only 6.75KB total because it watches only 32 sets.

**How it works:**
- The LLC has 2048 sets. The sampler picks 32 of them (every 64th set).
- Each sampler set has 12 entries (even though the LLC is 16-way). The paper found 12-way samplers outperform 16-way because blocks in smaller sets get evicted sooner, so the sampler learns faster.
- Each sampler entry stores: a 15-bit partial tag (not the full address — just enough to identify blocks), a 15-bit PC signature (the PC of the last instruction that accessed this block), a 1-bit prediction, a 1-bit valid, and 4 bits for LRU ordering.
- Total sampler storage: 32 sets × 12 entries × 36 bits = **6.75KB**

**The critical insight:** Memory access patterns are consistent across sets. What happens in 1.6% of sets reliably represents what happens in the other 98.4%. This is why sampling works — you get nearly the same prediction accuracy at a fraction of the cost.

---

## 3. What is a "skewed predictor"?

A **skewed predictor** uses **3 separate tables**, each indexed by a **different hash function** applied to the same PC. This is borrowed from branch prediction research.

**Why 3 tables instead of 1?**
- With 1 table, two different PCs can hash to the same entry ("aliasing" or "destructive interference"). When this happens, the counter bounces up and down and never converges.
- With 3 tables using 3 different hash functions, two PCs might collide in one table but are unlikely to collide in all three. The sum of 3 counters still gives an accurate answer even if one is corrupted by aliasing.

**Structure:** 3 tables × 4,096 entries × 2-bit saturating counters = **3KB total**.

**How prediction works:** For a given PC, look up all 3 tables, sum the 3 counter values (range 0–6). If sum ≥ threshold (8... wait — with 2-bit counters max is 3 each, so max sum = 9, threshold = 8 means "very confident dead"). Actually the paper uses confidence threshold of 8 from a range that allows finer sensitivity — with three tables, you get 9 possible confidence levels (0–2 per table, sum 0–6) vs only 4 from a single table. The paper found threshold of 8 is best when counter values go up to 3 each (sum 0–9).

**"Skewed"** means each table sees the data from a different angle (hash). Unrelated PCs that collide in table 1 probably don't collide in tables 2 and 3, so the destructive interference is dramatically reduced.

---

## 4. Do they actually bypass or pseudo-bypass?

**Fill-skip only — NOT true bypass (NOT MSHR-skip).**

When SDBP decides to bypass:
1. The LLC miss still happens normally
2. An MSHR (Miss Status Holding Register) entry IS created
3. The request goes to DRAM normally
4. Data comes back from DRAM normally
5. **BUT: the data is NOT placed into the cache** — the fill step is skipped
6. The data is still forwarded to the requesting upper-level cache (L2/L1)

This is fundamentally different from your bypass system which skips MSHR creation entirely. SDBP's approach is safer (MSHR handles everything normally) but wastes MSHR occupancy on data that won't be cached.

**What bypass means in SDBP:** "If a block is predicted dead on arrival (dead at its first access), don't store it in the LLC." The block still travels through the normal miss-handling pipeline.

---

## 5. How does the design work end-to-end?

**Scenario: LLC miss for address A, accessed by instruction at PC=0x4012a0**

**Step 1 — Check if this is a sampled set:**
The LLC set index for address A is computed. If this set is one of the 32 sampled sets (e.g., set 64, 128, 192, ..., 2016), go to Step 2. Otherwise skip to Step 4.

**Step 2 — Update sampler (training):**
- Look up address A in the sampler for this set using the 15-bit partial tag.
- **Sampler HIT** (address was already being tracked): The block was re-accessed before eviction — it was LIVE. Take the old PC stored in this entry. Increment the 3 predictor counters for that old PC (it produced a live block → "less dead"). Update the entry's PC to the current PC (0x4012a0).
- **Sampler MISS** (address not tracked): Evict the LRU sampler entry. The evicted entry's PC produced a dead block (evicted without reuse). Decrement the 3 predictor counters for that evicted PC ("more dead"). Insert new entry with current PC.

**Step 3 — The sampler has now trained the predictor.**

**Step 4 — Make prediction for current access:**
- Hash current PC (0x4012a0) three ways → look up 3 predictor tables → sum counters.
- If sum ≥ 8 → predict DEAD.

**Step 5 — Use prediction:**
- **For replacement:** Mark this cache block with the prediction bit (1 = dead, 0 = live). When a future eviction is needed, prefer blocks marked dead.
- **For bypass:** If this is a new block entering the LLC AND predicted dead → don't fill it into the cache.

---

## 6. What is the secret sauce?

Three synergistic insights:

1. **Sampling generalizes:** You don't need to watch every set. 1.6% of sets tells you what you need to know. This slashes storage from 64–108KB (previous predictors) to 13.75KB.

2. **Last-touching PC is enough:** Previous predictors tracked entire traces of PCs that accessed a block. SDBP shows that just the LAST PC to touch a block before it dies is sufficient. This eliminates per-block signatures entirely — just 1 bit per cache block instead of 16 bits.

3. **Skewing reduces interference:** Three small tables with different hashes beat one large table, because aliasing in one table doesn't corrupt the other two.

The paper shows these 3 components are synergistic (Fig 6): each alone helps ~3.4-3.8% speedup, but together they reach 5.9%. The sampler's filtering effect makes the skewed predictor work better, and the skewed predictor makes the sampler's limited training data go further.

---

## 7. When is bypass activated?

Bypass activates when **all** of these conditions hold:
- It's an LLC miss (new data arriving)
- The incoming block is predicted "dead on arrival" (confidence sum ≥ threshold)
- The threshold is **8** (out of max 9 from three 2-bit counters)

The threshold of 8 means bypass is conservative — only blocks with very high dead confidence are bypassed. This minimizes false positives (live blocks incorrectly bypassed).

**Prediction accuracy (from paper):**
- Sampler predictor: **59% coverage**, **3.0% false positive rate** — meaning it identifies 59% of dead blocks while only wrongly flagging 3% of live blocks as dead
- Compare to reftrace: 88% coverage but 19.9% false positive rate
- Compare to counting: 67% coverage, 7.19% false positive rate
- Lower coverage is OK because lower false positives means less damage from wrong predictions

---

## 8. Hardware cost

| Component | Storage |
|-----------|---------|
| Predictor tables | 3 × 4,096 × 2-bit = 3KB |
| Sampler | 32 sets × 12 entries × (15-bit tag + 15-bit PC + 1-bit pred + 1-bit valid + 4-bit LRU) = 6.75KB |
| Cache metadata | 1 bit per cache block × 32K blocks = 4KB |
| **Total** | **13.75KB** (~1% of 2MB LLC) |

**Power:** 3.1% of LLC dynamic power, 1.2% of LLC leakage power. Compare to counting predictor: 11% dynamic, 4.7% leakage. SDBP is dramatically cheaper because the sampler is only accessed on 1.6% of LLC accesses.

---

## 9. Critical insights for TRUE bypass (MSHR-skip)

Your system does MSHR-skip: when bypass is decided, no MSHR entry is created and the request is forwarded directly to the lower level. SDBP's predictor could feed into this, but with important differences:

**What works directly:**
- The PC-indexed skewed predictor is lightweight and could predict "should this load bypass?" at the point where your bypass decision is made (before MSHR creation)
- The "dead on arrival" signal maps directly to "this data isn't worth caching at this level"

**What needs adaptation:**
- SDBP's sampler trains on LLC evictions — it sees when blocks die. With MSHR-skip, bypassed blocks never enter the cache, so there's no eviction signal for them. You'd need an alternative training signal (e.g., train on fill-return latency instead).
- SDBP operates at LLC only. Your system operates at L1+L2+LLC. The sampler concept would need per-level instances.
- SDBP's "bypass" is risk-free (MSHR still handles the request). Your MSHR-skip is irreversible — if the prediction is wrong, you've committed to the bypass path. **False positive rate matters more** for true bypass. SDBP's 3% false positive rate might be acceptable, but needs validation.

**Borrowable ideas:**
- The skewed predictor design (3 tables, different hashes) is directly reusable as a feature in your bypass predicate
- The "sample a few sets to learn for all" idea could reduce training overhead for your per-level predictors

---

## 10. Results

| Metric | Value |
|--------|-------|
| Geomean speedup (single-thread) | **5.9%** over LRU baseline |
| Geomean weighted speedup (multi-core) | **12.5%** |
| LLC miss reduction (single-thread) | **11.7%** |
| LLC miss reduction (multi-core) | **23%** |
| % of optimal policy achieved | **63%** |
| Works with random replacement | Yes — 3.4% speedup even with random (no LRU needed) |
| Comparison to RRIP | Beats RRIP (4.1%) and DIP (3.1%) |

The sampling predictor achieves the best accuracy with the lowest storage and power among all tested predictors, and it's the only one that works with both LRU and random replacement baselines.

---

## Flowchart

```
=== TRAINING (only on sampled sets — 1.6% of LLC accesses) ===

(START: LLC access at address A, from instruction PC)
  =>
<is this a sampled set? (set_index mod 64 == 0)>
  => NO => (skip training, go to PREDICTION)
  => YES =>
    <does partial_tag(A) match any entry in sampler[set]?>
      => YES (sampler hit — block was REUSED = LIVE) =>
        [old_PC = sampler_entry.last_PC]
        /predictor[hash1(old_PC)].counter++/  (old PC produced live block)
        /predictor[hash2(old_PC)].counter++/
        /predictor[hash3(old_PC)].counter++/
        [sampler_entry.last_PC = current_PC]
      => NO (sampler miss — must evict) =>
        [victim = LRU entry in sampler[set]]
        [victim_PC = victim.last_PC]
        /predictor[hash1(victim_PC)].counter--/  (victim PC produced dead block)
        /predictor[hash2(victim_PC)].counter--/
        /predictor[hash3(victim_PC)].counter--/
        [insert new entry: tag=partial_tag(A), last_PC=current_PC]

=== PREDICTION (on every LLC access) ===

(START: LLC access from instruction PC)
  =>
[c1 = predictor[hash1(PC)]]
[c2 = predictor[hash2(PC)]]
[c3 = predictor[hash3(PC)]]
[confidence = c1 + c2 + c3]
  =>
<confidence >= 8?>
  => YES => [predict DEAD]
  => NO  => [predict LIVE]

=== REPLACEMENT (on LLC miss needing victim) ===

(START: need to evict a block from set)
  =>
<any block in set marked DEAD?>
  => YES => [evict a dead block]
  => NO  => [evict random block (or LRU)]

=== BYPASS (on LLC miss, new block arriving) ===

(START: block arriving for fill)
  =>
<block predicted DEAD on arrival?>
  => YES => [skip fill — do NOT place in cache]
  => NO  => [fill into cache normally, mark with prediction bit]
  =>
(END)
```
