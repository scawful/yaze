#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <set>
#include "app/editor/dungeon/ui/minecart/minecart_canvas_tool.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "core/features.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::editor {
namespace {
absl::Status UnusedAssemblyIds(const DungeonRoomStore& rooms, const Rom& rom,
                               const std::set<int>& carts,
                               const std::set<int>& channels,
                               int skip_room = -1) {
  if (carts.empty() && channels.empty())
    return absl::OkStatus();
  auto check = [&](int id, int subtype) -> absl::Status {
    if ((id == 0xA3 && carts.contains(subtype)) ||
        ((id == 0xAF || id == 0xB0) && channels.contains(subtype)))
      return absl::FailedPreconditionError(
          "Cart ID or switch channel is already used in the loaded/project "
          "scope");
    return absl::OkStatus();
  };
  for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
    if (room == skip_room)
      continue;
    const auto* authored = rooms.GetIfMaterialized(room);
    if (authored &&
        (authored->AreSpritesLoaded() || authored->sprites_dirty())) {
      for (const auto& sprite : authored->GetSprites())
        if (!sprite.deleted())
          RETURN_IF_ERROR(check(sprite.id(), sprite.subtype()));
    } else {
      ASSIGN_OR_RETURN(auto stream, zelda3::ReadDungeonSpriteStream(rom, room));
      const auto& bytes = stream.encoded_stream;
      for (size_t cursor = 1;
           cursor + 2 < bytes.size() && bytes[cursor] != 0xFF; cursor += 3)
        RETURN_IF_ERROR(check(
            bytes[cursor + 2],
            ((bytes[cursor + 1] & 0xE0) >> 5) | ((bytes[cursor] & 0x60) >> 2)));
    }
  }
  return absl::OkStatus();
}

class RailAssemblyAction final : public UndoAction {
 public:
  RailAssemblyAction(std::function<absl::Status(bool)> restore,
                     std::string description)
      : restore_(std::move(restore)), description_(std::move(description)) {}
  absl::Status Undo() override { return restore_(false); }
  absl::Status Redo() override { return restore_(true); }
  std::string Description() const override { return description_; }

 private:
  std::function<absl::Status(bool)> restore_;
  std::string description_;
};
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

absl::Status DungeonEditorV2::ValidateMinecartAssemblyIdentities(
    const MinecartAssemblyPlan& plan) const {
  std::set<int> carts, channels;
  for (size_t i = plan.selection.before.sprites.size();
       i < plan.selection.after.sprites.size(); ++i) {
    const auto& sprite = plan.selection.after.sprites[i];
    (sprite.id == 0xA3 ? carts : channels).insert(sprite.subtype);
  }
  return UnusedAssemblyIds(rooms_, *rom_, carts, channels);
}

absl::Status DungeonEditorV2::PreflightMinecartAssembly(
    const MinecartAssemblyPlan& plan, bool forward) {
  RETURN_IF_ERROR(PreflightStreamMutation(
      plan.selection.room_id,
      forward ? plan.selection.after : plan.selection.before,
      plan.selection.domains));
  Rom scratch;
  RETURN_IF_ERROR(scratch.LoadFromData(rom_->vector()));
  RETURN_IF_ERROR(zelda3::WriteTrackCollision(
      &scratch, plan.selection.room_id,
      forward ? plan.collision_after : plan.collision_before));
  if (dependencies_.project && dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> writes;
    for (size_t i = 0; i < rom_->size();) {
      if (rom_->vector()[i] == scratch.vector()[i]) {
        ++i;
        continue;
      }
      const auto begin = i++;
      while (i < rom_->size() && rom_->vector()[i] != scratch.vector()[i])
        ++i;
      writes.emplace_back(begin, i);
    }
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        dependencies_.project->hack_manifest,
        dependencies_.project->rom_metadata.write_policy, writes,
        "minecart assembly collision", "DungeonEditorV2", nullptr));
  }
  return absl::OkStatus();
}

absl::StatusOr<DungeonSelectionClipboard>
DungeonEditorV2::CaptureSelectedMinecartAssembly(int room_id) {
  auto* room = rooms_.GetIfLoaded(room_id);
  auto* viewer = GetViewerForRoom(room_id);
  if (!room || !viewer || !rom_ || room->rom() != rom_ ||
      !minecart_track_editor_panel_ || viewer->current_room_id() != room_id ||
      minecart_track_editor_panel_->active_room() != room_id)
    return absl::FailedPreconditionError(
        "Select the rail assembly in a loaded Room view first");
  auto& interaction = viewer->object_interaction();
  const auto profile =
      zelda3::CustomObjectManager::Get().GetMinecartRailProfile();
  ASSIGN_OR_RETURN(
      auto clipboard,
      CaptureMinecartAssembly(
          *room, interaction.GetSelectedObjectIndices(),
          interaction.entity_coordinator().SelectedEntitiesForEdit(),
          minecart_track_editor_panel_->GetTracks(), profile));
  std::set<int> carts, channels;
  for (const auto& sprite : clipboard.sprites)
    (sprite.id == 0xA3 ? carts : channels).insert(sprite.subtype);
  RETURN_IF_ERROR(UnusedAssemblyIds(rooms_, *rom_, carts, channels, room_id));
  clipboard.minecart->source_rom = rom_;
  clipboard.minecart->session_generation = minecart_collision_generation_;
  selection_clipboard_ = clipboard;
  return clipboard;
}

absl::StatusOr<MinecartAssemblyPlan> DungeonEditorV2::PreviewMinecartAssembly(
    int room_id, const DungeonSelectionClipboard& clipboard, int x, int y,
    const MinecartAssemblyMapping& mapping) {
  auto* room = rooms_.GetIfLoaded(room_id);
  auto* panel = minecart_track_editor_panel_;
  if (!room || !rom_ || room->rom() != rom_ || !panel ||
      !core::FeatureFlags::get().kEnableCustomObjects ||
      (GetViewerForRoom(room_id) &&
       GetViewerForRoom(room_id)->header_read_only()))
    return absl::FailedPreconditionError(
        "Load an editable custom-object room first");
  if (clipboard.minecart && clipboard.minecart->source_room >= 0 &&
      (clipboard.minecart->source_rom != rom_ ||
       clipboard.minecart->session_generation !=
           minecart_collision_generation_))
    return absl::FailedPreconditionError(
        "Assembly clipboard belongs to an earlier project");
  auto& assets = zelda3::CustomObjectManager::Get();
  for (const auto& object : clipboard.objects) {
    ASSIGN_OR_RETURN(auto asset,
                     assets.GetObjectInternal(object.id_, object.size_));
    const auto bounds = asset->GetBoundingBox();
    if (asset->IsEmpty() || bounds.min_x < 0 || bounds.min_y < 0 ||
        bounds.max_x > 1 || bounds.max_y > 1)
      return absl::FailedPreconditionError(
          "Path tools require admitted rail art within its 2x2 footprint");
  }
  if (panel->GetTracks().size() != kMinecartTrackSlotCount)
    return absl::FailedPreconditionError(
        "Load the complete minecart source before rail planning");
  const auto found = minecart_collision_ownership_.find(room_id);
  const auto ownership = found == minecart_collision_ownership_.end()
                             ? zelda3::MinecartTrackCollisionOwnership{}
                             : found->second;
  ASSIGN_OR_RETURN(
      auto plan,
      PlanMinecartAssembly(*room, clipboard, x, y, mapping, panel->GetTracks(),
                           ownership, assets.GetMinecartRailProfile()));
  RETURN_IF_ERROR(ValidateMinecartAssemblyIdentities(plan));
  RETURN_IF_ERROR(PreflightMinecartAssembly(plan, true));
  plan.source_rom = rom_;
  plan.room_metadata = room->CaptureMetadataSnapshot();
  plan.room_generation = minecart_collision_generation_;
  plan.source_generation = panel->track_generation();
  plan.asset_generation = assets.asset_generation();
  return plan;
}

absl::Status DungeonEditorV2::ApplyMinecartAssembly(
    const MinecartAssemblyPlan& input) {
  const auto plan = input;
  if (!plan.selection.changed() &&
      plan.collision_before == plan.collision_after &&
      plan.tracks_before == plan.tracks_after)
    return absl::OkStatus();
  if (HasPendingUndoSnapshot())
    return absl::FailedPreconditionError(
        "Finish the previous gesture before applying the rail assembly");
  auto* panel = minecart_track_editor_panel_;
  if (!panel)
    return absl::FailedPreconditionError("Minecart source closed");
  auto restore = [this, panel, plan](bool forward) -> absl::Status {
    const int room_id = plan.selection.room_id;
    auto* room = rooms_.GetIfLoaded(room_id);
    auto& assets = zelda3::CustomObjectManager::Get();
    if (!room || !rom_ || rom_ != plan.source_rom || room->rom() != rom_ ||
        minecart_collision_generation_ != plan.room_generation ||
        minecart_track_editor_panel_ != panel ||
        panel->track_generation() != plan.source_generation ||
        assets.asset_generation() != plan.asset_generation ||
        assets.GetMinecartRailProfile() != plan.profile ||
        (GetViewerForRoom(room_id) &&
         GetViewerForRoom(room_id)->header_read_only()))
      return absl::FailedPreconditionError(
          "Rail preview belongs to an earlier room, asset or project "
          "generation");
    const auto& expected =
        forward ? plan.selection.before : plan.selection.after;
    const auto& replacement =
        forward ? plan.selection.after : plan.selection.before;
    const auto found = minecart_collision_ownership_.find(room_id);
    const auto ownership = found == minecart_collision_ownership_.end()
                               ? zelda3::MinecartTrackCollisionOwnership{}
                               : found->second;
    if (room->CaptureMetadataSnapshot() != plan.room_metadata ||
        ChangedDungeonSelectionDomains(CaptureDungeonSelectionEditState(*room),
                                       expected) ||
        !(room->custom_collision() ==
          (forward ? plan.collision_before : plan.collision_after)) ||
        ownership != (forward ? plan.ownership_before : plan.ownership_after))
      return absl::FailedPreconditionError(
          "Room or collision ownership changed; preview the assembly again");
    if (forward)
      RETURN_IF_ERROR(ValidateMinecartAssemblyIdentities(plan));
    RETURN_IF_ERROR(PreflightMinecartAssembly(plan, forward));
    // Batch validates every source slot and renders the complete existing source
    // document before publishing any draft. Nothing fallible follows it.
    RETURN_IF_ERROR(panel->RestoreTrackDraftBatch(
        plan.source_generation,
        forward ? plan.tracks_before : plan.tracks_after,
        forward ? plan.tracks_after : plan.tracks_before));
    ApplyDungeonSelectionEditState(*room, replacement, plan.selection.domains);
    RestoreRoomCustomCollision(
        room_id, forward ? plan.collision_after : plan.collision_before);
    minecart_collision_ownership_[room_id] =
        forward ? plan.ownership_after : plan.ownership_before;
    panel->MarkPairedTrackDraft();
    RefreshSelectionEditViews(room_id, &replacement);
    rom_->set_dirty(true);
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(restore(true));
  undo_manager_.Push(
      std::make_unique<RailAssemblyAction>(restore, plan.description));
  return absl::OkStatus();
}

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
