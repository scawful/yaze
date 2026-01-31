# YAZE Internal Documentation

**Last Updated**: January 7, 2026

Internal documentation for planning, AI agents, research, and historical build notes. These files are intentionally excluded from the public Doxygen site so they can remain verbose and speculative.

## Quick Links

- **Active Work**: [Coordination Board](agents/coordination-board.md)
- **Roadmap**: [roadmap.md](roadmap.md)
- **Doc Hygiene Rules**: [agents/doc-hygiene.md](agents/doc-hygiene.md)
- **Documentation Audit**: [DOCS_AUDIT.md](DOCS_AUDIT.md)
- **Templates**: [templates/](templates/)

## Technical References

| Module | Documentation |
|--------|---------------|
| Zelda3 Core | [zelda3/README.md](zelda3/README.md) - ROM data structures, constants, loaders |
| GUI Layer | [gui/README.md](gui/README.md) - Canvas system, widgets, theming |

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `agents/` | AI agent coordination, personas, and board |
| `architecture/` | System design and architectural documentation |
| `archive/` | Retired plans, completed features, closed investigations, and maintenance logs |
| `debug/` | Debugging guides, active logs, and accuracy reports |
| `gui/` | GUI abstraction layer reference (canvas, widgets) |
| `hand-off/` | Active handoff documents for in-progress work (e.g. [Editor comparison & plan](hand-off/HANDOFF_EDITOR_COMPARISON_AND_PLAN.md)) |
| `plans/` | Active implementation plans and roadmaps |
| `research/` | Exploratory notes, ideas, and technical analysis |
| `templates/` | Document templates (checklists, initiatives) |
| `testing/` | Test infrastructure configuration and strategy |
| `wasm/` | WASM/Web port documentation and guides |
| `zelda3/` | Game-specific documentation (ALTTP internals) |
| `roadmap.md` | Master project roadmap |

## Doc Hygiene

- **Single Source of Truth**: Maintain one canonical spec per initiative.
- **Templates**: Use `templates/` for new initiatives and release checklists.
- **Archiving**: Move completed specs to `archive/completed_features/` or `archive/investigations/`.
- **Coordination**: Use the [Coordination Board](agents/coordination-board.md) for active tasks.

## Version Control & Safety

- **Coordinate before forceful changes**: Never rewrite history on shared branches.
- **Back up ROMs and assets**: Work on copies, enable automatic backup.
- **Run `scripts/verify-build-environment.*`** after pulling significant build changes.
