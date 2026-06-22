# champsim_refactor/src/trace_helper.cc — function bounds

## TraceHelper::run  L8-130
  block_01  L9-9     `const size_t instr_size = sizeof(input_instr);`
  block_02  L11-129  `while (running.load(std::memory_order_relaxed)) {`
  block_03  L14-18   `std::unique_lock<std::mutex> lk(wake_mtx);`
  block_04  L20-20   `if (!running.load(std::memory_order_relaxed)) break;`
  block_05  L23-24   `bool full[NUM_CPUS] = {};`
  block_06  L25-127  `while (full_count < NUM_CPUS && running.load(std::memory_order_relaxed)) {`
  block_07  L28-35   `int order[NUM_CPUS];`
  block_08  L36-38   `for (int i = 0; i < NUM_CPUS - 1; i++)`
  block_09  L40-126  `for (int idx = 0; idx < NUM_CPUS; idx++) {`
  block_10  L46-55   `uint32_t h = buf.head.load(std::memory_order_relaxed);`
  block_11  L58-71   `if (raw_buf_pos[cpu] >= raw_buf_count[cpu]) {`
  block_12  L74-75   `uint32_t raw_available = raw_buf_count[cpu] - raw_buf_pos[cpu];`
  block_13  L77-123  `for (uint32_t n = 0; n < to_produce; n++) {`
  block_14  L93-99   `for (uint32_t i = 0; i < NUM_INSTR_DESTINATIONS; i++) {`
  block_15  L101-107 `for (uint8_t i = 0; i < NUM_INSTR_SOURCES; i++) {`
  block_16  L114-118 `if (raw.is_branch) {`
  block_17  L120-122 `h++;`
  block_18  L125-125 `buf.head.store(h, std::memory_order_release);`
  flow: Worker thread loop — waits on condition variable, then fills all per-CPU ring buffers (emptiest-first sort) by reading XZ trace, decoding instructions, running branch prediction, and publishing head pointer.

## TraceHelper::start  L132-148
  block_01  L133-133 `cpus = cpu_array;`
  block_02  L134-135 `running.store(true, std::memory_order_release);`
  block_03  L136-147 `#ifdef _WIN32`
  block_04  L139-139 `worker = std::thread(&TraceHelper::run, this);`
  block_05  L141-147 `sigset_t ss, oss;`
  flow: Stores CPU array pointer, sets running/needs_work flags, then spawns the worker thread (blocking SIGINT on POSIX so it only reaches the main thread).

## TraceHelper::stop  L150-154
  block_01  L151-151 `running.store(false, std::memory_order_release);`
  block_02  L152-152 `wake_cv.notify_one();`
  block_03  L153-153 `if (worker.joinable()) worker.join();`
  flow: Signals the worker to exit by clearing running flag, notifies the condition variable, then joins the thread.
