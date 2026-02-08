# Changelog — ecs
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
- **Antivax system**: Added Antivax tag assignment in spawn.cpp (p_antivax probability). Created register_antivax_steering_system in systems.cpp that applies strong repulsion force from doctors within visual range (ADDITIVE to flocking). Antivax boids can still be cured by doctors.
- **Sex system**: Added opposite-sex requirement to reproduction system. Boids now only reproduce when one parent is Male and the other is Female. Same-sex collisions skip reproduction logic.
- **Infected debuffs**: Modified infection, cure, and reproduction systems in systems.cpp to apply debuff multipliers from SimConfig when boids are infected

## 2026-02-07
- **21:05Z** | `src/ecs/systems.cpp` | Edited: 'void register_rebuild_grid_system(flecs::world& world) {    ' -> 'void register_rebuild_grid_system(flecs::world& world) {    ' | session:`cefb983f`
- **21:07Z** | `src/ecs/spawn.cpp` | Edited: '        auto boid = world.entity()             .add<NormalBo' -> '        auto boid = world.entity()             .add<NormalBo' | session:`cefb983f`
- **21:07Z** | `src/ecs/spawn.cpp` | Edited: '        auto boid = world.entity()             .add<DoctorBo' -> '        auto boid = world.entity()             .add<DoctorBo' | session:`cefb983f`
- **21:07Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_initial_population(flecs::world& world) {     con' -> 'void spawn_initial_population(flecs::world& world) {     con' | session:`cefb983f`
- **21:07Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_normal_boids(flecs::world& world, int count) {   ' -> 'void spawn_normal_boids(flecs::world& world, int count) {   ' | session:`cefb983f`
- **21:07Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_doctor_boids(flecs::world& world, int count) {   ' -> 'void spawn_doctor_boids(flecs::world& world, int count) {   ' | session:`cefb983f`
- **21:09Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_normal_boids(flecs::world& world, int count) {   ' -> 'void spawn_normal_boids(flecs::world& world, int count) {   ' | session:`cefb983f`
- **21:09Z** | `src/ecs/spawn.cpp` | Edited: '        // Initial infection         if (dist_infect(rng) < ' -> '        // Initial infection         if (dist_infect(rng) < ' | session:`cefb983f`
- **21:09Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_doctor_boids(flecs::world& world, int count) {   ' -> 'void spawn_doctor_boids(flecs::world& world, int count) {   ' | session:`cefb983f`
- **21:09Z** | `src/ecs/spawn.cpp` | Edited: '        // Initial infection         if (dist_infect(rng) < ' -> '        // Initial infection         if (dist_infect(rng) < ' | session:`cefb983f`
- **21:09Z** | `src/ecs/spawn.cpp` | Edited: 'void spawn_initial_population(flecs::world& world) {     con' -> 'void spawn_initial_population(flecs::world& world) {     con' | session:`cefb983f`
- **21:09Z** | `src/ecs/world.cpp` | Edited: '    // Create SpatialGrid singleton     const auto* config =' -> '    // Create SpatialGrid singleton     const SimConfig& con' | session:`cefb983f`
- **21:09Z** | `src/ecs/systems.cpp` | Edited: 'void register_movement_system(flecs::world& world) {     wor' -> 'void register_movement_system(flecs::world& world) {     wor' | session:`cefb983f`
- **21:12Z** | `src/ecs/stats.cpp` | Edited: 'void register_stats_system(flecs::world& world) {     world.' -> 'void register_stats_system(flecs::world& world) {     world.' | session:`cefb983f`
- **21:12Z** | `src/ecs/systems.cpp` | Edited: 'void register_rebuild_grid_system(flecs::world& world) {    ' -> 'void register_rebuild_grid_system(flecs::world& world) {    ' | session:`cefb983f`
- **21:12Z** | `src/ecs/stats.cpp` | Edited: 'void register_stats_system(flecs::world& world) {     world.' -> 'void register_stats_system(flecs::world& world) {     world.' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: 'void register_rebuild_grid_system(flecs::world& world) {    ' -> 'void register_rebuild_grid_system(flecs::world& world) {    ' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: '            // Update heading based on velocity             ' -> '            // Update heading based on velocity             ' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: 'void register_movement_system(flecs::world& world) {     wor' -> 'void register_movement_system(flecs::world& world) {     wor' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: 'void register_reproduction_system(flecs::world& world) {    ' -> 'void register_reproduction_system(flecs::world& world) {    ' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: 'void register_render_sync_system(flecs::world& world) {     ' -> 'void register_render_sync_system(flecs::world& world) {     ' | session:`cefb983f`
- **21:13Z** | `src/ecs/systems.cpp` | Edited: 'void register_movement_system(flecs::world& world) {     wor' -> 'void register_movement_system(flecs::world& world) {     wor' | session:`cefb983f`
- **21:14Z** | `src/ecs/systems.cpp` | Edited: 'void register_reproduction_system(flecs::world& world) {    ' -> 'void register_reproduction_system(flecs::world& world) {    ' | session:`cefb983f`
- **21:15Z** | `src/ecs/spatial_grid_stub.cpp` | File written/created | session:`cefb983f`
- **21:18Z** | `src/ecs/stats.cpp` | Edited: 'void register_stats_system(flecs::world& world) {     world.' -> 'void register_stats_system(flecs::world& world) {     world.' | session:`cefb983f`
- **21:18Z** | `src/ecs/stats.cpp` | Edited: 'void register_stats_system(flecs::world& world) {     world.' -> 'void register_stats_system(flecs::world& world) {     world.' | session:`cefb983f`
- **21:19Z** | `src/ecs/systems.cpp` | Edited: 'void register_rebuild_grid_system(flecs::world& world) {    ' -> 'void register_rebuild_grid_system(flecs::world& world) {    ' | session:`cefb983f`
- **22:15Z** | `src/ecs/world.cpp` | Edited: '#include "world.h" #include "components.h" #include "spatial' -> '#include "world.h" #include "components.h" #include "spatial' | session:`e9956e72`
- **22:16Z** | `src/ecs/systems.cpp` | File written/created | session:`e9956e72`
- **22:16Z** | `src/ecs/systems.cpp` | Edited: '#include <flecs.h> #include <cmath> ' -> '#include <flecs.h> #include <cmath> #include <algorithm> ' | session:`e9956e72`
