# Collaboration Framework

**Status**: ACTIVE
**Mission**: Accelerate `yaze` development through strategic division of labor between Architecture and Automation specialists.
**See also**: [personas.md](./personas.md) for detailed role definitions.

---

## Team Structure

### Architecture Team (System Specialists)
**Focus**: Core C++, Emulator logic, UI systems, Build architecture.

**Active Personas**:
*   `backend-infra-engineer`: CMake, packaging, CI plumbing.
*   `snes-emulator-expert`: Emulator core, CPU/PPU logic, debugging.
*   `imgui-frontend-engineer`: UI rendering, ImGui widgets.
*   `zelda3-hacking-expert`: ROM data, gameplay logic.

**Responsibilities**:
*   Diagnosing complex C++ compilation/linker errors.
*   Designing system architecture and refactoring.
*   Implementing core emulator features.
*   Resolving symbol conflicts and ODR violations.

### Automation Team (Tooling Specialists)
**Focus**: Scripts, CI Optimization, CLI tools, Test Harness.

**Active Personas**:
*   `ai-infra-architect`: Agent infrastructure, CLI/TUI, Network layer.
*   `test-infrastructure-expert`: Test harness, flake triage, gMock.
*   `GEMINI_AUTOM`: General scripting, log analysis, quick prototyping.

**Responsibilities**:
*   Creating helper scripts (`scripts/`).
*   Optimizing CI/CD pipelines (speed, caching).
*   Building CLI tools (`z3ed`).
*   Automating repetitive tasks (formatting, linting).

---

## Collaboration Protocol

### 1. Work Division Guidelines

#### **For Build Failures**:
| Failure Type | Primary Owner | Support Role |
|--------------|---------------|--------------|
| Compiler errors (Logic) | Architecture | Automation (log analysis) |
| Linker errors (Symbols) | Architecture | Automation (tracking scripts) |
| CMake configuration | Architecture | Automation (preset validation) |
| CI Infrastructure | Automation | Architecture (requirements) |

#### **For Code Quality**:
| Issue Type | Primary Owner | Support Role |
|------------|---------------|--------------|
| Formatting/Linting | Automation | Architecture (complex cases) |
| Logic/Security | Architecture | Automation (scanning tools) |

### 2. Handoff Process

When passing work between roles:

1.  **Generate Context**: Use `z3ed agent handoff` to package your current state.
2.  **Log Intent**: Post to [coordination-board.md](./coordination-board.md).
3.  **Specify Deliverables**: Clearly state what was done and what is next.

**Example Handoff**:
```
### 2025-11-20 snes-emulator-expert – handoff
- TASK: PPU Sprite Rendering (Phase 1)
- HANDOFF TO: test-infrastructure-expert
- DELIVERABLES:
  - Implemented 8x8 sprite fetching in `ppu.cc`
  - Added unit tests in `ppu_test.cc`
- REQUESTS:
  - REQUEST → test-infrastructure-expert: Add regression tests for sprite priority flipping.
```

---

## Anti-Patterns to Avoid

### For Architecture Agents
- ❌ **Ignoring Automation**: Don't manually do what a script could do forever. Request tooling from the Automation team.
- ❌ **Siloing**: Don't keep architectural decisions in your head; document them.

### For Automation Agents
- ❌ **Over-Engineering**: Don't build a complex tool for a one-off task.
- ❌ **Masking Issues**: Don't script around a root cause; request a proper fix from Architecture.
