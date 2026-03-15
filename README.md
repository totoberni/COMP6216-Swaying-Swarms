# COMP6216-Swaying-Swarms

2D pandemic boid simulation with SIR disease model. Two swarms (Normal Boids, Doctor Boids) with configurable flocking behaviors, infection, and cure mechanics. Includes a 3×3 experimental matrix (3 swarm shapes × 3 doctor strategies) with headless batch execution and automated analysis. Built with C++17, FLECS ECS, and Raylib.

## Research Question

> What Doctor behavior most effectively contains a pandemic, and how does this depend on the swarm's flocking characteristics?

---

## Quick Start

```bash
git clone https://github.com/your-org/COMP6216-Swaying-Swarms
cd COMP6216-Swaying-Swarms
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

| Action | Linux / macOS / WSL | Windows (Dev PowerShell) |
|---|---|---|
| Run simulation | `./build/boid_swarm` | `.\build\Debug\boid_swarm.exe` |
| Run with config | `./build/boid_swarm configs/B2_D2.ini` | `.\build\Debug\boid_swarm.exe configs\B2_D2.ini` |
| Run headless | `./build/boid_swarm -nogui configs/B2_D2.ini` | `.\build\Debug\boid_swarm.exe -nogui configs\B2_D2.ini` |
| Run headless (force CPU) | `./build/boid_swarm -nogui --cpu configs/B2_D2.ini` | `.\build\Debug\boid_swarm.exe -nogui --cpu configs\B2_D2.ini` |
| Run tests | `cd build && ctest --output-on-failure` | `cd build && ctest --output-on-failure -C Debug` |

> **Windows:** Always use "Developer PowerShell for VS 2022". The `-C Debug` flag is required for ctest on MSVC multi-config builds.

---

## Prerequisites

| Dependency | Windows (PowerShell) | macOS | Linux / WSL |
|---|---|---|---|
| **CMake 3.20+** | `winget install Kitware.CMake` | `brew install cmake` | `sudo apt install cmake` |
| **C++17 compiler** | VS 2022 Build Tools (Desktop C++ workload) | `xcode-select --install` | `sudo apt install g++ build-essential` |
| **Git 2.17+** | `winget install Git.Git` | `brew install git` | `sudo apt install git` |

**Linux/WSL only** — Raylib needs X11/GL headers:
```bash
sudo apt install libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

**WSL GUI** — Install an X server on Windows (VcXsrv or X410), then in WSL:
```bash
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
```

### Auto-Fetched Dependencies (via CPM.cmake)

| Library | Version | Purpose |
|---|---|---|
| FLECS | v4.1.4 | Entity Component System |
| Raylib | 5.5 | 2D rendering + input |
| raygui | 4.0 | Immediate-mode GUI (stats panel, sliders) |
| GoogleTest | 1.14.0 | Unit testing |

---

## Configuration

The simulation reads an optional INI config file (positional argument). `config.ini` is the default for interactive GUI use; `configs/` contains the 9 experiment presets.

```bash
./build/boid_swarm                        # uses config.ini if present, else defaults
./build/boid_swarm configs/B2_D1.ini      # custom config (positional arg)
```

- `[normal_swarm]` and `[doctor_swarm]` sections accept independent steering params (speed, force, radii, fov, weights)
- `[world]` section: set `wall_bounce = false` for toroidal wrapping
- Partial configs are valid — missing keys keep built-in defaults
- Unknown keys warn to stderr but don't crash
- Sliders override config values at runtime; the file sets starting values
- See `include/components.h` (`SimConfig`/`SwarmParams` structs) for the full parameter reference

---

## Running Simulations

### Modes

| Mode | Command | Description |
|---|---|---|
| GUI (interactive) | `./build/boid_swarm configs/B2_D1.ini` | Real-time visualization with sliders and graphs |
| Headless (CPU) | `./build/boid_swarm -nogui configs/B2_D1.ini` | Batch mode, CSV output to `sim-out/outN/` |
| Headless (GPU) | `./build/boid_swarm -nogui configs/milan.ini` | GPU-accelerated, up to 1.4M boids (requires CUDA build) |
| Headless (force CPU) | `./build/boid_swarm -nogui --cpu configs/B2_D1.ini` | Force CPU/FLECS path even with CUDA build |
| Monte Carlo sweep | `./build/boid_swarm --sweep --n-samples 600 configs/covid_base.ini` | LHS parameter sweep with thread pool |

Each headless run writes to `sim-out/outN/` (auto-incrementing):
- `metrics.csv` — per-frame SIR + swarm metrics (15 columns)
- `config_used.ini` — config snapshot for reproducibility
- `summary.txt` — peak infection, final counts

### GPU Acceleration (CUDA)

Optional. Requires an NVIDIA GPU + CUDA toolkit. The default build (`USE_CUDA=OFF`) works without CUDA.

```bash
cmake -B build -DUSE_CUDA=ON && cmake --build build
```

**Teammates without NVIDIA GPUs**: build without `-DUSE_CUDA=ON` (the default), or pass `--cpu` to force the CPU path. GUI mode and `--sweep` always use CPU/FLECS regardless of build flags.

### Sweep CLI

```bash
./build/boid_swarm --sweep [options] config.ini
  --n-samples N    LHS samples (default: 600)
  --threads T      Worker threads (default: cores - 2)
  --seed S         RNG seed (default: 42)
  --duration D     Sim duration in seconds
  --output-dir DIR Output directory (default: sim-out)
```

---

## Experiments & Analysis

### 3x3 Experiment Matrix

|  | D1 (Normal) | D2 (Seek Nearest) | D3 (Seek Centroid) |
|---|---|---|---|
| **B1 (Narrow FOV)** | B1_D1.ini | B1_D2.ini | B1_D3.ini |
| **B2 (Wide FOV)** | B2_D1.ini | B2_D2.ini | B2_D3.ini |
| **B3 (Chaotic)** | B3_D1.ini | B3_D2.ini | B3_D3.ini |

### Full Experiment Sequence

```bash
# 1. Run all 9 experiments (single trial, with analysis)
./scripts/run_experiments.sh --clean --analyze

# 2. Multi-trial with parallelism
./scripts/run_experiments.sh --clean --repeat 3 --parallel 4 --analyze

# 3. Monte Carlo sweep (600 LHS samples, 12 threads)
./build/boid_swarm --sweep --n-samples 600 --threads 12 --duration 300 \
  --output-dir sim-out configs/covid_base.ini

# 4. Sweep analysis
.venv/bin/python3 scripts/analyze_sweep.py --sim-dir sim-out --manifest sim-out/configs/manifest.json

# 5. Scale validation (compare dynamics across boid counts)
.venv/bin/python3 scripts/validate_scale.py \
  --runs sim-out/out0:130 sim-out/out1:100k sim-out/out2:1.4M \
  --output sim-out/validation
```

### Analysis Scripts

| Script | Input | Output |
|---|---|---|
| `scripts/compare_results.py` | `sim-out/outN/` dirs | `sim-out/analysis/` — infection grids, heatmaps, growth rates |
| `scripts/analyze_sweep.py` | sweep manifest + CSVs | `sim-out/sweep-analysis/` — correlation, Pareto, tornado, violin plots |
| `scripts/validate_scale.py` | 2+ run dirs with labels | `sim-out/validation/` — cross-scale SIR comparison |
| `scripts/compare_covid.py` | sim output + COVID data | COVID real-data overlay plots |

All scripts require `.venv/bin/python3` with matplotlib, numpy, pandas, scipy.

---

## In-Simulation Controls

| Control | Description |
|---|---|
| **Pause / Resume** button | Toggles simulation (also SPACE key) |
| **Reset (R)** button | Destroys all boids, re-spawns initial population |
| **Hide (H)** button | Hides the stats overlay (also H key) |
| **Graph selector** dropdown | None, Population, Cohesion, Alignment, Separation, Infected, % Infected, Growth Rate, Recovered |
| **Controls** dropdown | Infection, Cure, Interaction, Movement, Debuffs — each shows relevant sliders |
| **Export CSV** button | Visible when paused; exports history buffers to `sim-out/outN/metrics.csv` |
| **Stats panel** | Swarm populations, average cohesion, alignment, RMS separation |

---

## Project Architecture

```
include/           Shared headers (API contract between modules)
src/main.cpp       Entry point: FLECS world + Raylib window + main loop
src/ecs/           FLECS systems, world init, spawning, stats
src/sim/           Behavior logic: infection, cure, config loader, headless output
src/spatial/       Fixed-cell spatial hash grid (pure C++, no FLECS/Raylib)
src/render/        Raylib rendering, raygui stats overlay, sliders, population graph
configs/           9 experiment config files (3×3 matrix)
scripts/           Experiment runner + analysis scripts
tests/             Unit tests (spatial grid, config loader, cure contract)
config.ini         Default simulation parameters
```

### Module Boundaries

| Module | Key Rule |
|---|---|
| `src/ecs/` | Owns FLECS system registration and pipeline phases |
| `src/sim/` | Pure logic — no rendering, no direct FLECS iteration |
| `src/spatial/` | Pure C++ — uses raymath for vector ops, no FLECS |
| `src/render/` | No simulation logic — reads `RenderState` only |

---

## Contributing

### Adding a Feature
1. Read `.orchestrator/context.md` for simulation rules
2. Read `include/components.h` for the data model (SimConfig, SimStats, all components/tags)
3. Add code in the appropriate `src/` module (respect module boundaries)
4. Build: `cmake --build build`
5. Write tests in `tests/` if applicable

### Adding a Simulation Parameter
1. Add field to `SimConfig` in `include/components.h` (with default initializer)
2. Add `key = value` to `config.ini`
3. Add parsing in `src/sim/config_loader.cpp`
4. Add test in `tests/test_config_loader.cpp`
5. Use via `world.get<SimConfig>()->your_param` — never hardcode values

### Conventions
- C++17, no `using namespace std;`, 4-space indent, braces on same line
- `float` over `double` for sim values, `#pragma once` for headers
- `<random>` with seeded engine, never `std::rand()`
- No Raylib outside `src/render/`, no sim logic in render code

### Agent-Managed Files (leave alone)
`.claude/`, `.orchestrator/`, `CLAUDE.md` files, `src/*/changelog.md`, `docs/` — these support the AI agent workflow and don't affect compilation.

---

## Current Status

| Feature | Status |
|---|---|
| Reynolds flocking (separation, alignment, cohesion with FOV) | Done |
| SIR disease model (infection, cure, permanent immunity) | Done |
| Two swarms (Normal, Doctor) with configurable behaviors | Done |
| Doctor strategies: D1 normal, D2 seek nearest, D3 seek centroid | Done |
| Swarm formations: B1 narrow-FOV line, B2 wide-FOV oval, B3 chaotic (noise injection) | Done |
| Headless mode with CLI flags + incremental CSV output | Done |
| 9 experiment configs + batch runner + analysis script | Done |
| Interactive GUI: sliders, graphs, stats panel | Done |
| INI config file loader | Done |
| CUDA GPU acceleration (headless, up to 1.4M boids) | Done |

---

## References

1. Reynolds, C.W. (1987). *Flocks, Herds, and Schools: A Distributed Behavioral Model*. Computer Graphics, 21(4), 25-34.
2. Kermack, W.O. & McKendrick, A.G. (1927). *A Contribution to the Mathematical Theory of Epidemics*. Proc. Royal Society A, 115(772), 700-721.
3. Tracy, M., Cerdá, M. & Keyes, K.M. (2018). *Agent-Based Modeling in Public Health: Current Applications and Future Directions*. Annual Review of Public Health, 39, 77-94.
4. McKay, M.D., Beckman, R.J. & Conover, W.J. (1979). *A Comparison of Three Methods for Selecting Values of Input Variables in the Analysis of Output from a Computer Code*. Technometrics, 21(2), 239-245.
5. Teschner, M. et al. (2003). *Optimized Spatial Hashing for Collision Detection of Deformable Objects*. VMV, 47-54.
6. Allen, M.P. & Tildesley, D.J. (2017). *Computer Simulation of Liquids* (2nd ed.). Oxford University Press. (Periodic boundary conditions)
7. Green, S. (2013). *Particle Simulation using CUDA*. NVIDIA GPU Computing SDK.
8. Nystrom, R. (2014). *Game Programming Patterns*. Genever Benning. (ECS architecture)
9. Richmond, P. et al. (2023). *FLAMEGPU2: A framework for agent-based simulation on GPU architectures*. Software: Practice and Experience, 53(8), 1659-1680.

---

## License

TODO

## Contributors

- COMP6216 Research Group
- Claude Code orchestrator + specialized agents
