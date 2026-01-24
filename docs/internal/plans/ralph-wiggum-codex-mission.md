# Yaze Ralph Wiggum Codex Mission

Status: IN_PROGRESS
Owner: ai-infra-architect
Created: 2026-01-24
Last Reviewed: 2026-01-24
Next Review: 2026-02-07
Coordination Board: docs/internal/agents/coordination-board.md (2026-01-24 ai-infra-architect - Codex Ralph loop)

## Summary

This mission drives automated iteration for Yaze using the Codex Ralph loop. The loop should focus on
AI integration quality, test coverage expansion, API exposure, and build stability across macOS,
Windows, Linux, iOS, and WASM.

## Constraints

- Respect `AGENTS.md` and the coordination board before making changes.
- Use `z3ed --sandbox` for any ROM modifications and keep backups.
- Prefer `scripts/agents/*` helpers for builds/tests.
- Do not assume HTTPS for local AI hosts; keep per-host configuration.
- Avoid destructive commands. If unsure, stop and document the blocker.
- This agent may act as a model trainer: set up Vast.ai runs, generate/train data
  from model gaps, and train via MLX on macOS when cost-effective.
- Maintain strict data quality standards. If results look stale, use gemini-cli
  with google_web_search for up-to-date grounding before generating data.

## System Guardrails and Workspace Map

- Source of truth docs:
  - `~/src/docs/NERV_INFRASTRUCTURE.md` (workspace infra overview)
  - `~/src/docs/SRC_UNIVERSE_NETWORK.md` (source universe sync)
  - `~/src/lab/afs/docs/WORKSPACE_INTEGRATION.md` (AFS in this workspace)
- Codenames (no IPs): ORACLE = macOS primary, MECHANICA = Windows GPU node, NEXUS = Linux server.
- Use SSH host aliases and `~/Mounts/...` for remote filesystems; keep `.context/` local.
- Prefer `ws` for discovery (`ws list`, `ws status`, `ws find`) and avoid ad-hoc path guessing.

## Agentic File System (AFS) Rules

- Discover contexts first: `~/src/lab/afs/scripts/afs context discover --path ~/src --json`.
- Ensure scaffolding when needed: `~/src/lab/afs/scripts/afs context ensure-all --path ~/src`.
- Read/write via AFS where possible; write only to `scratchpad`, not `memory` or `knowledge`.
- Use `afs fs read/write/list` for structured updates; keep `.context` clean.
- Use AFS plugin surfaces and wrappers (AFS_CLI/AFS_VENV) when invoking non-interactive agents.
- Note: AFS CLI writes to `~/.context/projects/yaze` are blocked by the sandbox; use repo `.context` paths for scratchpad updates.
- Loop-local context note: `.context/notes/ralph-loop-context.md` (repo-local, safe to write).

## ~/src Universe (quick map)

- `~/src/hobby/` - primary projects (Yaze, Oracle-of-Secrets, ZScreamDungeon, zAsar).
- `~/src/tools/` - tooling (hyrule-historian, book-of-mudora, ws, train-status).
- `~/src/lab/` - infra/experiments (afs, vast, cortex, barista).
- `~/src/training/` - model training workspace (datasets, runs, checkpoints).
- `~/src/third_party/` - external dependencies; avoid edits unless explicitly requested.

## Deliverables

- Improved AI provider discovery and settings UX (UI + CLI parity).
- Expanded test coverage for editor panels and automation API.
- Hardened HTTP + gRPC API surfaces with clear docs and smoke tests.
- Stable builds for macOS, Windows, Linux, iOS, and WASM (nightly readiness).
- Improved codebase readability and navigation (smaller components, atomic functions, pure structs, clearer APIs).
  - Public headers live in `inc/` (short for public API) and remain focused; internal headers stay co-located in `src/`.

## Exit Criteria

- `scripts/agents/smoke-build.sh` succeeds on mac-ai and win-ai (or documented blockers).
- `scripts/agents/run-tests.sh` passes core suites or failing tests are triaged and logged.
- API docs updated for any new/changed endpoints.
- Loop outputs completion promise when all deliverables are met.

## Loop Prompt (use with ralph-loop-codex.sh)

You are running a Ralph Wiggum loop for Yaze. Your mission:

Decision policy:

- If the repo is dirty, proceed without asking; log a short summary of the dirty state to the coordination board.
- Do not block on questions. Pick the next most valuable task and continue.
- Keep changes scoped to mission deliverables; if a conflict arises, document it and move on.
- Do not end early. Only emit the completion promise when exit criteria are met; if unsure, keep iterating.

Coordination guardrails:

- Roles (avoid overlap): Codex = build/test + CLI/API docs + infra scripts. Claude Main = editor UX/feature fixes.
  Claude Overseer = triage, consolidation, decision log, and conflict resolution.
- Use locks when touching shared areas:
  - Acquire: `scripts/agents/ralph-loop-lock.sh acquire --area <name> --owner <agent>`
  - Release: `scripts/agents/ralph-loop-lock.sh release --area <name>`
  - Status: `scripts/agents/ralph-loop-lock.sh status`
- Shared scratchpad: append a 3-6 line summary to `.context/scratchpad/ralph-loop.md` each iteration.
- If 3 consecutive failures or repeated conflicts occur, switch to a different deliverable and log the blocker.

1) AI integration

- Ensure provider/model discovery works for OpenAI, Anthropic, Gemini, Ollama, LM Studio.
- Keep UI + CLI settings in sync (shared defaults, normalized base URLs, clear errors).
- Add safe caching and avoid unnecessary network calls.

1) Test coverage + automation

- Add/refresh editor smoke tests and API checks.
- Use `scripts/agents/run-tests.sh` and log results to the coordination board.
- Run a smoke build/test regularly (e.g., every 3 iterations or after build/test-affecting changes).
- If tests fail, triage and document the blockers.

1) API exposure + stability

- Verify HTTP + gRPC endpoints are documented and discoverable.
- Add or update docs for any new/changed commands.
- Keep builds stable across macOS/Windows/Linux/iOS/WASM; document any CI gaps.

1) Safety + process

- Follow `AGENTS.md` and update the coordination board.
- Use `z3ed --sandbox` for ROM edits and keep backups.
- Avoid destructive commands; if uncertain, stop and document.

1) Codebase readability + structure

- Prefer smaller, atomic functions and pure structs where possible.
- Reduce oversized components and split responsibilities cleanly.
- Keep public APIs minimal and well-documented (expose only what’s needed).
- Ensure data structs remain serialization-friendly (export/manipulation), and be cautious about over-reliance on protobuf reflection.
- Improve file/folder navigation with consistent naming + lightweight index docs.
- Public headers live in `inc/` (short for public API); internal headers stay co-located in `src/`.
- Maintain the existing folder structure unless a change is clearly warranted; propose any new structure conventions before refactors.

Completion promise (ONLY when all exit criteria are true):
<promise>YAZE_RALPH_DONE</promise>
