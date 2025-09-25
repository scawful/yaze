# Dungeon Editor Design Plan & Future Development Guide

## Overview
This document provides a comprehensive design plan for the Yaze Dungeon Editor, including current architecture, identified issues, and a roadmap for future developers to continue development effectively.

## Current Architecture

### Main Components

#### 1. **DungeonEditor** (Main Controller)
- **File**: `src/app/editor/dungeon/dungeon_editor.h/cc`
- **Purpose**: Main UI controller and coordinator
- **Responsibilities**:
  - Managing the 3-column UI layout
  - Coordinating between different editor components
  - Handling ROM loading and initialization
  - Managing editor state and undo/redo

#### 2. **DungeonRoomSelector** (Room/Entrance Selection)
- **File**: `src/app/editor/dungeon/dungeon_room_selector.h/cc`
- **Purpose**: Handles room and entrance selection UI
- **Responsibilities**:
  - Room list display and selection
  - Entrance configuration and selection
  - Room properties editing

#### 3. **DungeonCanvasViewer** (Main Canvas)
- **File**: `src/app/editor/dungeon/dungeon_canvas_viewer.h/cc`
- **Purpose**: Main canvas rendering and interaction
- **Responsibilities**:
  - Room graphics rendering
  - Object rendering and positioning
  - Canvas interaction (pan, zoom, select)
  - Coordinate system management

#### 4. **DungeonObjectSelector** (Object Management)
- **File**: `src/app/editor/dungeon/dungeon_object_selector.h/cc`
- **Purpose**: Object selection, preview, and editing
- **Responsibilities**:
  - Object preview rendering
  - Object editing controls
  - Graphics sheet display
  - Integrated editing panels

### Core Systems

#### 1. **ObjectRenderer**
- **File**: `src/app/zelda3/dungeon/object_renderer.h/cc`
- **Purpose**: Renders dungeon objects to bitmaps
- **Features**: Caching, performance monitoring, memory management

#### 2. **DungeonEditorSystem**
- **File**: `src/app/zelda3/dungeon/dungeon_editor_system.h/cc`
- **Purpose**: High-level dungeon editing operations
- **Features**: Undo/redo, room management, object operations

#### 3. **DungeonObjectEditor**
- **File**: `src/app/zelda3/dungeon/dungeon_object_editor.h/cc`
- **Purpose**: Interactive object editing
- **Features**: Object placement, editing modes, validation

## Current Issues & Fixes Applied

### 1. **Crash Prevention** ✅ FIXED
- **Issue**: Multiple crashes when ROM not loaded or invalid data accessed
- **Fixes Applied**:
  - Added comprehensive null checks in `DrawSpriteTile`
  - Added ROM validation in `LoadAnimatedGraphics` and `CopyRoomGraphicsToBuffer`
  - Added bounds checking for all array/vector accesses
  - Added graceful error handling with early returns

### 2. **UI Simplification** ✅ FIXED
- **Issue**: 4-column layout was too crowded
- **Fixes Applied**:
  - Reduced to 3-column layout: Room/Entrance Selector | Canvas | Object Selector/Editor
  - Fixed column widths for better space utilization
  - Separated logical components into dedicated classes

### 3. **Coordinate System Issues** ✅ PARTIALLY FIXED
- **Issue**: Object previews and coordinates were incorrect
- **Fixes Applied**:
  - Fixed object preview centering in canvas
  - Added coordinate conversion helper functions
  - Improved bounds checking for object rendering

## Remaining Issues

### 1. **Object Preview Not Showing**
- **Current Status**: Objects may not render in preview due to:
  - ROM data not properly loaded
  - Palette issues
  - Object parsing failures
- **Recommended Fix**: Add debug logging and fallback rendering

### 2. **Graphics Loading Issues**
- **Current Status**: Room graphics may not load properly
- **Recommended Fix**: Implement proper error handling and user feedback

### 3. **Memory Management**
- **Current Status**: Potential memory leaks in graphics caching
- **Recommended Fix**: Implement proper cleanup and memory monitoring

## Future Development Roadmap

### Phase 1: Stability & Bug Fixes (Priority: High)

#### 1.1 Object Preview System
```cpp
// In DungeonObjectSelector::DrawObjectRenderer()
void DungeonObjectSelector::DrawObjectRenderer() {
  // Add debug information
  if (ImGui::Button("Debug Object Preview")) {
    // Log object data, ROM state, palette info
    LogObjectPreviewDebugInfo();
  }
  
  // Add fallback rendering
  if (!object_loaded_) {
    ImGui::Text("No object preview available");
    if (ImGui::Button("Force Load Preview")) {
      ForceLoadObjectPreview();
    }
  }
}
```

#### 1.2 Error Handling & User Feedback
```cpp
// Add to all major operations
absl::Status LoadRoomGraphics(int room_id) {
  auto result = DoLoadRoomGraphics(room_id);
  if (!result.ok()) {
    ShowErrorMessage("Failed to load room graphics", result.message());
    return result;
  }
  ShowSuccessMessage("Room graphics loaded successfully");
  return absl::OkStatus();
}
```

#### 1.3 Memory Management
```cpp
// Add memory monitoring
class DungeonEditor {
private:
  void MonitorMemoryUsage() {
    auto stats = object_renderer_.GetPerformanceStats();
    if (stats.memory_usage > kMaxMemoryUsage) {
      ClearObjectCache();
      ShowWarningMessage("Memory usage high, cleared cache");
    }
  }
};
```

### Phase 2: Feature Enhancement (Priority: Medium)

#### 2.1 Advanced Object Editing
- Multi-object selection
- Object grouping
- Copy/paste operations
- Object templates

#### 2.2 Enhanced Canvas Features
- Zoom controls
- Grid snapping
- Layer management
- Real-time preview

#### 2.3 Room Management
- Room duplication
- Room templates
- Bulk operations
- Room validation

### Phase 3: Advanced Features (Priority: Low)

#### 3.1 Scripting Support
- Lua scripting for custom operations
- Automation tools
- Batch processing

#### 3.2 Plugin System
- Modular architecture
- Third-party plugins
- Custom object types

## Implementation Guidelines

### 1. **Error Handling Pattern**
```cpp
absl::Status DoOperation() {
  // Validate inputs
  if (!IsValidInput()) {
    return absl::InvalidArgumentError("Invalid input");
  }
  
  // Perform operation with error checking
  auto result = PerformOperation();
  if (!result.ok()) {
    LogError("Operation failed", result.status());
    return result.status();
  }
  
  return absl::OkStatus();
}
```

### 2. **UI Component Pattern**
```cpp
class ComponentName {
public:
  void Draw() {
    if (!IsValid()) {
      ImGui::Text("Component not ready");
      return;
    }
    
    // Draw component UI
    DrawMainUI();
    
    // Handle interactions
    HandleInteractions();
  }
  
private:
  bool IsValid() const {
    return rom_ != nullptr && rom_->is_loaded();
  }
};
```

### 3. **Memory Management Pattern**
```cpp
class GraphicsManager {
public:
  void ClearCache() {
    object_cache_.clear();
    texture_cache_.clear();
    memory_pool_.Reset();
  }
  
  size_t GetMemoryUsage() const {
    return object_cache_.size() * sizeof(CachedObject) +
           texture_cache_.size() * sizeof(Texture);
  }
  
private:
  std::vector<CachedObject> object_cache_;
  std::unordered_map<int, Texture> texture_cache_;
  MemoryPool memory_pool_;
};
```

## Testing Strategy

### 1. **Unit Tests**
- Test each component in isolation
- Mock ROM data for consistent testing
- Test error conditions and edge cases

### 2. **Integration Tests**
- Test component interactions
- Test with real ROM data
- Test performance under load

### 3. **UI Tests**
- Test user interactions
- Test layout responsiveness
- Test accessibility

## Performance Considerations

### 1. **Rendering Optimization**
- Implement object culling
- Use texture atlases
- Implement level-of-detail (LOD)

### 2. **Memory Optimization**
- Implement smart caching
- Use memory pools
- Monitor memory usage

### 3. **UI Responsiveness**
- Use async operations for heavy tasks
- Implement progress indicators
- Minimize UI blocking operations

## Debugging Tools

### 1. **Debug Console**
```cpp
class DebugConsole {
public:
  void LogObjectState(const RoomObject& obj) {
    ImGui::Text("Object ID: %d", obj.id_);
    ImGui::Text("Position: (%d, %d)", obj.x_, obj.y_);
    ImGui::Text("Size: %d", obj.size_);
    ImGui::Text("Layer: %d", static_cast<int>(obj.layer_));
    ImGui::Text("Tiles Loaded: %s", obj.tiles().empty() ? "No" : "Yes");
  }
};
```

### 2. **Performance Monitor**
```cpp
class PerformanceMonitor {
public:
  void DrawPerformanceStats() {
    auto stats = GetStats();
    ImGui::Text("Render Time: %.2f ms", stats.render_time.count());
    ImGui::Text("Memory Usage: %.2f MB", stats.memory_usage / 1024.0 / 1024.0);
    ImGui::Text("Cache Hit Rate: %.2f%%", stats.cache_hit_rate * 100);
  }
};
```

## Current Implementation Status

### ✅ Completed (Phase 1)
- **UI Component Separation**: Successfully separated DungeonEditor into 3 main components:
  - `DungeonRoomSelector`: Room and entrance selection UI
  - `DungeonCanvasViewer`: Main canvas rendering and interaction
  - `DungeonObjectSelector`: Object management and editing panels
- **3-Column Layout**: Implemented clean 3-column layout as requested
- **Debug Elements Popup**: Moved debug controls into modal popup for cleaner UI
- **Crash Prevention**: Added comprehensive null checks and bounds validation
- **Component Architecture**: Established proper separation of concerns

### 🔄 In Progress (Phase 2)
- **Component Integration**: Currently integrating UI components into main DungeonEditor
- **Method Implementation**: Adding missing SetRom, SetRooms, SetCurrentPaletteGroup methods
- **Data Flow**: Establishing proper data flow between components

### ⏳ Next Steps (Phase 3)
1. **Complete Method Implementation**: Add all missing methods to UI components
2. **Test Integration**: Verify all components work together correctly
3. **Error Handling**: Add proper error handling and user feedback
4. **Performance Optimization**: Implement caching and memory management

## Implementation Notes for Future Developers

### Component Integration Pattern
```cpp
// Main DungeonEditor coordinates components
class DungeonEditor {
private:
  DungeonRoomSelector room_selector_;
  DungeonCanvasViewer canvas_viewer_;
  DungeonObjectSelector object_selector_;
  
public:
  void Load() {
    // Initialize components with data
    room_selector_.set_rom(rom_);
    room_selector_.set_rooms(&rooms_);
    room_selector_.set_entrances(&entrances_);
    
    canvas_viewer_.SetRom(rom_);
    canvas_viewer_.SetRooms(rooms_);
    canvas_viewer_.SetCurrentPaletteGroup(current_palette_group_);
    
    object_selector_.SetRom(rom_);
    object_selector_.SetCurrentPaletteGroup(current_palette_group_);
  }
  
  void UpdateDungeonRoomView() {
    // 3-column layout
    if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags)) {
      TableNextColumn();
      room_selector_.Draw();  // Column 1: Room/Entrance Selector
      
      TableNextColumn();
      canvas_viewer_.Draw(current_room_id_);  // Column 2: Canvas
      
      TableNextColumn();
      object_selector_.Draw();  // Column 3: Object Selector/Editor
      
      EndTable();
    }
  }
};
```

### Required Methods for UI Components
Each UI component needs these methods for proper integration:

```cpp
// DungeonRoomSelector
void SetRom(Rom* rom);
void SetRooms(std::array<Room, 0x128>* rooms);
void SetEntrances(std::array<RoomEntrance, 0x8C>* entrances);
void Draw();

// DungeonCanvasViewer  
void SetRom(Rom* rom);
void SetRooms(std::array<Room, 0x128>& rooms);
void SetCurrentPaletteGroup(const gfx::PaletteGroup& palette_group);
void SetCurrentPaletteId(uint64_t palette_id);
void Draw(int room_id);

// DungeonObjectSelector
void SetRom(Rom* rom);
void SetCurrentPaletteGroup(const gfx::PaletteGroup& palette_group);
void SetCurrentPaletteId(uint64_t palette_id);
void Draw();
```

## Conclusion

The Dungeon Editor has a solid foundation with proper separation of concerns and crash prevention measures in place. The main areas for improvement are:

1. **Component Integration**: Complete the integration of UI components (IN PROGRESS)
2. **Object Preview System**: Needs debugging and fallback mechanisms
3. **Error Handling**: Needs better user feedback and recovery
4. **Memory Management**: Needs monitoring and cleanup
5. **UI Polish**: Needs better visual feedback and responsiveness

Following this design plan will ensure the Dungeon Editor becomes a robust, user-friendly tool for Zelda 3 ROM editing.

## Quick Start for New Developers

1. **Read the Architecture**: Understand the component separation
2. **Study the Crash Fixes**: Learn the error handling patterns
3. **Start with Debugging**: Add logging to understand current issues
4. **Implement Incrementally**: Make small, testable changes
5. **Test Thoroughly**: Always test with real ROM data
6. **Document Changes**: Update this document as you make improvements

The codebase is well-structured and ready for continued development. Focus on stability first, then features.
