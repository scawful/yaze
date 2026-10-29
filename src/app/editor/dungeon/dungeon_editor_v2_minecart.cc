#include "app/editor/dungeon/dungeon_editor_v2.h"

#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "core/features.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"

namespace yaze::editor {
namespace {
class CartStartAction final : public UndoAction {
 public:
  explicit CartStartAction(std::function<absl::Status(bool)> restore)
      : restore_(std::move(restore)) {}
  absl::Status Undo() override { return restore_(false); }
  absl::Status Redo() override { return restore_(true); }
  std::string Description() const override { return "Edit cart start"; }

 private:
  std::function<absl::Status(bool)> restore_;
};

class RailObjectsAction final : public UndoAction {
 public:
  using Restore = std::function<absl::Status(bool, UndoAction&)>;
  RailObjectsAction(std::unique_ptr<UndoAction> objects, Restore restore,
                    size_t collision_bytes)
      : objects_(std::move(objects)),
        restore_(std::move(restore)),
        collision_bytes_(collision_bytes) {}
  absl::Status Undo() override { return restore_(false, *objects_); }
  absl::Status Redo() override { return restore_(true, *objects_); }
  std::string Description() const override { return objects_->Description(); }
  size_t MemoryUsage() const override {
    return objects_->MemoryUsage() + collision_bytes_;
  }

 private:
  std::unique_ptr<UndoAction> objects_;
  Restore restore_;
  size_t collision_bytes_;
};
}  // namespace

absl::Status DungeonEditorV2::PreflightMinecartObjectCollision(
    int room_id, const std::vector<zelda3::RoomObject>& objects) const {
  if (!minecart_track_editor_panel_ ||
      !minecart_track_editor_panel_->automatic_rail_collision() ||
      !core::FeatureFlags::get().kEnableCustomObjects) {
    return absl::OkStatus();
  }
  const auto* room = rooms_.GetIfLoaded(room_id);
  if (!room) {
    return absl::FailedPreconditionError("Track room is no longer loaded");
  }
  const auto* before = &room->GetTileObjects();
  if (pending_selection_undo_.plan &&
      pending_selection_undo_.plan->room_id == room_id) {
    before = &pending_selection_undo_.plan->before.objects;
  } else if (pending_undo_.room_id == room_id) {
    before = &pending_undo_.before_objects;
  }
  const auto found = minecart_collision_ownership_.find(room_id);
  const auto ownership = found == minecart_collision_ownership_.end()
                             ? zelda3::MinecartTrackCollisionOwnership{}
                             : found->second;
  return zelda3::SynchronizeMinecartTrackCollision(
             *before, objects, room->custom_collision(), ownership,
             zelda3::CustomObjectManager::Get().GetMinecartRailProfile())
      .status();
}

absl::StatusOr<std::unique_ptr<UndoAction>>
DungeonEditorV2::PairMinecartObjectCollision(
    int room_id, const std::vector<zelda3::RoomObject>& before,
    const std::vector<zelda3::RoomObject>& after,
    std::unique_ptr<UndoAction> object_action) {
  if (!minecart_track_editor_panel_ ||
      !minecart_track_editor_panel_->automatic_rail_collision() ||
      !core::FeatureFlags::get().kEnableCustomObjects) {
    return object_action;
  }
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || room->rom() != rom_) {
    return absl::FailedPreconditionError("Track room is no longer loaded");
  }
  const auto collision_before = room->custom_collision();
  const auto found = minecart_collision_ownership_.find(room_id);
  const auto ownership_before = found == minecart_collision_ownership_.end()
                                    ? zelda3::MinecartTrackCollisionOwnership{}
                                    : found->second;
  ASSIGN_OR_RETURN(
      auto edit,
      zelda3::SynchronizeMinecartTrackCollision(
          before, after, collision_before, ownership_before,
          zelda3::CustomObjectManager::Get().GetMinecartRailProfile()));
  if (edit.collision == collision_before &&
      edit.ownership == ownership_before) {
    return object_action;
  }
  const uint64_t generation = minecart_collision_generation_;
  auto* source_rom = rom_;
  const size_t bytes =
      sizeof(collision_before) + sizeof(edit.collision) +
      (ownership_before.size() + edit.ownership.size()) *
          sizeof(zelda3::MinecartTrackCollisionOwnership::value_type);
  auto restore = [this, room_id, generation, source_rom, collision_before,
                  collision_after = edit.collision, ownership_before,
                  ownership_after = edit.ownership](
                     bool forward, UndoAction& objects) -> absl::Status {
    auto* current = rooms_.GetIfLoaded(room_id);
    if (!current || current->rom() != source_rom || rom_ != source_rom ||
        minecart_collision_generation_ != generation) {
      return absl::FailedPreconditionError(
          "Rail edit belongs to a different project generation");
    }
    const auto& expected = forward ? collision_before : collision_after;
    if (!(current->custom_collision() == expected)) {
      return absl::FailedPreconditionError(
          "Rail collision changed outside Undo; repeat the edit");
    }
    RETURN_IF_ERROR(forward ? objects.Redo() : objects.Undo());
    RestoreRoomCustomCollision(room_id,
                               forward ? collision_after : collision_before);
    minecart_collision_ownership_[room_id] =
        forward ? ownership_after : ownership_before;
    return absl::OkStatus();
  };
  // The ordinary object mutation has already completed. Collision joins it
  // only after the complete generated candidate has passed validation.
  RestoreRoomCustomCollision(room_id, edit.collision);
  minecart_collision_ownership_[room_id] = std::move(edit.ownership);
  return std::make_unique<RailObjectsAction>(std::move(object_action),
                                             std::move(restore), bytes);
}

absl::Status DungeonEditorV2::ApplyMinecartTrackEdit(
    const MinecartTrack& before, const MinecartTrack& after, bool move_cart) {
  auto* panel = minecart_track_editor_panel_;
  if (!panel)
    return absl::FailedPreconditionError("Track document is unavailable");
  if (before == after)
    return absl::OkStatus();
  FinalizePendingUndoActions();
  const uint64_t generation = panel->track_generation();
  std::optional<size_t> sprite_index;
  std::optional<zelda3::Sprite> old_sprite, new_sprite;
  if (move_cart) {
    if (before.room_id != after.room_id || before.id != after.id)
      return absl::InvalidArgumentError(
          "Move a start within its existing room");
    auto* room = rooms_.GetIfLoaded(before.room_id);
    if (!room)
      return absl::FailedPreconditionError("Load the start room first");
    ASSIGN_OR_RETURN(const size_t index, FindCartStartSprite(*room, before));
    RETURN_IF_ERROR(ValidateCartStartDestination(*room, after));
    sprite_index = index;
    old_sprite = room->GetSprites()[index];
    const auto local = *MinecartLocalPosition(after);
    for (size_t other = 0; other < room->GetSprites().size(); ++other) {
      const auto& sprite = room->GetSprites()[other];
      if (other != index && !sprite.deleted() && sprite.id() == 0xA3 &&
          sprite.layer() == 0 && sprite.x() * 16 == local.first &&
          sprite.y() * 16 == local.second)
        return absl::FailedPreconditionError("Another cart occupies this stop");
    }
    new_sprite.emplace(old_sprite->id(), local.first / 16, local.second / 16,
                       old_sprite->subtype(), old_sprite->layer());
    new_sprite->set_key_drop(old_sprite->key_drop());
    new_sprite->set_deleted(old_sprite->deleted());
    auto candidates = room->GetSprites();
    candidates[index] = *new_sprite;
    RETURN_IF_ERROR(PreflightSpriteMutation(before.room_id, candidates));
  }
  auto restore = [this, generation, before, after, sprite_index, old_sprite,
                  new_sprite](bool forward) -> absl::Status {
    auto* current_panel = minecart_track_editor_panel_;
    if (!current_panel)
      return absl::FailedPreconditionError("Track document closed");
    const auto& expected = forward ? before : after;
    const auto& replacement = forward ? after : before;
    zelda3::Room* room = nullptr;
    if (sprite_index) {
      room = rooms_.GetIfLoaded(before.room_id);
      if (!room || *sprite_index >= room->GetSprites().size())
        return absl::FailedPreconditionError(
            "The paired cart is no longer loaded");
      const auto& current = room->GetSprites()[*sprite_index];
      const auto& wanted = forward ? *old_sprite : *new_sprite;
      if (current.id() != wanted.id() || current.x() != wanted.x() ||
          current.y() != wanted.y() || current.subtype() != wanted.subtype() ||
          current.layer() != wanted.layer() ||
          current.deleted() != wanted.deleted() ||
          current.key_drop() != wanted.key_drop())
        return absl::FailedPreconditionError(
            "The paired cart changed; repeat the edit");
    }
    RETURN_IF_ERROR(
        current_panel->RestoreTrackDraft(generation, expected, replacement));
    if (room) {
      room->GetSprites()[*sprite_index] = forward ? *new_sprite : *old_sprite;
      room->MarkSpritesDirty();
      current_panel->MarkPairedTrackDraft();
      if (rom_)
        rom_->set_dirty(true);
    }
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(restore(true));
  undo_manager_.Push(std::make_unique<CartStartAction>(std::move(restore)));
  return absl::OkStatus();
}
}  // namespace yaze::editor
