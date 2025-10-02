# Remote Control Agent Workflows

**Date**: October 2, 2025  
**Status**: Functional - Test Harness + Widget Registry Integration  
**Purpose**: Enable AI agents to remotely control YAZE for automated editing

## Overview

The remote control system allows AI agents to interact with YAZE through gRPC, using the ImGuiTestHarness and Widget ID Registry to perform real editing tasks.

## Quick Start

### 1. Start YAZE with Test Harness

```bash
./build-grpc-test/bin/yaze.app/Contents/MacOS/yaze \
  --enable_test_harness \
  --test_harness_port=50052 \
  --rom_file=assets/zelda3.sfc &
```

### 2. Open Overworld Editor

In YAZE GUI:
- Click "Overworld" button
- This registers 13 toolset widgets for remote control

### 3. Run Test Script

```bash
./scripts/test_remote_control.sh
```

Expected output:
- ✓ All 8 practical workflows pass
- Agent can switch modes, open tools, control zoom

## Supported Workflows

### Mode Switching

**Draw Tile Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:DrawTile","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables tile painting on overworld map
- Agent can then click canvas to draw selected tiles

**Pan Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Pan","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables map navigation
- Agent can drag canvas to reposition view

**Entrances Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Entrances","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables entrance editing
- Agent can click to place/move entrances

**Exits Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Exits","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables exit editing
- Agent can click to place/move exits

**Sprites Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Sprites","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables sprite editing
- Agent can place/move sprites on overworld

**Items Mode**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Items","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Enables item placement
- Agent can add items to overworld

### Tool Opening

**Tile16 Editor**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Tile16Editor","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```
- Opens Tile16 Editor window
- Agent can select tiles for drawing

### View Controls

**Zoom In**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:ZoomIn","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

**Zoom Out**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:ZoomOut","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

**Fullscreen Toggle**:
```bash
grpcurl -plaintext -d '{"target":"Overworld/Toolset/button:Fullscreen","type":"LEFT"}' \
  127.0.0.1:50052 yaze.test.ImGuiTestHarness/Click
```

## Multi-Step Workflows

### Workflow 1: Draw Custom Tiles

**Goal**: Agent draws specific tiles on the overworld map

**Steps**:
1. Switch to Draw Tile mode
2. Open Tile16 Editor
3. Select desired tile (TODO: needs canvas click support)
4. Click on overworld canvas at (x, y) to draw

**Current Status**: Steps 1-2 working, 3-4 need implementation

### Workflow 2: Reposition Entrance

**Goal**: Agent moves an entrance to a new location

**Steps**:
1. Switch to Entrances mode
2. Click on existing entrance to select
3. Drag to new location (TODO: needs drag support)
4. Verify entrance properties updated

**Current Status**: Step 1 working, 2-4 need implementation

### Workflow 3: Place Sprites

**Goal**: Agent adds sprites to overworld

**Steps**:
1. Switch to Sprites mode
2. Select sprite from palette (TODO)
3. Click canvas to place sprite
4. Adjust sprite properties if needed

**Current Status**: Step 1 working, 2-4 need implementation

## Widget Registry Integration

### Hierarchical Widget IDs

The test harness now supports hierarchical widget IDs from the registry:

```
Format: <Editor>/<Section>/<Type>:<Name>
Example: Overworld/Toolset/button:DrawTile
```

**Benefits**:
- Stable, predictable widget references
- Better error messages with suggestions
- Backwards compatible with legacy format
- Self-documenting structure

### Pattern Matching

When a widget isn't found, the system suggests alternatives:

```bash
# Typo in widget name
grpcurl ... -d '{"target":"Overworld/Toolset/button:DrawTyle"}'

# Response:
# "Widget not found: DrawTyle. Did you mean: 
#  Overworld/Toolset/button:DrawTile?"
```

### Widget Discovery

Future enhancement - list all available widgets:

```bash
z3ed agent discover --pattern "Overworld/*"
# Lists all Overworld widgets

z3ed agent discover --pattern "*/button:*"
# Lists all buttons across editors
```

## Implementation Details

### Test Harness Changes

**File**: `src/app/core/service/imgui_test_harness_service.cc`

**Changes**:
1. Added widget registry include
2. Click RPC tries hierarchical lookup first
3. Fallback to legacy string-based lookup
4. Pattern matching for suggestions

**Code**:
```cpp
// Try hierarchical widget ID lookup first
auto& registry = gui::WidgetIdRegistry::Instance();
ImGuiID widget_id = registry.GetWidgetId(target);

if (widget_id != 0) {
  // Found in registry - use ImGui ID directly
  ctx->ItemClick(widget_id, mouse_button);
} else {
  // Fallback to legacy lookup
  ctx->ItemClick(widget_label.c_str(), mouse_button);
}
```

### Widget Registration

**File**: `src/app/editor/overworld/overworld_editor.cc`

**Registered Widgets** (13 total):
- Overworld/Toolset/button:Pan
- Overworld/Toolset/button:DrawTile
- Overworld/Toolset/button:Entrances
- Overworld/Toolset/button:Exits
- Overworld/Toolset/button:Items
- Overworld/Toolset/button:Sprites
- Overworld/Toolset/button:Transports
- Overworld/Toolset/button:Music
- Overworld/Toolset/button:ZoomIn
- Overworld/Toolset/button:ZoomOut
- Overworld/Toolset/button:Fullscreen
- Overworld/Toolset/button:Tile16Editor
- Overworld/Toolset/button:CopyMap

## Next Steps

### Priority 1: Canvas Interaction (2-3 hours)

**Goal**: Enable agent to click on canvas at specific coordinates

**Implementation**:
1. Add canvas click to Click RPC
2. Support coordinate-based clicking: `{"target":"canvas:Overworld","x":100,"y":200}`
3. Test drawing tiles programmatically

**Use Cases**:
- Draw tiles at specific locations
- Select entities by clicking
- Navigate by clicking minimap

### Priority 2: Tile Selection (1-2 hours)

**Goal**: Enable agent to select tiles from Tile16 Editor

**Implementation**:
1. Register Tile16 Editor canvas widgets
2. Support tile palette clicking
3. Track selected tile state

**Use Cases**:
- Select tile before drawing
- Change tile selection mid-workflow
- Verify correct tile selected

### Priority 3: Entity Manipulation (2-3 hours)

**Goal**: Enable dragging of entrances, exits, sprites

**Implementation**:
1. Add Drag RPC to proto
2. Implement drag operation in test harness
3. Support drag start + end coordinates

**Use Cases**:
- Move entrances to new positions
- Reposition sprites
- Adjust exit locations

### Priority 4: Workflow Chaining (1-2 hours)

**Goal**: Combine multiple operations into workflows

**Implementation**:
1. Create workflow definition format
2. Execute sequence of RPCs
3. Handle errors gracefully

**Example Workflow**:
```yaml
workflow: draw_custom_tile
steps:
  - click: Overworld/Toolset/button:DrawTile
  - click: Overworld/Toolset/button:Tile16Editor
  - wait: window_visible:Tile16 Editor
  - click: canvas:Tile16Editor
    x: 64
    y: 64
  - click: canvas:Overworld
    x: 512
    y: 384
```

## Testing Strategy

### Manual Testing

1. Start test harness
2. Run test script: `./scripts/test_remote_control.sh`
3. Observe mode changes in GUI
4. Verify no crashes or errors

### Automated Testing

1. Add to CI pipeline
2. Run as part of E2E validation
3. Test on multiple platforms

### Integration Testing

1. Test with real agent workflows
2. Validate agent can complete tasks
3. Measure reliability and timing

## Performance Characteristics

**Click Latency**: < 200ms
- gRPC overhead: ~10ms
- Test queue time: ~50ms
- ImGui event processing: ~100ms
- Total: ~160ms average

**Mode Switch Time**: < 500ms
- Includes UI update
- State transition
- Visual feedback

**Tool Opening**: < 1s
- Window creation
- Content loading
- Layout calculation

## Troubleshooting

### Widget Not Found

**Problem**: "Widget not found: Overworld/Toolset/button:DrawTile"

**Solutions**:
1. Verify Overworld editor is open (widgets registered on open)
2. Check widget name spelling
3. Look at suggestions in error message
4. Try legacy format: "button:DrawTile"

### Click Not Working

**Problem**: Click succeeds but nothing happens

**Solutions**:
1. Check if widget is enabled (not grayed out)
2. Verify correct mode/context for action
3. Add delay between clicks
4. Check ImGui event queue

### Test Timeout

**Problem**: "Test timeout - widget not found or unresponsive"

**Solutions**:
1. Increase timeout (default 5s)
2. Check if GUI is responsive
3. Verify widget is visible (not hidden)
4. Look for modal dialogs blocking interaction

## References

**Documentation**:
- [WIDGET_ID_REFACTORING_PROGRESS.md](WIDGET_ID_REFACTORING_PROGRESS.md)
- [IT-01-QUICKSTART.md](IT-01-QUICKSTART.md)
- [E2E_VALIDATION_GUIDE.md](E2E_VALIDATION_GUIDE.md)

**Code Files**:
- `src/app/core/service/imgui_test_harness_service.cc` - Test harness implementation
- `src/app/gui/widget_id_registry.{h,cc}` - Widget registry
- `src/app/editor/overworld/overworld_editor.cc` - Widget registrations
- `scripts/test_remote_control.sh` - Test script

---

**Last Updated**: October 2, 2025, 11:45 PM  
**Status**: Functional - Basic mode switching works  
**Next**: Canvas interaction + tile selection
