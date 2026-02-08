Record a mistake to `.orchestrator/mistakes.md` under the correct worker's table.

If arguments are provided ($ARGUMENTS), pass them pipe-delimited to the hook:
```bash
"$CLAUDE_PROJECT_DIR"/.claude/hooks/record-mistake.sh "$ARGUMENTS"
```

Format: `worker | phase | what went wrong | root cause | fix applied | prevention rule`

Examples:
- `/record-mistake ECS Worker | 10 | Used std::rand() instead of mt19937 | Didn't read anti-patterns | Replaced with seeded generator | Always use <random> with SimConfig seed`
- `/record-mistake ralph | 11 | Broke flocking by replacing forces | Antivax implemented as replacement not additive | Made additive | Antivax steering is ADDITIVE to flocking`
- `/record-mistake orchestrator | 9 | Spawned worker without propagating hooks | Forgot Step 11.3 checklist | Re-ran with verification | Always verify hook propagation before worker launch`

If no arguments are provided, determine from the current context:
1. Which worker/section this mistake belongs to (orchestrator, ecs, spatial, render, integration, behavior, ralph, subagent)
2. The current phase number
3. What went wrong, the root cause, what fix was applied, and a prevention rule

Then pipe as JSON to the hook:
```bash
echo '{"worker":"...","phase":"...","what":"...","cause":"...","fix":"...","rule":"..."}' | "$CLAUDE_PROJECT_DIR"/.claude/hooks/record-mistake.sh
```