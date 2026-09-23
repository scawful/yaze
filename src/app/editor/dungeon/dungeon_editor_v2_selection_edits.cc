#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <memory>
#include <utility>

#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "util/macro.h"

namespace yaze::editor {
namespace {

class DungeonSelectionAction final : public UndoAction {
 public:
  using Restore = std::function<absl::Status(
      const std::vector<DungeonSelectionEditPlan>&, bool)>;
  DungeonSelectionAction(std::vector<DungeonSelectionEditPlan> plans,
                         std::string description, Restore restore)
      : plans_(std::move(plans)),
        description_(std::move(description)),
        restore_(std::move(restore)) {}
  absl::Status Undo() override { return restore_(plans_, false); }
  absl::Status Redo() override { return restore_(plans_, true); }
  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override {
    auto bytes = [](const DungeonSelectionEditState& state) {
      return state.objects.size() * sizeof(zelda3::RoomObject) +
             state.chests.size() * sizeof(chest_data) +
             state.doors.size() * sizeof(zelda3::Room::Door) +
             state.sprites.size() * sizeof(DungeonSpriteSnapshot) +
             state.items.size() * sizeof(zelda3::PotItem) +
             state.selected_objects.size() * sizeof(size_t) +
             state.selected_entities.size() * sizeof(SelectedEntity);
    };
    size_t total = description_.capacity();
    for (const auto& plan : plans_) {
      total += bytes(plan.before) + bytes(plan.after);
    }
    return total;
  }

 private:
  std::vector<DungeonSelectionEditPlan> plans_;
  std::string description_;
  Restore restore_;
};

}  // namespace

absl::Status DungeonEditorV2::CommitSelectionEdit(
    const DungeonSelectionEditPlan& plan, bool continuous) {
  auto* room = rooms_.GetIfLoaded(plan.room_id);
  if (!room || room->rom() != rom_) {
    return absl::FailedPreconditionError("Selection room is not loaded");
  }
  if (!plan.changed()) {
    return absl::OkStatus();
  }
  if (plan.domains & kSelectionObjects) {
    RETURN_IF_ERROR(PreflightObjectMutation(plan.room_id, plan.after.objects,
                                            plan.after.chests));
  }
  if (continuous && pending_selection_undo_.plan &&
      pending_selection_undo_.plan->room_id != plan.room_id) {
    return absl::FailedPreconditionError(
        "Finish the current selection drag before editing another room");
  }
  if (!continuous) {
    FinalizePendingUndoActions();
  } else if (!pending_selection_undo_.plan) {
    // Close older history before capturing the group gesture. Releasing viewers
    // here would cancel the coordinator's newly started continuous drag.
    if (pending_undo_.room_id >= 0) {
      FinalizeUndoAction(pending_undo_.room_id);
    }
    if (pending_collision_undo_.room_id >= 0) {
      FinalizeCollisionUndoAction(pending_collision_undo_.room_id);
    }
    if (pending_water_fill_undo_.room_id >= 0) {
      FinalizeWaterFillUndoAction(pending_water_fill_undo_.room_id);
    }
    FinalizePendingEntityUndoActions();
  }
  // A release may commit a single-entity preview. Never overwrite that newer
  // state with a candidate planned before its gesture completed.
  const auto current = CaptureDungeonSelectionEditState(*room);
  if (ChangedDungeonSelectionDomains(current, plan.before) & plan.domains) {
    return absl::FailedPreconditionError(
        "Selection changed while finishing the previous gesture; repeat the "
        "edit");
  }

  if (continuous && !pending_selection_undo_.plan) {
    pending_selection_undo_.plan = plan;
    pending_selection_undo_.dirty_before = room->CaptureSaveDirtySnapshot();
  }
  // Publish every affected collection before rendering or updating selection.
  // The planner has already validated every new or changed authored record.
  ApplyDungeonSelectionEditState(*room, plan.after, plan.domains);
  if (continuous) {
    auto& pending = *pending_selection_undo_.plan;
    pending.after = plan.after;
    pending.domains |= plan.domains;
  } else {
    PushSelectionUndoAction(plan);
  }
  RefreshSelectionEditViews(plan.room_id, nullptr);
  return absl::OkStatus();
}

void DungeonEditorV2::PushSelectionUndoAction(DungeonSelectionEditPlan plan) {
  const auto verb = [&]() {
    switch (plan.kind) {
      case DungeonSelectionEditKind::kDelete:
        return "Delete";
      case DungeonSelectionEditKind::kDuplicate:
        return "Duplicate";
      case DungeonSelectionEditKind::kMove:
        return "Move";
      case DungeonSelectionEditKind::kPaste:
        return "Paste";
    }
    return "Edit";
  }();
  const auto description =
      absl::StrFormat("%s room %03X selection", verb, plan.room_id);
  PushSelectionUndoBatch({std::move(plan)}, description);
}

void DungeonEditorV2::PushSelectionUndoBatch(
    std::vector<DungeonSelectionEditPlan> plans, std::string description) {
  undo_manager_.Push(std::make_unique<DungeonSelectionAction>(
      std::move(plans), std::move(description),
      [this](const std::vector<DungeonSelectionEditPlan>& states, bool after) {
        return RestoreSelectionEditBatch(states, after);
      }));
}

void DungeonEditorV2::FinalizeSelectionUndoAction() {
  if (!pending_selection_undo_.plan) {
    return;
  }
  auto pending = std::move(pending_selection_undo_);
  pending_selection_undo_ = {};
  auto plan = std::move(*pending.plan);
  plan.domains &= ChangedDungeonSelectionDomains(plan.before, plan.after);
  if (!plan.changed()) {
    // Returning a drag to its original position is a no-op. Retain existing
    // redo history and restore the original save-dirty state, including any
    // unrelated unsaved work that predated this gesture.
    if (auto* room = rooms_.GetIfLoaded(plan.room_id);
        room && room->rom() == rom_) {
      room->RestoreSaveDirtySnapshot(pending.dirty_before);
    }
    return;
  }
  PushSelectionUndoAction(std::move(plan));
}

absl::Status DungeonEditorV2::RestoreSelectionEditBatch(
    const std::vector<DungeonSelectionEditPlan>& plans, bool after) {
  // Validate every endpoint before restoring any. A missing second room must
  // not leave the first room changed by a failed Undo or Redo.
  for (const auto& plan : plans) {
    const auto* room = rooms_.GetIfLoaded(plan.room_id);
    if (!room || room->rom() != rom_ || room->id() != plan.room_id) {
      return absl::FailedPreconditionError("Selection undo room is not loaded");
    }
  }
  for (const auto& plan : plans) {
    ApplyDungeonSelectionEditState(*rooms_.GetIfLoaded(plan.room_id),
                                   after ? plan.after : plan.before,
                                   plan.domains);
  }
  for (const auto& plan : plans) {
    RefreshSelectionEditViews(plan.room_id, after ? &plan.after : &plan.before);
  }
  undo_restore_triggered_ping_ = true;
  return absl::OkStatus();
}

void DungeonEditorV2::RefreshSelectionEditViews(
    int room_id, const DungeonSelectionEditState* selection) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room)
    return;
  room->MarkObjectsDirty();
  if (rom_ && room->rom() == rom_ && rom_->is_loaded()) {
    room->RenderRoomGraphics();
  }
  auto refresh = [&](DungeonCanvasViewer* viewer) {
    if (!viewer)
      return;
    // A changed door may affect a connected-room graph centered elsewhere.
    viewer->InvalidateConnectedRoomGraph();
    if (viewer->current_room_id() != room_id)
      return;
    if (selection) {
      auto& interaction = viewer->object_interaction();
      interaction.CancelPlacement();
      interaction.SetSelectedObjects(selection->selected_objects);
      interaction.entity_coordinator().SetSelectedEntities(
          selection->selected_entities);
    }
    viewer->TriggerChangePing();
  };
  room_viewers_.ForEach([&](int, std::unique_ptr<DungeonCanvasViewer>& viewer) {
    refresh(viewer.get());
  });
  refresh(workbench_viewer_.get());
  refresh(workbench_compare_viewer_.get());
  if (minecart_track_editor_panel_) {
    minecart_track_editor_panel_->InvalidateRoomAudit();
  }
}

}  // namespace yaze::editor
