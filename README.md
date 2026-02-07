# COMP6216-Swaying-Swarms
Just a chill group of simulation modelling students

## Research Questions
- Optimal number of doctors to save a swarm from a pandemic
- Optimal behaviours of a doctor swarm to save a swarm from a pandemic

---

## System-Level Dependencies (Prerequisites)

These must be installed **before** you can build or contribute. They are not pulled automatically.

| Dependency | Purpose | Install (Windows) | Install (macOS) | Install (Linux) |
|---|---|---|---|---|
| **CMake 3.20+** | Build system | `winget install Kitware.CMake --source winget` | `brew install cmake` | `sudo apt install cmake` |
| **C++17 Compiler** | Compilation | Visual Studio 2022 Build Tools (see below) | `xcode-select --install` | `sudo apt install g++` |
| **Git 2.17+** | Version control + worktrees | `winget install Git.Git --source winget` | `brew install git` | `sudo apt install git` |
| **jq** | JSON parsing (used by changelog hooks) | `winget install jqlang.jq --source winget` | `brew install jq` | `sudo apt install jq` |
| **Claude Code CLI** | AI agent tooling (optional) | `npm install -g @anthropic-ai/claude-code` | Same | Same |

### Windows-Specific Notes

**C++ compiler setup:**
1. Install Visual Studio 2022 Build Tools: `winget install Microsoft.VisualStudio.2022.BuildTools --source winget`
2. Open **Visual Studio Installer** -> Modify -> check **"Desktop development with C++"** (includes MSVC v143) -> Install.
3. **Always use "Developer PowerShell for VS 2022"** as your terminal (find it in Start Menu). Regular PowerShell will not find the compiler.

After installing everything, verify in Developer PowerShell:
```powershell
cmake --version    # 3.20+
cl                 # Should print Microsoft C/C++ Compiler version
git --version      # 2.17+
jq --version       # Any version
```

### clangd / IDE Setup (Optional but Recommended)

If your editor uses **clangd** for C++ IntelliSense, you need a `compile_flags.txt` at the project root so clangd can find FLECS and Raylib headers. This file is gitignored because the paths are machine-specific.

**After your first build**, create it:

```bash
# Find your CPM cache paths (default: C:/.cpm on Windows)
ls C:/.cpm/flecs/*/include    # e.g. C:/.cpm/flecs/d5ad/include
ls C:/.cpm/raylib/*/src       # e.g. C:/.cpm/raylib/cebd/src
```

Then create `compile_flags.txt` in the project root:
```
-std=c++17
-Iinclude
-IC:/.cpm/flecs/<hash>/include
-IC:/.cpm/raylib/<hash>/src
-IC:/.cpm/raylib/<hash>/src/external/glfw/include
```

Replace `<hash>` with the actual directory names from the `ls` output above.

A `.clang-format` file is already committed to enforce 4-space indentation project-wide.

---

## Tech Stack (Pulled Automatically by CMake)

These dependencies are fetched at build time via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). **Do not install them manually.**

| Library | Version | Purpose |
|---|---|---|
| **FLECS** | v4.1.4 | Entity Component System — manages all boid state, systems, and queries |
| **Raylib** | 5.5 | 2D rendering — window, drawing, input |
| **raygui** | (bundled with Raylib) | Immediate-mode GUI — stats overlay, parameter sliders |

On Windows, CPM caches dependency sources to `C:/.cpm` to avoid MAX_PATH issues with long project paths (e.g. OneDrive). This is set automatically in `CMakeLists.txt`. On macOS/Linux, CPM uses its default cache location.

---

## Building and Running

### First-Time Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The executable is at:
- **Windows:** `build/Debug/boid_swarm.exe`
- **macOS/Linux:** `build/boid_swarm`

### Running

```bash
# Windows (from project root)
./build/Debug/boid_swarm.exe

# macOS/Linux
./build/boid_swarm
```

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
│   ├── sim/              # Behavior logic: infection, cure, reproduction, death
│   ├── spatial/          # Fixed-grid spatial index (pure C++, no FLECS)
│   └── render/           # Raylib rendering pipeline + stats overlay
├── tests/                # Unit tests
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
- `.orchestrator/task-queue.md` — What to work on next
- `.orchestrator/active-tasks.md` — Running worker sessions
- `.orchestrator/decisions.md` — Architecture decision records

---

## Current Status

**Phase 7 complete.** Build system, shared API headers, and all agent infrastructure are in place. The project compiles and links FLECS v4.1.4 + Raylib 5.5. Next: parallel module development (ECS core, spatial grid, renderer).
