# FLECS Patterns & Simulation Domain Knowledge

## Component Design
- Plain structs, no methods: `struct Position { float x, y; };`
- Tags (empty structs) for classification: `struct NormalBoid {}; struct DoctorBoid {};`
- State tags: `struct Infected {}; struct Alive {}; struct Male {}; struct Female {}; struct Antivax {};`
- Singletons: `world.set<SimConfig>({...}); world.set<SimStats>({...}); world.set<SpatialGrid>({...});`

## Pipeline Phase Mapping
- **PreUpdate**: Rebuild spatial grid from all alive boid positions
- **OnUpdate**: Boid steering (separation, alignment, cohesion), velocity integration, position update
- **PostUpdate**: Collision detection → infection → cure → reproduction → death → doctor promotion
- **OnStore**: Render pass (read-only queries populating RenderState for drawing)

## Deferred Operations
Always wrap entity creation/destruction inside deferred blocks during system iteration:
```cpp
world.defer_begin();
// Safe: create/destroy entities, add/remove components
world.defer_end();
```

## Cached Queries
Create at startup, reuse every frame:
```cpp
auto q = world.query_builder<Position, Velocity, NormalBoid>().build();
q.each([](Position& p, Velocity& v, NormalBoid) { /* ... */ });
```

## Entity Lifecycle
- Spawn: `world.entity().add<NormalBoid>().add<Alive>().set<Position>({x,y}).set<Velocity>({vx,vy})`
- Kill: `e.destruct()` inside deferred block
- Classify: `e.add<Infected>()`, `e.remove<Infected>()`, `e.remove<NormalBoid>().add<DoctorBoid>()`

---

## Simulation Parameter Reference

All parameters are stored in the `SimConfig` singleton. Never use magic numbers.

| Parameter | Field Name | Default | Description |
|-----------|-----------|---------|-------------|
| Initial infection (normal) | `p_initial_infect_normal` | 0.05 | Chance normal boid starts infected |
| Initial infection (doctor) | `p_initial_infect_doctor` | 0.02 | Chance doctor starts infected |
| Infection spread (normal) | `p_infect_normal` | 0.50 | NormalÃ—Normal infection chance on collision |
| Infection spread (doctor) | `p_infect_doctor` | 0.50 | DoctorÃ—Doctor infection chance on collision |
| Reproduction (normal) | `p_offspring_normal` | 0.40 | NormalÃ—Normal reproduction chance |
| Reproduction (doctor) | `p_offspring_doctor` | 0.05 | DoctorÃ—Doctor reproduction chance |
| Cure probability | `p_cure` | 0.80 | Doctor cures sick boid on collision |
| Doctor promotion | `p_become_doctor` | 0.05 | Adult normal boid becomes doctor (per frame) |
| Antivax percentage | `p_antivax` | 0.10 | Normal boids that actively avoid doctors |
| Interact radius (normal) | `r_interact_normal` | 30.0f | Normal boid collision radius |
| Interact radius (doctor) | `r_interact_doctor` | 40.0f | Doctor boid interaction radius |
| Death time | `t_death` | 300.0f | Frames until infected boid dies |
| Adult age | `t_adult` | 500.0f | Frames until eligible for doctor promotion |
| Offspring count (normal) | N(2,1) | — | `std::normal_distribution<float>(2.0f, 1.0f)`, clamp â‰¥0 |
| Offspring count (doctor) | N(1,1) | — | `std::normal_distribution<float>(1.0f, 1.0f)`, clamp â‰¥0 |
| World size | `world_width` × `world_height` | 1920×1080 | Simulation bounds |
| Initial counts | `initial_normal_count` / `initial_doctor_count` | 200 / 10 | Starting populations |
| Max speed | `max_speed` | 3.0f | Boid velocity cap |
| Max force | `max_force` | 0.1f | Steering force cap |
| Flocking weights | `separation_weight` / `alignment_weight` / `cohesion_weight` | 1.5 / 1.0 / 1.0 | Boid flocking |

### Extension: Infected Debuff Multipliers

| Parameter | Normal Debuff | Doctor Debuff |
|-----------|:---:|:---:|
| r_interact | ×0.8 | ×0.7 |
| p_offspring | ×0.5 | ×0.5 |
| p_cure | — | ×0.5 |

Store as `debuff_r_interact_normal`, `debuff_r_interact_doctor`, etc. in SimConfig.

---

## Behavior Rules Matrix

| Event | Normal×Normal | Doctor×Doctor | Doctor×Normal |
|-------|:---:|:---:|:---:|
| Infection spread | `p_infect_normal` | `p_infect_doctor` | ✗ (no cross-swarm) |
| Reproduction | `p_offspring_normal`, N(2,1) kids | `p_offspring_doctor`, N(1,1) kids | ✗ (no cross-swarm) |
| Cure | ✗ | `p_cure` (doctor cures doctor) | `p_cure` (doctor cures normal) |

### Critical Rules
1. **Cross-swarm infection does NOT happen** — only cure crosses swarm boundary
2. **Two infected parents reproduce** — children get contagion from only ONE parent (roll `p_infect` once)
3. **Doctors cannot cure healthy boids** — no-op if target is not infected
4. **Reproduction requires cooldown** — use ReproductionCooldown component
5. **Death = deferred destroy** — always use `world.defer_begin()`/`defer_end()`
6. **Doctor promotion preserves state** — remove NormalBoid tag, add DoctorBoid tag, keep everything else

### Extension-Specific Rules
- **Sex system**: Male/Female tags at 50/50 spawn. Reproduction only between Male + Female pairs.
- **Antivax**: Antivax boids add strong repulsion force from DoctorBoid. This is ADDITIVE to existing flocking, never a replacement. They can still be cured if a doctor physically reaches them.
- **Infected debuffs**: Apply multipliers to effective parameters during collision checks. Do not modify SimConfig base values — multiply at point of use.

---

## Implementation Patterns

### Infection Check Pattern
```cpp
// In PostUpdate system — after spatial grid query returns neighbors
void check_infection(Entity a, Entity b, const SimConfig& cfg) {
    bool a_normal = a.has<NormalBoid>();
    bool b_normal = b.has<NormalBoid>();
    // Cross-swarm: no infection
    if (a_normal != b_normal) return;
    // Both must be same type; one infected, one not
    if (a.has<Infected>() == b.has<Infected>()) return;
    float p = a_normal ? cfg.p_infect_normal : cfg.p_infect_doctor;
    if (rng.uniform() < p) {
        // Infect the healthy one
        Entity& target = a.has<Infected>() ? b : a;
        target.add<Infected>();
        target.set<InfectionState>({true, 0.0f});
    }
}
```

### Reproduction Pattern
```cpp
// Only same-swarm pairs. With sex extension: require Male+Female.
void check_reproduction(Entity a, Entity b, const SimConfig& cfg, World& world) {
    bool a_normal = a.has<NormalBoid>();
    if (a_normal != b.has<NormalBoid>()) return; // no cross-swarm
    // Sex check (extension)
    if (a.has<Male>() == b.has<Male>()) return; // same sex, skip
    float p = a_normal ? cfg.p_offspring_normal : cfg.p_offspring_doctor;
    if (rng.uniform() < p) {
        float mean = a_normal ? 2.0f : 1.0f;
        int n_kids = std::max(0, (int)std::round(rng.normal(mean, 1.0f)));
        world.defer_begin();
        for (int i = 0; i < n_kids; i++) { /* spawn child */ }
        world.defer_end();
    }
}
```

### Antivax Steering Pattern (Extension)
```cpp
// ADDITIVE to existing flocking forces — never replace
Vector2 compute_antivax_repulsion(Entity boid, const SpatialGrid& grid, float visual_range) {
    Vector2 repulsion = {0, 0};
    if (!boid.has<Antivax>()) return repulsion;
    auto doctors = grid.query_neighbors(boid_pos, visual_range);
    for (auto& [doc_id, dist] : doctors) {
        if (!doc_id.has<DoctorBoid>()) continue;
        Vector2 away = normalize(boid_pos - doc_pos) / dist;
        repulsion += away * ANTIVAX_REPULSION_WEIGHT;
    }
    return repulsion; // Add this to the existing flocking force
}
```

---

## Anti-Patterns (DO NOT)

- ✗ **`std::rand()`** — always use `<random>` with a seeded `std::mt19937`
- ✗ **Magic numbers** — all values come from SimConfig
- ✗ **Raylib includes outside src/render/** — rendering is isolated
- ✗ **Raw pointer ownership** — use FLECS entity handles
- ✗ **Modifying existing component fields** — only add new components/fields
- ✗ **Entity create/destroy outside deferred blocks** — always defer during iteration
- ✗ **Cross-swarm infection** — only cure crosses the swarm boundary
- ✗ **Replacing flocking forces for antivax** — antivax repulsion is additive
- ✗ **`using namespace std;`** — explicit namespacing only
- ✗ **`double` for sim values** — use `float` everywhere