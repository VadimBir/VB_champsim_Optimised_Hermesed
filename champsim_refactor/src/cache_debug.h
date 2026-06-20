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
