#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <functional>
#include <memory>
#include <tuple>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/dungeon/dungeon_entrance_edit_policy.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/editor/registry/undo_action.h"
#include "imgui/imgui_internal.h"
#include "util/macro.h"

namespace yaze::editor {

namespace {

bool SameProperties(const zelda3::RoomEntrance& a,
                    const zelda3::RoomEntrance& b) {
  auto a_camera = CaptureDungeonEntranceCamera(a);
  auto b_camera = CaptureDungeonEntranceCamera(b);
  a_camera.dirty = b_camera.dirty = false;
  return a_camera == b_camera &&
         std::tie(a.entrance_id_, a.blockset_, a.floor_, a.dungeon_id_,
                  a.ladder_bg_, a.scrolling_, a.exit_, a.music_, a.door_) ==
             std::tie(b.entrance_id_, b.blockset_, b.floor_, b.dungeon_id_,
                      b.ladder_bg_, b.scrolling_, b.exit_, b.music_, b.door_);
}

bool SameProperties(const zelda3::DungeonSpawnPoint& a,
                    const zelda3::DungeonSpawnPoint& b) {
  auto a_camera = CaptureDungeonSpawnCamera(a);
  auto b_camera = CaptureDungeonSpawnCamera(b);
  a_camera.dirty = b_camera.dirty = false;
  return a.spawn_id() == b.spawn_id() && a_camera == b_camera &&
         std::tie(a.entrance_id, a.main_gfx, a.floor, a.dungeon_id, a.layer,
                  a.camera_scroll_controller, a.overworld_door_tilemap,
                  a.song) == std::tie(b.entrance_id, b.main_gfx, b.floor,
                                      b.dungeon_id, b.layer,
                                      b.camera_scroll_controller,
                                      b.overworld_door_tilemap, b.song);
}

template <typename Record>
class DungeonEntrancePropertiesAction final : public UndoAction {
 public:
  using ApplyCallback = std::function<absl::Status(const Record&)>;

  DungeonEntrancePropertiesAction(int slot_index, const Record& before,
                                  const Record& after, ApplyCallback apply,
                                  std::shared_ptr<const int> gesture,
                                  uint64_t save_generation)
      : slot_index_(slot_index),
        before_(before),
        after_(after),
        apply_(std::move(apply)),
        gesture_(std::move(gesture)),
        save_generation_(save_generation) {}

  absl::Status Undo() override { return apply_(before_); }
  absl::Status Redo() override { return apply_(after_); }
  std::string Description() const override {
    return absl::StrFormat("Edit entrance properties 0x%02X", slot_index_);
  }
  size_t MemoryUsage() const override {
    return sizeof(*this) + Description().size();
  }
  bool CanMergeWith(const UndoAction& previous) const override {
    const auto* prev =
        dynamic_cast<const DungeonEntrancePropertiesAction*>(&previous);
    // A successful save starts a new undo step even if the field stays active
    // or the gesture happened to return the record to its saved value.
    return prev && gesture_ && gesture_ == prev->gesture_ &&
           save_generation_ == prev->save_generation_ &&
           slot_index_ == prev->slot_index_ &&
           before_.dirty() == prev->after_.dirty() &&
           SameProperties(before_, prev->after_);
  }
  void MergeWith(UndoAction& previous) override {
    before_ = static_cast<DungeonEntrancePropertiesAction&>(previous).before_;
  }

 private:
  int slot_index_;
  Record before_;
  Record after_;
  ApplyCallback apply_;
  std::shared_ptr<const int> gesture_;
  uint64_t save_generation_;
};

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

void DungeonEntrancesPanel::ObservePropertyEditGesture() {
  const ImGuiID active_id = ImGui::GetActiveID();
  if (active_id != property_edit_active_id_) {
    property_edit_active_id_ = active_id;
    property_edit_gesture_ =
        active_id != 0 ? std::make_shared<const int>(0) : nullptr;
  }
}

void DungeonEntrancesPanel::RegisterPropertyInput(const char* field_label,
                                                  const char* widget_label) {
  // InputHex groups have no ID while inactive; register their real text input
  // ID with the group bounds so automation can find them before the first click.
  ImGui::PushID(field_label);
  const ImGuiID input_id = ImGui::GetID("");
  ImGui::PopID();
  const auto& item = ImGui::GetCurrentContext()->LastItemData;
  gui::WidgetIdRegistry::WidgetMetadata metadata;
  metadata.label = widget_label;
  metadata.visible = ImGui::IsItemVisible();
  metadata.enabled = (item.ItemFlags & ImGuiItemFlags_Disabled) == 0;
  metadata.bounds = gui::WidgetIdRegistry::WidgetBounds{
      item.Rect.Min.x, item.Rect.Min.y, item.Rect.Max.x, item.Rect.Max.y, true};
  gui::WidgetIdRegistry::Instance().RegisterWidget(
      "Dungeon/Entrances/input:" +
          gui::WidgetIdRegistry::NormalizeLabel(widget_label),
      "input", input_id, "", metadata);
}

void DungeonEditorV2::ConfigureEntrancePropertyEditCallbacks(
    DungeonEntrancesPanel* panel) {
  panel->SetPropertyEditCallbacks(
      [this, panel](int slot, const zelda3::RoomEntrance& before,
                    const zelda3::RoomEntrance& after) {
        RecordEntrancePropertyEdit(slot, before, after,
                                   panel->property_edit_gesture());
      },
      [this, panel](int slot, const zelda3::DungeonSpawnPoint& before,
                    const zelda3::DungeonSpawnPoint& after) {
        RecordSpawnPropertyEdit(slot, before, after,
                                panel->property_edit_gesture());
      });
  panel->SetCameraTools(
      [this](int slot) { return GetEntranceCameraState(slot); },
      [this](int slot) { return RepairEntranceCamera(slot); },
      [this]() { RefreshEntranceCameraOverlays(); });
}

void DungeonEditorV2::RecordEntrancePropertyEdit(
    int slot_index, const zelda3::RoomEntrance& before,
    const zelda3::RoomEntrance& after, std::shared_ptr<const int> gesture) {
  if (!CanEditDungeonEntrance(slot_index, before) ||
      !CanEditDungeonEntrance(slot_index, after) ||
      SameProperties(before, after)) {
    return;
  }
  FinalizePendingUndoActions();
  RefreshEntranceDirtyState(slot_index);
  undo_manager_.Push(
      std::make_unique<DungeonEntrancePropertiesAction<zelda3::RoomEntrance>>(
          slot_index, before, after,
          [this, slot_index](const zelda3::RoomEntrance& record) {
            return RestoreEntranceProperties(slot_index, record);
          },
          std::move(gesture), entrance_property_save_generation_));
}

void DungeonEditorV2::RecordSpawnPropertyEdit(
    int slot_index, const zelda3::DungeonSpawnPoint& before,
    const zelda3::DungeonSpawnPoint& after,
    std::shared_ptr<const int> gesture) {
  if (!CanEditDungeonSpawnPoint(slot_index, before) ||
      !CanEditDungeonSpawnPoint(slot_index, after) ||
      SameProperties(before, after)) {
    return;
  }
  FinalizePendingUndoActions();
  RefreshEntranceDirtyState(slot_index);
  undo_manager_.Push(
      std::make_unique<
          DungeonEntrancePropertiesAction<zelda3::DungeonSpawnPoint>>(
          slot_index, before, after,
          [this, slot_index](const zelda3::DungeonSpawnPoint& record) {
            return RestoreSpawnProperties(slot_index, record);
          },
          std::move(gesture), entrance_property_save_generation_));
}

absl::Status DungeonEditorV2::RestoreEntranceProperties(
    int slot_index, const zelda3::RoomEntrance& record) {
  if (!CanEditDungeonEntrance(slot_index, record)) {
    return absl::OutOfRangeError("Dungeon entrance slot is out of range");
  }
  auto& entrance = entrances_[slot_index];
  if (!CanEditDungeonEntrance(slot_index, entrance) ||
      entrance.entrance_id_ != record.entrance_id_) {
    return absl::FailedPreconditionError("Dungeon entrance is mismatched");
  }
  entrance = record;
  RefreshEntranceDirtyState(slot_index);
  RefreshEntranceCameraOverlays();
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::RestoreSpawnProperties(
    int slot_index, const zelda3::DungeonSpawnPoint& record) {
  if (!CanEditDungeonSpawnPoint(slot_index, record)) {
    return absl::OutOfRangeError("Dungeon spawn slot is out of range");
  }
  auto& spawn = spawn_points_[slot_index];
  if (!CanEditDungeonSpawnPoint(slot_index, spawn)) {
    return absl::FailedPreconditionError("Dungeon spawn point is unavailable");
  }
  spawn = record;
  RefreshEntranceDirtyState(slot_index);
  RefreshEntranceCameraOverlays();
  return absl::OkStatus();
}

void DungeonEditorV2::RefreshEntranceDirtyState(int slot_index) {
  // Saving keeps history but clears dirty flags. Compare against the current
  // ROM, rather than restoring a flag captured before that save.
  if (slot_index < zelda3::kNumDungeonSpawnPoints) {
    auto& spawn = spawn_points_[slot_index];
    if (rom_ != nullptr && rom_->is_loaded()) {
      const auto saved = zelda3::DungeonSpawnPoint::Load(*rom_, slot_index);
      if (saved.ok() && SameProperties(spawn, *saved)) {
        spawn.ClearDirty();
        return;
      }
    }
    spawn.MarkDirty();
  } else {
    auto& entrance = entrances_[slot_index];
    if (rom_ != nullptr && rom_->is_loaded()) {
      const zelda3::RoomEntrance saved(
          rom_, slot_index - zelda3::kNumDungeonSpawnPoints, false);
      if (saved.load_status().ok() && SameProperties(entrance, saved)) {
        entrance.ClearDirty();
        return;
      }
    }
    entrance.MarkDirty();
  }
}

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
  RefreshEntranceDirtyState(slot_index);
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

  FinalizePendingUndoActions();
  RETURN_IF_ERROR(RestoreEntranceCameraState(slot_index, after));
  undo_manager_.Push(std::make_unique<DungeonEntranceCameraRepairAction>(
      slot_index, *before, after,
      [this, slot_index](const DungeonEntranceCameraState& state) {
        return RestoreEntranceCameraState(slot_index, state);
      }));
  return absl::OkStatus();
}

}  // namespace yaze::editor
