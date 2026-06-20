// ==== DRAM_SANITY_CHECK macros (dram_controller.cc local) ====
#ifdef DRAM_SANITY_CHECK
  #define SANITY_DRAM_REQUEST_IDX_BOUND(queue, request_index) \
      do { if ((request_index) == (queue)->SIZE) assert(0); } while(0)
#else
  #define SANITY_DRAM_REQUEST_IDX_BOUND(queue, request_index) ((void)0)
#endif
// ==== END DRAM_SANITY_CHECK macros ====

// ==== COMPREHENSIVE DRAM DP MACROS (file-local) ====
// DRAM_FULL_DP: pkt + bank + ch + RQ/WQ headers + reason.
// DRAM_CH_DP:   no-packet variant for channel/wm/gate events.
// Gated by DP_GATE_WW (cpu, cy, addr, iid). DEBUG_CPU used for channel-only.
#ifdef DEBUG_PRINT
#define DRAM_FULL_DP(TAG, CY, CPU, CH, RANK, BANK, PKT, REASON) \
    do { if (DP_GATE_WW((CY), (CPU), (PKT).address, (PKT).instr_id)) { \
        std::cout << "[DRAM_" TAG "] cy=" << (CY) \
            << " cpu=" << (int)(CPU) << " ch=" << (int)(CH) << " r=" << (int)(RANK) << " b=" << (int)(BANK) \
            << " addr=0x" << std::hex << (PKT).address << std::dec \
            << " iid=" << (PKT).instr_id \
            << " type=" << (int)(PKT).type \
            << " fill=" << (int)(PKT).fill_level \
            << " ByP=" << (int)(PKT).l1_bypassed << (int)(PKT).l2_bypassed << (int)(PKT).llc_bypassed \
            << " sched=" << (int)(PKT).scheduled \
            << " ev_cy=" << (PKT).event_cycle \
            << " || bk: wk=" << (int)bank_hot.entries[(CH)][(RANK)][(BANK)].working \
            << " cy_av=" << bank_cycle_available[(CH)][(RANK)][(BANK)] \
            << " op_row=" << bank_request[(CH)][(RANK)][(BANK)].open_row \
            << " req_idx=" << bank_request[(CH)][(RANK)][(BANK)].request_index \
            << " is_rd=" << (int)bank_request[(CH)][(RANK)][(BANK)].is_read \
            << " is_wr=" << (int)bank_request[(CH)][(RANK)][(BANK)].is_write \
            << " || ch: wm=" << (int)write_mode[(CH)] \
            << " dbus_av=" << dbus_cycle_available[(CH)] \
            << " sr=" << scheduled_reads[(CH)] \
            << " sw=" << scheduled_writes[(CH)] \
            << " || RQ:" << RQ[(CH)].occupancy << "/" << RQ[(CH)].SIZE \
            << " nfc=" << RQ[(CH)].next_schedule_cycle << "@" << RQ[(CH)].next_schedule_index \
            << " npc=" << RQ[(CH)].next_process_cycle << "@" << RQ[(CH)].next_process_index \
            << " || WQ:" << WQ[(CH)].occupancy << "/" << WQ[(CH)].SIZE \
            << " nfc=" << WQ[(CH)].next_schedule_cycle << "@" << WQ[(CH)].next_schedule_index \
            << " npc=" << WQ[(CH)].next_process_cycle << "@" << WQ[(CH)].next_process_index \
            << " || why=" << (REASON) << std::endl; \
    } } while(0)

#define DRAM_CH_DP(TAG, CY, CH, REASON) \
    do { if (DP_GATE_WW((CY), DEBUG_CPU, 0, 0)) { \
        std::cout << "[DRAM_" TAG "] cy=" << (CY) << " ch=" << (int)(CH) \
            << " || ch: wm=" << (int)write_mode[(CH)] \
            << " dbus_av=" << dbus_cycle_available[(CH)] \
            << " sr=" << scheduled_reads[(CH)] \
            << " sw=" << scheduled_writes[(CH)] \
            << " || RQ:" << RQ[(CH)].occupancy << "/" << RQ[(CH)].SIZE \
            << " nfc=" << RQ[(CH)].next_schedule_cycle << "@" << RQ[(CH)].next_schedule_index \
            << " npc=" << RQ[(CH)].next_process_cycle << "@" << RQ[(CH)].next_process_index \
            << " || WQ:" << WQ[(CH)].occupancy << "/" << WQ[(CH)].SIZE \
            << " nfc=" << WQ[(CH)].next_schedule_cycle << "@" << WQ[(CH)].next_schedule_index \
            << " npc=" << WQ[(CH)].next_process_cycle << "@" << WQ[(CH)].next_process_index \
            << " || why=" << (REASON) << std::endl; \
    } } while(0)
#else
#define DRAM_FULL_DP(TAG, CY, CPU, CH, RANK, BANK, PKT, REASON) ((void)0)
#define DRAM_CH_DP(TAG, CY, CH, REASON) ((void)0)
#endif
// ==== END COMPREHENSIVE DRAM DP MACROS ====

void print_dram_config()
{
    cout << std::right << setw(16) << "dram_chnl_width;" << std::left << setw(5) << DRAM_CHANNEL_WIDTH << ";" <<
        std::right << setw(38) << "dram_rq_sz ;" << std::left << setw(3) << DRAM_RQ_SIZE << ";" <<
        std::right << setw(12) << "dram_wq_sz ;" << std::left << setw(5) << DRAM_WQ_SIZE << ";" << endl <<
        std::right << setw(11) << "dram_mtps ;" << std::left << setw(5) << DRAM_MTPS << ";" <<
        std::right << setw(5) << "tRP ;" << std::left << setw(5) << tRP_DRAM_NANOSECONDS << ";" <<
        std::right << setw(5) << "tRCD ;" << std::left << setw(5) << tRCD_DRAM_NANOSECONDS << ";" <<
        std::right << setw(5) << "tCAS ;" << std::left << setw(5) << tCAS_DRAM_NANOSECONDS << ";" << endl <<
        std::right << setw(16) << "dram_dbus_TAT ;" << std::left << setw(5) << DRAM_DBUS_TURN_AROUND_TIME << ";" << // TAT - TURN AROUND TIME
        std::right << setw(17) << "dram_write_hi_wm;" << std::left << setw(5) << DRAM_WRITE_HIGH_WM << ";" <<
        std::right << setw(17) << "dram_write_lo_wm;" << std::left << setw(5) << DRAM_WRITE_LOW_WM << ";" << endl <<
        std::right << setw(23) << "min_dram_write_per_sw;" << std::left << setw(5) << MIN_DRAM_WRITES_PER_SWITCH << ";" <<
        std::right << setw(17) << "dram_dbus_ret_time;" << std::left << setw(5)<< DRAM_DBUS_RETURN_TIME << endl
        << endl;
}
