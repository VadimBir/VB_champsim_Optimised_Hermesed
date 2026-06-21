#!/usr/bin/env bash
# merge_valid_logs.sh — merge valid sim logs from out03 + out06 into out_merged_03_06
# Valid = contains "FINAL ROI CORE AVG IPC"
# out03 processed FIRST → wins on filename conflict (newer rerun)
set -uo pipefail
shopt -s nullglob

BASE="/home/cc/champsim_VB/outputs"
SRC_DIRS=("$BASE/out03" "$BASE/out06")
DEST="$BASE/out_merged_03_06"
CORE_DIRS=(core1 core2 core4 core8 core16)
MANIFEST="$DEST/manifest.csv"
WARNINGS="$DEST/warnings.log"

# Create output dirs
for cd in "${CORE_DIRS[@]}"; do
    mkdir -p "$DEST/$cd"
done

# Init manifest + warnings
printf 'core,filename,ipc,source_dir\n' > "$MANIFEST"
: > "$WARNINGS"

copied=0
skipped=0
warned=0

for src in "${SRC_DIRS[@]}"; do
    src_name="$(basename "$src")"
    for cd in "${CORE_DIRS[@]}"; do
        src_core="$src/$cd"
        [[ -d "$src_core" ]] || continue
        for f in "$src_core"/*.log; do
            [[ -f "$f" ]] || continue
            # Check if log has FINAL ROI CORE AVG IPC
            if grep -q "FINAL ROI CORE AVG IPC" "$f"; then
                fname="$(basename "$f")"
                dest_file="$DEST/$cd/$fname"
                # Extract IPC value
                ipc="$(grep -oP 'FINAL ROI CORE AVG IPC: ;\K[0-9.]+' "$f" | head -1)"
                [[ -z "$ipc" ]] && ipc="PARSE_ERROR"

                if [[ -f "$dest_file" ]]; then
                    # Dup — dest already has this file (from out03, processed first)
                    existing_ipc="$(grep -oP 'FINAL ROI CORE AVG IPC: ;\K[0-9.]+' "$dest_file" | head -1)"
                    if [[ "$ipc" != "$existing_ipc" ]]; then
                        echo "[WARN] IPC mismatch: $cd/$fname — $src_name=$ipc vs existing=$existing_ipc" >> "$WARNINGS"
                        ((warned++))
                    fi
                    ((skipped++))
                    # Keep existing (out03 = winner)
                else
                    cp "$f" "$dest_file"
                    printf '%s,%s,%s,%s\n' "$cd" "$fname" "$ipc" "$src_name" >> "$MANIFEST"
                    ((copied++))
                fi
            fi
        done
    done
done

echo "=== MERGE COMPLETE ==="
echo "Copied: $copied"
echo "Skipped (dup): $skipped"
echo "Warnings (IPC mismatch): $warned"
echo "Manifest: $MANIFEST"
echo "Warnings log: $WARNINGS"
