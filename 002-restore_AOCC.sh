set -e  # Exit on any error

echo "=== AOCC Setup Script ==="

# Step 1: Combine p1 and p2
echo "Step 1: Combining aocc-p1 and aocc-p2..."
cat aocc-p1 aocc-p2 > aocc-compiler-5.0.0.tar

# Verify combination
echo "Verifying combined file..."
if [ ! -f aocc-compiler-5.0.0.tar ]; then
    echo "ERROR: Failed to create combined tar file"
    exit 1
fi

SIZE=$(stat -c%s aocc-compiler-5.0.0.tar)
echo "Combined file size: $SIZE bytes"

echo "AOCC Archive restored successfully."