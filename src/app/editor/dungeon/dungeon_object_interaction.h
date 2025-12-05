#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H

#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

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

// Remove duplicate room.h include
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
  explicit DungeonObjectInteraction(gui::Canvas* canvas) : canvas_(canvas) {}
  
  void SetRom(Rom* rom) { rom_ = rom; }

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
  void SetCurrentRoom(std::array<zelda3::Room, 0x128>* rooms, int room_id);
  void SetPreviewObject(const zelda3::RoomObject& object, bool loaded);
  void SetCurrentPaletteGroup(const gfx::PaletteGroup& group) {
    current_palette_group_ = group;
  }
  bool IsObjectLoaded() const { return object_loaded_; }
  void CancelPlacement() {
    object_loaded_ = false;
    preview_object_ = zelda3::RoomObject{0, 0, 0, 0, 0};
    ghost_preview_buffer_.reset();
    CancelDoorPlacement();
  }

  // Door placement mode
  void SetDoorPlacementMode(bool enabled, zelda3::DoorType type = zelda3::DoorType::NormalDoor);
  bool IsDoorPlacementActive() const { return door_placement_mode_; }
  void SetPreviewDoorType(zelda3::DoorType type) { preview_door_type_ = type; }
  zelda3::DoorType GetPreviewDoorType() const { return preview_door_type_; }
  void DrawDoorGhostPreview();  // Draw door ghost preview with wall snapping
  void PlaceDoorAtPosition(int canvas_x, int canvas_y);  // Place door at snapped position
  void CancelDoorPlacement() {
    door_placement_mode_ = false;
    detected_door_direction_ = zelda3::DoorDirection::North;
    snapped_door_position_ = 0;
  }

  // Sprite placement mode
  void SetSpritePlacementMode(bool enabled, uint8_t sprite_id = 0);
  bool IsSpritePlacementActive() const { return sprite_placement_mode_; }
  void SetPreviewSpriteId(uint8_t id) { preview_sprite_id_ = id; }
  uint8_t GetPreviewSpriteId() const { return preview_sprite_id_; }
  void DrawSpriteGhostPreview();  // Draw sprite ghost preview
  void PlaceSpriteAtPosition(int canvas_x, int canvas_y);
  void CancelSpritePlacement() { sprite_placement_mode_ = false; }

  // Item placement mode
  void SetItemPlacementMode(bool enabled, uint8_t item_id = 0);
  bool IsItemPlacementActive() const { return item_placement_mode_; }
  void SetPreviewItemId(uint8_t id) { preview_item_id_ = id; }
  uint8_t GetPreviewItemId() const { return preview_item_id_; }
  void DrawItemGhostPreview();  // Draw item ghost preview
  void PlaceItemAtPosition(int canvas_x, int canvas_y);
  void CancelItemPlacement() { item_placement_mode_ = false; }

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
  const char* GetLayerFilterName() const { return selection_.GetLayerFilterName(); }
  void SetLayersMerged(bool merged) { selection_.SetLayersMerged(merged); }
  bool AreLayersMerged() const { return selection_.AreLayersMerged(); }

  // Check keyboard shortcuts for layer operations
  void HandleLayerKeyboardShortcuts();

  // Callbacks
  void SetObjectPlacedCallback(
      std::function<void(const zelda3::RoomObject&)> callback) {
    object_placed_callback_ = callback;
  }
  void SetCacheInvalidationCallback(std::function<void()> callback) {
    cache_invalidation_callback_ = callback;
  }
  void SetMutationHook(std::function<void()> callback) {
    mutation_hook_ = std::move(callback);
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
    entity_changed_callback_ = std::move(callback);
  }

 private:
  gui::Canvas* canvas_;
  zelda3::DungeonEditorSystem* editor_system_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  int current_room_id_ = 0;
  Rom* rom_ = nullptr;
  std::unique_ptr<zelda3::ObjectDrawer> object_drawer_;

  // Helper to calculate object bounds
  std::pair<int, int> CalculateObjectBounds(const zelda3::RoomObject& object);

  // Preview object state
  zelda3::RoomObject preview_object_{0, 0, 0, 0, 0};
  bool object_loaded_ = false;

  // Ghost preview bitmap (persists across frames for placement preview)
  std::unique_ptr<gfx::BackgroundBuffer> ghost_preview_buffer_;
  gfx::PaletteGroup current_palette_group_;
  void RenderGhostPreviewBitmap();

  // Unified selection system - replaces legacy selection state
  ObjectSelection selection_;

  // Drag infrastructure
  bool is_dragging_ = false;
  ImVec2 drag_start_pos_;
  ImVec2 drag_current_pos_;

  // Hover detection for resize
  size_t hovered_object_index_ = static_cast<size_t>(-1);
  bool has_hovered_object_ = false;

  // Callbacks
  std::function<void(const zelda3::RoomObject&)> object_placed_callback_;
  std::function<void()> cache_invalidation_callback_;
  std::function<void()> mutation_hook_;

  // Clipboard for copy/paste
  std::vector<zelda3::RoomObject> clipboard_;
  bool has_clipboard_data_ = false;

  // Door placement state
  bool door_placement_mode_ = false;
  zelda3::DoorType preview_door_type_ = zelda3::DoorType::NormalDoor;
  zelda3::DoorDirection detected_door_direction_ = zelda3::DoorDirection::North;
  uint8_t snapped_door_position_ = 0;  // Position along wall (0-31)

  // Sprite placement state
  bool sprite_placement_mode_ = false;
  uint8_t preview_sprite_id_ = 0;

  // Item placement state
  bool item_placement_mode_ = false;
  uint8_t preview_item_id_ = 0;

  // Entity selection state (doors, sprites, items)
  SelectedEntity selected_entity_;
  bool is_entity_dragging_ = false;
  bool is_entity_mode_ = false;  // When true, suppress all object interactions
  ImVec2 entity_drag_start_pos_;
  ImVec2 entity_drag_current_pos_;
  std::function<void()> entity_changed_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_OBJECT_INTERACTION_H
