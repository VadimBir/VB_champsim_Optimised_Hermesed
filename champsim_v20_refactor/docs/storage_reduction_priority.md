# Storage Reduction Priority Book
Generated: 2026-03-24
RULE: No field removed without confirming zero read sites. Manager approves each before any edit.
EQUIVALENCY: removing a dead field = no read sites exist → behavior identical by definition.

---

## RANK 1 — BLOCK::tag (REDUNDANT, ~563 KB across all cache levels)

| Field | Type | Saving/instance | Total instances | Total saving |
|-------|------|----------------|-----------------|--------------|
| tag | uint64_t | 8 bytes | ~70,400 (1-CPU) | ~563 KB |

**Evidence:** `cache.cc:1643` sets `tag = packet->address`; `cache.cc:1644` sets `address = packet->address` — always identical.
**Hot-path impact:** `check_hit` at `cache.cc:1602` compares `block[set][way].tag == packet->address`. After removal: compare `block[set][way].address == address` — same value, same result.
**Why this matters:** `BLOCK` is 56 bytes. Removing `tag` → 48 bytes. LLC way-scan touches `ceil(48×16/64)` = 12 cache lines vs `ceil(56×16/64)` = 14 cache lines. That's a **14% reduction in cache lines touched per LLC lookup** — directly cuts `check_hit` miss rate.
**Change required:** remove `tag` field from BLOCK; replace `block[set][way].tag` with `block[set][way].address` in all read sites (only `check_hit`); remove write at `cache.cc:1643`.

---

## RANK 2 — BLOCK::instr_id + BLOCK::cpu (DEAD, ~844 KB)

| Field | Type | Saving/instance | Total saving |
|-------|------|----------------|--------------|
| instr_id | uint64_t | 8 bytes | ~563 KB |
| cpu | int | 4 bytes | ~281 KB |

**Evidence:** Written at `cache.cc:1647-1648` on every `fill_cache()` call. Zero read hits anywhere in codebase.
**Combined saving:** 12 bytes × 70,400 = ~844 KB + eliminates 2 writes per fill (saves write bandwidth into LLC block array).
**BLOCK size after rank1+rank2:** 56 - 8 - 8 - 4 = **36 bytes**. Way-scan: `ceil(36×16/64)` = 9 cache lines — **36% fewer cache lines than current 14**.

---

## RANK 3 — ooo_model_instr::source_virtual_address[4] + destination_virtual_address[2] (DEAD, ~48 KB ROB)

| Field | Type | Saving/instance | Instances | Total saving |
|-------|------|----------------|-----------|--------------|
| source_virtual_address[4] | uint64_t[4] | 32 bytes | 512 (ROB/CPU) | 32 KB/CPU |
| destination_virtual_address[2] | uint64_t[2] | 16 bytes | 512 | 16 KB/CPU |

**Evidence:** `ooo_cpu.cc:376` writes source_virtual_address; `ooo_cpu.cc:349` writes destination_virtual_address. Zero reads anywhere.
**ROB impact:** ROB entries are pointer-chased in `update_rob` (25.77% of runtime). Smaller ooo_model_instr = fewer cache lines per ROB entry access.

---

## RANK 4 — BLOCK::l1_bypassed, l2_bypassed, llc_bypassed (DEAD, ~211 KB)

| Field | Type | Saving/instance | Total saving |
|-------|------|----------------|--------------|
| l1_bypassed | uint8_t | 1 byte | ~70 KB |
| l2_bypassed | uint8_t | 1 byte | ~70 KB |
| llc_bypassed | uint8_t | 1 byte | ~70 KB |

**Evidence:** Never read from `block[][]` anywhere in .cc files. Bypass state tracked on PACKET (not BLOCK).
**Note:** With tag+instr_id+cpu already removed (ranks 1+2), removing these 3 bytes brings BLOCK to 33 bytes. Padding to 40 bytes (next 8-byte boundary) means only 7 bytes saved in practice without reordering. **Consider reordering fields alongside this change** to achieve 32-byte BLOCK (1 full cache line per 2 ways).

---

## RANK 5 — PACKET::l1_bypassed + l2_bypassed + llc_bypassed → bitfield (2 bytes saved)

| Current | Proposed | Saving |
|---------|----------|--------|
| 3× uint8_t (3 bytes) | 1× uint8_t with 3 bits | 2 bytes |

**Note:** PACKET is used in queues (RQ/WQ/PQ/MSHR), not in the block array. Saving is per-queue-entry. Lower priority than BLOCK savings but reduces MSHR scan cost.

---

## RANK 6 — ooo_model_instr::num_reg_ops + others → int8_t (9 bytes)

| Field | Current | Proposed | Saving |
|-------|---------|----------|--------|
| num_reg_ops | int (dead) | remove | 4 bytes |
| num_mem_ops | int | int8_t | 3 bytes |
| num_reg_dependent | int | int8_t | 3 bytes |

**Evidence:** num_reg_ops written but never read. num_mem_ops/num_reg_dependent go negative (subtracted in ooo_cpu.cc:1105, 1535) → int8_t range (-128..127) sufficient.

---

## Implementation Order (requires manager approval per rank)

1. **RANK 1+2 together** — BLOCK dead/redundant fields (tag, instr_id, cpu, l1/l2/llc_bypassed). Do as one pass. Reorder fields for optimal alignment to hit 32 or 40-byte boundary.
2. **RANK 3** — ooo_model_instr virtual address arrays.
3. **RANK 4+5** — PACKET bitfield + ooo_model_instr int→int8_t.

**BEFORE ANY EDIT:** run `./qcheck_consistency.sh` to establish baseline. After each rank: run again and confirm PASS.

---

## Field removal checklist (per field, per worker)
- [ ] Grep for ALL read sites in all .cc and .h files
- [ ] Confirm zero reads (or list each read and explain why safe to remove)
- [ ] State equivalency: "field X is never read → removing it produces identical observable output"
- [ ] Remove field declaration
- [ ] Remove all write sites
- [ ] Compile and fix any errors
- [ ] Run qcheck_consistency.sh — must PASS
