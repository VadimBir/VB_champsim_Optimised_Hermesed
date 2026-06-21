# champsim_refactor/src/ooo_llc_byp_model.cc — function bounds

## llc_bypass_initialize  L19-23
  block_01  L20-20  `if (llc_bypass_init_4000fix[cpu]) return;`
  block_02  L21-22  `cout << "[model: 4000fix-KappaPhiL1L2.llc_bypass] LLC Bypass [4000fix-KappaPhiL1L2]...`

## llc_bypass_operate  L25-32
  block_01  L26-26  `llc_bypass_initialize(cpu, L1D, L2C, LLC);`
  block_02  L28-31  `uint64_t addr = LLC->RQ.entry[LLC->RQ.head].address;`

## llc_bypass_fill  L34-37
  block_01  L35-36  `uint64_t blk = pkt.address >> LOG2_BLOCK_SIZE;`
