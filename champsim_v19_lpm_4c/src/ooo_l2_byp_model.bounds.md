# ooo_l2_byp_model.cc — function bounds

## l2c_bypass_initialize  L19-23
  block_00  L20-20  `if (l2_bypass_init_1100[cpu]) return;`
  block_01  L21-21  `cout << "L2 Bypass [1100-PMCHeadroom]: PMC-headroom L2: same`
  block_02  L22-22  `l2_bypass_init_1100[cpu] = true;`

## l2c_bypass_operate  L25-37
  block_00  L26-26  `l2c_bypass_initialize(cpu, L1D, L2C, LLC);`
  block_01  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_02  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_03  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_04  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_05  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_06  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_07  L28-28  `double pmc_share = lpm[cpu][LPM_L2C].wsm.camat_activeMemCyDi`
  block_08  L30-34  `#if DBG_1100`
  block_09  L36-36  `return byp;`
