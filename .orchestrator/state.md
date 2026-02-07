# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## Current Phase
Phase 9 — Testing & Debug (SpatialGrid singleton crash)

## Active Workers
<!-- Update when spawning/completing workers. Clear rows when done. -->
| Worker | Task | Branch | Worktree | Session ID | PID | Status | Started |
|--------|------|--------|----------|------------|-----|--------|---------|
| Debugger | Fix SpatialGrid singleton crash | main | — | — | 39329 | Running | 22:30 UTC |

## Task Queue

### Ready
1. **Debug SpatialGrid crash** — Fix SIGABRT during world.set<SpatialGrid>() — ACTIVE (PID 39329)

### Blocked
2. **Phase 10 start** — Blocked on Phase 9 verification (crash must be fixed first)

### Future
3. Behavior rules: infection, death, cure, reproduction, aging, promotion — Phase 10
4. Extensions via Ralph Loop: debuffs, sex, antivax, sliders, pause/reset, graphs — Phase 11

### Completed
- [x] Phase 8: Parallel module development (ECS, Spatial, Render) — 1,123 lines
- [x] Phase 9: Integration wiring (systems connected, main loop) — commit db71f56
- [x] Build verified clean (11M binary)
- [x] All previous phases (scaffolding, CMake, FLECS, headers, etc.)

## Blocking Issues
**CRITICAL:** boid_swarm crashes with SIGABRT during init_world() at world.set<SpatialGrid>()
- Location: src/ecs/world.cpp line 46
- Cause: Unknown (investigating nested vector move semantics vs FLECS v4 singleton requirements)
- Debugger worker PID 39329 investigating

## Session Decisions
<!-- Ephemeral decisions made THIS session. Cleared on fresh session start. -->
- Spawned debugger worker (Sonnet) to investigate SpatialGrid crash
- Worker will report findings to .orchestrator/inbox/debugger-spatialgrid-fix.md

## Note-Taking Reminders
<!-- These are your duties every cycle — do not skip. -->
- After every non-trivial choice → append to `.orchestrator/decisions.md` (DEC-NNN format)
- After fixing a worker mistake → append to `.orchestrator/mistakes.md` (worker's table)
- Before spawning a worker → check `.orchestrator/mistakes.md` for that worker's error history
- Before clearing session decisions → promote important ones to decisions.md
