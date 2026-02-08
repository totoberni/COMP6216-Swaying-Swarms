# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## Note-Taking Reminders
<!-- These are your duties every cycle — do not skip. -->
- After every non-trivial choice → append to `.orchestrator/decisions.md` (DEC-NNN format)
- After fixing a worker mistake → append to `.orchestrator/mistakes.md` (worker's table)
- Before spawning a worker → check `.orchestrator/mistakes.md` for that worker's error history
- Before clearing session decisions → promote important ones to `.orchestrator/decisions.md`
- After Agents complete tasks, update the `.orchestrator/state.md` `Active Workers` and `Jobs Complete` tables.

## Current Phase
Phase 9 — COMPLETE ✅

**Next:** Phase 10 — Behavior Rules (infection, death, cure, reproduction, aging, promotion)

## Active Workers
<!-- Update when spawning workers. Clear rows ONLY when task is done. -->
| Worker | Task | Branch | Worktree | Session ID | PID | Status | Started |
|--------|------|--------|----------|------------|-----|--------|---------|
| — | No active workers | — | — | — | — | — | — |

## Jobs Complete
<!-- Update when workers have completed tasks.-->
| Worker | Task | Branch | Worktree | Session ID | PID | Status | Started |
|--------|------|--------|----------|------------|-----|--------|---------|
| — | No active workers | — | — | — | — | — | — |

## Task Queue

### Ready
0. **Interrim task**: Debugging: The windows build `cmake --build build` on Powershell currently returns some errors! These must be fixed with the code-reviewer agent and debugger.
1. **Phase 10: Behavior Rules** — Implement all simulation logic from context.md (infection, death, cure, reproduction, promotion, aging)

### Blocked
None

### Future
2. Extensions via Ralph Loop: debuffs, sex, antivax, sliders, pause/reset, graphs — Phase 11

### Completed
- [x] Phase 8: Parallel module development (ECS, Spatial, Render) — 1,123 lines
- [x] Phase 9: Integration wiring (systems connected, main loop) — commit db71f56
- [x] Phase 9: SpatialGrid crash fix (default constructor) — commit a54a21e
- [x] Phase 9: Debug cleanup — commit 00a2118
- [x] Build verified clean (11M binary, all 11 tests pass)
- [x] Runtime verified: 60 FPS, 210 boids, flocking behavior working
- [x] All previous phases (scaffolding, CMake, FLECS, headers, etc.)

## Blocking Issues
None. All Phase 9 blockers resolved.