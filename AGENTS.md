# Agent Protocol

This document defines the standard operating procedures for AI agents contributing to `yaze`.

## 1. Persona Adoption
**Rule:** You must adopt a specific persona for every session.
*   **Source of Truth:** [docs/internal/agents/personas.md](docs/internal/agents/personas.md)
*   **Requirement:** Use the exact `Agent ID` from that list in all logs, commits, and board updates.
*   **Legacy IDs:** Do not use `CLAUDE_CORE`, `CLAUDE_AIINF`, etc. Use the role-based IDs (e.g., `ai-infra-architect`).

## 2. Workflows & Coordination

### Quick Tasks (< 30 min)
*   **Board:** No update required.
*   **Tools:** Use `z3ed agent todo` to track your own sub-steps if helpful.
*   **Commit:** Commit directly with a clear message.

### Substantial Work (> 30 min / Multi-file)
1.  **Check Context:**
    *   Read [docs/internal/agents/coordination-board.md](docs/internal/agents/coordination-board.md) for `REQUEST` or `BLOCKER` tags.
    *   Run `git status` and `git diff` to understand the current state.
2.  **Declare Intent:**
    *   If your work overlaps with an active task on the board, post a note or Request for Comments (RFC) there first.
    *   Otherwise, log a new entry on the **Coordination Board**.
3.  **Execute:**
    *   Use `z3ed agent todo` to break down the complex task.
    *   Use `z3ed agent handoff` if you cannot finish in one session.

### Multi-Day Initiatives
*   Create a dedicated document using [docs/internal/agents/initiative-template.md](docs/internal/agents/initiative-template.md).
*   Link to this document from the Coordination Board.

## 3. The Coordination Board
**Location:** `docs/internal/agents/coordination-board.md`

*   **Hygiene:** Keep entries concise (≤ 5 lines).
*   **Status:** Update your entry status to `COMPLETE` or `ARCHIVED` when done.
*   **Maintenance:** Archive completed work weekly to `docs/internal/agents/archive/`.

## 4. Helper Scripts
Located in `scripts/agents/`:
*   `run-gh-workflow.sh`: Trigger CI manually.
*   `smoke-build.sh`: Fast verification build.
*   `test-http-api.sh`: Validate the agent API.

**Log results:** When running these scripts for significant validation, paste the run ID or result summary to the Board.