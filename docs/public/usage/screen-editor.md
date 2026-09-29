# Screen Editor: Menu Tilemap (2bpp)

The Screen editor's **Menu Tilemap (2bpp)** panel edits the raw 2bpp SNES
BG3 tilemap files that ROM hacks like Oracle of Secrets use for a custom
in-game menu, with a live preview rendered from real CHR graphics and a
real palette. Its first-class use case is Oracle of Secrets' "Masks &
Rings" page 3 prototype (`ring_box.tilemap`, `Menu/menu_page3.asm`), but it
works on any file in the same format.

---

## Opening the panel

- Keyboard: **Alt+6** while the Screen editor is active, or the panel's
  auto-generated visibility toggle in the command palette / Windows menu
  under its workflow label, **"Edit Menu Tilemap (2bpp)."**
- The panel is registered as an **embedded tool** hosted by the Screen
  editor (not a default global workspace window): it needs a loaded ROM,
  and it does not open automatically.

## File format

Files are little-endian arrays of 16-bit SNES BG tilemap words
(`vhopppcc cccccccc`: 10-bit tile ID, 3-bit palette, priority, H/V flip).
The map is always **32 words wide**; row count = file size / 64 bytes.
Oracle of Secrets' files are either full 2048-byte (32x32) maps, or
partial screens that only cover the rows they use:

| File | Size | Rows |
|---|---|---|
| `Menu/tilemaps/ring_box.tilemap`, `menu_frame.tilemap`, `magic_bag.tilemap`, `song_menu.tilemap`, `dung_map.bin`, `Menu/rings/ring.tilemap` | 2048 B | 32 |
| `Menu/tilemaps/hud.tilemap` | 384 B | 6 |
| `Menu/tilemaps/quest_icons.tilemap` | 128 B | 2 |

Saving never changes a file's byte size (Oracle's ASM `incbin`s these at a
fixed VRAM upload size); an odd-sized file is rejected on open with a
descriptive error rather than silently truncated or padded.

### Opening a project file

With an Oracle-of-Secrets-style project loaded, the panel lists
`Menu/tilemaps/*.tilemap`, `Menu/tilemaps/*.bin`, and
`Menu/rings/*.tilemap` under the project's directory, with a `*` dirty
marker on the currently-open file. If the project also has
`Menu/menu_page3.asm` (the "Masks & Rings" page 3 prototype), the list is
headed **"Masks & Rings (page 3)"** and `ring_box.tilemap` -- the frame
that page draws via `Menu_DrawRingBox` -- is sorted first.

You can also **Open...** any `.tilemap`/`.bin` file directly.

## CHR and palette sources

The live preview needs two things: which 2bpp graphics tiles to draw, and
which colors to draw them in.

**CHR (tile graphics):**
- **ROM** (default) -- the 2bpp sheets the game puts in BG3 while the
  item/menu screen is open (sheets `0x71`, `0x72`, `0xDA`-`0xDE`, the same
  ones `zelda3::Load2BppGraphics` decompresses for the Inventory screen).
  These are ZScream/yaze-managed compressed graphics, so they're present
  in a project's normal ROM.
- **File** -- a raw 2bpp `.bin` chr file or VRAM dump.

**Palette** (radio buttons in the panel; **Reload** re-reads every source,
e.g. after editing `menu_palette.asm` on disk):
- **Auto** (default) -- the **Symbol** source when it resolves to real
  data, otherwise the **ASM** source when the project has a
  `Menu/menu_palette.asm`. This is the case that matters for Oracle: the
  project ROM `oos168.sfc` is the base ROM, where the assembled table
  reads all zero, so Auto falls back to the ASM source instead of
  showing an error. The status line says which one is active, e.g.
  `Palette: ASM menu_palette.asm (auto: symbol reads all zero ...)`.
- **Symbol** -- 32 SNES colors read from a project ROM symbol's
  address (default label `Oracle_Menu_Palette`, editable in the panel),
  applying Oracle's menu-upload **+1 CGRAM offset**: `Menu_UploadLeft`
  copies its 32-word table to CGRAM starting at color 1, not color 0, so
  sub-palette *p* color *c* (*c*=1..3) is `table[p*4 + c - 1]`; color 0 of
  every sub-palette is always transparent.
  The symbols file comes from the project's `symbols_filename` when set
  (e.g. `Roms/oos168x.sym`), or can be picked manually.
- **ASM** -- parses `Menu/menu_palette.asm` directly (looked up in
  `<project>/Menu/`, then `<project>/<code_folder>/Menu/`; override with
  the **...** button). It reads the table after the `Menu_Palette:` label
  (the `Oracle_` prefix of the symbol label is ignored) and uses the same
  +1 CGRAM mapping. Accepted lines: `dw hexto555($RRGGBB)` (the file's
  macro: each channel / 8, packed `B<<10 | G<<5 | R`; `$814f16` is
  `0x0930`), plain `dw $XXXX` words, several comma-separated operands per
  `dw`, `;` comments, blank lines, CRLF or LF. The table ends at the first
  non-`dw` line or after 32 entries; at least 31 are required (the real
  file defines 31). A malformed line is reported with its line number
  (`menu_palette.asm: line 12: ...`).
- **HUD** -- `palette_groups.hud[0]`, the 32-color HUD palette group
  already loaded from the base ROM (vanilla colors, not Oracle's runtime
  HUD palette patches).
- **File** -- a raw 512-byte CGRAM dump or a `.pal` file (32 colors
  starting at byte offset 2, i.e. CGRAM color 1, matching the same +1
  alignment).

The active source and a short status line are shown above the canvas.

### Important: Oracle of Secrets' two-ROM pipeline

Oracle of Secrets builds a **base ROM** (`Roms/oos168.sfc`, edited by
yaze/ZScream: room layouts, overworld, graphics sheets) and a separate
**patched ROM** (`Roms/oos168x.sfc`, the base ROM with all ASM hooks
applied by asar). `Menu_Palette`, `Menu_DrawRingBox`, and the tilemap
`incbin`s are pure ASM -- they only exist in the *patched* ROM. If you
point the Symbol palette source at a project's base ROM, the resolved
bytes will read as all zero; the panel detects this and shows an error
naming the label and suggesting the patched ROM or the ASM/HUD/File source
instead, rather than silently rendering a black palette (Auto mode then
uses `Menu/menu_palette.asm` if the project has it). The 2bpp CHR
sheets are unaffected by this -- they're ZScream-managed and present in
either ROM.

## Canvas and tools

- **Zoom** slider (1x-4x) and a **grid** toggle.
- **Hover readout**: x, y, tile hex, palette, H/V flip, priority, and the
  raw 16-bit word, updated as you move the mouse.
- **Paint** tool: click (or click-drag) to stamp the selected tile from the
  **Tile Picker**, with its palette/H-flip/V-flip/priority.
- **Tile Picker**: renders the active CHR source in the selected palette,
  with a **Fit/1x/2x/4x** adaptive scale control (the same convention as
  the Dungeon editor's Room Graphics panel and the Overworld tile16
  selector) since a full 7-sheet CHR source can be several hundred pixels
  tall.
- **Eyedropper**: right-click, or Alt+click, on any cell to load its tile
  ID/palette/flags into the current tool state.
- **Select** tool: click-drag a rectangle, then **Copy**, **Paste**,
  **Fill** (with the current tool tile), or **Erase** (writes a
  configurable word, default `0x0000`).

## Preview overlays

Under **Preview Overlays**:

- **Reference image**: load a PNG (e.g. a Mesen screenshot of the real
  menu) as a semi-transparent overlay, with an opacity slider and x/y
  nudge, to compare the frame you're editing against the finished page.
- **WRAM composite dump**: load a raw 2048-byte dump of `$7E1000-$17FF`
  (the fully composed BG3 page) and render it with the same CHR/palette
  sources, as a read-only comparison layer.
- **Dynamic content (page 3 mask icons)**: a display-only overlay of the
  five mask icons (Deku/Zora/Wolf/Bunny/Stone) `Menu_Page3_Draw` writes
  directly into the `$1000` buffer at their `menu_offset()` cells,
  transcribed from `Menu/menu_gfx_table.asm`. **Not** included: the title
  text ("MASKS  RINGS", needs the menu's font/text-encoding table) or the
  six ring slots (ownership-dependent, drawn by
  `Menu_DrawMagicRingsInBox`) -- both need more than a fixed-table lookup
  to render correctly, so they're left for a future pass rather than
  guessed at.

## Save, undo, and external changes

- **Save** / **Save As...** write via a temp-file-plus-rename so a reader
  never sees a partially-written file. The first save each session also
  writes a `<file>.bak` with the pristine pre-edit bytes (never
  overwritten again that session).
- **Revert** reloads from disk, discarding in-memory edits.
- Every paint stroke, fill, erase, and paste is one undo/redo step through
  the Screen editor's shared undo system (`Ctrl/Cmd+Z` / `Ctrl/Cmd+Shift+Z`,
  same as the rest of the Screen editor).
- **Ctrl/Cmd+S** saves while the panel is focused, without stealing the
  application's global Save shortcut (it only fires while this panel's
  window has focus).
- If the open file changes on disk (e.g. you re-ran an export script), the
  panel shows a reload prompt instead of silently overwriting your edits
  on the next save.

## Headless rendering (`z3ed gfx-tilemap-render`)

For scripted or agent workflows (pixel comparisons, CI checks), the same
render pipeline is available from the command line:

```
z3ed gfx-tilemap-render --tilemap <file> --out <png> \
    [--rom <rom>] [--symbols <sym file>] \
    [--palette-source symbol|asm|hud|file] [--palette-label <label>] \
    [--palette-asm <Menu/menu_palette.asm>] \
    [--palette-file <path>] [--palette-file-offset <n>] \
    [--chr-source rom|file] [--chr-file <path>]
```

`--rom` is only required for whichever source needs it (`--chr-source rom`
and/or `--palette-source symbol|hud`); a fully file-backed invocation
(`--chr-source file --palette-source file`) needs no ROM at all. The
output is an 8-bit indexed PNG (color 0 of each sub-palette is
transparent, via a `tRNS` chunk), matching the panel's own render.

### Comparing a render with a Mesen screenshot

A live screenshot of the menu is not the raw tilemap at row 0: BG3 is
vertically scrolled. Oracle's `Menu_ScrollDown` (Menu/menu.asm) ends when
BG3 V-scroll (`$EA`, the BG3VOFS shadow) reaches `$FF12`, so at rest the
screen shows the 256-line map starting at `$12` = 18 lines down (the map
wraps at 256), and the SNES PPU draws BG line `VOFS + 1` on its first
visible scanline: screenshot row *Y* is tilemap row *Y + 19*. Cropping a
`gfx-tilemap-render` PNG at rows 19..242 lines up with Mesen's 256x224
captures of the Items and Masks & Rings pages (a shift search against both
captures independently peaks at exactly +19, and the static parts of the
Items frame then match about 98%). The panel itself shows the file as-is,
starting at row 0.

## Known limits

- The dynamic-content overlay covers the five mask icons only (see above).
- Symbol-file parsing uses the project's `.sym` (WLA-DX bank:address
  format); it does not currently normalize FastROM-mirror ($80xxxx)
  addresses from other toolchains' symbol exports.
