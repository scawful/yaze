# Overworld Map Data Structure

**Status**: Draft
**Last Updated**: 2025-11-21
**Related Code**: `src/zelda3/overworld/overworld_map.h`, `src/zelda3/overworld/overworld_map.cc`

This document details the internal structure of an Overworld Map in YAZE.

## Overview

An `OverworldMap` represents a single screen of the overworld. In vanilla ALttP, these are indexed 0x00 to 0xBF.

### Key Data Structures

*   **Map Index**: unique identifier (0-159).
*   **Parent Index**: For large maps, points to the "main" map that defines properties.
*   **Graphics**:
    *   `current_gfx_`: The raw graphics tiles loaded for this map.
    *   `current_palette_`: The 16-color palette rows used by this map.
    *   `bitmap_data_`: The rendered pixels (indexed color).
*   **Properties**:
    *   `message_id_`: ID of the text message displayed when entering.
    *   `area_music_`: Music track IDs.
    *   `sprite_graphics_`: Which sprite sheets are loaded.

### Persistence (Loading/Saving)

*   **Loading**:
    *   `Overworld::LoadOverworldMaps` iterates through all map IDs.
    *   `OverworldMap` constructor initializes basic data.
    *   `BuildMap` decompresses the tile data from ROM (Map32/Map16 conversion).
*   **Saving**:
    *   `Overworld::SaveOverworldMaps` serializes the tile data back to the compressed format.
    *   It handles checking for space and repointing if the data size increases.

### ZSCustomOverworld Integration

The `OverworldMap` class has been extended to support ZSCustomOverworld (ZSO) features.

*   **Custom Properties**:
    *   `area_specific_bg_color_`: Custom background color per map.
    *   `subscreen_overlay_`: ID for custom cloud/fog overlays.
    *   `animated_gfx_`: ID for custom animated tiles (water, flowers).
    *   `mosaic_expanded_`: Flags for per-map mosaic effects.
*   **Data Storage**:
    *   These properties are stored in expanded ROM areas defined by ZSO.
    *   `LoadCustomOverworldData` reads these values from their specific ROM addresses.

### Overlay System

Some maps have interactive overlays (e.g., the cloud layer in the Desert Palace entrance).
*   `overlay_id_`: ID of the overlay.
*   `overlay_data_`: The compressed tile data for the overlay layer.
*   The editor renders this on top of the base map if enabled.
