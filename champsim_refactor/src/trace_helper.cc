#include "trace_helper.h"
#include "ooo_cpu.h"
#include <iostream>
#include <signal.h>

TraceHelper trace_helper;

void TraceHelper::run() {
    const size_t instr_size = sizeof(input_instr);

    while (running.load(std::memory_order_relaxed)) {

        // Sleep until main signals there is work to do
        {
            std::unique_lock<std::mutex> lk(wake_mtx);
            wake_cv.wait(lk, [this]{ return needs_work.load(std::memory_order_relaxed) || !running.load(std::memory_order_relaxed); });
            needs_work.store(false, std::memory_order_relaxed);
        }

        if (!running.load(std::memory_order_relaxed)) break;

        // Fill all core buffers until all are full — emptiest-first
        bool full[NUM_CPUS] = {};
        int  full_count = 0;
        while (full_count < NUM_CPUS && running.load(std::memory_order_relaxed)) {

            // Sort cores by occupancy (emptiest first)
            int order[NUM_CPUS];
            uint32_t occ[NUM_CPUS];
            for (int i = 0; i < NUM_CPUS; i++) {
                order[i] = i;
                uint32_t h = buffers[i].head.load(std::memory_order_relaxed);
                uint32_t t = buffers[i].tail.load(std::memory_order_relaxed);
                occ[i] = (h >= t) ? (h - t) : (TRACE_BUFFER_SIZE - t + h);
            }
            for (int i = 0; i < NUM_CPUS - 1; i++)
                for (int j = i + 1; j < NUM_CPUS; j++)
                    if (occ[order[i]] > occ[order[j]]) std::swap(order[i], order[j]);

            for (int idx = 0; idx < NUM_CPUS; idx++) {
                int cpu = order[idx];
                if (full[cpu]) continue;

                TraceBuffer& buf = buffers[cpu];

                uint32_t h = buf.head.load(std::memory_order_relaxed);
                uint32_t t = buf.tail.load(std::memory_order_acquire);

                // Compute free slots in ring
                uint32_t free_slots = (t > h) ? (t - h - 1) : (TRACE_BUFFER_SIZE - 1 - h + t);
                if (free_slots == 0) {
                    full[cpu] = true;
                    full_count++;
                    continue;
                }

                // Stage 1: refill raw_buf if exhausted
                if (raw_buf_pos[cpu] >= raw_buf_count[cpu]) {
                    XZReader& reader = cpus[cpu].xz_reader;
                    if (reader.eof()) {
                        if (!reader.reopen()) {
                            std::cerr << "*** TRACE HELPER: CANNOT REOPEN XZ TRACE FOR CPU: " << cpu << " ***" << std::endl;
                            continue;
                        }
                        std::cout << "*** Trace Helper: Restarted XZ trace for Core: " << cpu << " ***" << std::endl;
                    }
                    raw_buf_count[cpu] = reader.read(
                        raw_buf[cpu], instr_size, TRACE_BUFFER_SIZE);
                    raw_buf_pos[cpu] = 0;
                    if (raw_buf_count[cpu] == 0) continue;
                }

                // Stage 2: bulk-fill as many entries as possible
                uint32_t raw_available = raw_buf_count[cpu] - raw_buf_pos[cpu];
                uint32_t to_produce = std::min(free_slots, raw_available);

                for (uint32_t n = 0; n < to_produce; n++) {
                    input_instr& raw = raw_buf[cpu][raw_buf_pos[cpu]];
                    raw_buf_pos[cpu]++;

                    TraceBufEntry& dst = buf.entries[h];
                    ooo_model_instr& arch_instr = dst.instr;

                    arch_instr.quickReset();

                    int num_reg_ops = 0, num_mem_ops = 0;
                    arch_instr.ip = raw.ip;
                    arch_instr.is_branch = raw.is_branch;
                    arch_instr.branch_taken = raw.branch_taken;
                    arch_instr.asid[0] = cpu;
                    arch_instr.asid[1] = cpu;

                    for (uint32_t i = 0; i < NUM_INSTR_DESTINATIONS; i++) {
                        arch_instr.destination_registers[i] = raw.destination_registers[i];
                        arch_instr.destination_memory[i] = raw.destination_memory[i];

                        if (raw.destination_registers[i]) num_reg_ops++;
                        if (raw.destination_memory[i])    num_mem_ops++;
                    }

                    for (uint8_t i = 0; i < NUM_INSTR_SOURCES; i++) {
                        arch_instr.source_registers[i] = raw.source_registers[i];
                        arch_instr.source_memory[i] = raw.source_memory[i];

                        if (raw.source_registers[i]) num_reg_ops++;
                        if (raw.source_memory[i])    num_mem_ops++;
                    }

                    arch_instr.num_reg_ops = num_reg_ops;
                    arch_instr.num_mem_ops = num_mem_ops;

                    dst.branch_prediction = 0;
                    dst.branch_mispredicted = 0;
                    if (raw.is_branch) {
                        dst.branch_prediction = cpus[cpu].predict_branch(raw.ip);
                        cpus[cpu].last_branch_result(raw.ip, raw.branch_taken);
                        dst.branch_mispredicted = (raw.branch_taken != dst.branch_prediction) ? 1 : 0;
                    }

                    h++;
                    if (h >= TRACE_BUFFER_SIZE) h = 0;
                    total_produced[cpu]++;
                }

                buf.head.store(h, std::memory_order_release);
            } // end for cpu
        } // end while not all full
        // All buffers full — loop back to wait
    }
}

void TraceHelper::start(O3_CPU* cpu_array) {
    cpus = cpu_array;
    running.store(true, std::memory_order_release);
    needs_work.store(true, std::memory_order_release);
#ifdef _WIN32
    // No per-thread signal mask on Windows; the process-wide SIGINT handler
    // (installed in main) still fires. Just spawn the helper thread.
    worker = std::thread(&TraceHelper::run, this);
#else
    sigset_t ss, oss;
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    pthread_sigmask(SIG_BLOCK, &ss, &oss);   // block SIGINT so it lands on main thread
    worker = std::thread(&TraceHelper::run, this);
    pthread_sigmask(SIG_SETMASK, &oss, nullptr);
#endif
}

void TraceHelper::stop() {
    running.store(false, std::memory_order_release);
    wake_cv.notify_one();
    if (worker.joinable()) worker.join();
}
