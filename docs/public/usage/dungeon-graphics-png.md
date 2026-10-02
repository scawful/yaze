# Dungeon graphics PNG interchange

A dungeon background sheet has eight raw color indices, 0–7. Its visible colors
depend on the room palette, the entrance's main graphics group, and the slot
where it is loaded. Some slots use colors 1–7 of a palette row; others use 9–15.
Raw index 0 stays transparent in both cases.

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
