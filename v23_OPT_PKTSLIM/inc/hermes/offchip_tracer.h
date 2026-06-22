#ifndef OFFCHIP_TRACER_H
#define OFFCHIP_TRACER_H

#include <stdio.h>
#ifndef _WIN32
#include <zlib.h>   // unused here (no zlib symbol in this header or offchip_tracer.cc); system header on Linux, absent in w64devkit
#endif
#include <string>
#include <unordered_map>

class OffchipTracer
{
    private:
        std::string filename;
        FILE *trace_file = NULL;
        std::unordered_map<uint64_t, uint64_t> latency_map;

    public:
        OffchipTracer(){}
        ~OffchipTracer(){}
        void init_tracing(std::string filename, uint32_t cpu);
        void fini_tracing();
        void record(uint64_t latency);
        void dump_trace();
};

#endif /* OFFCHIP_TRACER_H */

