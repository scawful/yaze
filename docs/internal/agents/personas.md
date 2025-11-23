# Agent Personas

Use these canonical identifiers when updating the
[coordination board](coordination-board.md) or referencing responsibilities in other documents.

| Agent ID                   | Primary Focus (shared with Oracle-of-Secrets/.claude/agents)      | Notes |
|----------------------------|-------------------------------------------------------------------|-------|
| `ai-infra-architect`       | AI/agent infra, z3ed CLI/TUI, model providers, gRPC/network       | Replaces legacy `CLAUDE_AIINF`. |
| `backend-infra-engineer`   | Build/packaging, CMake/toolchains, CI reliability                 | Use for build/binary/release plumbing. |
| `docs-janitor`             | Documentation, onboarding, release notes, process hygiene         | Replaces legacy `CLAUDE_DOCS`. |
| `imgui-frontend-engineer`  | ImGui/renderer/UI systems, widget and canvas work                 | Pair with `snes-emulator-expert` for rendering issues. |
| `snes-emulator-expert`     | Emulator core (CPU/APU/PPU), debugging, performance               | Use for yaze_emu or emulator-side regressions. |
| `test-infrastructure-expert` | Test harness/ImGui test engine, CTest/gMock infra, flake triage | Handles test bloat/flake reduction. |
| `zelda3-hacking-expert`    | Gameplay/ROM logic, Zelda3 data model, hacking workflows          | Replaces legacy `CLAUDE_CORE`. |
| `GEMINI_AUTOM`             | Automation/testing/CLI improvements, CI integrations              | Scripting-heavy or test harness tasks. |
| `CODEX`                    | Codex CLI assistant / overseer                                    | Default persona; also monitors docs/build coordination. |

Add new rows as additional personas are created. Every new persona must follow the protocol in
`AGENTS.md` and post updates to the coordination board before starting work.
