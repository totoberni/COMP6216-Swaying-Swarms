# Architecture Decisions

## ADR-001: ECS Framework
**Decision:** FLECS v4.1.4 (C++17 API)
**Rationale:** Built-in ECS with pipeline phases, tag components, singleton support. Lightweight. Active development.

## ADR-002: Rendering
**Decision:** Raylib 5.5 + raygui
**Rationale:** Simple 2D rendering, no heavy dependencies. raygui for immediate-mode stats overlay. Upgrade path to Dear ImGui via rlImGui.

## ADR-003: Spatial Partitioning
**Decision:** Fixed-cell hash grid, cell size = max interaction radius
**Rationale:** O(1) insert, O(neighbors) query. Flat arrays for cache efficiency. Pure C++ module, no FLECS dependency.

## ADR-004: Agent Isolation
**Decision:** Git worktrees for parallel write-heavy work, subagents for read-heavy parallel tasks
**Rationale:** Worktrees prevent file conflicts between agents. Subagents share checkout but get isolated context windows.