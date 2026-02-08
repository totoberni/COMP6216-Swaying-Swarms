#!/bin/bash
# record-mistake.sh — Records agent mistakes to .orchestrator/mistakes.md
# Appends rows to the correct worker section's table.
#
# Input formats (tries each in order):
#   1. Pipe-delimited args:  ./record-mistake.sh "worker | phase | what | cause | fix | rule"
#   2. JSON on stdin:        echo '{"worker":"ECS Worker","phase":"10","what":"...","cause":"...","fix":"...","rule":"..."}' | ./record-mistake.sh
#   3. SubagentStop hook JSON on stdin — exits silently (no-op) unless mistake data present
#
# Worker must match a ### section header in mistakes.md (partial match OK):
#   "orchestrator", "ecs", "spatial", "render", "integration", "behavior", "ralph", "subagent"
set -uo pipefail
# NOTE: -e removed intentionally — hooks must be best-effort and never block
# the parent Task tool result. Python or grep failures should not be fatal.

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
MISTAKES_FILE="$PROJECT_DIR/.orchestrator/mistakes.md"

# ─── Parse input ───────────────────────────────────────────────────────────────

WORKER="" PHASE="" WHAT="" CAUSE="" FIX="" RULE=""

if [ $# -gt 0 ] && [ -n "$*" ]; then
  # Pipe-delimited arguments
  IFS='|' read -r WORKER PHASE WHAT CAUSE FIX RULE <<< "$*"
  # Trim whitespace
  WORKER=$(echo "$WORKER" | xargs)
  PHASE=$(echo "$PHASE" | xargs)
  WHAT=$(echo "$WHAT" | xargs)
  CAUSE=$(echo "${CAUSE:-}" | xargs)
  FIX=$(echo "${FIX:-}" | xargs)
  RULE=$(echo "${RULE:-}" | xargs)
elif [ ! -t 0 ]; then
  INPUT=$(cat)
  [ -z "$INPUT" ] && exit 0

  # Try JSON parse
  if echo "$INPUT" | jq -e '.worker' >/dev/null 2>&1; then
    WORKER=$(echo "$INPUT" | jq -r '.worker // empty')
    PHASE=$(echo "$INPUT" | jq -r '.phase // empty')
    WHAT=$(echo "$INPUT" | jq -r '.what // .what_went_wrong // .description // empty')
    CAUSE=$(echo "$INPUT" | jq -r '.cause // .root_cause // empty')
    FIX=$(echo "$INPUT" | jq -r '.fix // .fix_applied // empty')
    RULE=$(echo "$INPUT" | jq -r '.rule // .prevention_rule // empty')
  else
    # SubagentStop hook JSON or unrecognized — no-op
    exit 0
  fi
else
  # No input at all
  exit 0
fi

# Must have at least worker and what-went-wrong
[ -z "$WORKER" ] && exit 0
[ -z "$WHAT" ] && exit 0

# ─── Resolve worker to section header ──────────────────────────────────────────

WORKER_LOWER=$(echo "$WORKER" | tr '[:upper:]' '[:lower:]')

# Map common aliases to the section header grep pattern
case "$WORKER_LOWER" in
  *orchestrator*|*self*)  SECTION_PATTERN="Orchestrator Self" ;;
  *ecs*)                  SECTION_PATTERN="ECS Worker" ;;
  *spatial*)              SECTION_PATTERN="Spatial Worker" ;;
  *render*)               SECTION_PATTERN="Render Worker" ;;
  *integration*)          SECTION_PATTERN="Integration Worker" ;;
  *behavior*|*sim*)       SECTION_PATTERN="Behavior/Sim Worker" ;;
  *ralph*)                SECTION_PATTERN="Ralph Loop" ;;
  *subagent*|*reviewer*|*debugger*|*architect*|*builder*|*scribe*)
                          SECTION_PATTERN="Subagents" ;;
  *)                      SECTION_PATTERN="$WORKER" ;;
esac

# ─── Verify file exists ───────────────────────────────────────────────────────

if [ ! -f "$MISTAKES_FILE" ]; then
  echo "ERROR: $MISTAKES_FILE not found" >&2
  exit 1
fi

# ─── Find section and insert row ──────────────────────────────────────────────

python3 << PYEOF
import re, sys

with open("$MISTAKES_FILE", "r", encoding="utf-8") as f:
    lines = f.readlines()

# Find the target section header
section_pattern = "$SECTION_PATTERN"
section_line = None
for i, line in enumerate(lines):
    if line.startswith("### ") and section_pattern.lower() in line.lower():
        section_line = i
        break

if section_line is None:
    print(f"WARNING: No section matching '{section_pattern}' found in mistakes.md", file=sys.stderr)
    sys.exit(0)

# Find the table rows after the header
# Skip header line, separator line (|---|...), find data rows
table_start = None
table_end = None
for i in range(section_line + 1, len(lines)):
    line = lines[i].strip()
    if line.startswith("|") and "---" in line:
        # This is the separator row — data starts after
        table_start = i + 1
    elif table_start is not None:
        if not line.startswith("|"):
            table_end = i
            break
        else:
            table_end = i + 1

if table_start is None:
    print("WARNING: No table found after section header", file=sys.stderr)
    sys.exit(0)

if table_end is None:
    table_end = len(lines)

# Count existing real rows (not placeholder)
existing_rows = []
placeholder_indices = []
for i in range(table_start, table_end):
    row = lines[i].strip()
    if not row.startswith("|"):
        continue
    cells = [c.strip() for c in row.split("|")[1:-1]]  # strip empty first/last from split
    if len(cells) >= 2 and cells[0] == "—" and "No mistakes recorded" in row:
        placeholder_indices.append(i)
    elif len(cells) >= 2 and cells[0] != "#":
        existing_rows.append(i)

next_num = len(existing_rows) + 1
# If placeholder exists, skip numbering from it
if placeholder_indices and not existing_rows:
    next_num = 1

# Build new row
phase = """$PHASE""" or "—"
what = """$WHAT"""
cause = """$CAUSE""" or "—"
fix = """$FIX""" or "—"
rule = """$RULE""" or "—"

# Detect if this is Ralph Loop (has Iteration column) or Subagents (has Subagent + Phase)
header_line = ""
for i in range(section_line + 1, table_start):
    if lines[i].strip().startswith("| #"):
        header_line = lines[i]
        break

if "Iteration" in header_line:
    # Ralph Loop table: | # | Iteration | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
    new_row = f"| {next_num} | {phase} | {what} | {cause} | {fix} | {rule} |\n"
elif "Subagent" in header_line:
    # Subagents table: | # | Subagent | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
    # Use worker name as subagent identifier
    subagent = """$WORKER"""
    new_row = f"| {next_num} | {subagent} | {phase} | {what} | {cause} | {fix} | {rule} |\n"
else:
    # Standard table: | # | Phase | What Went Wrong | Root Cause | Fix Applied | Prevention Rule |
    new_row = f"| {next_num} | {phase} | {what} | {cause} | {fix} | {rule} |\n"

# Remove placeholder rows
for idx in sorted(placeholder_indices, reverse=True):
    del lines[idx]
    if table_end > idx:
        table_end -= 1

# Insert new row at end of table
lines.insert(table_end, new_row)

with open("$MISTAKES_FILE", "w", encoding="utf-8") as f:
    f.writelines(lines)

print(f"Recorded mistake #{next_num} in '{section_pattern}'")

# ─── Pattern detection ─────────────────────────────────────────────────────
# Count total rows in this section after update
total = next_num
if total >= 3 and total % 3 == 0:
    print(f"⚠ Pattern alert: {section_pattern} has {total} mistakes. Consider promoting to Cross-Cutting Patterns.")
PYEOF

exit 0