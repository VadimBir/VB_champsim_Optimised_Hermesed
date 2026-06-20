# Hermes TTP & HMP — Off-Chip Predictors
**Source:** Hermes simulator codebase (Bera et al., arXiv 2022 — "Hermes: Accelerating long-latency load requests via perception-based off-chip load prediction")

---

## 1. What is TTP (Tag-Tracking Predictor)?

TTP predicts whether a load will **go off-chip (hit DRAM)** or **stay on-chip (hit in some cache level)**. It answers: "Will this address be found in the cache hierarchy, or will we have to go all the way to DRAM?"

**How it works:** TTP maintains a **catalog cache** — a small set-associative structure that stores partial tags of addresses that are currently in the cache hierarchy. Think of it as a compressed address book: "these addresses are cached somewhere."

- **Tag in catalog → predict ON-CHIP** (will hit in caches)
- **Tag NOT in catalog → predict OFF-CHIP** (will go to DRAM)

The catalog is trained reactively:
- When a load hits in cache (didn't go off-chip): add its tag to the catalog (it's cached)
- When a load goes to DRAM (went off-chip): remove its tag from the catalog (it's not cached)
- When a line is evicted from LLC: remove its tag from the catalog (no longer cached)

**Why it works:** The catalog approximates the set of addresses currently resident in the cache hierarchy. If an address is in the catalog, it's likely still in a cache somewhere. If it's not, the data has either been evicted or was never cached → DRAM access likely.

---

## 2. What is HMP (History-based Miss Predictor)?

HMP-Local predicts off-chip behavior using **per-PC local history** — a pattern of recent off-chip/on-chip outcomes for each load instruction.

It has two structures:
- **LHR (Local History Register):** A table indexed by hashed PC. Each entry stores an N-bit history of recent off-chip outcomes for that PC. With history_length=8, each entry is 8 bits recording the last 8 outcomes (1=off-chip, 0=on-chip).
- **PHT (Pattern History Table):** Indexed by the history pattern itself. Each entry is a 2-bit saturating counter (STRONG_NT, WEAK_NT, WEAK_T, STRONG_T). "T" = predict off-chip, "NT" = predict on-chip.

**How it works:** For a load from PC=X:
1. Hash PC → index into LHR → get 8-bit history (e.g., `01100110`)
2. Use that history as index into PHT → get confidence counter
3. If counter ≥ WEAK_T → predict off-chip

**Insight:** Individual load instructions have patterns in their off-chip behavior. A load that goes to DRAM in a pattern (e.g., every other time, or in bursts) can be predicted by learning the pattern via local history.

---

## 3. How do they work end-to-end?

### TTP: Load from issue to completion

**At load issue (in the out-of-order pipeline):**
1. Load instruction enters LSQ with virtual address
2. `offchip_pred->predict(arch_instr, data_index, lq_entry)` is called
3. TTP computes `partial_tag = folded_xor(vaddr >> 6, 2)`, then hashes via HashZoo (type 5), masks to 16 bits
4. `set = partial_tag % 1024`
5. Search `catalog_cache[set]` for matching partial_tag
6. Found → predict on-chip (return false). Not found → predict off-chip (return true).
7. Prediction stored in LQ entry for later use by prefetch scheduling

**At load completion (retirement):**
1. `offchip_pred->train(arch_instr, data_index, lq_entry)` is called
2. Ground truth: `lq_entry->went_offchip` — did this load actually go to DRAM?
3. If NOT off-chip (cache hit): add tag to catalog (or do nothing if already there). On insert, if catalog set is full, LRU-evict oldest entry (also clean up reverse index).
4. If off-chip (DRAM): remove tag from catalog if present (prediction was wrong — data wasn't cached).

**On LLC eviction (reactive cleanup):**
1. `offchip_pred->track_llc_eviction(paddr)` called
2. Compute physical partial tag
3. Look up reverse index → find which catalog set contains this physical tag
4. Delete from both catalog and reverse index
5. This keeps the catalog consistent: when data leaves LLC, its catalog entry is removed

### HMP-Local: Load from issue to completion

**At load issue:**
1. `predict(arch_instr, ...)` called
2. `lhr_index = folded_xor(PC, 2) hashed via HashZoo(type 2) % 2038`
3. `local_history = LHR[lhr_index]` (8-bit pattern of recent outcomes)
4. `conf_state = PHT[local_history]` (2-bit counter)
5. If conf_state ≥ WEAK_T → predict off-chip

**At load completion:**
1. `train(arch_instr, ...)` called
2. Get old history and old confidence
3. Update confidence via 2-bit saturating FSM:
   - STRONG_NT + offchip → WEAK_NT
   - WEAK_NT + offchip → WEAK_T
   - WEAK_T + offchip → STRONG_T
   - STRONG_T + offchip → STRONG_T
   - STRONG_NT + hit → STRONG_NT
   - WEAK_NT + hit → STRONG_NT
   - WEAK_T + hit → WEAK_NT
   - STRONG_T + hit → WEAK_T
4. Update history: `new_history = (old_history << 1) | went_offchip` masked to 8 bits
5. Write back: `PHT[old_history] = new_conf`, `LHR[lhr_index] = new_history`

---

## 4. What is the secret sauce?

**TTP:** The catalog directly approximates cache contents. Instead of learning complex patterns, it just remembers "what's in the cache." Simple but effective because the catalog closely mirrors actual cache state. The LLC eviction tracking keeps it accurate.

**HMP-Local:** Per-PC behavioral patterns repeat. A load that alternates DRAM/cache has a recognizable history signature. The PHT learns which history patterns lead to off-chip access. It's the same idea as branch prediction — local history captures instruction-level temporal patterns.

---

## 5. When are predictions made?

Predictions are made **at load issue time** (when the load enters the LSQ, before the cache hierarchy is accessed). This is early enough to:
- Prioritize prefetch requests based on predicted DRAM accesses
- Schedule memory-level parallelism (MLP) — issue predicted off-chip loads early
- Guide Hermes's DRAM-side prefetching (DDRP) — predicted off-chip loads can be prefetched directly from DRAM

Training happens **at load completion** (retirement/commit), when ground truth (did it actually go off-chip?) is known.

---

## 6. Training mechanism

**TTP training:**
- Cache hit: Add address tag to catalog → "this address is cached"
- DRAM access: Remove address tag from catalog → "this address is NOT cached"
- LLC eviction: Remove evicted address from catalog → keep catalog consistent
- The catalog is self-correcting: wrong entries get removed by ground truth

**HMP-Local training:**
- Standard 2-bit saturating counter FSM (same as branch prediction)
- History shifted left, new outcome appended
- PHT initialized to WEAK_T (slightly biased toward predicting off-chip)
- Takes 2 consecutive same-direction outcomes to reach STRONG state

---

## 7. Hardware cost

### TTP
| Component | Size |
|-----------|------|
| Catalog cache | 1024 sets × 24-way × (16-bit vtag + 16-bit ptag) = **96 KB** |
| Reverse index | Hash map of ptag → set (variable, ~24K entries max) | 
| Total | ~96-120 KB |

### HMP-Local
| Component | Size |
|-----------|------|
| LHR | 2038 entries × 8 bits = **~2 KB** |
| PHT | 2^8 = 256 entries × 2 bits = **64 bytes** |
| Total | **~2 KB** |

HMP-Local is dramatically smaller than TTP. The ensemble variant combines local+gshare+gskew for more accuracy at higher cost.

---

## 8. What is the reverse index in TTP?

The **reverse index** (`catalog_cache_rev_index`) is a map from **physical partial tag → catalog set number**. It exists to solve one problem: LLC eviction tracking.

When the LLC evicts a line, it provides the **physical address**. But the catalog is indexed by **virtual address** partial tag. The reverse index lets TTP find the catalog entry from a physical address:
1. LLC evicts physical address → compute physical partial tag
2. Look up reverse index: physical tag → which catalog set?
3. Search that catalog set for matching physical tag in the second field (each entry stores both virtual and physical partial tags)
4. Delete the entry

Without the reverse index, TTP would have to search ALL 1024 × 24 catalog entries on every LLC eviction — too slow.

---

## 9. Are these bypass predictors or off-chip predictors?

**They are off-chip predictors, NOT bypass predictors.** They predict WHERE data will be found (cache vs DRAM), not WHETHER to cache data.

In Hermes, they're used for:
- **Prefetch request prioritization:** Predicted off-chip loads get higher priority
- **DRAM-side prefetching (DDRP):** Hermes pre-positions data in a small DRAM-side buffer for predicted off-chip loads
- **MLP optimization:** Issue predicted off-chip loads earlier to overlap DRAM latency

**Could they be reused as bypass signals?** YES — with this mapping:
- "Predicted off-chip" → "data not in caches" → "even if we cache it, it might just be evicted again" → **bypass candidate**
- "Predicted on-chip" → "data is in caches" → "worth keeping in cache hierarchy" → **do not bypass**

This is a natural fit: if TTP says "this address is NOT in the catalog" (predicted off-chip), your bypass system could take that as a signal that caching this data hasn't been productive → bypass.

---

## 10. Critical insights for your bypass system

**Direct reuse opportunities:**
- TTP's catalog concept could feed directly into your bypass predicate: "is this address in the off-chip predictor's catalog?" If not → likely DRAM-bound → bypass candidate
- HMP-Local's per-PC history is very cheap (~2KB) and could be added as a feature to your bypass perceptron

**Key differences to account for:**
- TTP/HMP predict off-chip for the CURRENT cache hierarchy state. Your bypass CHANGES that state (bypassed data isn't cached, which changes future off-chip predictions). There's a feedback loop.
- They predict at load issue (before cache access). Your bypass decides at cache miss time (after cache access). The prediction timing is different.
- Hermes uses predictions for scheduling, not caching decisions. Using them for bypass is a new application.

**Combinability:** Hermes predictors are orthogonal to your bypass. You could run BOTH: Hermes predicts off-chip for prefetch scheduling, your bypass decides caching. They share the same signal (off-chip probability) but act on it differently.

---

## 11. All HMP variants

| Variant | Mechanism | Key difference |
|---------|-----------|---------------|
| **HMP-Local** | Per-PC local history (LHR + PHT) | Each PC has independent history |
| **HMP-Gshare** | Global shared history XORed with PC | All PCs share one global history register; captures cross-PC patterns |
| **HMP-Gskew** | Global history with 5 skewed hash tables | Like gshare but with multiple hash functions to reduce aliasing (same idea as SDBP's skewed predictor) |
| **HMP-Ensemble** | Combination of local + gshare + gskew | Tournament-style: picks the predictor that has been most accurate recently |

---

## Flowcharts

### TTP Flowchart
```
=== PREDICTION (at load issue) ===

(START: load enters LSQ with virtual_address)
  =>
[vcladdr = virtual_address >> 6]
[partial_tag = hash(folded_xor(vcladdr, 2)) & ((1 << 16) - 1)]
[set = partial_tag % 1024]
  =>
/search catalog_cache[set] for entry with first == partial_tag/
  =>
<tag found?>
  => YES => [predict ON-CHIP — will hit in caches] => return false
  => NO  => [predict OFF-CHIP — will go to DRAM] => return true
  =>
(END — prediction stored in LQ entry)

=== TRAINING (at load completion) ===

(START: load completes, ground truth known)
  =>
[v_partial_tag = hash(vcladdr)]
[p_partial_tag = hash(pcladdr)]
[set = v_partial_tag % 1024]
  =>
<went_offchip?>
  => NO (hit in cache) =>
    /search catalog_cache[set] for v_partial_tag/
    <found?>
      => YES => (do nothing — already tracked)
      => NO  =>
        <catalog_cache[set].size >= 24?>
          => YES =>
            [victim = LRU entry (front of deque)]
            /delete victim.p_partial_tag from rev_index/
            [erase victim from catalog_cache[set]]
        [push_back (v_partial_tag, p_partial_tag) into catalog_cache[set]]
        /add p_partial_tag → set to rev_index/
  => YES (went to DRAM) =>
    /search catalog_cache[set] for v_partial_tag/
    <found?>
      => YES =>
        /delete p_partial_tag from rev_index/
        [erase entry from catalog_cache[set]]
      => NO => (do nothing)
  =>
(END)

=== LLC EVICTION TRACKING ===

(START: LLC evicts line at physical_address)
  =>
<ocp_ttp_enable_track_llc_eviction enabled?>
  => NO => (skip)
  => YES =>
    [p_partial_tag = hash(paddr >> 6)]
    /lookup rev_index for p_partial_tag/
    <found?>
      => NO => (skip — not tracked)
      => YES =>
        [set = rev_index[p_partial_tag]]
        /search catalog_cache[set] for entry with second == p_partial_tag/
        <found?>
          => YES =>
            /delete p_partial_tag from rev_index/
            [erase entry from catalog_cache[set]]
          => NO => (inconsistency — rev_index stale)
    =>
(END)
```

### HMP-Local Flowchart
```
=== PREDICTION (at load issue) ===

(START: load at PC enters LSQ)
  =>
[lhr_index = hash(folded_xor(PC, 2)) % 2038]
  =>
/read LHR[lhr_index] => local_history (8-bit pattern)/
  =>
/read PHT[local_history] => conf_state (2-bit)/
  =>
<conf_state >= WEAK_T?>
  => YES => [predict OFF-CHIP] => return true
  => NO  => [predict ON-CHIP] => return false
  =>
(END)

=== TRAINING (at load completion) ===

(START: load completes, went_offchip known)
  =>
[lhr_index = hash(folded_xor(PC, 2)) % 2038]
/read old_history = LHR[lhr_index]/
/read old_conf = PHT[old_history]/
  =>
[update confidence via saturating FSM:]
  STRONG_NT + offchip => WEAK_NT
  WEAK_NT   + offchip => WEAK_T
  WEAK_T    + offchip => STRONG_T
  STRONG_T  + offchip => STRONG_T
  STRONG_NT + onchip  => STRONG_NT
  WEAK_NT   + onchip  => STRONG_NT
  WEAK_T    + onchip  => WEAK_NT
  STRONG_T  + onchip  => WEAK_T
  =>
/PHT[old_history] = new_conf/
  =>
[new_history = (old_history << 1) | went_offchip]
[new_history = new_history & 0xFF]  (mask to 8 bits)
/LHR[lhr_index] = new_history/
  =>
(END)
```

---

## TTP Knobs (defaults)
- `ocp_ttp_partial_tag_size = 16` — 16-bit partial tags
- `ocp_ttp_catalog_cache_sets = 1024` — catalog has 1024 sets
- `ocp_ttp_catalog_cache_assoc = 24` — 24-way associative
- `ocp_ttp_hash_type = 5` — HashZoo hash function #5
- `ocp_ttp_enable_track_llc_eviction = true` — clean up catalog on LLC evictions

## HMP-Local Knobs (defaults)
- `ocp_hmp_local_history_length = 8` — 8-bit local history per PC
- `ocp_hmp_local_lhr_size = 2038` — 2038-entry LHR table
- `ocp_hmp_local_lhr_index_hash_type = 2` — HashZoo hash function #2

## Key source files
- `~/Hermes/src/offchip_pred_ttp.cc` — TTP implementation
- `~/Hermes/src/offchip_pred_hmp_local.cc` — HMP-Local implementation
- `~/Hermes/inc/offchip_pred_base.h` — base class interface
- `~/Hermes/src/offchip_pred.cc` — dispatcher (selects predictor by knob)
- `~/Hermes/src/knobs.cc` — all knob defaults
- `~/Hermes/src/ooo_cpu.cc` — predict at issue, train at retire
