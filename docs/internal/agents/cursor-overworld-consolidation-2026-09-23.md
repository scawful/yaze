# Cursor overworld consolidation — 2026-09-23

## State

Reviewed Cursor's uncommitted main-checkout work against `55ada11d7` and the
repair branch at `628f6694b`. The result combines the Tile16 domain/workbench
extraction, immediate editing and shared Undo/Redo, organized overworld sources,
painting/context-target repairs, and reviewed floating-window UI.

## Resolved review findings

1. Floating requests used base panel IDs and could be consumed by another ROM
   session. Requests now use exact session-prefixed window IDs.
2. Failed opens and canceled/retired windows could leave stale requests. Queue
   only after successful open; cancel on close, hide, unregister, preset hiding,
   visibility restore, and session retirement.
3. Explicit Edit could undock into an unusably narrow former dock size. Restore
   preferred size for that opening only; subsequent docking remains available.
4. The empty-state widget had no build registration or production caller. It is
   registered and used by the Tile16 view without a loaded ROM. Titles wrap and
   buttons stay within narrow panels. Tests now activate the action callback.

## Source ownership

- `overworld/tile16/`: editing domain and document history.
- `overworld/ui/tiles/`: Tile16 workbench layout and panel shell.
- `overworld/canvas/`: painting, context targets, and canvas orchestration.
- `system/workspace/`: session-specific panel lifecycle and floating requests.
- `gui/widgets/empty_state.*`: shared themed empty-state presentation.

Do not restore staged Commit/Discard interactions or the old cache callback.
All Tile16 document mutations must use the shared history transaction.

## Excluded unfinished inputs

`shortcut_group_test.cc` calls absent `InferShortcutGroup` APIs.
`status_bar_context_test.cc` calls absent StatusBar context APIs. These drafts
are preserved outside active test sources for a future bounded implementation.
The empty `tile16_workbench.h` placeholder is also preserved outside active
sources; the current façade owns the declarations.
`OWNERS` and the two root dungeon validation report deletions are unrelated and
must remain unstaged. Original Cursor input is preserved before consolidation.

## Verification

Executed from the repair worktree with `build/presets/mac-ai`:

```sh
cmake --build --preset mac-ai --target yaze yaze_test_unit yaze_test_integration --parallel 4
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='WorkspaceFloatingWindowTest.*:WorkspaceWindowManagerPolicyTest.*:PanelWindowTest.*:EmptyStateTest.*' --gtest_list_tests
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='WorkspaceFloatingWindowTest.*:WorkspaceWindowManagerPolicyTest.*:PanelWindowTest.*:EmptyStateTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='Tile16DocumentHistoryTest.*:*TilePaintingManager*:OverworldTilePaintActionTest.*:OverworldPaintRefreshTest.*:MapRefreshCoordinatorTest.*:CanvasNavigationManagerTest.*:OverworldEditorStateTest.*:Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:Tile8SourceInteractionTest.*:MapPropertiesContextMenuTest.*:OverworldContextTargetTest.*:CanvasContextMenuOpenTest.*:CanvasContextMenuRoleTest.*'
build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_list_tests
build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='Tile16EditorSyntheticFixture.*'
git diff --check
```

Build passed. 26 workspace/widget tests, 120 overworld/history/input tests,
and 19 synthetic panel tests passed. Test selection was enumerated. Floating
coverage uses real ImGui frames and two sessions; panel coverage is synthetic.
No native app, installed package, ROM-file persistence, GPU upload, emulator,
remote CI, or release acceptance is implied. Prior scoped Clang analyzer results
apply to the earlier history/painting increment, not the new workspace code.

## Next

Qualify this exact consolidated source with disposable ROMs and native UI:
Tile16 double-click → edit → dock narrow → reopen, two ROM sessions, map paint
and Tile16 edits interleaved with Undo/Redo, then save and independently reopen.
See the repair handoff for sprite persistence and loading-budget follow-ups.
