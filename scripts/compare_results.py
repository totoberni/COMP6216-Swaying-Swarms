#!/usr/bin/env python3
"""Compare results from the 3x3 experiment matrix.

Reads sim-out/out*/metrics.csv and config_used.ini to generate:
  1. 3x3 grid of infection curves
  2. Overlay of all 9 infection curves
  3. Heatmap of peak infection count
  4. Heatmap of time to eradication
  5. Recovery curves per experiment

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
BEHAVIOR_LABELS = {"B1": "Line", "B2": "Oval", "B3": "Chaotic"}
DOCTORS = ["D1", "D2", "D3"]
DOCTOR_LABELS = {"D1": "Normal", "D2": "Seek Nearest", "D3": "Seek Centroid"}

SWARM_TO_B = {"normal": "B1", "line": "B1", "oval": "B2", "chaotic": "B3"}
DOCTOR_TO_D = {"normal": "D1", "seek_nearest": "D2", "seek_centroid": "D3"}


def parse_args():
    parser = argparse.ArgumentParser(description="Compare experiment results")
    parser.add_argument("--sim-dir", default="sim-out", help="Simulation output directory")
    return parser.parse_args()


def read_config(config_path):
    """Parse config_used.ini to extract behavior labels."""
    cp = configparser.ConfigParser()
    cp.read(config_path)

    swarm = "normal"
    doctor = "normal"

    for section in cp.sections():
        if section == "normal_swarm" and cp.has_option(section, "behavior"):
            swarm = cp.get(section, "behavior").strip()
        if section == "doctor_swarm" and cp.has_option(section, "behavior"):
            doctor = cp.get(section, "behavior").strip()

    return swarm, doctor


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

        swarm, doctor = read_config(config_file)
        b_key = SWARM_TO_B.get(swarm, None)
        d_key = DOCTOR_TO_D.get(doctor, None)

        if b_key is None or d_key is None:
            print(f"Warning: {d.name} has unknown behavior ({swarm}/{doctor}), skipping",
                  file=sys.stderr)
            continue

        cell = f"{b_key}_{d_key}"
        data = read_metrics(metrics_file)

        if cell in runs:
            print(f"Warning: duplicate run for {cell} ({d.name}), using latest", file=sys.stderr)

        runs[cell] = {"data": data, "dir": d.name, "swarm": swarm, "doctor": doctor}

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

    return {
        "peak_infected": peak_infected,
        "peak_time": peak_time,
        "final_infected": final_infected,
        "eradication_time": eradication_time,
        "total_recovered": total_recovered,
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
    print(f"{'Cell':<8} {'Swarm':<10} {'Doctor':<15} {'Peak Inf':>9} {'Peak Time':>10} "
          f"{'Final Inf':>10} {'Erad Time':>10} {'Recovered':>10}")
    print("-" * 80)

    for b in BEHAVIORS:
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell not in runs:
                print(f"{cell:<8} {'---':<10} {'---':<15} {'N/A':>9} {'N/A':>10} "
                      f"{'N/A':>10} {'N/A':>10} {'N/A':>10}")
                continue

            run = runs[cell]
            stats = all_stats[cell]
            erad = f"{stats['eradication_time']:.1f}s" if not np.isinf(
                stats["eradication_time"]) else "never"

            print(f"{cell:<8} {run['swarm']:<10} {run['doctor']:<15} "
                  f"{stats['peak_infected']:>9.0f} {stats['peak_time']:>9.1f}s "
                  f"{stats['final_infected']:>10.0f} {erad:>10} "
                  f"{stats['total_recovered']:>10.0f}")

    print("=" * 80)


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

    print("Generating infection overlay...")
    plot_overlay(runs, out_dir)

    print("Generating recovery curves...")
    plot_recovery_curves(runs, out_dir)

    # Build heatmap matrices
    peak_matrix = []
    erad_matrix = []
    for b in BEHAVIORS:
        peak_row = []
        erad_row = []
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell in all_stats:
                peak_row.append(all_stats[cell]["peak_infected"])
                erad_row.append(all_stats[cell]["eradication_time"])
            else:
                peak_row.append(None)
                erad_row.append(None)
        peak_matrix.append(peak_row)
        erad_matrix.append(erad_row)

    # Replace None with 0 for peak (for colormap)
    peak_for_plot = [[v if v is not None else 0 for v in row] for row in peak_matrix]

    print("Generating peak infection heatmap...")
    plot_heatmap(peak_for_plot, "Peak Infection Count", "heatmap_peak.png", out_dir)

    # For eradication, replace None with inf and cap inf for display
    erad_for_plot = [[v if v is not None else float("inf") for v in row] for row in erad_matrix]
    finite_vals = [v for row in erad_for_plot for v in row if not np.isinf(v)]
    cap = max(finite_vals) * 1.2 if finite_vals else 300.0
    erad_capped = [[v if not np.isinf(v) else cap for v in row] for row in erad_for_plot]

    print("Generating eradication time heatmap...")
    plot_heatmap(erad_capped, "Time to Eradication (s)", "heatmap_eradication.png",
                 out_dir, fmt=".1f", cmap="YlGnBu")

    print(f"\nPlots saved to {out_dir}/")

    # Summary table
    print_summary_table(runs, all_stats)


if __name__ == "__main__":
    main()
