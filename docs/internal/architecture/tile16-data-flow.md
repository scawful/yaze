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
     ([tile16/tile16_edit_session.cc](../../../src/app/editor/overworld/tile16/tile16_edit_session.cc)).
   - Core implementation:
     `RenderTile16PixelsFromMetadata`
     ([tile16_renderer.cc](../../../src/zelda3/overworld/tile16_renderer.cc)).
   - Quadrant transform is canonical:
     `(pixel & 0x0F) + (palette_index * 0x10)`
     ([tile16_renderer.cc](../../../src/zelda3/overworld/tile16_renderer.cc)).

2. Bitmap assembly
   - Core materializes 16x16 8bpp bitmap:
     `RenderTile16BitmapFromMetadata`
     ([tile16_renderer.cc](../../../src/zelda3/overworld/tile16_renderer.cc)).

3. Editor preview palette binding
   - Tile16 preview palette application:
     `ApplyPaletteToCurrentTile16Bitmap`
     ([tile16/tile16_edit_session.cc](../../../src/app/editor/overworld/tile16/tile16_edit_session.cc)).
   - Default path applies full 256-color palette when pixels already encode
     row/high-nibble offsets.
   - Fallback path (normalization mode) keeps sub-palette behavior.

### Display Palette Contract

- The active overworld map palette is the display source of truth for the
  Tile8 source sheet, selected Tile8 hover preview, selected Tile16 preview,
  Tile16 selector/blockset atlas, and overworld map presentation.
- Tile16 palette buttons map directly to CGRAM rows `0-7`, matching ZScream
  and `OverworldMap::BuildTiles16Gfx`. Tile8 source graphics keep their low
  nibble, so the Tile8 source sheet and held Tile8 preview remap to the
  selected direct row instead of adding a graphics-sheet palette base.
- `current_gfx_bmp` is an indexed 8bpp display bitmap over the decoded
  overworld graphics buffer. Do not use `0x40` as its bitmap depth; `0x40` is
  the byte count of one 8x8 8bpp tile payload, not the surface bpp.
- Tile16 blockset refresh must update `current_gfx_bmp` from the current
  `OverworldMap::current_graphics()` before reloading Tile8s. The Tile16 atlas
  and selected-tile preview must be built from the same map graphics buffer, or
  switching maps can leave the editor preview using stale Tile8 source pixels.
- Document Tile16 metadata is authoritative. `edited_tile_bitmaps_` is only a
  derived preview cache and must be regenerated from the current Tile8 source
  when an edited tile is selected after a graphics refresh.
- `MapRefreshCoordinator` pushes the current map palette into `Tile16Editor`
  through `set_palette()` whenever map palette or Tile16 blockset state
  changes. `Tile16Editor::set_palette()` owns remapping `current_gfx_bmp` to the
  active brush row; refresh callers must not immediately replace that bitmap
  palette with the raw area palette, or the Tile8 source sheet can diverge from
  the held Tile8 preview and painted Tile16 pixels.
- Tile8 source sheets may shrink their display scale to fit available editor
  width, but palette application must not change with scale.

4. Immediate document edits and shared history
   - Domain operations run through `Tile16EditSession::RunEdit` and
     `ApplyDefinitions` in
     [tile16_edit_history.cc](../../../src/app/editor/overworld/tile16/tile16_edit_history.cc).
   - Validate/render the whole operation before applying definitions. History
     stores metadata, so Undo/Redo rebuilds previews with the active map palette.
   - Finish an active map stroke before adding a definition edit to shared
     overworld history. The workbench has no Commit/Discard editing step.

5. Save to ROM
   - Document edits are visible immediately. ROM publication remains part of
     the normal editor Save operation; editing a definition is not a disk save.
   - Legacy methods named `CommitAllChanges`/`CommitChangesToOverworld` remain
     internal compatibility/save plumbing, not user workflow instructions.

## Tile8 Source Interaction Flow

- Tile8 source panel UI logic is isolated in:
  - `DrawTile8SourcePanel`
  - `HandleTile8SourceSelection`
  ([tile16_workbench.cc](../../../src/app/editor/overworld/ui/tiles/tile16_workbench.cc)).
- Pure helper rules live in:
  `tile8_source_interaction.h`
  ([tile8_source_interaction.h](../../../src/app/editor/overworld/tile16/tile8_source_interaction.h)).
- This keeps coordinate math and RMB usage-mode behavior testable without ImGui
  frame plumbing.

## Validation Matrix

- Renderer transform parity:
  - `Tile16RendererTest.RendersQuadrantsWithPaletteRowEncoding`
  - `Tile16RendererTest.MatchesOverworldMapBuildTiles16GfxForPaletteRowsAndFlips`
  - `Tile16EditorIntegrationTest.RegenerateEncodesPerQuadrantPaletteInPixels`
- Map refresh/source graphics binding:
  - `OverworldEditorTest.RefreshTile16BlocksetSyncsTile8SourceGraphicsToCurrentMap`
- Pending preview cache invalidation:
  - `Tile16EditorSyntheticFixture.SetCurrentTileRegeneratesPendingBitmapFromCurrentTile8Source`
- Palette application behavior:
  - `Tile16EditorSyntheticFixture.RefreshAllPalettesRecolorsTile8SourceToBrushPalette`
  - `Tile16EditorSyntheticFixture.HeldTile8PreviewPaletteMatchesPaintedTile16Pixels`
  - `Tile16EditorIntegrationTest.ApplyPaletteUsesFullPaletteWhenRowsEncoded`
  - `Tile16EditorIntegrationTest.NormalizedPixelsUseFallbackSubPalettePath`
- Palette metadata propagation:
  - `Tile16EditorIntegrationTest.ApplyPaletteToAllSetsAllQuadrantPalettes`
  - `Tile16EditorIntegrationTest.ApplyPaletteToQuadrantUpdatesOnlyTargetQuadrant`
- Immediate document/history semantics:
  - `Tile16EditorSyntheticFixture.*`
  - `Tile16DocumentHistoryTest.*`
- Save/reload persistence:
  - `Tile16EditorSaveTest.*` (rom-dependent e2e)
