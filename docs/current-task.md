# Current Task: Phase 12 — Refinements & Review Fixes

Read CLAUDE.md for full project context. Check src/*/changelog.md for recent changes.

## Requirements (implement ONE per session, in order)
- [ ] Remove FLECS include from renderer — `src/render/renderer.cpp` line 5 includes `<flecs.h>`, violating module boundaries. Fix: pass `SimConfig*` and `SimulationState*` through `RenderState` (in `include/render_state.h`). Populate in `register_render_sync_system()` (in `src/ecs/systems.cpp`). Then remove `<flecs.h>` and the `flecs::world*` cast from renderer.cpp. The `void* world_ptr` parameter can become unused or removed.
- [ ] Expand slider ranges — In `src/render/renderer.cpp`, change `r_interact_normal` and `r_interact_doctor` slider ranges from 10-100 to 5-200 to allow wider experimentation.
- [ ] Smooth population graph Y-scale — In `src/render/renderer.cpp`, the population graph `max_pop` rescales every frame causing visual jumps. Use a smoothed/damped max: `smoothed_max = max(smoothed_max * 0.99, current_max)` so the graph settles gradually. Use a `static` variable inside `draw_population_graph()`. Reset it when `history_count < 2`.
- [ ] Add keyboard shortcuts — In `src/main.cpp`, add `IsKeyPressed(KEY_SPACE)` for pause toggle and `IsKeyPressed(KEY_R)` for reset. Update the button labels in renderer.cpp to show the shortcuts: "Pause (SPACE)" / "Resume (SPACE)" and "Reset (R)".
- [ ] Also delete dead boids on reset — In `src/ecs/spawn.cpp`, `reset_simulation()` only deletes entities with `Alive` tag. Dead boids (those without `Alive` but still existing) are left behind. Add a second query for entities WITHOUT `Alive` that have `NormalBoid` or `DoctorBoid` to clean them up too. Or use a simpler approach: query all entities with `Position` component.

## Guardrails
- Do NOT break existing simulation rules
- Do NOT modify existing component struct fields — only ADD new components/fields
- All new parameters go in SimConfig
- Build and test after EACH change
- Update the relevant module's changelog.md
- Commit with descriptive message before finishing: "fix(scope): description" or "feat(scope): description"
- Do NOT add Raylib includes outside src/render/
- Do NOT add FLECS includes in src/render/ (this is the WHOLE POINT of task 1)
- If all tasks checked, output RALPH_COMPLETE
- All FLECS singleton types MUST be default-constructible (add `T() = default;` + member initializers)
- The render module receives data ONLY through `RenderState` — no direct FLECS access
- When modifying `render_state.h`, remember it is in `include/` (shared API contract)
- `void* world_ptr` was a temporary workaround — task 1 eliminates it
