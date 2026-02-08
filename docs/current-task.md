# Current Task: Implement Extensions

Read CLAUDE.md for full project context. Check src/*/changelog.md for recent changes.

## Requirements (implement ONE per session, in order)
- [x] Infected debuffs — Doctors: reduce p_cure ×0.5, r_interact_doctor ×0.7, p_offspring_doctor ×0.5. Normal: r_interact_normal ×0.8, p_offspring_normal ×0.5. Store debuff multipliers in SimConfig.
- [x] Sex system: add Male/Female tags. 50/50 at spawn. Reproduction requires one Male + one Female. Same-sex collisions skip reproduction.
- [x] Antivax boids: p_antivax percentage of Normal boids get Antivax tag at spawn. Antivax boids add a strong repulsion force from DoctorBoid within visual range (ADDITIVE to existing flocking, not replacement). They can still be cured if a doctor reaches them.
- [x] Parameter sliders: raygui sliders in stats overlay for p_infect_normal, p_cure, r_interact_normal, r_interact_doctor, initial_normal_count, initial_doctor_count. Slider changes update SimConfig singleton in real-time.
- [ ] Pause/Reset controls: Pause button (freezes simulation, rendering continues). Reset button (destroys all entities, re-spawns from SimConfig).
- [ ] Population graph: real-time line chart (raygui or manual) showing normal_alive and doctor_alive over last 500 frames.

## Guardrails
- Do NOT break existing simulation rules
- Do NOT modify existing component struct fields — only ADD new components/fields
- All new parameters go in SimConfig
- Build and test after EACH change
- Update the relevant module's changelog.md
- Commit with descriptive message before finishing: "feat(scope): description"
- Use `<random>` with seeded engine, never `std::rand()`
- Antivax steering must be ADDITIVE to flocking rules, not a replacement
- Do NOT add Raylib includes outside src/render/
- If all tasks checked, output RALPH_COMPLETE
- Always add header-only libs as explicit CPM dependencies (never use hacky include paths)
- Hook scripts must not use `set -e` — use explicit error handling instead
- All FLECS singleton types MUST be default-constructible (add `T() = default;` + member initializers)
