Record a process update to .orchestrator/state.md.

Parse the arguments as: worker_name | branch | status | description

Valid statuses: started, in-progress, completed, failed, blocked

Run the record-process hook:

```bash
echo "$ARGUMENTS" | "$CLAUDE_PROJECT_DIR"/.claude/hooks/record-process.sh
```

If no arguments provided, determine the update from context:
1. Check which task you are currently working on
2. Determine the appropriate status
3. Record the update:

```bash
echo '{"worker":"[your-name]","branch":"[current-branch]","status":"[status]","description":"[what happened]"}' | "$CLAUDE_PROJECT_DIR"/.claude/hooks/record-process.sh
```

Behavior:
- status=started or in-progress → updates the "Active Workers" table
- status=completed → moves the entry from "Active Workers" to "Jobs Complete"
- status=failed or blocked → flags the entry and adds to "Blocking Issues"