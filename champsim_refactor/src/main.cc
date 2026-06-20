#define _BSD_SOURCE


#include "lpm_tracker.h"
#include "ooo_l1_byp_model.cc"
#ifdef BYPASS_L2_LOGIC
#include "ooo_l2_byp_model.cc"
#endif
#ifdef BYPASS_LLC_LOGIC
#include "ooo_llc_byp_model.cc"
#endif

#include <getopt.h>
#include "ooo_cpu.h"
#ifdef USE_TRACE_HELPER
#include "trace_helper.h"
#endif


#include <unistd.h>   // getcwd
#include <limits.h>   // PATH_MAX

#include "uncore.h"
#include <fstream>

#include "instr_event.h"
#include "db_writer.h"

#include <iostream>
// #include <iomanip>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <sstream>
#ifdef USE_HERMES
namespace knob { extern std::string offchip_pred_type; }
#ifdef USE_HERMES_TTP
#define HERMES_LABEL "Hermes:TTP"
#elif defined(USE_HERMES_HMP)
#define HERMES_LABEL "Hermes:HMP"
#else
#define HERMES_LABEL "Hermes:std_active"
#endif
#else
#define HERMES_LABEL "Hermes:inactive"
#endif
// #include <map>
#include <string>

#include <stdint.h> // C-style

#define FIXED_FLOAT(x) std::fixed << std::setprecision(5) << (x)
#define FIXED_FLOAT4(x) std::fixed << std::setprecision(4) << (x)
#define FIXED_FLOAT2(x) std::fixed << std::setprecision(2) << (x)
#define FIXED_FLOAT1(x) std::fixed << std::setprecision(1) << (x)

#define STR_CORE_NUM       "C:"
#define STR_INSTR        "I#"
#define STR_CYCLES         "cy"
#define STR_IPC_NOW        "IPC "
#define STR_IPC_AVG        "IPCavg "
#define STR_BYPASS         "ByP% "
#define STR_APC        "CPA "
#define STR_LPM         "LPM "
#define STR_cAMT        "cAMT "
#define STR_MST         "MST "
#define STR_MSHR_OCCUPANCY_PERCENT       "MSHR%"
#define STR_LOAD_HIT_RATE       "LoadH%"
#define STR_TIME           "⏱"
#define STR_TimePerMillionInstr           "TPMI "

// CACHE TYPEs
#define ITLB_type 0
#define DTLB_type 1
#define STLB_type 2
#define L1I_type  3
#define L1D_type  4
#define L2C_type  5
#define LLC_type  6
#define DRAM_type 7


float TOTAL_SUM_FINAL_SIM_IPC = 0;
uint8_t warmup_complete[NUM_CPUS], 
        simulation_complete[NUM_CPUS], 
        all_warmup_complete = 0, 
        all_simulation_complete = 0;
uint8_t MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS;
uint8_t knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000,
         champsim_seed;

extern int64_t execution_checksum[NUM_CPUS];

#ifdef BYPASS_DEBUG
extern std::vector<CACHE*> ALL_CACHES;
#endif


time_t start_time;
time_t last_print_time[NUM_CPUS] = {};

// PAGE TABLE
uint32_t PAGE_TABLE_LATENCY = 0, SWAP_LATENCY = 0;
queue <uint64_t > page_queue;
ankerl::unordered_dense::map <uint64_t, uint64_t> page_table, inverse_table, recent_page, unique_cl[NUM_CPUS];
uint64_t previous_ppage, num_adjacent_page, num_cl[NUM_CPUS], allocated_pages, num_page[NUM_CPUS], minor_fault[NUM_CPUS], major_fault[NUM_CPUS];

#include "main_stats.h"

void signal_handler(int signal)
{
	cout << "Caught signal: " << signal << endl;
	IF_TRACE_HELPER(
	trace_helper.stop();
	)
	exit(1);
}



RANDOM champsim_rand(champsim_seed);
#include "main_paging.h"
#include "main_loop.h"
#define ROB_MASK (ROB_SIZE - 1)
// static char zero_arena[1024*1024*1024] = {0};
int main(int argc, char** argv)
{
    
    ChampsimDBConfig db_cfg = {};

    int num_instr_dest = NUM_INSTR_DESTINATIONS;
    int num_instr_dest_sparc = NUM_INSTR_DESTINATIONS_SPARC;
#ifdef SANITY_CHECK
#else
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl;
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl;
    cerr << "\033[1;31m*** SANITY CHECK IS DISABLED!!!! ***\033[0m" << endl << endl;
#endif

if (num_instr_dest == 2) {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;33m*** NON CLOUD SUITE NUM_INSTR_DESTINATIONS \033[1;31m(%d)\033[0m \033[1;33m***\033[0m\n",
                     num_instr_dest);
} else if (num_instr_dest_sparc != 4) {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;34m*** HYBRID NUM_INSTR_DESTINATIONS \033[1;31m%d\033[0m \033[1;34m***\033[0m\n",
                     num_instr_dest_sparc);
} else {
    for (int r = 0; r < 3; ++r)
        std::fprintf(stderr, "\033[1;31m*** CLOUD NUM_INSTR_DESTINATIONS \033[1;31m(%d)\033[0m \033[1;32m***\033[0m\n",
                     num_instr_dest);
}


        // pf_stat_num_retired = 0;
	// interrupt signal hanlder
#ifdef _WIN32
	// MinGW has no sigaction; plain signal() installs the same SIGINT handler.
	signal(SIGINT, signal_handler);
#else
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);
#endif

    // cout << "*************************************************" << endl
    //      << "   ChampSim Multicore Out-of-Order Simulator" << endl
    //      << "   Last compiled: " << __DATE__ << " " << __TIME__ << endl
    //      << "*************************************************" << endl;

    #include <cstdio>
    fprintf(stdout, "*************************************************\n");
    fprintf(stdout, "   ChampSim Multicore Out-of-Order Simulator\n");
    fprintf(stdout, "   Last compiled: %s %s\n", __DATE__, __TIME__);
    fprintf(stdout, "*************************************************\n");


    // initialize knobs
    uint8_t show_heartbeat = 1;

    uint32_t seed_number = 0;

    // check to see if knobs changed using getopt_long()
    int c;
    while (1) {
        static struct option long_options[] =
        {
            {"warmup_instructions", required_argument, 0, 'w'},
            {"simulation_instructions", required_argument, 0, 'i'},
            {"hide_heartbeat", no_argument, 0, 'h'},
            {"cloudsuite", no_argument, 0, 'c'},
            {"low_bandwidth",  no_argument, 0, 'b'},
            {"traces",  no_argument, 0, 't'},
            // ── DB args ──
            {"db",           required_argument, 0, 'D'},
            {"arch",         required_argument, 0, 'A'},
            {"bypass",       required_argument, 0, 'B'},
            {"pf_l1",        required_argument, 0, '1'},
            {"pf_l2",        required_argument, 0, '2'},
            {"pf_l3",        required_argument, 0, '3'},
            {"unique_epoch", no_argument,       0, 'E'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        c = getopt_long_only(argc, argv, "wihsb", long_options, &option_index);

        // no more option characters
        if (c == -1)
            break;

        int traces_encountered = 0;

        switch(c) {
            case 'w':
                warmup_instructions = atol(optarg);
                break;
            case 'i':
                simulation_instructions = atol(optarg);
                break;
            case 'h':
                show_heartbeat = 0;
                break;
            case 'c':
                knob_cloudsuite = 1;
                MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
                break;
            case 'b':
                knob_low_bandwidth = 1;
                break;
            case 't':
                traces_encountered = 1;
                break;
            // ── DB args ──
            case 'D':
                std::strncpy(db_cfg.db_path, optarg, sizeof(db_cfg.db_path) - 1);
                break;
            case 'A':
                std::strncpy(db_cfg.arch, optarg, sizeof(db_cfg.arch) - 1);
                break;
            case 'B':
                std::strncpy(db_cfg.bypass, optarg, sizeof(db_cfg.bypass) - 1);
                break;
            case '1':
                std::strncpy(db_cfg.pf_l1, optarg, sizeof(db_cfg.pf_l1) - 1);
                break;
            case '2':
                std::strncpy(db_cfg.pf_l2, optarg, sizeof(db_cfg.pf_l2) - 1);
                break;
            case '3':
                std::strncpy(db_cfg.pf_l3, optarg, sizeof(db_cfg.pf_l3) - 1);
                break;
            case 'E':
                db_cfg.use_epoch = true;
                break;
            default:
                abort();
        }

        if (traces_encountered == 1)
            break;
    }

    if (knob_low_bandwidth)
        DRAM_MTPS = DRAM_IO_FREQ/4;
    else
        DRAM_MTPS = DRAM_IO_FREQ;

    // DRAM access latency
    tRP  = (uint32_t)((1.0 * tRP_DRAM_NANOSECONDS  * CPU_FREQ) / 1000); 
    tRCD = (uint32_t)((1.0 * tRCD_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 
    tCAS = (uint32_t)((1.0 * tCAS_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 

    // default: 16 = (64 / 8) * (3200 / 1600)
    // it takes 16 CPU cycles to tranfser 64B cache block on a 8B (64-bit) bus 
    // note that dram burst length = BLOCK_SIZE/DRAM_CHANNEL_WIDTH
    DRAM_DBUS_RETURN_TIME = (BLOCK_SIZE / DRAM_CHANNEL_WIDTH) * (1.0 * CPU_FREQ / DRAM_MTPS);

    // end consequence of knobs

    // search through the argv for "-traces"
// search through the argv for "-traces"
    int found_traces = 0;
    int count_traces = 0;
    cout << endl;
    for (int i=0; i<argc; i++) {
        if (found_traces == 0) {
            if (strcmp(argv[i], "-traces") == 0) {
                found_traces = 1;
            }
            continue;
        }

        // Copy argv into trace_string buffer
        std::snprintf(ooo_cpu[count_traces].trace_string,
                    sizeof(ooo_cpu[count_traces].trace_string),
                    "%s",
                    argv[i]);
        ooo_cpu[count_traces].trace_string[sizeof(ooo_cpu[count_traces].trace_string)-1] = '\0';

        // Point to the buffer we just initialized
        const char *full_name = ooo_cpu[count_traces].trace_string;
        // cout << "" << full_name << endl;
        printf("trace_%d %s\n", count_traces, argv[i]);

        // Validate extension
        const char *last_dot = std::strrchr(full_name, '.');
        if (!last_dot || last_dot[1] == '\0') {
            std::cerr << "TRACE FILE HAS NO EXTENSION: " << full_name << '\n';
            assert(false && "TRACE FILE HAS NO EXTENSION");
        }

        char ext = last_dot[1];

        if (ext == 'g') {
            std::snprintf(ooo_cpu[count_traces].gunzip_command,
                        sizeof(ooo_cpu[count_traces].gunzip_command),
                        "gunzip -c %s", full_name);

            ooo_cpu[count_traces].trace_file = popen(ooo_cpu[count_traces].gunzip_command, "r");
            if (!ooo_cpu[count_traces].trace_file) {
                std::fprintf(stderr, "Unable to popen gunzip command: %s\n", ooo_cpu[count_traces].gunzip_command);
                assert(false && "Unable to popen gunzip command");
            }

            setvbuf(ooo_cpu[count_traces].trace_file, NULL, _IOFBF, 16 * 1024 * 1024);
            ooo_cpu[count_traces].is_xz_trace = false;
        }
        else if (ext == 'x') {
            // fprintf(stderr, "DEBUG: count_traces=%d, NUM_CPUS=%d\n", count_traces, NUM_CPUS);
            // fprintf(stderr, "DEBUG: full_name='%s'\n", full_name);
            // fprintf(stderr, "DEBUG: &ooo_cpu[count_traces]=%p\n", (void*)&ooo_cpu[count_traces]);
            // fprintf(stderr, "DEBUG: &xz_reader=%p\n", (void*)&ooo_cpu[count_traces].xz_reader);

            // if (!ooo_cpu[count_traces].xz_reader.open(full_name)) {
            //     std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s\n", full_name);
            //     assert(false && "TRACE FILE DOES NOT EXIST");
            // }
            if (!ooo_cpu[count_traces].xz_reader.open(full_name)) {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                    std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s/%s\n", cwd, full_name);
                } else {
                    std::perror("getcwd failed");
                    std::fprintf(stderr, "CANNOT OPEN XZ TRACE FILE: %s\n", full_name);
                }
                assert(false && "TRACE FILE DOES NOT EXIST");
            }
            ooo_cpu[count_traces].is_xz_trace = true;
            ooo_cpu[count_traces].verify_xz_trace();
        }
        else {
            std::cerr << "ChampSim does not support traces other than gz or xz compression: " << full_name << '\n';
            assert(false && "ChampSim does not support traces other than gz or xz compression!");
        }

        char *pch[100] = {nullptr};
        int count_str = 0;
        pch[count_str] = std::strtok(argv[i], " /,.-");
        while (pch[count_str] != nullptr && count_str + 1 < (int)std::size(pch)) {
            ++count_str;
            pch[count_str] = std::strtok(nullptr, " /,.-");
        }

        if (count_str < 3 || pch[count_str - 3] == nullptr) {
            std::cerr << "TRACE FILENAME TOKENIZATION FAILED: " << argv[i] << '\n';
            assert(false && "TRACE FILENAME TOKENIZATION FAILED");
        }

        int j = 0;
        while (pch[count_str - 3][j] != '\0') {
            seed_number += static_cast<unsigned long>(pch[count_str - 3][j]);
            ++j;
        }

        if (!ooo_cpu[count_traces].is_xz_trace && ooo_cpu[count_traces].trace_file == NULL) {
            std::fprintf(stderr, "\n*** Trace file not found: %s ***\n\n", full_name);
            assert(false && "Trace file not found");
        }

        ++count_traces;
        if (count_traces > NUM_CPUS) {
            std::fprintf(stderr, "\n*** Too many traces for the configured number of cores ***\n\n");
            assert(false && "Too many traces for the configured number of cores");
        }
    }

    // cout << "NUM_CPUS " << NUM_CPUS << endl;
    fprintf(stdout, "NUM_CPUS %d\n", NUM_CPUS);

    if (count_traces != NUM_CPUS) {
        printf("\n*** Not enough traces for the configured number of cores ***\n\n");
        assert(0&&"Not enough traces for the configured number of cores");
    }
    // end trace file setup

    // TODO: can we initialize these variables from the class constructor?
    srand(seed_number);
    champsim_seed = seed_number;

    lpm_init(); // INITIALIZE THE STATS COLLECTOR FOR THE Layer Performance Metric (LPM)
    byplat_init_all(); // BYPASS LATENCY + MLP TRACKER
    for (int i=0; i<NUM_CPUS; i++) {

        ooo_cpu[i].cpu = i; 
        ooo_cpu[i].warmup_instructions = warmup_instructions;
        ooo_cpu[i].simulation_instructions = simulation_instructions;
        ooo_cpu[i].begin_sim_cycle = 0; 
        ooo_cpu[i].begin_sim_instr = warmup_instructions;

        // ROB
        ooo_cpu[i].ROB.cpu = i;

        // BRANCH PREDICTOR
        ooo_cpu[i].initialize_branch_predictor();
        IF_HERMES(
        ooo_cpu[i].offchip_pred = NULL;
        ooo_cpu[i].ddrp_monitor = NULL;
        ooo_cpu[i].initialize_offchip_predictor(i);
        ooo_cpu[i].initialize_ddrp_monitor();
        memset(&ooo_cpu[i].stats, 0, sizeof(ooo_cpu[i].stats));
        if (i == 0) cout << HERMES_LABEL << endl;
        )

        // TLBs
        ooo_cpu[i].ITLB.cpu = i;
        ooo_cpu[i].ITLB.cache_type = IS_ITLB;
        ooo_cpu[i].ITLB.fill_level = FILL_L1;
        ooo_cpu[i].ITLB.extra_interface = &ooo_cpu[i].L1I;
        ooo_cpu[i].ITLB.lower_level = &ooo_cpu[i].STLB; 

        ooo_cpu[i].DTLB.cpu = i;
        ooo_cpu[i].DTLB.cache_type = IS_DTLB;
        ooo_cpu[i].DTLB.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].DTLB.fill_level = FILL_L1;
        ooo_cpu[i].DTLB.extra_interface = &ooo_cpu[i].L1D;
        ooo_cpu[i].DTLB.lower_level = &ooo_cpu[i].STLB;

        ooo_cpu[i].STLB.cpu = i;
        ooo_cpu[i].STLB.cache_type = IS_STLB;
        ooo_cpu[i].STLB.fill_level = FILL_L2;
        ooo_cpu[i].STLB.upper_level_icache[i] = &ooo_cpu[i].ITLB;
        ooo_cpu[i].STLB.upper_level_dcache[i] = &ooo_cpu[i].DTLB;

        // PRIVATE CACHE
        ooo_cpu[i].L1I.cpu = i;
        ooo_cpu[i].L1I.cache_type = IS_L1I;
        ooo_cpu[i].L1I.MAX_READ = (FETCH_WIDTH > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : FETCH_WIDTH;
        ooo_cpu[i].L1I.fill_level = FILL_L1;
        ooo_cpu[i].L1I.lower_level = &ooo_cpu[i].L2C; 

        ooo_cpu[i].L1D.cpu = i;
        ooo_cpu[i].L1D.cache_type = IS_L1D;
        ooo_cpu[i].L1D.WQ.is_WQ = 1;
        ooo_cpu[i].L1D.MAX_READ = (2 > MAX_READ_PER_CYCLE) ? MAX_READ_PER_CYCLE : 2;
        ooo_cpu[i].L1D.fill_level = FILL_L1;
        ooo_cpu[i].L1D.lower_level = &ooo_cpu[i].L2C;
        ooo_cpu[i].L1D.l1d_prefetcher_initialize();
        IF_BYP_L1(
        l1d_bypass_initialize(i, &ooo_cpu[i].L1D, &ooo_cpu[i].L2C, &uncore.LLC);
        )
        // ooo_cpu[i].L1D.MSHR.occupancy

        ooo_cpu[i].L2C.cpu = i;
        ooo_cpu[i].L2C.cache_type = IS_L2C;
        ooo_cpu[i].L2C.fill_level = FILL_L2;
        ooo_cpu[i].L2C.upper_level_icache[i] = &ooo_cpu[i].L1I;
        ooo_cpu[i].L2C.upper_level_dcache[i] = &ooo_cpu[i].L1D;
        ooo_cpu[i].L2C.lower_level = &uncore.LLC;
        ooo_cpu[i].L2C.l2c_prefetcher_initialize();
        IF_BYP_L2(
        l2c_bypass_initialize(i, &ooo_cpu[i].L1D, &ooo_cpu[i].L2C, &uncore.LLC);
        )

        // SHARED CACHE
        uncore.LLC.cache_type = IS_LLC;
        uncore.LLC.fill_level = FILL_LLC;
        uncore.LLC.MAX_READ = NUM_CPUS;
        uncore.LLC.upper_level_icache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.upper_level_dcache[i] = &ooo_cpu[i].L2C;
        uncore.LLC.lower_level = &uncore.DRAM;

        // OFF-CHIP DRAM
        uncore.DRAM.fill_level = FILL_DRAM;
        uncore.DRAM.upper_level_icache[i] = &uncore.LLC;
        uncore.DRAM.upper_level_dcache[i] = &uncore.LLC;
        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            uncore.DRAM.RQ[i].is_RQ = 1;
            uncore.DRAM.WQ[i].is_WQ = 1;
        }

        warmup_complete[i] = 0;
        //all_warmup_complete = NUM_CPUS;
        simulation_complete[i] = 0;
        current_core_cycle[i] = 0;
        stall_cycle[i] = 0;
        
        previous_ppage = 0;
        num_adjacent_page = 0;
        num_cl[i] = 0;
        allocated_pages = 0;
        num_page[i] = 0;
        minor_fault[i] = 0;
        major_fault[i] = 0;
    }
#ifdef BYPASS_DEBUG
    for (int i=0; i<NUM_CPUS; i++) {
        ALL_CACHES.push_back(&ooo_cpu[i].ITLB);
        ALL_CACHES.push_back(&ooo_cpu[i].DTLB);
        ALL_CACHES.push_back(&ooo_cpu[i].STLB);

        ALL_CACHES.push_back(&ooo_cpu[i].L1I);
        ALL_CACHES.push_back(&ooo_cpu[i].L1D);
        ALL_CACHES.push_back(&ooo_cpu[i].L2C);
    }
    ALL_CACHES.push_back(&uncore.LLC);
    // ALL_CACHES.push_back(&uncore.DRAM); // INCORECT!!!
#endif

    uncore.LLC.llc_initialize_replacement();
    uncore.LLC.llc_prefetcher_initialize();
    IF_BYP_LLC(
    llc_bypass_initialize(0, &ooo_cpu[0].L1D, &ooo_cpu[0].L2C, &uncore.LLC);
    )

    if (false){
        cout << "MAIN.cc ln 788 stats print" << endl;
    } else {
        cout << "MAIN.cc ln 790 stats print to disable" << endl;
        print_knobs();
    }



    // simulation entry point
    start_time = time(NULL);

#ifdef USE_TRACE_HELPER
    trace_helper.start(ooo_cpu);
    // Prefill: wait until all core buffers are half-full before simulation
    for (int i = 0; i < NUM_CPUS; i++) {
        while (true) {
            uint32_t h = trace_helper.buffers[i].head.load(std::memory_order_acquire);
            uint32_t t = trace_helper.buffers[i].tail.load(std::memory_order_relaxed);
            uint32_t used = (h >= t) ? (h - t) : (TRACE_BUFFER_SIZE - t + h);
            if (used >= TRACE_BUFFER_SIZE / 2) break;
            std::this_thread::yield();
        }
    }
#endif

    simulation_loop(show_heartbeat);

#ifdef USE_TRACE_HELPER
    trace_helper.stop();
    cout << endl << "[Trace Helper Stats]" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
        cout << "Core_" << i << "_io_trace_idle " << trace_helper.io_trace_idle[i] << endl;
        cout << "Core_" << i << "_total_produced " << trace_helper.total_produced[i] << endl;
    }
#endif

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

    cout << endl << "ChampSim completed all CPUs" << endl;
//     if (NUM_CPUS > 1) {
//         cout << endl << "Total Simulation Statistics (including warmup)" << endl;
//         for (uint32_t i=0; i<NUM_CPUS; i++) {
//             cout << endl << "CPU ;" << i << "; cumulative IPC: ;" << (float) (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle); 
//             cout << " instructions: " << ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr << " cycles: " << current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle << endl;
// #ifndef CRC2_COMPILE
//             print_sim_stats(i, &ooo_cpu[i].L1D);
//             print_sim_stats(i, &ooo_cpu[i].L1I);
//             print_sim_stats(i, &ooo_cpu[i].L2C);
//             ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
//             ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
// #endif
//             print_sim_stats(i, &uncore.LLC);
//         }
//         uncore.LLC.llc_prefetcher_final_stats();
//     }

    print_end_of_sim_report();

    for (uint32_t i = 0; i < NUM_CPUS; i++) lpm_demand_compute_all(i); // Lazy: refresh met[] before db write
    champsim_db_store(db_cfg, TOTAL_SUM_FINAL_SIM_IPC / NUM_CPUS,
                      major_fault, minor_fault);

    return 0;
}
