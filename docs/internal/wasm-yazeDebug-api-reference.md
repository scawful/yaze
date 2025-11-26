# Yaze WASM JavaScript API Reference

## Overview

The yaze WASM build exposes a comprehensive set of JavaScript APIs for programmatic control and data access. These APIs are organized into six main namespaces:

- **`window.yaze.control`** - Editor control and manipulation
- **`window.yaze.editor`** - Query current editor state
- **`window.yaze.data`** - Read-only access to ROM data
- **`window.yaze.gui`** - GUI automation and interaction
- **`window.yazeDebug`** - Debug utilities and diagnostics
- **`window.aiTools`** - High-level AI assistant tools (Gemini Antigravity)

## API Version

- Version: 2.4.0
- Last Updated: 2025-11-25
- Capabilities: `['palette', 'arena', 'graphics', 'timeline', 'pixel-inspector', 'rom', 'overworld', 'emulator', 'editor', 'control', 'data', 'gui', 'loading-progress', 'ai-tools', 'async-editor-switch', 'card-groups', 'tree-sidebar', 'properties-panel']`

## Build Requirements

The WASM module must be built with these Emscripten flags for full API access:

```
-s MODULARIZE=1
-s EXPORT_NAME='createYazeModule'
-s EXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap','lengthBytesUTF8','stringToUTF8','UTF8ToString','getValue','setValue']
-s INITIAL_MEMORY=268435456   # 256MB initial heap
-s ALLOW_MEMORY_GROWTH=1      # Dynamic heap growth
-s MAXIMUM_MEMORY=1073741824  # 1GB max
-s STACK_SIZE=8388608         # 8MB stack
```

The dev server must set COOP/COEP headers for SharedArrayBuffer support. Use `./scripts/serve-wasm.sh` which handles this automatically.

### Memory Configuration

The WASM build uses optimized memory settings to reduce heap resize operations during ROM loading:
- **Initial Memory**: 256MB - Reduces heap resizing during overworld map loading (~200MB required)
- **Maximum Memory**: 1GB - Prevents runaway allocations
- **Stack Size**: 8MB - Handles recursive operations during asset decompression

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

For LLM agents to interact with the ImGui UI.

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

#### waitForElement(elementId, timeoutMs)

```javascript
const bounds = await window.yaze.gui.waitForElement('dungeon_room_selector', 5000)
```

Wait for an element to appear.

### Interaction

#### click(target)

```javascript
window.yaze.gui.click('dungeon_room_selector')
// OR
window.yaze.gui.click({x: 100, y: 200})
```

Simulate a click at coordinates or on an element by ID.

#### doubleClick(target)

```javascript
window.yaze.gui.doubleClick('dungeon_room_selector')
```

Simulate a double-click.

#### drag(from, to, steps)

```javascript
window.yaze.gui.drag({x: 0, y: 0}, {x: 100, y: 100}, 10)
```

Simulate a drag operation.

#### pressKey(key, modifiers)

```javascript
window.yaze.gui.pressKey('Enter', {ctrl: true})
```

Send a keyboard event to the canvas.

#### type(text, delayMs)

```javascript
await window.yaze.gui.type('Hello World', 50)
```

Type a string of text.

#### scroll(deltaX, deltaY)

```javascript
window.yaze.gui.scroll(0, 100)
```

Scroll the canvas.

### Canvas & State

#### takeScreenshot(format, quality)

```javascript
const dataUrl = window.yaze.gui.takeScreenshot('png', 0.92)
```

Take a screenshot of the canvas.

#### getCanvasInfo()

```javascript
const info = window.yaze.gui.getCanvasInfo()
```

Get canvas dimensions and position.

#### updateCanvasState()

```javascript
const state = window.yaze.gui.updateCanvasState()
```

Update canvas data-* attributes with current editor state.

#### startAutoUpdate(intervalMs)

```javascript
window.yaze.gui.startAutoUpdate(500)
```

Start automatic canvas state updates.

#### stopAutoUpdate()

```javascript
window.yaze.gui.stopAutoUpdate()
```

Stop automatic canvas state updates.

### Card Management

#### getAvailableCards()

```javascript
const cards = window.yaze.gui.getAvailableCards()
```

Get all available cards with their metadata.

#### showCard(cardId)

```javascript
window.yaze.gui.showCard('dungeon.room_selector')
```

Show a specific card.

#### hideCard(cardId)

```javascript
window.yaze.gui.hideCard('dungeon.room_selector')
```

Hide a specific card.

### Selection

#### getSelection()

```javascript
const selection = window.yaze.gui.getSelection()
```

Get the current selection in the active editor.

#### setSelection(ids)

```javascript
window.yaze.gui.setSelection(['obj_1', 'obj_2'])
```

Set selection programmatically.

---

## window.yazeDebug - Extended UI Control APIs (v2.4)

### Async Editor Switching

#### switchToEditorAsync(editorName)

Promise-based editor switching with operation tracking for reliable automation.

```javascript
const result = await window.yazeDebug.switchToEditorAsync('Dungeon');
// Returns: { success: true, editor: "Dungeon", session_id: 1 }
```

**Parameters:**
- `editorName` (string): One of 14 editor types: `Assembly`, `Dungeon`, `Graphics`, `Music`, `Overworld`, `Palette`, `Screen`, `Sprite`, `Message`, `Hex`, `Agent`, `Settings`, `World`, `Map`

**Returns:** `Promise<{success, editor?, session_id?, error?}>`

**Notes:**
- 5-second timeout with proper error reporting
- Polls internally at 16ms intervals for completion
- Requires ROM to be loaded

### Card Control API (`yazeDebug.cards`)

#### yazeDebug.cards.show(cardId)

```javascript
yazeDebug.cards.show('dungeon.room_selector');
// Returns: { success: true, card: "dungeon.room_selector", visible: true }
```

#### yazeDebug.cards.hide(cardId)

```javascript
yazeDebug.cards.hide('dungeon.object_editor');
// Returns: { success: true, card: "dungeon.object_editor", visible: false }
```

#### yazeDebug.cards.toggle(cardId)

```javascript
yazeDebug.cards.toggle('dungeon.room_selector');
// Returns: { success: true, card: "dungeon.room_selector", visible: true/false }
```

#### yazeDebug.cards.getState()

```javascript
const state = yazeDebug.cards.getState();
// Returns: { cards: [{ id, visible, category }, ...] }
```

#### yazeDebug.cards.getInCategory(category)

```javascript
yazeDebug.cards.getInCategory('dungeon');
// Returns: { cards: ["dungeon.room_selector", "dungeon.object_editor", ...] }
```

#### yazeDebug.cards.showGroup(groupName)

Show predefined card groups:
- `dungeon_editing` - Room selector, object editor, canvas
- `dungeon_debug` - Debug visualization cards
- `overworld_editing` - Map selector, tile editor, entity editor
- `graphics_editing` - Sheet viewer, tile editor
- `minimal` - Hide all cards

```javascript
yazeDebug.cards.showGroup('dungeon_editing');
// Returns: { success: true, group: "dungeon_editing", cards_shown: 3 }
```

#### yazeDebug.cards.hideGroup(groupName)

```javascript
yazeDebug.cards.hideGroup('dungeon_debug');
// Returns: { success: true, group: "dungeon_debug", cards_hidden: 2 }
```

#### yazeDebug.cards.getGroups()

```javascript
yazeDebug.cards.getGroups();
// Returns: { groups: [{ name, cards: [...] }, ...] }
```

### Sidebar Control API (`yazeDebug.sidebar`)

The left sidebar has two modes:
- **Tree View** (200px): Hierarchical expandable categories with checkboxes
- **Icon Mode** (48px): Compact icon-only view

#### yazeDebug.sidebar.isTreeView()

```javascript
yazeDebug.sidebar.isTreeView();
// Returns: boolean
```

#### yazeDebug.sidebar.setTreeView(enabled)

```javascript
yazeDebug.sidebar.setTreeView(true);  // Enable tree view (200px)
yazeDebug.sidebar.setTreeView(false); // Enable icon mode (48px)
// Returns: { success: true, mode: "tree" | "icon" }
```

#### yazeDebug.sidebar.toggle()

```javascript
yazeDebug.sidebar.toggle();
// Returns: { success: true, mode: "tree" | "icon" }
```

#### yazeDebug.sidebar.getState()

```javascript
yazeDebug.sidebar.getState();
// Returns: { available: true, mode: "tree", width: 200, collapsed: false }
```

### Right Panel Control API (`yazeDebug.rightPanel`)

Panel types: `properties`, `agent`, `proposals`, `settings`, `help`

#### yazeDebug.rightPanel.open(panelName)

```javascript
yazeDebug.rightPanel.open('properties');
// Returns: { success: true, panel: "properties" }
```

#### yazeDebug.rightPanel.close()

```javascript
yazeDebug.rightPanel.close();
// Returns: { success: true }
```

#### yazeDebug.rightPanel.toggle(panelName)

```javascript
yazeDebug.rightPanel.toggle('agent');
// Returns: { success: true, state: "open" | "closed", panel?: "agent" }
```

#### yazeDebug.rightPanel.getState()

```javascript
yazeDebug.rightPanel.getState();
// Returns: { available: true, active: "properties", expanded: true, width: 320 }
```

#### Convenience Methods

```javascript
yazeDebug.rightPanel.openProperties();  // Open properties panel
yazeDebug.rightPanel.openAgent();       // Open agent chat panel
```

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

---

## window.aiTools - High-level AI Assistant Tools

High-level tools designed for AI agents to inspect and control the application state.

### State Inspection

#### getAppState()

```javascript
const state = window.aiTools.getAppState()
```

Get full application state (timestamp, module ready, API ready, editor, ROM, cards).

#### getEditorState()

```javascript
const state = window.aiTools.getEditorState()
```

Get current editor state snapshot.

#### getVisibleCards()

```javascript
const cards = window.aiTools.getVisibleCards()
```

List visible cards.

#### getAvailableCards()

```javascript
const cards = window.aiTools.getAvailableCards()
```

List all available cards.

#### getRoomData()

```javascript
const data = window.aiTools.getRoomData()
```

Get current dungeon room data.

#### getMapData()

```javascript
const data = window.aiTools.getMapData()
```

Get current overworld map data.

### Navigation & Control

#### navigateTo()

```javascript
window.aiTools.navigateTo()
```

Prompt for navigation target (interactive).

#### jumpToRoom(roomId)

```javascript
window.aiTools.jumpToRoom(0)
```

Jump to a specific dungeon room.

#### jumpToMap(mapId)

```javascript
window.aiTools.jumpToMap(0)
```

Jump to a specific overworld map.

#### showCard()

```javascript
window.aiTools.showCard()
```

Prompt to show a card (interactive).

#### hideCard()

```javascript
window.aiTools.hideCard()
```

Prompt to hide a card (interactive).

### Documentation

#### dumpAPIReference()

```javascript
const ref = window.aiTools.dumpAPIReference()
```

Dump complete API reference for AI assistants.

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

## window.aiTools - Gemini Antigravity AI Tools

High-level helper functions designed for AI assistants like Gemini Antigravity that struggle to discover elements in the ImGui WASM application. These tools provide simplified access to common operations with console output for easy reading.

### Application State

#### getAppState()

```javascript
window.aiTools.getAppState()
```

Get complete application state including ROM status, current editor, visible cards, and available actions. Results are logged to console.

**Console Output:**
```
=== YAZE APPLICATION STATE ===
ROM Status: { loaded: true, filename: "zelda3.sfc", ... }
Current Editor: { name: "Dungeon", type: 1 }
Visible Cards: ["Room Selector", "Object Editor", ...]
Available Editors: ["Overworld", "Dungeon", "Graphics", ...]
Available Layouts: ["overworld_default", "dungeon_default", ...]
```

#### getEditorState()

```javascript
window.aiTools.getEditorState()
```

Get detailed snapshot of current editor state. Results are logged to console.

### Card Management

#### getVisibleCards()

```javascript
window.aiTools.getVisibleCards()
```

List all currently visible cards/panels. Results are logged to console.

#### getAvailableCards()

```javascript
window.aiTools.getAvailableCards()
```

List all available cards across all editors. Results are logged to console.

#### showCard(cardName)

```javascript
window.aiTools.showCard()        // Prompts for card name
window.aiTools.showCard('Room Selector')  // Direct call
```

Show/open a specific card. If called without argument, prompts user for card name.

#### hideCard(cardName)

```javascript
window.aiTools.hideCard()        // Prompts for card name
window.aiTools.hideCard('Object Editor')  // Direct call
```

Hide/close a specific card. If called without argument, prompts user for card name.

### Navigation

#### navigateTo(target)

```javascript
window.aiTools.navigateTo()      // Prompts for target
window.aiTools.navigateTo('room:0')      // Go to dungeon room 0
window.aiTools.navigateTo('map:5')       // Go to overworld map 5
window.aiTools.navigateTo('Dungeon')     // Switch to Dungeon editor
```

Navigate to a specific location. Supports:
- `room:<id>` - Navigate to dungeon room
- `map:<id>` - Navigate to overworld map
- Editor name - Switch to editor

### Data Access

#### getRoomData(roomId)

```javascript
window.aiTools.getRoomData()     // Prompts for room ID
window.aiTools.getRoomData(0)    // Get data for room 0
```

Get dungeon room data including tiles, objects, and properties. Results logged to console.

#### getMapData(mapId)

```javascript
window.aiTools.getMapData()      // Prompts for map ID
window.aiTools.getMapData(0)     // Get data for map 0
```

Get overworld map data including tiles, entities, and properties. Results logged to console.

### Documentation

#### dumpAPIReference()

```javascript
window.aiTools.dumpAPIReference()
```

Output complete API reference to console, including all available methods across all namespaces (`window.yaze.*`, `window.yazeDebug`, `window.aiTools`).

**Console Output:**
```
=== YAZE WASM API REFERENCE ===

--- window.yaze.control ---
  switchEditor(editorName) - Switch to a different editor
  getCurrentEditor() - Get current editor info
  ...

--- window.aiTools ---
  getAppState() - Get full application state
  getEditorState() - Get current editor snapshot
  ...
```

---

## Nav Bar UI for AI Assistants

The web interface includes dedicated dropdown menus for AI assistant access:

### Editor Switcher Dropdown

Quick access to all 13 editors:
- Overworld, Dungeon, Graphics, Palette, Sprite
- Music, Message, Screen, Assembly, Hex
- Agent, Code, Settings

### Emulator Controls Dropdown

- **Show Emulator** - Display emulator window
- **Run** - Start emulation
- **Pause** - Pause emulation
- **Step** - Single step execution
- **Reset** - Reset emulator state
- **Memory Viewer** - Open memory inspection
- **Disassembly** - View disassembled code

### Layouts Dropdown

Preset card configurations:
- `overworld_default` - Optimal for overworld editing
- `dungeon_default` - Optimal for dungeon editing
- `graphics_default` - Optimal for graphics editing
- `debug_default` - Debug-focused layout
- `minimal` - Minimal interface
- `all_cards` - Show all available cards

### AI Tools Dropdown

Dedicated menu with all `window.aiTools` functions accessible via UI clicks.

---

## Command Palette (Ctrl+K)

All AI tools are accessible via the command palette:

| Command | Action |
|---------|--------|
| `Editor: Overworld` | Switch to Overworld editor |
| `Editor: Dungeon` | Switch to Dungeon editor |
| `Emulator: Show` | Show emulator window |
| `Emulator: Run` | Start emulation |
| `AI: Get App State` | Run `aiTools.getAppState()` |
| `AI: Get Editor State` | Run `aiTools.getEditorState()` |
| `AI: API Reference` | Run `aiTools.dumpAPIReference()` |

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
- **Stable** (v2.3): `window.aiTools` - AI assistant helper functions
- **Experimental** (v2.1): `window.yaze.gui` UI interaction methods

## Loading Progress System

The WASM build includes a comprehensive loading progress system that reports status during ROM loading.

### JavaScript Loading Indicator API

```javascript
// These functions are called by C++ via WasmLoadingManager
window.createLoadingIndicator(id, taskName)  // Creates loading overlay
window.updateLoadingProgress(id, progress, message)  // Updates progress (0.0-1.0)
window.removeLoadingIndicator(id)  // Removes loading overlay
window.isLoadingCancelled(id)  // Check if user cancelled
```

### Loading Progress Flow

When a ROM is loaded, progress updates occur at these stages:

| Progress | Stage |
|----------|-------|
| 0% | Reading ROM file... |
| 5% | Loading ROM data... |
| 10% | Initializing editors... |
| 18% | Loading graphics sheets... |
| 26% | Loading overworld... |
| 34% | Loading dungeons... |
| 42% | Loading screen editor... |
| 50%+ | Loading remaining editors... |
| 100% | Complete |

### Monitoring Loading in Console

```javascript
// Check if ROM is loaded
window.yaze.control.getRomStatus()

// Get current editor state after loading
window.yaze.editor.getSnapshot()

// Check arena status for graphics loading
window.yazeDebug.arena.getStatus()
```

---

## Agent Discoverability Infrastructure

The WASM build includes infrastructure for external AI agents to discover and interact with ImGui elements that aren't natively part of the DOM.

### Overview

Since ImGui renders to a canvas element, traditional DOM queries can't find UI elements. The agent discoverability system bridges this gap by:

1. Creating invisible DOM overlays that mirror ImGui widget state
2. Exposing canvas data attributes for editor state
3. Providing JavaScript APIs for card/widget queries

### Components

#### 1. Widget Overlay (`src/web/core/widget_overlay.js`)

Creates invisible DOM elements mirroring ImGui widget state, enabling standard DOM queries for UI elements.

```javascript
// Access the overlay singleton
const overlay = window.yaze.gui.widgetOverlay;

// Query widgets by type
const buttons = overlay.getWidgetsByType('button');
const canvases = overlay.getWidgetsByType('canvas');

// Query widgets by window
const dungeonWidgets = overlay.getWidgetsByWindow('Dungeon Editor');

// Get all visible widgets
const visible = overlay.getVisibleWidgets();

// Get a specific widget by ID
const widget = overlay.getWidget('overworld_canvas');

// Export current widget state
const state = overlay.exportState();
// Returns: { count: 42, widgets: [...], timestamp: 1732536000000 }
```

#### 2. Canvas Data Attributes (`src/web/shell.html`)

The main canvas element exposes current editor state via data attributes:

```html
<canvas id="canvas"
        data-editor-type="Overworld"
        data-visible-cards="Map Properties,Entrance List"
        data-rom-loaded="true"
        data-session-id="0"
        data-zoom-level="1.0">
</canvas>
```

Query these attributes:

```javascript
const canvas = document.getElementById('canvas');
console.log('Editor:', canvas.dataset.editorType);
console.log('Visible Cards:', canvas.dataset.visibleCards);
console.log('ROM Loaded:', canvas.dataset.romLoaded === 'true');
console.log('Session ID:', canvas.dataset.sessionId);
```

#### 3. Card Registry APIs (`window.yaze.gui`)

Query and control editor cards programmatically:

```javascript
// Get all available cards with metadata
const cards = window.yaze.gui.getAvailableCards();
// Returns array of: { id, display_name, window_title, icon, category, visible, enabled, priority }

// Show/hide specific cards
window.yaze.gui.showCard('Map Properties');
window.yaze.gui.hideCard('Entrance Editor');

// Sync canvas data attributes with current state
window.yaze.gui.updateCanvasState();

// Start automatic state updates (useful for agents monitoring state)
window.yaze.gui.startAutoUpdate(500);  // Update every 500ms

// Stop automatic updates
window.yaze.gui.stopAutoUpdate();
```

### Usage Example

```javascript
// Full agent workflow example

// 1. Start auto-updating canvas state and widget overlay
window.yaze.gui.startAutoUpdate(500);

// 2. Wait for a specific card to be visible
const mapProps = await window.yaze.gui.waitForElement('Map Properties', 5000);
if (mapProps) {
  console.log('Map Properties card is visible at:', mapProps.x, mapProps.y);
}

// 3. Query current editor state from canvas
const canvas = document.getElementById('canvas');
console.log('Current editor:', canvas.dataset.editorType);

// 4. Get all visible cards
const visibleCards = canvas.dataset.visibleCards.split(',');
console.log('Visible cards:', visibleCards);

// 5. Click on a UI element
window.yaze.gui.click('overworld_canvas');  // Click by element ID
window.yaze.gui.click({ x: 100, y: 200 });  // Click by coordinates

// 6. Take a screenshot for visual analysis
const screenshot = window.yaze.gui.takeScreenshot();
console.log('Screenshot:', screenshot.dataUrl);

// 7. Stop auto-updates when done
window.yaze.gui.stopAutoUpdate();
```

### Widget Overlay Data Attributes

Each overlay element includes these data attributes for querying:

| Attribute | Description |
|-----------|-------------|
| `data-widget-id` | Unique widget identifier |
| `data-widget-type` | Widget type (button, canvas, input, etc.) |
| `data-label` | Display label/text |
| `data-visible` | "true" or "false" |
| `data-enabled` | "true" or "false" |
| `data-window` | Parent window name |
| `data-x`, `data-y` | Position coordinates |
| `data-width`, `data-height` | Dimensions |

### C++ Integration

The widget overlay system integrates with C++ via:

- **`WasmControlApi::GetVisibleCards()`** - Returns JSON array of visible card IDs
- **`WasmControlApi::GetAvailableCards()`** - Returns full card metadata
- **`WasmControlApi::GetCardsInCategory(category)`** - Returns cards in a category
- **`Module.guiGetUIElementTree()`** - Returns widget tree for overlay sync

---

## Version History

**2.5.0** (2025-11-25)
- Added Agent Discoverability Infrastructure section
- Documented Widget Overlay system (`widget_overlay.js`)
- Documented Canvas Data Attributes for editor state exposure
- Documented Card Registry APIs for programmatic card control

**2.4.0** (2025-11-25)
- Added `yazeDebug.switchToEditorAsync()` - Promise-based editor switching with operation tracking
- Added `yazeDebug.cards` namespace - Show/hide/toggle cards, card groups, category queries
- Added `yazeDebug.sidebar` namespace - Tree view (200px) vs icon mode (48px) control
- Added `yazeDebug.rightPanel` namespace - Properties, agent, proposals, settings, help panels
- New tree view sidebar with hierarchical card navigation
- New selection properties panel for context-aware entity editing
- Supports all 14 editor types for switching

**2.3.0** (2025-11-25)
- Added `window.aiTools` API for Gemini Antigravity AI integration
- New nav bar dropdowns: Editor Switcher, Emulator Controls, Layouts, AI Tools
- Command palette integration for all AI tools
- High-level helper functions with console output for AI readability
- Documentation for nav bar UI elements

**2.2.0** (2025-11-25)
- Added loading progress system documentation
- Added memory configuration section (256MB initial, 1GB max)
- C++ now manages loading indicator via WasmLoadingManager
- Progress updates throughout ROM loading stages
- Fixed loading indicator not being removed after ROM load

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

**Primary WASM Documentation (3 docs total):**

- [WASM Development Guide](./agents/wasm-development-guide.md) - Building, debugging, CMake config, performance, ImGui ID conflict prevention
- [WASM Antigravity Playbook](./agents/wasm-antigravity-playbook.md) - AI agent workflows, Gemini integration, quick start guides
- This API Reference - JavaScript APIs, Agent Discoverability Infrastructure

**General Documentation:**

- [Architecture Documentation](./architecture/README.md) - System design details
- [CI/CD Documentation](./ci-and-testing.md) - Build and test infrastructure

**Archived:** `docs/internal/agents/archive/wasm-docs-2025/` - Historical WASM docs
