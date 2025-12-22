# Overworld Editor Architecture

**Status**: Draft
**Last Updated**: 2025-11-21
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
    *   **Properties**: Changes in the `MapPropertiesSystem` update the `OverworldMap` state and trigger a re-render.

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

## Deferred Loading

To improve performance, `OverworldEditor` implements a deferred texture creation system.
*   Map data is loaded from ROM, but textures are not created immediately.
*   `EnsureMapTexture` is called only when a map becomes visible, creating the SDL texture on-demand.
*   `ProcessDeferredTextures` creates a batch of textures each frame to avoid stalling the UI.
