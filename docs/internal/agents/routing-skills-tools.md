# Skill and Tool Routing Map

Status: ACTIVE  
Owner: `ai-infra-architect`  
Last Reviewed: 2026-02-24

This file maps task classes to focused skills and tool stacks.  
Rule: load only the smallest skill/tool set needed for the current task.

## Skill Routing

Use session-available skill lists as source of truth. Common mappings:

| Task Class | Preferred Skills |
|---|---|
| `ai_agent_cli` | `agentic-context`, `yaze-z3ed-workflow` |
| `rom_gameplay_data` | `z3ed-dungeon-edit-commands`, `z3ed-dungeon-edit-audit`, `alttp-disasm-labels` |
| `emu_runtime_debug` | `oracle-debugging`, `mesen2-oos-debugging` |
| `build_ci_release` | `yaze-z3ed-workflow` (for validation scripts), no extra skill by default |
| `docs_process` | `agentic-context` (if `.context` coordination is required) |

Notes:
- If a skill is unavailable in the active session, continue with direct CLI workflows.
- Do not preload unrelated skills “just in case”.

## Tool Routing

Prefer this order:
1. Repo scripts in `scripts/agents/` and `scripts/dev/`.
2. Shell + ripgrep (`rg`) for local discovery.
3. MCP/tooling endpoints only for their domain-specific value.

Common tool stack by task:

| Task Class | Tool Stack |
|---|---|
| `ui_ux_editor` | build/test commands + focused unit tests |
| `build_ci_release` | CMake presets, CI workflow scripts, release checks |
| `ai_agent_cli` | z3ed CLI handlers + command registry + unit tests |
| `emu_runtime_debug` | emulator/debug MCP tools + ROM guardrails |
| `rom_gameplay_data` | z3ed edit/readback commands + ROM preflight/smoke |
| `testing_harness` | `ctest` groups, `yaze_test_unit`, `yaze_test_integration` |
| `docs_process` | doc lint/check scripts + protocol audit |

## Coordination and Migration Tools

- Primary coordination:
  - `scripts/agents/coord`
  - `scripts/agents/universe-coord.sh`
- Migration helpers:
  - `scripts/agents/import-coordination-board.sh`
  - `scripts/agents/migrate-coordination-board.sh`
- Protocol checks:
  - `scripts/agents/protocol-audit.sh`
  - `scripts/agents/test-universe-coord.sh`
