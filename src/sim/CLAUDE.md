# Simulation Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- Infection, cure, reproduction, death logic
- Boid behavior rules from context.md

## Rules
- Pure logic — no rendering, no direct FLECS iteration
- Functions take params, return results. Called by ECS systems.
- All probability/timing params from SimConfig, never hardcoded
- Use `<random>` with seeded engine, not `std::rand()`

## Recent Changes
@changelog.md