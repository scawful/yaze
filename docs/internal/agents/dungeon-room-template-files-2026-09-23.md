# DA-4: room template files

Source baseline: `e1e2cb3b1`, branch `codex/combined-editor-candidate`.
Status: implementation and focused automated checks passed; manual UI deferred.

## Workflow and ownership

The existing Room transfer inspector/popup exposes **Save Room File...**.
**Import JSON → Open Room File...** loads a template into the form, requiring
Preview Replacement and Apply Replacement before any room mutation. Loading
preserves domain choices; successful loads invalidate a previous preview.
Cancellation leaves the form alone; failures preserve prior JSON and preview.
Read-only viewers can export but cannot import/apply.

`dungeon_room_document_file.*` owns bounded file reads, schema validation and
atomic publication through the existing project-file writer. Export accepts
`.json` only, replaces existing valid room documents, and refuses unrelated
files/directories/symlinks. Browser builds retain clipboard/text exchange.
Neither operation writes ROM bytes. No new project format or asset remapper.

## Evidence

```bash
cmake --preset mac-ai
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_list_tests --gtest_filter='*DungeonRoomDocument*:*DungeonRoomTransfer*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='*DungeonRoomDocument*:*DungeonRoomTransfer*'
```

128 tests across five suites passed, zero failures/skips. Ten new cases cover
file roundtrip/replacement, invalid export/import, bounded reading, protected
destinations, cancellation, preview state and read-only UI. File tests use
throwaway temporary JSON files, while room lifecycle tests use synthetic ROMs.
Native dialogs were not clicked and actual ROM-file/game-runtime qualification
was not performed. Earlier combined baseline evidence remains source-specific.

## Next work

Keep all user acceptance in the [manual checklist](../plans/manual-editor-checklist-2026-09-23.md).
DA-4 still needs asset compatibility/remapping and project-managed template
catalogs. For the next independent dungeon slice, inspect DA-3 pit/stair
connections against engine rules, preserving existing destination metadata and
shared history. Do not assume staircase object order identifies a header slot.
Sprite editor/catalog and Oracle cutscene/sprite work belong to other agents.
Do not modify those active worktrees or overwrite their shared build changes.
