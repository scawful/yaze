# Dungeon graphics PNG interchange

A dungeon background sheet has eight raw color indices, 0–7. Its visible colors
depend on the room palette, the entrance's main graphics group, and the slot
where it is loaded. Some slots use colors 1–7 of a palette row; others use 9–15.
Raw index 0 stays transparent in both cases.

## Reorder source colors without changing selected artwork

1. Use **Edit Graphics** on a dungeon object or Overworld Tile16. The source
   supplies its actual palette row, half, graphics sheets, and entrance context.
2. Beside the Pixel Editor's color swatches, choose **Reorder**. Choose
   **Current tile** (the initial scope) or **Source pattern** (the source's
   resolved tiles). Click two colors in the **After** row to exchange slots.
3. Review the tile/color counts, then **Apply**. **Cancel** changes nothing.
   Slot 0 stays transparent. One Graphics **Undo** restores both colors and pixels.
4. Use **Return to source** to review the room/map, then File **Save As**.
   Palette and graphics saving must both be enabled in the project's save policy.

This changes the real palette slots and selected indexed pixels together; it is
not a preview-palette adjustment. Other artwork sharing those palette slots or
physical tiles can change. The operation does not clone resources or claim a
complete game-wide usage census.

The operation supports loader-addressed, static 3bpp targets in dungeon and
Overworld sources. Missing mappings, animated/HUD sources, conflicting selected
mappings, stale entrance/sheet assignments, and foreign sessions are refused.
Changing the source while the popup is open requires closing and reopening it.
An older Palette-editor Undo/Redo that conflicts with this edit stays retained
but unavailable until its expected colors are restored; undo the paired edit
in Graphics first.

Headless product checks cover the actual control, one-step Undo/Redo, failed-save
retention, and Save As/fresh reopen on disposable Oracle copies, including the
custom wall-corner pattern in room `$089`. This is not whole-game visual or
runtime qualification.

## Export one sheet with its room colors

In **Graphics → Sheet Browser → PNG export and import**, select **Room background
slot**, the room, and the palette row. The **Loaded slot** menu lists the slots
that use the selected sheet. Choose the intended slot when it appears more than
once. **Export sheet PNG** includes the sheet's unsaved pixel edits. Imports
continue through preview, Apply import, undo, and the normal graphics save path.

When arriving through **Edit Graphics** from a source tile, **Selected source
tile** reuses that target's supplied colors and entrance context directly.
The manual room-slot mode resolves the saved room header and default entrance.

The corresponding CLI workflow is:

```sh
z3ed gfx-export --rom /path/to/base.sfc --sheet 0x12 \
  --palette room-slot:0xE6:2:2 --png /path/to/wall.png \
  --json /path/to/wall.json
z3ed gfx-import --rom /path/to/base.sfc --sheet 0x12 \
  --palette room-slot:0xE6:2:2 --png /path/to/wall.png --dry-run
z3ed gfx-import --rom /path/to/base.sfc --sheet 0x12 \
  --palette room-slot:0xE6:2:2 --png /path/to/wall.png \
  --write --out /path/to/edited.sfc
```

`room-slot:<room>:<slot>:<row>` uses destination slots 0–7 (`tile ID / 64`),
not the reversed runtime upload counter. The command refuses a sheet that the
requested slot does not load. Numbers are decimal unless prefixed with `0x` or
`$`. These example IDs describe Oracle's cave sheet; inspect another ROM's
context instead of assuming it uses the same sheet.

The JSON records the physical sheet, roomset override, main group, entrance
provenance, palette row and half, exact RGB colors, and raw-index-to-CGRAM mapping.
An inferred default entrance describes the editor's reconstruction, not a proven
gameplay route. The source sheet excludes animation overlays.

Keep PNGs indexed with values 0–7 for lossless interchange. RGB/RGBA import
requires exact palette colors and does not quantize. When palette entries have
identical RGB values, RGB pixels cannot distinguish those indices; indexed PNG
retains that information. Pass `--project-context /path/to/project.yaze` on imports
to apply the project's protected regions and graphics allocation policy. Writes
use the existing sheet writer and refuse unsupported growth or protection conflicts.

## Export the room's sheets

```sh
z3ed gfx-room-export --rom /path/to/base.sfc --room 0xE6 --row 2 \
  --out-dir /path/to/new-room-bundle
```

The parent folder must exist and the output directory must be new. Each supported
slot gets its own PNG with raw indices 0–7 and the appropriate palette.
`manifest.json` identifies each file and its matching `gfx-import` palette spec.
Import an edited sheet with `gfx-import`; there is no bulk bundle import.
Repeated sheet IDs are shared data, not independent assets. Unsupported 2bpp
slots are explicitly marked and are not exported.

Existing `room-bg:<room>:<row>` explicitly selects the **left half**. The legacy
128×256 `gfx-room-export --png` and the panel's room PNG buttons apply one uniform
palette to all slots. They remain compatible with earlier indexed files but do
not represent every slot's displayed room colors. `room-slot` applies only to
single-sheet interchange; use the bundle for per-slot colors across a room.
