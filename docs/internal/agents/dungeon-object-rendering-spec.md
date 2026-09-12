# Dungeon Object Rendering & Selection Spec (ALTTP)

Status: ACTIVE  
Owner: zelda3-hacking-expert  
Created: 2025-12-06  
Last Reviewed: 2026-09-10
Next Review: 2026-12-09
Coordination: Universe task lifecycle via `scripts/agents/coord` (snapshot optional: `docs/internal/agents/coordination-board.generated.md`)

## Scope
- Source of truth: `assets/asm/usdasm/bank_01.asm` (US 1.0 disasm) plus room headers in the same bank.
- Goal: spell out how layouts and objects are drawn, how layers are selected/merged, and how object symbology should match the real draw semantics (arrows, “large”/4x4 growth, BothBG).
- Pain points to fix: corner ceilings and ceiling variants (4x4, vertical 2x2, horizontal 2x2), BG merge vs layer type treated as exclusive, layout objects occasionally drawing over background objects, and selection outlines that do not match the real footprint.

## Room Build & Layer Order (bank_01.asm)
- `LoadAndBuildRoom` (`assets/asm/usdasm/bank_01.asm:$01873A`):
  1) `LoadRoomHeader` ($01B564) pulls header bits: blockset/light bits to `$0414`, layer type bits to `$046C`, merge/effect bits to `$063C-$063F/$0640`, palette/spriteset/tag bytes immediately after.
  2) `RoomDraw_DrawFloors` ($0189DC): uses the first room word. High nibble → `$0490` (BG2 floor set), low nibble → `$046A` (BG1 floor set). Draws 4×4 quadrant “super squares” through `RoomDraw_FloorChunks`, targeting BG2 pointers first then BG1 pointers.
  3) Layout pointer: reads the next byte at `$B7+BA` into `$040E`, converts to a 3-byte pointer via `RoomLayoutPointers`, resets `BA=0`, restores `RoomData_TilemapPointers_upper_layer`, and runs `RoomDraw_DrawAllObjects` on that layout list (this is the upper/BG1 template layer; it should stay underneath later upper objects).
  4) Primary room objects: restores the room’s object pointer (`RoomData_ObjectDataPointers`) and runs `RoomDraw_DrawAllObjects` again against the upper/BG1 tilemap pointers (BA now points past the layout byte).
  5) BG2 overlay list: skips the `0xFFFF` sentinel (`INC BA` twice), reloads pointer tables with `RoomData_TilemapPointers_lower_layer`, and draws a third object list to BG2.
  6) BG1 overlay list: skips the next `0xFFFF`, reloads pointer tables with `RoomData_TilemapPointers_upper_layer`, and draws the final object list to BG1.
  7) Door/control records are handled after the final object list. Marker records update transition metadata without painting room tiles; physical doors can write to BG1, BG2, or both and can promote priority on tiles already present.
  8) Pushable blocks (`$7EF940`) and torches (`$7EFB40`) are drawn after the object and door passes. Both use stored bit 13 to select upper/BG1 or lower/BG2: their draw routines mask the encoded word with `AND #$3FFF`, retaining bit 13 in the offset from the installed upper/BG1 tilemap base. Pushable-block bit 14 controls behavior/pit checks; torch bit 14 is reserved and bit 15 selects the initially lit art.
- Implication: BG merge and layer type are **not** exclusive—four object streams are processed in order, with explicit pointer swaps for BG2 then BG1 overlays. Layout objects should never overdraw later passes; if they do in the editor, the pass order is wrong.

## Object Encoding (RoomDraw_RoomObject at $01893C)
- Type detection:
  - Type 2 sentinel: low byte `>= $FC` triggers `.subtype_2` ($018983).
  - Type 3 sentinel: object ID `>= $F8` triggers `.subtype_3` ($0189B8) after the ID is loaded.
  - Type 1: everything else (standard 3-byte objects).
- Type 1 format (`xxxxxxss | yyyyyyss | id`):
  - `x = (byte0 & 0xFC) >> 2`, `y = (byte1 & 0xFC) >> 2` (tile-space, 0–63).
  - `size_nibble = ((byte0 & 0x03) << 2) | (byte1 & 0x03)` (0–15).
  - ID = byte2.
- Size helpers (ground truth for outline math):
  - `RoomDraw_GetSize_1to16(_timesA)` at $01B0AC: `size = nibble + A` (`A=1` for most routines; diagonal ceilings pass `A=4` to force a 4-tile base span).
  - `RoomDraw_GetSize_1to15or26` at $01B0BE: nibble 0 → 26 tiles; otherwise nibble value.
  - `RoomDraw_GetSize_1to15or32` at $01B0CC: nibble 0 → 32 tiles; otherwise nibble value.
  - After calling any helper: `$B2` holds the final count; `$B4` is cleared.
- Type 2 format (`byte0 >= $FC`) uses tables at `.type2_data_offset` ($0183F0) and `.type2_routine` ($018470). No size field; fixed dimensions per routine.
- Type 3 format (`id >= $F8`) uses `.type3_data_offset` ($0184F0) and `.type3_routine` ($0185F0). Somaria path pieces and most other Type 3 objects have fixed draw footprints; some routines select different tiles or footprints from room state.

## Draw Routine Families & Expected Symbology
- Type 1 routine table: `.type1_routine` at `$018200`.
  - `Rightwards*` → arrow right; grows horizontally by `size` blocks. Base footprints: `2x4`, `2x2`, `4x4`, etc. Spacing suffix (`spaced2/4/8/12`) means step that many tiles between columns.
  - `Downwards*` → arrow down; grows vertically by `size` blocks with the same spacing conventions.
  - `DiagonalAcute/Grave` → 45° diagonals; use diagonal arrow/corner icon. Although routines 5/6 load `nibble+7`, they enter the shared loop at its pre-draw decrement; routines 17/18 load `nibble+6` and enter after that decrement. All four therefore draw `nibble+6` columns with a five-tile column stamp. `_BothBG` variants must draw to both BG1 and BG2.
  - `DiagonalCeiling*` (IDs 0xA0–0xAC): size = nibble + 4 (`GetSize_1to16_timesA` with `A=4`). Bounding box is square (`size × size`) because each step moves x+y by 1.
  - `4x4Floor*/Blocks*/SuperSquare` (IDs 0xC0–0xCA, 0xD1–0xE8): use “large square” icon. For variable super-square routines, size bits 3–2 select `1..4` horizontal 4×4 blocks and bits 1–0 select `1..4` vertical blocks. `0xC4` and `0xDB` copy the room header's active Floor 1/Floor 2 eight-tile pattern instead of a normal object payload.
  - `Edge/Corner` variants: use L-corner or edge glyph; many have `_BothBG` meaning they write to BG1 and BG2 simultaneously (should not be layer-exclusive).
- Type 2 routines (`.type2_routine`):
  - IDs 0x108–0x117: `RoomDraw_4x4Corner_BothBG` and “WeirdCorner*” draw to both layers—icon should denote dual-layer.
  - IDs 0x12D–0x133: inter-room fat stairs (A/B) and auto-stairs north (multi-layer vs merged) explicitly encode whether they target both layers or a merged layer; UI must not force exclusivity.
  - IDs 0x135–0x13F: water-hop stairs, spiral stairs, sanctuary wall, magic bat altar—fixed-size, no size nibble.
- Type 3 routines (`.type3_routine`):
  - Chests/big chests (0x218–0x232) are single 1×1 anchors; selection should stay 1 tile.
  - Somaria path pieces (ASM 0x203–0x20C, 0x20E, and 0x20F; yaze 0xF83–0xF8C, 0xF8E, and 0xF8F) each select one tile word and write it once at the object anchor. Size bits do not extend a piece.
  - Pipes (0x23A–0x23D) are fixed 2×? rectangles; use arrows that match their orientation.

## Ceiling and Large Object Ground Truth
- Corner/diagonal ceilings (Type 1 IDs 0xA0–0xAC): `RoomDraw_DiagonalCeiling*` ($018BE0–$018C36). Size = nibble+4; outline should be a square whose side equals that size; growth is along the diagonal (x+1,y+1 per step).
- Big hole & overlays: ID 0xA4 → `RoomDraw_BigHole4x4_1to16`. IDs 0xD8/0xDA enter stateful water routines: saved water state changes tilemap writes, destination, HDMA geometry, and potentially the active layer mode. Yaze currently renders an editor approximation on BG2; structural coverage is tested, but full runtime-state parity is open.
- 4x4 ceilings/floors: IDs 0xC5–0xCA, 0xD1–0xD2, 0xD9, 0xDF–0xE8 → `RoomDraw_4x4FloorIn4x4SuperSquare`. Use a “large square” glyph. The size nibble is split into two two-bit repeat counts, producing a `4..16` tile width and height.
- Floor copies: `0xC4` loads the Floor 1 selector from `$046A`; `0xDB` loads Floor 2 from `$0490`. Both stamp that decoded 4×2 pattern twice per 4×4 block. Room-aware renderers must receive the in-memory room header values so unsaved floor edits preview correctly.
- 2x2 ceilings:
  - Horizontal/right-growing: IDs 0x07–0x08 and 0xB8–0xB9 use `RoomDraw_Rightwards2x2_*` (size-driven width, height=2). Arrow right, outline width = `2 * size`, height = 2.
  - Vertical/down-growing: IDs 0x060 and 0x092–0x093 use `RoomDraw_Downwards2x2_*` (size-driven height, width=2). Arrow down, outline height = `2 * size`, width = 2. When the size nibble is 0 (`1to15or32`), treat size as 32 for bounds.

## Layer Merge Semantics
- Header bits at `LoadRoomHeader` ($01B5F4–$01B683):
  - Bits 5–7 of header byte 0 → `$0414` (blockset/light flags).
  - Bits 2–4 → `$046C` (layer type selector used later when building draw state).
  - Bits 0–1 of later header bytes → `$063C-$0640` (effect/merge/tag flags).
- `RoomDraw_DrawAllObjects` is run four times with different tilemap pointer tables; `_BothBG` routines ignore the active pointer swap and write to both buffers. The editor must allow “BG merge” and “layer type” to coexist; never force a mutually exclusive radio button.
- Ordering for correctness:
  1) Floors (BG2 then BG1)  
  2) Layout list (upper/BG1)
  3) Main list (upper/BG1 by default, unless the routine itself writes both)
  4) BG2 overlay list (after first `0xFFFF`)  
  5) BG1 overlay list (after second `0xFFFF`)  
  6) Doors and control records
  7) Pushable blocks and torches

## Selection & Outline Rules
- Use the decoding rules above; do not infer size from UI icons.
- Type 1 size nibble:
  - Standard (`1to16`): `size = nibble + 1`.
  - `1to15or26`: nibble 0 → size 26.
  - `1to15or32`: nibble 0 → size 32.
  - Diagonal ceilings: size = nibble + 4; outline is a square of that many tiles.
- Base footprints:
  - `2x4` routines: width=2, height=4; repeated along the growth axis.
  - `2x2` routines: width=2, height=2; repeated along the growth axis.
  - `4x4` routines: width=4, height=4; ignore size nibble unless the routine name includes `1to16`.
  - Variable super-square routines: decode the two two-bit repeat counts; each count step contributes one 4×4 block.
- Routine-name suffixes such as `plus3`, `plus7`, `plus12`, `plus13`, and `plus23` describe the final count/extent rule. They are not an origin offset. In particular, thin solid/corner objects `0x2F`, `0x30`, `0x34`, `0x6C`, `0x6D`, and `0x71` start at the encoded object position.
- Diagonal ceilings use their full measured square footprint for rectangular selection. A centered inset excludes real edge tiles and is not source-backed.
- `_BothBG` routines should carry a dual-layer badge in the palette and never be filtered out by the current layer toggle—selection must remain visible regardless of BG toggle because the object truly occupies both buffers.

## Mapping UI Symbology to Real Objects
- Arrows right/left: any `Rightwards*` routine; growth = size nibble (with fallback rules above). Use “large” badge only when the routine name includes `4x4` or `SuperSquare`.
- Arrows down/up: any `Downwards*` routine; same sizing rules.
- Diagonal arrow: `DiagonalAcute/Grave` and `DiagonalCeiling*`.
- Large square badge: `4x4Floor*`, `4x4Blocks*`, `BigHole4x4`, water overlays, chest platforms; these do **not** change size with the nibble.
- Dual-layer badge: routines with `_BothBG` in the disasm name, plus the
  multi/separate-layer auto-stair variants (yaze IDs 0x130–0x131 and
  0xF9B–0xF9C). The merged/swim variants (0x132–0x133, 0xF9D, and 0xFB3)
  stay on their stored target and must not be labeled BothBG.

## Action Items for the Editor
- Enforce the build order above so layout objects never sit above BG overlays; respect the two post-`0xFFFF` lists for BG2/BG1 overlays.
- Update selection bounds to honor the size helpers (including the nibble-zero fallbacks and the `+4` base for diagonal ceilings).
- Mark BothBG routines and multi/separate-layer auto stairs so layer toggles do
  not hide or exclude them.
- Align palette/symbology labels with the disasm names: arrows for
  `Rightwards/Downwards`, diagonal for `Diagonal*`, large-square for
  `4x4*/SuperSquare`, dual-layer for `_BothBG` and multi-layer auto stairs,
  and mixed-layer for 0xFA6–0xFA9. Merged/swim auto stairs remain
  stored-placement objects.

## Evidence Status (September 2026)

Passing one row does not imply the rows below it pass. In particular, synthetic replay is an internal regression guard, not independent visual truth.

| Evidence tier | Current status | What it proves |
| --- | --- | --- |
| Registry and synthetic routine replay | Pass for the current audited set | Object-to-routine mapping, write order, coordinates, and layer routing stay internally consistent. |
| USDASM-backed focused tests | Pass for audited doors, thin edges/corners, diagonal walls/ceilings, straight and spiral stairs, conditional caps, and Floor 1/Floor 2 copies | The asserted behavior follows named `bank_01.asm` routines rather than an older Yaze snapshot. |
| ROM parser + drawer tests | Pass for existing vanilla fixtures; Floor 1/Floor 2 copy coverage is included | Real room bytes, header fields, tile words, and renderer placement agree for those cases. |
| Room fingerprint goldens | Pass at their recorded revision | Full-room output did not drift from Yaze's own stored baseline. This is not independent emulator truth. |
| Independent Mesen RGBA ROIs | Partial | Exact ROIs cover Sanctuary wall/carpet, bombable floor states, TableRock `0xDD`, BigHole `0xA4`, rails `0x5F`/`0x8A`, and one west `NormalDoorLower`. Other door families, diagonal families, and stateful water paths remain open. |
| `z3ed dungeon-object-validate` | Pass at the last recorded canonical-ROM audit | Measured bounds and registered dimensions agree across the audited object/size/state cases; it does not prove pixels or runtime effects. |

Run the maintained ladder with
`YAZE_TEST_ROM_VANILLA=$PWD/roms/zelda3.sfc scripts/agents/audit-dungeon-visual-parity.sh --with-validate-report /tmp/yaze-dungeon-object-validation.json`.
Its synthetic tier includes thin edges/corners, diagonals, conditional caps,
floor copies, moving walls, water/stairs, and reveal-mask ordering; ROM and
Mesen tiers remain separate so synthetic agreement is never presented as
independent pixel proof.

### Known preview boundaries

- `0xD8`/`0xDA` water is structural/editor-preview coverage only until state-labeled Mesen captures verify each vanilla branch and layer-mode side effect.
- Moving-floor objects have static tile stamps, but Yaze does not emulate the SNES runtime BG2 scrolling effect.
- RGB averaging and indexed-palette fallback paths approximate SNES color math; only committed Mesen ROIs are pixel-parity claims.
- More key, shutter, bombable, and exploding door ROIs are required before claiming full door-family parity.

### Oracle project witnesses

Custom object `0x31` is subtype-overloaded. Only subtypes `0–12` and `14` are
minecart track pieces and may enable the `0x100–0x103` track-corner aliases.
Subtype `13` (`wall_sword_house`) and subtype `15` (`small_statue`) are
decorations and must leave those ordinary 4×4 wall corners on their built-in
routine. Mushroom Grotto uses subtype `15` in rooms `0x1A`, `0x1B`, `0x2A`,
`0x3B`, `0x4A`, `0x4B`, and `0x6A`.

The next Oracle-specific pixel fixtures are intentionally small:

| Family | Primary witness | What must be isolated |
| --- | --- | --- |
| Static water `0xC8`, edges `0x3F–0x46` | Mushroom Grotto `0x4A`, then water-dense `0x33` | Interior motif versus edge/cap overlap order |
| Icy floor `0xD1` | `0x08C` at `(10,11)` for BG1; `0x0CE` at `(29,23)` for BG2 | Shared routine-58 pixels and stored-layer routing |
| Bars `0x4C`, `0x8F`, `0xFD6–0xFD9` | `0x042` | A complete vertical-bar, corner, and horizontal-bar join |

These are investigation targets, not parity claims. Do not alter shared draw
routines from a visual report alone; first capture the affected layer and a
matching emulator or known-good editor ROI.

### Render issue capture UX

The room canvas context menu exposes `Capture Issue` for room rendering, room palette, or the current selection. Opening or closing the dialog does not create a log entry. `Copy Report` and `Copy Diagnostics` only copy; `Save to Issue Log` is the explicit persistence action. Raw diagnostics and local file paths are collapsed by default.

Historical implementation notes live in `docs/internal/plans/dungeon-object-rendering-parity-2026-04.md`; this file is the active behavior and evidence contract.
