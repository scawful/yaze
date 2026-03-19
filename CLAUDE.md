# CLAUDE.md (Compact)

Purpose: concise Claude-specific routing for this folder.

Claude Rules
1. Follow local `AGENTS.md` first when both exist.
2. Keep edits minimal, reversible, and task-scoped.
3. Prefer existing scripts/tools over ad-hoc commands.
4. Validate with focused tests/build checks when possible.
5. Never claim verification that was not actually run.
6. Escalate ambiguity or conflicting requirements quickly.

Reference Knowledge

Consult the global knowledge base at `~/.context/knowledge/` for background:

| Task | Read |
|------|------|
| YAZE architecture overview | `hobby/yaze.md` |
| Oracle of Secrets integration | `hobby/oracle-of-secrets.md` + `hobby/workflows.md` |
| ALTTP game architecture | `alttp/architecture.md` |
| ROM data structures/tables | `alttp/data_tables.md` |
| Vanilla routines reference | `alttp/routine_index.md` |
| Sprite types and IDs | `alttp/sprite_catalog.md` |
| SNES hardware | `snes/cpu_memory.md`, `snes/ppu_registers.md`, `snes/dma_registers.md` |
| Disassembly bank map | `hobby/usdasm.md` |
| Mesen2 debugging integration | `hobby/mesen2-oos.md` |
| Z3DK toolchain | `hobby/z3dk.md` |

All paths relative to `~/.context/knowledge/`.

Response Contract
- What changed
- How it was validated
- Remaining risks or unknowns

Reference Material
- Full legacy guidance: `.context/knowledge/agent-reference.md`.
- Use `README.md` and `docs/` for project behavior details.
