#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_MODE_H_
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_MODE_H_

#include <optional>

#include "imgui/imgui.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @brief Unified interaction mode for the dungeon editor
 *
 * Only ONE mode can be active at a time. This replaces the previous
 * pattern of multiple boolean flags (object_loaded_, door_placement_mode_,
 * sprite_placement_mode_, item_placement_mode_, is_dragging_, etc.).
 *
 * State transitions:
 *   Select <-> PlaceObject (when user picks object from palette)
 *   Select <-> PlaceDoor (when user picks door type)
 *   Select <-> PlaceSprite (when user picks sprite)
 *   Select <-> PlaceItem (when user picks item)
 *   Select <-> DraggingObjects (when user starts drag on selection)
 *   Select <-> DraggingEntity (when user starts drag on entity)
 *   Select <-> RectangleSelect (when user starts drag on empty space)
 */
enum class InteractionMode {
  Select,           // Normal selection mode (no placement active)
  PlaceObject,      // Placing a room tile object
  PlaceDoor,        // Placing a door entity
  PlaceSprite,      // Placing a sprite entity
  PlaceItem,        // Placing a pot item entity
  DraggingObjects,  // Dragging selected tile objects
  DraggingEntity,   // Dragging selected door/sprite/item
  RectangleSelect,  // Drawing rectangle selection box
  PaintCollision,   // Painting custom collision tiles
  PaintWaterFill,   // Painting water fill zones (Oracle of Secrets)
};

/**
 * @brief Mode-specific state data
 *
 * Centralizes all preview data and drag state that was previously
 * scattered across multiple member variables.
 */
struct ModeState {
  // Placement preview data
  std::optional<zelda3::RoomObject> preview_object;
  std::optional<zelda3::DoorType> preview_door_type;
  std::optional<uint8_t> preview_sprite_id;
  std::optional<uint8_t> preview_item_id;

  // Door placement specifics
  zelda3::DoorDirection detected_door_direction = zelda3::DoorDirection::North;
  uint8_t snapped_door_position = 0;

  // Drag state
  ImVec2 drag_start = ImVec2(0, 0);
  ImVec2 drag_current = ImVec2(0, 0);
  bool duplicate_on_drag = false;
  bool drag_mutation_started = false;
  bool drag_has_duplicated = false;
  int drag_last_tile_dx = 0;
  int drag_last_tile_dy = 0;

  // Rectangle selection bounds (canvas coordinates)
  int rect_start_x = 0;
  int rect_start_y = 0;
  int rect_end_x = 0;
  int rect_end_y = 0;

  // Entity drag state
  ImVec2 entity_drag_start = ImVec2(0, 0);
  ImVec2 entity_drag_current = ImVec2(0, 0);

  // Collision paint state
  uint8_t paint_collision_value = 0;
  bool is_painting = false;

  /**
   * @brief Clear all mode state
   */
  void Clear() {
    preview_object.reset();
    preview_door_type.reset();
    preview_sprite_id.reset();
    preview_item_id.reset();
    detected_door_direction = zelda3::DoorDirection::North;
    snapped_door_position = 0;
    drag_start = ImVec2(0, 0);
    drag_current = ImVec2(0, 0);
    duplicate_on_drag = false;
    drag_mutation_started = false;
    drag_has_duplicated = false;
    drag_last_tile_dx = 0;
    drag_last_tile_dy = 0;
    rect_start_x = 0;
    rect_start_y = 0;
    rect_end_x = 0;
    rect_end_y = 0;
    entity_drag_start = ImVec2(0, 0);
    entity_drag_current = ImVec2(0, 0);
    paint_collision_value = 0;
    is_painting = false;
  }

  /**
   * @brief Clear only placement preview data
   */
  void ClearPlacementData() {
    preview_object.reset();
    preview_door_type.reset();
    preview_sprite_id.reset();
    preview_item_id.reset();
    detected_door_direction = zelda3::DoorDirection::North;
    snapped_door_position = 0;
  }

  /**
   * @brief Clear only drag-related state
   */
  void ClearDragData() {
    drag_start = ImVec2(0, 0);
    drag_current = ImVec2(0, 0);
    duplicate_on_drag = false;
    drag_mutation_started = false;
    drag_has_duplicated = false;
    drag_last_tile_dx = 0;
    drag_last_tile_dy = 0;
    entity_drag_start = ImVec2(0, 0);
    entity_drag_current = ImVec2(0, 0);
  }

  /**
   * @brief Clear only rectangle selection state
   */
  void ClearRectangleData() {
    rect_start_x = 0;
    rect_start_y = 0;
    rect_end_x = 0;
    rect_end_y = 0;
  }
};

/**
 * @brief Manages interaction mode state and transitions
 *
 * Provides a unified interface for mode management, replacing
 * scattered boolean flags with explicit state machine semantics.
 *
 * Usage:
 *   InteractionModeManager mode_manager;
 *
 *   // Enter placement mode
 *   mode_manager.SetMode(InteractionMode::PlaceObject);
 *   mode_manager.GetModeState().preview_object = some_object;
 *
 *   // Query mode
 *   if (mode_manager.IsPlacementActive()) { ... }
 *
 *   // Cancel and return to select
 *   mode_manager.CancelCurrentMode();
 */
class InteractionModeManager {
 public:
  /**
   * @brief Get current interaction mode
   */
  InteractionMode GetMode() const { return current_mode_; }

  /**
   * @brief Get previous mode (for undo/escape handling)
   */
  InteractionMode GetPreviousMode() const { return previous_mode_; }

  /**
   * @brief Set interaction mode
   *
   * Clears appropriate state data when transitioning between modes.
   * @param mode The new mode to enter
   */
  void SetMode(InteractionMode mode);

  /**
   * @brief Cancel current mode and return to Select
   *
   * Clears mode-specific state and returns to normal selection mode.
   */
  void CancelCurrentMode();

  /**
   * @brief Check if any placement mode is active
   */
  bool IsPlacementActive() const {
    return current_mode_ == InteractionMode::PlaceObject ||
           current_mode_ == InteractionMode::PlaceDoor ||
           current_mode_ == InteractionMode::PlaceSprite ||
           current_mode_ == InteractionMode::PlaceItem;
  }

  /**
   * @brief Check if in normal selection mode
   */
  bool IsSelectMode() const { return current_mode_ == InteractionMode::Select; }

  /**
   * @brief Check if any dragging operation is in progress
   */
  bool IsDragging() const {
    return current_mode_ == InteractionMode::DraggingObjects ||
           current_mode_ == InteractionMode::DraggingEntity;
  }

  /**
   * @brief Check if rectangle selection is in progress
   */
  bool IsRectangleSelecting() const {
    return current_mode_ == InteractionMode::RectangleSelect;
  }

  /**
   * @brief Check if object placement mode is active
   */
  bool IsObjectPlacementActive() const {
    return current_mode_ == InteractionMode::PlaceObject;
  }

  /**
   * @brief Check if door placement mode is active
   */
  bool IsDoorPlacementActive() const {
    return current_mode_ == InteractionMode::PlaceDoor;
  }

  /**
   * @brief Check if sprite placement mode is active
   */
  bool IsSpritePlacementActive() const {
    return current_mode_ == InteractionMode::PlaceSprite;
  }

  /**
   * @brief Check if item placement mode is active
   */
  bool IsItemPlacementActive() const {
    return current_mode_ == InteractionMode::PlaceItem;
  }

  /**
   * @brief Check if entity (non-object) placement is active
   */
  bool IsEntityPlacementActive() const {
    return current_mode_ == InteractionMode::PlaceDoor ||
           current_mode_ == InteractionMode::PlaceSprite ||
           current_mode_ == InteractionMode::PlaceItem;
  }

  /**
   * @brief Get mutable reference to mode state
   */
  ModeState& GetModeState() { return mode_state_; }

  /**
   * @brief Get const reference to mode state
   */
  const ModeState& GetModeState() const { return mode_state_; }

  /**
   * @brief Get mode name for debugging/UI
   */
  const char* GetModeName() const;

 private:
  InteractionMode current_mode_ = InteractionMode::Select;
  InteractionMode previous_mode_ = InteractionMode::Select;
  ModeState mode_state_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_INTERACTION_INTERACTION_MODE_H_
