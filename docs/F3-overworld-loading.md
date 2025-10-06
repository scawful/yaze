# Overworld Loading Guide

This document provides a comprehensive guide to understanding how overworld loading works in both ZScream (C#) and yaze (C++), including the differences between vanilla ROMs and ZSCustomOverworld v2/v3 ROMs.

## Table of Contents

1. [Overview](#overview)
2. [ROM Types and Versions](#rom-types-and-versions)
3. [Overworld Map Structure](#overworld-map-structure)
4. [Loading Process](#loading-process)
5. [ZScream Implementation](#zscream-implementation)
6. [Yaze Implementation](#yaze-implementation)
7. [Key Differences](#key-differences)
8. [Common Issues and Solutions](#common-issues-and-solutions)

## Overview

Both ZScream and yaze are Zelda 3 ROM editors that support editing overworld maps. They handle three main types of ROMs:

- **Vanilla ROMs**: Original Zelda 3 ROMs without modifications
- **ZSCustomOverworld v2**: ROMs with expanded overworld features
- **ZSCustomOverworld v3**: ROMs with additional features like overlays and custom background colors

## ROM Types and Versions

### Version Detection

Both editors detect the ROM version using the same constant:

```cpp
// Address: 0x140145
constexpr int OverworldCustomASMHasBeenApplied = 0x140145;

// Version values:
// 0xFF = Vanilla ROM
// 0x02 = ZSCustomOverworld v2
// 0x03 = ZSCustomOverworld v3
```

### Feature Support by Version

| Feature | Vanilla | v2 | v3 |
|---------|---------|----|----| 
| Basic Overworld Maps | ✅ | ✅ | ✅ |
| Area Size Enum | ❌ | ❌ | ✅ |
| Main Palette | ❌ | ✅ | ✅ |
| Custom Background Colors | ❌ | ✅ | ✅ |
| Subscreen Overlays | ✅ | ✅ | ✅ |
| Animated GFX | ❌ | ❌ | ✅ |
| Custom Tile Graphics | ❌ | ❌ | ✅ |
| Vanilla Overlays | ✅ | ✅ | ✅ |

**Note:** Subscreen overlays are visual effects (fog, rain, backgrounds, etc.) that are shared between vanilla ROMs and ZSCustomOverworld. ZSCustomOverworld v2+ expands on this by adding support for custom overlay configurations and additional overlay types.

## Overworld Map Structure

### Core Properties

Each overworld map contains the following core properties:

```cpp
class OverworldMap {
  // Basic properties
  uint8_t index_;                    // Map index (0-159)
  uint8_t parent_;                   // Parent map ID
  uint8_t world_;                    // World type (0=LW, 1=DW, 2=SW)
  uint8_t game_state_;               // Game state (0=Beginning, 1=Zelda, 2=Agahnim)
  
  // Graphics and palettes
  uint8_t area_graphics_;            // Area graphics ID
  uint8_t area_palette_;             // Area palette ID
  uint8_t main_palette_;             // Main palette ID (v2+)
  std::array<uint8_t, 3> sprite_graphics_;  // Sprite graphics IDs
  std::array<uint8_t, 3> sprite_palette_;   // Sprite palette IDs
  
  // Map properties
  uint16_t message_id_;              // Message ID
  bool mosaic_;                      // Mosaic effect enabled
  bool large_map_;                   // Is large map (vanilla)
  AreaSizeEnum area_size_;           // Area size (v3)
  
  // Custom features (v2/v3)
  uint16_t area_specific_bg_color_;  // Custom background color
  uint16_t subscreen_overlay_;       // Subscreen overlay ID (references special area maps)
  uint8_t animated_gfx_;             // Animated graphics ID
  std::array<uint8_t, 8> custom_gfx_ids_;  // Custom tile graphics
  
  // Overlay support (vanilla and custom)
  uint16_t vanilla_overlay_id_;      // Vanilla overlay ID
  bool has_vanilla_overlay_;         // Has vanilla overlay data
  std::vector<uint8_t> vanilla_overlay_data_;  // Raw overlay data
};
```

## Overlays and Special Area Maps

### Understanding Overlays

Overlays in Zelda 3 are **visual effects** that are displayed over or behind the main overworld map. They include effects like fog, rain, canopy, backgrounds, and other atmospheric elements. Overlays are collections of tile positions and tile IDs that specify where to place specific graphics on the map.

### Special Area Maps (0x80-0x9F)

The special area maps (0x80-0x9F) contain the actual tile data for overlays. These maps store the graphics that overlays reference and use to create visual effects:

- **0x80-0x8F**: Various special area maps containing overlay graphics
- **0x90-0x9F**: Additional special area maps including more overlay graphics

### Overlay ID Mappings

Overlay IDs directly correspond to special area map indices. Common overlay mappings:

| Overlay ID | Special Area Map | Description |
|------------|------------------|-------------|
| 0x0093 | 0x93 | Triforce Room Curtain |
| 0x0094 | 0x94 | Under the Bridge |
| 0x0095 | 0x95 | Sky Background (LW Death Mountain) |
| 0x0096 | 0x96 | Pyramid Background |
| 0x0097 | 0x97 | First Fog Overlay (Master Sword Area) |
| 0x009C | 0x9C | Lava Background (DW Death Mountain) |
| 0x009D | 0x9D | Second Fog Overlay (Lost Woods/Skull Woods) |
| 0x009E | 0x9E | Tree Canopy (Forest) |
| 0x009F | 0x9F | Rain Effect (Misery Mire) |

### Drawing Order

Overlays are drawn in a specific order based on their type:

- **Background Overlays** (0x95, 0x96, 0x9C): Drawn behind the main map tiles
- **Foreground Overlays** (0x9D, 0x97, 0x93, 0x94, 0x9E, 0x9F): Drawn on top of the main map tiles with transparency

### Vanilla Overlay Loading

In vanilla ROMs, overlays are loaded by parsing SNES assembly-like commands that specify tile positions and IDs:

```cpp
absl::Status LoadVanillaOverlay() {
  uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
  
  // Only load vanilla overlays for vanilla ROMs
  if (asm_version != 0xFF) {
    has_vanilla_overlay_ = false;
    return absl::OkStatus();
  }
  
  // Load overlay pointer for this map
  int address = (kOverlayPointersBank << 16) +
                ((*rom_)[kOverlayPointers + (index_ * 2) + 1] << 8) +
                (*rom_)[kOverlayPointers + (index_ * 2)];
  
  // Parse overlay commands:
  // LDA #$xxxx - Load tile ID into accumulator
  // LDX #$xxxx - Load position into X register  
  // STA $xxxx - Store tile at position
  // STA $xxxx,x - Store tile at position + X
  // INC A - Increment accumulator (for sequential tiles)
  // JMP $xxxx - Jump to another overlay routine
  // END (0x60) - End of overlay data
  
  return absl::OkStatus();
}
```

### Special Area Graphics Loading

Special area maps require special handling for graphics loading:

```cpp
void LoadAreaInfo() {
  if (parent_ >= kSpecialWorldMapIdStart) {
    // Special World (SW) areas
    if (asm_version >= 3 && asm_version != 0xFF) {
      // Use expanded sprite tables for v3
      sprite_graphics_[0] = (*rom_)[kOverworldSpecialSpriteGfxGroupExpandedTemp + 
                                     parent_ - kSpecialWorldMapIdStart];
    } else {
      // Use original sprite tables for v2/vanilla
      sprite_graphics_[0] = (*rom_)[kOverworldSpecialGfxGroup + 
                                     parent_ - kSpecialWorldMapIdStart];
    }
    
    // Handle special cases for specific maps
    if (index_ == 0x88 || index_ == 0x93) {
      area_graphics_ = 0x51;
      area_palette_ = 0x00;
    } else if (index_ == 0x95) {
      // Make this the same GFX as LW death mountain areas
      area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x03];
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x03];
    } else if (index_ == 0x96) {
      // Make this the same GFX as pyramid areas
      area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x5B];
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x5B];
    } else if (index_ == 0x9C) {
      // Make this the same GFX as DW death mountain areas
      area_graphics_ = (*rom_)[kAreaGfxIdPtr + 0x43];
      area_palette_ = (*rom_)[kOverworldMapPaletteIds + 0x43];
    }
  }
}
```

## Loading Process

### 1. Version Detection

Both editors first detect the ROM version:

```cpp
uint8_t asm_version = rom[OverworldCustomASMHasBeenApplied];
```

### 2. Map Initialization

For each of the 160 overworld maps (0x00-0x9F):

```cpp
// ZScream
var map = new OverworldMap(index, overworld);

// Yaze
OverworldMap map(index, rom);
```

### 3. Property Loading

The loading process varies by ROM version:

#### Vanilla ROMs (asm_version == 0xFF)

```cpp
void LoadAreaInfo() {
  // Load from vanilla tables
  message_id_ = rom[kOverworldMessageIds + index_ * 2];
  area_graphics_ = rom[kOverworldMapGfx + index_];
  area_palette_ = rom[kOverworldMapPaletteIds + index_];
  
  // Determine large map status
  large_map_ = (rom[kOverworldMapSize + index_] != 0);
  
  // Load vanilla overlay
  LoadVanillaOverlay();
}
```

#### ZSCustomOverworld v2/v3

```cpp
void LoadAreaInfo() {
  // Use expanded tables for v3
  if (asm_version >= 3) {
    message_id_ = rom[kOverworldMessagesExpanded + index_ * 2];
    area_size_ = static_cast<AreaSizeEnum>(rom[kOverworldScreenSize + index_]);
  } else {
    message_id_ = rom[kOverworldMessageIds + index_ * 2];
    area_size_ = large_map_ ? LargeArea : SmallArea;
  }
  
  // Load custom overworld data
  LoadCustomOverworldData();
}
```

### 4. Custom Data Loading

For ZSCustomOverworld ROMs:

```cpp
void LoadCustomOverworldData() {
  // Load main palette
  main_palette_ = rom[OverworldCustomMainPaletteArray + index_];
  
  // Load custom background color
  if (rom[OverworldCustomAreaSpecificBGEnabled] != 0) {
    area_specific_bg_color_ = rom[OverworldCustomAreaSpecificBGPalette + index_ * 2];
  }
  
  // Load v3 features
  if (asm_version >= 3) {
    subscreen_overlay_ = rom[OverworldCustomSubscreenOverlayArray + index_ * 2];
    animated_gfx_ = rom[OverworldCustomAnimatedGFXArray + index_];
    
    // Load custom tile graphics (8 sheets)
    for (int i = 0; i < 8; i++) {
      custom_gfx_ids_[i] = rom[OverworldCustomTileGFXGroupArray + index_ * 8 + i];
    }
  }
}
```

## ZScream Implementation

### OverworldMap Constructor

```csharp
public OverworldMap(byte index, Overworld overworld) {
    Index = index;
    this.overworld = overworld;
    
    // Load area info
    LoadAreaInfo();
    
    // Load custom data if available
    if (ROM.DATA[Constants.OverworldCustomASMHasBeenApplied] != 0xFF) {
        LoadCustomOverworldData();
    }
    
    // Build graphics and palette
    BuildMap();
}
```

### Key Methods

- `LoadAreaInfo()`: Loads basic map properties from ROM
- `LoadCustomOverworldData()`: Loads ZSCustomOverworld features
- `LoadPalette()`: Loads and processes palette data
- `BuildMap()`: Constructs the final map bitmap

**Note**: ZScream is the original C# implementation that yaze is designed to be compatible with.

## Yaze Implementation

### OverworldMap Constructor

```cpp
OverworldMap::OverworldMap(int index, Rom* rom) : index_(index), rom_(rom) {
  LoadAreaInfo();
  LoadCustomOverworldData();
  SetupCustomTileset(asm_version);
}
```

### Key Methods

- `LoadAreaInfo()`: Loads basic map properties
- `LoadCustomOverworldData()`: Loads ZSCustomOverworld features  
- `LoadVanillaOverlay()`: Loads vanilla overlay data
- `LoadPalette()`: Loads and processes palette data
- `BuildTileset()`: Constructs graphics tileset
- `BuildBitmap()`: Creates the final map bitmap

### Current Status

✅ **ZSCustomOverworld v2/v3 Support**: Fully implemented and tested
✅ **Vanilla ROM Support**: Complete compatibility maintained
✅ **Overlay System**: Both vanilla and custom overlays supported
✅ **Map Properties System**: Integrated with UI components
✅ **Graphics Loading**: Optimized with caching and performance monitoring

## Key Differences

### 1. Language and Architecture

| Aspect | ZScream | Yaze |
|--------|---------|------|
| Language | C# | C++ |
| Memory Management | Garbage Collected | Manual (RAII) |
| Graphics | System.Drawing | Custom OpenGL |
| UI Framework | WinForms | ImGui |

### 2. Data Structures

**ZScream:**
```csharp
public class OverworldMap {
    public byte Index { get; set; }
    public AreaSizeEnum AreaSize { get; set; }
    public Bitmap GFXBitmap { get; set; }
    // ... other properties
}
```

**Yaze:**
```cpp
class OverworldMap {
  uint8_t index_;
  AreaSizeEnum area_size_;
  std::vector<uint8_t> bitmap_data_;
  // ... other member variables
};
```

### 3. Error Handling

**ZScream:** Uses exceptions and try-catch blocks
**Yaze:** Uses `absl::Status` return values and `RETURN_IF_ERROR` macros

### 4. Graphics Processing

**ZScream:** Uses .NET's `Bitmap` class and GDI+
**Yaze:** Uses custom `gfx::Bitmap` class with OpenGL textures

## Common Issues and Solutions

### 1. Version Detection Issues

**Problem:** ROM not recognized as ZSCustomOverworld
**Solution:** Check that `OverworldCustomASMHasBeenApplied` is set correctly

### 2. Palette Loading Errors

**Problem:** Maps appear with wrong colors
**Solution:** Verify palette group addresses and 0xFF fallback handling

### 3. Graphics Not Loading

**Problem:** Blank textures or missing graphics
**Solution:** Check graphics buffer bounds and ProcessGraphicsBuffer implementation

### 4. Overlay Issues

**Problem:** Vanilla overlays not displaying
**Solution:** 
- Verify overlay pointer addresses and SNES-to-PC conversion
- Ensure special area maps (0x80-0x9F) are properly loaded with correct graphics
- Check that overlay ID mappings are correct (e.g., 0x009D → map 0x9D)
- Verify that overlay preview shows the actual bitmap of the referenced special area map

**Problem:** Overlay preview showing incorrect information
**Solution:** Ensure overlay preview correctly maps overlay IDs to special area map indices and displays the appropriate bitmap from the special area maps (0x80-0x9F)

### 5. Large Map Problems

**Problem:** Large maps not rendering correctly
**Solution:** Check parent-child relationships and large map detection logic

### 6. Special Area Graphics Issues

**Problem:** Special area maps (0x80-0x9F) showing blank or incorrect graphics
**Solution:** 
- Verify special area graphics loading in `LoadAreaInfo()`
- Check that special cases for maps like 0x88, 0x93, 0x95, 0x96, 0x9C are handled correctly
- Ensure proper sprite graphics table selection for v2 vs v3 ROMs
- Verify that special area maps use the correct graphics from referenced LW/DW maps

## Best Practices

### 1. Version-Specific Code

Always check the ASM version before accessing version-specific features:

```cpp
uint8_t asm_version = (*rom_)[OverworldCustomASMHasBeenApplied];
if (asm_version >= 3) {
    // v3 features
} else if (asm_version == 0xFF) {
    // Vanilla features
}
```

### 2. Error Handling

Use proper error handling for ROM operations:

```cpp
absl::Status LoadPalette() {
    RETURN_IF_ERROR(LoadPaletteData());
    RETURN_IF_ERROR(ProcessPalette());
    return absl::OkStatus();
}
```

### 3. Memory Management

Be careful with memory management in C++:

```cpp
// Good: RAII and smart pointers
std::vector<uint8_t> data;
std::unique_ptr<OverworldMap> map;

// Bad: Raw pointers without cleanup
uint8_t* raw_data = new uint8_t[size];
OverworldMap* map = new OverworldMap();
```

### 4. Thread Safety

Both editors use threading for performance:

```cpp
// Yaze: Use std::async for parallel processing
auto future = std::async(std::launch::async, [this](int map_index) {
    RefreshChildMap(map_index);
}, map_index);
```

## Conclusion

Understanding the differences between ZScream and yaze implementations is crucial for maintaining compatibility and adding new features. Both editors follow similar patterns but use different approaches due to their respective languages and architectures.

The key is to maintain the same ROM data structure understanding while adapting to each editor's specific implementation patterns.
