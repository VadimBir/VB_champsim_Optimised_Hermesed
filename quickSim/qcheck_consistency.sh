HERE="$(cd "$(dirname "$0")" && pwd)"
echo "NO BYPASS CONSISTENCY:"

noByP=$("$HERE/quick.sh" -p 1 --L1 no --L2 no --L3 no \
--trace LLM256.Pythia-70M_21M -d 1 -c 2 -bypca --l1byp no -f \
2>&1 | grep -oP '(?<=FINAL ROI CORE AVG IPC: ;)[^;]+' | tail -n1)

echo "NO BYPASS IPC: $noByP"

if [ -z "$noByP" ]; then
    echo "ERROR: NO IPC FOUND"
    exit 1
fi

ref1="0.81575"

# check baseline vs reference
awk -v a="$noByP" -v b="$ref1" 'BEGIN {
    diff = (a > b) ? a - b : b - a;
    if (diff >= 0.000001) exit 1;
}'

if [ $? -ne 0 ]; then
    echo "FAILED: BASELINE != REF → STOP"
    exit 1
fi

echo "BASELINE OK → RUNNING BYPASS TEST"

echo "TESTING BYPASS CONSISTENCY:"
ref2="0.82183"
ByP=$("$HERE/quick.sh" -p 1 --L1 no --L2 no --L3 no \
--trace LLM256.Pythia-70M_21M -d 1 -c 2 -bypca --l1byp ByP_w_capacityDemandProjection_3_derived -f \
2>&1 | grep -oP '(?<=FINAL ROI CORE AVG IPC: ;)[^;]+' | tail -n1)

echo "BYPASS IPC: $ByP"

if [ -z "$ByP" ]; then
    echo "ERROR: NO IPC FOUND"
    exit 1
fi

# check bypass vs baseline
awk -v a="$ByP" -v b="$ref2" 'BEGIN {
    diff = (a > b) ? a - b : b - a;
    if (diff < 0.000001) {
        print "EQUIVALENT";
    } else {
        print "NOT EQUIVALENT";
        exit 1;
    }
}'