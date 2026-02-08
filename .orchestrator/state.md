# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## No Urgent Issues ✅

raygui.h build error fixed (commit fa63385). Build clean, all 11 tests pass.

---

## Note-Taking Reminders
<!-- These are your duties every cycle — do not skip. -->
- After every non-trivial choice → append to `.orchestrator/decisions.md` (DEC-NNN format)
- After fixing a worker mistake → append to `.orchestrator/mistakes.md` (worker's table)
- Before spawning a worker → check `.orchestrator/mistakes.md` for that worker's error history
- Before clearing session decisions → promote important ones to `.orchestrator/decisions.md`
- After agents complete tasks → update Active Workers and Jobs Complete tables below

---

## Current Phase

**Phase 11 — Extensions via Ralph Loop — ACTIVE 🔄**

Infrastructure created. Ralph Loop ready to execute.

---

## Phase Progress

### Phases 1–7: Setup & Infrastructure ✅
All manual setup, scaffolding, orchestrator state, CLAUDE.md hierarchy, agent definitions, changelog hooks, build system, and shared API headers — complete.

### Phase 8: Parallel Module Development ✅
Three worktree workers built core modules in parallel (1,123 lines total):
- ECS core — FLECS world, systems pipeline, spawn logic, stats tracking
- Spatial grid — Fixed-cell hash grid, 11 passing tests, 33ms for 10k entities
- Renderer — Full Raylib pipeline with raygui stats overlay
- Branches merged to main in dependency order (spatial → ecs → render)

### Phase 9: Integration & Wiring ✅
Single integration worker connected all modules:
- Main simulation loop — FLECS world + Raylib window integrated
- Spatial grid wiring — ECS systems rebuild grid every frame
- Rendering wiring — ECS sync feeds boid positions to renderer
- Flocking behavior — 210 boids (200 Normal + 10 Doctor), cohesion/alignment/separation
- Stats overlay — Real-time population counts via raygui panel
- Build: 11M binary, all 11 tests pass, 60 FPS steady, no memory leaks
- Key commits: db71f56 (integration), a54a21e (SpatialGrid crash fix), 00a2118 (debug cleanup)

### Phase 10: Behavior Rules — COMPLETE ✅
Single worker (Option A, DEC-013) implemented all rules in one commit (c0a3aef, 516 lines):
- Aging — Health.age and InfectionState.time_infected incremented per frame
- Death — time_infected >= t_death removes Alive tag, updates dead counters
- Infection — same-swarm only, p_infect probability, spatial grid neighbor queries
- Cure — doctors cure ANY infected boid (cross-swarm), p_cure probability
- Reproduction — same-swarm, offspring from N(mean,stddev), cooldown, deferred spawning
- Promotion — Normal boid age >= t_adult, p_become_doctor per frame
- Stats — cumulative death/newborn counters, per-frame alive recounting
- 13 new files in src/sim/, systems.cpp filled in, stats.cpp updated
- Build clean, all 11 tests pass, simulation runs at 60 FPS

### Phase 11: Extensions via Ralph Loop — ACTIVE 🔄
Infrastructure created:
- `docs/current-task.md` — 6 extension tasks with guardrails
- `ralph.sh` — Stateless loop script (--model sonnet, max 30 iterations)
- Stray `ProjectsCOMP6216-Swaying-Swarmsbuild/` directory cleaned up

---

## Active Workers
<!-- record-process.sh upserts rows here on SubagentStop. -->
<!-- Format must be preserved — hook parses by | delimiters. -->
| Worker | Task | Branch | Worktree | Session ID | PID | Status | Started |
|--------|------|--------|----------|------------|-----|--------|---------|
| — | No active workers | — | — | — | — | — | — |

## Jobs Complete
<!-- record-process.sh moves rows here when status=completed. -->
| Worker | Task | Branch | Result | Completed |
|--------|------|--------|--------|-----------|
| ECS Worker | Phase 8: ECS core module | feature/ecs-core | ✅ Merged | Phase 8 |
| Spatial Worker | Phase 8: Spatial grid module | feature/spatial-grid | ✅ Merged | Phase 8 |
| Render Worker | Phase 8: Renderer module | feature/rendering | ✅ Merged | Phase 8 |
| Integration Worker | Phase 9: Module wiring | main | ✅ 60 FPS, 11 tests | Phase 9 |
| Integration Worker | Phase 9: SpatialGrid crash fix | main | ✅ commit a54a21e | Phase 9 |
| Integration Worker | Phase 9: Debug cleanup | main | ✅ commit 00a2118 | Phase 9 |
| cpp-builder | Fix raygui.h MSVC build error | main | ✅ commit fa63385 | Phase 10 pre |
| Behavior Worker | Phase 10: All behavior rules | main | ✅ commit c0a3aef, 516 lines | Phase 10 |

---

## Blocking Issues

| Issue | Severity | Blocks | Status |
|-------|----------|--------|--------|
| raygui.h not found on MSVC/PowerShell build | 🔴 Critical | Phase 10, 11 | ✅ Fixed (fa63385) |

---

## Session Decisions
<!-- Scratch space for this session. Promote to decisions.md before session end. -->

- DEC-014: Phase 11 Ralph Loop infrastructure created (docs/current-task.md + ralph.sh)