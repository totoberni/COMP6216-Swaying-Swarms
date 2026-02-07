#!/bin/bash
# scripts/update-env.sh
# =====================
# Convenience script for updating specific environment variables
# 
# Usage:
#   ./scripts/update-env.sh VARIABLE_NAME VALUE
#   
# Examples:
#   ./scripts/update-env.sh CLAUDE_CODE_SUBAGENT_MODEL opus
#   ./scripts/update-env.sh CLAUDE_CODE_EFFORT_LEVEL high

# Get the project root directory (parent of scripts/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
ENV_FILE="${PROJECT_ROOT}/.env"

# Check arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 VARIABLE_NAME VALUE"
    echo ""
    echo "Example:"
    echo "  $0 CLAUDE_CODE_SUBAGENT_MODEL opus"
    echo "  $0 CLAUDE_CODE_EFFORT_LEVEL high"
    echo ""
    echo "Available variables:"
    echo "  - CLAUDE_CODE_SUBAGENT_MODEL (haiku|sonnet|opus)"
    echo "  - CLAUDE_CODE_EFFORT_LEVEL (low|medium|high)"
    echo "  - CLAUDE_CODE_ORCHESTRATOR_MODEL (haiku|sonnet|opus)"
    echo "  - CLAUDE_CODE_WORKER_MODEL (haiku|sonnet|opus)"
    echo "  - CLAUDE_CODE_SCOUT_MODEL (haiku|sonnet|opus)"
    exit 1
fi

VAR_NAME="$1"
VAR_VALUE="$2"

# Check if .env exists
if [ ! -f "$ENV_FILE" ]; then
    echo "Error: .env file not found at $ENV_FILE"
    echo "Creating from .env.example..."
    if [ -f "${PROJECT_ROOT}/.env.example" ]; then
        cp "${PROJECT_ROOT}/.env.example" "$ENV_FILE"
        echo "✓ Created .env from .env.example"
    else
        echo "Error: .env.example also not found!"
        exit 1
    fi
fi

# Check if variable exists in .env
if grep -q "^${VAR_NAME}=" "$ENV_FILE" 2>/dev/null; then
    # Update existing variable
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        sed -i '' "s/^${VAR_NAME}=.*/${VAR_NAME}=${VAR_VALUE}/" "$ENV_FILE"
    else
        # Linux
        sed -i "s/^${VAR_NAME}=.*/${VAR_NAME}=${VAR_VALUE}/" "$ENV_FILE"
    fi
    echo "✓ Updated: ${VAR_NAME}=${VAR_VALUE}"
elif grep -q "^# ${VAR_NAME}=" "$ENV_FILE" 2>/dev/null; then
    # Uncomment and update commented variable
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS
        sed -i '' "s/^# ${VAR_NAME}=.*/${VAR_NAME}=${VAR_VALUE}/" "$ENV_FILE"
    else
        # Linux
        sed -i "s/^# ${VAR_NAME}=.*/${VAR_NAME}=${VAR_VALUE}/" "$ENV_FILE"
    fi
    echo "✓ Enabled and set: ${VAR_NAME}=${VAR_VALUE}"
else
    # Add new variable
    echo "${VAR_NAME}=${VAR_VALUE}" >> "$ENV_FILE"
    echo "✓ Added: ${VAR_NAME}=${VAR_VALUE}"
fi

echo ""
echo "To apply changes to your current shell, run:"
echo "  source scripts/load-env.sh"
