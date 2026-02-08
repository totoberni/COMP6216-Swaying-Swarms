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

## Phase 10 — Prerequisites (Bug Fix)

The Windows/MSVC build is broken and must be fixed before any Phase 10 work begins.

**Error:**
```
error C1083: Cannot open include file: 'raygui.h': No such file or directory
  [C:\Projects\COMP6216-Swaying-Swarms\build\boid_swarm.vcxproj]
```

**Diagnosis steps:**
1. Spawn a `debugger` subagent with the exact error text above.
2. Check `CMakeLists.txt` — is the CPM-fetched raygui target's include directory added to the boid_swarm target? (`target_include_directories` or similar)
3. Verify `raygui.h` is being downloaded: check `build/_deps/raygui-src/src/raygui.h` or equivalent.
4. If raygui is header-only, ensure one `.cpp` file defines `RAYGUI_IMPLEMENTATION` before including it.
5. Build must pass on BOTH PowerShell (`cmake --build build`) and WSL/MinGW (`cmake -B build && cmake --build build`).

**After fix:** Log the root cause and fix in `.orchestrator/mistakes.md` § "Integration Worker" or "Orchestrator Self-Errors" as appropriate. Log the decision in `decisions.md`. Then proceed to Phase 10.

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

**Goal:** Autonomously implement extensions using the Ralph Loop — each iteration gets a fresh context window, using files on disk as persistent memory.
**Who:** ORCHESTRATOR prepares infrastructure, then RALPH LOOP runs autonomously.
**Time:** Ongoing, autonomous.

### Step 11.0 — Orchestrator Prepares Ralph Infrastructure (CRITICAL)

Before the Ralph Loop can run, the orchestrator must create the supporting files. This step is executed by the orchestrator agent — not manually.

**Orchestrator prompt:**

```
Read .orchestrator/state.md and the master plan's Phase 11 extension requirements.
You must now prepare the Ralph Loop infrastructure. Execute the following in order:

1. CREATE docs/current-task.md:
   - Extract ALL uncompleted extension tasks from .orchestrator/state.md
   - Format them as a checklist with guardrails (see template below)
   - Read .orchestrator/mistakes.md § "Ralph Loop Iterations"
   - Append any "Prevention Rule" entries to the Guardrails section
   - This file becomes the Ralph Loop's fixed control input

2. CREATE ralph.sh at project root:
   - Stateless loop: fresh `claude -p` session per iteration
   - Reads docs/current-task.md as prompt
   - Detects RALPH_COMPLETE sentinel to stop
   - Logs iteration count and timestamps

3. VERIFY hook propagation:
   - Confirm .claude/settings.json has PostToolUse hooks registered
   - Confirm .claude/hooks/update-changelog.sh is executable
   - Confirm .claude/hooks/record-mistake.sh is executable
   - Confirm .claude/hooks/record-process.sh is executable

4. UPDATE .orchestrator/state.md:
   - Set current phase to "Phase 11 — Ralph Loop active"
   - Record ralph.sh creation in Completed Tasks

5. COMMIT: "chore(phase-11): create Ralph Loop infrastructure"

Do NOT start running the Ralph Loop yet — only prepare the files.
```

**Template for docs/current-task.md (orchestrator generates this):**

```markdown
# Current Task: Implement Extensions

Read CLAUDE.md for full project context. Check src/*/changelog.md for recent changes.

## Requirements (implement ONE per session, in order)
- [ ] Infected debuffs — Doctors: reduce p_cure ×0.5, r_interact_doctor ×0.7, p_offspring_doctor ×0.5. Normal: r_interact_normal ×0.8, p_offspring_normal ×0.5. Store debuff multipliers in SimConfig.
- [ ] Sex system: add Male/Female tags. 50/50 at spawn. Reproduction requires one Male + one Female. Same-sex collisions skip reproduction.
- [ ] Antivax boids: p_antivax percentage of Normal boids get Antivax tag at spawn. Antivax boids add a strong repulsion force from DoctorBoid within visual range (ADDITIVE to existing flocking, not replacement). They can still be cured if a doctor reaches them.
- [ ] Parameter sliders: raygui sliders in stats overlay for p_infect_normal, p_cure, r_interact_normal, r_interact_doctor, initial_normal_count, initial_doctor_count. Slider changes update SimConfig singleton in real-time.
- [ ] Pause/Reset controls: Pause button (freezes simulation, rendering continues). Reset button (destroys all entities, re-spawns from SimConfig).
- [ ] Population graph: real-time line chart (raygui or manual) showing normal_alive and doctor_alive over last 500 frames.

## Guardrails
- Do NOT break existing simulation rules
- Do NOT modify existing component struct fields — only ADD new components/fields
- All new parameters go in SimConfig
- Build and test after EACH change
- Update the relevant module's changelog.md
- Commit with descriptive message before finishing: "feat(scope): description"
- Use `<random>` with seeded engine, never `std::rand()`
- Antivax steering must be ADDITIVE to flocking rules, not a replacement
- Do NOT add Raylib includes outside src/render/
- If all tasks checked, output RALPH_COMPLETE
```

**Template for ralph.sh (orchestrator generates this):**

```bash
#!/bin/bash
# ralph.sh — Stateless development loop. Each iteration = fresh context.
# Created by orchestrator in Phase 11, Step 11.0
set -euo pipefail

PROMPT="docs/current-task.md"
MAX_ITERATIONS=30
ITERATION=0
LOG="docs/ralph-log.md"

# Initialize log
echo "# Ralph Loop Log" > "$LOG"
echo "Started: $(date -u '+%Y-%m-%d %H:%MZ')" >> "$LOG"
echo "" >> "$LOG"

while [ $ITERATION -lt $MAX_ITERATIONS ]; do
  ITERATION=$((ITERATION + 1))
  START_TIME=$(date -u '+%H:%M:%SZ')
  echo "=== Ralph iteration $ITERATION ($START_TIME) ==="

  OUTPUT=$(claude -p "$(cat "$PROMPT")

Read CLAUDE.md for project context. Check src/*/changelog.md for recent changes by other sessions.
Read .claude/skills/flecs-patterns/SKILL.md for domain patterns and parameter reference.

Find the NEXT UNCHECKED task in the requirements list above.
Implement ONLY that single task.
Build the project. Run tests. Fix any failures.
Update the relevant changelog.md files.
Mark the task as checked in docs/current-task.md by changing [ ] to [x].
Commit with a descriptive message: 'feat(scope): description'

If you encounter a repeated mistake or anti-pattern, record it:
Run: echo 'MISTAKE: [description]' so the orchestrator can add a guardrail.

If ALL tasks are checked, output RALPH_COMPLETE.
Do NOT work on more than one task per session." 2>&1)

  END_TIME=$(date -u '+%H:%M:%SZ')
  echo "$OUTPUT" | tail -5

  # Log iteration
  echo "## Iteration $ITERATION" >> "$LOG"
  echo "- Start: $START_TIME | End: $END_TIME" >> "$LOG"
  echo "- Output (last 3 lines):" >> "$LOG"
  echo "$OUTPUT" | tail -3 | sed 's/^/  /' >> "$LOG"
  echo "" >> "$LOG"

  # Check for mistakes to escalate
  if echo "$OUTPUT" | grep -q "MISTAKE:"; then
    MISTAKE=$(echo "$OUTPUT" | grep "MISTAKE:" | head -1)
    MISTAKE_DESC="${MISTAKE#MISTAKE: }"
    echo "⚠ Detected mistake: $MISTAKE_DESC"
    echo "- ⚠ $MISTAKE_DESC" >> "$LOG"
    # Append as guardrail to prevent repetition
    echo "- $MISTAKE_DESC" >> docs/current-task.md
    # Record in .orchestrator/mistakes.md
    echo "{\"worker\":\"ralph\",\"phase\":\"11\",\"what\":\"$MISTAKE_DESC\",\"cause\":\"Ralph iteration $ITERATION\",\"fix\":\"Added guardrail to current-task.md\",\"rule\":\"$MISTAKE_DESC\"}" \
      | .claude/hooks/record-mistake.sh 2>/dev/null || true
  fi

  if echo "$OUTPUT" | grep -q "RALPH_COMPLETE"; then
    echo "=== All tasks complete after $ITERATION iterations. ==="
    echo "" >> "$LOG"
    echo "## COMPLETE" >> "$LOG"
    echo "All tasks done after $ITERATION iterations at $(date -u '+%H:%M:%SZ')" >> "$LOG"
    break
  fi

  echo "--- Iteration $ITERATION done. Fresh session starting... ---"
  sleep 3
done

echo "Ralph loop finished. $ITERATION iterations completed."
```

### Step 11.1 — Run the Ralph Loop

```bash
chmod +x ralph.sh
./ralph.sh
```

**What happens each iteration:**
1. Fresh Claude session starts (clean 200K context window)
2. Reads CLAUDE.md + @imports (project context, orchestrator state, changelogs)
3. Reads `.claude/skills/flecs-patterns/SKILL.md` (domain knowledge on demand)
4. Reads `docs/current-task.md`
5. Finds next unchecked requirement
6. Implements it, builds, tests, commits
7. Marks task complete in the file
8. Exits. Loop starts fresh with zero context rot.

**If Ralph makes a repeating mistake**, the loop auto-detects `MISTAKE:` sentinels and appends guardrails. You can also manually add guardrails:

```markdown
## Guardrails
- ... existing ...
- NEW: The antivax steering must be ADDITIVE to existing flocking rules, not a replacement
```

### Step 11.2 — Orchestrator-Managed Ralph (Advanced)

For orchestrator-supervised Ralph, the orchestrator manages each iteration and updates state:

```bash
tmux new-session -s orchestrator
cd COMP6216-Swaying-Swarms
claude
```

> Note: Use plain `claude` (not `--agent orchestrator`) so the root session can spawn subagents via the Task tool.

```
Manage the Ralph Loop for extensions. For each iteration:
1. Read .orchestrator/state.md for current progress
2. Read docs/current-task.md to find the next unchecked task
3. Spawn a worker: claude -p with the task prompt, --model sonnet
4. Capture output, check for success or MISTAKE sentinels
5. Use /record-process to update state.md with progress
6. If the worker fails or reports MISTAKE, use /record-mistake to log it, add a guardrail to docs/current-task.md, retry
7. Continue until all tasks are complete or MAX_ITERATIONS reached
```

### Step 11.3 — Worker Hook & Skill Propagation

Workers spawned by `ralph.sh` or the orchestrator automatically inherit hooks and skills because they run in the same project directory. Verify this checklist:

- [ ] `.claude/settings.json` has all hooks registered (changelog, record-mistake, record-process)
- [ ] `.claude/hooks/*.sh` are all executable (`chmod +x`)
- [ ] `.claude/skills/flecs-patterns/SKILL.md` exists with full parameter reference
- [ ] Workers are spawned with `--model sonnet` for implementation work
- [ ] `docs/current-task.md` references the skill: "Read .claude/skills/flecs-patterns/SKILL.md"

For worktree-based workers, hooks propagate via the shared `.claude/` directory since worktrees link to the same git repo. If a worktree doesn't pick up hooks, verify with:

```bash
ls -la .claude/hooks/  # Should show all hook scripts
cat .claude/settings.json | jq '.hooks'  # Should show all registrations
```

**✅ Phase 11 complete** when all extension tasks are checked in `docs/current-task.md`.

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