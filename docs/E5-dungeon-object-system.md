# Dungeon Object System

## Overview

The Dungeon Object System provides a comprehensive framework for editing and managing dungeon rooms, objects, and layouts in The Legend of Zelda: A Link to the Past. This system combines real-time visual editing with precise data manipulation to create a powerful dungeon creation and modification toolkit.

## Architecture

### Core Components

The dungeon system is built around several key components that work together to provide a seamless editing experience:

#### 1. DungeonEditor (`src/app/editor/dungeon/dungeon_editor.h`)
The main interface that orchestrates all dungeon editing functionality. It provides:
- **Windowed Canvas System**: Fixed-size canvas that prevents UI layout disruption
- **Tabbed Room Interface**: Multiple rooms can be open simultaneously for easy comparison and editing
- **Integrated Object Placement**: Direct object placement from selector to canvas
- **Real-time Preview**: Live object preview follows mouse cursor during placement

#### 2. DungeonObjectSelector (`src/app/editor/dungeon/dungeon_object_selector.h`)
Combines object browsing and editing in a unified interface:
- **Object Browser**: Visual grid of all available objects with real-time previews
- **Object Editor**: Integrated editing panels for sprites, items, entrances, doors, and chests
- **Callback System**: Notifies main editor when objects are selected for placement

#### 3. DungeonCanvasViewer (`src/app/editor/dungeon/dungeon_canvas_viewer.h`)
Specialized canvas for rendering dungeon rooms:
- **Background Layer Rendering**: Proper BG1/BG2 layer composition
- **Object Rendering**: Cached object rendering with palette support
- **Coordinate System**: Seamless translation between room and canvas coordinates

#### 4. Room Management System (`src/app/zelda3/dungeon/room.h`)
Core data structures for room representation:
- **Room Objects**: Tile-based objects (walls, floors, decorations)
- **Room Layout**: Structural elements and collision data
- **Sprites**: Enemy and NPC placement
- **Entrances/Exits**: Room connections and transitions

## Object Types and Hierarchies

### Room Objects
Room objects are the fundamental building blocks of dungeon rooms. They follow a hierarchical structure:

#### Type 1 Objects (0x00-0xFF)
Basic structural elements:
- **0x10-0x1F**: Wall objects (various types and orientations)
- **0x20-0x2F**: Floor tiles (stone, wood, carpet, etc.)
- **0x30-0x3F**: Decorative elements (torches, statues, pillars)
- **0x40-0x4F**: Interactive elements (switches, blocks)

#### Type 2 Objects (0x100-0x1FF)
Complex multi-tile objects:
- **0x100-0x10F**: Large wall sections
- **0x110-0x11F**: Complex floor patterns
- **0x120-0x12F**: Multi-tile decorations

#### Type 3 Objects (0x200+)
Special dungeon-specific objects:
- **0x200-0x20F**: Boss room elements
- **0x210-0x21F**: Puzzle-specific objects
- **0xF9-0xFA**: Chests (small and large)

### Object Properties
Each object has several key properties:

```cpp
class RoomObject {
  int id_;           // Object type identifier
  int x_, y_;        // Position in room (16x16 tile units)
  int size_;         // Size modifier (affects rendering)
  LayerType layer_; // Rendering layer (0=BG, 1=MID, 2=FG)
  // ... additional properties
};
```

## How Object Placement Works

### Selection Process
1. **Object Browser**: User selects an object from the visual grid
2. **Preview Generation**: Object is rendered with current room palette
3. **Callback Trigger**: Selection notifies main editor via callback
4. **Preview Update**: Main editor receives object and enables placement mode

### Placement Process
1. **Mouse Tracking**: Preview object follows mouse cursor on canvas
2. **Coordinate Translation**: Mouse position converted to room coordinates
3. **Visual Feedback**: Semi-transparent preview shows placement position
4. **Click Placement**: Left-click places object at current position
5. **Room Update**: Object added to room data and cache cleared for redraw

### Code Flow
```cpp
// Object selection in DungeonObjectSelector
if (ImGui::Selectable("", is_selected)) {
  preview_object_ = selected_object;
  object_loaded_ = true;
  
  // Notify main editor
  if (object_selected_callback_) {
    object_selected_callback_(preview_object_);
  }
}

// Object placement in DungeonEditor
void PlaceObjectAtPosition(int room_x, int room_y) {
  auto new_object = preview_object_;
  new_object.x_ = room_x;
  new_object.y_ = room_y;
  new_object.set_rom(rom_);
  new_object.EnsureTilesLoaded();
  
  room.AddTileObject(new_object);
  object_render_cache_.clear(); // Force redraw
}
```

## Rendering Pipeline

### Object Rendering
The system uses a sophisticated rendering pipeline:

1. **Tile Loading**: Object tiles loaded from ROM based on object ID
2. **Palette Application**: Room-specific palette applied to object
3. **Bitmap Generation**: Object rendered to bitmap with proper composition
4. **Caching**: Rendered objects cached for performance
5. **Canvas Drawing**: Bitmap drawn to canvas at correct position

### Performance Optimizations
- **Render Caching**: Objects cached based on ID, position, size, and palette hash
- **Bounds Checking**: Only objects within canvas bounds are rendered
- **Lazy Loading**: Graphics and objects loaded on-demand
- **Palette Hashing**: Efficient cache invalidation when palettes change

## User Interface Components

### Three-Column Layout
The dungeon editor uses a carefully designed three-column layout:

#### Column 1: Room Control Panel (280px fixed)
- **Room Selector**: Browse and select rooms
- **Debug Controls**: Room properties in table format
- **Object Statistics**: Live object counts and cache status

#### Column 2: Windowed Canvas (800px fixed)
- **Tabbed Interface**: Multiple rooms open simultaneously
- **Fixed Dimensions**: Prevents UI layout disruption
- **Real-time Preview**: Object placement preview follows cursor
- **Layer Visualization**: Proper background/foreground rendering

#### Column 3: Object Selector/Editor (stretch)
- **Object Browser Tab**: Visual grid of available objects
- **Object Editor Tab**: Integrated editing for sprites, items, etc.
- **Placement Tools**: Object property editing and placement controls

### Debug and Control Features

#### Room Properties Table
Real-time editing of room attributes:
```
Property    | Value
------------|--------
Room ID     | 0x001 (1)
Layout      | [Hex Input]
Blockset    | [Hex Input]
Spriteset   | [Hex Input]
Palette     | [Hex Input]
Floor 1     | [Hex Input]
Floor 2     | [Hex Input]
Message ID  | [Hex Input]
```

#### Object Statistics
Live feedback on room contents:
- Total objects count
- Layout objects count
- Sprites count
- Chests count
- Cache status and controls

## Integration with ROM Data

### Data Sources
The system integrates with multiple ROM data sources:

#### Room Headers (`0x1F8000`)
- Room layout index
- Blockset and spriteset references
- Palette assignments
- Floor type definitions

#### Object Data
- Object definitions and tile mappings
- Size and layer information
- Interaction properties

#### Graphics Data
- Tile graphics (4bpp SNES format)
- Palette data (15-color palettes)
- Blockset compositions

### Assembly Integration
The system references the US disassembly (`assets/asm/usdasm/`) for:
- Room data structure validation
- Object type definitions
- Memory layout verification
- Data pointer validation

## Comparison with ZScream

### Architectural Differences
YAZE's approach differs from ZScream in several key ways:

#### Component-Based Architecture
- **YAZE**: Modular components with clear separation of concerns
- **ZScream**: More monolithic approach with integrated functionality

#### Real-time Rendering
- **YAZE**: Live object preview with mouse tracking
- **ZScream**: Static preview with separate placement step

#### UI Organization
- **YAZE**: Fixed-width columns prevent layout disruption
- **ZScream**: Resizable panels that can affect overall layout

#### Caching Strategy
- **YAZE**: Sophisticated object render caching with hash-based invalidation
- **ZScream**: Simpler caching approach

### Shared Concepts
Both systems share fundamental concepts:
- Object-based room construction
- Layer-based rendering
- ROM data integration
- Visual object browsing

## Best Practices

### Performance
1. **Use Render Caching**: Don't clear cache unnecessarily
2. **Bounds Checking**: Only render visible objects
3. **Lazy Loading**: Load graphics and objects on-demand
4. **Efficient Callbacks**: Minimize callback frequency

### Code Organization
1. **Separation of Concerns**: Keep UI, data, and rendering separate
2. **Clear Interfaces**: Use callbacks for component communication
3. **Error Handling**: Validate ROM data and handle errors gracefully
4. **Memory Management**: Clean up resources properly

### User Experience
1. **Visual Feedback**: Provide clear object placement preview
2. **Consistent Layout**: Use fixed dimensions for stable UI
3. **Contextual Information**: Show relevant object properties
4. **Efficient Workflow**: Minimize clicks for common operations

## Future Enhancements

### Planned Features
1. **Drag and Drop**: Direct object dragging from selector to canvas
2. **Multi-Selection**: Select and manipulate multiple objects
3. **Copy/Paste**: Copy object configurations between rooms
4. **Undo/Redo**: Full edit history management
5. **Template System**: Save and load room templates

### Technical Improvements
1. **GPU Acceleration**: Move rendering to GPU for better performance
2. **Advanced Caching**: Predictive loading and intelligent cache management
3. **Background Processing**: Asynchronous ROM data loading
4. **Memory Optimization**: Reduce memory footprint for large dungeons

This documentation provides a comprehensive understanding of how the YAZE dungeon object system works, from high-level architecture to low-level implementation details. The system is designed to be both powerful for advanced users and accessible for newcomers to dungeon editing.
