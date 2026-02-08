# CLAUDE.md — COMP6216 Boid Swarm Simulation

## Project Summary
2D pandemic boid simulation: two swarms (Normal Boids, Doctor Boids).
FLECS ECS, Raylib rendering, fixed spatial grid for collisions. C++17.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/boid_swarm
```

## Test
```bash
cmake --build build --target tests
cd build && ctest --output-on-failure
```

## Architecture
- `src/ecs/` — FLECS components, systems, world setup
- `src/sim/` — Behavior logic: infection, cure, reproduction, death
- `src/spatial/` — Fixed-grid spatial index (FLECS singleton)
- `src/render/` — Raylib window, drawing, raygui stats overlay
- `include/` — Shared headers (API contract between modules)
- `tests/` — Unit tests

## Code Style
- C++17. No `using namespace std;`
- 4-space indentation, braces on same line
- `#pragma once` header guards
- FLECS components = plain structs, no methods
- `float` over `double` for all sim values
- All params in `SimConfig` struct, never magic numbers
- `<random>` with seeded engine, never `std::rand()`

## Module Boundaries (CRITICAL)
- Agents MUST NOT edit files outside their assigned directory
- `include/` headers = API contract. Change only after coordination.
- `src/ecs/` owns components + system registration
- `src/sim/` owns behavior logic (called by ECS systems)
- `src/spatial/` owns grid + neighbor queries (pure C++, no FLECS includes)
- `src/render/` owns ALL Raylib calls. No Raylib outside this dir.

## FLECS Patterns
- Tag components: `struct NormalBoid {};`
- Singletons: `world.set<T>()` for SimConfig, SpatialGrid, SimStats
  - **CRITICAL:** Singleton types MUST be default-constructible (add `T() = default;` + member initializers)
  - FLECS internally default-constructs storage before `world.set<T>()` can move/copy into it
- Pipeline: PreUpdate→spatial rebuild, OnUpdate→steering, PostUpdate→collision/infection, OnStore→render
- Deferred ops: `world.defer_begin()`/`defer_end()` for spawn/destroy during iteration

## Orchestrator State
@.orchestrator/state.md

## Orchestator-Tacked mistakes
@.orchestrator/mistakes.md

## Module Changelogs
@src/ecs/changelog.md
@src/spatial/changelog.md
@src/render/changelog.md
@src/sim/changelog.md
@include/changelog.md

## Agent File Ownership
Agents and the orchestrator OWN and MANAGE these paths — read, write, update freely:
- `.claude/` — agents, commands, hooks, skills, settings
- `.orchestrator/` — state, task-queue, active-tasks, decisions, inbox, outbox
- `CLAUDE.md` files — root + all per-module child files
- `src/*/changelog.md`, `include/changelog.md` — auto-managed by hooks
- `ralph.sh`, `docs/current-task.md`
- `README.md` — orchestrator updates changelog summary for collaborators
The "leave alone" guidance in README.md targets human contributors only. It does NOT apply to agents.
README.md itself contains an HTML comment carve-out above its "leave alone" section reinforcing this.

## DO NOT
- Modify CMakeLists.txt without explicit instruction
- Add dependencies without discussion
- Use raw pointers for ownership — use FLECS entity handles
- Put rendering logic in simulation code or vice versa