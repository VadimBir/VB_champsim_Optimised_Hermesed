#ifndef DP_HERMES_H
#define DP_HERMES_H
#include "champsim.h"
#ifdef USE_HERMES
#ifdef DEBUG_PRINT
#define DP_HERMES_PRED(cpu_v, lq_idx, addr_v, dec_v) DP( do { if (DP_GATE(current_core_cycle[(cpu_v)],(cpu_v)) && warmup_complete[(cpu_v)]) { \
    cout << "cy=" << current_core_cycle[(cpu_v)] << " cpu=" << (int)(cpu_v) \
         << " lq=" << (lq_idx) << " [HERMES_OCP] PREDICT addr=" << hex << (addr_v) << dec \
         << " decision=" << dec_v << endl; } } while(0) )
#define DP_HERMES_DDRP_FIRE(cpu_v, lq_idx, instr_v, addr_v, ctype_v) DP( do { if (DP_GATE(current_core_cycle[(cpu_v)],(cpu_v)) && warmup_complete[(cpu_v)]) { \
    cout << "cy=" << current_core_cycle[(cpu_v)] << " cpu=" << (int)(cpu_v) \
         << " instr=" << (instr_v) << " [HERMES_DDRP] FIRE addr=" << hex << (addr_v) << dec \
         << " lq=" << (lq_idx) << " ctype=" << (ctype_v) << endl; } } while(0) )
#define DP_HERMES_DDRP_RET(cpu_v, instr_v, addr_v) DP( do { if (DP_GATE(current_core_cycle[(cpu_v)],(cpu_v)) && warmup_complete[(cpu_v)]) { \
    cout << "cy=" << current_core_cycle[(cpu_v)] << " cpu=" << (int)(cpu_v) \
         << " instr=" << (instr_v) << " [HERMES_DDRP] RETURN addr=" << hex << (addr_v) << dec << endl; } } while(0) )
#define DP_HERMES_OCP_OUTCOME(cpu_v, lq_idx, true_pos_v) DP( do { if (DP_GATE(current_core_cycle[(cpu_v)],(cpu_v)) && warmup_complete[(cpu_v)]) { \
    cout << "cy=" << current_core_cycle[(cpu_v)] << " cpu=" << (int)(cpu_v) \
         << " lq=" << (lq_idx) << " [HERMES_OCP] OUTCOME class=" << true_pos_v << endl; } } while(0) )
#else
#define DP_HERMES_PRED(c,lq,a,d)
#define DP_HERMES_DDRP_FIRE(c,lq,i,a,t)
#define DP_HERMES_DDRP_RET(c,i,a)
#define DP_HERMES_OCP_OUTCOME(c,lq,t)
#endif
#else // !USE_HERMES
#define DP_HERMES_PRED(c,lq,a,d)
#define DP_HERMES_DDRP_FIRE(c,lq,i,a,t)
#define DP_HERMES_DDRP_RET(c,i,a)
#define DP_HERMES_OCP_OUTCOME(c,lq,t)
#endif
#endif
