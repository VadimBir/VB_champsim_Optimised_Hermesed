# L2C MSHR Hashmap Proposal

## 1. Current Bottleneck

**L2C MSHR size:** 48 entries (`L2C_MSHR_SIZE` in `inc/defs.h:103`).

**Current lookup:** `check_mshr()` at `src/cache.cc:1746` does a branchless linear scan of all 48 MSHR entries. A bloom filter (`bloom[BLOOM_QTYPE_MSHR]`) provides early-exit for definite misses, but on bloom pass-through the full 48-iteration loop runs.

**Cost per call:** 48 iterations, each doing a 64-bit compare + branchless mask arithmetic. Called from:
- `handle_write` (cache.cc:525) — every WQ entry processed
- `handle_read` (cache.cc:670) — every RQ entry processed
- `handle_prefetch` (cache.cc:786) — every PQ entry processed
- `add_mshr` (cache.cc:1504) — every new miss insertion
- DERFILL paths (cache.h:1338, 1419) — bypass return data

**Eviction cost:** On MSHR entry removal, `bloom_rebuild_mshr()` (cache.h:190) rebuilds the entire bloom filter by scanning all 48 entries. This is O(MSHR_SIZE) per eviction.

**Why the LLC has a hashmap but L2C does not:** The LLC hashmap (`LlcMshrDirectMap`, cache.h:62) was added because LLC MSHR is `NUM_CPUS*64` = 256+ entries. L2C at 48 entries was considered "small enough" for linear scan. But L2C is hit far more frequently per cycle than LLC (every L1D miss goes to L2C), so the cumulative cost is significant.

## 2. Proposed Data Structure

Reuse the existing `LlcMshrDirectMap` template with a smaller capacity. For 48 entries, `next_pow2(2*48) = 128` buckets — same open-addressing scheme, same hash function.

```cpp
// inc/cache.h, near line 59 (after LLC_MSHR_DIRECT_MAP_CAP)

#define L2C_MSHR_DIRECT_MAP_CAP (llc_mshr_next_pow2(2u * (uint32_t)(L2C_MSHR_SIZE)))
// L2C_MSHR_SIZE=48 => 2*48=96 => next_pow2=128 buckets
// Load factor: 48/128 = 37.5% — low collision rate
```

No new struct needed. `LlcMshrDirectMap<128>` is the instantiation.

### Memory footprint

Per `LlcMshrDirectMap<128>`:
- `keys[128]`: 128 * 8 = 1024 bytes (alignas(64))
- `vals[128]`: 128 * 2 = 256 bytes (alignas(64))
- `size_`: 2 bytes
- **Total: ~1282 bytes per L2C instance**

With `NUM_CPUS` L2C instances (typically 1-4), this is 1.3-5.1 KB total. Trivial.

## 3. Hash Function

Same as LLC — shift+XOR fold, no modulo:

```cpp
static inline uint32_t hash(uint64_t key) {
    uint64_t x = key;
    x ^= x >> 11;
    return (uint32_t)((x >> 6) & MASK);  // MASK = 127 for 128 buckets
}
```

- `>> 6` skips the 64-byte cache-line offset bits (addresses are cache-line aligned)
- `>> 11` XOR fold mixes higher bits to defend against sequential page runs
- `& MASK` is a single AND — no modulo

### Collision analysis at 37.5% load factor

With 128 buckets and max 48 occupied, expected probe length is ~1.3 (open addressing, linear probing). Worst case with pathological clustering: ~4-5 probes. Still far better than 48-iteration linear scan.

## 4. Operations

### Lookup (replaces branchless linear scan)

```cpp
// In check_mshr(), for L2C path:
int32_t found = l2c_mshr_map.find(packet->address);
// find() does: hash → probe chain → return val or -1
// Average: 1-2 probes vs 48 iterations
```

### Insert (on new MSHR allocation)

```cpp
// src/cache.cc ~line 2013, inside add_mshr after MSHR.entry[index] = *packet:
#ifdef USE_L2C_HASHMAP_MSHR
    if (cache_type == IS_L2C)
        l2c_mshr_map.insert(MSHR.entry[index].address, index);
    else
#endif
```

### Remove (on MSHR eviction)

```cpp
// src/cache.cc ~line 395 and cache.h ~line 1486:
#ifdef USE_L2C_HASHMAP_MSHR
    if (cache_type == IS_L2C)
        l2c_mshr_map.erase(removed_addr);
    else
#endif
```

### Rebuild (bloom_rebuild_mshr fallback)

Already handled — `bloom_rebuild_mshr()` at cache.h:190 would get an L2C branch:

```cpp
#ifdef USE_L2C_HASHMAP_MSHR
    if (cache_type == IS_L2C) {
        l2c_mshr_map.clear();
        for (uint16_t i = 0; i < MSHR_SIZE; i++) {
            if (MSHR.entry[i].address != 0)
                l2c_mshr_map.insert(MSHR.entry[i].address, i);
        }
        return;
    }
#endif
```

## 5. Integration Points

### cache.h changes

| Line | Change |
|------|--------|
| ~59 | Add `#define L2C_MSHR_DIRECT_MAP_CAP` after LLC one |
| ~48 | Expand `CHECK_MSHR` macro to cover L2C: `(cache_type == IS_LLC ? check_mshr_hashmap(pkt) : cache_type == IS_L2C ? check_mshr_l2c_hashmap(pkt) : check_mshr(pkt))` |
| ~177 | Add `LlcMshrDirectMap<L2C_MSHR_DIRECT_MAP_CAP> l2c_mshr_map;` member in CACHE class |
| ~190 | Add L2C branch in `bloom_rebuild_mshr()` |
| ~500 | Declare `check_mshr_l2c_hashmap(PACKET *packet)` |

### cache.cc changes

| Line | Change |
|------|--------|
| ~395 | Add L2C erase branch (same pattern as LLC) |
| ~2013 | Add L2C insert branch (same pattern as LLC) |
| After 1889 | Add `check_mshr_l2c_hashmap()` function body — identical to `check_mshr_hashmap` but with L1 bypass logic included (LLC version omits it) |

### Alternatively: merge into existing `check_mshr_hashmap`

Since `check_mshr_hashmap` (cache.cc:1889) and a hypothetical L2C version would share 90% of code, a cleaner approach:

1. Make `check_mshr_hashmap` work for both LLC and L2C
2. Add a second `LlcMshrDirectMap` member for L2C, select at runtime:
   ```cpp
   auto& map = (cache_type == IS_LLC) ? mshr_map : l2c_mshr_map;
   int32_t found = map.find(packet->address);
   ```
3. The bypass-flag merge logic already handles both L1/L2/LLC ifdefs

**Trade-off:** One extra branch per call (LLC vs L2C map select) but eliminates code duplication. Given check_mshr_hashmap is already behind a dispatch macro, this is negligible.

## 6. What Becomes Unnecessary

With the hashmap for L2C:
- **Bloom filter for L2C MSHR** — no longer needed. The hashmap IS the fast lookup. The bloom adds a probabilistic layer that is strictly worse than a direct hashmap lookup. Can be disabled for L2C MSHR (keep bloom for other queue types if used).
- **Branchless scan loop** (cache.cc:1757-1765) — not reached for L2C anymore.
- **bloom_rebuild_mshr for L2C** — replaced by hashmap erase (O(1) amortized vs O(48) rebuild).

## 7. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| Hashmap-MSHR index desync (insert without erase or vice versa) | HIGH — silent wrong results | Same risk as LLC hashmap, which has been running stable. Mirror the exact same insert/erase guard pattern. |
| `address == 0` used as empty sentinel in MSHR but hashmap uses `EMPTY=0xFFFF` for vals | LOW | These are independent: MSHR uses address==0, hashmap uses vals[]==EMPTY. No conflict. |
| PF-to-demand takeover changes address? | NONE | Takeover at cache.cc:1803 copies packet over MSHR entry but address stays the same (same cache line). Hashmap key remains valid. |
| MSHR index reuse after remove_queue | LOW | `remove_queue` zeros the entry (address=0). Next `add_mshr` finds it by address==0 scan, assigns new address, inserts into hashmap. Old key was already erased. |
| `#ifdef BYPASS_L1_LOGIC` inside L2C hashmap path | MEDIUM | LLC's `check_mshr_hashmap` deliberately omits L1 bypass logic. L2C version MUST include it. If reusing single function, ensure ifdefs are present. |
| Open-addressing erase rehash cost | LOW | Erase does re-insert of contiguous chain. At 37.5% load factor, chains are typically 1-2 entries. Max observed rehash: ~5 entries. |
| Template bloat (two instantiations) | NEGLIGIBLE | One at CAP=128 (L2C), one at CAP=256+ (LLC). Compiler generates two copies of find/insert/erase — ~200 bytes of code each. |

## 8. Expected Speedup

- **Lookup:** 48-iteration branchless scan → 1-2 probe hashmap lookup. ~20-30x fewer memory accesses per lookup.
- **Eviction:** O(48) bloom rebuild → O(1) amortized hashmap erase. ~10-20x fewer operations.
- **Bloom overhead eliminated:** No bloom insert on MSHR add, no bloom rebuild on MSHR remove, no bloom check before lookup.
- **Net:** L2C MSHR operations are not the dominant bottleneck (DRAM latency dominates wall time), but this removes unnecessary work on a very hot path. Estimated 1-3% sim throughput improvement depending on MSHR pressure.

## 9. Build Guard

```cpp
// inc/cache.h
#define USE_L2C_HASHMAP_MSHR  // Enable L2C hashmap (disable to revert to bloom+linear)
```

Keep as separate define from `USE_LLC_HASHMAP_MSHR` so they can be independently toggled for A/B comparison.

## 10. Bit-Exact Verification

The hashmap is a pure lookup accelerator — it returns the same MSHR index that the linear scan would. To verify:

1. Build with both paths active (hashmap + linear scan)
2. Assert `hashmap_result == linear_scan_result` for every check_mshr call
3. Run 10M instructions on 3-4 traces
4. Remove assertion, keep hashmap-only path

This is the same verification strategy used for the LLC hashmap (v15_HSH C1).
