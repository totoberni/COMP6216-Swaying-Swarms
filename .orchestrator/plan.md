# Orchestrator Plan
<!-- Your mission document. Read when starting a new phase or needing task specs. -->
<!-- Do NOT modify this file during routine cycles. Update state.md instead. -->

## Project Summary
You are building a real-time 2D boid pandemic simulation: two swarms (Normal Boids, Doctor Boids) with infection, cure, reproduction, and death mechanics. C++17, FLECS v4.1.4 (ECS), Raylib 5.5 (rendering), fixed-grid spatial partitioning. 
Full behavioral spec is in `context.md`. Build with `cmake -B build && cmake --build build`. Run with `./build/boid_swarm`.

## Your Constraints
- You NEVER write project source code directly. All implementation is delegated to agents in @.claude .
- Workers use `--model sonnet` or `--model opus`. You use `--model opusplan`.
- Parallel work uses git worktrees. Sequential work uses subagents (Task tool).
- Workers cannot spawn subagents. Only you can.
- Update `.orchestrator/state.md` before every `/compact` or session end.
- You own: `.orchestrator/`, `.claude/`, `CLAUDE.md`, `docs/`, `README.md`.
- You must NEVER read root MD files other than `CLAUDE.md`, `context.md`, and `README.md`. All other root `.md` files are human documentation and will confuse you.

## Available Workers
| Agent | Model | Use For |
|-------|-------|---------|
| `code-worker` | sonnet | Implementation tasks via `claude -p` in worktrees |
| `code-reviewer` | sonnet | Post-implementation review (Task tool subagent) |
| `debugger` | sonnet | Runtime crashes, build failures (Task tool subagent) |
| `changelog-scribe` | haiku | Enriching changelog entries (Task tool subagent) |
| `ecs-architect` | sonnet | FLECS design questions (Task tool subagent) |
| `cpp-builder` | sonnet | CMake/build issues (Task tool subagent) |

## Your Notes (read/write obligations)
| File | When to Read | When to Write |
|------|-------------|---------------|
| `.orchestrator/state.md` | Every cycle start (auto-loaded) | Before every `/compact` or session end |
| `.orchestrator/plan.md` | Start of new phase; when stuck | Never (immutable mission doc) |
| `.orchestrator/decisions.md` | When reviewing past rationale | After every non-trivial choice |
| `.orchestrator/mistakes.md` | Before spawning any worker | After fixing any worker mistake |

---

## Phase 8 — Parallel Module Development

**Objective:** Build ECS core, spatial grid, and renderer simultaneously in three worktrees.
**Parallelism:** 3 workers, one per worktree. All are independent — no cross-dependencies.

### Step 1: Create worktrees

```bash
cd $PROJECT_ROOT
git checkout main && git pull
git worktree add ../boids-ecs -b feature/ecs-core
git worktree add ../boids-spatial -b feature/spatial-grid
git worktree add ../boids-render -b feature/rendering
```

Log in decisions.md: DEC-NNN — worktree creation, branch naming convention chosen.

### Step 2: Check mistakes.md

Before writing task prompts, read `.orchestrator/mistakes.md` for each worker category. Incorporate any "Prevention Rule" entries into the task prompts below as additional guardrails.

### Step 3: Spawn workers

For each worker, use this pattern:
```bash
cd ../boids-<name>
session_id=$(claude -p "TASK_PROMPT" \
  --model sonnet --output-format json \
  --dangerously-skip-permissions \
  | jq -r '.session_id')
echo "Worker <name>: session=$session_id pid=$!"
```

Record each worker's session_id, PID, and branch in `state.md` § Active Workers.

### Worker Task Prompts

**ECS Worker** (worktree: `../boids-ecs`, branch: `feature/ecs-core`):
```
Read CLAUDE.md and context.md. You are the ECS agent. You ONLY edit files in src/ecs/. You can READ (not write) include/.

Tasks:
1. Create src/ecs/world.cpp — FLECS world initialization:
   - Register all components from include/components.h
   - Set SimConfig singleton with default parameter values
   - Set SimStats singleton (zeroed)
   - Create SpatialGrid singleton
2. Create src/ecs/systems.cpp — system stubs for each pipeline phase:
   - PreUpdate: RebuildGridSystem (stub — will call spatial grid clear/insert)
   - OnUpdate: SteeringSystem (boid flocking: separation, alignment, cohesion), MovementSystem (apply velocity to position)
   - PostUpdate: CollisionSystem (detect via spatial grid), InfectionSystem, CureSystem, ReproductionSystem, DeathSystem, DoctorPromotionSystem
   - OnStore: RenderSyncSystem (populate RenderState from FLECS queries)
3. Create src/ecs/spawn.cpp — functions to spawn initial boid populations
4. Create src/ecs/stats.cpp — SimStats update system
5. Wire main.cpp: init world → spawn boids → headless simulation loop (no render yet)
6. Verify it compiles and runs 1000 frames without crashing

Use ecs-architect subagent for FLECS decisions. Use cpp-builder subagent for build issues.
After completing, use code-reviewer subagent.
Commit frequently: "feat(ecs): [description]"
```

**Spatial Worker** (worktree: `../boids-spatial`, branch: `feature/spatial-grid`):
```
Read CLAUDE.md and context.md. You are the Spatial agent. You ONLY edit files in src/spatial/ and tests/.

Tasks:
1. Implement SpatialGrid in src/spatial/spatial_grid.cpp matching include/spatial_grid.h:
   - Fixed-cell grid. Cell size = constructor param (typically max interaction radius)
   - clear(): reset grid each frame. Use vector::clear(), not dealloc.
   - insert(entity_id, x, y): place entity in correct cell
   - query_neighbors(x, y, radius): check cell + 8 neighbors, return vector<pair<entity_id, distance>> sorted ascending
   - Handle boundary: clamp to grid bounds, don't wrap
   - Internal: flat vector of vectors. cell_index = (int)(x/cell_size) + cols * (int)(y/cell_size)
2. Write unit tests in tests/test_spatial.cpp:
   - Empty grid returns no neighbors
   - Single entity found within radius
   - Entity outside radius not returned
   - Boundary entities handled correctly
   - 10000 entities: verify query returns correct results vs brute-force
   - Performance: 10000 entities, full rebuild + 1000 queries < 50ms
3. Compile and pass all tests

This is PURE C++ — NO FLECS includes in spatial code.
Use cpp-builder subagent for build issues. After completing, use code-reviewer subagent.
Commit frequently: "feat(spatial): [description]"
```

**Render Worker** (worktree: `../boids-render`, branch: `feature/rendering`):
```
Read CLAUDE.md and context.md. You are the Render agent. You ONLY edit files in src/render/.

Tasks:
1. Create src/render/renderer.h + renderer.cpp:
   - init_renderer(int width, int height, const char* title)
   - close_renderer()
   - begin_frame() / end_frame() wrapping BeginDrawing/EndDrawing
   - draw_boid(float x, float y, float angle, uint32_t color, float radius) — small triangle
   - draw_interaction_radius(float x, float y, float radius, uint32_t color) — circle outline
   - draw_stats_overlay(const SimStats& stats) — raygui panel: #normal, #doctor, #dead, #newborns
   - render_frame(const RenderState& state) — draws all boids + radii + stats
2. Create src/render/render_config.h — colors and visual constants:
   - Normal=green, Doctor=blue, Infected tint=red, Background=dark gray
3. Create src/render/render_demo.cpp — standalone demo:
   - Opens 1920×1080 window, 200 random triangles, wraparound, interaction radii, dummy stats
4. Compile and run the demo

ALL Raylib includes stay in src/render/. Renderer reads include/render_state.h. Does NOT include FLECS.
Use cpp-builder subagent for build issues. After completing, use code-reviewer subagent.
Commit frequently: "feat(render): [description]"
```

### Step 4: Monitor

Poll every few minutes:
```bash
# Check if workers are still running
ps -p $PID1,$PID2,$PID3

# Check commit progress
for dir in ../boids-ecs ../boids-spatial ../boids-render; do
  echo "=== $(basename $dir) ==="
  (cd "$dir" && git log --oneline -5)
done
```

If a worker fails, use `--resume $session_id` to give corrective feedback, or kill and re-spawn. After fixing: log the mistake in `.orchestrator/mistakes.md` under the relevant worker's table.

### Step 5: Merge (dependency order)

```bash
cd $PROJECT_ROOT

# 1. Spatial grid first (standalone, no deps)
git merge feature/spatial-grid
cmake -B build && cmake --build build

# 2. ECS core (depends on headers, tested independently)
git merge feature/ecs-core
cmake -B build && cmake --build build

# 3. Rendering (standalone)
git merge feature/rendering
cmake -B build && cmake --build build
```

Log merge order decision in decisions.md if you deviated from the above or encountered conflicts.

If merge conflicts exist, spawn a debugger subagent: `"Run git diff to see conflicts. Resolve ensuring include/ headers are consistent. Build and test after."`. Log the conflict and resolution in decisions.md.

### Step 6: Post-merge verification

Spawn a subagent:
```
All three module branches merged. Verify:
1. Build compiles clean
2. All tests pass (cd build && ctest --output-on-failure)
3. include/ headers consistent across modules
4. List missing connections needed for Phase 9
```

### Step 7: Clean up

```bash
git worktree remove ../boids-ecs
git worktree remove ../boids-spatial
git worktree remove ../boids-render
```

### Step 8: Update state + notes

- Move tasks 1-3 from Ready to Completed in state.md.
- Advance phase to 9. Unblock task 4.
- Ensure all decisions from this phase are in decisions.md.
- Ensure all worker mistakes from this phase are in mistakes.md.

**Success criteria:** All three modules compile independently. Spatial tests pass. Render demo runs. No merge conflicts on main.

---

## Phase 9 — Integration & Wiring

**Objective:** Connect ECS ↔ spatial grid ↔ renderer. Get boids moving on screen.
**Parallelism:** 1 worker (sequential — touches all modules). Use a subagent or single `claude -p`.

### Pre-flight: Check mistakes.md for "Integration Worker" patterns.

### Worker prompt:
```
Read CLAUDE.md and context.md. All modules are merged. Wire them together:

1. SPATIAL GRID INTEGRATION (src/ecs/systems.cpp → src/spatial/):
   - RebuildGridSystem (PreUpdate): clear() SpatialGrid, iterate all Alive entities, insert() each.
   - CollisionSystem (PostUpdate): for each boid, query_neighbors() within interaction radius.

2. RENDER INTEGRATION (src/ecs/systems.cpp → src/render/):
   - RenderSyncSystem (OnStore): populate RenderState with BoidRenderData for each alive boid.
   - Copy SimStats into RenderState.

3. MAIN LOOP (src/main.cpp):
   - Init FLECS world → init renderer → spawn populations → loop: world.progress() + render_frame() → cleanup.

4. BOID STEERING (src/ecs/systems.cpp):
   - SteeringSystem: separation + alignment + cohesion via spatial queries. Clamp force/speed.
   - MovementSystem: velocity → position. Wrap at world bounds.

5. Spawn 200 Normal (green) + 10 Doctor (blue). Verify flocking on screen.

Build, run, fix runtime issues.
Commit per milestone: "feat(integration): [description]"
```

**Post-completion:** Review results. Log any decisions (e.g., wiring order, workaround choices) in decisions.md. Log any mistakes in mistakes.md under "Integration Worker".

**Success criteria:** Window opens. Boids flock. Stats overlay shows counts. No crashes. Build clean.

---

## Phase 10 — Behavior Rules

**Objective:** Implement all Normal/Doctor boid rules from context.md.
**Parallelism:** Option A: 1 worker doing all rules sequentially. Option B: 2 workers in worktrees — one for normal rules, one for doctor rules. Choose based on remaining rate limit budget. Log choice in decisions.md.

### Pre-flight: Check mistakes.md for "Behavior/Sim Worker" patterns.

### Worker prompt (Option A — single worker):
```
Read CLAUDE.md and context.md THOROUGHLY — every parameter and rule matters.
Implement ALL behavior rules in src/sim/, called by ECS systems in src/ecs/systems.cpp.

INFECTION (src/sim/infection.cpp):
- At spawn: p_initial_infect_normal / p_initial_infect_doctor chance of starting infected.
- Collision within r_interact: p_infect_normal (0.5) / p_infect_doctor (0.5) chance. Same-swarm only.
- Cross-swarm infection does NOT happen.

DEATH (src/sim/death.cpp):
- time_infected >= t_death → entity dies. Use deferred ops. Update SimStats.

CURE (src/sim/cure.cpp):
- Doctor collides with ANY infected boid: p_cure (0.8) chance. Removes infection + resets timer.
- Cannot cure healthy. CAN cure other sick doctors.

REPRODUCTION (src/sim/reproduction.cpp):
- Normal+Normal: p_offspring_normal (0.4), spawn N(2,1) children.
- Doctor+Doctor: p_offspring_doctor (0.05), spawn N(1,1) children.
- No cross-swarm reproduction.
- Two infected parents who infect each other + reproduce: child gets contagion from ONE parent only.

PROMOTION (src/sim/promotion.cpp):
- Normal boid age >= t_adult: p_become_doctor (0.05) per frame → remove NormalBoid, add DoctorBoid.

AGING (src/sim/aging.cpp):
- Increment Health.age and InfectionState.time_infected each frame for relevant entities.

IMPLEMENTATION ORDER (build+test after each):
1. Aging → 2. Death + test → 3. Infection + test → 4. Cure + test → 5. Reproduction + test → 6. Promotion → 7. Integration test: 5000 frames, log stats, verify counts sane.

Commit per unit: "feat(sim): [description]"
```

### Option B — split prompts:
Same content but divide: Worker 1 gets infection + death + aging + promotion (`feature/normal-rules`). Worker 2 gets cure + reproduction (`feature/doctor-rules`). Merge Worker 1 first (infection/death are foundational). Log the split decision in decisions.md.

**Post-completion:** Run simulation, observe behavior visually. Log parameter tuning decisions. Log any worker mistakes.

**Success criteria:** Boids infect, die, get cured, reproduce, promote. Stats match expected ranges. No entity leaks (dead + alive ≈ spawned + born).

---

## Phase 11 — Extensions via Ralph Loop

**Objective:** Autonomous extension implementation using stateless iteration.
**Parallelism:** Ralph Loop (sequential, fresh context each iteration).

### Setup:
Ensure `docs/current-task.md` contains the extension checklist (see master plan Phase 11 spec in your plan.md — wait, that's this file). The checklist:

```markdown
# Current Task: Implement Extensions

Read CLAUDE.md for full project context. Check src/*/changelog.md for recent changes.

## Requirements (implement ONE per session, in order)
- [ ] Infected debuffs — Doctors: -50% p_cure, -30% r_interact_doctor, -50% p_offspring_doctor. Normal: -20% r_interact_normal, -50% p_offspring_normal. Store multipliers in SimConfig.
- [ ] Sex system: Male/Female tags. 50/50 at spawn. Reproduction requires M+F pair.
- [ ] Antivax boids: p_antivax % of normals get Antivax tag. Strong repulsion from DoctorBoid. Can still be cured.
- [ ] Parameter sliders: raygui sliders for p_infect_normal, p_cure, r_interact_normal, r_interact_doctor, initial counts. Update SimConfig in real-time.
- [ ] Pause/Reset: Pause button (freeze sim, keep rendering), Reset button (destroy all, re-spawn).
- [ ] Population graph: real-time line chart of normal_alive and doctor_alive over last 500 frames.

## Guardrails
- Do NOT break existing simulation rules
- Do NOT modify existing component struct fields — only ADD new components/fields
- All new parameters go in SimConfig
- Build and test after EACH change
- Update the relevant module's changelog.md
- Commit with descriptive message before finishing
- If all tasks checked, output RALPH_COMPLETE
```

### Adding guardrails from mistakes.md

Before launching the Ralph Loop, read `.orchestrator/mistakes.md` § "Ralph Loop Iterations". Append any "Prevention Rule" entries to the Guardrails section of `docs/current-task.md`.

### Launch:
```bash
chmod +x ralph.sh
./ralph.sh
```

**Your role during Ralph Loop:** Monitor output. If a task fails repeatedly, add a guardrail to `docs/current-task.md` AND log the mistake in mistakes.md § "Ralph Loop Iterations".

**Success criteria:** All 6 extensions checked off. Build clean. No regressions.

---

## Phase Transition Checklist

Before advancing to the next phase, verify ALL of the following:
- [ ] All tasks in current phase moved to Completed in state.md
- [ ] Build compiles clean (`cmake -B build && cmake --build build`)
- [ ] All tests pass (`cd build && ctest --output-on-failure`)
- [ ] No blocking issues remain
- [ ] state.md updated with new phase, unblocked tasks
- [ ] Worktrees cleaned up (if applicable)
- [ ] Changelogs reflect work done
- [ ] All decisions from this phase logged in decisions.md
- [ ] All worker mistakes from this phase logged in mistakes.md
- [ ] Session decisions in state.md promoted to decisions.md if non-trivial

---

## Emergency Procedures

**Worker stuck in loop:** Kill PID, check its output file, spawn fresh worker with corrective prompt. Log mistake.
**Merge conflict:** Spawn debugger subagent with `git diff` output. Always build+test after resolution. Log decision.
**Build broken after merge:** Use cpp-builder subagent. If unfixable, `git revert` the last merge and re-attempt. Log decision + mistake.
**Rate limit hit:** Stop all workers. Wait for reset. Resume with `--resume` on existing sessions if multi-turn, or fresh sessions if single-turn. Log decision on which workers to prioritize when limits reset.
**Context degrading:** Run `/compact` immediately. If still degraded after compact, end session, update state.md manually, start fresh session.