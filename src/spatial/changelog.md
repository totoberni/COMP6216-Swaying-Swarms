# Changelog — spatial
<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->
## 2026-02-07

- **22:32Z** | `src/spatial/spatial_grid.cpp` | Edited: 'namespace {     // Small epsilon to prevent entities from la' -> 'namespace {     // Small epsilon to prevent entities from la' | session:`d15cd9ab`
- **22:32Z** | `src/spatial/spatial_grid.cpp` | Edited: '#include "spatial_grid.h" #include <cmath> #include <algorit' -> '#include "spatial_grid.h" #include <cmath> #include <algorit' | session:`d15cd9ab`
- **22:33Z** | `src/spatial/spatial_grid.cpp` | Edited: 'SpatialGrid::SpatialGrid(float world_w, float world_h, float' -> 'SpatialGrid::SpatialGrid(float world_w, float world_h, float' | session:`d15cd9ab`
- **22:33Z** | `src/spatial/spatial_grid.cpp` | Edited: '#include "spatial_grid.h" #include <cmath> #include <algorit' -> '#include "spatial_grid.h" #include <cmath> #include <algorit' | session:`d15cd9ab`

## 2026-02-18

- **23:31Z** | `src/spatial/spatial_grid.cpp` | Edited: '    // Check 3x3 grid centered on current cell     for (int ' -> '    // Dynamic search window: expand beyond 3x3 when radius ' | session:`4dfbd6b1`
- **23:31Z** | `src/spatial/spatial_grid.cpp` | Edited: '    // Determine cell range to check (current cell + 8 neigh' -> '    // Determine cell of query point ' | session:`4dfbd6b1`

## 2026-02-19

- **00:43Z** | `src/spatial/spatial_grid.cpp` | Edited: 'std::vector<std::pair<uint64_t, float>> SpatialGrid::query_n' -> 'void SpatialGrid::query_neighbors(float x, float y, float ra' | session:`4dfbd6b1`
- **00:43Z** | `src/spatial/spatial_grid.cpp` | Edited: '#include "spatial_grid.h" #include <cmath> #include <algorit' -> '#include "spatial_grid.h" #include <cmath> ' | session:`4dfbd6b1`
- **00:47Z** | `src/spatial/CLAUDE.md` | Edited: '- query_neighbors() returns results sorted by distance ascen' -> '- query_neighbors() fills a caller-supplied vector (unsorted' | session:`4dfbd6b1`
- **01:33Z** | `src/spatial/spatial_grid.cpp` | File written/created | session:`414c9a66`
