# YAZE Internal Documentation

**Last Updated**: November 26, 2025

Internal documentation for planning, AI agents, research, and historical build notes. These
files are intentionally excluded from the public Doxygen site so they can remain verbose and
speculative without impacting the published docs.

## Quick Links

- **Active Work**: [Coordination Board](agents/coordination-board.md)
- **v0.4.0 Initiative**: [initiative-v040.md](agents/initiative-v040.md)
- **Roadmap**: [roadmaps/roadmap.md](roadmaps/roadmap.md)
- **Doc Hygiene Rules**: [agents/doc-hygiene.md](agents/doc-hygiene.md)

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `agents/` | AI agent coordination, personas, handoffs, WASM playbooks |
| `architecture/` | System design docs (dungeon, graphics, message, music, overworld, undo/redo) |
| `blueprints/` | Architectural proposals and refactoring plans |
| `plans/` | Active development plans (archived plans in `plans/archive/`) |
| `research/` | Emulator investigations, timing analyses, web ideas |
| `roadmaps/` | Release sequencing and feature parity tracking |
| `testing/` | Test infrastructure docs and pre-push checklists |

## Current Status (v0.3.9 → v0.4.0)

### Completed ✅
- SDL3 backend infrastructure (17 abstraction files)
- WASM web port with real-time collaboration
- EditorManager refactoring (90% parity, 44% code reduction)
- AI agent tools Phases 1-4
- GUI bug fixes (BeginChild/EndChild patterns, duplicate rendering)

### In Progress 🟡
- SDL3 concrete backend implementation
- Emulator accuracy (PPU JIT catch-up)
- Dungeon object rendering fixes

## Doc Hygiene

- Keep a single canonical spec per initiative and link it from the coordination board.
- Use `agents/doc-hygiene.md` for templates, review cadence, and archiving rules.
- Archive completed/idle specs (>14 days) into `archive/` folders with dates.
- Cap new doc creation: one spec + one handoff per initiative.

## Version Control & Safety

- **Coordinate before forceful changes**: Never rewrite history on shared branches.
- **Back up ROMs and assets**: Work on copies, enable automatic backup.
- **Run `scripts/verify-build-environment.*`** after pulling significant build changes.
- **Use the coordination board** for multi-agent or large tree changes.
