# Overworld editing flow review — 2026-09-23

## Scope and evidence

Reviewed navigation, map properties, painting/selection, Tile16 editing, scratch,
entities, overlays, history, and save handoff. This is a source-backed workflow
review plus bounded live checks, not a complete gameplay or save qualification.

- Yaze source baseline: `0f3855d6f2df4d8cad409ec10a23ef98880d5676`, branch
  `codex/dungeon-workbench-bottom-drawer`, with the scoped working-tree changes
  described below. Existing sprite-owner changes were preserved.
- ZScream source inspected locally at `0f6812da95bd4e4707e482df01934fadffdd6802`:
  [SceneOW.cs](https://github.com/Zarby89/ZScreamDungeon/blob/0f6812da95bd4e4707e482df01934fadffdd6802/ZeldaFullEditor/Gui/Scene/SceneOW.cs)
  and [OverworldEditor.Designer.cs](https://github.com/Zarby89/ZScreamDungeon/blob/0f6812da95bd4e4707e482df01934fadffdd6802/ZeldaFullEditor/Gui/MainTabs/OverworldEditor.Designer.cs).
  This is the inspected checkout, not a claim about the newest release.
- Hyrule Magic reference: the author's firsthand workflow descriptions in
  [Orochimaru's Perfect Guide, sections 03-01 through 03-20](https://doczz.net/doc/534017/orochimaru-s-perfect-guide-to-hyrule-magic).
  Search-index text exposed the relevant guide sections; the directly opened
  mirror did not expose the full transcription. No HM executable was run.
- Live Yaze verification used a newly built, separate app instance with an
  isolated `YAZE_APP_DATA_DIR` and a disposable copy of the local vanilla ROM.
  Existing main-checkout and combined-candidate sessions were not modified.

## Fixes delivered

The user explicitly chose **follow cursor whenever unpinned**. This supersedes
previous click-only selection plans.

1. Unpinned selection now follows the physical screen under the cursor in Mouse,
   Brush, and Fill modes. Tracking runs before tile edits. Repeated hover within
   one screen does not repeat selection refreshes. Entity dragging holds the
   selection until release.
2. Middle-click no longer pins or opens properties. Pinning remains available in
   the toolbar, Ctrl+L, and the context menu. This removes the conflict between
   starting a middle-drag pan and freezing the property target.
3. Tracking reuses context-target scale/bounds validation. Negative subpixel
   coordinates, non-finite coordinates, and unallocated Special World rows do
   not select screen zero or another invalid screen.
4. Property readbacks now distinguish the physical screen from its parent area.
   Quick controls, sidebar sections, and toolbar metadata read parent properties.
   Sprite metadata labels use the selected game state rather than state zero.
   World switching refreshes toolbar metadata before the remaining controls draw.
5. Common properties remain directly above the canvas: Game State, Area GFX,
   Area Palette, Sprite GFX, Sprite Palette, and Message ID. Inputs fit their
   cells without overflowing step buttons. Same-world hover tracking preserves
   the existing paint-stroke undo batch across screen boundaries.

## Workflow comparison and remaining improvements

| Stage | Reference workflow | Yaze evidence and recommendation |
| --- | --- | --- |
| Choose an area | HM exposes an area list. ZScream resolves its selected screen and parent on mouse-down before dispatching the editing mode. | Tracking is fixed to the user's requested hover policy. Add a persistent **Following cursor / Pinned** label and visible physical-screen/parent IDs. Current icon-only pin state is easy to miss. |
| Configure an area | HM's Properties tool groups sign text and event-dependent music/ambience. ZScream has a Selected map group, graphics fields, and event-labelled music/ambient controls. | The new quick row restores direct access. `OverworldSidebar::DrawMusicTab` still exposes four generic music bytes. Use named event rows and music/ambience selectors with hex values as secondary detail. |
| Select and edit tiles | HM documents block-ID entry, tile search, double-click block editing, and a path from map tile to Tile16 to graphics. ZScream exposes tile search and Tile16/ScratchPad tabs. | Yaze already has ID filtering, double-click Tile16 editing, right-click sampling, and a captured rectangular brush. Make the active tile/brush size and sample/select gestures visible beside the canvas. Keep the existing document-wide Tile16 undo path. |
| Paint, fill, select, paste | HM separates Draw, Select, Rectangle, and Paste tools. ZScream exposes Tile and Fill modes directly. | Yaze's Brush/Fill toolbar help describes sampling but does not make right-drag rectangular selection obvious. Add concise mode-specific help and a Clear selection action; show Fill Screen scope before a click. |
| Reuse terrain | ZScream gives ScratchPad a dedicated tab next to tile selection. | Yaze has scratch save/load callbacks and a separate workspace panel. Show whether a brush is captured, its dimensions, and whether scratch has data without requiring another panel. |
| Add and edit entities | Both reference editors expose entity-specific tools; ZScream includes entrances, exits, items, sprites, transports, gravestones, and notes. | Yaze's insert menu includes entrances, holes, exits, items, and sprites. Review transport parity separately. Promote entity visibility and active entity mode near the canvas instead of relying on menus and the workbench. |
| Undo entity changes | Reference controls make Undo/Redo prominent; this review does not claim universal entity undo in either reference editor. | **Confirmed source gap:** canvas drag changes `x_`/`y_`, calls `UpdateMapProperties`, and marks dirty without an undo action. Item commands separately use snapshots. Route drag, delete, and property edits through one identity-based mutation/history path; test each entity type. |
| Use contextual actions | Yaze already captures a stable menu target at the opening click. | Canvas context menus are enabled only in Mouse mode, because right-click samples/selects in Brush/Fill. Add an always-visible Map actions button or a documented alternate gesture that preserves the captured target. Do not replace the existing eyedropper. |
| Inspect graphics and overlays | ZScream exposes Overlay and Overlay Animation modes, graphics slots, and mosaic controls. | Yaze has version-gated custom settings and overlay preview. Put feature availability and parent ownership beside each setting, and distinguish preview-only actions from persisted edits. Expanded-ROM rendering was not live-tested here. |
| Save and test | ZScream's toolbar has Save, Save and Debug, and Save and Run. | Yaze's save path has sprite-plan validation and hack-manifest checks. Add a clear changed-map/entity summary and an explicit handoff to emulator testing. This review did not save the disposable ROM or validate persistence/reload. |

## Prioritized next work

1. **Entity history correctness:** one undoable transaction per drag/delete/property
   edit, with stable identities and release-outside-canvas coverage.
2. **Visible editing state:** screen/parent, Following/Pinned, active mode, brush
   dimensions, and clear selection. Preserve usable layout at narrow dock widths.
3. **Map actions in every mode:** reach map properties and view controls without
   leaving Brush/Fill or sacrificing sampling and rectangular selection.
4. **Named music and property controls:** event labels, music/ambience names,
   property previews, and consistent parent-aware reads across remaining legacy
   popup implementations.
5. **Complete round-trip acceptance:** paint across screens, edit a Tile16,
   place/move entities, undo/redo, save a copy, reload, and test in an emulator.
   Include vanilla and expanded ROMs, all worlds, zoom/scroll, and game states.

## Verification

Build completed successfully (linker emitted duplicate-library warnings):

```sh
cmake --build build/presets/mac-ai --config Debug \
  --target yaze_test_quick_unit_editor yaze --parallel 4
```

The rebuilt test executable ran **76 tests from 9 suites; all passed**, including
8 new tracking regressions and 1 new parent/game-state metadata regression:

```sh
build/presets/mac-ai/bin/Debug/yaze_test_quick_unit_editor \
  --gtest_filter='CanvasMapTrackingTest.*:CanvasNavigationManagerTest.*:OverworldMapMetadataTest.*:OverworldPropertyEditTest.*:OverworldPropertyBatchEditActionTest.*:TilePaintingManager*.*:OverworldTilePaintActionTest.*:Tile16DocumentHistoryTest.*'
git diff --check
```

Live observations in the isolated app:

- Moving the pointer using canvas scroll actions changed screen `0x00` to `0x01`
  and back without clicking a map. Toolbar and sidebar agreed.
- Screen `0x01` displayed `Area properties: parent 0x00`, with matching quick and
  sidebar values.
- After activating Pin, movement over screen zero held screen `0x01`. Unpin
  resumed tracking. Tracking continued after clicking Brush and Fill controls.
- Middle-click followed by pointer movement did not freeze selection.
- The quick row wrapped and remained readable in the normal and enlarged window.

Not established: every editing flow in live UI; a continuous physical middle-drag;
all scaling factors; expanded-ROM effects; entity undo correctness; save/reload;
gameplay; CI; installed-app deployment. The existing user sessions continue to
run their previous binaries until deliberately replaced.

## Combined-candidate integration scope

The missing entity-history finding above applies to the reviewed main-checkout
baseline, not the combined candidate, which already has entity-history edits.
Integration preserves those edits, the no-hover guard, and pinned hover status.
The two navigation overlaps were resolved with the overworld owner.
