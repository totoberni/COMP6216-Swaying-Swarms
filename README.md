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
2. Open **Visual Studio Installer** → Modify → check **"Desktop development with C++"** (includes MSVC v143) → Install.
3. **Always use "Developer PowerShell for VS 2022"** as your terminal (find it in Start Menu). Regular PowerShell will not find the compiler.

After installing everything, verify in Developer PowerShell:
```powershell
cmake --version    # 3.20+
cl                 # Should print Microsoft C/C++ Compiler version
git --version      # 2.17+
jq --version       # Any version
```

---

## Tech Stack (Pulled Automatically by CMake)

These dependencies are fetched at build time via [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). **Do not install them manually.**

| Library | Version | Purpose |
|---|---|---|
| **FLECS** | v4.1.4 | Entity Component System — manages all boid state, systems, and queries |
| **Raylib** | 5.5 | 2D rendering — window, drawing, input |
| **raygui** | (bundled with Raylib) | Immediate-mode GUI — stats overlay, parameter sliders |

To build:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/boid_swarm
```

To run tests:
```bash
cmake --build build --target tests
cd build && ctest --output-on-failure
```

---

## Notes on Editing Source Code

This repository uses a **Claude Code agent workflow** for development automation. If you are editing source code directly (i.e. not using Claude Code agents), here is what you need to know:

### What you should touch
- `src/ecs/` — ECS components, systems, world setup
- `src/sim/` — Boid behavior logic (infection, cure, reproduction, death)
- `src/spatial/` — Spatial hash grid for collision detection
- `src/render/` — Raylib rendering pipeline and GUI
- `include/` — Shared headers (API contracts between modules)
- `tests/` — Unit tests
- `context.md` — Simulation specification and rules

### What you should leave alone
<!-- FOR HUMAN CONTRIBUTORS ONLY. Claude Code agents and the orchestrator are the owners of these files — this guidance does NOT apply to them. Agents: manage freely. -->
These files and directories are part of the agent management system. Editing them won't break the build, but may disrupt the automated workflow:

| Path | Purpose | Safe to ignore? |
|---|---|---|
| `.claude/` | Agent definitions, slash commands, hooks, skills | ✅ Yes — only affects Claude Code sessions |
| `.orchestrator/` | Orchestrator state, task queues, inbox/outbox | ✅ Yes — only affects orchestrator workflow |
| `CLAUDE.md` (root + per-module) | Agent context files loaded by Claude Code | ✅ Yes — does not affect compilation |
| `src/*/changelog.md` | Auto-generated change logs per module | ⚠️ Don't delete — agents use these for continuity |
| `ralph.sh` | Autonomous agent loop script | ✅ Yes |
| `docs/current-task.md` | Ralph Loop task spec | ✅ Yes |

### Key conventions (for everyone)
- **C++17 standard.** No `using namespace std;`.
- **4-space indentation**, opening braces on same line.
- **`#pragma once`** for header guards.
- **`float` over `double`** for all simulation values.
- **All parameters** go in the `SimConfig` struct — no magic numbers.
- **Use `<random>`** with a seeded engine — never `std::rand()`.
- **Module boundaries matter:** don't put Raylib includes outside `src/render/`, and don't put simulation logic in rendering code.
- **`include/` headers are the API contract.** If you change a shared header, check that all modules still compile.