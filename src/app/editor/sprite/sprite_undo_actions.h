#ifndef YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/undo_action.h"
#include "app/editor/sprite/zsprite.h"

namespace yaze::editor {

struct SpriteSnapshot {
  int sprite_index = -1;
  zsprite::ZSprite sprite;
  std::string sprite_path;
  bool zsm_dirty = false;
  int current_frame = 0;
  int current_animation_index = 0;
  int selected_tile_index = -1;
  int selected_routine_index = -1;
  bool animation_playing = false;
  float frame_timer = 0.0f;
};

class SpriteEditAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(const SpriteSnapshot&)>;

  SpriteEditAction(SpriteSnapshot before, SpriteSnapshot after,
                   RestoreFn restore, std::string description)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)),
        description_(std::move(description)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::FailedPreconditionError("Restore callback is not set");
    }
    restore_(before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::FailedPreconditionError("Restore callback is not set");
    }
    restore_(after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    if (!description_.empty()) {
      return description_;
    }
    return "Edit custom sprite";
  }

  size_t MemoryUsage() const override {
    return EstimateSpriteMemory(before_) + EstimateSpriteMemory(after_);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    return false;
  }

 private:
  static size_t EstimateSpriteMemory(const SpriteSnapshot& snapshot) {
    size_t total = snapshot.sprite_path.size() + snapshot.sprite.sprName.size();
    for (const auto& animation : snapshot.sprite.animations) {
      total += animation.frame_name.size() +
               (animation.Tiles.size() * sizeof(zsprite::OamTile));
    }
    for (const auto& routine : snapshot.sprite.userRoutines) {
      total += routine.name.size() + routine.code.size();
    }
    for (const auto& frame : snapshot.sprite.editor.Frames) {
      total += frame.Tiles.size() * sizeof(zsprite::OamTile);
    }
    return total;
  }

  SpriteSnapshot before_;
  SpriteSnapshot after_;
  RestoreFn restore_;
  std::string description_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_SPRITE_SPRITE_UNDO_ACTIONS_H_
