# Agent Coordination Hub

Welcome to the `yaze` internal agent workspace. This directory contains the rules, roles, and records for AI agents contributing to the project.

## Quick Start
1.  **Identify Your Role**: Check [personas.md](./personas.md) to choose the correct Agent ID.
2.  **Load Your Prompt**: Open `.claude/agents/<agent-id>.md` for your persona’s system prompt (available to all agents).
3.  **Follow the Protocol**: Read [AGENTS.md](../../../AGENTS.md) for rules on communication, task logging, and handoffs.
4.  **Check Status**: Review the [coordination-board.md](./coordination-board.md) for active tasks and blockers.
5.  **Keep docs lean**: Use [doc-hygiene.md](./doc-hygiene.md) and avoid creating duplicate specs; archive idle docs.

## Documentation Index

### Core Coordination (Keep Visible)
| File | Purpose |
|------|---------|
| [AGENTS.md](../../../AGENTS.md) | **The Core Protocol.** Read this first. Defines how to work. |
| [personas.md](./personas.md) | **Who is Who.** Canonical list of Agent IDs and their scopes. |
| [.claude/agents/*](../../../.claude/agents) | **System Prompts.** Persona-specific prompts for all agents. |
| [coordination-board.md](./coordination-board.md) | **The Live Board.** Shared state, active tasks, and requests. |
| [collaboration-framework.md](./collaboration-framework.md) | **Team Organization.** Architecture vs Automation team structure and protocols. |

### Active Projects
| File | Purpose | Status |
|------|---------|--------|
| [initiative-v040.md](./initiative-v040.md) | v0.4.0 development plan with 5 parallel workstreams | ACTIVE |
| [handoff-sidebar-menubar-sessions.md](./handoff-sidebar-menubar-sessions.md) | UI systems architecture reference | Active Reference |
| [wasm-development-guide.md](./wasm-development-guide.md) | WASM build and development guide | UPDATED |
| [wasm-antigravity-playbook.md](./wasm-antigravity-playbook.md) | WASM with Gemini integration and debugging | UPDATED |
| [web-port-handoff.md](./web-port-handoff.md) | WASM web build status and blockers | IN_PROGRESS |

### Documentation Hygiene
| File | Purpose |
|------|---------|
| [doc-hygiene.md](./doc-hygiene.md) | Rules to keep specs/notes lean and archived on time. |

## Tools
Agents have built-in CLI capabilities to assist with this workflow:
*   `z3ed agent todo`: Manage your personal task list.
*   `z3ed agent handoff`: Generate context bundles for the next agent.
*   `z3ed agent simple-chat`: (If enabled) Inter-agent communication channel.

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
Default builds now share `build/` (native) and `build-wasm/` (WASM). If you need isolation, set `YAZE_BUILD_DIR` or create a local `CMakeUserPresets.json`.
*   **Build**: `./scripts/agent_build.sh [target]` (Default target: `yaze`)
*   **Test**: `./scripts/agent_build.sh yaze_test && ./build/bin/yaze_test`
*   **Directory**: `build/` by default (override via `YAZE_BUILD_DIR`).
*   **ROM safety**: `docs/internal/agents/rom-safety-guardrails.md` (preflight: `scripts/rom_safety_preflight.sh`)

## Archive

This directory maintains a **lean, active documentation set**. Historical or reference-only documents are organized in `archive/`:

### Archived Categories
- **large-ref-docs/** (148KB) - Large reference documents (40KB+): dungeon rendering, ZSOW, design specs
- **foundation-docs-old/** (68KB) - Older foundational docs (October) on agent architecture and CLI design
- **utility-tools/** (52KB) - Tool documentation: filesystem, dev-assist, modularity, AI dev tools
- **wasm-planning-2025/** (64KB) - Earlier WASM planning iterations
- **testing-docs-2025/** (112KB) - Test infrastructure and CI documentation archives
- **gemini-session-2025-11-23/** (40KB) - Gemini-specific session context and task planning
- **plans-2025-11/** (56KB) - Completed or archived feature/project plans
- **legacy-2025-11/** (28KB) - Completed initiatives and coordination improvements
- **session-handoffs/** (24KB) - Agent handoff and onboarding documents
- **reports/** (24KB) - Audit reports and documentation assessments

**Target: 10-15 active files in root directory. Archive completed work within 1-2 weeks of completion.**
