#include "cache.h"
#ifdef USE_HERMES
#pragma message("HERMES IS ENABLED")
#include "ooo_cpu.h"
namespace knob { extern bool offchip_pred_mark_merged_load; }
#endif
#include "set.h"
#include "cycle_pack.h" 
#include "ooo_l1_byp_model.cc"
#ifdef BYPASS_L2_LOGIC
#include "ooo_l2_byp_model.cc"
#endif
#ifdef BYPASS_LLC_LOGIC
#include "ooo_llc_byp_model.cc"    
#endif

uint64_t l2pf_access = 0;
bool FORCE_ALL_HITS = false;

#include "lpm_tracker.h"
LPM_Tracker lpm[NUM_CPUS][LPM_NUM_TYPES];   /* definition */
ByPLatTracker g_l1_byplat[NUM_CPUS];
ByPLatTracker g_l2_byplat[NUM_CPUS];
ByPLatTracker g_llc_byplat[NUM_CPUS];
#ifdef LPM_CROSS_LEVEL_MLP
uint32_t g_crossmlp_load_peak[NUM_CPUS] = {};
uint32_t g_crossmlp_all_peak[NUM_CPUS] = {};
#endif

// ==== TRUE_SANITY_CHECK macros (cache.cc local) ====
#ifdef TRUE_SANITY_CHECK
  #define SANITY_MSHR_IDX_BOUND(idx, sz) \
      do { if ((idx) >= (sz)) assert(0); } while(0)

  #define SANITY_FILL_TLB_DATA(ct, pkt) \
      do { \
          if (((ct) == IS_ITLB || (ct) == IS_DTLB || (ct) == IS_STLB) && (pkt)->data == 0) assert(0); \
      } while(0)

  #define SANITY_SET_BOUND_PKT(func, pkt, set) \
      do { if (NUM_SET < (set)) { \
          cerr << "[" << NAME << "_ERROR] " << func << " invalid set idx: " << (set) << " NUM_SET: " << NUM_SET; \
          cerr << " addr: " << hex << (pkt)->address << " full_addr: " << (pkt)->full_addr << dec; \
          cerr << " event: " << (pkt)->event_cycle << endl; assert(0); } } while(0)

  #define SANITY_SET_BOUND_INV(func, inval_addr, set) \
      do { if (NUM_SET < (set)) { \
          cerr << "[" << NAME << "_ERROR] " << func << " invalid set idx: " << (set) << " NUM_SET: " << NUM_SET; \
          cerr << " inval_addr: " << hex << (inval_addr) << dec << endl; assert(0); } } while(0)

  #define SANITY_NO_LOWER_FOR_TLB_L1I(ct) \
      do { if ((ct) == IS_ITLB || (ct) == IS_DTLB || (ct) == IS_L1I) assert(0); } while(0)

  #define SANITY_QENTRY_EMPTY(func, Q, idx) \
      do { if ((Q).entry[idx].address != 0) { \
          cerr << "[" << NAME << "_ERROR] " << func << " is not empty idx: " << (idx); \
          cerr << " addr: " << hex << (Q).entry[idx].address; \
          cerr << " full_addr: " << (Q).entry[idx].full_addr << dec << endl; assert(0); } } while(0)

  #define SANITY_PKT_ADDR_NZ(pkt) \
      do { if ((pkt)->address == 0) assert(0); } while(0)

  #define SANITY_WQ_OCCUPANCY() \
      do { if (WQ.occupancy >= WQ.SIZE) assert(0); } while(0)

  #define SANITY_WQENTRY_EMPTY(func, idx) \
      do { if (WQ.entry[idx].address != 0 || WQ.entry[idx].l1_bypassed == 1) { \
          cerr << "[" << NAME << "_ERROR] " << func << " is not empty idx: " << (idx); \
          cerr << " addr: " << hex << WQ.entry[idx].address; \
          cerr << " full_addr: " << WQ.entry[idx].full_addr << dec << endl; assert(0); } } while(0)

  #define SANITY_PKT_ADDR_NZ_OR_FAH(pkt) \
      do { if ((pkt)->address == 0 && !FORCE_ALL_HITS) assert(0); } while(0)

  #define SANITY_MSHR_FOUND(func, pkt, mshr_index) \
      do { if ((mshr_index) == -1) { \
          cerr << "[" << NAME << "_MSHR] " << func << " instr_id: " << (pkt)->instr_id << " cannot find a matching entry!"; \
          cerr << " full_addr: " << hex << (pkt)->full_addr; \
          cerr << " addr: " << (pkt)->address << dec; \
          cerr << " event: " << (pkt)->event_cycle << " curr: " << current_core_cycle[(pkt)->cpu] << endl; assert(0); } } while(0)

  #define SANITY_MSHR_EMPTY_FOUND(func, pkt, index, sz) \
      do { if ((index) == (sz)) { \
          cerr << "[" << NAME << "_MSHR] " << func << " cannot find an empty entry!"; \
          cerr << " instr_id: " << (pkt)->instr_id << " addr: " << hex << (pkt)->address; \
          cerr << " full_addr: " << (pkt)->full_addr << dec << endl; assert(0); } } while(0)
#else
  #define SANITY_MSHR_IDX_BOUND(idx, sz)               ((void)0)
  #define SANITY_FILL_TLB_DATA(ct, pkt)                ((void)0)
  #define SANITY_SET_BOUND_PKT(func, pkt, set)         ((void)0)
  #define SANITY_SET_BOUND_INV(func, inval_addr, set)  ((void)0)
  #define SANITY_NO_LOWER_FOR_TLB_L1I(ct)              ((void)0)
  #define SANITY_QENTRY_EMPTY(func, Q, idx)            ((void)0)
  #define SANITY_PKT_ADDR_NZ(pkt)                      ((void)0)
  #define SANITY_WQ_OCCUPANCY()                        ((void)0)
  #define SANITY_WQENTRY_EMPTY(func, idx)              ((void)0)
  #define SANITY_PKT_ADDR_NZ_OR_FAH(pkt)               ((void)0)
  #define SANITY_MSHR_FOUND(func, pkt, mshr_index)     ((void)0)
  #define SANITY_MSHR_EMPTY_FOUND(func, pkt, index, sz)((void)0)
#endif
// ==== END TRUE_SANITY_CHECK macros (cache.cc local) ====

void CACHE::set_force_all_hits(bool toEnable) {
    FORCE_ALL_HITS = toEnable;
}

#define CACHE_LVL_BASE IS_L1D
#define get_cache_lvl_bit(curr_cache_type)   (1u << ((curr_cache_type) - CACHE_LVL_BASE))
#define set_this_lvl_existance(packet_exist_lvls, cache_type) ((packet_exist_lvls) |= get_cache_lvl_bit(cache_type))
#define check_upper_has_entry(packet_exist_lvls, cache_type) (((packet_exist_lvls) & ((1u << ((cache_type) - CACHE_LVL_BASE)) - 1)) != 0)



void CACHE::dump_req(PACKET& o)
{
    cout << std::hex << " Addr: "  << o.address;
    //<< " FAddr: " << o.full_addr
    cout << std::dec
    << " instr " << o.instr_id << " fill " << (int)o.fill_level << " type " << int(o.type)
    << " ROB " << int(o.rob_index);    
    cout << " {";
    for (auto rob_set : o.rob_index_depend_on_me) {
        if (warmup_complete[o.cpu]) {   
            if (rob_set == 0) continue;
            cout << rob_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "LQ " << int(o.lq_index);
    cout << " {";
    ITERATE_SET(lq_set, o.lq_index_depend_on_me, LQ_SIZE) {
        if (warmup_complete[o.cpu]) {
            if (lq_set == 0) continue;
            cout << lq_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "SQ " << int(o.sq_index);
    cout << " {";
    ITERATE_SET(sq_set, o.sq_index_depend_on_me, LQ_SIZE) {
        if (warmup_complete[o.cpu]) {
            if (sq_set == 0) continue;
            cout << sq_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "ByP " << int(o.l1_bypassed) << " " << int(o.l2_bypassed) << int(o.llc_bypassed);
}
void CACHE::dump_req(PACKET* o)
{
    cout << std::hex << " Addr " << o->address;
    //<< " FAddr " << o->full_addr
    cout << std::dec
    << " instr " << o->instr_id << " type " << int(o->type) << " fill " << (int)o->fill_level
    // << " rob " << int(o->rob_index) << " LQ " << int(o->lq_index) << " SQ " << int(o->sq_index);
    << " ROB " << int(o->rob_index);
    cout << " {";
    for (auto rob_set : o->rob_index_depend_on_me) {
        if (warmup_complete[o->cpu]) {
            if (rob_set == 0) continue;
            cout << rob_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "LQ " << int(o->lq_index);
    cout << " {";
    ITERATE_SET(lq_set, o->lq_index_depend_on_me, LQ_SIZE) {
        if (warmup_complete[o->cpu]) {
            if (lq_set == 0) continue;
            cout << lq_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "SQ " << int(o->sq_index);
    cout << " {";
    ITERATE_SET(sq_set, o->sq_index_depend_on_me, LQ_SIZE) {
        if (warmup_complete[o->cpu]) {
            if (sq_set == 0) continue;
            cout << sq_set << " " << dec;
        }
    }
    cout << "} ";
    cout << "ByP " << int(o->l1_bypassed) << " " << int(o->l2_bypassed) << " " << int(o->llc_bypassed);
}

// ============================================================
// dump_req_min_* — compact one-line dumps per call-site context.
// No trailing newline (caller macro appends endl).
// Dict: instr type fill ret cy ByP MSHR RQ WQ PQ occu sz Addr FAddr
// Aligned columns via setw. LQ/ROB/SQ sets printed as {a b c}.
// ============================================================
#define _DRMIN_SET_ROB(o) \
    cout << " ROB=" << setw(3) << int((o).rob_index) << " {"; \
    for (auto _rs : (o).rob_index_depend_on_me) { if (_rs == 0) continue; cout << _rs << " "; } \
    cout << "}"
#define _DRMIN_SET_LQ(o) \
    cout << " LQ=" << setw(3) << int((o).lq_index) << " {"; \
    ITERATE_SET(_ls, (o).lq_index_depend_on_me, LQ_SIZE) { if (_ls == 0) continue; cout << _ls << " "; } \
    cout << "}"
#define _DRMIN_SET_SQ(o) \
    cout << " SQ=" << setw(3) << int((o).sq_index) << " {"; \
    ITERATE_SET(_ss, (o).sq_index_depend_on_me, LQ_SIZE) { if (_ss == 0) continue; cout << _ss << " "; } \
    cout << "}"
#define _DRMIN_CORE(o) \
    cout << "Addr=0x"   << setw(11) << setfill('0') << hex << (o).address \
         << " FAddr=0x" << setw(14) << setfill('0') << (o).full_addr << dec << setfill(' ') \
         << " instr="   << setw(7)  << (o).instr_id \
         << " type="    << setw(1)  << int((o).type) \
         << " fill="    << setw(2)  << (int)(o).fill_level \
         << " ret="     << setw(1)  << int((o).returned) \
         << " cy="      << setw(6)  << (o).event_cycle \
         << " ByP="     << int((o).l1_bypassed) << int((o).l2_bypassed) << int((o).llc_bypassed)
#define _DRMIN_TAIL() \
    cout << " | MSHR=" << setw(3) << MSHR.occupancy << "/" << setw(3) << MSHR_SIZE \
         << " RQ="    << setw(3) << RQ.occupancy  << "/" << setw(3) << RQ.SIZE \
         << " WQ="    << setw(3) << WQ.occupancy  << "/" << setw(3) << WQ.SIZE \
         << " PQ="    << setw(3) << PQ.occupancy  << "/" << setw(3) << PQ.SIZE

void CACHE::dump_req_min_read(PACKET& o)  { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  _DRMIN_TAIL(); }
void CACHE::dump_req_min_read(PACKET* o)  { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL(); }
void CACHE::dump_req_min_write(PACKET& o) { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  _DRMIN_TAIL(); }
void CACHE::dump_req_min_write(PACKET* o) { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL(); }
void CACHE::dump_req_min_mshr(PACKET& o)  { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  _DRMIN_TAIL(); }
void CACHE::dump_req_min_mshr(PACKET* o)  { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL(); }
void CACHE::dump_req_min_ret(PACKET& o)   { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  _DRMIN_TAIL(); }
void CACHE::dump_req_min_ret(PACKET* o)   { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL(); }
void CACHE::dump_req_min_pq(PACKET& o)    { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  cout << " pfmd=" << o.pf_metadata;  _DRMIN_TAIL(); }
void CACHE::dump_req_min_pq(PACKET* o)    { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); cout << " pfmd=" << o->pf_metadata; _DRMIN_TAIL(); }
void CACHE::dump_req_min_fill(PACKET& o)  { _DRMIN_CORE(o);  _DRMIN_SET_ROB(o);  _DRMIN_SET_LQ(o);  _DRMIN_SET_SQ(o);  _DRMIN_TAIL(); }
void CACHE::dump_req_min_fill(PACKET* o)  { _DRMIN_CORE(*o); _DRMIN_SET_ROB(*o); _DRMIN_SET_LQ(*o); _DRMIN_SET_SQ(*o); _DRMIN_TAIL(); }





#ifdef BYPASS_DEBUG
std::vector<CACHE*> ALL_CACHES;
#endif

void print_cache_config() {
    cout << std::right << setw(3) <<"itlb_sz ;"<<std::left << setw(4) << (ITLB_SET*ITLB_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"itlb_set ;"<<std::left << setw(5) << ITLB_SET << ";"
        << std::right << setw(12) <<"itlb_way ;"<<std::left << setw(3) << ITLB_WAY << ";"
        << std::right << setw(12) <<"itlb_rq_sz ;"<<std::left << setw(3) << ITLB_RQ_SIZE << ";"
        << std::right << setw(12) <<"itlb_wq_sz ;"<<std::left << setw(3) << ITLB_WQ_SIZE << ";"
        << std::right << setw(12) <<"itlb_pq_sz ;"<<std::left << setw(3) << ITLB_PQ_SIZE << ";"
        << std::right << setw(12) <<"itlb_mshr_sz ;"<<std::left << setw(5) << ITLB_MSHR_SIZE << ";"
        << std::right << setw(12) <<"itlb_latency ;"<<std::left << setw(5) << ITLB_LATENCY
        << endl
        << std::right << setw(3) <<"dtlb_sz ;"<<std::left << setw(4) << (DTLB_SET*DTLB_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"dtlb_set ;"<<std::left << setw(5) << DTLB_SET << ";"
        << std::right << setw(12) <<"dtlb_way ;"<<std::left << setw(3) << DTLB_WAY << ";"
        << std::right << setw(12) <<"dtlb_rq_sz ;"<<std::left << setw(3) << DTLB_RQ_SIZE << ";"
        << std::right << setw(12) <<"dtlb_wq_sz ;"<<std::left << setw(3) << DTLB_WQ_SIZE << ";"
        << std::right << setw(12) <<"dtlb_pq_sz ;"<<std::left << setw(3) << DTLB_PQ_SIZE << ";"
        << std::right << setw(12) <<"dtlb_mshr_sz ;"<<std::left << setw(5) << DTLB_MSHR_SIZE << ";"
        << std::right << setw(12) <<"dtlb_latency ;"<<std::left << setw(5) << DTLB_LATENCY
        << endl
        << std::right << setw(3) <<"stlb_sz ;"<<std::left << setw(4) << (STLB_SET*STLB_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"stlb_set ;"<<std::left << setw(5) << STLB_SET << ";"
        << std::right << setw(12) <<"stlb_way ;"<<std::left << setw(3) << STLB_WAY << ";"
        << std::right << setw(12) <<"stlb_rq_sz ;"<<std::left << setw(3) << STLB_RQ_SIZE << ";"
        << std::right << setw(12) <<"stlb_wq_sz ;"<<std::left << setw(3) << STLB_WQ_SIZE << ";"
        << std::right << setw(12) <<"stlb_pq_sz ;"<<std::left << setw(3) << STLB_PQ_SIZE << ";"
        << std::right << setw(12) <<"stlb_mshr_sz ;"<<std::left << setw(5) << STLB_MSHR_SIZE << ";"
        << std::right << setw(12) <<"stlb_latency ;"<<std::left << setw(5) << STLB_LATENCY
        << endl
        << std::right << setw(5) <<"l1i_sz ;"<<std::left << setw(5) << (L1I_SET*L1I_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"l1i_set ;"<<std::left << setw(5) << L1I_SET << ";"
        << std::right << setw(12) <<"l1i_way ;"<<std::left << setw(3) << L1I_WAY << ";"
        << std::right << setw(12) <<"l1i_rq_sz ;"<<std::left << setw(3) << L1I_RQ_SIZE << ";"
        << std::right << setw(12) <<"l1i_wq_sz ;"<<std::left << setw(3) << L1I_WQ_SIZE << ";"
        << std::right << setw(12) <<"l1i_pq_sz ;"<<std::left << setw(3) << L1I_PQ_SIZE << ";"
        << std::right << setw(12) <<"l1i_mshr_sz ;"<<std::left << setw(5) << L1I_MSHR_SIZE << ";"
        << std::right << setw(12) <<"l1i_latency ;"<<std::left << setw(5) << L1I_LATENCY
        << endl
        << std::right << setw(5) <<"l1d_sz ;"<<std::left << setw(5) << (L1D_SET*L1D_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"l1d_set ;"<<std::left << setw(5) << L1D_SET << ";"
        << std::right << setw(12) <<"l1d_way ;"<<std::left << setw(3) << L1D_WAY << ";"
        << std::right << setw(12) <<"l1d_rq_sz ;"<<std::left << setw(3) << L1D_RQ_SIZE << ";"
        << std::right << setw(12) <<"l1d_wq_sz ;"<<std::left << setw(3) << L1D_WQ_SIZE << ";"
        << std::right << setw(12) <<"l1d_pq_sz ;"<<std::left << setw(3) << L1D_PQ_SIZE << ";"
        << std::right << setw(12) <<"l1d_mshr_sz ;"<<std::left << setw(5) << L1D_MSHR_SIZE << ";"
        << std::right << setw(12) <<"l1d_latency ;"<<std::left << setw(5) << L1D_LATENCY
        << endl
        << std::right << setw(5) <<"l2c_sz ;"<<std::left << setw(5) << (L2C_SET*L2C_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"l2c_set ;"<<std::left << setw(5) << L2C_SET << ";"
        << std::right << setw(12) <<"l2c_way ;"<<std::left << setw(3) << L2C_WAY << ";"
        << std::right << setw(12) <<"l2c_rq_sz ;"<<std::left << setw(3) << L2C_RQ_SIZE << ";"
        << std::right << setw(12) <<"l2c_wq_sz ;"<<std::left << setw(3) << L2C_WQ_SIZE << ";"
        << std::right << setw(12) <<"l2c_pq_sz ;"<<std::left << setw(3) << L2C_PQ_SIZE << ";"
        << std::right << setw(12) <<"l2c_mshr_sz ;"<<std::left << setw(5) << L2C_MSHR_SIZE << ";"
        << std::right << setw(12) <<"l2c_latency ;"<<std::left << setw(5) << L2C_LATENCY
        << endl
        << std::right << setw(5) <<"llc_sz ;"<<std::left << setw(5) << (LLC_SET*LLC_WAY*BLOCK_SIZE)/1024 << ";"
        << std::right << setw(12) <<"llc_set ;"<<std::left << setw(5) << LLC_SET << ";"
        << std::right << setw(12) <<"llc_way ;"<<std::left << setw(3) << LLC_WAY << ";"
        << std::right << setw(12) <<"llc_rq_sz ;"<<std::left << setw(3) << LLC_RQ_SIZE << ";"
        << std::right << setw(12) <<"llc_wq_sz ;"<<std::left << setw(3) << LLC_WQ_SIZE << ";"
        << std::right << setw(12) <<"llc_pq_sz ;"<<std::left << setw(3) << LLC_PQ_SIZE << ";"
        << std::right << setw(12) <<"llc_mshr_sz ;"<<std::left << setw(5) << LLC_MSHR_SIZE << ";"
        << std::right << setw(12) <<"llc_latency ;"<<std::left << setw(5) << LLC_LATENCY
        << endl;
}
// pending vector requests
#include <vector>
std::vector<PACKET> pending_requests;
void CACHE::handle_fill() {

    // Tier B hoist: snapshot hot scalars (kills 2nd-cacheline reloads)
    const uint16_t mshr_nfi_ = MSHR.next_fill_index;
    const uint64_t mshr_nfc_ = MSHR.next_fill_cycle;

    // handle fill
    uint16_t fill_cpu = (mshr_nfi_ == MSHR_SIZE) ? NUM_CPUS : MSHR.entry[mshr_nfi_].cpu;
    if (fill_cpu == NUM_CPUS)
        return;

    if (mshr_nfc_ != CYC_PACKED_MAX) PCYCLE_SANITY(mshr_nfc_, current_core_cycle[fill_cpu], std::string(NAME) + "_handle_fill");
    if ((mshr_nfc_ != CYC_PACKED_MAX) && PCYCLE_LE(mshr_nfc_, PACK_CYCLE(current_core_cycle[fill_cpu]))) {

        SANITY_MSHR_IDX_BOUND(mshr_nfi_, MSHR.SIZE);

        uint16_t mshr_index = mshr_nfi_;

        DP( if (DP_GATE_WW(current_core_cycle[fill_cpu], fill_cpu, MSHR.entry[mshr_index].address, MSHR.entry[mshr_index].instr_id) && cache_type == IS_LLC) {
            cout << "[LLC_handle_fill_ENTRY] mshr_idx: " << mshr_index
                 << " addr: 0x" << hex << MSHR.entry[mshr_index].address << dec
                 << " full_addr: 0x" << hex << MSHR.entry[mshr_index].full_addr << dec
                 << " type: " << (int)MSHR.entry[mshr_index].type
                 << " fill_level: " << (int)MSHR.entry[mshr_index].fill_level
                 << " returned: " << (int)MSHR.entry[mshr_index].returned
                 << " event_cycle: " << MSHR.entry[mshr_index].event_cycle
                 << " cpu: " << (int)MSHR.entry[mshr_index].cpu
                 << " cy: " << current_core_cycle[fill_cpu] << endl;
        });

        DP_FILL_M(("PICK idx=" + std::to_string(mshr_index) + " occu=" + std::to_string(MSHR.occupancy) + " nret=" + std::to_string(MSHR.num_returned) + " type=" + std::to_string(MSHR.entry[mshr_index].type) + " fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &MSHR.entry[mshr_index]);

        // find victim
        uint32_t set = get_set(MSHR.entry[mshr_index].address);
        uint32_t way = handle_fill_find_victim(fill_cpu, mshr_index, set);


#ifdef LLC_BYPASS
        if ((cache_type == IS_LLC) && (way == LLC_WAY)) { // this is a bypass that does not fill the LLC
            DP_FILL_M(("LLC_BYP_FILL_PATH idx=" + std::to_string(mshr_index) + " set=" + std::to_string(set)).c_str(), &MSHR.entry[mshr_index]);

            // update replacement policy
            if (cache_type == IS_LLC) {
            if (way < LLC_WAY) block[set][way].pmc++;
                llc_update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0);

            }
            else
                update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, 0, MSHR.entry[mshr_index].type, 0);

            // COLLECT STATS
            handle_fill_stats(fill_cpu, mshr_index);

            // check fill level
            DP_FILL_M(("STAGE_FILL_RETURN_LLCBYP idx=" + std::to_string(mshr_index) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &MSHR.entry[mshr_index]);
            handle_fill_return(mshr_index);

	    if(warmup_complete[fill_cpu])
	      {
		uint64_t current_miss_latency = PCYCLE_DIFF(PACK_CYCLE(current_core_cycle[fill_cpu]), MSHR.entry[mshr_index].cycle_enqueued);
		total_miss_latency += current_miss_latency;
	      }


            if (warmup_complete[fill_cpu] && MSHR.entry[mshr_index].type == LOAD)
                llc_bypass_fill(fill_cpu, (CACHE*)upper_level_dcache[fill_cpu]->upper_level_dcache[fill_cpu], (CACHE*)upper_level_dcache[fill_cpu], (CACHE*)this, MSHR.entry[mshr_index]);
            // LPM counter: MSHR removal (LLC bypass path)
            {
                bool _byp = false;
#ifdef BYPASS_L2_LOGIC
                if (MSHR.entry[mshr_index].l2_bypassed) _byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
                if (MSHR.entry[mshr_index].llc_bypassed) _byp = true;
#endif
                if (_byp) { lpm_mshr_byp_count--; lpm_mshr_byp_per_cpu[fill_cpu]--; }
                if (MSHR.entry[mshr_index].returned == INFLIGHT) {
                    lpm_mshr_inflight_count--;
                    lpm_mshr_inflight_per_cpu[fill_cpu]--;
                }
                lpm_mshr_occ_per_cpu[fill_cpu]--;
            }
            uint64_t removed_addr = MSHR.entry[mshr_index].address;
            MSHR.remove_queue(&MSHR.entry[mshr_index]);
            MSHR.num_returned--;
#ifdef USE_LLC_HASHMAP_MSHR
            if (cache_type == IS_LLC)
                mshr_map.erase(removed_addr);
            else
#endif
                bloom_rebuild_mshr();

            update_fill_cycle();

            return; // return here, no need to process further in this function
        }
#endif

        uint8_t do_fill = 1;

        // is this dirty?
        DP_FILL_M(("STAGE_FILL_EVICT_DIRTY idx=" + std::to_string(mshr_index) + " set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &MSHR.entry[mshr_index]);
        handle_fill_evict_dirty(fill_cpu, mshr_index, set, way, do_fill);

        if (do_fill){
            DP_FILL_M(("FILL_DO_FILL idx=" + std::to_string(mshr_index) + " set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &MSHR.entry[mshr_index]);
            // update prefetcher
            handle_fill_pf_fill(fill_cpu, mshr_index, set, way);

            // update replacement policy
            handle_fill_replacement(fill_cpu, mshr_index, set, way);

            // COLLECT STATS
            handle_fill_stats(fill_cpu, mshr_index);

#ifdef USE_HERMES
            if (cache_type == IS_LLC && MSHR.entry[mshr_index].type == LOAD) {
                uint32_t _cpu = MSHR.entry[mshr_index].cpu;
                uint32_t _lq = MSHR.entry[mshr_index].lq_index;
                if (_lq < ooo_cpu[_cpu].LQ.SIZE) {
                    ooo_cpu[_cpu].LQ.entry[_lq].went_offchip = 1;
                    if (knob::offchip_pred_mark_merged_load) {
                        ITERATE_SET(merged, MSHR.entry[mshr_index].lq_index_depend_on_me, ooo_cpu[_cpu].LQ.SIZE) {
                            ooo_cpu[_cpu].LQ.entry[merged].went_offchip = 1;
                        }
                    }
                }
            }
#endif

            // fill cache and mark dirty if RFO
            handle_fill_cache_and_dirty(mshr_index, set, way);

            if (warmup_complete[fill_cpu] && MSHR.entry[mshr_index].type == LOAD) {
                if (cache_type == IS_L1D)
                    l1d_bypass_fill(fill_cpu, (CACHE*)this, (CACHE*)lower_level, (CACHE*)lower_level->lower_level, MSHR.entry[mshr_index]);
                else if (cache_type == IS_L2C)
                    l2c_bypass_fill(fill_cpu, (CACHE*)upper_level_dcache[fill_cpu], (CACHE*)this, (CACHE*)lower_level, MSHR.entry[mshr_index]);
                else if (cache_type == IS_LLC)
                    llc_bypass_fill(fill_cpu, (CACHE*)upper_level_dcache[fill_cpu]->upper_level_dcache[fill_cpu], (CACHE*)upper_level_dcache[fill_cpu], (CACHE*)this, MSHR.entry[mshr_index]);
            }

            // check fill level
            DP_FILL_M(("STAGE_FILL_RETURN_DOFILL idx=" + std::to_string(mshr_index) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " self.fill=" + std::to_string(fill_level) + " set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &MSHR.entry[mshr_index]);
            handle_fill_return(mshr_index);

            // update processed packets / bypass returns
            DP_FILL_M(("STAGE_FILL_PROCESSED_BYPRET idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
            handle_fill_processed_and_bypass_return(fill_cpu, mshr_index, set, way);

            // update miss latency, remove MSHR, update fill cycle
            DP_FILL_M(("STAGE_FILL_REMOVE idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
            handle_fill_remove(fill_cpu, mshr_index);
        }
    }
}

void CACHE::merge_with_prefetch(PACKET &mshr_packet, PACKET &queue_packet) {
    // ORIGINAL info retainer
    uint8_t prior_returned = mshr_packet.returned;
    uint64_t prior_event_cycle = mshr_packet.event_cycle;
#ifdef BYPASS_L1_LOGIC
    uint8_t prior_fill = mshr_packet.fill_level;
    uint8_t prior_l1_bypassed = mshr_packet.l1_bypassed;
#endif
#ifdef BYPASS_L2_LOGIC
    uint8_t prior_l2_bypassed = mshr_packet.l2_bypassed;
#endif
#ifdef BYPASS_LLC_LOGIC
    uint8_t prior_llc_bypassed = mshr_packet.llc_bypassed;
#endif
    uint8_t prior_pf_merged = mshr_packet.pf_merged_from_upper;
    mshr_packet = queue_packet;
    // restore original retained data
    mshr_packet.returned = prior_returned;
    mshr_packet.event_cycle = prior_event_cycle;
#ifdef BYPASS_L1_LOGIC
    mshr_packet.fill_level = (prior_fill < queue_packet.fill_level) ? prior_fill : queue_packet.fill_level;
    mshr_packet.l1_bypassed = prior_l1_bypassed;
#endif
#ifdef BYPASS_L2_LOGIC
    mshr_packet.l2_bypassed = prior_l2_bypassed;
#endif
#ifdef BYPASS_LLC_LOGIC
    mshr_packet.llc_bypassed = prior_llc_bypassed;
#endif
    mshr_packet.pf_merged_from_upper = prior_pf_merged;
}

void CACHE::handle_writeback() {
    if (WQ.occupancy == 0) return;
    // Tier B hoist
    const uint16_t wq_h_ = WQ.head;
    // handle write
    uint32_t writeback_cpu = WQ.entry[wq_h_].cpu;
    if (writeback_cpu == NUM_CPUS)
        return;

    // handle the oldest entry
    if (WQ.occupancy > 0) PCYCLE_SANITY(WQ.entry[wq_h_].event_cycle, current_core_cycle[writeback_cpu], std::string(NAME) + "_handle_write");
    if ((PCYCLE_LE(WQ.entry[wq_h_].event_cycle, PACK_CYCLE(current_core_cycle[writeback_cpu]))) && (WQ.occupancy > 0)) {
        int index = wq_h_;

        // access cache
        uint32_t set = get_set(WQ.entry[index].address);
        int way = check_hit(&WQ.entry[index]);

        DP_FILL_M(("WB_ENTRY set=" + std::to_string(set) + " way=" + std::to_string(way) + " wq.type=" + std::to_string(WQ.entry[index].type) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level)).c_str(), &WQ.entry[index]);

        if (way >= 0) { // writeback hit (or RFO hit for L1D)
            DP_FILL_M(("STAGE_WB_HIT set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &WQ.entry[index]);
            handle_writeback_hit(writeback_cpu, index, set, way);
        }
        else { // writeback miss (or RFO miss for L1D)
            if (cache_type == IS_L1D) { // RFO miss
                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = CHECK_MSHR(&WQ.entry[index]);
                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss
                    DP_FILL_M(("STAGE_WB_MISS_NEW set=" + std::to_string(set) + " occu=" + std::to_string(MSHR.occupancy)).c_str(), &WQ.entry[index]);
                    handle_writeback_miss_new(index, miss_handled);
                } else {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
                        DP_FILL_M(("STAGE_WB_MISS_MSHR_FULL occu=" + std::to_string(MSHR.occupancy)).c_str(), &WQ.entry[index]);
                        handle_writeback_miss_mshr_full(index, miss_handled);
                    }
                    else if (mshr_index != -1) { // already in-flight miss
                        DP_FILL_M(("STAGE_WB_MISS_INFLIGHT mshr_idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level)).c_str(), &WQ.entry[index]);
                        handle_writeback_miss_inflight(index, mshr_index);
                    }
                    else { // WE SHOULD NOT REACH HERE
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }

                if (miss_handled) {
                    DP_FILL_M("STAGE_WB_MISS_HANDLED_REMOVE", &WQ.entry[index]);
                    handle_writeback_miss_handled_stats_remove(index);
                }

            }
            else {
                // find victim
                uint32_t set = get_set(WQ.entry[index].address);
                uint32_t way2 = handle_writeback_find_victim(writeback_cpu, index, set);

#ifdef LLC_BYPASS
                if ((cache_type == IS_LLC) && (way2 == LLC_WAY)) {
                    cerr << "LLC bypassing for writebacks is not allowed!" << endl;
                    assert(0);
                }
#endif

                uint8_t do_fill = 1;

                // is this dirty?
                DP_FILL_M(("STAGE_WB_EVICT_DIRTY set=" + std::to_string(set) + " way2=" + std::to_string(way2)).c_str(), &WQ.entry[index]);
                handle_writeback_evict_dirty(writeback_cpu, index, set, way2, do_fill);

                if (do_fill) {
                    DP_FILL_M(("STAGE_WB_DO_FILL set=" + std::to_string(set) + " way2=" + std::to_string(way2)).c_str(), &WQ.entry[index]);
                    handle_writeback_do_fill(writeback_cpu, index, set, way2);
                }
            }
        }
    }
}
bool CACHE::handle_read_miss_bypass(uint16_t read_cpu, int index, int mshr_index) {
#if defined(BYPASS_L1D_OnNewMiss) || defined(BYPASS_L2_LOGIC) || defined(BYPASS_LLC_LOGIC)
    if (warmup_complete[cpu] && (cache_type == IS_L1D || cache_type == IS_L2C || cache_type == IS_LLC)
            && RQ.entry[index].type == LOAD && mshr_index == -1
            && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)) {
        total_ByP_req[read_cpu]++;
        ByP_req[read_cpu]++;
    }
#endif
#ifdef BYPASS_L1D_OnNewMiss
    if (warmup_complete[cpu] && cache_type == IS_L1D && RQ.entry[index].type == LOAD
            && mshr_index == -1 && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (lpm_demand_compute_all(read_cpu), l1d_bypass_operate(read_cpu, (CACHE*)this, (CACHE*)lower_level, (CACHE*)lower_level->lower_level))) {
        RQ.entry[index].l1_bypassed = 1;
        RQ.entry[index].fill_level = FILL_L2;
        total_ByP_issued[read_cpu]++;
        ByP_issued[read_cpu]++;
        sim_byp_wByP[read_cpu]++;
        sim_access_wByP[read_cpu]++;
        DP_BYP(&RQ.entry[index],"L1");
        DP_FWD(&RQ.entry[index],"lower");
        lower_level->add_rq(&RQ.entry[index]);
        return true;
    }
#endif
#ifdef BYPASS_L2_LOGIC
    if (warmup_complete[cpu] && cache_type == IS_L2C && RQ.entry[index].type == LOAD
            && !RQ.entry[index].instruction && mshr_index == -1
            && RQ.entry[index].l1_bypassed == 0
            && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (lpm_demand_compute_all(read_cpu), l2c_bypass_operate(read_cpu, (CACHE*)upper_level_dcache[read_cpu], (CACHE*)this, (CACHE*)lower_level))) {
        RQ.entry[index].l2_bypassed = 1;
        RQ.entry[index].fill_level = FILL_LLC;
        total_ByP_issued[read_cpu]++;
        ByP_issued[read_cpu]++;
        sim_byp_wByP[read_cpu]++;
        sim_access_wByP[read_cpu]++;
        DP_BYP(&RQ.entry[index],"L2");
        DP_FWD(&RQ.entry[index],"lower");
        lower_level->add_rq(&RQ.entry[index]);
        return true;
    }
#endif
#ifdef BYPASS_LLC_LOGIC
    if (warmup_complete[cpu] && cache_type == IS_LLC && RQ.entry[index].type == LOAD
            && mshr_index == -1 && RQ.entry[index].l2_bypassed == 0
            && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (lpm_demand_compute_all(read_cpu), llc_bypass_operate(read_cpu, (CACHE*)lower_level->lower_level, (CACHE*)lower_level, (CACHE*)this))) {
        RQ.entry[index].llc_bypassed = 1;
        total_ByP_issued[read_cpu]++;
        ByP_issued[read_cpu]++;
        sim_byp_wByP[read_cpu]++;
        sim_access_wByP[read_cpu]++;
        DP_BYP(&RQ.entry[index],"LLC");
        DP_FWD(&RQ.entry[index],"lower");
        lower_level->add_rq(&RQ.entry[index]);
        return true;
    }
#endif
    return false;
}

void CACHE::handle_read() {
    for (uint16_t i=0; i<MAX_READ; i++) {
        // Tier B hoist (per-iter; head mutates via remove_queue)
        const uint16_t rq_h_ = RQ.head;
        uint16_t read_cpu = RQ.entry[rq_h_].cpu;
        if (read_cpu == NUM_CPUS)
            return;

        if (RQ.occupancy > 0) PCYCLE_SANITY(RQ.entry[rq_h_].event_cycle, current_core_cycle[read_cpu], std::string(NAME) + "_handle_read");
        if ((PCYCLE_LE(RQ.entry[rq_h_].event_cycle, PACK_CYCLE(current_core_cycle[read_cpu]))) && (RQ.occupancy > 0)) {
            int index = rq_h_;
            uint32_t set = get_set(RQ.entry[index].address);
            int way = check_hit(&RQ.entry[index]);

            if (way >= 0) { // read hit
                DP_HIT(&RQ.entry[index],set,way);
                DP_FILL_M(("STAGE_HIT_PROCESSED set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &RQ.entry[index]);
                handle_read_hit_processed(index, set, way);
                DP_FILL_M("STAGE_HIT_BYP_RETURN", &RQ.entry[index]);
                handle_read_hit_bypass_return(read_cpu, index);
                DP_FILL_M("STAGE_HIT_PF_OPERATE", &RQ.entry[index]);
                handle_read_hit_pf_operate(read_cpu, index, set, way);
                DP_FILL_M("STAGE_HIT_REPLACEMENT", &RQ.entry[index]);
                handle_read_hit_replacement(read_cpu, index, set, way);
                DP_FILL_M("STAGE_HIT_STATS", &RQ.entry[index]);
                handle_read_hit_stats(read_cpu, index);
                DP_FILL_M("STAGE_HIT_RETURN", &RQ.entry[index]);
                handle_read_hit_return(index);
                DP_FILL_M("STAGE_HIT_PF_USEFUL_REMOVE", &RQ.entry[index]);
                handle_read_hit_pf_useful_and_remove(index, set, way);
            } else { // read miss
                uint8_t miss_handled = 1;
                int mshr_index = CHECK_MSHR(&RQ.entry[index]);
                DP_MISS(&RQ.entry[index],mshr_index);

                bool lower_rq_ok = (lower_level != NULL) &&
                                   (lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address));
                if (lower_rq_ok && handle_read_miss_bypass(read_cpu, index, mshr_index)) {
                    DP_FILL_M("STAGE_MISS_BYPASS_TAKEN", &RQ.entry[index]);
                } else if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) {
                    if (cache_type == IS_LLC) {
                        DP_FILL_M(("STAGE_MISS_NEW_LLC occu=" + std::to_string(MSHR.occupancy)).c_str(), &RQ.entry[index]);
                        handle_read_miss_new_llc(index, miss_handled);
                    } else {
                        DP_FILL_M(("STAGE_MISS_NEW_OTHER occu=" + std::to_string(MSHR.occupancy)).c_str(), &RQ.entry[index]);
                        handle_read_miss_new_other(read_cpu, index, miss_handled);
                    }
                } else if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) {
                    DP_FILL_M(("STAGE_MISS_MSHR_FULL occu=" + std::to_string(MSHR.occupancy)).c_str(), &RQ.entry[index]);
                    handle_read_miss_mshr_full(index, miss_handled);
                } else if (mshr_index != -1) {
                    DP_FILL_M(("PRE_INFLIGHT idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned) + " rq.type=" + std::to_string(RQ.entry[index].type) + " rq.fill=" + std::to_string(RQ.entry[index].fill_level)).c_str(), &MSHR.entry[mshr_index]);
                    DP_FILL_M(("STAGE_INFLIGHT_MERGE_DEPS idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
                    handle_read_miss_inflight_merge_deps(index, mshr_index);
                    DP_FILL_M(("STAGE_INFLIGHT_FILL_LEVEL idx=" + std::to_string(mshr_index) + " pre.mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " rq.fill=" + std::to_string(RQ.entry[index].fill_level)).c_str(), &MSHR.entry[mshr_index]);
                    handle_read_miss_inflight_fill_level(index, mshr_index);
                    DP_FILL_M(("STAGE_INFLIGHT_BYP_L1_MISMATCH idx=" + std::to_string(mshr_index) + " rq.l1byp=" + std::to_string(RQ.entry[index].l1_bypassed) + " mshr.l1byp=" + std::to_string(MSHR.entry[mshr_index].l1_bypassed)).c_str(), &MSHR.entry[mshr_index]);
                    handle_read_miss_inflight_bypass_l1_mismatch(index, mshr_index);
                    DP_FILL_M(("STAGE_INFLIGHT_BYP_L2_MISMATCH idx=" + std::to_string(mshr_index) + " rq.l2byp=" + std::to_string(RQ.entry[index].l2_bypassed) + " mshr.l2byp=" + std::to_string(MSHR.entry[mshr_index].l2_bypassed)).c_str(), &MSHR.entry[mshr_index]);
                    handle_read_miss_inflight_bypass_l2_mismatch(index, mshr_index);
                    if (MSHR.entry[mshr_index].type == PREFETCH) {
                        DP_FILL_M(("STAGE_INFLIGHT_PF_TAKEOVER idx=" + std::to_string(mshr_index) + " rq.type=" + std::to_string(RQ.entry[index].type)).c_str(), &MSHR.entry[mshr_index]);
                        handle_read_miss_inflight_prefetch_takeover(index, mshr_index);
                    } else {
                        DP_FILL_M(("STAGE_INFLIGHT_NONPF_MERGE idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type)).c_str(), &MSHR.entry[mshr_index]);
                        handle_read_miss_inflight_non_prefetch_merge(index, mshr_index);
                    }
                    MSHR_MERGED[RQ.entry[index].type]++;
                    DP_FILL_M(("POST_INFLIGHT idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned) + " nxtfi=" + std::to_string(MSHR.next_fill_index) + " nxtfc=" + std::to_string(MSHR.next_fill_cycle)).c_str(), &MSHR.entry[mshr_index]);
                } else {
                    cerr << "[" << NAME << "] MSHR errors" << endl;
                    assert(0);
                }

                if (miss_handled) {
                    DP_FILL_M("STAGE_MISS_HANDLED_PF_OPERATE", &RQ.entry[index]);
                    handle_read_miss_handled_pf_operate(read_cpu, index);
                    DP_FILL_M("STAGE_MISS_HANDLED_STATS_REMOVE", &RQ.entry[index]);
                    handle_read_miss_handled_stats_remove(index);
                }
            }
        } else {
            return;
        }
        if (reads_available_this_cycle == 0)
            return;
    }
}

void CACHE::handle_prefetch() {
    // handle prefetch
    for (uint16_t i=0; i<MAX_READ; i++) {
        // Tier B hoist (per-iter; head mutates via remove_queue)
        const uint16_t pq_h_ = PQ.head;
        uint16_t prefetch_cpu = PQ.entry[pq_h_].cpu;
        if (prefetch_cpu == NUM_CPUS){
            return;
        }
        // handle the oldest entry
        if (PQ.occupancy > 0) PCYCLE_SANITY(PQ.entry[pq_h_].event_cycle, current_core_cycle[prefetch_cpu], std::string(NAME) + "_handle_prefetch");
        if ((PCYCLE_LE(PQ.entry[pq_h_].event_cycle, PACK_CYCLE(current_core_cycle[prefetch_cpu]))) && (PQ.occupancy > 0)) {
            int index = pq_h_;

            // access cache
            uint32_t set = get_set(PQ.entry[index].address);
            int way = check_hit(&PQ.entry[index]);

            if (way >= 0) { // prefetch hit
                // update replacement policy
                if (cache_type == IS_LLC) {
            if (way < LLC_WAY) block[set][way].pmc++;
                    llc_update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[index].ip, 0, PQ.entry[index].type, 1);

                }
                else
                    update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[index].ip, 0, PQ.entry[index].type, 1);
                // COLLECT STATS
                sim_hit[prefetch_cpu][PQ.entry[index].type]++;
                sim_access[prefetch_cpu][PQ.entry[index].type]++;
                lpm_shadow_inc(prefetch_cpu, PQ.entry[index].type, false);
		        // run prefetcher on prefetches from higher caches
                if(PQ.entry[index].pf_origin_level < fill_level){
                    if (cache_type == IS_L1D)
                        l1d_prefetcher_operate(PQ.entry[index].full_addr, PQ.entry[index].ip, 1, PREFETCH);
                    else if (cache_type == IS_L2C)
                        PQ.entry[index].pf_metadata = l2c_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, PQ.entry[index].ip, 1, PREFETCH, PQ.entry[index].pf_metadata);
                    else if (cache_type == IS_LLC){
                        cpu = prefetch_cpu;
                        PQ.entry[index].pf_metadata = llc_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, PQ.entry[index].ip, 1, PREFETCH, PQ.entry[index].pf_metadata);
                        cpu = 0;
                    }
                }
                // check fill level
                if (PQ.entry[index].fill_level < fill_level) {
                    DP_FILL_M(("STAGE_PQ_HIT_RETURN_UP pq.fill=" + std::to_string(PQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &PQ.entry[index]);
                    return_to_upper_level(PQ.entry[index]);
                }
                HIT[PQ.entry[index].type]++;
                ACCESS[PQ.entry[index].type]++;
                // remove this entry from PQ
                PQ.remove_queue(&PQ.entry[index]);
		        reads_available_this_cycle--;
            }
            else { // prefetch miss
                DP_MISS(&PQ.entry[index],CHECK_MSHR(&PQ.entry[index]));

                // check mshr
                uint8_t miss_handled = 1;
                int mshr_index = CHECK_MSHR(&PQ.entry[index]);

                if ((mshr_index == -1) && (MSHR.occupancy < MSHR_SIZE)) { // this is a new miss
                    DP_PQ(&PQ.entry[index]);

                    // first check if the lower level PQ is full or not
                    // this is possible since multiple prefetchers can exist at each level of caches
                    if (lower_level) {
                        if (cache_type == IS_LLC) {
                            if (lower_level->get_occupancy(1, PQ.entry[index].address) == lower_level->get_size(1, PQ.entry[index].address))
                                miss_handled = 0;
                            else {
                                // run prefetcher on prefetches from higher caches
                                if(PQ.entry[index].pf_origin_level < fill_level){
                                    if (cache_type == IS_LLC){
                                        cpu = prefetch_cpu;
                                        PQ.entry[index].pf_metadata = llc_prefetcher_operate(PQ.entry[index].address<<LOG2_BLOCK_SIZE, PQ.entry[index].ip, 0, PREFETCH, PQ.entry[index].pf_metadata);
                                        cpu = 0;
                                    }
                                }

                                // add it to MSHRs if this prefetch miss will be filled to this cache level
                                if (PQ.entry[index].fill_level <= fill_level)
                                    add_mshr(&PQ.entry[index]);
                                lower_level->add_rq(&PQ.entry[index]); // add it to the DRAM RQ
                            }
                        } else {
                            if (lower_level->get_occupancy(3, PQ.entry[index].address) == lower_level->get_size(3, PQ.entry[index].address))
                                miss_handled = 0;
                            else {
                                // run prefetcher on prefetches from higher caches
                                if(PQ.entry[index].pf_origin_level < fill_level) {
                                    if (cache_type == IS_L1D)
                                        l1d_prefetcher_operate(PQ.entry[index].full_addr, PQ.entry[index].ip, 0, PREFETCH);
                                    if (cache_type == IS_L2C)
                                        PQ.entry[index].pf_metadata = l2c_prefetcher_operate(PQ.entry[index].address<<LOG2_BLOCK_SIZE, PQ.entry[index].ip, 0, PREFETCH, PQ.entry[index].pf_metadata);
                                }
                                // add it to MSHRs if this prefetch miss will be filled to this cache level
                                if (PQ.entry[index].fill_level <= fill_level)
                                    add_mshr(&PQ.entry[index]);

                                int success = lower_level->add_pq(&PQ.entry[index]); // add it to the DRAM RQ
                                if (success == -2) {
                                    assert(0&&" PQ added MSHR && lower lvl add_pq FAILURE!!!");
                                }
			                }
		                }
		            }
                } else {
                    if ((mshr_index == -1) && (MSHR.occupancy == MSHR_SIZE)) { // not enough MSHR resource
                        // TODO: should we allow prefetching with lower fill level at this case?
                        // cannot handle miss request until one of MSHRs is available
                        miss_handled = 0;
                        STALL[PQ.entry[index].type]++;
                    } else if (mshr_index != -1) { // already in-flight miss

                        // WE SIMPLY DO NOT HANDLE IT THIS TIME ... LET IT IGNORE ...

                        // no need to update request except fill_level
                        // update fill_level
                        // if (PQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level && MSHR.entry[mshr_index].type == PREFETCH)// VB fix: THIS IS A PROBLEM IF WE HAVE ALREADY THIS IN FLGIHT AND INFLIGHT BEING THE DEMAND REQUEST. THUS,
                        //     MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        if (PQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level) {
                            if (MSHR.entry[mshr_index].type == PREFETCH) {
                                MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                            }
                    #ifdef BYPASS_L1_LOGIC
                        else if (MSHR.entry[mshr_index].l1_bypassed == 1) {
                            // Prefetcher predicted data needed at L1 — clear bypass.
                            // Normal return path: L2C fill → return_to_upper_level → L1D return_data
                            // promotes prefetch MSHR to LOAD, propagates deps. No zombie.
                            MSHR.entry[mshr_index].l1_bypassed = 0;
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        }
                    #endif
                    #ifdef BYPASS_L2_LOGIC
                        else if (MSHR.entry[mshr_index].l2_bypassed == 1) {
                            // Cannot clear l2_bypassed here: L2C only has a PREFETCH
                            // MSHR (no promotion logic), so normal return would fill
                            // L2C but never forward to L1D.  Keep bypass active and
                            // tag for MSHR cleanup on the LLC→L1D direct return path.
                            MSHR.entry[mshr_index].pf_merged_from_upper = 1;
                        }
                    #endif
                        }
                    #ifdef BYPASS_LLC_LOGIC
                        if (MSHR.entry[mshr_index].llc_bypassed == 1) {
                            MSHR.entry[mshr_index].llc_bypassed = 0;
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        }
                    #endif
                        MSHR_MERGED[PQ.entry[index].type]++;
                        DP_MSHR_MERGE(&PQ.entry[index],MSHR.entry[mshr_index],mshr_index);
                    } else { // WE SHOULD NOT REACH HERE
                        cerr << "[" << NAME << "] MSHR errors" << endl;
                        assert(0);
                    }
                }
                if (miss_handled) {
                    DP_FWD(&PQ.entry[index],lower_level ? "lower" : "none");
                    MISS[PQ.entry[index].type]++;
                    ACCESS[PQ.entry[index].type]++;

                    // remove this entry from PQ
                    PQ.remove_queue(&PQ.entry[index]);
		            reads_available_this_cycle--;
                }
            }
        } else{
	        return;
	    }
        if(reads_available_this_cycle == 0) {
            return;
        }
    }
}


void CACHE::operate() {
    DP( do { if (DP_GATE_WW(current_core_cycle[cpu],cpu,RQ.entry[RQ.head].address,RQ.entry[RQ.head].instr_id)) {
        cout << NAME << " RQ_HEAD idx:" << RQ.head << " iid:" << RQ.entry[RQ.head].instr_id
             << " MSHR_HEAD idx:" << MSHR.head << " iid:" << MSHR.entry[MSHR.head].instr_id << endl;
    } } while(0) );
   if (warmup_complete[cpu]) {

        /* ---- α = total accesses at this level (Tier A: shadow read, 1 cacheline) ---- */
        const auto& __ls = lpm_shadow[cpu];
        uint64_t α = __ls.alpha_total;
        uint64_t load_α = __ls.load_alpha;
        uint64_t load_miss = __ls.load_miss;

        /* ---- hit_active: requests in H-cycle phase ---- */
        bool hit_active = (RQ.occupancy | WQ.occupancy) > 0;

        /* ---- miss_active: outstanding miss penalty ---- */
        /* ---- O(1) counters replace MSHR scan ---- */
#ifdef LPM_STRICT_MISS
        bool miss_active = (lpm_mshr_inflight_count > 0);
#else
        bool miss_active = (MSHR.occupancy > 0);
#endif
        bool has_byp = (lpm_mshr_byp_count > 0);

#ifdef SANITY_CHECK
        // Verify counters match brute-force scan
        {
            bool _sc_miss = false, _sc_byp = false;
            uint16_t _sc_seen = 0;
            for (uint16_t i = 0; i < MSHR_SIZE && _sc_seen < MSHR.occupancy; i++) {
                const auto& e = MSHR.entry[i];
                if (!e.address) continue;
                _sc_seen++;
#ifdef LPM_STRICT_MISS
                if (!_sc_miss && e.returned != COMPLETED) _sc_miss = true;
#else
                _sc_miss = true;
#endif
#ifdef BYPASS_L1_LOGIC
                if (!_sc_byp && cache_type == IS_L2C && e.l1_bypassed) _sc_byp = true;
#endif
#ifdef BYPASS_L2_LOGIC
                if (!_sc_byp && cache_type == IS_LLC && e.l2_bypassed) _sc_byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
                if (!_sc_byp && cache_type == IS_LLC && e.llc_bypassed) _sc_byp = true;
#endif
                if (_sc_miss && _sc_byp) break;
            }
#ifndef LPM_STRICT_MISS
            _sc_miss = (MSHR.occupancy > 0);
#endif
            assert(miss_active == _sc_miss && "LPM counter mismatch: miss_active");
            assert(has_byp == _sc_byp && "LPM counter mismatch: has_byp");
        }
#endif

        /* ---- tick + update cached metrics ---- */
        if (cache_type == IS_LLC) {
#ifdef TRACKER_LPM_SHARED
            /* LLC is shared — tick lpm for every CPU that completed warmup */
            for (uint32_t c = 0; c < NUM_CPUS; c++) {
                if (!warmup_complete[c]) continue;
                const auto& __lsc = lpm_shadow[c];
                uint64_t α_c = __lsc.alpha_total;
                uint64_t load_α_c = __lsc.load_alpha;
                uint64_t load_miss_c = __lsc.load_miss;
                lpm_operate(c, cache_type, hit_active, miss_active, α_c, has_byp, load_α_c, load_miss_c);
            }
#else
            /* LLC per-core — bucket MSHR state by entry.cpu, then tick each CPU separately */
            /* ---- O(1) per-CPU counters replace per-CPU MSHR scan ---- */
            bool miss_active_pc[NUM_CPUS] = {false};
            bool has_byp_pc[NUM_CPUS]     = {false};
            for (uint32_t c = 0; c < NUM_CPUS; c++) {
#ifdef LPM_STRICT_MISS
                miss_active_pc[c] = (lpm_mshr_inflight_per_cpu[c] > 0);
#else
                miss_active_pc[c] = (lpm_mshr_occ_per_cpu[c] > 0);
#endif
                has_byp_pc[c] = (lpm_mshr_byp_per_cpu[c] > 0);
            }
#ifdef SANITY_CHECK
            // Verify per-CPU counters match brute-force scan
            {
                bool _sc_miss_pc[NUM_CPUS] = {false};
                bool _sc_byp_pc[NUM_CPUS]  = {false};
                uint16_t _sc_seen2 = 0;
                for (uint16_t i = 0; i < MSHR_SIZE && _sc_seen2 < MSHR.occupancy; i++) {
                    const auto& e = MSHR.entry[i];
                    if (!e.address) continue;
                    _sc_seen2++;
                    uint32_t ec = e.cpu;
                    if (ec >= NUM_CPUS) continue;
#ifdef LPM_STRICT_MISS
                    if (!_sc_miss_pc[ec] && e.returned != COMPLETED) _sc_miss_pc[ec] = true;
#else
                    _sc_miss_pc[ec] = true;
#endif
#ifdef BYPASS_L2_LOGIC
                    if (!_sc_byp_pc[ec] && e.l2_bypassed) _sc_byp_pc[ec] = true;
#endif
#ifdef BYPASS_LLC_LOGIC
                    if (!_sc_byp_pc[ec] && e.llc_bypassed) _sc_byp_pc[ec] = true;
#endif
                }
                for (uint32_t c = 0; c < NUM_CPUS; c++) {
                    assert(miss_active_pc[c] == _sc_miss_pc[c] && "LPM per-cpu counter mismatch: miss_active_pc");
                    assert(has_byp_pc[c] == _sc_byp_pc[c] && "LPM per-cpu counter mismatch: has_byp_pc");
                }
            }
#endif
            for (uint32_t c = 0; c < NUM_CPUS; c++) {
                if (!warmup_complete[c]) continue;
                const auto& __lsc = lpm_shadow[c];
                uint64_t α_c = __lsc.alpha_total;
                uint64_t load_α_c = __lsc.load_alpha;
                uint64_t load_miss_c = __lsc.load_miss;
                bool hit_active_c = (RQ.occupancy | WQ.occupancy) > 0;
                lpm_operate(c, cache_type, hit_active_c, miss_active_pc[c], α_c, has_byp_pc[c], load_α_c, load_miss_c);
            }
#endif
        } else {
            lpm_operate(cpu, cache_type, hit_active, miss_active,
                        α, has_byp, load_α, load_miss);
        }
    }
    /* >>> end LPM <<< */

    if (__builtin_expect(has_work == 0, 1)) return;
    has_work = 0;

    handle_fill();
    handle_writeback();
    reads_available_this_cycle = MAX_READ;
    handle_read();
    if (PQ.occupancy && (reads_available_this_cycle > 0))
        handle_prefetch();

    has_work = (MSHR.num_returned > 0) | (RQ.occupancy > 0) | (WQ.occupancy > 0) | (PQ.occupancy > 0);
}

uint32_t CACHE::get_way(const uint64_t address, const uint32_t set) const {
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == address))
            return way;
    }

    return NUM_WAY;
}

void CACHE::fill_cache(const uint32_t set, const uint32_t way, PACKET *packet) {
    SANITY_FILL_TLB_DATA(cache_type, packet);
    if (block[set][way].prefetch && (block[set][way].used == 0))
        pf_useless++;

#ifdef BYPASS_L1_LOGIC
    if (cache_type == IS_L1D)
        g_l1_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
#endif
#ifdef BYPASS_L2_LOGIC
    if (cache_type == IS_L2C)
        g_l2_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
#endif
#ifdef BYPASS_LLC_LOGIC
    if (cache_type == IS_LLC)
        g_llc_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
#endif

#ifdef USE_HERMES
    if (cache_type == IS_LLC && block[set][way].valid)
        ooo_cpu[packet->cpu].offchip_predictor_track_llc_eviction(set, way, block[set][way].full_addr);
#endif

    if (block[set][way].valid == 0)
        block[set][way].valid = 1;
    block[set][way].dirty = 0;
    block[set][way].prefetch = (packet->type == PREFETCH) ? 1 : 0;
    block[set][way].used = 0;
        block[set][way].pmc = 0;

    if (block[set][way].prefetch)
        pf_fill++;

    // block[set][way].delta = packet->delta;
    // block[set][way].depth = packet->depth;
    // block[set][way].signature = packet->signature;
    // block[set][way].confidence = packet->confidence;

    block[set][way].tag = packet->address;
    block[set][way].address = packet->address;
    block[set][way].full_addr = packet->full_addr;
    block[set][way].data = packet->data;
    // DEAD-2026-05-25: BLOCK.cpu — field never read
    // block[set][way].cpu = packet->cpu;
    // DEAD-2026-05-25: BLOCK.instr_id — field never read
    // block[set][way].instr_id = packet->instr_id;

    // DP( if ((current_core_cycle[cpu] > 58318781) || warmup_complete[packet->cpu] || (NAME == "L1D" || NAME == "L2C" || NAME == "LLC")) {
    // DP( if (warmup_complete[packet->cpu] || (NAME == "L1D" || NAME == "L2C" || NAME == "LLC")) {
    // cout << "[" << NAME << "] " << __func__ << " set: " << set << " way: " << way;
    // cout << " lru: " << block[set][way].lru << " tag: " << hex << block[set][way].tag << " full_addr: " << block[set][way].full_addr;
    // cout << " data: " << block[set][way].data << dec << endl; });
}

int CACHE::check_hit(const PACKET *packet)
{

    uint32_t set = get_set(packet->address);
    int match_way = -1;

    //pf_issued=pf_issued+1024;
    if (FORCE_ALL_HITS) { //(cache_type == IS_L1D || cache_type == IS_L2C || cache_type == IS_LLC)) {
        // Force a hit by using way 0
        block[set][0].valid = 1;
        block[set][0].tag = packet->address;
        // Optionally, set other necessary fields to simulate a valid cache line
        return 0; // Indicate a hit in way 0
    }
    SANITY_SET_BOUND_PKT(__func__, packet, set);

    // hit
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == packet->address)) {
            match_way = way;
            DP_HIT(packet,set,way);
            break;
        }
    }

    return match_way;
}

int CACHE::invalidate_entry(uint64_t inval_addr) {
    uint32_t set = get_set(inval_addr);
    int match_way = -1;
    SANITY_SET_BOUND_INV(__func__, inval_addr, set);
    // invalidate
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == inval_addr)) {
            block[set][way].valid = 0;
            match_way = way;
            DP_INVAL(inval_addr,set,way);
            break;
        }
    }
    return match_way;
}

int CACHE::add_rq(PACKET *packet) {

    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
   // check if WQ packet has ByPass and new packet not


    DP( do { if (packet->l1_bypassed == 1 && wq_index != -1 && DP_GATE_WW(current_core_cycle[packet->cpu],packet->cpu,packet->address,packet->instr_id)) {
        cout << " > add_rq(): ByP wq hit idx:" << wq_index << " iid:" << packet->instr_id << " addr:" << hex << packet->address << dec << endl;
    } } while(0) );

    if (wq_index != -1) {
        if (WQ.entry[wq_index].l1_bypassed == 1 && packet->l1_bypassed == 0)
            assert(0);
        // check fill level
        if (packet->fill_level < fill_level) {
            DP_FILL_M(("STAGE_ADDRQ_WQHIT_RETURN_UP wq_idx=" + std::to_string(wq_index) + " pkt.fill=" + std::to_string(packet->fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), packet);
            packet->data = WQ.entry[wq_index].data;
            return_to_upper_level(*packet);
        }
        SANITY_NO_LOWER_FOR_TLB_L1I(cache_type);
        // update processed packets
        if ((cache_type == IS_L1D) && (packet->type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE){
                PROCESSED.add_queue(packet);
            } else {
                std::cout << "\033[1;31m\n** RARE UNHANDLED CASE line=" << __LINE__
                          << " func=" << __func__
                          << " cache=[" << NAME << "]"
                          << " ISSUE: PROCESSED_FULL_WQ_FWD"
                          << " **\033[0m\n" << std::flush;
                assert(2);
            }

            DP_RQ_HIT(packet,"WQ_HIT");
        }
#ifdef BYPASS_SANITY_CHECK
        // if (packet->type != LOAD)
        //     assert(0&&" DID NOT EXPECT ADD RQ TO TAKE NON LOAD");
#endif
#ifdef BYPASS_L1_LOGIC
        if ((cache_type == IS_L2C) && (packet->type == LOAD) && packet->l1_bypassed == 1 && !packet->instruction) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
    DP( do { if (DP_GATE_WW(current_core_cycle[packet->cpu],packet->cpu,packet->address,packet->instr_id)) {
        cout << " > add_rq(): L2C PROCESSED_ADD iid:" << packet->instr_id << " addr:" << hex << packet->address << dec << endl;
    } } while(0) );
                PROCESSED.add_queue(packet);
            } else {
                std::cout << "\033[1;31m\n** RARE UNHANDLED CASE line=" << __LINE__
                          << " func=" << __func__
                          << " cache=[" << NAME << "]"
                          << " ISSUE: PROCESSED_FULL_WQ_FWD"
                          << " **\033[0m\n" << std::flush;
            }
            g_l1_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
            DP_RQ_HIT(packet,"WQ_HIT_L1BYP");
        }
#endif
#ifdef BYPASS_L2_LOGIC
        if ((cache_type == IS_LLC) && (packet->type == LOAD) && packet->l2_bypassed == 1 && !packet->instruction) {
            // if (PROCESSED.occupancy < PROCESSED.SIZE) {
                upper_level_dcache[packet->cpu]->upper_level_dcache[packet->cpu]->return_data(packet);
            g_l2_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
                // PROCESSED.add_queue(packet);
            // }
            DP_RQ_HIT(packet,"WQ_HIT_L2BYP");
        }
#endif
        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        RQ.ACCESS++;

        return -1;
    }


    // check for duplicates in the read queue
    int index = RQ.check_queue(packet);
    DP( do { if (DP_GATE_WW(current_core_cycle[packet->cpu],packet->cpu,packet->address,packet->instr_id) && index != -1
              && RQ.entry[index].l1_bypassed == 1 && packet->l1_bypassed == 0) {
        DP_RQ_MERGE(packet,RQ.entry[index],"BYP_MISMATCH");
    } } while(0) );
    if (index != -1) {
        if (packet->instruction) {
            uint16_t rob_index = packet->rob_index;
            RQ.entry[index].rob_index_depend_on_me.insert (rob_index);
            RQ.entry[index].instr_merged = 1;
            // DP( if ((current_core_cycle[cpu] > 58318781) || warmup_complete[packet->cpu]) {
            // DP( if (warmup_complete[packet->cpu]) {
            // cout << "[INSTR_MERGED] " << __func__ << " cpu: " << (int) packet->cpu << " instr_id: " << RQ.entry[index].instr_id;
            // cout << " merged idx: " << rob_index << " instr_id: " << packet->instr_id << endl; });
        } else {
            // mark merged consumer
            if (packet->type == RFO) {
                uint16_t sq_index = packet->sq_index;
                RQ.entry[index].sq_index_depend_on_me.insert (sq_index);
                RQ.entry[index].sq_index_depend_on_me.join(packet->sq_index_depend_on_me, SQ_SIZE);  // ← ADD
                RQ.entry[index].store_merged = 1;
#ifdef BYPASS_L1_LOGIC
                if (cache_type == IS_L2C) {
                    if (RQ.entry[index].l1_bypassed != packet->l1_bypassed) {
                        RQ.entry[index].l1_bypassed = 0;
                        // packet->l1_bypassed already 0 for RFO
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
#endif
#ifdef BYPASS_L2_LOGIC
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].l2_bypassed != packet->l2_bypassed) {
                        RQ.entry[index].l2_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
#endif
#ifdef BYPASS_LLC_LOGIC
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].llc_bypassed != packet->llc_bypassed) {
                        RQ.entry[index].llc_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
#endif
                DP_RQ_MERGE(packet,RQ.entry[index],"RFO");

            } 
            else {
                uint16_t lq_index = packet->lq_index;
                // RQ.entry[index].lq_index_depend_on_me.insert (lq_index);
                RQ.entry[index].lq_index_depend_on_me.insert(lq_index);
                RQ.entry[index].lq_index_depend_on_me.join(packet->lq_index_depend_on_me, LQ_SIZE);  // ← ADD
                RQ.entry[index].load_merged = 1;
#ifdef BYPASS_L2_LOGIC
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].l2_bypassed != packet->l2_bypassed) {
                        RQ.entry[index].l2_bypassed = 0;
                        packet->l2_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
#endif
#ifdef BYPASS_LLC_LOGIC
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].llc_bypassed != packet->llc_bypassed) {
                        RQ.entry[index].llc_bypassed = 0;
                        packet->llc_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
#endif
#ifdef BYPASS_L1_LOGIC
                if (cache_type == IS_L2C) {
                    if (RQ.entry[index].l1_bypassed != packet->l1_bypassed) {
                        RQ.entry[index].l1_bypassed = 0;
                        packet->l1_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                    DP_RQ_MERGE(packet,RQ.entry[index],"L1BYP_DATA");
                } else {
#endif
                    DP_RQ_MERGE(packet,RQ.entry[index],"NOT_L2");
                }
            }
        }
        RQ.MERGED++;
        RQ.ACCESS++;
        return index; // merged index
    }

    // check occupancy
    if (RQ.occupancy == RQ_SIZE) {
        RQ.FULL++;
        return -2; // cannot handle this request
    }
    // if there is no duplicate, add it to RQ
    index = RQ.tail;

    SANITY_QENTRY_EMPTY(__func__, RQ, index);

    RQ.entry[index]= std::move(*packet);
    // packet->fast_copy_packet(RQ.entry[index], *packet);

    // ADD LATENCY
    if (PCYCLE_LT(RQ.entry[index].event_cycle, PACK_CYCLE(current_core_cycle[packet->cpu])))
        RQ.entry[index].event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + LATENCY);
    else
        RQ.entry[index].event_cycle = PACK_CYCLE(RQ.entry[index].event_cycle + LATENCY);

    RQ.occupancy++;
    has_work = 1;
    RQ.tail++;
    if (RQ.tail >= RQ.SIZE)
        RQ.tail = 0;
    DP_RQ(packet,"RQ_NEW");
    //
    SANITY_PKT_ADDR_NZ(packet);
    RQ.TO_CACHE++;
    RQ.ACCESS++;

    return -1;
}

int CACHE::add_wq(PACKET *packet) {
    // check for duplicates in the write queue
    int index = WQ.check_queue(packet);
    if (index != -1) {
        WQ.MERGED++;
        WQ.ACCESS++;
        return index; // merged index
    }
    SANITY_WQ_OCCUPANCY();

    // if there is no duplicate, add it to the write queue
    index = WQ.tail;
    SANITY_WQENTRY_EMPTY(__func__, index);
    WQ.entry[index]= std::move(*packet);
    // WQ.entry[index]= *packet.fast;
    // packet->fast_copy_packet(WQ.entry[index], *packet);
    // WQ.entry[index].quickReset();
    // ADD LATENCY
    if (PCYCLE_LT(WQ.entry[index].event_cycle, PACK_CYCLE(current_core_cycle[packet->cpu])))
        WQ.entry[index].event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + LATENCY);
    else
        WQ.entry[index].event_cycle = PACK_CYCLE(WQ.entry[index].event_cycle + LATENCY);

    WQ.occupancy++;
    has_work = 1;
    WQ.tail++;
    if (WQ.tail >= WQ.SIZE)
        WQ.tail = 0;
    DP_WQ(&WQ.entry[index]);
    WQ.TO_CACHE++;
    WQ.ACCESS++;
    return -1;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, uint32_t prefetch_metadata) {
    pf_requested++;
    if (PQ.occupancy < PQ.SIZE) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            PACKET pf_packet;
            pf_packet.fill_level = pf_fill_level;
            pf_packet.pf_origin_level = fill_level;
            pf_packet.pf_metadata = prefetch_metadata;
            pf_packet.cpu = cpu;
            //pf_packet.data_index = LQ.entry[lq_index].data_index;
            //pf_packet.lq_index = lq_index;
            pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
            pf_packet.full_addr = pf_addr;
            //pf_packet.instr_id = LQ.entry[lq_index].instr_id;
            //pf_packet.rob_index = LQ.entry[lq_index].rob_index;
            pf_packet.ip = ip;
            pf_packet.type = PREFETCH;
            pf_packet.event_cycle = PACK_CYCLE(current_core_cycle[cpu]);

            // give a dummy 0 as the IP of a prefetch
            add_pq(&pf_packet);
            pf_issued++;
            return 1;
        }
    }
    return 0;
}

int CACHE::add_pq(PACKET *packet) {
    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
    if (wq_index != -1) {
        // check fill level
        if (packet->fill_level < fill_level) {
            DP_FILL_M(("STAGE_ADDPQ_WQHIT_RETURN_UP wq_idx=" + std::to_string(wq_index) + " pkt.fill=" + std::to_string(packet->fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), packet);
            packet->data = WQ.entry[wq_index].data;
            return_to_upper_level(*packet);
        }
        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        PQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the PQ
    int index = PQ.check_queue(packet);
    if (index != -1) {
        if (packet->fill_level < PQ.entry[index].fill_level)
            PQ.entry[index].fill_level = packet->fill_level;

        PQ.MERGED++;
        PQ.ACCESS++;

        return index; // merged index
    }

    // check occupancy
    if (PQ.occupancy == PQ_SIZE) {
        PQ.FULL++;
        DP_PQ_FULL(packet);
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to PQ
    index = PQ.tail;

    SANITY_QENTRY_EMPTY(__func__, PQ, index);

    PQ.entry[index]= std::move(*packet);
    // PQ.entry[index]= *packet.fast;
    // packet->fast_copy_packet(PQ.entry[index], *packet);

    // ADD LATENCY
    if (PCYCLE_LT(PQ.entry[index].event_cycle, PACK_CYCLE(current_core_cycle[packet->cpu])))
        PQ.entry[index].event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + LATENCY);
    else
        PQ.entry[index].event_cycle = PACK_CYCLE(PQ.entry[index].event_cycle + LATENCY);

    PQ.occupancy++;
    has_work = 1;
    PQ.tail++;
    if (PQ.tail >= PQ.SIZE)
        PQ.tail = 0;

    DP_PQ(&PQ.entry[index]);
    SANITY_PKT_ADDR_NZ_OR_FAH(packet);
    PQ.TO_CACHE++;
    PQ.ACCESS++;

    return -1;
}

void CACHE::return_data(PACKET *packet) {
    DP_RET_M("entry", packet);
    DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id) && cache_type == IS_LLC) {
        cout << "[LLC_return_data_IN] addr: 0x" << hex << packet->address << dec
             << " full_addr: 0x" << hex << packet->full_addr << dec
             << " type: " << (int)packet->type
             << " fill_level: " << (int)packet->fill_level
             << " cpu: " << (int)packet->cpu
             << " cy: " << current_core_cycle[packet->cpu] << endl;
    });
    // check MSHR information
    int mshr_index = CHECK_MSHR(packet);
    DP( if (DP_GATE_WW(current_core_cycle[packet->cpu], packet->cpu, packet->address, packet->instr_id) && cache_type == IS_LLC) {
        cout << "[LLC_return_data_IN] CHECK_MSHR mshr_idx: " << mshr_index
             << " addr: 0x" << hex << packet->address << dec
             << " type: " << (int)packet->type
             << " cy: " << current_core_cycle[packet->cpu] << endl;
    });
    DP_RET_M((std::string("mshr_lookup idx=") + std::to_string(mshr_index)).c_str(), packet);

    SANITY_MSHR_FOUND(__func__, packet, mshr_index);
    DP_RET(packet,"lower");
    // if (packet->instr_id == MSHR.entry[mshr_index].instr_id) {
    //     MSHR.entry[mshr_index].lq_index_depend_on_me.join(packet->lq_index_depend_on_me);
    // }

    // MSHR holds the most updated information about this request
    // no need to do memcpy
    MSHR.num_returned++;
    // LPM counter: INFLIGHT → COMPLETED transition
    if (MSHR.entry[mshr_index].returned == INFLIGHT) {
        lpm_mshr_inflight_count--;
        lpm_mshr_inflight_per_cpu[MSHR.entry[mshr_index].cpu]--;
    }
    MSHR.entry[mshr_index].returned = COMPLETED;
    has_work = 1;
    MSHR.entry[mshr_index].data = packet->data;
    MSHR.entry[mshr_index].pf_metadata = packet->pf_metadata;

    // ADD LATENCY
    if (PCYCLE_LT(MSHR.entry[mshr_index].event_cycle, PACK_CYCLE(current_core_cycle[packet->cpu])))
        MSHR.entry[mshr_index].event_cycle = PACK_CYCLE(current_core_cycle[packet->cpu] + LATENCY);
    else
        MSHR.entry[mshr_index].event_cycle = PACK_CYCLE(MSHR.entry[mshr_index].event_cycle + LATENCY);
#ifdef BYPASS_L1_LOGIC
    // CASE: ONLY PREFETCH promotion — LOAD MSHR is handled at L2C mismatch time
    if (cache_type == IS_L1D && packet->type == LOAD && !packet->instruction
        && MSHR.entry[mshr_index].type == PREFETCH) {
        DP_RET_M("L1D_pf_promote", packet);
        MSHR.entry[mshr_index].type      = packet->type;
        MSHR.entry[mshr_index].instr_id   = packet->instr_id;
        MSHR.entry[mshr_index].lq_index   = packet->lq_index;
        MSHR.entry[mshr_index].rob_index  = packet->rob_index;
        MSHR.entry[mshr_index].full_addr  = packet->full_addr;
        MSHR.entry[mshr_index].ip         = packet->ip;
        if (packet->load_merged) {
            MSHR.entry[mshr_index].load_merged = 1;
            MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                packet->lq_index_depend_on_me, LQ_SIZE);
        }
        // (trivial val print removed — low value for deadlock debug)
        MSHR.entry[mshr_index].lq_index_depend_on_me.remove(
            MSHR.entry[mshr_index].lq_index);
        }
    // CASE: THE RETURNING PACKET MAY HAVE DEPENDENCIES ACCUM FROM BYPASS AND L1D NOT HAVE THOSE, SO ON RETURN WE NATURALLY MERGE DEPENDENCES
    if (cache_type == IS_L1D && packet->type == LOAD && !packet->instruction && MSHR.entry[mshr_index].type == LOAD) {
        DP_RET_M("L1D_load_merge", packet);
        MSHR.entry[mshr_index].lq_index_depend_on_me.join(packet->lq_index_depend_on_me, LQ_SIZE);
        if (MSHR.entry[mshr_index].instr_id != packet->instr_id) {
            MSHR.entry[mshr_index].lq_index_depend_on_me.insert(packet->lq_index);
        }
    }
    // CASE: LOAD => RFO merge (L2C merged LOAD+RFO, returned via LOAD path)
    if (cache_type == IS_L1D && packet->type == LOAD && !packet->instruction && MSHR.entry[mshr_index].type == RFO) {
        DP_RET_M("L1D_load_into_rfo", packet);
        MSHR.entry[mshr_index].load_merged = 1;
        MSHR.entry[mshr_index].lq_index_depend_on_me.insert(packet->lq_index);
        if (packet->load_merged) {
            MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                packet->lq_index_depend_on_me, LQ_SIZE);
        }
        if (packet->store_merged) {
            MSHR.entry[mshr_index].store_merged = 1;
            MSHR.entry[mshr_index].sq_index_depend_on_me.join(
                packet->sq_index_depend_on_me, SQ_SIZE);
        }
    }
    // CASE: RFO packet => RFO MSHR (RQ entry retained type=RFO, but accumulated LOAD dependencies from merge)
    if (cache_type == IS_L1D && packet->type == RFO && MSHR.entry[mshr_index].type == RFO) {
        DP_RET_M("L1D_rfo_merge", packet);
        if (packet->load_merged) {
            MSHR.entry[mshr_index].load_merged = 1;
            MSHR.entry[mshr_index].lq_index_depend_on_me.join(
                packet->lq_index_depend_on_me, LQ_SIZE);
        }
        if (packet->store_merged) {
            MSHR.entry[mshr_index].store_merged = 1;
            MSHR.entry[mshr_index].sq_index_depend_on_me.join(
                packet->sq_index_depend_on_me, SQ_SIZE);
        }
        }


#endif
//     // VB: ===== NEW: propagate bypass-accumulated deps from lower level =====
// #ifdef BYPASS_L1_LOGIC
//     if (cache_type == IS_L1D) {
//         if (packet->load_merged) {
//             MSHR.entry[mshr_index].load_merged = 1;
//             MSHR.entry[mshr_index].lq_index_depend_on_me.join(
//                 packet->lq_index_depend_on_me, LQ_SIZE);
//             // MSHR.entry[mshr_index].lq_index_depend_on_me.setbit(packet->lq_index);
//
//         }
//         if (packet->store_merged) {
//             MSHR.entry[mshr_index].store_merged = 1;
//             MSHR.entry[mshr_index].sq_index_depend_on_me.join(
//                 packet->sq_index_depend_on_me, SQ_SIZE);
//         }
//     }
// #endif
//     // ===== END NEW =====


 // VB FIX (2026-05-06): SPP-prefetch-vs-L2-bypass MSHR deadlock + segfault
 // BUG 1 (deadlock): LOAD bypasses L2C → SPP fires PREFETCH for same addr at L2C.
 //   When LLC returns data, L2C MSHR type stays PREFETCH → LQ never notified → deadlock.
 //   FIX: adopt LOAD fields into L2C PREFETCH MSHR (type, instr_id, lq_index, etc.).
 //   Set fill_level=FILL_L1 so handle_fill_return propagates data up to L1D.
 //
 // BUG 2 (segfault): When LOAD had l2_bypassed=1, LLC bypass path already called
 //   L1D return_data directly. fill_level=FILL_L1 caused L2C handle_fill_return to
 //   call L1D return_data AGAIN → MSHR already removed → check_mshr returns -1
 //   → dump_req(MSHR.entry[-1]) → segfault.
 //   FIX: only set fill_level=FILL_L1 when !packet->l2_bypassed (LLC did not already
 //   deliver data to L1D via bypass double-hop).
   #ifdef BYPASS_L2_LOGIC
    if (cache_type == IS_L2C && packet->type == LOAD && !packet->instruction
        && MSHR.entry[mshr_index].type == PREFETCH) {
        DP_RET_M("L2C_pf_promote", packet);
        MSHR.entry[mshr_index].type      = packet->type;
        MSHR.entry[mshr_index].instr_id  = packet->instr_id;
        MSHR.entry[mshr_index].lq_index  = packet->lq_index;
        MSHR.entry[mshr_index].rob_index = packet->rob_index;
        MSHR.entry[mshr_index].full_addr = packet->full_addr;
        MSHR.entry[mshr_index].ip        = packet->ip;
        MSHR.entry[mshr_index].load_merged = 1;
        // Only propagate to L1D if bypass path did not already deliver data there
        if (!packet->l2_bypassed)
            MSHR.entry[mshr_index].fill_level = FILL_L1;
        MSHR.entry[mshr_index].lq_index_depend_on_me.join(
            packet->lq_index_depend_on_me, LQ_SIZE);
    }
    if (cache_type == IS_L2C && packet->type == LOAD && !packet->instruction
        && MSHR.entry[mshr_index].type == LOAD) {
        DP_RET_M("L2C_load_merge", packet);
        MSHR.entry[mshr_index].lq_index_depend_on_me.join(
            packet->lq_index_depend_on_me, LQ_SIZE);
        if (MSHR.entry[mshr_index].instr_id != packet->instr_id)
            MSHR.entry[mshr_index].lq_index_depend_on_me.insert(packet->lq_index);
    }
#endif
    DP_RET_M("exit_pre_update", packet);
 update_fill_cycle();

    DP_RET_AFTER(MSHR.entry[mshr_index],mshr_index);
}

void CACHE::update_fill_cycle() {
    DP( do { if (warmup_complete[cpu]) { std::cout << "[" << NAME << "_" << __func__ << "][SCHED] ENTRY num_ret=" << MSHR.num_returned << " occu=" << MSHR.occupancy << " curr_cy=" << current_core_cycle[cpu] << std::endl; } } while(0) );
    // FULL MSHR DUMP (ungated by addr/iid) — every entry, every call. Lets caller see why min isn't picked.
    DP( do { if (warmup_complete[cpu]) {
        for (uint16_t _i=0; _i<MSHR.SIZE; _i++) {
            const PACKET& _e = MSHR.entry[_i];
            if (_e.address == 0 && _e.instr_id == 0 && _e.returned == 0 && _e.event_cycle == 0) continue; // skip empty slot
            std::cout << "[" << NAME << "_" << __func__ << "][DUMP] idx=" << _i
                      << " iid=" << _e.instr_id
                      << " addr=0x" << std::hex << _e.address << std::dec
                      << " fill=" << (int)_e.fill_level
                      << " ret=" << (int)_e.returned
                      << " ev_cy=" << _e.event_cycle
                      << " type=" << (int)_e.type
                      << " cpu=" << (int)_e.cpu
                      << " l1ByP=" << (int)_e.l1_bypassed
                      << " l2ByP=" << (int)_e.l2_bypassed
                      << " llcByP=" << (int)_e.llc_bypassed
                      << " rob=" << _e.rob_index
                      << " lq=" << _e.lq_index
                      << std::endl;
        }
    } } while(0) );
    if (MSHR.num_returned == 0) {
        MSHR.next_fill_cycle = CYC_PACKED_MAX;
        MSHR.next_fill_index = MSHR.SIZE;
        DP( do { if (warmup_complete[cpu]) { std::cout << "[" << NAME << "_" << __func__ << "][SCHED] EMPTY -> nxtfi=SIZE nxtfc=MAX" << std::endl; } } while(0) );
        return;
    }
    uint64_t min_cycle = CYC_PACKED_MAX;
    uint16_t min_index = MSHR.SIZE;
    uint16_t seen = 0;
    for (uint16_t i=0; i<MSHR.SIZE && seen < MSHR.num_returned; i++) {
        if (MSHR.entry[i].returned == COMPLETED) {
            seen++;
            DP( do { if (warmup_complete[cpu]) { std::cout << "[" << NAME << "_" << __func__ << "][CAND] idx=" << i
                      << " iid=" << MSHR.entry[i].instr_id
                      << " addr=0x" << std::hex << MSHR.entry[i].address << std::dec
                      << " ev_cy=" << MSHR.entry[i].event_cycle
                      << " min_so_far=" << min_cycle
                      << " fill=" << (int)MSHR.entry[i].fill_level
                      << std::endl; } } while(0) );
            if ((min_index >= MSHR.SIZE) || PCYCLE_LT(MSHR.entry[i].event_cycle, min_cycle)) {
                min_cycle = MSHR.entry[i].event_cycle;
                min_index = i;
            }
        }
        // DP( if ((current_core_cycle[cpu] > 58318781) || warmup_complete[MSHR.entry[i].cpu]) {
        // DP( if (warmup_complete[MSHR.entry[i].cpu]) {
        // cout << "[" << NAME << "_MSHR] " <<  __func__ << " checking instr_id: " << MSHR.entry[i].instr_id;
        // cout << " addr: " << hex << MSHR.entry[i].address << " full_addr: " << MSHR.entry[i].full_addr;
        // cout << " data: " << MSHR.entry[i].data << dec << " returned: " << +MSHR.entry[i].returned << " fill_level: " << (int) MSHR.entry[i].fill_level;
        // cout << " idx: " << i << " occup: " << MSHR.occupancy;
        // cout << " event: " << MSHR.entry[i].event_cycle << " curr: " << current_core_cycle[MSHR.entry[i].cpu] << " next: " << MSHR.next_fill_cycle;
        // cout << " MSHR ByP: " << (int) MSHR.entry[i].l1_bypassed << " type: " << (int) MSHR.entry[i].type << endl; });
    }
    MSHR.next_fill_cycle = min_cycle;
    MSHR.next_fill_index = min_index;
    if (min_index < MSHR.SIZE) {
        DP( do { if (warmup_complete[cpu]) { std::cout << "[" << NAME << "_" << __func__ << "][WINNER] idx=" << min_index
                  << " iid=" << MSHR.entry[min_index].instr_id
                  << " addr=0x" << std::hex << MSHR.entry[min_index].address << std::dec
                  << " nxtfc=" << min_cycle
                  << " fill=" << (int)MSHR.entry[min_index].fill_level
                  << " curr_cy=" << current_core_cycle[cpu]
                  << std::endl; } } while(0) );
        DP_FILL_M(("SCHED_WINNER idx=" + std::to_string(min_index) + " nxtfc=" + std::to_string(min_cycle)).c_str(), &MSHR.entry[min_index]);
        // DP( if ((current_core_cycle[cpu] > 58318781) || warmup_complete[MSHR.entry[min_index].cpu]) {
        // DP( if (warmup_complete[MSHR.entry[min_index].cpu]) {
        // cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << MSHR.entry[min_index].instr_id;
        // cout << " addr: " << hex << MSHR.entry[min_index].address << " full_addr: " << MSHR.entry[min_index].full_addr;
        // cout << " data: " << MSHR.entry[min_index].data << dec << " num_returned: " << MSHR.num_returned;
        // cout << " event: " << MSHR.entry[min_index].event_cycle << " curr: " << current_core_cycle[MSHR.entry[min_index].cpu] << " next: " << MSHR.next_fill_cycle;
        // cout << " MSHR ByP: " << (int) MSHR.entry[min_index].l1_bypassed << " type: " << (int) MSHR.entry[min_index].type  << endl; });
    }
}

int CACHE::probe_mshr(PACKET *packet) const {
    for (uint16_t index = 0; index < MSHR_SIZE; index++) {
        if (MSHR.entry[index].address == packet->address)
            return index;
    }
    return -1;
}

int CACHE::check_mshr(const PACKET *packet) {
    bloom_check_total++;
    // Bloom filter early exit: guaranteed no false negatives
    if (!bloom[BLOOM_QTYPE_MSHR].maybe_contains(packet->address)) {
        bloom_reject++;
        return -1;
    }

    // Branchless MSHR scan: no branches in hot loop, arithmetic early exit
    const uint64_t target = packet->address;
    int32_t found = -1;
    for (uint16_t index = 0; index < MSHR_SIZE; index++) {
#ifdef BYPASS_L1_LOGIC_EQUIVALENCY_ON_ADDR_AND_BYPASS
        int32_t mask = -((MSHR.entry[index].address == target) & (MSHR.entry[index].l1_bypassed == packet->l1_bypassed));
#else
        int32_t mask = -(MSHR.entry[index].address == target);
#endif
        found = ((int32_t)index & mask) | (found & ~mask);
        index += ((MSHR_SIZE - index - 1) & mask);
    }
    if (found >= 0) bloom_pass_hit++; else bloom_pass_miss++;

    if (found >= 0) {
        // LPM counter: snapshot old bypass state + cpu before merge
        bool _old_byp = false;
        uint32_t _old_cpu = MSHR.entry[found].cpu;
#ifdef BYPASS_L1_LOGIC
        if (cache_type == IS_L2C && MSHR.entry[found].l1_bypassed) _old_byp = true;
#endif
#ifdef BYPASS_L2_LOGIC
        if (cache_type == IS_LLC && MSHR.entry[found].l2_bypassed) _old_byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
        if (cache_type == IS_LLC && MSHR.entry[found].llc_bypassed) _old_byp = true;
#endif

        // step 1: always promote fill_level (vanilla L569-570 / v10 handle_read_miss_inflight_fill_level)
        if (packet->fill_level < MSHR.entry[found].fill_level)
            MSHR.entry[found].fill_level = packet->fill_level;

        // step 2: PF→demand takeover (v10 merge_with_prefetch + handle_read_miss_inflight_prefetch_takeover)
        if (MSHR.entry[found].type == PREFETCH && packet->type != PREFETCH) {
            pf_late++;
            uint8_t  prior_returned    = MSHR.entry[found].returned;
            uint64_t prior_event_cycle = MSHR.entry[found].event_cycle;
            uint8_t  prior_fill        = MSHR.entry[found].fill_level;
#ifdef BYPASS_L1_LOGIC
            uint8_t  prior_l1_bypassed = MSHR.entry[found].l1_bypassed;
#endif
#ifdef BYPASS_L2_LOGIC
            uint8_t  prior_l2_bypassed = MSHR.entry[found].l2_bypassed;
#endif
#ifdef BYPASS_LLC_LOGIC
            uint8_t  prior_llc_bypassed = MSHR.entry[found].llc_bypassed;
#endif
            uint8_t  prior_pf_merged   = MSHR.entry[found].pf_merged_from_upper;

            MSHR.entry[found] = *packet;

            MSHR.entry[found].returned    = prior_returned;
            MSHR.entry[found].event_cycle = prior_event_cycle;
            MSHR.entry[found].fill_level  = (packet->fill_level < prior_fill) ? packet->fill_level : prior_fill;
#ifdef BYPASS_L1_LOGIC
            MSHR.entry[found].l1_bypassed = prior_l1_bypassed;
            if (packet->l1_bypassed) MSHR.entry[found].l1_bypassed = 1;
#endif
#ifdef BYPASS_L2_LOGIC
            MSHR.entry[found].l2_bypassed = prior_l2_bypassed;
            if (packet->l2_bypassed) MSHR.entry[found].l2_bypassed = 1;
#endif
#ifdef BYPASS_LLC_LOGIC
            MSHR.entry[found].llc_bypassed = prior_llc_bypassed;
            if (packet->llc_bypassed && !prior_llc_bypassed && cache_type == IS_LLC) {
                // I3 fix: demand with llc_bypassed merging into PF MSHR at LLC.
                // Do NOT propagate llc_bypassed — this MSHR has a pending DRAM
                // request that must return through LLC handle_fill to clean up.
                // Clear the bypass; adopt demand's fill_level so data routes up.
                MSHR.entry[found].llc_bypassed = 0;
                MSHR.entry[found].fill_level = packet->fill_level;
            } else if (packet->llc_bypassed) {
                MSHR.entry[found].llc_bypassed = 1;
            }
#endif
            MSHR.entry[found].pf_merged_from_upper = prior_pf_merged;
        }

        // step 3: bypass-flag clear when demand says NOT bypass (preserves prior behavior)
#ifdef BYPASS_L1_LOGIC
        if (MSHR.entry[found].l1_bypassed == 1 && packet->l1_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l1_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif
#ifdef BYPASS_L2_LOGIC
        if (MSHR.entry[found].l2_bypassed == 1 && packet->l2_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l2_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif
#ifdef BYPASS_LLC_LOGIC
        if (MSHR.entry[found].llc_bypassed == 1 && packet->llc_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].llc_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif

        // LPM counter: compute new bypass state, adjust counters for bypass/cpu changes
        {
            bool _new_byp = false;
            uint32_t _new_cpu = MSHR.entry[found].cpu;
#ifdef BYPASS_L1_LOGIC
            if (cache_type == IS_L2C && MSHR.entry[found].l1_bypassed) _new_byp = true;
#endif
#ifdef BYPASS_L2_LOGIC
            if (cache_type == IS_LLC && MSHR.entry[found].l2_bypassed) _new_byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
            if (cache_type == IS_LLC && MSHR.entry[found].llc_bypassed) _new_byp = true;
#endif
            if (_old_byp && !_new_byp) { lpm_mshr_byp_count--; lpm_mshr_byp_per_cpu[_old_cpu]--; }
            if (!_old_byp && _new_byp) { lpm_mshr_byp_count++; lpm_mshr_byp_per_cpu[_new_cpu]++; }
            if (_old_cpu != _new_cpu) {
                lpm_mshr_occ_per_cpu[_old_cpu]--;
                lpm_mshr_occ_per_cpu[_new_cpu]++;
                if (MSHR.entry[found].returned == INFLIGHT) {
                    lpm_mshr_inflight_per_cpu[_old_cpu]--;
                    lpm_mshr_inflight_per_cpu[_new_cpu]++;
                }
                if (_old_byp && _new_byp) {
                    lpm_mshr_byp_per_cpu[_old_cpu]--;
                    lpm_mshr_byp_per_cpu[_new_cpu]++;
                }
            }
        }

        DP_MSHR_MERGE(packet,MSHR.entry[found],found);
    } else {
        DP_MSHR_NEW_ADDR(packet);
    }
    return found;
}

#ifdef USE_LLC_HASHMAP_MSHR
int CACHE::check_mshr_hashmap(PACKET *packet) {
    int32_t found = mshr_map.find(packet->address);
    if (found < 0) return -1;

    if (found >= 0) {
        // LPM counter: snapshot old bypass state + cpu before merge (hashmap path, LLC only)
        bool _old_byp = false;
        uint32_t _old_cpu = MSHR.entry[found].cpu;
#ifdef BYPASS_L2_LOGIC
        if (MSHR.entry[found].l2_bypassed) _old_byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
        if (MSHR.entry[found].llc_bypassed) _old_byp = true;
#endif

        if (packet->fill_level < MSHR.entry[found].fill_level)
            MSHR.entry[found].fill_level = packet->fill_level;

        if (MSHR.entry[found].type == PREFETCH && packet->type != PREFETCH) {
            pf_late++;
            uint8_t  prior_returned    = MSHR.entry[found].returned;
            uint64_t prior_event_cycle = MSHR.entry[found].event_cycle;
            uint8_t  prior_fill        = MSHR.entry[found].fill_level;
#ifdef BYPASS_L1_LOGIC
            uint8_t  prior_l1_bypassed = MSHR.entry[found].l1_bypassed;
#endif
#ifdef BYPASS_L2_LOGIC
            uint8_t  prior_l2_bypassed = MSHR.entry[found].l2_bypassed;
#endif
#ifdef BYPASS_LLC_LOGIC
            uint8_t  prior_llc_bypassed = MSHR.entry[found].llc_bypassed;
#endif
            uint8_t  prior_pf_merged   = MSHR.entry[found].pf_merged_from_upper;

            MSHR.entry[found] = *packet;

            MSHR.entry[found].returned    = prior_returned;
            MSHR.entry[found].event_cycle = prior_event_cycle;
            MSHR.entry[found].fill_level  = (packet->fill_level < prior_fill) ? packet->fill_level : prior_fill;
#ifdef BYPASS_L1_LOGIC
            MSHR.entry[found].l1_bypassed = prior_l1_bypassed;
            if (packet->l1_bypassed) MSHR.entry[found].l1_bypassed = 1;
#endif
#ifdef BYPASS_L2_LOGIC
            MSHR.entry[found].l2_bypassed = prior_l2_bypassed;
            if (packet->l2_bypassed) MSHR.entry[found].l2_bypassed = 1;
#endif
#ifdef BYPASS_LLC_LOGIC
            MSHR.entry[found].llc_bypassed = prior_llc_bypassed;
            if (packet->llc_bypassed && !prior_llc_bypassed && cache_type == IS_LLC) {
                MSHR.entry[found].llc_bypassed = 0;
                MSHR.entry[found].fill_level = packet->fill_level;
            } else if (packet->llc_bypassed) {
                MSHR.entry[found].llc_bypassed = 1;
            }
#endif
            MSHR.entry[found].pf_merged_from_upper = prior_pf_merged;
        }

#ifdef BYPASS_L1_LOGIC
        if (MSHR.entry[found].l1_bypassed == 1 && packet->l1_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l1_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif
#ifdef BYPASS_L2_LOGIC
        if (MSHR.entry[found].l2_bypassed == 1 && packet->l2_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l2_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif
#ifdef BYPASS_LLC_LOGIC
        if (MSHR.entry[found].llc_bypassed == 1 && packet->llc_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].llc_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
#endif

        // LPM counter: compute new bypass state, adjust counters (hashmap path, LLC only)
        {
            bool _new_byp = false;
            uint32_t _new_cpu = MSHR.entry[found].cpu;
#ifdef BYPASS_L2_LOGIC
            if (MSHR.entry[found].l2_bypassed) _new_byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
            if (MSHR.entry[found].llc_bypassed) _new_byp = true;
#endif
            if (_old_byp && !_new_byp) { lpm_mshr_byp_count--; lpm_mshr_byp_per_cpu[_old_cpu]--; }
            if (!_old_byp && _new_byp) { lpm_mshr_byp_count++; lpm_mshr_byp_per_cpu[_new_cpu]++; }
            if (_old_cpu != _new_cpu) {
                lpm_mshr_occ_per_cpu[_old_cpu]--;
                lpm_mshr_occ_per_cpu[_new_cpu]++;
                if (MSHR.entry[found].returned == INFLIGHT) {
                    lpm_mshr_inflight_per_cpu[_old_cpu]--;
                    lpm_mshr_inflight_per_cpu[_new_cpu]++;
                }
                if (_old_byp && _new_byp) {
                    lpm_mshr_byp_per_cpu[_old_cpu]--;
                    lpm_mshr_byp_per_cpu[_new_cpu]++;
                }
            }
        }

        DP_MSHR_MERGE(packet,MSHR.entry[found],found);
    } else {
        DP_MSHR_NEW_ADDR(packet);
    }
    return found;
}
#endif

inline void CACHE::add_mshr(PACKET *packet) {
    uint16_t index = 0;
    packet->cycle_enqueued = PACK_CYCLE(current_core_cycle[packet->cpu]);
    // search mshr
    for (index=0; index<MSHR_SIZE; index++) {
        if (MSHR.entry[index].address == 0) {
            MSHR.entry[index]= std::move(*packet);
            // MSHR.entry[index]= *packet.fast;
            // packet->fast_copy_packet(MSHR.entry[index], *packet);
            MSHR.entry[index].returned = INFLIGHT;
#ifdef USE_LLC_HASHMAP_MSHR
            if (cache_type == IS_LLC)
                mshr_map.insert(MSHR.entry[index].address, index);
            else
#endif
                bloom[BLOOM_QTYPE_MSHR].insert(MSHR.entry[index].address);

            MSHR.occupancy++;

            // LPM counter: new entry is INFLIGHT
            lpm_mshr_inflight_count++;
            lpm_mshr_inflight_per_cpu[packet->cpu]++;
            lpm_mshr_occ_per_cpu[packet->cpu]++;
            // LPM counter: check bypass flag for this cache level
            {
                bool _byp = false;
#ifdef BYPASS_L1_LOGIC
                if (cache_type == IS_L2C && MSHR.entry[index].l1_bypassed) _byp = true;
#endif
#ifdef BYPASS_L2_LOGIC
                if (cache_type == IS_LLC && MSHR.entry[index].l2_bypassed) _byp = true;
#endif
#ifdef BYPASS_LLC_LOGIC
                if (cache_type == IS_LLC && MSHR.entry[index].llc_bypassed) _byp = true;
#endif
                if (_byp) { lpm_mshr_byp_count++; lpm_mshr_byp_per_cpu[packet->cpu]++; }
            }

            DP_MSHR_ADD(packet,index);
            break;
        }
        DP( do { if (PCYCLE_LT(PACK_CYCLE(MSHR.entry[index].event_cycle+10000), PACK_CYCLE(current_core_cycle[packet->cpu])) && MSHR.entry[index].l1_bypassed == 1
                && DP_GATE_WW(current_core_cycle[packet->cpu],packet->cpu,MSHR.entry[index].address,MSHR.entry[index].instr_id)) {
            cout << " MSHR NOT RESOLVED FOR BYPASS lvl:" << NAME << " addr:" << hex << MSHR.entry[index].address << dec << " iid:" << MSHR.entry[index].instr_id << endl;
        } } while(0) );

    }
    SANITY_MSHR_EMPTY_FOUND(__func__, packet, index, MSHR_SIZE);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address) const {
    switch (queue_type) {
        case 0: return MSHR.occupancy;
        case 1: return RQ.occupancy;
        case 2: return WQ.occupancy;
        case 3: return PQ.occupancy;
        default: return 0;
    }
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address) const
{
    switch (queue_type) {
        case 0: return MSHR.SIZE;
        case 1: return RQ.SIZE;
        case 2: return WQ.SIZE;
        case 3: return PQ.SIZE;
        default: return 0; // if guaranteed 0-3 range
    }
}

void CACHE::increment_WQ_FULL(uint64_t address)
{
    WQ.FULL++;
}

void CACHE::prefetcher_feedback(uint64_t &pref_gen, uint64_t &pref_fill, uint64_t &pref_used, uint64_t &pref_late)
{
    pref_gen = pf_issued;
    pref_fill = pf_fill;
    pref_used = pf_useful;
    pref_late = pf_late;   
}