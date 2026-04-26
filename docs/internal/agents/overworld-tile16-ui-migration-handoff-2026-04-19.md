# Overworld Tile16 + UI Migration Handoff — 2026-04-19

Status: IN_PROGRESS (not release-ready; follow-up required)
Owner: `imgui-frontend-engineer`  
Created: 2026-04-19  
Last Reviewed: 2026-04-26
Next Review: after action-rail/discard UX fix and adversarial edit-flow coverage
Universe Task: unavailable in snapshot; follow up after `scripts/agents/coord` is healthy again

## Summary

This slice documented and fixed the main Overworld Tile16 correctness issues,
then finished the active overworld `WindowContent` migration away from
`src/app/editor/overworld/panels/` onto `src/app/editor/overworld/ui/...`.

Automated close-out on 2026-04-26 covers ROM-backed Tile16 selection,
staged-change guarding, and commit refresh. The opt-in visible GUI smoke pass
now covers basic rendering, palette switching, and staged live-preview feedback.
Do not treat that as product readiness. The remaining work is action-rail UX,
adversarial edit-flow coverage, large-service extraction, and coordination
snapshot repair.

Follow-up on 2026-04-26 found one concrete basic rendering bug in the selected
Tile16 preview: `SetCurrentTile()` could load pixels from the blockset atlas
before rebuilding from Tile16 metadata and tile8 graphics. That let stale or
blank atlas data win over the authoritative tile definition. The selected-tile
preview now rebuilds from metadata unless an explicit pending edited bitmap is
present.

## Readiness Position

The Tile16 editor should stay marked not release-ready until it has evidence
across all of these gates:

- Manual visible GUI smoke still needed for dialogs, all selection handoff
  paths, and write/discard/undo feedback. Basic render, palette switching, and
  staged live-preview smoke have passed once.
- Adversarial edit coverage for multi-tile pending queues, current-tile changes,
  discard-current vs discard-all, undo/redo interaction, and selection requests
  from selector/eyedropper/keyboard paths.
- Data integrity coverage for save/reload after Tile16 writes on the intended ROM
  profiles, including expanded Tile16/Tile32 ROMs.
- Usability review of the editing surface for discoverability, destructive action
  clarity, keyboard shortcuts, and stale pending-state visibility.
- Service-boundary cleanup so `OverworldEditor` is not the sole owner of too much
  Tile16, map refresh, ROM mutation, and UI state coordination.

## Canonical Sibling Docs

- Plan: [docs/internal/plans/overworld-tile16-editor-fix-plan-2026-04.md](../plans/overworld-tile16-editor-fix-plan-2026-04.md)
- Architecture pattern: [docs/internal/architecture/editor-ui-module-pattern.md](../architecture/editor-ui-module-pattern.md)

## What Landed

- Documented the Tile16 bugs and staged fix plan in the canonical plan doc.
- Unified Tile16 selection through one guarded overworld-owned path so selector,
  eyedropper, drag/drop, and tile cycling share staged-change protection.
- Wired Tile16 commits back into authoritative in-memory overworld Tile16 state
  and refreshed derived blockset/map presentation after commit.
- Fixed selected Tile16 preview rendering so `SetCurrentTile()` regenerates from
  Tile16 metadata plus tile8 graphics instead of trusting the blockset atlas as
  the primary source.
- Refreshed Tile16-bound surfaces on palette changes and exposed `Redo` in the
  Tile16 action rail.
- Moved active overworld window surfaces into feature-oriented `ui/` modules:
  - `ui/shared/overworld_window_context.h`
  - `ui/canvas/overworld_canvas_view.{h,cc}`
  - `ui/tiles/{tile8_selector_view,tile16_selector_view,tile16_editor_view}.{h,cc}`
  - `ui/graphics/{area_graphics_view,gfx_groups_view}.{h,cc}`
  - `ui/properties/{map_properties_view,v3_settings_view}.{h,cc}`
  - `ui/items/overworld_item_list_view.{h,cc}`
  - `ui/workspace/scratch_space_view.{h,cc}`
  - `ui/analytics/usage_statistics_view.{h,cc}`
  - `ui/debug/debug_window_view.{h,cc}`
- Updated the module-pattern RFC so it explicitly treats the overworld `ui/`
  tree as canonical.

## Validation Completed

```bash
cmake --preset mac-ai-fast
ctest --test-dir build/presets/mac-ai-fast -R 'Tile16EditorActionStateTest|Tile16EditorShortcutsTest|MapRefreshCoordinatorTest' --output-on-failure
ctest --test-dir build/presets/mac-ai-fast -I 1465,1481,1 --output-on-failure
ninja -C build/presets/mac-ai-fast src/CMakeFiles/yaze_editor.dir/Debug/cmake_pch.hxx.pch src/CMakeFiles/yaze_editor.dir/Debug/app/editor/overworld/ui/debug/debug_window_view.cc.o
```

Observed results:

- Focused Tile16/unit coverage passed: 22/22.
- Synthetic Tile16 integration coverage passed: 2/2.
- ROM-backed `OverworldEditorTest` cases in the same integration slice still
  skip in this environment.
- The moved overworld `ui/*View` sources compile inside the `yaze_editor`
  library surface.

2026-04-26 automated close-out validation:

```bash
cmake --build --preset mac-ai --target yaze yaze_test_unit yaze_test_integration --parallel 2
./build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:MapRefreshCoordinatorTest.*:OverworldEditorStateTest.*:Tile16MetadataTest.*:Tile16RendererTest.*:Tile16UsageIndexTest.*'
./build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='OverworldEditorTest.RequestTile16SelectionWithStagedCurrentTileOpensEditorGuard:OverworldEditorTest.CommitTile16ChangesRefreshesOverworldState:OverworldEditorTest.RequestTile16SelectionUpdatesEditorSelection'
./build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='OverworldEditorTest.*:Tile16EditorSyntheticFixture.*:Tile16EditorIntegrationTest.CommitAllChangesClearsPendingQueue:Tile16EditorIntegrationTest.SetCurrentTileWithROM:Tile16EditorIntegrationTest.ValidateTile16DataWithROM'
```

Observed results:

- `yaze`, `yaze_test_unit`, and `yaze_test_integration` built successfully.
  Warnings observed were duplicate-library linker warnings; later incremental
  reruns also printed `ninja: warning: premature end of file; recovering`
  before rebuilding successfully.
- Focused Tile16/unit coverage passed: 44/44.
- Targeted ROM-backed Overworld Tile16 regression coverage passed: 3/3.
- Broader Overworld/Tile16 integration coverage passed: 25/25.
- No visible app launch was performed in this automated close-out; see the
  2026-04-26 opt-in visible GUI smoke below for the later visible pass.

2026-04-26 selected Tile16 rendering regression validation:

```bash
cmake --build --preset mac-ai --target yaze_test_integration --parallel 2
./build/presets/mac-ai/bin/Debug/yaze_test_integration --gtest_filter='Tile16EditorSyntheticFixture.SetCurrentTileRendersFromMetadataNotStaleBlocksetAtlas:Tile16EditorSyntheticFixture.*:Tile16EditorIntegrationTest.ValidateTile16DataWithROM:Tile16EditorIntegrationTest.SetCurrentTileWithROM'
cmake --build --preset mac-ai --target yaze_test_unit --parallel 2
./build/presets/mac-ai/bin/Debug/yaze_test_unit --gtest_filter='Tile16EditorActionStateTest.*:Tile16EditorShortcutsTest.*:MapRefreshCoordinatorTest.*:OverworldEditorStateTest.*:Tile16MetadataTest.*:Tile16RendererTest.*:Tile16UsageIndexTest.*'
```

Observed results:

- `yaze_test_integration` built successfully. The linker still emitted existing
  duplicate-library warnings.
- Focused Tile16 editor rendering/editing coverage passed: 12/12.
- Focused Tile16 renderer/state unit coverage passed: 44/44.
- The new stale-atlas regression proves the selected Tile16 preview comes from
  metadata plus tile8 graphics, not from the blockset atlas.
- This regression test does not prove the full visible selector/blockset atlas
  or overworld map presentation; the visible smoke below covers the first pass
  for those surfaces.

2026-04-26 opt-in visible GUI smoke validation:

```bash
cmake --build --preset mac-ai --target yaze --parallel 2
YAZE_APP_DATA_DIR=/tmp/yaze-tile16-visible-smoke2/appdata \
  ./build/presets/mac-ai/bin/Debug/yaze.app/Contents/MacOS/yaze \
  --rom_file=roms/alttp_vanilla.sfc \
  '--editor=Overworld Editor' \
  --map=0 \
  --open_panels=overworld.canvas,overworld.tile16_selector,overworld.tile16_editor \
  --startup_welcome=hide \
  --startup_dashboard=hide \
  --startup_sidebar=show \
  --enable_test_harness \
  --test_harness_port=50052 \
  --log_file=/tmp/yaze-tile16-visible-smoke2/yaze.log \
  --log_to_console \
  --log_level=info
./build/presets/mac-ai/bin/Debug/z3ed --rom=roms/alttp_vanilla.sfc --gui-server-address localhost:50052 gui-discover-tool --format json
./build/presets/mac-ai/bin/Debug/z3ed --rom=roms/alttp_vanilla.sfc --gui-server-address localhost:50052 gui-screenshot
swift -e 'import CoreGraphics; import Foundation; let p = CGPoint(x: 1081, y: 869); let src = CGEventSource(stateID: .hidSystemState); let down = CGEvent(mouseEventSource: src, mouseType: .leftMouseDown, mouseCursorPosition: p, mouseButton: .left)!; let up = CGEvent(mouseEventSource: src, mouseType: .leftMouseUp, mouseCursorPosition: p, mouseButton: .left)!; down.post(tap: .cghidEventTap); usleep(80000); up.post(tap: .cghidEventTap);'
swift -e 'import CoreGraphics; import Foundation; let p = CGPoint(x: 1060, y: 1001); let src = CGEventSource(stateID: .hidSystemState); let down = CGEvent(mouseEventSource: src, mouseType: .leftMouseDown, mouseCursorPosition: p, mouseButton: .left)!; let up = CGEvent(mouseEventSource: src, mouseType: .leftMouseUp, mouseCursorPosition: p, mouseButton: .left)!; down.post(tap: .cghidEventTap); usleep(80000); up.post(tap: .cghidEventTap);'
```

Observed results:

- Visible launch reached the overworld surface with `Overworld Canvas`,
  `Tile16 Editor`, `Tile16 Selector`, and `Map Properties` open.
- Launch caveat: even with `--editor=Overworld Editor`, startup logs still
  reported `Unknown editor specified via flag: Overworld`. The surface reached
  Overworld because the requested panel visibility was restored, so the startup
  editor parser still needs cleanup.
- Initial screenshot showed the overworld map, selected Tile16 preview, Tile16
  blockset preview, and Tile8 source all visible and nonblank.
- Startup logs showed the selected Tile16 preview regenerated from ROM data:
  `Regenerated Tile16 bitmap for tile 0 from ROM data`.
- Real pointer input switched the Tile16 brush palette from 0 to 2; the header
  and active palette button updated, and logs reported
  `Palette successfully changed to 2`.
- Applying the brush to the active quadrant staged tile 0, changed the selected
  Tile16 preview, changed the blockset preview, and updated the visible minimap
  area. The status strip showed `Tile 00: STAGED | Queue: 1 tile pending`, and
  logs reported `Marked tile 0 as modified (total pending: 1)`.
- The harness `gui-discover-tool` only surfaced one visible widget
  (`PanelWindow:Map Properties`), so visible smoke used screenshots plus real
  pointer events rather than relying on widget discovery.
- Remaining UX gap: at the default visible layout the bottom action rail is
  effectively below the immediately usable area. The status text promises
  `Write Pending / Discard / Undo`, but the buttons are clipped/awkward to
  operate without resizing or scrolling. A `gui-click` target for
  `Discard Current` did not visibly clear the staged state during this smoke
  pass, so discard/write feedback still needs a focused follow-up.

## Follow-Up Required

1. Fix and re-smoke the bottom action rail so `Write Pending`, `Discard Current`,
   `Discard All`, and `Undo` are visible and operable at the default launched
   Tile16 layout, with clear feedback after each action.
2. Add adversarial Tile16 edit-flow coverage for pending queues, discard paths,
   undo/redo interaction, and all selection entry points.
3. Add save/reload coverage for committed Tile16 edits on the intended ROM
   profiles, including expanded Tile16/Tile32 ROMs.
4. Revisit the remaining large-service extraction in `OverworldEditor` and its
   coordinators. The directory migration is done for active surfaces, but the
   editor is still too state-heavy.
5. Fix the startup editor flag/parser mismatch so `--editor=Overworld Editor`
   or a documented category alias opens Overworld without an unknown-editor
   warning.
6. Regenerate `docs/internal/agents/coordination-board.generated.md` once
   `scripts/agents/coord` / `universe-coord.sh` is healthy again. Current local
   coordination calls appear wedged, so this handoff is the human-readable
   record for now.
