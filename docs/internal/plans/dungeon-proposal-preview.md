# Dungeon Proposal Preview (v1)

Status: v1 implemented in `codex/combined-editor-candidate` worktree. Uncommitted.
Linux: parser unit tests pass; UI TUs pass a syntax check. Mac build and visual check: pending.

## Purpose

Show a layout proposal on top of the real rendered rooms, in a separate window.
The window shows only the rooms that the proposal names.
It is read-only. It does not change room data, the ROM, or undo history.

## Open it

1. Open a ROM and the Dungeon workbench.
2. Sidebar quick actions ("...") → **Proposal Preview...**.
3. Click **Open...** and select an overlay JSON. Or type a path and press Enter.
4. Use **Reload** after you edit the JSON.

Controls: "Show proposal" toggle, one checkbox per layer, zoom (0.5x–2.0x),
"Open $XX" buttons (select that room in the main canvas), hover tooltips
(room, tile, and the label/detail of shapes under the cursor).

If a reload fails, the window keeps the last good overlay and shows the error.

## File format

Units: local 8 px tiles, 0–63 on each axis. Rects are inclusive.

```json
{
  "format": "yaze-dungeon-proposal-overlay",
  "version": 1,
  "title": "…",
  "status": "…",
  "notes": ["…"],
  "rooms": [{"room": "0x87", "label": "…"}, "0x77"],
  "layers": [
    {
      "id": "floor",
      "label": "New floor",
      "color": "#4CAF50CC",
      "visible": true,
      "shapes": [
        {"room": "0x87", "type": "rect", "tiles": [40, 16, 48, 23], "label": "dock"},
        {"room": "0x77", "type": "path", "tiles": [[16, 44], [24, 44]], "label": "route"},
        {"room": "0x77", "type": "marker", "tile": [16, 44], "label": "A", "detail": "…"},
        {"room": "0x87", "type": "remove", "tile": [44, 10], "label": "retired"}
      ]
    }
  ]
}
```

- Room ids: integer, `"0x87"`, `"$87"`, or `"135"`. Max `0x127`.
- Rooms draw left to right in list order.
- Colors: `#RRGGBB` or `#RRGGBBAA`.
- Shape types: `rect` (fill + outline), `path` (≥2 points, arrow at the end),
  `marker` (circle + label), `remove` (X in a circle).
- `label`, `detail`, `visible`, `title`, `status`, `notes` are optional.

The parser rejects: wrong format or version, no rooms, duplicate rooms,
room ids out of range, shapes on rooms not in `rooms`, tiles outside 0–63,
bad colors, duplicate layer ids.

## Code

- `src/app/editor/dungeon/dungeon_proposal_overlay.{h,cc}`: model, parser, geometry helpers.
- `src/app/editor/dungeon/dungeon_canvas_proposal_preview.cc`:
  `DungeonCanvasViewer::DrawProposalPreview` uses `PrepareConnectedRoomCompositeBitmap`
  (the same composites as the connected-room view) and draws shapes with the ImGui draw list.
- `src/app/editor/dungeon/workspace/dungeon_workbench_content.{h,cc}`: window, file load, menu item.
- `test/unit/editor/dungeon_proposal_overlay_test.cc`: 8 tests.

## Example

Oracle of Secrets: `Docs/Planning/Sketches/goron_77_87_preview_overlay.json`
(rooms $87 and $77, 8 layers, 28 shapes).

## Mac checks (pending)

```sh
cmake --build --preset mac-ai --target yaze yaze_test_quick_unit_editor --parallel 4
$(find build/presets/mac-ai -name yaze_test_quick_unit_editor -type f -perm -u+x | head -1) \
  --gtest_filter='DungeonProposalOverlayTest.*'
```

Then open the Goron example and check: both rooms render; shapes line up with
the room tiles at 1x and 2x; layer toggles work; the tooltip shows labels;
"Open $77" selects the room; a bad JSON shows an error and keeps the old overlay.

## Later (not in v1)

- Draw proposals on the main canvas as a layer.
- "Apply" into a scratch copy with undo.
- Emit overlays from z3ed or layout manifests.
