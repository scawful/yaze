# Yaze WASM JavaScript API Reference

## Overview

The yaze WASM build exposes a comprehensive set of JavaScript APIs for programmatic control and data access. These APIs are organized into five main namespaces:

- **`window.yaze.control`** - Editor control and manipulation
- **`window.yaze.editor`** - Query current editor state
- **`window.yaze.data`** - Read-only access to ROM data
- **`window.yaze.gui`** - GUI automation and interaction
- **`window.yazeDebug`** - Debug utilities and diagnostics

## API Version

- Version: 2.1.0
- Last Updated: 2025-11-25
- Capabilities: `['palette', 'arena', 'graphics', 'timeline', 'pixel-inspector', 'rom', 'overworld', 'emulator', 'editor', 'control', 'data', 'gui']`

## Build Requirements

The WASM module must be built with these Emscripten flags for full API access:

```
-s MODULARIZE=1
-s EXPORT_NAME='createYazeModule'
-s EXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap','lengthBytesUTF8','stringToUTF8','UTF8ToString','getValue','setValue']
```

The dev server must set COOP/COEP headers for SharedArrayBuffer support. Use `./scripts/serve-wasm.sh` which handles this automatically.

## Quick Start

### Check if API is Ready

```javascript
// All control APIs share the same ready state
if (window.yaze.control.isReady()) {
  // APIs are available
}
```

### Basic Example

```javascript
// Switch to Dungeon editor
window.yaze.control.switchEditor('Dungeon');

// Get current editor state
const snapshot = window.yaze.editor.getSnapshot();
console.log('Active editor:', snapshot.editor_type);

// Get room tile data
const roomData = window.yaze.data.getRoomTiles(0);
console.log('Room dimensions:', roomData.width, 'x', roomData.height);

// Get available layouts
const layouts = window.yaze.control.getAvailableLayouts();
console.log('Available layouts:', layouts);
```

---

## window.yaze.control - Editor Control API

Provides programmatic control over the editor UI, ROM operations, and session management.

### Utility

#### isReady()

```javascript
const ready = window.yaze.control.isReady()
// Returns: boolean
```

Checks if the control API is initialized and ready for use.

### Editor Control

#### switchEditor(editorName)

```javascript
window.yaze.control.switchEditor('Dungeon')
window.yaze.control.switchEditor('Overworld')
window.yaze.control.switchEditor('Graphics')
```

**Parameters:**
- `editorName` (string): Name of editor to switch to
  - Valid values: `"Overworld"`, `"Dungeon"`, `"Graphics"`, `"Palette"`, `"Sprite"`, `"Music"`, `"Message"`, `"Screen"`, `"Assembly"`, `"Hex"`, `"Agent"`, `"Settings"`

**Returns:**
```json
{
  "success": true,
  "editor": "Dungeon"
}
```

#### getCurrentEditor()

```javascript
const editor = window.yaze.control.getCurrentEditor()
```

**Returns:**
```json
{
  "name": "Dungeon",
  "type": 1,
  "active": true
}
```

#### getAvailableEditors()

```javascript
const editors = window.yaze.control.getAvailableEditors()
```

**Returns:**
```json
[
  {"name": "Overworld", "type": 0},
  {"name": "Dungeon", "type": 1},
  {"name": "Graphics", "type": 2}
]
```

### Card Control

Cards are dockable panels within each editor.

#### openCard(cardId)

```javascript
window.yaze.control.openCard('dungeon.room_selector')
```

**Parameters:**
- `cardId` (string): Card identifier

**Returns:**
```json
{
  "success": true,
  "card_id": "dungeon.room_selector",
  "visible": true
}
```

#### closeCard(cardId)

```javascript
window.yaze.control.closeCard('dungeon.room_selector')
```

**Returns:**
```json
{
  "success": true,
  "card_id": "dungeon.room_selector",
  "visible": false
}
```

#### toggleCard(cardId)

```javascript
window.yaze.control.toggleCard('dungeon.room_selector')
```

#### getVisibleCards()

```javascript
const cards = window.yaze.control.getVisibleCards()
```

#### getAvailableCards()

```javascript
const cards = window.yaze.control.getAvailableCards()
```

#### getCardsInCategory(category)

```javascript
const cards = window.yaze.control.getCardsInCategory('Dungeon')
```

### Layout Control

#### setCardLayout(layoutName)

```javascript
window.yaze.control.setCardLayout('dungeon_default')
```

**Parameters:**
- `layoutName` (string): Name of layout preset

**Returns:**
```json
{
  "success": true,
  "layout": "dungeon_default"
}
```

#### getAvailableLayouts()

```javascript
const layouts = window.yaze.control.getAvailableLayouts()
```

**Returns:**
```json
[
  "overworld_default",
  "dungeon_default",
  "graphics_default",
  "debug_default",
  "minimal",
  "all_cards"
]
```

#### saveCurrentLayout(layoutName)

```javascript
window.yaze.control.saveCurrentLayout('my_custom_layout')
```

### Menu/UI Actions

#### triggerMenuAction(actionPath)

```javascript
window.yaze.control.triggerMenuAction('File.Save')
```

**Parameters:**
- `actionPath` (string): Menu path (format: `"Menu.Action"`)

#### getAvailableMenuActions()

```javascript
const actions = window.yaze.control.getAvailableMenuActions()
```

### Session Control

#### getSessionInfo()

```javascript
const info = window.yaze.control.getSessionInfo()
```

**Returns:**
```json
{
  "session_index": 0,
  "session_count": 1,
  "rom_loaded": true,
  "rom_filename": "zelda3.sfc",
  "rom_title": "THE LEGEND OF ZELDA",
  "current_editor": "Dungeon"
}
```

#### createSession()

```javascript
const result = window.yaze.control.createSession()
```

#### switchSession(sessionIndex)

```javascript
window.yaze.control.switchSession(0)
```

### ROM Control

#### getRomStatus()

```javascript
const status = window.yaze.control.getRomStatus()
```

**Returns:**
```json
{
  "loaded": true,
  "filename": "zelda3.sfc",
  "title": "THE LEGEND OF ZELDA",
  "size": 1048576,
  "dirty": false
}
```

#### readRomBytes(address, count)

```javascript
const bytes = window.yaze.control.readRomBytes(0x10000, 32)
```

**Parameters:**
- `address` (number): ROM address to read from
- `count` (number, optional): Number of bytes (default: 16, max: 256)

#### writeRomBytes(address, bytes)

```javascript
window.yaze.control.writeRomBytes(0x10000, [0x00, 0x01, 0x02, 0x03])
```

**Parameters:**
- `address` (number): ROM address to write to
- `bytes` (array): Array of byte values (0-255)

#### saveRom()

```javascript
const result = window.yaze.control.saveRom()
```

---

## window.yaze.editor - Editor State API

Query current editor state.

### getSnapshot()

```javascript
const snapshot = window.yaze.editor.getSnapshot()
```

Get comprehensive snapshot of current editor state.

### getCurrentRoom()

```javascript
const room = window.yaze.editor.getCurrentRoom()
```

Get current dungeon room (only in Dungeon editor).

### getCurrentMap()

```javascript
const map = window.yaze.editor.getCurrentMap()
```

Get current overworld map (only in Overworld editor).

### getSelection()

```javascript
const selection = window.yaze.editor.getSelection()
```

Get current selection in active editor.

---

## window.yaze.data - Read-only Data API

Access ROM data without modifying it.

### Dungeon Data

#### getRoomTiles(roomId)

```javascript
const tiles = window.yaze.data.getRoomTiles(0)
```

Get tile data for a dungeon room.

**Parameters:**
- `roomId` (number): Room ID (0-295)

#### getRoomObjects(roomId)

```javascript
const objects = window.yaze.data.getRoomObjects(0)
```

Get tile objects in a dungeon room.

#### getRoomProperties(roomId)

```javascript
const props = window.yaze.data.getRoomProperties(0)
```

Get properties for a dungeon room.

### Overworld Data

#### getMapTiles(mapId)

```javascript
const tiles = window.yaze.data.getMapTiles(0)
```

Get tile data for an overworld map.

**Parameters:**
- `mapId` (number): Map ID (0-159)

#### getMapEntities(mapId)

```javascript
const entities = window.yaze.data.getMapEntities(0)
```

Get entities (entrances, exits, items, sprites) on a map.

#### getMapProperties(mapId)

```javascript
const props = window.yaze.data.getMapProperties(0)
```

Get properties for an overworld map.

### Palette Data

#### getPalette(groupName, paletteId)

```javascript
const palette = window.yaze.data.getPalette('ow_main', 0)
```

Get palette colors.

**Parameters:**
- `groupName` (string): Palette group name
- `paletteId` (number): Palette ID within group

#### getPaletteGroups()

```javascript
const groups = window.yaze.data.getPaletteGroups()
```

Get list of available palette groups.

---

## window.yaze.gui - GUI Automation API

For LLM agents to interact with the ImGui UI. (Structure defined; interaction methods planned for v2.1)

### UI Discovery

#### discover()

```javascript
const elements = window.yaze.gui.discover()
```

Get complete UI element tree for discovery and automation.

#### getElementBounds(elementId)

```javascript
const bounds = window.yaze.gui.getElementBounds('dungeon_room_selector')
```

Get precise bounds for a specific UI element.

---

## window.yazeDebug - Debug API

Debug utilities for analyzing ROM state, graphics pipeline, and emulator status.

### Utility

#### isReady()

```javascript
if (window.yazeDebug.isReady()) {
  // Debug API is available
}
```

#### dumpAll()

```javascript
const dump = window.yazeDebug.dumpAll()
```

Get complete debug state dump.

#### formatForAI()

```javascript
const aiPrompt = window.yazeDebug.formatForAI()
```

Get debug state formatted for AI consumption.

### ROM Debug

#### rom.getStatus()

```javascript
const status = window.yazeDebug.rom.getStatus()
```

#### rom.readBytes(address, count)

```javascript
const bytes = window.yazeDebug.rom.readBytes(0x10000, 32)
```

#### rom.getPaletteGroup(groupName, paletteIndex)

```javascript
const palette = window.yazeDebug.rom.getPaletteGroup("ow_main", 0)
```

### Overworld Debug

#### overworld.getMapInfo(mapId)

```javascript
const mapInfo = window.yazeDebug.overworld.getMapInfo(0)
```

#### overworld.getTileInfo(mapId, tileX, tileY)

```javascript
const tileInfo = window.yazeDebug.overworld.getTileInfo(0, 10, 15)
```

### Palette Debug

#### palette.getEvents()

```javascript
const events = window.yazeDebug.palette.getEvents()
```

#### palette.getFullState()

```javascript
const state = window.yazeDebug.palette.getFullState()
```

#### palette.getData()

```javascript
const data = window.yazeDebug.palette.getData()
```

#### palette.getComparisons()

```javascript
const comparisons = window.yazeDebug.palette.getComparisons()
```

#### palette.samplePixel(x, y)

```javascript
const color = window.yazeDebug.palette.samplePixel(100, 100)
```

#### palette.clear()

```javascript
window.yazeDebug.palette.clear()
```

### Arena/Graphics Debug

#### arena.getStatus()

```javascript
const status = window.yazeDebug.arena.getStatus()
```

#### arena.getSheetInfo(index)

```javascript
const sheet = window.yazeDebug.arena.getSheetInfo(0)
```

### Graphics Diagnostics

#### graphics.getDiagnostics()

```javascript
const diag = window.yazeDebug.graphics.getDiagnostics()
```

#### graphics.detect0xFFPattern()

```javascript
const result = window.yazeDebug.graphics.detect0xFFPattern()
```

### Emulator Debug

#### emulator.getStatus()

```javascript
const status = window.yazeDebug.emulator.getStatus()
```

#### emulator.readMemory(address, count)

```javascript
const memory = window.yazeDebug.emulator.readMemory(0x7E0000, 16)
```

#### emulator.getVideoState()

```javascript
const video = window.yazeDebug.emulator.getVideoState()
```

### Editor Debug

#### editor.getState()

```javascript
const state = window.yazeDebug.editor.getState()
```

#### editor.executeCommand(command)

```javascript
const result = window.yazeDebug.editor.executeCommand('get_room_0')
```

### Timeline Analysis

#### timeline.get()

```javascript
const timeline = window.yazeDebug.timeline.get()
```

### AI Analysis Helpers

#### analysis.getSummary()

```javascript
const summary = window.yazeDebug.analysis.getSummary()
```

#### analysis.getHypothesis()

```javascript
const hypothesis = window.yazeDebug.analysis.getHypothesis()
```

#### analysis.getFullState()

```javascript
const fullState = window.yazeDebug.analysis.getFullState()
```

---

## Error Handling

All API functions return error objects on failure:

```javascript
{
  "error": "Module not ready"
}
```

Always check for the `error` field before using data:

```javascript
const result = window.yaze.control.switchEditor('Invalid');
if (result.error) {
  console.error('API call failed:', result.error);
} else {
  console.log('Success:', result);
}
```

---

## Browser Console Quick Reference

```javascript
window.yaze.control.isReady()
window.yaze.control.getRomStatus()
window.yaze.control.switchEditor('Dungeon')
window.yaze.editor.getSnapshot()
window.yaze.data.getRoomTiles(0)
window.yaze.control.getAvailableEditors()
window.yazeDebug.dumpAll()
console.log(window.yazeDebug.formatForAI())
```

---

## API Maturity

- **Stable** (v2.0): `window.yaze.control`, `window.yaze.editor`, `window.yaze.data`, `window.yazeDebug`
- **Experimental** (v2.1): `window.yaze.gui` UI interaction methods

## Version History

**2.1.0** (2025-11-25)
- Added build requirements section documenting required Emscripten flags
- Documented MODULARIZE mode and EXPORTED_RUNTIME_METHODS requirements
- Updated for serve-wasm.sh COOP/COEP header support
- Thread pool increased to 8 workers (PTHREAD_POOL_SIZE=8)

**2.0.0** (2025-11-24)
- Consolidated all five API namespaces into single reference
- Comprehensive documentation of `window.yaze.control` (editor control, ROM operations)
- Comprehensive documentation of `window.yaze.editor` (editor state queries)
- Comprehensive documentation of `window.yaze.data` (read-only ROM data access)
- Added `window.yaze.gui` structure with UI discovery APIs
- Expanded from debug-only reference to comprehensive API documentation
- All methods verified against actual implementation in `wasm_control_api.cc`

**1.1.0** (2025-11-20)
- Initial `window.yazeDebug` API reference

---

## Related Documentation

- [WASM Development Guide](./agents/wasm-development-guide.md) - Building and debugging WASM
- [Architecture Documentation](./architecture/README.md) - System design details
- [CI/CD Documentation](./ci-and-testing.md) - Build and test infrastructure
