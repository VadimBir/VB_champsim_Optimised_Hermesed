# champsim_refactor/src/ooo_l1_byp_model.cc — function bounds

## l1d_bypass_initialize  L19-23
  block_01  L20-20  `if (l1_bypass_init_4000fix[cpu]) return;`
  block_02  L21-22  `cout << "[model: 4000fix-KappaPhiL1L2.l1_bypass] L1 Bypass ...`

## l1d_bypass_operate  L25-39
  block_01  L26-26  `l1d_bypass_initialize(cpu, L1D, L2C, LLC);`
  block_02  L28-31  `double k_s = get_kappa_short(cpu, LPM_L1D);`
  block_03  L33-34  `bool kp = ((k_s > k_l) && (p_s < p_l));`
  block_04  L35-38  `uint64_t addr = L1D->RQ.entry[L1D->RQ.head].address;`

## l1d_bypass_fill  L41-44
  block_01  L42-43  `uint64_t blk = pkt.address >> LOG2_BLOCK_SIZE;`
