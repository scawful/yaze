# window.yazeDebug JavaScript API Documentation

## Overview
The `window.yazeDebug` API provides unified access to yaze's WASM debug infrastructure for AI assistants (Gemini/Antigravity) to query ROM state, overworld data, palette info, and rendering pipeline status.

## API Version
- Version: 1.1.0
- Capabilities: `['palette', 'arena', 'graphics', 'timeline', 'pixel-inspector', 'rom', 'overworld', 'emulator', 'editor']`

## Basic Usage

### Check if Module is Ready
```javascript
window.yazeDebug.isReady()
// Returns: boolean
```

### Get Complete State Dump
```javascript
window.yazeDebug.dumpAll()
// Returns: object with all debug state
```

### Get Formatted State for AI
```javascript
window.yazeDebug.formatForAI()
// Returns: human-readable string formatted for AI consumption
```

---

## ROM Debug Functions

### Get ROM Status
```javascript
window.yazeDebug.rom.getStatus()
```
**Returns:**
```json
{
  "loaded": true,
  "size": 1048576,
  "title": "THE LEGEND OF ZELDA",
  "version": 0
}
```

### Read ROM Bytes
```javascript
window.yazeDebug.rom.readBytes(address, count)
```
**Parameters:**
- `address` (number): ROM address to read from
- `count` (number, optional): Number of bytes to read (default: 16, max: 256)

**Returns:**
```json
{
  "address": 65536,
  "count": 16,
  "bytes": [0x12, 0x34, 0x56, ...]
}
```

**Example:**
```javascript
// Read 32 bytes from address 0x10000
window.yazeDebug.rom.readBytes(0x10000, 32)
```

### Get ROM Palette Group
```javascript
window.yazeDebug.rom.getPaletteGroup(groupName, paletteIndex)
```
**Parameters:**
- `groupName` (string): Palette group name. Valid values:
  - `"ow_main"`, `"ow_aux"`, `"ow_animated"` - Overworld palettes
  - `"dungeon_main"` - Dungeon palettes
  - `"hud"` - HUD palettes
  - `"global_sprites"`, `"sprites_aux1"`, `"sprites_aux2"`, `"sprites_aux3"` - Sprite palettes
  - `"armors"`, `"swords"`, `"shields"` - Equipment palettes
  - `"grass"`, `"3d_object"`, `"ow_mini_map"` - Misc palettes
- `paletteIndex` (number): Index within the group

**Returns:**
```json
{
  "group_name": "ow_main",
  "palette_index": 0,
  "size": 16,
  "colors": [
    {"r": 255, "g": 0, "b": 0},
    {"r": 0, "g": 255, "b": 0},
    ...
  ]
}
```

**Example:**
```javascript
// Get the first overworld main palette
window.yazeDebug.rom.getPaletteGroup("ow_main", 0)

// Get dungeon palette
window.yazeDebug.rom.getPaletteGroup("dungeon_main", 0)
```

---

## Overworld Debug Functions

### Get Overworld Map Info
```javascript
window.yazeDebug.overworld.getMapInfo(mapId)
```
**Parameters:**
- `mapId` (number): Map ID (0-159)
  - 0-63: Light World
  - 64-127: Dark World
  - 128-159: Special World

**Returns:**
```json
{
  "map_id": 0,
  "size_flag": 32,
  "is_large": true,
  "parent_id": 0,
  "world": "light"
}
```

**Example:**
```javascript
// Get info for Hyrule Castle (Light World)
window.yazeDebug.overworld.getMapInfo(0)

// Get info for Dark World map
window.yazeDebug.overworld.getMapInfo(64)
```

### Get Overworld Tile Info
```javascript
window.yazeDebug.overworld.getTileInfo(mapId, tileX, tileY)
```
**Parameters:**
- `mapId` (number): Map ID
- `tileX` (number): Tile X coordinate
- `tileY` (number): Tile Y coordinate

**Returns:**
```json
{
  "map_id": 0,
  "tile_x": 10,
  "tile_y": 15,
  "note": "Full tile data requires overworld to be loaded in editor"
}
```

---

## Palette Debug Functions

### Get Palette Events
```javascript
window.yazeDebug.palette.getEvents()
```
Returns all palette debug events logged during rendering.

### Get Full Palette State
```javascript
window.yazeDebug.palette.getFullState()
```
Returns complete palette state for AI analysis.

### Get Palette Data
```javascript
window.yazeDebug.palette.getData()
```
Returns palette data with all colors.

### Get Color Comparisons
```javascript
window.yazeDebug.palette.getComparisons()
```
Returns color comparison data for validation.

### Sample Pixel
```javascript
window.yazeDebug.palette.samplePixel(x, y)
```
Samples a pixel at canvas coordinates for debugging.

### Clear Palette Events
```javascript
window.yazeDebug.palette.clear()
```
Clears all palette debug events.

---

## Arena/Graphics Debug Functions

### Get Arena Status
```javascript
window.yazeDebug.arena.getStatus()
```
**Returns:**
```json
{
  "texture_queue_size": 10,
  "gfx_sheets": [
    {
      "index": 0,
      "width": 128,
      "height": 128,
      "has_texture": true,
      "has_surface": true
    }
  ]
}
```

### Get Graphics Sheet Info
```javascript
window.yazeDebug.arena.getSheetInfo(index)
```
**Parameters:**
- `index` (number): Sheet index (0-222)

**Returns:**
```json
{
  "index": 0,
  "active": true,
  "width": 128,
  "height": 128,
  "has_texture": true,
  "has_surface": true,
  "surface_format": 372645892,
  "palette_colors": 256
}
```

---

## Graphics Diagnostics Functions

### Get Graphics Diagnostics
```javascript
window.yazeDebug.graphics.getDiagnostics()
```
Returns comprehensive ROM graphics loading diagnostics for debugging regression issues.

**Returns:**
```json
{
  "rom_size": 1048576,
  "header_stripped": true,
  "checksum_valid": true,
  "ptr1_loc": 80896,
  "ptr2_loc": 81152,
  "ptr3_loc": 81408,
  "sheets": [
    {
      "index": 0,
      "decomp_size_param": 2048,
      "actual_decomp_size": 2048,
      "decompression_succeeded": true,
      "first_8_bytes": [12, 34, 56, 78, 90, 12, 34, 56]
    }
  ],
  "analysis": {
    "all_sheets_0xFF": false,
    "size_param_zero_bug": false,
    "header_misaligned": false,
    "suspected_regression": false
  }
}
```

### Detect 0xFF Pattern
```javascript
window.yazeDebug.graphics.detect0xFFPattern()
```
Quick check for the graphics loading regression (all sheets returning 0xFF).

**Returns:**
```json
{
  "detected": false,
  "message": "No 0xFF pattern detected"
}
```

---

## Emulator Debug Functions

### Get Emulator Status
```javascript
window.yazeDebug.emulator.getStatus()
```
Returns current emulator CPU state and performance metrics.

**Returns:**
```json
{
  "running": true,
  "cycles": 123456,
  "fps": 60.0,
  "registers": {
    "A": 255,
    "X": 0,
    "Y": 128,
    "S": 511,
    "P": 36,
    "PC": 32768,
    "DB": 0,
    "D": 0
  }
}
```

### Read Emulator Memory
```javascript
window.yazeDebug.emulator.readMemory(address, count)
```
**Parameters:**
- `address` (number): WRAM address to read
- `count` (number, optional): Number of bytes (default: 16, max: 256)

**Returns:**
```json
{
  "address": 32768,
  "count": 16,
  "bytes": [0, 0, 0, 0, ...]
}
```

### Get Video State
```javascript
window.yazeDebug.emulator.getVideoState()
```
Returns PPU state and scanline information.

**Returns:**
```json
{
  "scanline": 128,
  "h_count": 256,
  "frame_count": 12345,
  "brightness": 15
}
```

---

## Editor Debug Functions

### Get Editor State
```javascript
window.yazeDebug.editor.getState()
```
Returns current active editor and session information.

**Returns:**
```json
{
  "active_editor": "Dungeon",
  "session_id": "abc123",
  "rom_loaded": true
}
```

### Execute Command
```javascript
window.yazeDebug.editor.executeCommand(command)
```
Executes a z3ed CLI command in the editor context.

**Parameters:**
- `command` (string): z3ed command to execute

**Returns:**
```json
{
  "success": true,
  "output": "Command output here"
}
```

---

## Timeline Analysis

### Get Event Timeline
```javascript
window.yazeDebug.timeline.get()
```
Returns event timeline for analyzing order of operations.

---

## AI Analysis Helpers

### Get Diagnostic Summary
```javascript
window.yazeDebug.analysis.getSummary()
```
Returns human-readable diagnostic summary.

### Get Hypothesis Analysis
```javascript
window.yazeDebug.analysis.getHypothesis()
```
Returns hypothesis analysis based on captured events.

### Get Full Debug State
```javascript
window.yazeDebug.analysis.getFullState()
```
Returns complete debug state combining all subsystems.

---

## Complete Usage Example

```javascript
// Wait for WASM module to be ready
if (window.yazeDebug.isReady()) {
  // Check ROM status
  const romStatus = window.yazeDebug.rom.getStatus();
  console.log('ROM:', romStatus.title, 'Size:', romStatus.size);
  
  // Read some ROM bytes
  const bytes = window.yazeDebug.rom.readBytes(0x10000, 16);
  console.log('Bytes at 0x10000:', bytes.bytes);
  
  // Get overworld map info
  const mapInfo = window.yazeDebug.overworld.getMapInfo(0);
  console.log('Map 0 is large:', mapInfo.is_large);
  
  // Get palette from ROM
  const palette = window.yazeDebug.rom.getPaletteGroup("ow_main", 0);
  console.log('Palette has', palette.size, 'colors');
  
  // Get arena status
  const arena = window.yazeDebug.arena.getStatus();
  console.log('Texture queue size:', arena.texture_queue_size);
  
  // Dump everything for AI analysis
  const fullState = window.yazeDebug.dumpAll();
  console.log('Full state:', fullState);
  
  // Or get formatted string for AI
  const aiPrompt = window.yazeDebug.formatForAI();
  console.log(aiPrompt);
} else {
  console.log('WASM module not ready yet');
}
```

---

## Error Handling

All API functions check if the module is ready and return error objects on failure:

```javascript
{
  "error": "Module not ready"
}
```

or

```javascript
{
  "error": "Exception message here"
}
```

Always check for the `error` field in responses before using the data.

---

## Browser Console Testing

Open the browser console and try these commands:

```javascript
// Quick status check
window.yazeDebug.isReady()

// See all capabilities
window.yazeDebug.capabilities

// Get ROM info
window.yazeDebug.rom.getStatus()

// Read some ROM data
window.yazeDebug.rom.readBytes(0x10000, 32)

// Get map info for Light World map 0
window.yazeDebug.overworld.getMapInfo(0)

// Get complete dump
window.yazeDebug.dumpAll()

// Get formatted for AI
console.log(window.yazeDebug.formatForAI())
```
