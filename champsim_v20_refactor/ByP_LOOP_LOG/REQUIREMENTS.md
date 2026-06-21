# LOOP REQUIREMENTS — ByP Formula Search

## HARD RULES (re-read every 2 iterations)

1. **NO THRESHOLD TUNING.** Never tune constants like 0.75, 0.97, 1.0×, etc. as the primary change. Formula SHAPE must change each iteration.
2. **Max 3 iterations of the same formula family.** If 3 variants of φ-gated didn't beat winner → abandon that family, try a different signal combo.
3. **Test BOTH traces** for every single model: `256.Pyth` AND `256.GPT`. Never ship a model tested on only one.
4. **Worker reports COMPACT**: last-200-lines extract → `IPC, ByP%, what's suffering (MPKI spike? MSHR full? LLC dead?)`. Never dump 200 lines.
5. **Winner ratchet**: only declare new winner if BOTH traces improve or one improves meaningfully with the other flat (≤0.001 loss).
6. **Log every iteration** in `loop.log` with: model, formula description (one line), Pyth IPC, GPT IPC, verdict.
7. **Goal**: maximize FINAL ROI IPC on both traces. Current winner **429a**: Pyth=0.58559 / GPT=0.56052.
8. **Predict future pure miss / overlap** — that's the paper's key idea. Use derivatives/deltas between short and long window (short = predictor of near future).

## FORMULA FAMILIES TO CYCLE THROUGH

- **F1** κ-ratio (DONE: 429a winner) — `κ_s vs κ_l` [3/3 used, abandon]
- **F2** φ-gated — φ_short vs φ_long as gate on κ signal
- **F3** Overlap-delta — `(1-κ)_short vs (1-κ)_long` (overlap collapsing = future pure miss)
- **F4** APC/MST cross-level — use next level's APC×MST as absorb signal
- **F5** C-AMAT product — φ×APC×MST normalized against self-history (not fixed threshold)
- **F6** Sign-of-derivative — bypass when `d_κ/d_t > 0` AND `d_φ/d_t < 0` (pure miss rising + hits falling)
- **F7** Ratio-of-ratios — `(κ_s/κ_l) > (φ_s/φ_l)` — pure scale-free comparison
- **F8** Per-level asymmetric — different signal per level (L1=κ, L2=overlap, LLC=uncond)

## WORKFLOW PER ITERATION

1. Pick next formula family (NOT same as last 2).
2. Write 3 files: `NNNa-ByPw_<shape>.l1_bypass/.l2_bypass/.llc_bypass`.
3. Dispatch worker: run Pyth + GPT foreground, report compact only.
4. Append to `loop.log`: model, shape description, IPCs, verdict.
5. Every 2 iterations: `cat REQUIREMENTS.md` to re-anchor.
6. Update winner pointer if ratchet condition met.

## SCOREBOARD (latest at bottom)

- 409a baseline: P=0.56722 / G=0.52272
- 423a (F1 + uncond LLC): P=0.58038 / G=0.55439
- 425a (F1 all-κ + uncond LLC): P=0.58246 / G=0.55625
- 429a **WINNER** (F1: L1 0.75κ, L2 κ, LLC uncond): P=0.58559 / G=0.56052
- 430a (F1 L1 0.5): P=0.57746 / G=0.56122 — split loss
- 431a (F1 L1 0.7): P=0.58266 / G=0.56127 — split loss
- 432a (F1 L2 0.97): PENDING

## NEXT CANDIDATES

- **433a** — F6 Sign-of-derivative: L1 bypass if `κ_s>κ_l && φ_s<φ_l`; L2 bypass if `(1-κ)_s < (1-κ)_l`; LLC uncond
- **434a** — F7 Ratio-of-ratios: bypass if `(κ_s·φ_l) > (κ_l·φ_s)` (cross-product, scale-free)
- **435a** — F4 APC×MST cross: use next-level absorb capacity as gate
