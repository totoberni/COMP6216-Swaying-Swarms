# COMP6216-Swaying-Swarms

2D pandemic boid simulation: three swarms (Normal Boids, Doctor Boids, Antivax Boids) with infection, cure, reproduction, death, and promotion mechanics. Built with C++17, FLECS ECS, and Raylib.

## Research Questions
- Optimal number of doctors to save a swarm from a pandemic
- Optimal behaviours of a doctor swarm to save a swarm from a pandemic
- How does an antivax subpopulation affect pandemic outcomes?

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
| Run with config | `./build/boid_swarm config.ini` | `.\build\Debug\boid_swarm.exe config.ini` |
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

The simulation reads an optional INI config file. A fully documented default is included at `config.ini`.

```bash
./build/boid_swarm                        # uses config.ini if present, else defaults
./build/boid_swarm experiment.ini         # custom config
```

- Partial configs are valid — missing keys keep built-in defaults
- Unknown keys warn to stderr but don't crash
- Sliders override config values at runtime; the file sets starting values
- See `config.ini` for all ~40 parameters with comments

---

## In-Simulation Controls

| Control | Description |
|---|---|
| **Pause / Resume** button | Toggles simulation |
| **Reset** button | Destroys all boids, re-spawns initial population |
| **Sliders** | p_infect_normal, p_cure, r_interact_normal, r_interact_doctor |
| **Population graph** | Real-time line chart (green=normal, blue=doctor, orange=antivax, 500-frame window) |
| **Cohesion graph** | Average distance to centroid over time |
| **Alignment graph** | Average alignment angle over time |
| **Separation graph** | RMS pairwise separation over time (cyan) |
| **Stats panel** | Average position, cohesion, alignment angle, RMS separation |

---

## Project Architecture

```
include/           Shared headers (API contract between modules)
src/main.cpp       Entry point: FLECS world + Raylib window + main loop
src/ecs/           FLECS systems, world init, spawning, stats
src/sim/           Behavior logic: infection, cure, reproduction, death, aging, promotion, config loader
src/spatial/       Fixed-cell spatial hash grid (pure C++, no FLECS/Raylib)
src/render/        Raylib rendering, raygui stats overlay, sliders, population graph
tests/             40 unit tests (15 spatial grid + 13 config loader + 2 cure contract + 10 antivax)
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

All core simulation features from `context.md` are implemented. The project is in the refinement phase.

| Feature | Status |
|---|---|
| Flocking (Reynolds steering, vectorized with raymath) | ✅ |
| Infection, death, cure, reproduction, aging, promotion | ✅ |
| SIRS disease model (death/recovery, time-decaying immunity, cross-swarm infection) | ✅ |
| Three swarms (Normal, Doctor, Antivax with doctor avoidance) | ✅ |
| Infected debuffs, sex system | ✅ |
| Interactive sliders, pause/reset, population graph | ✅ |
| Stats overlay (cohesion, alignment, RMS separation graphs) | ✅ |
| FOV-based neighbor detection with configurable angle | ✅ |
| INI config file loader | ✅ |
| Obstacles (future extension) | Planned |

**Runtime:** 60 FPS, 27/30 tests passing (3 pre-existing config loader / test-data failures).

---

## License

TODO

## Contributors

- COMP6216 Research Group
- Claude Code orchestrator + specialized agents
