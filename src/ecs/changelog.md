# Changelog — ecs
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
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

## 2026-02-18

- **00:00Z** | `src/ecs/world.cpp` | Edited: '    world.component<Antivax>();      // Register SpatialGrid' -> '    world.component<Antivax>();     world.component<AntivaxB' | session:`f655d95a`
- **00:01Z** | `src/ecs/spawn.cpp` | Edited: '        auto boid = world.entity()             .add<NormalBo' -> '        auto boid = world.entity()             .add<Alive>()' | session:`f655d95a`
- **00:05Z** | `src/ecs/systems.cpp` | Edited: '                brd.is_doctor = e.has<DoctorBoid>();        ' -> '                // Determine swarm type                 if (' | session:`f655d95a`
- **00:09Z** | `src/ecs/systems.cpp` | Edited: '                    // Try to infect                     if ' -> '                    // Try to infect                     if ' | session:`f655d95a`
- **00:10Z** | `src/ecs/systems.cpp` | Edited: '                    // Update stats                     stat' -> '                    // Update stats                     stat' | session:`f655d95a`
- **00:11Z** | `src/ecs/systems.cpp` | Edited: '                        auto child = w.entity()             ' -> '                        auto child = w.entity()             ' | session:`f655d95a`
- **00:12Z** | `src/ecs/systems.cpp` | Edited: '                // Query neighbors within effective doctor i' -> '                // Query neighbors within effective doctor i' | session:`f655d95a`
- **00:14Z** | `src/ecs/stats.cpp` | Edited: '            // Reset per-frame counters (deaths and newborns' -> '            // Reset per-frame counters (deaths and newborns' | session:`f655d95a`
- **00:14Z** | `src/ecs/stats.cpp` | Edited: '            // Count alive doctor boids             auto q_d' -> '            // Count alive doctor boids             auto q_d' | session:`f655d95a`
- **00:14Z** | `src/ecs/stats.cpp` | Edited: '            // Record population history for graph          ' -> '            // Record population history for graph          ' | session:`f655d95a`
- **00:14Z** | `src/ecs/systems.cpp` | Edited: '                    // Update stats                     stat' -> '                    // Update stats                     stat' | session:`f655d95a`
- **00:14Z** | `src/ecs/spawn.cpp` | Edited: '    stats.newborns_total = 0;     stats.newborns_normal = 0;' -> '    stats.newborns_total = 0;     stats.newborns_normal = 0;' | session:`f655d95a`
- **00:17Z** | `src/ecs/systems.cpp` | Edited: '            // Only process antivax normal boids            ' -> '            // Only process AntivaxBoid entities (primary sw' | session:`f655d95a`
- **00:17Z** | `src/ecs/world.cpp` | Edited: '    // Register tag components     world.component<NormalBoi' -> '    // Register tag components     world.component<NormalBoi' | session:`f655d95a`
- **00:19Z** | `src/ecs/systems.h` | Edited: '#pragma once  #include <flecs.h>  void register_all_systems(' -> '#pragma once  #include <flecs.h>  void register_all_systems(' | session:`f655d95a`
- **00:37Z** | `src/ecs/systems.h` | Edited: '#pragma once  #include <flecs.h>  void register_all_systems(' -> '#pragma once  #include <flecs.h>  void register_all_systems(' | session:`f655d95a`
