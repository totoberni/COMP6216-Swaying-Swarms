#!/usr/bin/env python3
"""R₀ calibration grid search for Italy COVID-19 first wave.

Sweeps p_infect_normal × r_interact_normal, runs short sims, estimates R₀
from early exponential growth, and finds params matching Italy R₀ ≈ 3.0.

Usage:
    python3 scripts/calibrate.py --base-config configs/covid_base.ini \\
        --sim-binary build/boid_swarm --threads 12
    python3 scripts/calibrate.py --report-only  # regenerate plots from cache
"""
import argparse
import configparser
import csv
import json
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


# Italy first-wave R₀ target (pre-lockdown estimate)
R0_TARGET = 3.0
R0_RANGE = (2.5, 3.5)

# Grid search parameter ranges
P_INFECT_MIN, P_INFECT_MAX, P_INFECT_N = 0.01, 0.3, 15
R_INTERACT_MIN, R_INTERACT_MAX, R_INTERACT_N = 10, 50, 10

# Sim settings
SIM_DURATION_CALIBRATION = 60  # short sims for R₀ estimation
SIM_DURATION_VALIDATION = 300  # longer sims for curve shape matching
N_REPLICATES = 3
N_VALIDATION_RUNS = 10

# Time scale: 1 sim-second = 4 real hours
TIME_SCALE_HOURS = 4.0

# Formation params: read from canonical B*_D1.ini configs
FORMATION_CONFIG_MAP = {
    "B1": "configs/B1_D1.ini",
    "B2": "configs/B2_D1.ini",
    "B3": "configs/B3_D1.ini",
}
FORMATION_PARAMS = ["fov", "alignment_weight", "cohesion_weight", "noise_factor",
                    "separation_weight", "separation_radius", "alignment_radius",
                    "cohesion_radius", "max_speed", "max_force", "min_speed"]


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base-config", default="configs/covid_base.ini",
                        help="Base config template")
    parser.add_argument("--sim-binary", default="build/boid_swarm",
                        help="Path to compiled sim binary")
    parser.add_argument("--threads", type=int, default=12,
                        help="Number of parallel sim workers")
    parser.add_argument("--output-dir", default="sim-out/calibration",
                        help="Output directory for calibration results")
    parser.add_argument("--report-only", action="store_true",
                        help="Regenerate plots from cached results (skip sim runs)")
    parser.add_argument("--formation", default="B2",
                        help="Formation to use for calibration (default: B2/Oval)")
    return parser.parse_args()


def read_formation_params(config_path, repo_root):
    """Read formation steering params from a canonical B*_D1.ini config."""
    cp = configparser.ConfigParser()
    full_path = os.path.join(repo_root, config_path)
    cp.read(full_path)
    params = {}
    for key in FORMATION_PARAMS:
        if cp.has_option("normal_swarm", key):
            params[key] = cp.get("normal_swarm", key)
    return params


def generate_grid():
    """Generate log-spaced p_infect × linear r_interact grid."""
    p_infect_vals = np.logspace(np.log10(P_INFECT_MIN), np.log10(P_INFECT_MAX), P_INFECT_N)
    r_interact_vals = np.linspace(R_INTERACT_MIN, R_INTERACT_MAX, R_INTERACT_N)
    return p_infect_vals, r_interact_vals


def write_calibration_config(base_config_path, output_path, p_infect, r_interact,
                             duration, formation_params, run_output_dir):
    """Write a calibration config with overridden infection params."""
    cp = configparser.ConfigParser()
    cp.read(base_config_path)

    cp.set("infection", "p_infect_normal", str(p_infect))
    cp.set("interaction", "r_interact_normal", str(r_interact))
    cp.set("headless", "nogui_duration", str(float(duration)))
    cp.set("headless", "nogui", "true")
    cp.set("output", "output_dir", run_output_dir)

    # Apply formation steering params
    for key, val in formation_params.items():
        if cp.has_section("normal_swarm"):
            cp.set("normal_swarm", key, val)

    with open(output_path, "w") as f:
        cp.write(f)


def run_single_sim(args_tuple):
    """Run a single sim and return (p_infect, r_interact, replicate, R0, success)."""
    sim_binary, config_path, run_dir, p_infect, r_interact, replicate = args_tuple

    try:
        result = subprocess.run(
            [sim_binary, "-nogui", config_path],
            capture_output=True, text=True, timeout=120
        )

        # Binary writes to {output_dir}/out0/metrics.csv
        metrics_path = os.path.join(run_dir, "out0", "metrics.csv")
        if not os.path.exists(metrics_path):
            return (p_infect, r_interact, replicate, None, False)

        r0, lam, r_sq = estimate_R0(metrics_path)
        return (p_infect, r_interact, replicate, r0, True)

    except (subprocess.TimeoutExpired, FileNotFoundError, Exception) as e:
        return (p_infect, r_interact, replicate, None, False)


def estimate_R0(metrics_csv_path):
    """Estimate R₀ from early exponential growth in infection curve.

    Method: fit I(t) = I₀ × e^(λt) to pre-peak infection data.
    R₀ = 1 + λ × T_generation.

    In this small ABM (135 agents), the entire epidemic cycle runs in ~10
    seconds, so we must identify the growth phase dynamically (up to the
    first peak), not by fixed time fraction.
    """
    time_s, infected, recovered = [], [], []
    with open(metrics_csv_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            time_s.append(float(row["time_s"]))
            infected.append(float(row["infected"]))
            recovered.append(float(row.get("recovered", 0)))

    time_s = np.array(time_s)
    infected = np.array(infected)
    recovered = np.array(recovered)

    if len(time_s) < 10 or infected.max() < 2:
        return (np.nan, np.nan, 0.0)

    # Find the first peak: use smoothed infected to avoid noise
    kernel_size = min(11, len(infected) // 3)
    if kernel_size % 2 == 0:
        kernel_size += 1
    if kernel_size >= 3:
        smoothed = np.convolve(infected, np.ones(kernel_size) / kernel_size,
                               mode="same")
    else:
        smoothed = infected

    peak_idx = np.argmax(smoothed)
    if peak_idx < 3:
        # Peak at start — no growth phase to fit
        return (np.nan, np.nan, 0.0)

    # Growth phase: from first nonzero infected to 80% of peak time
    # (avoid saturation near peak which flattens the exponential)
    growth_end_idx = max(3, int(peak_idx * 0.8))
    mask = np.zeros(len(time_s), dtype=bool)
    mask[:growth_end_idx + 1] = True
    mask &= infected > 0

    if mask.sum() < 3:
        return (np.nan, np.nan, 0.0)

    t_fit = time_s[mask]
    i_fit = infected[mask]

    # Fit log(I) = log(I₀) + λt using least squares
    log_i = np.log(np.maximum(i_fit, 0.5))
    A = np.vstack([t_fit, np.ones(len(t_fit))]).T
    result = np.linalg.lstsq(A, log_i, rcond=None)
    coeffs = result[0]
    lam = coeffs[0]  # growth rate

    # R² computation
    log_pred = A @ coeffs
    ss_res = np.sum((log_i - log_pred) ** 2)
    ss_tot = np.sum((log_i - np.mean(log_i)) ** 2)
    r_sq = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0

    # Estimate generation interval: mean time an agent stays infected.
    # Look at when cumulative recoveries first reach 50% of peak infected
    # (approximates median infectious period).
    half_peak = infected.max() * 0.5
    t_gen = 3.0  # default fallback
    if recovered[-1] > 2:
        # Find time between when infection first reaches half_peak and when
        # recovered count exceeds that same threshold
        t_half_inf = None
        for i in range(len(infected)):
            if infected[i] >= half_peak and t_half_inf is None:
                t_half_inf = time_s[i]
                break
        t_half_rec = None
        for i in range(len(recovered)):
            if recovered[i] >= half_peak:
                t_half_rec = time_s[i]
                break
        if t_half_inf is not None and t_half_rec is not None:
            t_gen = max(t_half_rec - t_half_inf, 1.0)
            t_gen = min(t_gen, 15.0)

    r0 = 1.0 + lam * t_gen

    return (r0, lam, r_sq)


def run_calibration_grid(args, formation_params, repo_root):
    """Run the full calibration grid search."""
    p_infect_vals, r_interact_vals = generate_grid()

    sim_binary = os.path.join(repo_root, args.sim_binary)
    base_config = os.path.join(repo_root, args.base_config)

    if not os.path.exists(sim_binary):
        print(f"ERROR: Sim binary not found at {sim_binary}", file=sys.stderr)
        sys.exit(1)

    # Create temp directory for all calibration runs
    cal_tmp = tempfile.mkdtemp(prefix="boid_calibration_")
    print(f"Calibration temp dir: {cal_tmp}")

    # Prepare all run configurations
    tasks = []
    for i, p_inf in enumerate(p_infect_vals):
        for j, r_int in enumerate(r_interact_vals):
            for rep in range(N_REPLICATES):
                run_name = f"cal_p{i}_r{j}_rep{rep}"
                run_dir = os.path.join(cal_tmp, run_name)
                os.makedirs(run_dir, exist_ok=True)

                config_path = os.path.join(run_dir, "config.ini")
                write_calibration_config(
                    base_config, config_path, p_inf, r_int,
                    SIM_DURATION_CALIBRATION, formation_params, run_dir
                )

                tasks.append((sim_binary, config_path, run_dir,
                              p_inf, r_int, rep))

    total = len(tasks)
    print(f"Running {total} calibration sims ({P_INFECT_N}×{R_INTERACT_N}×{N_REPLICATES})...")

    # Run with process pool
    results = []
    completed = 0
    with ProcessPoolExecutor(max_workers=args.threads) as pool:
        futures = {pool.submit(run_single_sim, t): t for t in tasks}
        for future in as_completed(futures):
            result = future.result()
            results.append(result)
            completed += 1
            if completed % 50 == 0 or completed == total:
                print(f"  Progress: {completed}/{total} "
                      f"({completed/total*100:.0f}%)")

    # Aggregate results: median R₀ per (p_infect, r_interact)
    r0_grid = np.full((P_INFECT_N, R_INTERACT_N), np.nan)
    for i, p_inf in enumerate(p_infect_vals):
        for j, r_int in enumerate(r_interact_vals):
            r0_vals = [r[3] for r in results
                       if abs(r[0] - p_inf) < 1e-10
                       and abs(r[1] - r_int) < 1e-10
                       and r[3] is not None and not np.isnan(r[3])]
            if r0_vals:
                r0_grid[i, j] = np.median(r0_vals)

    # Clean up temp dir
    shutil.rmtree(cal_tmp, ignore_errors=True)

    return p_infect_vals, r_interact_vals, r0_grid


def find_best_params(p_infect_vals, r_interact_vals, r0_grid):
    """Find params closest to target R₀."""
    best_dist = float("inf")
    best_p, best_r, best_r0 = None, None, None

    for i in range(len(p_infect_vals)):
        for j in range(len(r_interact_vals)):
            r0 = r0_grid[i, j]
            if np.isnan(r0):
                continue
            dist = abs(r0 - R0_TARGET)
            if dist < best_dist:
                best_dist = dist
                best_p = p_infect_vals[i]
                best_r = r_interact_vals[j]
                best_r0 = r0

    return best_p, best_r, best_r0


def plot_calibration_surface(p_infect_vals, r_interact_vals, r0_grid, output_dir):
    """Heatmap of median R₀ across the calibration grid."""
    os.makedirs(output_dir, exist_ok=True)

    plt.rcParams.update({
        'font.size': 9, 'axes.labelsize': 9, 'axes.titlesize': 10,
        'axes.linewidth': 0.5, 'lines.linewidth': 1.0,
        'xtick.major.width': 0.5, 'ytick.major.width': 0.5,
        'figure.dpi': 300, 'savefig.dpi': 300,
        'text.color': '#373737', 'axes.edgecolor': '#373737',
    })

    fig, ax = plt.subplots(figsize=(8, 6))

    # Mask NaN values for display
    masked_grid = np.ma.masked_invalid(r0_grid)

    im = ax.pcolormesh(r_interact_vals, p_infect_vals, masked_grid,
                       cmap="RdYlGn_r", shading="nearest")
    ax.set_yscale("log")
    ax.set_xlabel("Interaction Radius (r_interact_normal)")
    ax.set_ylabel("Infection Probability (p_infect_normal)")
    ax.set_title("Calibration Surface: Median R₀")

    cbar = fig.colorbar(im, ax=ax, label="Median R₀")

    # Mark target R₀ range contour
    if not np.all(np.isnan(r0_grid)):
        try:
            cs = ax.contour(r_interact_vals, p_infect_vals, r0_grid,
                            levels=[R0_RANGE[0], R0_TARGET, R0_RANGE[1]],
                            colors=["white", "black", "white"],
                            linewidths=[0.8, 1.5, 0.8],
                            linestyles=["--", "-", "--"])
            ax.clabel(cs, fmt="R₀=%.1f", fontsize=8)
        except Exception:
            pass  # contour may fail if grid is too sparse

    # Mark best params
    best_p, best_r, best_r0 = find_best_params(p_infect_vals, r_interact_vals, r0_grid)
    if best_p is not None:
        ax.plot(best_r, best_p, "w*", markersize=15, markeredgecolor="black",
                markeredgewidth=1.0)
        ax.annotate(f"R₀={best_r0:.2f}", (best_r, best_p),
                    xytext=(10, 10), textcoords="offset points",
                    fontsize=8, color="white",
                    bbox=dict(boxstyle="round,pad=0.2", fc="black", alpha=0.7))

    fig.tight_layout()
    path = os.path.join(output_dir, "calibration_surface.png")
    fig.savefig(path, dpi=300)
    plt.close(fig)
    print(f"Saved: {path}")


def run_validation(best_p, best_r, args, formation_params, repo_root, output_dir):
    """Run longer sims with best params and compute normalized curve RMSE vs Italy."""
    sim_binary = os.path.join(repo_root, args.sim_binary)
    base_config = os.path.join(repo_root, args.base_config)

    val_tmp = tempfile.mkdtemp(prefix="boid_validation_")
    print(f"\nValidation: running {N_VALIDATION_RUNS} sims at "
          f"p_infect={best_p:.4f}, r_interact={best_r:.1f}...")

    tasks = []
    for rep in range(N_VALIDATION_RUNS):
        run_dir = os.path.join(val_tmp, f"val_{rep}")
        os.makedirs(run_dir, exist_ok=True)

        config_path = os.path.join(run_dir, "config.ini")
        write_calibration_config(
            base_config, config_path, best_p, best_r,
            SIM_DURATION_VALIDATION, formation_params, run_dir
        )
        tasks.append((sim_binary, config_path, run_dir, best_p, best_r, rep))

    with ProcessPoolExecutor(max_workers=args.threads) as pool:
        results = list(pool.map(run_single_sim, tasks))

    # Load Italy data
    italy_path = os.path.join(repo_root, "data", "covid_italy.csv")
    italy_days, italy_total = [], []
    with open(italy_path, "r") as f:
        reader = csv.DictReader(f)
        for row in reader:
            from datetime import datetime
            d = datetime.strptime(row["date"], "%Y-%m-%d")
            if not italy_days:
                base_date = d
            italy_days.append((d - base_date).days)
            italy_total.append(float(row["total_cases"]) if row["total_cases"] else 0)

    italy_days = np.array(italy_days, dtype=float)
    italy_total = np.array(italy_total)
    italy_pop = 59037472
    italy_frac = italy_total / italy_pop

    # Normalize Italy to [0,1]
    italy_norm = italy_frac / italy_frac.max() if italy_frac.max() > 0 else italy_frac
    italy_t_norm = italy_days / italy_days.max() if italy_days.max() > 0 else italy_days

    # Load sim curves and normalize
    sim_curves = []
    for rep in range(N_VALIDATION_RUNS):
        metrics_path = os.path.join(val_tmp, f"val_{rep}", "out0", "metrics.csv")
        if not os.path.exists(metrics_path):
            continue
        time_s, pct_inf = [], []
        with open(metrics_path, "r") as f:
            reader = csv.DictReader(f)
            for row in reader:
                time_s.append(float(row["time_s"]))
                pct_inf.append(float(row["pct_infected"]))
        sim_curves.append((np.array(time_s), np.array(pct_inf)))

    if not sim_curves:
        print("WARNING: No validation sims completed successfully")
        shutil.rmtree(val_tmp, ignore_errors=True)
        return np.nan, np.nan

    # Compute median sim curve at common time points
    min_len = min(len(c[0]) for c in sim_curves)
    common_time = sim_curves[0][0][:min_len]
    stacked = np.array([c[1][:min_len] for c in sim_curves])
    median_curve = np.median(stacked, axis=0)

    # Normalize sim to [0,1]
    sim_norm = median_curve / median_curve.max() if median_curve.max() > 0 else median_curve
    sim_t_norm = common_time / common_time.max() if common_time.max() > 0 else common_time

    # Interpolate Italy data onto sim's normalized time grid
    italy_interp = np.interp(sim_t_norm, italy_t_norm, italy_norm)

    # RMSE and R² on normalized curves
    rmse = np.sqrt(np.mean((sim_norm - italy_interp) ** 2))
    ss_res = np.sum((sim_norm - italy_interp) ** 2)
    ss_tot = np.sum((italy_interp - np.mean(italy_interp)) ** 2)
    r_squared = 1.0 - ss_res / ss_tot if ss_tot > 0 else 0.0

    shutil.rmtree(val_tmp, ignore_errors=True)
    return rmse, r_squared


def write_italy_config(best_p, best_r, base_config_path, formation_params,
                       repo_root, rmse, r_squared, best_r0):
    """Write calibrated italy_base.ini config."""
    cp = configparser.ConfigParser()
    cp.read(base_config_path)

    cp.set("infection", "p_infect_normal", f"{best_p:.6f}")
    cp.set("interaction", "r_interact_normal", f"{best_r:.1f}")
    cp.set("headless", "nogui_duration", "522.0")  # 87 days × 24h / 4h

    # Apply formation params
    for key, val in formation_params.items():
        if cp.has_section("normal_swarm"):
            cp.set("normal_swarm", key, val)

    output_path = os.path.join(repo_root, "configs", "italy_base.ini")
    with open(output_path, "w") as f:
        f.write(f"# Italy COVID-19 first wave calibrated config\n")
        f.write(f"# Calibrated: p_infect={best_p:.6f}, r_interact={best_r:.1f}\n")
        f.write(f"# R0={best_r0:.2f}, RMSE={rmse:.4f}, R2={r_squared:.4f}\n")
        f.write(f"# Time mapping: 1 sim-second = 4 real hours\n")
        f.write(f"# Duration: 522s = 87 days (Italy first wave Feb 21 - May 18, 2020)\n\n")
        cp.write(f)

    print(f"Saved: {output_path}")
    return output_path


def write_report(output_dir, best_p, best_r, best_r0, rmse, r_squared):
    """Write calibration report text file."""
    os.makedirs(output_dir, exist_ok=True)
    path = os.path.join(output_dir, "calibration_report.txt")
    with open(path, "w") as f:
        f.write("=== R₀ Calibration Report ===\n\n")
        f.write(f"Target: R₀ ∈ [{R0_RANGE[0]}, {R0_RANGE[1]}] (Italy first wave)\n")
        f.write(f"Grid: p_infect ∈ [{P_INFECT_MIN}, {P_INFECT_MAX}] "
                f"({P_INFECT_N} log-spaced)\n")
        f.write(f"       r_interact ∈ [{R_INTERACT_MIN}, {R_INTERACT_MAX}] "
                f"({R_INTERACT_N} linear)\n")
        f.write(f"Replicates per combo: {N_REPLICATES}\n")
        f.write(f"Calibration sim duration: {SIM_DURATION_CALIBRATION}s\n")
        f.write(f"Validation sim duration: {SIM_DURATION_VALIDATION}s\n")
        f.write(f"Validation runs: {N_VALIDATION_RUNS}\n\n")
        f.write(f"--- Best-fit Parameters ---\n")
        f.write(f"p_infect_normal = {best_p:.6f}\n")
        f.write(f"r_interact_normal = {best_r:.1f}\n")
        f.write(f"R₀ = {best_r0:.2f}\n\n")
        f.write(f"--- Validation (Normalized Curve Comparison) ---\n")
        f.write(f"RMSE = {rmse:.4f}\n")
        f.write(f"R² = {r_squared:.4f}\n")
    print(f"Saved: {path}")


def save_cache(output_dir, p_infect_vals, r_interact_vals, r0_grid,
               best_p, best_r, best_r0, rmse, r_squared):
    """Save calibration results for --report-only mode."""
    os.makedirs(output_dir, exist_ok=True)
    cache = {
        "p_infect_vals": p_infect_vals.tolist(),
        "r_interact_vals": r_interact_vals.tolist(),
        "r0_grid": r0_grid.tolist(),
        "best_p": best_p,
        "best_r": best_r,
        "best_r0": best_r0,
        "rmse": float(rmse),
        "r_squared": float(r_squared),
    }
    path = os.path.join(output_dir, "calibration_cache.json")
    with open(path, "w") as f:
        json.dump(cache, f, indent=2)


def load_cache(output_dir):
    """Load cached calibration results."""
    path = os.path.join(output_dir, "calibration_cache.json")
    if not os.path.exists(path):
        return None
    with open(path, "r") as f:
        cache = json.load(f)
    cache["p_infect_vals"] = np.array(cache["p_infect_vals"])
    cache["r_interact_vals"] = np.array(cache["r_interact_vals"])
    cache["r0_grid"] = np.array(cache["r0_grid"])
    return cache


def main():
    args = parse_args()

    # Determine repo root (parent of scripts/)
    repo_root = str(Path(__file__).resolve().parent.parent)
    output_dir = os.path.join(repo_root, args.output_dir)

    # Read formation params from canonical config
    formation_config = FORMATION_CONFIG_MAP.get(args.formation)
    if not formation_config:
        print(f"Unknown formation: {args.formation}", file=sys.stderr)
        sys.exit(1)
    formation_params = read_formation_params(formation_config, repo_root)
    print(f"Using formation {args.formation} params from {formation_config}")

    if args.report_only:
        cache = load_cache(output_dir)
        if cache is None:
            print("No cached results found. Run without --report-only first.",
                  file=sys.stderr)
            sys.exit(1)
        print("Regenerating plots from cached results...")
        plot_calibration_surface(cache["p_infect_vals"], cache["r_interact_vals"],
                                 cache["r0_grid"], output_dir)
        write_report(output_dir, cache["best_p"], cache["best_r"],
                     cache["best_r0"], cache["rmse"], cache["r_squared"])
        print(f"\nCalibrated params: p_infect={cache['best_p']:.6f}, "
              f"r_interact={cache['best_r']:.1f}, "
              f"R₀={cache['best_r0']:.2f}, RMSE={cache['rmse']:.4f}")
        return

    # Run calibration grid search
    p_vals, r_vals, r0_grid = run_calibration_grid(args, formation_params, repo_root)

    # Find best params
    best_p, best_r, best_r0 = find_best_params(p_vals, r_vals, r0_grid)
    if best_p is None:
        print("ERROR: No valid R₀ estimates from calibration grid", file=sys.stderr)
        sys.exit(1)

    print(f"\nBest-fit: p_infect={best_p:.6f}, r_interact={best_r:.1f}, R₀={best_r0:.2f}")

    # Run validation
    rmse, r_squared = run_validation(best_p, best_r, args, formation_params,
                                     repo_root, output_dir)

    # Generate outputs
    plot_calibration_surface(p_vals, r_vals, r0_grid, output_dir)
    write_report(output_dir, best_p, best_r, best_r0, rmse, r_squared)
    save_cache(output_dir, p_vals, r_vals, r0_grid, best_p, best_r, best_r0,
               rmse, r_squared)

    # Write calibrated config
    base_config = os.path.join(repo_root, args.base_config)
    write_italy_config(best_p, best_r, base_config, formation_params,
                       repo_root, rmse, r_squared, best_r0)

    print(f"\nCalibrated params: p_infect={best_p:.6f}, r_interact={best_r:.1f}, "
          f"R₀={best_r0:.2f}, RMSE={rmse:.4f}")


if __name__ == "__main__":
    main()
