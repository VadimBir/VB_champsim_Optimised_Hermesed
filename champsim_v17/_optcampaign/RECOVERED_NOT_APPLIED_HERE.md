# RECOVERED — KEPT IN REMOTE, NOT YET APPLIED HERE

**What this file is.** A reconstruction of every optimization / refactor from the **lost
remote ChampSim optimization campaign** (`/home/cc/champsim_VB`, server lost 2026-06-10) that
was **accepted-and-kept there** (PASS / held-winner / merged into v19 or queued for v20) **but
is NOT present in the current Windows reconstruction tree** (`champsim_v17` fork +
`champsim_v18_TEST_OPT`). Section B lists what the remote session itself reversed/rejected (do
NOT port). Section D flags uncertainties. The former **Section C (opts already applied in v18)**
has been removed — this file now lists only what is **NOT applied here**.

**Source corpus actually read (in full):**
- `0610_2238_Untitled-13_73e72b5f.txt` — the OPT_REPORT master (§1–§10 + Wave-3 rows + agent table). **Primary ground truth.**
- `CHAMPSIM_VERSION_HISTORY.md` — reconstructed lineage (Era 0 → v20 in-flight).
- `RECOVERY_CONTEXT.md` — state-at-disconnect snapshot, rejected/excluded list, v20 plan.
- `0610_2236_Untitled-12_73e72b60.txt` — REFACTOR_CACHE_SPLIT_PLAN.md (+ Addendum, + Addendum 2).
- `0610_2236_Untitled-11_73e72b61.txt` — 1047-line live session scrollback (v19 creation, Wave-3 launch, profile).
- `0608_1850_Untitled-15_73e72b5d.txt` — 06-08 v15 acceleration study (data-packings).
- `0609_0607_Untitled-16_73e72b5c.txt` — hoist/const-param checklist.
- `0610_2230_Untitled-10_73e72b62.txt` — paper-writing note (NO optimization content).
- `0610_0642_Untitled-20_73e72b43.txt` — test-command + equivalency note (NO new dispositions).

**Method.** Cross-referenced the OPT_REPORT disposition tables (§1, §9, §10, Wave-3 rows) and
the VERSION_HISTORY scoreboard against the BASELINE set already present in this tree (the opts
already applied in v18 — since removed from this file), then took the complement. Every accepted/reversed claim below is cited to source file + quoted
phrase.

**LIMITATION (load-bearing).** This corpus is **description-only** — it contains OPT_REPORT
prose, branch tables, and design blueprints, **NOT source diffs**. The exact `.bk`/`_BK`
backups, WORKLOG.md files, and the per-branch trees (e.g. `diff -ru v18_DRAM v18_DRAM_P24`)
lived only on the lost remote disk (RECOVERY_CONTEXT §"Key remote artifacts"). So each item
below gives **intent, file/site, and reported benefit — not the verbatim hunk.** Porting
requires re-deriving the diff from the blueprint and re-gating bit-exact.

---

## SECTION A — KEPT IN REMOTE, NOT APPLIED HERE (the core answer)

> Gate invariant for every remote "PASS": bit-exact `FINAL ROI CORE AVG IPC: ;0.51803;` (16c) /
> `0.55145` (1c) + 16× `execution_checksum 140039767200773` + LPM ROI diff 0 vs
> `champsim_v17_DRAM_P2/P2_stage2_sim.log` (CHAMPSIM_VERSION_HISTORY.md L7–L10).

### A1 — P8: LPM_Tracker hot-field colocation reorder
- **What it did:** reordered `LPM_Tracker` struct fields so hot fields colocate; `sizeof` shrinks and cache-line footprint drops.
- **File/site:** `inc/lpm_tracker.h`.
- **Benefit (quoted):** OPT_REPORT §1 `P8 | PASS | LPM_Tracker hot-field colocation reorder (sizeof 704→688, 9→6 lines), lpm_tracker.h | 320.968s`. VERSION_HISTORY restates `704→688 B, 9→6 cache lines`.
- **Remote disposition:** **ACCEPTED & MERGED into v18.** STACK stage 5 `+P8 | PASS | 300.253s` (§3); VERSION_HISTORY v18 content = "11 opts: P1, P2, P6, P7+P7ext+P7c, **P8**, P9D, P10, P11, P14, P15" (L79). It rode into v19.
- **Why absent here:** the applied set (the 14 v18 opts + fork pre-existing) does NOT list P8. v18 here was rebuilt from the fork + the 14 named opts; P8 is not among them.
- **Current state (verified this session):** P8's only *recoverable* substance — removing the 2 dead `*_byp_llc_pureMissCy` fields — is **ALREADY in v18** (`DEAD-2026-05-25`). Measured `sizeof(LPM_Tracker)=704`, which does **not** map to the 688 B blueprint (this config carries long/short buckets). So **nothing faithful is left to apply** beyond the un-spec'd field reorder.
- **Risk/effort to port:** **LOW but largely moot.** Pure field reorder in one header; blueprint gives target size (688 B) not field order. No bit-exact change remains to port beyond what's already done.

### A2 — P9D: cache.h `auto& re` reference hoist
- **What it did:** hoists a reference (`auto& re`) in `cache.h` to eliminate repeated lookups (the one accepted item "D" of an analyzer list; the other items were honest-skipped).
- **File/site:** `inc/cache.h` (`auto& re` ref hoist).
- **Benefit (quoted):** OPT_REPORT §1 `P9 | PASS | cache.h \`auto& re\` ref hoist (item D; 6 analyzer items honest-skipped) | 326.853s`. STACK stage 4 `+P9D | PASS | 308.598s`.
- **Remote disposition:** **ACCEPTED & MERGED into v18** ("P9D" in the 11-opt v18 list, VERSION_HISTORY L79), carried into v19.
- **Why absent here:** not in the BASELINE 14-opt set nor listed as pre-existing in the fork.
- **Risk/effort to port:** **LOW.** Single ref hoist in `cache.h`. Blueprint names the site ("`auto& re`") but not the surrounding lines; re-locate by name and gate.

### A3 — P11: O3_CPU hoists / prefetch (exec_mem, sched_mem, update_rob)
- **What it did:** O3 back-end hoists + prefetch in `exec_mem`, `sched_mem`, `update_rob` (combination branch built on P1X = P1+P10).
- **File/site:** `src/ooo_cpu.cc` (exec_mem / sched_mem / update_rob paths). Cross-ref hoist checklist `0609_0607_Untitled-16` (execute_load lqe hoist, add_load_queue rob hoist, etc.).
- **Benefit (quoted):** OPT_REPORT §2 `P11 | PASS | P1X + O3 hoists/prefetch (exec_mem, sched_mem, update_rob) | 298.235s`. STACK stage 7 `+P11 (O3 hoists) | PASS | 295.149s`.
- **Remote disposition:** **ACCEPTED & MERGED into v18** ("P11" in the 11-opt list, VERSION_HISTORY L79), carried into v19.
- **Why absent here:** not in BASELINE 14-opt set.
- **Risk/effort to port:** **MED.** Touches multiple O3 hot functions in `ooo_cpu.cc`. Blueprint names the functions and the checklist names the specific hoists (lqe/rob refs, const params) but **gives no diff**. Higher merge-conflict surface against other `ooo_cpu.cc` opts (P15/P21/P24 region). Re-derive carefully, gate bit-exact after each hoist.

### A4 — v19 cache.cc / cache.h SPLIT refactor (REFACTOR_CACHE_SPLIT_PLAN + Addendum 1 & 2)
- **What it did:** **DESIGN-ONLY blueprint** to split `src/cache.cc` + `inc/cache.h` into per-queue TUs:
  `cache_rq.cc` / `cache_wq.cc` / `cache_pq.cc` / `cache_fill.cc` + `cache_helper.cc` (getters/print/stat bloat) + `inc/cache_macro.h` (~74 macros consolidated). Plus: `dram_helper.cc` for DRAM controller bloat; `ooo_cpu.cc` → `ooo_cpu_fetch.cc/_decode.cc/_schedule.cc/_execute.cc` (core keeps `handle_branch/add_rob/update_rob/retire`). Includes a `ByP → Fwd` internal-identifier rename map (23 renames, 12 KEEP-AS-IS, 8 CLI-DO-NOT-RENAME categories) and `add_rq` hard-split (>200 lines).
- **File/site:** `src/cache.cc`, `inc/cache.h`, `src/dram_controller.cc`, `src/ooo_cpu.cc` (per the move tables in `0610_2236_Untitled-12`).
- **Benefit (quoted):** maintainability/codegen-neutral, NOT a speed win. Gate goal is **no regression**: T3 `ACCEPT iff: T_new <= T_base * 1.015 (1.5% noise)`; "zero codegen change goal" (Addendum 1 §3). Relies on `-flto=thin` (Makefile:233,249) to preserve inlining across the split.
- **Remote disposition:** **DESIGN-ONLY, NEVER EXECUTED.** Plan header: `Status: DESIGN ONLY — zero source edits in this pass. Executor follows mechanically AFTER v20 merge of sibling branches P-BLOOM and P-FASTSET`. VERSION_HISTORY v20 section: "Cache split refactor ... DESIGN ONLY until v20 lands". **Not accepted-and-kept code** — it is a kept *plan*.
- **Why absent here:** no split was ever applied anywhere; gated on v20 which died with the server.
- **Risk/effort to port:** **HIGH effort, but it is a refactor not an opt.** This is the one Section-A item that is a **blueprint, not a landed change** — included because the prompt explicitly asks to investigate it and because the plan itself was *kept* on the remote. Porting = a multi-hour mechanical split + the two-tier gate (T1 1c IPC 0.55145, T2 16c IPC 0.51803, T3 ≤1.5% regression). **Recommend deferring** until/unless the maintainability is wanted; it yields no speedup.

### A5 — Held winner P19B: TRUE_SANITY_CHECK guards around dead rob_maps debug blocks
- **What it did:** wraps 3 debug-mode `rob_maps` maintenance blocks in `#ifdef TRUE_SANITY_CHECK` so they become dead/eliminated at `NDEBUG`.
- **File/site:** `src/ooo_cpu.cc` (~L357/383/391, 3 isolated guards — OPT_REPORT §9 conflict map).
- **Benefit (quoted):** OPT_REPORT §9 `P19B | PASS | ... eliminate dead branches at NDEBUG | 13.433s | 284.173s | -2.043s (-0.71%)`. Ranked #3 of Wave-2.
- **Remote disposition:** **PASS-faster, HELD BACK from v19 by master directive** — VERSION_HISTORY L116/L180: "P19B/P25/P28 deliberately held back ... Held-back proven winners still on the table for v20+: **P19B (−0.71%)**, P25, P28." A kept winner, explicitly on the v20 shelf.
- **Why absent here:** the prompt's BASELINE lists **P19B as a NO-OP here** ("queued in reconstruction but found nothing to implement here") — i.e. the target dead `rob_maps` debug blocks **do not exist in this tree**. So although it was a held winner remotely, there is **nothing to guard here**.
- **Risk/effort to port:** **N/A in this tree** unless the `rob_maps` debug maintenance blocks are reintroduced. If they exist, effort is LOW (3 ifdef guards). **Flagged as a contradiction in Section D** (held-winner-remote vs no-op-here).

### A6 — 06-08 "VERIFIED-safe" data-packings (champsim_v15 study)
- **What they did / file / per-item benefit** (all from `0608_1850_Untitled-15`, "DATA PACKING — PROVEN SAFE CHANGES" / "PROVEN SAFE DATA PACKING" tables):
  - **BLOCK 48B→32B** bundle (`block.h`, "saves 16 MB at 16-core"):
    - `Remove tag (always == address)` — `8B/block = 8.6MB`, `VERIFIED — 5 callsites, all substitutable` (block.h:41; cache.cc:998/1043/1069/1077/1093).
    - `lru uint32→uint8` — `3B = 3.2MB`, `VERIFIED — max=19` (block.h:48).
    - `pmc uint32→uint16` — `2B = 2.1MB`, `VERIFIED — max=350` (block.h:49).
    - `Drop l1/l2/llc_bypassed on BLOCK` — `3B = 2.1MB`, `VERIFIED — zero reads from BLOCK` (block.h:30-32).
  - **PACKET** `Union data/instruction_pa/data_pa` — `16B/pkt`, `VERIFIED mutually exclusive by packet type` (block.h:125-127).
  - **ROB** `producer_id removal` — `8B/entry`, `VERIFIED dead — zero reads from ROB` (instruction.h:101).
  - **ROB** `3×int → int8+int8+int16` — `8B/entry`, `VERIFIED — ranges fit, signed preserved` (instruction.h:127).
  - **O3_CPU** `RTE/RTL/RTS uint32→uint16` — `44KB@16c`, `sentinel=ROB_SIZE fits` (ooo_cpu.h:496-497).
  - **BANK_REQUEST pack** — `8B/entry`: `open_row uint32→uint16 (max=65535), request_index int→int8 (max=63), 4 flags→1 bitfield` (memory_class.h:57-93).
- **Remote disposition:** **MARKED "PROVEN SAFE" / "VERIFIED" by the 06-08 24-agent study, but NO PASS-gate evidence of being merged.** This study is a *pre-v17* design snapshot (study targets `champsim_v15/`). **No OPT_REPORT branch (P1–P28, Wave-3) corresponds to applying these packings**, and the v18/v19 opt lists do not include any of them. **See Section D — their kept/applied status is UNRESOLVED.**
- **Why absent here:** not in the BASELINE set; and there is no evidence they were ever turned into a gated branch on v17+.
- **Risk/effort to port:** **LOW-MED per item** (narrowings + a union + dead-field removals), **but the "VERIFIED" claims are from a v15-era static audit, not a v17 bit-exact gate.** Each must be re-audited against the current tree (max-value ranges, callsite substitutability) and gated bit-exact before trust. **Do NOT treat "VERIFIED" as gate-passed.**

### A7 — Wave-3 slice opts on v19: the NOT-applied remainder
> The applied Wave-3 opts — B1/B3/B8, C3A/C5, D2/D5, P28, A8 — are already in v18 and have
> been removed from this file. What remains are the Wave-3 items NOT in this tree.

- **B2** — PARKED remotely (largest refactor; "spec misses ROB-slot reuse invalidation"); never applied anywhere. `src/ooo_cpu.cc` + `inc/ooo_cpu.h`.
- **C7** — skipped "per conservative bit-exact obligation" (not applied remotely). `inc/instruction.h` + `inc/ooo_cpu.h`.
- **D1** — `run_clsbyp` u16 shadow fusing the 5-way cmp chain. **NO-OP here** ("found nothing to implement"); the `lpm_operate` continuation site differs in this tree. **Flagged Section D.**
- **A6 / A3 / A1** — PARKED remotely (A6 needs `CACHE::operate` signature refactor; A1 == P3-resembling SoA tag shadow, multi-hour, never settled). `src/ooo_cpu.cc`.
- **Net:** the parked items (B2, C7, A1/A3/A6) were never accepted-and-kept remotely either, and D1 is a no-op here — none belong cleanly to A or B (see Section D).

### A8 — Sibling branch P-FASTSET (pending at server loss; P-BLOOM removed — already applied)
- **What it did:** edited `src/cache.cc` / `inc/cache.h`; **contents not in recovered material.** Name implies the fastset (LQ/SQ small-set) path. (Its sibling **P-BLOOM is already in v18** and has been removed from this file.)
- **Benefit:** **not specified in recovered material** (VERSION_HISTORY L138–140: contents not recovered; results were pending the retime).
- **Remote disposition:** **IN FLIGHT / PENDING at server loss** — queued for the v20 retime; never confirmed accepted.
- **Why absent here:** never landed; **P-FASTSET is genuinely absent and its content is unrecoverable.**
- **Risk/effort to port:** **UNKNOWN — content unrecoverable.** Cannot port from this corpus. **Flagged Section D.**

---

## SECTION B — REVERSED / REJECTED IN REMOTE (excluded by the remote session itself — do NOT port)

> Only items with explicit source evidence of failure/rejection are listed; each is quoted.

| Item | What | Failure evidence (quoted) | Source |
|---|---|---|---|
| **P20** | persistent sched dirty flags (Idea A) | `FAIL ... deadlock — assertion \`0\` in print_deadlock at 2.206s`; `src/main.cc:908 Assertion '0' failed; Aborted (core dumped)`; "restored from `src/ooo_cpu.cc.P20_BK`; not eligible for merge" | OPT_REPORT §9, §9-footer |
| **P22** | CACHE::operate full idle-skip / dirty-flag in fetch | `FAIL on proof | dirty-flag in fetch path violates has_work⇏miss_active=0 invariant (rejected pre-Wave2)` | OPT_REPORT §9 |
| **P27** | ctz-iterate fc&rmask bits / sparse PCYCLE_LE | `PASS (slower) | ... | +67.926s (+23.73%) ... REGRESSION — keep IPC-exact, but DO NOT MERGE` | OPT_REPORT §9 + ranking |
| **P17** | EMH_INT_HASH=2 Murmur3 + map reserve | `PASS | ... | +6.897s (+2.41%)` loser | OPT_REPORT §9 |
| **P26** | lazy DTLB PACKET template copy | `PASS | ... | +2.780s (+0.97%)` loser | OPT_REPORT §9 |
| **P23** | DRAM ctrl next-event gate | `PASS | ... | +1.688s (+0.59%)` loser (note: pre-Wave2 contended) | OPT_REPORT §9 |
| **P16** | inline SmallLqSet / small-set | `PASS | ... | +0.873s (+0.30%)` within-noise loser | OPT_REPORT §9 |
| **P3** | get_way tagv shadow `((tag<<1)\|valid)` | "gated PASS but slower; excluded" | OPT_REPORT §1 (326.113s) + VERSION_HISTORY L55 |
| **P4** | LPM_Tracker transpose `cy_t[class][bucket]` | "gated PASS but slower; excluded" | VERSION_HISTORY L55 |
| **P5** | schedule lazy fused_scan (stop at first stall) | `PASS* ... 393.276s*` — "heavily contended + suspect slower than base; excluded from stack" | OPT_REPORT §1 + footnote |
| **P13** | RT-queue event_cycle mirror arrays | "gated PASS but slower; excluded"; §8: "P3/P4/P5/P13 intentionally excluded as slower / suspect" | OPT_REPORT §8 + VERSION_HISTORY L55 |
| **P12** | dep-array colocation | `SKIP — premise false (one array, not two); no edits` (honest skip) | OPT_REPORT §1 |
| **P18** | address_to_entries slot shrink (88B floor) | `SKIP — slot 88B floor (rejected pre-Wave2)` | OPT_REPORT §9 |
| **P19** | rob_maps slot value packing | `SKIP — rob_maps reader dead (rejected pre-Wave2)` | OPT_REPORT §9 |
| **event_cycle u64→u32** | narrow event_cycle | `VERDICT: UNSAFE — raw unbounded cycles stored; u32 wrap re-opens the documented PCYCLE wrap-deadlock bug class ... NOT pursued.` | OPT_REPORT §5 |
| **schedule-merge / skip-schedule-on-no-new-ROB** | skip schedule when no new ROB entries | `UNSAFE: Skip schedule when no new ROB entries — reg_RAW_release makes existing entries ready` | `0608_1850` equivalency constraints |
| **OpenMP parallel cores** | parallelize the per-core loop | `FAILED EMPIRICALLY. User tried it — results are NON-EQUIVALENT due to L2→LLC request ordering` | `0608_1850` |
| **DRAM min-heap** | min-heap for next-event | `FRAGILE: DRAM min-heap — row-buffer state dynamic, FCFS needs seq numbers, worst=O(N)` | `0608_1850` |

**Notes / scope-clarifications on Section B:**
- **P-FASTSET** is NOT in Section B — there is no evidence it was rejected; it was *pending* (Section A8 / Section D).
- **P2 / P6 / P7 / P7c / P14 / P10 / P1** appear in §1 as PASS but are **already present in this tree** (the applied set, removed from this file) — not listed as reversed.
- The 06-08 "CANNOT PACK (adversary-proven)" list belongs here as **rejected packings**: `event_cycle` (PCYCLE wrap), `address/full_addr` (rotr64 masking), PACKET layout before `rob_index_depend_on_me` (memcpy+offsetof frozen), `lq/sq_index` (UINT16_MAX sentinel), LPM counters (uint64 no-halve), Fastset (already minimal) — `0608_1850` "THINGS THAT CANNOT BE PACKED".

---

## SECTION D — CONTRADICTIONS / UNCERTAINTIES (do not guess; flagged)

1. **Were the 06-08 data-packings KEPT or REJECTED? — UNRESOLVED (leaning: never gate-applied).**
   The 06-08 study (`0608_1850`) labels BLOCK 48→32B, lru/pmc narrowings, BANK_REQUEST,
   RTE/RTL/RTS u32→u16, producer_id removal, and the PACKET union as **"PROVEN SAFE" / "VERIFIED"**.
   But: (a) the study targets `champsim_v15/`, *before* the v17 optimization campaign; (b) **no
   OPT_REPORT branch (P1–P28, Wave-3) corresponds to applying any of them**; (c) they appear in
   **none** of the v18/v19 opt lists. There is **no bit-exact gate evidence** that they were ever
   merged. **Resolution: "VERIFIED" here means a static-audit safe-claim, NOT a gate-passed kept
   change.** Treat as *candidate* packings requiring fresh per-item audit + gate, **not** as
   confirmed remote-accepted code. *I cannot confirm from this corpus that any were applied;
   stating they were "kept" would be a guess.*

2. **P19B: held-winner-remote vs no-op-here.** OPT_REPORT/VERSION_HISTORY explicitly call P19B a
   **held-back proven winner (−0.71%)** for v20. The prompt's BASELINE says P19B is a **NO-OP in
   this tree** (target dead `rob_maps` debug blocks absent). Both are consistent only if this
   tree never had those debug blocks. **Flagged:** P19B is "missing" in the sense of being a kept
   remote winner, but **un-portable here** unless the dead `rob_maps` maintenance blocks exist.
   Verify by grepping this tree for `TRUE_SANITY_CHECK` rob_maps blocks before concluding.

3. **D1 (run_clsbyp u16 shadow): kept-in-W3-D-remote vs no-op-here.** W3-D applied D1 on v19
   remotely (OPT_REPORT Wave-3 D row). BASELINE marks D1 a **NO-OP here**. Same shape as P19B:
   the LPM continuation 5-way cmp chain it fuses may differ in this tree. **Flagged — verify the
   `lpm_operate` continuation site exists before attempting.**

4. **P-FASTSET: content unrecoverable.** A genuine remote branch editing `cache.cc`/`cache.h`,
   pending at server loss, **not** in the BASELINE set and **not** described anywhere in the
   recovered corpus. **Cannot be reconstructed from this material** — its intent (fastset path)
   is inferable from the name only. Marked **UNVERIFIED / unrecoverable.**

5. **v20 itself never existed as code.** VERSION_HISTORY: "v20 — IN FLIGHT at server loss";
   "v20 | unknown — agent died with the server". So **no v20 tree, no v20 winner-merge, no
   GAP/DRAM slice (plan A–E) implementation** exists to port — only the *plans*
   (REFACTOR_CACHE_SPLIT_PLAN; GAP bank-bucket A→E ranked queue). These are **design intent, not
   accepted code.**

6. **Blueprint-not-diff (global).** For every Section A item, the recovered text gives the
   site + intent + reported time, **never the exact source hunk** (the `.bk`/WORKLOG/tree diffs
   were remote-only). Any port is a *re-implementation from spec* that MUST be re-gated bit-exact
   (`;0.51803;` 16c + 16× checksum `140039767200773` + LPM ROI diff 0) — the recovered numbers
   are **not** a substitute for re-verification on this tree.
