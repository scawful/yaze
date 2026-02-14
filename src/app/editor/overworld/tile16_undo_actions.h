#ifndef YAZE_APP_EDITOR_OVERWORLD_TILE16_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_OVERWORLD_TILE16_UNDO_ACTIONS_H_

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace editor {

/**
 * @brief Snapshot of a Tile16's editable state for undo/redo.
 *
 * Captures all the data needed to fully restore a Tile16 editing state:
 * pixel data, palette, flip/priority flags, and the tile ID.
 */
struct Tile16Snapshot {
  int tile_id = 0;
  std::vector<uint8_t> bitmap_data;
  gfx::SnesPalette bitmap_palette;
  gfx::Tile16 tile_data;
  uint8_t palette = 0;
  bool x_flip = false;
  bool y_flip = false;
  bool priority = false;
};

/**
 * @class Tile16EditAction
 * @brief Undoable action for edits to a single Tile16.
 *
 * Stores before and after snapshots of the tile state and uses a restore
 * callback to apply them back to the Tile16Editor. The callback is a
 * std::function so the action doesn't need to know about the editor's
 * internals directly.
 */
class Tile16EditAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(const Tile16Snapshot&)>;

  Tile16EditAction(Tile16Snapshot before, Tile16Snapshot after,
                   RestoreFn restore)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    restore_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    restore_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit tile16 #%d", before_.tile_id);
  }

  size_t MemoryUsage() const override {
    return before_.bitmap_data.size() + after_.bitmap_data.size();
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    return false;
  }

 private:
  Tile16Snapshot before_;
  Tile16Snapshot after_;
  RestoreFn restore_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_OVERWORLD_TILE16_UNDO_ACTIONS_H_
