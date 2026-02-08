#ifndef YAZE_APP_EDITOR_DUNGEON_INTERACTION_TILE_OBJECT_HANDLER_H
#define YAZE_APP_EDITOR_DUNGEON_INTERACTION_TILE_OBJECT_HANDLER_H

#include <vector>
#include "app/editor/dungeon/interaction/base_entity_handler.h"
#include "app/editor/dungeon/interaction/interaction_context.h"
#include "app/gfx/render/background_buffer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::editor {

/**
 * @brief Handles functional mutations and queries for tile objects.
 * 
 * This class abstracts the raw manipulation of zelda3::RoomObject vectors,
 * providing a cleaner API for the UI and future CLI.
 */
class TileObjectHandler : public BaseEntityHandler {
 public:
  TileObjectHandler() : ghost_preview_buffer_(nullptr) {}
  explicit TileObjectHandler(InteractionContext* ctx) { SetContext(ctx); }

  // ========================================================================
  // BaseEntityHandler interface
  // ========================================================================
  
  void BeginPlacement() override;
  void CancelPlacement() override;
  bool IsPlacementActive() const override { return object_placement_mode_; }
  
  bool HandleClick(int canvas_x, int canvas_y) override;
  void HandleDrag(ImVec2 current_pos, ImVec2 delta) override;
  void HandleRelease() override;
  bool HandleMouseWheel(float delta) override;
  
  void DrawGhostPreview() override;
  void DrawSelectionHighlight() override;
  
  void InitDrag(const ImVec2& start_pos);

  // ========================================================================
  // Marquee (Rectangle) Selection
  // ========================================================================

  // Begin a rectangle selection drag on empty space (canvas-local coords).
  void BeginMarqueeSelection(const ImVec2& start_pos);

  // Update/draw active marquee selection and finalize on mouse release.
  void HandleMarqueeSelection(const ImVec2& mouse_pos, bool mouse_left_down,
                              bool mouse_left_released, bool shift_down,
                              bool toggle_down, bool alt_down,
                              bool draw_box = true);
  
  std::optional<size_t> GetEntityAtPosition(int canvas_x, int canvas_y) const override;

  /**
   * @brief Move a set of objects by a tile delta.
   */
  void MoveObjects(int room_id, const std::vector<size_t>& indices, int delta_x,
                   int delta_y, bool notify_mutation = true);

  /**
   * @brief Clone a set of objects and move them by a tile delta.
   * @return Indices of the newly created clones.
   */
  std::vector<size_t> DuplicateObjects(int room_id,
                                       const std::vector<size_t>& indices,
                                       int delta_x, int delta_y,
                                       bool notify_mutation = true);

  /**
   * @brief Delete objects by indices.
   */
  void DeleteObjects(int room_id, std::vector<size_t> indices);

  /**
   * @brief Delete all objects in a room.
   */
  void DeleteAllObjects(int room_id);

  /**
   * @brief Reorder objects.
   */
  void SendToFront(int room_id, const std::vector<size_t>& indices);
  void SendToBack(int room_id, const std::vector<size_t>& indices);
  void MoveForward(int room_id, const std::vector<size_t>& indices);
  void MoveBackward(int room_id, const std::vector<size_t>& indices);

  /**
   * @brief Resize objects by a delta.
   */
  void ResizeObjects(int room_id, const std::vector<size_t>& indices, int delta);

  /**
   * @brief Place a new object.
   */
  void PlaceObjectAt(int room_id, const zelda3::RoomObject& object, int x, int y);

  void UpdateObjectsId(int room_id, const std::vector<size_t>& indices, int16_t new_id);

  void UpdateObjectsSize(int room_id, const std::vector<size_t>& indices, uint8_t new_size);

  void UpdateObjectsLayer(int room_id, const std::vector<size_t>& indices, int new_layer);

  /**
   * @brief Set object for placement.
   */
  void SetPreviewObject(const zelda3::RoomObject& object);

  // ========================================================================
  // Clipboard Operations
  // ========================================================================

  /**
   * @brief Copy objects to internal clipboard.
   */
  void CopyObjectsToClipboard(int room_id, const std::vector<size_t>& indices);

  /**
   * @brief Paste objects from clipboard with offset.
   * @return Indices of newly pasted objects.
   */
  std::vector<size_t> PasteFromClipboard(int room_id, int offset_x, int offset_y);

  /**
   * @brief Paste objects from clipboard at target location.
   * Use first clipboard item as origin.
   */
  std::vector<size_t> PasteFromClipboardAt(int room_id, int target_x, int target_y);

  /**
   * @brief Check if clipboard has data.
   */
  bool HasClipboardData() const { return !clipboard_.empty(); }

  /**
   * @brief Clear the clipboard.
   */
  void ClearClipboard() { clipboard_.clear(); }

 private:
  // Placement state
  bool object_placement_mode_ = false;
  zelda3::RoomObject preview_object_{-1, 0, 0, 0};
  std::unique_ptr<gfx::BackgroundBuffer> ghost_preview_buffer_;

  // Clipboard
  std::vector<zelda3::RoomObject> clipboard_;

  void RenderGhostPreviewBitmap();
  std::pair<int, int> CalculateObjectBounds(const zelda3::RoomObject& object);
  
  // Drag state
  bool is_dragging_ = false;
  ImVec2 drag_start_{0, 0};
  ImVec2 drag_current_{0, 0};
  int drag_last_dx_ = 0;
  int drag_last_dy_ = 0;
  bool drag_has_duplicated_ = false;
  bool drag_mutation_started_ = false;

  ImVec2 ApplyDragModifiers(const ImVec2& delta) const;

  zelda3::Room* GetRoom(int room_id);
  void NotifyChange(zelda3::Room* room);
};

} // namespace yaze::editor

#endif // YAZE_APP_EDITOR_DUNGEON_INTERACTION_TILE_OBJECT_HANDLER_H
