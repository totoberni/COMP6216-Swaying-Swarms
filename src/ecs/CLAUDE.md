# ECS Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- Component definitions → include/components.h
- System registration → systems.cpp
- World initialization → world.cpp

## Rules
- Components = plain C++ structs, no methods, no inheritance
- Systems take `flecs::iter&`, not raw `ecs_iter_t*`
- Register systems in pipeline phases (see root CLAUDE.md)
- Never allocate in system callbacks — use pre-allocated buffers
- Systems must be idempotent (safe for restart mid-work)
- One system per behavior

## Recent Changes
@changelog.md