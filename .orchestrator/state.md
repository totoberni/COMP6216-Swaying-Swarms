# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## ⚠ URGENT — Fix Before Any New Work

**PowerShell/MSVC build is broken.** The following error occurs on `cmake --build build` in Windows PowerShell:

```
error C1083: Cannot open include file: 'raygui.h': No such file or directory
  [C:\Projects\COMP6216-Swaying-Swarms\build\boid_swarm.vcxproj]
```

**Root cause (likely):** `raygui.h` is fetched by CPM.cmake but not added to the include path for the MSVC build, or the `RAYGUI_IMPLEMENTATION` define is missing from the translation unit that includes it. The MinGW/WSL build may work because of different include resolution.

**Action required:** Spawn a `debugger` subagent with the exact error text. Fix CMakeLists.txt or the CPM fetch target. Verify with both PowerShell (`cmake --build build`) and WSL (`cmake -B build && cmake --build build`). This blocks ALL Phase 10+ work.

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

**Phase 9 — COMPLETE ✅**

**Next action:** Fix raygui.h build error (see URGENT above), then begin Phase 10.

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

### Phase 10: Behavior Rules — NOT STARTED ⏳
**Blocked by:** raygui.h build error (see URGENT section above)

Tasks (from plan.md):
- [ ] Infection mechanics (p_infect per frame within r_interact)
- [ ] Death system (t_death seconds after infection)
- [ ] Cure behavior (Doctor proximity, p_cure)
- [ ] Reproduction system (two boids collide → offspring)
- [ ] Aging & promotion (Normal → Doctor after t_adult)
- [ ] Stats tracking for all population metrics

### Phase 11: Extensions via Ralph Loop — FUTURE
Depends on Phase 10 completion. Six extensions defined in plan.md.

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

---

## Blocking Issues

| Issue | Severity | Blocks | Status |
|-------|----------|--------|--------|
| raygui.h not found on MSVC/PowerShell build | 🔴 Critical | Phase 10, 11 | Open — fix first |

---

## Session Decisions
<!-- Scratch space for this session. Promote to decisions.md before session end. -->

(none yet)