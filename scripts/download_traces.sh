#!/bin/bash
P_MAT=4

run_download() {
    local FILE=$1
    local OUTDIR=$2
    mkdir -p "$OUTDIR"
    echo "Downloading from $FILE into $OUTDIR ..."
    cat "$FILE" | xargs -n 1 -P $P_MAT -I {} wget -P "$OUTDIR" -c https://dpc3.compas.cs.stonybrook.edu/champsim-traces/speccpu/{}
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

run_download "$SCRIPT_DIR/SPEC06_ALLmemTraces.txt" "$SCRIPT_DIR/../traces_SPEC2006"
P_MAT=3
# run_download "$SCRIPT_DIR/SPEC17_ALLmemTraces.txt" "$SCRIPT_DIR/../traces_SPEC2017"

echo "All downloads completed!"
