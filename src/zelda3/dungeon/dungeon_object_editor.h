#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_OBJECT_EDITOR_H
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_OBJECT_EDITOR_H

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/platform/window.h"
#include "app/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Interactive dungeon object editor with scroll wheel support
 *
 * This class provides a comprehensive object editing system for dungeon rooms,
 * including:
 * - Object insertion and deletion
 * - Object size editing with scroll wheel
 * - Object position editing with mouse
 * - Layer management
 * - Real-time preview and validation
 * - Undo/redo functionality
 * - Object property editing
 */
class DungeonObjectEditor {
 public:
  // Editor modes
  enum class Mode {
    kSelect,  // Select and move objects
    kInsert,  // Insert new objects
    kDelete,  // Delete objects
    kEdit,    // Edit object properties
    kLayer,   // Layer management
    kPreview  // Preview mode
  };

  // Object selection state
  struct SelectionState {
    std::vector<size_t> selected_objects;  // Indices of selected objects
    bool is_multi_select = false;
    bool is_dragging = false;
    int drag_start_x = 0;
    int drag_start_y = 0;
  };

  // Object editing state
  struct EditingState {
    Mode current_mode = Mode::kSelect;
    int current_layer = 0;
    int current_object_type = 0x10;  // Default to wall
    int scroll_wheel_delta = 0;
    bool is_editing_size = false;
    bool is_editing_position = false;
    int preview_x = 0;
    int preview_y = 0;
    int preview_size = 0x12;  // Default size
  };

  // Editor configuration
  struct EditorConfig {
    bool snap_to_grid = true;
    int grid_size = 16;  // 16x16 pixel grid
    bool show_grid = true;
    bool show_preview = true;
    bool auto_save = false;
    int auto_save_interval = 300;  // 5 minutes
    bool validate_objects = true;
    bool show_collision_bounds = false;

    // Phase 4: Visual feedback settings
    bool show_selection_highlight = true;
    bool show_layer_colors = true;
    bool show_property_panel = true;
    uint32_t selection_color = 0xFFFFFF00;  // Yellow
    uint32_t layer0_color = 0xFFFF0000;     // Red tint
    uint32_t layer1_color = 0xFF00FF00;     // Green tint
    uint32_t layer2_color = 0xFF0000FF;     // Blue tint
  };

  // Undo/Redo system
  struct UndoPoint {
    std::vector<RoomObject> objects;
    SelectionState selection;
    EditingState editing;
    std::chrono::steady_clock::time_point timestamp;
  };

  explicit DungeonObjectEditor(Rom* rom);
  ~DungeonObjectEditor() = default;

  // Core editing operations
  absl::Status LoadRoom(int room_id);
  absl::Status SaveRoom();
  absl::Status ClearRoom();

  // Object manipulation
  absl::Status InsertObject(int x, int y, int object_type, int size = 0x12,
                            int layer = 0);
  absl::Status DeleteObject(size_t object_index);
  absl::Status DeleteSelectedObjects();
  absl::Status MoveObject(size_t object_index, int new_x, int new_y);
  absl::Status ResizeObject(size_t object_index, int new_size);
  absl::Status ChangeObjectType(size_t object_index, int new_type);
  absl::Status ChangeObjectLayer(size_t object_index, int new_layer);

  // Selection management
  absl::Status SelectObject(int screen_x, int screen_y);
  absl::Status SelectObjects(int start_x, int start_y, int end_x, int end_y);
  absl::Status ClearSelection();
  absl::Status AddToSelection(size_t object_index);
  absl::Status RemoveFromSelection(size_t object_index);

  // Mouse and scroll wheel handling
  absl::Status HandleMouseClick(int x, int y, bool left_button,
                                bool right_button, bool shift_pressed);
  absl::Status HandleMouseDrag(int start_x, int start_y, int current_x,
                               int current_y);
  absl::Status HandleMouseRelease(int x,
                                  int y);  // Phase 4: End drag operations
  absl::Status HandleScrollWheel(int delta, int x, int y, bool ctrl_pressed);
  absl::Status HandleKeyPress(int key_code, bool ctrl_pressed,
                              bool shift_pressed);

  // Mode management
  void SetMode(Mode mode);
  Mode GetMode() const { return editing_state_.current_mode; }

  // Layer management
  void SetCurrentLayer(int layer);
  int GetCurrentLayer() const { return editing_state_.current_layer; }
  absl::StatusOr<std::vector<RoomObject>> GetObjectsByLayer(int layer);
  absl::Status MoveObjectToLayer(size_t object_index, int layer);

  // Object type management
  void SetCurrentObjectType(int object_type);
  int GetCurrentObjectType() const {
    return editing_state_.current_object_type;
  }
  absl::StatusOr<std::vector<int>> GetAvailableObjectTypes();
  absl::Status ValidateObjectType(int object_type);

  // Rendering and preview
  absl::StatusOr<gfx::Bitmap> RenderPreview(int x, int y);
  void SetPreviewPosition(int x, int y);
  void UpdatePreview();

  // Phase 4: Visual feedback and GUI
  void RenderSelectionHighlight(gfx::Bitmap& canvas);
  void RenderLayerVisualization(gfx::Bitmap& canvas);
  void RenderObjectPropertyPanel();  // ImGui panel
  void RenderLayerControls();        // ImGui controls
  absl::Status HandleDragOperation(int current_x, int current_y);

  // Undo/Redo functionality
  absl::Status Undo();
  absl::Status Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void ClearHistory();

  // Configuration
  void SetROM(Rom* rom);
  void SetConfig(const EditorConfig& config);
  EditorConfig GetConfig() const { return config_; }
  void SetSnapToGrid(bool enabled);
  void SetGridSize(int size);
  void SetShowGrid(bool enabled);

  // Validation and error checking
  absl::Status ValidateRoom();
  absl::Status ValidateObject(const RoomObject& object);
  std::vector<std::string> GetValidationErrors();

  // Event callbacks
  using ObjectChangedCallback =
      std::function<void(size_t object_index, const RoomObject& object)>;
  using RoomChangedCallback = std::function<void()>;
  using SelectionChangedCallback = std::function<void(const SelectionState&)>;

  void SetObjectChangedCallback(ObjectChangedCallback callback);
  void SetRoomChangedCallback(RoomChangedCallback callback);
  void SetSelectionChangedCallback(SelectionChangedCallback callback);

  // Getters
  const Room& GetRoom() const { return *current_room_; }
  Room* GetMutableRoom() { return current_room_.get(); }
  const SelectionState& GetSelection() const { return selection_state_; }
  const EditingState& GetEditingState() const { return editing_state_; }
  size_t GetObjectCount() const {
    return current_room_ ? current_room_->GetTileObjects().size() : 0;
  }
  const std::vector<RoomObject>& GetObjects() const {
    return current_room_ ? current_room_->GetTileObjects() : empty_objects_;
  }

 private:
  // Internal helper methods
  absl::Status InitializeEditor();
  absl::Status CreateUndoPoint();
  absl::Status ApplyUndoPoint(const UndoPoint& undo_point);

  // Coordinate conversion
  std::pair<int, int> ScreenToRoomCoordinates(int screen_x, int screen_y);
  std::pair<int, int> RoomToScreenCoordinates(int room_x, int room_y);
  int SnapToGrid(int coordinate);

  // Object finding and collision detection
  std::optional<size_t> FindObjectAt(int room_x, int room_y);
  std::vector<size_t> FindObjectsInArea(int start_x, int start_y, int end_x,
                                        int end_y);
  bool IsObjectAtPosition(const RoomObject& object, int x, int y);
  bool ObjectsCollide(const RoomObject& obj1, const RoomObject& obj2);

  // Preview and rendering helpers
  absl::StatusOr<gfx::Bitmap> RenderObjectPreview(int object_type, int x, int y,
                                                  int size);
  void UpdatePreviewObject();
  absl::Status ValidatePreviewPosition(int x, int y);

  // Size editing with scroll wheel
  absl::Status HandleSizeEdit(int delta, int x, int y);
  int GetNextSize(int current_size, int delta);
  int GetPreviousSize(int current_size, int delta);
  bool IsValidSize(int size);

  // Member variables
  Rom* rom_;
  std::unique_ptr<Room> current_room_;

  SelectionState selection_state_;
  EditingState editing_state_;
  EditorConfig config_;

  std::vector<UndoPoint> undo_history_;
  std::vector<UndoPoint> redo_history_;
  static constexpr size_t kMaxUndoHistory = 50;

  // Preview system
  std::optional<RoomObject> preview_object_;
  bool preview_visible_ = false;

  // Event callbacks
  ObjectChangedCallback object_changed_callback_;
  RoomChangedCallback room_changed_callback_;
  SelectionChangedCallback selection_changed_callback_;

  // Constants
  static constexpr int kMinObjectSize = 0x00;
  static constexpr int kMaxObjectSize = 0xFF;
  static constexpr int kDefaultObjectSize = 0x12;
  static constexpr int kMinLayer = 0;
  static constexpr int kMaxLayer = 2;

  // Empty objects vector for const getter
  std::vector<RoomObject> empty_objects_;
};

/**
 * @brief Factory function to create dungeon object editor
 */
std::unique_ptr<DungeonObjectEditor> CreateDungeonObjectEditor(Rom* rom);

/**
 * @brief Object type categories for easier selection
 */
namespace ObjectCategories {

struct ObjectCategory {
  std::string name;
  std::vector<int> object_ids;
  std::string description;
};

/**
 * @brief Get all available object categories
 */
std::vector<ObjectCategory> GetObjectCategories();

/**
 * @brief Get objects in a specific category
 */
absl::StatusOr<std::vector<int>> GetObjectsInCategory(
    const std::string& category_name);

/**
 * @brief Get category for a specific object
 */
absl::StatusOr<std::string> GetObjectCategory(int object_id);

/**
 * @brief Get object information
 */
struct ObjectInfo {
  int id;
  std::string name;
  std::string description;
  std::vector<std::pair<int, int>> valid_sizes;
  std::vector<int> valid_layers;
  bool is_interactive;
  bool is_collidable;
};

absl::StatusOr<ObjectInfo> GetObjectInfo(int object_id);

}  // namespace ObjectCategories

/**
 * @brief Scroll wheel behavior configuration
 */
struct ScrollWheelConfig {
  bool enabled = true;
  int sensitivity = 1;  // How much size changes per scroll
  int min_size = 0x00;
  int max_size = 0xFF;
  bool wrap_around = false;  // Wrap from max to min
  bool smooth_scrolling = true;
  int smooth_factor = 2;  // Divide delta by this for smoother scrolling
};

/**
 * @brief Mouse interaction configuration
 */
struct MouseConfig {
  bool left_click_select = true;
  bool right_click_context = true;
  bool middle_click_drag = false;
  bool drag_to_select = true;
  bool snap_drag_to_grid = true;
  int double_click_threshold = 500;  // milliseconds
  int drag_threshold = 5;            // pixels before drag starts
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_OBJECT_EDITOR_H
