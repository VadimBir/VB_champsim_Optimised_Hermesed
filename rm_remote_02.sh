#!/bin/bash
REMOTE_HOSTS=(10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96)
KEY="/home/cc/.ssh/id_rsa"
for h in "${REMOTE_HOSTS[@]}"; do
  echo "=== $h ==="
  ssh -n -i "$KEY" -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes cc@$h "rm -rf /home/cc/champsim_VB/outputs/Hx_out_LLM-4096-2048-1024-512-256-02; ls -d /home/cc/champsim_VB/outputs/Hx_out_LLM-4096-2048-1024-512-256-* 2>/dev/null || echo CLEAN"
done
