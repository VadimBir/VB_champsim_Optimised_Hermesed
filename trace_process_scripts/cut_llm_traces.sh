#!/bin/bash
# cut_llm_traces.sh — Cut LLM traces based on phase boundaries
#
# Variant A (--variant A): Cut post-phase for 1024+. Keep 256/512 as-is.
# Variant B (--variant B): Same as A, but 2048+4096 also get 15% proportional cut.
#
# Reads phase CSVs from logs/ to get phase_start (line2) and phase_end (last line).
# Reads from .xzBK backups, writes new .xz files.
#
# Usage: ./cut_llm_traces.sh --variant A|B --traces-dir /path/to/traces --logs-dir /path/to/logs

set -euo pipefail

VARIANT=""
TRACES_DIR=""
LOGS_DIR=""
SHM_DIR="/dev/shm/traces"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TRACE_CUT="${SCRIPT_DIR}/trace_cut"
CUT_PERCENT=15  # for variant B
XZ_LEVEL=9
XZ_THREADS=16

while [[ $# -gt 0 ]]; do
    case "$1" in
        --variant) VARIANT="$2"; shift 2 ;;
        --traces-dir) TRACES_DIR="$2"; shift 2 ;;
        --logs-dir) LOGS_DIR="$2"; shift 2 ;;
        --cut-pct) CUT_PERCENT="$2"; shift 2 ;;
        --shm-dir) SHM_DIR="$2"; shift 2 ;;
        *) echo "Unknown arg: $1"; exit 1 ;;
    esac
done

if [[ -z "$VARIANT" || -z "$TRACES_DIR" || -z "$LOGS_DIR" ]]; then
    echo "Usage: $0 --variant A|B --traces-dir DIR --logs-dir DIR"
    exit 1
fi

if [[ ! -x "$TRACE_CUT" ]]; then
    echo "ERROR: trace_cut not found at $TRACE_CUT"
    exit 1
fi

echo "=== cut_llm_traces.sh ==="
echo "Variant: $VARIANT"
echo "Traces:  $TRACES_DIR"
echo "Logs:    $LOGS_DIR"
echo "Cut %:   $CUT_PERCENT (variant B only, for 2048+4096)"
echo ""

for bk in "$TRACES_DIR"/LLM*.champsimtrace.xzBK; do
    base=$(basename "$bk" .champsimtrace.xzBK)

    # Prefer reading from SHM (faster I/O) — use original .xz there
    shm_src="$SHM_DIR/${base}.champsimtrace.xz"
    if [[ -f "$shm_src" ]]; then
        input_file="$shm_src"
    else
        input_file="$bk"
    fi
    csv="$LOGS_DIR/${base}.phase_fine.csv"

    if [[ ! -f "$csv" ]]; then
        echo "[SKIP] $base — no phase CSV found"
        continue
    fi

    # Extract context size (256, 512, 1024, 2048, 4096)
    ctx=$(echo "$base" | grep -oP 'LLM\K[0-9]+')

    # Get phase_start (line 2, field 1) and phase_end (last line, field 1)
    phase_start=$(sed -n '2p' "$csv" | cut -d',' -f1 | tr -d ' ')
    phase_end=$(tail -1 "$csv" | cut -d',' -f1 | tr -d ' ')

    echo "--- $base ---"
    echo "  ctx=$ctx  phase_start=$phase_start  phase_end=$phase_end"

    # Decide action
    skip=0
    limit=0
    action="none"

    if [[ "$VARIANT" == "B" && "$ctx" -ge 2048 ]]; then
        # 15% proportional cut + post-phase cut
        action="cut_proportional"
        skip=$(python3 -c "print(int($phase_start * $CUT_PERCENT / 100))")
        new_phase_len=$(python3 -c "print(int(($phase_end - $phase_start) * (100 - $CUT_PERCENT) / 100))")
        new_before=$(python3 -c "print(int($phase_start * (100 - $CUT_PERCENT) / 100))")
        limit=$((new_before + new_phase_len))
    else
        # Cut post-phase only (Variant A for 1024+, or Variant B for 1024)
        action="cut_post"
        limit=$phase_end
    fi

    # Extract original _XXXM from backup filename
    orig_m=$(echo "$base" | grep -oP '_\K[0-9]+(?=M$)')

    if [[ "$action" == "cut_post" ]]; then
        total_out=$limit
        echo "  action=CUT_POST  limit=$limit"
    else
        total_out=$limit
        echo "  action=CUT_PROPORTIONAL  skip=$skip  limit=$limit  cut=${CUT_PERCENT}%"
    fi

    # New filename with updated _XXXM + variant tag
    total_m=$(python3 -c "print(int(round($total_out / 1000000)))")
    if [[ "$action" == "cut_post" ]]; then
        tag="noPostPhase"
    else
        tag="noEnd${CUT_PERCENT}pPhase"
    fi
    # LLM4096.OPT-350M_10532M → LLM4096.OPTnoPostPhase-10532M_4236M
    llm_prefix=$(echo "$base" | grep -oP '^LLM[0-9]+')
    model_name=$(echo "$base" | grep -oP '\.(\K[A-Za-z]+)(?=-)')
    model_param=$(echo "$base" | grep -oP '-\K[0-9]+M(?=_)')
    new_name="${llm_prefix}.${model_name}${tag}-${orig_m}M_${total_m}M"

    echo "  output: ${new_name}.champsimtrace.xz  (${total_m}M instrs)"

    # Remove old .xz if exists (could have different name due to _XXXM change)
    if [[ "$new_name" != "$base" ]]; then
        rm -f "$TRACES_DIR/${base}.champsimtrace.xz"
    fi

    # Stream: trace_cut -> xz -> new file (write to SHM first, then copy)
    shm_out="$SHM_DIR/${new_name}.champsimtrace.xz"
    "$TRACE_CUT" "$input_file" --skip "$skip" --limit "$limit" \
        | xz -${XZ_LEVEL} -c -T ${XZ_THREADS} > "$shm_out"

    # Copy from SHM to traces dir
    cp "$shm_out" "$TRACES_DIR/${new_name}.champsimtrace.xz"

    # Verify output exists and is non-empty
    out_size=$(stat --format=%s "$TRACES_DIR/${new_name}.champsimtrace.xz")
    echo "  size: ${out_size} bytes"

    if [[ "$out_size" -lt 100 ]]; then
        echo "  ERROR: output too small, something went wrong!"
        exit 1
    fi

    echo "  OK"
    echo ""
done

echo "=== DONE ==="
