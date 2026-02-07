# Shared Headers — API Contract
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Files
- components.h — all FLECS component structs + tags + singletons
- spatial_grid.h — SpatialGrid public API
- render_state.h — data the renderer reads each frame
- sim_config.h — all simulation parameters (if separate from components.h)

## Rules
- Changes here affect ALL modules — coordinate first
- Declarations only, no implementations
- Minimal includes

## Recent Changes
@changelog.md