// trace_cut.cc — stream xz trace, skip N records, emit M records as raw bytes to stdout
// Usage: trace_cut <input.xz> --skip S --limit L | xz -c > output.xz
// Records are 64-byte input_instr structs (same as champsim trace format).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <lzma.h>

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
};

static constexpr size_t REC = sizeof(input_instr);
static constexpr size_t OUTBUF = 8ULL * 1024 * 1024;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "Usage: trace_cut <input.xz> [--skip S] [--limit L] [--bufsize B]\n");
        return 1;
    }

    std::string path;
    uint64_t skip_records = 0;
    uint64_t limit_records = 0;
    size_t inbuf_size = 2048ULL * 1024;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--skip" && i + 1 < argc) {
            skip_records = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--limit" && i + 1 < argc) {
            limit_records = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--bufsize" && i + 1 < argc) {
            inbuf_size = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "-h" || a == "--help") {
            std::fprintf(stderr, "Usage: trace_cut <input.xz> [--skip S] [--limit L] [--bufsize B]\n");
            return 0;
        } else if (path.empty()) {
            path = a;
        } else {
            std::fprintf(stderr, "trace_cut: unexpected arg '%s'\n", a.c_str());
            return 1;
        }
    }
    if (path.empty()) { std::fprintf(stderr, "trace_cut: no input file\n"); return 1; }

    std::fprintf(stderr, "[trace_cut] input=%s skip=%llu limit=%llu rec_size=%zu bufsize=%zu\n",
                 path.c_str(), (unsigned long long)skip_records,
                 (unsigned long long)limit_records, REC, inbuf_size);

    std::ifstream input_file(path, std::ios::binary);
    if (!input_file.good()) {
        std::fprintf(stderr, "[trace_cut] ERROR: cannot open %s\n", path.c_str());
        return 1;
    }

    uint8_t* inbuf = nullptr;
    uint8_t* outbuf = nullptr;
    if (posix_memalign((void**)&inbuf, 64, inbuf_size) != 0 ||
        posix_memalign((void**)&outbuf, 64, OUTBUF) != 0) {
        std::fprintf(stderr, "[trace_cut] ERROR: posix_memalign failed\n");
        return 1;
    }

    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_stream_decoder(&strm, UINT64_MAX, LZMA_CONCATENATED) != LZMA_OK) {
        std::fprintf(stderr, "[trace_cut] ERROR: lzma init failed\n");
        return 1;
    }

    strm.next_in = nullptr;
    strm.avail_in = 0;

    uint8_t carry[REC];
    size_t carry_len = 0;
    uint64_t records_seen = 0;
    uint64_t records_written = 0;
    bool input_exhausted = false;
    bool stop = false;
    int consecutive_no_output = 0;

    // Write buffer for stdout batching
    static constexpr size_t WBUF_SIZE = 4ULL * 1024 * 1024;
    uint8_t* wbuf = (uint8_t*)malloc(WBUF_SIZE);
    size_t wbuf_pos = 0;

    auto flush_wbuf = [&]() {
        if (wbuf_pos > 0) {
            size_t written = fwrite(wbuf, 1, wbuf_pos, stdout);
            if (written != wbuf_pos) {
                std::fprintf(stderr, "[trace_cut] ERROR: stdout write failed\n");
                stop = true;
            }
            wbuf_pos = 0;
        }
    };

    auto emit = [&](const uint8_t* rec_ptr) {
        records_seen++;
        if (records_seen <= skip_records) return;
        if (limit_records && records_written >= limit_records) { stop = true; return; }

        if (wbuf_pos + REC > WBUF_SIZE) flush_wbuf();
        std::memcpy(wbuf + wbuf_pos, rec_ptr, REC);
        wbuf_pos += REC;
        records_written++;

        if (limit_records && records_written >= limit_records) stop = true;
    };

    while (!stop) {
        strm.next_out = outbuf;
        strm.avail_out = OUTBUF;

        if (strm.avail_in == 0 && !input_exhausted) {
            input_file.read((char*)inbuf, inbuf_size);
            std::streamsize bytes_read = input_file.gcount();
            if (bytes_read > 0) {
                strm.next_in = inbuf;
                strm.avail_in = (size_t)bytes_read;
                consecutive_no_output = 0;
            } else {
                input_exhausted = true;
            }
        }

        lzma_action action = (input_exhausted && strm.avail_in == 0) ? LZMA_FINISH : LZMA_RUN;
        size_t avail_before = strm.avail_out;
        lzma_ret r = lzma_code(&strm, action);
        size_t produced = avail_before - strm.avail_out;

        if (produced == 0) {
            consecutive_no_output++;
            if (r == LZMA_STREAM_END || consecutive_no_output >= 5) break;
            if (r == LZMA_BUF_ERROR && action == LZMA_FINISH && strm.avail_in == 0) break;
            continue;
        }

        size_t buf_pos = 0;

        if (carry_len > 0) {
            size_t need = REC - carry_len;
            size_t take = (need < produced) ? need : produced;
            std::memcpy(carry + carry_len, outbuf, take);
            carry_len += take;
            buf_pos += take;
            if (carry_len == REC) {
                emit(carry);
                carry_len = 0;
                if (stop) break;
            }
        }

        while (buf_pos + REC <= produced) {
            emit(outbuf + buf_pos);
            buf_pos += REC;
            if (stop) break;
        }
        if (stop) break;

        if (buf_pos < produced) {
            size_t left = produced - buf_pos;
            std::memcpy(carry + carry_len, outbuf + buf_pos, left);
            carry_len += left;
        }

        if (r == LZMA_STREAM_END) break;
    }

    flush_wbuf();
    fflush(stdout);
    lzma_end(&strm);
    input_file.close();
    free(inbuf);
    free(outbuf);
    free(wbuf);

    std::fprintf(stderr, "[trace_cut] DONE seen=%llu skipped=%llu written=%llu\n",
                 (unsigned long long)records_seen,
                 (unsigned long long)skip_records,
                 (unsigned long long)records_written);
    return 0;
}
