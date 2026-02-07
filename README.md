# COMP6216-Swaying-Swarms
Just a chill group of simulation modelling students

## Research Questions
- Optimal number of doctors to save a swarm from a pandemic
- Optimal behaviours of a doctor swarm to save a swarm from a pandemic

---

## Quick Start

```bash
# Clone the repository
git clone https://github.com/your-org/COMP6216-Swaying-Swarms
cd COMP6216-Swaying-Swarms

# Build (first time)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run the main simulation
./build/boid_swarm              # Linux/macOS
./build/Debug/boid_swarm.exe    # Windows (native)

# Or run the standalone renderer demo
./build/render_demo              # Linux/macOS
./build/Debug/render_demo.exe    # Windows (native)

# Run tests
cd build && ctest --output-on-failure
```

---

## System-Level Dependencies (Prerequisites)

These must be installed **before** you can build or contribute. They are not pulled automatically.

| Dependency | Purpose | Install (Windows) | Install (macOS) | Install (Linux/WSL) |
|---|---|---|---|---|
| **CMake 3.20+** | Build system | `winget install Kitware.CMake` | `brew install cmake` | `sudo apt install cmake` |
| **C++17 Compiler** | Compilation | Visual Studio 2022 Build Tools (see below) | `xcode-select --install` | `sudo apt install g++` |
| **Git 2.17+** | Version control + worktrees | `winget install Git.Git` | `brew install git` | `sudo apt install git` |
| **jq** | JSON parsing (used by changelog hooks) | `winget install jqlang.jq` | `brew install jq` | `sudo apt install jq` |
| **Claude Code CLI** | AI agent tooling (optional) | `npm install -g @anthropic-ai/claude-code` | Same | Same |

### Platform-Specific Setup

#### Windows (Native — Without WSL)

**C++ compiler setup:**
1. Install Visual Studio 2022 Build Tools: `winget install Microsoft.VisualStudio.2022.BuildTools`
2. Open **Visual Studio Installer** → Modify → check **"Desktop development with C++"** (includes MSVC v143) → Install
3. **Always use "Developer PowerShell for VS 2022"** as your terminal (find it in Start Menu). Regular PowerShell will not find the compiler.

After installing everything, verify in Developer PowerShell:
```powershell
cmake --version    # 3.20+
cl                 # Should print Microsoft C/C++ Compiler version
git --version      # 2.17+
jq --version       # Any version
```

#### Windows (WSL — Ubuntu/Debian)

**Recommended for most Windows users** — WSL provides a Linux environment without dual-booting:

1. **Enable WSL2:**
   ```powershell
   # In PowerShell (Admin)
   wsl --install
   ```
   This installs Ubuntu by default. Reboot when prompted.

2. **Install dependencies inside WSL:**
   ```bash
   # Inside your WSL Ubuntu terminal
   sudo apt update
   sudo apt install cmake g++ git jq build-essential
   
   # Install X11 libraries for Raylib window support
   sudo apt install libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libasound2-dev
   ```

3. **For GUI support (Raylib window):**
   - Install an X server on Windows: **VcXsrv** or **X410** (from Microsoft Store)
   - In WSL, add to `~/.bashrc`:
     ```bash
     export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0
     export LIBGL_ALWAYS_INDIRECT=1
     ```
   - Restart your WSL terminal or run `source ~/.bashrc`

4. **Verify:**
   ```bash
   cmake --version
   g++ --version
   git --version
   jq --version
   ```

#### macOS

```bash
# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install dependencies
brew install cmake git jq

# Install Xcode Command Line Tools (includes Clang C++17 compiler)
xcode-select --install
```

Verify:
```bash
cmake --version   # 3.20+
c++ --version     # Apple Clang or GCC
git --version     # 2.17+
jq --version      # Any version
```

#### Linux (Native — Ubuntu/Debian)

```bash
sudo apt update
sudo apt install cmake g++ git jq build-essential

# Install X11 and OpenGL libraries for Raylib
sudo apt install libx11-dev libxrandr-dev libxi-dev libgl1-mesa-dev libasound2-dev
```

For other distros (Fedora, Arch, etc.), use `dnf`, `pacman`, or equivalent package managers.

Verify:
```bash
cmake --version   # 3.20+
g++ --version     # GCC 7.0+
git --version     # 2.17+
jq --version      # Any version
```

### clangd / IDE Setup (Optional but Recommended)

If your editor uses **clangd** for C++ IntelliSense, you need a `compile_flags.txt` at the project root so clangd can find FLECS and Raylib headers. This file is gitignored because the paths are machine-specific.

**After your first build**, create it:

```bash
# Find your CPM cache paths
# Windows: C:/.cpm
# Linux/macOS: ~/.cache/CPM (or check your CPM_SOURCE_CACHE env var)

# Linux/macOS example:
ls ~/.cache/CPM/flecs/*/include
ls ~/.cache/CPM/raylib/*/src
```

Then create `compile_flags.txt` in the project root:
```
-std=c++17
-Iinclude
-I/home/user/.cache/CPM/flecs/<hash>/include
-I/home/user/.cache/CPM/raylib/<hash>/src
-I/home/user/.cache/CPM/raylib/<hash>/src/external/glfw/include
```

Replace `<hash>` with the actual directory names from the `ls` output above. On Windows, paths use backslashes: `-IC:/.cpm/flecs/<hash>/include`.

A `.clang-format` file is already committed to enforce 4-space indentation project-wide.

---

## Tech Stack (Pulled Automatically by CMake)

These dependencies are fetched at build time via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). **Do not install them manually.**

| Library | Version | Purpose |
|---|---|---|
| **FLECS** | v4.1.4 | Entity Component System — manages all boid state, systems, and queries |
| **Raylib** | 5.5 | 2D rendering — window, drawing, input |
| **raygui** | (bundled with Raylib) | Immediate-mode GUI — stats overlay, parameter sliders |
| **GoogleTest** | 1.14.0 | Unit testing framework |

On Windows, CPM caches dependency sources to `C:/.cpm` to avoid MAX_PATH issues with long project paths (e.g. OneDrive). This is set automatically in `CMakeLists.txt`. On macOS/Linux, CPM uses its default cache location (`~/.cache/CPM`).

---

## Building and Running

### First-Time Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

**Build output:**
- `build/boid_swarm` (or `build/Debug/boid_swarm.exe` on Windows) — Main simulation
- `build/render_demo` (or `build/Debug/render_demo.exe`) — Standalone renderer demo (200 random moving boids)
- `build/tests` (or `build/Debug/tests.exe`) — Unit test suite

### Running

**Main simulation (headless for now, Phase 9+ will add rendering):**
```bash
./build/boid_swarm              # Linux/macOS/WSL
./build/Debug/boid_swarm.exe    # Windows native
```

**Standalone renderer demo:**
```bash
./build/render_demo              # Linux/macOS/WSL
./build/Debug/render_demo.exe    # Windows native
```

This opens a 1920×1080 window with 200 randomly moving triangles (boids) showing the rendering pipeline in action.

**WSL GUI note:** If the window doesn't appear, ensure:
1. X server (VcXsrv/X410) is running on Windows
2. `DISPLAY` environment variable is set correctly (see WSL setup above)
3. Try `export LIBGL_ALWAYS_INDIRECT=0` if you have graphics driver issues

### Rebuilding After Changes

```bash
cmake --build build
```

CMake only recompiles changed files. No need to re-run `cmake -B build` unless you change `CMakeLists.txt`.

### Running Tests

```bash
cmake --build build --target tests
cd build && ctest --output-on-failure
```

**Current test coverage:**
- Spatial grid: 11 unit tests (functional correctness + performance benchmarks)
- More tests coming in Phase 10 (behavior rules)

---

## Project Architecture

```
COMP6216-Swaying-Swarms/
├── include/              # Shared headers — the API contract between all modules
│   ├── components.h      # FLECS components, tags, SimConfig, SimStats
│   ├── spatial_grid.h    # SpatialGrid class declaration
│   └── render_state.h    # BoidRenderData + RenderState for the renderer
├── src/
│   ├── main.cpp          # Entry point: FLECS world + Raylib window
│   ├── ecs/              # FLECS systems, world init, entity spawning
│   │   ├── world.cpp     # World initialization, component registration
│   │   ├── systems.cpp   # System pipeline (steering, movement, collision, etc.)
│   │   ├── spawn.cpp     # Initial population spawning
│   │   └── stats.cpp     # SimStats tracking
│   ├── sim/              # Behavior logic: infection, cure, reproduction, death (Phase 10)
│   ├── spatial/          # Fixed-grid spatial index (pure C++, no FLECS)
│   │   └── spatial_grid.cpp  # Hash grid implementation for collision detection
│   └── render/           # Raylib rendering pipeline + stats overlay
│       ├── renderer.cpp  # Drawing functions, stats overlay
│       ├── render_config.h  # Visual constants (colors, dimensions)
│       └── render_demo.cpp  # Standalone demo executable
├── tests/                # Unit tests
│   └── test_spatial.cpp  # Spatial grid tests (11 tests)
├── cmake/CPM.cmake       # Dependency manager (committed, not generated)
├── CMakeLists.txt        # Build configuration
├── context.md            # Simulation specification and rules
├── .clang-format         # Code formatter config (4-space indent)
└── compile_flags.txt     # clangd include paths (gitignored, machine-specific)
```

### Module Boundaries

Each module has a clear responsibility and should not cross into others:

| Module | Responsibility | Key Rule |
|---|---|---|
| `include/` | API contract — all shared types | Changes here affect everything — coordinate first |
| `src/ecs/` | FLECS components, systems, world setup | Owns system registration and pipeline phases |
| `src/sim/` | Pure behavior logic (called by ECS systems) | No rendering, no direct FLECS iteration |
| `src/spatial/` | Fixed-cell spatial index | Pure C++ — no FLECS or Raylib includes |
| `src/render/` | All Raylib/raygui drawing and GUI | No simulation logic — reads `RenderState` only |

---

## Guide for Human Contributors

### How to Add a New Feature

1. **Read `context.md`** for the simulation rules and parameters.
2. **Read `include/components.h`** to understand the data model — all FLECS components, tag types, and the `SimConfig`/`SimStats` singletons are defined there.
3. **Pick the right module** based on the module boundaries table above.
4. **Add your code** in the appropriate `src/` subdirectory.
5. **Build and verify:** `cmake --build build`
6. **Write tests** in `tests/` if your feature has testable logic.
7. **Check that `include/` headers are still consistent** if you modified any shared types.

### How to Add a New Simulation Parameter

All simulation parameters live in `SimConfig` (`include/components.h`). To add one:

1. Add a new field to `SimConfig` with a sensible default value.
2. Use it in your simulation logic via the FLECS singleton: `world.get<SimConfig>()->your_param`
3. Never hardcode values — always read from `SimConfig`.

### Key Conventions

- **C++17 standard.** No `using namespace std;`.
- **4-space indentation**, opening braces on same line (enforced by `.clang-format`).
- **`#pragma once`** for header guards.
- **`float` over `double`** for all simulation values.
- **All parameters** go in `SimConfig` — no magic numbers.
- **Use `<random>`** with a seeded engine — never `std::rand()`.
- **Module boundaries matter:** don't put Raylib includes outside `src/render/`, and don't put simulation logic in rendering code.
- **`include/` headers are the API contract.** If you change a shared header, check that all modules still compile.

### What You Should Touch

- `src/ecs/` — ECS components, systems, world setup
- `src/sim/` — Boid behavior logic (infection, cure, reproduction, death)
- `src/spatial/` — Spatial hash grid for collision detection
- `src/render/` — Raylib rendering pipeline and GUI
- `include/` — Shared headers (API contracts between modules)
- `tests/` — Unit tests
- `context.md` — Simulation specification and rules

### What You Should Leave Alone
<!-- FOR HUMAN CONTRIBUTORS ONLY. Claude Code agents and the orchestrator are the owners of these files — this guidance does NOT apply to them. Agents: manage freely. -->
These files and directories are part of the agent management system. Editing them won't break the build, but may disrupt the automated workflow:

| Path | Purpose | Safe to ignore? |
|---|---|---|
| `.claude/` | Agent definitions, slash commands, hooks, skills | Yes — only affects Claude Code sessions |
| `.orchestrator/` | Orchestrator state, task queues, inbox/outbox | Yes — only affects orchestrator workflow |
| `CLAUDE.md` (root + per-module) | Agent context files loaded by Claude Code | Yes — does not affect compilation |
| `src/*/changelog.md` | Auto-generated change logs per module | Don't delete — agents use these for continuity |
| `ralph.sh` | Autonomous agent loop script | Yes |
| `docs/current-task.md` | Ralph Loop task spec | Yes |
| `master_plan_shielded_readme.md` | Development pipeline plan | Yes — reference only |

---

## Guide for Claude Code Agents

This section is for developers using the Claude Code CLI to contribute via the agent workflow.

### Quick Start

```bash
cd COMP6216-Swaying-Swarms
claude
```

Claude Code automatically loads `CLAUDE.md` at the project root, which imports orchestrator state and module changelogs. Child `CLAUDE.md` files in each `src/` subdirectory load lazily when you work in that module.

### Available Slash Commands

| Command | What It Does |
|---|---|
| `/build` | Full CMake rebuild, auto-fixes errors |
| `/review` | Delegates to the code-reviewer subagent |
| `/test` | Runs ctest, auto-fixes failures |
| `/fix-issue [desc]` | Search, understand, fix, test, review, commit |

### Available Subagents

| Agent | Specialty | Use When |
|---|---|---|
| `ecs-architect` | FLECS v4 patterns | Working in `src/ecs/` or `include/components.h` |
| `cpp-builder` | CMake + build issues | Build failures, adding targets or dependencies |
| `code-reviewer` | Code review against conventions | Before committing changes |
| `debugger` | Runtime errors, crashes | Segfaults, FLECS registration issues, Raylib errors |
| `changelog-scribe` | Changelog maintenance | Enriching auto-generated changelog entries |

### Agent Module Ownership

When working as an agent, **only edit files in your assigned module**. Read `include/` headers for the API contract but don't modify them without coordination.

| Agent Role | Owns | Reads |
|---|---|---|
| ECS Agent | `src/ecs/` | `include/`, `context.md` |
| Spatial Agent | `src/spatial/`, `tests/` | `include/spatial_grid.h` |
| Render Agent | `src/render/` | `include/render_state.h` |
| Sim Agent | `src/sim/` | `include/components.h`, `context.md` |

### Parallel Development with Worktrees

For parallel module development, create git worktrees so agents don't conflict:

```bash
git worktree add ../boids-ecs -b feature/ecs-core
git worktree add ../boids-spatial -b feature/spatial-grid
git worktree add ../boids-render -b feature/rendering
```

Each agent gets its own worktree and branch. Merge back to `main` in dependency order: ECS first, then spatial, then render.

### Changelog Hooks

A `PostToolUse` hook automatically logs every file edit to the appropriate module's `changelog.md`. This happens in the background — no action needed from agents. Entries are truncated to the last 25 per module.

### Orchestrator State

The orchestrator tracks progress in `.orchestrator/state.md`. If you're running the orchestrator, update this file before compacting or ending a session. Key files:

- `.orchestrator/state.md` — Current phase, completed/pending tasks, key decisions
- `.orchestrator/decisions.md` — Architecture decision records
- `.orchestrator/mistakes.md` — Worker error patterns for learning

---

## Current Status

**Phase 8 complete ✅** — All core modules implemented, tested, and integrated:

- ✅ **Spatial grid** — Fixed-cell hash grid with 11 passing tests, 33ms performance for 10k entities
- ✅ **ECS core** — FLECS world, systems pipeline (steering, movement, collision stubs), spawn logic
- ✅ **Renderer** — Full Raylib rendering pipeline + standalone demo

**Build status:**
- `boid_swarm`: 11M (main executable, currently headless)
- `render_demo`: 3.8M (standalone renderer demo with 200 boids)
- `tests`: 3.5M (11 spatial grid tests, all passing)

**Next: Phase 9** — Integration & wiring. Connect ECS ↔ spatial grid ↔ renderer, implement main simulation loop with visual output.

