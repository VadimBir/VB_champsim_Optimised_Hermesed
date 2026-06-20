#include "champsim.h"
#ifdef USE_HERMES
// Hermes OCP factory/dispatch — adapted for champsim_v10_ByP_L1Fille_incHermess target

#include <iostream>
#include "ooo_cpu.h"
#include "uncore.h"
#include "hermes/offchip_pred_base.h"
#include "hermes/offchip_pred_basic.h"
#include "hermes/offchip_pred_random.h"
#include "hermes/offchip_pred_perc.h"
#include "hermes/offchip_pred_hmp_local.h"
#include "hermes/offchip_pred_hmp_gshare.h"
#include "hermes/offchip_pred_hmp_gskew.h"
#include "hermes/offchip_pred_hmp_ensemble.h"
#include "hermes/offchip_pred_ttp.h"

namespace knob
{
    extern std::string offchip_pred_type;
    extern bool   offchip_pred_mark_merged_load;
    extern bool   enable_ddrp;
    extern uint32_t ddrp_req_latency;
    extern bool   dram_cntlr_enable_ddrp_buffer;
}

void O3_CPU::initialize_offchip_predictor(uint64_t seed)
{
    if(!knob::offchip_pred_type.compare("none"))
        offchip_pred = new OffchipPredBase(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("basic"))
        offchip_pred = new OffchipPredBasic(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("random"))
        offchip_pred = new OffchipPredRandom(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("perc"))
        offchip_pred = new OffchipPredPerc(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("hmp-local"))
        offchip_pred = new OffchipPredHMPLocal(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("hmp-gshare"))
        offchip_pred = new OffchipPredHMPGshare(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("hmp-gskew"))
        offchip_pred = new OffchipPredHMPGskew(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("hmp-ensemble"))
        offchip_pred = new OffchipPredHMPEnsemble(cpu, knob::offchip_pred_type, seed);
    else if(!knob::offchip_pred_type.compare("ttp"))
        offchip_pred = new OffchipPredTTP(cpu, knob::offchip_pred_type, seed);
    std::cout << "[Hermes] CPU " << cpu << " offchip_pred = " << knob::offchip_pred_type << std::endl;
}

void O3_CPU::print_config_offchip_predictor()
{
    std::cout << "offchip_pred_type " << knob::offchip_pred_type << std::endl
              << "offchip_pred_mark_merged_load " << knob::offchip_pred_mark_merged_load << std::endl;
    if (offchip_pred) offchip_pred->print_config();
}

void O3_CPU::dump_stats_offchip_predictor()
{
    float precision = (float)stats.offchip_pred.true_pos /
                      (stats.offchip_pred.true_pos + stats.offchip_pred.false_pos + 1e-9);
    float recall    = (float)stats.offchip_pred.true_pos /
                      (stats.offchip_pred.true_pos + stats.offchip_pred.false_neg + 1e-9);
    std::cout << "Core_" << cpu << "_offchip_pred_true_pos "  << stats.offchip_pred.true_pos  << std::endl
              << "Core_" << cpu << "_offchip_pred_false_pos " << stats.offchip_pred.false_pos << std::endl
              << "Core_" << cpu << "_offchip_pred_false_neg " << stats.offchip_pred.false_neg << std::endl
              << "Core_" << cpu << "_offchip_pred_precision " << precision*100 << std::endl
              << "Core_" << cpu << "_offchip_pred_recall "    << recall*100    << std::endl
              << "Core_" << cpu << "_ddrp_total "             << stats.ddrp.total << std::endl
              << "Core_" << cpu << "_ddrp_issued_direct "     << stats.ddrp.issued[0] << std::endl
              << "Core_" << cpu << "_ddrp_issued_merged "     << stats.ddrp.issued[1] << std::endl
              << "Core_" << cpu << "_ddrp_dram_rq_full "      << stats.ddrp.dram_rq_full  << std::endl
              << "Core_" << cpu << "_ddrp_dram_mshr_full "    << stats.ddrp.dram_mshr_full<< std::endl;
    if (offchip_pred) offchip_pred->dump_stats();
}

void O3_CPU::offchip_predictor_update_dram_bw(uint8_t dram_bw)
{
    if (offchip_pred) offchip_pred->update_dram_bw(dram_bw);
}

void O3_CPU::offchip_predictor_track_llc_eviction(uint32_t set, uint32_t way, uint64_t address)
{
    (void)set; (void)way;
    if (offchip_pred && !knob::offchip_pred_type.compare("ttp")) {
        OffchipPredTTP *p = (OffchipPredTTP*) offchip_pred;
        p->track_llc_eviction(address);
    }
}

// ---- Training + DDRP issue stub (simplified vs Hermes; training/issue wiring happens in hook sites) ----
void O3_CPU::offchip_pred_stats_and_train(uint32_t lq_index)
{
    if (!offchip_pred) return;
    LSQ_ENTRY &lq = LQ.entry[lq_index];

    // Update confusion-matrix counters
    if      ( lq.went_offchip &&  lq.went_offchip_pred) stats.offchip_pred.true_pos++;
    else if (!lq.went_offchip &&  lq.went_offchip_pred) stats.offchip_pred.false_pos++;
    else if ( lq.went_offchip && !lq.went_offchip_pred) stats.offchip_pred.false_neg++;
    else                                                stats.offchip_pred.true_neg++;

    // Train (arch_instr pointer comes from ROB — use rob_index on LSQ_ENTRY)
    ooo_model_instr *arch_instr = &ROB.entry[lq.rob_index];
    if (arch_instr) offchip_pred->train(arch_instr, lq.data_index, &lq);
}

void O3_CPU::issue_ddrp_request(uint32_t lq_index, uint32_t call_type)
{
    stats.ddrp.total++;
    assert(LQ.entry[lq_index].translated == COMPLETED);
    assert(LQ.entry[lq_index].physical_address != 0);
    assert(knob::enable_ddrp);

    if (ddrp_monitor && ddrp_monitor->disable_ddrp == true)
        return;

    if (uncore.DRAM.get_occupancy(1, LQ.entry[lq_index].physical_address >> LOG2_BLOCK_SIZE)
        == uncore.DRAM.get_size(1, LQ.entry[lq_index].physical_address >> LOG2_BLOCK_SIZE))
    {
        stats.ddrp.dram_rq_full++;
        return;
    }

    PACKET data_packet;
    data_packet.fill_level = FILL_DDRP;
    data_packet.cpu = cpu;
    data_packet.data_index = LQ.entry[lq_index].data_index;
    data_packet.lq_index = lq_index;
    data_packet.address = LQ.entry[lq_index].physical_address >> LOG2_BLOCK_SIZE;
    data_packet.full_addr = LQ.entry[lq_index].physical_address;
    data_packet.instr_id = LQ.entry[lq_index].instr_id;
    data_packet.rob_index = LQ.entry[lq_index].rob_index;
    data_packet.rob_position = LQ.entry[lq_index].rob_position;
    data_packet.ip = LQ.entry[lq_index].ip;
    data_packet.type = PREFETCH;
    data_packet.asid[0] = LQ.entry[lq_index].asid[0];
    data_packet.asid[1] = LQ.entry[lq_index].asid[1];
    data_packet.event_cycle = PACK_CYCLE(LQ.entry[lq_index].event_cycle + knob::ddrp_req_latency + L1D_LATENCY); // The DRAM hermes request is PARALELL to L1D latency but champsim has is sequential, thus, we must overlap the identical cycles from the L1D into the PATH latency 

    uncore.DRAM.add_rq(&data_packet);
    stats.ddrp.issued[call_type]++;
}

void O3_CPU::initialize_ddrp_monitor()
{
    ddrp_monitor = nullptr; // opt-in — not wired by default
}

void O3_CPU::print_config_ddrp_monitor()
{
    std::cout << "ddrp_monitor disabled (stub)" << std::endl;
}


#endif // USE_HERMES
