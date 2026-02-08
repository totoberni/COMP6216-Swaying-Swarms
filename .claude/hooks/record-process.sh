#!/bin/bash
# record-process.sh — Records process updates to .orchestrator/state.md
# Conditionally updates "Active Workers" or "Jobs Complete" tables
# depending on whether the process is starting/in-progress vs completed.
#
# Usage via slash command (/record-process):
#   Passes $ARGUMENTS as stdin with format:
#   "worker_name | branch | status | description"
#   Status: started | in-progress | completed | failed | blocked
#
# Usage via direct call:
#   echo '{"worker":"ecs-agent","branch":"feature/ecs-core","status":"completed","description":"ECS core done"}' | ./record-process.sh
#
# Usage via hook (SubagentStop):
#   Receives standard hook JSON on stdin
set -uo pipefail
# NOTE: -e removed intentionally — hooks must be best-effort and never block
# the parent Task tool result. grep/sed failures should not be fatal.

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
STATE_FILE="$PROJECT_DIR/.orchestrator/state.md"

# Ensure state file exists
if [ ! -f "$STATE_FILE" ]; then
  echo "ERROR: $STATE_FILE not found. Cannot record process." >&2
  exit 1
fi

TIMESTAMP=$(date -u '+%Y-%m-%d %H:%MZ')
SHORT_TS=$(date -u '+%H:%MZ')

# Parse input — support multiple formats
if [ -t 0 ] && [ $# -gt 0 ]; then
  # Called with arguments (from slash command $ARGUMENTS)
  # Expected format: "worker_name | branch | status | description"
  IFS='|' read -ra PARTS <<< "$*"
  WORKER=$(echo "${PARTS[0]:-unknown}" | xargs)
  BRANCH=$(echo "${PARTS[1]:-unknown}" | xargs)
  STATUS=$(echo "${PARTS[2]:-in-progress}" | xargs)
  DESCRIPTION=$(echo "${PARTS[3]:-no description}" | xargs)
  SESSION_ID="${CLAUDE_SESSION_ID:-manual}"
  PID="—"
else
  INPUT=$(cat)
  
  # Try hook JSON format (SubagentStop or Stop)
  if echo "$INPUT" | jq -e '.session_id' >/dev/null 2>&1; then
    SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')
    
    # Check if this is our custom JSON or hook JSON
    if echo "$INPUT" | jq -e '.worker' >/dev/null 2>&1; then
      # Custom JSON format
      WORKER=$(echo "$INPUT" | jq -r '.worker // "unknown"')
      BRANCH=$(echo "$INPUT" | jq -r '.branch // "unknown"')
      STATUS=$(echo "$INPUT" | jq -r '.status // "in-progress"')
      DESCRIPTION=$(echo "$INPUT" | jq -r '.description // "no description"')
      PID=$(echo "$INPUT" | jq -r '.pid // "—"')
    else
      # Hook JSON (SubagentStop) — no worker tracking data, exit gracefully.
      # These auto-generated entries were causing table format mismatches
      # and grep failures that blocked Task tool result reporting.
      exit 0
    fi
  else
    # Try pipe-delimited format
    IFS='|' read -ra PARTS <<< "$INPUT"
    WORKER=$(echo "${PARTS[0]:-unknown}" | xargs)
    BRANCH=$(echo "${PARTS[1]:-unknown}" | xargs)
    STATUS=$(echo "${PARTS[2]:-in-progress}" | xargs)
    DESCRIPTION=$(echo "${PARTS[3]:-no description}" | xargs)
    SESSION_ID="${CLAUDE_SESSION_ID:-unknown}"
    PID="—"
  fi
fi

# Normalize status to lowercase
STATUS=$(echo "$STATUS" | tr '[:upper:]' '[:lower:]')

# ============================================================
# CONDITIONAL UPDATE LOGIC
# ============================================================

case "$STATUS" in
  started|in-progress)
    # ---- UPDATE "Active Workers" TABLE ----
    
    # Check if "## Active Workers" section exists
    if ! grep -q "## Active Workers" "$STATE_FILE"; then
      # Section doesn't exist — append it before "## Completed Tasks" or at end
      if grep -q "## Completed Tasks\|## Jobs Complete\|## Pending Tasks" "$STATE_FILE"; then
        # Insert before the first of these sections
        INSERT_BEFORE=$(grep -n "## Completed Tasks\|## Jobs Complete\|## Pending Tasks" "$STATE_FILE" | head -1 | cut -d: -f1)
        CONTENT_BEFORE=$(head -n $((INSERT_BEFORE - 1)) "$STATE_FILE")
        CONTENT_AFTER=$(tail -n +$INSERT_BEFORE "$STATE_FILE")
        {
          echo "$CONTENT_BEFORE"
          echo ""
          echo "## Active Workers"
          echo ""
          echo "| Worker | Branch | Session ID | PID | Status | Last Update |"
          echo "|--------|--------|------------|-----|--------|-------------|"
          echo "| $WORKER | $BRANCH | \`${SESSION_ID:0:8}\` | $PID | $STATUS | $SHORT_TS |"
          echo ""
          echo "$CONTENT_AFTER"
        } > "$STATE_FILE.tmp"
        mv "$STATE_FILE.tmp" "$STATE_FILE"
      else
        # Append at end
        echo "" >> "$STATE_FILE"
        echo "## Active Workers" >> "$STATE_FILE"
        echo "" >> "$STATE_FILE"
        echo "| Worker | Branch | Session ID | PID | Status | Last Update |" >> "$STATE_FILE"
        echo "|--------|--------|------------|-----|--------|-------------|" >> "$STATE_FILE"
        echo "| $WORKER | $BRANCH | \`${SESSION_ID:0:8}\` | $PID | $STATUS | $SHORT_TS |" >> "$STATE_FILE"
      fi
    else
      # Section exists — check if this worker already has a row
      if grep -q "| $WORKER |" "$STATE_FILE"; then
        # Update existing row: replace the line matching this worker
        sed -i "s#^| $WORKER |.*#| $WORKER | $BRANCH | \`${SESSION_ID:0:8}\` | $PID | $STATUS | $SHORT_TS |#" "$STATE_FILE"
      else
        # Add new row after the table header separator
        # Find the "Active Workers" section's table separator line (|---|...)
        SECTION_LINE=$(grep -n "## Active Workers" "$STATE_FILE" | tail -1 | cut -d: -f1)
        # Find the separator line (|---|) after the section header
        SEPARATOR_LINE=$(tail -n +$SECTION_LINE "$STATE_FILE" | grep -n '^|[-|]*$' | head -1 | cut -d: -f1 || true)
        if [ -n "$SEPARATOR_LINE" ]; then
          ACTUAL_LINE=$((SECTION_LINE + SEPARATOR_LINE - 1))
          sed -i "${ACTUAL_LINE}a\\| $WORKER | $BRANCH | \`${SESSION_ID:0:8}\` | $PID | $STATUS | $SHORT_TS |" "$STATE_FILE"
        fi
      fi
    fi
    ;;

  completed|done)
    # ---- MOVE FROM "Active Workers" TO "Jobs Complete" ----
    
    # Remove from Active Workers if present
    if grep -q "| $WORKER |" "$STATE_FILE"; then
      sed -i "/| $WORKER |/d" "$STATE_FILE"
    fi
    
    # Check if "## Jobs Complete" section exists (also accept "## Completed Tasks")
    if grep -q "## Jobs Complete\|## Completed Tasks" "$STATE_FILE"; then
      # Find the section and append after its table separator
      SECTION_NAME=$(grep -o "## Jobs Complete\|## Completed Tasks" "$STATE_FILE" | head -1)
      SECTION_LINE=$(grep -n "$SECTION_NAME" "$STATE_FILE" | tail -1 | cut -d: -f1)
      
      # Check if it's a table format or checklist format
      if tail -n +$SECTION_LINE "$STATE_FILE" | head -10 | grep -q '^|'; then
        # Table format — add row after separator
        SEPARATOR_LINE=$(tail -n +$SECTION_LINE "$STATE_FILE" | grep -n '^|[-|]*$' | head -1 | cut -d: -f1 || true)
        if [ -n "$SEPARATOR_LINE" ]; then
          ACTUAL_LINE=$((SECTION_LINE + SEPARATOR_LINE - 1))
          sed -i "${ACTUAL_LINE}a\\| $WORKER | $BRANCH | $DESCRIPTION | $TIMESTAMP |" "$STATE_FILE"
        fi
      else
        # Checklist format — add as checked item
        # Find the last checklist item in this section
        NEXT_SECTION=$(tail -n +$((SECTION_LINE + 1)) "$STATE_FILE" | grep -n '^## ' | head -1 | cut -d: -f1)
        if [ -n "$NEXT_SECTION" ]; then
          INSERT_LINE=$((SECTION_LINE + NEXT_SECTION - 1))
        else
          INSERT_LINE=$(wc -l < "$STATE_FILE")
        fi
        sed -i "${INSERT_LINE}i\\- [x] ${DESCRIPTION} (${WORKER}, ${BRANCH}) — ${TIMESTAMP}" "$STATE_FILE"
      fi
    else
      # Section doesn't exist — create it
      echo "" >> "$STATE_FILE"
      echo "## Jobs Complete" >> "$STATE_FILE"
      echo "" >> "$STATE_FILE"
      echo "| Worker | Branch | Description | Completed |" >> "$STATE_FILE"
      echo "|--------|--------|-------------|-----------|" >> "$STATE_FILE"
      echo "| $WORKER | $BRANCH | $DESCRIPTION | $TIMESTAMP |" >> "$STATE_FILE"
    fi
    ;;

  failed|blocked)
    # ---- UPDATE Active Workers with failure status ----
    if grep -q "| $WORKER |" "$STATE_FILE"; then
      sed -i "s#^| $WORKER |.*#| $WORKER | $BRANCH | \`${SESSION_ID:0:8}\` | $PID | ⚠ $STATUS | $SHORT_TS |#" "$STATE_FILE"
    fi
    
    # Also append to Blocking Issues section if it exists
    if grep -q "## Blocking Issues" "$STATE_FILE"; then
      BLOCK_LINE=$(grep -n "## Blocking Issues" "$STATE_FILE" | tail -1 | cut -d: -f1)
      # Check if "None." is present and remove it
      NEXT_LINE=$((BLOCK_LINE + 1))
      if sed -n "${NEXT_LINE}p" "$STATE_FILE" | grep -qi "^none"; then
        sed -i "${NEXT_LINE}d" "$STATE_FILE"
      fi
      sed -i "${BLOCK_LINE}a\\- [$SHORT_TS] $WORKER ($BRANCH): $DESCRIPTION" "$STATE_FILE"
    fi
    ;;

  *)
    echo "WARNING: Unknown status '$STATUS'. Valid: started, in-progress, completed, failed, blocked" >&2
    exit 1
    ;;
esac

# Clean up: remove empty rows from Active Workers table
# (rows with only dashes or empty pipe-separated cells)
# Remove placeholder/empty rows (em-dash or en-dash, any column count)
sed -i '/^| — | No active workers/d' "$STATE_FILE"
sed -i '/^| [—–] | [—–] | [—–] /d' "$STATE_FILE"

exit 0