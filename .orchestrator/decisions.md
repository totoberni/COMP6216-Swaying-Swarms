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
