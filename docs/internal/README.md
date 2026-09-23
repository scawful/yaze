# YAZE Internal Documentation

**Last Updated**: 2026-09-22

Internal documentation for architecture, AI agent coordination, and development planning.

## Quick Links

| What | Where |
|------|-------|
| Editor completion baseline and instructions | [Editor capability parity plan](plans/editor-capability-parity-plan.md) — begin with `DA-1` / `DA-2` for dungeon authoring |
| Product priority and release sequencing | [Roadmap](roadmap.md) and [0.x release ladder](plans/release-ladder-0x-2026.md) |
| Current editor readiness | [Feature coverage report](../public/reference/feature-coverage-report.md) and [capability assessment](../public/reference/capability-assessment.md) |
| Active task tracking | `scripts/agents/coord task-list` ([universe coordination](agents/universe-coordination-spec.md)) |
| Refactoring work | Tracked as universe tasks. The 0.7 plan is closed: [archive/plans/refactoring-plan-0.7-2026-02.md](archive/plans/refactoring-plan-0.7-2026-02.md) |
| Handoffs | Dated `*-handoff-*.md` files in [agents/](agents/); match the task, branch, and exact source SHA |
| Testing docs | [testing/README.md](testing/README.md) |
| Script classification | [scripts/README.md](../../scripts/README.md) |
| Release acceptance | [Release checklist](release-checklist.md) and [dungeon evidence backlog](plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md) |
| Oracle runtime checks | [agents/oracle-morning-test-checklist.org](agents/oracle-morning-test-checklist.org) |
| Goron Mines regression tracker | [oracle/goron-mines-minecart-regression-tracker-2026-02-26.md](oracle/goron-mines-minecart-regression-tracker-2026-02-26.md) |
| Doc + code hygiene rules | [agents/doc-hygiene.md](agents/doc-hygiene.md) |
| Agent scripts | [scripts/agents/README.md](../../scripts/agents/README.md) |

## Directory Structure

| Directory | Purpose |
|-----------|---------|
| `agents/` | Agent coordination, personas, routing rules, active plans |
| `agents/archive/` | Retired initiatives, handoffs, and drafts (do not edit) |
| `plans/` | Canonical capability plan, release ladder, and bounded initiative plans |
| `architecture/` | System design docs (editor, dungeon, overworld, ROM, graphics, etc.) |
| `archive/` | Completed features, closed investigations, old plans |
| `oracle/` | Oracle-of-Secrets dungeon/collision runtime docs and regression tracking |
| `zelda3/` | ALTTP-specific data format documentation |
| `gui/` | Canvas system and widget layer reference |
| `wasm/` | Web/WASM port documentation |
| `testing/` | Test infrastructure and strategy |

## Status and evidence

Use the canonical capability plan for current package status and exact source
baselines. Preserve dated handoffs as evidence for their own commits. A newer
file date does not supersede the canonical plan, establish a merge, or prove
which app is installed. Update readiness only with evidence for the named
workflow and candidate.

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
