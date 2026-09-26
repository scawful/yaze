# Graphics sheet store: one source of truth for sheet pixels (proposal)

Status: proposal, 2026-09-26. Written from P1 (`claude/gfx-live-preview`,
Usage Preview panel). Not implemented; needs scawful's go-ahead because it
touches files G1/G3 are editing.

## Problem

Sheet pixels exist in three copies, and edits reach only one:

| Copy | Filled | Written by | Read by |
|------|--------|------------|---------|
| `GameData::gfx_bitmaps` (223 Bitmaps) | `LoadGameData` | load only | copied into Arena at ROM load (`editor_manager.cc`) |
| `GameData::graphics_buffer` (223 x 4096, "legacy contiguous") | `LoadGameData` (`game_data.cc:519`) | load only | `Room`, `OverworldMap`, `TitleScreen`, `ScreenEditor`, `SpriteUsagePreview` fallback |
| `Arena::gfx_sheets` (global singleton) | copy of `gfx_bitmaps` | pixel editor, palette tools, undo, PNG import | Graphics editor panels, `SpriteEditor::LoadSpriteGraphicsBuffer`, save |

Consequences:

1. The Dungeon and Overworld editors never show unsaved graphics edits.
   They render from `graphics_buffer`, which is not updated until the ROM is
   saved and reloaded.
2. The Arena is shared across sessions. After a session switch it can hold
   another ROM's sheets. `gfx_sheets_owner()` exists only to detect this.
3. Every new consumer has to choose a copy. P1 needed override hooks
   (`Room::SetGraphicsSheetOverrides`, `OverworldMap::SetGraphicsSheetOverrides`)
   to bridge Arena to the renderers.

## Proposal

Add a `zelda3::GraphicsSheetStore` owned by `GameData` (one per ROM session):

- Storage: one contiguous `223 * 4096` 8bpp buffer, the exact
  `graphics_buffer` layout, so `Room` and `OverworldMap` offsets do not change.
- Per-sheet `uint64_t revision`, bumped on every write.
- Write API: `WritePixel`, `WriteSheet(id, span)`, `Snapshot(id)`, and a
  scoped `Edit` for strokes, which bumps the revision once on stroke end.
- `graphics_buffer` becomes `store.data()`. Readers keep compiling.
- Arena sheets become display caches. A Bitmap re-uploads when its sheet's
  revision changes, and nothing writes Arena pixels directly. Undo stores
  store diffs.
- Consumers that cache pixels (a Room's `current_gfx16_`, OverworldMap's
  `current_gfx_`) record the revisions of the sheets in their blocks. Before
  drawing they compare those revisions and call `MarkGraphicsDirty()` when
  one changed. Dungeon and Overworld then show edits live, with no per-editor
  wiring.

The P1 override hooks stay. With the store in place they are used only for
what-if renders that must not touch the store. Examples:
- G1's PNG import dry-run preview ("how would this room look with the
  imported sheet").
- G3's clipboard paste preview.

## Migration (each step ships alone)

1. Add the store inside `GameData`. Fill it where `graphics_buffer` is filled,
   and alias `graphics_buffer` to it. No behavior change.
2. Route pixel-editor writes (`PixelEditorPanel`, `graphics_undo_actions.h`,
   palette tools, PNG import) through the store. Arena bitmaps refresh from
   revisions.
3. Add revision checks in `Room::PrepareForRender` and the overworld map
   rebuild path. Dungeon and Overworld then show live edits.
4. Make save read from the store, not Arena. Drop the `gfx_sheets_owner()`
   guard, since each session has its own store.
5. Remove the direct `Arena::mutable_gfx_sheets()` pixel writes. Arena keeps
   only textures.

Risk is concentrated in step 2 (undo and stroke batching) and step 4 (save).
G3's "stable saving" item is the natural owner of step 4.
