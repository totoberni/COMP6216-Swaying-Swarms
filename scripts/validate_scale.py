#!/usr/bin/env python3
"""Scale validation: compare SIR dynamics across population scales.

Usage:
    .venv/bin/python3 scripts/validate_scale.py \
        --runs sim-out/out0:130 sim-out/out1:100k sim-out/out2:1.4M \
        --output sim-out/validation
"""

import argparse
import os
import sys
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


def load_run(path: str) -> pd.DataFrame:
    csv_path = os.path.join(path, "metrics.csv")
    if not os.path.exists(csv_path):
        print(f"ERROR: {csv_path} not found", file=sys.stderr)
        sys.exit(1)
    df = pd.read_csv(csv_path)
    total = df["infected"] + df["recovered"] + (
        df["infected"].iloc[0] + df["recovered"].iloc[0]
        if df["infected"].iloc[0] + df["recovered"].iloc[0] > 0
        else 1
    )
    # Estimate total pop from first frame: total = pop - infected - recovered (susceptible)
    # Use pct_infected to back-calculate: total_pop = infected / pct_infected
    first_nonzero = df[df["pct_infected"] > 0].head(1)
    if len(first_nonzero) > 0:
        row = first_nonzero.iloc[0]
        total_pop = int(round(row["infected"] / row["pct_infected"])) if row["pct_infected"] > 0 else 1
    else:
        total_pop = 1
    df["pct_infected_calc"] = df["infected"] / total_pop * 100.0
    df["pct_recovered_calc"] = df["recovered"] / total_pop * 100.0
    df["pct_susceptible"] = 100.0 - df["pct_infected_calc"] - df["pct_recovered_calc"]
    return df


def parse_run_arg(arg: str):
    if ":" in arg:
        path, label = arg.rsplit(":", 1)
    else:
        path = arg
        label = os.path.basename(arg)
    return path, label


def main():
    parser = argparse.ArgumentParser(description="Scale validation for SIR dynamics")
    parser.add_argument("--runs", nargs="+", required=True,
                        help="path:label pairs (e.g., sim-out/out0:130)")
    parser.add_argument("--output", default="sim-out/validation",
                        help="Output directory for plots")
    args = parser.parse_args()

    runs = []
    for r in args.runs:
        path, label = parse_run_arg(r)
        df = load_run(path)
        runs.append((label, df))

    os.makedirs(args.output, exist_ok=True)

    colors = plt.cm.Set1(np.linspace(0, 1, max(len(runs), 3)))

    # --- Plot 1: Infection curves overlay ---
    fig, ax = plt.subplots(figsize=(10, 6))
    for idx, (label, df) in enumerate(runs):
        ax.plot(df["time_s"], df["pct_infected_calc"],
                label=f"{label} boids", color=colors[idx], linewidth=1.5)
    ax.set_xlabel("Simulation Time (s)")
    ax.set_ylabel("Infected (%)")
    ax.set_title("Infection Curves Across Scales")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(args.output, "infection_overlay.png"), dpi=150)
    plt.close(fig)

    # --- Plot 2: SIR phase plot (% infected vs % recovered) ---
    fig, ax = plt.subplots(figsize=(10, 6))
    for idx, (label, df) in enumerate(runs):
        ax.plot(df["pct_recovered_calc"], df["pct_infected_calc"],
                label=f"{label} boids", color=colors[idx], linewidth=1.5, alpha=0.8)
        # Mark start and end
        ax.scatter(df["pct_recovered_calc"].iloc[0], df["pct_infected_calc"].iloc[0],
                   color=colors[idx], marker="o", s=60, zorder=5)
        ax.scatter(df["pct_recovered_calc"].iloc[-1], df["pct_infected_calc"].iloc[-1],
                   color=colors[idx], marker="s", s=60, zorder=5)
    ax.set_xlabel("Recovered (%)")
    ax.set_ylabel("Infected (%)")
    ax.set_title("SIR Phase Plot (circle=start, square=end)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(args.output, "sir_phase.png"), dpi=150)
    plt.close(fig)

    # --- Plot 3: Peak infection bar chart ---
    fig, ax = plt.subplots(figsize=(8, 5))
    labels_list = [label for label, _ in runs]
    peaks = [df["pct_infected_calc"].max() for _, df in runs]
    bars = ax.bar(labels_list, peaks, color=[colors[i] for i in range(len(runs))], alpha=0.8)
    for bar, peak in zip(bars, peaks):
        ax.text(bar.get_x() + bar.get_width() / 2, bar.get_height() + 0.5,
                f"{peak:.1f}%", ha="center", va="bottom", fontsize=10)
    ax.set_ylabel("Peak Infected (%)")
    ax.set_title("Peak Infection Comparison")
    ax.grid(True, alpha=0.3, axis="y")
    fig.tight_layout()
    fig.savefig(os.path.join(args.output, "peak_comparison.png"), dpi=150)
    plt.close(fig)

    # --- Plot 4: Convergence (steady-state) ---
    fig, axes = plt.subplots(1, 2, figsize=(12, 5))
    final_infected = []
    final_recovered = []
    for _, df in runs:
        # Average last 10% of simulation as steady-state estimate
        tail = df.tail(max(1, len(df) // 10))
        final_infected.append(tail["pct_infected_calc"].mean())
        final_recovered.append(tail["pct_recovered_calc"].mean())

    axes[0].bar(labels_list, final_infected,
                color=[colors[i] for i in range(len(runs))], alpha=0.8)
    axes[0].set_ylabel("Infected (%)")
    axes[0].set_title("Steady-State Infected")
    axes[0].grid(True, alpha=0.3, axis="y")

    axes[1].bar(labels_list, final_recovered,
                color=[colors[i] for i in range(len(runs))], alpha=0.8)
    axes[1].set_ylabel("Recovered (%)")
    axes[1].set_title("Steady-State Recovered")
    axes[1].grid(True, alpha=0.3, axis="y")

    fig.tight_layout()
    fig.savefig(os.path.join(args.output, "convergence.png"), dpi=150)
    plt.close(fig)

    # --- Summary table ---
    print("\n=== Scale Validation Summary ===\n")
    print(f"{'Scale':<12} {'Peak Inf%':>10} {'Final Inf%':>11} {'Final Rec%':>11} {'Frames':>8}")
    print("-" * 56)
    for i, (label, df) in enumerate(runs):
        print(f"{label:<12} {peaks[i]:>9.1f}% {final_infected[i]:>10.1f}% "
              f"{final_recovered[i]:>10.1f}% {len(df):>8}")

    print(f"\nPlots saved to {args.output}/")


if __name__ == "__main__":
    main()
