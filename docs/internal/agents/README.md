# Agent Coordination Hub

Welcome to the `yaze` internal agent workspace. This directory contains the rules, roles, and records for AI agents contributing to the project.

## Quick Start
1.  **Identify Your Role**: Check [personas.md](./personas.md) to choose the correct Agent ID for your current session context.
2.  **Follow the Protocol**: Read [AGENTS.md](../../../AGENTS.md) for rules on communication, task logging, and handoffs.
3.  **Check Status**: Review the [coordination-board.md](./coordination-board.md) for active tasks and blockers.

## Documentation Index

| File | Purpose |
|------|---------|
| [AGENTS.md](../../../AGENTS.md) | **The Core Protocol.** Read this first. Defines how to work. |
| [personas.md](./personas.md) | **Who is Who.** Canonical list of Agent IDs and their scopes. |
| [coordination-board.md](./coordination-board.md) | **The Live Board.** Shared state, active tasks, and requests. |
| [agent-architecture.md](./agent-architecture.md) | **The Technical Manual.** How the agent tools (`z3ed agent`) work. |
| [initiative-template.md](./initiative-template.md) | Template for tracking large, multi-day features. |

## Tools
Agents have built-in CLI capabilities to assist with this workflow:
*   `z3ed agent todo`: Manage your personal task list.
*   `z3ed agent handoff`: Generate context bundles for the next agent.
*   `z3ed agent chat`: (If enabled) Inter-agent communication channel.

## Build & Test
**Strict Rule:** Never use the user's `build/` folder.
*   **Build**: `./scripts/agent_build.sh [target]` (Default target: `yaze`)
*   **Test**: `./scripts/agent_build.sh yaze_test && ./build_ai/bin/yaze_test`
*   **Directory**: All artifacts go to `build_ai/`.
