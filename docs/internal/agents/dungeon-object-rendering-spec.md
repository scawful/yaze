# Dungeon Object Rendering & Selection Spec (ALTTP)

Status: ACTIVE  
Owner: zelda3-hacking-expert  
Created: 2025-12-06  
Last Reviewed: 2025-12-06  
Next Review: 2025-12-20  
Board: docs/internal/agents/coordination-board.md (2025-12-06 zelda3-hacking-expert – Dungeon object render/selection spec)

## Scope
- Source of truth: `assets/asm/usdasm/bank_01.asm` (US 1.0 disasm) plus room headers in the same bank.
- Goal: spell out how layouts and objects are drawn, how layers are selected/merged, and how object symbology should match the real draw semantics (arrows, “large”/4x4 growth, BothBG).
- Pain points to fix: corner ceilings and ceiling variants (4x4, vertical 2x2, horizontal 2x2), BG merge vs layer type treated as exclusive, layout objects occasionally drawing over background objects, and selection outlines that do not match the real footprint.

## Room Build & Layer Order (bank_01.asm)
- `LoadAndBuildRoom` (`assets/asm/usdasm/bank_01.asm:$01873A`):
  1) `LoadRoomHeader` ($01B564) pulls header bits: blockset/light bits to `$0414`, layer type bits to `$046C`, merge/effect bits to `$063C-$063F/$0640`, palette/spriteset/tag bytes immediately after.
  2) `RoomDraw_DrawFloors` ($0189DC): uses the first room word. High nibble → `$0490` (BG2 floor set), low nibble → `$046A` (BG1 floor set). Draws 4×4 quadrant “super squares” through `RoomDraw_FloorChunks`, targeting BG2 pointers first then BG1 pointers.
  3) Layout pointer: reads the next byte at `$B7+BA` into `$040E`, converts to a 3-byte pointer via `RoomLayoutPointers`, resets `BA=0`, and runs `RoomDraw_DrawAllObjects` on that layout list (this is the template layer; it should stay underneath everything else).
  4) Primary room objects: restores the room’s object pointer (`RoomData_ObjectDataPointers`) and runs `RoomDraw_DrawAllObjects` again (BA now points past the layout byte).
  5) BG2 overlay list: skips the `0xFFFF` sentinel (`INC BA` twice), reloads pointer tables with `RoomData_TilemapPointers_lower_layer`, and draws a third object list to BG2.
  6) BG1 overlay list: skips the next `0xFFFF`, reloads pointer tables with `RoomData_TilemapPointers_upper_layer`, and draws the final object list to BG1.
  7) Pushable blocks (`$7EF940`) and torches (`$7EFB40`) are drawn after the four passes.
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
- Type 3 format (`id >= $F8`) uses `.type3_data_offset` ($0184F0) and `.type3_routine` ($0185F0). Some use size nibble (e.g., Somaria lines); most are fixed-size objects like chests and stair blocks.

## Draw Routine Families & Expected Symbology
- Type 1 routine table: `.type1_routine` at `$018200`.
  - `Rightwards*` → arrow right; grows horizontally by `size` blocks. Base footprints: `2x4`, `2x2`, `4x4`, etc. Spacing suffix (`spaced2/4/8/12`) means step that many tiles between columns.
  - `Downwards*` → arrow down; grows vertically by `size` blocks with the same spacing conventions.
  - `DiagonalAcute/Grave` → 45° diagonals; use diagonal arrow/corner icon. Size = nibble+1 tiles long (helper is `1to16`). `_BothBG` variants must draw to both BG1 and BG2.
  - `DiagonalCeiling*` (IDs 0xA0–0xAC): size = nibble + 4 (`GetSize_1to16_timesA` with `A=4`). Bounding box is square (`size × size`) because each step moves x+y by 1.
  - `4x4Floor*/Blocks*/SuperSquare` (IDs 0xC0–0xCA, 0xD1–0xE8): use “large square” icon. They tile 4×4 blocks inside 16×16 “super squares” (128×128 pixels) and do **not** use the size nibble—dimensions are fixed per routine.
  - `Edge/Corner` variants: use L-corner or edge glyph; many have `_BothBG` meaning they write to BG1 and BG2 simultaneously (should not be layer-exclusive).
- Type 2 routines (`.type2_routine`):
  - IDs 0x108–0x117: `RoomDraw_4x4Corner_BothBG` and “WeirdCorner*” draw to both layers—icon should denote dual-layer.
  - IDs 0x12D–0x133: inter-room fat stairs (A/B) and auto-stairs north (multi-layer vs merged) explicitly encode whether they target both layers or a merged layer; UI must not force exclusivity.
  - IDs 0x135–0x13F: water-hop stairs, spiral stairs, sanctuary wall, magic bat altar—fixed-size, no size nibble.
- Type 3 routines (`.type3_routine`):
  - Chests/big chests (0x218–0x232) are single 1×1 anchors; selection should stay 1 tile.
  - Somaria lines (0x203–0x20C/0x20E) use the size nibble as a tile count; they extend along X with no Y growth.
  - Pipes (0x23A–0x23D) are fixed 2×? rectangles; use arrows that match their orientation.

## Ceiling and Large Object Ground Truth
- Corner/diagonal ceilings (Type 1 IDs 0xA0–0xAC): `RoomDraw_DiagonalCeiling*` ($018BE0–$018C36). Size = nibble+4; outline should be a square whose side equals that size; growth is along the diagonal (x+1,y+1 per step).
- Big hole & overlays: ID 0xA4 → `RoomDraw_BigHole4x4_1to16` (fixed 4×4). IDs 0xD8/0xDA → `RoomDraw_WaterOverlayA/B8x8_1to16` (fixed 8×8 overlay; should remain on BG2 overlay pass).
- 4x4 ceilings/floors: IDs 0xC5–0xCA, 0xD1–0xD2, 0xD9, 0xDF–0xE8 → `RoomDraw_4x4FloorIn4x4SuperSquare` (fixed 4×4 tiles repeated in super squares). Use “large square” glyph; ignore size nibble.
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
  2) Layout list (BG2)  
  3) Main list (BG2 by default, unless the routine itself writes both)  
  4) BG2 overlay list (after first `0xFFFF`)  
  5) BG1 overlay list (after second `0xFFFF`)  
  6) Pushable blocks and torches

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
  - Super-square routines: treat as 16×16 tiles when computing selection bounds (they stamp 4×4 blocks into a 32×32-tile area).
- `_BothBG` routines should carry a dual-layer badge in the palette and never be filtered out by the current layer toggle—selection must remain visible regardless of BG toggle because the object truly occupies both buffers.

## Mapping UI Symbology to Real Objects
- Arrows right/left: any `Rightwards*` routine; growth = size nibble (with fallback rules above). Use “large” badge only when the routine name includes `4x4` or `SuperSquare`.
- Arrows down/up: any `Downwards*` routine; same sizing rules.
- Diagonal arrow: `DiagonalAcute/Grave` and `DiagonalCeiling*`.
- Large square badge: `4x4Floor*`, `4x4Blocks*`, `BigHole4x4`, water overlays, chest platforms; these do **not** change size with the nibble.
- Dual-layer badge: routines with `_BothBG` in the disasm name, plus `AutoStairs*MergedLayer*` (IDs 0x132–0x133, 0x233). These must be allowed even when a “merged BG” flag is set in the header.

## Action Items for the Editor
- Enforce the build order above so layout objects never sit above BG overlays; respect the two post-`0xFFFF` lists for BG2/BG1 overlays.
- Update selection bounds to honor the size helpers (including the nibble-zero fallbacks and the `+4` base for diagonal ceilings).
- Mark BothBG/merged-layer routines so layer toggles do not hide or exclude them.
- Align palette/symbology labels with the disasm names: arrows for `Rightwards/Downwards`, diagonal for `Diagonal*`, large-square for `4x4*/SuperSquare`, dual-layer for `_BothBG`/merged stairs.

