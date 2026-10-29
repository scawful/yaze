# AGENTS.md (Protocol Router)

Purpose: route agents to the minimum correct context. Not a repo overview.

Operating rules (native windows, coordination, project tracking, installs, user
issues, Windows): [`docs/internal/agents/operating-rules.md`](docs/internal/agents/operating-rules.md).
They override older protocol pages.

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
Load only:
1. The selected persona entry in `docs/internal/agents/personas.md`
2. `docs/internal/agents/routing-personas.md`
3. Relevant entries in `docs/internal/agents/routing-skills-tools.md`

### 4) Tool Routing
- Code/build/test: shell + project scripts
- Universe coordination: `scripts/agents/coord`
- Legacy board migration: `scripts/agents/import-coordination-board.sh`,
  `scripts/agents/migrate-coordination-board.sh`


### Remote native testing
Before launching a native Yaze verification app, read
[`docs/internal/testing/remote-native-testing.md`](docs/internal/testing/remote-native-testing.md).
Use a dedicated macOS Space and an exact-app floating rule. Always quit owned
test apps and verify process/window cleanup before finishing or handing off.

### 5) Essential Repo Facts
- Local agent build: `scripts/agent_build.sh yaze` (or the specific CLI/test target).
  Reuses the preset directory and configures only when needed. Builds never take
  the native heavy lock `/tmp/oos-heavy.lock`.
- Jobs: the agent helper follows `~/.config/oos/build_mode` like `quiet-build`:
  desk mode uses all host cores at normal priority; laptop or unset uses 2
  background workers (max 4). Explicit worker limits remain available.
- Run `scripts/agent_build.sh --preflight <targets>` before a build. Reuse the
  current owned source/cache; `--reuse-from <checkout>` requires the same clean
  commit. A missing cache requires explicit `--allow-new-build`, with the cold
  build reason recorded in the handoff. Never move a CMake cache between paths.
  `--jobs` or `YAZE_BUILD_JOBS` override the default; presets keep their own counts.
- Tests before push: `ctest --preset mac-ai` (same `^stable$` selection as CI; `mac-ai-unit` is a subset)
- One test: `build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='Suite.Case'` (ctest entries are shards)
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
- Dungeon geometry and layout reasoning: `docs/public/usage/dungeon-spatial-contract.md`;
  use `dungeon-describe-room --spatial` before `dungeon-template-plan`. Keep
  default-state geometry, collision replay and native gameplay proof distinct.

### 5b) Cloud / ROM-less Sessions
- Linux cloud agents (Claude Code web, Codex, Cursor): follow
  `docs/internal/agents/cloud-agents.md` instead of the macOS defaults above.
- Setup and verification: `scripts/cloud/bootstrap.sh [configure build test]`.
- No ROMs, local MCP servers, or universe coordination in cloud sessions.

### 6) Dependency Graph
For editor capability, parity, or milestone work, first read
`docs/internal/plans/editor-capability-parity-plan.md`. It defines the reference
baseline, work-package dependencies, and completion evidence. Use its package
IDs; dated handoffs and archived parity estimates are historical context.

`Task Class` -> `Primary Persona` -> `Focused Context Files` -> `Tools/Scripts` -> `Validation`.

Concretely:
- `AGENTS.md` -> persona catalog + routing docs -> scripts/tools -> tests/build checks.
- Coordination state flows through universe events/state; markdown snapshot is derived output only.

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
