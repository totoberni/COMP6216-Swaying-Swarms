#!/usr/bin/env python3
"""Compare results from the 3x3 experiment matrix.

Reads sim-out/out*/metrics.csv and config_used.ini to generate:
  1. 3x3 grid of infection curves
  2. 3x3 grid of growth rate curves
  3. 3x3 grid of % infected curves
  4. Overlay of all 9 infection curves
  5. Recovery curves per experiment
  6. Heatmap of peak infection count
  7. Heatmap of steady-state infected count
  8. Heatmap of convergence time

Output: sim-out/analysis/*.png + summary table to stdout.

Usage: python3 scripts/compare_results.py [--sim-dir <path>]
"""

import argparse
import configparser
import csv
import os
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np


# Matrix layout
BEHAVIORS = ["B1", "B2", "B3"]
BEHAVIOR_LABELS = {"B1": "Stable", "B2": "Normal", "B3": "Chaotic"}
DOCTORS = ["D1", "D2", "D3"]
DOCTOR_LABELS = {"D1": "Naive", "D2": "Greedy Search", "D3": "Centroid Search"}


def parse_args():
    parser = argparse.ArgumentParser(description="Compare experiment results")
    parser.add_argument("--sim-dir", default="sim-out", help="Simulation output directory")
    return parser.parse_args()


def classify_behavior(cp):
    """Classify B-type from normal_swarm config values."""
    noise = cp.getfloat("normal_swarm", "noise_factor", fallback=0.0)
    fov = cp.getfloat("normal_swarm", "fov", fallback=3.14)
    if noise > 0:
        return "B3"
    elif fov < 2.0:
        return "B1"
    else:
        return "B2"


def classify_doctor(cp):
    """Classify D-type from doctor_swarm behavior."""
    behavior = cp.get("doctor_swarm", "behavior", fallback="normal").strip()
    if behavior in ("nearest", "seek_nearest"):
        return "D2"
    elif behavior in ("centroid", "seek_centroid"):
        return "D3"
    else:
        return "D1"


def read_config(config_path):
    """Parse config_used.ini and return (b_key, d_key)."""
    cp = configparser.ConfigParser()
    cp.read(config_path)
    return classify_behavior(cp), classify_doctor(cp)


def read_metrics(csv_path):
    """Read metrics.csv and return dict of column lists."""
    data = {}
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            for key, val in row.items():
                key = key.strip()
                if key not in data:
                    data[key] = []
                try:
                    data[key].append(float(val))
                except (ValueError, TypeError):
                    data[key].append(0.0)
    return data


def discover_runs(sim_dir):
    """Find all out* directories with metrics.csv and config_used.ini."""
    runs = {}
    sim_path = Path(sim_dir)

    if not sim_path.exists():
        print(f"Error: {sim_dir} does not exist", file=sys.stderr)
        sys.exit(1)

    for d in sorted(sim_path.iterdir()):
        if not d.is_dir() or not d.name.startswith("out"):
            continue

        metrics_file = d / "metrics.csv"
        config_file = d / "config_used.ini"

        if not metrics_file.exists():
            print(f"Warning: {d.name} has no metrics.csv, skipping", file=sys.stderr)
            continue

        if not config_file.exists():
            print(f"Warning: {d.name} has no config_used.ini, skipping", file=sys.stderr)
            continue

        b_key, d_key = read_config(config_file)
        cell = f"{b_key}_{d_key}"
        data = read_metrics(metrics_file)

        if cell in runs:
            print(f"Warning: duplicate run for {cell} ({d.name}), using latest", file=sys.stderr)

        runs[cell] = {"data": data, "dir": d.name, "b_key": b_key, "d_key": d_key}

    return runs


def extract_stats(data):
    """Extract summary statistics from a run's data."""
    infected = data.get("infected", [])
    recovered = data.get("recovered", [])
    time_s = data.get("time_s", [])

    peak_infected = max(infected) if infected else 0
    peak_idx = infected.index(peak_infected) if infected else 0
    peak_time = time_s[peak_idx] if time_s and peak_idx < len(time_s) else 0

    final_infected = infected[-1] if infected else 0

    # Time to eradication: first time infected drops to 0 after being nonzero
    eradication_time = float("inf")
    saw_infection = False
    for i, count in enumerate(infected):
        if count > 0:
            saw_infection = True
        elif saw_infection and count == 0:
            eradication_time = time_s[i] if i < len(time_s) else float("inf")
            break

    total_recovered = recovered[-1] if recovered else 0

    # Steady-state infected (average over last 20% of frames)
    n_tail = max(1, len(infected) // 5)
    steady_state_infected = float(np.mean(infected[-n_tail:])) if infected else 0

    # Steady-state pct infected
    pct_inf = data.get("pct_infected", [])
    steady_state_pct = float(np.mean(pct_inf[-n_tail:])) * 100 if pct_inf else 0

    # Time to convergence: first time |growth_rate| stays < threshold for window_s
    growth = data.get("growth_rate", [])
    window_s = 10.0
    threshold = 0.5
    convergence_time = float("inf")
    streak_start = None
    for i, g in enumerate(growth):
        if abs(g) < threshold:
            if streak_start is None:
                streak_start = i
            elif time_s[i] - time_s[streak_start] >= window_s:
                convergence_time = time_s[streak_start]
                break
        else:
            streak_start = None

    # Mean growth rate (second half)
    half = len(growth) // 2
    mean_growth_2h = float(np.mean(growth[half:])) if growth else 0

    return {
        "peak_infected": peak_infected,
        "peak_time": peak_time,
        "final_infected": final_infected,
        "eradication_time": eradication_time,
        "total_recovered": total_recovered,
        "steady_state_infected": steady_state_infected,
        "steady_state_pct": steady_state_pct,
        "convergence_time": convergence_time,
        "mean_growth_2h": mean_growth_2h,
    }


def plot_infection_grid(runs, out_dir):
    """3x3 grid of infection curves."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True, sharey=True)
    fig.suptitle("Infection Curves — 3x3 Experiment Matrix", fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                data = runs[cell]["data"]
                time_s = data.get("time_s", [])
                infected = data.get("infected", [])
                ax.plot(time_s, infected, color="tab:red", linewidth=1.2)
                ax.fill_between(time_s, infected, alpha=0.15, color="tab:red")
            else:
                ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                        ha="center", va="center", fontsize=12, color="gray")

            if r == 0:
                ax.set_title(f"{d} ({DOCTOR_LABELS[d]})", fontsize=10)
            if c == 0:
                ax.set_ylabel(f"{b} ({BEHAVIOR_LABELS[b]})\nInfected", fontsize=9)
            if r == 2:
                ax.set_xlabel("Time (s)", fontsize=9)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(os.path.join(out_dir, "infection_grid.png"), dpi=150)
    plt.close(fig)


def plot_overlay(runs, out_dir):
    """All 9 infection curves on one plot."""
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_title("Infection Curves — All Experiments Overlaid", fontsize=13, fontweight="bold")

    colors = list(mcolors.TABLEAU_COLORS.values())
    styles = ["-", "--", ":"]

    idx = 0
    for b in BEHAVIORS:
        for di, d in enumerate(DOCTORS):
            cell = f"{b}_{d}"
            if cell not in runs:
                idx += 1
                continue

            data = runs[cell]["data"]
            time_s = data.get("time_s", [])
            infected = data.get("infected", [])
            label = f"{b}_{d} ({BEHAVIOR_LABELS[b]}, {DOCTOR_LABELS[d]})"
            ax.plot(time_s, infected, color=colors[idx % len(colors)],
                    linestyle=styles[di % len(styles)], linewidth=1.3, label=label)
            idx += 1

    ax.set_xlabel("Time (s)", fontsize=11)
    ax.set_ylabel("Infected Count", fontsize=11)
    ax.legend(fontsize=7, ncol=3, loc="upper right")
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, "infection_overlay.png"), dpi=150)
    plt.close(fig)


def plot_growth_rate_grid(runs, out_dir):
    """3x3 grid of growth rate curves."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True)
    fig.suptitle("Infection Growth Rate — 3x3 Experiment Matrix",
                 fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                data = runs[cell]["data"]
                time_s = data.get("time_s", [])
                growth = data.get("growth_rate", [])
                ax.plot(time_s, growth, color="tab:orange", linewidth=0.8)
                ax.axhline(y=0, color="gray", linestyle="--", linewidth=0.5)
                growth_arr = np.array(growth)
                time_arr = np.array(time_s)
                ax.fill_between(time_arr, growth_arr, 0,
                                where=growth_arr > 0, alpha=0.2,
                                color="tab:red", interpolate=True)
                ax.fill_between(time_arr, growth_arr, 0,
                                where=growth_arr < 0, alpha=0.2,
                                color="tab:green", interpolate=True)
            else:
                ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                        ha="center", va="center", fontsize=12, color="gray")

            if r == 0:
                ax.set_title(f"{d} ({DOCTOR_LABELS[d]})", fontsize=10)
            if c == 0:
                ax.set_ylabel(f"{b} ({BEHAVIOR_LABELS[b]})\nGrowth Rate", fontsize=9)
            if r == 2:
                ax.set_xlabel("Time (s)", fontsize=9)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(os.path.join(out_dir, "growth_rate_grid.png"), dpi=150)
    plt.close(fig)


def plot_pct_infected_grid(runs, out_dir):
    """3x3 grid of % population infected curves."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True, sharey=True)
    fig.suptitle("% Population Infected — 3x3 Experiment Matrix",
                 fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                data = runs[cell]["data"]
                time_s = data.get("time_s", [])
                pct = [v * 100 for v in data.get("pct_infected", [])]
                ax.plot(time_s, pct, color="tab:purple", linewidth=1.2)
                ax.fill_between(time_s, pct, alpha=0.15, color="tab:purple")
            else:
                ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                        ha="center", va="center", fontsize=12, color="gray")

            if r == 0:
                ax.set_title(f"{d} ({DOCTOR_LABELS[d]})", fontsize=10)
            if c == 0:
                ax.set_ylabel(f"{b} ({BEHAVIOR_LABELS[b]})\n% Infected", fontsize=9)
            if r == 2:
                ax.set_xlabel("Time (s)", fontsize=9)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(os.path.join(out_dir, "pct_infected_grid.png"), dpi=150)
    plt.close(fig)


def plot_heatmap(matrix, title, filename, out_dir, fmt=".0f", cmap="YlOrRd"):
    """Generic 3x3 heatmap."""
    fig, ax = plt.subplots(figsize=(7, 5))
    im = ax.imshow(matrix, cmap=cmap, aspect="auto")

    ax.set_xticks(range(3))
    ax.set_xticklabels([f"{d}\n{DOCTOR_LABELS[d]}" for d in DOCTORS], fontsize=9)
    ax.set_yticks(range(3))
    ax.set_yticklabels([f"{b} ({BEHAVIOR_LABELS[b]})" for b in BEHAVIORS], fontsize=9)

    for r in range(3):
        for c in range(3):
            val = matrix[r][c]
            text = "N/A" if val is None or np.isinf(val) else f"{val:{fmt}}"
            color = "white" if val is not None and not np.isinf(val) and val > np.nanmax(
                [v for row in matrix for v in row if v is not None and not np.isinf(v)]
            ) * 0.6 else "black"
            ax.text(c, r, text, ha="center", va="center", fontsize=11, color=color)

    ax.set_title(title, fontsize=13, fontweight="bold")
    fig.colorbar(im, ax=ax, shrink=0.8)
    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, filename), dpi=150)
    plt.close(fig)


def plot_recovery_curves(runs, out_dir):
    """Recovery curves per experiment."""
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_title("Recovery Curves — All Experiments", fontsize=13, fontweight="bold")

    colors = list(mcolors.TABLEAU_COLORS.values())
    idx = 0
    for b in BEHAVIORS:
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell not in runs:
                idx += 1
                continue

            data = runs[cell]["data"]
            time_s = data.get("time_s", [])
            recovered = data.get("recovered", [])
            label = f"{b}_{d}"
            ax.plot(time_s, recovered, color=colors[idx % len(colors)],
                    linewidth=1.2, label=label)
            idx += 1

    ax.set_xlabel("Time (s)", fontsize=11)
    ax.set_ylabel("Recovered Count", fontsize=11)
    ax.legend(fontsize=8, ncol=3, loc="lower right")
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, "recovery_curves.png"), dpi=150)
    plt.close(fig)


def print_summary_table(runs, all_stats):
    """Print summary table to stdout."""
    print("\n" + "=" * 80)
    print("EXPERIMENT RESULTS SUMMARY")
    print("=" * 80)
    print(f"{'Cell':<8} {'Boid':<10} {'Doctor':<16} {'Peak Inf':>9} {'Peak Time':>10} "
          f"{'SS Inf':>8} {'SS %Inf':>8} {'Conv Time':>10} {'Mean GR(2H)':>12}")
    print("-" * 92)

    for b in BEHAVIORS:
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell not in runs:
                print(f"{cell:<8} {'---':<10} {'---':<16} {'N/A':>9} {'N/A':>10} "
                      f"{'N/A':>8} {'N/A':>8} {'N/A':>10} {'N/A':>12}")
                continue

            stats = all_stats[cell]
            conv = (f"{stats['convergence_time']:.0f}s"
                    if not np.isinf(stats["convergence_time"]) else "never")
            gr = f"{stats['mean_growth_2h']:+.2f}"

            print(f"{cell:<8} {BEHAVIOR_LABELS[b]:<10} {DOCTOR_LABELS[d]:<16} "
                  f"{stats['peak_infected']:>9.0f} {stats['peak_time']:>9.1f}s "
                  f"{stats['steady_state_infected']:>8.1f} "
                  f"{stats['steady_state_pct']:>7.1f}% "
                  f"{conv:>10} {gr:>12}")

    print("=" * 92)


def main():
    args = parse_args()
    sim_dir = args.sim_dir

    print(f"Scanning {sim_dir}/ for experiment results...")
    runs = discover_runs(sim_dir)

    if not runs:
        print("No valid experiment runs found.", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(runs)} experiment run(s): {', '.join(sorted(runs.keys()))}")

    # Compute stats
    all_stats = {}
    for cell, run in runs.items():
        all_stats[cell] = extract_stats(run["data"])

    # Create output directory
    out_dir = os.path.join(sim_dir, "analysis")
    os.makedirs(out_dir, exist_ok=True)

    # Generate plots
    print("Generating infection grid...")
    plot_infection_grid(runs, out_dir)

    print("Generating growth rate grid...")
    plot_growth_rate_grid(runs, out_dir)

    print("Generating % infected grid...")
    plot_pct_infected_grid(runs, out_dir)

    print("Generating infection overlay...")
    plot_overlay(runs, out_dir)

    print("Generating recovery curves...")
    plot_recovery_curves(runs, out_dir)

    # Build heatmap matrices
    peak_matrix = []
    ss_matrix = []
    conv_matrix = []
    for b in BEHAVIORS:
        peak_row = []
        ss_row = []
        conv_row = []
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell in all_stats:
                peak_row.append(all_stats[cell]["peak_infected"])
                ss_row.append(all_stats[cell]["steady_state_infected"])
                conv_row.append(all_stats[cell]["convergence_time"])
            else:
                peak_row.append(None)
                ss_row.append(None)
                conv_row.append(None)
        peak_matrix.append(peak_row)
        ss_matrix.append(ss_row)
        conv_matrix.append(conv_row)

    # Peak infection heatmap
    peak_for_plot = [[v if v is not None else 0 for v in row] for row in peak_matrix]
    print("Generating peak infection heatmap...")
    plot_heatmap(peak_for_plot, "Peak Infection Count", "heatmap_peak.png", out_dir)

    # Steady-state infected heatmap
    ss_for_plot = [[v if v is not None else 0 for v in row] for row in ss_matrix]
    print("Generating steady-state infected heatmap...")
    plot_heatmap(ss_for_plot, "Steady-State Infected Count",
                 "heatmap_steady_state.png", out_dir, fmt=".1f", cmap="YlGnBu_r")

    # Convergence time heatmap
    conv_for_plot = [[v if v is not None else float("inf") for v in row]
                     for row in conv_matrix]
    finite_vals = [v for row in conv_for_plot for v in row if not np.isinf(v)]
    cap = max(finite_vals) * 1.2 if finite_vals else 300.0
    conv_capped = [[v if not np.isinf(v) else cap for v in row] for row in conv_for_plot]
    print("Generating convergence time heatmap...")
    plot_heatmap(conv_capped, "Time to Convergence (s)",
                 "heatmap_convergence.png", out_dir, fmt=".1f", cmap="YlGnBu")

    print(f"\nPlots saved to {out_dir}/")

    # Summary table
    print_summary_table(runs, all_stats)


if __name__ == "__main__":
    main()
