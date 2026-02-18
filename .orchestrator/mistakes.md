# Worker Mistake Log
<!-- Orchestrator records mistakes AFTER fixing them, to inform future delegations. -->
<!-- Before spawning a worker, check their table. Add prevention rules to task prompts. -->
<!-- Never delete entries — patterns compound and inform guardrails. -->

## How to Use This File

1. When a worker produces incorrect output and you fix the problem, add a row to that worker's table.
2. Before delegating to a worker, scan their table for recurring patterns.
3. Incorporate "Prevention Rule" text directly into the worker's task prompt as a guardrail.
4. If a pattern appears 3+ times across any workers, escalate to a permanent guardrail in the relevant CLAUDE.md file.

---

### Orchestrator Self-Errors
| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| 1 | Phase 8-9 | Wrote state.md/decisions.md content as Bash() permission entries in settings.local.json | Orchestrator tried to write files using Bash heredoc, which got captured as permission allowlist entries | Deleted settings.local.json | "NEVER modify .claude/settings*.json. Your files are in .orchestrator/ only." |

### ECS Worker (code-worker on feature/ecs-core)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Spatial Worker (code-worker on feature/spatial-grid)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Render Worker (code-worker on feature/rendering)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| 1 | Phase 8 | raygui.h include path was hacky `_deps/raylib-src/examples/shapes` — broke MSVC build | raygui not added as CPM dependency; assumed it was bundled with raylib | Added `CPMAddPackage(raygui v4.0)`, used `${raygui_SOURCE_DIR}/src` (commit fa63385) | "Always add header-only libs as explicit CPM dependencies. Never use hacky paths into other packages' source trees." |

### Integration Worker (code-worker on main)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| 1 | Phase 9 | Cohesion steering used raw positional difference `(COM - position) * weight` instead of Reynolds canonical steering formula `(normalize(direction) * max_speed - current_velocity) * weight`. Force magnitude scaled with distance from center-of-mass causing oscillation. | Implementation followed simplified tutorial rather than Reynolds' GDC'99 specification. No code review caught it because boids "moved" — they just didn't flock naturally. | Must rewrite cohesion: normalize direction → multiply by max_speed → subtract current velocity → multiply by weight. (Phase 13 Task C1) | "ALL steering behaviors must compute `desired_velocity - current_velocity` as the steering force. The desired velocity must be `normalize(direction) * max_speed`. Never use raw positional differences as forces." |
| 2 | Phase 9 | Alignment steering did not normalize average velocity to max_speed. Computed `(avg_neighbor_velocity - current_velocity) * weight` directly. When neighbors are slow, alignment force weakens proportionally — exactly when it shouldn't. | Same root cause — simplified implementation. The canonical formula normalizes the average velocity *direction* and scales to max_speed so alignment always steers toward the flock's heading regardless of speed magnitude. | Must normalize average velocity direction → multiply by max_speed → then compute steering difference. (Phase 13 Task C2) | "Alignment desired velocity = `normalize(avg_neighbor_vel) * max_speed`. Never use raw averaged velocities — they produce speed-dependent alignment strength." |
| 3 | Phase 9 | No swarm-specific flocking — all boids align/cohere with ALL other boids regardless of swarm tag (NormalBoid, DoctorBoid, Antivax). Result: one merged super-flock instead of three distinct swarms. | Original integration task prompt said "implement boid flocking" without specifying swarm filtering. context.md at the time described two swarms but didn't explicitly state alignment/cohesion should be same-swarm only. | Must filter alignment and cohesion neighbors to same-swarm only. Separation stays cross-swarm. (Phase 13 Task C3) | "Separation = ALL boids. Alignment and Cohesion = SAME-SWARM ONLY. This is what creates visually distinct flocks. Always check swarm tag before including a neighbor in alignment/cohesion calculations." |

### Behavior/Sim Worker (code-worker on sim rules)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Ralph Loop Iterations

| # | Iteration | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-----------|-----------------|------------|-------------|-----------------|
| 1 | Phase 11 Antivax | Antivax implemented as additive tag `struct Antivax {}` added on top of `NormalBoid` entities. Antivax boids still classified as NormalBoid for infection, reproduction, flocking, and stats — not a distinct swarm. | Original context.md listed antivax under "Extensions to Behaviors" as "editing some of the boid rules for the normal swarm" — treated as a modifier, not a new entity type. | Must create `AntivaxBoid` as a primary tag (mutually exclusive with NormalBoid/DoctorBoid), with its own infection, reproduction, flocking, rendering, and stats tracking. (Phase 13 Tasks A1-A9) | "Each swarm must be a MUTUALLY EXCLUSIVE primary tag (NormalBoid OR DoctorBoid OR AntivaxBoid). Never use additive tags for swarm classification — it creates ambiguous entity identities." |

### Subagents (code-reviewer, debugger, ecs-architect, cpp-builder, changelog-scribe)

| # | Subagent | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|----------|-------|-----------------|------------|-------------|-----------------|
| 1 | sonnet (B2-B8 tests) | Phase 13 | Reproduction test used `reproduction_cooldown = 0.0f` with `p_offspring_normal = 1.0f` and `offspring_mean_normal = 3.0f` over 120 frames. Exponential population growth caused test to hang indefinitely. | Worker copied test parameters from plan.md spec literally without considering exponential growth dynamics. Zero cooldown means every offspring reproduces next frame: 3^120 → unbounded. | Changed `reproduction_cooldown = 60.0f` and frame count from 120 to 5. Applied same fix to NoCrossSwarm test. | "Reproduction tests MUST use non-zero cooldowns (≥60.0f) and minimal frame counts (≤10). Zero cooldown with p=1.0 causes exponential growth." |
| 2 | ecs-architect (C1-C3) | Phase 13 | Fixed max_force clamp in SteeringSystem (`if (force_mag > 1)` → `config.max_force`) but missed the identical bug in AntivaxSteeringSystem. Code review caught it. | Worker's task prompt only mentioned SteeringSystem. AntivaxSteeringSystem was a copy-paste of the same logic but wasn't in scope of C1-C3. | Applied same max_force fix to AntivaxSteeringSystem after code review flagged CRIT-1. | "When fixing a bug in one steering system, ALWAYS check ALL steering systems (SteeringSystem, AntivaxSteeringSystem, any future additions) for the same bug. Steering systems are often copy-pasted." |

---

## Cross-Cutting Patterns
<!-- Promote recurring mistakes here when they appear 3+ times across workers. -->
<!-- These become permanent guardrails in CLAUDE.md or worker prompt templates. -->

**PATTERN: Steering implementation deviates from Reynolds canonical model (3 occurrences — Integration Worker #1, #2, #3)**
- Guardrail: ALL steering behaviors MUST use `steering = (desired_velocity - current_velocity) * weight` where `desired_velocity = normalize(direction) * max_speed`. Raw positional differences and un-normalized averages are NEVER acceptable as steering forces.
- Guardrail: Separation = ALL boids. Alignment + Cohesion = SAME-SWARM ONLY.
