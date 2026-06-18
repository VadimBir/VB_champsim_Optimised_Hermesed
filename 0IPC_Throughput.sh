#!/bin/bash

# Performance monitoring with signal-based IPC control
# Type + to add 60s, - to subtract 60s  
# Multiple characters: +++ adds 180s, --- subtracts 180s

# Data storage arrays
declare -a active_cores_history=()
declare -a avg_ipc_history=()
declare -a total_ipc_history=()
declare -a timestamps=()
sudo sysctl -w kernel.perf_event_paranoid=-1
# Global configuration - shared via signals
MEASUREFOR=2
SHIFT_BY=5
START_EPOCH=$(date +%s)
START_STAMP=$(date +'%d/%m:%H')
# Signal handlers for child process
handle_set_interval() {
    local val=$(cat /tmp/perf_newval 2>/dev/null || echo "")
    [[ "$val" =~ ^[0-9]+$ ]] && [ "$val" -gt 0 ] && MEASUREFOR=$val
    echo "$(date '+%H:%M:%S') [SET] → ${MEASUREFOR}s" >&2
    rm -f /tmp/perf_newval
}

# Performance monitoring function - child process
monitor_performance() {
    # Install signal handlers
    trap 'handle_set_interval' SIGUSR1
    
    local i=1
    while true; do
        # Original perf monitoring logic
        # pids=($(pgrep -f "Lib/"))
        # pids=($(pgrep -f "python3"))
    # pids=($(pgrep -f "perceptron"))
    pids=($(pgrep -f "/champsim "))
        pid_list=$(IFS=,; echo "${pids[*]}")
        # pid_list=$(IFS=,; echo "${pids[*]}")
        
        perf_cache=$(perf stat -e cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses,L1-icache-loads,L1-icache-load-misses,dTLB-loads,dTLB-load-misses,iTLB-loads,iTLB-load-misses,branch-instructions,branch-misses,instructions,cycles,context-switches,cpu-migrations,stalled-cycles-frontend -p $pid_list sleep $MEASUREFOR 2>&1) >/dev/null 2>&1
        
        # Extract performance metrics
        cache_miss_pct=$(echo "$perf_cache" | grep "cache-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        l1d_miss_pct=$(echo "$perf_cache" | grep "L1-dcache-load-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        l1i_miss_pct=$(echo "$perf_cache" | grep "L1-icache-load-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        dtlb_miss_pct=$(echo "$perf_cache" | grep "dTLB-load-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        itlb_miss_pct=$(echo "$perf_cache" | grep "iTLB-load-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        branch_miss_pct=$(echo "$perf_cache" | grep "branch-misses" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        ipc=$(echo "$perf_cache" | grep "insn per cycle" | sed -n 's/.*#[[:space:]]*\([0-9.]*\)[[:space:]]*insn per cycle.*/\1/p' | tail -1)
        context_switches=$(echo "$perf_cache" | grep "context-switches" | awk '{print $(NF-1)}' | tail -1)
        cpu_migrations=$(echo "$perf_cache" | grep "cpu-migrations" | awk '{print $(NF-1)}' | tail -1)
        frontend_stalls_pct=$(echo "$perf_cache" | grep "stalled-cycles-frontend" | sed -n 's/.*#[[:space:]]*\([0-9.]*%\).*/\1/p' | tail -1)
        cache_miss_raw=$(echo "$perf_cache" | grep "cache-misses" | awk '{gsub(/,/,""); print $1}' | tail -1)
        dram_rps=$(awk "BEGIN{printf \"%.0f\", ${cache_miss_raw:-0} / $MEASUREFOR}")
        dram_bw=$(awk "BEGIN{printf \"%.1f\", ${cache_miss_raw:-0} * 64.0 / $MEASUREFOR / 1073741824}")
        
        # Calculate derived metrics
        proc_count=${#pids[@]}
        ipc_tot=$(echo "$ipc * $proc_count" | bc -l)
        local elapsed_s=$(( $(date +%s) - START_EPOCH ))
        local eh=$((elapsed_s / 3600)) em=$(( (elapsed_s % 3600) / 60 )) es=$((elapsed_s % 60))
        # tot_time=$((i*MEASUREFOR/60))
        # if proc count is 0 then sleep for 7 minutes
        [[ $proc_count -eq 0 ]] && { echo "$(date '+%H:%M:%S') No active processes found. Sleeping for 5 minutes..." >&2; sleep 360; continue; }
        
        local tl="" mi_l="" ma_l=""
        for cn in 1 4 8 16; do
            local ts=0 tc=0 cn_mi=99999 cn_ma=0
            local sym; case $cn in 1) sym="①";; 4) sym="④";; 8) sym="⑧";; 16) sym="⑯";; esac
            for lf in "${tpmi_files[@]}"; do
                [[ "$lf" == */core${cn}/* ]] || continue
                local tv=$(grep -oP 'TPMI \K[0-9.]+' "$lf" | tail -1)
                [ -n "$tv" ] || continue
                read ts tc cn_mi cn_ma <<< $(awk "BEGIN{nv=$tv/$cn; ts=$ts+nv; mi=($tv<$cn_mi)?$tv:$cn_mi; ma=($tv>$cn_ma)?$tv:$cn_ma; printf \"%.6f %d %.2f %.2f\",ts,$tc+1,mi,ma}")
            done
            if [ $tc -gt 0 ]; then
                local avg=$(awk "BEGIN{printf \"%.2f\", $ts/$tc}")
                local dn=$(awk "BEGIN{printf \"%.2f\", $avg * $cn}")
                local rt=$(awk "BEGIN{printf \"%.2f\", $tc / $dn}")
                tl="${tl} \033[36m${sym}\033[37m\033[1m${tc}\033[0m\033[36m/\033[37m\033[1m${dn}\033[0m\033[36m=\033[37m\033[1m${rt}\033[0m"
                mi_l="${mi_l}\033[36m${sym}\033[37m\033[1m$(printf "%.2f" $cn_mi)\033[0m "
                ma_l="${ma_l}\033[36m${sym}\033[37m\033[1m$(printf "%.2f" $cn_ma)\033[0m "
            fi
        done
        printf "\n\033[32mT:\033[31m\033[1m%dh%2dm%2ds\033[0m \033[35mTPMI\033[0m$(echo -e "$tl") \033[33mMi\033[0m $(echo -e "$mi_l")\033[33mMa\033[0m $(echo -e "$ma_l")\033[35mIPC\033[0m \033[36mΣ\033[37m\033[1m%s\033[0m \033[36mP\033[37m\033[1m%3d\033[0m \033[36mavg\033[37m\033[1m%5s\033[0m \033[36mSW\033[37m\033[1m%s\033[0m \033[36mMig\033[37m\033[1m%s\033[0m \033[33mFrnt \033[37m\033[1m%s\033[0m \033[36mCond \033[37m\033[1m%s%%\033[0m\n" "$eh" "$em" "$es" "$ipc_tot" "$proc_count" "$ipc" "$context_switches" "$cpu_migrations" "$frontend_stalls_pct" "$(echo "$branch_miss_pct" | sed 's/%//' | awk '{printf "%.1f", $1}')"
        printf "\033[31mRAMreq \033[33m\033[1m%sM\033[0m \033[31mBnd \033[37m\033[1m%s\033[0m\033[31mGBs\033[0m \033[35mCPUmem:\033[0m \033[33mToT \033[37m\033[1m%6s\033[0m \033[36mdTLB \033[37m\033[1m%s\033[0m \033[36mL1D \033[37m\033[1m%s\033[0m \033[36miTLB \033[37m\033[1m%s\033[0m \033[36mL1I \033[37m\033[1m%s\033[0m\n" "$(awk "BEGIN{printf \"%.0f\", $dram_rps / 1000000}")" "$dram_bw" "$cache_miss_pct" "$dtlb_miss_pct" "$l1d_miss_pct" "$itlb_miss_pct" "$l1i_miss_pct"
        mapfile -t tpmi_files < <(find outputs/*/core* -maxdepth 1 -name "*.log" -mmin -60 -type f)

        i=$((i+1))
    done
}

process_input() {
    local input="$1"
    [[ "$input" =~ ^[0-9]+$ ]] && [ "$input" -gt 0 ] && { echo "$input" > /tmp/perf_newval; kill -SIGUSR1 $MONITOR_PID; }
}



# Cleanup function
cleanup() {
    kill $MONITOR_PID 2>/dev/null
    wait $MONITOR_PID 2>/dev/null
    echo -e "\n\033[0mMonitoring stopped."
    exit 0
}

trap cleanup EXIT INT TERM

# Start performance monitoring in background
monitor_performance &
MONITOR_PID=$!

# Display instructions
printf "\033[36mType seconds to set interval. Ctrl+C to exit.\033[0m\n"

while true; do
    printf "\033[90m[secs]>\033[0m"
    read -r user_input
    process_input "$user_input"
done
