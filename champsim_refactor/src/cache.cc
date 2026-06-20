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

#include "cache_helper.cc"

// ---- Composite body-macros (built on IF_BYP_* wrappers from champsim.h) ----
// Live in cache.cc because they reference file-local globals (g_l1_byplat[] etc.).
// BYPLAT_ON_FILL unites the byte-identical byplat-on-fill triplet. Expands to the
// same tokens as the old per-level #ifdef blocks — zero codegen change.
#define BYPLAT_ON_FILL(ct, cpu, blk) do { \
    IF_BYP_L1(  if ((ct) == IS_L1D)  g_l1_byplat[cpu].on_fill(blk);  ) \
    IF_BYP_L2(  if ((ct) == IS_L2C)  g_l2_byplat[cpu].on_fill(blk);  ) \
    IF_BYP_LLC( if ((ct) == IS_LLC)  g_llc_byplat[cpu].on_fill(blk); ) \
} while (0)

void CACHE::set_force_all_hits(bool toEnable) {
    FORCE_ALL_HITS = toEnable;
}

#define CACHE_LVL_BASE IS_L1D
#define get_cache_lvl_bit(curr_cache_type)   (1u << ((curr_cache_type) - CACHE_LVL_BASE))
#define set_this_lvl_existance(packet_exist_lvls, cache_type) ((packet_exist_lvls) |= get_cache_lvl_bit(cache_type))
#define check_upper_has_entry(packet_exist_lvls, cache_type) (((packet_exist_lvls) & ((1u << ((cache_type) - CACHE_LVL_BASE)) - 1)) != 0)






#ifdef BYPASS_DEBUG
std::vector<CACHE*> ALL_CACHES;
#endif

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

        DP_HF_LLC_ENTRY(mshr_index, fill_cpu);

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
                IF_BYP_L2(  if (MSHR.entry[mshr_index].l2_bypassed) _byp = true;  )
                IF_BYP_LLC( if (MSHR.entry[mshr_index].llc_bypassed) _byp = true; )
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

            IF_HERMES(
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
            )

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
    IF_BYP_L1(
    uint8_t prior_fill = mshr_packet.fill_level;
    uint8_t prior_l1_bypassed = mshr_packet.l1_bypassed;
    )
    IF_BYP_L2(  uint8_t prior_l2_bypassed = mshr_packet.l2_bypassed;  )
    IF_BYP_LLC( uint8_t prior_llc_bypassed = mshr_packet.llc_bypassed; )
    uint8_t prior_pf_merged = mshr_packet.pf_merged_from_upper;
    mshr_packet = queue_packet;
    // restore original retained data
    mshr_packet.returned = prior_returned;
    mshr_packet.event_cycle = prior_event_cycle;
    IF_BYP_L1(
    mshr_packet.fill_level = (prior_fill < queue_packet.fill_level) ? prior_fill : queue_packet.fill_level;
    mshr_packet.l1_bypassed = prior_l1_bypassed;
    )
    IF_BYP_L2(  mshr_packet.l2_bypassed = prior_l2_bypassed;  )
    IF_BYP_LLC( mshr_packet.llc_bypassed = prior_llc_bypassed; )
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

                IF_LLC_BYPASS(
                if ((cache_type == IS_LLC) && (way2 == LLC_WAY)) {
                    cerr << "LLC bypassing for writebacks is not allowed!" << endl;
                    assert(0);
                }
                )

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
    IF_BYP_L1_ONNEWMISS(
    if (warmup_complete[cpu] && cache_type == IS_L1D && RQ.entry[index].type == LOAD
            && mshr_index == -1 && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (l1d_bypass_operate(read_cpu, (CACHE*)this, (CACHE*)lower_level, (CACHE*)lower_level->lower_level))) {
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
    )
    IF_BYP_L2(
    if (warmup_complete[cpu] && cache_type == IS_L2C && RQ.entry[index].type == LOAD
            && !RQ.entry[index].instruction && mshr_index == -1
            && RQ.entry[index].l1_bypassed == 0
            && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (l2c_bypass_operate(read_cpu, (CACHE*)upper_level_dcache[read_cpu], (CACHE*)this, (CACHE*)lower_level))) {
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
    )
    IF_BYP_LLC(
    if (warmup_complete[cpu] && cache_type == IS_LLC && RQ.entry[index].type == LOAD
            && mshr_index == -1 && RQ.entry[index].l2_bypassed == 0
            && lower_level->get_occupancy(1, RQ.entry[index].address) < lower_level->get_size(1, RQ.entry[index].address)
            && (llc_bypass_operate(read_cpu, (CACHE*)lower_level->lower_level, (CACHE*)lower_level, (CACHE*)this))) {
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
    )
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
                        IF_BYP_L1(
                        else if (MSHR.entry[mshr_index].l1_bypassed == 1) {
                            // Prefetcher predicted data needed at L1 — clear bypass.
                            // Normal return path: L2C fill → return_to_upper_level → L1D return_data
                            // promotes prefetch MSHR to LOAD, propagates deps. No zombie.
                            MSHR.entry[mshr_index].l1_bypassed = 0;
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        }
                        )
                        IF_BYP_L2(
                        else if (MSHR.entry[mshr_index].l2_bypassed == 1) {
                            // Cannot clear l2_bypassed here: L2C only has a PREFETCH
                            // MSHR (no promotion logic), so normal return would fill
                            // L2C but never forward to L1D.  Keep bypass active and
                            // tag for MSHR cleanup on the LLC→L1D direct return path.
                            MSHR.entry[mshr_index].pf_merged_from_upper = 1;
                        }
                        )
                        }
                        IF_BYP_LLC(
                        if (MSHR.entry[mshr_index].llc_bypassed == 1) {
                            MSHR.entry[mshr_index].llc_bypassed = 0;
                            MSHR.entry[mshr_index].fill_level = PQ.entry[index].fill_level;
                        }
                        )
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
    DP_OPERATE_RQ_HEAD();

    /* ---- Fully idle: no queue work AND no MSHR activity for LPM ---- */
    /* When truly idle (ha=0,ma=0,has_byp=0), all LPM cycles are CY_E.
     * Buffer them and flush in bulk via advance_idle() when work arrives. */
#ifdef LPM_STRICT_MISS
    bool _lpm_has_mshr = (lpm_mshr_inflight_count > 0) | (lpm_mshr_byp_count > 0);
#else
    bool _lpm_has_mshr = (MSHR.occupancy > 0) | (lpm_mshr_byp_count > 0);
#endif
    if (__builtin_expect(has_work == 0 && !_lpm_has_mshr, 1)) {
        if (warmup_complete[cpu]) cache_idle_accumulator++;
        return;
    }

    /* ---- Flush accumulated idle cycles into LPM trackers ---- */
    if (warmup_complete[cpu] && cache_idle_accumulator > 0) {
        uint32_t _idle_n = cache_idle_accumulator;
        cache_idle_accumulator = 0;
        if (cache_type == IS_LLC) {
            // NOTE: #ifdef TRACKER_LPM_SHARED collapsed — both arms were byte-identical.
            for (uint32_t c = 0; c < NUM_CPUS; c++) {
                if (!warmup_complete[c]) continue;
                lpm[c][cache_type].advance_idle(_idle_n);
            }
        } else {
            lpm[cpu][cache_type].advance_idle(_idle_n);
        }
    }

    /* ---- LPM tick for this active cycle ---- */
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
                lpm_operate(lpm[c][cache_type], c, cache_type, hit_active, miss_active, α_c, has_byp, load_α_c, load_miss_c);
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
                lpm_operate(lpm[c][cache_type], c, cache_type, hit_active_c, miss_active_pc[c], α_c, has_byp_pc[c], load_α_c, load_miss_c);
            }
#endif
        } else {
            lpm_operate(lpm[cpu][cache_type], cpu, cache_type, hit_active, miss_active,
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

    BYPLAT_ON_FILL(cache_type, packet->cpu, packet->address >> LOG2_BLOCK_SIZE);

    IF_HERMES(
    if (cache_type == IS_LLC && block[set][way].valid)
        ooo_cpu[packet->cpu].offchip_predictor_track_llc_eviction(set, way, block[set][way].full_addr);
    )

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

int CACHE::check_hit(PACKET *packet)
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
    // D5: instruction/fetch packets (L1I/ITLB) never enter any cache WQ
    // (proven: all WQ writers use RFO/WRITEBACK packets with instruction==0),
    // so check_queue would always return not-found (-1) for them. Skip it.
    int wq_index = (packet->instruction == 1) ? -1 : WQ.check_queue(packet);
   // check if WQ packet has ByPass and new packet not


    DP_ADDRQ_BYP_WQ_HIT(packet, wq_index);

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
        IF_BYP_L1(
        if ((cache_type == IS_L2C) && (packet->type == LOAD) && packet->l1_bypassed == 1 && !packet->instruction) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
    DP_ADDRQ_L2C_PROCESSED_ADD(packet);
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
        )
        IF_BYP_L2(
        if ((cache_type == IS_LLC) && (packet->type == LOAD) && packet->l2_bypassed == 1 && !packet->instruction) {
            // if (PROCESSED.occupancy < PROCESSED.SIZE) {
                upper_level_dcache[packet->cpu]->upper_level_dcache[packet->cpu]->return_data(packet);
            g_l2_byplat[packet->cpu].on_fill(packet->address >> LOG2_BLOCK_SIZE);
                // PROCESSED.add_queue(packet);
            // }
            DP_RQ_HIT(packet,"WQ_HIT_L2BYP");
        }
        )
        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        RQ.ACCESS++;

        return -1;
    }


    // check for duplicates in the read queue
    int index = RQ.check_queue(packet);
    DP_ADDRQ_BYP_MISMATCH(packet, index);
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
                IF_BYP_L1(
                if (cache_type == IS_L2C) {
                    if (RQ.entry[index].l1_bypassed != packet->l1_bypassed) {
                        RQ.entry[index].l1_bypassed = 0;
                        // packet->l1_bypassed already 0 for RFO
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
                )
                IF_BYP_L2(
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].l2_bypassed != packet->l2_bypassed) {
                        RQ.entry[index].l2_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
                )
                IF_BYP_LLC(
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].llc_bypassed != packet->llc_bypassed) {
                        RQ.entry[index].llc_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
                )
                DP_RQ_MERGE(packet,RQ.entry[index],"RFO");

            } 
            else {
                uint16_t lq_index = packet->lq_index;
                // RQ.entry[index].lq_index_depend_on_me.insert (lq_index);
                RQ.entry[index].lq_index_depend_on_me.insert(lq_index);
                RQ.entry[index].lq_index_depend_on_me.join(packet->lq_index_depend_on_me, LQ_SIZE);  // ← ADD
                RQ.entry[index].load_merged = 1;
                IF_BYP_L2(
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].l2_bypassed != packet->l2_bypassed) {
                        RQ.entry[index].l2_bypassed = 0;
                        packet->l2_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
                )
                IF_BYP_LLC(
                if (cache_type == IS_LLC) {
                    if (RQ.entry[index].llc_bypassed != packet->llc_bypassed) {
                        RQ.entry[index].llc_bypassed = 0;
                        packet->llc_bypassed = 0;
                        if (packet->fill_level < RQ.entry[index].fill_level)
                            RQ.entry[index].fill_level = packet->fill_level;
                    }
                }
                )
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
    DP_RETDATA_LLC_IN(packet);
    // check MSHR information
    int mshr_index = CHECK_MSHR(packet);
    DP_RETDATA_LLC_CHECK_MSHR(packet, mshr_index);
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
    IF_BYP_L1(
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
    )
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
   IF_BYP_L2(
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
    )
    DP_RET_M("exit_pre_update", packet);
 update_fill_cycle();

    DP_RET_AFTER(MSHR.entry[mshr_index],mshr_index);
}

void CACHE::update_fill_cycle() {
    DP_UFC_SCHED_ENTRY();
    // FULL MSHR DUMP (ungated by addr/iid) — every entry, every call. Lets caller see why min isn't picked.
    DP_UFC_DUMP();
    if (MSHR.num_returned == 0) {
        MSHR.next_fill_cycle = CYC_PACKED_MAX;
        MSHR.next_fill_index = MSHR.SIZE;
        DP_UFC_SCHED_EMPTY();
        return;
    }
    uint64_t min_cycle = CYC_PACKED_MAX;
    uint16_t min_index = MSHR.SIZE;
    uint16_t seen = 0;
    for (uint16_t i=0; i<MSHR.SIZE && seen < MSHR.num_returned; i++) {
        if (MSHR.entry[i].returned == COMPLETED) {
            seen++;
            DP_UFC_CAND(i, min_cycle);
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
        DP_UFC_WINNER(min_index, min_cycle);
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

int CACHE::check_mshr(PACKET *packet) {
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
        IF_BYP_L1(  if (cache_type == IS_L2C && MSHR.entry[found].l1_bypassed) _old_byp = true;  )
        IF_BYP_L2(  if (cache_type == IS_LLC && MSHR.entry[found].l2_bypassed) _old_byp = true;  )
        IF_BYP_LLC( if (cache_type == IS_LLC && MSHR.entry[found].llc_bypassed) _old_byp = true; )

        // step 1: always promote fill_level (vanilla L569-570 / v10 handle_read_miss_inflight_fill_level)
        if (packet->fill_level < MSHR.entry[found].fill_level)
            MSHR.entry[found].fill_level = packet->fill_level;

        // step 2: PF→demand takeover (v10 merge_with_prefetch + handle_read_miss_inflight_prefetch_takeover)
        if (MSHR.entry[found].type == PREFETCH && packet->type != PREFETCH) {
            pf_late++;
            uint8_t  prior_returned    = MSHR.entry[found].returned;
            uint64_t prior_event_cycle = MSHR.entry[found].event_cycle;
            uint8_t  prior_fill        = MSHR.entry[found].fill_level;
            IF_BYP_L1(  uint8_t  prior_l1_bypassed = MSHR.entry[found].l1_bypassed;  )
            IF_BYP_L2(  uint8_t  prior_l2_bypassed = MSHR.entry[found].l2_bypassed;  )
            IF_BYP_LLC( uint8_t  prior_llc_bypassed = MSHR.entry[found].llc_bypassed; )
            uint8_t  prior_pf_merged   = MSHR.entry[found].pf_merged_from_upper;

            MSHR.entry[found] = *packet;

            MSHR.entry[found].returned    = prior_returned;
            MSHR.entry[found].event_cycle = prior_event_cycle;
            MSHR.entry[found].fill_level  = (packet->fill_level < prior_fill) ? packet->fill_level : prior_fill;
            IF_BYP_L1(
            MSHR.entry[found].l1_bypassed = prior_l1_bypassed;
            if (packet->l1_bypassed) MSHR.entry[found].l1_bypassed = 1;
            )
            IF_BYP_L2(
            MSHR.entry[found].l2_bypassed = prior_l2_bypassed;
            if (packet->l2_bypassed) MSHR.entry[found].l2_bypassed = 1;
            )
            IF_BYP_LLC(
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
            )
            MSHR.entry[found].pf_merged_from_upper = prior_pf_merged;
        }

        // step 3: bypass-flag clear when demand says NOT bypass (preserves prior behavior)
        IF_BYP_L1(
        if (MSHR.entry[found].l1_bypassed == 1 && packet->l1_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l1_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )
        IF_BYP_L2(
        if (MSHR.entry[found].l2_bypassed == 1 && packet->l2_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l2_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )
        IF_BYP_LLC(
        if (MSHR.entry[found].llc_bypassed == 1 && packet->llc_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].llc_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )

        // LPM counter: compute new bypass state, adjust counters for bypass/cpu changes
        {
            bool _new_byp = false;
            uint32_t _new_cpu = MSHR.entry[found].cpu;
            IF_BYP_L1(  if (cache_type == IS_L2C && MSHR.entry[found].l1_bypassed) _new_byp = true;  )
            IF_BYP_L2(  if (cache_type == IS_LLC && MSHR.entry[found].l2_bypassed) _new_byp = true;  )
            IF_BYP_LLC( if (cache_type == IS_LLC && MSHR.entry[found].llc_bypassed) _new_byp = true; )
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
        IF_BYP_L2(  if (MSHR.entry[found].l2_bypassed) _old_byp = true;  )
        IF_BYP_LLC( if (MSHR.entry[found].llc_bypassed) _old_byp = true; )

        if (packet->fill_level < MSHR.entry[found].fill_level)
            MSHR.entry[found].fill_level = packet->fill_level;

        if (MSHR.entry[found].type == PREFETCH && packet->type != PREFETCH) {
            pf_late++;
            uint8_t  prior_returned    = MSHR.entry[found].returned;
            uint64_t prior_event_cycle = MSHR.entry[found].event_cycle;
            uint8_t  prior_fill        = MSHR.entry[found].fill_level;
            IF_BYP_L1(  uint8_t  prior_l1_bypassed = MSHR.entry[found].l1_bypassed;  )
            IF_BYP_L2(  uint8_t  prior_l2_bypassed = MSHR.entry[found].l2_bypassed;  )
            IF_BYP_LLC( uint8_t  prior_llc_bypassed = MSHR.entry[found].llc_bypassed; )
            uint8_t  prior_pf_merged   = MSHR.entry[found].pf_merged_from_upper;

            MSHR.entry[found] = *packet;

            MSHR.entry[found].returned    = prior_returned;
            MSHR.entry[found].event_cycle = prior_event_cycle;
            MSHR.entry[found].fill_level  = (packet->fill_level < prior_fill) ? packet->fill_level : prior_fill;
            IF_BYP_L1(
            MSHR.entry[found].l1_bypassed = prior_l1_bypassed;
            if (packet->l1_bypassed) MSHR.entry[found].l1_bypassed = 1;
            )
            IF_BYP_L2(
            MSHR.entry[found].l2_bypassed = prior_l2_bypassed;
            if (packet->l2_bypassed) MSHR.entry[found].l2_bypassed = 1;
            )
            IF_BYP_LLC(
            MSHR.entry[found].llc_bypassed = prior_llc_bypassed;
            if (packet->llc_bypassed && !prior_llc_bypassed && cache_type == IS_LLC) {
                MSHR.entry[found].llc_bypassed = 0;
                MSHR.entry[found].fill_level = packet->fill_level;
            } else if (packet->llc_bypassed) {
                MSHR.entry[found].llc_bypassed = 1;
            }
            )
            MSHR.entry[found].pf_merged_from_upper = prior_pf_merged;
        }

        IF_BYP_L1(
        if (MSHR.entry[found].l1_bypassed == 1 && packet->l1_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l1_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )
        IF_BYP_L2(
        if (MSHR.entry[found].l2_bypassed == 1 && packet->l2_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].l2_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )
        IF_BYP_LLC(
        if (MSHR.entry[found].llc_bypassed == 1 && packet->llc_bypassed == 0 && packet->type == LOAD) {
            MSHR.entry[found].llc_bypassed = 0;
            MSHR.entry[found].fill_level = packet->fill_level;
        }
        )

        // LPM counter: compute new bypass state, adjust counters (hashmap path, LLC only)
        {
            bool _new_byp = false;
            uint32_t _new_cpu = MSHR.entry[found].cpu;
            IF_BYP_L2(  if (MSHR.entry[found].l2_bypassed) _new_byp = true;  )
            IF_BYP_LLC( if (MSHR.entry[found].llc_bypassed) _new_byp = true; )
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
                IF_BYP_L1(  if (cache_type == IS_L2C && MSHR.entry[index].l1_bypassed) _byp = true;  )
                IF_BYP_L2(  if (cache_type == IS_LLC && MSHR.entry[index].l2_bypassed) _byp = true;  )
                IF_BYP_LLC( if (cache_type == IS_LLC && MSHR.entry[index].llc_bypassed) _byp = true; )
                if (_byp) { lpm_mshr_byp_count++; lpm_mshr_byp_per_cpu[packet->cpu]++; }
            }

            DP_MSHR_ADD(packet,index);
            break;
        }
        DP_MSHR_NOT_RESOLVED_BYP(index, packet);

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

// ===== handle_* sub-helpers moved from cache.h (Phase 0) =====
inline void CACHE::return_to_upper_level(PACKET& packet) {
        DP_RET_M("RETURN_UP_ENTRY", &packet);
        if (packet.instruction) {
            DP_RET_M("RETURN_UP_to_icache", &packet);
            upper_level_icache[packet.cpu]->return_data(&packet);
        } else {
            DP_RET_M("RETURN_UP_to_dcache", &packet);
            upper_level_dcache[packet.cpu]->return_data(&packet);
        }
        DP_RET_M("RETURN_UP_DONE", &packet);
    }

inline void CACHE::handle_read_hit_processed(int index, uint32_t set, uint32_t way) {
        if (cache_type == IS_ITLB) {
            RQ.entry[index].instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "ITLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_DTLB) {
            RQ.entry[index].data_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "DTLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_STLB) {
            RQ.entry[index].data = block[set][way].data;
        }
        else if (cache_type == IS_L1I) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L1I PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if ((cache_type == IS_L1D) && (RQ.entry[index].type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L1D PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
    }

inline bool CACHE::handle_read_hit_bypass_return(uint16_t read_cpu, int index) {
        IF_BYP_L1(
        if ((cache_type == IS_L2C) && (RQ.entry[index].type == LOAD
                && RQ.entry[index].l1_bypassed == 1 && !RQ.entry[index].instruction)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE) {
                char* dst = (char*)&PROCESSED.entry[PROCESSED.tail];
                for (size_t i = 0; i < sizeof(PACKET); i += 64)
                    __builtin_prefetch(dst + i, 1, 3);
                PROCESSED.add_queue(&RQ.entry[index]);
            } else { cerr << "L2C PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
            g_l1_byplat[read_cpu].on_fill(RQ.entry[index].address >> LOG2_BLOCK_SIZE);
            DP_RET(&RQ.entry[index],"L1BYP_HIT");
            return true;
        }
        )
        IF_BYP_L2(
        if ((cache_type == IS_LLC) && (RQ.entry[index].type == LOAD
                && RQ.entry[index].l2_bypassed == 1 && !RQ.entry[index].instruction)) {
            upper_level_dcache[read_cpu]->upper_level_dcache[read_cpu]->return_data(&RQ.entry[index]);
            g_l2_byplat[read_cpu].on_fill(RQ.entry[index].address >> LOG2_BLOCK_SIZE);
            DP_RET(&RQ.entry[index],"L2BYP_HIT");
            return true;
        }
        )
        return false;
    }

inline void CACHE::handle_read_hit_pf_operate(uint16_t read_cpu, int index, uint32_t set, uint32_t way) {
        if (RQ.entry[index].type == LOAD) {
            if (cache_type == IS_L1D)
                l1d_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 1, RQ.entry[index].type);
            else if (cache_type == IS_L2C)
                l2c_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
            else if (cache_type == IS_LLC) {
                cpu = read_cpu;
                llc_prefetcher_operate(block[set][way].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 1, RQ.entry[index].type, 0);
                cpu = 0;
            }
        }
    }

inline void CACHE::handle_read_hit_replacement(uint16_t read_cpu, int index, uint32_t set, uint32_t way) {
        if (cache_type == IS_LLC)
            llc_update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1);
        else
            update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type, 1);
    }

inline void CACHE::handle_read_hit_stats(uint16_t read_cpu, int index) {
        sim_hit[read_cpu][RQ.entry[index].type]++;
        sim_access[read_cpu][RQ.entry[index].type]++;
        lpm_shadow_inc(read_cpu, RQ.entry[index].type, false);
        if (RQ.entry[index].type == LOAD) {
            sim_hit_wByP[read_cpu]++;
            sim_access_wByP[read_cpu]++;
        }
    }

inline void CACHE::handle_read_hit_return(int index) {
        bool fire = (RQ.entry[index].fill_level < fill_level);
        DP_FILL_M(("HIT_RET_CHK rq.fill=" + std::to_string(RQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level) + " fire=" + (fire?"Y":"N")).c_str(), &RQ.entry[index]);
        if (fire) {
            DP_FWD(&RQ.entry[index],"upper");
            return_to_upper_level(RQ.entry[index]);
        }
    }

inline void CACHE::handle_read_hit_pf_useful_and_remove(int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("HIT_REMOVE pf_used=" + std::string(block[set][way].prefetch?"Y":"N") + " set=" + std::to_string(set) + " way=" + std::to_string(way)).c_str(), &RQ.entry[index]);
        if (block[set][way].prefetch) {
            pf_useful++;
            block[set][way].prefetch = 0;
        }
        block[set][way].used = 1;
        HIT[RQ.entry[index].type]++;
        ACCESS[RQ.entry[index].type]++;
        RQ.remove_queue(&RQ.entry[index]);
        reads_available_this_cycle--;
    }

inline void CACHE::handle_read_miss_new_llc(int index, uint8_t& miss_handled) {
        DP_FILL_M("NEW_LLC_ENTRY", &RQ.entry[index]);
        if (lower_level->get_occupancy(1, RQ.entry[index].address) == lower_level->get_size(1, RQ.entry[index].address)) {
            DP_FILL_M("NEW_LLC_DRAM_FULL_STALL", &RQ.entry[index]);
            miss_handled = 0;
        } else {
            DP_FILL_M("NEW_LLC_ADD_MSHR", &RQ.entry[index]);
            add_mshr(&RQ.entry[index]);
            DP_MSHR_ADD(&RQ.entry[index], MSHR.occupancy-1);
            if (lower_level) {
                DP_FWD(&RQ.entry[index],"DRAM");
                lower_level->add_rq(&RQ.entry[index]);
            }
        }
    }

inline void CACHE::handle_read_miss_new_other(uint16_t read_cpu, int index, uint8_t& miss_handled) {
        DP_FILL_M("NEW_OTHER_ENTRY", &RQ.entry[index]);
        if (lower_level && lower_level->get_occupancy(1, RQ.entry[index].address) == lower_level->get_size(1, RQ.entry[index].address)) {
            DP_FILL_M("NEW_OTHER_LOWER_FULL_STALL", &RQ.entry[index]);
            miss_handled = 0;
        } else {
            DP_FILL_M("NEW_OTHER_ADD_MSHR", &RQ.entry[index]);
            add_mshr(&RQ.entry[index]);
            DP_MSHR_ADD(&RQ.entry[index], MSHR.occupancy-1);
            if (lower_level) {
                DP_FWD(&RQ.entry[index],"lower");
                lower_level->add_rq(&RQ.entry[index]);
            } else {
                if (cache_type == IS_STLB) {
                    DP_FILL_M("NEW_OTHER_STLB_VA2PA_SELFRETURN", &RQ.entry[index]);
                    uint64_t pa = va_to_pa(read_cpu, RQ.entry[index].instr_id, RQ.entry[index].full_addr, RQ.entry[index].address);
                    RQ.entry[index].data = pa >> LOG2_PAGE_SIZE;
                    RQ.entry[index].event_cycle = PACK_CYCLE(current_core_cycle[read_cpu]);
                    return_data(&RQ.entry[index]);
                }
            }
        }
    }

inline void CACHE::handle_read_miss_mshr_full(int index, uint8_t& miss_handled) {
        DP_FILL_M(("MSHR_FULL_STALL type=" + std::to_string(RQ.entry[index].type)).c_str(), &RQ.entry[index]);
        miss_handled = 0;
        STALL[RQ.entry[index].type]++;
        pure_MSHR_Admission_STALL[RQ.entry[index].type]++;
    }

inline void CACHE::handle_read_miss_inflight_merge_deps(int index, int mshr_index) {
        DP_FILL_M(("MERGE_DEPS idx=" + std::to_string(mshr_index) + " rq.type=" + std::to_string(RQ.entry[index].type) + " rq.instr=" + std::to_string(RQ.entry[index].instruction) + " rq.lq=" + std::to_string(RQ.entry[index].lq_index) + " rq.sq=" + std::to_string(RQ.entry[index].sq_index)).c_str(), &RQ.entry[index]);
        if (RQ.entry[index].type == RFO) {
            if (RQ.entry[index].l1_bypassed)
                assert(0&&"RFO BYPASS NOT EXPECTED ... ");
            if (RQ.entry[index].tlb_access) {
                uint16_t sq_index = RQ.entry[index].sq_index;
                MSHR.entry[mshr_index].store_merged = 1;
                MSHR.entry[mshr_index].sq_index_depend_on_me.insert(sq_index);
                MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
            }
            if (RQ.entry[index].load_merged) {
                MSHR.entry[mshr_index].load_merged = 1;
                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
            }
        } else {
            if (RQ.entry[index].instruction) {
                uint16_t rob_index = RQ.entry[index].rob_index;
                MSHR.entry[mshr_index].instr_merged = 1;
                MSHR.entry[mshr_index].rob_index_depend_on_me.insert(rob_index);
                if (RQ.entry[index].instr_merged)
                    MSHR.entry[mshr_index].rob_index_depend_on_me.join(RQ.entry[index].rob_index_depend_on_me, ROB_SIZE);
            } else {
                uint16_t lq_index = RQ.entry[index].lq_index;
                MSHR.entry[mshr_index].load_merged = 1;
                MSHR.entry[mshr_index].lq_index_depend_on_me.insert(lq_index);
                MSHR.entry[mshr_index].lq_index_depend_on_me.join(RQ.entry[index].lq_index_depend_on_me, LQ_SIZE);
                if (RQ.entry[index].store_merged) {
                    MSHR.entry[mshr_index].store_merged = 1;
                    MSHR.entry[mshr_index].sq_index_depend_on_me.join(RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                }
            }
        }
    }

inline void CACHE::handle_read_miss_inflight_fill_level(int index, int mshr_index) {
        uint8_t pre_fl = MSHR.entry[mshr_index].fill_level;
        if (RQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
            MSHR.entry[mshr_index].fill_level = RQ.entry[index].fill_level;
        DP_FILL_M(("INFLIGHT_FILL_LVL idx=" + std::to_string(mshr_index) + " pre=" + std::to_string(pre_fl) + " rq=" + std::to_string(RQ.entry[index].fill_level) + " post=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " promoted=" + ((pre_fl != MSHR.entry[mshr_index].fill_level)?"Y":"N")).c_str(), &MSHR.entry[mshr_index]);
    }

inline void CACHE::handle_read_miss_inflight_bypass_l1_mismatch(int index, int mshr_index) {
#ifdef BYPASS_L1_LOGIC
        if (cache_type == IS_L2C) {
            if (RQ.entry[index].l1_bypassed != MSHR.entry[mshr_index].l1_bypassed) {
                if (MSHR.entry[mshr_index].type != PREFETCH) {
                    auto *l1d = (CACHE *) this->upper_level_dcache[cpu];
                    bool found_l1d_mshr = false;
                    for (uint16_t m = 0; m < l1d->MSHR_SIZE; m++) {
                        if (l1d->MSHR.entry[m].address == MSHR.entry[mshr_index].address) {
                            found_l1d_mshr = true;
                            if (l1d->MSHR.entry[m].type == PREFETCH) {
                                auto& __m = l1d->MSHR.entry[m];
                                if (__m.lq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.lq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (__m.sq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.sq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (!__m.rob_index_depend_on_me.empty())
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.rob_index_depend_on_me.entries.size()
                                              << " **\033[0m\n" << std::flush;
                            }
                            // Only inject load deps for LOAD packets — RFO lq_index is garbage
                            if (RQ.entry[index].type != RFO) {
                                l1d->MSHR.entry[m].load_merged = 1;
                                l1d->MSHR.entry[m].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
                            }
                            if (RQ.entry[index].load_merged) {
                                l1d->MSHR.entry[m].load_merged = 1;
                                ITERATE_SET(dep, RQ.entry[index].lq_index_depend_on_me, LQ_SIZE) {
                                    l1d->MSHR.entry[m].lq_index_depend_on_me.insert(dep);
                                }
                            }
                            if (RQ.entry[index].store_merged) {
                                l1d->MSHR.entry[m].store_merged = 1;
                                l1d->MSHR.entry[m].sq_index_depend_on_me.join(
                                    RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }
                            if (l1d->MSHR.entry[m].type == LOAD)
                                l1d->MSHR.entry[m].lq_index_depend_on_me.remove(l1d->MSHR.entry[m].lq_index);
                            break;
                        }
                    }
                    if (found_l1d_mshr) {
                        RQ.entry[index].l1_bypassed = 0;
                        MSHR.entry[mshr_index].l1_bypassed = 0;
                        MSHR.entry[mshr_index].fill_level = 1;
                    }
                } else if (MSHR.entry[mshr_index].fill_level < fill_level) {
                    RQ.entry[index].l1_bypassed = 0;
                }
            }
        }
#endif
    }

inline void CACHE::handle_read_miss_inflight_bypass_l2_mismatch(int index, int mshr_index) {
#ifdef BYPASS_L2_LOGIC
        if (cache_type == IS_LLC) {
            if (RQ.entry[index].l2_bypassed != MSHR.entry[mshr_index].l2_bypassed) {
                if (MSHR.entry[mshr_index].type != PREFETCH) {
                    // [FIX] LLC shared: class member 'cpu' is stale; use packet's cpu for correct L2C target
                    auto *l2c = (CACHE *) this->upper_level_dcache[RQ.entry[index].cpu];
                    bool found_l2c_mshr = false;
                    for (uint16_t m = 0; m < l2c->MSHR_SIZE; m++) {
                        if (l2c->MSHR.entry[m].address == MSHR.entry[mshr_index].address) {
                            found_l2c_mshr = true;
                            if (l2c->MSHR.entry[m].type == PREFETCH) {
                                auto& __m = l2c->MSHR.entry[m];
                                if (__m.lq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.lq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (__m.sq_index_depend_on_me.card != 0)
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.sq_index_depend_on_me.card
                                              << " **\033[0m\n" << std::flush;
                                if (!__m.rob_index_depend_on_me.empty())
                                    std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                              << " func=" << __func__ << " cache=[" << NAME << "]"
                                              << " count=" << __m.rob_index_depend_on_me.entries.size()
                                              << " **\033[0m\n" << std::flush;
                            }
                            // Only inject load deps for LOAD packets — RFO lq_index is garbage
                            if (RQ.entry[index].type != RFO) {
                                l2c->MSHR.entry[m].load_merged = 1;
                                l2c->MSHR.entry[m].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
                            }
                            if (RQ.entry[index].load_merged) {
                                l2c->MSHR.entry[m].load_merged = 1;
                                ITERATE_SET(dep, RQ.entry[index].lq_index_depend_on_me, LQ_SIZE) {
                                    l2c->MSHR.entry[m].lq_index_depend_on_me.insert(dep);
                                }
                            }
                            if (RQ.entry[index].store_merged) {
                                l2c->MSHR.entry[m].store_merged = 1;
                                l2c->MSHR.entry[m].sq_index_depend_on_me.join(
                                    RQ.entry[index].sq_index_depend_on_me, SQ_SIZE);
                            }
                            if (l2c->MSHR.entry[m].type == LOAD)
                                l2c->MSHR.entry[m].lq_index_depend_on_me.remove(l2c->MSHR.entry[m].lq_index);
                            break;
                        }
                    }
                    if (found_l2c_mshr) {
                        RQ.entry[index].l2_bypassed = 0;
                        MSHR.entry[mshr_index].l2_bypassed = 0;
                        MSHR.entry[mshr_index].fill_level = FILL_L2;
                    } else {
                        std::cout << "\033[1;31m\n** RARE UNHANDLED CASE line=" << __LINE__
                                  << " func=" << __func__
                                  << " cache=[" << NAME << "]"
                                  << " ISSUE: L2C_MSHR_MISSING_BYPASS_MISMATCH"
                                  << " **\033[0m\n" << std::flush;
                    }
                } else if (MSHR.entry[mshr_index].fill_level < fill_level) {
                    RQ.entry[index].l2_bypassed = 0;
                }
            }
        }
#endif
    }

inline void CACHE::handle_read_miss_inflight_prefetch_takeover(int index, int mshr_index) {
        DP_FILL_M(("PF_TAKEOVER_PRE idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " rq.type=" + std::to_string(RQ.entry[index].type) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned)).c_str(), &MSHR.entry[mshr_index]);
        pf_late++;
        merge_with_prefetch(MSHR.entry[mshr_index], RQ.entry[index]);
        IF_BYP_L1(
        if (RQ.entry[index].l1_bypassed)
            MSHR.entry[mshr_index].l1_bypassed = 1;
        )
        IF_BYP_L2(
        if (RQ.entry[index].l2_bypassed)
            MSHR.entry[mshr_index].l2_bypassed = 1;
        )
        IF_BYP_LLC(
        if (RQ.entry[index].llc_bypassed)
            MSHR.entry[mshr_index].llc_bypassed = 1;
        )
        DP_FILL_M(("PF_TAKEOVER_POST idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.ret=" + std::to_string(MSHR.entry[mshr_index].returned)).c_str(), &MSHR.entry[mshr_index]);
    }

inline void CACHE::handle_read_miss_inflight_non_prefetch_merge(int index, int mshr_index) {
        DP_FILL_M(("NONPF_MERGE idx=" + std::to_string(mshr_index) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type) + " rq.type=" + std::to_string(RQ.entry[index].type) + " rq.lq=" + std::to_string(RQ.entry[index].lq_index)).c_str(), &MSHR.entry[mshr_index]);
        if (RQ.entry[index].instruction) {
            MSHR.entry[mshr_index].rob_index_depend_on_me.insert(RQ.entry[index].rob_index);
            MSHR.entry[mshr_index].instr_merged = 1;
        } else if (RQ.entry[index].type == LOAD) {
            MSHR.entry[mshr_index].lq_index_depend_on_me.insert(RQ.entry[index].lq_index);
            MSHR.entry[mshr_index].load_merged = 1;
        } else if (RQ.entry[index].type == RFO) {
            MSHR.entry[mshr_index].sq_index_depend_on_me.insert(RQ.entry[index].sq_index);
            MSHR.entry[mshr_index].store_merged = 1;
        }
    }

inline void CACHE::handle_read_miss_handled_pf_operate(uint16_t read_cpu, int index) {
        if (RQ.entry[index].type == LOAD) {
            DP_FILL_M("MISS_PF_OPERATE", &RQ.entry[index]);
            if (cache_type == IS_L1D)
                l1d_prefetcher_operate(RQ.entry[index].full_addr, RQ.entry[index].ip, 0, RQ.entry[index].type);
            if (cache_type == IS_L2C)
                l2c_prefetcher_operate(RQ.entry[index].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
            if (cache_type == IS_LLC) {
                cpu = read_cpu;
                llc_prefetcher_operate(RQ.entry[index].address<<LOG2_BLOCK_SIZE, RQ.entry[index].ip, 0, RQ.entry[index].type, 0);
                cpu = 0;
            }
        }
    }

inline void CACHE::handle_read_miss_handled_stats_remove(int index) {
        DP_FILL_M("MISS_REMOVE_RQ", &RQ.entry[index]);
        MISS[RQ.entry[index].type]++;
        ACCESS[RQ.entry[index].type]++;
        RQ.remove_queue(&RQ.entry[index]);
        reads_available_this_cycle--;
    }

inline void CACHE::handle_writeback_hit(uint16_t writeback_cpu, int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("WB_HIT set=" + std::to_string(set) + " way=" + std::to_string(way) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &WQ.entry[index]);
        if (cache_type == IS_LLC)
            llc_update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1);
        else
            update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[index].ip, 0, WQ.entry[index].type, 1);
        sim_hit[writeback_cpu][WQ.entry[index].type]++;
        sim_access[writeback_cpu][WQ.entry[index].type]++;
        lpm_shadow_inc(writeback_cpu, WQ.entry[index].type, false);
        block[set][way].dirty = 1;
        if (cache_type == IS_ITLB)
            WQ.entry[index].instruction_pa = block[set][way].data;
        else if (cache_type == IS_DTLB)
            WQ.entry[index].data_pa = block[set][way].data;
        else if (cache_type == IS_STLB)
            WQ.entry[index].data = block[set][way].data;
        if (WQ.entry[index].fill_level < fill_level) {
            DP_FILL_M("WB_HIT_RET_UP", &WQ.entry[index]);
            return_to_upper_level(WQ.entry[index]);
        }
        HIT[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

inline void CACHE::handle_writeback_miss_new(int index, uint8_t& miss_handled) {
        DP_FILL_M("WB_MISS_NEW_ENTRY", &WQ.entry[index]);
        if (cache_type == IS_LLC) {
            if (lower_level->get_occupancy(1, WQ.entry[index].address) == lower_level->get_size(1, WQ.entry[index].address)) {
                DP_FILL_M("WB_MISS_NEW_LOWER_FULL", &WQ.entry[index]);
                miss_handled = 0;
            } else {
                DP_FILL_M("WB_MISS_NEW_ADD_MSHR_RQ", &WQ.entry[index]);
                add_mshr(&WQ.entry[index]);
                lower_level->add_rq(&WQ.entry[index]);
            }
        } else {
            if (lower_level && lower_level->get_occupancy(1, WQ.entry[index].address) == lower_level->get_size(1, WQ.entry[index].address)) {
                DP_FILL_M("WB_MISS_NEW_LOWER_FULL", &WQ.entry[index]);
                miss_handled = 0;
            } else {
                DP_FILL_M("WB_MISS_NEW_ADD_MSHR_RQ", &WQ.entry[index]);
                add_mshr(&WQ.entry[index]);
                lower_level->add_rq(&WQ.entry[index]);
            }
        }
    }

inline void CACHE::handle_writeback_miss_mshr_full(int index, uint8_t& miss_handled) {
        miss_handled = 0;
        STALL[WQ.entry[index].type]++;
    }

inline void CACHE::handle_writeback_miss_inflight(int index, int mshr_index) {
        DP_FILL_M(("WB_INFLIGHT idx=" + std::to_string(mshr_index) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " mshr.type=" + std::to_string(MSHR.entry[mshr_index].type)).c_str(), &WQ.entry[index]);
        if (WQ.entry[index].fill_level < MSHR.entry[mshr_index].fill_level)
            MSHR.entry[mshr_index].fill_level = WQ.entry[index].fill_level;
        if (MSHR.entry[mshr_index].type == PREFETCH)
            merge_with_prefetch(MSHR.entry[mshr_index], WQ.entry[index]);
        MSHR_MERGED[WQ.entry[index].type]++;
        DP_FILL_M(("WB_INFLIGHT_POST idx=" + std::to_string(mshr_index) + " mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level)).c_str(), &MSHR.entry[mshr_index]);
    }

inline void CACHE::handle_writeback_miss_handled_stats_remove(int index) {
        MISS[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

inline uint32_t CACHE::handle_writeback_find_victim(uint32_t writeback_cpu, int index, uint32_t set) {
        if (cache_type == IS_LLC)
            return llc_find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);
        return find_victim(writeback_cpu, WQ.entry[index].instr_id, set, block[set], WQ.entry[index].ip, WQ.entry[index].full_addr, WQ.entry[index].type);
    }

inline void CACHE::handle_writeback_evict_dirty(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way, uint8_t& do_fill) {
        if (block[set][way].dirty && lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)) {
                do_fill = 0;
                lower_level->increment_WQ_FULL(block[set][way].address);
                STALL[WQ.entry[index].type]++;
            } else {
                PACKET writeback_packet;
                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = writeback_cpu;
                writeback_packet.address = block[set][way].address;
                writeback_packet.full_addr = block[set][way].full_addr;
                writeback_packet.data = block[set][way].data;
                writeback_packet.instr_id = WQ.entry[index].instr_id;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = PACK_CYCLE(current_core_cycle[writeback_cpu]);
                lower_level->add_wq(&writeback_packet);
            }
        }
    }

inline void CACHE::handle_writeback_do_fill(uint32_t writeback_cpu, int index, uint32_t set, uint32_t way) {
        DP_FILL_M(("WB_DO_FILL set=" + std::to_string(set) + " way=" + std::to_string(way) + " wq.fill=" + std::to_string(WQ.entry[index].fill_level) + " self.fill=" + std::to_string(fill_level)).c_str(), &WQ.entry[index]);
        if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(WQ.entry[index].full_addr, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
        else if (cache_type == IS_L2C)
            WQ.entry[index].pf_metadata = l2c_prefetcher_cache_fill(WQ.entry[index].address<<LOG2_BLOCK_SIZE, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
        if (cache_type == IS_LLC) {
            cpu = writeback_cpu;
            WQ.entry[index].pf_metadata = llc_prefetcher_cache_fill(WQ.entry[index].address<<LOG2_BLOCK_SIZE, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[index].pf_metadata);
            cpu = 0;
        }
        if (cache_type == IS_LLC)
            llc_update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0);
        else
            update_replacement_state(writeback_cpu, set, way, WQ.entry[index].full_addr, WQ.entry[index].ip, block[set][way].full_addr, WQ.entry[index].type, 0);
        sim_miss[writeback_cpu][WQ.entry[index].type]++;
        sim_access[writeback_cpu][WQ.entry[index].type]++;
        lpm_shadow_inc(writeback_cpu, WQ.entry[index].type, true);
        fill_cache(set, way, &WQ.entry[index]);
        block[set][way].dirty = 1;
        if (WQ.entry[index].fill_level < fill_level) {
            DP_FILL_M("WB_DO_FILL_RET_UP", &WQ.entry[index]);
            return_to_upper_level(WQ.entry[index]);
        }
        MISS[WQ.entry[index].type]++;
        ACCESS[WQ.entry[index].type]++;
        WQ.remove_queue(&WQ.entry[index]);
    }

inline uint32_t CACHE::handle_fill_find_victim(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set) {
        if (cache_type == IS_LLC)
            return llc_find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);
        return find_victim(fill_cpu, MSHR.entry[mshr_index].instr_id, set, block[set], MSHR.entry[mshr_index].ip, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].type);
    }

inline void CACHE::handle_fill_evict_dirty(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way, uint8_t& do_fill) {
        if (block[set][way].dirty && lower_level) {
            if (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)) {
                DP_FILL_M(("FILL_EVICT_STALL set=" + std::to_string(set) + " way=" + std::to_string(way) + " idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
                do_fill = 0;
                DP_FILL_EVICT_DOFILL0(fill_cpu, mshr_index, set, way);
                lower_level->increment_WQ_FULL(block[set][way].address);
                STALL[MSHR.entry[mshr_index].type]++;
            } else {
                DP_FILL_M(("FILL_EVICT_WB set=" + std::to_string(set) + " way=" + std::to_string(way) + " idx=" + std::to_string(mshr_index) + " evict_addr=0x" + std::to_string(block[set][way].address)).c_str(), &MSHR.entry[mshr_index]);
                PACKET writeback_packet;
                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = fill_cpu;
                writeback_packet.address = block[set][way].address;
                writeback_packet.full_addr = block[set][way].full_addr;
                writeback_packet.data = block[set][way].data;
                writeback_packet.instr_id = MSHR.entry[mshr_index].instr_id;
                writeback_packet.ip = 0;
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                lower_level->add_wq(&writeback_packet);
            }
        }
    }

inline void CACHE::handle_fill_pf_fill(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(MSHR.entry[mshr_index].full_addr, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
        if (cache_type == IS_L2C)
            MSHR.entry[mshr_index].pf_metadata = l2c_prefetcher_cache_fill(MSHR.entry[mshr_index].address<<LOG2_BLOCK_SIZE, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
        if (cache_type == IS_LLC) {
            cpu = fill_cpu;
            MSHR.entry[mshr_index].pf_metadata = llc_prefetcher_cache_fill(MSHR.entry[mshr_index].address<<LOG2_BLOCK_SIZE, set, way, (MSHR.entry[mshr_index].type == PREFETCH) ? 1 : 0, block[set][way].address<<LOG2_BLOCK_SIZE, MSHR.entry[mshr_index].pf_metadata);
            cpu = 0;
        }
    }

inline void CACHE::handle_fill_replacement(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_LLC)
            llc_update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0);
        else
            update_replacement_state(fill_cpu, set, way, MSHR.entry[mshr_index].full_addr, MSHR.entry[mshr_index].ip, block[set][way].full_addr, MSHR.entry[mshr_index].type, 0);
    }

inline void CACHE::handle_fill_stats(uint16_t fill_cpu, uint16_t mshr_index) {
        sim_miss[fill_cpu][MSHR.entry[mshr_index].type]++;
        sim_access[fill_cpu][MSHR.entry[mshr_index].type]++;
        lpm_shadow_inc(fill_cpu, MSHR.entry[mshr_index].type, true);
        if (MSHR.entry[mshr_index].type == LOAD
            IF_BYP_LLC( && !MSHR.entry[mshr_index].llc_bypassed )
        ) {
            sim_miss_wByP[fill_cpu]++;
            sim_access_wByP[fill_cpu]++;
        }
    } 

inline void CACHE::handle_fill_cache_and_dirty(uint16_t mshr_index, uint32_t set, uint32_t way) {
        fill_cache(set, way, &MSHR.entry[mshr_index]);
        DP_FILL(MSHR.entry[mshr_index], set, way);
        if (cache_type == IS_L1D && MSHR.entry[mshr_index].type == RFO)
            block[set][way].dirty = 1;
    }

inline void CACHE::handle_fill_return(uint16_t mshr_index) {
        bool fire = (MSHR.entry[mshr_index].fill_level < fill_level);
        DP_FILL_RETURN(mshr_index, fire);
        DP_FILL_M(("FILL_RET_CHK mshr.fill=" + std::to_string(MSHR.entry[mshr_index].fill_level) + " self.fill=" + std::to_string(fill_level) + " fire=" + (fire?"Y":"N") + " idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
        if (fire) {
            DP_FILL_M(("FILL_RET_FIRE idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
            return_to_upper_level(MSHR.entry[mshr_index]);
        } else {
            DP_FILL_M(("FILL_RET_SKIP idx=" + std::to_string(mshr_index)).c_str(), &MSHR.entry[mshr_index]);
        }
    }

inline void CACHE::handle_fill_processed_and_bypass_return(uint16_t fill_cpu, uint16_t mshr_index, uint32_t set, uint32_t way) {
        if (cache_type == IS_ITLB) {
            MSHR.entry[mshr_index].instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "ITLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_DTLB) {
            MSHR.entry[mshr_index].data_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "DTLB PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if (cache_type == IS_L1I) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L1I PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
        else if ((cache_type == IS_L1D) && (MSHR.entry[mshr_index].type != PREFETCH)) {
#ifdef DEBUG_PRINT
            if (DP_GATE_WW(current_core_cycle[fill_cpu], fill_cpu, MSHR.entry[mshr_index].address, MSHR.entry[mshr_index].instr_id)) {
                cout << "[L1D_PROC_ADD] iid:" << MSHR.entry[mshr_index].instr_id
                     << " addr:0x" << hex << MSHR.entry[mshr_index].address << dec
                     << " lq:" << MSHR.entry[mshr_index].lq_index
                     << " card:" << MSHR.entry[mshr_index].lq_index_depend_on_me.card
                     << " bits[0]:0x" << hex << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[0]
                     << " bits[1]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[1]
                     << " bits[2]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[2]
                     << " bits[3]:0x" << MSHR.entry[mshr_index].lq_index_depend_on_me.data.bits[3] << dec
                     << endl;
            }
#endif
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L1D PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
        }
#ifdef BYPASS_L1_LOGIC
        else if ((cache_type == IS_L2C) && (MSHR.entry[mshr_index].type == LOAD) && MSHR.entry[mshr_index].l1_bypassed == 1) {
            // L1 bypass still active (no prefetch cleared it) — CPU gets data via L2C PROCESSED
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&MSHR.entry[mshr_index]);
            else { cerr << "L2C PROCESSED FULL" << endl; assert(0&&"RETURN IS LOST FOREVER!!!! "); }
#ifdef BYP_DERFILL_ACTIVE
            // DERIVATIVE FILL: L2C filled, L1D was bypassed. Fill L1D directly.
            {
                CACHE* l1d = (CACHE*)upper_level_dcache[fill_cpu];
                PACKET df_pkt = MSHR.entry[mshr_index];
                df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                {
                    uint32_t df_set = l1d->get_set(df_pkt.address);
                    uint32_t df_way = l1d->find_victim(fill_cpu, df_pkt.instr_id, df_set, l1d->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                    if (l1d->block[df_set][df_way].valid && l1d->block[df_set][df_way].dirty) {
                        if (l1d->lower_level->get_occupancy(2, l1d->block[df_set][df_way].address) < l1d->lower_level->get_size(2, l1d->block[df_set][df_way].address)) {
                            PACKET wb_pkt;
                            wb_pkt.fill_level = FILL_L2;
                            wb_pkt.cpu = fill_cpu;
                            wb_pkt.address = l1d->block[df_set][df_way].address;
                            wb_pkt.full_addr = l1d->block[df_set][df_way].full_addr;
                            wb_pkt.data = l1d->block[df_set][df_way].data;
                            wb_pkt.instr_id = df_pkt.instr_id;
                            wb_pkt.ip = 0;
                            wb_pkt.type = WRITEBACK;
                            wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                            l1d->lower_level->add_wq(&wb_pkt);
                        }
                    }
                    l1d_prefetcher_cache_fill(df_pkt.full_addr, df_set, df_way, (df_pkt.type == PREFETCH) ? 1 : 0, l1d->block[df_set][df_way].address<<LOG2_BLOCK_SIZE, df_pkt.pf_metadata);
                    l1d->fill_cache(df_set, df_way, &df_pkt);
                    l1d->update_replacement_state(fill_cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, l1d->block[df_set][df_way].full_addr, df_pkt.type, 0);
                    if (df_pkt.type == RFO)
                        l1d->block[df_set][df_way].dirty = 1;
                }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                // Inject completed MSHR at L1D — fills in L1D_LATENCY cycles
                {
                    df_pkt.returned = COMPLETED;
                    df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu] + l1d->LATENCY);
                    df_pkt.fill_level = FILL_L1;
                    df_pkt.type = PREFETCH;
                    int mshr_idx = l1d->check_mshr(&df_pkt);
                    if (mshr_idx == -1
                        && l1d->MSHR.occupancy < l1d->MSHR.SIZE
                        && !(l1d->MSHR.occupancy && (l1d->MSHR.head == l1d->MSHR.tail))) {
                        if (df_pkt.lq_index_depend_on_me.card != 0
                            || df_pkt.sq_index_depend_on_me.card != 0
                            || !df_pkt.rob_index_depend_on_me.empty()) {
                            auto& __m = df_pkt;
                            if (__m.lq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.lq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (__m.sq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.sq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (!__m.rob_index_depend_on_me.empty())
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.rob_index_depend_on_me.size()
                                          << " **\033[0m\n" << std::flush;
                        }
                        l1d->MSHR.add_queue(&df_pkt);
                        l1d->MSHR.num_returned++;
                        l1d->update_fill_cycle();
                    }
                    // else: MSHR full or duplicate — skip (best-effort)
                }
#endif
            }
#endif
        }
#endif
#ifdef BYPASS_L2_LOGIC
        else if ((cache_type == IS_LLC) && (MSHR.entry[mshr_index].type == LOAD) && MSHR.entry[mshr_index].l2_bypassed == 1) {
            // L2 bypass active — skip L2C, return directly to L1D
            upper_level_dcache[fill_cpu]->upper_level_dcache[fill_cpu]->return_data(&MSHR.entry[mshr_index]);
            // PF from L2C merged into this bypassed MSHR — complete L2C's prefetch MSHR.
            if (MSHR.entry[mshr_index].pf_merged_from_upper
                    && ((CACHE *)upper_level_dcache[fill_cpu])->probe_mshr(&MSHR.entry[mshr_index]) != -1) {
                return_to_upper_level(MSHR.entry[mshr_index]);
            }
#ifdef BYP_DERFILL_ACTIVE
            // DERIVATIVE FILL: LLC filled, L2C was bypassed. Fill L2C directly.
            {
                CACHE* l2c = (CACHE*)upper_level_dcache[fill_cpu];
                PACKET df_pkt = MSHR.entry[mshr_index];
                df_pkt.l2_bypassed = 0;
                df_pkt.l1_bypassed = 0;
#ifdef BYP_DERFILL_IMMEDIATE
                {
                    uint32_t df_set = l2c->get_set(df_pkt.address);
                    uint32_t df_way = l2c->find_victim(fill_cpu, df_pkt.instr_id, df_set, l2c->block[df_set], df_pkt.ip, df_pkt.full_addr, df_pkt.type);
                    if (l2c->block[df_set][df_way].valid && l2c->block[df_set][df_way].dirty) {
                        if (l2c->lower_level->get_occupancy(2, l2c->block[df_set][df_way].address) < l2c->lower_level->get_size(2, l2c->block[df_set][df_way].address)) {
                            PACKET wb_pkt;
                            wb_pkt.fill_level = FILL_LLC;
                            wb_pkt.cpu = fill_cpu;
                            wb_pkt.address = l2c->block[df_set][df_way].address;
                            wb_pkt.full_addr = l2c->block[df_set][df_way].full_addr;
                            wb_pkt.data = l2c->block[df_set][df_way].data;
                            wb_pkt.instr_id = df_pkt.instr_id;
                            wb_pkt.ip = 0;
                            wb_pkt.type = WRITEBACK;
                            wb_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu]);
                            l2c->lower_level->add_wq(&wb_pkt);
                        }
                    }
                    l2c_prefetcher_cache_fill(df_pkt.address<<LOG2_BLOCK_SIZE, df_set, df_way, (df_pkt.type == PREFETCH) ? 1 : 0, l2c->block[df_set][df_way].address<<LOG2_BLOCK_SIZE, df_pkt.pf_metadata);
                    l2c->fill_cache(df_set, df_way, &df_pkt);
                    l2c->update_replacement_state(fill_cpu, df_set, df_way, df_pkt.full_addr, df_pkt.ip, l2c->block[df_set][df_way].full_addr, df_pkt.type, 0);
                }
#elif defined(BYP_DERFILL_SEQUENTIAL)
                // Inject completed MSHR at L2C — fills in L2C_LATENCY cycles
                {
                    df_pkt.returned = COMPLETED;
                    df_pkt.event_cycle = PACK_CYCLE(current_core_cycle[fill_cpu] + l2c->LATENCY);
                    df_pkt.fill_level = FILL_L2;
                    df_pkt.type = PREFETCH;
                    int mshr_idx = l2c->check_mshr(&df_pkt);
                    if (mshr_idx == -1
                        && l2c->MSHR.occupancy < l2c->MSHR.SIZE
                        && !(l2c->MSHR.occupancy && (l2c->MSHR.head == l2c->MSHR.tail))) {
                        if (df_pkt.lq_index_depend_on_me.card != 0
                            || df_pkt.sq_index_depend_on_me.card != 0
                            || !df_pkt.rob_index_depend_on_me.empty()) {
                            auto& __m = df_pkt;
                            if (__m.lq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_LQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.lq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (__m.sq_index_depend_on_me.card != 0)
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_SQ_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.sq_index_depend_on_me.card
                                          << " **\033[0m\n" << std::flush;
                            if (!__m.rob_index_depend_on_me.empty())
                                std::cout << "\033[1;31m\n** PF_MSHR_HAS_ROB_DEPS line=" << __LINE__
                                          << " func=" << __func__ << " cache=[" << NAME << "]"
                                          << " count=" << __m.rob_index_depend_on_me.size()
                                          << " **\033[0m\n" << std::flush;
                        }
                        l2c->MSHR.add_queue(&df_pkt);
                        l2c->MSHR.num_returned++;
                        l2c->update_fill_cycle();
                    }
                }
#endif
            }
#endif
        }
#endif
    }

inline void CACHE::handle_fill_remove(uint16_t fill_cpu, uint16_t mshr_index) {
        DP_FILL_M(("REMOVE idx=" + std::to_string(mshr_index) + " nret=" + std::to_string(MSHR.num_returned)).c_str(), &MSHR.entry[mshr_index]);
        // LPM counter: MSHR removal (normal fill path)
        {
            bool _byp = false;
            IF_BYP_L1(  if (cache_type == IS_L2C && MSHR.entry[mshr_index].l1_bypassed) _byp = true;  )
            IF_BYP_L2(  if (cache_type == IS_LLC && MSHR.entry[mshr_index].l2_bypassed) _byp = true;  )
            IF_BYP_LLC( if (cache_type == IS_LLC && MSHR.entry[mshr_index].llc_bypassed) _byp = true; )
            if (_byp) { lpm_mshr_byp_count--; lpm_mshr_byp_per_cpu[fill_cpu]--; }
            if (MSHR.entry[mshr_index].returned == INFLIGHT) {
                lpm_mshr_inflight_count--;
                lpm_mshr_inflight_per_cpu[fill_cpu]--;
            }
            lpm_mshr_occ_per_cpu[fill_cpu]--;
        }
        if (warmup_complete[fill_cpu]) {
            uint64_t current_miss_latency = current_core_cycle[fill_cpu] - MSHR.entry[mshr_index].cycle_enqueued;
            total_miss_latency += current_miss_latency;
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
    }

