#!/usr/bin/env python3
"""Generate Latin Hypercube Sampled configs for Monte Carlo parameter sweep.

Uses covid_base.ini as template. Locks infection parameters.
Varies doctor/intervention and boid-formation parameters.

Usage: python3 scripts/generate_sweep.py [--n-samples 500] [--output-dir configs/sweep]
       [--base-config configs/covid_base.ini] [--duration 120]
"""
import argparse
import configparser
import json
import math
import os
import sys
from pathlib import Path

import numpy as np

# Continuous sweep parameters
SWEEP_PARAMS = {
    "p_cure":               {"min": 0.1, "max": 1.0, "type": "uniform"},
    "doctor_seek_radius":   {"min": 0.0, "max": 500.0, "type": "uniform"},
    "doctor_seek_weight":   {"min": 0.5, "max": 10.0, "type": "log-uniform"},
    "initial_doctor_count": {"min": 3, "max": 40, "type": "int"},
    "r_interact_doctor":    {"min": 20.0, "max": 120.0, "type": "uniform"},
}

# Categorical parameters
CATEGORICAL_PARAMS = {
    "doctor_behavior": ["normal", "nearest", "centroid"],
    "boid_formation":  ["B1", "B2", "B3"],
}

# Formation steering params (from existing experiment configs)
FORMATION_PARAMS = {
    "B1": {
        "fov": 1.05,
        "alignment_weight": 3.0,
        "cohesion_weight": 3.0,
        "noise_factor": 0.0,
    },
    "B2": {
        "fov": 3.14,
        "alignment_weight": 3.5,
        "cohesion_weight": 2.5,
        "noise_factor": 0.0,
    },
    "B3": {
        "fov": 3.14,
        "alignment_weight": 3.5,
        "cohesion_weight": 2.5,
        "noise_factor": 80.0,
    },
}


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--n-samples", type=int, default=500,
                        help="Number of LHS samples (default: 500)")
    parser.add_argument("--output-dir", default="configs/sweep",
                        help="Output directory for generated configs")
    parser.add_argument("--base-config", default="configs/covid_base.ini",
                        help="Base config template (infection params locked)")
    parser.add_argument("--duration", type=float, default=120.0,
                        help="Simulation duration override (default: 120s)")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed for reproducibility")
    return parser.parse_args()


def latin_hypercube_sample(n_samples, n_dims, rng):
    """Generate LHS samples in [0,1]^d."""
    try:
        from scipy.stats.qmc import LatinHypercube
        sampler = LatinHypercube(d=n_dims, seed=rng)
        return sampler.random(n=n_samples)
    except ImportError:
        # Fallback: stratified random sampling
        samples = np.zeros((n_samples, n_dims))
        for dim in range(n_dims):
            perm = rng.permutation(n_samples)
            for i in range(n_samples):
                samples[perm[i], dim] = (i + rng.random()) / n_samples
        return samples


def map_sample_to_params(sample_row, param_names, rng):
    """Map a [0,1]^d sample to actual parameter values."""
    values = {}
    for i, name in enumerate(param_names):
        spec = SWEEP_PARAMS[name]
        u = sample_row[i]

        if spec["type"] == "uniform":
            values[name] = spec["min"] + u * (spec["max"] - spec["min"])
        elif spec["type"] == "log-uniform":
            log_min = math.log(spec["min"])
            log_max = math.log(spec["max"])
            values[name] = math.exp(log_min + u * (log_max - log_min))
        elif spec["type"] == "int":
            values[name] = int(round(spec["min"] + u * (spec["max"] - spec["min"])))

    return values


def map_categorical(sample_row_extra, rng):
    """Map uniform [0,1] samples to categorical values."""
    cats = {}
    idx = 0
    for name, options in CATEGORICAL_PARAMS.items():
        u = sample_row_extra[idx]
        cat_idx = min(int(u * len(options)), len(options) - 1)
        cats[name] = options[cat_idx]
        idx += 1
    return cats


def read_base_config(path):
    """Read base config file as raw text (preserving comments)."""
    with open(path, "r") as f:
        return f.read()


def write_sweep_config(base_text, params, cats, duration, output_path):
    """Write a sweep config by modifying the base template."""
    # Parse the base config to get structure
    cp = configparser.ConfigParser()
    cp.read_string(base_text)

    # Apply continuous parameter overrides
    if "p_cure" in params:
        cp.set("cure", "p_cure", f"{params['p_cure']:.4f}")
    if "r_interact_doctor" in params:
        cp.set("interaction", "r_interact_doctor", f"{params['r_interact_doctor']:.1f}")
    if "initial_doctor_count" in params:
        cp.set("population", "initial_doctor_count", str(params["initial_doctor_count"]))

    # Doctor behavior + seeking params
    behavior = cats.get("doctor_behavior", "normal")
    cp.set("doctor_swarm", "behavior", behavior)
    if "doctor_seek_radius" in params:
        cp.set("doctor_swarm", "doctor_seek_radius", f"{params['doctor_seek_radius']:.1f}")
    if "doctor_seek_weight" in params:
        cp.set("doctor_swarm", "doctor_seek_weight", f"{params['doctor_seek_weight']:.2f}")

    # Boid formation steering params
    formation = cats.get("boid_formation", "B2")
    fp = FORMATION_PARAMS[formation]
    for key, val in fp.items():
        cp.set("normal_swarm", key, f"{val}")

    # Override duration
    cp.set("headless", "nogui_duration", f"{duration:.1f}")
    cp.set("headless", "nogui", "true")

    # Ensure CSV sampling is set
    if not cp.has_option("headless", "csv_sample_interval"):
        cp.set("headless", "csv_sample_interval", "0.5")

    # Write config
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w") as f:
        f.write(f"# Sweep config — {os.path.basename(output_path)}\n")
        f.write(f"# Formation: {formation}, Doctor: {behavior}\n")
        f.write(f"# p_cure={params.get('p_cure', '?'):.4f}, "
                f"seek_r={params.get('doctor_seek_radius', '?'):.1f}, "
                f"seek_w={params.get('doctor_seek_weight', '?'):.2f}, "
                f"n_doc={params.get('initial_doctor_count', '?')}, "
                f"r_int_doc={params.get('r_interact_doctor', '?'):.1f}\n\n")
        cp.write(f)


def main():
    args = parse_args()
    rng = np.random.default_rng(args.seed)

    if not os.path.exists(args.base_config):
        print(f"Base config not found: {args.base_config}", file=sys.stderr)
        sys.exit(1)

    base_text = read_base_config(args.base_config)
    param_names = list(SWEEP_PARAMS.keys())
    n_continuous = len(param_names)
    n_categorical = len(CATEGORICAL_PARAMS)
    n_total_dims = n_continuous + n_categorical

    print(f"Generating {args.n_samples} LHS samples...")
    print(f"  Continuous params ({n_continuous}): {', '.join(param_names)}")
    print(f"  Categorical params ({n_categorical}): {', '.join(CATEGORICAL_PARAMS.keys())}")
    print(f"  Base config: {args.base_config}")
    print(f"  Duration: {args.duration}s")

    # Generate LHS samples for all dimensions
    samples = latin_hypercube_sample(args.n_samples, n_total_dims, rng)

    os.makedirs(args.output_dir, exist_ok=True)

    manifest = {
        "n_samples": args.n_samples,
        "base_config": args.base_config,
        "duration": args.duration,
        "seed": args.seed,
        "params": {},
    }

    for i in range(args.n_samples):
        config_name = f"sweep_{i:03d}.ini"
        config_path = os.path.join(args.output_dir, config_name)

        # Map continuous params
        continuous = map_sample_to_params(samples[i, :n_continuous], param_names, rng)
        # Map categorical params
        categorical = map_categorical(samples[i, n_continuous:], rng)

        write_sweep_config(base_text, continuous, categorical, args.duration, config_path)

        # Record in manifest
        entry = {**continuous, **categorical}
        # Ensure numeric types for JSON
        for k, v in entry.items():
            if isinstance(v, (np.integer, np.int64)):
                entry[k] = int(v)
            elif isinstance(v, (np.floating, np.float64)):
                entry[k] = float(v)
        manifest["params"][config_name] = entry

    # Write manifest
    manifest_path = os.path.join(args.output_dir, "manifest.json")
    with open(manifest_path, "w") as f:
        json.dump(manifest, f, indent=2)

    print(f"\nGenerated {args.n_samples} configs in {args.output_dir}/")
    print(f"Manifest: {manifest_path}")

    # Print summary table of first few samples
    print(f"\nSample preview (first 5):")
    header = f"{'Config':<16} {'p_cure':>7} {'seek_r':>7} {'seek_w':>7} {'n_doc':>6} {'r_int':>6} {'Behavior':>10} {'Formation':>10}"
    print(header)
    print("-" * len(header))
    for i in range(min(5, args.n_samples)):
        name = f"sweep_{i:03d}.ini"
        p = manifest["params"][name]
        print(f"{name:<16} {p['p_cure']:>7.3f} {p['doctor_seek_radius']:>7.1f} "
              f"{p['doctor_seek_weight']:>7.2f} {p['initial_doctor_count']:>6d} "
              f"{p['r_interact_doctor']:>6.1f} {p['doctor_behavior']:>10} "
              f"{p['boid_formation']:>10}")


if __name__ == "__main__":
    main()
