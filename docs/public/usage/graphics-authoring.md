# Graphics authoring and scratch assets

Use the Graphics editor alongside Tile16, dungeon object, and sprite authoring.
Pixel edits use the current ROM session's graphics store. Usage previews update
from those unsaved pixels; a ROM save remains a separate operation.

## Edit pixels with their source visible

1. Select a Tile16 quadrant, dungeon object tile, or sprite OAM tile in its editor.
2. Choose **Edit Graphics** to open the Pixel Editor with the source palette.
3. Keep the source preview beside the canvas, or open **Usage Preview**.
4. Edit pixels and use Undo/Redo normally. Return to the source editor to continue
   editing its tile arrangement or object definition.

The Tile16 usage view follows unsaved definitions in the live Overworld editor,
including Undo/Redo. Its area selector distinguishes built live areas from ROM
areas. **Load other ROM areas** opts into scanning additional areas.
Dungeon source previews follow unsaved room object changes. A preview remains
bound to the ROM session that opened it.

## Keep unassigned graphics

Open **Scratch assets** in the Pixel Editor, or **Graphics Scratch** in the window
selector. This library contains up to 128 independent 128×32 pixel sheets.

1. Enter an asset name. Choose **Keep current sheet**, **New blank**, or import a
   BIN file at an explicit byte offset and 2, 3, or 4 bpp.
2. Select a saved asset. Left drag paints with the selected color; right click
   picks a color. Changes join the Graphics editor's Undo/Redo history.
3. Use **Export BIN** to save 64 tiles as an uncompressed SNES planar file.
   Exporting to the original BIN updates its imported byte range and preserves
   surrounding tiles. Changed source bytes cause a conflict instead of an overwrite.
4. When a destination is ready, select a ROM sheet and choose **Apply pixels to
   current sheet**. The replacement is one Undo action and uses normal ROM save.

BIN imports read exactly 64 complete tiles: 1024 bytes at 2 bpp, 1536 bytes at
3 bpp, or 2048 bytes at 4 bpp. Larger files are allowed; the byte offset selects
the imported range. A scratch sheet preserves its palette and structured BIN
provenance. Exporting to another existing file requires an exact chunk-size
match; open ROMs and the scratch library cannot be export destinations.
Applying pixels preserves the destination's palette and graphics-group mapping.

Scratch changes save to `GraphicsScratch.json` in Yaze's configuration directory.
The window shows its exact path and save errors. Saving uses atomic replacement
and a publication lock. A malformed file or a conflicting editor's changes are
preserved. Failed saves keep the pending assets in memory and block silent close.

The ordinary ROM writer currently supports writable 3 bpp sheets with indices
0–7. It refuses reserved sheets and read-only 2 bpp sheets. Other graphics can
remain in the scratch library or BIN files until their runtime loading is defined.

## Assemble overworld layouts before placement

Open **Scratch Workspace** in the Overworld editor. Named layouts contain Tile16
references, so their appearance follows current graphics and palettes.

1. Choose **New layout**, **Duplicate**, or **Capture selection**.
2. Rename the layout, adjust its dimensions, or paint and stamp tiles into it.
3. Choose **Copy layout as brush**, hover the destination map, and use Paste.

The library holds up to 32 layouts, each at most 32×32 Tile16 cells. Resizing
preserves hidden cells. Remove and Clear are reversible through the editor's
Undo/Redo history. Layout changes save to the existing `ScratchPad.dat` file;
the original single-layout format migrates on the first successful save.
Layouts do not allocate ROM sheets or rewrite world maps until explicitly pasted.

## Bind custom sprite graphics

The Sprite editor can create a detached authoring asset for every vanilla ID.
Known static layouts start as a pose copy. Other IDs start as a labeled blank
template; they do not claim to decode the game's animation or behavior.

Custom assets can bind each graphics slot to an ordinary sheet, a BIN range, or
an explicit raw-ROM PC offset at 2, 3, or 4 bpp. **Edit bound BIN in Graphics
Scratch** opens a detached copy with the selected OAM palette. Export the edited
BIN, update its binding if necessary, and choose **Reload graphics sources**.
Raw-ROM sources remain read-only preview ranges. See
[Sprite authoring](sprite-authoring.md) for frame, OAM, binding, and save details.

These bindings describe editor assets. They do not install sprite code, assign
new runtime IDs, allocate expanded ROM space, or infer a custom graphics loader.
