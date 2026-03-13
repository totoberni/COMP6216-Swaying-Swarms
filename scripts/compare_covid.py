#!/usr/bin/env python3
"""Compare simulation results against real COVID-19 data.

Overlays normalized infection curves from sim and real data.
Time mapping: 1 sim-second = TIME_SCALE_HOURS real hours.

Usage: python3 scripts/compare_covid.py [--sim-dir sim-out] [--time-scale 4]
"""
import argparse
import csv
import os
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


COVID_DATA = "data/covid_south_korea.csv"


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sim-dir", default="sim-out",
                        help="Simulation output directory")
    parser.add_argument("--time-scale", type=float, default=4.0,
                        help="Hours per sim-second (default: 4.0 = 1 sim-s -> 4h)")
    parser.add_argument("--output-dir", default=None,
                        help="Output directory for plots (default: <sim-dir>/analysis)")
    return parser.parse_args()


def load_covid_data(path):
    """Load COVID-19 CSV and return arrays."""
    dates, new_cases, new_smoothed, total_cases, population = [], [], [], [], []
    with open(path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            dates.append(row["date"])
            new_cases.append(float(row["new_cases"]) if row["new_cases"] else 0.0)
            new_smoothed.append(float(row["new_cases_smoothed"]) if row["new_cases_smoothed"] else 0.0)
            total_cases.append(float(row["total_cases"]) if row["total_cases"] else 0.0)
            pop = float(row["population"]) if row["population"] else 0.0
            population.append(pop)

    pop = population[-1] if population else 1.0
    # Day index from first date
    from datetime import datetime
    base = datetime.strptime(dates[0], "%Y-%m-%d")
    days = [(datetime.strptime(d, "%Y-%m-%d") - base).days for d in dates]

    return {
        "days": np.array(days, dtype=float),
        "dates": dates,
        "new_cases": np.array(new_cases),
        "new_smoothed": np.array(new_smoothed),
        "total_cases": np.array(total_cases),
        "population": pop,
        "cumulative_frac": np.array(total_cases) / pop,
        "daily_frac": np.array(new_smoothed) / pop,
    }


def discover_sim_runs(sim_dir):
    """Find all sim output directories with metrics.csv."""
    runs = []
    sim_path = Path(sim_dir)
    for d in sorted(sim_path.iterdir()):
        if d.is_dir() and (d / "metrics.csv").exists():
            runs.append(d)
    return runs


def load_sim_metrics(csv_path):
    """Load sim metrics.csv and return dict of arrays."""
    data = {}
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    if not rows:
        return None
    for key in rows[0]:
        vals = []
        for row in rows:
            try:
                vals.append(float(row[key]))
            except (ValueError, TypeError):
                vals.append(0.0)
        data[key] = np.array(vals)
    return data


def plot_comparison(covid, sim_runs, time_scale, output_dir):
    """Generate COVID vs sim comparison plots."""
    os.makedirs(output_dir, exist_ok=True)

    # Load all sim runs
    sim_data_list = []
    for run_dir in sim_runs:
        d = load_sim_metrics(run_dir / "metrics.csv")
        if d is not None:
            sim_data_list.append(d)

    if not sim_data_list:
        print("No simulation data found!", file=sys.stderr)
        return

    # Map sim time to real days: real_days = time_s * time_scale / 24
    for sd in sim_data_list:
        sd["real_days"] = sd["time_s"] * time_scale / 24.0

    # --- Figure 1: Overlay cumulative infection fraction ---
    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    # Panel 1: Cumulative infected fraction
    ax = axes[0]
    ax.plot(covid["days"], covid["cumulative_frac"] * 100, "k-", linewidth=2,
            label="COVID-19 (South Korea)")
    for i, sd in enumerate(sim_data_list):
        label = f"Sim run {i}" if len(sim_data_list) > 1 else "Simulation"
        alpha = 0.5 if len(sim_data_list) > 1 else 0.9
        ax.plot(sd["real_days"], sd["pct_infected"] * 100, alpha=alpha, label=label)
    ax.set_xlabel("Days")
    ax.set_ylabel("Infected fraction (%)")
    ax.set_title("Cumulative Infected Fraction")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)

    # Panel 2: Daily new infection rate
    ax = axes[1]
    ax.plot(covid["days"], covid["daily_frac"] * 1e6, "k-", linewidth=2,
            label="COVID-19 (per million)")
    for i, sd in enumerate(sim_data_list):
        # Approximate daily new infections from growth rate
        total_pop = 130  # typical sim population
        if "growth_rate" in sd:
            daily_new = np.abs(sd["growth_rate"]) / total_pop * 1e6
        else:
            # Finite difference on pct_infected
            di = np.gradient(sd["pct_infected"]) * 1e6
            daily_new = np.maximum(di, 0)
        label = f"Sim run {i}" if len(sim_data_list) > 1 else "Simulation"
        alpha = 0.5 if len(sim_data_list) > 1 else 0.9
        ax.plot(sd["real_days"], daily_new, alpha=alpha, label=label)
    ax.set_xlabel("Days")
    ax.set_ylabel("New infections (per million)")
    ax.set_title("Daily New Infection Rate")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)

    # Panel 3: Side-by-side normalized curves
    ax = axes[2]
    # Normalize both to [0, 1] range for shape comparison
    covid_norm = covid["cumulative_frac"] / (covid["cumulative_frac"].max() or 1)
    ax.plot(covid["days"] / (covid["days"].max() or 1), covid_norm, "k-",
            linewidth=2, label="COVID-19")
    for i, sd in enumerate(sim_data_list):
        sim_norm = sd["pct_infected"] / (sd["pct_infected"].max() or 1)
        t_norm = sd["real_days"] / (sd["real_days"].max() or 1)
        label = f"Sim run {i}" if len(sim_data_list) > 1 else "Simulation"
        alpha = 0.5 if len(sim_data_list) > 1 else 0.9
        ax.plot(t_norm, sim_norm, alpha=alpha, label=label)
    ax.set_xlabel("Normalized time")
    ax.set_ylabel("Normalized infected fraction")
    ax.set_title("Shape Comparison (normalized)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)

    fig.suptitle(f"Simulation vs COVID-19 (South Korea)\n"
                 f"Time scale: 1 sim-second = {time_scale:.1f} hours",
                 fontsize=13, fontweight="bold")
    fig.tight_layout()
    out_path = os.path.join(output_dir, "covid_comparison.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {out_path}")

    # --- Figure 2: SIR dynamics comparison ---
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))

    # Sim SIR curves
    ax = axes[0]
    for i, sd in enumerate(sim_data_list):
        total_pop = 130
        susceptible = total_pop - sd.get("infected", np.zeros(1)) - sd.get("recovered", np.zeros(1))
        ax.plot(sd["real_days"], sd.get("infected", []) / total_pop * 100,
                "r-", alpha=0.7, label="I (infected)" if i == 0 else "")
        ax.plot(sd["real_days"], sd.get("recovered", np.zeros(1)) / total_pop * 100,
                "g-", alpha=0.7, label="R (recovered)" if i == 0 else "")
        ax.plot(sd["real_days"], susceptible / total_pop * 100,
                "b-", alpha=0.7, label="S (susceptible)" if i == 0 else "")
    ax.set_xlabel("Days")
    ax.set_ylabel("Population fraction (%)")
    ax.set_title("Simulation SIR Dynamics")
    ax.legend()
    ax.grid(True, alpha=0.3)

    # COVID SIR-like curves
    ax = axes[1]
    total_deaths_arr = np.array([
        float(x) if x else 0.0 for x in
        [row for row in [0.0] * len(covid["total_cases"])]  # placeholder
    ])
    # Approximate recovered from COVID data: assume CFR ~1.5%, rest recovered
    # Simple approximation: recovered ~ total_cases - active (we don't have active)
    # Just show total cases trajectory
    ax.plot(covid["days"], covid["total_cases"] / covid["population"] * 100,
            "r-", linewidth=2, label="Total cases")
    ax.plot(covid["days"], covid["new_smoothed"] / covid["population"] * 100,
            "orange", linewidth=2, label="New cases (smoothed)")
    ax.set_xlabel("Days")
    ax.set_ylabel("Population fraction (%)")
    ax.set_title("COVID-19 South Korea (Feb-Apr 2020)")
    ax.legend()
    ax.grid(True, alpha=0.3)

    fig.suptitle("SIR Dynamics Comparison", fontsize=13, fontweight="bold")
    fig.tight_layout()
    out_path = os.path.join(output_dir, "covid_sir_comparison.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved: {out_path}")


def main():
    args = parse_args()

    if not os.path.exists(COVID_DATA):
        print(f"COVID data not found at {COVID_DATA}", file=sys.stderr)
        print("Run: python3 scripts/fetch_covid_data.py", file=sys.stderr)
        sys.exit(1)

    covid = load_covid_data(COVID_DATA)
    print(f"COVID data: {len(covid['days'])} days, "
          f"peak fraction: {covid['cumulative_frac'].max()*100:.4f}%")

    sim_runs = discover_sim_runs(args.sim_dir)
    if not sim_runs:
        print(f"No simulation runs found in {args.sim_dir}/", file=sys.stderr)
        sys.exit(1)
    print(f"Found {len(sim_runs)} simulation run(s)")

    output_dir = args.output_dir or os.path.join(args.sim_dir, "analysis")
    plot_comparison(covid, sim_runs, args.time_scale, output_dir)


if __name__ == "__main__":
    main()
