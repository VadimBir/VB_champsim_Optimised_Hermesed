#ifndef OOO_CPU_HELPER_H
#define OOO_CPU_HELPER_H
// ooo_cpu_helper.cc — behavior-preserving offload from ooo_cpu.cc (PURE code motion).
// Same-TU include from ooo_cpu.cc. Contains: file-local SANITY macro block + print_core_config.

// ==== TRUE_SANITY_CHECK macros (ooo_cpu.cc local) ====
#ifdef TRUE_SANITY_CHECK
  #define SANITY_LQ_BIT_FREE(lq_bits, cpu, w, b) \
      do { assert(!((lq_bits)[cpu][w] & (1ULL << (b)))); } while(0)

  #define SANITY_STA_TAIL_EMPTY() \
      do { if (STA[STA_tail] < UINT64_MAX) { if (STA_head != STA_tail) assert(0); } } while(0)

  #define SANITY_ROB_IP_NZ(func, index) \
      do { if (ROB.entry[index].ip == 0) { \
          cerr << "[ROB_ERROR] " << func << " ip is zero index: " << (index); \
          cerr << " instr_id: " << ROB.entry[index].instr_id << " ip: " << ROB.entry[index].ip << endl; \
          assert(0); } } while(0)

  #define SANITY_ROB_EVENTS_DUMP(i) \
      do { \
          for (int rob = 0; rob < ROB_SIZE; ++rob) { \
              uint8_t v = (BS_TST(rob_events.per_cpu[i].is_mem, rob) ? 1 : 0) \
                        | ((BS_TST(rob_events.per_cpu[i].fetched_inflight, rob) | (BS_TST(rob_events.per_cpu[i].fetched_complete, rob) << 1)) << 2) \
                        | ((BS_TST(rob_events.per_cpu[i].sched_inflight, rob) | (BS_TST(rob_events.per_cpu[i].sched_complete, rob) << 1)) << 4); \
              if (rob == ROB.head) cout << "\033[31m" << setw(2) << +v << "\033[0m"; \
              else cout << setw(2) << +v; \
          } \
          assert(0); \
      } while(0)

  #define SANITY_RTE_TAIL_INVALID(RTE, RTE_tail) \
      do { if ((RTE)[RTE_tail] < ROB_SIZE) assert(0); } while(0)

  #define SANITY_NOT_AVAILABLE_LE(not_available, num_mem_ops, rob_index) \
      do { if ((not_available) > (num_mem_ops)) { \
          cerr << "instr_id: " << ROB.entry[rob_index].instr_id << endl; assert(0); } } while(0)

  #define SANITY_LQ_FREE_SLOT(lq_index, rob_index) \
      do { if ((lq_index) == LQ.SIZE) { \
          cerr << "instr_id: " << ROB.entry[rob_index].instr_id << " no empty slot in the load queue!!!" << endl; \
          assert(0); } } while(0)

  #define SANITY_NUM_MEM_OPS_NONNEG(rob_entry_idx) \
      do { if (ROB.entry[rob_entry_idx].num_mem_ops < 0) { \
          cerr << "instr_id: " << ROB.entry[rob_entry_idx].instr_id << endl; assert(0); } } while(0)

  #define SANITY_NUM_MEM_OPS_NONNEG_PTR(rob_entry_ptr) \
      do { if ((rob_entry_ptr)->num_mem_ops < 0) { \
          cerr << "instr_id: " << (rob_entry_ptr)->instr_id << endl; assert(0); } } while(0)

  #define SANITY_SQ_ENTRY_EMPTY(sq_index) \
      do { if (SQ.entry[sq_index].virtual_address) assert(0); } while(0)

  #define SANITY_LQ_IDX_BOUND(lq_index) \
      do { if ((lq_index) >= LQ.SIZE) assert(0); } while(0)

  #define SANITY_ROB_NUM_MEM_OPS_DETAILED(rob_index, qentry) \
      do { if (ROB.entry[rob_index].num_mem_ops < 0) { \
          cerr << "#memOp: " << ROB.entry[rob_index].num_mem_ops << "load merged: " << (int)(qentry).load_merged << " store merged: " << (int)(qentry).store_merged << endl; \
          cerr << "instr_id: " << ROB.entry[rob_index].instr_id << " isByP? " << (int)(qentry).l1_bypassed << "/" << (int)(qentry).l2_bypassed << "/" << (int)(qentry).llc_bypassed << endl; \
          assert(0); } } while(0)

  #define SANITY_ROB_NUM_MEM_OPS_AT_MERGE(merged_rob_index) \
      do { if (ROB.entry[merged_rob_index].num_mem_ops < 0) { \
          cerr << "instr_id: " << ROB.entry[merged_rob_index].instr_id << " rob_index: " << (merged_rob_index) << endl; \
          assert(0); } } while(0)

  #define SANITY_CHECK_ROB_MATCH(queue, index, rob_index) \
      do { \
          DP(if (DP_GATE_WW(current_core_cycle[cpu], cpu, (queue)->entry[index].address, (queue)->entry[index].instr_id)) { \
              cout << __func__ << " instrID:" << (queue)->entry[index].instr_id << hex << " addr: " << (queue)->entry[index].address << dec << endl; \
          }) \
          if ((rob_index) != check_rob((queue)->entry[index].instr_id)) assert(0); \
      } while(0)

  #define SANITY_CHECK_ROB_RFO(queue, index, rob_index) \
      do { if ((queue)->entry[index].type != RFO) { \
          DP(if (DP_GATE_WW(current_core_cycle[cpu], cpu, (queue)->entry[index].address, (queue)->entry[index].instr_id)) { \
              cout << __func__ << " RFO instrID:" << (queue)->entry[index].instr_id << hex << " addr: " << (queue)->entry[index].address << dec << " #MemOP: " << ROB.entry[rob_index].num_mem_ops <<  endl; \
          }) \
          if ((rob_index) != check_rob((queue)->entry[index].instr_id)) assert(0); \
      } } while(0)

  #define SANITY_NOT_STORE_MERGED(qentry) \
      do { if ((qentry).store_merged) { \
          cerr << "instr: " << instr_unique_id << " addr: " << (qentry).address << " curr cy: " << current_core_cycle[cpu]; \
          assert(0); } } while(0)
#else
  #define SANITY_LQ_BIT_FREE(lq_bits, cpu, w, b)              ((void)0)
  #define SANITY_STA_TAIL_EMPTY()                             ((void)0)
  #define SANITY_ROB_IP_NZ(func, index)                       ((void)0)
  #define SANITY_ROB_EVENTS_DUMP(i)                           ((void)0)
  #define SANITY_RTE_TAIL_INVALID(RTE, RTE_tail)              ((void)0)
  #define SANITY_NOT_AVAILABLE_LE(na, nmo, rob_index)         ((void)0)
  #define SANITY_LQ_FREE_SLOT(lq_index, rob_index)            ((void)0)
  #define SANITY_NUM_MEM_OPS_NONNEG(rob_entry_idx)            ((void)0)
  #define SANITY_NUM_MEM_OPS_NONNEG_PTR(rob_entry_ptr)        ((void)0)
  #define SANITY_SQ_ENTRY_EMPTY(sq_index)                     ((void)0)
  #define SANITY_LQ_IDX_BOUND(lq_index)                       ((void)0)
  #define SANITY_ROB_NUM_MEM_OPS_DETAILED(rob_index, qentry)  ((void)0)
  #define SANITY_ROB_NUM_MEM_OPS_AT_MERGE(merged_rob_index)   ((void)0)
  #define SANITY_CHECK_ROB_MATCH(queue, index, rob_index)     ((void)0)
  #define SANITY_CHECK_ROB_RFO(queue, index, rob_index)       ((void)0)
  #define SANITY_NOT_STORE_MERGED(qentry)                     ((void)0)
#endif
// ==== END TRUE_SANITY_CHECK macros (ooo_cpu.cc local) ====

void print_core_config()
{
    cout << "fetch_width ;" << FETCH_WIDTH << "; "
         << "decode_width ;" << DECODE_WIDTH << "; "
         << "exec_width ;" << EXEC_WIDTH << "; "
         << "lq_width ;" << LQ_WIDTH << "; "
         << "sq_width ;" << SQ_WIDTH << "; "
         << "retire_width ;" << RETIRE_WIDTH << "; " << endl
         << "scheduler_size ;" << SCHEDULER_SIZE << "; " << endl
         << "branch_mispredict_penalty ;" << BRANCH_MISPREDICT_PENALTY << "; " << endl
         << "rob_size ;" << ROB_SIZE << "; "
         << "lq_size ;" << LQ_SIZE << "; "
         << "sq_size ;" << SQ_SIZE << "; " << endl
         << "num_instr_destinations_sparc ;" << NUM_INSTR_DESTINATIONS_SPARC << "; "
         << "num_instr_destinations ;" << NUM_INSTR_DESTINATIONS << "; "
         << "num_instr_sources ;" << NUM_INSTR_SOURCES << "; " << endl
         << endl;
}

#endif // OOO_CPU_HELPER_H
