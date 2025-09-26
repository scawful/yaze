# Dungeon Editor Design Plan

## Overview

This document provides a comprehensive guide for future developers working on the Zelda 3 Dungeon Editor system. The dungeon editor has been refactored into a modular, component-based architecture that separates concerns and improves maintainability.

## Architecture Overview

### Core Components

The dungeon editor system consists of several key components:

1. **DungeonEditor** - Main orchestrator class that manages the overall editor state
2. **DungeonRoomSelector** - Handles room and entrance selection UI
3. **DungeonCanvasViewer** - Manages the main canvas rendering and room display
4. **DungeonObjectSelector** - Provides object selection, editing panels, and tile graphics
5. **ObjectRenderer** - Core rendering engine for dungeon objects
6. **DungeonEditorSystem** - High-level system for managing dungeon editing operations

### File Structure

```
src/app/editor/dungeon/
├── dungeon_editor.h/cc              # Main editor orchestrator
├── dungeon_room_selector.h/cc       # Room/entrance selection component
├── dungeon_canvas_viewer.h/cc       # Canvas rendering component
├── dungeon_object_selector.h/cc     # Object editing component
└── dungeon_editor_system.h/cc       # Core editing system

src/app/zelda3/dungeon/
├── object_renderer.h/cc             # Object rendering engine
├── dungeon_object_editor.h/cc       # Object editing logic
├── room.h/cc                        # Room data structures
├── room_object.h/cc                 # Object data structures
└── room_entrance.h/cc               # Entrance data structures
```

## Component Responsibilities

### DungeonEditor (Main Orchestrator)

**Responsibilities:**
- Manages overall editor state and ROM data
- Coordinates between UI components
- Handles data initialization and propagation
- Implements the 3-column layout (Room Selector | Canvas | Object Selector)

**Key Methods:**
- `UpdateDungeonRoomView()` - Main UI update loop
- `Load()` - Initialize editor with ROM data
- `set_rom()` - Update ROM reference across components

### DungeonRoomSelector

**Responsibilities:**
- Room selection and navigation
- Entrance selection and editing
- Active room management
- Room list display with names

**Key Methods:**
- `Draw()` - Main rendering method
- `DrawRoomSelector()` - Room list and selection
- `DrawEntranceSelector()` - Entrance editing interface
- `set_rom()`, `set_rooms()`, `set_entrances()` - Data access methods

### DungeonCanvasViewer

**Responsibilities:**
- Main canvas rendering and display
- Room graphics loading and management
- Object rendering with proper coordinates
- Background layer management
- Coordinate conversion (room ↔ canvas)

**Key Methods:**
- `Draw(int room_id)` - Main canvas rendering
- `LoadAndRenderRoomGraphics()` - Graphics loading
- `RenderObjectInCanvas()` - Object rendering
- `RoomToCanvasCoordinates()` - Coordinate conversion
- `RenderRoomBackgroundLayers()` - Background rendering

### DungeonObjectSelector

**Responsibilities:**
- Object selection and preview
- Tile graphics display
- Compact editing panels for all editor modes
- Object renderer integration

**Key Methods:**
- `Draw()` - Main rendering with tabbed interface
- `DrawRoomGraphics()` - Tile graphics display
- `DrawIntegratedEditingPanels()` - Editing interface
- `DrawCompactObjectEditor()` - Object editing controls
- `DrawCompactSpriteEditor()` - Sprite editing controls
- Similar methods for Items, Entrances, Doors, Chests, Properties

## Data Flow

### Initialization Flow

1. **ROM Loading**: `DungeonEditor::Load()` is called with ROM data
2. **Component Setup**: ROM and data pointers are propagated to all components
3. **Graphics Initialization**: Room graphics are loaded and cached
4. **UI State Setup**: Active rooms, palettes, and editor modes are initialized

### Runtime Data Flow

1. **User Interaction**: User selects rooms, objects, or editing modes
2. **State Updates**: Components update their internal state
3. **Data Propagation**: Changes are communicated between components
4. **Rendering**: All components re-render with updated data

### Key Data Structures

```cpp
// Main editor state
std::array<zelda3::Room, 0x128> rooms_;
std::array<zelda3::RoomEntrance, 0x8C> entrances_;
ImVector<int> active_rooms_;
gfx::PaletteGroup current_palette_group_;

// Component instances
DungeonRoomSelector room_selector_;
DungeonCanvasViewer canvas_viewer_;
DungeonObjectSelector object_selector_;
```

## Integration Patterns

### Component Communication

Components communicate through:
1. **Direct method calls** - Parent calls child methods
2. **Data sharing** - Shared pointers to common data structures
3. **Event propagation** - State changes trigger updates

### ROM Data Management

```cpp
// ROM propagation pattern
void DungeonEditor::set_rom(Rom* rom) {
    rom_ = rom;
    room_selector_.set_rom(rom);
    canvas_viewer_.SetRom(rom);
    object_selector_.SetRom(rom);
}
```

### State Synchronization

Components maintain their own state but receive updates from the main editor:
- Room selection state is managed by `DungeonRoomSelector`
- Canvas rendering state is managed by `DungeonCanvasViewer`
- Object editing state is managed by `DungeonObjectSelector`

## UI Layout Architecture

### 3-Column Layout

The main editor uses a 3-column ImGui table layout:

```cpp
if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Room/Entrance Selector", ImGuiTableColumnFlags_WidthFixed, 250);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Object Selector/Editor", ImGuiTableColumnFlags_WidthFixed, 300);
    
    // Column 1: Room Selector
    TableNextColumn();
    room_selector_.Draw();
    
    // Column 2: Canvas
    TableNextColumn();
    canvas_viewer_.Draw(current_room);
    
    // Column 3: Object Selector
    TableNextColumn();
    object_selector_.Draw();
}
```

### Component Internal Layout

Each component manages its own internal layout:
- **DungeonRoomSelector**: Tabbed interface (Rooms | Entrances)
- **DungeonCanvasViewer**: Canvas with controls and debug popup
- **DungeonObjectSelector**: Tabbed interface (Graphics | Editor)

## Coordinate System

### Room Coordinates vs Canvas Coordinates

- **Room Coordinates**: 16x16 tile units (0-15 for a standard room)
- **Canvas Coordinates**: Pixel coordinates for rendering
- **Conversion**: `RoomToCanvasCoordinates(x, y) = (x * 16, y * 16)`

### Bounds Checking

All rendering operations include bounds checking:
```cpp
bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;
```

## Error Handling & Validation

### ROM Validation

All components validate ROM state before operations:
```cpp
if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
}
```

### Bounds Validation

Graphics operations include bounds checking:
```cpp
if (room_id < 0 || room_id >= rooms_->size()) {
    return; // Skip invalid operations
}
```

## Performance Considerations

### Caching Strategy

- **Object Render Cache**: Cached rendered bitmaps to avoid re-rendering
- **Graphics Cache**: Cached graphics sheets for frequently accessed data
- **Memory Pool**: Efficient memory allocation for temporary objects

### Rendering Optimization

- **Viewport Culling**: Objects outside visible area are not rendered
- **Lazy Loading**: Graphics are loaded only when needed
- **Selective Updates**: Only changed components re-render

## Testing Strategy

### Integration Tests

The system includes comprehensive integration tests:
- `dungeon_object_renderer_integration_test.cc` - Core rendering tests
- `dungeon_editor_system_integration_test.cc` - System integration tests
- `dungeon_object_renderer_mock_test.cc` - Mock ROM testing

### Test Categories

1. **Real ROM Tests**: Tests with actual Zelda 3 ROM data
2. **Mock ROM Tests**: Tests with simulated ROM data
3. **Performance Tests**: Rendering performance benchmarks
4. **Error Handling Tests**: Validation and error recovery

## Future Development Guidelines

### Adding New Features

1. **Identify Component**: Determine which component should handle the feature
2. **Extend Interface**: Add necessary methods to component header
3. **Implement Logic**: Add implementation in component source file
4. **Update Integration**: Modify main editor to use new functionality
5. **Add Tests**: Create tests for new functionality

### Component Extension Patterns

```cpp
// Adding new data access method
void Component::SetNewData(const NewDataType& data) { 
    new_data_ = data; 
}

// Adding new rendering method
void Component::DrawNewFeature() {
    // Implementation
}

// Adding to main Draw method
void Component::Draw() {
    // Existing code
    DrawNewFeature();
}
```

### Data Flow Extension

When adding new data types:
1. Add to main editor state
2. Create setter methods in relevant components
3. Update initialization in `Load()` method
4. Add to `set_rom()` propagation if ROM-dependent

### UI Layout Extension

For new UI elements:
1. Determine placement (new tab, new panel, etc.)
2. Follow existing ImGui patterns
3. Maintain consistent spacing and styling
4. Add to appropriate component's Draw method

## Common Pitfalls & Solutions

### Memory Management

- **Issue**: Dangling pointers to ROM data
- **Solution**: Always validate ROM state before use

### Coordinate System

- **Issue**: Objects rendering at wrong positions
- **Solution**: Use coordinate conversion helper methods

### State Synchronization

- **Issue**: Components showing stale data
- **Solution**: Ensure data propagation in setter methods

### Performance Issues

- **Issue**: Slow rendering with many objects
- **Solution**: Implement viewport culling and caching

## Debugging Tools

### Debug Popup

The canvas viewer includes a comprehensive debug popup with:
- Object statistics and metadata
- Cache information
- Performance metrics
- Object type breakdowns

### Logging

Key operations include logging for debugging:
```cpp
std::cout << "Loading room graphics for room " << room_id << std::endl;
```

## Build Integration

### CMake Configuration

New components are automatically included via:
```cmake
# In CMakeLists.txt
file(GLOB YAZE_SRC_FILES "src/app/editor/dungeon/*.cc")
```

### Dependencies

Key dependencies:
- ImGui for UI rendering
- gfx library for graphics operations
- zelda3 library for ROM data structures
- absl for status handling

## Conclusion

This modular architecture provides a solid foundation for future dungeon editor development. The separation of concerns makes the codebase maintainable, testable, and extensible. Future developers should follow the established patterns and extend components rather than modifying the main orchestrator class.

For questions or clarifications, refer to the existing integration tests and component implementations as examples of proper usage patterns.
