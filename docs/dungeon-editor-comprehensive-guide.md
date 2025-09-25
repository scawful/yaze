# Dungeon Editor Guide

## Overview

The Yaze Dungeon Editor is a comprehensive tool for editing Zelda 3: A Link to the Past dungeon rooms, objects, sprites, items, entrances, doors, and chests. It provides an integrated editing experience with real-time rendering, coordinate system management, and advanced features for dungeon modification.

## Architecture

### Core Components

#### 1. DungeonEditorSystem
- **Purpose**: Central coordinator for all dungeon editing operations
- **Location**: `src/app/zelda3/dungeon/dungeon_editor_system.h/cc`
- **Features**:
  - Room management (loading, saving, creating, deleting)
  - Sprite management (enemies, NPCs, interactive objects)
  - Item management (keys, hearts, rupees, etc.)
  - Entrance/exit management (room connections)
  - Door management (locked doors, key requirements)
  - Chest management (treasure placement)
  - Undo/redo system
  - Event callbacks for real-time updates

#### 2. DungeonObjectEditor
- **Purpose**: Specialized editor for room objects (walls, floors, decorations)
- **Location**: `src/app/zelda3/dungeon/dungeon_object_editor.h/cc`
- **Features**:
  - Object placement and editing
  - Layer management (BG1, BG2, BG3)
  - Object size editing with scroll wheel
  - Collision detection and validation
  - Selection and multi-selection
  - Grid snapping
  - Real-time preview

#### 3. ObjectRenderer
- **Purpose**: High-performance rendering system for dungeon objects
- **Location**: `src/app/zelda3/dungeon/object_renderer.h/cc`
- **Features**:
  - Graphics cache for performance optimization
  - Memory pool management
  - Performance monitoring and statistics
  - Object parsing from ROM data
  - Palette support and color management
  - Batch rendering for efficiency

#### 4. DungeonEditor (UI Layer)
- **Purpose**: User interface and interaction handling
- **Location**: `src/app/editor/dungeon/dungeon_editor.h/cc`
- **Features**:
  - Integrated tabbed interface
  - Canvas-based room editing
  - Coordinate system management
  - Object preview system
  - Real-time rendering
  - Compact editing panels

## Coordinate System

### Room Coordinates vs Canvas Coordinates

The dungeon editor uses a two-tier coordinate system:

1. **Room Coordinates**: 16x16 tile units (as used in the ROM)
2. **Canvas Coordinates**: Pixel coordinates for rendering

#### Conversion Functions

```cpp
// Convert room coordinates to canvas coordinates
std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const {
  return {room_x * 16, room_y * 16};
}

// Convert canvas coordinates to room coordinates  
std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const {
  return {canvas_x / 16, canvas_y / 16};
}

// Check if coordinates are within canvas bounds
bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
```

### Coordinate System Features

- **Automatic Bounds Checking**: Objects outside visible canvas area are culled
- **Scrolling Support**: Canvas handles scrolling internally with proper coordinate transformation
- **Grid Alignment**: 16x16 pixel grid for precise object placement
- **Margin Support**: Configurable margins for partial object visibility

## Object Rendering System

### Object Types

The system supports three main object subtypes based on ROM structure:

1. **Subtype 1** (0x00-0xFF): Standard room objects (walls, floors, decorations)
2. **Subtype 2** (0x100-0x1FF): Interactive objects (doors, switches, chests)
3. **Subtype 3** (0x200+): Special objects (stairs, warps, bosses)

### Rendering Pipeline

1. **Object Loading**: Objects are loaded from ROM data using `LoadObjects()`
2. **Tile Parsing**: Object tiles are parsed using `ObjectParser`
3. **Graphics Caching**: Frequently used graphics are cached for performance
4. **Palette Application**: SNES palettes are applied to object graphics
5. **Canvas Rendering**: Objects are rendered to canvas with proper coordinate transformation

### Performance Optimizations

- **Graphics Cache**: Reduces redundant graphics sheet loading
- **Memory Pool**: Efficient memory allocation for rendering
- **Batch Rendering**: Multiple objects rendered in single pass
- **Bounds Culling**: Objects outside visible area are skipped
- **Cache Invalidation**: Smart cache management based on palette changes

## User Interface

### Integrated Editing Panels

The dungeon editor features a consolidated interface with:

#### Main Canvas
- **Room Visualization**: Real-time room rendering with background layers
- **Object Display**: Objects rendered with proper positioning and sizing
- **Interactive Editing**: Click-to-select, drag-to-move, scroll-to-resize
- **Grid Overlay**: Optional grid display for precise positioning
- **Coordinate Display**: Real-time coordinate information

#### Compact Editing Panels

1. **Object Editor**
   - Mode selection (Select, Insert, Edit, Delete)
   - Layer management (BG1, BG2, BG3)
   - Object type selection
   - Size editing with scroll wheel
   - Configuration options (snap to grid, show grid)

2. **Sprite Editor**
   - Sprite placement and management
   - Enemy and NPC configuration
   - Layer assignment
   - Quick sprite addition

3. **Item Editor**
   - Item placement (keys, hearts, rupees)
   - Hidden item configuration
   - Item type selection
   - Room assignment

4. **Entrance Editor**
   - Room connection management
   - Bidirectional connection support
   - Position configuration
   - Connection validation

5. **Door Editor**
   - Door placement and configuration
   - Lock status management
   - Key requirement setup
   - Direction and target room assignment

6. **Chest Editor**
   - Treasure chest placement
   - Item and quantity configuration
   - Big chest support
   - Opened status tracking

7. **Properties Editor**
   - Room metadata management
   - Dungeon settings
   - Music and ambient sound configuration
   - Boss room and save room flags

### Object Preview System

- **Real-time Preview**: Objects are previewed in the canvas as they're selected
- **Centered Display**: Preview objects are centered in the canvas for optimal viewing
- **Palette Support**: Previews use current palette settings
- **Information Display**: Object properties are shown in preview window

## Integration with ZScream

The dungeon editor is designed to be compatible with ZScream C# patterns:

### Room Loading
- Uses same room loading patterns as ZScream
- Compatible with ZScream room data structures
- Supports ZScream room naming conventions

### Object Parsing
- Follows ZScream object parsing logic
- Compatible with ZScream object type definitions
- Supports ZScream size encoding

### Coordinate System
- Matches ZScream coordinate conventions
- Uses same tile size calculations
- Compatible with ZScream positioning logic

## Testing and Validation

### Integration Tests

The system includes comprehensive integration tests:

1. **Basic Object Rendering**: Tests fundamental object rendering functionality
2. **Multi-Palette Rendering**: Tests rendering with different palettes
3. **Real Room Object Rendering**: Tests with actual ROM room data
4. **Disassembly Room Validation**: Tests specific rooms from disassembly
5. **Performance Testing**: Measures rendering performance and memory usage
6. **Cache Effectiveness**: Tests graphics cache performance
7. **Error Handling**: Tests error conditions and edge cases

### Test Data

Tests use real ROM data from `build/bin/zelda3.sfc`:
- **Room 0x0000**: Ganon's room (from disassembly)
- **Room 0x0002, 0x0012**: Sewer rooms (from disassembly)
- **Room 0x0020**: Agahnim's tower (from disassembly)
- **Additional rooms**: 0x0001, 0x0010, 0x0033, 0x005A

### Performance Benchmarks

- **Rendering Time**: < 500ms for 100 objects
- **Memory Usage**: < 100MB for large object sets
- **Cache Hit Rate**: Optimized for frequent object access
- **Coordinate Conversion**: O(1) coordinate transformation

## Usage Examples

### Basic Object Editing

```cpp
// Load a room
auto room_result = dungeon_editor_system_->GetRoom(0x0000);

// Add an object
auto status = object_editor_->InsertObject(5, 5, 0x10, 0x12, 0);
// Parameters: x, y, object_type, size, layer

// Render objects
auto result = object_renderer_->RenderObjects(objects, palette);
```

### Coordinate Conversion

```cpp
// Convert room coordinates to canvas coordinates
auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(room_x, room_y);

// Check if coordinates are within bounds
if (IsWithinCanvasBounds(canvas_x, canvas_y)) {
    // Render object at this position
}
```

### Object Preview

```cpp
// Create preview object
auto preview_object = zelda3::RoomObject(id, 8, 8, 0x12, 0);
preview_object.set_rom(rom_);
preview_object.EnsureTilesLoaded();

// Render preview
auto result = object_renderer_->RenderObject(preview_object, palette);
```

## Configuration Options

### Editor Configuration

```cpp
struct EditorConfig {
  bool snap_to_grid = true;
  int grid_size = 16;
  bool show_grid = true;
  bool show_preview = true;
  bool auto_save = false;
  int auto_save_interval = 300;
  bool validate_objects = true;
  bool show_collision_bounds = false;
};
```

### Performance Configuration

```cpp
// Object renderer settings
object_renderer_->SetCacheSize(100);
object_renderer_->EnablePerformanceMonitoring(true);

// Canvas settings
canvas_.SetCanvasSize(ImVec2(512, 512));
canvas_.set_draggable(true);
```

## Troubleshooting

### Common Issues

1. **Objects Not Displaying**
   - Check if ROM is loaded
   - Verify object tiles are loaded with `EnsureTilesLoaded()`
   - Check coordinate bounds with `IsWithinCanvasBounds()`

2. **Coordinate Misalignment**
   - Use coordinate conversion functions
   - Check canvas scrolling settings
   - Verify grid alignment

3. **Performance Issues**
   - Enable graphics caching
   - Check memory usage with `GetMemoryUsage()`
   - Monitor performance stats with `GetPerformanceStats()`

4. **Preview Not Showing**
   - Verify object is within canvas bounds
   - Check palette is properly set
   - Ensure object has valid tiles

### Debug Information

The system provides comprehensive debug information:
- Object count and statistics
- Cache hit/miss rates
- Memory usage tracking
- Performance metrics
- Coordinate system validation

## Future Enhancements

### Planned Features

1. **Advanced Object Editing**
   - Multi-object selection and manipulation
   - Object grouping and layers
   - Advanced collision detection

2. **Enhanced Rendering**
   - Real-time lighting effects
   - Animation support
   - Advanced shader effects

3. **Improved UX**
   - Keyboard shortcuts
   - Context menus
   - Undo/redo visualization

4. **Integration Features**
   - ZScream project import/export
   - Collaborative editing
   - Version control integration

## Conclusion

The Yaze Dungeon Editor provides a comprehensive, high-performance solution for editing Zelda 3 dungeon rooms. With its integrated interface, robust coordinate system, and advanced rendering capabilities, it offers both novice and expert users the tools needed to create and modify dungeon content effectively.

The system's compatibility with ZScream patterns and comprehensive testing ensure reliability and consistency with existing tools, while its modern architecture provides a foundation for future enhancements and features.
