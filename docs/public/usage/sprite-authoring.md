# Live sprite frame and animation authoring

SpriteEditor shares the ZSpriteMaker frame model between ZSM documents, copies of
vanilla preview layouts, and supported literal ASM draw-table imports. Changes to
tile IDs, offsets, palette indices, size, and flips refresh the editor preview.
This is an editor preview, not live emulator memory editing.

## Open an editable asset

1. In **Custom Sprites**, use **New** or **Open** to create or load a `.zsm` file.
   Alternatively, use **Edit preview copy** in **Vanilla Sprites**. The latter
   copies Yaze's static layout, not the game's complete draw routine or animations.
   Every ID `$00–$FF` can start an asset. IDs without a decoded static layout offer
   **Create empty sprite template**, which starts with an empty frame.
2. For catalog source assets, select a variant and press **Import draw copy** next
   to its `draw` binding. This reads a copy from the configured source root and
   opens Custom Sprites. Mermaid and Librarian's literal tables are supported;
   Maple is supported by its explicit `oracle_maple_v1` adapter, which verifies
   the reviewed draw instructions before supplying X=0 and 16×16 tile sizes.
3. Load a ROM for graphics and choose the correct **Tilesheets**. Catalog imports
   do not infer graphics sheets or palette context. Each document retains its own
   eight sheet selections. Under **Asset bindings**, choose a source group and
   index for each OAM palette row. `auto` preserves the legacy preview heuristic;
   explicit rows select `global_sprites` or `sprites_aux1/2/3`. A vanilla preview
   copy starts with the currently selected sheets.
4. In **Animations**, select a frame and tile. Change offsets, tile ID, palette,
   size, flips, or BG priority. Add, duplicate, or delete frames and tiles. Frame
   deletion adjusts animation endpoints. Duplicate appends a frame; extend the
   animation range explicitly to include it. Editor Copy/Cut/Paste operate on the
   current frame; Paste appends to the selected document. Undo/Redo restores edits.
5. Use **Play**, previous/next frame, and the range/speed controls. Speed is ticks
   per frame at a nominal 60 Hz. Imported tables get a sequence named
   **Preview (author timing)**: neither timing nor runtime action boundaries were
   recovered from the ASM. Use **Save As** to preserve your asset as ZSM, then
   save the project to retain its bindings. Saved project assets reopen with their
   sheet and palette selections when SpriteEditor loads.

Tile offsets use the inherited ZSM origin `(128, 112)`. Stored X/Y bytes remain
canvas coordinates; Yaze displays their offsets. BG priority is saved and exported,
but its effect against game backgrounds is not simulated on the isolated canvas.
ZSM animations use byte endpoints, so authoring is limited to 256 frames.

## Use BIN files and raw expanded ROM graphics

1. Open **Graphics sources** in Custom Sprites and select a **Graphics slot**
   (`0–7`). Each slot holds 64 tiles, arranged as a `128 × 32` sheet.
2. Select **BIN file** or **Raw ROM range**, choose `2`, `3`, or `4` bits per pixel,
   and enter the hexadecimal **Byte offset**. A slot requires `$400`, `$600`, or
   `$800` complete SNES planar bytes respectively. ROM offsets are headerless PC
   byte offsets; no expansion table, compressed stream, or free-space allocation
   is inferred. BIN paths can be absolute or relative to the saved project.
3. Press **Apply graphics source**. Invalid or truncated ranges leave the existing
   binding unchanged. Undo/Redo restores the prior binding. The sprite canvas and
   tile selector use the selected source, including all 16 colors of 4bpp art.
4. To edit BIN pixels, use **Edit bound BIN in Graphics Scratch**. This opens a
   detached copy of that exact slot and the selected OAM palette. Export to the
   original BIN path to replace that slot while preserving the other bytes. If
   the source slot changed externally, export stops for review. A different
   export path creates a standalone 64-tile BIN; update the sprite binding to
   that path and offset zero. Press **Reload graphics sources** after changing
   a BIN or raw ROM bytes. File contents are not polled every frame.
5. Save the ZSM and project. The project retains these sources in version-3 asset
   bindings. Version-1 and version-2 assets continue to load. Source files and raw
   ROM ranges are read-only here; binding them does not install a runtime loader.

An asset using any BIN/raw ROM slot cannot use **Edit selected OAM pixels** to
edit fallback ROM sheets. Use Graphics Scratch for the external art. Missing BINs
or invalid ROM ranges produce a local preview error and preserve the asset.

## Copy draw-table data for review

**Copy draw tables (ASM)** copies a candidate containing `.start_index`,
`.nbr_of_tiles`, `.x_offsets`, `.y_offsets`, `.chr`, `.properties`, and `.sizes`.
It emits no registrations, hooks, behavior, animation calls, or ID allocations.
The source file and ROM are not modified. Review the candidate with the family's
existing driver, bank layout, OAM allocation, and animation/action code before
integrating it through the game's normal build.

The inherited driver doubles an 8-bit tile index for word offsets. Export is
therefore limited to 128 total tile entries across nonempty frames. It rejects
invalid tile/palette/priority values and nonzero Z rather than losing unsupported
data. Import accepts literal decimal, hexadecimal (`$`), and binary (`%`) values;
expressions, missing tables, duplicate tables, incompatible lengths, unsupported
sizes, and offsets that ZSM cannot represent are rejected. Maple adds its two
verified constants before using the same validator. Export from a Maple-bound
asset rejects nonzero X offsets or 8×8 tiles, because its existing driver would
ignore those edits.

## Saved bindings and source changes

A saved asset is identified by its ZSM path in the project's `[sprite_assets]`
section. Bindings contain the optional catalog variant key, adapter, eight sheets,
eight palette rows, source path/label, and a SHA-256 hash of the exact draw-source
file bytes. The ZSM format itself is unchanged. Paths to assets are serialized
relative to the project; source paths remain relative to `sprite_source_root`.
Copy both the project and its ZSM files when moving the workspace.

**Check source hash** reports whether the source matches the import snapshot.
Opening a saved asset also checks the source. A changed or unavailable source does
not replace your frames or prevent ZSM editing/saving. **Copy draw tables (ASM)**
rechecks the source and stops if it changed or cannot be read. Saving an edited ZSM
does not acknowledge source changes or replace the recorded hash. Import again
into a separate document to review new source; there is no automatic merge.

A Maple driver instruction change requires adapter review even when the source
binding is fresh. Whitespace and comments do not change its driver fingerprint;
its whole-file provenance hash still detects those changes.

Missing or malformed saved ZSM files produce a panel error while other assets can
open. ZSM reads are bounded to 8 MiB, 256 frames/animation groups, 128 tiles per
frame, 1 MiB per string, and 1,024 routines. Invalid explicit graphics/palette
bindings report errors instead of silently substituting another palette or stale
sheet data.

## Current limits and next work

Bindings establish asset identity and preview inputs, not an ASM write-back map.
The hash covers the file containing the draw routine; it does not attest the
complete build, behavior dependencies, ROM, or graphics bytes. ZSM saving and
project saving remain separate operations. Standalone ZSM files do not carry these
Yaze-specific bindings.

Pending sprite documents participate in the session close check. Normal Save
saves pending documents, and project Save persists the updated asset bindings.
Saving one ZSM does not clear another document's pending changes. A failed project
save retains pending binding metadata for another attempt.

The [Behavior tab](sprite-behavior.md) now provides an Oracle action model for
animation, solicited dialogue, player blocking, XY movement/tile bounce, and timer
transitions. It emits reviewable ASM candidates; it does not import or install
existing scripts. Hand-authored routines remain available in **Routines**.
Contextual variant selection, placement, and runtime integration remain separate
work with explicit loader and family-ownership contracts.

See [catalog setup](sprite-catalog.md) and the
[agent-facing standard](../../internal/architecture/sprite-catalog.md).
