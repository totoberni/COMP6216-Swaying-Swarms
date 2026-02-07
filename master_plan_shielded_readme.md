# Master Plan — COMP6216 Swaying Swarms
## Orchestrator-Driven Agent Development Pipeline

> **Audience:** A developer with Claude Code CLI and Claude Desktop installed (MAX subscription), starting from scratch.
> **Purpose:** Single authoritative plan synthesizing project spec, agent architecture, orchestration system, and phased implementation into one linear guide.

---

## Table of Contents

0. [Architecture Overview](#phase-0-architecture-overview)
1. [Environment & Dependencies](#phase-1-environment--dependencies)
2. [Repository Scaffolding](#phase-2-repository-scaffolding)
3. [Orchestrator State Infrastructure](#phase-3-orchestrator-state-infrastructure)
4. [Memory Hierarchy — CLAUDE.md Files](#phase-4-memory-hierarchy)
5. [Agent Definitions & Slash Commands](#phase-5-agent-definitions--slash-commands)
6. [Automated Changelog Hooks](#phase-6-automated-changelog-hooks)
7. [Build System & Shared API Headers](#phase-7-build-system--shared-api-headers)
8. [Parallel Module Development via Worktrees](#phase-8-parallel-module-development)
9. [Integration & Wiring](#phase-9-integration--wiring)
10. [Behavior Rules & Simulation Logic](#phase-10-behavior-rules--simulation-logic)
11. [Extensions via Ralph Loop](#phase-11-extensions-via-ralph-loop)
12. [Quick Reference](#phase-12-quick-reference)
13. [Troubleshooting](#phase-13-troubleshooting)

---

## Phase 0 — Architecture Overview

### What We're Building

A real-time 2D boid pandemic simulation with two swarms (**Normal Boids**, **Doctor Boids**) studying optimal doctor count and behaviour to save a swarm from a pandemic. Interactive GUI with live stats.

### Tech Stack

| Component | Technology | Version |
|-----------|-----------|---------|
| Language | C++17 | — |
| ECS Framework | FLECS | v4.1.4 |
| Rendering | Raylib + raygui | 5.5 |
| Build System | CMake + CPM.cmake | 3.20+ |
| Agent Tooling | Claude Code CLI | Latest |
| Orchestrator Model | `opusplan` | Opus 4.6 |
| Worker Model | `sonnet` | Sonnet 4.5 |
| Scout Model | `haiku` | Haiku 4.5 |

### Module Map

| Directory | Responsibility | Owner Agent |
|-----------|---------------|-------------|
| `src/ecs/` | FLECS components, systems, world setup | ECS Agent |
| `src/sim/` | Infection, cure, reproduction, death logic | Main/Integration Agent |
| `src/spatial/` | Fixed-grid spatial index for collision detection | Spatial Agent |
| `src/render/` | Raylib window, boid drawing, stats overlay | Render Agent |
| `include/` | Shared headers — API contract between modules | Defined first, then read-only |
| `tests/` | Unit tests per module | Each agent writes its own |

### Management Architecture — SuperAgent → Agent → SubAgent

```
┌─────────────────────────────────────────────────┐
│  ORCHESTRATOR (opusplan, persistent tmux session)│
│  Reads: .orchestrator/state.md                   │
│  Updates: task-queue, active-tasks, decisions     │
│  Never writes project code directly               │
├─────────────────────────────────────────────────┤
│                     │                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Worker 1 │  │ Worker 2 │  │ Worker 3 │       │
│  │ Worktree │  │ Worktree │  │ Worktree │       │
│  │ (sonnet) │  │ (sonnet) │  │ (sonnet) │       │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘       │
│       │              │              │              │
│  SubAgents:     SubAgents:     SubAgents:         │
│  ecs-architect  cpp-builder    code-reviewer      │
│  debugger       code-reviewer  debugger           │
│  code-reviewer  debugger       cpp-builder        │
└─────────────────────────────────────────────────┘
```

**Key distinction:**
- **Orchestrator** = long-running Opus session managing state files and spawning workers
- **Workers** = independent Claude sessions in git worktrees (write-heavy, parallel)
- **SubAgents** = lightweight Claude instances within a worker session (read-heavy, specialists)

### Context Rot — The Core Problem

Reasoning quality degrades as context fills. Performance is strong through 50%, declines at 70–85%, collapses above 90%. Sessions last ~25–30 messages or 60–90 min before needing compaction.

**Defenses (all implemented in this plan):**
1. CLAUDE.md hierarchy — lazy-loads module context only when needed
2. Per-module changelogs — compact change history for session continuity
3. PostToolUse hooks — automatic changelog updates, zero agent context cost
4. Ralph Loop — fresh session per task, files on disk as only memory
5. External state file — orchestrator memory survives compaction/restarts

---

## Phase 1 — Environment & Dependencies

**Goal:** Verify all tools are installed and working.
**Who:** YOU, manually.
**Time:** ~10 minutes.

### Step 1.1 — Verify Toolchain

```bash
claude --version       # Claude Code CLI (already installed per notes)
cmake --version        # Need 3.20+
g++ --version          # Or clang++ — need C++17 support
git --version          # Need 2.17+ for worktrees
jq --version           # Required by changelog hooks
tmux -V                # For persistent orchestrator session
```

Install any missing tools:

```bash
# macOS
brew install cmake gcc git jq tmux

# Ubuntu/Debian
sudo apt update && sudo apt install -y cmake g++ git jq tmux

# Windows (via winget/choco)
winget install Kitware.CMake Git.Git stedolan.jq
```

### Step 1.2 — Verify Claude Authentication

```bash
claude   # Should open without prompting for auth
# Type "exit" to close
```

### Step 1.3 — Set Environment Variables for Orchestration

The project uses `.env` file for environment variable management. This keeps configuration project-specific, version-controlled (via `.env.example`), and git-safe.

**Initial Setup:**

```bash
# Create .env from template (first time only)
cp .env.example .env

# (Optional) Customize settings
nano .env
```

**Load Variables:**

```bash
source scripts/load-env.sh
```

**Verify:**

```bash
env | grep CLAUDE_CODE
```

**Expected Output:**
```
CLAUDE_CODE_SUBAGENT_MODEL=sonnet
CLAUDE_CODE_EFFORT_LEVEL=medium
```

**Updating Configuration:**

Method 1 - Quick update:
```bash
./scripts/update-env.sh CLAUDE_CODE_SUBAGENT_MODEL opus
./scripts/update-env.sh CLAUDE_CODE_EFFORT_LEVEL high
source scripts/load-env.sh
```

Method 2 - Manual edit:
```bash
nano .env
source scripts/load-env.sh
```

**Auto-Load on Terminal Startup (Optional):**

Add to `~/.bashrc` or `~/.zshrc`:
```bash
# Auto-load COMP6216 environment
if [ -f ~/COMP6216-Swaying-Swarms/scripts/load-env.sh ]; then
    source ~/COMP6216-Swaying-Swarms/scripts/load-env.sh
fi
```

Reload: `source ~/.zshrc` (or `~/.bashrc`)

**Why This Approach:**
- ✅ Project-specific configuration (won't affect other projects)
- ✅ Easy to update (edit `.env`, reload)
- ✅ Version control friendly (`.env.example` committed, `.env` gitignored)
- ✅ Team collaboration ready (share template, customize locally)
- ✅ No shell profile pollution

> **Note:** The `.env` file is automatically ignored by git (see `.gitignore`). Team members copy `.env.example` to `.env` and customize for their setup.

---

---

## Phase 2 — Repository Scaffolding

**Goal:** Create the complete directory structure for both the project and the agent management system.
**Who:** YOU, manually.
**Time:** ~15 minutes.

### Step 2.1 — Clone and Create Structure

```bash
git clone https://github.com/YOUR_ORG/COMP6216-Swaying-Swarms.git
cd COMP6216-Swaying-Swarms
```

### Step 2.2 — Create Full Directory Tree

```bash
# Agent management infrastructure
mkdir -p .claude/agents .claude/commands .claude/skills/flecs-patterns .claude/hooks .claude/rules

# Orchestrator state infrastructure
mkdir -p .orchestrator/inbox .orchestrator/outbox .orchestrator/status

# Project source directories
mkdir -p src/ecs src/sim src/spatial src/render
mkdir -p include tests assets cmake docs
```

Resulting structure:

```
COMP6216-Swaying-Swarms/
├── .claude/
│   ├── agents/                # SubAgent definitions (specialist roles)
│   ├── commands/              # Slash commands (/build, /review, /test)
│   ├── hooks/                 # PostToolUse automation scripts
│   ├── rules/                 # Path-scoped agent rules
│   └── skills/
│       └── flecs-patterns/    # Domain knowledge for ECS work
├── .orchestrator/
│   ├── state.md               # Orchestrator persistent memory
│   ├── task-queue.md          # Pending delegations
│   ├── active-tasks.md        # Running worker sessions + PIDs
│   ├── decisions.md           # Architecture decision log
│   ├── inbox/                 # Worker results deposited here
│   ├── outbox/                # Orchestrator assigns tasks here
│   └── status/                # Worker heartbeat/status files
├── src/
│   ├── ecs/                   # FLECS systems + world init
│   ├── sim/                   # Behavior logic (infection, cure, etc.)
│   ├── spatial/               # Spatial hash grid
│   └── render/                # Raylib rendering pipeline
├── include/                   # Shared headers (API contracts)
├── tests/                     # Unit tests
├── assets/                    # Sounds, textures
├── cmake/                     # CPM.cmake
├── docs/                      # Task specs for Ralph Loop
├── context.md                 # Project specification
├── README.md
└── ToDo.md
```

### Step 2.3 — Create Project Settings

Create `.claude/settings.json`:

```json
{
  "permissions": {
    "allow": [
      "Bash(cmake *)",
      "Bash(make *)",
      "Bash(cd *)",
      "Bash(git status)",
      "Bash(git diff *)",
      "Bash(git log *)",
      "Bash(git add *)",
      "Bash(git commit *)",
      "Bash(ls *)",
      "Bash(cat *)",
      "Bash(head *)",
      "Bash(tail *)",
      "Bash(find *)",
      "Bash(grep *)",
      "Bash(./build/*)",
      "Bash(jq *)"
    ],
    "deny": [
      "Bash(rm -rf /)",
      "Bash(git push *)",
      "Bash(git checkout main)",
      "Read(.env*)",
      "Read(secrets/*)"
    ]
  },
  "hooks": {
    "PostToolUse": [
      {
        "matcher": "Edit|MultiEdit|Write",
        "hooks": [
          {
            "type": "command",
            "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/update-changelog.sh"
          }
        ]
      }
    ]
  }
}
```

**Key decisions:** Agents can build and commit but cannot push or destroy. Hooks auto-log every file edit. You push manually after review.

### Step 2.4 — Commit Scaffolding

```bash
git add -A
git commit -m "chore: scaffold repo structure for orchestrated agent development"
git push origin main
```

---

## Phase 3 — Orchestrator State Infrastructure

**Goal:** Create the external state files that give the orchestrator persistent memory across compaction cycles and session restarts.
**Who:** YOU, manually.
**Time:** ~10 minutes.

### Step 3.1 — Orchestrator State File

Create `.orchestrator/state.md`:

```markdown
# Orchestrator State
<!-- Updated by orchestrator before every /compact or session end -->

## Current Phase
Phase 2 — Setup complete. Awaiting Phase 3 (agent definitions).

## Active Workers
None.

## Completed Tasks
- [x] Repository scaffolded
- [x] .claude/settings.json created

## Pending Tasks
- [ ] CLAUDE.md hierarchy
- [ ] Agent definitions
- [ ] Changelog hooks
- [ ] Shared API headers
- [ ] Parallel module development
- [ ] Integration
- [ ] Behavior rules
- [ ] Extensions

## Blocking Issues
None.

## Key Decisions
- C++17, FLECS v4.1.4, Raylib 5.5
- opusplan for orchestrator, sonnet for workers, haiku for scouts
- File-based inter-agent communication via .orchestrator/inbox/outbox
```

### Step 3.2 — Task Queue

Create `.orchestrator/task-queue.md`:

```markdown
# Task Queue
<!-- Orchestrator reads this to decide next delegation -->

## Queued (in priority order)
1. Define shared API headers (include/*.h) — blocks all parallel work
2. ECS core: components, systems, world init — feature/ecs-core
3. Spatial grid: hash grid + unit tests — feature/spatial-grid
4. Rendering: Raylib renderer + stats overlay — feature/rendering
5. Integration: wire modules together
6. Normal Boid behavior rules
7. Doctor Boid behavior rules
8. Extensions: debuffs, sex, antivax, UI sliders

## In Progress
None.

## Completed
None.
```

### Step 3.3 — Active Tasks Tracker

Create `.orchestrator/active-tasks.md`:

```markdown
# Active Tasks
<!-- Tracks running worker sessions, PIDs, worktree locations -->

| Worker | Branch | Worktree Path | Session ID | PID | Status |
|--------|--------|---------------|------------|-----|--------|
| — | — | — | — | — | — |
```

### Step 3.4 — Decisions Log

Create `.orchestrator/decisions.md`:

```markdown
# Architecture Decisions

## ADR-001: ECS Framework
**Decision:** FLECS v4.1.4 (C++17 API)
**Rationale:** Built-in ECS with pipeline phases, tag components, singleton support. Lightweight. Active development.

## ADR-002: Rendering
**Decision:** Raylib 5.5 + raygui
**Rationale:** Simple 2D rendering, no heavy dependencies. raygui for immediate-mode stats overlay. Upgrade path to Dear ImGui via rlImGui.

## ADR-003: Spatial Partitioning
**Decision:** Fixed-cell hash grid, cell size = max interaction radius
**Rationale:** O(1) insert, O(neighbors) query. Flat arrays for cache efficiency. Pure C++ module, no FLECS dependency.

## ADR-004: Agent Isolation
**Decision:** Git worktrees for parallel write-heavy work, subagents for read-heavy parallel tasks
**Rationale:** Worktrees prevent file conflicts between agents. Subagents share checkout but get isolated context windows.
```

### Step 3.5 — Orchestrator Agent Definition

Create `.claude/agents/orchestrator.md`:

```markdown
---
name: orchestrator
description: Master coordinator that delegates all implementation work. Never writes project code directly.
tools: Read, Bash, Glob, Grep, Task
model: opusplan
permissionMode: default
---

You are the master orchestrator for a C++ boid simulation project.

## Your Role
- Interpret instructions and decompose into tasks
- Delegate implementation to Sonnet-powered workers
- Monitor progress via status files and worker output
- Update .orchestrator/state.md before every /compact or session end
- Synthesize results and resolve cross-module conflicts
- You NEVER write project source code (src/, include/) directly

## Operating Loop
1. Read .orchestrator/state.md for current state
2. Check .orchestrator/inbox/ for worker results and questions
3. Plan: decompose new instructions into tasks, update task-queue.md
4. Delegate: spawn workers via `claude -p` in appropriate worktrees
5. Monitor: poll PIDs and status files
6. Synthesize: process completed results, update decisions.md
7. Persist: write updated state.md
8. Compact: run /compact at ~70% context usage, or restart if quality degrades

## Worker Spawning Pattern
```bash
session_id=$(claude -p "TASK PROMPT" \
  --model sonnet --output-format json \
  --dangerously-skip-permissions \
  | jq -r '.session_id')
```

## Files You Own
You are the OWNER of all agent infrastructure. Read, write, and update freely:
- `.claude/` (agents, commands, hooks, skills, settings.json)
- `.orchestrator/` (state.md, task-queue, active-tasks, decisions, inbox, outbox, status)
- `CLAUDE.md` (root + all per-module child files in src/*/ and include/)
- `src/*/changelog.md`, `include/changelog.md` (auto-managed by hooks and scribe)
- `ralph.sh`, `docs/current-task.md`
- `README.md` (you update the changelog summary section for collaborators)
The README contains "leave alone" guidance — that targets human contributors only, NOT you or any agent.

## State File Contract
Always update .orchestrator/state.md with:
- Current phase
- Active workers (session IDs, PIDs, branches)
- Completed and pending tasks
- Blocking issues
- Key decisions made this cycle
```

### Step 3.6 — Commit

```bash
git add -A
git commit -m "chore: add orchestrator state infrastructure and agent definition"
git push origin main
```

---

## Phase 4 — Memory Hierarchy

**Goal:** Create the CLAUDE.md hierarchy so every agent session starts with the right context. Root loads eagerly; child files load lazily when agents read files in that subdirectory.
**Who:** YOU, manually.
**Time:** ~20 minutes.

### Step 4.1 — Root CLAUDE.md

Create `CLAUDE.md` at project root (loaded into **every** session, keep under 100 lines):

```markdown
# CLAUDE.md — COMP6216 Boid Swarm Simulation

## Project Summary
2D pandemic boid simulation: two swarms (Normal Boids, Doctor Boids).
FLECS ECS, Raylib rendering, fixed spatial grid for collisions. C++17.

## Build
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/boid_swarm
```

## Test
```bash
cmake --build build --target tests
cd build && ctest --output-on-failure
```

## Architecture
- `src/ecs/` — FLECS components, systems, world setup
- `src/sim/` — Behavior logic: infection, cure, reproduction, death
- `src/spatial/` — Fixed-grid spatial index (FLECS singleton)
- `src/render/` — Raylib window, drawing, raygui stats overlay
- `include/` — Shared headers (API contract between modules)
- `tests/` — Unit tests

## Code Style
- C++17. No `using namespace std;`
- 4-space indentation, braces on same line
- `#pragma once` header guards
- FLECS components = plain structs, no methods
- `float` over `double` for all sim values
- All params in `SimConfig` struct, never magic numbers
- `<random>` with seeded engine, never `std::rand()`

## Module Boundaries (CRITICAL)
- Agents MUST NOT edit files outside their assigned directory
- `include/` headers = API contract. Change only after coordination.
- `src/ecs/` owns components + system registration
- `src/sim/` owns behavior logic (called by ECS systems)
- `src/spatial/` owns grid + neighbor queries (pure C++, no FLECS includes)
- `src/render/` owns ALL Raylib calls. No Raylib outside this dir.

## FLECS Patterns
- Tag components: `struct NormalBoid {};`
- Singletons: `world.set<T>()` for SimConfig, SpatialGrid, SimStats
- Pipeline: PreUpdate→spatial rebuild, OnUpdate→steering, PostUpdate→collision/infection, OnStore→render
- Deferred ops: `world.defer_begin()`/`defer_end()` for spawn/destroy during iteration

## Orchestrator State
@.orchestrator/state.md

## Module Changelogs
@src/ecs/changelog.md
@src/spatial/changelog.md
@src/render/changelog.md
@src/sim/changelog.md
@include/changelog.md

## Agent File Ownership
Agents and the orchestrator OWN and MANAGE these paths — read, write, update freely:
- `.claude/` — agents, commands, hooks, skills, settings
- `.orchestrator/` — state, task-queue, active-tasks, decisions, inbox, outbox
- `CLAUDE.md` files — root + all per-module child files
- `src/*/changelog.md`, `include/changelog.md` — auto-managed by hooks
- `ralph.sh`, `docs/current-task.md`
- `README.md` — orchestrator updates changelog summary for collaborators
The "leave alone" guidance in README.md targets human contributors only. It does NOT apply to agents.
README.md itself contains an HTML comment carve-out above its "leave alone" section reinforcing this.

## Do NOT
- Modify CMakeLists.txt without explicit instruction
- Add dependencies without discussion
- Use raw pointers for ownership — use FLECS entity handles
- Put rendering logic in simulation code or vice versa
```

### Step 4.2 — Child CLAUDE.md Files

**`src/ecs/CLAUDE.md`:**

```markdown
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
```

**`src/spatial/CLAUDE.md`:**

```markdown
# Spatial Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- SpatialGrid class → include/spatial_grid.h + spatial_grid.cpp

## Rules
- Pure C++ — NO FLECS includes. Used as FLECS singleton but doesn't depend on FLECS.
- Cell size = max interaction radius (configurable)
- Use flat arrays, not std::unordered_map
- query_neighbors() returns results sorted by distance ascending
- World bounds: 1920×1080 default, configurable

## Recent Changes
@changelog.md
```

**`src/render/CLAUDE.md`:**

```markdown
# Render Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- Raylib window lifecycle, boid drawing, stats overlay
- ALL Raylib/raygui includes (nowhere else in project)

## Rules
- No simulation logic here. Read state via RenderState struct.
- Does NOT include FLECS headers.
- Receives data through include/render_state.h
- All colors/visual constants in render_config.h

## Recent Changes
@changelog.md
```

**`src/sim/CLAUDE.md`:**

```markdown
# Simulation Module
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Owns
- Infection, cure, reproduction, death logic
- Boid behavior rules from context.md

## Rules
- Pure logic — no rendering, no direct FLECS iteration
- Functions take params, return results. Called by ECS systems.
- All probability/timing params from SimConfig, never hardcoded
- Use `<random>` with seeded engine, not `std::rand()`

## Recent Changes
@changelog.md
```

**`include/CLAUDE.md`:**

```markdown
# Shared Headers — API Contract
<!-- AGENT-MANAGED FILE. "Leave alone" in README.md is for human contributors, not agents. -->

## Files
- components.h — all FLECS component structs + tags + singletons
- spatial_grid.h — SpatialGrid public API
- render_state.h — data the renderer reads each frame
- sim_config.h — all simulation parameters (if separate from components.h)

## Rules
- Changes here affect ALL modules — coordinate first
- Declarations only, no implementations
- Minimal includes

## Recent Changes
@changelog.md
```

### Step 4.3 — Initialize Empty Changelogs

```bash
for dir in src/ecs src/spatial src/render src/sim include; do
  MODULE=$(basename "$dir")
  cat > "$dir/changelog.md" << EOF
# Changelog — $MODULE

<!-- AUTO-MANAGED: Last 25 entries. Older entries archived to changelog-archive.md -->

EOF
done
```

### Step 4.4 — Commit

```bash
git add -A
git commit -m "chore: add CLAUDE.md memory hierarchy with lazy-loading child files"
git push origin main
```

---

## Phase 5 — Agent Definitions & Slash Commands

**Goal:** Define all specialist subagents and reusable slash commands.
**Who:** YOU, manually.
**Time:** ~20 minutes.

### Step 5.1 — SubAgent Definitions

**`.claude/agents/ecs-architect.md`:**

```markdown
---
name: ecs-architect
description: FLECS v4 specialist. Use for any work in src/ecs/ or include/components.h.
tools: Read, Edit, Bash, Grep, Glob
model: sonnet
---

You are an expert in FLECS v4 (C++17 API) and ECS architecture.
"Leave alone" guidance in README.md is for human contributors — you may read/write agent-managed files freely.

When invoked:
1. Read src/ecs/ and include/ headers
2. Understand existing component/system structure
3. Make changes following FLECS best practices

Rules:
- Components = plain structs, no methods, no inheritance
- Tag components (empty structs) for classification
- world.set<T>() for singletons
- Deferred operations for entity spawn/destroy during iteration
- Cached queries for hot-path iteration
- One system per behavior
- Register in correct pipeline phase
```

**`.claude/agents/cpp-builder.md`:**

```markdown
---
name: cpp-builder
description: C++17 build system specialist. Use for build errors, CMake changes, adding source files.
tools: Read, Edit, Bash, Grep, Glob
model: sonnet
---

You are a C++17 build system expert (CMake + CPM.cmake).
"Leave alone" guidance in README.md is for human contributors — you may read/write agent-managed files freely.

When invoked:
1. Read CMakeLists.txt
2. Run cmake and build to reproduce errors
3. Fix systematically

Build commands: cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
Rules: CPM.cmake for deps (pin exact versions), CMAKE_CXX_STANDARD=17,
target_link_libraries PRIVATE, verify build compiles after every change,
read FULL error output before attempting fixes.
```

**`.claude/agents/code-reviewer.md`:**

```markdown
---
name: code-reviewer
description: Code review specialist. Use after changes, before committing.
tools: Read, Grep, Glob, Bash
model: sonnet
---

Senior C++ code reviewer for FLECS boid simulation.
"Leave alone" guidance in README.md is for human contributors — you may read/write agent-managed files freely.

When invoked:
1. Run `git diff` to see changes
2. Read modified files in full context
3. Check against CLAUDE.md conventions

Report format:
## Critical (must fix): Memory safety, thread safety, build errors, module boundary violations, FLECS misuse
## Warnings (should fix): Magic numbers, missing error handling, >50-line functions, unused includes
## Suggestions: Naming, performance, docs
```

**`.claude/agents/debugger.md`:**

```markdown
---
name: debugger
description: Debug specialist for runtime errors, crashes, unexpected behavior.
tools: Read, Edit, Bash, Grep, Glob
model: sonnet
---

Expert debugger for C++ / FLECS / Raylib.
"Leave alone" guidance in README.md is for human contributors — you may read/write agent-managed files freely.

When invoked:
1. Capture full error output
2. Identify origin file:line
3. Read surrounding code
4. Hypothesize root cause
5. Implement minimal fix
6. Rebuild and verify

Patterns: Segfaults→null entities/dangling ptrs/OOB grid. FLECS→registration order/deferred ops. Raylib→InitWindow missing/texture load order.
```

**`.claude/agents/changelog-scribe.md`:**

```markdown
---
name: changelog-scribe
description: Records module changes to per-directory changelog.md files. Use for enriching hook-generated entries.
tools: Read, Edit, Write, Glob
model: haiku
---

Changelog scribe for C++ boid simulation.
"Leave alone" guidance in README.md is for human contributors — you may read/write agent-managed files freely.

When invoked with changed files:
1. Group by module (src/ecs/, src/spatial/, src/render/, src/sim/, include/)
2. Read each module's changelog.md
3. Append one entry per logical change: timestamp, files, description
4. If >25 entries, archive oldest to changelog-archive.md

Format: `- **HH:MMZ** | \`filename\` | description | agent:caller-name`
```

### Step 5.2 — Slash Commands

**`.claude/commands/build.md`:**

```markdown
Build the project from scratch:
1. cmake -B build -DCMAKE_BUILD_TYPE=Debug
2. cmake --build build 2>&1

If build fails, analyze errors and fix. Rebuild after each fix until zero errors. Report final status.
```

**`.claude/commands/review.md`:**

```markdown
Use the code-reviewer subagent to review all uncommitted changes. Run `git diff` first, then review against CLAUDE.md conventions. Summarize findings and suggest fixes for critical issues.
```

**`.claude/commands/test.md`:**

```markdown
Run full test suite:
1. cmake --build build --target tests
2. cd build && ctest --output-on-failure

If any fail: identify root cause, fix source code (not test unless test is wrong), re-run until all pass.
```

**`.claude/commands/fix-issue.md`:**

```markdown
Investigate and fix: $ARGUMENTS

1. Search codebase for related code
2. Understand current implementation
3. Identify root cause
4. Fix following project conventions
5. Build to verify
6. Run tests
7. Use code-reviewer subagent to review
8. Commit: "fix: [brief description]"
```

### Step 5.3 — FLECS Skill

**`.claude/skills/flecs-patterns/SKILL.md`:**

```markdown
# FLECS Patterns for Boid Simulation

## Component Design
- Plain structs, no methods: `struct Position { float x, y; };`
- Tags: `struct NormalBoid {}; struct DoctorBoid {};`
- Singletons: `world.set<SimConfig>({...});`

## Pipeline Phases
- **PreUpdate**: Rebuild spatial grid
- **OnUpdate**: Boid steering, velocity integration, position update
- **PostUpdate**: Collision detection, infection, cure, reproduction, death
- **OnStore**: Render (read-only queries for drawing)

## Deferred Operations
```cpp
world.defer_begin();
// Safe to create/destroy entities here during iteration
world.defer_end();
```

## Cached Queries
```cpp
auto q = world.query_builder<Position, Velocity, NormalBoid>().build();
q.each([](Position& p, Velocity& v, NormalBoid) { /* ... */ });
```

## Entity Lifecycle
- Spawn via `world.entity().add<NormalBoid>().set<Position>({x,y}).set<Velocity>({vx,vy})`
- Kill via `e.destruct()` inside deferred block
- Classify via tag add/remove: `e.add<Infected>()`, `e.remove<Infected>()`
```

### Step 5.4 — Commit

```bash
git add -A
git commit -m "chore: add subagent definitions, slash commands, and FLECS skill"
git push origin main
```

---

## Phase 6 — Automated Changelog Hooks

**Goal:** PostToolUse hook auto-logs every file edit to the correct module's changelog.md. Zero agent context cost (hooks are shell processes).
**Who:** YOU, manually.
**Time:** ~10 minutes.

### Step 6.1 — Create Hook Script

Create `.claude/hooks/update-changelog.sh`:

```bash
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
```

```bash
chmod +x .claude/hooks/update-changelog.sh
```

**Note:** The hook config was already added to `.claude/settings.json` in Phase 2. The hook fires synchronously with a 60-second timeout (our append operation completes in milliseconds).

### Step 6.2 — Commit

```bash
git add -A
git commit -m "chore: add PostToolUse changelog hook for automatic change tracking"
git push origin main
```

---

## Phase 7 — Build System & Shared API Headers

**Goal:** Set up CMake + CPM.cmake, fetch FLECS and Raylib, define the shared header files that all modules code against. This MUST happen before parallel work.
**Who:** 1 AGENT in main checkout (you supervise).
**Time:** ~1–2 hours.

### Step 7.1 — Launch Agent for Build System + Headers

```bash
cd COMP6216-Swaying-Swarms
claude
```

Give this prompt:

```
Read CLAUDE.md and context.md thoroughly. You have TWO jobs:

JOB 1 — BUILD SYSTEM:
1. Download CPM.cmake into cmake/CPM.cmake (fetch from GitHub)
2. Create CMakeLists.txt that:
   - Sets CMAKE_CXX_STANDARD to 17
   - Uses CPM.cmake to fetch FLECS v4.1.4 (gh:SanderMertens/flecs@v4.1.4)
   - Uses CPM.cmake to fetch Raylib 5.5 (gh:raysan5/raylib@5.5)
   - Creates a main executable target: boid_swarm
   - Creates a test target
   - Properly links FLECS and Raylib
   - Adds all src/ subdirectories and include/ to include paths
3. Create a minimal src/main.cpp placeholder
4. Verify it compiles: cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build

JOB 2 — SHARED API HEADERS (the contract between ALL modules):
Create these headers in include/. These define the interfaces all modules code against.

include/components.h — ALL FLECS component structs:
  - Position { float x, y; }
  - Velocity { float vx, vy; }
  - Heading { float angle; }  // radians
  - Health { float age; float lifespan; }
  - InfectionState { bool infected; float time_infected; float time_to_death; }
  - ReproductionCooldown { float cooldown; }
  - Tag components: NormalBoid, DoctorBoid, Male, Female, Infected, Alive, Antivax
  - SimConfig singleton with ALL parameters from context.md:
    * p_initial_infect_normal, p_initial_infect_doctor
    * p_infect_normal, p_infect_doctor (0.5 each)
    * p_offspring_normal (0.4), p_offspring_doctor (0.05)
    * p_cure (0.8), p_become_doctor (0.05), p_antivax
    * r_interact_normal, r_interact_doctor
    * t_death, t_adult
    * mean/stddev for offspring counts: normal N(2,1), doctor N(1,1)
    * world_width (1920.0f), world_height (1080.0f)
    * initial_normal_count, initial_doctor_count
    * Boid movement params: max_speed, max_force, separation/alignment/cohesion weights+radii
  - SimStats singleton:
    * int normal_alive, doctor_alive
    * int dead_total, dead_normal, dead_doctor
    * int newborns_total, newborns_normal, newborns_doctor

include/spatial_grid.h — SpatialGrid API:
  - Constructor: SpatialGrid(float world_w, float world_h, float cell_size)
  - void clear()
  - void insert(uint64_t entity_id, float x, float y)
  - std::vector<std::pair<uint64_t, float>> query_neighbors(float x, float y, float radius)
    (returns entity_id + distance pairs, sorted by distance ascending)

include/render_state.h — Data struct renderer reads each frame:
  - struct BoidRenderData { float x, y, angle; uint32_t color; float radius; bool is_doctor; }
  - struct RenderState { std::vector<BoidRenderData> boids; SimStats stats; }

Use the ecs-architect subagent for FLECS component decisions.
Use the cpp-builder subagent for CMake issues.
After completing both jobs, use code-reviewer subagent to review.
Commit with separate messages:
  "feat(build): set up CMake with CPM.cmake, FLECS, and Raylib"
  "feat(include): define shared API headers for all modules"
```

### Step 7.2 — Verify and Push

After the agent completes, verify:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build
```

Then push:

```bash
git push origin main
```

---

## Phase 8 — Parallel Module Development via Worktrees

**Goal:** Build ECS core, spatial grid, and renderer in parallel. Three independent agents, each in its own worktree on its own branch.
**Who:** 3 AGENTS, supervised by you. Optionally managed by the orchestrator.
**Time:** 1–3 days.

### Step 8.1 — Create Worktrees

```bash
cd COMP6216-Swaying-Swarms
git checkout main && git pull

git worktree add ../boids-ecs -b feature/ecs-core
git worktree add ../boids-spatial -b feature/spatial-grid
git worktree add ../boids-render -b feature/rendering
```

### Step 8.2 — Option A: Manual Launch (3 Terminals)

**Terminal 1 — ECS Agent:**

```bash
cd ../boids-ecs
claude
```

```
Read CLAUDE.md and context.md. You are the ECS agent. You ONLY edit files in
src/ecs/. You can READ (not write) include/.

Tasks:
1. Create src/ecs/world.cpp — FLECS world initialization:
   - Register all components from include/components.h
   - Set SimConfig singleton with default parameter values
   - Set SimStats singleton (zeroed)
   - Create SpatialGrid singleton
2. Create src/ecs/systems.cpp — system stubs for each pipeline phase:
   - PreUpdate: RebuildGridSystem (stub — will call spatial grid clear/insert)
   - OnUpdate: SteeringSystem (boid flocking: separation, alignment, cohesion), MovementSystem (apply velocity to position)
   - PostUpdate: CollisionSystem (detect via spatial grid), InfectionSystem, CureSystem, ReproductionSystem, DeathSystem, DoctorPromotionSystem
   - OnStore: RenderSyncSystem (populate RenderState from FLECS queries)
3. Create src/ecs/spawn.cpp — functions to spawn initial boid populations
4. Create src/ecs/stats.cpp — SimStats update system
5. Wire main.cpp: init world → spawn boids → headless simulation loop (no render yet)
6. Verify it compiles and runs 1000 frames without crashing

Use ecs-architect subagent for FLECS decisions.
Use cpp-builder subagent for build issues.
After completing, use code-reviewer subagent.
Commit frequently: "feat(ecs): [description]"
```

**Terminal 2 — Spatial Agent:**

```bash
cd ../boids-spatial
claude
```

```
Read CLAUDE.md and context.md. You are the Spatial agent. You ONLY edit files
in src/spatial/ and tests/.

Tasks:
1. Implement SpatialGrid in src/spatial/spatial_grid.cpp matching include/spatial_grid.h:
   - Fixed-cell grid. Cell size = constructor param (typically max interaction radius)
   - clear(): reset grid each frame. Use vector::clear(), not dealloc.
   - insert(entity_id, x, y): place entity in correct cell
   - query_neighbors(x, y, radius): check cell + 8 neighbors, return vector<pair<entity_id, distance>> sorted by distance
   - Handle boundary: clamp to grid bounds, don't wrap
   - Internal: flat vector of vectors. cell_index = (int)(x/cell_size) + cols * (int)(y/cell_size)
2. Write unit tests in tests/test_spatial.cpp:
   - Empty grid returns no neighbors
   - Single entity found within radius
   - Entity outside radius not returned
   - Boundary entities handled correctly
   - 10000 entities: verify query returns correct results vs brute-force
   - Performance: 10000 entities, full rebuild + 1000 queries < 50ms
3. Compile and pass all tests

This is PURE C++ — NO FLECS includes in spatial code.
Use cpp-builder subagent for build issues.
After completing, use code-reviewer subagent.
Commit frequently: "feat(spatial): [description]"
```

**Terminal 3 — Render Agent:**

```bash
cd ../boids-render
claude
```

```
Read CLAUDE.md and context.md. You are the Render agent. You ONLY edit files
in src/render/.

Tasks:
1. Create src/render/renderer.h + renderer.cpp:
   - init_renderer(int width, int height, const char* title)
   - close_renderer()
   - begin_frame() / end_frame() wrapping BeginDrawing/EndDrawing
   - draw_boid(float x, float y, float angle, uint32_t color, float radius)
     (draw as small triangle pointing in direction of angle)
   - draw_interaction_radius(float x, float y, float radius, uint32_t color)
     (circle outline, semi-transparent)
   - draw_stats_overlay(const SimStats& stats)
     (raygui panel showing: #normal, #doctor, #dead_total/normal/doctor, #newborns_total/normal/doctor)
   - render_frame(const RenderState& state) — draws all boids + radii + stats
2. Create src/render/render_config.h — colors and visual constants:
   - Normal boid color (green), Doctor boid color (blue), Infected tint (red)
   - Background color (dark gray), Interaction radius alpha
3. Create src/render/render_demo.cpp — standalone demo:
   - Opens 1920×1080 window
   - Spawns 200 triangles moving randomly with wraparound
   - Draws them each frame with interaction radius circles
   - Shows a dummy stats overlay
4. Compile and run the demo

ALL Raylib includes stay in src/render/. Renderer reads include/render_state.h.
Does NOT include FLECS headers.
Use cpp-builder subagent for build issues.
After completing, use code-reviewer subagent.
Commit frequently: "feat(render): [description]"
```

### Step 8.3 — Option B: Orchestrator-Managed Launch

Start the orchestrator in a tmux session:

```bash
tmux new-session -s orchestrator
cd COMP6216-Swaying-Swarms
claude --agent orchestrator
```

Then instruct it:

```
Three worktrees are ready:
- ../boids-ecs (feature/ecs-core) — ECS agent territory: src/ecs/
- ../boids-spatial (feature/spatial-grid) — Spatial agent territory: src/spatial/, tests/
- ../boids-render (feature/rendering) — Render agent territory: src/render/

Spawn three workers, one per worktree. For each:
1. Use `claude -p` with --model sonnet and the appropriate task prompt
2. Capture session_id and PID
3. Update .orchestrator/active-tasks.md with worker details
4. Monitor output files for completion

Worker task prompts are the same as described for each terminal above.
After all three complete, update .orchestrator/state.md and task-queue.md.
```

### Step 8.4 — Monitor Progress

Check every 15–30 minutes:

```bash
for dir in ../boids-ecs ../boids-spatial ../boids-render; do
  echo "=== $(basename $dir) ==="
  (cd "$dir" && git log --oneline -5)
  echo ""
done
```

If an agent goes off track: press **Esc** in that terminal, give corrective feedback.

### Step 8.5 — Merge Branches (Dependency Order)

Once all agents complete:

```bash
cd COMP6216-Swaying-Swarms

# 1. ECS core first (foundational — other modules depend on component defs)
git merge feature/ecs-core
cmake -B build && cmake --build build   # Verify

# 2. Spatial grid (standalone module)
git merge feature/spatial-grid
cmake -B build && cmake --build build   # Verify

# 3. Rendering (standalone module)
git merge feature/rendering
cmake -B build && cmake --build build   # Verify
```

If merge conflicts occur, launch a Claude session:

```bash
claude
```

```
Merge conflicts exist after merging feature branches. Run `git diff` to see
conflicts. Resolve them, ensuring include/ headers are consistent. Build and
test after resolving.
```

### Step 8.6 — Post-Merge Integration Check

```bash
claude
```

```
All three module branches merged. Verify integration:
1. Build compiles clean (cmake -B build && cmake --build build)
2. Run all tests (cd build && ctest --output-on-failure)
3. Check include/ headers are consistent across all modules
4. Identify any missing connections between modules
5. List what needs to happen for integration (Phase 9)
```

### Step 8.7 — Clean Up Worktrees

```bash
git worktree remove ../boids-ecs
git worktree remove ../boids-spatial
git worktree remove ../boids-render
git push origin main
```

---

## Phase 9 — Integration & Wiring

**Goal:** Connect ECS systems to spatial grid and renderer. Get boids moving on screen.
**Who:** 1 AGENT in main checkout.
**Time:** ~1 day.

```bash
cd COMP6216-Swaying-Swarms
claude
```

```
Read CLAUDE.md and context.md. All modules are merged. Wire them together:

1. SPATIAL GRID INTEGRATION (src/ecs/systems.cpp → src/spatial/):
   - RebuildGridSystem (PreUpdate): each frame, clear() the SpatialGrid singleton,
     then iterate all Alive entities with Position and insert() each into the grid.
   - CollisionSystem (PostUpdate): for each Alive boid, call query_neighbors()
     to find entities within their interaction radius. Store collision pairs.

2. RENDER INTEGRATION (src/ecs/systems.cpp → src/render/):
   - RenderSyncSystem (OnStore): iterate all Alive boids, populate a RenderState
     struct with BoidRenderData for each (position, heading angle, color based on
     NormalBoid/DoctorBoid and Infected state, interaction radius).
   - Copy SimStats into RenderState.

3. MAIN LOOP WIRING (src/main.cpp):
   - Initialize FLECS world (call world init from src/ecs/)
   - Initialize renderer (call init_renderer from src/render/)
   - Spawn initial populations: initial_normal_count Normal Boids + initial_doctor_count Doctor Boids
     at random positions with random velocities
   - Main loop:
     * If Raylib window should close → break
     * Call world.progress() (runs all FLECS systems in pipeline order)
     * Call render_frame(render_state) with the populated RenderState
   - Cleanup: close renderer, destroy world

4. BOID STEERING (src/ecs/systems.cpp):
   - SteeringSystem: implement basic boid flocking using spatial grid queries:
     * Separation: steer away from nearby boids within separation radius
     * Alignment: match average heading of neighbors within alignment radius
     * Cohesion: steer toward center of mass of neighbors within cohesion radius
     * Clamp resulting force to max_force, apply to velocity, clamp speed to max_speed
   - MovementSystem: apply velocity to position each frame. Wrap at world bounds.

5. Spawn 200 Normal Boids (green) + 10 Doctor Boids (blue) at random positions.
   Verify they appear on screen, flock together, and move smoothly.

6. Build, run, fix any runtime issues.

Use ecs-architect subagent for FLECS wiring. Use debugger subagent for crashes.
Commit after each working milestone:
  "feat(integration): wire spatial grid to ECS systems"
  "feat(integration): wire renderer to ECS world"
  "feat(integration): implement boid flocking steering"
  "feat(integration): complete main loop with rendering"
```

After this phase, you should see boids flocking on screen. Push:

```bash
git push origin main
```

---

## Phase 10 — Behavior Rules & Simulation Logic

**Goal:** Implement all Normal Boid and Doctor Boid rules from context.md.
**Who:** 1–2 AGENTS.
**Time:** 1–2 days.

### Step 10.1 — Behavior Implementation

The simulation behavior functions live in `src/sim/` and are called by ECS systems in `src/ecs/systems.cpp`. Each rule gets its own file for clarity.

```bash
claude
```

```
Read CLAUDE.md and context.md THOROUGHLY — every parameter and rule matters.
Implement ALL behavior rules. Create functions in src/sim/ called by ECS systems.

INFECTION LOGIC (src/sim/infection.cpp):
- At spawn: Normal boids have p_initial_infect_normal chance of starting infected.
  Doctor boids have p_initial_infect_doctor chance.
- When two boids collide (distance < r_interact for their type):
  * If one is Infected and the other is not:
    Normal→Normal: p_infect_normal (0.5) chance of spreading
    Doctor→Doctor: p_infect_doctor (0.5) chance of spreading
  * Cross-swarm infection does NOT happen (normal cannot infect doctor or vice versa
    — only cure interactions cross swarms)
- Infected boids get InfectionState.time_infected incremented each frame.

DEATH LOGIC (src/sim/death.cpp):
- When InfectionState.time_infected >= t_death → entity dies.
- Death means: remove Alive tag, increment SimStats.dead_normal or dead_doctor.
- Use deferred operations for entity destruction.

CURE LOGIC (src/sim/cure.cpp):
- When a Doctor collides with ANY Infected boid (normal OR doctor):
  * p_cure (0.8) chance of curing them
  * Curing: set InfectionState.infected = false, reset time_infected
  * Doctors cannot cure healthy boids (no-op if target not infected)
  * Doctors CAN cure other sick doctors

REPRODUCTION LOGIC (src/sim/reproduction.cpp):
- Normal-Normal collision: p_offspring_normal (0.4) chance of reproducing
  * Spawn N(2,1) children (clamp to ≥0), each child is a NormalBoid
  * Position: near parents. Random velocity.
- Doctor-Doctor collision: p_offspring_doctor (0.05) chance
  * Spawn N(1,1) children (clamp to ≥0), each child is a DoctorBoid
- Cross-swarm: Normal+Doctor do NOT reproduce
- IF two infected parents meet AND they infect each other AND produce offspring:
  * Children are subject to contagion from only ONE parent (roll p_infect for one parent only)
- Increment SimStats.newborns_normal or newborns_doctor

DOCTOR PROMOTION (src/sim/promotion.cpp):
- When a Normal Boid's Health.age >= t_adult:
  * p_become_doctor (0.05) chance per frame of becoming a Doctor
  * Promotion: remove NormalBoid tag, add DoctorBoid tag
  * Keep all other state (position, velocity, health, infection)

AGING (src/sim/aging.cpp):
- Every frame: increment Health.age for all alive boids
- Every frame: increment InfectionState.time_infected for infected boids

IMPLEMENTATION ORDER (build+test after each):
1. Aging system
2. Death system + unit test
3. Infection system + unit test
4. Cure system + unit test
5. Reproduction system + unit test
6. Doctor promotion system
7. Full integration test: run 5000 frames, log stats, verify counts are sane

Use the ecs-architect subagent for system registration.
Use the code-reviewer subagent after completing all rules.
Commit per logical unit: "feat(sim): implement infection spread logic"
```

### Step 10.2 — Verify Simulation

After implementation, run and observe:

```bash
./build/boid_swarm
```

Expected behavior:
- Green boids (normal) flock together
- Blue boids (doctor) flock separately
- Some boids turn red-tinted (infected)
- Infected boids near doctors sometimes lose red tint (cured)
- Boids disappear after t_death if infected and uncured
- New boids appear (reproduction)
- Stats overlay updates in real-time

Push:

```bash
git push origin main
```

---

## Phase 11 — Extensions via Ralph Loop

**Goal:** Autonomously implement extensions using the Ralph Loop — each iteration gets a fresh context window, using files on disk as persistent memory.
**Time:** Ongoing, autonomous.

### Step 11.1 — Create Task Spec

Create `docs/current-task.md`:

```markdown
# Current Task: Implement Extensions

Read CLAUDE.md for full project context. Check src/*/changelog.md for recent changes.

## Requirements (implement ONE per session, in order)
- [ ] Infected debuffs — Doctors: reduce p_cure by 50%, reduce r_interact_doctor by 30%, reduce p_offspring_doctor by 50%. Normal: reduce r_interact_normal by 20%, reduce p_offspring_normal by 50%. Store debuff multipliers in SimConfig.
- [ ] Sex system: add Male/Female tags. 50/50 at spawn. Reproduction requires one Male + one Female in the collision pair. Same-sex collisions skip reproduction.
- [ ] Antivax boids: p_antivax percentage of Normal boids get Antivax tag at spawn. Antivax boids add a strong repulsion force from any DoctorBoid within visual range (modified steering). They can still be cured if a doctor reaches them.
- [ ] Parameter sliders: add raygui sliders to the stats overlay panel for: p_infect_normal, p_cure, r_interact_normal, r_interact_doctor, initial_normal_count, initial_doctor_count. Changing a slider updates SimConfig singleton in real-time.
- [ ] Pause/Reset controls: add Pause button (freezes simulation, rendering continues), Reset button (destroys all entities, re-spawns from SimConfig).
- [ ] Population graph: add a small real-time line chart (raygui or manual drawing) showing normal_alive and doctor_alive over time (last 500 frames).

## Guardrails
- Do NOT break existing simulation rules
- Do NOT modify existing component struct fields — only ADD new components/fields
- All new parameters go in SimConfig
- Build and test after EACH change
- Update the relevant module's changelog.md
- Commit with descriptive message before finishing
- If all tasks checked, output RALPH_COMPLETE
```

### Step 11.2 — Create Ralph Loop Script

Create `ralph.sh`:

```bash
#!/bin/bash
# ralph.sh — Stateless development loop. Each iteration = fresh context.
set -euo pipefail

PROMPT="docs/current-task.md"
MAX_ITERATIONS=30
ITERATION=0

while [ $ITERATION -lt $MAX_ITERATIONS ]; do
  ITERATION=$((ITERATION + 1))
  echo "=== Ralph iteration $ITERATION ($(date -u '+%H:%M:%SZ')) ==="

  OUTPUT=$(claude -p "$(cat "$PROMPT")

Read CLAUDE.md for project context. Check src/*/changelog.md for recent changes by other sessions.
Find the NEXT UNCHECKED task in the requirements list above.
Implement ONLY that single task.
Build the project. Run tests. Fix any failures.
Update the relevant changelog.md files.
Mark the task as checked in docs/current-task.md by changing [ ] to [x].
Commit with a descriptive message: 'feat(scope): description'
If ALL tasks are checked, output RALPH_COMPLETE.
Do NOT work on more than one task per session." 2>&1)

  echo "$OUTPUT" | tail -5

  if echo "$OUTPUT" | grep -q "RALPH_COMPLETE"; then
    echo "=== All tasks complete after $ITERATION iterations. ==="
    break
  fi

  echo "--- Iteration $ITERATION done. Fresh session starting... ---"
  sleep 3
done

echo "Ralph loop finished. $ITERATION iterations completed."
```

```bash
chmod +x ralph.sh
```

### Step 11.3 — Run the Ralph Loop

```bash
./ralph.sh
```

**What happens each iteration:**
1. Fresh Claude session starts (clean 200K context window)
2. Reads CLAUDE.md + @imports (project context + changelogs)
3. Reads docs/current-task.md
4. Finds next unchecked requirement
5. Implements it, builds, tests, commits
6. Marks task complete in the file
7. Exits. Loop starts fresh.

**If Ralph makes a repeating mistake**, add a guardrail to docs/current-task.md:

```markdown
## Guardrails
- ... existing ...
- NEW: The antivax steering must be ADDITIVE to existing flocking rules, not a replacement
```

### Step 11.4 — Orchestrator-Managed Ralph (Advanced)

For more control, have the orchestrator manage the Ralph Loop:

```bash
tmux new-session -s orchestrator
cd COMP6216-Swaying-Swarms
claude --agent orchestrator
```

```
Run a Ralph Loop for the extensions in docs/current-task.md.
For each iteration:
1. Read docs/current-task.md to find the next unchecked task
2. Spawn a worker: claude -p with the task prompt, --model sonnet
3. Capture output, check for success
4. Update .orchestrator/state.md with progress
5. If the worker fails, read the error, add a guardrail, retry
6. Continue until all tasks are complete
```

---

## Phase 12 — Quick Reference

### Session Commands

| Action | Command |
|--------|---------|
| New session | `claude` |
| Resume last | `claude --continue` |
| Pick session | `claude --resume` |
| Plan mode (read-only) | `claude --permission-mode plan` |
| Headless (scripting) | `claude -p "prompt" --output-format json` |
| With specific agent | `claude --agent orchestrator` |
| Interrupt | **Esc** |
| Compact context | `/compact` |
| Rename session | `/rename my-name` |
| Extended thinking | Type "think hard about" in prompt |

### Slash Commands (defined in .claude/commands/)

| Command | What it does |
|---------|-------------|
| `/build` | Full cmake rebuild, auto-fix errors |
| `/review` | Delegates to code-reviewer subagent |
| `/test` | Run ctest, auto-fix failures |
| `/fix-issue [desc]` | Search → understand → fix → test → review → commit |

### Git Worktrees

| Action | Command |
|--------|---------|
| Create | `git worktree add ../name -b branch-name` |
| List | `git worktree list` |
| Remove | `git worktree remove ../name` |
| Force remove | `git worktree remove --force ../name` |

### Prompting Patterns for Agents

```
# Explore before implementing
> Before implementing X, explore the codebase to understand how Y works

# Enforce boundaries
> You ONLY edit files in src/ecs/. Do NOT touch src/render/ or src/spatial/.

# Delegate to subagents
> Use the code-reviewer subagent to review changes in src/ecs/

# Request parallel subagents
> Use 3 parallel subagents to explore src/ecs/, src/sim/, and src/spatial/

# Verify after changes
> After making changes, build and run tests to verify

# Extended thinking for hard problems
> Think hard about this: how should the spatial grid handle boids at world edges?
```

### File Map

```
CLAUDE.md                           ← Root: every session loads this
  @.orchestrator/state.md           ← Orchestrator persistent memory
  @src/*/changelog.md               ← Module change histories

src/ecs/CLAUDE.md                   ← Lazy: loads when agent reads src/ecs/
src/spatial/CLAUDE.md               ← Lazy: loads when agent reads src/spatial/
src/render/CLAUDE.md                ← Lazy: loads when agent reads src/render/
src/sim/CLAUDE.md                   ← Lazy: loads when agent reads src/sim/
include/CLAUDE.md                   ← Lazy: loads when agent reads include/

.claude/agents/*.md                 ← SubAgent specialist definitions
.claude/commands/*.md               ← Slash command templates
.claude/hooks/update-changelog.sh   ← PostToolUse auto-logging
.claude/settings.json               ← Permissions + hook config
.claude/skills/flecs-patterns/      ← Domain knowledge for ECS

.orchestrator/state.md              ← Orchestrator reads this every cycle
.orchestrator/task-queue.md         ← What to work on next
.orchestrator/active-tasks.md       ← Running workers tracker
.orchestrator/decisions.md          ← Architecture decision log
.orchestrator/inbox/                ← Worker deposits results here
.orchestrator/outbox/               ← Orchestrator assigns tasks here

docs/current-task.md                ← Ralph Loop task file
ralph.sh                            ← Ralph Loop script
```

### Model Selection

| Role | Model | When |
|------|-------|------|
| Orchestrator | `opusplan` | Planning, delegation, state management |
| Implementation workers | `sonnet` | All code writing in worktrees |
| Read-only scouts | `haiku` | File exploration, changelog maintenance |
| Hard architecture decisions | `opus` with `effort: high` | Complex design, tricky bugs |

### Token Budget (200K window)

| Component | ~Tokens | Notes |
|-----------|---------|-------|
| System prompt | 8,500 | Fixed |
| CLAUDE.md + imports | 4,000–10,000 | Keep root < 100 lines |
| Tool schemas | 5,000–17,000 | Fixed |
| **Available for work** | **~140,000–150,000** | Degrades after 70% fill |
| 25-entry changelog | ~1,750 | Per module |
| Compaction savings | ~50% | Lossy — external state file is safer |

---

## Phase 13 — Troubleshooting

| Problem | Fix |
|---------|-----|
| **Agent edits wrong files** | Repeat boundary in prompt: "You ONLY edit src/ecs/. Do NOT touch src/render/." Add to CLAUDE.md Module Boundaries. |
| **Agent loops reading same files** | Context full. Exit, start fresh session. Use `/compact` mid-session. |
| **Build fails, agent stuck** | Run build yourself, copy FULL error, paste: "Here is the exact error. Fix this." |
| **Merge conflicts between worktrees** | File ownership wasn't clean. Resolve manually. Have agent verify: "Check include/ headers are consistent." |
| **Agent runs forever without progress** | Press **Esc**. Ask: "What have you done? What's blocking you?" Give direct instruction. |
| **Changelogs not updating** | `chmod +x .claude/hooks/update-changelog.sh`. Verify `jq` installed. Restart Claude session (hooks snapshot at start). |
| **SubAgent results too verbose** | "Use code-reviewer subagent. Return ONLY critical issues, no suggestions." |
| **Ralph Loop repeats mistake** | Add guardrail to docs/current-task.md: "Do NOT [specific mistake]" |
| **Hit rate limits** | You're on MAX — switch to off-peak hours or reduce parallel workers from 3→2. Use `effort: low` for routine orchestrator decisions. |
| **Child CLAUDE.md not loading** | Known reliability issue. Add explicit `@src/ecs/CLAUDE.md` in root CLAUDE.md as fallback. |
| **Orchestrator context degrading** | Update .orchestrator/state.md, then restart session entirely (don't just compact). Fresh session + state file = full recovery. |
| **Worker spawned by orchestrator fails** | Read output JSON. Add error to .orchestrator/decisions.md as lesson learned. Add guardrail to task prompt. Retry. |
| **Cross-module API mismatch after merge** | Single Claude session: "Read all include/*.h and all files that reference these headers. Fix any inconsistencies." |

---

## Phase Summary

| Phase | What | Who | Time |
|-------|------|-----|------|
| **0** | Architecture overview | READ | — |
| **1** | Environment & dependencies | YOU | 10 min |
| **2** | Repository scaffolding | YOU | 15 min |
| **3** | Orchestrator state infrastructure | YOU | 10 min |
| **4** | CLAUDE.md memory hierarchy | YOU | 20 min |
| **5** | Agent definitions & slash commands | YOU | 20 min |
| **6** | Automated changelog hooks | YOU | 10 min |
| **7** | Build system & shared API headers | 1 AGENT | 1–2 hrs |
| **8** | Parallel module development (ECS, Spatial, Render) | 3 AGENTS | 1–3 days |
| **9** | Integration & wiring | 1 AGENT | 1 day |
| **10** | Behavior rules (infection, cure, reproduction, death) | 1–2 AGENTS | 1–2 days |
| **11** | Extensions via Ralph Loop | RALPH LOOP | Ongoing |

**Manual setup time (Phases 1–6):** ~85 minutes.
**Agent development time (Phases 7–10):** ~4–7 days.
**Phase 11:** Autonomous, ongoing.

---

## Simulation Parameters Reference

All parameters from context.md, collected for SimConfig implementation:

| Parameter | Symbol | Default | Description |
|-----------|--------|---------|-------------|
| Initial infection (normal) | `p_initial_infect_normal` | 0.05 | Chance normal boid starts infected |
| Initial infection (doctor) | `p_initial_infect_doctor` | 0.02 | Chance doctor starts infected |
| Infection spread (normal) | `p_infect_normal` | 0.50 | Chance of infecting on collision |
| Infection spread (doctor) | `p_infect_doctor` | 0.50 | Chance of infecting on collision |
| Reproduction (normal) | `p_offspring_normal` | 0.40 | Chance of reproduction on collision |
| Reproduction (doctor) | `p_offspring_doctor` | 0.05 | Chance of reproduction on collision |
| Cure probability | `p_cure` | 0.80 | Doctor cures sick boid on collision |
| Doctor promotion | `p_become_doctor` | 0.05 | Adult normal boid becomes doctor |
| Antivax percentage | `p_antivax` | 0.10 | Normal boids that avoid doctors |
| Interact radius (normal) | `r_interact_normal` | 30.0f | Collision detection radius |
| Interact radius (doctor) | `r_interact_doctor` | 40.0f | Doctor interaction radius |
| Death time | `t_death` | 300.0f | Frames until infected boid dies |
| Adult age | `t_adult` | 500.0f | Frames until eligible for doctor promotion |
| Offspring count (normal) | `N(2,1)` | — | Normal distribution, mean=2, std=1 |
| Offspring count (doctor) | `N(1,1)` | — | Normal distribution, mean=1, std=1 |
| World size | `world_width × world_height` | 1920×1080 | Simulation bounds |
| Initial normal count | `initial_normal_count` | 200 | Starting normal boid population |
| Initial doctor count | `initial_doctor_count` | 10 | Starting doctor boid population |
| Max speed | `max_speed` | 3.0f | Boid velocity cap |
| Max force | `max_force` | 0.1f | Steering force cap |
| Separation weight | `separation_weight` | 1.5f | Flocking: avoid nearby boids |
| Alignment weight | `alignment_weight` | 1.0f | Flocking: match neighbor heading |
| Cohesion weight | `cohesion_weight` | 1.0f | Flocking: steer toward group center |

### Behavior Matrix

| Event | Normal×Normal | Doctor×Doctor | Doctor×Normal |
|-------|:---:|:---:|:---:|
| Infection spread | p_infect_normal | p_infect_doctor | ✗ |
| Reproduction | p_offspring_normal, N(2,1) kids | p_offspring_doctor, N(1,1) kids | ✗ |
| Cure | ✗ | p_cure (doctor cures doctor) | p_cure (doctor cures normal) |

### Extension Debuffs (when infected)

| Parameter | Normal Debuff | Doctor Debuff |
|-----------|:---:|:---:|
| r_interact | ×0.8 | ×0.7 |
| p_offspring | ×0.5 | ×0.5 |
| p_cure | — | ×0.5 |
