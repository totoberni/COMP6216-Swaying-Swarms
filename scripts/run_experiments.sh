#!/bin/bash
# Run all 9 experiments in the 3x3 matrix
# Usage: ./scripts/run_experiments.sh [build_dir]
#   build_dir defaults to "build"

set -euo pipefail

BUILD_DIR="${1:-build}"
BINARY="$BUILD_DIR/boid_swarm"

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found or not executable. Build first."
    exit 1
fi

CONFIGS=(configs/B*_D*.ini)
TOTAL=${#CONFIGS[@]}
CURRENT=0

echo "=== Swaying Swarms Experiment Runner ==="
echo "Running $TOTAL experiments..."
echo ""

for config in "${CONFIGS[@]}"; do
    CURRENT=$((CURRENT + 1))
    name=$(basename "$config" .ini)
    echo "[$CURRENT/$TOTAL] Running $name..."
    "$BINARY" -nogui "$config"
    echo "[$CURRENT/$TOTAL] $name complete."
    echo ""
done

echo "=== All $TOTAL experiments complete ==="
echo "Results in sim-out/"
