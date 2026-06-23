# Perceptron Latency Predictor — Design Doc

## Goal
Predict miss-to-fill latency per cache level. Bypass when predicted latency exceeds dynamic threshold.

## Feature Vector (N_FEAT = 7)

| idx | Feature | Raw range | Normalization |
|-----|---------|-----------|---------------|
| 0 | kappa (reuse distance estimate) | 0..1 | already normalized |
| 1 | CAMAT (current avg memory access time) | 0..~500 cycles | x / 512.0 |
| 2 | MSHR occupancy ratio | 0..1 | mshr_used / MSHR_SIZE |
| 3 | PC hash bucket (low) | 0..63 | (pc>>2) & 0x3F / 63.0 |
| 4 | PC hash bucket (high) | 0..63 | (pc>>8) & 0x3F / 63.0 |
| 5 | cache_type encoding | {L1=0, L2=0.5, LLC=1.0} | direct |
| 6 | bias | 1.0 | constant |

Two PC features (low/high hash) give the perceptron XOR-like capacity for PC-correlated patterns without a full table.

## Weight Table

Single `Perceptron<7>` instance per cache level (3 instances total).
- 7 weights x 2 bytes = 14 bytes per perceptron
- 3 levels x 14 = **42 bytes** for weights

No hashed weight tables needed; feature count is small enough for a single weight vector.

## Bypass History Side Table

Bypassed packets skip MSHR, so miss-timestamp is lost. Solution: small ring buffer.

```cpp
struct BypassHistoryEntry {
    uint64_t addr;        // block address (tag)
    uint64_t cycle_sent;  // cycle when bypass decision made
    uint8_t  cache_level; // which level was bypassed
    bool     valid;
};

// 256 entries, ~2.5 KB
BypassHistoryEntry bypass_history[256];
uint8_t bh_head = 0; // ring pointer
```

Lookup: on fill arrival, linear scan (256 is fine at fill rate ~1/100 cycles). Match on addr + cache_level. Extract `actual_latency = current_cycle - cycle_sent`.

## Training Signal

**For non-bypassed packets**: `actual_latency = fill_cycle - mshr_entry.cycle_enqueued`
**For bypassed packets**: `actual_latency = fill_cycle - bypass_history[match].cycle_sent`

Training is regression-style: convert to binary label relative to dynamic threshold.

```
label = (actual_latency > threshold) ? +1 : -1
```

+1 means "latency was high, bypass was correct". -1 means "latency was low, should not bypass".

## Dynamic Threshold

Per-level exponential moving average of observed latencies:

```cpp
// Per cache level
uint64_t ema_latency[3] = {0, 0, 0};  // fixed-point x256

void update_ema(int level, uint64_t actual) {
    // alpha = 1/16 → shift
    ema_latency[level] = ema_latency[level] - (ema_latency[level] >> 4)
                       + (actual << 4);  // x256 fixed point
}

uint64_t get_threshold(int level) {
    return (ema_latency[level] >> 8) + MARGIN;
    // MARGIN: tunable, start at 0 (bypass when above average)
}
```

**8 bytes x 3 levels = 24 bytes.**

## Inference Pseudocode

```cpp
// At miss decision point in cache.cc
double feat[7];
feat[0] = kappa;
feat[1] = min(camat / 512.0, 1.0);
feat[2] = (double)mshr_used / MSHR_SIZE;
feat[3] = ((pkt.ip >> 2) & 0x3F) / 63.0;
feat[4] = ((pkt.ip >> 8) & 0x3F) / 63.0;
feat[5] = (cache_type == IS_L1D) ? 0.0 : (cache_type == IS_L2C) ? 0.5 : 1.0;
feat[6] = 1.0;

int32_t quant[7];
int32_t score = latency_perc[level].infer(feat, quant);

bool should_bypass = (score > 0);  // positive = high latency predicted

if (should_bypass) {
    // record in bypass_history
    bypass_history[bh_head] = {pkt.address >> LOG2_BLOCK_SIZE,
                                current_cycle, level, true};
    bh_head = (bh_head + 1) & 0xFF;
    // set bypass bit, skip MSHR, forward downstream
}
```

## Training Pseudocode

```cpp
// On fill/return_data arrival
uint64_t actual_latency;
bool found = false;

// Check bypass history first
for (int i = 0; i < 256; i++) {
    if (bypass_history[i].valid &&
        bypass_history[i].addr == (pkt.address >> LOG2_BLOCK_SIZE) &&
        bypass_history[i].cache_level == level) {
        actual_latency = current_cycle - bypass_history[i].cycle_sent;
        bypass_history[i].valid = false;
        found = true;
        break;
    }
}

// Or from MSHR timestamp (non-bypassed)
if (!found && mshr_hit) {
    actual_latency = current_cycle - mshr_entry.cycle_enqueued;
    found = true;
}

if (found) {
    update_ema(level, actual_latency);
    uint64_t thresh = get_threshold(level);
    int8_t label = (actual_latency > thresh) ? +1 : -1;

    // Re-extract features (stored via snapshot at inference time)
    latency_perc[level].train(label, /*theta=*/32);
}
```

Note: `snapshot()` must be called at inference time to save `prev_quant` and `prev_sum` for the subsequent `train()` call. For bypassed packets, snapshot is stored in a parallel array indexed by `bh_head`. For MSHR packets, snapshot is stored alongside MSHR entry (add `prev_sum`/`prev_quant` fields to MSHR or a shadow table).

## HW Budget Estimate

| Component | Size |
|-----------|------|
| Weight tables (3 perceptrons x 7 x 2B) | 42 B |
| Prev_quant snapshots (3 x 7 x 4B) | 84 B |
| Bypass history table (256 x 18B) | ~4.5 KB |
| EMA counters (3 x 8B) | 24 B |
| MSHR snapshot shadow (per MSHR entry, 32B each, 16 entries) | 512 B |
| **Total** | **~5.2 KB** |

Realistic for L2/LLC-class structures. Well under 8 KB budget.

## Integration Points

1. **Inference**: `cache.cc` at miss-handling path, inside `#ifdef BYPASS_L{1,2,LLC}_LOGIC` blocks, after existing bypass condition check. Latency predictor is an additional gate: `existing_bypass_decision && (latency_score > 0)`.
2. **Training**: `cache.cc` at `return_data()` / fill path. Match packet to either MSHR or bypass_history. Compute label. Call `train()`.
3. **Snapshot storage**: Extend MSHR entry struct with 32B snapshot (or use a `uint16_t snapshot_idx` pointing into a fixed shadow table).

## Key Design Decisions

- **Regression via classification**: Rather than predicting raw cycle count, we classify above/below dynamic threshold. This matches the existing `Perceptron<>` train interface ({+1,-1} labels) and avoids needing a regression output head.
- **Dynamic threshold via EMA**: Adapts to workload phase changes. No hardcoded cycle counts.
- **PC split into two hash buckets**: Avoids needing a large hashed perceptron table. Two 6-bit slices give 12 bits of PC entropy within a 7-feature vector.
- **Single perceptron per level, not per-PC**: Keeps HW budget tiny. PC information is embedded in features, not used for table indexing.
