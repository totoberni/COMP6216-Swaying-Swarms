# Changelog — ecs
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
- **00:45Z** | `src/ecs/systems.cpp` | Edited: '            // Only doctors can cure             auto q_doct' -> '            // Only doctors can cure             auto q_doct' | session:`4dfbd6b1`
- **00:45Z** | `src/ecs/systems.cpp` | Edited: '            w.defer_begin();              // Process Normal ' -> '            w.defer_begin();              std::vector<std::p' | session:`4dfbd6b1`
- **00:46Z** | `src/ecs/systems.cpp` | Edited: 'grid.query_neighbors(pos.x, pos.y, effective_r_interact); ' -> 'grid.query_neighbors(pos.x, pos.y, effective_r_interact, nei' | session:`4dfbd6b1`
- **00:48Z** | `src/ecs/systems.cpp` | Edited: '            q.each([&](flecs::entity e, const Position& pos,' -> '            q.each([&](flecs::entity e, const Position& pos,' | session:`4dfbd6b1`
- **00:48Z** | `src/ecs/systems.cpp` | Edited: 'void register_movement_system(flecs::world& world) {     wor' -> 'void register_movement_system(flecs::world& world) {     wor' | session:`4dfbd6b1`
- **00:51Z** | `src/ecs/systems_steering.cpp` | File written/created | session:`4dfbd6b1`
- **00:51Z** | `src/ecs/systems_infection.cpp` | File written/created | session:`4dfbd6b1`
- **00:51Z** | `src/ecs/systems_lifecycle.cpp` | File written/created | session:`4dfbd6b1`
- **00:51Z** | `src/ecs/systems_render_sync.cpp` | File written/created | session:`4dfbd6b1`
- **00:52Z** | `src/ecs/systems_reproduction.cpp` | File written/created | session:`4dfbd6b1`
- **00:59Z** | `src/ecs/systems.cpp` | File written/created | session:`4dfbd6b1`
- **00:59Z** | `src/ecs/systems.h` | Edited: '// Individual system registration functions (also available ' -> '// Individual system registration functions (also available ' | session:`4dfbd6b1`
- **01:33Z** | `src/ecs/systems_lifecycle.cpp` | Edited: '#include "systems.h" #include "components.h" #include "sim/a' -> '#include "systems.h" #include "components.h" #include "sim/a' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems_lifecycle.cpp` | Edited: '            // Check normal boids for promotion             ' -> '            // Check normal boids for promotion             ' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems.h` | Edited: 'void register_render_sync_system(flecs::world& world); ' -> 'void register_render_sync_system(flecs::world& world); void ' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems.cpp` | Edited: '    // OnStore     register_render_sync_system(world); } ' -> '    // OnStore     register_render_sync_system(world);      ' | session:`414c9a66`

## 2026-02-19

- **01:34Z** | `src/ecs/systems_render_sync.cpp` | Edited: '            // Clear previous frame data             rs.boid' -> '            // Copy current stats             rs.stats = w.g' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems_reproduction.cpp` | Edited: '            std::mt19937& rng = sim_rng();             float' -> '            std::mt19937& rng = sim_rng();             std::' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                    // Spawn offspring                     s' -> '                    // Spawn offspring                     f' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                    std::uniform_real_distribution<float> di' -> '                    for (int i = 0; i < count; ++i) {       ' | session:`414c9a66`
- **01:34Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                    // Spawn offspring — inherit AntivaxBo' -> '                    // Spawn offspring — inherit AntivaxBo' | session:`414c9a66`
- **01:35Z** | `src/ecs/systems_reproduction.cpp` | Edited: '            std::vector<std::pair<uint64_t, float>> neighbor' -> '            std::vector<SpatialGrid::QueryResult> neighbors;' | session:`414c9a66`
- **01:35Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                for (const auto& [nid, dist] : neighbors) { ' -> '                for (const auto& qr : neighbors) {          ' | session:`414c9a66`
- **01:35Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                for (const auto& [nid, dist] : neighbors) { ' -> '                for (const auto& qr : neighbors) {          ' | session:`414c9a66`
- **01:35Z** | `src/ecs/systems_reproduction.cpp` | Edited: '                for (const auto& [nid, dist] : neighbors) { ' -> '                for (const auto& qr : neighbors) {          ' | session:`414c9a66`
- **01:37Z** | `src/ecs/systems_steering.cpp` | File written/created | session:`414c9a66`
- **01:37Z** | `src/ecs/systems_infection.cpp` | File written/created | session:`414c9a66`
