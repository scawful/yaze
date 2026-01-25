# Agent Protocol

_Extends: [unified_agent_protocol.md](file:///Users/scawful/.context/memory/unified_agent_protocol.md)_

Project-specific operating procedures for AI agents contributing to `yaze` (~/src/hobby/yaze).

## 1. Persona Adoption
**Rule:** You must adopt a specific persona for every session.
*   **Source of Truth:** [docs/internal/agents/personas.md](docs/internal/agents/personas.md)
*   **Requirement:** Use the exact `Agent ID` from that list in all logs, commits, and board updates.
*   **Legacy IDs:** Do not use `CLAUDE_CORE`, `CLAUDE_AIINF`, etc. Use the role-based IDs (e.g., `ai-infra-architect`).
*   **System Prompts:** Load the matching persona prompt from `.claude/agents/<agent-id>.md` (accessible to all agents) before starting work.

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

### specs & docs
*   Keep one canonical spec per initiative (link it from the board entry and back).
*   Add a header with Status/Owner/Created/Last Reviewed/Next Review (≤14 days) and validation/exit criteria.
*   **Automation:** See [docs/internal/agents/automation-workflows.md](docs/internal/agents/automation-workflows.md) for headless/server mode instructions.
*   Use existing templates (`initiative-template.md`, `release-checklist-template.md`) instead of creating ad-hoc files.
*   Archive idle or completed specs to `docs/internal/agents/archive/` with the date; do not open duplicate status pages.

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

## 5. Documentation Hygiene
- Follow [docs/internal/agents/doc-hygiene.md](docs/internal/agents/doc-hygiene.md) to avoid doc sprawl.
- Keep specs short, template-driven, and linked to the coordination board; prefer edits over new files.
- Archive completed/idle docs (>=14 days) under `docs/internal/agents/archive/` with dates to keep the root clean.
