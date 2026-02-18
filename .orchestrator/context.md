# CONTEXT: COMP6216 Boid Swarm Pandemic Simulation

This is the authoritative behavioral specification for the simulation. All agents and the orchestrator must treat this document as the single source of truth for what the simulation should do. Refer to `config.ini` for the exact default parameter values.

---

## The Model

The simulation runs a 2D pandemic model with **three separate boid swarms**, each following the Reynolds boid flocking model with swarm-specific behavioral extensions:

1. **Normal Boid Swarm** — The general population. Subject to infection, reproduction, and promotion to Doctor.
2. **Doctor Boid Swarm** — Healers. Can cure infected boids of any swarm. Separate flocking group.
3. **Antivax Boid Swarm** — Dissenters. Created from Normal boids via `p_antivax` transition. Actively flee from Doctors. Separate flocking group.

### Research Questions
- What is the optimal number of doctors to save a swarm from a pandemic?
- What behavioral strategies for the doctor swarm best contain infection spread?
- How does the antivax subpopulation affect pandemic outcomes?

---

## Core Boid Flocking Model (All Three Swarms)

Our implementation is a **hybrid model** combining elements from two canonical reference implementations. For formal mathematical definitions with LaTeX equations, see `docs/boid_model_reference.md`.

- **Separation:** Model A (V. Hunter Adams / Cornell ECE) — raw positional-difference accumulation, cross-swarm
- **Alignment & Cohesion:** Model B (Reynolds GDC'99 / Shiffman) — `desired = normalize(direction) * max_speed; steer = (desired - velocity) * weight`, same-swarm only
- **Force clamping:** Total force clamped to `max_force` (not per-behavior like Model B)
- **Integration:** Frame-rate independent forward Euler: `v += F * dt`, `p += v * dt`

Every boid in every swarm follows the Reynolds boid model (1987). The three fundamental steering behaviors are:

### Separation
Each boid steers away from nearby flockmates within `separation_radius` (default: 12.0) to avoid crowding. Separation applies to ALL nearby boids regardless of swarm — no boid should collide with any other.

**Canonical formula:**
```
for each neighbor within separation_radius:
    close_dx += my.x - neighbor.x
    close_dy += my.y - neighbor.y
velocity += (close_dx, close_dy) * separation_weight
```

### Alignment
Each boid steers toward the average heading of nearby **same-swarm** flockmates within `alignment_radius` (default: 50.0). Cross-swarm boids are excluded from alignment — this is what causes the three swarms to form distinct flocks.

**Canonical formula (Reynolds GDC'99):**
```
avg_vel = average velocity of same-swarm neighbors within alignment_radius
desired = normalize(avg_vel) * max_speed
steering = (desired - current_velocity) * alignment_weight
```

The normalization to `max_speed` is critical: it ensures the boid always steers toward the flock's *direction*, not its *speed*. Without normalization, alignment force weakens when neighbors are slow, breaking flock cohesion.

### Cohesion
Each boid steers toward the center of mass of nearby **same-swarm** flockmates within `cohesion_radius` (default: 50.0). Cross-swarm boids are excluded — each swarm coheres independently.

**Canonical formula (Reynolds GDC'99):**
```
com = center of mass of same-swarm neighbors within cohesion_radius
desired = normalize(com - position) * max_speed
steering = (desired - current_velocity) * cohesion_weight
```

The normalization to `max_speed` is critical: without it, the force magnitude scales with distance from center-of-mass, causing oscillation (too strong when far) and stalling (too weak when near).

### Speed Limits
- **Maximum speed:** `max_speed` (default: 180.0 units/second). Velocity is clamped after all forces are applied.
- **Maximum steering force:** `max_force` (default: 180.0 units/second). The total steering vector (sum of separation + alignment + cohesion + any special forces) is clamped before being added to velocity.
- **Minimum speed:** Recommended at ~30% of max_speed to prevent boids from stalling. Flocking entities should never be stationary.

### Swarm-Specific Flocking Rule
This is the mechanism that produces three visually distinct swarms:
- **Separation:** Applies to ALL nearby boids (prevents collisions across swarms)
- **Alignment:** Applies ONLY to same-swarm neighbors (makes each swarm head in its own direction)
- **Cohesion:** Applies ONLY to same-swarm neighbors (makes each swarm cluster independently)

Without this filtering, all three swarms would merge into one super-flock.

---

## Swarm Behaviors and Interaction Rules

### Normal Boids

**Tag:** `NormalBoid`

**Infection:**
- At spawn: `p_initial_infect_normal` (default: 0.05) chance of starting infected
- On collision with another Normal boid within `r_interact_normal` (default: 30.0): infected Normal has `p_infect_normal` (default: 0.5) probability of spreading infection
- Cross-swarm infection does NOT happen: Normal boids cannot infect Doctors or Antivax boids, and vice versa
- Infected boids die after `t_death` (default: 5.0 seconds) if not cured

**Reproduction:**
- On collision between two Normal boids within `r_interact_normal`: `p_offspring_normal` (default: 0.4) chance of reproducing
- Offspring count drawn from N(mean=2.0, stddev=1.0), clamped to ≥0
- Only Normal×Normal pairs reproduce (no cross-swarm reproduction)
- Sex system: reproduction requires Male + Female pair
- Reproduction cooldown: `reproduction_cooldown` (default: 5.0 seconds)
- If two infected parents reproduce: children are subject to contagion from only ONE parent (roll `p_infect_normal` once)

**Promotion:**
- When `Health.age >= t_adult` (default: 8.33 seconds): `p_become_doctor` (default: 0.05) chance per frame of becoming a Doctor
- Promotion: remove `NormalBoid` tag, add `DoctorBoid` tag, preserve all other state

**Antivax Transition:**
- At spawn or via periodic transition: `p_antivax` (default: 0.1) chance of becoming Antivax
- Transition: remove `NormalBoid` tag, add `AntivaxBoid` tag

**Infected Debuffs:**
- Interaction radius: ×`debuff_r_interact_normal_infected` (default: 0.8)
- Reproduction probability: ×`debuff_p_offspring_normal_infected` (default: 0.5)

---

### Doctor Boids

**Tag:** `DoctorBoid`

**This is a SEPARATE swarm** from Normal boids, with its own flocking group and interaction parameters.

**Infection:**
- At spawn: `p_initial_infect_doctor` (default: 0.02) chance of starting infected
- On collision with another Doctor within `r_interact_doctor` (default: 40.0): infected Doctor has `p_infect_doctor` (default: 0.5) probability of spreading infection
- Cross-swarm infection does NOT happen: Doctors cannot infect Normal or Antivax boids
- Infected doctors die after `t_death` (default: 5.0 seconds) if not cured

**Cure:**
- On collision with ANY infected boid (Normal, Doctor, or Antivax) within `r_interact_doctor`: `p_cure` (default: 0.8) chance of curing them
- Curing removes the `Infected` tag and resets `InfectionState.time_infected`
- Doctors CANNOT cure healthy individuals (no-op if target is not infected)
- Doctors CANNOT cure themselves — only another Doctor can cure them upon collision
- Doctors CAN cure other sick Doctors

**Reproduction:**
- On collision between two Doctor boids within `r_interact_doctor`: `p_offspring_doctor` (default: 0.05) chance of reproducing
- Offspring count drawn from N(mean=1.0, stddev=1.0), clamped to ≥0
- Only Doctor×Doctor pairs reproduce
- Sex system applies: Male + Female required
- If two infected parents reproduce: children get contagion from only ONE parent

**Infected Debuffs:**
- Cure probability: ×`debuff_p_cure_infected` (default: 0.5)
- Interaction radius: ×`debuff_r_interact_doctor_infected` (default: 0.7)
- Reproduction probability: ×`debuff_p_offspring_doctor_infected` (default: 0.5)

---

### Antivax Boids

**Tag:** `AntivaxBoid` (mutually exclusive with `NormalBoid` and `DoctorBoid`)

**This is a SEPARATE swarm** from both Normal and Doctor boids. Antivax boids are created from Normal boids and form their own flocking group.

**Origin:**
- Antivax boids are randomly created from Normal boids according to `p_antivax` (default: 0.1)
- This transition happens at spawn time: a newly spawned Normal boid has a `p_antivax` chance of becoming Antivax instead
- Once Antivax, a boid stays Antivax (no reversal mechanism)

**Flocking (standard boid model):**
- Antivax boids follow ALL three Reynolds rules (separation, alignment, cohesion)
- Alignment and cohesion: with other Antivax boids ONLY (forms a distinct Antivax flock)
- Separation: from ALL boids (prevents collisions with any swarm)

**Doctor Avoidance (special rule):**
- Antivax boids have an ADDITIONAL steering force: strong repulsion from any `DoctorBoid` within `antivax_repulsion_radius` (default: 100.0)
- Repulsion weight: `antivax_repulsion_weight` (default: 3.0) — this is high relative to flocking weights (1.0–1.5) to ensure they actively flee
- This force is ADDITIVE to the standard flocking forces, it never replaces them
- Consequence: Antivax flocks dynamically avoid doctors, making them harder to cure but still possible if cornered

**Infection:**
- Antivax boids can infect each other: same rules as Normal boids (`p_infect_normal`, `r_interact_normal`)
- Cross-swarm infection does NOT happen: Antivax cannot infect Normal or Doctor boids
- Infected Antivax boids die after `t_death` if not cured
- Doctors CAN still cure Antivax boids if they get close enough (despite avoidance behavior)

**Reproduction:**
- Antivax×Antivax collision: same reproduction parameters as Normal boids (`p_offspring_normal`, offspring N(2.0, 1.0))
- Offspring are born as Antivax (inheriting parent swarm type)
- Sex system applies

**Infected Debuffs:** Same as Normal boids.

**No Promotion:** Antivax boids do NOT become Doctors (they distrust medicine — thematic consistency).

---

## Interaction Matrix

| Event | Normal×Normal | Doctor×Doctor | Antivax×Antivax | Doctor×Normal | Doctor×Antivax | Normal×Antivax |
|-------|:---:|:---:|:---:|:---:|:---:|:---:|
| Infection | p_infect_normal | p_infect_doctor | p_infect_normal | ✗ | ✗ | ✗ |
| Reproduction | p_offspring_normal, N(2,1) | p_offspring_doctor, N(1,1) | p_offspring_normal, N(2,1) | ✗ | ✗ | ✗ |
| Cure | ✗ | p_cure | ✗ | p_cure (doctor cures normal) | p_cure (doctor cures antivax) | ✗ |
| Promotion | → Doctor (p_become_doctor) | ✗ | ✗ | — | — | — |

---

## Simulation Parameters Reference

All parameters are stored in the `SimConfig` singleton and loaded from `config.ini`. Sliders override config values at runtime.

### Infection & Cure

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Initial infection (normal) | `p_initial_infect_normal` | 0.05 | Chance normal/antivax boid starts infected |
| Initial infection (doctor) | `p_initial_infect_doctor` | 0.02 | Chance doctor starts infected |
| Infection spread (normal) | `p_infect_normal` | 0.50 | Normal/Antivax×same infection on collision |
| Infection spread (doctor) | `p_infect_doctor` | 0.50 | Doctor×Doctor infection on collision |
| Cure probability | `p_cure` | 0.80 | Doctor cures infected boid on collision |

### Reproduction

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Reproduction (normal) | `p_offspring_normal` | 0.40 | Normal/Antivax reproduction chance |
| Reproduction (doctor) | `p_offspring_doctor` | 0.05 | Doctor reproduction chance |
| Offspring count (normal) | `offspring_mean_normal` / `offspring_stddev_normal` | 2.0 / 1.0 | N(2,1) kids, clamped ≥0 |
| Offspring count (doctor) | `offspring_mean_doctor` / `offspring_stddev_doctor` | 1.0 / 1.0 | N(1,1) kids, clamped ≥0 |
| Reproduction cooldown | `reproduction_cooldown` | 5.0 sec | Minimum time between reproductions |

### Transitions

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Doctor promotion | `p_become_doctor` | 0.05 | Adult normal → doctor chance per frame |
| Antivax transition | `p_antivax` | 0.10 | Normal boid → antivax at spawn |

### Interaction Radii

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Normal/Antivax interact | `r_interact_normal` | 30.0 | Collision radius for normal/antivax |
| Doctor interact | `r_interact_doctor` | 40.0 | Doctor interaction radius |

### Timing

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Death time | `t_death` | 5.0 sec | Seconds after infection before death |
| Adult age | `t_adult` | 8.33 sec | Seconds before eligible for doctor promotion |

### World & Population

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| World size | `world_width` × `world_height` | 1920 × 1080 | Simulation bounds |
| Initial normal count | `initial_normal_count` | 200 | Starting normal boid population |
| Initial doctor count | `initial_doctor_count` | 10 | Starting doctor boid population |

### Movement & Flocking

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Max speed | `max_speed` | 180.0 /sec | Boid velocity cap |
| Max force | `max_force` | 180.0 /sec | Steering force cap |
| Separation weight | `separation_weight` | 1.5 | Avoid nearby boids (all swarms) |
| Alignment weight | `alignment_weight` | 1.0 | Match heading (same-swarm only) |
| Cohesion weight | `cohesion_weight` | 1.0 | Steer toward group center (same-swarm only) |
| Separation radius | `separation_radius` | 12.0 | Protected range for separation |
| Alignment radius | `alignment_radius` | 50.0 | Visual range for alignment |
| Cohesion radius | `cohesion_radius` | 50.0 | Visual range for cohesion |

### Infected Debuffs (Multipliers)

| Parameter | Normal/Antivax | Doctor |
|-----------|:-:|:-:|
| Interaction radius | ×0.8 (`debuff_r_interact_normal_infected`) | ×0.7 (`debuff_r_interact_doctor_infected`) |
| Reproduction probability | ×0.5 (`debuff_p_offspring_normal_infected`) | ×0.5 (`debuff_p_offspring_doctor_infected`) |
| Cure probability | — | ×0.5 (`debuff_p_cure_infected`) |

### Antivax-Specific

| Parameter | Field | Default | Description |
|-----------|-------|---------|-------------|
| Repulsion radius | `antivax_repulsion_radius` | 100.0 | Visual range for detecting doctors |
| Repulsion weight | `antivax_repulsion_weight` | 3.0 | Strength of doctor-avoidance force |

---

## Simulation View

The simulation is interactive with a GUI showing real-time data:

### Stats Panel
- Normal alive / Doctor alive / Antivax alive
- Dead total / Dead normal / Dead doctor / Dead antivax
- Newborns total / Newborns normal / Newborns doctor / Newborns antivax

### Visual Elements
- All live boids rendered with swarm-specific colors (Normal=green, Doctor=blue, Antivax=orange/yellow)
- Infected boids shown with red tint
- Interaction radii rendered as circles
- Population graph (real-time line chart, 500-frame window)
- Parameter sliders for runtime tuning
- Pause/Resume and Reset controls
- Keyboard shortcuts: SPACE (pause), R (reset)

---

## Technologies

- **C++17** — Main language
- **FLECS v4.1.4** — Entity Component System
- **Raylib 5.5** — 2D rendering + input
- **raygui 4.0** — Immediate-mode GUI (stats panel, sliders)
- **GoogleTest 1.14.0** — Unit testing
- **CMake 3.20+** — Build system
- **CPM.cmake** — Dependency management (auto-fetches all libraries)

## Architecture

- `src/ecs/` — FLECS components, systems pipeline, world setup
- `src/sim/` — Behavior logic: infection, cure, reproduction, death, aging, promotion, config loader
- `src/spatial/` — Fixed-cell spatial hash grid (pure C++, no FLECS/Raylib)
- `src/render/` — Raylib window, drawing, raygui stats overlay
- `include/` — Shared headers (API contract between modules)
- `docs/` — Boid model formal reference, task logs
- `tests/` — Unit tests (35 total: 12 ConfigLoader + 11 SpatialGrid + 2 CureContract + 10 Antivax)
- `config.ini` — Default simulation parameters

---

## Known Issues — ALL RESOLVED ✅

All five original issues have been fixed as of Phase 13:

1. ~~Cohesion steering is incorrect~~ → Fixed: Reynolds canonical `(normalize(COM-pos) * max_speed - vel) * weight` (Phase 13 C1)
2. ~~Alignment steering is partially incorrect~~ → Fixed: `normalize(avg_vel) * max_speed` (Phase 13 C2)
3. ~~No swarm-specific flocking~~ → Fixed: same-swarm filtering for alignment/cohesion (Phase 13 C3)
4. ~~Antivax is a tag, not a separate swarm~~ → Fixed: `AntivaxBoid` as primary swarm tag (Phase 13 A1-A9)
5. ~~No minimum speed enforcement~~ → Fixed: `min_speed = 54.0` enforcement in MovementSystem (Phase 13 C4)

Additional post-Phase 13 fixes:
6. Offspring velocity: spawn at `max_speed` instead of random (DEC-025)
7. Separation formula: raw accumulation per spec (DEC-025)
8. Parameter tuning: `max_force = 180.0`, `separation_radius = 12.0` for responsive flocking (DEC-026)