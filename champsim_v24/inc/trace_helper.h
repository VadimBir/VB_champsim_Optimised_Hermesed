#ifndef TRACE_HELPER_H
#define TRACE_HELPER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "instruction.h"
#include "champsim.h"
#include "defs.h"

#define TRACE_BUFFER_SIZE ROB_SIZE/NUM_CPUS*3001

// Each buffer entry: fully prepared ooo_model_instr + branch prediction result
struct alignas(64) TraceBufEntry {
    ooo_model_instr instr;
    uint8_t  branch_prediction;      // predict_branch() result
    uint8_t  branch_mispredicted;    // (branch_taken != branch_prediction)
};

// Lock-free SPSC circular buffer, one per core
// head = producer (helper advances after writing)
// tail = consumer (main thread advances after consuming)
// tail == head means empty (nothing ready)
struct alignas(64) TraceBuffer {
    TraceBufEntry entries[TRACE_BUFFER_SIZE];

    alignas(64) std::atomic<uint32_t> head{0};  // producer (helper)
    alignas(64) std::atomic<uint32_t> tail{0};  // consumer (main thread)
};

// Forward declare — defined in ooo_cpu.h
class O3_CPU;
class XZReader;

class TraceHelper {
public:
    TraceBuffer buffers[NUM_CPUS]; 

    // Stats
    uint64_t io_trace_idle[NUM_CPUS] = {};
    uint64_t total_produced[NUM_CPUS] = {};

    // Wake helper when buffer drops to <=1/3 full.
    // Main sets needs_work true and notifies; helper clears it when it wakes.
    std::atomic<bool> needs_work{true};
    std::mutex        wake_mtx;
    std::condition_variable wake_cv;

    void start(O3_CPU* cpu_array);
    void stop();

    // Called by main thread after advancing tail — wakes helper if buffer low.
    // Inline, branchless on the hot path (exchange only notifies on 0->1 edge).
    inline void maybe_wake(int cpu_idx) {
        uint32_t h = buffers[cpu_idx].head.load(std::memory_order_relaxed);
        uint32_t t = buffers[cpu_idx].tail.load(std::memory_order_relaxed);
        uint32_t used = (h >= t) ? (h - t) : (TRACE_BUFFER_SIZE - t + h);
        if (used <= TRACE_BUFFER_SIZE / (2 * NUM_CPUS)) {
            // Only notify on 0->1 transition to avoid redundant lock+notify
            if (!needs_work.exchange(true, std::memory_order_relaxed)) {
                wake_cv.notify_one();
            }
        }
    }

private:
    O3_CPU* cpus = nullptr;

    // Per-core input_instr bulk-read buffer (thread-local to helper)
    input_instr raw_buf[NUM_CPUS][TRACE_BUFFER_SIZE];
    uint32_t raw_buf_pos[NUM_CPUS] = {};
    uint32_t raw_buf_count[NUM_CPUS] = {};

    std::atomic<bool> running{false};
    std::thread worker;

    void run();
};

extern TraceHelper trace_helper;

#endif
