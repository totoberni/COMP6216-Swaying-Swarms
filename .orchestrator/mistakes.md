# Worker Mistake Log
<!-- Orchestrator records mistakes AFTER fixing them, to inform future delegations. -->
<!-- Before spawning a worker, check their table. Add prevention rules to task prompts. -->
<!-- Never delete entries — patterns compound and inform guardrails. -->

## How to Use This File

1. When a worker produces incorrect output and you fix the problem, add a row to that worker's table.
2. Before delegating to a worker, scan their table for recurring patterns.
3. Incorporate "Prevention Rule" text directly into the worker's task prompt as a guardrail.
4. If a pattern appears 3+ times across any workers, escalate to a permanent guardrail in the relevant CLAUDE.md file.

---

### ECS Worker (code-worker on feature/ecs-core)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Spatial Worker (code-worker on feature/spatial-grid)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Render Worker (code-worker on feature/rendering)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Integration Worker (code-worker on main)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Behavior/Sim Worker (code-worker on sim rules)

| # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Ralph Loop Iterations

| # | Iteration | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|-----------|-----------------|------------|-------------|-----------------|
| — | — | No mistakes recorded yet | — | — | — |

### Subagents (code-reviewer, debugger, ecs-architect, cpp-builder, changelog-scribe)

| # | Subagent | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
|---|----------|-------|-----------------|------------|-------------|-----------------|
| — | — | — | No mistakes recorded yet | — | — | — |

---

## Cross-Cutting Patterns
<!-- Promote recurring mistakes here when they appear 3+ times across workers. -->
<!-- These become permanent guardrails in CLAUDE.md or worker prompt templates. -->

None identified yet.