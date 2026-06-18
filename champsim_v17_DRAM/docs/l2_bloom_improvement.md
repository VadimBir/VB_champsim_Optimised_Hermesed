# L2C Bloom Filter Analysis & Improvement Proposals

## What it does

MSHR early-exit filter. Before linear-scanning L2C MSHR (48 entries), bloom rejects guaranteed non-members in O(1). Avoids the O(48) scan on negative lookups.

LLC uses a hashmap instead (`USE_LLC_HASHMAP_MSHR`); L1D/L2C/TLBs use bloom.

## Where defined

- **Struct**: `inc/cache.h:30-42` — `DualBloom128`
- **Instance**: `inc/cache.h:172` — `DualBloom128 bloom[BLOOM_QTYPE_COUNT]`
- **Rebuild**: `inc/cache.h:190-206` — `bloom_rebuild_mshr()` — full reset+reinsert on every MSHR remove
- **Insert**: `src/cache.cc:2016` — on new MSHR entry allocation
- **Check**: `src/cache.cc:1746-1766` — `check_mshr()` bloom gate
- **Stats**: `inc/cache.h:161` — `bloom_check_total`, `bloom_reject`, `bloom_pass_hit`, `bloom_pass_miss`

## Current Parameters

| Parameter | Value |
|-----------|-------|
| Filter type | Dual independent 64-bit (DualBloom128) |
| Total bits | 128 (2 x 64) |
| Hash functions | 2 (one per 64-bit filter) |
| Bits set per insert | 1 per filter (2 total) |
| Max insertions (L2C) | 48 (L2C_MSHR_SIZE) |
| Clear interval | Every MSHR remove (full rebuild) |

### Hash functions

```c
hash_a(addr) = 1ULL << (((addr << 7) ^ addr) & 63);   // 1 bit in [0..63]
hash_b(addr) = 1ULL << (((addr << 17) ^ (addr >> 3)) & 63);  // 1 bit in [0..63]
```

## Why ~50% False Positive Rate

### Mathematical analysis

For a single 64-bit filter with k=1 hash, after n=48 insertions:

- Prob(bit j is set) = 1 - (63/64)^48 = 1 - 0.473 = **0.527 (52.7%)**
- Single-filter FPR = 52.7%
- Dual-filter FPR (both must agree) = 0.527^2 = **27.8% theoretical minimum**

### Why user sees ~50%

The theoretical 27.8% assumes perfectly uniform hashing. In practice:

1. **Address correlation**: Cache-line addresses share upper bits (same page), making `(addr << 7) ^ addr` and `(addr << 17) ^ (addr >> 3)` produce correlated bit positions across nearby addresses. Working sets within a page all hash to similar positions.

2. **Only 6 bits of entropy used**: `& 63` means only bits [0:5] of the hash output matter. The XOR-shift hashes don't spread real address entropy well into those low bits.

3. **Rebuild doesn't help**: Rebuild happens on every MSHR remove, but the filter immediately repopulates with the remaining 47 entries — still near-saturated.

4. **bloom_pass_miss = false positives**: Bloom said "maybe yes" but linear scan found nothing. At 48 entries in 64 bits, ~33 of 64 bits are set in each filter, so random addresses hit set bits frequently.

### Core problem

**64 bits is too small for 48 entries with k=1.** Optimal bloom filter for n=48, FPR=1% needs ~460 bits with k=7 hashes. Current 128-bit dual design is 3.6x undersized.

## Proposed Improvements

### Option A: Wider bloom (drop-in, no struct change needed)

Replace `uint64_t bits_a, bits_b` with wider arrays. Simplest effective change.

**File**: `inc/cache.h:30-42`

```cpp
// Option A: 4x256-bit bloom (1024 bits total, 4 independent filters)
struct QuadBloom1024 {
    uint64_t bits[4][4]; // 4 filters x 256 bits each

    static inline void hash(uint64_t addr, uint32_t filter, uint64_t &word, uint64_t &bit) {
        // Different mixing per filter using golden-ratio-derived constants
        static constexpr uint64_t mix[4] = {
            0x9E3779B97F4A7C15ULL, 0x517CC1B727220A95ULL,
            0x6C62272E07BB0142ULL, 0x62B821756295C58DULL
        };
        uint64_t h = addr * mix[filter];
        h ^= h >> 17;
        word = (h >> 8) & 3;   // which of 4 uint64_t words
        bit  = 1ULL << (h & 63); // which bit in that word
    }

    inline void reset() { memset(bits, 0, sizeof(bits)); }
    inline void insert(uint64_t addr) {
        for (uint32_t f = 0; f < 4; f++) {
            uint64_t w, b; hash(addr, f, w, b);
            bits[f][w] |= b;
        }
    }
    inline bool maybe_contains(uint64_t addr) const {
        for (uint32_t f = 0; f < 4; f++) {
            uint64_t w, b; hash(addr, f, w, b);
            if (!(bits[f][w] & b)) return false;
        }
        return true;
    }
};
```

**Math**: 1024 bits, k=4, n=48 → FPR = (1 - e^(-4*48/1024))^4 = (0.171)^4 = **0.085%**

Storage: 128 bytes per cache instance. L2C has 1 per core. Negligible.

### Option B: Use hashmap for L2C (like LLC already does)

LLC already uses `LlcMshrDirectMap` — O(1) lookup, zero false positives.

**File**: `inc/cache.h:48`

Change dispatch to also use hashmap for L2C:

```cpp
// Current:
#define CHECK_MSHR(pkt) (cache_type == IS_LLC ? check_mshr_hashmap(pkt) : check_mshr(pkt))

// Proposed:
#define CHECK_MSHR(pkt) ((cache_type == IS_LLC || cache_type == IS_L2C) ? check_mshr_hashmap(pkt) : check_mshr(pkt))
```

Also need: instantiate `mshr_map` for L2C (currently only `#ifdef USE_LLC_HASHMAP_MSHR` in LLC path).

**Requires**: `bloom_rebuild_mshr()` at line 190-206 to also do hashmap rebuild for L2C, and insert at line 2016 to do hashmap insert for L2C.

**Tradeoff**: 0% FPR. Storage = `next_pow2(2*48) = 128` entries x (8-byte key + 2-byte val) = ~1.3 KB per L2C instance. Still small.

### Option C: L2C-specific larger DualBloom (minimal change)

Keep DualBloom128 for L1D (16 entries, FPR is fine), but give L2C a bigger one.

**File**: `inc/cache.h:172`

```cpp
// Per-level bloom sizing
#if L2C_MSHR_SIZE > 32
    DualBloom512 bloom_l2c[BLOOM_QTYPE_COUNT];  // 512-bit dual for L2C
#endif
    DualBloom128 bloom[BLOOM_QTYPE_COUNT];       // 128-bit for L1D/TLBs
```

With 512-bit dual (2 x 256), k=1, n=48: FPR = (1-(255/256)^48)^2 = (0.171)^2 = **2.9%**

### Recommendation

**Option B (hashmap for L2C)** is best:
- Zero false positives
- Code already exists for LLC — minimal new code
- ~1.3 KB overhead is nothing
- Eliminates bloom_rebuild_mshr full-scan cost for L2C too (hashmap erase is O(1))

**Option A** is second-best if you want to keep the bloom approach for some reason (e.g., cache-line alignment concerns with hashmap).

## Code locations requiring changes (Option B)

1. `inc/cache.h:48` — extend `CHECK_MSHR` macro to include `IS_L2C`
2. `inc/cache.h:174-178` — instantiate `mshr_map` for L2C (or make conditional on `IS_LLC || IS_L2C`)
3. `inc/cache.h:190-206` — `bloom_rebuild_mshr()`: add L2C hashmap path
4. `src/cache.cc:393-398` — handle_fill MSHR remove: add L2C hashmap erase path
5. `src/cache.cc:2012-2016` — MSHR insert: add L2C hashmap insert path
6. `inc/cache.h:59` — define `L2C_MSHR_DIRECT_MAP_CAP` (next_pow2(2*48) = 128)

## L1D bloom is fine

L1D_MSHR_SIZE = 16. With 64-bit dual bloom:
- Prob(bit set) = 1-(63/64)^16 = 0.222
- Dual FPR = 0.222^2 = **4.9%** — acceptable.

Only L2C (48 entries in 64-bit filter) has the saturation problem.
