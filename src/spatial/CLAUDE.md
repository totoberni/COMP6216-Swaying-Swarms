# Spatial Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- SpatialGrid class → include/spatial_grid.h + spatial_grid.cpp

## Rules
- Pure C++ — NO FLECS includes. Used as FLECS singleton but doesn't depend on FLECS.
- Cell size = max interaction radius (configurable)
- Use flat arrays, not std::unordered_map
- query_neighbors() fills a caller-supplied vector (unsorted, amortized zero-alloc)
- World bounds: 1920×1080 default, configurable

## Recent Changes
@changelog.md