# Persona Routing Map

Status: ACTIVE  
Owner: `ai-infra-architect`  
Last Reviewed: 2026-02-24

Use this file to select one primary persona for a task. If a task spans multiple surfaces, keep one primary owner and coordinate handoff for secondary owners.

## Routing Matrix

| Task Class | Primary Persona | Secondary Persona (if needed) | Load These First |
|---|---|---|---|
| `ui_ux_editor` | `imgui-frontend-engineer` | `test-infrastructure-expert` | `.claude/agents/imgui-frontend-engineer.md`, `docs/internal/agents/rom-safety-guardrails.md` |
| `build_ci_release` | `backend-infra-engineer` | `ai-infra-architect` | `.claude/agents/backend-infra-engineer.md`, `docs/internal/agents/dev-release-workflow.md` |
| `ai_agent_cli` | `ai-infra-architect` | `backend-infra-engineer` | `.claude/agents/ai-infra-architect.md`, `scripts/agents/README.md` |
| `emu_runtime_debug` | `snes-emulator-expert` | `zelda3-hacking-expert` | `.claude/agents/snes-emulator-expert.md`, `docs/internal/agents/rom-safety-guardrails.md` |
| `rom_gameplay_data` | `zelda3-hacking-expert` | `imgui-frontend-engineer` | `.claude/agents/zelda3-hacking-expert.md`, `docs/internal/agents/rom-safety-guardrails.md` |
| `testing_harness` | `test-infrastructure-expert` | `backend-infra-engineer` | `.claude/agents/test-infrastructure-expert.md`, `test/CMakeLists.txt` |
| `docs_process` | `docs-janitor` | `ai-infra-architect` | `.claude/agents/docs-janitor.md`, `docs/internal/agents/doc-hygiene.md` |

## Selection Rules
- Pick one primary owner only.
- Only add a secondary owner if task contains a hard dependency outside primary scope.
- If ownership changes mid-task, record a handoff in universe coordination.

## Coordination Hooks
- Start: `scripts/agents/coord task-add ...`
- Claim: `scripts/agents/coord task-claim ...`
- Heartbeat for long tasks: `scripts/agents/coord task-heartbeat ...`
- Handoff: `scripts/agents/coord task-handoff ...`
- Complete: `scripts/agents/coord task-complete ...`
