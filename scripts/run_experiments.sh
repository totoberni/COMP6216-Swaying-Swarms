#!/bin/bash
# Run experiments with optional multi-trial, parallel execution, and auto-analysis.
# Usage: ./scripts/run_experiments.sh [options] [build_dir]
#   --clean          Wipe sim-out/out* and sim-out/analysis/ before running
#   --repeat N       Run each config N times (default: 1)
#   --analyze        Auto-run compare_results.py after all experiments
#   --parallel N     Run up to N simulations in parallel (default: 1)
#   --configs-dir D  Use configs from directory D instead of configs/B*_D*.ini
#   --duration S     Override nogui_duration in each config
#   build_dir        Build directory (default: build)

set -uo pipefail  # NOT -e: we handle errors per-run

CLEAN=false
REPEAT=1
ANALYZE=false
BUILD_DIR="build"
N_PARALLEL=1
CONFIGS_DIR=""
OVERRIDE_DURATION=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=true; shift ;;
        --repeat) REPEAT="$2"; shift 2 ;;
        --analyze) ANALYZE=true; shift ;;
        --parallel) N_PARALLEL="$2"; shift 2 ;;
        --configs-dir) CONFIGS_DIR="$2"; shift 2 ;;
        --duration) OVERRIDE_DURATION="$2"; shift 2 ;;
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
echo "Config: repeat=$REPEAT, clean=$CLEAN, analyze=$ANALYZE, parallel=$N_PARALLEL"
if [[ -n "$CONFIGS_DIR" ]]; then
    echo "Configs: $CONFIGS_DIR/*.ini"
fi
if [[ -n "$OVERRIDE_DURATION" ]]; then
    echo "Duration override: ${OVERRIDE_DURATION}s"
fi
echo ""

# --clean: wipe old data
if $CLEAN; then
    echo "Cleaning sim-out/..."
    rm -rf sim-out/out* sim-out/analysis/ sim-out/sweep-analysis/ sim-out/sweep_summary.csv
fi

# Warn about existing data if not cleaning
if ! $CLEAN && ls sim-out/out* >/dev/null 2>&1; then
    existing=$(ls -d sim-out/out* 2>/dev/null | wc -l)
    echo "WARNING: $existing existing run(s) in sim-out/. Use --clean to remove, or results will accumulate."
fi

# Discover configs
if [[ -n "$CONFIGS_DIR" ]]; then
    CONFIGS=("$CONFIGS_DIR"/*.ini)
else
    CONFIGS=(configs/B*_D*.ini)
fi

N_CONFIGS=${#CONFIGS[@]}
TOTAL=$((N_CONFIGS * REPEAT))
FAILURES=0
CAMPAIGN_START=$SECONDS

echo "Running $TOTAL experiments ($N_CONFIGS configs x $REPEAT trial(s), $N_PARALLEL parallel)..."
echo ""

run_one_config() {
    local config="$1"
    local name
    name=$(basename "$config" .ini)

    if [[ -n "$OVERRIDE_DURATION" ]]; then
        local tmp_config
        tmp_config=$(mktemp --suffix=.ini)
        cp "$config" "$tmp_config"
        echo "nogui_duration = $OVERRIDE_DURATION" >> "$tmp_config"
        "$BINARY" -nogui "$tmp_config"
        local rc=$?
        rm -f "$tmp_config"
        return $rc
    else
        "$BINARY" -nogui "$config"
    fi
}

if [[ $N_PARALLEL -gt 1 ]]; then
    # Parallel execution
    RUNNING=0
    COMPLETED=0
    for trial in $(seq 1 "$REPEAT"); do
        for config in "${CONFIGS[@]}"; do
            name=$(basename "$config" .ini)
            echo "[start] Trial $trial — $name"
            run_one_config "$config" &
            RUNNING=$((RUNNING + 1))
            if [[ $RUNNING -ge $N_PARALLEL ]]; then
                wait -n 2>/dev/null || FAILURES=$((FAILURES + 1))
                RUNNING=$((RUNNING - 1))
                COMPLETED=$((COMPLETED + 1))
                echo "[progress] $COMPLETED/$TOTAL completed"
            fi
        done
    done
    # Wait for remaining jobs
    while [[ $RUNNING -gt 0 ]]; do
        wait -n 2>/dev/null || FAILURES=$((FAILURES + 1))
        RUNNING=$((RUNNING - 1))
        COMPLETED=$((COMPLETED + 1))
        echo "[progress] $COMPLETED/$TOTAL completed"
    done
else
    # Sequential execution
    CURRENT=0
    for trial in $(seq 1 "$REPEAT"); do
        for config in "${CONFIGS[@]}"; do
            CURRENT=$((CURRENT + 1))
            name=$(basename "$config" .ini)
            run_start=$SECONDS

            echo "[$CURRENT/$TOTAL] Trial $trial/$REPEAT — $name..."

            if run_one_config "$config"; then
                elapsed=$((SECONDS - run_start))
                echo "[$CURRENT/$TOTAL] $name done (${elapsed}s)"
            else
                FAILURES=$((FAILURES + 1))
                echo "[$CURRENT/$TOTAL] $name FAILED (exit $?)"
            fi
            echo ""
        done
    done
fi

CAMPAIGN_ELAPSED=$((SECONDS - CAMPAIGN_START))
echo ""
echo "=== Campaign complete: $((TOTAL - FAILURES))/$TOTAL succeeded in ${CAMPAIGN_ELAPSED}s ==="

if [[ $FAILURES -gt 0 ]]; then
    echo "($FAILURES runs failed)"
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
