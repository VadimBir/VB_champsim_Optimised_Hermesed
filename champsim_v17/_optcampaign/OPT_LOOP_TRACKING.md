# OPT LOOP TRACKING (append-only)

Frozen base = `champsim_v18_TEST_OPT` @ git `0295747` (NEVER edit — only `cp`-duplicate).
**GATE:** execution_checksum MUST == `140038115682937`; wall ≤ `216.7s` (×5 median only if a run looks slower).
**Workload:** `no spp no` + `--hermes ttp` + `--bypass 4000fix-KappaPhiL1L2`, perceptron/lru/glc, 8 cores, 2M warmup / 14M sim, trace `LLM256.Pythia-70M_21M`. Runner = `quickSim/run_one.ps1`.
All agents = Opus. Failed opts NEVER enter OPT_CATALOG.md. Next merged bump = v19.

Format — PASS: `vXX_YYYY | <≤16w opt> | checksum=… | IPC=… | wall=…s | PASS`
Format — FAIL: `vXX_YYYY FAIL AVG5 WORSE BY ZZ% — CONTINUE FROM <checkpoint> DIR TO DUP — cause:<why> — idea:<opt>`

**⚠ TIMING METHODOLOGY (revised 2026-06-20):** wall noise is ~4% (200.8–208.9s observed) and the
host DRIFTS — LARGER than per-opt effects (1–5s). Single-run gating vs a stale reference is invalid.
ALL wall gating now uses **paired interleaved** runs (candidate vs current checkpoint, alternating
×4–5, drop the first/warmup run) so drift cancels in the delta. checksum gate (architectural) is
unaffected and remains the hard correctness gate. Early single-run walls (v18_0000..0003) are
drift-confounded — being re-validated paired.

---

v18_0000 | BASELINE fork+14 (frozen ref) | checksum=140038115682937 | IPC=0.62178 | wall=216.7s | PASS (reference)
**VALIDATED CUMULATIVE (paired interleaved ×5, drop-warmup, drift-controlled, 2026-06-20):** frozen-v18 median **215.5s** vs 3-opt checkpoint (Opt-A+P11+Opt-B) median **209.0s** = **−6.5s / −3.0% REAL** (tight non-overlapping clusters; checksum 140038115682937 both). Opt-A+P11 carry it; Opt-B flat. (Same CKPT binary read 202.5s earlier vs 209.0s now → host drift ~6s → paired gating mandatory.)
v18_0001 | Opt-A working_bank_count (DRAM per-cycle bank scan -> O(1) counter, stats-only) | checksum=140038115682937 | IPC=0.62178 | wall=214.1s | PASS  [checkpoint dir: champsim_v18_optA]
v18_0002 | P11 O3 const-ref hoists (~10 hot fns in ooo_cpu.cc, reload elim) | checksum=140038115682937 | IPC=0.62178 | wall=209.4s | PASS  [checkpoint dir: champsim_v18_optA_P11]
v18_0003 | Opt-B DRAM idle-batch (accumulate idle cycles -> advance_idle(N), stats-only; advance_idle==N*tick proven exhaustively) | checksum=140038115682937 | IPC=0.62178 | wall=209.1s | PASS (flat — DRAM busy on this workload)  [checkpoint dir: champsim_v18_optA_P11_B]
v18_0004 FAIL AVG5 211.6s (median; 5 runs 211.0-212.0, checksum exact all 5) vs 209.1s — WORSE BY 1.2% — CONTINUE FROM champsim_v18_optA_P11_B DIR TO DUP — cause: V2 [[gnu::always_inline]] on per-cycle do_scheduling = i-cache bloat (R8 cc_now hoist neutral, R3 skipped) — idea: W3-E core micro-opts  [NOTE: not drift-controlled; but ×5 tight 211-212 + plausible cause → discard stands]
v18_0005 FAIL PAIRED +2.3s (interleaved ×4: median_M 204.8 vs median_B 202.5, drift-controlled, checksum exact all 8) — WORSE BY ~1.1% — CONTINUE FROM champsim_v18_optA_P11_B DIR TO DUP — cause: M1 lru u32->u8 = int-promotion overhead in hot LRU-update loop with NO real BLOCK shrink (alignment padding); M2 pmc skipped (unbounded accumulator); M3 drop-bypassed bundled — idea: M1+M3 footprint (defer: may still help 16c under a footprint-specific gate)
v18_0006 FAIL PAIRED +0.36% (median_C 202.4 vs median_K 201.7, interleaved ×4, overlapping clusters = within noise, NO benefit; checksum exact all 8) — CONTINUE FROM champsim_v18_optA_P11_B DIR TO DUP — cause: cache.cc const-ref hoists REDUNDANT with -O3 (compiler already eliminates these reloads; P11 only won because ooo_cpu's LQ.entry[idx] sat behind an aliasing barrier -O3 couldn't cross) — idea: cache.cc reload-elim. LEARNING: micro-opt vein exhausted; pivot to algorithmic (data-structure) opts -O3 can't do.
v18_0007 SKIPPED (feasibility) — B2 single-slot reg->producer flat array: reg-producer is actually a reg->ordered-list-of-all-in-flight-ROB-producers (ProducerList, keyed by uint8 0-255); get_latest_producer = windowed reverse-scan w/ fallback, remove_producer = tombstone-by-rob_index. Single slot can't reproduce -> would diverge checksum. REDIRECT -> B2' (replace the hash-MAP container with flat ProducerList[256] direct-index, ProducerList semantics unchanged).
v18_0008 | B2' reg_producers ankerl-map -> flat ProducerList[256] direct-index (ooo_cpu.h only; ProducerList window/tombstone semantics byte-unchanged) | checksum=140038115682937 | IPC=0.62178 | wall=PAIRED median 199.55s vs ckpt 201.87 = -2.32s/-1.15% (interleaved ×4, non-overlapping, checksum exact all 8) | PASS  [checkpoint dir: champsim_v18_optA_P11_B_B2]  (= 4th kept opt)
v18_0009 SKIPPED — no flattenable dense-key hot map left (census: mem_producers/address_to_lq/rob_maps/address_to_entries/mshr_map all SPARSE 64-bit keys; reg_producers already done by B2'). Map-flatten vein = one-shot, exhausted.
=== **v19 BANKED (committed + frozen)** === dir `champsim_v18_optA_P11_B_B2` = Opt-A+P11+Opt-B+B2'. VALIDATED PAIRED ×5 (drop-warmup): frozen-v18 median 207.9s vs stack median 198.9s = **−9.0s / −4.33%**, checksum 140038115682937 exact all 10 runs. FROZEN — never edit, only dup. Loop continues dup'ing this as v19_XXXX.

**VEIN STATUS (after 8 iters): SAFE veins mined out** — micro-opts captured by -O3 (W3-E/cache-hoists failed), map-flatten done (B2'), cache/DRAM flat (not the bottleneck; core=ooo_cpu is). 4 kept opts. Remaining = low-value-here (cache/DRAM) or RISKY (A1 SoA tag mirror — cache not hot; DRAM-GAP selection — bit-exact risk; __restrict — UB). → banking v19 at 4; re-validating cumulative.
