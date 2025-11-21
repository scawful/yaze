# z3ed Hive Mode Blueprint

**Status:** DRAFT  
**Owner:** CODEX (pending collaborator)  
**Purpose:** Describe how `z3ed` could orchestrate its own mini-hive (agents, tools, workflows) to assist Zelda hacking tasks end-to-end.

## Goals
1. Enable scripted “guided hack” sessions where `z3ed` plans, applies, and validates edits.
2. Share the same coordination concepts as the main hive (board, personas, mini-games).
3. Plug into emulator real-time features and ALTTP knowledge bases.

## Proposed Personas
| Agent ID | Focus | Notes |
|----------|-------|-------|
| `Z3ED_COORD` | Session coordinator / CLI state machine | Owns the conversation and tracks TODOs. |
| `Z3ED_PATCHER` | Applies ROM patches / ASM snippets | Calls assembler, patch queue, live emulator API. |
| `Z3ED_TESTER` | Runs tests (ctest, scripted emulator checks) | Verifies changes before/after patching. |
| `Z3ED_DOCS` | Emits session transcripts + tutorials | Converts successful sessions into how-to docs. |

## Coordination Flow
1. User starts `z3ed hive --session <name>`.
2. CLI spawns a lightweight board (in-memory or log file).
3. Each persona posts plan/update entries, mirroring AGENTS.md protocol.
4. Keep-chatting rules still apply (CLI prints morale prompts + actionable tasks).

## Required Integrations
- **ALTTP Knowledge Base:** Sprite/object/ASM reference (see Sprite Systems Reference proposal).
- **State Sync Protocol:** Use SSP hooks to patch emulator safely.
- **Test Harness:** Wrap `scripts/agents/run-tests.sh`, emulator smoke tests, and custom Lua/Lisp scripts.
- **Doc Export:** Convert session log → markdown quick start (auto-summarize steps).

## Open Questions
- Storage for per-session boards? (Local file vs sqlite vs git branch.)
- How to authenticate CLI personas with the main hive board (if at all)?
- Which tasks stay CLI-only vs escalated to “human” agents?

## Next Steps
1. Flesh out per-persona responsibilities + scripts.
2. Design CLI UX (`z3ed hive start`, `hive status`, `hive export-doc`).
3. Prototype with one scenario (e.g., “edit dungeon chest contents”).

## Sample Session Flow
1. `z3ed hive start “dungeon-overhaul”`
2. `Z3ED_COORD` posts plan: “Audit dungeon 2 chests”
3. `Z3ED_PATCHER` exports current chest data, proposes edits, applies via `LivePatch`
4. `Z3ED_TESTER` runs `run-tests.sh mac-ai --preset stable-ai` + emulator smoke script
5. `Z3ED_DOCS` exports transcript → `docs/public/examples/dungeon-overhaul.md`

## Competition Hooks
- Weekly “Hive Hack” challenge (fastest session to add new feature)  
- Meme points for most creative AI workflow that still passes SSP/CI checks  
- Easy to plug into the main leaderboard since roles map cleanly.

## Integration Roadmap
1. **MVP**: Fake board (JSON log), personas implemented as CLI commands (`z3ed hive plan/update/complete`).  
2. **SSP Hookup**: Use State Sync Protocol to apply patches safely during sessions.  
3. **Knowledge Base**: Link to Sprite Systems Reference + ALTTP ASM docs for smarter suggestions.  
4. **Doc Export**: Auto-generate Quick Start guides from successful sessions.  
5. **External Hive Bridge**: Optional sync to the main coordination board when tasks cross teams.
