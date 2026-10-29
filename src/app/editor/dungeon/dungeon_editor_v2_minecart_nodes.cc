#include "app/editor/dungeon/ui/minecart/minecart_node_tool.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/dungeon_selection_edit.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::editor {
namespace {

bool IsRail(uint8_t value) {
  return value >= 0xB0 && value <= 0xB6;
}

absl::Status ValidateUnusedCartIdentity(const DungeonRoomStore& rooms,
                                        const Rom& rom, int target_room,
                                        int identity) {
  for (int id = 0; id < zelda3::kNumberOfRooms; ++id) {
    if (id == target_room)
      continue;
    if (const auto* authored = rooms.GetIfMaterialized(id);
        authored &&
        (authored->AreSpritesLoaded() || authored->sprites_dirty())) {
      for (const auto& sprite : authored->GetSprites()) {
        if (!sprite.deleted() && sprite.id() == 0xA3 &&
            sprite.subtype() == identity)
          return absl::FailedPreconditionError(
              "This cart identity is already used in another room");
      }
      continue;
    }
    // Strict read-only parsing rejects malformed unopened streams rather than
    // treating an unreadable room as evidence that an identity is unused.
    ASSIGN_OR_RETURN(auto record, zelda3::ReadDungeonSpriteStream(rom, id));
    const auto& bytes = record.encoded_stream;
    for (size_t cursor = 1; cursor + 2 < bytes.size(); cursor += 3) {
      if (bytes[cursor] == 0xFF)
        break;
      const int subtype =
          ((bytes[cursor + 1] & 0xE0) >> 5) | ((bytes[cursor] & 0x60) >> 2);
      if (bytes[cursor + 2] == 0xA3 && subtype == identity)
        return absl::FailedPreconditionError(
            "This cart identity is already used in another room");
    }
  }
  return absl::OkStatus();
}

bool SameSprites(const std::vector<zelda3::Sprite>& lhs,
                 const std::vector<zelda3::Sprite>& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    const auto& a = lhs[i];
    const auto& b = rhs[i];
    if (a.id() != b.id() || a.x() != b.x() || a.y() != b.y() ||
        a.subtype() != b.subtype() || a.layer() != b.layer() ||
        a.key_drop() != b.key_drop() || a.deleted() != b.deleted())
      return false;
  }
  return true;
}

class MinecartNodeAction final : public UndoAction {
 public:
  using Restore = std::function<absl::Status(bool)>;
  MinecartNodeAction(std::string description, Restore restore, size_t bytes)
      : description_(std::move(description)),
        restore_(std::move(restore)),
        bytes_(bytes) {}
  absl::Status Undo() override { return restore_(false); }
  absl::Status Redo() override { return restore_(true); }
  std::string Description() const override { return description_; }
  size_t MemoryUsage() const override { return bytes_; }

 private:
  std::string description_;
  Restore restore_;
  size_t bytes_;
};

}  // namespace

absl::StatusOr<MinecartNodePlacement> PreviewMinecartNodePlacement(
    const zelda3::Room& room, int tile_x, int tile_y,
    const MinecartNodeBrush& brush, std::span<const MinecartTrack> tracks,
    int layer) {
  if (layer != 0)
    return absl::FailedPreconditionError(
        "Minecart nodes require the upper layer");
  if (room.id() < 0 || room.id() >= zelda3::kNumberOfRooms || tile_x < 0 ||
      tile_y < 0 || tile_x > 62 || tile_y > 62 || tile_x % 2 || tile_y % 2)
    return absl::InvalidArgumentError(
        "Place the node inside the room on the 16 pixel sprite grid");
  if (brush.id < 0 || brush.id >= static_cast<int>(kMinecartTrackSlotCount))
    return absl::InvalidArgumentError(
        "Cart identity or switch channel is 0..31");

  uint8_t sprite_id = 0;
  switch (brush.kind) {
    case MinecartNodeKind::kCartStart:
      sprite_id = 0xA3;
      if (room.id() == 0)
        return absl::InvalidArgumentError(
            "Room zero is reserved for inactive cart starts");
      if (brush.collision < 0xB7 || brush.collision > 0xBA)
        return absl::InvalidArgumentError("Choose a cart stop B7..BA");
      break;
    case MinecartNodeKind::kSwitch:
      sprite_id = 0xAF;
      break;
    case MinecartNodeKind::kSwitchTrack:
      sprite_id = 0xB0;
      if (brush.collision < 0xD0 || brush.collision > 0xD3)
        return absl::InvalidArgumentError("Choose a switch track D0..D3");
      break;
    default:
      return absl::InvalidArgumentError("Unknown minecart node kind");
  }

  const bool moving = brush.source_x != -1 || brush.source_y != -1;
  if (brush.expected_collision &&
      !(room.custom_collision() == *brush.expected_collision))
    return absl::FailedPreconditionError(
        "Collision changed; start the node move again");
  if (moving && brush.kind == MinecartNodeKind::kCartStart)
    return absl::FailedPreconditionError(
        "Move carts with their existing Move Cart start operation");
  if (moving &&
      (brush.source_x < 0 || brush.source_y < 0 || brush.source_x > 62 ||
       brush.source_y > 62 || brush.source_x % 2 || brush.source_y % 2 ||
       !brush.expected_collision))
    return absl::InvalidArgumentError(
        "A node move needs its 16 pixel source and original collision map");

  MinecartNodePlacement candidate{room.GetSprites(), room.custom_collision(),
                                  std::nullopt, std::nullopt};
  std::optional<size_t> source_index;
  if (moving) {
    for (size_t i = 0; i < candidate.sprites.size(); ++i) {
      const auto& sprite = candidate.sprites[i];
      if (sprite.deleted() || sprite.layer() != 0 ||
          sprite.x() * 2 != brush.source_x || sprite.y() * 2 != brush.source_y)
        continue;
      if (source_index || sprite.id() != sprite_id ||
          sprite.subtype() != brush.id ||
          sprite.key_drop() != brush.source_key_drop)
        return absl::FailedPreconditionError(
            "The source node changed or is not unique; start the move again");
      source_index = i;
    }
    if (!source_index)
      return absl::FailedPreconditionError(
          "The source node is no longer present on the upper layer");
    if (brush.kind == MinecartNodeKind::kSwitchTrack) {
      const size_t source_offset = brush.source_y * 64 + brush.source_x;
      if (!candidate.collision.HasOverride(source_offset) ||
          candidate.collision.tiles[source_offset] != brush.collision)
        return absl::FailedPreconditionError(
            "The switch track no longer matches its exact D0..D3 source cell");
    }
  }
  for (size_t i = 0; i < candidate.sprites.size(); ++i) {
    const auto& sprite = candidate.sprites[i];
    if (sprite.deleted() || source_index == i)
      continue;
    if (sprite.layer() == 0 && sprite.x() * 2 == tile_x &&
        sprite.y() * 2 == tile_y)
      return absl::FailedPreconditionError(
          "A sprite already occupies this node");
    if (sprite.id() == 0xA3 && brush.kind == MinecartNodeKind::kCartStart &&
        sprite.subtype() == brush.id)
      return absl::FailedPreconditionError(
          "This cart identity is already placed; move its existing start");
    if (sprite.id() == 0xAF && brush.kind == MinecartNodeKind::kSwitch &&
        sprite.subtype() == brush.id)
      return absl::FailedPreconditionError(
          "This room already has a lever for that switch channel");
  }

  const size_t offset = tile_y * 64 + tile_x;
  if (candidate.collision.HasOverride(offset)) {
    const auto value = candidate.collision.tiles[offset];
    const bool lever = brush.kind == MinecartNodeKind::kSwitch;
    if ((lever && value != 0) ||
        (!lever && !IsRail(value) && value != brush.collision))
      return absl::FailedPreconditionError(
          "Other collision occupies this node; choose a clear anchor");
  }
  if (brush.kind == MinecartNodeKind::kCartStart) {
    if (tracks.size() != kMinecartTrackSlotCount)
      return absl::FailedPreconditionError(
          "Load the complete 32 slot minecart source first");
    const auto& before = tracks[brush.id];
    if (before.id != brush.id || before.room_id != 0)
      return absl::FailedPreconditionError(
          "Choose a reserved cart slot; move existing starts with Move Cart");
    candidate.track_before = before;
    candidate.track_after =
        MinecartTrack{brush.id, room.id(), (room.id() & 15) * 512 + tile_x * 8,
                      (room.id() >> 4) * 512 + tile_y * 8};
  }
  if (source_index) {
    auto& sprite = candidate.sprites[*source_index];
    sprite.set_x(tile_x / 2);
    sprite.set_y(tile_y / 2);
    if (brush.kind == MinecartNodeKind::kSwitchTrack) {
      ASSIGN_OR_RETURN(
          candidate.collision,
          PreviewMinecartCollisionStamp(
              candidate.collision,
              MinecartCollisionStamp{
                  tile_x, tile_y, 1, brush.collision,
                  MinecartCollisionSource{brush.source_x, brush.source_y}}));
    }
  } else {
    candidate.sprites.emplace_back(sprite_id, tile_x / 2, tile_y / 2, brush.id,
                                   0);
    if (brush.kind != MinecartNodeKind::kSwitch)
      candidate.collision.SetOverride(offset, brush.collision);
  }
  return candidate;
}

absl::StatusOr<int> DungeonEditorV2::FindAvailableMinecartNodeId(
    MinecartNodeKind kind, int room_id) const {
  const auto* room = rooms_.GetIfLoaded(room_id);
  if (!room || !rom_ || room->rom() != rom_ || !minecart_track_editor_panel_) {
    return absl::FailedPreconditionError("Load the target minecart room first");
  }
  for (int identity = 0; identity < static_cast<int>(kMinecartTrackSlotCount);
       ++identity) {
    bool used = false;
    if (kind == MinecartNodeKind::kCartStart) {
      const auto& tracks = minecart_track_editor_panel_->GetTracks();
      const auto found = std::find_if(tracks.begin(), tracks.end(),
                                      [identity](const MinecartTrack& track) {
                                        return track.id == identity;
                                      });
      if (found == tracks.end() || found->room_id != 0)
        continue;
    }
    for (const auto& sprite : room->GetSprites()) {
      if (sprite.deleted() || sprite.subtype() != identity)
        continue;
      if ((kind == MinecartNodeKind::kCartStart && sprite.id() == 0xA3) ||
          (kind != MinecartNodeKind::kCartStart &&
           (sprite.id() == 0xAF || sprite.id() == 0xB0))) {
        used = true;
        break;
      }
    }
    if (used)
      continue;
    if (kind == MinecartNodeKind::kCartStart) {
      const auto status =
          ValidateUnusedCartIdentity(rooms_, *rom_, room_id, identity);
      if (!status.ok()) {
        if (status.message() ==
            "This cart identity is already used in another room")
          continue;
        return status;
      }
    }
    return identity;
  }
  return absl::ResourceExhaustedError(
      kind == MinecartNodeKind::kCartStart
          ? "All 32 cart identities are in use"
          : "All 32 switch channels are in use");
}

absl::Status DungeonEditorV2::PlaceMinecartNode(
    int room_id, int tile_x, int tile_y, const MinecartNodeBrush& brush) {
  // Finalization can retire the canvas gesture that owns the caller's brush.
  const auto brush_snapshot = brush;
  FinalizePendingUndoActions();
  auto* panel = minecart_track_editor_panel_;
  auto* room = rooms_.GetIfLoaded(room_id);
  if (!panel || !room || !rom_ || !rom_->is_loaded() || room->rom() != rom_)
    return absl::FailedPreconditionError(
        "Load the project's minecart document and target room first");
  if (auto* viewer = GetViewerForRoom(room_id);
      viewer && viewer->header_read_only())
    return absl::FailedPreconditionError("This room is read only");
  const auto& tracks = panel->GetTracks();
  ASSIGN_OR_RETURN(auto candidate,
                   PreviewMinecartNodePlacement(*room, tile_x, tile_y,
                                                brush_snapshot, tracks));
  if (!candidate.track_before &&
      SameSprites(room->GetSprites(), candidate.sprites) &&
      room->custom_collision() == candidate.collision)
    return absl::OkStatus();
  // Coordinated node candidates use the same stream validation service while
  // generic sprite transforms remain guarded against separating B0/collision.
  const auto preflight_node_sprites =
      [this,
       room_id](const std::vector<zelda3::Sprite>& sprites) -> absl::Status {
    const auto* target = rooms_.GetIfLoaded(room_id);
    if (!target || target->rom() != rom_)
      return absl::FailedPreconditionError("Node room is no longer loaded");
    auto state = CaptureDungeonSelectionEditState(*target);
    state.sprites.clear();
    for (const auto& sprite : sprites) {
      state.sprites.push_back({sprite.id(), sprite.x(), sprite.y(),
                               sprite.subtype(), sprite.layer(),
                               sprite.key_drop(), sprite.deleted()});
    }
    return PreflightStreamMutation(room_id, state, kSelectionSprites);
  };
  RETURN_IF_ERROR(preflight_node_sprites(candidate.sprites));
  const auto sprites_before = room->GetSprites();
  const auto collision_before = room->custom_collision();
  auto* source_rom = rom_;
  const uint64_t generation = minecart_collision_generation_;
  const uint64_t source_generation = panel->track_generation();
  const size_t bytes = sizeof(collision_before) + sizeof(candidate.collision) +
                       (sprites_before.size() + candidate.sprites.size()) *
                           sizeof(zelda3::Sprite);
  auto restore =
      [this, room_id, source_rom, panel, generation, source_generation,
       sprites_before, collision_before, preflight_node_sprites,
       candidate = std::move(candidate)](bool forward) -> absl::Status {
    auto* current = rooms_.GetIfLoaded(room_id);
    if (rom_ != source_rom || !current || current->rom() != source_rom ||
        minecart_collision_generation_ != generation ||
        minecart_track_editor_panel_ != panel ||
        panel->track_generation() != source_generation)
      return absl::FailedPreconditionError(
          "Minecart node belongs to a different project generation");
    if (auto* viewer = GetViewerForRoom(room_id);
        viewer && viewer->header_read_only())
      return absl::FailedPreconditionError("This room is read only");
    const auto& expected_sprites = forward ? sprites_before : candidate.sprites;
    const auto& expected_collision =
        forward ? collision_before : candidate.collision;
    if (!SameSprites(current->GetSprites(), expected_sprites) ||
        !(current->custom_collision() == expected_collision))
      return absl::FailedPreconditionError(
          "Minecart node sprites or collision changed outside Undo");
    const auto& replacement_sprites =
        forward ? candidate.sprites : sprites_before;
    RETURN_IF_ERROR(preflight_node_sprites(replacement_sprites));
    if (candidate.track_before) {
      if (forward)
        RETURN_IF_ERROR(ValidateUnusedCartIdentity(rooms_, *rom_, room_id,
                                                   candidate.track_after->id));
      RETURN_IF_ERROR(panel->RestoreTrackDraft(
          source_generation,
          forward ? *candidate.track_before : *candidate.track_after,
          forward ? *candidate.track_after : *candidate.track_before));
    }
    // All fallible work is complete. Publish only the in-memory candidate.
    current->GetSprites() = replacement_sprites;
    current->MarkSpritesDirty();
    const auto& replacement_collision =
        forward ? candidate.collision : collision_before;
    if (!(current->custom_collision() == replacement_collision))
      RestoreRoomCustomCollision(room_id, replacement_collision);
    panel->InvalidateRoomAudit();
    if (candidate.track_before)
      panel->MarkPairedTrackDraft();
    rom_->set_dirty(true);
    current->MarkObjectsDirty();
    InvalidateConnectedRoomGraphs();
    if (auto* viewer = GetViewerForRoom(room_id))
      viewer->TriggerChangePing();
    return absl::OkStatus();
  };
  RETURN_IF_ERROR(restore(true));
  const char* description = brush_snapshot.kind == MinecartNodeKind::kCartStart
                                ? "Place cart start"
                            : brush_snapshot.kind == MinecartNodeKind::kSwitch
                                ? "Place minecart switch"
                                : "Place switch track";
  undo_manager_.Push(std::make_unique<MinecartNodeAction>(
      description, std::move(restore), bytes));
  return absl::OkStatus();
}

}  // namespace yaze::editor
