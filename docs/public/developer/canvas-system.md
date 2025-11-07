# Canvas System Guide

This guide provides a comprehensive overview of the `yaze` canvas system, its architecture, and best practices for integration. It reflects the state of the system after the October 2025 refactoring.

## 1. Architecture

The canvas system was refactored from a monolithic class into a modular, component-based architecture. The main `gui::Canvas` class now acts as a façade, coordinating a set of single-responsibility components and free functions.

### Core Principles
- **Modular Components**: Logic is broken down into smaller, testable units (e.g., state, rendering, interaction, menus).
- **Data-Oriented Design**: Plain-old-data (POD) structs like `CanvasState` and `CanvasConfig` hold state, which is operated on by free functions.
- **Backward Compatibility**: The refactor was designed to be 100% backward compatible. Legacy APIs still function, but new patterns are encouraged.
- **Editor Agnostic**: The core canvas system has no knowledge of `zelda3` specifics, making it reusable for any editor.

### Code Organization
The majority of the canvas code resides in `src/app/gui/canvas/`.
```
src/app/gui/canvas/
├── canvas.h/cc                      # Main Canvas class (facade)
├── canvas_state.h                   # POD state structs
├── canvas_config.h                  # Unified configuration struct
├── canvas_geometry.h/cc             # Geometry calculation helpers
├── canvas_rendering.h/cc            # Rendering free functions
├── canvas_events.h                  # Interaction event structs
├── canvas_interaction.h/cc          # Interaction event handlers
├── canvas_menu.h/cc                 # Declarative menu structures
├── canvas_menu_builder.h/cc         # Fluent API for building menus
├── canvas_popup.h/cc                # PopupRegistry for persistent popups
└── canvas_utils.h/cc                # General utility functions
```

## 2. Core Concepts

### Configuration (`CanvasConfig`)
- A single, unified `gui::CanvasConfig` struct (defined in `canvas_config.h`) holds all configuration for a canvas instance.
- This includes display settings (grid, labels), sizing, scaling, and usage mode.
- This replaces duplicated config structs from previous versions.

### State (`CanvasState`)
- A POD struct (`canvas_state.h`) that holds the dynamic state of the canvas, including geometry, zoom, and scroll.
- Editors can inspect this state for custom rendering and logic.

### Coordinate Systems
The canvas operates with three distinct coordinate spaces. Using the correct one is critical to avoid bugs.

1.  **Screen Space**: Absolute pixel coordinates on the monitor (from `ImGui::GetIO().MousePos`). **Never use this for canvas logic.**
2.  **Canvas/World Space**: Coordinates relative to the canvas's content, accounting for scrolling and panning. Use `Canvas::hover_mouse_pos()` to get this. This is the correct space for entity positioning and high-level calculations.
3.  **Tile/Grid Space**: Coordinates in tile units. Use `Canvas::CanvasToTile()` to convert from world space.

A critical fix was made to ensure `Canvas::hover_mouse_pos()` is updated continuously whenever the canvas is hovered, decoupling it from specific actions like painting.

## 3. Interaction System

The canvas supports several interaction modes, managed via the `CanvasUsage` enum.

### Interaction Modes
- `kTilePainting`: For painting tiles onto a tilemap.
- `kTileSelection`: For selecting one or more tiles.
- `kRectangleSelection`: For drag-selecting a rectangular area.
- `kEntityManipulation`: For moving and interacting with entities.
- `kPaletteEditing`: For palette-related work.
- `kDiagnostics`: For performance and debug overlays.

Set the mode using `canvas.SetUsageMode(gui::CanvasUsage::kTilePainting)`. This ensures the context menu and interaction handlers behave correctly.

### Event-Driven Model
Interaction logic is moving towards an event-driven model. Instead of inspecting canvas state directly, editors should handle events returned by interaction functions.

**Example**:
```cpp
RectSelectionEvent event = HandleRectangleSelection(geometry, ...);
if (event.is_complete) {
  // Process the selection event
}
```

## 4. Context Menu & Popups

The context menu system is now unified, data-driven, and supports persistent popups.

### Key Features
- **Unified Item Definition**: All menu items use the `gui::CanvasMenuItem` struct.
- **Priority-Based Ordering**: Menu sections are automatically sorted based on the `MenuSectionPriority` enum, ensuring a consistent layout:
    1.  `kEditorSpecific` (highest priority)
    2.  `kBitmapOperations`
    3.  `kCanvasProperties`
    4.  `kDebug` (lowest priority)
- **Automatic Popup Persistence**: Popups defined declaratively will remain open until explicitly closed by the user (ESC or close button), rather than closing on any click outside.
- **Fluent Builder API**: The `gui::CanvasMenuBuilder` provides a clean, chainable API for constructing complex menus.

### API Patterns

**Add a Simple Menu Item**:
```cpp
canvas.AddContextMenuItem(
  gui::CanvasMenuItem("Label", ICON_MD_ICON, []() { /* Action */ })
);
```

**Add a Declarative Popup Item**:
This pattern automatically handles popup registration and persistence.
```cpp
auto item = gui::CanvasMenuItem::WithPopup(
  "Properties",
  "props_popup_id",
  []() {
    // Render popup content here
    ImGui::Text("My Properties");
  }
);
canvas.AddContextMenuItem(item);
```

**Build a Complex Menu with the Builder**:
```cpp
gui::CanvasMenuBuilder builder;
canvas.editor_menu() = builder
  .BeginSection("Editor Actions", gui::MenuSectionPriority::kEditorSpecific)
    .AddItem("Cut", ICON_MD_CUT, []() { Cut(); })
    .AddPopupItem("Settings", "settings_popup", []() { RenderSettings(); })
  .EndSection()
  .Build();
```

## 5. Entity System

A generic, Zelda-agnostic entity system allows editors to manage on-canvas objects.

- **Flat Functions**: Entity creation logic is handled by pure functions in `src/app/editor/overworld/operations/entity_operations.h`, such as `InsertEntrance`, `InsertSprite`, etc. These functions are designed for ZScream feature parity.
- **Delegation Pattern**: The `OverworldEditor` delegates to the `MapPropertiesSystem`, which in turn calls these flat functions to modify the ROM state.
- **Mode-Aware Menu**: The "Insert Entity" context submenu is only available when the canvas is in `kEntityManipulation` mode.

**Usage Flow**:
1.  Set canvas mode to `kEntityManipulation`.
2.  Right-click on the canvas to open the context menu.
3.  Select "Insert Entity" and choose the entity type.
4.  The appropriate callback is fired, which calls the corresponding `Insert...` function.
5.  A popup appears to configure the new entity's properties.

## 6. Integration Guide for Editors

1.  **Construct `Canvas`**: Instantiate `gui::Canvas`, providing a unique ID.
2.  **Configure**: Set the desired `CanvasUsage` mode via `canvas.SetUsageMode()`. Configure available modes and other options in the `CanvasConfig` struct.
3.  **Register Callbacks**: If using interaction modes like tile painting, register callbacks for events like `finish_paint`.
4.  **Render Loop**:
    - Call `canvas.Begin(size)`.
    - Draw your editor-specific content (bitmaps, entities, overlays).
    - Call `canvas.End()`. This handles rendering the grid, overlays, and the context menu.
5.  **Provide Custom Menus**: Use `canvas.AddContextMenuItem()` or the `CanvasMenuBuilder` to add editor-specific actions to the context menu. Assign the `kEditorSpecific` priority to ensure they appear at the top.
6.  **Handle State**: Respond to user interactions by inspecting the `CanvasState` or handling events returned from interaction helpers.

## 7. Debugging

If you encounter issues with the canvas, check the following:

- **Context Menu Doesn't Appear**:
    - Is `config.enable_context_menu` true?
    - Is the mouse button right-click?
    - Is the canvas focused and not being dragged?
- **Popup Doesn't Persist**:
    - Are you using the `CanvasMenuItem::WithPopup` pattern?
    - Is `canvas.End()` being called every frame to allow the `PopupRegistry` to render?
- **Incorrect Coordinates**:
    - Are you using `canvas.hover_mouse_pos()` for world coordinates instead of `ImGui::GetIO().MousePos`?
    - Verify that you are correctly converting between world space and tile space.
- **Menu Items in Wrong Order**:
    - Have you set the correct `MenuSectionPriority` for your custom menu sections?

## 8. Automation API

The `CanvasAutomationAPI` provides hooks for testing and automation. It allows for programmatic control of tile operations (`SetTileAt`, `SelectRect`), view controls (`ScrollToTile`, `SetZoom`), and entity manipulation. This API is exposed via the `z3ed` CLI and a gRPC service.