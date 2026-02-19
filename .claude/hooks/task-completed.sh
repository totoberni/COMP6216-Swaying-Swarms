#!/bin/bash
# TaskCompleted hook: verify build passes before allowing task completion.
# Exit 0 = allow, exit 2 = reject completion.
set -uo pipefail

PROJECT_DIR="${CLAUDE_PROJECT_DIR:-$(pwd)}"

# Only check if build directory exists (skip on fresh checkouts)
if [ ! -d "$PROJECT_DIR/build" ]; then
  exit 0
fi

cmake --build "$PROJECT_DIR/build" 2>&1 | tail -5
if [ ${PIPESTATUS[0]} -ne 0 ]; then
  echo "Build failed — cannot complete task until build is clean." >&2
  exit 2
fi
exit 0
