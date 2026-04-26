# Dungeon UI Stability Handoff (2026-04-26)

## Context

Goal: stabilize a visibly broken DungeonEditorV2 workspace after recent UI/UX and
ZS-style interaction work, then leave a safe continuation point.

Dominant surface: `ui_ux_editor` for the code changes, followed by
`docs_process` for this handoff. Current owner for follow-up should be
`imgui-frontend-engineer` unless the next slice is only build/test cleanup.

Important user constraint: do not visibly launch or focus `/Applications/Yaze.app`
unless the user explicitly asks. Use focused, non-GUI verification by default.

## Current State

The stabilization patch is implemented but not committed. The working tree also
contains concurrent editor work from the broader ZS parity/editor grammar effort,
so do not blindly `git add -A` or commit the whole dirty tree.

Stabilization changes made in this pass:
- `src/app/editor/system/session/user_settings.{cc,h}`: bumped panel layout
  defaults to revision 18 and pruned numeric transient `dungeon.room_<digits>`
  visibility/pin state.
- `src/app/editor/system/workspace/workspace_window_manager_state.cc`: skips
  numeric transient dungeon room panels when saving/restoring visibility.
- `src/app/editor/editor_manager.cc`: avoids writing numeric transient room panel
  visibility into preferences.
- `src/app/editor/system/workspace/workspace_window_manager.cc`: duplicate
  `WindowContent` registration now closes/replaces the old instance, preventing
  stale editor/session draw callbacks after project reload.
- `src/zelda3/dungeon/palette_debug.cc`: successful palette debug events remain
  in memory for issue reports but no longer spam INFO logs unless compiled with
  `YAZE_PALETTE_DEBUG_VERBOSE`; warnings/errors still log.
- `src/app/editor/dungeon/interaction/interaction_coordinator.{cc,h}`: mixed
  entity group drag batches cache/entity notifications until release.

Tests added/updated:
- `test/unit/editor/user_settings_layout_defaults_test.cc`
- `test/unit/editor/workspace_window_manager_policy_test.cc`
- `test/unit/editor/interaction_coordinator_test.cc`

Documentation updated:
- `docs/internal/architecture/editor_card_layout_system.md`
- `docs/internal/architecture/dungeon_interaction_architecture.md`
- `docs/internal/agents/automation-workflows.md`

## Verified

Verified commands from the stabilization pass:

```bash
cmake --build --preset mac-ai --target yaze_test_unit yaze_test_integration yaze --parallel 4
./build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='WorkspaceWindowManagerPolicyTest.*:UserSettingsLayoutDefaultsTest.*:InteractionCoordinatorTest.*:DungeonCanvasViewerNavigationTest.DrawIssueReportIncludesRomContextAndSanePaletteEvents:RoomGraphicsPaletteTest.PaletteDebuggerSamplesMappedDungeonRenderPalette'
./build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='InteractionDelegationTest.*'
git diff --check
```

Results: all passed.

Deployment evidence: `/Applications/Yaze.app` was updated from
`build/presets/mac-ai/bin/Debug/yaze.app`, and the deployed executable hash
matched the build output at that time.

Runtime evidence:
- One real crash log existed before the `WindowContent` replacement fix:
  `~/.yaze/crash_logs/crash_20260426_015046.log`, with a stack through
  `DungeonRoomStore::GetIfMaterialized()` and `DungeonWorkbenchContent::DrawCanvasPane()`.
- Later `crash_*.log` files at 01:53/01:55/01:56 were 114-byte headers with no
  stack. Treat them as unverified placeholders, not proven crashes.
- `pkill -x yaze || true` was run after the user objected to visible launches.

## Open Risks

- No final commit has been made. The dirty tree includes both stabilization
  fixes and concurrent UI/editor changes.
- Manual dungeon GUI verification is incomplete because the user explicitly
  asked to stop launching the app in front of them.
- The `/Applications/Yaze.app` deployment was performed before the final docs
  edits. If code changes are made again, rebuild and resync the bundle without
  opening it.
- Door/entity multi-selection and door copy/paste are still limited by earlier
  editor-grammar scope decisions; do not treat them as fixed by this stability
  pass.

## Next Step

Stage a focused commit for the stabilization/documentation work only. Review
with `git diff --stat` and `git diff -- <paths>` before staging because the tree
contains unrelated/concurrent editor changes.

Recommended commit message:

```text
Stabilize dungeon workspace UI state
```

After that, continue the ZS parity/editor grammar work in a separate commit.
Keep visible GUI checks opt-in.

## Useful Commands

Quiet status and review:

```bash
git status --short
git diff --stat
git diff -- src/app/editor/system/session/user_settings.cc \
  src/app/editor/system/session/user_settings.h \
  src/app/editor/system/workspace/workspace_window_manager.cc \
  src/app/editor/system/workspace/workspace_window_manager_state.cc \
  src/app/editor/editor_manager.cc \
  src/zelda3/dungeon/palette_debug.cc \
  src/app/editor/dungeon/interaction/interaction_coordinator.cc \
  src/app/editor/dungeon/interaction/interaction_coordinator.h \
  test/unit/editor/user_settings_layout_defaults_test.cc \
  test/unit/editor/workspace_window_manager_policy_test.cc \
  test/unit/editor/interaction_coordinator_test.cc
```

Quiet verification:

```bash
git diff --check
cmake --build --preset mac-ai --target yaze yaze_test_unit --parallel 2
./build/presets/mac-ai/bin/Debug/yaze_test_unit \
  --gtest_filter='WorkspaceWindowManagerPolicyTest.*:UserSettingsLayoutDefaultsTest.*:InteractionCoordinatorTest.*'
```

Bundle sync without launch:

```bash
rm -rf /Applications/Yaze.app.deploying
ditto build/presets/mac-ai/bin/Debug/yaze.app /Applications/Yaze.app.deploying
rm -rf /Applications/Yaze.app
mv /Applications/Yaze.app.deploying /Applications/Yaze.app
xattr -dr com.apple.quarantine /Applications/Yaze.app
```

Do not append `open /Applications/Yaze.app` unless the user asks for it.
