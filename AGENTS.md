# AGENTS.md (Protocol Router)

Purpose: route agents to the minimum correct context. Not a repo overview.

## Core Rules
1. Hierarchical context only — load what the task needs.
2. Prefer existing scripts/tools; keep edits minimal and task-scoped.
3. Validate with focused tests/build checks; never claim unverified results.
4. Escalate ambiguity quickly.
5. If discoverable via search, do not duplicate it here.

## Layer 1: Protocol Router

### 1) Task Classification
One dominant surface:
- `ui_ux_editor`
- `build_ci_release`
- `ai_agent_cli`
- `emu_runtime_debug`
- `rom_gameplay_data`
- `testing_harness`
- `docs_process`

### 2) Persona Selection
Primary owner from `docs/internal/agents/personas.md` (ids: `imgui-frontend-engineer`,
`backend-infra-engineer`, `ai-infra-architect`, `snes-emulator-expert`,
`zelda3-hacking-expert`, `test-infrastructure-expert`, `docs-janitor`).

### 3) Focused Context Loading
1. `.claude/agents/<agent-id>.md`
2. `docs/internal/agents/routing-personas.md`
3. Relevant entries in `docs/internal/agents/routing-skills-tools.md`

### 4) Tool Routing
- Code/build/test: shell + project scripts
- Universe coordination: `scripts/agents/coord`
- Legacy board migration: `scripts/agents/import-coordination-board.sh`,
  `scripts/agents/migrate-coordination-board.sh`

### 5) Essential Repo Facts
- Build: `cmake --preset mac-ai && cmake --build --preset mac-ai` (agent preset: `build_ai`)
- Jobs: presets default ≤4 workers; override with `--parallel` / `YAZE_BUILD_JOBS`
- Unit tests: `ctest --preset mac-ai-unit`
- Coord SoT: `~/.context/agent-universe/{events.jsonl,state.json}`
- Snapshot: `docs/internal/agents/coordination-board.generated.md` (legacy board = history only)
- Status / roadmap / changelog: `docs/internal/status.md`, `docs/internal/roadmap.md`,
  `docs/CHANGELOG.md`
- Knowledge base: `~/.context/knowledge/` (`hobby/yaze.md`, `hobby/oracle-of-secrets.md`,
  `alttp/*`, `snes/*`, `hobby/usdasm.md`, `hobby/mesen2-oos.md`, `hobby/z3dk.md`)

### 5a) Oracle of Secrets
- `oos<VERSION>.sfc` = edit target; `oos<VERSION>x.sfc` = patched/emulator only
- `z3ed --write` → base ROM (`oos168.sfc`), never `oos168x.sfc`
- Handoff: `../oracle-of-secrets/.context/scratchpad/agent_handoff.md`

### 6) Dependency Graph
`Task Class` → `Primary Persona` → focused context → tools → validation.
Coordination flows through universe events; markdown snapshot is derived only.

## Layer 2: Focused Persona/Skill Context
Load the smallest subset from `routing-personas.md`, `routing-skills-tools.md`,
and `personas.md`.

## Layer 3: Maintenance Agent
Owner: `ai-infra-architect` (+ `docs-janitor`). Keep routing current; run
`scripts/agents/protocol-audit.sh` and `scripts/agents/test-universe-coord.sh`.

## Coordination Contract
`scripts/agents/coord task-{add,claim,heartbeat,handoff,complete,list}`;
snapshot: `scripts/agents/coord task-generate-board --out docs/internal/agents/coordination-board.generated.md`

## Delivery Contract
What changed · exact validation commands · residual risks / follow-ups.
Reference: `.context/knowledge/agent-reference.md`, `README.md`, `docs/`.
