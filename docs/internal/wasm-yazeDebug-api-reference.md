# Yaze WASM JavaScript API Reference

> **Note**: For a general debugging walkthrough, see the [WASM Debugging Guide](wasm-debugging-guide.md).

## Overview

The yaze WASM build exposes a comprehensive set of JavaScript APIs for programmatic control and data access. These APIs are organized into six main namespaces:

- **`window.yaze.control`** - Editor control and manipulation
- **`window.yaze.editor`** - Query current editor state
- **`window.yaze.data`** - Read-only access to ROM data
- **`window.yaze.gui`** - GUI automation and interaction
- **`window.yaze.agent`** - AI agent integration (chat, proposals, configuration)
- **`window.yazeDebug`** - Debug utilities and diagnostics
- **`window.aiTools`** - High-level AI assistant tools (Gemini Antigravity)

## API Version

- Version: 2.5.0
- Last Updated: 2025-11-27
- Capabilities: `['palette', 'arena', 'graphics', 'timeline', 'pixel-inspector', 'rom', 'overworld', 'emulator', 'editor', 'control', 'data', 'gui', 'agent', 'loading-progress', 'ai-tools', 'async-editor-switch', 'card-groups', 'tree-sidebar', 'properties-panel']`

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

## window.yaze.agent - AI Agent Integration API

Provides programmatic control over the AI agent system from JavaScript. Enables browser-based AI agents to interact with the built-in chat, manage proposals, and configure AI providers.

### Utility

#### isReady()

```javascript
const ready = window.yaze.agent.isReady()
// Returns: boolean
```

Checks if the agent system is initialized and ready for use. Requires ROM to be loaded.

### Chat Operations

#### sendMessage(message)

```javascript
const result = window.yaze.agent.sendMessage("Help me analyze dungeon room 0")
```

Send a message to the AI agent chat.

**Parameters:**
- `message` (string): User message to send

**Returns:**
```json
{
  "success": true,
  "status": "queued",
  "message": "Help me analyze dungeon room 0"
}
```

#### getChatHistory()

```javascript
const history = window.yaze.agent.getChatHistory()
```

Get the chat message history.

**Returns:**
```json
[
  {"role": "user", "content": "Hello"},
  {"role": "assistant", "content": "Hi! How can I help?"}
]
```

### Configuration

#### getConfig()

```javascript
const config = window.yaze.agent.getConfig()
```

Get current agent configuration.

**Returns:**
```json
{
  "provider": "mock",
  "model": "",
  "ollama_host": "http://localhost:11434",
  "verbose": false,
  "show_reasoning": true,
  "max_tool_iterations": 4
}
```

#### setConfig(config)

```javascript
window.yaze.agent.setConfig({
  provider: "ollama",
  model: "llama3",
  ollama_host: "http://localhost:11434",
  verbose: true
})
```

Update agent configuration.

**Parameters:**
- `config` (object): Configuration object with optional fields:
  - `provider`: AI provider ID ("mock", "ollama", "gemini")
  - `model`: Model name/ID
  - `ollama_host`: Ollama server URL
  - `verbose`: Enable verbose logging
  - `show_reasoning`: Show AI reasoning in responses
  - `max_tool_iterations`: Max tool call iterations

**Returns:**
```json
{
  "success": true
}
```

#### getProviders()

```javascript
const providers = window.yaze.agent.getProviders()
```

Get list of available AI providers.

**Returns:**
```json
[
  {
    "id": "mock",
    "name": "Mock Provider",
    "description": "Testing provider that echoes messages"
  },
  {
    "id": "ollama",
    "name": "Ollama",
    "description": "Local Ollama server",
    "requires_host": true
  },
  {
    "id": "gemini",
    "name": "Google Gemini",
    "description": "Google's Gemini API",
    "requires_api_key": true
  }
]
```

### Proposal Management

#### getProposals()

```javascript
const proposals = window.yaze.agent.getProposals()
```

Get list of pending/recent code proposals.

**Returns:**
```json
[
  {
    "id": "proposal-123",
    "status": "pending",
    "summary": "Modify room palette"
  }
]
```

#### acceptProposal(proposalId)

```javascript
window.yaze.agent.acceptProposal("proposal-123")
```

Accept a proposal and apply its changes.

#### rejectProposal(proposalId)

```javascript
window.yaze.agent.rejectProposal("proposal-123")
```

Reject a proposal.

---

## window.yazeDebug - Debug Utilities

Low-level debugging tools for the WASM environment.

### dumpAll()

```javascript
window.yazeDebug.dumpAll()
```

Dump full application state to console.

### graphics.getDiagnostics()

```javascript
window.yazeDebug.graphics.getDiagnostics()
```

Get graphics subsystem diagnostics.

### memory.getUsage()

```javascript
window.yazeDebug.memory.getUsage()
```

Get current memory usage statistics.

---

## window.aiTools - High-Level Assistant Tools

Helper functions for the Gemini Antigravity agent.

### getAppState()

```javascript
window.aiTools.getAppState()
```

Get high-level application state summary.

### getEditorState()

```javascript
window.aiTools.getEditorState()
```

Get detailed state of the active editor.

### getVisibleCards()

```javascript
window.aiTools.getVisibleCards()
```

List currently visible UI cards.

### getAvailableCards()

```javascript
window.aiTools.getAvailableCards()
```

List all available UI cards.

### showCard(cardId)

```javascript
window.aiTools.showCard(cardId)
```

Show a card (wrapper for `window.yaze.control.openCard`).

### hideCard(cardId)

```javascript
window.aiTools.hideCard(cardId)
```

Hide a card.

### navigateTo(target)

```javascript
window.aiTools.navigateTo(target)
```

Navigate to a specific editor or view.

### getRoomData(roomId)

```javascript
window.aiTools.getRoomData(roomId)
```

Get dungeon room data.

### getMapData(mapId)

```javascript
window.aiTools.getMapData(mapId)
```

Get overworld map data.

### dumpAPIReference()

```javascript
window.aiTools.dumpAPIReference()
```

Dump this API reference to the console.
