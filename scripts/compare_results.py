#!/usr/bin/env python3
"""Compare results from the 3x3 experiment matrix.

Reads sim-out/out*/metrics.csv and config_used.ini to generate:
  1. 3x3 grid of infection curves (with error bands for multi-trial)
  2. 3x3 grid of growth rate curves (with error bands)
  3. 3x3 grid of % infected curves (with error bands)
  4. Overlay of all 9 infection curves (with error bands)
  5. Recovery curves per experiment (with error bands)
  6. Heatmap of peak infection count (mean +/- std)
  7. Heatmap of steady-state infected count (mean +/- std)
  8. Heatmap of convergence time (mean +/- std)

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
    """Find all out* directories, group by experiment cell."""
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

        if cell not in runs:
            runs[cell] = {"trials": [], "b_key": b_key, "d_key": d_key}
        runs[cell]["trials"].append({"data": data, "dir": d.name})

    # Print discovery summary
    for cell, info in sorted(runs.items()):
        n = len(info["trials"])
        print(f"  {cell}: {n} trial(s)")

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


def aggregate_trials(trial_stats_list):
    """Compute mean +/- std across trials for each metric."""
    agg = {}
    keys = trial_stats_list[0].keys()
    for key in keys:
        values = [s[key] for s in trial_stats_list]
        finite = [v for v in values if not np.isinf(v)]
        if finite:
            agg[f"{key}_mean"] = float(np.mean(finite))
            agg[f"{key}_std"] = float(np.std(finite, ddof=1)) if len(finite) > 1 else 0.0
        else:
            agg[f"{key}_mean"] = float("inf")
            agg[f"{key}_std"] = 0.0
        agg[f"{key}_n"] = len(finite)
    return agg


def aggregate_time_series(trials, column):
    """Stack a column across trials, compute per-frame mean and std.

    Returns (time_s, mean_arr, std_arr). Truncates to shortest trial.
    """
    series = []
    time_s = None
    min_len = float("inf")

    for t in trials:
        vals = t["data"].get(column, [])
        if not vals:
            continue
        min_len = min(min_len, len(vals))
        series.append(vals)
        if time_s is None:
            time_s = t["data"].get("time_s", [])

    if not series or time_s is None:
        return [], [], []

    min_len = int(min_len)
    time_s = time_s[:min_len]
    stacked = np.array([s[:min_len] for s in series])
    mean = np.mean(stacked, axis=0)
    std = np.std(stacked, axis=0) if len(series) > 1 else np.zeros(min_len)

    return time_s, mean, std


def plot_infection_grid(runs, out_dir):
    """3x3 grid of infection curves with error bands."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True, sharey=True)
    fig.suptitle("Infection Curves — 3x3 Experiment Matrix", fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                time_s, mean, std = aggregate_time_series(
                    runs[cell]["trials"], "infected")
                ax.plot(time_s, mean, color="tab:red", linewidth=1.2)
                ax.fill_between(time_s, mean - std, mean + std,
                                alpha=0.2, color="tab:red")
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
    """All 9 infection curves on one plot with error bands."""
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.set_title("Infection Curves — All Experiments Overlaid",
                 fontsize=13, fontweight="bold")

    colors = list(mcolors.TABLEAU_COLORS.values())
    styles = ["-", "--", ":"]

    idx = 0
    for b in BEHAVIORS:
        for di, d in enumerate(DOCTORS):
            cell = f"{b}_{d}"
            if cell not in runs:
                idx += 1
                continue

            time_s, mean, std = aggregate_time_series(
                runs[cell]["trials"], "infected")
            label = f"{b}_{d} ({BEHAVIOR_LABELS[b]}, {DOCTOR_LABELS[d]})"
            color = colors[idx % len(colors)]
            ax.plot(time_s, mean, color=color,
                    linestyle=styles[di % len(styles)],
                    linewidth=1.3, label=label)
            ax.fill_between(time_s, mean - std, mean + std,
                            alpha=0.1, color=color)
            idx += 1

    ax.set_xlabel("Time (s)", fontsize=11)
    ax.set_ylabel("Infected Count", fontsize=11)
    ax.legend(fontsize=7, ncol=3, loc="upper right")
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, "infection_overlay.png"), dpi=150)
    plt.close(fig)


def plot_growth_rate_grid(runs, out_dir):
    """3x3 grid of growth rate curves with error bands."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True)
    fig.suptitle("Infection Growth Rate — 3x3 Experiment Matrix",
                 fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                time_s, mean, std = aggregate_time_series(
                    runs[cell]["trials"], "growth_rate")
                ax.plot(time_s, mean, color="tab:orange", linewidth=0.8)
                ax.fill_between(time_s, mean - std, mean + std,
                                alpha=0.15, color="tab:orange")
                ax.axhline(y=0, color="gray", linestyle="--", linewidth=0.5)
                # Directional fill on the mean
                ax.fill_between(time_s, mean, 0, where=mean > 0,
                                alpha=0.1, color="tab:red", interpolate=True)
                ax.fill_between(time_s, mean, 0, where=mean < 0,
                                alpha=0.1, color="tab:green", interpolate=True)
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
    """3x3 grid of % population infected curves with error bands."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True, sharey=True)
    fig.suptitle("% Population Infected — 3x3 Experiment Matrix",
                 fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell in runs:
                time_s, mean, std = aggregate_time_series(
                    runs[cell]["trials"], "pct_infected")
                mean_pct = mean * 100
                std_pct = std * 100
                ax.plot(time_s, mean_pct, color="tab:purple", linewidth=1.2)
                ax.fill_between(time_s, mean_pct - std_pct,
                                mean_pct + std_pct,
                                alpha=0.2, color="tab:purple")
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


def plot_heatmap(matrix, title, filename, out_dir, fmt=".0f", cmap="YlOrRd",
                 std_matrix=None):
    """Generic 3x3 heatmap with optional mean +/- std annotations."""
    fig, ax = plt.subplots(figsize=(8, 5))
    im = ax.imshow(matrix, cmap=cmap, aspect="auto")

    ax.set_xticks(range(3))
    ax.set_xticklabels([f"{d}\n{DOCTOR_LABELS[d]}" for d in DOCTORS], fontsize=9)
    ax.set_yticks(range(3))
    ax.set_yticklabels([f"{b} ({BEHAVIOR_LABELS[b]})" for b in BEHAVIORS], fontsize=9)

    # Threshold for text color
    finite_vals = [v for row in matrix for v in row
                   if v is not None and not np.isinf(v)]
    color_thresh = max(finite_vals) * 0.6 if finite_vals else 0

    for r in range(3):
        for c in range(3):
            val = matrix[r][c]
            if val is None or np.isinf(val):
                text = "N/A"
            elif std_matrix is not None and std_matrix[r][c] > 0:
                text = f"{val:{fmt}}\n+/-{std_matrix[r][c]:{fmt}}"
            else:
                text = f"{val:{fmt}}"
            color = ("white" if val is not None and not np.isinf(val)
                     and val > color_thresh else "black")
            ax.text(c, r, text, ha="center", va="center",
                    fontsize=10, color=color)

    ax.set_title(title, fontsize=13, fontweight="bold")
    fig.colorbar(im, ax=ax, shrink=0.8)
    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, filename), dpi=150)
    plt.close(fig)


def plot_recovery_curves(runs, out_dir):
    """Recovery curves per experiment with error bands."""
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

            time_s, mean, std = aggregate_time_series(
                runs[cell]["trials"], "recovered")
            label = f"{b}_{d}"
            color = colors[idx % len(colors)]
            ax.plot(time_s, mean, color=color, linewidth=1.2, label=label)
            ax.fill_between(time_s, mean - std, mean + std,
                            alpha=0.1, color=color)
            idx += 1

    ax.set_xlabel("Time (s)", fontsize=11)
    ax.set_ylabel("Recovered Count", fontsize=11)
    ax.legend(fontsize=8, ncol=3, loc="lower right")
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(out_dir, "recovery_curves.png"), dpi=150)
    plt.close(fig)


def plot_convergence_diagnostic(runs, out_dir):
    """Per-cell convergence: running mean + CoV for the most variable cell."""
    # Collect steady_state_pct per cell
    cell_values = {}
    for cell, info in runs.items():
        vals = []
        for t in info["trials"]:
            stats = extract_stats(t["data"])
            vals.append(stats["steady_state_pct"])
        if len(vals) >= 3:
            cell_values[cell] = np.array(vals)

    if not cell_values:
        print("  Skipping convergence diagnostic: too few trials per cell")
        return

    # Find the most variable cell (worst-case for convergence)
    worst_cell = max(cell_values, key=lambda c: np.std(cell_values[c]))
    worst_vals = cell_values[worst_cell]

    rng = np.random.RandomState(42)
    rng.shuffle(worst_vals)

    fig, ax1 = plt.subplots(figsize=(8, 5))
    ax2 = ax1.twinx()

    # Plot thin CoV lines for all cells (background context)
    for cell, vals in sorted(cell_values.items()):
        v = vals.copy()
        rng = np.random.RandomState(42)
        rng.shuffle(v)
        ns = np.arange(1, len(v) + 1)
        cum_sum = np.cumsum(v)
        r_mean = cum_sum / ns
        cum_sq = np.cumsum(v ** 2)
        r_var = np.maximum(cum_sq / ns - r_mean ** 2, 0)
        r_std = np.sqrt(r_var)
        r_cov = np.divide(r_std, np.abs(r_mean), out=np.zeros_like(r_std), where=np.abs(r_mean) > 1e-9)
        ax2.plot(ns, r_cov, color="tab:gray", linewidth=0.4, alpha=0.4)

    # Highlight worst-case cell
    ns = np.arange(1, len(worst_vals) + 1)
    cum_sum = np.cumsum(worst_vals)
    running_mean = cum_sum / ns
    cum_sq = np.cumsum(worst_vals ** 2)
    running_var = np.maximum(cum_sq / ns - running_mean ** 2, 0)
    running_std = np.sqrt(running_var)
    running_se = running_std / np.sqrt(ns)
    running_cov = np.divide(running_std, np.abs(running_mean),
                            out=np.zeros_like(running_std), where=np.abs(running_mean) > 1e-9)

    ax1.plot(ns, running_mean, color="tab:blue", linewidth=1.3, label="Running Mean")
    ax1.fill_between(ns, running_mean - running_se, running_mean + running_se,
                     alpha=0.25, color="tab:blue", label="\u00b1 SE")
    ax1.set_xlabel("Number of Runs (per cell)")
    ax1.set_ylabel("Steady-State Infected (%)", color="tab:blue")
    ax1.tick_params(axis="y", labelcolor="tab:blue")

    ax2.plot(ns, running_cov, color="tab:orange", linewidth=1.2,
             label=f"CoV ({worst_cell})")
    ax2.axhline(0.05, color="tab:orange", linestyle="--", linewidth=0.7,
                alpha=0.7, label="CoV = 0.05")
    ax2.set_ylabel("Coefficient of Variation", color="tab:orange")
    ax2.tick_params(axis="y", labelcolor="tab:orange")
    ax2.set_ylim(bottom=0)

    ax1.set_title(
        f"Monte Carlo Convergence Diagnostic (worst-case: {worst_cell})",
        fontsize=12, fontweight="bold")
    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, fontsize=8, loc="upper right")
    fig.tight_layout()
    fig.savefig(os.path.join(out_dir, "convergence_diagnostic.png"), dpi=150)
    plt.close(fig)


def plot_interaction_effects(runs, all_stats, out_dir):
    """1x3 interaction plot: formation on x-axis, colored lines per doctor behavior."""
    metrics = ["steady_state_pct", "convergence_time", "peak_infected"]
    metric_labels = {
        "steady_state_pct": "Steady-State Infected (%)",
        "convergence_time": "Convergence Time (s)",
        "peak_infected": "Peak Infected",
    }
    doctor_colors = {"D1": "#1b9e77", "D2": "#d95f02", "D3": "#7570b3"}

    fig, axes = plt.subplots(1, 3, figsize=(14, 5))

    for idx, metric in enumerate(metrics):
        ax = axes[idx]
        for d_key in DOCTORS:
            x_vals, y_means, y_ci = [], [], []
            for b_idx, b_key in enumerate(BEHAVIORS):
                cell = f"{b_key}_{d_key}"
                if cell not in all_stats:
                    continue
                st = all_stats[cell]
                mean_key = f"{metric}_mean"
                std_key = f"{metric}_std"
                n_key = f"{metric}_n"
                m = st.get(mean_key, 0)
                s = st.get(std_key, 0)
                n = st.get(n_key, 1)
                ci = 1.96 * s / max(np.sqrt(n), 1) if not np.isinf(m) else 0
                x_vals.append(b_idx)
                y_means.append(m if not np.isinf(m) else 0)
                y_ci.append(ci)

            if x_vals:
                ax.errorbar(x_vals, y_means, yerr=y_ci,
                            color=doctor_colors[d_key], marker="o",
                            linewidth=1.3, capsize=4,
                            label=DOCTOR_LABELS[d_key])

        ax.set_xticks(range(len(BEHAVIORS)))
        ax.set_xticklabels([BEHAVIOR_LABELS[b] for b in BEHAVIORS], fontsize=9)
        ax.set_xlabel("Formation")
        ax.set_ylabel(metric_labels[metric])
        ax.set_title(metric_labels[metric], fontsize=10)
        ax.legend(fontsize=7)
        ax.grid(True, alpha=0.2)

    fig.suptitle("Interaction Effects: Formation \u00d7 Doctor Behavior",
                 fontsize=13, fontweight="bold")
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(os.path.join(out_dir, "interaction_effects.png"), dpi=150)
    plt.close(fig)


def plot_sick_healthy_grid(runs, out_dir):
    """3x3 grid with overlaid infected (red) and healthy (blue) fan charts."""
    fig, axes = plt.subplots(3, 3, figsize=(14, 10), sharex=True, sharey=True)
    fig.suptitle("Infected vs Healthy Population \u2014 3\u00d73 Matrix",
                 fontsize=14, fontweight="bold")

    for r, b in enumerate(BEHAVIORS):
        for c, d in enumerate(DOCTORS):
            ax = axes[r][c]
            cell = f"{b}_{d}"

            if cell not in runs:
                ax.text(0.5, 0.5, "No data", transform=ax.transAxes,
                        ha="center", va="center", fontsize=12, color="gray")
            else:
                trials = runs[cell]["trials"]
                # Infected fan chart
                ts_i, mean_i, std_i = aggregate_time_series(trials, "infected")
                # Recovered for healthy computation
                ts_r, mean_r, std_r = aggregate_time_series(trials, "recovered")

                if len(ts_i) > 0:
                    # Estimate total pop from first frame
                    total_pop = 135
                    for t in trials:
                        inf = t["data"].get("infected", [])
                        pct = t["data"].get("pct_infected", [])
                        if inf and pct and pct[0] > 0 and inf[0] > 0:
                            total_pop = round(inf[0] / pct[0])
                            break

                    # Healthy = total - infected - recovered
                    n = min(len(mean_i), len(mean_r)) if len(ts_r) > 0 else len(mean_i)
                    time_s = np.array(ts_i[:n])

                    # Infected: individual run stacking for proper fan chart
                    inf_series = []
                    healthy_series = []
                    min_len = float("inf")
                    for t in trials:
                        inf_arr = t["data"].get("infected", [])
                        rec_arr = t["data"].get("recovered", [])
                        if inf_arr and rec_arr:
                            ln = min(len(inf_arr), len(rec_arr))
                            min_len = min(min_len, ln)
                            inf_series.append(inf_arr[:ln])
                            h = [max(0, total_pop - inf_arr[i] - rec_arr[i])
                                 for i in range(ln)]
                            healthy_series.append(h)

                    if inf_series:
                        min_len = int(min_len)
                        time_s = np.array(ts_i[:min_len])
                        inf_stacked = np.array([s[:min_len] for s in inf_series])
                        h_stacked = np.array([s[:min_len] for s in healthy_series])

                        # Infected fan chart (red)
                        i_med = np.median(inf_stacked, axis=0)
                        i_25 = np.percentile(inf_stacked, 25, axis=0)
                        i_75 = np.percentile(inf_stacked, 75, axis=0)
                        ax.fill_between(time_s, i_25, i_75,
                                        alpha=0.3, color="tab:red", linewidth=0)
                        ax.plot(time_s, i_med, color="tab:red", lw=1.0,
                                label="Infected" if r == 0 and c == 0 else "")

                        # Healthy fan chart (blue)
                        h_med = np.median(h_stacked, axis=0)
                        h_25 = np.percentile(h_stacked, 25, axis=0)
                        h_75 = np.percentile(h_stacked, 75, axis=0)
                        ax.fill_between(time_s, h_25, h_75,
                                        alpha=0.3, color="tab:blue", linewidth=0)
                        ax.plot(time_s, h_med, color="tab:blue", lw=1.0,
                                label="Healthy" if r == 0 and c == 0 else "")

            if r == 0:
                ax.set_title(f"{d} ({DOCTOR_LABELS[d]})", fontsize=10)
            if c == 0:
                ax.set_ylabel(f"{b} ({BEHAVIOR_LABELS[b]})\nCount", fontsize=9)
            if r == 2:
                ax.set_xlabel("Time (s)", fontsize=9)

    # Legend from first cell with data
    handles, labels = [], []
    for r_ax in range(len(BEHAVIORS)):
        for c_ax in range(len(DOCTORS)):
            handles, labels = axes[r_ax][c_ax].get_legend_handles_labels()
            if handles:
                break
        if handles:
            break
    if handles:
        fig.legend(handles, labels, loc="upper right", fontsize=9)

    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(os.path.join(out_dir, "sick_healthy_grid.png"), dpi=150)
    plt.close(fig)


def print_summary_table(runs, all_stats):
    """Print summary table to stdout."""
    print("\n" + "=" * 110)
    print("EXPERIMENT RESULTS SUMMARY")
    print("=" * 110)
    print(f"{'Cell':<8} {'N':>3} {'Boid':<10} {'Doctor':<16} {'Peak Inf':>12} "
          f"{'SS Inf':>12} {'SS %Inf':>10} {'Conv Time':>10} {'Mean GR':>10}")
    print("-" * 110)

    for b in BEHAVIORS:
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell not in runs:
                print(f"{cell:<8} {'':>3} {'---':<10} {'---':<16} {'N/A':>12} "
                      f"{'N/A':>12} {'N/A':>10} {'N/A':>10} {'N/A':>10}")
                continue

            stats = all_stats[cell]
            n = stats.get("n_trials", 1)

            # Convergence time
            conv_mean = stats["convergence_time_mean"]
            conv_std = stats.get("convergence_time_std", 0)
            if np.isinf(conv_mean):
                conv_text = "never"
            elif n > 1 and conv_std > 0:
                conv_text = f"{conv_mean:.0f}+/-{conv_std:.0f}s"
            else:
                conv_text = f"{conv_mean:.0f}s"

            # Growth rate
            gr_mean = stats["mean_growth_2h_mean"]
            gr_std = stats.get("mean_growth_2h_std", 0)
            if n > 1 and gr_std > 0:
                gr_text = f"{gr_mean:+.2f}+/-{gr_std:.2f}"
            else:
                gr_text = f"{gr_mean:+.2f}"

            if n > 1:
                peak_text = (f"{stats['peak_infected_mean']:.0f}"
                             f"+/-{stats['peak_infected_std']:.0f}")
                ss_text = (f"{stats['steady_state_infected_mean']:.1f}"
                           f"+/-{stats['steady_state_infected_std']:.1f}")
                pct_text = (f"{stats['steady_state_pct_mean']:.1f}"
                            f"+/-{stats['steady_state_pct_std']:.1f}%")
            else:
                peak_text = f"{stats['peak_infected_mean']:.0f}"
                ss_text = f"{stats['steady_state_infected_mean']:.1f}"
                pct_text = f"{stats['steady_state_pct_mean']:.1f}%"

            print(f"{cell:<8} {n:>3} {BEHAVIOR_LABELS[b]:<10} "
                  f"{DOCTOR_LABELS[d]:<16} {peak_text:>12} "
                  f"{ss_text:>12} {pct_text:>10} "
                  f"{conv_text:>10} {gr_text:>10}")

    print("=" * 110)


def main():
    args = parse_args()
    sim_dir = args.sim_dir

    print(f"Scanning {sim_dir}/ for experiment results...")
    runs = discover_runs(sim_dir)

    if not runs:
        print("No valid experiment runs found.", file=sys.stderr)
        sys.exit(1)

    n_cells = len(runs)
    total_trials = sum(len(info["trials"]) for info in runs.values())
    print(f"Found {n_cells} experiment cell(s), {total_trials} total trial(s)")

    # Compute stats: per-trial then aggregate
    all_stats = {}
    for cell, run_info in runs.items():
        trial_stats = [extract_stats(t["data"]) for t in run_info["trials"]]
        all_stats[cell] = aggregate_trials(trial_stats)
        all_stats[cell]["n_trials"] = len(run_info["trials"])

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

    print("Generating convergence diagnostic...")
    plot_convergence_diagnostic(runs, out_dir)

    print("Generating interaction effects...")
    plot_interaction_effects(runs, all_stats, out_dir)

    print("Generating sick/healthy grid...")
    plot_sick_healthy_grid(runs, out_dir)

    # Build heatmap matrices (mean + std)
    peak_matrix = []
    peak_std_matrix = []
    ss_matrix = []
    ss_std_matrix = []
    conv_matrix = []
    conv_std_matrix = []
    for b in BEHAVIORS:
        peak_row, peak_std_row = [], []
        ss_row, ss_std_row = [], []
        conv_row, conv_std_row = [], []
        for d in DOCTORS:
            cell = f"{b}_{d}"
            if cell in all_stats:
                peak_row.append(all_stats[cell]["peak_infected_mean"])
                peak_std_row.append(all_stats[cell].get("peak_infected_std", 0))
                ss_row.append(all_stats[cell]["steady_state_infected_mean"])
                ss_std_row.append(all_stats[cell].get("steady_state_infected_std", 0))
                conv_row.append(all_stats[cell]["convergence_time_mean"])
                conv_std_row.append(all_stats[cell].get("convergence_time_std", 0))
            else:
                peak_row.append(None)
                peak_std_row.append(0)
                ss_row.append(None)
                ss_std_row.append(0)
                conv_row.append(None)
                conv_std_row.append(0)
        peak_matrix.append(peak_row)
        peak_std_matrix.append(peak_std_row)
        ss_matrix.append(ss_row)
        ss_std_matrix.append(ss_std_row)
        conv_matrix.append(conv_row)
        conv_std_matrix.append(conv_std_row)

    # Peak infection heatmap
    peak_for_plot = [[v if v is not None else 0 for v in row] for row in peak_matrix]
    print("Generating peak infection heatmap...")
    plot_heatmap(peak_for_plot, "Peak Infection Count", "heatmap_peak.png", out_dir,
                 std_matrix=peak_std_matrix)

    # Steady-state infected heatmap
    ss_for_plot = [[v if v is not None else 0 for v in row] for row in ss_matrix]
    print("Generating steady-state infected heatmap...")
    plot_heatmap(ss_for_plot, "Steady-State Infected Count",
                 "heatmap_steady_state.png", out_dir, fmt=".1f", cmap="YlGnBu_r",
                 std_matrix=ss_std_matrix)

    # Convergence time heatmap
    conv_for_plot = [[v if v is not None else float("inf") for v in row]
                     for row in conv_matrix]
    finite_vals = [v for row in conv_for_plot for v in row if not np.isinf(v)]
    cap = max(finite_vals) * 1.2 if finite_vals else 300.0
    conv_capped = [[v if not np.isinf(v) else cap for v in row]
                   for row in conv_for_plot]
    # Cap std too for display
    conv_std_for_plot = [[s if not np.isinf(conv_for_plot[r][c]) else 0
                          for c, s in enumerate(row)]
                         for r, row in enumerate(conv_std_matrix)]
    print("Generating convergence time heatmap...")
    plot_heatmap(conv_capped, "Time to Convergence (s)",
                 "heatmap_convergence.png", out_dir, fmt=".1f", cmap="YlGnBu",
                 std_matrix=conv_std_for_plot)

    print(f"\nPlots saved to {out_dir}/")

    # Summary table
    print_summary_table(runs, all_stats)


if __name__ == "__main__":
    main()
