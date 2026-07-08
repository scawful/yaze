# Overworld Editor Architecture

**Status**: Active
**Last Updated**: 2026-04-29
**Related Code**: `src/app/editor/overworld/`, `src/zelda3/overworld/`

This document outlines the architecture of the Overworld Editor in YAZE.

## High-Level Overview

The Overworld Editor allows users to view and modify the game's overworld maps, including terrain (tiles), entities (sprites, entrances, exits, items), and map properties.

### Key Components

| Component | Location | Responsibility |
|-----------|----------|----------------|
| **OverworldEditor** | `src/app/editor/overworld/` | Main UI coordinator. Manages the `Overworld` data object, handles user input (mouse/keyboard), manages sub-editors (Tile16, GfxGroup), and renders the main view. |
| **Overworld** | `src/zelda3/overworld/` | System coordinator. Manages the collection of `OverworldMap` objects, global tilesets, palettes, and loading/saving of the entire overworld structure. |
| **OverworldMap** | `src/zelda3/overworld/` | Data model for a single overworld screen (Area). Manages its own graphics, properties, and ZSCustomOverworld data. |
| **OverworldEntityRenderer** | `src/app/editor/overworld/` | Helper class to render entities (sprites, entrances, etc.) onto the canvas. |
| **MapPropertiesSystem** | `src/app/editor/overworld/` | UI component for editing map-specific properties (music, palette, etc.). |
| **OverworldToolbar** | `src/app/editor/overworld/` | Top-row world/map metadata workflow, including common ZScream-style metadata edits. |
| **Tile16Editor** | `src/app/editor/overworld/` | Sub-editor for modifying the 16x16 tile definitions. |

## Interaction Flow

1.  **Initialization**:
    *   `OverworldEditor` is initialized with a `Rom` pointer.
    *   It calls `overworld_.Load(rom)` which triggers the loading of all maps and global data.

2.  **Rendering**:
    *   `OverworldEditor::DrawOverworldCanvas` is the main rendering loop.
    *   It iterates through visible `OverworldMap` objects.
    *   Each `OverworldMap` maintains a `Bitmap` of its visual state.
    *   `OverworldEditor` draws these bitmaps onto a `gui::Canvas`.
    *   `OverworldEntityRenderer` draws entities on top of the map.

3.  **Editing**:
    *   **Tile Painting**: User selects a tile from the `Tile16Selector` (or scratch pad) and clicks on the map. `OverworldEditor` updates the `Overworld` data model (`SetTile`).
    *   **Entity Manipulation**: User can drag/drop entities. `OverworldEditor` updates the corresponding data structures in `Overworld`.
    *   **Properties**: Toolbar, map properties, sidebar, and context-menu changes route through `MapPropertiesSystem` property edit APIs. `OverworldEditor` wraps these edits to finalize pending paint operations and push undo actions.

## Metadata Editing Path

Overworld metadata edits use a shared payload:

- `OverworldPropertyEdit` identifies the target map, field, state/slot index,
  and value.
- `OverworldMapMetadataClipboard` stores a copyable map-metadata payload for
  context-menu workflows, including scoped copy/paste for all metadata,
  graphics-only metadata, palette-only metadata, and music/message metadata.
- `DescribeOverworldMapMetadataClipboard()` gives both the toolbar and canvas
  context menu the same user-facing description of the copied scope and source
  map.

`MapPropertiesSystem` owns direct property mutation and validation:

- `ApplyPropertyEdit()` dispatches to the editor-level callback when undo
  tracking is available.
- `ApplyPropertyEdits()` applies a batch of property edits, used by metadata
  paste.
- `ApplyPropertyEditDirect()` mutates the `OverworldMap`, refreshes affected
  graphics/palette/canvas state, and writes stable direct ROM table fields when
  needed.
- `ReadPropertyValue()` reads the effective before/after value for undo.

`OverworldEditor` owns undo integration:

- `ApplyOverworldPropertyEdit()` records a single
  `OverworldMapPropertyEditAction`.
- `ApplyOverworldPropertyEdits()` records one
  `OverworldMapPropertyBatchEditAction` for multi-field operations such as
  context-menu metadata paste.

Canvas context-menu metadata paste skips fields unsupported by the loaded ROM
version instead of aborting after partially applying earlier fields. Full-map
metadata copies can paste either the full payload or a scoped subset; scoped
copies only enable the matching scoped paste action so a palette copy cannot
accidentally overwrite graphics, music, or messages.

The canvas context menu also exposes related-map navigation for multi-area
maps. Parent and sibling map entries select the clicked map's effective area
without requiring mouse travel through the side properties area.

The toolbar mirrors ZScream-style top-row metadata editing while also showing
the current scoped metadata clipboard when one exists. The metadata popup
includes project-label rows for referenced graphics, palette, message, and
music IDs so Oracle projects can name hack-specific resources directly from the
map context instead of opening the global label manager.

## Coordinate Systems

*   **Global Coordinates**: The overworld is conceptually a large grid.
    *   Light World: 8x8 maps.
    *   Dark World: 8x8 maps.
    *   Special Areas: Independent maps.
*   **Map Coordinates**: Each map is 512x512 pixels (32x32 tiles of 16x16 pixels).
*   **Tile Coordinates**: Objects are placed on a 16x16 grid within a map.

## Large Maps

ALttP combines smaller maps into larger scrolling areas (e.g., 2x2).
*   **Parent Map**: The top-left map typically holds the main properties.
*   **Child Maps**: The other 3 maps inherit properties from the parent but contain their own tile data.
*   **ZSCustomOverworld**: Introduces more flexible map sizing (Wide, Tall, Large). The `Overworld` class handles the logic for configuring these (`ConfigureMultiAreaMap`).

Property edits that belong to the area rather than an individual child screen
resolve through the effective parent map. This keeps child-map edits from
silently writing duplicate metadata that the game will not use.

## Vanilla vs. ZSCustomOverworld Paths

The editor uses `OverworldVersionHelper` for feature gates:

- Vanilla supports small and large area sizes plus vanilla metadata fields.
- ZSCustomOverworld v1+ enables expanded-space fields such as custom tile
  graphics.
- ZSCustomOverworld v2+ enables main palette, area-specific background color,
  and directional mosaic fields.
- ZSCustomOverworld v3+ enables Wide/Tall area sizes and animated graphics.

Unsupported fields fail closed for direct single-field edits. Batch metadata
paste filters unsupported fields so a vanilla or older custom ROM can still
receive the compatible subset of copied metadata.

## Deferred Loading

To improve performance, `OverworldEditor` implements a deferred texture creation system.
*   Map data is loaded from ROM, but textures are not created immediately.
*   `EnsureMapTexture` is called only when a map becomes visible, creating the SDL texture on-demand.
*   `ProcessDeferredTextures` creates a batch of textures each frame to avoid stalling the UI.
