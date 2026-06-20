# RECOVERED — not applied here (compact)

Genuine gaps from the lost remote campaign (`/home/cc/champsim_VB`, lost 2026-06-10) still
missing in `champsim_v18_TEST_OPT`. **Excluded:** already-applied (P8 substance, P9D, P-BLOOM,
the 14 v18 opts), no-op-here (P19B, D1 — sites absent), and reversed/rejected items.
**Blueprint-only — no diffs survived; every port is a re-implementation from spec that MUST be
re-gated bit-exact** (`;0.51803;` 16c + 16× checksum `140039767200773` + LPM ROI diff 0).

| # | gap | file(s) | benefit | effort | note |
|---|---|---|---|---|---|
| **P11 ★** | O3 hoists/prefetch in `exec_mem`/`sched_mem`/`update_rob` | `ooo_cpu.cc` | ~1% host (298→295s) | MED | **BEST PICK** — real, behavior-preserving; checklist `0609_0607` names hoists; conflicts P15/P21/P24 region |
| data-packings | BLOCK 48→32B, lru/pmc/RTE-RTL-RTS/BANK_REQUEST narrowings + PACKET union | `block.h`,`ooo_cpu.h`,`memory_class.h` | footprint (~16MB@16c), not IPC | LOW-MED /item | "VERIFIED" is a **v15 static audit, never gated** — re-audit ranges/callsites + gate each. ⚠ **EXCLUDE `producer_id` removal — FALSE: `LSQ_ENTRY.producer_id` (block.h:710) is LIVE, read 6× in `ooo_cpu.cc` (1291/1297/1324/1332/1588/2116). The v15 "dead" claim was the same-named `ooo_model_instr` field — a name-collision footgun.** PACKET union also unverified (3 fields live). |
| cache-split | split `cache.cc/.h` + `ooo_cpu.cc` into per-queue TUs | many | **none** (maintainability) | HIGH | design-only blueprint, never executed; defer |
| Wave-3 parked | `B2`, `C7`, `A6/A3/A1` (remote slice labels) | `ooo_cpu.cc`,`instruction.h` | — | — | parked remotely, never accepted-and-kept |
| P-FASTSET | fastset LQ/SQ small-set path | `cache.cc/.h` | unknown | — | **content unrecoverable** from corpus |

**Caveats:** data-packings' kept/applied status is UNRESOLVED (no gate evidence); **v20 never
existed as code** (only plans — cache-split, GAP bank-bucket A→E); recovered times are not a
substitute for re-verifying on this tree.
