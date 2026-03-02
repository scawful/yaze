#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/sprite/zsprite.h"

namespace yaze {
namespace editor {

/**
 * @brief Snapshot of a custom ZSprite's editable state for undo/redo.
 *
 * Captures the entire ZSprite data plus the editor's selection indices,
 * which is sufficient to fully restore the editing state after any mutation
 * to the custom sprite's properties, animations, frames, or routines.
 */
struct SpriteSnapshot {
  int sprite_index = -1;
  zsprite::ZSprite sprite_data;
  int current_frame = 0;
  int current_animation_index = 0;
};

/**
 * @class SpriteEditAction
 * @brief Undoable action for edits to a custom ZSprite.
 *
 * Stores before and after snapshots of the sprite state and uses a restore
 * callback to apply them back to the SpriteEditor. The callback is a
 * std::function so the action doesn't need to know about the editor's
 * internals directly.
 */
class SpriteEditAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(const SpriteSnapshot&)>;

  SpriteEditAction(SpriteSnapshot before, SpriteSnapshot after,
                   RestoreFn restore)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError("SpriteEditAction: no restore callback");
    }
    restore_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError("SpriteEditAction: no restore callback");
    }
    restore_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    if (before_.sprite_data.sprName.empty()) {
      return absl::StrFormat("Edit sprite #%d", before_.sprite_index);
    }
    return absl::StrFormat("Edit sprite '%s'", before_.sprite_data.sprName);
  }

  size_t MemoryUsage() const override {
    // Rough estimate based on frame/tile/routine data
    size_t before_size = 0;
    for (const auto& frame : before_.sprite_data.editor.Frames) {
      before_size += frame.Tiles.size() * sizeof(zsprite::OamTile);
    }
    size_t after_size = 0;
    for (const auto& frame : after_.sprite_data.editor.Frames) {
      after_size += frame.Tiles.size() * sizeof(zsprite::OamTile);
    }
    return before_size + after_size + sizeof(SpriteSnapshot) * 2;
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  SpriteSnapshot before_;
  SpriteSnapshot after_;
  RestoreFn restore_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_
