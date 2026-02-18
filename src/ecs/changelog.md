# Changelog — ecs
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
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
- **01:35Z** | `src/ecs/systems.cpp` | Edited: '                for (const auto& [nid, dist] : neighbors) { ' -> '                for (const auto& [nid, dist] : neighbors) { ' | session:`f655d95a`
- **01:35Z** | `src/ecs/systems.cpp` | Edited: '                // Average and apply alignment force (desire' -> '                // Average and apply alignment force (desire' | session:`f655d95a`
- **01:35Z** | `src/ecs/systems.cpp` | Edited: '                // Average and apply cohesion force (steer t' -> '                // Average and apply cohesion force (desired' | session:`f655d95a`
- **01:35Z** | `src/ecs/systems.cpp` | Edited: '                // Clamp total force to max_force           ' -> '                // Clamp total force to max_force           ' | session:`f655d95a`
- **01:36Z** | `src/ecs/systems.cpp` | Edited: 'void register_movement_system(flecs::world& world) {     wor' -> 'void register_movement_system(flecs::world& world) {     wor' | session:`f655d95a`
- **01:39Z** | `src/ecs/systems.cpp` | Edited: '                    // Clamp repulsion force to max_force   ' -> '                    // Clamp repulsion force to max_force   ' | session:`f655d95a`
- **01:39Z** | `src/ecs/systems.cpp` | Edited: '            // Enforce minimum speed — prevents boids from' -> '            // Enforce minimum speed — prevents boids from' | session:`f655d95a`
- **02:09Z** | `src/ecs/systems.cpp` | Edited: '                    // Add repulsion force (stronger when cl' -> '                    // Add repulsion force (raw accumulation' | session:`f655d95a`
- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                if (doctor_count > 0) {                     ' -> '                if (doctor_count > 0) {                     ' | session:`f655d95a`
- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                    if (dist < config.separation_radius) {  ' -> '                    if (dist < config.separation_radius) {  ' | session:`f655d95a`
- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                // Average and apply separation force       ' -> '                // Apply accumulated separation force (raw p' | session:`f655d95a`
- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                    // Spawn offspring                     s' -> '                    // Spawn offspring                     s' | session:`f655d95a`
- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                    // Spawn offspring                     s' -> '                    // Spawn offspring                     s' | session:`f655d95a`

## 2026-02-18

- **02:13Z** | `src/ecs/systems.cpp` | Edited: '                    // Spawn offspring — inherit AntivaxBo' -> '                    // Spawn offspring — inherit AntivaxBo' | session:`f655d95a`
- **02:15Z** | `src/ecs/spawn.cpp` | Edited: '    std::uniform_real_distribution<float> dist_angle(0.0f, 2' -> '    std::uniform_real_distribution<float> dist_angle(0.0f, 2' | session:`f655d95a`
- **02:15Z** | `src/ecs/spawn.cpp` | Edited: '    std::uniform_real_distribution<float> dist_angle(0.0f, 2' -> '    std::uniform_real_distribution<float> dist_angle(0.0f, 2' | session:`f655d95a`
