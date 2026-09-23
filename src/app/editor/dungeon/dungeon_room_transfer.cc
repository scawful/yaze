#include "app/editor/dungeon/dungeon_room_transfer.h"

#include <algorithm>
#include <utility>

#include "app/editor/dungeon/interaction/sprite_interaction_handler.h"
#include "util/macro.h"
#include "zelda3/dungeon/chest_edit.h"
#include "zelda3/dungeon/object_layer_semantics.h"

namespace yaze::editor {
namespace {

bool SameCollision(const zelda3::CustomCollisionMap& a,
                   const zelda3::CustomCollisionMap& b) {
  return a.has_data == b.has_data && a.tiles == b.tiles;
}

bool SameWater(const zelda3::WaterFillZoneMap& a,
               const zelda3::WaterFillZoneMap& b) {
  return a.has_data == b.has_data && a.tiles == b.tiles &&
         a.sram_bit_mask == b.sram_bit_mask;
}

absl::Status ValidateDomains(const DungeonRoomDocument& document,
                             uint16_t domains,
                             bool require_chest_mapping = true) {
  if (document.source_room_id < 0 ||
      document.source_room_id >= zelda3::kNumberOfRooms) {
    return absl::InvalidArgumentError(
        "Room document has an invalid source room ID");
  }
  const auto& contents = document.contents;
  if (domains & kTransferObjects) {
    if (contents.objects.size() > zelda3::kMaxTileObjects ||
        contents.chests.size() > zelda3::kMaxChests) {
      return absl::ResourceExhaustedError(
          "Room object or chest limit exceeded");
    }
    for (const auto& object : contents.objects) {
      const int options = static_cast<int>(object.options());
      const bool torch = (options & 8) != 0;
      const bool block = (options & 4) != 0;
      if (options < 0 || (options & ~63) != 0 || (torch && block)) {
        return absl::InvalidArgumentError(
            "Object has unsupported option flags");
      }
      if (zelda3::UsesRoomObjectStream(object)) {
        RETURN_IF_ERROR(zelda3::ValidateRoomObjectStreamEntryForSave(object));
      } else if (object.x() > (torch ? 62 : 63) ||
                 object.y() > (torch ? 62 : 63) || object.GetLayerValue() > 1 ||
                 object.size() != 0 || object.id_ != (torch ? 0x150 : 0xE00)) {
        return absl::InvalidArgumentError(
            "Special object has unsupported ROM fields");
      }
      if (object.block_behavior_layer() > 1 ||
          object.torch_reserved_bit() > 1 ||
          object.block_load_order() < zelda3::RoomObject::kBlockLoadOrderNew ||
          object.block_load_order() > 65535) {
        return absl::InvalidArgumentError(
            "Special object metadata is out of range");
      }
    }
    if (require_chest_mapping) {
      RETURN_IF_ERROR(zelda3::ValidateChestObjectMapping(contents.objects,
                                                         contents.chests));
      // Treat the candidate as newly placed objects so the shared chest planner
      // validates six-slot ordering instead of taking its unchanged fast path.
      std::vector<std::optional<size_t>> origins(contents.objects.size());
      auto chest_plan =
          zelda3::PlanChestObjectEdit({}, {}, contents.objects, origins);
      if (!chest_plan.ok())
        return chest_plan.status();
    }
  }
  if (domains & kTransferDoors) {
    if (contents.doors.size() > zelda3::kMaxDoors) {
      return absl::ResourceExhaustedError("Room door limit exceeded");
    }
    for (const auto& door : contents.doors) {
      const unsigned type = static_cast<unsigned>(door.type);
      if (door.position > 15 || static_cast<unsigned>(door.direction) > 3 ||
          type > 0x66 || (type & 1) != 0) {
        return absl::InvalidArgumentError(
            "Door contains unsupported ROM fields");
      }
      const auto bytes = door.EncodeBytes();
      if (bytes.first != door.byte1 || bytes.second != door.byte2) {
        return absl::InvalidArgumentError(
            "Door raw bytes do not match its encodable properties");
      }
    }
  }
  if (domains & kTransferSprites) {
    if (contents.sprites.size() > zelda3::kMaxTotalSprites) {
      return absl::ResourceExhaustedError("Room sprite limit exceeded");
    }
    for (const auto& sprite : contents.sprites) {
      RETURN_IF_ERROR(SpriteInteractionHandler::ValidateSpriteProperties(
          sprite.id, sprite.x, sprite.y, sprite.subtype, sprite.layer,
          sprite.key_drop));
    }
  }
  if (domains & kTransferItems) {
    if (contents.items.size() > 4096) {
      return absl::ResourceExhaustedError("Room pot item limit exceeded");
    }
    for (const auto& item : contents.items) {
      // Preserve the encoded tilemap address and layer/control bits. The
      // current position widget's limited pixel decoder is not a ROM validity
      // test (for example, vanilla lower-layer positions include bit 0x2000).
      if (item.position == 0xFFFF) {
        return absl::InvalidArgumentError(
            "Pot item position collides with the stream terminator");
      }
    }
  }
  if (domains & kTransferMetadata) {
    const auto& m = document.metadata;
    if (m.layout > 7 || m.floor1 > 15 || m.floor2 > 15 ||
        static_cast<unsigned>(m.bg2) > 8 || m.layer2_mode > 7 ||
        static_cast<unsigned>(m.collision) > 7 || m.pit_target_layer > 3 ||
        m.layer_merging.ID > 8 || m.layer_merging.Name.size() > 64 ||
        std::any_of(m.staircase_planes.begin(), m.staircase_planes.end(),
                    [](uint8_t value) { return value > 3; })) {
      return absl::InvalidArgumentError(
          "Room metadata contains unencodable fields");
    }
    // LoadRoomHeaderFromRom and SetBg2 derive this tuple from the same header
    // byte. A dark room still retains its hidden three-bit BG2 mode.
    const bool dark = m.bg2 == background2::DarkRoom;
    const auto& expected_merge =
        zelda3::kLayerMergeTypeList[dark ? 8 : m.layer2_mode];
    if (m.is_dark != dark || m.is_light != dark ||
        (!dark && static_cast<uint8_t>(m.bg2) != m.layer2_mode) ||
        m.layer_merging != expected_merge) {
      return absl::InvalidArgumentError(
          "Room BG2 mode, darkness and derived layer settings disagree");
    }
  }
  if ((domains & kTransferCollision) && !document.collision.has_data &&
      std::any_of(document.collision.tiles.begin(),
                  document.collision.tiles.end(),
                  [](uint8_t value) { return value != 0; })) {
    return absl::InvalidArgumentError("Collision tiles require has_data=true");
  }
  if (domains & kTransferWater) {
    const auto& water = document.water;
    const bool any = std::any_of(water.tiles.begin(), water.tiles.end(),
                                 [](uint8_t value) { return value != 0; });
    if (water.has_data != any ||
        std::any_of(water.tiles.begin(), water.tiles.end(),
                    [](uint8_t value) { return value > 1; }) ||
        (water.sram_bit_mask & (water.sram_bit_mask - 1)) != 0) {
      return absl::InvalidArgumentError(
          "Water fill needs boolean tiles and a single SRAM bit");
    }
  }
  return absl::OkStatus();
}

}  // namespace

DungeonRoomDocument CaptureDungeonRoomDocument(const zelda3::Room& room) {
  DungeonRoomDocument result;
  result.source_room_id = room.id();
  result.contents = CaptureDungeonSelectionEditState(room);
  // RoomObject is copyable but contains rendering caches and a borrowed ROM.
  // Rebuild each record from authored fields for a detached document.
  for (auto& source : result.contents.objects) {
    zelda3::RoomObject object(source.id_, source.x_, source.y_, source.size_,
                              source.GetLayerValue());
    object.set_options(source.options());
    object.all_bgs_ = source.all_bgs_;
    object.lit_ = source.lit_;
    object.set_block_load_order(source.block_load_order());
    object.set_block_behavior_layer(source.block_behavior_layer());
    object.set_torch_reserved_bit(source.torch_reserved_bit());
    source = std::move(object);
  }
  result.metadata = room.CaptureMetadataSnapshot();
  result.collision = room.custom_collision();
  result.water = room.water_fill_zone();
  return result;
}

bool SameDungeonRoomDocument(const DungeonRoomDocument& a,
                             const DungeonRoomDocument& b) {
  return ChangedDungeonSelectionDomains(a.contents, b.contents) == 0 &&
         a.metadata == b.metadata && SameCollision(a.collision, b.collision) &&
         SameWater(a.water, b.water);
}

bool DungeonRoomTransferPlan::changed() const {
  return !SameDungeonRoomDocument(before, after);
}

absl::Status ValidateDungeonRoomDocument(const DungeonRoomDocument& document) {
  return ValidateDomains(document, kTransferAll);
}

absl::Status ValidateDungeonRoomDocumentForInterchange(
    const DungeonRoomDocument& document) {
  return ValidateDomains(document, kTransferAll, false);
}

absl::StatusOr<DungeonRoomTransferPlan> PlanDungeonRoomTransfer(
    const zelda3::Room& target, const DungeonRoomDocument& source,
    DungeonRoomTransferOptions options) {
  if (target.id() < 0 || target.id() >= zelda3::kNumberOfRooms ||
      (options.domains & ~kTransferAll) != 0) {
    return absl::InvalidArgumentError(
        "Invalid room transfer destination or domains");
  }
  RETURN_IF_ERROR(ValidateDomains(source, options.domains));
  // Interchange/undo retain unknown legacy bytes; new authoring cannot choose
  // graphics past the table consumed by Room::LoadRoomGraphics (ID + 64).
  if ((options.domains & kTransferMetadata) &&
      source.metadata.spriteset > zelda3::kMaxDungeonSpriteset &&
      source.metadata.spriteset != target.spriteset()) {
    return absl::InvalidArgumentError(
        "Dungeon sprite graphics must be in range 0x00..0x4F");
  }
  DungeonRoomTransferPlan plan;
  plan.rom = target.rom();
  plan.target_room_id = target.id();
  plan.options = options;
  plan.before = CaptureDungeonRoomDocument(target);
  plan.after = plan.before;
  auto& after = plan.after;
  if (options.domains & kTransferObjects) {
    after.contents.objects = source.contents.objects;
    for (auto& object : after.contents.objects) {
      object = object.CopyForNewPlacement();
      object.SetRom(nullptr);
    }
    after.contents.chests = source.contents.chests;
  }
  if (options.domains & kTransferDoors)
    after.contents.doors = source.contents.doors;
  if (options.domains & kTransferSprites)
    after.contents.sprites = source.contents.sprites;
  if (options.domains & kTransferItems)
    after.contents.items = source.contents.items;
  if (options.domains & kTransferMetadata) {
    after.metadata = source.metadata;
    if (!options.copy_destinations) {
      after.metadata.holewarp = plan.before.metadata.holewarp;
      after.metadata.pit_target_layer = plan.before.metadata.pit_target_layer;
      after.metadata.staircase_rooms = plan.before.metadata.staircase_rooms;
      after.metadata.staircase_planes = plan.before.metadata.staircase_planes;
    }
  }
  if (options.domains & kTransferCollision)
    after.collision = source.collision;
  if (options.domains & kTransferWater)
    after.water = source.water;
  RETURN_IF_ERROR(ValidateDomains(after, options.domains));
  return plan;
}

void ApplyDungeonRoomDocument(zelda3::Room& room,
                              const DungeonRoomDocument& document) {
  auto contents = document.contents;
  for (auto& object : contents.objects)
    object.SetRom(room.rom());
  ApplyDungeonSelectionEditState(room, contents,
                                 kSelectionObjects | kSelectionDoors |
                                     kSelectionSprites | kSelectionItems);
  room.RestoreMetadataSnapshot(document.metadata);
  if (!SameCollision(room.custom_collision(), document.collision)) {
    room.custom_collision() = document.collision;
    room.MarkCustomCollisionDirty();
  }
  if (!SameWater(room.water_fill_zone(), document.water)) {
    room.ClearWaterFillZone();
    room.set_water_fill_sram_bit_mask(document.water.sram_bit_mask);
    for (size_t tile = 0; tile < document.water.tiles.size(); ++tile) {
      if (document.water.tiles[tile] != 0) {
        room.SetWaterFillTile(static_cast<int>(tile % 64),
                              static_cast<int>(tile / 64), true);
      }
    }
    // Keep the cached count from the setters, then restore exact legacy bytes
    // for history. Newly imported documents already require boolean tiles.
    room.water_fill_zone() = document.water;
    room.MarkWaterFillDirty();
  }
}

}  // namespace yaze::editor
