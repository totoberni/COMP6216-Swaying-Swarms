#!/usr/bin/env python3
"""Compare ABM simulation output against real COVID-19 epidemic curves.

Generates publication-quality fan-chart comparisons for the COMP6216 report.

Figure 1 -- covid_shape_comparison.png:
    Normalized [0,1] cumulative and daily incidence comparison between the
    simulation ensemble and real COVID data.  RMSE and R^2 annotated.

Figure 2 -- sir_dynamics_2x2.png:
    S/I/R compartment fan charts with COVID overlay on the Infected panel.

Usage:
    python3 scripts/compare_covid.py --dataset italy --sweep-dir sim-out
    python3 scripts/compare_covid.py --dataset south_korea --sweep-dir sim-out
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


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DATASET_CONFIG = {
    "italy": {
        "csv": "data/covid_italy.csv",
        "label": "Italy (Feb\u2013May 2020)",
        "color": "#d95f02",
    },
    "south_korea": {
        "csv": "data/covid_south_korea.csv",
        "label": "South Korea (Feb\u2013Apr 2020)",
        "color": "#d95f02",
    },
}

PUB_RCPARAMS = {
    "font.size": 9,
    "axes.labelsize": 9,
    "axes.titlesize": 10,
    "axes.linewidth": 0.5,
    "lines.linewidth": 1.0,
    "xtick.major.width": 0.5,
    "ytick.major.width": 0.5,
    "figure.dpi": 300,
    "savefig.dpi": 300,
    "text.color": "#373737",
    "axes.edgecolor": "#373737",
    "xtick.color": "#373737",
    "ytick.color": "#373737",
}

TIME_SCALE_HOURS = 4.0  # 1 sim-second = 4 real hours


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--dataset", choices=list(DATASET_CONFIG.keys()),
                        default="italy",
                        help="COVID dataset to compare against (default: italy)")
    parser.add_argument("--sweep-dir", default="sim-out",
                        help="Directory containing out*/ simulation runs")
    parser.add_argument("--output-dir", default=None,
                        help="Output directory (default: <sweep-dir>/analysis)")
    return parser.parse_args()


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------

def sim_to_days(time_s):
    """Convert sim time (seconds) to real-world days."""
    return time_s * TIME_SCALE_HOURS / 24.0


def load_covid_data(csv_path):
    """Load COVID-19 CSV and return structured arrays."""
    dates, new_cases, new_smoothed, total_cases = [], [], [], []
    pop_val = 1.0
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            dates.append(row["date"])
            new_cases.append(float(row["new_cases"] or 0))
            new_smoothed.append(float(row["new_cases_smoothed"] or 0))
            total_cases.append(float(row["total_cases"] or 0))
            if row.get("population"):
                pop_val = float(row["population"])

    from datetime import datetime
    base = datetime.strptime(dates[0], "%Y-%m-%d")
    days = np.array([(datetime.strptime(d, "%Y-%m-%d") - base).days
                     for d in dates], dtype=float)

    total_cases = np.array(total_cases)
    new_smoothed = np.array(new_smoothed)
    return {
        "days": days,
        "new_smoothed": new_smoothed,
        "total_cases": total_cases,
        "population": pop_val,
        "cumulative_frac": total_cases / pop_val,
    }


def discover_sim_runs(sweep_dir):
    """Find all out*/ directories containing metrics.csv."""
    runs = []
    sweep_path = Path(sweep_dir)
    if not sweep_path.exists():
        return runs
    for d in sorted(sweep_path.iterdir()):
        if d.is_dir() and d.name.startswith("out"):
            if (d / "metrics.csv").exists():
                runs.append(d)
    return runs


def load_sim_metrics(csv_path):
    """Load a single metrics.csv into a dict of numpy arrays."""
    data = {}
    with open(csv_path, "r") as f:
        reader = csv.DictReader(f)
        rows = list(reader)
    if not rows:
        return None
    for key in rows[0]:
        key_s = key.strip()
        vals = []
        for row in rows:
            try:
                vals.append(float(row[key]))
            except (ValueError, TypeError):
                vals.append(0.0)
        data[key_s] = np.array(vals)
    return data


# ---------------------------------------------------------------------------
# Statistics helpers
# ---------------------------------------------------------------------------

def compute_percentiles(runs_data, column):
    """Per-timestep percentiles across runs. Truncates to shortest run."""
    series = []
    time_s = None
    min_len = float("inf")

    for d in runs_data:
        if column not in d:
            continue
        arr = d[column]
        min_len = min(min_len, len(arr))
        series.append(arr)
        if time_s is None:
            time_s = d.get("time_s")

    if not series or time_s is None:
        return None

    min_len = int(min_len)
    time_s = time_s[:min_len]
    stacked = np.array([s[:min_len] for s in series])

    return {
        "time_s": time_s,
        "median": np.median(stacked, axis=0),
        "p25": np.percentile(stacked, 25, axis=0),
        "p75": np.percentile(stacked, 75, axis=0),
        "p5": np.percentile(stacked, 5, axis=0),
        "p95": np.percentile(stacked, 95, axis=0),
        "n_runs": len(series),
    }


def scale_percentiles(pct, factor):
    """Multiply all percentile arrays by *factor*."""
    return {k: pct[k] * factor
            for k in ("median", "p25", "p75", "p5", "p95")}


def estimate_total_pop(runs_data):
    """Infer total agent count from infected / pct_infected."""
    for d in runs_data:
        infected = d.get("infected", np.array([]))
        pct = d.get("pct_infected", np.array([]))
        if len(infected) > 0 and len(pct) > 0:
            mask = (infected > 0) & (pct > 0)
            if mask.any():
                idx = int(np.argmax(mask))
                return round(infected[idx] / pct[idx])
    return 135  # fallback


# ---------------------------------------------------------------------------
# Drawing helpers
# ---------------------------------------------------------------------------

def draw_fan_chart(ax, x, pct, color="#1b9e77", label=None):
    """Fan chart: 90% PI + IQR + median line."""
    ax.fill_between(x, pct["p5"], pct["p95"],
                    alpha=0.15, color=color, linewidth=0)
    ax.fill_between(x, pct["p25"], pct["p75"],
                    alpha=0.30, color=color, linewidth=0)
    ax.plot(x, pct["median"], color=color, linewidth=1.5, label=label)


# ---------------------------------------------------------------------------
# Figure 1 -- Normalized shape comparison
# ---------------------------------------------------------------------------

def plot_shape_comparison(covid, runs_data, dataset_cfg, output_dir):
    """Two-panel figure: normalized cumulative + daily incidence."""
    plt.rcParams.update(PUB_RCPARAMS)

    # -- Cumulative percentiles (per-run normalization for true shape comparison) --
    # Normalize each run to [0,1] BEFORE computing percentiles so that bands
    # represent uncertainty in shape, not magnitude.
    for d in runs_data:
        if "pct_infected" in d:
            run_max = d["pct_infected"].max()
            d["_cum_norm"] = d["pct_infected"] / run_max if run_max > 0 else d["pct_infected"]

    cum_pct = compute_percentiles(runs_data, "_cum_norm")
    if cum_pct is None:
        print("No pct_infected data for shape comparison", file=sys.stderr)
        return

    n_runs = cum_pct["n_runs"]

    # Time normalization only (values already in [0,1])
    sim_t_max = cum_pct["time_s"].max() or 1.0
    sim_t_norm = cum_pct["time_s"] / sim_t_max
    cum_norm = {k: cum_pct[k] for k in ("median", "p25", "p75", "p5", "p95")}

    # Normalize COVID cumulative to [0, 1]
    covid_cum_max = covid["cumulative_frac"].max() or 1.0
    covid_t_max = covid["days"].max() or 1.0
    covid_t_norm = covid["days"] / covid_t_max
    covid_cum_norm = covid["cumulative_frac"] / covid_cum_max

    # RMSE + R^2 (median vs COVID, interpolated to COVID time points)
    sim_interp = np.interp(covid_t_norm, sim_t_norm, cum_norm["median"])
    residuals = sim_interp - covid_cum_norm
    rmse = float(np.sqrt(np.mean(residuals ** 2)))
    ss_res = float(np.sum(residuals ** 2))
    ss_tot = float(np.sum((covid_cum_norm - covid_cum_norm.mean()) ** 2))
    r_sq = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0

    # -- Daily incidence per run (per-run normalization) --
    for d in runs_data:
        if "pct_infected" in d:
            daily = np.gradient(d["pct_infected"])
            daily_max = daily.max()
            d["_daily_norm"] = daily / daily_max if daily_max > 0 else daily
    daily_pct = compute_percentiles(runs_data, "_daily_norm")

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4))

    # Left panel: cumulative
    draw_fan_chart(ax1, sim_t_norm, cum_norm, color="#1b9e77",
                   label=f"ABM (n={n_runs})")
    ax1.plot(covid_t_norm, covid_cum_norm, color=dataset_cfg["color"],
             marker="o", markersize=3, linewidth=1.0, markeredgewidth=0,
             label=dataset_cfg["label"])
    ax1.text(0.05, 0.95,
             f"RMSE = {rmse:.3f}\nR\u00b2 = {r_sq:.3f}",
             transform=ax1.transAxes, fontsize=8, verticalalignment="top",
             bbox=dict(boxstyle="round,pad=0.3", facecolor="white",
                       edgecolor="#cccccc", alpha=0.8))
    ax1.set_xlabel("Normalized Time")
    ax1.set_ylabel("Normalized Cumulative Infected Fraction")
    ax1.set_title("Cumulative Incidence")
    ax1.legend(fontsize=7, loc="lower right")
    ax1.set_xlim(0, 1)
    ax1.set_ylim(0, 1.05)

    # Right panel: daily (already per-run normalized)
    if daily_pct is not None:
        daily_norm = {k: daily_pct[k] for k in ("median", "p25", "p75", "p5", "p95")}
        draw_fan_chart(ax2, sim_t_norm[:len(daily_norm["median"])],
                       daily_norm, color="#1b9e77",
                       label=f"ABM (n={n_runs})")

    covid_daily_max = covid["new_smoothed"].max() or 1.0
    covid_daily_norm = covid["new_smoothed"] / covid_daily_max
    ax2.plot(covid_t_norm, covid_daily_norm, color=dataset_cfg["color"],
             marker="o", markersize=3, linewidth=1.0, markeredgewidth=0,
             label=dataset_cfg["label"])
    ax2.set_xlabel("Normalized Time")
    ax2.set_ylabel("Normalized Daily Incidence")
    ax2.set_title("Daily Incidence")
    ax2.legend(fontsize=7, loc="upper right")
    ax2.set_xlim(0, 1)

    fig.tight_layout()
    out_path = os.path.join(output_dir, "covid_shape_comparison.png")
    fig.savefig(out_path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")
    print(f"  RMSE={rmse:.4f}, R\u00b2={r_sq:.4f} (n={n_runs} runs)")


# ---------------------------------------------------------------------------
# Figure 2 -- SIR dynamics 2x2
# ---------------------------------------------------------------------------

def plot_sir_dynamics(covid, runs_data, dataset_cfg, output_dir):
    """2x2 grid: S, I (+ COVID overlay), R, combined medians."""
    plt.rcParams.update(PUB_RCPARAMS)

    total_pop = estimate_total_pop(runs_data)

    # Compute fractional S/I/R per run
    for d in runs_data:
        infected = d.get("infected", np.zeros(1))
        recovered = d.get("recovered", np.zeros(1))
        n = min(len(infected), len(recovered), len(d.get("time_s", [])))
        d["_s_frac"] = (total_pop - infected[:n] - recovered[:n]) / total_pop
        d["_i_frac"] = infected[:n] / total_pop
        d["_r_frac"] = recovered[:n] / total_pop

    s_pct = compute_percentiles(runs_data, "_s_frac")
    i_pct = compute_percentiles(runs_data, "_i_frac")
    r_pct = compute_percentiles(runs_data, "_r_frac")

    if any(p is None for p in (s_pct, i_pct, r_pct)):
        print("Missing SIR data for dynamics plot", file=sys.stderr)
        return

    days = sim_to_days(s_pct["time_s"])

    fig, axes = plt.subplots(2, 2, figsize=(10, 7), sharex=True)

    # (0,0) Susceptible
    ax = axes[0, 0]
    draw_fan_chart(ax, days, scale_percentiles(s_pct, 100),
                   color="#1b9e77", label="Susceptible")
    ax.set_ylabel("Population (%)")
    ax.set_title("Susceptible")
    ax.legend(fontsize=7)

    # (0,1) Infected + COVID daily incidence overlay
    ax = axes[0, 1]
    draw_fan_chart(ax, days, scale_percentiles(i_pct, 100),
                   color="#d95319", label="Infected (ABM)")
    # Overlay COVID daily incidence (new cases / pop), scaled to sim peak
    # Daily incidence peaks and declines like prevalence — comparable quantities
    sim_peak_pct = i_pct["median"].max() * 100
    covid_daily_frac = covid["new_smoothed"] / covid["population"] * 100
    covid_daily_peak = covid_daily_frac.max() or 1.0
    covid_scaled = covid_daily_frac * (sim_peak_pct / covid_daily_peak)
    ax.plot(covid["days"], covid_scaled, color=dataset_cfg["color"],
            marker="o", markersize=2, linewidth=0.8, markeredgewidth=0,
            label=f"{dataset_cfg['label']} daily (scaled)", alpha=0.8)
    ax.set_ylabel("Population (%)")
    ax.set_title("Infected")
    ax.legend(fontsize=7)

    # (1,0) Recovered
    ax = axes[1, 0]
    draw_fan_chart(ax, days, scale_percentiles(r_pct, 100),
                   color="#7570b3", label="Recovered")
    ax.set_xlabel("Days")
    ax.set_ylabel("Population (%)")
    ax.set_title("Recovered")
    ax.legend(fontsize=7)

    # (1,1) Combined medians
    ax = axes[1, 1]
    ax.plot(days, s_pct["median"] * 100, color="#1b9e77", lw=1.2, label="S")
    ax.plot(days, i_pct["median"] * 100, color="#d95319", lw=1.2, label="I")
    ax.plot(days, r_pct["median"] * 100, color="#7570b3", lw=1.2, label="R")
    ax.set_xlabel("Days")
    ax.set_ylabel("Population (%)")
    ax.set_title("Combined SIR (median)")
    ax.legend(fontsize=7)

    fig.tight_layout()
    out_path = os.path.join(output_dir, "sir_dynamics_2x2.png")
    fig.savefig(out_path, dpi=300, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    args = parse_args()
    dataset_cfg = DATASET_CONFIG[args.dataset]
    csv_path = dataset_cfg["csv"]

    if not os.path.exists(csv_path):
        print(f"COVID data not found: {csv_path}", file=sys.stderr)
        print("Run: python3 scripts/fetch_covid_data.py", file=sys.stderr)
        sys.exit(1)

    covid = load_covid_data(csv_path)
    print(f"COVID data ({args.dataset}): {len(covid['days'])} days, "
          f"peak fraction: {covid['cumulative_frac'].max()*100:.4f}%")

    sim_runs = discover_sim_runs(args.sweep_dir)
    if not sim_runs:
        print(f"No simulation runs in {args.sweep_dir}/", file=sys.stderr)
        sys.exit(1)
    print(f"Found {len(sim_runs)} simulation run(s)")

    runs_data = []
    for run_dir in sim_runs:
        d = load_sim_metrics(run_dir / "metrics.csv")
        if d is not None:
            runs_data.append(d)

    if not runs_data:
        print("No valid simulation data loaded", file=sys.stderr)
        sys.exit(1)

    output_dir = args.output_dir or os.path.join(args.sweep_dir, "analysis")
    os.makedirs(output_dir, exist_ok=True)

    print("Generating shape comparison (Figure 1)...")
    plot_shape_comparison(covid, runs_data, dataset_cfg, output_dir)

    print("Generating SIR dynamics (Figure 2)...")
    plot_sir_dynamics(covid, runs_data, dataset_cfg, output_dir)

    print("Done.")


if __name__ == "__main__":
    main()
