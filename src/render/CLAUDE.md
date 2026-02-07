# Render Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- Raylib window lifecycle, boid drawing, stats overlay
- ALL Raylib/raygui includes (nowhere else in project)

## Rules
- No simulation logic here. Read state via RenderState struct.
- Does NOT include FLECS headers.
- Receives data through include/render_state.h
- All colors/visual constants in render_config.h

## Recent Changes
@changelog.md