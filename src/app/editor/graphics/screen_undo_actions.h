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
#include "zelda3/screen/dungeon_map.h"

namespace yaze::editor {

struct ScreenSnapshot {
  std::vector<zelda3::DungeonMap> dungeon_maps;
  zelda3::DungeonMapLabels dungeon_map_labels;
  int selected_dungeon = 0;
  int floor_number = 0;
  uint8_t selected_room = 0;
};

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
    return "Edit screen data";
  }

  size_t MemoryUsage() const override {
    return EstimateSnapshotMemory(before_) + EstimateSnapshotMemory(after_) +
           description_.size();
  }

  bool CanMergeWith(const UndoAction& /*prev*/) const override { return false; }

 private:
  static size_t EstimateSnapshotMemory(const ScreenSnapshot& snapshot) {
    size_t total = sizeof(ScreenSnapshot);
    for (const auto& map : snapshot.dungeon_maps) {
      total += sizeof(zelda3::DungeonMap);
      total += map.floor_rooms.size() *
               sizeof(std::array<uint8_t, zelda3::kNumRooms>);
      total +=
          map.floor_gfx.size() * sizeof(std::array<uint8_t, zelda3::kNumRooms>);
    }
    for (const auto& dungeon_labels : snapshot.dungeon_map_labels) {
      total += dungeon_labels.size() *
               sizeof(std::array<std::string, zelda3::kNumRooms>);
      for (const auto& floor_labels : dungeon_labels) {
        for (const auto& label : floor_labels) {
          total += label.size();
        }
      }
    }
    return total;
  }

  ScreenSnapshot before_;
  ScreenSnapshot after_;
  RestoreFn restore_;
  std::string description_;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_GRAPHICS_SCREEN_UNDO_ACTIONS_H_
