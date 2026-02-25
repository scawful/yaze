# Agent Coordination Hub

This directory is the index for yaze agent protocol and coordination.

## Layered Context Model
- Layer 1 router: `AGENTS.md`
- Layer 2 focused routing:
  - `docs/internal/agents/routing-personas.md`
  - `docs/internal/agents/routing-skills-tools.md`
  - `docs/internal/agents/personas.md`
- Layer 3 maintenance:
  - `scripts/agents/protocol-audit.sh`
  - `scripts/agents/test-universe-coord.sh`

## Quick Start
1. Choose primary owner persona using `docs/internal/agents/routing-personas.md`.
2. Load only the selected prompt from `.claude/agents/<agent-id>.md`.
3. Claim work in universe coordination via `scripts/agents/coord`.
4. Execute task with the smallest relevant context and tool stack.
5. Validate changes and report exact commands used.

## Coordination Source of Truth
- Event log: `~/.context/agent-universe/events.jsonl`
- Materialized state: `~/.context/agent-universe/state.json`
- Human snapshot (generated): `docs/internal/agents/coordination-board.generated.md`
- Legacy history file (no new manual writes): `docs/internal/agents/coordination-board.md`

## Migration and Validation Commands
- `scripts/agents/migrate-coordination-board.sh --dry-run`
- `scripts/agents/migrate-coordination-board.sh --apply`
- `scripts/agents/test-universe-coord.sh`
- `scripts/agents/protocol-audit.sh`

## Documentation Hygiene
- Keep process docs short and linked from this index.
- Archive stale plans/reports under `docs/internal/agents/archive/`.
- Do not create duplicate protocol pages when an existing canonical page can be updated.
