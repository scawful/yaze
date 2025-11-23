# Agent Coordination Hub

Welcome to the `yaze` internal agent workspace. This directory contains the rules, roles, and records for AI agents contributing to the project.

## Quick Start
1.  **Identify Your Role**: Check [personas.md](./personas.md) to choose the correct Agent ID.
2.  **Load Your Prompt**: Open `.claude/agents/<agent-id>.md` for your persona’s system prompt (available to all agents).
3.  **Follow the Protocol**: Read [AGENTS.md](../../../AGENTS.md) for rules on communication, task logging, and handoffs.
4.  **Check Status**: Review the [coordination-board.md](./coordination-board.md) for active tasks and blockers.
5.  **Keep docs lean**: Use [doc-hygiene.md](./doc-hygiene.md) and avoid creating duplicate specs; archive idle docs.

## Documentation Index

| File | Purpose |
|------|---------|
| [AGENTS.md](../../../AGENTS.md) | **The Core Protocol.** Read this first. Defines how to work. |
| [personas.md](./personas.md) | **Who is Who.** Canonical list of Agent IDs and their scopes. |
| [.claude/agents/*](../../../.claude/agents) | **System Prompts.** Persona-specific prompts for all agents. |
| [coordination-board.md](./coordination-board.md) | **The Live Board.** Shared state, active tasks, and requests. |
| [agent-architecture.md](./agent-architecture.md) | **The Technical Manual.** How the agent tools (`z3ed agent`) work. |
| [initiative-template.md](./initiative-template.md) | Template for tracking large, multi-day features. |
| [doc-hygiene.md](./doc-hygiene.md) | Rules to keep specs/notes lean and archived on time. |

## Tools
Agents have built-in CLI capabilities to assist with this workflow:
*   `z3ed agent todo`: Manage your personal task list.
*   `z3ed agent handoff`: Generate context bundles for the next agent.
*   `z3ed agent chat`: (If enabled) Inter-agent communication channel.

## Picking the right agent
- **ai-infra-architect**: AI/agent infra, MCP/gRPC, z3ed tooling, model plumbing.
- **backend-infra-engineer**: Build/packaging/toolchains, CI reliability, release plumbing.
- **imgui-frontend-engineer**: ImGui/editor UI, renderer/backends, canvas/docking UX.
- **snes-emulator-expert**: Emulator core (CPU/APU/PPU), performance/accuracy/debugging.
- **zelda3-hacking-expert**: Gameplay/ROM logic, data formats, hacking workflows.
- **test-infrastructure-expert**: Test harnesses, CTest/gMock infra, flake/bloat triage.
- **docs-janitor**: Docs/process hygiene, onboarding, checklists.
Pick the persona that owns the dominant surface of your task; cross-surface work should add both owners to the board entry.

## Build & Test
**Strict Rule:** Never use the user's `build/` folder.
*   **Build**: `./scripts/agent_build.sh [target]` (Default target: `yaze`)
*   **Test**: `./scripts/agent_build.sh yaze_test && ./build_ai/bin/yaze_test`
*   **Directory**: All artifacts go to `build_ai/`.

## Archive
- Legacy docs (leaderboard, ai-infrastructure-initiative, coordination-improvements) now live under `archive/legacy-2025-11/`. Keep only active specs in this directory.
