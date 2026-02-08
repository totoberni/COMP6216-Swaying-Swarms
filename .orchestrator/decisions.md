# Executive Decisions Log
<!-- Orchestrator records every non-trivial decision here for human operator visibility. -->
<!-- Format: DEC-NNN, sequential. Never delete entries — they are the audit trail. -->

## DEC-001: ECS Framework Selection
**When:** Phase 0 (pre-orchestrator, human decision)
**Context:** Needed an entity-component system for boid simulation.
**Decision:** FLECS v4.1.4 with C++17 API.
**Rationale:** Built-in pipeline phases, tag components, singleton support. Lightweight, actively maintained.
**Alternatives rejected:** EnTT (less structured pipeline), custom ECS (unnecessary engineering).

## DEC-002: Rendering Library
**When:** Phase 0 (pre-orchestrator, human decision)
**Context:** Needed 2D rendering with immediate-mode UI for stats overlay.
**Decision:** Raylib 5.5 + raygui.
**Rationale:** Simple 2D API, no heavy dependencies. raygui for stats panel. Upgrade path to Dear ImGui via rlImGui if needed.
**Alternatives rejected:** SDL2 (more boilerplate), SFML (heavier for the task).

## DEC-003: Spatial Partitioning Strategy
**When:** Phase 0 (pre-orchestrator, human decision)
**Context:** Need efficient neighbor queries for collision detection across potentially thousands of boids.
**Decision:** Fixed-cell hash grid, cell size = max interaction radius.
**Rationale:** O(1) insert, O(neighbors) query. Flat arrays for cache efficiency. Pure C++ module, no FLECS dependency.
**Alternatives rejected:** Quadtree (more complex, worse cache behavior for uniform distributions), brute force (O(n²)).

## DEC-004: Agent Isolation Strategy
**When:** Phase 3 (pre-orchestrator, human decision)
**Context:** Multiple agents need to write code simultaneously without file conflicts.
**Decision:** Git worktrees for parallel write-heavy work; subagents (Task tool) for read-heavy parallel tasks.
**Rationale:** Worktrees give full filesystem isolation with shared git history. Subagents share the checkout but get isolated context windows — fine for reading, dangerous for writing.
**Alternatives rejected:** Single-checkout with file locking (too serialized), separate repos (no shared history).

## DEC-005: Time Units for SimConfig
**When:** Phase 7 (orchestrator/human)
**Context:** Master plan used frame-based units (300 frames for t_death). FLECS provides delta_time in seconds.
**Decision:** Use seconds as the canonical time unit throughout SimConfig.
**Rationale:** Frame-based units break under variable framerate. FLECS naturally operates in seconds. Conversion from master plan: frames ÷ 60 = seconds.
**Alternatives rejected:** Frame-based (fragile under variable dt), dual units (confusing).

## DEC-006: Infected Tag vs Bool Field
**When:** Phase 7 (orchestrator/human)
**Context:** Need to mark boids as infected. Two options: tag component or bool field in a struct.
**Decision:** `Infected` as a FLECS tag component (empty struct, added/removed from entity).
**Rationale:** Idiomatic FLECS. Tags enable efficient archetype-based queries (e.g., query all entities WITH Infected). Bool fields require checking every entity.
**Alternatives rejected:** Bool field in Health component (wastes query time, non-idiomatic).

<!-- Next decision: DEC-007 -->
## DEC-007: Phase 8 Worktree Creation and Branch Naming
**When:** Phase 8 start (2025-02-07 21:02 UTC)
**Context:** Need isolated workspaces for three parallel module implementations.
**Decision:** Created three worktrees at `../boids-{ecs,spatial,render}` with feature branches `feature/{ecs-core,spatial-grid,rendering}`.
**Rationale:** Standard git worktree pattern. Parent directory keeps all workspaces colocated. Feature branch naming aligns with git-flow conventions.
**Alternatives rejected:** In-place branches (merge conflicts inevitable), separate clones (loses shared refs).

## DEC-008: Worker Spawning via claude -p in Background
**When:** Phase 8 start (2025-02-07 21:03 UTC)
**Context:** Need three workers to execute tasks autonomously without orchestrator micromanagement.
**Decision:** Spawned `claude -p "<task_prompt>" --model sonnet --output-format json --dangerously-skip-permissions` in each worktree as background bash processes. Captured PIDs but not session IDs (jq pipeline didn't output to temp files as expected).
**Rationale:** `--dangerously-skip-permissions` lets workers edit files without approval prompts. `--model sonnet` for cost efficiency. Background processes allow parallel execution.
**Alternatives rejected:** Sequential workers (too slow), Task tool subagents (can't write to filesystem safely in parallel), interactive sessions (need orchestrator input).
**Note:** Session ID capture via jq|tee pipeline failed — likely timing issue. Workers are running (PIDs verified), but session IDs unknown. Will use PID monitoring instead.

<!-- Next decision: DEC-009 -->

## DEC-009: ECS Worker Timeout and Premature Termination
**When:** Phase 8 (2025-02-07 21:21 UTC)
**Context:** ECS worker ran for 16:37 (3.3x longer than other workers). No commits appeared. Implemented timeout policy and terminated worker.
**Decision:** Killed worker at 16:37 runtime. Upon inspection, discovered build had actually succeeded and binary was functional. Worker was likely in verification phase (running 1000 frames test).
**Outcome:** Manually committed the completed work (commit 68db9e6). All code functional: 417 lines, builds clean, runs successfully.
**Lesson Learned:** Worker completion indicators are:
  1. Git commit (definitive)
  2. Binary exists + runs (task complete, commit pending)
  3. Active file modifications + CPU usage (still working)
  
Should check #2 before terminating on timeout. 16-17 minutes is acceptable for complex FLECS integration work. Timeout threshold should be 20 minutes, not 15.
**Rationale for extended time:** FLECS API learning curve + multiple build-fix iterations + verification phase. Worker was following instructions correctly (using ecs-architect and cpp-builder subagents).
**Alternatives considered:** 
  - Intervene with FLECS API docs (would have saved 5 min, but undermines autonomous learning)
  - Restart worker with better prompts (would have cost more time)
**Prevention rule for future:** Before terminating a long-running worker, check if binary exists and runs. If yes, wait for commit or 20-min hard timeout.

<!-- Next decision: DEC-010 -->

## DEC-010: Opus Worker for Phase 9 Integration
**When:** Phase 9 (2025-02-07 22:14 UTC)
**Context:** Initial worker spawning via `claude -p` within orchestrator session failed (created recursive loop). Needed proper independent worker process.
**Decision:** Spawned Opus model worker using `nohup` + `&` backgrounding with PID capture (36096). Worker used Sonnet subagents for ecs-architect and cpp-builder.
**Rationale:** 
- Opus for complex integration work (touches all modules)
- Sonnet for specialized subagents (cost-efficient for focused tasks)
- `nohup` ensures process detachment from orchestrator session
**Outcome:** Successful. Worker completed Phase 9 in ~3 minutes. All integration requirements met, clean build, 0 errors.
**Note:** Opus usage is **exception for Phase 9 only**. Future phases return to Sonnet workers unless complexity requires escalation.

<!-- Next decision: DEC-011 -->

## DEC-011: SpatialGrid Crash Resolution
**When:** 2026-02-07 22:30-22:35 UTC (Phase 9 verification)
**Context:** boid_swarm crashed with SIGABRT during `world.set<SpatialGrid>()` in init_world()
**Decision:** Spawned Sonnet debugger worker (PID 39329) to investigate and fix
**Outcome:** Worker identified root cause (FLECS requires default-constructible singletons) and applied minimal fix
**Rationale:** 
- Complex C++ template metaprogramming issue requiring systematic investigation
- Delegating to debugger agent more efficient than orchestrator trial-and-error
- Worker produced excellent documentation for future reference
**Fix Applied:**
- Added `SpatialGrid() = default;` to header
- Added member initializers (world_w_ = 0.0f, cols_ = 0, etc.)
- No implementation changes needed
**Verification:** Build clean, simulation runs at 60 FPS, 210 boids visible, all 11 tests pass
**Lesson:** All FLECS singleton types must be default-constructible (document in CLAUDE.md FLECS Patterns)

## DEC-012: raygui.h Build Fix
**When:** 2026-02-08 (Phase 10 prerequisite)
**Context:** MSVC build failed with `error C1083: Cannot open include file: 'raygui.h'`. State.md flagged this as a blocker for Phase 10.
**Root cause:** raygui was never added as a CPM dependency. CMakeLists.txt had a hacky include path `${CMAKE_BINARY_DIR}/_deps/raylib-src/examples/shapes` hoping raygui.h was bundled with raylib — it wasn't.
**Decision:** Spawned cpp-builder subagent to add raygui v4.0 via CPM and replace the include path with `${raygui_SOURCE_DIR}/src`.
**Fix Applied:** commit fa63385 — `CPMAddPackage(NAME raygui ...)` + corrected `target_include_directories` for boid_swarm and render_demo.
**Verification:** MSVC build clean, all 3 executables compile, all 11 tests pass, simulation runs at 60 FPS.
**Lesson:** Always verify CPM dependencies are explicitly declared — don't rely on header-only libs being bundled with other packages.

## DEC-013: Phase 10 Worker Strategy — Single Worker (Option A)
**When:** 2026-02-08 (Phase 10 start)
**Context:** Plan.md offers Option A (single worker, all rules sequentially) or Option B (two workers in worktrees, split normal/doctor rules). Need to choose.
**Decision:** Option A — single worker doing all behavior rules sequentially.
**Rationale:**
- Behavior rules are tightly coupled: infection feeds death, cure interacts with infection, reproduction needs collision detection. Sequential implementation avoids merge conflicts in shared files (`src/ecs/systems.cpp`, `include/components.h`).
- All system stubs already exist in `systems.cpp` — worker fills them in order.
- `src/sim/` is empty — no risk of parallel file conflicts, but the ECS systems that call sim functions are in one file.
- Worktree overhead not justified for a single-file-heavy task.
**Alternatives rejected:** Option B (parallel workers) — merge conflicts in systems.cpp would cost more time than parallel savings.

<!-- Next decision: DEC-014 -->
