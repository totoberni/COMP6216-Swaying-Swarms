# CONTEXT: COMP6216 Boid Swarm Pandemic Simulation

This is the authoritative behavioral specification for the simulation. All agents and the orchestrator must treat this document as the single source of truth for what the simulation should do. Refer to `config.ini` for the exact default parameter values.

---

## The Model

The simulation runs a 2D pandemic model with **three separate boid swarms**, each following the Shiffman/Reynolds boid flocking model (Model B) with swarm-specific behavioral extensions:

1. **Normal Boid Swarm** — The general population. Subject to infection, reproduction, and promotion to Doctor.
2. **Doctor Boid Swarm** — Healers. Can cure infected boids of any swarm. Separate flocking group.
3. **Antivax Boid Swarm** — Dissenters. Created from Normal boids via `p_antivax` transition. Actively flee from Doctors. Separate flocking group.

### Research Questions
- What is the optimal number of doctors to save a swarm from a pandemic?
- What behavioral strategies for the doctor swarm best contain infection spread?
- How does the antivax subpopulation affect pandemic outcomes?

### Boid Flocking Model
For the formal mathematical definition of the Pure Shiffman Model B implementation (separation, alignment, cohesion, force clamping, frame-rate independence), see `docs/boid_model_reference.md`.

For all simulation parameters and their defaults, see `config.ini` and `include/components.h` (SimConfig struct).

---

## Swarm Behaviors and Interaction Rules

### Normal Boids

**Tag:** `NormalBoid`

**Infection:**
- At spawn: `p_initial_infect_normal` (default: 0.05) chance of starting infected
- On collision with another Normal boid within `r_interact_normal` (default: 30.0): infected Normal has `p_infect_normal` (default: 0.5) probability of spreading infection
- Normal↔Antivax cross-infection IS allowed (same epidemiological population). Normal boids cannot infect Doctors, and vice versa
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
- Antivax boids can infect each other and Normal boids: same rules as Normal boids (`p_infect_normal`, `r_interact_normal`)
- Normal↔Antivax cross-infection IS allowed (same epidemiological population). Antivax cannot infect Doctor boids
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
| Infection | p_infect_normal | p_infect_doctor | p_infect_normal | ✗ | ✗ | p_infect_normal |
| Reproduction | p_offspring_normal, N(2,1) | p_offspring_doctor, N(1,1) | p_offspring_normal, N(2,1) | ✗ | ✗ | ✗ |
| Cure | ✗ | p_cure | ✗ | p_cure (doctor cures normal) | p_cure (doctor cures antivax) | ✗ |
| Promotion | → Doctor (p_become_doctor) | ✗ | ✗ | — | — | — |

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
- `tests/` — Unit tests (40 total: 13 ConfigLoader + 15 SpatialGrid + 2 CureContract + 10 Antivax)
- `config.ini` — Default simulation parameters

