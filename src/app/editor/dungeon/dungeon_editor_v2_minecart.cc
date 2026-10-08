#include "app/editor/dungeon/dungeon_editor_v2.h"

#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "util/macro.h"

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
}  // namespace

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
