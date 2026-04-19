# YAZE Internal Documentation

**Last Updated**: 2026-04-19

Internal documentation for architecture, AI agent coordination, and development planning.

## Quick Links

| What | Where |
|------|-------|
| Active task tracking | `scripts/agents/coord task-list` ([universe coordination](agents/universe-coordination-spec.md)) |
| Refactoring plan | [agents/refactoring-plan-0.7.md](agents/refactoring-plan-0.7.md) |
| Latest handoff | [agents/refactoring-handoff-2026-02-24.md](agents/refactoring-handoff-2026-02-24.md) |
| Release test checklist | [agents/oracle-morning-test-checklist.org](agents/oracle-morning-test-checklist.org) |
| Goron Mines regression tracker | [oracle/goron-mines-minecart-regression-tracker-2026-02-26.md](oracle/goron-mines-minecart-regression-tracker-2026-02-26.md) |
| Doc hygiene rules | [agents/doc-hygiene.md](agents/doc-hygiene.md) |
| Agent scripts | [scripts/agents/README.md](../../scripts/agents/README.md) |

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `agents/` | Agent coordination, personas, routing rules, active plans |
| `agents/archive/` | Retired initiatives, handoffs, and drafts (do not edit) |
| `architecture/` | System design docs (editor, dungeon, overworld, ROM, graphics, etc.) |
| `archive/` | Completed features, closed investigations, old plans |
| `oracle/` | Oracle-of-Secrets dungeon/collision runtime docs and regression tracking |
| `zelda3/` | ALTTP-specific data format documentation |
| `gui/` | Canvas system and widget layer reference |
| `wasm/` | Web/WASM port documentation |
| `testing/` | Test infrastructure and strategy |

## Key Architecture Docs

| System | Doc |
|--------|-----|
| EditorManager | [architecture/editor_manager.md](architecture/editor_manager.md) |
| Dungeon Editor | [architecture/dungeon_editor_system.md](architecture/dungeon_editor_system.md) |
| Dungeon Interaction | [architecture/dungeon_interaction_architecture.md](architecture/dungeon_interaction_architecture.md) |
| Overworld Editor | [architecture/overworld_editor_system.md](architecture/overworld_editor_system.md) |
| Graphics Pipeline | [architecture/graphics_system_architecture.md](architecture/graphics_system_architecture.md) |
| ROM Layer | [architecture/rom_architecture.md](architecture/rom_architecture.md) |
| Platform Backends | [architecture/platform_backends.md](architecture/platform_backends.md) |
| Undo/Redo | [architecture/undo_redo_system.md](architecture/undo_redo_system.md) |
| Editor folder map (registry / shell / system) | [architecture/editor-ui-module-pattern.md](architecture/editor-ui-module-pattern.md) |

## Coordination

Task coordination uses the universe event log (not manual markdown edits):

```bash
scripts/agents/coord task-add --title "Description" --agent "persona-id"
scripts/agents/coord task-list --status active
scripts/agents/coord task-complete --id <task_id> --agent <persona-id>
```

The legacy `coordination-board.md` is history-only. Generated snapshots go to `coordination-board.generated.md`.
