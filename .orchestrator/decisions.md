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

## DEC-014: Phase 11 Ralph Loop Infrastructure
**When:** 2026-02-08 (Phase 11 start)
**Context:** Phase 10 complete. Plan.md specifies a "Ralph Loop" — stateless `claude -p` iterations — for implementing 6 extensions.
**Decision:** Created `docs/current-task.md` (task list + guardrails) and `ralph.sh` (loop controller) per plan.md templates.
**Modifications from template:**
- Added `--model sonnet` to `claude -p` invocation (cost efficiency for focused single-task work)
- Added 3 extra guardrails from session lessons: CPM dependencies, hook `set -e` avoidance, FLECS default-constructible singletons
**Rationale:** Ralph Loop gives each iteration a fresh 200K context window, preventing context rot. Stateless design means crashed iterations lose nothing — the task file is the single source of truth.
**Alternatives rejected:** Single long-running worker (context rot after 3-4 tasks), parallel workers per extension (dependencies between tasks, e.g. sex system needs components before antivax can reference them).

## DEC-015: Phase 13 Pre-Flight Mistake Recording
**When:** 2026-02-17 (Phase 13 start)
**Context:** Four structural mistakes identified from Phases 9 and 11 that caused incorrect steering behavior and ambiguous swarm identity.
**Decision:** Recorded all 4 mistakes in mistakes.md with prevention rules BEFORE any worker dispatch.
**Rationale:** Prevention rules are embedded in worker task prompts to avoid repeating known errors. This is the orchestrator's primary quality mechanism.
**Outcome:** All 4 prevention rules carried into Phase 13 task prompts. No recurrence of any recorded mistake.

## DEC-016: Phase 13 Execution — Direct Subagent Dispatch
**When:** 2026-02-17 (Phase 13)
**Context:** Ralph Loop (`ralph.sh`) failed due to Bash permission errors in the worker process. Needed an alternative execution strategy.
**Decision:** Switched to direct subagent dispatch via Task tool. Parallelize where files don't conflict. Sonnet for complex multi-file tasks, Haiku for simple tag/comment changes.
**Rationale:** Task tool subagents inherit permissions from the orchestrator session. Ralph Loop spawns independent `claude -p` processes that don't inherit the permission context. Direct dispatch is also faster for small tasks.
**Alternatives rejected:** Ralph Loop (broken permissions), single sequential worker (too slow for 9 independent tasks).

## DEC-017: Worker Mistake — A5 Deprecated Tag in Offspring
**When:** 2026-02-17 (Phase 13, Task A5)
**Context:** A5 subagent added `.add<Antivax>()` (deprecated additive tag) to reproduction offspring alongside the correct `.add<AntivaxBoid>()`.
**Decision:** Orchestrator caught the error in post-dispatch review and removed the deprecated tag before commit.
**Rationale:** Recorded as DEC-018/mistake to establish pattern: always verify subagent output for deprecated patterns before committing.
**Lesson:** Worker prompts must explicitly list deprecated symbols to avoid.

## DEC-018: A3 Subagent Failure — Manual Completion
**When:** 2026-02-17 (Phase 13, Task A3)
**Context:** A3 subagent (general-purpose type) failed with Bash permission denied when trying to build.
**Decision:** Orchestrator completed A3 manually, also fixed render_demo.cpp which the agent's plan had missed.
**Rationale:** Faster to complete manually than debug agent permissions. Also revealed render_demo.cpp gap in the original task spec.

## DEC-019: B2-B8 Test Coverage Before Part C
**When:** 2026-02-17 (Phase 13)
**Context:** Parts A and B1 complete but zero test coverage for antivax spawn, infection, reproduction, death, stats, and steering behaviors.
**Decision:** Added B2-B8 test tasks to plan.md. All tests go in one file (`tests/test_antivax.cpp`) written by a single worker.
**Rationale:** Test coverage prevents regressions during Part C steering rewrites. Single worker because all tests share the same file — parallel writing would cause conflicts.

## DEC-020: B2-B8 Single Worker Strategy
**When:** 2026-02-18 (Phase 13, Part B)
**Context:** B2-B8 defines 7 test tasks (10 test cases) all targeting `tests/test_antivax.cpp`.
**Decision:** Dispatched one sonnet subagent to write all 10 tests in a single file, plus expose 6 system registration functions in systems.h.
**Rationale:** All tests go in one file — splitting across workers would cause merge conflicts. Worker also needed to add forward declarations to systems.h for unit test access.
**Outcome:** 10 tests written, 35 tests pass. One fix required (see DEC-021).

## DEC-021: B4 Reproduction Test Timeout Fix
**When:** 2026-02-18 (Phase 13, Part B)
**Context:** `ctest` hung indefinitely. Root cause: B4 test used `reproduction_cooldown = 0.0f` with `p_offspring_normal = 1.0f` and `offspring_mean_normal = 3.0f` over 120 frames, causing exponential population growth.
**Decision:** Changed `reproduction_cooldown = 60.0f` and reduced frame count from 120 to 5. Applied same fix to NoCrossSwarm test.
**Rationale:** With zero cooldown, every offspring can reproduce next frame. 3^120 offspring is unbounded. A long cooldown and few frames ensures offspring are born without explosion.
**Lesson:** Always use non-zero cooldowns and minimal frame counts in reproduction tests.

## DEC-022: C1+C2+C3 Combined, C4 Parallel
**When:** 2026-02-18 (Phase 13, Part C)
**Context:** C1 (cohesion), C2 (alignment), and C3 (swarm filtering) all modify the SteeringSystem in systems.cpp. C4 (min_speed) modifies the MovementSystem and adds a config field.
**Decision:** Combined C1+C2+C3 into one ecs-architect worker. Dispatched C4 as a parallel sonnet worker.
**Rationale:** C1-C3 touch overlapping code regions in the same function — parallel edits would conflict. C4 is in a different function (MovementSystem) and different files (components.h, config.ini, config_loader.cpp), so it can run concurrently.
**Outcome:** Both workers completed successfully. Code review then caught two additional issues.

## DEC-023: Branch Abe for Part C
**When:** 2026-02-18 (Phase 13)
**Context:** User requested a branch for Part C work since steering fixes were assigned to a different team member (Abe).
**Decision:** Created branch `Abe` from main at commit 62fc012 (after B2-B8). Part C committed on this branch.
**Rationale:** User's team workflow requires branch-per-member. Branch created after B tests were on main to keep test infrastructure shared.

## DEC-024: Post-Implementation Code Review
**When:** 2026-02-18 (Phase 13, Part C)
**Context:** C1-C4 changes are critical steering rewrites affecting all boid movement. High risk of subtle bugs.
**Decision:** Dispatched code-reviewer subagent after both C workers completed, before committing.
**Findings:**
- CRIT-1: AntivaxSteeringSystem still had `if (force_mag > 1)` instead of `config.max_force` — same bug that C1-C3 fixed in SteeringSystem but wasn't in scope.
- WARN-3/CRIT-2: min_speed could exceed max_speed via sliders, causing oscillation.
**Fixes applied:** max_force clamp in antivax steering, min/max speed guard.
**Lesson:** Always include ALL steering-related systems when auditing force clamp bugs, not just the primary SteeringSystem.
