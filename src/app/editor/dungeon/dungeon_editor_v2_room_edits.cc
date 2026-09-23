#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "util/macro.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

class RoomMetadataAction final : public UndoAction {
 public:
  using Snapshot = zelda3::Room::MetadataSnapshot;
  using States = std::vector<std::pair<int, Snapshot>>;
  using Restore = std::function<absl::Status(const States&)>;
  RoomMetadataAction(States before, States after, Restore restore)
      : before_(std::move(before)),
        after_(std::move(after)),
        restore_(std::move(restore)) {}
  absl::Status Undo() override { return restore_(before_); }
  absl::Status Redo() override { return restore_(after_); }
  std::string Description() const override {
    if (before_.size() == 1) {
      return absl::StrFormat("Edit room %03X properties",
                             before_.front().first);
    }
    return absl::StrFormat("Edit properties in %zu rooms", before_.size());
  }
  size_t MemoryUsage() const override {
    size_t bytes =
        (before_.size() + after_.size()) * sizeof(States::value_type);
    for (const auto* states : {&before_, &after_}) {
      for (const auto& [id, snapshot] : *states) {
        bytes += snapshot.layer_merging.Name.capacity();
      }
    }
    return bytes;
  }

 private:
  States before_, after_;
  Restore restore_;
};

// Existing-record edits never reorder the chest table or change its capacity.
// Keep only the affected record so undo cannot rewrite another chest's contents.
class ChestContentsAction final : public UndoAction {
 public:
  using Restore = std::function<absl::Status(int, size_t, chest_data)>;
  ChestContentsAction(int room_id, size_t index, chest_data before,
                      chest_data after, Restore restore)
      : room_id_(room_id),
        index_(index),
        before_(before),
        after_(after),
        restore_(std::move(restore)) {}
  absl::Status Undo() override { return restore_(room_id_, index_, before_); }
  absl::Status Redo() override { return restore_(room_id_, index_, after_); }
  std::string Description() const override {
    return absl::StrFormat("Edit room %03X chest %d contents", room_id_,
                           index_ + 1);
  }
  size_t MemoryUsage() const override { return 2 * sizeof(chest_data); }

 private:
  int room_id_;
  size_t index_;
  chest_data before_, after_;
  Restore restore_;
};

}  // namespace

absl::Status DungeonEditorV2::EditRoomMetadata(int room_id,
                                               const RoomMetadataEdit& edit) {
  return EditRoomMetadataBatch({{room_id, edit}});
}

absl::Status DungeonEditorV2::EditRoomMetadataBatch(
    const std::vector<RoomMetadataRequest>& requests) {
  std::map<int, std::vector<RoomMetadataEdit>> room_edits;
  for (const auto& request : requests) {
    RETURN_IF_ERROR(ValidateRoomMetadataEdit(request.edit));
    const auto* room = rooms_.GetIfLoaded(request.room_id);
    if (!room || room->rom() != rom_) {
      return absl::FailedPreconditionError("Room properties are not loaded");
    }
    room_edits[request.room_id].push_back(request.edit);
  }

  RoomMetadataAction::States before;
  RoomMetadataAction::States after;
  // Reuse the same model setters for planning, with no ROM or render resources.
  // In particular, a batch may change a field and then restore its original
  // value. Plan the final state before dirtying any real room or its history.
  zelda3::Room staged;
  for (const auto& [room_id, edits] : room_edits) {
    const auto original =
        rooms_.GetIfLoaded(room_id)->CaptureMetadataSnapshot();
    staged.RestoreMetadataSnapshot(original);
    for (const auto& edit : edits) {
      RETURN_IF_ERROR(ApplyRoomMetadataEdit(staged, edit));
    }
    auto planned = staged.CaptureMetadataSnapshot();
    if (original != planned) {
      before.emplace_back(room_id, original);
      after.emplace_back(room_id, std::move(planned));
    }
  }
  if (before.empty()) {
    return absl::OkStatus();
  }
  FinalizePendingUndoActions();
  RETURN_IF_ERROR(RestoreRoomMetadataBatch(after));
  undo_manager_.Push(std::make_unique<RoomMetadataAction>(
      std::move(before), std::move(after),
      [this](const RoomMetadataAction::States& states) {
        return RestoreRoomMetadataBatch(states);
      }));
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::RestoreRoomMetadataBatch(
    const std::vector<std::pair<int, zelda3::Room::MetadataSnapshot>>& states) {
  for (const auto& [room_id, snapshot] : states) {
    const auto* room = rooms_.GetIfLoaded(room_id);
    if (!room || room->rom() != rom_) {
      return absl::FailedPreconditionError("Room properties are not loaded");
    }
  }
  for (const auto& [room_id, snapshot] : states) {
    rooms_.GetIfLoaded(room_id)->RestoreMetadataSnapshot(snapshot);
  }
  for (const auto& [room_id, snapshot] : states) {
    RefreshRoomMetadataViews(room_id);
  }
  // Refresh only the edited room, even when undo targets an offscreen room.
  undo_restore_triggered_ping_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::EditChest(int room_id, size_t index,
                                        uint8_t item_id, bool big_chest) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_ || !room->AreChestsLoaded()) {
    return absl::FailedPreconditionError("Chest contents are not loaded");
  }
  if (index >= room->GetChests().size()) {
    return absl::InvalidArgumentError(
        "Select an existing chest contents record");
  }
  const auto before = room->GetChests()[index];
  const chest_data after{item_id, big_chest};
  if (before.id == after.id && before.size == after.size) {
    return absl::OkStatus();
  }
  FinalizePendingUndoActions();
  RETURN_IF_ERROR(RestoreChest(room_id, index, after));
  undo_manager_.Push(std::make_unique<ChestContentsAction>(
      room_id, index, before, after,
      [this](int id, size_t restored_index, chest_data chest) {
        return RestoreChest(id, restored_index, chest);
      }));
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::RestoreChest(int room_id, size_t index,
                                           chest_data chest) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_ || !room->AreChestsLoaded() ||
      index >= room->GetChests().size()) {
    return absl::FailedPreconditionError(
        "Chest contents record is unavailable");
  }
  room->GetChests()[index] = chest;
  room->MarkChestsDirty();
  // Contents do not alter the visual chest object. Do not flash a different room
  // or dirty its tile-object stream when this record is restored offscreen.
  undo_restore_triggered_ping_ = true;
  return absl::OkStatus();
}

void DungeonEditorV2::RefreshRoomMetadataViews(int room_id) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room) {
    return;
  }
  if (room_tag_editor_panel_) {
    room_tag_editor_panel_->InvalidateUsageCache();
  }

  // A destination edit can change a graph centered on any neighboring room.
  // Update existing viewers only: undo must not create a viewer or navigate.
  auto refresh_viewer = [&](DungeonCanvasViewer* viewer, bool is_target) {
    if (!viewer) {
      return;
    }
    viewer->InvalidateConnectedRoomGraph();
    if (!is_target) {
      return;
    }
    const int palette_id = room->ResolveDungeonPaletteId();
    viewer->SetCurrentPaletteId(palette_id);
    if (game_data_ && palette_id >= 0 &&
        palette_id <
            static_cast<int>(game_data_->palette_groups.dungeon_main.size())) {
      viewer->SetCurrentPaletteGroup(
          zelda3::BuildDungeonRenderPaletteGroupFromGameData(
              game_data_->palette_groups.dungeon_main[palette_id], game_data_),
          /*force_refresh=*/true);
    }
    viewer->TriggerChangePing();
  };
  room_viewers_.ForEach(
      [&](int id, std::unique_ptr<DungeonCanvasViewer>& viewer) {
        refresh_viewer(viewer.get(), id == room_id);
      });
  for (auto* viewer :
       {workbench_viewer_.get(), workbench_compare_viewer_.get()}) {
    const auto* ctx = viewer ? viewer->object_interaction()
                                   .entity_coordinator()
                                   .tile_handler()
                                   .context()
                             : nullptr;
    refresh_viewer(viewer, ctx && ctx->current_room_id == room_id);
  }

  if (current_room_id_ != room_id) {
    return;
  }
  current_palette_id_ = room->ResolveDungeonPaletteId();
  current_palette_group_id_ = current_palette_id_;
  palette_editor_.SetCurrentPaletteId(current_palette_id_);
  if (!game_data_ ||
      current_palette_id_ >= game_data_->palette_groups.dungeon_main.size()) {
    return;
  }
  current_palette_ =
      game_data_->palette_groups.dungeon_main[current_palette_id_];
  current_palette_group_ = zelda3::BuildDungeonRenderPaletteGroupFromGameData(
      current_palette_, game_data_);
  if (object_selector_panel_) {
    object_selector_panel_->SetCurrentPaletteGroup(current_palette_group_);
  }
  if (room_graphics_panel_) {
    room_graphics_panel_->SetCurrentPaletteGroup(current_palette_group_);
  }
  if (object_tile_editor_panel_) {
    object_tile_editor_panel_->SetCurrentPaletteGroupForRoom(
        room_id, current_palette_group_);
  }
}

}  // namespace yaze::editor
