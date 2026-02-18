# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## Phase 13 In Progress 🔧 — Branch: `Abe`

Part A (A1-A9): Antivax separate swarm — COMPLETE ✅ (10 commits)
Part B (B1): Doctor self-cure test — COMPLETE ✅ (commit 33ebb77)
Part B (B2-B8): Antivax test coverage — COMPLETE ✅ (commit 62fc012, 10 new tests)
Part C (C1-C4): Steering model fixes — IN PROGRESS 🔧 (branch Abe)
Build clean, 35 tests pass (12 ConfigLoader + 11 SpatialGrid + 2 CureContract + 10 Antivax)

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

**Phase 11 — Extensions via Ralph Loop — COMPLETE ✅**

**Phase 12 — Refinements & Configuration — COMPLETE ✅**

**Phase 13 — Swarm Refinement & Steering Fixes — IN PROGRESS 🔧**

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

### Phase 11: Extensions via Ralph Loop — COMPLETE ✅
Ralph Loop completed 6/6 tasks in 6 iterations (~23 min):
- Infected debuffs (70d36bd), Sex system (ac90f8c), Antivax (0a52cd0)
- Parameter sliders (cebd9b8), Pause/Reset (889eeab), Population graph (caaafa9)
- Code review: 3 critical fixes applied (d9a76ef)
- Config loader: INI-based SimConfig loading with CLI arg support (d9a76ef)
- Build clean, 23 tests pass (12 ConfigLoader + 11 SpatialGrid)

### Phase 12: Refinements & Configuration — COMPLETE ✅
Ralph Loop completed 5/5 tasks in 4 iterations:
- Remove FLECS include from renderer (263d30f) — module boundary violation fixed
- Expand slider ranges 5-200 (6303b12, f60caf9)
- Smooth population graph Y-scale (b077f1e)
- Add keyboard shortcuts SPACE/R (e872eb5)
- Delete dead boids on reset (3cc25be)
Also: hook sed fix (dc3b081), README synthesized 585→163 lines (f4638cb)
Build clean, 23 tests pass, all context.md spec requirements implemented.

### Phase 13: Swarm Refinement & Steering Fixes — IN PROGRESS 🔧
Part A (A1-A9) complete — AntivaxBoid promoted to primary swarm tag:
- A1: AntivaxBoid component + stats fields (7c713d1)
- A2: Spawn AntivaxBoid from normal pool (e6c3a1b)
- A3: Render antivax in orange, swarm_type replaces is_doctor (f2b15ef)
- A4+A5: Antivax infection + reproduction, same-swarm only (5b8a21a)
- A6: Verified doctor-cures-antivax + no-self-cure contract (5271d3b)
- A7: Antivax stats tracking + population graph orange line (a204f38)
- A8: Antivax steering uses AntivaxBoid primary tag (4ace9ac)
- A9: Deprecated Antivax tag removed from codebase (caf88d4)
Part B1 complete — Doctor no-self-cure unit test (33ebb77).
Part B2-B8 complete — 10 antivax tests in test_antivax.cpp (62fc012), 35 tests pass.
Part C pending — Steering fixes (C1-C4) paused per user request.

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
| Ralph Loop | Phase 11: 6 extensions (6 iters) | main | ✅ 6 commits, ~23 min | Phase 11 |
| code-reviewer | Phase 11: Code review | main | ✅ 3 critical, 3 warnings | Phase 11 |
| cpp-builder | Phase 11: Build verification | main | ✅ 23 tests pass | Phase 11 |
| Orchestrator | Config loader + review fixes | main | ✅ commit d9a76ef | Phase 11 |
| Ralph Loop | Phase 12: 5 refinements (4 iters) | main | ✅ 5 commits, ~8 min | Phase 12 |
| Orchestrator | README synthesis + hook fix + spec audit | main | ✅ f4638cb, dc3b081 | Phase 12 |
| Ralph Loop | Phase 12: 5 refinements (4 iters) | main | ✅ 5 commits | Phase 12 |
| Orchestrator | Hook fix + README synthesis | main | ✅ dc3b081, f4638cb | Phase 12 |
| Orchestrator+Subagents | Phase 13 Part A: AntivaxBoid swarm (A1-A9) | main | ✅ 9 tasks, 10 commits | Phase 13 |
| Subagent (sonnet) | Phase 13 Part B1: Doctor no-self-cure test | main | ✅ commit 33ebb77, 25 tests | Phase 13 |
| Subagent (sonnet) + Orchestrator | Phase 13 Part B2-B8: Antivax test coverage | main | ✅ commit 62fc012, 35 tests | Phase 13 |

---

## Blocking Issues

| Issue | Severity | Blocks | Status |
|-------|----------|--------|--------|
| raygui.h not found on MSVC/PowerShell build | 🔴 Critical | Phase 10, 11 | ✅ Fixed (fa63385) |

---

## Session Decisions
<!-- Scratch space for this session. Promote to decisions.md before session end. -->

- DEC-016: Phase 13 pre-flight — recorded 4 structural mistakes in mistakes.md with prevention rules
- DEC-017: Phase 13 execution — switched from Ralph Loop (failed on permissions) to direct subagent dispatch. Parallel where files don't conflict. Sonnet for complex tasks, Haiku for simple ones.
- DEC-018: Worker mistake caught — A5 subagent added deprecated `.add<Antivax>()` to offspring. Fixed by orchestrator before commit. Prevention: always verify subagent output for deprecated patterns.
- DEC-019: A3 subagent failed (Bash permission denied in general-purpose agent). Orchestrator completed the task manually, also fixed render_demo.cpp which the agent missed.
- DEC-020: Part A+B complete but zero test coverage for antivax behaviors. Adding B2-B8 test tasks before proceeding to Part C.