# Orchestrator State
<!-- SINGLE mutable state. Read at cycle start (auto-loaded via CLAUDE.md @import). -->
<!-- Update before /compact or session end. -->
<!-- Sibling files: plan.md (mission), decisions.md (audit trail), mistakes.md (error patterns). -->

## Current Phase
Phase 8 — Parallel module development. Three modules to build in isolated worktrees.

## Active Workers
<!-- Update when spawning/completing workers. Clear rows when done. -->
| Worker | Task | Branch | Worktree | Session ID | PID | Status | Started |
|--------|------|--------|----------|------------|-----|--------|---------|
| — | — | — | — | — | — | — | — |

## Task Queue

### Ready (unblocked, in priority order)
1. **ECS core** — world init, systems, spawn, stats — `feature/ecs-core` — `src/ecs/`
2. **Spatial grid** — hash grid impl + unit tests — `feature/spatial-grid` — `src/spatial/`, `tests/`
3. **Renderer** — Raylib pipeline, stats overlay, demo — `feature/rendering` — `src/render/`

### Blocked
None.

### Future (unlocked after current phase completes)
4. Integration: wire ECS ↔ spatial ↔ renderer + main loop — Phase 9
5. Behavior rules: infection, death, cure, reproduction, aging, promotion — Phase 10
6. Extensions via Ralph Loop: debuffs, sex, antivax, sliders, pause/reset, graphs — Phase 11

### Completed
- [x] Repository scaffolding (Phase 2)
- [x] .claude/settings.json, permissions (Phase 2)
- [x] Orchestrator state infrastructure (Phase 3)
- [x] CLAUDE.md memory hierarchy — root + per-module (Phase 4)
- [x] Agent definitions: orchestrator, code-worker, code-reviewer, debugger, changelog-scribe (Phase 5)
- [x] Slash commands: /build, /review, /test, /fix-issue (Phase 5)
- [x] FLECS skill: .claude/skills/flecs-patterns/ (Phase 5)
- [x] Changelog hooks: PostToolUse → update-changelog.sh (Phase 6)
- [x] CMake + CPM.cmake + FLECS v4.1.4 + Raylib 5.5 (Phase 7)
- [x] Shared API headers: components.h, spatial_grid.h, render_state.h (Phase 7)
- [x] Build verified: boid_swarm.exe compiles and links (Phase 7)
- [x] clangd support: compile_flags.txt + .clang-format (Phase 7)
- [x] SimConfig defaults aligned (time-based seconds, not frame counts) (Phase 7)

## Blocking Issues
None.

## Session Decisions
<!-- Ephemeral decisions made THIS session. Cleared on fresh session start. -->
<!-- IMPORTANT: Before clearing, copy any non-trivial decisions to .orchestrator/decisions.md -->
None yet this session.

## Note-Taking Reminders
<!-- These are your duties every cycle — do not skip. -->
- After every non-trivial choice → append to `.orchestrator/decisions.md` (DEC-NNN format)
- After fixing a worker mistake → append to `.orchestrator/mistakes.md` (worker's table)
- Before spawning a worker → check `.orchestrator/mistakes.md` for that worker's error history
- Before clearing session decisions → promote important ones to decisions.md