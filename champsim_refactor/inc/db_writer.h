/*******************************************************************************
 * db_writer.h — ChampSim Results SQLite Writer
 *
 * Place in: inc/db_writer.h
 * Add to Makefile LIBS: -lsqlite3
 * Requires: apt install libsqlite3-dev
 *
 * Called ONCE at end of simulation. Zero hot-path impact.
 * WAL mode + busy_timeout(30s) → safe with 128+ concurrent writers.
 ******************************************************************************/
#pragma once

#include <sqlite3.h>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <string>

// ── Config struct: populated from CLI args in main() ────────────────────────
struct ChampsimDBConfig {
    char db_path[512] = {0};
    char arch[64]     = {0};
    char bypass[128]  = {0};
    char pf_l1[256]   = {0};
    char pf_l2[256]   = {0};
    char pf_l3[256]   = {0};
    bool use_epoch    = false;
};

// ── Schema ──────────────────────────────────────────────────────────────────
static const char* CHAMPSIM_DB_SCHEMA = R"SQL(
PRAGMA journal_mode=WAL;
PRAGMA busy_timeout=30000;
PRAGMA synchronous=NORMAL;

CREATE TABLE IF NOT EXISTS runs (
    run_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    arch        TEXT NOT NULL,
    num_cpus    INTEGER NOT NULL,
    trace       TEXT NOT NULL,
    bypass      TEXT NOT NULL DEFAULT '',
    pf_l1       TEXT NOT NULL DEFAULT 'no',
    pf_l2       TEXT NOT NULL DEFAULT 'no',
    pf_l3       TEXT NOT NULL DEFAULT 'no',
    avg_ipc     REAL,
    epoch       INTEGER NOT NULL DEFAULT 0,
    timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(arch, num_cpus, trace, bypass, pf_l1, pf_l2, pf_l3, epoch)
);

CREATE TABLE IF NOT EXISTS core_stats (
    run_id              INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    cpu                 INTEGER NOT NULL,
    instructions        INTEGER,
    cycles              INTEGER,
    ipc                 REAL,
    major_fault         INTEGER DEFAULT 0,
    minor_fault         INTEGER DEFAULT 0,
    branch_pred_acc     REAL,
    branch_mpki         REAL,
    avg_rob_occ_misp    REAL,
    PRIMARY KEY (run_id, cpu)
);

CREATE TABLE IF NOT EXISTS cache_stats (
    run_id          INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    cpu             INTEGER NOT NULL,
    cache_name      TEXT NOT NULL,
    mpki            REAL,
    stall_load_mshr INTEGER DEFAULT 0,
    stall_rfo       INTEGER DEFAULT 0,
    stall_pf_mshr   INTEGER DEFAULT 0,
    stall_wrbk      INTEGER DEFAULT 0,
    apc             REAL DEFAULT 0,
    lpm             REAL DEFAULT 0,
    c_amat          REAL DEFAULT 0,
    total_access    INTEGER,
    total_hit       INTEGER,
    total_miss      INTEGER,
    loads           INTEGER,
    load_hit        INTEGER,
    load_miss       INTEGER,
    rfos            INTEGER,
    rfo_hit         INTEGER,
    rfo_miss        INTEGER,
    prefetches      INTEGER,
    prefetch_hit    INTEGER,
    prefetch_miss   INTEGER,
    writebacks      INTEGER,
    writeback_hit   INTEGER,
    writeback_miss  INTEGER,
    pf_requested    INTEGER,
    pf_issued       INTEGER,
    pf_useful       INTEGER,
    pf_useless      INTEGER,
    pf_late         INTEGER,
    avg_miss_lat    REAL,
    PRIMARY KEY (run_id, cpu, cache_name)
);

CREATE TABLE IF NOT EXISTS dram_stats (
    run_id          INTEGER NOT NULL REFERENCES runs(run_id) ON DELETE CASCADE,
    channel         INTEGER NOT NULL,
    rq_row_hit      INTEGER,
    rq_row_miss     INTEGER,
    wq_row_hit      INTEGER,
    wq_row_miss     INTEGER,
    wq_full         INTEGER,
    dbus_congested  INTEGER,
    PRIMARY KEY (run_id, channel)
);

CREATE INDEX IF NOT EXISTS idx_runs_trace ON runs(trace);
CREATE INDEX IF NOT EXISTS idx_runs_pf ON runs(pf_l1, pf_l2, pf_l3);
CREATE INDEX IF NOT EXISTS idx_runs_arch_cpus ON runs(arch, num_cpus);
)SQL";

// ── Helpers ─────────────────────────────────────────────────────────────────

static inline std::string _db_extract_trace_name(const char* trace_path) {
    const char* base = std::strrchr(trace_path, '/');
    base = base ? base + 1 : trace_path;
    std::string name(base);
    // strip .champsim.xz / .champsim.gz / .xz / .gz
    auto p = name.find(".champsim");
    if (p != std::string::npos) { name = name.substr(0, p); return name; }
    p = name.rfind(".xz");
    if (p != std::string::npos && p == name.size() - 3) name = name.substr(0, p);
    p = name.rfind(".gz");
    if (p != std::string::npos && p == name.size() - 3) name = name.substr(0, p);
    return name;
}

static inline bool _db_exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[ChampsimDB] SQL error: %s\n  SQL: %.200s\n", err ? err : "unknown", sql);
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ── Main store function ─────────────────────────────────────────────────────
// Call from main.cc after all ROI stats are printed.
// Accesses globals: ooo_cpu[], uncore, lpm[][]
// Passes: major_fault[], minor_fault[] (file-scope in main.cc)

#include "ooo_cpu.h"
#include "uncore.h"
#include "lpm_tracker.h"

static inline void champsim_db_store(
    const ChampsimDBConfig& cfg,
    double avg_ipc,
    const uint64_t* p_major_fault,
    const uint64_t* p_minor_fault)
{
    if (cfg.db_path[0] == '\0') return;

    sqlite3* db = nullptr;
    int rc = sqlite3_open(cfg.db_path, &db);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[ChampsimDB] Cannot open DB: %s — %s\n",
                     cfg.db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return;
    }

    // Schema init
    if (!_db_exec(db, CHAMPSIM_DB_SCHEMA)) { sqlite3_close(db); return; }

    // Begin transaction (IMMEDIATE = acquire write lock now, others wait)
    if (!_db_exec(db, "BEGIN IMMEDIATE")) { sqlite3_close(db); return; }

    // ── Build trace name ────────────────────────────────────────────────
    std::string trace_combined;
    for (int i = 0; i < NUM_CPUS; i++) {
        if (i > 0) trace_combined += "|";
        trace_combined += _db_extract_trace_name(ooo_cpu[i].trace_string);
    }

    // ── Epoch value ─────────────────────────────────────────────────────
    int64_t epoch_val = cfg.use_epoch ? (int64_t)time(nullptr) : 0;

    // ── INSERT runs ─────────────────────────────────────────────────────
    {
        const char* sql = "INSERT OR REPLACE INTO runs "
            "(arch, num_cpus, trace, bypass, pf_l1, pf_l2, pf_l3, avg_ipc, epoch) "
            "VALUES (?,?,?,?,?,?,?,?,?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, cfg.arch, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, NUM_CPUS);
        sqlite3_bind_text(stmt, 3, trace_combined.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, cfg.bypass, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, cfg.pf_l1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, cfg.pf_l2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, cfg.pf_l3, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 8, avg_ipc);
        sqlite3_bind_int64(stmt, 9, epoch_val);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            std::fprintf(stderr, "[ChampsimDB] INSERT runs failed: %s\n", sqlite3_errmsg(db));
            _db_exec(db, "ROLLBACK");
            sqlite3_close(db);
            return;
        }
    }

    // Get run_id (the one we just inserted or replaced)
    int64_t run_id = 0;
    {
        const char* sql = "SELECT run_id FROM runs WHERE arch=? AND num_cpus=? AND trace=? "
                          "AND bypass=? AND pf_l1=? AND pf_l2=? AND pf_l3=? AND epoch=?";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, cfg.arch, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, NUM_CPUS);
        sqlite3_bind_text(stmt, 3, trace_combined.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, cfg.bypass, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 5, cfg.pf_l1, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 6, cfg.pf_l2, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, cfg.pf_l3, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 8, epoch_val);
        if (sqlite3_step(stmt) == SQLITE_ROW)
            run_id = sqlite3_column_int64(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (run_id == 0) {
        std::fprintf(stderr, "[ChampsimDB] Failed to retrieve run_id\n");
        _db_exec(db, "ROLLBACK");
        sqlite3_close(db);
        return;
    }

    // ── DELETE old child rows (for REPLACE case) ────────────────────────
    {
        char delsql[256];
        std::snprintf(delsql, sizeof(delsql),
            "DELETE FROM core_stats WHERE run_id=%lld", (long long)run_id);
        _db_exec(db, delsql);
        std::snprintf(delsql, sizeof(delsql),
            "DELETE FROM cache_stats WHERE run_id=%lld", (long long)run_id);
        _db_exec(db, delsql);
        std::snprintf(delsql, sizeof(delsql),
            "DELETE FROM dram_stats WHERE run_id=%lld", (long long)run_id);
        _db_exec(db, delsql);
    }

    // ── INSERT core_stats ───────────────────────────────────────────────
    {
        const char* sql = "INSERT INTO core_stats "
            "(run_id,cpu,instructions,cycles,ipc,major_fault,minor_fault,"
            "branch_pred_acc,branch_mpki,avg_rob_occ_misp) "
            "VALUES (?,?,?,?,?,?,?,?,?,?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        for (int i = 0; i < NUM_CPUS; i++) {
            double ipc = (double)ooo_cpu[i].finish_sim_instr / (double)ooo_cpu[i].finish_sim_cycle;
            double bp_acc = (ooo_cpu[i].num_branch > 0) ?
                100.0 * (ooo_cpu[i].num_branch - ooo_cpu[i].branch_mispredictions) / ooo_cpu[i].num_branch : 0;
            double bp_mpki = (ooo_cpu[i].num_retired > ooo_cpu[i].warmup_instructions) ?
                1000.0 * ooo_cpu[i].branch_mispredictions / (ooo_cpu[i].num_retired - ooo_cpu[i].warmup_instructions) : 0;
            double rob_occ = (ooo_cpu[i].branch_mispredictions > 0) ?
                1.0 * ooo_cpu[i].total_rob_occupancy_at_branch_mispredict / ooo_cpu[i].branch_mispredictions : 0;

            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, run_id);
            sqlite3_bind_int(stmt, 2, i);
            sqlite3_bind_int64(stmt, 3, (int64_t)ooo_cpu[i].finish_sim_instr);
            sqlite3_bind_int64(stmt, 4, (int64_t)ooo_cpu[i].finish_sim_cycle);
            sqlite3_bind_double(stmt, 5, ipc);
            sqlite3_bind_int64(stmt, 6, (int64_t)p_major_fault[i]);
            sqlite3_bind_int64(stmt, 7, (int64_t)p_minor_fault[i]);
            sqlite3_bind_double(stmt, 8, bp_acc);
            sqlite3_bind_double(stmt, 9, bp_mpki);
            sqlite3_bind_double(stmt, 10, rob_occ);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }

    // ── INSERT cache_stats (per cpu × per cache level) ──────────────────
    {
        const char* sql = "INSERT INTO cache_stats "
            "(run_id,cpu,cache_name,mpki,"
            "stall_load_mshr,stall_rfo,stall_pf_mshr,stall_wrbk,"
            "apc,lpm,c_amat,"
            "total_access,total_hit,total_miss,"
            "loads,load_hit,load_miss,"
            "rfos,rfo_hit,rfo_miss,"
            "prefetches,prefetch_hit,prefetch_miss,"
            "writebacks,writeback_hit,writeback_miss,"
            "pf_requested,pf_issued,pf_useful,pf_useless,pf_late,"
            "avg_miss_lat) "
            "VALUES (?,?,?,?, ?,?,?,?, ?,?,?, ?,?,?, ?,?,?, ?,?,?, ?,?,?, ?,?,?, ?,?,?,?,?, ?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);

        // Lambda: insert one cache level for one cpu
        auto insert_cache = [&](int cpu, CACHE* cache, const char* name) {
            uint64_t ta = 0, th = 0, tm = 0;
            for (uint32_t t = 0; t < NUM_TYPES; t++) {
                ta += cache->roi_access[cpu][t];
                th += cache->roi_hit[cpu][t];
                tm += cache->roi_miss[cpu][t];
            }
            double mpki = (ooo_cpu[cpu].finish_sim_instr > 0) ?
                tm * 1000.0 / ooo_cpu[cpu].finish_sim_instr : 0;
            double miss_lat = (tm > 0) ?
                1.0 * cache->total_miss_latency / tm : 0;

            sqlite3_reset(stmt);
            int c = 1;
            sqlite3_bind_int64(stmt, c++, run_id);
            sqlite3_bind_int(stmt, c++, cpu);
            sqlite3_bind_text(stmt, c++, name, -1, SQLITE_STATIC);
            sqlite3_bind_double(stmt, c++, mpki);
            // stalls
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->STALL[0]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->STALL[1]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->STALL[2]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->STALL[3]);
            // lpm metrics
            sqlite3_bind_double(stmt, c++, lpm[cpu][cache->cache_type].met[MET_G].apc_accessesDivActiveMemCy);
            sqlite3_bind_double(stmt, c++, lpm[cpu][cache->cache_type].met[MET_G].lpmr_activeMemCyDivIdealCy);
            sqlite3_bind_double(stmt, c++, lpm[cpu][cache->cache_type].met[MET_G].camat_activeMemCyDivAccesses);
            // totals
            sqlite3_bind_int64(stmt, c++, (int64_t)ta);
            sqlite3_bind_int64(stmt, c++, (int64_t)th);
            sqlite3_bind_int64(stmt, c++, (int64_t)tm);
            // loads
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_access[cpu][0]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_hit[cpu][0]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_miss[cpu][0]);
            // rfos
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_access[cpu][1]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_hit[cpu][1]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_miss[cpu][1]);
            // prefetches
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_access[cpu][2]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_hit[cpu][2]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_miss[cpu][2]);
            // writebacks
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_access[cpu][3]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_hit[cpu][3]);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->roi_miss[cpu][3]);
            // pf stats
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->pf_requested);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->pf_issued);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->pf_useful);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->pf_useless);
            sqlite3_bind_int64(stmt, c++, (int64_t)cache->pf_late);
            // latency
            sqlite3_bind_double(stmt, c++, miss_lat);

            sqlite3_step(stmt);
        };

        for (int i = 0; i < NUM_CPUS; i++) {
            insert_cache(i, &ooo_cpu[i].L1D, "L1D");
            insert_cache(i, &ooo_cpu[i].L1I, "L1I");
            insert_cache(i, &ooo_cpu[i].L2C, "L2C");
            insert_cache(i, &uncore.LLC,      "LLC");
        }
        sqlite3_finalize(stmt);
    }

    // ── INSERT dram_stats ───────────────────────────────────────────────
    {
        const char* sql = "INSERT INTO dram_stats "
            "(run_id,channel,rq_row_hit,rq_row_miss,wq_row_hit,wq_row_miss,wq_full,dbus_congested) "
            "VALUES (?,?,?,?,?,?,?,?)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        for (uint32_t ch = 0; ch < DRAM_CHANNELS; ch++) {
            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, run_id);
            sqlite3_bind_int(stmt, 2, (int)ch);
            sqlite3_bind_int64(stmt, 3, (int64_t)uncore.DRAM.RQ[ch].ROW_BUFFER_HIT);
            sqlite3_bind_int64(stmt, 4, (int64_t)uncore.DRAM.RQ[ch].ROW_BUFFER_MISS);
            sqlite3_bind_int64(stmt, 5, (int64_t)uncore.DRAM.WQ[ch].ROW_BUFFER_HIT);
            sqlite3_bind_int64(stmt, 6, (int64_t)uncore.DRAM.WQ[ch].ROW_BUFFER_MISS);
            sqlite3_bind_int64(stmt, 7, (int64_t)uncore.DRAM.WQ[ch].FULL);
            sqlite3_bind_int64(stmt, 8, (int64_t)uncore.DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }

    // ── COMMIT ──────────────────────────────────────────────────────────
    _db_exec(db, "COMMIT");
    sqlite3_close(db);

    std::fprintf(stdout, "[ChampsimDB] Stored run_id=%lld trace=%s pf=L1D:%s+L2C:%s+LLC:%s avg_ipc=%.5f → %s\n",
                 (long long)run_id, trace_combined.c_str(),
                 cfg.pf_l1, cfg.pf_l2, cfg.pf_l3, avg_ipc, cfg.db_path);
}
