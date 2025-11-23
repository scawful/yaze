# ZSCustomOverworld Integration

**Status**: Draft
**Last Updated**: 2025-11-21
**Related Code**: `src/zelda3/overworld/overworld.cc`, `src/zelda3/overworld/overworld_map.cc`

This document details how YAZE integrates with ZSCustomOverworld (ZSO), a common assembly patch for ALttP that expands overworld capabilities.

## Feature Support

YAZE supports the following ZSO features:

### 1. Multi-Area Maps (Map Sizing)
Vanilla ALttP has limited map configurations. ZSO allows any map to be part of a larger scrolling area.
*   **Area Sizes**: Small (1x1), Large (2x2), Wide (2x1), Tall (1x2).
*   **Implementation**: `Overworld::ConfigureMultiAreaMap` updates the internal ROM tables that define map relationships and scrolling behavior.

### 2. Custom Graphics & Palettes
*   **Per-Area Background Color**: Allows specific background colors for each map, overriding the default world color.
    *   Storage: `OverworldCustomAreaSpecificBGPalette` (0x140000)
*   **Animated Graphics**: Assigns different animated tile sequences (water, lava) per map.
    *   Storage: `OverworldCustomAnimatedGFXArray` (0x1402A0)
*   **Main Palette Override**: Allows changing the main 16-color palette per map.

### 3. Visual Effects
*   **Mosaic Effect**: Enables the pixelation effect on a per-map basis.
    *   Storage: `OverworldCustomMosaicArray` (0x140200)
*   **Subscreen Overlay**: Controls cloud/fog layers.
    *   Storage: `OverworldCustomSubscreenOverlayArray` (0x140340)

## ASM Patching

YAZE includes the capability to apply the ZSO ASM patch directly to a ROM.
*   **Method**: `OverworldEditor::ApplyZSCustomOverworldASM`
*   **Process**:
    1.  Checks current ROM version.
    2.  Uses `core::AsarWrapper` to apply the assembly patch.
    3.  Updates version markers in the ROM header.
    4.  Initializes the new data structures in expanded ROM space.

## Versioning & ROM Detection

The editor detects the ZSO version present in the ROM to enable/disable features.

### Version Detection
- **Source**: `overworld_version_helper.h` - Contains version detection logic
- **Check Point**: ROM header byte `asm_version` indicates which ZSO version is installed
- **Supported Versions**: v1, v2, v3 (with v3 being the most feature-rich)
- **Key Method**: `OverworldMap::SetupCustomTileset(uint8_t asm_version)` - Initializes custom properties based on detected version

### UI Adaptation
- `MapPropertiesSystem` shows/hides ZSO-specific controls based on detected version
- Version 1 controls are hidden if ROM doesn't have v1 ASM patch
- Version 3 controls appear only when ROM has v3+ patch installed
- Helpful messages displayed for unsupported features (e.g., "Requires ZSCustomOverworld v3+")

### Storage Locations

ROM addresses for ZSCustomOverworld data (expanded ROM area):

| Feature | Constant | Address | Size | Notes |
|---------|----------|---------|------|-------|
| Area-Specific BG Palette | OverworldCustomAreaSpecificBGPalette | 0x140000 | 2 bytes × 160 maps | Per-map override for background color |
| Main Palette Override | OverworldCustomMainPaletteArray | 0x140160 | 1 byte × 160 maps | Per-map main palette selection |
| Mosaic Effect | OverworldCustomMosaicArray | 0x140200 | 1 byte × 160 maps | Pixelation effect per-map |
| Subscreen Overlay | OverworldCustomSubscreenOverlayArray | 0x140340 | 2 bytes × 160 maps | Cloud/fog layer IDs |
| Animated GFX | OverworldCustomAnimatedGFXArray | 0x1402A0 | 1 byte × 160 maps | Water, lava animation sets |
| Custom Tile GFX | OverworldCustomTileGFXGroupArray | 0x140480 | 8 bytes × 160 maps | Custom tile graphics groups |
| Feature Enables | Various (0x140141-0x140148) | — | 1 byte each | Toggle flags for each feature |

## Implementation Details

### Configuration Method
```cpp
// This is the critical method for multi-area map configuration
absl::Status Overworld::ConfigureMultiAreaMap(int parent_index, AreaSizeEnum size);
```

**Process**:
1. Takes parent map index and desired size
2. Updates ROM parent ID table at appropriate address based on ZSO version
3. Recalculates scroll positions for area boundaries
4. Persists changes back to ROM
5. Reloads affected map data

**Never set `area_size` property directly** - Always use `ConfigureMultiAreaMap()` to ensure ROM consistency.

### Custom Properties Access
```cpp
// Get mutable reference to a map
auto& map = overworld_.maps[map_id];

// Check if custom features are available
if (rom->asm_version >= 1) {
    map.SetupCustomTileset(rom->asm_version);
    // Now custom properties are initialized
    uint16_t custom_bg_color = map.area_specific_bg_color_;
}

// Modify custom properties
map.subscreen_overlay_ = new_overlay_id;  // Will be saved to ROM on next save
```
