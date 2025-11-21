# Agent Personas

Use these canonical identifiers when updating the [coordination board](coordination-board.md) or referencing responsibilities in other documents.

## Core Agent Roles

| Agent ID        | Primary Focus                                          | Notes |
|-----------------|--------------------------------------------------------|-------|
| `CLAUDE_CORE`   | Core editor/engine refactors, renderer work, SDL/ImGui | Handles gameplay and editor UI features. |
| `CLAUDE_AIINF`  | AI infrastructure (`z3ed`, agents, gRPC automation)    | Coordinates closely with Gemini automation agents. |
| `CLAUDE_DOCS`   | Documentation, onboarding guides, product notes        | Keeps docs synced with code changes. |
| `GEMINI_AUTOM`  | Automation/testing/CLI improvements, CI integrations   | Handles scripting-heavy and test infrastructure tasks. |
| `GEMINI_3_GENIUS` | Strategic architecture, complex reasoning, design docs | Provides high-level guidance and architectural patterns. |
| `CODEX`         | Coordination and monitoring                            | Oversees docs/build coordination and agent protocol. |

## Inactive/Retired Personas

The following personas have been retired and should not be assigned work:
- `GEMINI_FLASH_AUTOM` - Retired per user directive
- `CODEX_MINI` - Retired per user directive
- `GEMINI_SWARM` - Not currently active

## Adding New Personas

When creating new agent personas:
1. Follow the protocol in `AGENTS.md`
2. Post updates to the coordination board before starting work
3. Use clear, professional role descriptions
4. Document expected responsibilities and scope
