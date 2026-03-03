# Tile16 Data Flow (Editor -> Zelda3 Core)

This document is the implementation map for Tile16 rendering/editing so future
changes keep parity with ZScream/Hyrule Magic behavior.

## Source Of Truth

- Tile16 composition metadata lives in `gfx::Tile16` (`tile0_..tile3_`).
- Each quadrant uses `gfx::TileInfo`:
  - `id_` (tile8 id)
  - `palette_` (0-7 row)
  - mirror flags
  - priority flag

## Render Pipeline

1. Metadata -> pixel indices
   - Editor delegates rendering to zelda3 core via
     `Tile16Editor::BuildTile16BitmapFromData`
     ([tile16_editor.cc:640](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:640)).
   - Core implementation:
     `RenderTile16PixelsFromMetadata`
     ([tile16_renderer.cc:28](/Users/scawful/src/hobby/yaze/src/zelda3/overworld/tile16_renderer.cc:28)).
   - Quadrant transform is canonical:
     `(pixel & 0x0F) + (palette_index * 0x10)`
     ([tile16_renderer.cc:68](/Users/scawful/src/hobby/yaze/src/zelda3/overworld/tile16_renderer.cc:68)).

2. Bitmap assembly
   - Core materializes 16x16 8bpp bitmap:
     `RenderTile16BitmapFromMetadata`
     ([tile16_renderer.cc:76](/Users/scawful/src/hobby/yaze/src/zelda3/overworld/tile16_renderer.cc:76)).

3. Editor preview palette binding
   - Tile16 preview palette application:
     `ApplyPaletteToCurrentTile16Bitmap`
     ([tile16_editor.cc:3126](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:3126)).
   - Default path applies full 256-color palette when pixels already encode
     row/high-nibble offsets.
   - Fallback path (normalization mode) keeps sub-palette behavior.

4. Local staging + atlas preview sync
   - Edits flow through `DrawToCurrentTile16`
     ([tile16_editor.cc:723](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:723)).
   - Staged tile bitmaps are copied to editor blockset + atlas via
     `CopyTileBitmapToBlockset`
     ([tile16_editor.cc:646](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:646)).

5. Commit to ROM/overworld
   - Batch commit entrypoint:
     `CommitAllChanges`
     ([tile16_editor.cc:2731](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:2731)).
   - Overworld write entrypoint:
     `CommitChangesToOverworld`
     ([tile16_editor.cc:2689](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:2689)).

## Tile8 Source Interaction Flow

- Tile8 source panel UI logic is isolated in:
  - `DrawTile8SourcePanel`
  - `HandleTile8SourceSelection`
  ([tile16_editor.cc:1777](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile16_editor.cc:1777)).
- Pure helper rules live in:
  `tile8_source_interaction.h`
  ([tile8_source_interaction.h:1](/Users/scawful/src/hobby/yaze/src/app/editor/overworld/tile8_source_interaction.h:1)).
- This keeps coordinate math and RMB usage-mode behavior testable without ImGui
  frame plumbing.

## Validation Matrix

- Renderer transform parity:
  - `Tile16RendererTest.RendersQuadrantsWithPaletteRowEncoding`
  - `Tile16EditorIntegrationTest.RegenerateEncodesPerQuadrantPaletteInPixels`
- Palette application behavior:
  - `Tile16EditorIntegrationTest.ApplyPaletteUsesFullPaletteWhenRowsEncoded`
  - `Tile16EditorIntegrationTest.NormalizedPixelsUseFallbackSubPalettePath`
- Palette metadata propagation:
  - `Tile16EditorIntegrationTest.ApplyPaletteToAllSetsAllQuadrantPalettes`
  - `Tile16EditorIntegrationTest.ApplyPaletteToQuadrantUpdatesOnlyTargetQuadrant`
- Staging/commit semantics:
  - `Tile16EditorSyntheticFixture.*`
  - `Tile16EditorIntegrationTest.DiscardCurrentTileChangesKeepsOtherPendingTiles`
  - `Tile16EditorIntegrationTest.CommitAllChangesClearsPendingQueue`
- Save/reload persistence:
  - `Tile16EditorSaveTest.*` (rom-dependent e2e)
