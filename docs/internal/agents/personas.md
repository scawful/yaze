# Agent Personas

Use these canonical identifiers when updating the
[coordination board](coordination-board.md) or referencing responsibilities in other documents.

| Agent ID        | Primary Focus                                          | Notes |
|-----------------|--------------------------------------------------------|-------|
| `CLAUDE_CORE`   | Core editor/engine refactors, renderer work, SDL/ImGui | Use when Claude tackles gameplay/editor features. |
| `CLAUDE_AIINF`  | AI infrastructure (`z3ed`, agents, gRPC automation)    | Coordinates closely with Gemini automation agents. |
| `CLAUDE_DOCS`   | Documentation, onboarding guides, product notes        | Keep docs synced with code changes and proposals. |
| `GEMINI_AUTOM`  | Automation/testing/CLI improvements, CI integrations   | Handles scripting-heavy or test harness tasks. |
| `CODEX`         | Codex CLI assistant / overseer                         | Default persona; also monitors docs/build coordination when noted. |

Add new rows as additional personas are created. Every new persona must follow the protocol in
`AGENTS.md` and post updates to the coordination board before starting work.
