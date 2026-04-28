# Overworld Editor Metadata UX Handoff - 2026-04-27

Status: Follow-up required
Owner: `imgui-frontend-engineer`
Related coordination tasks:
- `task_20260426T160120Z_18696`
- `task_20260427T135554Z_17281`
- `task_20260427T142410Z_22821`
- `task_20260428T053849Z_3809`

## Scope Completed

This pass consolidated overworld map metadata editing around a shared property
edit path so toolbar, map properties, sidebar, and canvas context-menu edits do
not mutate map state through separate code paths.

Implemented pieces:
- `OverworldPropertyEdit` and `OverworldMapMetadataClipboard` shared payloads.
- `MapPropertiesSystem::ApplyPropertyEdit`, `ApplyPropertyEdits`,
  `ApplyPropertyEditDirect`, and `ReadPropertyValue`.
- `OverworldEditor::ApplyOverworldPropertyEdit` and
  `ApplyOverworldPropertyEdits` wrappers that finalize pending paint edits and
  record undo actions.
- `OverworldMapPropertyEditAction` for single metadata changes.
- `OverworldMapPropertyBatchEditAction` for context-menu metadata paste.
- `OverworldProjectLabelEditAction` for project resource label edits.
- Canvas context menu actions for selecting a parent map, copying map metadata,
  pasting map metadata, and renaming map labels.
- Toolbar metadata editor popup for common overworld metadata, keeping
  ZScream-style top-row editing without requiring mouse travel to the side
  properties area.

2026-04-28 continuation:
- Canvas context menu now groups metadata copy/paste actions under
  `Copy / Paste Metadata`.
- Added scoped copy/paste for graphics metadata, palette metadata, and
  music/message metadata.
- Added related-map context-menu navigation for parent and sibling maps.
- Added scoped paste builders so partial clipboard copies cannot enable
  unrelated paste actions.

## Behavior Notes

- Child maps route property reads and writes to their effective parent map when
  the property belongs to the parent area.
- ROM-version support is checked before applying a field:
  - Wide/Tall area sizes require ZSCustomOverworld v3+.
  - Main palette, area-specific background color, and directional mosaic
    require v2+.
  - Animated graphics require v3+.
  - Custom tile graphics require expanded-space support.
  - Subscreen overlays require a non-vanilla custom overworld.
- Metadata paste is now one undo step. Unsupported fields are skipped for the
  current ROM version instead of aborting the paste after partially applying
  earlier fields.
- Full metadata copies can paste either the full payload or a scoped subset.
  Scoped copies only paste the matching scope, so copying palettes does not
  later overwrite graphics or music/message fields.
- Direct ROM byte/word writes are limited to fields that have stable direct
  tables in the current editor path, such as light/dark-world music, area
  background color, and subscreen overlay.

## Validation

Commands run:

```bash
clang-format -i \
  src/app/editor/overworld/map_properties.h \
  src/app/editor/overworld/map_properties.cc \
  src/app/editor/overworld/overworld_editor.h \
  src/app/editor/overworld/overworld_editor.cc \
  src/app/editor/overworld/overworld_undo_actions.h \
  src/app/editor/overworld/overworld_property_edit.h \
  src/app/editor/overworld/overworld_property_edit.cc \
  test/unit/editor/overworld_property_edit_test.cc

cmake --build --preset mac-ai --target yaze_test_unit -j8

build/presets/mac-ai/bin/Debug/yaze_test_unit \
  --gtest_filter='OverworldPropertyEditTest.*:OverworldPropertyBatchEditActionTest.*:MapPropertiesContextMenuTest.*:OverworldMapMetadataTest.*:OverworldEditorStateTest.*:CanvasNavigationManagerTest.*:OverworldRegressionTest.SaveMapProperties_DarkWorldDoesNotOverwriteLightWorldSpriteTables:OverworldRegressionTest.SaveMapProperties_V3PersistsSpecialWorldToExpandedTables'

git diff --check -- \
  src/app/editor/overworld/map_properties.h \
  src/app/editor/overworld/map_properties.cc \
  src/app/editor/overworld/overworld_editor.h \
  src/app/editor/overworld/overworld_editor.cc \
  src/app/editor/overworld/overworld_undo_actions.h \
  test/unit/editor/overworld_property_edit_test.cc

cmake --build --preset mac-ai --target yaze -j8

mkdir -p /Applications/yaze.app
rsync -a --delete build/presets/mac-ai/bin/Debug/yaze.app/ /Applications/yaze.app/
test -x /Applications/yaze.app/Contents/MacOS/yaze
stat -f '%Sm %N' /Applications/yaze.app/Contents/MacOS/yaze
```

Focused overworld test result: 29 tests passed.
Deployed app binary timestamp: `Apr 27 10:05:06 2026`.

2026-04-28 continuation validation:

```bash
clang-format -i \
  src/app/editor/overworld/overworld_property_edit.h \
  src/app/editor/overworld/overworld_property_edit.cc \
  src/app/editor/overworld/map_properties.cc \
  test/unit/editor/overworld_property_edit_test.cc

cmake --build --preset mac-ai --target yaze_test_unit -j8

build/presets/mac-ai/bin/Debug/yaze_test_unit \
  --gtest_filter='OverworldPropertyEditTest.*:OverworldMetadataPasteEditTest.*:OverworldPropertyBatchEditActionTest.*:MapPropertiesContextMenuTest.*:OverworldMapMetadataTest.*:OverworldEditorStateTest.*:CanvasNavigationManagerTest.*:OverworldRegressionTest.SaveMapProperties_DarkWorldDoesNotOverwriteLightWorldSpriteTables:OverworldRegressionTest.SaveMapProperties_V3PersistsSpecialWorldToExpandedTables'

git diff --check -- \
  src/app/editor/overworld/overworld_property_edit.h \
  src/app/editor/overworld/overworld_property_edit.cc \
  src/app/editor/overworld/map_properties.cc \
  test/unit/editor/overworld_property_edit_test.cc

cmake --build --preset mac-ai --target yaze -j8

mkdir -p /Applications/yaze.app
rsync -a --delete build/presets/mac-ai/bin/Debug/yaze.app/ /Applications/yaze.app/
test -x /Applications/yaze.app/Contents/MacOS/yaze
stat -f '%Sm %N' /Applications/yaze.app/Contents/MacOS/yaze
```

Focused overworld test result: 31 tests passed.
Deployed app binary timestamp: `Apr 28 01:46:58 2026`.

Known caveats:
- Full `git diff --check` still reports pre-existing trailing whitespace in
  `src/app/gfx/core/bitmap.h` from another dirty worktree lane. The scoped
  overworld diff check passed.
- Build startup still prints `ninja: warning: premature end of file; recovering`;
  both requested build targets completed successfully.

## Next Follow-Ups

1. Live smoke `/Applications/yaze.app` against the Oracle ROM:
   - Load the Oracle edit target ROM, not the patched emulator-only target.
   - Switch Light World, Dark World, and Special World.
   - Edit toolbar metadata.
   - Copy/paste metadata from the canvas context menu.
   - Undo/redo after metadata paste.
   - Confirm Dark World and Special World rendering remains stable after edits.
2. Add editable project labels for referenced metadata:
   - GFX groups.
   - Palette IDs.
   - Music IDs.
   - Message IDs.
   - Overlay IDs.
3. After live validation, demote duplicate side properties controls that are
   now better handled from the toolbar or context menu.
4. Consider a small clipboard-scope indicator in the toolbar/context menu if
   users need clearer feedback after copying a scoped metadata subset.
