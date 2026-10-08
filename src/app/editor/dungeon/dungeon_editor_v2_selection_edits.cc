#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <memory>
#include <utility>

#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "core/dungeon_stream_layout_adapter.h"
#include "core/features.h"
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

absl::Status DungeonEditorV2::PreflightSpriteMutation(
    int room_id, const std::vector<zelda3::Sprite>& sprites) {
  const auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_) {
    return absl::FailedPreconditionError("Sprite room is not loaded");
  }
  auto candidate = CaptureDungeonSelectionEditState(*room);
  candidate.sprites.clear();
  for (const auto& sprite : sprites) {
    candidate.sprites.push_back({sprite.id(), sprite.x(), sprite.y(),
                                 sprite.subtype(), sprite.layer(),
                                 sprite.key_drop(), sprite.deleted()});
  }
  return PreflightStreamMutation(room_id, candidate, kSelectionSprites);
}

absl::Status DungeonEditorV2::PreflightStreamMutation(
    int room_id, const DungeonSelectionEditState& candidate, uint8_t domains) {
  const auto* original = rooms_.GetIfLoaded(room_id);
  if (!original || original->rom() != rom_) {
    return absl::FailedPreconditionError("Stream room is not loaded");
  }
  constexpr uint8_t streams =
      kSelectionObjects | kSelectionDoors | kSelectionSprites;
  if (!(domains & streams)) {
    return absl::OkStatus();
  }
  // Compare encoded bytes: torches and blocks live outside the object stream,
  // while doors and hidden sprite key records contribute to its true size.
  zelda3::Room probe(room_id, rom_);
  ApplyDungeonSelectionEditState(
      probe, CaptureDungeonSelectionEditState(*original), streams);
  ApplyDungeonSelectionEditState(probe, candidate, domains & streams);
  const bool objects_changed =
      (domains & (kSelectionObjects | kSelectionDoors)) &&
      probe.EncodeObjects() != original->EncodeObjects();
  const bool sprites_changed =
      (domains & kSelectionSprites) &&
      probe.EncodeSprites() != original->EncodeSprites();
  if (!objects_changed && !sprites_changed) {
    return absl::OkStatus();
  }

  uint8_t changed = objects_changed ? kSelectionObjects | kSelectionDoors : 0;
  if (sprites_changed)
    changed |= kSelectionSprites;
  ASSIGN_OR_RETURN(auto ranges,
                   PlanPendingRoomStreamRanges(room_id, &candidate, changed));
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        dependencies_.project->hack_manifest,
        dependencies_.project->rom_metadata.write_policy, ranges,
        "dungeon stream authoring", "DungeonEditorV2", nullptr));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::vector<std::pair<uint32_t, uint32_t>>>
DungeonEditorV2::PlanPendingRoomStreamRanges(
    int room_id, const DungeonSelectionEditState* candidate,
    uint8_t domains) const {
  constexpr uint8_t streams =
      kSelectionObjects | kSelectionDoors | kSelectionSprites;
  const auto& flags = core::FeatureFlags::get().dungeon;
  const bool objects_enabled =
      flags.kSaveObjects || (domains & (kSelectionObjects | kSelectionDoors));
  const bool sprites_enabled =
      flags.kSaveSprites || (domains & kSelectionSprites);
  bool objects_pending = domains & (kSelectionObjects | kSelectionDoors);
  bool sprites_pending = domains & kSelectionSprites;
  rooms_.ForEachLoaded([&](int, const zelda3::Room& room) {
    objects_pending |= objects_enabled && (room.object_stream_dirty() ||
                                           room.object_stream_header_dirty());
    sprites_pending |= sprites_enabled && room.sprites_dirty();
  });
  std::vector<std::pair<uint32_t, uint32_t>> writes;
  if (!objects_pending && !sprites_pending)
    return writes;
  if (!rom_ || !rom_->is_loaded())
    return absl::FailedPreconditionError("ROM is not loaded");
  std::optional<zelda3::DungeonStreamLayout> object_layout, sprite_layout;
  const auto* project = dependencies_.project;
  const auto load_layout = [&](core::DungeonStreamType type, bool pending,
                               auto* result) -> absl::Status {
    if (!pending || !project || !project->hack_manifest.loaded())
      return absl::OkStatus();
    const auto* layout = project->hack_manifest.GetDungeonStreamLayout(type);
    if (layout &&
        layout->strategy == core::DungeonWriteStrategy::kCopyOnWrite) {
      ASSIGN_OR_RETURN(*result,
                       core::ToDungeonStreamAllocatorLayout(type, *layout));
    }
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(load_layout(core::DungeonStreamType::kObjects,
                              objects_pending, &object_layout));
  RETURN_IF_ERROR(load_layout(core::DungeonStreamType::kSprites,
                              sprites_pending, &sprite_layout));
  const auto append = [&](const auto& ranges) {
    writes.insert(writes.end(), ranges.begin(), ranges.end());
  };
  // Save visits loaded rooms in ascending order. Simulate every pending stream
  // in that same order so object/header and sprite relocations share the actual
  // remaining space. Detached authored state never loads graphics or touches Undo.
  Rom scratch(*rom_);
  absl::Status status;
  rooms_.ForEachLoaded([&](int id, const zelda3::Room& source) {
    if (!status.ok())
      return;
    const auto check_room = [&]() -> absl::Status {
      auto dirty = source.CaptureSaveDirtySnapshot();
      if (candidate && id == room_id) {
        dirty.object_stream |=
            (domains & (kSelectionObjects | kSelectionDoors)) != 0;
        dirty.sprites |= (domains & kSelectionSprites) != 0;
      }
      if (!(objects_enabled &&
            (dirty.object_stream || dirty.object_stream_header)) &&
          !(sprites_enabled && dirty.sprites))
        return absl::OkStatus();
      zelda3::Room staged(id, &scratch);
      staged.RestoreMetadataSnapshot(source.CaptureMetadataSnapshot());
      ApplyDungeonSelectionEditState(
          staged, CaptureDungeonSelectionEditState(source), streams);
      if (candidate && id == room_id)
        ApplyDungeonSelectionEditState(staged, *candidate, domains);
      staged.RestoreSaveDirtySnapshot(dirty);
      const auto* objects = object_layout ? &*object_layout : nullptr;
      const auto* sprites = sprite_layout ? &*sprite_layout : nullptr;
      if (objects_enabled && dirty.object_stream) {
        ASSIGN_OR_RETURN(auto ranges, staged.GetObjectSaveWriteRanges(objects));
        append(ranges);
        RETURN_IF_ERROR(staged.SaveObjects(objects));
      }
      if (objects_enabled && dirty.object_stream_header) {
        ASSIGN_OR_RETURN(auto ranges,
                         staged.GetObjectStreamHeaderSaveWriteRanges(objects));
        append(ranges);
        RETURN_IF_ERROR(staged.SaveObjectStreamHeader(objects));
      }
      if (sprites_enabled && dirty.sprites) {
        ASSIGN_OR_RETURN(auto ranges, staged.GetSpriteSaveWriteRanges(sprites));
        append(ranges);
        RETURN_IF_ERROR(staged.SaveSprites(sprites));
      }
      return absl::OkStatus();
    };
    status = check_room();
  });
  RETURN_IF_ERROR(status);
  return writes;
}

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
                                            plan.after.chests, false));
  }
  RETURN_IF_ERROR(
      PreflightStreamMutation(plan.room_id, plan.after, plan.domains));
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
