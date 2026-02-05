#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "app/editor/dungeon/dungeon_coordinates.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/editor/dungeon/interaction/interaction_coordinator.h"
#include "app/editor/dungeon/interaction/interaction_mode.h"
#include "app/editor/dungeon/object_selection.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/door_position.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/sprite/sprite.h"

#include "rom/rom.h"

namespace yaze {
namespace editor {

/**
 * @brief Type of entity that can be selected in the dungeon editor
 */
enum class EntityType {
  None,
  Object,   // Room tile objects
  Door,     // Door entities
  Sprite,   // Enemy/NPC sprites
  Item      // Pot items
};

/**
 * @brief Represents a selected entity in the dungeon editor
 */
struct SelectedEntity {
  EntityType type = EntityType::None;
  size_t index = 0;  // Index into the respective container
  
  bool operator==(const SelectedEntity& other) const {
    return type == other.type && index == other.index;
  }
};

/**
 * @brief Handles object selection, placement, and interaction within the
 * dungeon canvas
 *
 * This component manages mouse interactions for object selection (similar to
 * OverworldEditor), object placement, drag operations, and multi-object
 * selection.
 */
class DungeonObjectInteraction {
 public:
  explicit DungeonObjectInteraction(gui::Canvas* canvas) : canvas_(canvas) {
    // Set up initial context
    interaction_context_.canvas = canvas;
    entity_coordinator_.SetContext(&interaction_context_);
  }

  // ========================================================================
  // Context and Configuration
  // ========================================================================

  /**
   * @brief Set the unified interaction context
   *
   * This is the preferred method for configuring the interaction handler.
   * It propagates context to all sub-handlers.
   */
  void SetContext(const InteractionContext& ctx) {
    interaction_context_ = ctx;
    interaction_context_.canvas = canvas_;  // Always use our canvas
    entity_coordinator_.SetContext(&interaction_context_);
  }

  /**
   * @brief Get the interaction coordinator for entity handling
   *
   * Use this for advanced entity operations (doors, sprites, items).
   */
  InteractionCoordinator& entity_coordinator() { return entity_coordinator_; }
  const InteractionCoordinator& entity_coordinator() const {
    return entity_coordinator_;
  }

  // Legacy setter - kept for backwards compatibility
  void SetRom(Rom* rom) {
    rom_ = rom;
    interaction_context_.rom = rom;
    entity_coordinator_.SetContext(&interaction_context_);
  }

  // Main interaction handling
  void HandleCanvasMouseInput();
  void CheckForObjectSelection();
  void PlaceObjectAtPosition(int room_x, int room_y);

  // Selection rectangle (like OverworldEditor)
  void DrawObjectSelectRect();
  void SelectObjectsInRect();
  void DrawSelectionHighlights();  // Draw highlights for selected objects
  void DrawHoverHighlight(const std::vector<zelda3::RoomObject>& objects);  // Draw hover indicator

  // Drag and select box functionality
  void DrawSelectBox();
  void DrawDragPreview();
  void DrawGhostPreview();  // Draw ghost preview for object placement
  void UpdateSelectedObjects();
  bool IsObjectInSelectBox(const zelda3::RoomObject& object) const;

  // Coordinate conversion
  std::pair<int, int> RoomToCanvasCoordinates(int room_x, int room_y) const;
  std::pair<int, int> CanvasToRoomCoordinates(int canvas_x, int canvas_y) const;
  bool IsWithinCanvasBounds(int canvas_x, int canvas_y, int margin = 32) const;

  // State management
  void SetCurrentRoom(std::array<zelda3::Room, dungeon_coords::kRoomCount>* rooms,
                      int room_id);
  void SetPreviewObject(const zelda3::RoomObject& object, bool loaded);
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    current_palette_group_ = group;
    interaction_context_.current_palette_group = group;
    entity_coordinator_.SetContext(&interaction_context_);
  }

  // Mode manager access
  InteractionModeManager& mode_manager() { return mode_manager_; }
  const InteractionModeManager& mode_manager() const { return mode_manager_; }

  // Mode queries - delegate to mode manager
  bool IsObjectLoaded() const {
    return mode_manager_.GetMode() == InteractionMode::PlaceObject;
  }

  void CancelPlacement() {
    mode_manager_.CancelCurrentMode();
    ghost_preview_buffer_.reset();
  }

  // Door placement mode
  void SetDoorPlacementMode(bool enabled, zelda3::DoorType type = zelda3::DoorType::NormalDoor);
  bool IsDoorPlacementActive() const {
    return mode_manager_.GetMode() == InteractionMode::PlaceDoor;
  }
  void SetPreviewDoorType(zelda3::DoorType type) {
    mode_manager_.GetModeState().preview_door_type = type;
  }
  zelda3::DoorType GetPreviewDoorType() const {
    return mode_manager_.GetModeState().preview_door_type.value_or(
        zelda3::DoorType::NormalDoor);
  }
  void DrawDoorGhostPreview();  // Draw door ghost preview with wall snapping
  void PlaceDoorAtPosition(int canvas_x, int canvas_y);  // Place door at snapped position
  void CancelDoorPlacement() {
    if (mode_manager_.GetMode() == InteractionMode::PlaceDoor) {
      mode_manager_.CancelCurrentMode();
    }
  }

  // Sprite placement mode
  void SetSpritePlacementMode(bool enabled, uint8_t sprite_id = 0);
  bool IsSpritePlacementActive() const {
    return mode_manager_.GetMode() == InteractionMode::PlaceSprite;
  }
  void SetPreviewSpriteId(uint8_t id) {
    mode_manager_.GetModeState().preview_sprite_id = id;
  }
  uint8_t GetPreviewSpriteId() const {
    return mode_manager_.GetModeState().preview_sprite_id.value_or(0);
  }
  void DrawSpriteGhostPreview();  // Draw sprite ghost preview
  void PlaceSpriteAtPosition(int canvas_x, int canvas_y);
  void CancelSpritePlacement() {
    if (mode_manager_.GetMode() == InteractionMode::PlaceSprite) {
      mode_manager_.CancelCurrentMode();
    }
  }

  // Item placement mode
  void SetItemPlacementMode(bool enabled, uint8_t item_id = 0);
  bool IsItemPlacementActive() const {
    return mode_manager_.GetMode() == InteractionMode::PlaceItem;
  }
  void SetPreviewItemId(uint8_t id) {
    mode_manager_.GetModeState().preview_item_id = id;
  }
  uint8_t GetPreviewItemId() const {
    return mode_manager_.GetModeState().preview_item_id.value_or(0);
  }
  void DrawItemGhostPreview();  // Draw item ghost preview
  void PlaceItemAtPosition(int canvas_x, int canvas_y);
  void CancelItemPlacement() {
    if (mode_manager_.GetMode() == InteractionMode::PlaceItem) {
      mode_manager_.CancelCurrentMode();
    }
  }

  // Selection state - delegates to ObjectSelection
  std::vector<size_t> GetSelectedObjectIndices() const {
    return selection_.GetSelectedIndices();
  }
  void SetSelectedObjects(const std::vector<size_t>& indices) {
    selection_.ClearSelection();
    for (size_t idx : indices) {
      selection_.SelectObject(idx, ObjectSelection::SelectionMode::Add);
    }
  }
  bool IsObjectSelectActive() const {
    return selection_.HasSelection() || selection_.IsRectangleSelectionActive();
  }
  void ClearSelection();
  bool IsObjectSelected(size_t index) const {
    return selection_.IsObjectSelected(index);
  }
  size_t GetSelectionCount() const { return selection_.GetSelectionCount(); }

  // Selection change notification
  void SetSelectionChangeCallback(std::function<void()> callback) {
    selection_.SetSelectionChangedCallback(std::move(callback));
  }

  // Helper for click selection with proper mode handling
  bool TrySelectObjectAtCursor();

  // Object manipulation
  void HandleScrollWheelResize();  // Resize selected objects with scroll wheel
  size_t GetHoveredObjectIndex() const;  // Get index of object under cursor

  void HandleDeleteSelected();
  void HandleDeleteAllObjects();
  void HandleCopySelected();
  void HandlePasteObjects();
  bool HasClipboardData() const { return has_clipboard_data_; }

  // Layer assignment for selected objects
  void SendSelectedToLayer(int target_layer);

  // Object ordering (changes draw order within the layer)
  // SNES draws objects in list order - first objects appear behind, last on top
  void SendSelectedToFront();   // Move to end of list (drawn last, appears on top)
  void SendSelectedToBack();    // Move to start of list (drawn first, appears behind)
  void BringSelectedForward();  // Move up one position in list
  void SendSelectedBackward();  // Move down one position in list

  // Layer filter access (delegates to ObjectSelection)
  void SetLayerFilter(int layer) { selection_.SetLayerFilter(layer); }
  int GetLayerFilter() const { return selection_.GetLayerFilter(); }
  bool IsLayerFilterActive() const { return selection_.IsLayerFilterActive(); }
  bool IsMaskModeActive() const { return selection_.IsMaskModeActive(); }
  const char* GetLayerFilterName() const { return selection_.GetLayerFilterName(); }
  void SetLayersMerged(bool merged) { selection_.SetLayersMerged(merged); }
  bool AreLayersMerged() const { return selection_.AreLayersMerged(); }

  // Check keyboard shortcuts for layer operations
  void HandleLayerKeyboardShortcuts();

  // Callbacks - stored in interaction_context_ (single source of truth)
  void SetObjectPlacedCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_placed_callback_ = std::move(callback);
  }
  void SetCacheInvalidationCallback(std::function<void()> callback) {
    interaction_context_.on_invalidate_cache = std::move(callback);
    entity_coordinator_.SetContext(&interaction_context_);
  }
  void SetMutationCallback(std::function<void()> callback) {
    interaction_context_.on_mutation = std::move(callback);
    entity_coordinator_.SetContext(&interaction_context_);
  }
  // Backward compatibility alias
  [[deprecated("Use SetMutationCallback() instead")]]
  void SetMutationHook(std::function<void()> callback) {
    SetMutationCallback(std::move(callback));
  }

  void SetEditorSystem(zelda3::DungeonEditorSystem* system) {
    editor_system_ = system;
  }

  // Entity selection (doors, sprites, items)
  void SelectEntity(EntityType type, size_t index);
  void ClearEntitySelection();
  bool HasEntitySelection() const { return selected_entity_.type != EntityType::None; }
  const SelectedEntity& GetSelectedEntity() const { return selected_entity_; }
  
  // Entity hit detection
  std::optional<SelectedEntity> GetEntityAtPosition(int canvas_x, int canvas_y) const;
  
  // Draw entity selection highlights
  void DrawEntitySelectionHighlights();
  void DrawDoorSnapIndicators();  // Show valid snap positions during door drag

  // Entity interaction
  bool TrySelectEntityAtCursor();  // Try to select door/sprite/item at cursor
  void HandleEntityDrag();         // Handle dragging selected entity
  
  // Callbacks for entity changes
  void SetEntityChangedCallback(std::function<void()> callback) {
    interaction_context_.on_entity_changed = std::move(callback);
    entity_coordinator_.SetContext(&interaction_context_);
  }

 private:
  gui::Canvas* canvas_;
  zelda3::DungeonEditorSystem* editor_system_ = nullptr;
  std::array<zelda3::Room, dungeon_coords::kRoomCount>* rooms_ = nullptr;
  int current_room_id_ = 0;
  Rom* rom_ = nullptr;
  std::unique_ptr<zelda3::ObjectDrawer> object_drawer_;

  // Unified interaction context and coordinator for entity handling
  InteractionContext interaction_context_;
  InteractionCoordinator entity_coordinator_;

  // Unified mode state machine - replaces scattered boolean flags
  InteractionModeManager mode_manager_;

  // Helper to calculate object bounds
  std::pair<int, int> CalculateObjectBounds(const zelda3::RoomObject& object);
  ImVec2 ApplyDragModifiers(const ImVec2& delta) const;

  // Preview object state (used by ModeState but kept here for ghost bitmap)
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};

  // Ghost preview bitmap (persists across frames for placement preview)
  std::unique_ptr<gfx::BackgroundBuffer> ghost_preview_buffer_;
  gfx::PaletteGroup current_palette_group_;
  void RenderGhostPreviewBitmap();

  // Unified selection system - replaces legacy selection state
  ObjectSelection selection_;

  // Hover detection for resize
  size_t hovered_object_index_ = static_cast<size_t>(-1);
  bool has_hovered_object_ = false;

  // Callbacks - stored only in interaction_context_ (no duplication)
  std::function<void(const zelda3::RoomObject&)> object_placed_callback_;

  // Clipboard for copy/paste
  std::vector<zelda3::RoomObject> clipboard_;
  bool has_clipboard_data_ = false;

  // Entity selection state (doors, sprites, items)
  SelectedEntity selected_entity_;
  bool is_entity_mode_ = false;  // When true, suppress all object interactions
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
