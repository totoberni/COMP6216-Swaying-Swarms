# Changelog — include
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-08
- **RenderState extension**: Added SimConfig* and SimulationState* pointer fields to RenderState struct in render_state.h. These pointers are populated by register_render_sync_system() to provide renderer access to config/state without FLECS dependency.
- **Population graph**: Added PopulationHistoryPoint struct and history tracking fields to SimStats (circular buffer of 500 frames) in components.h
- **Pause/Reset controls**: Added SimulationState singleton with is_paused and reset_requested flags to components.h
- **Antivax system**: Added antivax_repulsion_radius and antivax_repulsion_weight to SimConfig. Antivax tag already existed in components.h
- **Infected debuffs**: Added 5 new debuff multiplier fields to SimConfig in components.h

## 2026-02-07

- **22:31Z** | `include/spatial_grid.h` | Edited: 'class SpatialGrid { public:     SpatialGrid(float world_w, f' -> 'class SpatialGrid { public:     SpatialGrid(float world_w, f' | session:`d15cd9ab`
- **22:32Z** | `include/spatial_grid.h` | Edited: '#pragma once  #include <cstdint> #include <vector> #include ' -> '#pragma once  #include <cstdint> #include <vector> #include ' | session:`d15cd9ab`
- **22:32Z** | `include/spatial_grid.h` | Edited: 'class SpatialGrid { public:     SpatialGrid() = default;    ' -> 'class SpatialGrid { public:     SpatialGrid() = default;    ' | session:`d15cd9ab`
- **22:33Z** | `include/spatial_grid.h` | Edited: 'class SpatialGrid { public:     SpatialGrid() = default;    ' -> 'class SpatialGrid { public:     SpatialGrid() = default;    ' | session:`d15cd9ab`

## 2026-02-17

- **23:59Z** | `include/components.h` | Edited: 'struct Antivax {};  // =====================================' -> 'struct Antivax {}; struct AntivaxBoid {};  // Primary swarm ' | session:`f655d95a`
- **23:59Z** | `include/components.h` | Edited: 'struct PopulationHistoryPoint {     int normal_alive = 0;   ' -> 'struct PopulationHistoryPoint {     int normal_alive = 0;   ' | session:`f655d95a`

## 2026-02-18

- **00:00Z** | `include/components.h` | Edited: '    int newborns_normal = 0;     int newborns_doctor = 0;   ' -> '    int newborns_normal = 0;     int newborns_doctor = 0;   ' | session:`f655d95a`
- **00:03Z** | `include/render_state.h` | Edited: '    bool is_doctor; ' -> '    int swarm_type;  // 0=normal, 1=doctor, 2=antivax ' | session:`f655d95a`
- **00:17Z** | `include/components.h` | Edited: 'struct NormalBoid {}; struct DoctorBoid {}; struct Male {}; ' -> 'struct NormalBoid {}; struct DoctorBoid {}; struct Male {}; ' | session:`f655d95a`
