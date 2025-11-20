#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_TOOLSET_H
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_TOOLSET_H

#include <array>
#include <functional>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @brief Handles the dungeon editor toolset UI
 *
 * This component manages the toolbar with placement modes, background layer
 * selection, and other editing tools.
 */
class DungeonToolset {
 public:
  enum BackgroundType {
    kNoBackground,
    kBackground1,
    kBackground2,
    kBackground3,
    kBackgroundAny,
  };

  enum PlacementType {
    kNoType,
    kObject,    // Object editing mode
    kSprite,    // Sprite editing mode
    kItem,      // Item placement mode
    kEntrance,  // Entrance/exit editing mode
    kDoor,      // Door configuration mode
    kChest,     // Chest management mode
    kBlock      // Legacy block mode
  };

  DungeonToolset() = default;

  void Draw();

  // Getters
  BackgroundType background_type() const { return background_type_; }
  PlacementType placement_type() const { return placement_type_; }

  // Setters
  void set_background_type(BackgroundType type) { background_type_ = type; }
  void set_placement_type(PlacementType type) { placement_type_ = type; }

  // Callbacks
  void SetUndoCallback(std::function<void()> callback) {
    undo_callback_ = callback;
  }
  void SetRedoCallback(std::function<void()> callback) {
    redo_callback_ = callback;
  }
  void SetPaletteToggleCallback(std::function<void()> callback) {
    palette_toggle_callback_ = callback;
  }

 private:
  BackgroundType background_type_ = kBackgroundAny;
  PlacementType placement_type_ = kNoType;

  // Callbacks for editor actions
  std::function<void()> undo_callback_;
  std::function<void()> redo_callback_;
  std::function<void()> palette_toggle_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_TOOLSET_H
