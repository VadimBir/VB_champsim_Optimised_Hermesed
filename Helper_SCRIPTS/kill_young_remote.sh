#!/bin/bash
REMOTE_HOSTS=(10.52.3.46 10.52.3.166 10.52.2.124 10.52.1.108 10.52.2.153 10.52.3.1 10.52.0.126 10.52.3.96)
KEY="/home/cc/.ssh/id_rsa"
REMOTE_SCRIPT='/tmp/kill_young_local.sh'

cat > /tmp/kill_young_local.sh <<'EOF'
#!/bin/bash
MAX_AGE=3600
before=$(pgrep -c champsim)
young=$(ps -eo pid,etimes,comm --no-headers | awk -v m=$MAX_AGE '$3=="champsim" && $2 < m {print $1}')
n_young=$(echo "$young" | grep -c .)
if [[ $n_young -gt 0 ]]; then
  echo "$young" | xargs -r kill -TERM
  sleep 3
  echo "$young" | xargs -r kill -KILL
fi
after=$(pgrep -c champsim)
echo "before=$before young_killed=$n_young after=$after"
EOF

for h in "${REMOTE_HOSTS[@]}"; do
  echo "=== $h ==="
  scp -i "$KEY" -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes /tmp/kill_young_local.sh cc@$h:$REMOTE_SCRIPT < /dev/null
  ssh -n -i "$KEY" -o StrictHostKeyChecking=no -o ConnectTimeout=10 -o BatchMode=yes cc@$h "bash $REMOTE_SCRIPT"
done
