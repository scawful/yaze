#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <algorithm>
#include <optional>

#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "util/macro.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/dungeon_limits.h"
#include "zelda3/dungeon/object_dimensions.h"

namespace yaze::editor {
namespace {
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

std::optional<size_t> ObjectForChest(const zelda3::Room& room, size_t chest) {
  for (size_t i = 0; i < room.GetTileObjects().size(); ++i) {
    if (zelda3::ChestIndexForObject(room.GetTileObjects(), i) == chest) {
      return i;
    }
  }
  return std::nullopt;
}

std::vector<std::optional<size_t>> ObjectOrigins(size_t count) {
  std::vector<std::optional<size_t>> origins(count);
  for (size_t i = 0; i < count; ++i)
    origins[i] = i;
  return origins;
}
}  // namespace

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
  if (before.size != big_chest) {
    const auto object_index = ObjectForChest(*room, index);
    if (!object_index) {
      return absl::FailedPreconditionError(
          "Chest object and contents do not match; review the room before "
          "changing chest type");
    }
    auto objects = room->GetTileObjects();
    auto& object = objects[*object_index];
    object.set_id(big_chest ? 0xFB1 : 0xF99);
    object.size_ = zelda3::CanonicalRoomObjectSize(object.id_, 0);
    object.set_options(zelda3::ObjectOption::Chest);
    std::vector<std::optional<chest_data>> overrides(objects.size());
    overrides[*object_index] = after;
    ASSIGN_OR_RETURN(auto chests,
                     zelda3::PlanChestObjectEdit(
                         room->GetTileObjects(), room->GetChests(), objects,
                         ObjectOrigins(objects.size()), overrides));
    return ApplyChestObjectEdit(room_id, objects, chests, {*object_index});
  }
  auto chests = room->GetChests();
  chests[index] = after;
  RETURN_IF_ERROR(
      PreflightObjectMutation(room_id, room->GetTileObjects(), chests));
  FinalizePendingUndoActions();
  RETURN_IF_ERROR(RestoreChest(room_id, index, after));
  undo_manager_.Push(std::make_unique<ChestContentsAction>(
      room_id, index, before, after,
      [this](int id, size_t restored_index, chest_data chest) {
        return RestoreChest(id, restored_index, chest);
      }));
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::DeleteChest(int room_id, size_t index) {
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_ || !room->AreChestsLoaded()) {
    return absl::FailedPreconditionError("Chest contents are not loaded");
  }
  if (index >= room->GetChests().size()) {
    return absl::InvalidArgumentError("Select an existing chest");
  }
  const auto object_index = ObjectForChest(*room, index);
  if (!object_index) {
    return absl::FailedPreconditionError(
        "Chest object and contents do not match; review the room before "
        "deleting a chest");
  }
  auto objects = room->GetTileObjects();
  auto origins = ObjectOrigins(objects.size());
  objects.erase(objects.begin() + *object_index);
  origins.erase(origins.begin() + *object_index);
  ASSIGN_OR_RETURN(auto chests, zelda3::PlanChestObjectEdit(
                                    room->GetTileObjects(), room->GetChests(),
                                    objects, origins));
  return ApplyChestObjectEdit(room_id, objects, chests, {});
}

absl::Status DungeonEditorV2::PreflightObjectMutation(
    int room_id, const std::vector<zelda3::RoomObject>& objects,
    const std::vector<chest_data>& chests) {
  const auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_) {
    return absl::FailedPreconditionError("Room objects are not loaded");
  }
  if (objects.size() > zelda3::kMaxTileObjects &&
      objects.size() > room->GetTileObjects().size()) {
    return absl::ResourceExhaustedError("Room object limit reached");
  }
  const auto& original = room->GetChests();
  if (original.size() == chests.size() &&
      std::equal(
          original.begin(), original.end(), chests.begin(),
          [](auto a, auto b) { return a.id == b.id && a.size == b.size; })) {
    return absl::OkStatus();
  }
  if (!room->AreChestsLoaded() || !rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("Chest contents are not loaded");
  }
  // The shared save planner accounts for physical records in unopened rooms
  // and every other room's pending edits, without writing any ROM bytes.
  zelda3::Room staged(room_id, rom_);
  staged.GetChests() = chests;
  staged.MarkChestsDirty();
  ASSIGN_OR_RETURN(auto plan, zelda3::BuildChestSavePlan(
                                  rom_, static_cast<int>(rooms_.size()),
                                  [&](int id) -> const zelda3::Room* {
                                    return id == room_id
                                               ? &staged
                                               : rooms_.GetIfMaterialized(id);
                                  }));
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        dependencies_.project->hack_manifest,
        dependencies_.project->rom_metadata.write_policy, plan.write_ranges(),
        "chest authoring", "DungeonEditorV2", nullptr));
  }
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::ApplyChestObjectEdit(
    int room_id, const std::vector<zelda3::RoomObject>& objects,
    const std::vector<chest_data>& chests,
    const std::vector<size_t>& selection) {
  RETURN_IF_ERROR(PreflightObjectMutation(room_id, objects, chests));
  FinalizePendingUndoActions();
  BeginUndoSnapshot(room_id);
  RETURN_IF_ERROR(RestoreRoomObjects(room_id, objects, selection, chests));
  FinalizeUndoAction(room_id);
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

}  // namespace yaze::editor
