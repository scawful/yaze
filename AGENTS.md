# AGENTS.md (Protocol Router)

Purpose: route agents to the minimum correct context for the current task.
Do not treat this file as a repo overview or style dump.

## Core Principle
- Prefer hierarchical context loading over monolithic prompts.
- Load only what is needed for the current task surface.
- If a file is discoverable from code search, do not duplicate it here.

## Layer 1: Protocol Router

### 1) Task Classification
Classify the task into one dominant surface:
- `ui_ux_editor`
- `build_ci_release`
- `ai_agent_cli`
- `emu_runtime_debug`
- `rom_gameplay_data`
- `testing_harness`
- `docs_process`

### 2) Persona Selection (Primary Owner)
Use one primary owner from `docs/internal/agents/personas.md`:
- `imgui-frontend-engineer`: ImGui/editor UX, panel behavior, interaction patterns.
- `backend-infra-engineer`: CMake/toolchains, packaging, CI/CD, release plumbing.
- `ai-infra-architect`: z3ed CLI/TUI, agent workflows, model/provider plumbing.
- `snes-emulator-expert`: emulator runtime, performance, rendering correctness.
- `zelda3-hacking-expert`: ROM/gameplay logic, dungeon/overworld data behavior.
- `test-infrastructure-expert`: test architecture, flakes, harness reliability.
- `docs-janitor`: docs, process hygiene, onboarding and migration docs.

### 3) Focused Context Loading
Load only:
1. `.claude/agents/<agent-id>.md`
2. `docs/internal/agents/routing-personas.md`
3. Relevant entries in `docs/internal/agents/routing-skills-tools.md`

### 4) Tool Routing
Use tool classes intentionally:
- Code/search/build/test: shell + project scripts.
- Universe coordination: `scripts/agents/coord`.
- Legacy import/migration: `scripts/agents/import-coordination-board.sh`, `scripts/agents/migrate-coordination-board.sh`.

### 5) Essential Repo Facts (non-discoverable defaults)
- Build presets commonly used for agent work: `build_ai`.
- Coordination source of truth: `~/.context/agent-universe/{events.jsonl,state.json}`.
- Generated human snapshot: `docs/internal/agents/coordination-board.generated.md`.
- Legacy board file is history-only: `docs/internal/agents/coordination-board.md`.

### 6) Dependency Graph
`Task Class` -> `Primary Persona` -> `Focused Context Files` -> `Tools/Scripts` -> `Validation`.

Concretely:
- `AGENTS.md` -> `.claude/agents/<id>.md` + routing docs -> scripts/tools -> tests/build checks.
- Coordination state flows through universe events/state; markdown snapshot is derived output only.

## Layer 2: Focused Persona/Skill Context
- Persona routing rules: `docs/internal/agents/routing-personas.md`.
- Skill/tool routing rules: `docs/internal/agents/routing-skills-tools.md`.
- Persona definitions: `docs/internal/agents/personas.md`.

Load the smallest subset that can complete the task.

## Layer 3: Maintenance Agent
Owner: `ai-infra-architect` (with `docs-janitor` support).

Maintenance responsibilities:
- Keep protocol routing files current when scripts/personas/tools change.
- Prevent stale references to archived/legacy workflows.
- Run protocol checks:
  - `scripts/agents/protocol-audit.sh`
  - `scripts/agents/test-universe-coord.sh`

## Coordination Contract (Universe Log)
Use project wrapper:
- `scripts/agents/coord task-add --title "..."`
- `scripts/agents/coord task-claim --id <task_id> --agent <agent-id>`
- `scripts/agents/coord task-heartbeat --id <task_id> --agent <agent-id>`
- `scripts/agents/coord task-handoff --id <task_id> --agent <agent-id> --to <agent-id>`
- `scripts/agents/coord task-complete --id <task_id> --agent <agent-id>`
- `scripts/agents/coord task-list --status active`

Generate markdown snapshot only when humans need it:
- `scripts/agents/coord task-generate-board --out docs/internal/agents/coordination-board.generated.md`

## Delivery Contract
At completion, report:
- What changed.
- What was validated (exact commands).
- Residual risks and next follow-ups.
