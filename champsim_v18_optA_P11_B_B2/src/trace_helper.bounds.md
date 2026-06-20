# trace_helper.cc — function bounds

## run  L7-109
  block_00  L8-8  `const size_t instr_size = sizeof(input_instr);`
  block_01  L10-108  `while (running.load(std::memory_order_relaxed)) {`

## start  L111-116
  block_00  L112-112  `cpus = cpu_array;`
  block_01  L113-113  `running.store(true, std::memory_order_release);`
  block_02  L114-114  `needs_work.store(true, std::memory_order_release);`
  block_03  L115-115  `worker = std::thread(&TraceHelper::run, this);`

## stop  L118-122
  block_00  L119-119  `running.store(false, std::memory_order_release);`
  block_01  L120-120  `wake_cv.notify_one();`
  block_02  L121-121  `if (worker.joinable()) worker.join();`
