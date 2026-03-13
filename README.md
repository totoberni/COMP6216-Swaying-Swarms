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

## Headless Mode

Run simulations without a GUI window for batch experiments:

```bash
./build/boid_swarm -nogui configs/B2_D2.ini
```

Output is written to `sim-out/outN/` (auto-incrementing). Each run produces:
- `metrics.csv` — per-frame infection/swarm metrics (15 columns)
- `config_used.ini` — snapshot of the config for reproducibility
- `summary.txt` — peak infection, final counts, swarm metrics

Set `nogui_duration` in the config file to control simulation length (seconds).

---

## Running Experiments

The `configs/` directory contains 9 experiment configs in a 3×3 matrix:

|  | D1 (Normal) | D2 (Seek Nearest) | D3 (Seek Centroid) |
|---|---|---|---|
| **B1 (Narrow FOV)** | B1_D1.ini | B1_D2.ini | B1_D3.ini |
| **B2 (Wide FOV)** | B2_D1.ini | B2_D2.ini | B2_D3.ini |
| **B3 (Chaotic)** | B3_D1.ini | B3_D2.ini | B3_D3.ini |

Run all 9 experiments:
```bash
./scripts/run_experiments.sh          # uses build/ by default
./scripts/run_experiments.sh mybuild  # custom build dir
```

---

## Analysis

Generate comparison plots from experiment results:

```bash
python3 scripts/compare_results.py --sim-dir sim-out
```

Produces `sim-out/analysis/` with:
- 3×3 infection curve grid, overlay plot, recovery curves
- Heatmaps: peak infection count, time to eradication

Requires `matplotlib` (`pip3 install matplotlib`).

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

---

## License

TODO

## Contributors

- COMP6216 Research Group
- Claude Code orchestrator + specialized agents
