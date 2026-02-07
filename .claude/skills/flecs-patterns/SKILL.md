# FLECS Patterns for Boid Simulation

## Component Design
- Plain structs, no methods: `struct Position { float x, y; };`
- Tags: `struct NormalBoid {}; struct DoctorBoid {};`
- Singletons: `world.set<SimConfig>({...});`

## Pipeline Phases
- **PreUpdate**: Rebuild spatial grid
- **OnUpdate**: Boid steering, velocity integration, position update
- **PostUpdate**: Collision detection, infection, cure, reproduction, death
- **OnStore**: Render (read-only queries for drawing)

## Deferred Operations
```cpp
world.defer_begin();
// Safe to create/destroy entities here during iteration
world.defer_end();
```

## Cached Queries
```cpp
auto q = world.query_builder<Position, Velocity, NormalBoid>().build();
q.each([](Position& p, Velocity& v, NormalBoid) { /* ... */ });
```

## Entity Lifecycle
- Spawn via `world.entity().add<NormalBoid>().set<Position>({x,y}).set<Velocity>({vx,vy})`
- Kill via `e.destruct()` inside deferred block
- Classify via tag add/remove: `e.add<Infected>()`, `e.remove<Infected>()`