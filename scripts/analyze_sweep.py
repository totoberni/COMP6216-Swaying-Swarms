#!/usr/bin/env python3
"""Analyze Monte Carlo parameter sweep results.

Two-phase pipeline:
  Phase 1: Parallel CSV collection -> sweep_summary.csv (cached)
  Phase 2: Publication plots from summary DataFrame

Output: sim-out/sweep-analysis/*.png + sweep_summary.csv

Usage: python3 scripts/analyze_sweep.py --sim-dir sim-out --manifest configs/sweep/manifest.json
       [--skip-collect]  # reuse cached sweep_summary.csv
"""
import argparse
import configparser
import json
import os
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import numpy as np

# Try pandas, fall back to manual
try:
    import pandas as pd
    HAS_PANDAS = True
except ImportError:
    HAS_PANDAS = False
    print("Warning: pandas not available; using basic CSV collection", file=sys.stderr)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sim-dir", default="sim-out",
                        help="Simulation output directory")
    parser.add_argument("--manifest", required=True,
                        help="Path to sweep manifest.json")
    parser.add_argument("--skip-collect", action="store_true",
                        help="Skip Phase 1, reuse cached sweep_summary.csv")
    parser.add_argument("--output-dir", default=None,
                        help="Output directory (default: <sim-dir>/sweep-analysis)")
    return parser.parse_args()


# ── Phase 1: Collect ─────────────────────────────────────────────────

def extract_stats_from_csv(csv_path):
    """Read metrics.csv and compute summary statistics."""
    import csv as csv_mod
    with open(csv_path, "r") as f:
        reader = csv_mod.DictReader(f)
        rows = list(reader)

    if not rows:
        return None

    time_s = [float(r["time_s"]) for r in rows]
    infected = [float(r["infected"]) for r in rows]
    recovered = [float(r.get("recovered", 0)) for r in rows]
    pct_infected = [float(r.get("pct_infected", 0)) for r in rows]
    growth_rate = [float(r.get("growth_rate", 0)) for r in rows]

    duration = time_s[-1] if time_s else 0
    peak_infected = max(infected) if infected else 0
    peak_pct = max(pct_infected) if pct_infected else 0

    # Steady-state: average of last 20% of data
    tail_start = int(len(infected) * 0.8)
    ss_infected = np.mean(infected[tail_start:]) if tail_start < len(infected) else 0
    ss_pct = np.mean(pct_infected[tail_start:]) if tail_start < len(pct_infected) else 0

    # Convergence time: first time pct_infected drops below 50% of peak after peaking
    convergence_time = duration  # default: never converged
    if peak_pct > 0:
        peak_idx = pct_infected.index(peak_pct)
        threshold = peak_pct * 0.5
        for i in range(peak_idx, len(pct_infected)):
            if pct_infected[i] < threshold:
                convergence_time = time_s[i]
                break

    # Mean growth rate in first 2 "hours" (sim seconds * time_scale / 24 * 2)
    # For simplicity, first 30 seconds of sim time
    early_growth = [g for t, g in zip(time_s, growth_rate) if t <= 30.0]
    mean_growth_early = np.mean(early_growth) if early_growth else 0

    # Final recovered count
    final_recovered = recovered[-1] if recovered else 0

    return {
        "peak_infected": peak_infected,
        "peak_pct": peak_pct,
        "steady_state_infected": ss_infected,
        "steady_state_pct": ss_pct * 100,  # percentage
        "convergence_time": convergence_time,
        "mean_growth_early": mean_growth_early,
        "final_recovered": final_recovered,
        "duration": duration,
    }


def read_config_classification(config_path):
    """Read config_used.ini and extract B/D classification."""
    cp = configparser.ConfigParser()
    cp.read(str(config_path))

    # Classify formation
    noise = cp.getfloat("normal_swarm", "noise_factor", fallback=0.0)
    fov = cp.getfloat("normal_swarm", "fov", fallback=3.14)
    if noise > 0:
        formation = "B3"
    elif fov < 2.0:
        formation = "B1"
    else:
        formation = "B2"

    # Classify doctor behavior
    behavior = cp.get("doctor_swarm", "behavior", fallback="normal").strip()
    if behavior in ("nearest", "seek_nearest"):
        doctor_type = "SeekNearest"
    elif behavior in ("centroid", "seek_centroid"):
        doctor_type = "SeekCentroid"
    else:
        doctor_type = "Normal"

    return formation, doctor_type


def collect_one_run(run_dir_str):
    """Collect stats from a single run directory."""
    run_dir = Path(run_dir_str)
    csv_path = run_dir / "metrics.csv"
    config_path = run_dir / "config_used.ini"

    if not csv_path.exists():
        return None

    stats = extract_stats_from_csv(str(csv_path))
    if stats is None:
        return None

    if config_path.exists():
        formation, doctor_type = read_config_classification(str(config_path))
        stats["formation"] = formation
        stats["doctor_type"] = doctor_type

    stats["run_dir"] = run_dir.name
    return stats


def collect_all_runs(sim_dir, manifest_path):
    """Parallel collection of all run stats, merged with manifest params."""
    sim_path = Path(sim_dir)
    run_dirs = sorted([
        str(d) for d in sim_path.iterdir()
        if d.is_dir() and (d / "metrics.csv").exists()
    ])

    if not run_dirs:
        print("No run directories found!", file=sys.stderr)
        return None

    print(f"Collecting stats from {len(run_dirs)} runs...")

    with ProcessPoolExecutor(max_workers=min(8, len(run_dirs))) as pool:
        results = list(pool.map(collect_one_run, run_dirs))

    results = [r for r in results if r is not None]
    if not results:
        print("No valid results collected!", file=sys.stderr)
        return None

    if HAS_PANDAS:
        summary_df = pd.DataFrame(results)
    else:
        print("pandas required for full analysis", file=sys.stderr)
        return None

    # Merge with manifest parameter values
    with open(manifest_path, "r") as f:
        manifest = json.load(f)

    # Map run_dir index to sweep config params
    # Runs are numbered out0, out1, ... in order they were created
    # Manifest has sweep_000.ini, sweep_001.ini, ...
    param_entries = manifest.get("params", {})
    config_names = sorted(param_entries.keys())

    # Create mapping: run index -> param dict
    param_rows = []
    for _, row in summary_df.iterrows():
        run_idx_str = row["run_dir"].replace("out", "")
        try:
            run_idx = int(run_idx_str)
        except ValueError:
            param_rows.append({})
            continue

        if run_idx < len(config_names):
            config_name = config_names[run_idx]
            param_rows.append(param_entries.get(config_name, {}))
        else:
            param_rows.append({})

    param_df = pd.DataFrame(param_rows)
    if not param_df.empty:
        summary_df = pd.concat([summary_df.reset_index(drop=True),
                                 param_df.reset_index(drop=True)], axis=1)

    return summary_df


# ── Phase 2: Plots ───────────────────────────────────────────────────

PARAM_COLS = ["p_cure", "doctor_seek_radius", "doctor_seek_weight",
              "initial_doctor_count", "r_interact_doctor"]
METRIC_COLS = ["steady_state_pct", "convergence_time", "peak_pct", "mean_growth_early"]
METRIC_LABELS = {
    "steady_state_pct": "Steady-State Infected (%)",
    "convergence_time": "Convergence Time (s)",
    "peak_pct": "Peak Infected (fraction)",
    "mean_growth_early": "Early Growth Rate",
}
PARAM_LABELS = {
    "p_cure": "Cure Probability",
    "doctor_seek_radius": "Seek Radius",
    "doctor_seek_weight": "Seek Weight",
    "initial_doctor_count": "Doctor Count",
    "r_interact_doctor": "Interaction Radius",
}


def plot_correlation_heatmap(df, output_dir):
    """Spearman rank correlation: params vs metrics."""
    available_params = [c for c in PARAM_COLS if c in df.columns]
    available_metrics = [c for c in METRIC_COLS if c in df.columns]

    if not available_params or not available_metrics:
        print("  Skipping correlation heatmap: missing columns")
        return

    subset = df[available_params + available_metrics].dropna()
    if len(subset) < 5:
        print("  Skipping correlation heatmap: too few samples")
        return

    corr = subset.corr(method="spearman")
    # Extract params vs metrics sub-matrix
    corr_sub = corr.loc[available_params, available_metrics]

    fig, ax = plt.subplots(figsize=(8, 6))
    im = ax.imshow(corr_sub.values, cmap="RdBu_r", vmin=-1, vmax=1, aspect="auto")

    ax.set_xticks(range(len(available_metrics)))
    ax.set_xticklabels([METRIC_LABELS.get(m, m) for m in available_metrics],
                       rotation=30, ha="right", fontsize=9)
    ax.set_yticks(range(len(available_params)))
    ax.set_yticklabels([PARAM_LABELS.get(p, p) for p in available_params], fontsize=9)

    # Annotate cells
    for i in range(len(available_params)):
        for j in range(len(available_metrics)):
            val = corr_sub.values[i, j]
            color = "white" if abs(val) > 0.5 else "black"
            ax.text(j, i, f"{val:.2f}", ha="center", va="center",
                    color=color, fontsize=10, fontweight="bold")

    fig.colorbar(im, ax=ax, label="Spearman Correlation")
    ax.set_title("Parameter-Metric Correlation (Spearman)", fontsize=13, fontweight="bold")
    fig.tight_layout()
    path = os.path.join(output_dir, "correlation_heatmap.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def identify_pareto_front(x, y):
    """Find Pareto-optimal indices (minimize both x and y)."""
    n = len(x)
    is_pareto = np.ones(n, dtype=bool)
    for i in range(n):
        for j in range(n):
            if i == j:
                continue
            if x[j] <= x[i] and y[j] <= y[i] and (x[j] < x[i] or y[j] < y[i]):
                is_pareto[i] = False
                break
    return is_pareto


def plot_pareto_front(df, output_dir):
    """Scatter: convergence_time vs steady_state_pct, colored by doctor behavior."""
    if "convergence_time" not in df.columns or "steady_state_pct" not in df.columns:
        print("  Skipping Pareto front: missing columns")
        return

    clean = df.dropna(subset=["convergence_time", "steady_state_pct"])
    if len(clean) < 3:
        print("  Skipping Pareto front: too few samples")
        return

    fig, ax = plt.subplots(figsize=(10, 7))

    behavior_colors = {"Normal": "#1f77b4", "SeekNearest": "#ff7f0e", "SeekCentroid": "#2ca02c"}
    formation_markers = {"B1": "o", "B2": "s", "B3": "^"}

    x = clean["convergence_time"].values
    y = clean["steady_state_pct"].values

    # Plot each combination
    for behavior, color in behavior_colors.items():
        for formation, marker in formation_markers.items():
            mask = (clean.get("doctor_type", "") == behavior) & \
                   (clean.get("formation", "") == formation)
            if hasattr(mask, "sum") and mask.sum() > 0:
                ax.scatter(x[mask], y[mask], c=color, marker=marker,
                          alpha=0.5, s=30, label=f"{behavior} / {formation}")

    # Highlight Pareto front
    pareto_mask = identify_pareto_front(x, y)
    if pareto_mask.any():
        ax.scatter(x[pareto_mask], y[pareto_mask], facecolors="none",
                  edgecolors="black", s=120, linewidths=2, label="Pareto optimal",
                  zorder=5)
        # Sort and draw Pareto line
        pareto_x = x[pareto_mask]
        pareto_y = y[pareto_mask]
        sort_idx = np.argsort(pareto_x)
        ax.plot(pareto_x[sort_idx], pareto_y[sort_idx], "k--", alpha=0.4, linewidth=1)

    ax.set_xlabel("Convergence Time (s)", fontsize=11)
    ax.set_ylabel("Steady-State Infected (%)", fontsize=11)
    ax.set_title("Pareto Front: Containment Speed vs Effectiveness", fontsize=13,
                 fontweight="bold")
    ax.legend(loc="upper right", fontsize=8, ncol=2)
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    path = os.path.join(output_dir, "pareto_front.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def plot_sensitivity_tornado(df, output_dir):
    """Horizontal bar chart of absolute Spearman correlations per metric."""
    available_params = [c for c in PARAM_COLS if c in df.columns]
    available_metrics = [c for c in METRIC_COLS if c in df.columns]

    if not available_params or not available_metrics:
        print("  Skipping sensitivity tornado: missing columns")
        return

    subset = df[available_params + available_metrics].dropna()
    if len(subset) < 5:
        print("  Skipping sensitivity tornado: too few samples")
        return

    corr = subset.corr(method="spearman")
    n_metrics = len(available_metrics)
    fig, axes = plt.subplots(1, n_metrics, figsize=(4 * n_metrics, 5), sharey=True)
    if n_metrics == 1:
        axes = [axes]

    for idx, metric in enumerate(available_metrics):
        ax = axes[idx]
        vals = corr.loc[available_params, metric].values
        abs_vals = np.abs(vals)
        sort_idx = np.argsort(abs_vals)

        labels = [PARAM_LABELS.get(available_params[i], available_params[i])
                  for i in sort_idx]
        sorted_vals = vals[sort_idx]
        colors = ["#d62728" if v > 0 else "#1f77b4" for v in sorted_vals]

        ax.barh(range(len(labels)), sorted_vals, color=colors)
        ax.set_yticks(range(len(labels)))
        ax.set_yticklabels(labels, fontsize=9)
        ax.set_xlabel("Spearman ρ")
        ax.set_title(METRIC_LABELS.get(metric, metric), fontsize=10)
        ax.axvline(0, color="black", linewidth=0.5)
        ax.set_xlim(-1, 1)
        ax.grid(True, alpha=0.2, axis="x")

    fig.suptitle("Sensitivity Analysis (Tornado Chart)", fontsize=13, fontweight="bold")
    fig.tight_layout()
    path = os.path.join(output_dir, "sensitivity_tornado.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def plot_box_by_category(df, category_col, category_label, output_dir, filename):
    """Box plots of metrics grouped by a categorical variable."""
    available_metrics = [c for c in METRIC_COLS if c in df.columns]
    if category_col not in df.columns or not available_metrics:
        print(f"  Skipping box plot ({category_label}): missing columns")
        return

    categories = sorted(df[category_col].dropna().unique())
    if len(categories) < 2:
        print(f"  Skipping box plot ({category_label}): fewer than 2 categories")
        return

    n_metrics = len(available_metrics)
    fig, axes = plt.subplots(1, n_metrics, figsize=(4 * n_metrics, 5))
    if n_metrics == 1:
        axes = [axes]

    for idx, metric in enumerate(available_metrics):
        ax = axes[idx]
        data_groups = []
        labels = []
        for cat in categories:
            vals = df.loc[df[category_col] == cat, metric].dropna().values
            if len(vals) > 0:
                data_groups.append(vals)
                labels.append(str(cat))

        if data_groups:
            bp = ax.boxplot(data_groups, tick_labels=labels, patch_artist=True)
            colors = plt.cm.Set2(np.linspace(0, 1, len(data_groups)))
            for patch, color in zip(bp["boxes"], colors):
                patch.set_facecolor(color)

        ax.set_xlabel(category_label)
        ax.set_ylabel(METRIC_LABELS.get(metric, metric))
        ax.set_title(METRIC_LABELS.get(metric, metric), fontsize=10)
        ax.grid(True, alpha=0.2, axis="y")

    fig.suptitle(f"Metrics by {category_label}", fontsize=13, fontweight="bold")
    fig.tight_layout()
    path = os.path.join(output_dir, filename)
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def plot_ecdf(df, output_dir):
    """Empirical CDF of steady_state_pct by doctor behavior."""
    if "steady_state_pct" not in df.columns:
        print("  Skipping ECDF: missing steady_state_pct")
        return

    fig, ax = plt.subplots(figsize=(8, 5))

    behavior_col = "doctor_type" if "doctor_type" in df.columns else None
    if behavior_col and df[behavior_col].nunique() > 1:
        for behavior in sorted(df[behavior_col].dropna().unique()):
            vals = df.loc[df[behavior_col] == behavior, "steady_state_pct"].dropna().values
            if len(vals) > 0:
                sorted_vals = np.sort(vals)
                ecdf = np.arange(1, len(sorted_vals) + 1) / len(sorted_vals)
                ax.step(sorted_vals, ecdf, label=behavior, linewidth=2)
    else:
        vals = df["steady_state_pct"].dropna().values
        sorted_vals = np.sort(vals)
        ecdf = np.arange(1, len(sorted_vals) + 1) / len(sorted_vals)
        ax.step(sorted_vals, ecdf, linewidth=2, label="All configs")

    ax.set_xlabel("Steady-State Infected (%)", fontsize=11)
    ax.set_ylabel("Cumulative Probability", fontsize=11)
    ax.set_title("ECDF of Steady-State Infection", fontsize=13, fontweight="bold")
    ax.legend(fontsize=10)
    ax.grid(True, alpha=0.3)
    ax.set_ylim(0, 1.05)
    fig.tight_layout()
    path = os.path.join(output_dir, "ecdf_steady_state.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def plot_hexbin_interaction(df, output_dir):
    """2D hexbin of top 2 most correlated parameters vs steady_state_pct."""
    available_params = [c for c in PARAM_COLS if c in df.columns]
    if "steady_state_pct" not in df.columns or len(available_params) < 2:
        print("  Skipping hexbin: missing columns")
        return

    subset = df[available_params + ["steady_state_pct"]].dropna()
    if len(subset) < 10:
        print("  Skipping hexbin: too few samples")
        return

    # Find top 2 most correlated params
    corr = subset.corr(method="spearman")
    param_corr = corr.loc[available_params, "steady_state_pct"].abs().sort_values(ascending=False)
    top2 = param_corr.index[:2].tolist()

    fig, ax = plt.subplots(figsize=(8, 6))
    x = subset[top2[0]].values
    y = subset[top2[1]].values
    c = subset["steady_state_pct"].values

    gridsize = min(15, max(5, int(np.sqrt(len(subset)) / 2)))
    hb = ax.hexbin(x, y, C=c, gridsize=gridsize, cmap="RdYlGn_r",
                   reduce_C_function=np.mean, mincnt=1)
    fig.colorbar(hb, ax=ax, label="Mean Steady-State Infected (%)")
    ax.set_xlabel(PARAM_LABELS.get(top2[0], top2[0]), fontsize=11)
    ax.set_ylabel(PARAM_LABELS.get(top2[1], top2[1]), fontsize=11)
    ax.set_title(f"Interaction: {PARAM_LABELS.get(top2[0], top2[0])} vs "
                 f"{PARAM_LABELS.get(top2[1], top2[1])}", fontsize=12, fontweight="bold")
    fig.tight_layout()
    path = os.path.join(output_dir, f"hexbin_{top2[0]}_vs_{top2[1]}.png")
    fig.savefig(path, dpi=150)
    plt.close(fig)
    print(f"  Saved: {path}")


def print_top_configs(df, n=10):
    """Print top-N configs by composite score."""
    if "steady_state_pct" not in df.columns or "convergence_time" not in df.columns:
        print("  Cannot compute top configs: missing columns")
        return

    clean = df.dropna(subset=["steady_state_pct", "convergence_time"]).copy()
    if len(clean) == 0:
        return

    max_conv = clean["convergence_time"].max()
    if max_conv > 0:
        norm_conv = clean["convergence_time"] / max_conv
    else:
        norm_conv = 0

    max_ss = clean["steady_state_pct"].max()
    if max_ss > 0:
        norm_ss = clean["steady_state_pct"] / max_ss
    else:
        norm_ss = 0

    clean["score"] = 0.5 * (1.0 - norm_ss) + 0.5 * (1.0 - norm_conv)
    top = clean.nlargest(n, "score")

    print(f"\nTop-{n} Configs (by composite score):")
    header = (f"{'Rank':>4} | {'Score':>5} | {'p_cure':>6} | {'seek_r':>6} | "
              f"{'seek_w':>6} | {'n_doc':>5} | {'r_int':>5} | "
              f"{'Behavior':>12} | {'Formation':>9} | {'SS_%':>6} | {'Conv_T':>6}")
    print(header)
    print("-" * len(header))

    for rank, (_, row) in enumerate(top.iterrows(), 1):
        print(f"{rank:>4} | {row.get('score', 0):>5.3f} | "
              f"{row.get('p_cure', 0):>6.3f} | "
              f"{row.get('doctor_seek_radius', 0):>6.1f} | "
              f"{row.get('doctor_seek_weight', 0):>6.2f} | "
              f"{int(row.get('initial_doctor_count', 0)):>5d} | "
              f"{row.get('r_interact_doctor', 0):>5.1f} | "
              f"{row.get('doctor_type', 'N/A'):>12} | "
              f"{row.get('formation', 'N/A'):>9} | "
              f"{row.get('steady_state_pct', 0):>6.1f} | "
              f"{row.get('convergence_time', 0):>6.1f}")


def main():
    args = parse_args()
    output_dir = args.output_dir or os.path.join(args.sim_dir, "sweep-analysis")
    os.makedirs(output_dir, exist_ok=True)

    summary_path = os.path.join(args.sim_dir, "sweep_summary.csv")

    # Phase 1: Collect
    if args.skip_collect and os.path.exists(summary_path):
        print(f"Phase 1: Loading cached summary from {summary_path}")
        if HAS_PANDAS:
            df = pd.read_csv(summary_path)
        else:
            print("pandas required for analysis", file=sys.stderr)
            sys.exit(1)
    else:
        print("Phase 1: Collecting run statistics...")
        df = collect_all_runs(args.sim_dir, args.manifest)
        if df is None:
            sys.exit(1)
        df.to_csv(summary_path, index=False)
        print(f"  Summary saved: {summary_path} ({len(df)} rows)")

    print(f"\nPhase 2: Generating plots ({len(df)} samples)...")

    # Generate all plots
    plot_correlation_heatmap(df, output_dir)
    plot_pareto_front(df, output_dir)
    plot_sensitivity_tornado(df, output_dir)
    plot_box_by_category(df, "doctor_type", "Doctor Behavior", output_dir, "box_by_doctor.png")
    plot_box_by_category(df, "formation", "Boid Formation", output_dir, "box_by_formation.png")
    plot_ecdf(df, output_dir)
    plot_hexbin_interaction(df, output_dir)

    # Top configs table
    print_top_configs(df)

    print(f"\nAnalysis complete. Plots saved to {output_dir}/")


if __name__ == "__main__":
    main()
