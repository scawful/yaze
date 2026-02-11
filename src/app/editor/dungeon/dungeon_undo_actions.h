#ifndef YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

/**
 * @class DungeonObjectsAction
 * @brief Undoable action for dungeon room object edits.
 *
 * Captures a full snapshot of a room's tile objects before and after
 * an editing operation. Undo restores the before-state, Redo restores
 * the after-state, using a caller-provided restore callback that
 * applies the snapshot back into the room.
 */
class DungeonObjectsAction : public UndoAction {
 public:
  using RestoreFn = std::function<void(int room_id,
                                       const std::vector<zelda3::RoomObject>&)>;

  DungeonObjectsAction(int room_id,
                       std::vector<zelda3::RoomObject> before,
                       std::vector<zelda3::RoomObject> after,
                       RestoreFn restore)
      : room_id_(room_id),
        before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}

  absl::Status Undo() override {
    if (!restore_) {
      return absl::InternalError("DungeonObjectsAction: no restore callback");
    }
    restore_(room_id_, before_);
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    if (!restore_) {
      return absl::InternalError("DungeonObjectsAction: no restore callback");
    }
    restore_(room_id_, after_);
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit room %03X objects", room_id_);
  }

  size_t MemoryUsage() const override {
    // Rough estimate: each RoomObject is ~40-80 bytes
    return (before_.size() + after_.size()) * sizeof(zelda3::RoomObject);
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override {
    // Object edits are already grouped per mutation (drag, delete, etc.)
    // so merging is not needed.
    return false;
  }

 private:
  int room_id_;
  std::vector<zelda3::RoomObject> before_;
  std::vector<zelda3::RoomObject> after_;
  RestoreFn restore_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_UNDO_ACTIONS_H_
