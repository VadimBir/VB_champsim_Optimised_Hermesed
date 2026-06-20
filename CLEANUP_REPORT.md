# ChampSim script cleanup — execution report

Repo: `C:\Users\vasum\VB_champsim_Optimised_Hermesed`
Started: session of 2026-06-20

## Rules applied
- Delete = `v13` OR `v14_*` OR non-`v14-9` bulk.
- Rename helpers: drop `_v14` (quick_v14, qrun_champsim_v14, qbuildPrefetcher_v14), fix internal calls.
- HETERO bulks with `HETERO_RUN=0` -> rename HETERO->HOMO (9 files). `=1` stay (2).
- Phase-by-step. No `2>/dev/null` / `2>&1` / `||true`.

## OPEN / pending
- "NO NAMES" meaning still unknown — left out of scope until recalled.

---

## Phase 1 — DELETE (10 files)
Status: DONE — all 10 `git rm`'d (staged), verified none remain.

Targets:
- v13: qbuildPrefetcher_v13.sh, qrun_champsim_v13.sh, quick_v13.sh, quick_v13_drainer.sh
- v14_*: quick_v14_drainer.sh, quick_v14_Pythia.sh, quickByP_v14_bulk.sh, "quickByP_v14_bulk copy.sh", quickByP_v14_bulk_fixed.sh
- non-v14-9 bulk: quickByP_v14-8_bulk.sh

## Phase 2 — RENAME helpers (drop _v14) + fix calls
Status: IN PROGRESS

NOW I WILL: git mv the 3 helpers to drop _v14, IN ORDER TO match the requested
naming, THEN rewrite internal references so calls don't break.
- quick_v14.sh -> quick.sh
- qrun_champsim_v14.sh -> qrun_champsim.sh
- qbuildPrefetcher_v14.sh -> qbuildPrefetcher.sh
Live refs to fix: PfBuilder/PfRunner vars in quick_v14.sh (l30/31) and
qrun_champsim_v14.sh (l15/16); usage echoes "quick_v14.sh" in the 12 surviving
bulks; bootstrap_workers.sh remote smoke call `./quick_v14.sh` (l72) + comments.
FLAG: quick_v14.sh l365 echo points at quick_v14_drainer.sh which was DELETED in
Phase 1 -> dangling reference, will report for user decision (not invented).
Decision: pure-comment refs in win_*/_win.sh port files left as-is (out of "call" scope).

DONE: git mv'd 3 helpers; PfBuilder=qbuildPrefetcher, PfRunner=qrun_champsim in
quick.sh + qrun_champsim.sh; `quick.sh` ref updated in 12 bulks + bootstrap l72 call.
`sh -n` PASS on quick.sh / qrun_champsim.sh / qbuildPrefetcher.sh.
Leftover (intentional, comments only): win_quick.sh l2, win_qrun.sh l2, win_qbuild.sh l3,
plus champsim_v17/, champsim_v18_TEST_OPT/ *_win.sh port comments — name the old Linux files.
RESOLVED (user: "CUT OUT THAT DRAIN FLAG"): removed --drain end-to-end from quick.sh
— DRAIN_FILE var, --drain arg-parse case, 3-line help block (cited deleted
quick_v14_drainer.sh), and the drain-mode append block. grep drain = none; sh -n PASS.

## Phase 3 — RENAME HETERO->HOMO (HETERO_RUN=0)
Status: DONE
NOW-I-DID: git mv (HETERO-bulk -> HOMO-bulk) for the 9 files with HETERO_RUN=0;
kept HETERO for the 2 with HETERO_RUN=1 (_Heteros, _SPEC_HETEROv2).
No collisions, no cross-references. _SPEC_HETERO_1c suffix kept (only primary token changed).
Renamed: base,_GAP,_GAP_Repl,_GAP-A14,_LIGRA,_LLMs,_SPEC,_SPEC_HETERO_1c,_SPEC_LLM_A14.

## Phase 9 — Root file dispositions (user list)
Status: DONE + VERIFIED.
- Helper_SCRIPTS/ (created): git mv 12 — bootstrap_workers.sh, champsim_renice_watcher.sh,
  check_ipc_diffs.py, check_remote_dirs.sh, copy_winners.sh, dist_status.sh, extract.py,
  kill_young_remote.sh, merge_logs_singleton.sh, merge_logs_v2.sh, run_guarded.sh, watch_hosts.sh.
  Plain move (user did not ask path-fix). No external callers broken (repo-wide grep = none).
- ToDEL_DIR/ (created): git mv CHAMPSIM_RESULTS.db.
- quickSim/: git mv qcheck_consistency.sh + FIXED: added HERE self-locate (l1),
  updated dead name ./quick_v11.sh -> "$HERE/quick.sh" (x2, l4/l32). quick_v11.sh confirmed
  absent; quick.sh is its descendant. bash -n PASS.
- KEEP at root (untouched): 000-Repo_Operate.sh, 001-setup-env.sh, 002-restore_AOCC.sh,
  0IPC_Throughput.sh, 0pdf_txt_extract.py.

FLAGGED / NEEDS HUMAN:
1. qcheck: flag-compatibility of current quick.sh with the old quick_v11.sh CLI
   (-p/--L1/--L2/--L3/--trace/-d/-c/-bypca/--l1byp/-f) is UNVERIFIED — needs a real run
   (build+traces) to confirm. Name updated on best-evidence (quick.sh is the descendant).
2. qcheck lines ~5/~33 have PRE-EXISTING `2>&1` (project BAN). Not introduced by me;
   left untouched (only name/path fix requested). Recommend revisiting.
3. Helper_SCRIPTS movers with pre-existing hardcoded /home/cc paths (check_ipc_diffs.py,
   extract.py, copy_winners.sh, merge_logs_*) remain broken-on-Windows regardless of
   location — original HANDOFF de-hardcode scope, not fixed here.

## Phase 8 — Move 13 bulks into bulkSim/ + cd-fix (agent)
Status: IN PROGRESS — agent launched.
NOW I WILL: agent creates bulkSim/, git mv all quickByP_v14-9*.sh in, adds
HERE/ROOT + `cd "$ROOT"` (repo root = parent of bulkSim) so LOCAL repo-root-relative
paths keep resolving; LEAVES remote paths (REMOTE_BASE=/home/cc, ssh) untouched;
updates usage-text quick.sh->quickSim/quick.sh; verifies bash -n (bash, not sh).
Pre-scoped: no external callers (doc/README hits are other scripts). Pre-existing
`df /home/cc` local check NOT fixed here (separate de-hardcode concern) — flagged.

DONE + INDEPENDENTLY VERIFIED:
- All 13 in bulkSim/, none at root. bash -n PASS x13. External callers: NONE (re-confirmed).
- KEY DEVIATION (correct): bulks ALREADY self-locate via existing
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)" (~l625) and derive every local
  path as ${ROOT}/... (CWD-independent). Adding a fresh HERE/ROOT cd-preamble would have
  COLLIDED with that load-bearing ROOT. Agent instead retargeted the existing self-locate
  to the PARENT: ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)" so ROOT still = repo
  root post-move. Same fix applied to RESUME_DIR (--resume-from base, ~l375).
- Verified exactly ONE ROOT= per file (no double-def), zero stale old-form self-locate
  lines, and FUNCTIONAL test: ROOT resolves to repo root. RESUME_DIR retargeted too.
- Usage text -> "quickSim/quick.sh" in all 13 (only quick.sh ref; help text, no real call).
- LEFT by design: REMOTE_BASE=/home/cc (l19), ssh/rsync targets, ${#$ROOT/} remote derivations.
- FLAG: pre-existing `df ... /home/cc` local-disk check in all 13 — NOT fixed (de-hardcode scope).
- NOTE: `act` hook still absent on PATH (waived this session).

## Phase 7 — 000-BashHelper_GlobalOutput.sh (user asked: is it used?)
Status: DONE.
FINDING (verified 3 ways incl. git grep at HEAD): the file is UNUSED in this repo
— no source/path ref, its functions (set_output_file, simulation_file_postProcessing,
merge_simulation_results) matched ONLY itself. Its logic is reimplemented INLINE in
quickSim/quick.sh and all quickByP_v14-9* bulks (they build `global_results` themselves).
ACTION (user choice): git mv into quickSim/ (shelved alongside run-scripts). Nothing
sources it, so no path fixes needed.

## Phase 6 — Move 6 run-scripts into quickSim/ + cd-fix (agent)
Status: IN PROGRESS — agent launched.
NOW I WILL: agent creates quickSim/, git mv {quick.sh,qrun_champsim.sh,
qbuildPrefetcher.sh,win_quick.sh,win_qrun.sh,win_qbuild.sh}, adds HERE/ROOT
self-locate + `cd "$ROOT"` (repo root = parent of quickSim) so resource paths
keep working; fixes Linux-trio sibling calls to "$HERE/..."; fixes external
callers 000-Repo_Operate.sh (l291,l306) + bootstrap_workers.sh l72 (REMOTE).
Goal: runnable from inside OR outside the dir. Verify sh -n on all 6.

DONE + INDEPENDENTLY VERIFIED:
- All 6 in quickSim/, none at root. sh -n PASS x6. cd "$ROOT" preamble in all 6.
- quick.sh sibling calls -> "$HERE/$PfBuilder".sh / "$HERE/$PfRunner".sh (l408,417,442,467).
- win_quick.sh siblings via "$HERE/..." (l116,122). qrun/qbuild define Pf vars but never call -> nothing to fix.
- External callers fixed: 000-Repo_Operate.sh l291,l306 -> quickSim/; bootstrap_workers.sh l72 (REMOTE) -> ./quickSim/quick.sh.
- Bulk usage strings = text only, no real call -> left.

FLAGGED / NEEDS HUMAN:
1. win_ trio (win_quick/win_qrun/win_qbuild.sh) were UNTRACKED in git, not tracked.
   Agent git add'd then git mv'd them — now tracked at new path, no content lost.
2. 000-Repo_Operate.sh l291 chmod uses $PWD which (per l288-289) still holds the
   PARENT dir, so `$PWD/quickSim/quick.sh` is relative to the wrong base — a
   PRE-EXISTING path bug, not introduced here. Real launch l306 is correct.
   Left as mirror-of-move; user to decide intended base.
3. quick.sh l417 has pre-existing `2>&1` (stderr captured into $output var, then
   checked — not error-suppression). Predates this work; left untouched.

## Phase 5 — Move papers into PAPERS/ (user request)
Status: DONE. git mv'd all 7 root PDFs (all tracked) into new PAPERS/. None left at root.
FOLLOW-UP (user: Hermes_clean.paper missed): first pass only globbed *.pdf. Moved the
5 missed *.paper files too: CARE_paper_clean.paper, Hermes_clean.paper,
IIT_C-AMAT_clean.paper, VERSIONOFICCD.paper, ZION.paper. Final sweep (paper/iccd/mars/
camera/zion/hermes/amat) = no documents left at root. PAPERS/ now holds 12 files.
NOTE (not actioned): extract.py still points at absolute "/home/cc/.../ICCD26_MARS (4).pdf"
— both a /home/cc path AND now a stale location (file is in PAPERS/). Flagged only.

## Phase 4 — Agent union/identical analysis
Status: DONE.
NOW I WILL: have a subagent diff all 13 surviving v14-9 bulks against base
quickByP_v14-9_bulk.sh, IN ORDER TO classify each as IDENTICAL-CONFIG
(only commented const-list entries or a single PF/REPL/OCP/MODEL/HETERO_RUN
toggle) vs CODE-DIFF (real logic change). Results appended on agent return.
Surviving 13: _bulk(base), HOMO base,_GAP,_GAP_Repl,_GAP-A14,_LIGRA,_LLMs,
_MARS_OCP_mixes_GAP_SPEC,_SPEC,_SPEC_HETERO_1c,_SPEC_LLM_A14; HETERO _Heteros,_SPEC_HETEROv2.

DONE — agent result (base = quickByP_v14-9_bulk.sh):

HONEST CAVEAT: the chosen base is an OLDER/simpler revision. All 12 other files
share one identical large block of genuine new code (HETERO engine, host monitors,
background sweep thread, [HEALTH] monitor, parallel rsync). So strictly none is
"config-only vs the base". Classification below is the per-file delta ABOVE that
shared engine (i.e. collapsible-to-base+config or not).

IDENTICAL-CONFIG cluster (10) — collapsible to "shared engine + a config block":
  HOMO base, _GAP, _GAP_Repl, _GAP-A14, _LIGRA, _LLMs, _SPEC, _SPEC_HETERO_1c,
  _SPEC_LLM_A14, and HETERO _SPEC_HETEROv2.
  Differ only by: HETERO_RUN (0; =1 only for _SPEC_HETEROv2), CORE_LIST,
  TRACE_LIST comment-toggles (SPEC/GAP/LIGRA/LLM/HETERO), REPL/HERMES/OCP toggles,
  `arch` string (A14 / glc_Hetero4c), scalar tunables (PROCS, SWEEP, BUILD_JOBS,
  REMOTE_HOSTS, OUT_DIR_SIGNATURE).

GENUINE CODE-DIFF (2):
  - _MARS_OCP_mixes_GAP_SPEC: flips MARS+OCP model-combo filter from forbidden
    (echo 1) to allowed (echo 2) — real control-flow change. (uses champsim_v17_DRAM)
  - HETERO _Heteros: HETERO_RUN=1; refactors remote bin/trace sync into batched
    single-SSH mkdir + parallel rsync with _rpids wait-loop — real logic change.
