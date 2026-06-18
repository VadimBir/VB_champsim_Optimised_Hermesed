// trace_decode.cc — single-threaded ChampSim xz trace decoder + phase analyzer.
// One pass: libLZMA stream decode -> per-instr PhaseAnalyzer hook -> CSV.
// NO threads, NO queue, NO chrono, NO fake parallelism.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <lzma.h>

// ----------------------------------------------------------------------------
// input_instr — verbatim copy from champsim_v14/inc/instruction.h (28-59).
// NUM_INSTR_DESTINATIONS=2, NUM_INSTR_SOURCES=4 (from instruction.h:18-19).
// POD layout MUST match champsim binary trace records exactly.
// ----------------------------------------------------------------------------
#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES      4

class input_instr {
public:
    uint64_t ip;

    uint8_t is_branch;
    uint8_t branch_taken;

    uint8_t destination_registers[NUM_INSTR_DESTINATIONS];
    uint8_t source_registers[NUM_INSTR_SOURCES];

    uint64_t destination_memory[NUM_INSTR_DESTINATIONS];
    uint64_t source_memory[NUM_INSTR_SOURCES];

    input_instr() {
        ip = 0;
        is_branch = 0;
        branch_taken = 0;
        for (uint32_t i = 0; i < NUM_INSTR_SOURCES; i++) {
            source_registers[i] = 0;
            source_memory[i]    = 0;
        }
        for (uint32_t i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
            destination_registers[i] = 0;
            destination_memory[i]    = 0;
        }
    }
};

static constexpr size_t LZMA_INBUF_BYTES   = 64ULL  * 1024;          //  64 KiB
static constexpr size_t LZMA_OUTBUF_BYTES  = 8ULL   * 1024 * 1024;   //   8 MiB

// ----------------------------------------------------------------------------
// MicroBucket + PhaseAnalyzer (unchanged) — fed in-order by decoder.
// ----------------------------------------------------------------------------
struct MicroBucket {
    uint32_t loads          = 0;
    uint32_t stores         = 0;
    uint32_t branches       = 0;
    uint32_t taken_branches = 0;
    std::vector<std::pair<uint64_t, uint32_t>> pcs;

    void clear() {
        loads = stores = branches = taken_branches = 0;
        pcs.clear();
    }
};

class PhaseAnalyzer {
public:
    PhaseAnalyzer(uint64_t micro, uint64_t fine_mult, uint64_t base_mult,
                  const std::string& out_path)
        : M_(micro), FM_(fine_mult), BM_(base_mult),
          F_(micro * fine_mult), B_(micro * base_mult),
          fine_ring_(fine_mult), base_ring_(base_mult),
          out_path_(out_path) {
        fine_pc_count_.reserve(2048);
        base_pc_count_.reserve(70000);
        out_.open(out_path_, std::ios::out | std::ios::trunc);
        if (out_.is_open()) {
            char hbuf[1024];
            int hn = std::snprintf(hbuf, sizeof(hbuf),
                "%10s,%5s,"
                "%17s,%17s,%17s,%17s,"
                "%17s,%17s,%17s,%17s,"
                "%15s,%15s,"
                "%10s,%14s\n",
                "instr_idx","valid",
                "fine_loads_per_K","fine_stores_per_K","fine_branches_per_K","fine_taken_per_K",
                "base_loads_per_K","base_stores_per_K","base_branches_per_K","base_taken_per_K",
                "fine_unique_pcs","base_unique_pcs",
                "composite","cust_composite");
            if (hn > 0) out_.write(hbuf, hn);
        }
    }

    bool ok() const { return out_.is_open(); }
    const std::string& path() const { return out_path_; }

    inline void add(const input_instr& ins) {
        MicroBucket& cur = cur_bucket_;

        if (ins.is_branch) {
            cur.branches++;
            if (ins.branch_taken) cur.taken_branches++;
        }
        for (uint32_t k = 0; k < NUM_INSTR_SOURCES; k++) {
            if (ins.source_memory[k]) { cur.loads++; break; }
        }
        for (uint32_t k = 0; k < NUM_INSTR_DESTINATIONS; k++) {
            if (ins.destination_memory[k]) { cur.stores++; break; }
        }

        bool found = false;
        for (auto& p : cur.pcs) {
            if (p.first == ins.ip) { p.second++; found = true; break; }
        }
        if (!found) cur.pcs.emplace_back(ins.ip, 1u);

        seen_in_micro_++;
        total_seen_++;
        if (seen_in_micro_ >= M_) {
            close_micro();
        }
    }

    void finalize() {
        if (out_.is_open()) out_.close();
    }

private:
    static inline void add_pc_to_map(
        std::unordered_map<uint64_t, uint32_t>& m,
        const MicroBucket& b)
    {
        for (auto& p : b.pcs) m[p.first] += p.second;
    }
    static inline void sub_pc_from_map(
        std::unordered_map<uint64_t, uint32_t>& m,
        const MicroBucket& b)
    {
        for (auto& p : b.pcs) {
            auto it = m.find(p.first);
            if (it != m.end()) {
                if (it->second <= p.second) m.erase(it);
                else                         it->second -= p.second;
            }
        }
    }

    void close_micro() {
        MicroBucket inserted;
        inserted.loads          = cur_bucket_.loads;
        inserted.stores         = cur_bucket_.stores;
        inserted.branches       = cur_bucket_.branches;
        inserted.taken_branches = cur_bucket_.taken_branches;
        inserted.pcs            = std::move(cur_bucket_.pcs);
        cur_bucket_.clear();
        seen_in_micro_ = 0;

        MicroBucket demoted;
        bool have_demoted = false;
        if (fine_filled_ >= FM_) {
            size_t oldest_fine = (fine_head_ + 1) % FM_;
            MicroBucket& ev = fine_ring_[oldest_fine];
            fine_loads_    -= ev.loads;
            fine_stores_   -= ev.stores;
            fine_branches_ -= ev.branches;
            fine_taken_    -= ev.taken_branches;
            sub_pc_from_map(fine_pc_count_, ev);
            demoted.loads          = ev.loads;
            demoted.stores         = ev.stores;
            demoted.branches       = ev.branches;
            demoted.taken_branches = ev.taken_branches;
            demoted.pcs            = std::move(ev.pcs);
            ev.clear();
            have_demoted = true;
        } else {
            fine_filled_++;
        }

        fine_head_ = (fine_head_ + 1) % FM_;
        fine_loads_    += inserted.loads;
        fine_stores_   += inserted.stores;
        fine_branches_ += inserted.branches;
        fine_taken_    += inserted.taken_branches;
        add_pc_to_map(fine_pc_count_, inserted);
        fine_ring_[fine_head_].loads          = inserted.loads;
        fine_ring_[fine_head_].stores         = inserted.stores;
        fine_ring_[fine_head_].branches       = inserted.branches;
        fine_ring_[fine_head_].taken_branches = inserted.taken_branches;
        fine_ring_[fine_head_].pcs            = std::move(inserted.pcs);

        if (have_demoted) {
            if (base_filled_ >= BM_) {
                size_t oldest_base = (base_head_ + 1) % BM_;
                MicroBucket& ev = base_ring_[oldest_base];
                base_loads_    -= ev.loads;
                base_stores_   -= ev.stores;
                base_branches_ -= ev.branches;
                base_taken_    -= ev.taken_branches;
                sub_pc_from_map(base_pc_count_, ev);
                ev.clear();
            } else {
                base_filled_++;
            }
            base_head_ = (base_head_ + 1) % BM_;
            base_loads_    += demoted.loads;
            base_stores_   += demoted.stores;
            base_branches_ += demoted.branches;
            base_taken_    += demoted.taken_branches;
            add_pc_to_map(base_pc_count_, demoted);
            base_ring_[base_head_].loads          = demoted.loads;
            base_ring_[base_head_].stores         = demoted.stores;
            base_ring_[base_head_].branches       = demoted.branches;
            base_ring_[base_head_].taken_branches = demoted.taken_branches;
            base_ring_[base_head_].pcs            = std::move(demoted.pcs);
        }

        emit_row();
    }

    static inline double safe_log_ratio(double a, double b) {
        const double EPS = 1e-9;
        if (a <= 0.0) a = EPS;
        if (b <= 0.0) b = EPS;
        double r = a / b;
        double lr = std::log(r);
        if (lr > 5.0)  lr = 5.0;
        if (lr < -5.0) lr = -5.0;
        return lr;
    }
    static inline double safe_ratio(double a, double b) {
        const double EPS = 1e-9;
        if (a == 0.0 && b == 0.0) return 1.0;
        if (a < EPS) a = EPS;
        if (b < EPS) b = EPS;
        return a / b;
    }

    void emit_row() {
        if (!out_.is_open()) return;

        bool valid = (fine_filled_ >= FM_) && (base_filled_ >= BM_);
        uint64_t idx = total_seen_;

        if (!valid) return;  // skip rows until both windows fully filled

        double fdiv = (double)F_ / 1000.0;
        double bdiv = (double)B_ / 1000.0;
        double f_loads_pk = (double)fine_loads_    / fdiv;
        double f_stor_pk  = (double)fine_stores_   / fdiv;
        double f_br_pk    = (double)fine_branches_ / fdiv;
        double f_tk_pk    = (double)fine_taken_    / fdiv;
        double b_loads_pk = (double)base_loads_    / bdiv;
        double b_stor_pk  = (double)base_stores_   / bdiv;
        double b_br_pk    = (double)base_branches_ / bdiv;
        double b_tk_pk    = (double)base_taken_    / bdiv;

        size_t fine_uniq = fine_pc_count_.size();
        size_t base_uniq = base_pc_count_.size();
        size_t intersect = 0;
        for (auto& kv : fine_pc_count_) {
            if (base_pc_count_.count(kv.first)) intersect++;
        }
        size_t union_sz = fine_uniq + base_uniq - intersect;
        double jaccard  = (union_sz == 0) ? 1.0
                        : (double)intersect / (double)union_sz;

        double f_total = 0.0;
        for (auto& kv : fine_pc_count_) f_total += kv.second;
        double fine_H = 0.0;
        if (f_total > 0.0) {
            for (auto& kv : fine_pc_count_) {
                double p = kv.second / f_total;
                if (p > 0.0) fine_H -= p * std::log2(p);
            }
        }

        if (emits_since_base_entropy_ == 0 || base_entropy_dirty_) {
            double b_total = 0.0;
            for (auto& kv : base_pc_count_) b_total += kv.second;
            double H = 0.0;
            if (b_total > 0.0) {
                for (auto& kv : base_pc_count_) {
                    double p = kv.second / b_total;
                    if (p > 0.0) H -= p * std::log2(p);
                }
            }
            cached_base_entropy_     = H;
            cached_base_unique_pcs_  = base_pc_count_.size();
            base_entropy_dirty_      = false;
        }
        double base_H = cached_base_entropy_;
        size_t base_uniq_reported = cached_base_unique_pcs_;

        emits_since_base_entropy_++;
        if (emits_since_base_entropy_ >= BASE_ENTROPY_REFRESH) {
            emits_since_base_entropy_ = 0;
            base_entropy_dirty_ = true;
        }

        double entropy_d = fine_H - base_H;

        double l1 = safe_log_ratio(f_loads_pk, b_loads_pk);
        double l2 = safe_log_ratio(f_stor_pk,  b_stor_pk);
        double l3 = safe_log_ratio(f_br_pk,    b_br_pk);
        double l4 = safe_log_ratio(f_tk_pk,    b_tk_pk);
        double jd = 1.0 - jaccard;
        double composite = std::sqrt(l1*l1 + l2*l2 + l3*l3 + l4*l4
                                     + jd*jd + entropy_d*entropy_d);

        // cust_composite = (|fine_loads - fine_stores| * |fine_taken/fine_branches|) / fine_unique_pcs
        // Low cust => loads~=stores AND/OR taken~=0 => signals tight-loop / low-IPC phase entry.
        double ls_diff = std::fabs((double)fine_loads_ - (double)fine_stores_);
        double tb_r    = std::fabs(safe_ratio((double)fine_taken_, (double)fine_branches_));
        double denom_uniq = (fine_uniq == 0) ? 1.0 : (double)fine_uniq;
        double cust_composite = (ls_diff * tb_r) / denom_uniq;

        // Emit IFF cust_composite == 0 AND fine_loads != 0 AND fine_stores != 0.
        // Captures low-IPC tight-loop signal: balanced loads/stores with real activity.
        if (!(cust_composite == 0.0 && fine_loads_ != 0 && fine_stores_ != 0)) return;

        char buf[1024];
        char fl[32],fs[32],fb[32],ft[32];
        char bl[32],bs[32],bb[32],bt[32];
        char cp[32], ccp[32];
        std::snprintf(fl,sizeof(fl),"%.4f",f_loads_pk);
        std::snprintf(fs,sizeof(fs),"%.4f",f_stor_pk);
        std::snprintf(fb,sizeof(fb),"%.4f",f_br_pk);
        std::snprintf(ft,sizeof(ft),"%.4f",f_tk_pk);
        std::snprintf(bl,sizeof(bl),"%.4f",b_loads_pk);
        std::snprintf(bs,sizeof(bs),"%.4f",b_stor_pk);
        std::snprintf(bb,sizeof(bb),"%.4f",b_br_pk);
        std::snprintf(bt,sizeof(bt),"%.4f",b_tk_pk);
        std::snprintf(cp,sizeof(cp),"%.4f",composite);
        std::snprintf(ccp,sizeof(ccp),"%.6f",cust_composite);
        int n = std::snprintf(buf, sizeof(buf),
            "%10llu,%5d,"
            "%17s,%17s,%17s,%17s,"
            "%17s,%17s,%17s,%17s,"
            "%15zu,%15zu,"
            "%10s,%14s\n",
            (unsigned long long)idx, 1,
            fl,fs,fb,ft,
            bl,bs,bb,bt,
            fine_uniq, base_uniq_reported,
            cp, ccp);
        if (n > 0) out_.write(buf, n);
    }

    uint64_t M_;
    uint64_t FM_;
    uint64_t BM_;
    uint64_t F_;
    uint64_t B_;

    std::vector<MicroBucket> fine_ring_;
    std::vector<MicroBucket> base_ring_;
    size_t fine_head_   = (size_t)-1;
    size_t base_head_   = (size_t)-1;
    size_t fine_filled_ = 0;
    size_t base_filled_ = 0;

    MicroBucket cur_bucket_;
    uint64_t    seen_in_micro_ = 0;
    uint64_t    total_seen_    = 0;

    uint64_t fine_loads_    = 0;
    uint64_t fine_stores_   = 0;
    uint64_t fine_branches_ = 0;
    uint64_t fine_taken_    = 0;
    uint64_t base_loads_    = 0;
    uint64_t base_stores_   = 0;
    uint64_t base_branches_ = 0;
    uint64_t base_taken_    = 0;

    std::unordered_map<uint64_t, uint32_t> fine_pc_count_;
    std::unordered_map<uint64_t, uint32_t> base_pc_count_;

    static constexpr uint64_t BASE_ENTROPY_REFRESH = 64;
    uint64_t emits_since_base_entropy_ = 0;
    bool     base_entropy_dirty_       = true;
    double   cached_base_entropy_      = 0.0;
    size_t   cached_base_unique_pcs_   = 0;

    std::string   out_path_;
    std::ofstream out_;
};

// ----------------------------------------------------------------------------
// Decoder — libLZMA stream, inline phase->add per record. Single-threaded.
// ----------------------------------------------------------------------------
struct DecodeStats {
    bool        ok            = true;
    uint64_t    records_read  = 0;
    uint64_t    loads         = 0;
    uint64_t    stores        = 0;
    uint64_t    branches      = 0;
    uint64_t    bytes_decoded = 0;
    std::string err;
};

static DecodeStats run_decoder(const std::string& path,
                               uint64_t limit_instrs,
                               PhaseAnalyzer* phase)
{
    DecodeStats out;

    std::ifstream input_file(path, std::ios::binary);
    if (!input_file.good()) {
        out.ok = false;
        out.err = "cannot open " + path;
        return out;
    }

    uint8_t* inbuf  = nullptr;
    uint8_t* outbuf = nullptr;
    if (posix_memalign((void**)&inbuf,  64, LZMA_INBUF_BYTES)  != 0 ||
        posix_memalign((void**)&outbuf, 64, LZMA_OUTBUF_BYTES) != 0) {
        out.ok = false;
        out.err = "posix_memalign failed";
        return out;
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    lzma_ret r = lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED);
    if (r != LZMA_OK) {
        out.ok = false;
        out.err = "lzma_stream_decoder init failed";
        free(inbuf); free(outbuf);
        return out;
    }

    strm.next_in  = nullptr;
    strm.avail_in = 0;

    constexpr size_t REC = sizeof(input_instr);
    bool input_exhausted = false;
    bool stream_end      = false;
    int  consecutive_no_output       = 0;
    constexpr int MAX_NO_OUTPUT_ATTEMPTS = 5;

    uint8_t  carry[REC];
    size_t   carry_len = 0;

    auto consume = [&](const input_instr& tmp) {
        if (phase) phase->add(tmp);
        out.records_read++;
        if (tmp.is_branch) out.branches++;
        for (uint32_t k = 0; k < NUM_INSTR_SOURCES; k++) {
            if (tmp.source_memory[k]) { out.loads++; break; }
        }
        for (uint32_t k = 0; k < NUM_INSTR_DESTINATIONS; k++) {
            if (tmp.destination_memory[k]) { out.stores++; break; }
        }
    };

    bool stop = false;
    while (!stop) {
        strm.next_out  = outbuf;
        strm.avail_out = LZMA_OUTBUF_BYTES;

        if (strm.avail_in == 0 && !input_exhausted) {
            input_file.read((char*)inbuf, LZMA_INBUF_BYTES);
            std::streamsize bytes_read = input_file.gcount();
            if (bytes_read > 0) {
                strm.next_in  = inbuf;
                strm.avail_in = (size_t)bytes_read;
                consecutive_no_output = 0;
            } else {
                input_exhausted = true;
            }
        }

        lzma_action action =
            (input_exhausted && strm.avail_in == 0) ? LZMA_FINISH : LZMA_RUN;

        size_t avail_out_before = strm.avail_out;
        r = lzma_code(&strm, action);
        size_t produced = avail_out_before - strm.avail_out;
        out.bytes_decoded += produced;

        if (produced == 0) {
            consecutive_no_output++;
            if (r == LZMA_STREAM_END) {
                stream_end = true;
                if (consecutive_no_output >= MAX_NO_OUTPUT_ATTEMPTS) break;
                continue;
            } else if (r == LZMA_OK || r == LZMA_BUF_ERROR) {
                if (input_exhausted && stream_end &&
                    consecutive_no_output >= MAX_NO_OUTPUT_ATTEMPTS) break;
                if (r == LZMA_BUF_ERROR && action == LZMA_FINISH &&
                    strm.avail_in == 0 && input_exhausted) break;
                continue;
            } else {
                out.ok = false;
                out.err = "lzma_code error " + std::to_string((int)r);
                break;
            }
        }

        size_t buf_pos = 0;

        if (carry_len > 0) {
            size_t need = REC - carry_len;
            size_t take = std::min(need, produced);
            std::memcpy(carry + carry_len, outbuf, take);
            carry_len += take;
            buf_pos   += take;
            if (carry_len == REC) {
                input_instr tmp;
                std::memcpy(&tmp, carry, REC);
                consume(tmp);
                carry_len = 0;
                if (limit_instrs && out.records_read >= limit_instrs) { stop = true; break; }
            }
        }

        while (buf_pos + REC <= produced) {
            input_instr tmp;
            std::memcpy(&tmp, outbuf + buf_pos, REC);
            buf_pos += REC;
            consume(tmp);
            if (limit_instrs && out.records_read >= limit_instrs) { stop = true; break; }
        }

        if (stop) break;

        if (buf_pos < produced) {
            size_t left = produced - buf_pos;
            std::memcpy(carry + carry_len, outbuf + buf_pos, left);
            carry_len += left;
        }

        if (r == LZMA_STREAM_END) {
            stream_end = true;
            if (consecutive_no_output >= MAX_NO_OUTPUT_ATTEMPTS) break;
        }
    }

    lzma_end(&strm);
    if (input_file.is_open()) input_file.close();
    free(inbuf);
    free(outbuf);
    return out;
}

// ----------------------------------------------------------------------------
// CLI + main
// ----------------------------------------------------------------------------
static std::string resolve_trace_path(const std::string& arg) {
    namespace fs = std::filesystem;
    if (fs::exists(arg)) return arg;
    fs::path shm = fs::path("/dev/shm/traces") / arg;
    if (fs::exists(shm)) return shm.string();
    // Substring-match against filenames in /dev/shm/traces/. Compare against
    // arg's basename (strip dir), and also strip trailing .xz/.champsimtrace
    // so "LLM256.Pythia-70M_21M.xz" matches "LLM256.Pythia-70M_21M.champsimtrace.xz".
    std::string needle = fs::path(arg).filename().string();
    auto strip_suf = [&](const std::string& suf) {
        if (needle.size() >= suf.size() &&
            needle.compare(needle.size() - suf.size(), suf.size(), suf) == 0) {
            needle.resize(needle.size() - suf.size());
        }
    };
    strip_suf(".xz");
    strip_suf(".champsimtrace");
    fs::path dir = "/dev/shm/traces";
    if (fs::exists(dir) && fs::is_directory(dir)) {
        std::vector<fs::path> hits;
        for (auto& e : fs::directory_iterator(dir)) {
            if (e.path().filename().string().find(needle) != std::string::npos) {
                hits.push_back(e.path());
            }
        }
        if (hits.size() == 1) return hits.front().string();
        if (hits.size() > 1) {
            std::cerr << "trace_decode: ambiguous trace arg '" << arg << "' (needle='"
                      << needle << "') matches " << hits.size() << " files in /dev/shm/traces/:\n";
            for (auto& h : hits) std::cerr << "  " << h.filename().string() << "\n";
        }
    }
    return arg;
}

static std::string trace_basename(const std::string& path) {
    namespace fs = std::filesystem;
    std::string name = fs::path(path).filename().string();
    auto strip = [&](const std::string& suf) {
        if (name.size() >= suf.size() &&
            name.compare(name.size() - suf.size(), suf.size(), suf) == 0) {
            name.resize(name.size() - suf.size());
        }
    };
    strip(".xz");
    strip(".champsimtrace");
    return name;
}

static void usage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " <trace_name_or_path> [--limit M_instrs]"
              << " [--micro N=500] [--fine-mult N=20] [--base-mult N=200]"
              << " [--no-phase]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    std::string trace_arg;
    uint64_t    limit_instrs = 0;
    uint64_t    micro        = 500ULL;
    uint64_t    fine_mult    = 20ULL;
    uint64_t    base_mult    = 200ULL;
    bool        no_phase     = false;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--limit" && i + 1 < argc) {
            limit_instrs = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--micro" && i + 1 < argc) {
            micro = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--fine-mult" && i + 1 < argc) {
            fine_mult = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--base-mult" && i + 1 < argc) {
            base_mult = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--no-phase") {
            no_phase = true;
        } else if (a == "-h" || a == "--help") {
            usage(argv[0]); return 0;
        } else if (trace_arg.empty()) {
            trace_arg = a;
        } else {
            usage(argv[0]); return 1;
        }
    }
    if (trace_arg.empty()) { usage(argv[0]); return 1; }
    if (micro == 0 || fine_mult == 0 || base_mult == 0) {
        std::cerr << "trace_decode: invalid --micro/--fine-mult/--base-mult (all >0)\n";
        return 1;
    }

    std::string path = resolve_trace_path(trace_arg);

    std::fprintf(stdout, "[trace_decode] trace=%s outbuf=%zuMiB limit=%llu micro=%llu fine_mult=%llu base_mult=%llu phase=%s\n",
                 path.c_str(),
                 (size_t)(LZMA_OUTBUF_BYTES >> 20),
                 (unsigned long long)limit_instrs,
                 (unsigned long long)micro,
                 (unsigned long long)fine_mult,
                 (unsigned long long)base_mult,
                 no_phase ? "off" : "on");
    std::fflush(stdout);

    namespace fs = std::filesystem;
    std::unique_ptr<PhaseAnalyzer> phase;
    std::string phase_csv_path;
    if (!no_phase) {
        std::error_code ec;
        // Anchor logs/ next to the binary, not CWD-relative.
        fs::path logs_dir;
        {
            std::error_code ec2;
            fs::path exe = fs::canonical("/proc/self/exe", ec2);
            logs_dir = ec2 ? fs::path("logs") : (exe.parent_path() / "logs");
        }
        fs::create_directories(logs_dir, ec);
        phase_csv_path = (logs_dir / (trace_basename(path) + ".phase_fine.csv")).string();
        phase = std::make_unique<PhaseAnalyzer>(micro, fine_mult, base_mult, phase_csv_path);
        if (!phase->ok()) {
            std::cerr << "[trace_decode] WARNING: cannot open " << phase_csv_path
                      << " - disabling phase analysis\n";
            phase.reset();
            phase_csv_path.clear();
        }
    }

    DecodeStats dec = run_decoder(path, limit_instrs, phase.get());

    if (phase) phase->finalize();

    if (!dec.ok) {
        std::cerr << "[trace_decode] DECODER ERROR: " << dec.err << "\n";
    }

    std::fprintf(stdout,
                 "[trace_decode] DONE records=%llu loads=%llu stores=%llu branches=%llu bytes=%llu\n",
                 (unsigned long long)dec.records_read,
                 (unsigned long long)dec.loads,
                 (unsigned long long)dec.stores,
                 (unsigned long long)dec.branches,
                 (unsigned long long)dec.bytes_decoded);
    std::fprintf(stdout, "TOTAL_INSTRUCTIONS: %llu\n", (unsigned long long)dec.records_read);
    if (phase && !phase_csv_path.empty()) {
        std::fprintf(stdout, "PHASE_FINE_CSV: %s\n", phase_csv_path.c_str());
    }
    std::fflush(stdout);

    return dec.ok ? 0 : 2;
}
