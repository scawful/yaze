# Overworld Entity System

This document provides a technical overview of the overworld entity system, including critical bug fixes that enable its functionality and the ongoing plan to refactor it for modularity and ZScream feature parity.

## 1. System Overview

The overworld entity system manages all interactive objects on the overworld map, such as entrances, exits, items, and sprites. The system is undergoing a refactor to move from a monolithic architecture within the `Overworld` class to a modular design where each entity's save/load logic is handled in dedicated files.

**Key Goals of the Refactor**:
-   **Modularity**: Isolate entity logic into testable, self-contained units.
-   **ZScream Parity**: Achieve feature compatibility with ZScream's entity management, including support for expanded ROM formats.
-   **Maintainability**: Simplify the `Overworld` class by delegating I/O responsibilities.

## 2. Core Components & Bug Fixes

Several critical bugs were fixed to make the entity system functional. Understanding these fixes is key to understanding the system's design.

### 2.1. Entity Interaction and Hover Detection

**File**: `src/app/editor/overworld/overworld_entity_renderer.cc`

-   **Problem**: Exit entities were not responding to mouse interactions because the hover state was being improperly reset.
-   **Fix**: The hover state (`hovered_entity_`) is now reset only once at the beginning of the entity rendering cycle, specifically in `DrawExits()`, which is the first rendering function called. Subsequent functions (`DrawEntrances()`, `DrawItems()`, etc.) can set the hover state without it being cleared, preserving the correct hover priority (last-drawn entity wins).

```cpp
// In DrawExits(), which is called first:
hovered_entity_ = nullptr; // Reset hover state at the start of the cycle.

// In DrawEntrances() and other subsequent renderers:
// The reset is removed to allow hover state to persist.
```

### 2.2. Entity Property Popup Save/Cancel Logic

**File**: `src/app/editor/overworld/entity.cc`

-   **Problem**: The "Done" and "Cancel" buttons in entity property popups had inverted logic, causing changes to be saved on "Cancel" and discarded on "Done".
-   **Fix**: The `set_done` flag, which controls the popup's return value, is now correctly managed. The "Done" and "Delete" buttons set `set_done = true` to signal a save action, while the "Cancel" button does not, correctly discarding changes.

```cpp
// Corrected logic for the "Done" button in popups
if (ImGui::Button(ICON_MD_DONE)) {
  set_done = true; // Save changes
  ImGui::CloseCurrentPopup();
}

// Corrected logic for the "Cancel" button
if (ImGui::Button(ICON_MD_CANCEL)) {
  // Discard changes (do not set set_done)
  ImGui::CloseCurrentPopup();
}
```

### 2.3. Exit Entity Coordinate System

**File**: `src/zelda3/overworld/overworld_exit.h`

-   **Problem**: Saving a vanilla ROM would corrupt exit positions, causing them to load at (0,0). This was because the `OverworldExit` class used `uint8_t` for player coordinates, truncating 16-bit values.
-   **Fix**: The coordinate-related members of `OverworldExit` were changed to `uint16_t` to match the full 0-4088 coordinate range, achieving parity with ZScream's data structures.

```cpp
// In OverworldExit class definition:
class OverworldExit : public GameEntity {
 public:
  // ...
  uint16_t y_player_; // Changed from uint8_t
  uint16_t x_player_; // Changed from uint8_t
  uint16_t y_camera_; // Changed from uint8_t
  uint16_t x_camera_; // Changed from uint8_t
  // ...
};
```

### 2.4. Coordinate Synchronization on Drag

**File**: `src/zelda3/overworld/overworld_exit.h`

-   **Problem**: When dragging an exit, the visual position (`x_`, `y_`) would update, but the underlying data used for saving (`x_player_`, `y_player_`) would not, leading to a data desync and incorrect saves.
-   **Fix**: The `UpdateMapProperties` method now explicitly syncs the base entity coordinates to the player coordinates before recalculating scroll and camera values. This ensures that drag operations correctly persist.

```cpp
// In OverworldExit::UpdateMapProperties()
void UpdateMapProperties(uint16_t map_id) override {
  // Sync player position from the base entity coordinates updated by the drag system.
  x_player_ = static_cast<uint16_t>(x_);
  y_player_ = static_cast<uint16_t>(y_);

  // Proceed with auto-calculation using the now-correct player coordinates.
  // ...
}
```

## 3. Entity I/O Refactoring Plan

The next phase of development is to extract all entity save and load logic from the monolithic `overworld.cc` into dedicated files.

### 3.1. File Structure

New files will be created to handle I/O for each entity type:
-   `src/zelda3/overworld/overworld_entrance.cc`
-   `src/zelda3/overworld/overworld_exit.cc`
-   `src/zelda3/overworld/overworld_item.cc`
-   `src/zelda3/overworld/overworld_transport.cc` (for new transport/whirlpool support)

### 3.2. Core Functions

Each new file will implement a standard set of flat functions:
-   `LoadAll...()`: Reads all entities of a given type from the ROM.
-   `SaveAll...()`: Writes all entities of a given type to the ROM.
-   Helper functions for coordinate calculation and data manipulation, mirroring ZScream's logic.

### 3.3. ZScream Parity Goals

The refactor aims to implement key ZScream features:
-   **Expanded ROM Support**: Correctly read/write from vanilla or expanded ROM addresses for entrances and items.
-   **Pointer Deduplication**: When saving items, reuse pointers for identical item lists on different maps to conserve space.
-   **Automatic Coordinate Calculation**: For exits and transports, automatically calculate camera and scroll values based on player position, matching the `UpdateMapStuff` logic in ZScream.
-   **Transport Entity**: Add full support for transport entities (whirlpools, birds).

### 3.4. `Overworld` Class Role

After the refactor, the `Overworld` class will act as a coordinator, delegating all entity I/O to the new, modular functions. Its responsibility will be to hold the entity vectors and orchestrate the calls to the `LoadAll...` and `SaveAll...` functions.
