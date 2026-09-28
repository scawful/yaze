#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <functional>
#include <memory>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"
#include "app/editor/registry/undo_action.h"
#include "util/macro.h"

namespace yaze::editor {

namespace {

class DungeonEntranceCameraRepairAction final : public UndoAction {
 public:
  using ApplyCallback =
      std::function<absl::Status(const DungeonEntranceCameraState&)>;

  DungeonEntranceCameraRepairAction(int slot_index,
                                    DungeonEntranceCameraState before,
                                    DungeonEntranceCameraState after,
                                    ApplyCallback apply)
      : slot_index_(slot_index),
        before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)) {}

  absl::Status Undo() override { return apply_(before_); }
  absl::Status Redo() override { return apply_(after_); }
  std::string Description() const override {
    return absl::StrFormat("Repair entrance camera 0x%02X", slot_index_);
  }
  size_t MemoryUsage() const override {
    return sizeof(*this) + Description().size();
  }

 private:
  int slot_index_;
  DungeonEntranceCameraState before_;
  DungeonEntranceCameraState after_;
  ApplyCallback apply_;
};

}  // namespace

std::optional<DungeonEntranceCameraState>
DungeonEditorV2::GetEntranceCameraState(int slot_index) const {
  if (slot_index < 0 || slot_index >= static_cast<int>(entrances_.size())) {
    return std::nullopt;
  }
  if (slot_index < zelda3::kNumDungeonSpawnPoints) {
    const auto& spawn = spawn_points_[slot_index];
    if (!CanEditDungeonSpawnPoint(slot_index, spawn)) {
      return std::nullopt;
    }
    return CaptureDungeonSpawnCamera(spawn);
  }
  const auto& entrance = entrances_[slot_index];
  if (!CanEditDungeonEntrance(slot_index, entrance)) {
    return std::nullopt;
  }
  return CaptureDungeonEntranceCamera(entrance);
}

absl::Status DungeonEditorV2::RestoreEntranceCameraState(
    int slot_index, const DungeonEntranceCameraState& state) {
  if (slot_index < 0 || slot_index >= static_cast<int>(entrances_.size())) {
    return absl::OutOfRangeError("Dungeon entrance slot is out of range");
  }
  if (slot_index < zelda3::kNumDungeonSpawnPoints) {
    auto& spawn = spawn_points_[slot_index];
    if (!CanEditDungeonSpawnPoint(slot_index, spawn) ||
        state.kind != DungeonEntranceRecordKind::kSpawnPoint) {
      return absl::FailedPreconditionError(
          "Dungeon spawn point is unavailable or mismatched");
    }
    ApplyDungeonSpawnCameraState(state, &spawn);
  } else {
    auto& entrance = entrances_[slot_index];
    if (!CanEditDungeonEntrance(slot_index, entrance) ||
        state.kind != DungeonEntranceRecordKind::kRegularEntrance) {
      return absl::FailedPreconditionError(
          "Dungeon entrance is unavailable or mismatched");
    }
    ApplyDungeonEntranceCameraState(state, &entrance);
  }
  RefreshEntranceCameraOverlays();
  return absl::OkStatus();
}

void DungeonEditorV2::RefreshEntranceCameraOverlays() {
  room_viewers_.ForEach(
      [this](int room_id, std::unique_ptr<DungeonCanvasViewer>& viewer) {
        ConfigureViewerRenderContext(viewer.get(), room_id);
      });
  if (workbench_viewer_ != nullptr) {
    ConfigureViewerRenderContext(workbench_viewer_.get(), current_room_id_);
  }
  if (workbench_compare_viewer_ != nullptr) {
    ConfigureViewerRenderContext(workbench_compare_viewer_.get(),
                                 workbench_compare_viewer_->current_room_id());
  }
}

absl::Status DungeonEditorV2::RepairEntranceCamera(int slot_index) {
  const auto before = GetEntranceCameraState(slot_index);
  if (!before.has_value()) {
    return absl::FailedPreconditionError(
        "Dungeon entrance camera data is unavailable");
  }
  const auto validation = ValidateDungeonEntranceCamera(*before);
  if (!validation.repair_source_valid) {
    return absl::InvalidArgumentError(absl::StrJoin(validation.errors, " "));
  }
  if (validation.matches_derived()) {
    return absl::FailedPreconditionError(
        "Camera data already matches the derived values");
  }

  DungeonEntranceCameraState after =
      BuildRepairedDungeonEntranceCamera(*before);
  DungeonEntranceCameraState comparison = after;
  comparison.dirty = before->dirty;
  if (comparison == *before) {
    return absl::FailedPreconditionError(
        "No safely repairable camera fields differ");
  }
  after.dirty = true;

  RETURN_IF_ERROR(RestoreEntranceCameraState(slot_index, after));
  undo_manager_.Push(std::make_unique<DungeonEntranceCameraRepairAction>(
      slot_index, *before, after,
      [this, slot_index](const DungeonEntranceCameraState& state) {
        return RestoreEntranceCameraState(slot_index, state);
      }));
  return absl::OkStatus();
}

}  // namespace yaze::editor
