# Dungeon Proposal Preview (v1)

Status: v1 preview supports room shapes, actor anchors, object suggestions, and optional ordered scene steps. Verification receipts belong to each candidate; the format itself does not establish gameplay acceptance.

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

When editor control is enabled, `scripts/yaze_editor_control.py proposal-load /absolute/proposal.json` opens the same preview without a file dialog.
Optional scene steps add **All steps**, **Preview step**, **Previous**, and **Next**. A selected step filters the layers; it does not turn a manually hidden layer back on.

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
- `actor`: `tile` is the exact top-left 8 px tile origin, with optional `offset: [x, y]` pixels (0–7). `facing` is required: `north`, `east`, `south`, or `west`. The marker and arrow do not imply loaded sprite art or collision bounds.
- `object`: `tile`, `object_id`, and `footprint: [width, height]` are required. The footprint is explicitly supplied in tiles, not inferred from the draw routine. Optional `size` (0–255) and `list` (0–2) describe the suggestion. Corner brackets mark an unplaced object; no ROM or undo data changes.
- Optional root `steps` is an ordered list of `{ "id": "wake", "label": "Wake in cell", "detail": "...", "layers": ["actors"] }`. IDs must be unique and every layer reference must exist. The label defaults to the step ID.
- `label`, `detail`, `visible`, `title`, `status`, `notes` are optional.

Example shapes inside an existing layer:

```json
[
  {"room":"$65", "type":"actor", "tile":[26,43], "facing":"east", "label":"Link"},
  {"room":"$65", "type":"object", "tile":[44,52], "object_id":"$13D", "size":0, "list":0, "footprint":[4,3], "label":"Table idea"}
]
```

Existing version-1 documents without steps or new shape types retain their full preview.

The parser rejects: wrong format or version, no rooms, duplicate rooms,
room ids out of range, shapes on rooms not in `rooms`, tiles outside 0–63,
bad colors, duplicate layer ids, invalid actor directions or offsets, invalid object IDs or footprints, and missing or duplicate step references.

## Code

- `src/app/editor/dungeon/dungeon_proposal_overlay.{h,cc}`: model, parser, geometry helpers.
- `src/app/editor/dungeon/dungeon_canvas_proposal_preview.cc`:
  `DungeonCanvasViewer::DrawProposalPreview` uses `PrepareConnectedRoomCompositeBitmap`
  (the same composites as the connected-room view) and draws shapes with the ImGui draw list.
- `src/app/editor/dungeon/workspace/dungeon_workbench_content.{h,cc}`: window, file load, menu item.
- `test/unit/editor/dungeon_proposal_overlay_test.cc`: parser, coordinate, footprint, step, and legacy-format tests.

## Example

Oracle of Secrets: `Docs/Planning/Sketches/goron_77_87_preview_overlay.json`
(rooms $87 and $77, 8 layers, 28 shapes).

## Focused checks

```sh
cmake --build build/presets/mac-ai --target yaze yaze_test_unit --parallel 2
build/presets/mac-ai/bin/Debug/yaze_test_unit \
  --gtest_filter='DungeonProposalOverlayTest.*'
```

Then open the Goron example and check: both rooms render; shapes line up with
the room tiles at 1x and 2x; layer toggles work; the tooltip shows labels;
"Open $77" selects the room; a bad JSON shows an error and keeps the old overlay.
For a stepped proposal, check Next/Previous, All steps, actor facing, and object footprints. Save/reopen checks remain separate from proposal rendering.

## Later (not in v1)

- Draw proposals on the main canvas as a layer.
- "Apply" into a scratch copy with undo.
- Emit overlays from z3ed or layout manifests.
