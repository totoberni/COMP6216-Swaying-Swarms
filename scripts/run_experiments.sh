#!/bin/bash
# Run the 3x3 experiment matrix with optional multi-trial and auto-analysis.
# Usage: ./scripts/run_experiments.sh [--clean] [--repeat N] [--analyze] [build_dir]
#   --clean     Wipe sim-out/out* and sim-out/analysis/ before running
#   --repeat N  Run each config N times (default: 1)
#   --analyze   Auto-run compare_results.py after all experiments
#   build_dir   Build directory (default: build)

set -uo pipefail  # NOT -e: we handle errors per-run

CLEAN=false
REPEAT=1
ANALYZE=false
BUILD_DIR="build"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true; shift ;;
        --repeat) REPEAT="$2"; shift 2 ;;
        --analyze) ANALYZE=true; shift ;;
        -*) echo "Unknown flag: $1"; exit 1 ;;
        *) BUILD_DIR="$1"; shift ;;
    esac
done

BINARY="$BUILD_DIR/boid_swarm"

if [ ! -x "$BINARY" ]; then
    echo "Error: $BINARY not found or not executable. Build first."
    exit 1
fi

echo "=== Swaying Swarms Experiment Runner ==="
echo "Config: repeat=$REPEAT, clean=$CLEAN, analyze=$ANALYZE"
echo ""

# --clean: wipe old data
if $CLEAN; then
    echo "Cleaning sim-out/..."
    rm -rf sim-out/out* sim-out/analysis/
fi

# Warn about existing data if not cleaning
if ! $CLEAN && ls sim-out/out* >/dev/null 2>&1; then
    existing=$(ls -d sim-out/out* 2>/dev/null | wc -l)
    echo "WARNING: $existing existing run(s) in sim-out/. Use --clean to remove, or results will accumulate."
fi

# Run the experiment matrix
CONFIGS=(configs/B*_D*.ini)
N_CONFIGS=${#CONFIGS[@]}
TOTAL=$((N_CONFIGS * REPEAT))
CURRENT=0
FAILURES=0
FAILED_RUNS=""
CAMPAIGN_START=$SECONDS

echo "Running $TOTAL experiments ($N_CONFIGS configs x $REPEAT trial(s))..."
echo ""

for trial in $(seq 1 "$REPEAT"); do
    for config in "${CONFIGS[@]}"; do
        CURRENT=$((CURRENT + 1))
        name=$(basename "$config" .ini)
        run_start=$SECONDS

        echo "[$CURRENT/$TOTAL] Trial $trial/$REPEAT — $name..."

        if "$BINARY" -nogui "$config"; then
            elapsed=$((SECONDS - run_start))
            echo "[$CURRENT/$TOTAL] $name done (${elapsed}s)"
        else
            FAILURES=$((FAILURES + 1))
            FAILED_RUNS="$FAILED_RUNS  Trial $trial: $name\n"
            echo "[$CURRENT/$TOTAL] $name FAILED (exit $?)"
        fi
        echo ""
    done
done

CAMPAIGN_ELAPSED=$((SECONDS - CAMPAIGN_START))
echo "=== Campaign complete: $((TOTAL - FAILURES))/$TOTAL succeeded in ${CAMPAIGN_ELAPSED}s ==="

if [[ $FAILURES -gt 0 ]]; then
    echo ""
    echo "FAILED RUNS ($FAILURES):"
    echo -e "$FAILED_RUNS"
fi

# --analyze: auto-run analysis
if $ANALYZE; then
    echo ""
    echo "=== Running analysis ==="
    python3 scripts/compare_results.py --sim-dir sim-out
fi

echo ""
echo "Results: sim-out/ (${TOTAL} runs, ${N_CONFIGS} configs x ${REPEAT} trial(s))"
if $ANALYZE; then
    echo "Plots:   sim-out/analysis/"
fi
