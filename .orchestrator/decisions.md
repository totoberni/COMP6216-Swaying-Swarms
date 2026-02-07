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