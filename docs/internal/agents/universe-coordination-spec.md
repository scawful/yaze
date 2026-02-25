# Universe Coordination Spec

Status: ACTIVE  
Owner: `ai-infra-architect`  
Created: 2026-02-24  
Last Reviewed: 2026-02-24  
Next Review: 2026-03-03

## Goal
Replace manual coordination-board updates with a local, cross-project event log that agents can read/write directly.

## Scope
- Local-first (no server required).
- Cross-project namespace via `project_key`.
- Append-only event history (`events.jsonl`) + materialized state (`state.json`).
- Generated markdown snapshot for humans when needed.

## Storage
- Root: `~/.context/agent-universe/`
- Files:
  - `events.jsonl` (source of truth)
  - `state.json` (derived view)

## Commands
CLI: `scripts/agents/universe-coord.sh`  
Project wrapper: `scripts/agents/coord`
Migration helper: `scripts/agents/migrate-coordination-board.sh`  
Validation harness: `scripts/agents/test-universe-coord.sh`

Supported commands:
- `init`
- `rebuild-state`
- `task-add`
- `task-list`
- `task-claim`
- `task-heartbeat`
- `task-handoff`
- `task-complete`
- `task-generate-board`

## Event Model (JSONL)
Each line is one event object:
- `event_id`
- `type` (`task_added`, `task_claimed`, `task_heartbeat`, `task_handoff`, `task_completed`)
- `ts` (UTC ISO-8601)
- `task_id`
- `project_key`
- `agent`
- optional: `to`, `title`, `priority`, `tags`, `note`, `source`

## Materialized State
`state.json` fields:
- `version`
- `updated_at`
- `tasks` map keyed by `task_id`

Per-task fields:
- `task_id`, `title`, `project_key`, `priority`, `tags`
- `status` (`open`, `active`, `complete`)
- `assignee`
- `created_at`, `updated_at`, `completed_at`
- `last_note`
- `history` (event summaries)

## Concurrency
- Write lock via lock-directory (`$UNIVERSE_DIR/.lock`).
- Event append + state rebuild happen in one locked section.
- State rebuild writes to `state.json.tmp` then atomically renames to `state.json`.
- Read operations are lock-free.

## Migration Plan
1. Keep `docs/internal/agents/coordination-board.md` as legacy history.
2. Import legacy entries with:
   - `scripts/agents/import-coordination-board.py --dry-run`
   - `scripts/agents/import-coordination-board.py --apply`
3. Generate snapshots instead of manual board edits:
   - `scripts/agents/coord task-generate-board --out docs/internal/agents/coordination-board.generated.md`

## Validation / Exit Criteria
- Agents can claim/handoff/complete without editing markdown.
- `task-list` reflects active ownership and recent progress.
- Generated board snapshot renders active/open/completed tasks.
- Legacy import tool can bootstrap state from the current board.

## Validation Commands
- `scripts/agents/test-universe-coord.sh`
- `scripts/agents/protocol-audit.sh`
- `scripts/agents/migrate-coordination-board.sh --dry-run`
- `scripts/agents/migrate-coordination-board.sh --apply`

## Failure Modes and Recovery
- Lock timeout (`error: timeout acquiring lock`):
  - Wait and retry. If stale lock suspected, inspect/remove `$UNIVERSE_DIR/.lock`.
- Invalid/corrupt event line:
  - Append is rejected; no write occurs.
  - Repair by fixing/removing bad lines in `events.jsonl`, then run `rebuild-state`.
- Partial state write risk:
  - Mitigated by atomic temp-file rename.
- Duplicate import runs:
  - Importer de-duplicates by `task_id`; repeated apply appends only new entries.
