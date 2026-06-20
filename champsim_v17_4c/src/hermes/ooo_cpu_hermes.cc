#include "champsim.h"
#ifdef USE_HERMES

#include "ooo_cpu.h"
#include <cassert>

#define FRONTAL 0
#define NONE    1
#define DORSAL  2

namespace knob {
    extern uint32_t num_rob_partitions;
    extern vector<int32_t> rob_partition_boundaries;
    extern vector<int32_t> rob_frontal_partition_ids;
    extern vector<int32_t> rob_dorsal_partition_ids;
    extern bool enable_offchip_tracing;
}

uint32_t O3_CPU::get_rob_parition_id(int32_t rob_pos)
{
    assert(rob_pos < ROB_SIZE);
    uint32_t index = 0;
    for(index = 0; index < knob::rob_partition_boundaries.size(); ++index)
    {
        if((int32_t)rob_pos < knob::rob_partition_boundaries[index])
            return index;
    }
    return index;
}

bool O3_CPU::rob_part_id_is_frontal(uint32_t rob_part_id)
{
    for(uint32_t index = 0; index < knob::rob_frontal_partition_ids.size(); ++index)
    {
        if((int32_t)rob_part_id == knob::rob_frontal_partition_ids[index])
            return true;
    }
    return false;
}

bool O3_CPU::rob_part_id_is_dorsal(uint32_t rob_part_id)
{
    for(uint32_t index = 0; index < knob::rob_dorsal_partition_ids.size(); ++index)
    {
        if((int32_t)rob_part_id == knob::rob_dorsal_partition_ids[index])
            return true;
    }
    return false;
}

bool O3_CPU::rob_pos_is_frontal(int32_t rob_pos)
{
    return rob_part_id_is_frontal(get_rob_parition_id(rob_pos));
}

bool O3_CPU::rob_pos_is_dorsal(int32_t rob_pos)
{
    return rob_part_id_is_dorsal(get_rob_parition_id(rob_pos));
}

int8_t O3_CPU::rob_part_id_get_part_type(uint32_t rob_part_id)
{
    if(rob_part_id_is_frontal(rob_part_id))     return FRONTAL;
    else if(rob_part_id_is_dorsal(rob_part_id)) return DORSAL;
    else                                        return NONE;
}

int8_t O3_CPU::rob_pos_get_part_type(int32_t rob_pos)
{
    return rob_part_id_get_part_type(get_rob_parition_id(rob_pos));
}

void O3_CPU::monitor_loads(uint32_t lq_index)
{
    (void)lq_index;
}

#endif // USE_HERMES
