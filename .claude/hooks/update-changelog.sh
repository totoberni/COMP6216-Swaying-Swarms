#!/bin/bash
# PostToolUse hook: auto-appends changelog entries on every file edit
set -euo pipefail

INPUT=$(cat)

TOOL_NAME=$(echo "$INPUT" | jq -r '.tool_name // "unknown"')
FILE_PATH=$(echo "$INPUT" | jq -r '.tool_input.file_path // empty')
SESSION_ID=$(echo "$INPUT" | jq -r '.session_id // "unknown"')

[ -z "$FILE_PATH" ] && exit 0

# Prevent recursion — skip changelog files
BASENAME=$(basename "$FILE_PATH")
case "$BASENAME" in
  changelog.md|changelog-archive.md|CHANGELOG.md) exit 0 ;;
esac

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"
REL_PATH="${FILE_PATH#$PROJECT_DIR/}"

# Route to correct module changelog
case "$REL_PATH" in
  src/ecs/*)      CHANGELOG="$PROJECT_DIR/src/ecs/changelog.md" ;;
  src/sim/*)      CHANGELOG="$PROJECT_DIR/src/sim/changelog.md" ;;
  src/spatial/*)   CHANGELOG="$PROJECT_DIR/src/spatial/changelog.md" ;;
  src/render/*)    CHANGELOG="$PROJECT_DIR/src/render/changelog.md" ;;
  include/*)       CHANGELOG="$PROJECT_DIR/include/changelog.md" ;;
  *)               exit 0 ;;
esac

TIMESTAMP=$(date -u '+%H:%MZ')
DATE=$(date -u '+%Y-%m-%d')

# Build description
case "$TOOL_NAME" in
  Write)     DESC="File written/created" ;;
  Edit)
    OLD=$(echo "$INPUT" | jq -r '.tool_input.old_string // empty' | head -c 60 | tr '\n' ' ')
    NEW=$(echo "$INPUT" | jq -r '.tool_input.new_string // empty' | head -c 60 | tr '\n' ' ')
    DESC="Edited: '${OLD}' -> '${NEW}'" ;;
  MultiEdit) DESC="Multiple edits applied" ;;
  *)         DESC="$TOOL_NAME operation" ;;
esac

# Create changelog if missing
if [ ! -f "$CHANGELOG" ]; then
  MODULE=$(basename "$(dirname "$CHANGELOG")")
  printf "# Changelog — %s\n\n<!-- AUTO-MANAGED -->\n\n" "$MODULE" > "$CHANGELOG"
fi

# Add date header if not present
if ! grep -q "## $DATE" "$CHANGELOG"; then
  printf "\n## %s\n\n" "$DATE" >> "$CHANGELOG"
fi

# Append entry
echo "- **${TIMESTAMP}** | \`${REL_PATH}\` | ${DESC} | session:\`${SESSION_ID:0:8}\`" >> "$CHANGELOG"

# Truncation: keep last 25 entries
ENTRY_COUNT=$(grep -c '^\- \*\*' "$CHANGELOG" 2>/dev/null || echo 0)
if [ "$ENTRY_COUNT" -gt 25 ]; then
  ARCHIVE="${CHANGELOG%.md}-archive.md"
  head -n 3 "$CHANGELOG" > "$CHANGELOG.tmp"
  tail -n 30 "$CHANGELOG" >> "$CHANGELOG.tmp"
  mv "$CHANGELOG.tmp" "$CHANGELOG"
fi

exit 0