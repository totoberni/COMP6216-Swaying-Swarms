# Orchestrator Plan — Phase 13: Swarm Refinement & Steering Fixes

> **Replaces** the previous plan.md. Phases 1–12 are complete (see `state.md` for history).
> This plan covers: (A) Antivax separate swarm creation, (B) Doctor self-cure prevention, (C) Critical steering model fixes, (D) Future extensions preserved for later.

---

## Guiding Principles

1. **Antivax swarm creation is the priority.** This is the assigned team deliverable. Complete it before touching steering math so teammates can collaborate on the main branch without merge conflicts from structural rewrites.
2. **Steering fixes come second.** They are critical for visual correctness but are self-contained in `src/ecs/systems.cpp` and won't block teammates.
3. **Every task is atomic.** One task = one worker session = one commit. Tasks are ordered so each builds on the previous output with no circular dependencies.
4. **Record structural mistakes.** The steering bugs and swarm-flocking omission are architectural errors that must be logged in `mistakes.md` to prevent recurrence.

---

## Pre-Flight: Mistake Recording (Orchestrator — Do This First)

Before delegating any tasks, the orchestrator MUST record the following structural issues in `.orchestrator/mistakes.md` to establish the prevention rules that all workers will carry forward.

### Mistakes to Record

**1. Cohesion steering used raw positional difference instead of Reynolds steering formula**
- Worker: Integration Worker
- Phase: 9
- What: Cohesion force computed as `(COM - position) * weight` — a raw positional offset — instead of the canonical `(normalize(COM - position) * max_speed - current_velocity) * weight`. Force magnitude scaled with distance from center-of-mass.
- Cause: Implementation followed a simplified tutorial rather than Reynolds' GDC'99 specification. No code review caught it because boids "moved" — they just didn't flock naturally.
- Fix: Must rewrite cohesion to normalize direction → multiply by max_speed → subtract current velocity → multiply by weight.
- Prevention Rule: "ALL steering behaviors must compute `desired_velocity - current_velocity` as the steering force. The desired velocity must be `normalize(direction) * max_speed`. Never use raw positional differences as forces."

**2. Alignment steering did not normalize average velocity to max_speed**
- Worker: Integration Worker
- Phase: 9
- What: Alignment computed `(avg_neighbor_velocity - current_velocity) * weight`. The average velocity was NOT normalized to max_speed before computing the difference. When neighbors are slow, alignment force weakens proportionally.
- Cause: Same root cause — simplified implementation. The canonical formula normalizes the average velocity *direction* and scales to max_speed so that alignment always steers toward the flock's heading regardless of speed magnitude.
- Fix: Must normalize average velocity direction → multiply by max_speed → then compute steering difference.
- Prevention Rule: "Alignment desired velocity = normalize(avg_neighbor_vel) * max_speed. Never use raw averaged velocities — they produce speed-dependent alignment strength."

**3. No swarm-specific flocking — all boids align/cohere with all other boids**
- Worker: Integration Worker
- Phase: 9
- What: The SteeringSystem applies separation, alignment, and cohesion to ALL neighbors regardless of swarm tag (NormalBoid, DoctorBoid, Antivax). This means all swarms merge into one super-flock instead of forming distinct groups.
- Cause: The original integration task prompt said "implement boid flocking" without specifying swarm filtering. The context.md at the time described two swarms but didn't explicitly state that alignment/cohesion should be same-swarm only.
- Fix: Filter alignment and cohesion neighbors to same-swarm. Separation stays cross-swarm (prevents collisions between different swarms).
- Prevention Rule: "Separation = ALL boids. Alignment and Cohesion = SAME-SWARM ONLY. This is what creates visually distinct flocks. Always check swarm tag before including a neighbor in alignment/cohesion calculations."

**4. Antivax implemented as additive tag instead of separate swarm**
- Worker: Ralph Loop (Phase 11)
- Phase: 11
- What: Antivax was implemented as a tag component (`struct Antivax {}`) added on top of `NormalBoid` entities. This means antivax boids are still classified as NormalBoid for infection, reproduction, flocking, and stats purposes — they're not a distinct swarm.
- Cause: The original context.md listed antivax under "Extensions to Behaviors" as "editing some of the boid rules for the normal swarm" — it was treated as a modifier, not a new entity type.
- Fix: Create `AntivaxBoid` as a primary tag (mutually exclusive with NormalBoid/DoctorBoid), with its own infection, reproduction, flocking, rendering, and stats tracking.
- Prevention Rule: "Each swarm must be a MUTUALLY EXCLUSIVE primary tag (NormalBoid OR DoctorBoid OR AntivaxBoid). Never use additive tags for swarm classification — it creates ambiguous entity identities."

Use the `/record-mistake` command or pipe JSON to `.claude/hooks/record-mistake.sh` for each entry.

---

## Part A: Antivax Separate Swarm (Tasks A1–A9)

**Objective:** Promote Antivax from an additive tag on NormalBoid to a fully independent third swarm (`AntivaxBoid`) with its own flocking, infection, reproduction, cure interactions, rendering, and stats tracking.

**Dependency chain:** A1 → A2 → A3 (can parallelize A4,A5,A6 after A3) → A7 → A8 → A9

### Task A1: Component & Stats Infrastructure

**Files:** `include/components.h`, `src/ecs/world.cpp`

**Changes:**
1. In `include/components.h`:
   - Keep `struct Antivax {};` temporarily (removed in A9 after migration)
   - Add `struct AntivaxBoid {};` as a new primary swarm tag, documented as mutually exclusive with NormalBoid and DoctorBoid
   - Add to `SimStats`: `int antivax_alive = 0;`, `int dead_antivax = 0;`, `int newborns_antivax = 0;`
   - Add to `SimStats::PopulationHistoryPoint`: `int antivax_alive = 0;`
2. In `src/ecs/world.cpp`:
   - Register `AntivaxBoid` component: `world.component<AntivaxBoid>();`

**Commit:** `feat(ecs): add AntivaxBoid primary tag and antivax stats fields`

**Success criteria:** Build compiles clean. All 23 existing tests pass. No runtime changes yet.

---

### Task A2: Spawn System — Create Antivax from Normal Pool

**Files:** `src/ecs/spawn.cpp` (or wherever initial population spawning lives), `src/ecs/systems.cpp` (if transition logic exists there)

**Changes:**
1. During initial population spawning for Normal boids:
   - For each would-be NormalBoid, roll `p_antivax` (default: 0.1)
   - If the roll succeeds: tag entity with `AntivaxBoid` instead of `NormalBoid`. Do NOT also add `NormalBoid` — they are mutually exclusive.
   - If the roll fails: tag entity with `NormalBoid` as before
2. Remove the old `Antivax` tag assignment logic (the Phase 11 implementation that added `Antivax` on top of `NormalBoid`)
3. Offspring born to Antivax parents (Task A5) will be `AntivaxBoid` — but that wiring happens in A5. For now, only initial spawn matters.
4. On reset (SimulationState.reset_requested): ensure antivax boids are also destroyed and respawned correctly

**Decision for orchestrator:** The old `struct Antivax {};` tag stays temporarily. NO entity should have both `Antivax` and `AntivaxBoid`. Once A2 is complete, `Antivax` is dead code — removed in A9.

**Commit:** `feat(ecs): spawn AntivaxBoid as separate swarm from normal pool`

**Success criteria:** Build clean. Running simulation spawns ~20 antivax boids (10% of 200). They exist as separate entities from NormalBoid. Print or log counts to verify.

---

### Task A3: Rendering — Distinct Antivax Color

**Files:** `src/ecs/systems.cpp` (RenderSyncSystem), `src/render/renderer.cpp` (if color logic lives there), `include/render_state.h` (if BoidRenderData needs update)

**Changes:**
1. In `BoidRenderData`: ensure there's a way to distinguish Normal, Doctor, and Antivax. Currently `is_doctor` is a bool — either:
   - Option A (preferred): Replace `bool is_doctor` with an enum or int `swarm_type` (0=normal, 1=doctor, 2=antivax)
   - Option B: Add `bool is_antivax` alongside `is_doctor`
2. In `RenderSyncSystem`: when iterating alive boids, check for `AntivaxBoid` tag and set color accordingly:
   - Normal = green (existing)
   - Doctor = blue (existing)
   - Antivax = orange or yellow (must be visually distinct from infected-red-tint)
3. In the renderer's draw loop: use swarm_type to select base color, then apply infected red tint on top

**Commit:** `feat(render): render antivax boids in distinct orange color`

**Success criteria:** Running simulation shows three visually distinct boid colors.

---

### Task A4: Infection — Antivax×Antivax Only

**Files:** `src/ecs/systems.cpp` (InfectionSystem)

**Changes:**
1. Add a third infection block (alongside the Normal and Doctor blocks):
   - Iterate all `AntivaxBoid` + `Alive` + `Infected` entities
   - For each, query neighbors within `r_interact_normal` (antivax uses normal interaction params)
   - Only infect neighbors that have `AntivaxBoid` tag (same-swarm only)
   - Use `p_infect_normal` probability and debuffs `debuff_r_interact_normal_infected` when infected
2. Verify: cross-swarm infection does NOT happen
3. At spawn: AntivaxBoid uses `p_initial_infect_normal` for initial infection chance

**Commit:** `feat(sim): wire antivax-to-antivax infection (same-swarm only)`

**Success criteria:** Build clean. Antivax boids can infect each other. No cross-swarm antivax infection.

---

### Task A5: Reproduction — Antivax×Antivax Only

**Files:** `src/ecs/systems.cpp` (ReproductionSystem)

**Changes:**
1. Add a third reproduction block:
   - Iterate `AntivaxBoid` + `Alive` entities for collision-based reproduction
   - Only reproduce with other `AntivaxBoid` entities (same-swarm)
   - Use Normal reproduction parameters: `p_offspring_normal`, `offspring_mean_normal` (2.0), `offspring_stddev_normal` (1.0)
   - Apply debuff `debuff_p_offspring_normal_infected` when parent is infected
   - Sex system: require Male + Female pair
   - Cooldown: `reproduction_cooldown`
   - Offspring are tagged `AntivaxBoid` (NOT NormalBoid — they inherit parent swarm)
2. Two infected Antivax parents producing offspring: child gets contagion from ONE parent only
3. Increment `SimStats.newborns_antivax` and `newborns_total`

**Commit:** `feat(sim): wire antivax reproduction (same-swarm, offspring inherit AntivaxBoid)`

**Success criteria:** Build clean. Antivax population can grow through reproduction.

---

### Task A6: Cure — Doctors Can Cure Antivax

**Files:** `src/ecs/systems.cpp` (CureSystem)

**Changes:**
1. The existing CureSystem already cures "ANY infected boid" — verify that it checks `ne.has<Infected>()` without filtering to NormalBoid/DoctorBoid only
2. If the cure system explicitly checks for NormalBoid or DoctorBoid tags before curing, add `AntivaxBoid` to the accepted targets
3. Most likely NO code change is needed — but this MUST be verified by reading the code
4. Also verify: doctors CANNOT cure themselves (self-ID skip `nid == e.id()`). Add a code comment making this contract explicit.

**Commit:** `feat(sim): verify and document doctor-cures-antivax + no-self-cure contract`

**Success criteria:** Infected antivax boids near a doctor can be cured. Doctor self-cure does not happen.

---

### Task A7: Stats Tracking — Antivax Counters

**Files:** `src/ecs/systems.cpp` (stats update system), `src/render/renderer.cpp` (stats overlay)

**Changes:**
1. In the stats counting system:
   - Add count for `AntivaxBoid` + `Alive` entities → `stats.antivax_alive`
   - Include antivax in population history
2. In death system: when an AntivaxBoid dies, increment `stats.dead_antivax` and `stats.dead_total`
3. In the renderer stats panel: add Antivax Alive, Dead Antivax, Newborns Antivax rows
4. In the population graph: add an orange/yellow line for antivax

**Commit:** `feat(render): add antivax stats tracking and population graph line`

**Success criteria:** Stats panel shows antivax counts. Population graph has three lines.

---

### Task A8: Antivax Doctor-Avoidance Steering

**Files:** `src/ecs/systems.cpp` (register_antivax_steering_system)

**Changes:**
1. Modify `register_antivax_steering_system` to query `AntivaxBoid` tag instead of old `Antivax` + `NormalBoid` combination
2. Verify the repulsion logic uses `antivax_repulsion_radius` and `antivax_repulsion_weight`
3. Confirm force is ADDITIVE to existing flocking forces

**Commit:** `feat(ecs): update antivax steering to use AntivaxBoid primary tag`

**Success criteria:** Antivax boids visibly flee from nearby doctors.

---

### Task A9: Cleanup & Migration Complete

**Files:** `include/components.h`, `src/ecs/world.cpp`, any remaining references

**Changes:**
1. Remove `struct Antivax {};` from `components.h`
2. Remove `world.component<Antivax>();` from `world.cpp`
3. Grep entire codebase for remaining references to old `Antivax` tag and remove
4. Full build + all tests

**Commit:** `refactor(ecs): remove deprecated Antivax tag, migration to AntivaxBoid complete`

**Success criteria:** Build clean. All tests pass. No references to old `Antivax` tag remain.

---

## Part B: Doctor Self-Cure Prevention (Task B1)

### Task B1: Verify and Test Doctor No-Self-Cure

**Files:** `src/ecs/systems.cpp` (CureSystem), `tests/` (new test)

**Changes:**
1. Verify existing `if (nid == e.id()) continue;` in CureSystem
2. Add explicit contract comment above the line
3. Write a unit test:
   - Create FLECS world with one infected DoctorBoid
   - Run CureSystem for several frames → assert doctor remains infected
   - Add second healthy DoctorBoid within cure radius → run → assert first can be cured

**Commit:** `test(sim): add doctor no-self-cure contract test`

**Success criteria:** New test passes. All existing tests pass.

---

## Part C: Critical Steering Fixes (Tasks C1–C4)

**CRITICAL WORKER INSTRUCTION FOR ALL PART C TASKS:** All steering behaviors MUST use the Reynolds steering formula: `steering = (desired_velocity - current_velocity) * weight`, where `desired_velocity = normalize(direction) * max_speed`. Never use raw positional differences. Never use un-normalized average velocities.

### Task C1: Fix Cohesion Steering

**Files:** `src/ecs/systems.cpp` (SteeringSystem, cohesion block)

**Replace** the cohesion calculation with the Reynolds canonical form:
```cpp
if (coh_count > 0) {
    coh_x /= static_cast<float>(coh_count);
    coh_y /= static_cast<float>(coh_count);
    float dx = coh_x - pos.x;
    float dy = coh_y - pos.y;
    float mag = std::sqrt(dx * dx + dy * dy);
    if (mag > 0.001f) {
        float desired_vx = (dx / mag) * config.max_speed;
        float desired_vy = (dy / mag) * config.max_speed;
        force_x += (desired_vx - vel.vx) * config.cohesion_weight;
        force_y += (desired_vy - vel.vy) * config.cohesion_weight;
    }
}
```

**Commit:** `fix(ecs): correct cohesion to use normalized desired velocity (Reynolds steering)`

---

### Task C2: Fix Alignment Steering

**Files:** `src/ecs/systems.cpp` (SteeringSystem, alignment block)

**Replace** the alignment calculation with the normalized form:
```cpp
if (ali_count > 0) {
    ali_vx /= static_cast<float>(ali_count);
    ali_vy /= static_cast<float>(ali_count);
    float ali_mag = std::sqrt(ali_vx * ali_vx + ali_vy * ali_vy);
    if (ali_mag > 0.001f) {
        float desired_vx = (ali_vx / ali_mag) * config.max_speed;
        float desired_vy = (ali_vy / ali_mag) * config.max_speed;
        force_x += (desired_vx - vel.vx) * config.alignment_weight;
        force_y += (desired_vy - vel.vy) * config.alignment_weight;
    }
}
```

**Commit:** `fix(ecs): normalize alignment desired velocity to max_speed (Reynolds steering)`

---

### Task C3: Implement Swarm-Specific Flocking

**Files:** `src/ecs/systems.cpp` (SteeringSystem)

**Prerequisite:** A2 complete (AntivaxBoid tag exists).

**Changes:** Add swarm-tag filtering in the neighbor loop:
- **Separation:** ALL neighbors (prevents cross-swarm collisions)
- **Alignment:** ONLY same-swarm neighbors
- **Cohesion:** ONLY same-swarm neighbors

Add helper:
```cpp
bool same_swarm(flecs::entity a, flecs::entity b) {
    if (a.has<NormalBoid>() && b.has<NormalBoid>()) return true;
    if (a.has<DoctorBoid>() && b.has<DoctorBoid>()) return true;
    if (a.has<AntivaxBoid>() && b.has<AntivaxBoid>()) return true;
    return false;
}
```

**Commit:** `feat(ecs): implement swarm-specific flocking (same-swarm alignment/cohesion)`

**Success criteria:** Three visually distinct flocks form and move independently.

---

### Task C4: Add Minimum Speed Enforcement

**Files:** `include/components.h`, `config.ini`, `src/sim/config_loader.cpp`, `src/ecs/systems.cpp`

**Changes:**
1. Add `float min_speed = 54.0f;` to SimConfig (30% of 180.0)
2. Add `min_speed = 54.0` to config.ini [movement] section
3. Add parsing in config_loader.cpp
4. After velocity clamping in the movement/steering system:
```cpp
if (speed > 0.001f && speed < config.min_speed) {
    float scale = config.min_speed / speed;
    vel.vx *= scale; vel.vy *= scale;
}
```

**Commit:** `feat(ecs): enforce minimum boid speed to prevent stalling`

---

## Part D: Future Extensions (Preserved — Not Scheduled)

- **Obstacles:** Static objects that boids must avoid. Requires collision geometry + avoidance steering.
- **Sound libraries:** Audio feedback for simulation events. Low priority.
- **Edge turning:** Replace position-wrapping with margin-based turning forces. More natural behavior.
- **Predator/prey dynamics:** Additional swarm type that hunts other boids.
- **HPC deployment:** Apptainer container for Iridis X cluster. Headless mode for batch parameter sweeps.

---

## Execution Order Summary

```
PHASE 13 TASK SEQUENCE:

  Pre-Flight: Record 4 structural mistakes in mistakes.md
      |
  A1: AntivaxBoid component + stats fields
      |
  A2: Spawn AntivaxBoid from Normal pool
      |
  A3: Render antivax in orange
      | (A4, A5, A6 can run in parallel after A3)
  A4: Antivax infection (antivax x antivax only)
  A5: Antivax reproduction (antivax x antivax, offspring inherit)
  A6: Verify doctor cures antivax + no-self-cure contract
      |
  A7: Stats tracking + population graph line
      |
  A8: Antivax doctor-avoidance steering (update tag filter)
      |
  A9: Remove deprecated Antivax tag, cleanup
      |
  B1: Doctor no-self-cure unit test
      |
  C1: Fix cohesion steering --+
  C2: Fix alignment steering --+ (C1, C2 independent)
      |                        |
  C3: Swarm-specific flocking <+ (depends on A2 + C1 + C2)
      |
  C4: Minimum speed enforcement
      |
  DONE -- commit, verify, push
```

## Worker Assignment Strategy

**Recommended:** Sequential single-worker via Ralph Loop (proven in Phases 10–12).
- Most tasks touch `src/ecs/systems.cpp` — parallel workers create merge conflicts
- Tasks are small (one commit each) — fresh context per task prevents rot
- Orchestrator CAN parallelize A4/A5/A6 in worktrees if budget allows

**Worker model:** Sonnet (cost-efficient, proven)
**Estimated time:** 14 tasks x ~5 min = ~70 minutes via Ralph Loop

## Success Criteria (Phase 13 Complete)

1. Three distinct swarms visible: green (Normal), blue (Doctor), orange (Antivax)
2. Three distinct flocks — each clusters independently
3. Antivax boids flee from doctors visibly
4. Doctors cannot cure themselves — verified by unit test
5. Smooth, natural flocking — no oscillation, no stalling
6. All stats tracked — antivax alive/dead/newborns in panel and graph
7. Build clean, all tests pass (24+)
8. No references to deprecated `Antivax` tag remain
