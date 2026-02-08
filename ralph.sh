#!/bin/bash
# ralph.sh — Stateless development loop. Each iteration = fresh context.
# Created by orchestrator in Phase 11, Step 11.0
set -euo pipefail

PROMPT="docs/current-task.md"
MAX_ITERATIONS=30
ITERATION=0
LOG="docs/ralph-log.md"

# Initialize log
echo "# Ralph Loop Log" > "$LOG"
echo "Started: $(date -u '+%Y-%m-%d %H:%MZ')" >> "$LOG"
echo "" >> "$LOG"

while [ $ITERATION -lt $MAX_ITERATIONS ]; do
  ITERATION=$((ITERATION + 1))
  START_TIME=$(date -u '+%H:%M:%SZ')
  echo "=== Ralph iteration $ITERATION ($START_TIME) ==="

  OUTPUT=$(claude -p "$(cat "$PROMPT")

Read CLAUDE.md for project context. Check src/*/changelog.md for recent changes by other sessions.
Read .claude/skills/flecs-patterns/SKILL.md for domain patterns and parameter reference.

Find the NEXT UNCHECKED task in the requirements list above.
Implement ONLY that single task.
Build the project. Run tests. Fix any failures.
Update the relevant changelog.md files.
Mark the task as checked in docs/current-task.md by changing [ ] to [x].
Commit with a descriptive message: 'feat(scope): description'

If you encounter a repeated mistake or anti-pattern, record it:
Run: echo 'MISTAKE: [description]' so the orchestrator can add a guardrail.

If ALL tasks are checked, output RALPH_COMPLETE.
Do NOT work on more than one task per session." --model sonnet --dangerously-skip-permissions 2>&1)

  END_TIME=$(date -u '+%H:%M:%SZ')
  echo "$OUTPUT" | tail -5

  # Log iteration
  echo "## Iteration $ITERATION" >> "$LOG"
  echo "- Start: $START_TIME | End: $END_TIME" >> "$LOG"
  echo "- Output (last 3 lines):" >> "$LOG"
  echo "$OUTPUT" | tail -3 | sed 's/^/  /' >> "$LOG"
  echo "" >> "$LOG"

  # Check for mistakes to escalate
  if echo "$OUTPUT" | grep -q "MISTAKE:"; then
    MISTAKE=$(echo "$OUTPUT" | grep "MISTAKE:" | head -1)
    MISTAKE_DESC="${MISTAKE#MISTAKE: }"
    echo "⚠ Detected mistake: $MISTAKE_DESC"
    echo "- ⚠ $MISTAKE_DESC" >> "$LOG"
    # Append as guardrail to prevent repetition
    echo "- $MISTAKE_DESC" >> docs/current-task.md
    # Record in .orchestrator/mistakes.md
    echo "{\"worker\":\"ralph\",\"phase\":\"11\",\"what\":\"$MISTAKE_DESC\",\"cause\":\"Ralph iteration $ITERATION\",\"fix\":\"Added guardrail to current-task.md\",\"rule\":\"$MISTAKE_DESC\"}" \
      | .claude/hooks/record-mistake.sh 2>/dev/null || true
  fi

  if echo "$OUTPUT" | grep -q "RALPH_COMPLETE"; then
    echo "=== All tasks complete after $ITERATION iterations. ==="
    echo "" >> "$LOG"
    echo "## COMPLETE" >> "$LOG"
    echo "All tasks done after $ITERATION iterations at $(date -u '+%H:%M:%SZ')" >> "$LOG"
    break
  fi

  echo "--- Iteration $ITERATION done. Fresh session starting... ---"
  sleep 3
done

echo "Ralph loop finished. $ITERATION iterations completed."
