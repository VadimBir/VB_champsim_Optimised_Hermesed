"""STAT_PATTERNS — grep/regex definitions for aggregator.py.

CP1 scope: ONLY ipc_final_avg. Extended incrementally at each checkpoint.

Aggregation choices (fixed per stat, documented here):
    ipc_final_avg : scalar from `FINAL ROI CORE AVG IPC: ;<val>;`.
                    This is already a core-averaged value emitted by champsim,
                    so no further Python aggregation is applied.

Missing-data sentinels:
    -1 = run still in progress / metric absent with no error markers
    -2 = run errored (assert, SIGSEGV, Aborted, Killed, deadlock, Segmentation)

NEVER fabricate, hardcode, or substitute any value. If grep fails, sentinel.
"""

STAT_PATTERNS = {
    "ipc_final_avg": (
        r"FINAL ROI CORE AVG IPC:\s*;([\d.]+);",
        "global",
    ),
}

# Per-level field extraction. Format in log:
#     Core_;<N>; <LVL>;_<spaces>;<field>;<spaces><val>;
# LVL ∈ {L1D, L1I, L2C, LLC}. Fields vary.
PER_LEVEL_FIELD_RE = (
    r"Core_;(\d+);\s*(L1D|L1I|L2C|LLC);_\s*;([A-Za-z0-9_\-]+);\s*([\-\d.]+|nan|inf);"
)

# Fields to aggregate across cores as SUM (counts)
SUM_FIELDS = {
    "LOAD_MSHR_cap", "RFO_cap", "Pf_MSHR_cap", "WrBk_MSHR_cap",
    "pureAdm_LOAD", "pureAdm_RFO", "pureAdm_Pf", "pureAdm_WrBk",
    "byp_req", "byp_issued",
    "total_access", "total_hit", "total_miss",
    "loads", "load_hit", "load_miss",
    "load_wByP_acc", "load_wByP_hit", "load_wByP_byp", "load_wByP_miss",
    "RFOs", "RFO_hit", "RFO_miss",
    "prefetches", "prefetch_hit", "prefetch_miss",
    "writebacks", "writeback_hit", "writeback_miss",
    "pf_requested", "pf_issued", "pf_useful", "pf_useless", "pf_late",
}

# Fields to aggregate across cores as AVERAGE (rates)
AVG_FIELDS = {
    "MPKI", "APC", "LPM", "C-AMAT", "MST", "avg_miss_lat",
    "BLT_mshr_avg", "BLT_mshr_pk", "BLT_byp_avg", "BLT_mlp_avg", "BLT_mlp_pk",
}

# Metric FAMILIES — one sheet per family. Each trace header spans 3 merged
# cells and the 3 cells below hold L1D / L2C / LLC values respectively.
# Format: (sheet_title, field_name). Levels hard-coded as (L1D, L2C, LLC).
CP5_FAMILIES = [
    ("MPKI",         "MPKI"),
    ("Loads",        "loads"),
    ("Miss",         "load_miss"),
    ("LoadMSHRcap",  "LOAD_MSHR_cap"),
    ("PfMSHRcap",    "Pf_MSHR_cap"),
    ("pureAdm_LOAD", "pureAdm_LOAD"),
    ("BLT_mshr_pk",  "BLT_mshr_pk"),
    ("BypReq",       "byp_req"),
    ("BypIss",       "byp_issued"),
    ("BLT_mshr",     "BLT_mshr_avg"),
    ("BLT_byp",      "BLT_byp_avg"),
    ("BLT_mlp",      "BLT_mlp_avg"),
    ("BLT_mlp_pk",   "BLT_mlp_pk"),
    ("PureMiss",     "LPM_m"),
    ("Overlap",      "LPM_x"),
]

LEVELS_TRIPLE = ("L1D", "L2C", "LLC")

ERROR_MARKERS = (
    "assert",
    "Segmentation",
    "SIGSEGV",
    "Aborted",
    "Killed",
    "deadlock",
)
