# DungeonEditor Refactoring Plan

## Overview
This document outlines the comprehensive refactoring of the 1444-line `dungeon_editor.cc` file into focused, single-responsibility components.

## Component Structure

### ✅ Created Components

#### 1. DungeonToolset (`dungeon_toolset.h/cc`)
**Responsibility**: Toolbar UI management
- Background layer selection (All/BG1/BG2/BG3)
- Placement mode selection (Object/Sprite/Item/etc.)
- Undo/Redo buttons with callbacks
- **Replaces**: `DrawToolset()` method (~70 lines)

#### 2. DungeonObjectInteraction (`dungeon_object_interaction.h/cc`)
**Responsibility**: Object selection and placement
- Mouse interaction handling
- Object selection rectangle (like OverworldEditor)
- Drag and drop operations
- Coordinate conversion utilities
- **Replaces**: All mouse/selection methods (~400 lines)

#### 3. DungeonRenderer (`dungeon_renderer.h/cc`)
**Responsibility**: All rendering operations
- Object rendering with caching
- Background layer composition
- Layout object visualization
- Render cache management
- **Replaces**: All rendering methods (~200 lines)

#### 4. DungeonRoomLoader (`dungeon_room_loader.h/cc`)
**Responsibility**: ROM data loading
- Room loading from ROM
- Room size calculation
- Entrance loading
- Graphics loading coordination
- **Replaces**: Room loading methods (~150 lines)

#### 5. DungeonUsageTracker (`dungeon_usage_tracker.h/cc`)
**Responsibility**: Resource usage analysis
- Blockset/spriteset/palette usage tracking
- Usage statistics display
- Resource optimization insights
- **Replaces**: Usage statistics methods (~100 lines)

## Refactored DungeonEditor Structure

### Before Refactoring: 1444 lines
```cpp
class DungeonEditor {
  // 30+ methods handling everything
  // Mixed responsibilities
  // Large data structures
  // Complex dependencies
};
```

### After Refactoring: ~400 lines
```cpp
class DungeonEditor {
  // Core editor interface (unchanged)
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;
  absl::Status Save() override;
  
  // High-level UI orchestration
  absl::Status UpdateDungeonRoomView();
  void DrawCanvasAndPropertiesPanel();
  void DrawRoomPropertiesDebugPopup();
  
  // Component coordination
  void OnRoomSelected(int room_id);
  
private:
  // Focused components
  DungeonToolset toolset_;
  DungeonObjectInteraction object_interaction_;
  DungeonRenderer renderer_;
  DungeonRoomLoader room_loader_;
  DungeonUsageTracker usage_tracker_;
  
  // Existing UI components
  DungeonRoomSelector room_selector_;
  DungeonCanvasViewer canvas_viewer_;
  DungeonObjectSelector object_selector_;
  
  // Core data and state
  std::array<zelda3::Room, 0x128> rooms_;
  bool is_loaded_ = false;
  // etc.
};
```

## Method Migration Map

### Core Editor Methods (Keep in main file)
- ✅ `Initialize()` - Component initialization
- ✅ `Load()` - Delegates to room_loader_
- ✅ `Update()` - High-level update coordination
- ✅ `Save()`, `Undo()`, `Redo()` - Editor interface
- ✅ `UpdateDungeonRoomView()` - UI orchestration

### UI Methods (Keep for coordination)
- ✅ `DrawCanvasAndPropertiesPanel()` - Tab management
- ✅ `DrawRoomPropertiesDebugPopup()` - Debug popup
- ✅ `DrawDungeonTabView()` - Room tab management
- ✅ `DrawDungeonCanvas()` - Canvas coordination
- ✅ `OnRoomSelected()` - Room selection handling

### Methods Moved to Components

#### → DungeonToolset
- ❌ `DrawToolset()` - Toolbar rendering

#### → DungeonObjectInteraction  
- ❌ `HandleCanvasMouseInput()` - Mouse handling
- ❌ `CheckForObjectSelection()` - Selection rectangle
- ❌ `DrawObjectSelectRect()` - Selection drawing
- ❌ `SelectObjectsInRect()` - Selection logic
- ❌ `PlaceObjectAtPosition()` - Object placement
- ❌ `DrawSelectBox()` - Selection visualization
- ❌ `DrawDragPreview()` - Drag preview
- ❌ `UpdateSelectedObjects()` - Selection updates
- ❌ `IsObjectInSelectBox()` - Selection testing
- ❌ Coordinate conversion helpers

#### → DungeonRenderer
- ❌ `RenderObjectInCanvas()` - Object rendering
- ❌ `DisplayObjectInfo()` - Object info overlay
- ❌ `RenderLayoutObjects()` - Layout rendering
- ❌ `RenderRoomBackgroundLayers()` - Background rendering
- ❌ `RefreshGraphics()` - Graphics refresh
- ❌ Object cache management

#### → DungeonRoomLoader
- ❌ `LoadDungeonRoomSize()` - Room size calculation
- ❌ `LoadAndRenderRoomGraphics()` - Graphics loading
- ❌ `ReloadAllRoomGraphics()` - Bulk reload
- ❌ Room size and address management

#### → DungeonUsageTracker
- ❌ `CalculateUsageStats()` - Usage calculation
- ❌ `DrawUsageStats()` - Usage display
- ❌ `DrawUsageGrid()` - Usage visualization
- ❌ `RenderSetUsage()` - Set usage rendering

## Component Communication

### Callback System
```cpp
// Object placement callback
object_interaction_.SetObjectPlacedCallback([this](const auto& object) {
  renderer_.ClearObjectCache();
});

// Toolset callbacks
toolset_.SetUndoCallback([this]() { Undo(); });
toolset_.SetPaletteToggleCallback([this]() { palette_showing_ = !palette_showing_; });

// Object selection callback
object_selector_.SetObjectSelectedCallback([this](const auto& object) {
  object_interaction_.SetPreviewObject(object, true);
  toolset_.set_placement_type(DungeonToolset::kObject);
});
```

### Data Sharing
```cpp
// Update components with current room
void OnRoomSelected(int room_id) {
  current_room_id_ = room_id;
  object_interaction_.SetCurrentRoom(&rooms_, room_id);
  // etc.
}
```

## Benefits of Refactoring

### 1. **Reduced Complexity**
- Main file: 1444 → ~400 lines (72% reduction)
- Single responsibility per component
- Clear separation of concerns

### 2. **Improved Testability**
- Individual components can be unit tested
- Mocking becomes easier
- Isolated functionality testing

### 3. **Better Maintainability**
- Changes isolated to relevant components
- Easier to locate and fix bugs
- Cleaner code reviews

### 4. **Enhanced Extensibility**
- New features added to appropriate components
- Component interfaces allow easy replacement
- Plugin-style architecture possible

### 5. **Cleaner Dependencies**
- UI separate from data manipulation
- Rendering separate from business logic
- Clear component boundaries

## Implementation Status

### ✅ Completed
- Created all component header files
- Created component implementation stubs
- Updated DungeonEditor header with components
- Basic component integration

### 🔄 In Progress
- Method migration from main file to components
- Component callback setup
- Legacy method removal

### ⏳ Pending
- Full method implementation in components
- Complete integration testing
- Documentation updates
- Build system updates

## Migration Strategy

### Phase 1: Create Components ✅
- Define component interfaces
- Create header and implementation files
- Set up basic structure

### Phase 2: Integrate Components 🔄
- Add components to DungeonEditor
- Set up callback systems
- Begin method delegation

### Phase 3: Move Methods
- Systematically move methods to components
- Update method calls to use components
- Remove old implementations

### Phase 4: Cleanup
- Remove unused member variables
- Clean up includes
- Update documentation

This refactoring transforms the monolithic DungeonEditor into a well-organized, component-based architecture that's easier to maintain, test, and extend.
