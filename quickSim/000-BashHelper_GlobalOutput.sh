#!/bin/bash
TARGET_OUTPUT_FILE=""

# # Set target file
set_output_file() {
    echo "Setting output file to $1"
    TARGET_OUTPUT_FILE="$1"
    touch "$TARGET_OUTPUT_FILE"
}

# add_full_config_toRun() {
#     local run_id=$1
#     local arch_info=$2
#     local trace_info=$3
#     local l1d_prefetcher=$4
#     local l2c_prefetcher=$5
#     local llc_prefetcher=$6
#     local new_line="ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};"
    
    
#     if ! grep -q "^RUN:${run_id};" "$TARGET_OUTPUT_FILE"; then
#         echo "ERROR: init RUN:${run_id} entry not found" >&2
#         exit 1
#     fi

# exec 200<> "$TARGET_OUTPUT_FILE"
# flock 200

#     # Infinite retry until success
#     for ((i=1; i<=10; i++)); do
#         # sed -i "s/^RUN:${run_id};/&${new_line}/" "$TARGET_OUTPUT_FILE"
#         sed -i "s/^RUN:${run_id};/&${new_line}/" "$TARGET_OUTPUT_FILE"
#         sync
#         # Check if succeeded
#         if grep -q "^RUN:${run_id};${new_line}" "$TARGET_OUTPUT_FILE"; then
#             break
#         fi
#         echo "ERROR: sed failed on add_full_config_toRun for RUN:${run_id} retrying..." >&2
#         # Random wait 0-0.1s and retry
#         sleep $(awk 'BEGIN{srand(); print rand()*0.1}')
#     done    
#     exec 200>&-
# }


# # Initialize RUN line (append if does not exist)
# init_run_line() {
#     local run_id=$1
#     local new_line="RUN:${run_id};"
#     local temp_content
    


# exec 200>> "$TARGET_OUTPUT_FILE"
# flock 200

# # Keep trying until line exists
# while ! grep -q "^RUN:${run_id};" "$TARGET_OUTPUT_FILE"; do
#     # echo "$new_line" >> "$TARGET_OUTPUT_FILE"
#     echo "$new_line" >&200
#     sync
#     sleep $(awk 'BEGIN{srand(); print rand()*0.1}')
# done


# exec 200>&-

#     # exec 200>> "$TARGET_OUTPUT_FILE"
#     # flock 200
    
#     # temp_content=$(cat "$TARGET_OUTPUT_FILE")
    
#     # if ! echo "$temp_content" | grep -q "^RUN:${run_id};"; then
#     #     echo "$new_line" >> "$TARGET_OUTPUT_FILE"
        
#     #     # Verify write
#     #     if ! tail -1 "$TARGET_OUTPUT_FILE" | grep -q "^${new_line}$"; then
#     #         echo "ERROR: Write verification failed for RUN:${run_id}" >&2
#     #         exit 1
#     #     else
#     #         tail -1 "$TARGET_OUTPUT_FILE" | grep -q "^${new_line}$"
#     #     fi
#     # fi
    
#     # exec 200>&-
# }
simulation_file_postProcessing() {
    local run_id=$1
    local arch_info=$2
    local trace_info=$3
    local l1d_prefetcher=$4
    local l2c_prefetcher=$5
    local llc_prefetcher=$6
    local output_file=$7
    local num_cores=$8

    sed -i 's/\x00/null/g' "$output_file"
    # Extract instruction counts and IPC
    WARMUP_INSTR=$(grep "warmup_instructions" "$output_file" | grep -o '[0-9]*')
    SIM_INSTR=$(grep "simulation_instructions" "$output_file" | grep -o '[0-9]*')
    AVG_IPC=$(grep "FINAL ROI CORE AVG IPC:" "$output_file" | sed 's/.*FINAL ROI CORE AVG IPC: ;\([^;]*\);.*/\1/' | tr -d ' ')
    
    # Build global_results string
    # global_results="WARMUP:${WARMUP_INSTR};SIMULATIONS:${SIM_INSTR};AVG_IPC:${AVG_IPC};"
    global_results="RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};WARMUP:${WARMUP_INSTR};SIMULATIONS:${SIM_INSTR};AVG_IPC:${AVG_IPC};"
    # Extract ALL cache statistics
    for ((core=0; core<num_cores; core++)); do
        for cache in L1D L1I L2C LLC; do
            MPKI=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*MPKI;\([^;]*\);.*/\1/' | tr -d ' ')
            LOAD_MSHR_cap=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*LOAD_MSHR_cap;\([^;]*\);.*/\1/' | tr -d ' ')
            RFO_cap=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*RFO_cap;\([^;]*\);.*/\1/' | tr -d ' ')
            Pf_MSHR_cap=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*Pf_MSHR_cap;\([^;]*\);.*/\1/' | tr -d ' ')
            WrBk_MSHR_cap=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*WrBk_MSHR_cap;\([^;]*\);.*/\1/' | tr -d ' ')
            TOTAL_ACCESS=$(grep "Core_;${core};_; ${cache};_.*total_access" "$output_file" | sed 's/.*total_access;\([^;]*\);.*/\1/' | tr -d ' ')
            TOTAL_HIT=$(grep "Core_;${core};_; ${cache};_.*total_hit" "$output_file" | sed 's/.*total_hit;\([^;]*\);.*/\1/' | tr -d ' ')
            TOTAL_MISS=$(grep "Core_;${core};_; ${cache};_.*total_miss" "$output_file" | sed 's/.*total_miss;\([^;]*\);.*/\1/' | tr -d ' ')
            LOADS=$(grep "Core_;${core};_; ${cache};_.*loads" "$output_file" | sed 's/.*loads;\([^;]*\);.*/\1/' | tr -d ' ')
            LOAD_HIT=$(grep "Core_;${core};_; ${cache};_.*load_hit" "$output_file" | sed 's/.*load_hit;\([^;]*\);.*/\1/' | tr -d ' ')
            LOAD_MISS=$(grep "Core_;${core};_; ${cache};_.*load_miss" "$output_file" | sed 's/.*load_miss;\([^;]*\);.*/\1/' | tr -d ' ')
            RFOS=$(grep "Core_;${core};_; ${cache};_.*RFOs" "$output_file" | sed 's/.*RFOs;\([^;]*\);.*/\1/' | tr -d ' ')
            RFO_HIT=$(grep "Core_;${core};_; ${cache};_.*RFO_hit" "$output_file" | sed 's/.*RFO_hit;\([^;]*\);.*/\1/' | tr -d ' ')
            RFO_MISS=$(grep "Core_;${core};_; ${cache};_.*RFO_miss" "$output_file" | sed 's/.*RFO_miss;\([^;]*\);.*/\1/' | tr -d ' ')
            PREFETCHES=$(grep "Core_;${core};_; ${cache};_.*prefetches" "$output_file" | sed 's/.*prefetches;\([^;]*\);.*/\1/' | tr -d ' ')
            PREFETCH_HIT=$(grep "Core_;${core};_; ${cache};_.*prefetch_hit" "$output_file" | sed 's/.*prefetch_hit;\([^;]*\);.*/\1/' | tr -d ' ')
            PREFETCH_MISS=$(grep "Core_;${core};_; ${cache};_.*prefetch_miss" "$output_file" | sed 's/.*prefetch_miss;\([^;]*\);.*/\1/' | tr -d ' ')
            WRITEBACKS=$(grep "Core_;${core};_; ${cache};_.*writebacks" "$output_file" | sed 's/.*writebacks;\([^;]*\);.*/\1/' | tr -d ' ')
            WRITEBACK_HIT=$(grep "Core_;${core};_; ${cache};_.*writeback_hit" "$output_file" | sed 's/.*writeback_hit;\([^;]*\);.*/\1/' | tr -d ' ')
            WRITEBACK_MISS=$(grep "Core_;${core};_; ${cache};_.*writeback_miss" "$output_file" | sed 's/.*writeback_miss;\([^;]*\);.*/\1/' | tr -d ' ')
            PF_REQUESTED=$(grep "Core_;${core};_; ${cache};_.*pf_requested" "$output_file" | sed 's/.*pf_requested;\([^;]*\);.*/\1/' | tr -d ' ')
            PF_ISSUED=$(grep "Core_;${core};_; ${cache};_.*pf_issued" "$output_file" | sed 's/.*pf_issued;\([^;]*\);.*/\1/' | tr -d ' ')
            PF_USEFUL=$(grep "Core_;${core};_; ${cache};_.*pf_useful" "$output_file" | sed 's/.*pf_useful;\([^;]*\);.*/\1/' | tr -d ' ')
            PF_USELESS=$(grep "Core_;${core};_; ${cache};_.*pf_useless" "$output_file" | sed 's/.*pf_useless;\([^;]*\);.*/\1/' | tr -d ' ')
            PF_LATE=$(grep "Core_;${core};_; ${cache};_.*pf_late" "$output_file" | sed 's/.*pf_late;\([^;]*\);.*/\1/' | tr -d ' ')
            AVG_MISS_LAT=$(grep "Core_;${core};_; ${cache};_.*avg_miss_lat" "$output_file" | sed 's/.*avg_miss_lat;\([^;]*\);.*/\1/' | tr -d ' ')
        
            global_results+="CORE:${core};${cache};MPKI:${MPKI};TOTAL_ACCESS:${TOTAL_ACCESS};TOTAL_HIT:${TOTAL_HIT};TOTAL_MISS:${TOTAL_MISS};LOADS:${LOADS};LOAD_HIT:${LOAD_HIT};LOAD_MISS:${LOAD_MISS};RFOS:${RFOS};RFO_HIT:${RFO_HIT};RFO_MISS:${RFO_MISS};PREFETCHES:${PREFETCHES};PREFETCH_HIT:${PREFETCH_HIT};PREFETCH_MISS:${PREFETCH_MISS};WRITEBACKS:${WRITEBACKS};WRITEBACK_HIT:${WRITEBACK_HIT};WRITEBACK_MISS:${WRITEBACK_MISS};PF_REQUESTED:${PF_REQUESTED};PF_ISSUED:${PF_ISSUED};PF_USEFUL:${PF_USEFUL};PF_USELESS:${PF_USELESS};PF_LATE:${PF_LATE};AVG_MISS_LAT:${AVG_MISS_LAT};"
        done
    done

    # Check for pipe character in global_results
    if [[ "$global_results" == *"|"* ]]; then
        echo "ERROR: global_results contains pipe character" >&2
    fi

    # Keep trying until the expected line exists
    
    echo "$global_results" >> "${TARGET_OUTPUT_FILE}_${run_id}"
    # sed -i "s#${search_pattern}#&${global_results}#" "$TARGET_OUTPUT_FILE"
    sync
}
merge_simulation_results() {
    local total_num_runs=$1
    
    # > "${TARGET_OUTPUT_FILE}" # create empty file 
    
    for ((run_id=1; run_id<=total_num_runs; run_id++)); do
        if [[ -f "${TARGET_OUTPUT_FILE}_${run_id}" ]]; then
            cat "${TARGET_OUTPUT_FILE}_${run_id}" >> "${TARGET_OUTPUT_FILE}"
        fi
    done
}
# simulation_file_postProcessing() {
#     local run_id=$1
#     local arch_info=$2
#     local trace_info=$3
#     local l1d_prefetcher=$4
#     local l2c_prefetcher=$5
#     local llc_prefetcher=$6
#     local output_file=$7
#     local num_cores=$8

#     # nullTerminatedLog=$(tr -d '\0' < "$output_file")
#     # output_file=$nullTerminatedLog
#     # echo "POST OUTPUT FILE: $output_file"
#     sed -i 's/\x00/null/g' "$output_file"
#     # Extract instruction counts and IPC
#     WARMUP_INSTR=$(grep "warmup_instructions" "$output_file" | grep -o '[0-9]*')
#     SIM_INSTR=$(grep "simulation_instructions" "$output_file" | grep -o '[0-9]*')
#     AVG_IPC=$(grep "FINAL SIM ONLY AVG IPC:" "$output_file" | sed 's/.*FINAL SIM ONLY AVG IPC: ;\([^;]*\);.*/\1/' | tr -d ' ')
    
#     # Build global_results string
#     global_results="WARMUP:${WARMUP_INSTR};SIMULATIONS:${SIM_INSTR};AVG_IPC:${AVG_IPC};"
    
#     # Extract ALL cache statistics
#     for ((core=0; core<num_cores; core++)); do
#         for cache in L1D L1I L2C LLC; do
#             MPKI=$(grep "Core_;${core};_; ${cache};_.*MPKI" "$output_file" | sed 's/.*MPKI;\([^;]*\);.*/\1/' | tr -d ' ')
#             TOTAL_ACCESS=$(grep "Core_;${core};_; ${cache};_.*total_access" "$output_file" | sed 's/.*total_access;\([^;]*\);.*/\1/' | tr -d ' ')
#             TOTAL_HIT=$(grep "Core_;${core};_; ${cache};_.*total_hit" "$output_file" | sed 's/.*total_hit;\([^;]*\);.*/\1/' | tr -d ' ')
#             TOTAL_MISS=$(grep "Core_;${core};_; ${cache};_.*total_miss" "$output_file" | sed 's/.*total_miss;\([^;]*\);.*/\1/' | tr -d ' ')
#             LOADS=$(grep "Core_;${core};_; ${cache};_.*loads" "$output_file" | sed 's/.*loads;\([^;]*\);.*/\1/' | tr -d ' ')
#             LOAD_HIT=$(grep "Core_;${core};_; ${cache};_.*load_hit" "$output_file" | sed 's/.*load_hit;\([^;]*\);.*/\1/' | tr -d ' ')
#             LOAD_MISS=$(grep "Core_;${core};_; ${cache};_.*load_miss" "$output_file" | sed 's/.*load_miss;\([^;]*\);.*/\1/' | tr -d ' ')
#             RFOS=$(grep "Core_;${core};_; ${cache};_.*RFOs" "$output_file" | sed 's/.*RFOs;\([^;]*\);.*/\1/' | tr -d ' ')
#             RFO_HIT=$(grep "Core_;${core};_; ${cache};_.*RFO_hit" "$output_file" | sed 's/.*RFO_hit;\([^;]*\);.*/\1/' | tr -d ' ')
#             RFO_MISS=$(grep "Core_;${core};_; ${cache};_.*RFO_miss" "$output_file" | sed 's/.*RFO_miss;\([^;]*\);.*/\1/' | tr -d ' ')
#             PREFETCHES=$(grep "Core_;${core};_; ${cache};_.*prefetches" "$output_file" | sed 's/.*prefetches;\([^;]*\);.*/\1/' | tr -d ' ')
#             PREFETCH_HIT=$(grep "Core_;${core};_; ${cache};_.*prefetch_hit" "$output_file" | sed 's/.*prefetch_hit;\([^;]*\);.*/\1/' | tr -d ' ')
#             PREFETCH_MISS=$(grep "Core_;${core};_; ${cache};_.*prefetch_miss" "$output_file" | sed 's/.*prefetch_miss;\([^;]*\);.*/\1/' | tr -d ' ')
#             WRITEBACKS=$(grep "Core_;${core};_; ${cache};_.*writebacks" "$output_file" | sed 's/.*writebacks;\([^;]*\);.*/\1/' | tr -d ' ')
#             WRITEBACK_HIT=$(grep "Core_;${core};_; ${cache};_.*writeback_hit" "$output_file" | sed 's/.*writeback_hit;\([^;]*\);.*/\1/' | tr -d ' ')
#             WRITEBACK_MISS=$(grep "Core_;${core};_; ${cache};_.*writeback_miss" "$output_file" | sed 's/.*writeback_miss;\([^;]*\);.*/\1/' | tr -d ' ')
#             PF_REQUESTED=$(grep "Core_;${core};_; ${cache};_.*pf_requested" "$output_file" | sed 's/.*pf_requested;\([^;]*\);.*/\1/' | tr -d ' ')
#             PF_ISSUED=$(grep "Core_;${core};_; ${cache};_.*pf_issued" "$output_file" | sed 's/.*pf_issued;\([^;]*\);.*/\1/' | tr -d ' ')
#             PF_USEFUL=$(grep "Core_;${core};_; ${cache};_.*pf_useful" "$output_file" | sed 's/.*pf_useful;\([^;]*\);.*/\1/' | tr -d ' ')
#             PF_USELESS=$(grep "Core_;${core};_; ${cache};_.*pf_useless" "$output_file" | sed 's/.*pf_useless;\([^;]*\);.*/\1/' | tr -d ' ')
#             PF_LATE=$(grep "Core_;${core};_; ${cache};_.*pf_late" "$output_file" | sed 's/.*pf_late;\([^;]*\);.*/\1/' | tr -d ' ')
#             AVG_MISS_LAT=$(grep "Core_;${core};_; ${cache};_.*avg_miss_lat" "$output_file" | sed 's/.*avg_miss_lat;\([^;]*\);.*/\1/' | tr -d ' ')
            
#             global_results+="CORE:${core};${cache};MPKI:${MPKI};TOTAL_ACCESS:${TOTAL_ACCESS};TOTAL_HIT:${TOTAL_HIT};TOTAL_MISS:${TOTAL_MISS};LOADS:${LOADS};LOAD_HIT:${LOAD_HIT};LOAD_MISS:${LOAD_MISS};RFOS:${RFOS};RFO_HIT:${RFO_HIT};RFO_MISS:${RFO_MISS};PREFETCHES:${PREFETCHES};PREFETCH_HIT:${PREFETCH_HIT};PREFETCH_MISS:${PREFETCH_MISS};WRITEBACKS:${WRITEBACKS};WRITEBACK_HIT:${WRITEBACK_HIT};WRITEBACK_MISS:${WRITEBACK_MISS};PF_REQUESTED:${PF_REQUESTED};PF_ISSUED:${PF_ISSUED};PF_USEFUL:${PF_USEFUL};PF_USELESS:${PF_USELESS};PF_LATE:${PF_LATE};AVG_MISS_LAT:${AVG_MISS_LAT};"
#         done
#     done
    
#     #     echo "DEBUG: run_id=[$run_id]"
#     # echo "DEBUG: arch_info=[$arch_info]" 
#     # echo "DEBUG: trace_info=[$trace_info]"
#     # echo "DEBUG: l1d_prefetcher=[$l1d_prefetcher]"
#     # echo "DEBUG: l2c_prefetcher=[$l2c_prefetcher]"
#     # echo "DEBUG: global_results=[$global_results]"

#     # Check for pipe character in global_results
#     if [[ "$global_results" == *"|"* ]]; then
#         echo "ERROR: global_results contains pipe character" >&2
#     fi


#     exec 200<> "$TARGET_OUTPUT_FILE"
#     flock 200

#     search_pattern="^RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};"
#     expected_line="${search_pattern}${global_results}"

#     # First verify the base line exists
#     if ! grep -q "$search_pattern" "$TARGET_OUTPUT_FILE"; then
#         echo "ERROR: Global entry failed Base pattern not found for RUN:${run_id}" >&2
#         exec 200>&-
#         exit 1
#     fi

#     # Keep trying until the expected line exists

#     for ((i=1; i<=10; i++)); do
#         if grep -qF "$expected_line" "$TARGET_OUTPUT_FILE"; then
#             break
#         fi
#         sed -i "s#${search_pattern}#&${global_results}#" "$TARGET_OUTPUT_FILE"
#         sync
#         sleep $(awk 'BEGIN{srand(); print rand()*0.1}')
#     done
# }
# while ! grep -qF "$expected_line" "$TARGET_OUTPUT_FILE"; do
#     sed -i "s#${search_pattern}#&${global_results}#" "$TARGET_OUTPUT_FILE"
#     sync
#     sleep $(awk 'BEGIN{srand(); print rand()*0.1}')
# done


# exec 200>&-

# exec 200>> "$TARGET_OUTPUT_FILE"
# flock 200

# search_pattern="^RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};"
# # later used to verify a given sed Op change has occured. 
# expected_line="RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};${global_results}"


# sed -i "s#^RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};#&${global_results}#" "$TARGET_OUTPUT_FILE"
#     # echo "RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};"
#     if ! grep -q "^RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};" "$TARGET_OUTPUT_FILE"; then
#         echo "ERROR: RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info} not found in output file" >&2
#         exit 1
#     fi

#     sed -i "s#${search_pattern}#&${global_results}#" "$TARGET_OUTPUT_FILE"
#     sync
#     # Verify the line was modified correctly
#     if grep -qF "${expected_after}" "$TARGET_OUTPUT_FILE"; then
#         echo "SUCCESS: sed completed"
#     else
#         echo "FAILED: sed did not modify line correctly" >&2
#         after_line=$(grep "^RUN:${run_id};" "$TARGET_OUTPUT_FILE" | head -1)
#         echo "Before: ${before_line}" >&2
#         echo "After: ${after_line}" >&2
#         echo "Expected: ${expected_after}" >&2
#     fi

#     sed -i "s#^RUN:${run_id};ARCH:${arch_info};TRACE:${trace_info};L1D:${l1d_prefetcher};L2C:${l2c_prefetcher};LLC:${llc_prefetcher};#&${global_results}#" "$TARGET_OUTPUT_FILE"
    
    
#     exec 200>&-
# Initialize with current TARGET_OUTPUT_FILE
if [ -n "$TARGET_OUTPUT_FILE" ]; then
    set_output_file "$TARGET_OUTPUT_FILE"
fi