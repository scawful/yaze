# G1 - Canvas System and Automation

This document provides a comprehensive guide to the Canvas system in yaze, including its architecture, features, and the powerful automation API that enables programmatic control for the `z3ed` CLI, AI agents, and GUI testing.

## 1. Core Concepts

### Canvas Structure
- **Background**: Drawing surface with border and optional scrolling
- **Content Layer**: Bitmaps, tiles, custom graphics
- **Grid Overlay**: Optional grid with hex labels
- **Interaction Layer**: Hover previews, selection rectangles

### Coordinate Systems
- **Screen Space**: ImGui window coordinates
- **Canvas Space**: Relative to canvas origin (0,0)
- **Tile Space**: Grid-aligned tile indices
- **World Space**: Overworld 4096x4096 large map coordinates

## 2. Canvas API and Usage

### Modern Begin/End Pattern
The recommended way to use the canvas is with the `Begin()`/`End()` pattern, which handles setup and teardown automatically.

```cpp
canvas.Begin(ImVec2(512, 512));
canvas.DrawBitmap(bitmap, 0, 0, 2.0f);
canvas.End();  // Automatic grid + overlay
```

### Feature: Tile Painting
The canvas provides methods for painting single tiles, painting from a tilemap, and painting with solid colors.

```cpp
if (canvas.DrawTilePainter(current_tile_bitmap, 16, 2.0f)) {
  ImVec2 paint_pos = canvas.drawn_tile_position();
  ApplyTileToMap(paint_pos, current_tile_id);
}
```

### Feature: Tile Selection
The canvas supports both single-tile and multi-tile rectangle selection.

```cpp
canvas.DrawSelectRect(current_map_id, 16, 1.0f);

if (canvas.select_rect_active()) {
  const auto& selected_tiles = canvas.selected_tiles();
  // Process selection...
}
```

### Feature: Large Map Support
The canvas can handle large maps with multiple local maps, including boundary clamping to prevent selection wrapping.

```cpp
canvas.SetClampRectToLocalMaps(true);  // Default - prevents wrapping
```

### Feature: Context Menu
The canvas has a customizable context menu.

```cpp
canvas.AddContextMenuItem({
  "My Action",
  [this]() { DoAction(); }
});
```

## 3. Canvas Automation API

The `CanvasAutomationAPI` provides a programmatic interface for controlling canvas operations, enabling scripted editing, AI agent integration, and automated testing.

### Accessing the API
```cpp
auto* api = canvas.GetAutomationAPI();
```

### Tile Operations
```cpp
// Paint a single tile
bool SetTileAt(int x, int y, int tile_id);

// Get the tile ID at a specific location
int GetTileAt(int x, int y) const;

// Paint multiple tiles in a batch
bool SetTiles(const std::vector<std::tuple<int,int,int>>& tiles);
```

### Selection Operations
```cpp
// Select a single tile
void SelectTile(int x, int y);

// Select a rectangular region of tiles
void SelectTileRect(int x1, int y1, int x2, int y2);

// Get the current selection state
SelectionState GetSelection() const;

// Clear the current selection
void ClearSelection();
```

### View Operations
```cpp
// Scroll the canvas to make a tile visible
void ScrollToTile(int x, int y);

// Set the canvas zoom level
void SetZoom(float zoom_level);

// Get the current zoom level
float GetZoom() const;

// Center the canvas view on a specific coordinate
void CenterOn(int x, int y);
```

### Query Operations
```cpp
// Get the dimensions of the canvas in tiles
CanvasDimensions GetDimensions() const;

// Get the currently visible region of the canvas
VisibleRegion GetVisibleRegion() const;

// Check if a tile is currently visible
bool IsTileVisible(int x, int y) const;

// Get the cursor position in logical tile coordinates
ImVec2 GetCursorPosition() const;
```

### Simulation Operations (for GUI Automation)
```cpp
// Simulate a mouse click at a specific tile
void SimulateClick(int x, int y, ImGuiMouseButton button = ImGuiMouseButton_Left);

// Simulate a drag operation between two tiles
void SimulateDrag(int x1, int y1, int x2, int y2);

// Wait for the canvas to finish processing
void WaitForIdle();
```

## 4. `z3ed` CLI Integration

The Canvas Automation API is exposed through the `z3ed` CLI, allowing for scripted overworld editing.

```bash
# Set a tile
z3ed overworld set-tile --map 0 --x 10 --y 10 --tile-id 0x0042 --rom zelda3.sfc

# Get a tile
z3ed overworld get-tile --map 0 --x 10 --y 10 --rom zelda3.sfc

# Select a rectangle
z3ed overworld select-rect --map 0 --x1 5 --y1 5 --x2 15 --y2 15 --rom zelda3.sfc

# Scroll to a tile
z3ed overworld scroll-to --map 0 --x 20 --y 20 --center --rom zelda3.sfc
```

## 5. gRPC Service

The Canvas Automation API is also exposed via a gRPC service, allowing for remote control of the canvas.

**Proto Definition (`protos/canvas_automation.proto`):**
```protobuf
service CanvasAutomation {
  rpc SetTileAt(SetTileRequest) returns (SetTileResponse);
  rpc GetTileAt(GetTileRequest) returns (GetTileResponse);
  rpc SelectTileRect(SelectTileRectRequest) returns (SelectTileRectResponse);
  // ... and so on for all API methods
}
```

This service is hosted by the `UnifiedGRPCServer` in the main yaze application, allowing tools like `grpcurl` or custom clients to interact with the canvas remotely.
