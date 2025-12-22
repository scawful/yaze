# Agent Coordination & Documentation Improvement Plan

## Findings
1. **Persona Inconsistency**: `coordination-board.md` uses a mix of legacy IDs (`CLAUDE_AIINF`) and new canonical IDs (`ai-infra-architect`).
2. **Tool Underutilization**: The protocol in `AGENTS.md` relies entirely on manual Markdown edits, ignoring the built-in `z3ed agent` CLI tools (todo, handoff) described in `agent-architecture.md`.
3. **Fragmented Docs**: There is no central entry point (`README.md`) for agents entering the directory.
4. **Undefined Systems**: `claude-gemini-collaboration.md` references a "challenge system" and "leaderboard" that do not exist.

## Proposed Actions

### 1. Update `AGENTS.md` (The Protocol)
*   **Mandate CLI Tools**: Update the "Quick tasks" and "Substantial work" sections to recommend using `z3ed agent todo` for personal task tracking.
*   **Clarify Handoffs**: Explicitly mention using `z3ed agent handoff` for transferring context, with the Markdown board used for *public signaling*.
*   **Strict Persona Usage**: Remove "Legacy aliases" mapping and simply link to `personas.md` as the source of truth.

### 2. Cleanup `coordination-board.md` (The Board)
*   **Header Update**: Add a bold warning to use only IDs from `personas.md`.
*   **Retroactive Fix**: Update recent active entries to use the correct new IDs (e.g., convert `CLAUDE_AIINF` -> `ai-infra-architect` where appropriate).

### 3. Create `docs/internal/agents/README.md` (The Hub)
*   Create a simple index file that links to:
    *   **Protocol**: `AGENTS.md`
    *   **Roles**: `personas.md`
    *   **Status**: `coordination-board.md`
    *   **Tools**: `agent-architecture.md`
*   Provide a 3-step "Start Here" guide for new agents.

### 4. Deprecate `claude-gemini-collaboration.md`
*   Rename to `docs/internal/agents/archive/collaboration-concept-legacy.md` or remove the "Challenge System" sections if the file is still valuable for its architectural definitions.
*   *Recommendation*: If the "Architecture vs. Automation" split is still relevant, update the file to use `backend-infra-engineer` (Architecture) vs `GEMINI_AUTOM` (Automation) instead of "Claude vs Gemini".

## Execution Order
1.  Create `docs/internal/agents/README.md`.
2.  Update `AGENTS.md`.
3.  Clean up `coordination-board.md`.
4.  Refactor `claude-gemini-collaboration.md`.
