#ifndef YAZE_APP_EDITOR_GRAPHICS_SCREEN_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_GRAPHICS_SCREEN_UNDO_ACTIONS_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze {
namespace editor {

/**
 * @brief Which screen type the snapshot belongs to.
 */
enum class ScreenEditType {
  kDungeonMap,  // Dungeon map room/gfx layout
  kTile16Edit,  // Tile16 composition modification
};

/**
 * @brief Snapshot of dungeon map editing state for undo/redo.
 *
 * Captures the full DungeonMap for one dungeon (floor_rooms, floor_gfx,
 * boss_room, nbr_of_floor, nbr_of_basement) so that undo restores the
 * previous layout and redo re-applies the change.
 */
struct DungeonMapSnapshot {
  int dungeon_index = 0;
  zelda3::DungeonMap map_data{0, 0, 0, {}, {}};
  std::vector<std::array<std::string, zelda3::kNumRooms>> labels;
};

/**
 * @brief Snapshot of tile16 composition state for undo/redo.
 *
 * Captures the 4 TileInfo entries that compose a single tile16 so that
 * modifications to tile16 definitions can be undone.
 */
struct Tile16CompSnapshot {
  int tile16_id = 0;
  std::array<gfx::TileInfo, 4> tile_info;
};

/**
 * @brief Unified screen editor snapshot.
 *
 * Stores enough data to restore either a dungeon map edit or a tile16
 * composition edit. Only the fields relevant to `edit_type` are populated.
 */
struct ScreenSnapshot {
  ScreenEditType edit_type = ScreenEditType::kDungeonMap;
  DungeonMapSnapshot dungeon_map;
  Tile16CompSnapshot tile16_comp;
};

/**
 * @class ScreenEditAction
 * @brief Undoable action for screen editor edits.
 *
 * Stores before and after snapshots and uses a caller-provided restore
 * callback to apply them back into the ScreenEditor. Follows the same
 * pattern as DungeonObjectsAction and Tile16EditAction.
 */
class ScreenEditAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(const ScreenSnapshot&)>;

  ScreenEditAction(ScreenSnapshot before, ScreenSnapshot after,
                   RestoreFn restore, std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError("ScreenEditAction: no restore callback");
    }
    restore_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError("ScreenEditAction: no restore callback");
    }
    restore_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override { return description_; }

  size_t MemoryUsage() const override {
    size_t size = sizeof(before_) + sizeof(after_);
    // Account for vector storage in dungeon map snapshots
    for (const auto& floor : before_.dungeon_map.map_data.floor_rooms)
      size += floor.size();
    for (const auto& floor : before_.dungeon_map.map_data.floor_gfx)
      size += floor.size();
    for (const auto& floor : after_.dungeon_map.map_data.floor_rooms)
      size += floor.size();
    for (const auto& floor : after_.dungeon_map.map_data.floor_gfx)
      size += floor.size();
    return size;
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  ScreenSnapshot before_;
  ScreenSnapshot after_;
  RestoreFn restore_;
  std::string description_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_UNDO_ACTIONS_H_
