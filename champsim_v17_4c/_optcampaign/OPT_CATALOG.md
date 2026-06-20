# OPT_CATALOG — AFK autonomous opt-loop candidate registry

Base (frozen) = `champsim_v18_TEST_OPT` (fork+14 opts + rw_occ/bank_hot infra). Engine = single
resumable Opus agent. **All spawned agents = Opus.** Next merged checkpoint bump = **v19**.
Gate config = **model ON + PF ON + OCP=TTP (Hermes)** — exact run flags/#defines PENDING user spec
(do NOT gate-run until provided).

**Confirmed in v18 tree (2026-06-20):** emhash7 IN USE (`ooo_cpu.cc:7-8`, P7/P7c) · ankerl
`unordered_dense` in 5 core headers · Hermes TTP compiled (`src/hermes/offchip_pred_ttp.{cc,h}`,
behind `USE_HERMES`).

---

## GATE (every attempt) — use the quickSim harness (do NOT hand-roll PowerShell)
Equivalence = `execution_checksum` MUST match baseline for any IPC-neutral opt (host-speed +
memory/footprint both architectural-neutral). Also record `FINAL ROI CORE AVG IPC` + median wall.

**BUILD** (`quickSim/win_qbuild.sh`): does CONFIG (copies defs/bpred/pref/repl, `sed` NUM_CPUS),
EXTRA_DEFS, CLEAN rebuild (`rm -rf obj_win bin`), descriptive-renames exe.
```
sh quickSim/win_qbuild.sh --dir <DIR> <L1pf> <L2pf> <L3pf> \
   --cores 8 --hermes ttp --repl lru --bp perceptron --arch glc \
   [--l1byp <M> --l2byp <M> --l3byp <M>]
```
OCP knob: `--hermes off|on|ttp|hmp` → `ttp` = `-DUSE_HERMES -DUSE_HERMES_TTP`.

**RUN** (`quickSim/win_qrun.sh`): spawns `--cores` trace copies, `Start-Process`, `ProcessorAffinity=0xF0`
(cores 4-7), `PriorityClass=RealTime`, redirect via `cygpath`; prints `checksum=` (Core_0_execution_checksum)
+ `FINAL ROI CORE AVG IPC` + median wall.
```
sh quickSim/win_qrun.sh --dir <DIR> --cores 8 --hermes ttp --bypass <MODEL|none> \
   --warmup 2000000 --sim 10000000 --affinity F0 --rt on --runs 1 <TRACE_KEY> <L1pf> <L2pf> <L3pf>
```
(`win_qrun` auto-computes warmup/sim for LLM traces = 20%; pass `--warmup 2000000 --sim 10000000`
explicitly for the campaign's 2M/10M.) ×5 = `--runs 5` (median) ONLY when a run looks worse.
NEVER 2 builds/runs in parallel (cc1plus OOM + corrupts timing). BAN `2>/dev/null` `2>&1` `||true`
`||echo 0`. Scratch → `$TMPDIR/champsim_scratch`.

⚠ PENDING for baseline run: the `<L1pf> <L2pf> <L3pf>` module names (PF on) + bypass `<MODEL>` name.

## LOOP
freeze (commit base, never edit) → `cp` dir → impl ONE opt (≤16-word note) → clean build → run 8c →
record checksum/IPC/wall → (checksum-identical AND not-slower → KEEP → dup → next) | (slower → ×5
avg → still worse → DISCARD, re-dup prior checkpoint, log FAIL) → every 5 KEPT → merge → build+gate
→ PASS → bump vXX+1. Failed opts NEVER enter this catalog.

---

## A. HOST-SPEED (IPC-neutral; gate = checksum-EXACT, IPC unchanged)

| id | opt | file(s) | est | risk | notes |
|---|---|---|---|---|---|
| **Opt-A** | `working_bank_count` counter replaces per-bank scan for idle signal | `dram_controller.{h,cc}` | ~2.4%@8c | LOW | feeds LPM stats only, never scheduling/timing → checksum-neutral. Sites: clear 122/592/718 (`--`), set 477 (`++`), miss_active at operate() top. v18 LACKS it. |
| **Opt-B** | idle-batch fast-path (`dram_idle_accumulator`/`has_work`) | `dram_controller.{h,cc}` | ~0.6% | LOW | DEPENDS on Opt-A. `has_work` clears in add_rq/add_wq/process(); needs `lpm_tracker.h:301 advance_idle(N)` (present v18). |
| **P11** | O3 hoists/prefetch in exec_mem/sched_mem/update_rob | `ooo_cpu.cc` | ~1% | MED | `execute_load const auto& lqe` (11 reloads), `add_load_queue const auto& rob` (8), reg_dependency src_reg, add_rq/check_hit/check_mshr const-params, complete_execution `const bool has_raw`. Conflicts P15/P21/P24 region. |
| **W3-E/R3** | num_mem_ops recount → 8-bit mask + ctz | `ooo_cpu.cc` | ~0.3% | LOW | popcount/ctz on a packed ready-mask vs recount loop. |
| ~~W3-E/V2~~ | ~~`always_inline` on do_scheduling~~ | `ooo_cpu.cc` | — | — | 🚫 FAILED (v18_0004): +1.2% (×5 median 211.6 vs 209.1), checksum-exact but i-cache bloat. → DO-NOT. |
| **W3-E/R8** | reload elimination (cache `ready[w]`) | `ooo_cpu.cc` | low | LOW | hoist repeated `ready[w]` loads. |
| **DRAM-GAP-D** | check_dram_queue lookup → `emhash7::HashMap` | `dram_controller.{h,cc}` | 1.5–2% | LOW | emhash7 already in tree. NOT std::unordered_map. Pure host-side lookup, no timing change. |
| **DRAM-GAP-E** | free-index bitmap for bank_request slots | `dram_controller.{h,cc}` | part of GAP | LOW | bitmap+ctz vs linear free-slot scan; selection-order PRESERVED. |
| **P25** | Makefile drop `-fno-omit-frame-pointer` | `Makefile.win` | ~-0.29% | LOW | tiny; verify still builds + checksum-exact. |

### A-RISK (host-speed but touch selection order → bit-exact RISK, gate hard)
| id | opt | file | est | notes |
|---|---|---|---|---|
| DRAM-GAP-A | bank-bucketed pending lists | `dram_controller.cc` | 5–10% on GAP | schedule()=15% host on GAP. Changes scan structure → must prove identical pick. |
| DRAM-GAP-B | open-row fast-gate | `dram_controller.cc` | part of GAP | early-accept open-row hit; verify tie-break unchanged. |
| DRAM-GAP-C | (min, 2nd-min) cache in selection scan | `dram_controller.cc` | part of GAP | caches two best candidates; bit-exact only if recompute matches. |
| W3-A1 | SoA tag mirror for `check_hit` | `cache.{cc,h}` | 3–5% | P3 form was SLOWER — re-attempt with mirror, gate; park if regresses. |
| W3-B2 | reg_producers → flat `latest[]` | `ooo_cpu.cc` | ~2% | dependency tracking restructure; checksum-exact required. |
| W3-C7 | min-event-cycle early-out in update_rob | `ooo_cpu.cc` | low | skip scan when min known; verify no missed wakeups. |

## B. MEMORY / FOOTPRINT (architectural-neutral; helps 16c — ×16 of 1-core mem traffic)

| id | opt | field/site | max | sentinel? | risk |
|---|---|---|---|---|---|
| M1 | `BLOCK.lru` u32→u8 | block.h | 19 | none | ✅ clean |
| M2 | `BLOCK.pmc` u32→u16 | block.h | 350 | none | ✅ clean |
| M3 | drop `BLOCK.l1/l2/llc_bypassed` | block.h | — | — | ✅ zero reads (re-grep before cut) |
| M4 | PACKET union `data/instruction_pa/data_pa` | block.h | — | — | ⚠ AUDIT — 3 fields live; prove mutual-exclusion across all join sites before union |
| M5 | `RTE/RTL/RTS` u32→u16 | ooo_cpu.h | ROB_SIZE | =ROB_SIZE | ⚠ confirm ROB_SIZE < 65535 |
| M6 | BANK_REQUEST `request_index` int→int8 | memory_class.h | — | −1 (signed, safe) | ⚠ narrowing-safe sentinel |
| M7 | BANK_REQUEST 4 flags → bitfield | memory_class.h | — | — | ⚠ pack `valid/scheduled/...` to 1 byte |
| M8 ★ | `lq_index`/`sq_index` u16→u8 via sentinel migration | block.h, ooo_cpu.cc | LQ/SQ_SIZE | UINT16_MAX→migrate | ⚠ CONDITIONAL — STEP 1 audit LQ_SIZE/SQ_SIZE (<255?) + grep all `==UINT16_MAX` compare/assign sites; migrate sentinel→0xFF (if headroom) or out-of-band valid-bit; then narrow + gate. User-flagged as a real packing win. |

### PACK_DEFINE scheme (rollout vehicle for B)
In `defs.h`: per packed field add `#define PACK_<f>` (selects narrowed type) + `#define
PACK_<f>_MAX` (static_assert / runtime max-guard). First concrete application = M1/M2. `defs.h` holds
all the sizes so a narrowing can be reverted by one `#define` flip.

## C. NEW CANDIDATES (untested here — parallel-branch fodder, gate like everything else)

| id | opt | file(s) | class | risk | rationale |
|---|---|---|---|---|---|
| N1 | emhash `reserve()`/pre-size the P7 maps to cut rehash churn | `ooo_cpu.cc` | host | LOW | maps already emhash7; pre-sizing is pure host-speed, checksum-neutral. |
| N2 | `[[likely]]`/`__builtin_expect` on hottest operate()/update_rob branches | `ooo_cpu.cc`,`dram_controller.cc` | host | LOW | branch-hint only; zero semantic change. |
| N3 | `__restrict` hot pointer params in cache/ooo inner loops | `cache.cc`,`ooo_cpu.cc` | host | MED | aliasing promise — must be TRUE or UB; gate checksum-exact. |
| N4 | cache decoded DRAM ch/rank/bank/row on PACKET (avoid re-decode) | `dram_controller.cc`,`block.h` | host/mem | MED | trades 16B footprint for fewer decodes; net depends on hit rate. |
| N5 | branchless overflow already in dbus — extend pattern to scheduled_reads/writes counters | `dram_controller.cc` | host | LOW | mirror existing `dbus_cycle_congested_ovf` branchless idiom. |
| N6 ★ | **HASHMAP EXPLORATION (user-blessed)** — survey each hot map site, test which map is fastest THERE | `ooo_cpu.cc`,`cache.h`,`dram_controller.*` | host | LOW–MED | sites: P7 `emhash7::HashMap/HashSet` (ooo_cpu.cc:7-8); ankerl `unordered_dense` in 5 headers (champsim/cache/ooo_cpu/instruction/byplat); DRAM-GAP-D queue lookup. For each, A/B emhash7 vs ankerl vs std vs alt (open-addressing/robin-hood) — keep whichever wins per-site, checksum-exact. |

---

## 🚫 DO-NOT (proven fail / unsafe — NEVER add, NEVER re-test silently)
- `producer_id` **REMOVE** = NO — LSQ_ENTRY.producer_id (block.h:710) LIVE, read 6× (ooo_cpu.cc 1291/1297/1324/1332/1588/2116). (NARROW is CONDITIONAL, NOT DO-NOT — the value is a 34-bit instr_id; only the UINT64_MAX sentinel widens it to 64. See SENTINELS table.)
- `event_cycle` u64→u32 — PCYCLE wrap deadlock (value-range, NOT a sentinel — unfixable by migration).
- (`open_row`→u16 and `lq/sq_index`→u8 are NO LONGER hard-DO-NOT — moved to CONDITIONAL via sentinel-migration; see SENTINELS table + M8.)
- P-FASTSET (FAIL-design), P20 (deadlock), P22, P27 (+23.7%), P17/P26/P23/P16 (slower), P3/P4/P5/P13 (slower), OpenMP cores (non-equiv), DRAM min-heap (fragile), schedule-merge (RAW release).
- `always_inline` on per-cycle `do_scheduling` (W3-E/V2) — FAILED v18_0004, +1.2% i-cache bloat (×5 median 211.6 vs 209.1).

## SENTINELS — the BLOCKER is the in-band sentinel, NOT the field (and sentinels are MIGRATABLE)
**Corrected rule (user, 2026-06-20):** a field can't be narrowed while code uses the *narrowed
type's MAX* as an in-band "invalid" marker — narrowing makes a real value alias the sentinel =
silent corruption. BUT the sentinel is changeable. To pack a sentineled field:
1. **Find real max** of the field (the SIZE constant / observed range).
2. **Confirm headroom**: narrowed type must hold real-max AND still spare ≥1 value for a sentinel.
3. **Migrate the sentinel** at EVERY assign/compare site — either to a reserved value the narrowed
   type can spare (e.g. `0xFF` if real-max < 255), OR **out-of-band** (a separate `valid` bit/flag,
   no spare value needed).
4. **Gate** checksum-exact.

| field | type | blocker kind | packable? |
|---|---|---|---|
| `lq_index`/`sq_index` | u16, sentinel UINT16_MAX | **in-band sentinel** | ✅ CONDITIONAL → u8 IF LQ_SIZE/SQ_SIZE < 255 (migrate sentinel→0xFF or out-of-band). AUDIT sizes + all `==UINT16_MAX` sites. → see M8. |
| `open_row` | u32, sentinel UINT32_MAX | in-band, NO spare (max row 65535 fills u16) | ⚠ HARDER → u16 needs **out-of-band valid-bit** (no spare u16 value). possible but more sites. |
| `producer_id` | u64, sentinel UINT64_MAX | holds a **34-bit instr_id**; the UINT64_MAX sentinel (NOT the data) forces 64-bit | ⚠ CONDITIONAL narrow → `:34` bitfield + **out-of-band `has_producer` bit** (or a reserved 34-bit id). REMOVE = NO (live, read 6×). Risk = M1-style: bitfield mask overhead + may not shrink LSQ_ENTRY (alignment), so 8c-flat/slower → **16c-footprint gate** candidate, not a clear 8c win. |
| `event_cycle`/CYC_PACKED_MAX | u64 | **value-range** (PCYCLE wrap), not a sentinel | 🚫 u32 wraps regardless of sentinel. |
| `STA[]` | u64 | sentinel | audit before any narrow. |
| `asid` | u8 | already minimal | — |
| `TOMBSTONE` | u16 | IS the marker itself | — |
| `request_index` | int, −1 | signed out-of-band already | ✅ narrowing-safe (int8). |

## TRACKING FILE FORMAT (append-only, separate file per campaign)
- PASS: `vXX_YYYY | <≤16w opt> | checksum=… | IPC=… | wall=…s | PASS`
- FAIL: `vXX_YYYY FAIL AVG5 WORSE BY ZZ% — CONTINUE FROM <checkpoint> DIR TO DUP — cause:<why> — idea:<opt>`
