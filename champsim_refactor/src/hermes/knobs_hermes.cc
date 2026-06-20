#include "champsim.h"
#ifdef USE_HERMES
// Hermes knob definitions (minimal — OCP + DDRP config knobs)

#include <string>
#include <vector>
#include <cstdint>

namespace knob
{
    // OCP selector + training
#ifdef USE_HERMES_TTP
    std::string offchip_pred_type = "ttp";
#elif defined(USE_HERMES_HMP)
    std::string offchip_pred_type = "hmp-local";
#else
    std::string offchip_pred_type = "perc";
#endif
    bool        offchip_pred_mark_merged_load = false;

    // Basic OCP
    uint32_t ocp_basic_table_size = 4096;
    uint32_t ocp_basic_counter_width = 10;
    float    ocp_basic_conf_thresh = 0.6;
    uint32_t ocp_basic_hash_type = 11;
    uint32_t ocp_basic_include_data_index_type = 0;
    uint32_t ocp_basic_pc_buf_size = 64;
    uint32_t ocp_basic_feature_type = 0;
    uint32_t ocp_basic_count_modulo = 8;
    uint32_t ocp_basic_page_buf_size = 64;

    // Random
    float ocp_random_pos_rate = 0.1;

    // Perceptron — paper config (ocp_hermes.ini, MICRO'22)
    std::vector<int32_t> ocp_perc_activated_features  = {5, 8, 9, 11, 16};
    std::vector<int32_t> ocp_perc_weight_array_sizes  = {1024, 1024, 128, 1024, 1024};
    std::vector<int32_t> ocp_perc_feature_hash_types  = {2, 2, 2, 2, 2};
    float ocp_perc_activation_threshold = -18; // ini 17 -18 paper 
    float ocp_perc_max_weight = 15;
    float ocp_perc_min_weight = -16;
    float ocp_perc_pos_weight_delta = 1;
    float ocp_perc_neg_weight_delta = 1;
    float ocp_perc_pos_train_thresh = 40;
    float ocp_perc_neg_train_thresh = -35;
    uint32_t ocp_perc_page_buf_sets = 8;
    uint32_t ocp_perc_page_buf_assoc = 8;
    uint32_t ocp_perc_last_n_load_pcs = 4;
    uint32_t ocp_perc_last_n_pcs = 4;
    bool ocp_perc_enable_dynamic_act_thresh = false;
    uint32_t ocp_perc_update_act_thresh_epoch = 2048;
    uint32_t ocp_perc_high_critical_dram_bw_level = 3;
    uint32_t ocp_perc_low_critical_dram_bw_level = 0;
    float ocp_perc_poor_precision_thresh = 0.97;
    float ocp_perc_act_thresh_update_gradient = 1.0;
    float ocp_perc_max_activation_threshold = -1;
    float ocp_perc_min_activation_threshold = -1;

    // HMP-Local
    uint32_t ocp_hmp_local_history_length = 8;
    uint32_t ocp_hmp_local_lhr_size = 2048;
    uint32_t ocp_hmp_local_lhr_index_hash_type = 2;

    // HMP-Gshare
    uint32_t ocp_hmp_gshare_history_length = 11;
    uint32_t ocp_hmp_gshare_pc_hash_type = 2;

    // HMP-Gskew
    uint32_t ocp_hmp_gskew_history_length = 20;
    uint32_t ocp_hmp_gskew_num_hashes = 5;
    std::vector<int32_t> ocp_hmp_gskew_hash_types = {2, 5, 7, 112, 1005};
    uint32_t ocp_hmp_gskew_pht_size = 1024;

    // TTP
    uint32_t ocp_ttp_partial_tag_size = 30;
    uint32_t ocp_ttp_catalog_cache_sets = 16384;
    uint32_t ocp_ttp_catalog_cache_assoc = 24;
    uint32_t ocp_ttp_hash_type = 5;
    bool     ocp_ttp_enable_track_llc_eviction = true;

    // ROB partitions
    uint32_t num_rob_partitions = 3;
    vector<int32_t> rob_partition_boundaries;
    vector<int32_t> rob_frontal_partition_ids;
    vector<int32_t> rob_dorsal_partition_ids;
    bool enable_offchip_tracing = false;

    // DDRP
    bool     enable_ddrp = true;
    uint32_t ddrp_req_latency = 18;
    bool     dram_cntlr_enable_ddrp_buffer = false;
    uint32_t dram_cntlr_ddrp_buffer_sets = 64;
    uint32_t dram_cntlr_ddrp_buffer_assoc = 16;
    uint32_t dram_cntlr_ddrp_buffer_hash_type = 2;
    bool     enable_ddrp_monitor = false;
    uint32_t ddrp_monitor_exploit_epoch = 100000;
    uint32_t ddrp_monitor_explore_epoch = 10000;
    bool     ddrp_monitor_enable_hysterisis = false;
}


#endif // USE_HERMES
