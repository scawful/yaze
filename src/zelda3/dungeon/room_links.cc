#include "zelda3/dungeon/room_links.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_collision.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_header_destination.h"
#include "zelda3/dungeon/room_layout.h"

namespace yaze::zelda3 {

namespace {

// ZScream's expanded entrance tables: the loader's TAX/ASL/ASL/TAY at
// $02:D99F becomes JSL $0FF008, which does the same and sets the data bank
// to $0F, so the loader reads rooms from $0F:8000 and dungeon IDs from
// $0F:9800.
constexpr int kEntranceLoaderHookPc = 0x1599F;
constexpr int kExpandedEntranceRoomPc = 0x078000;
constexpr int kExpandedEntranceDungeonPc = 0x079800;
constexpr int kDirectionWest = 2;
constexpr int kDirectionEast = 3;

bool IsEncodedStreamObject(const RoomObject& object) {
  return (object.options() & ObjectOption::Torch) == ObjectOption::Nothing &&
         (object.options() & ObjectOption::Block) == ObjectOption::Nothing;
}

bool IsHeaderStairObject(int id) {
  return id == 0x12D || id == 0x12E || id == 0x12F ||
         (id >= 0x138 && id <= 0x13B) || (id >= 0xF9E && id <= 0xFA1) ||
         (id >= 0xFA6 && id <= 0xFA9);
}

bool IsKeyStairDoor(DoorType type) {
  return type == DoorType::SmallKeyStairsUp ||
         type == DoorType::SmallKeyStairsDown ||
         type == DoorType::SmallKeyStairsUpLower ||
         type == DoorType::SmallKeyStairsDownLower;
}

// Outer-wall door slots (USDASM RoomDraw_DoorPartner*): N/W 0..5, S/E 6..11.
bool IsOuterDoorPosition(int direction, int position) {
  if (position >= 12) {
    return false;
  }
  return (direction == 0 || direction == 2) ? position < 6 : position >= 6;
}

int OppositeDirection(int direction) {
  return direction ^ 1;
}

const char* DirectionName(int direction) {
  switch (direction) {
    case 0:
      return "north";
    case 1:
      return "south";
    case 2:
      return "west";
    case 3:
      return "east";
  }
  return "?";
}

std::string RoomHex(int room_id) {
  return absl::StrFormat("0x%02X", room_id);
}

}  // namespace

const char* RoomReferenceKindName(RoomReferenceKind kind) {
  switch (kind) {
    case RoomReferenceKind::kEntrance:
      return "entrance";
    case RoomReferenceKind::kHole:
      return "hole";
    case RoomReferenceKind::kSpawn:
      return "spawn";
    case RoomReferenceKind::kUnplacedEntrance:
      return "unplaced_entrance";
    case RoomReferenceKind::kDoor:
      return "door";
    case RoomReferenceKind::kStair:
      return "stair";
    case RoomReferenceKind::kHolewarp:
      return "holewarp";
    case RoomReferenceKind::kWarpTag:
      return "warp_tag";
    case RoomReferenceKind::kHeaderOnly:
      return "header_only";
    case RoomReferenceKind::kProjectListing:
      return "project";
    case RoomReferenceKind::kDungeonMap:
      return "dungeon_map";
    case RoomReferenceKind::kTeleportDoor:
      return "teleport_door";
  }
  return "?";
}

const char* RoomDoorRoleName(RoomDoorRole role) {
  switch (role) {
    case RoomDoorRole::kNeighbor:
      return "neighbor";
    case RoomDoorRole::kTeleport:
      return "teleport";
    case RoomDoorRole::kExit:
      return "exit";
    case RoomDoorRole::kKeyStairs:
      return "key_stairs";
  }
  return "?";
}

bool IsDungeonRoomId(int room_id) {
  return room_id >= 0 && room_id < kDungeonRoomCount;
}

int DungeonNeighborRoom(int room_id, int direction) {
  if (!IsDungeonRoomId(room_id)) {
    return -1;
  }
  const int low = room_id & 0xFF;
  const int row = low >> 4;
  const int col = low & 0x0F;
  int target = -1;
  switch (direction) {
    case 0:  // North
      target = row > 0 ? room_id - 16 : -1;
      break;
    case 1:  // South
      target = row < 15 ? room_id + 16 : -1;
      break;
    case 2:  // West
      target = col > 0 ? room_id - 1 : -1;
      break;
    case 3:  // East
      target = col < 15 ? room_id + 1 : -1;
      break;
  }
  return IsDungeonRoomId(target) ? target : -1;
}

int CheckedHeaderDestinationRoom(int source_room_id, uint8_t header_byte) {
  const int room = ResolveHeaderDestinationRoom(source_room_id, header_byte);
  return IsDungeonRoomId(room) ? room : -1;
}

bool UsesExpandedEntranceTables(const Rom& rom) {
  const auto& data = rom.vector();
  if (kEntranceLoaderHookPc + 3 >= static_cast<int>(data.size())) {
    return false;
  }
  return data[kEntranceLoaderHookPc] == 0x22 &&
         data[kEntranceLoaderHookPc + 1] == 0x08 &&
         data[kEntranceLoaderHookPc + 2] == 0xF0 &&
         data[kEntranceLoaderHookPc + 3] == 0x0F;
}

std::optional<DungeonEntranceTarget> ReadDungeonEntranceTarget(
    const Rom& rom, int entrance_id) {
  if (entrance_id < 0 || entrance_id >= kDungeonEntranceCount) {
    return std::nullopt;
  }
  const auto& data = rom.vector();
  const bool expanded = UsesExpandedEntranceTables(rom);
  const int room_pc = expanded ? kExpandedEntranceRoomPc + entrance_id * 2
                               : kEntranceRoom + entrance_id * 2;
  const int dungeon_pc = expanded ? kExpandedEntranceDungeonPc + entrance_id
                                  : kEntranceDungeon + entrance_id;
  if (room_pc + 1 >= static_cast<int>(data.size()) ||
      dungeon_pc >= static_cast<int>(data.size())) {
    return std::nullopt;
  }
  DungeonEntranceTarget target;
  target.room_id = data[room_pc] | (data[room_pc + 1] << 8);
  target.dungeon_id = data[dungeon_pc];
  target.expanded = expanded;
  return target;
}

// Pits, pit edges, layer-2 pit masks, and bombable floor (bombing it opens a
// hole that uses the holewarp, e.g. Oracle D5 0xAD -> boss room 0xAC).
bool IsPitObjectId(int id) {
  return (id >= 0x023 && id <= 0x02E) || id == 0x06A || id == 0x06B ||
         id == 0x0A4 || id == 0x0C2 || id == 0x0C3 || id == 0xFC7 ||
         id == 0xFE6;
}

bool IsWarpTileObjectId(int id) {
  return id == 0xFCA;
}

bool IsHoleRoomTag(uint8_t tag) {
  switch (static_cast<TagKey>(tag)) {
    case TagKey::Holes_0:
    case TagKey::Open_Chest_Activate_Holes_0:
    case TagKey::Holes_1:
    case TagKey::Holes_2:
    case TagKey::Holes_3:
    case TagKey::Holes_4:
    case TagKey::Holes_5:
    case TagKey::Holes_6:
    case TagKey::Holes_7:
    case TagKey::Holes_8:
    case TagKey::Open_Chest_for_Holes_8:
      return true;
    default:
      return false;
  }
}

// TileBehavior_Pit (0x20) and TileBehavior_Warp (0x4B) in custom collision.
bool IsPitTileAttribute(uint8_t attribute) {
  return attribute == 0x20;
}

bool IsWarpTileAttribute(uint8_t attribute) {
  return attribute == 0x4B;
}

RoomLinkFacts CollectRoomLinkFacts(Rom* rom, const Room& room,
                                   const PitDamageTable* pit_table) {
  RoomLinkFacts facts;
  facts.room_id = room.id();
  facts.blockset = room.blockset();
  facts.tag1 = static_cast<uint8_t>(room.tag1());
  facts.tag2 = static_cast<uint8_t>(room.tag2());
  facts.holewarp_byte = room.holewarp();
  for (int slot = 0; slot < 4; ++slot) {
    facts.stair_bytes[slot] = room.staircase_room(slot);
  }
  facts.in_pit_damage_table =
      pit_table != nullptr &&
      pit_table->Contains(static_cast<uint16_t>(room.id()));
  facts.has_hole_tag = IsHoleRoomTag(facts.tag1) || IsHoleRoomTag(facts.tag2);

  RoomLayout layout(rom);
  const bool layout_ok = layout.LoadLayout(room.layout_id()).ok();
  const std::vector<RoomObject> no_objects;
  const auto& layout_objects = layout_ok ? layout.GetObjects() : no_objects;

  bool has_stairs = false;
  for (const auto& object : room.GetTileObjects()) {
    if (!IsEncodedStreamObject(object)) {
      continue;
    }
    facts.has_pits |= IsPitObjectId(object.id_);
    facts.has_warp_tiles |= IsWarpTileObjectId(object.id_);
    has_stairs |= IsHeaderStairObject(object.id_);
  }
  if (room.has_custom_collision()) {
    for (uint8_t tile : room.custom_collision().tiles) {
      facts.has_pits |= IsPitTileAttribute(tile);
      facts.has_warp_tiles |= IsWarpTileAttribute(tile);
    }
  }
  // Stair objects -> header slots, same replay as
  // `dungeon-describe-room --include-staircase-resolution`.
  if (has_stairs && rom != nullptr) {
    auto collision_input = MakeRoomCollisionInput(room);
    if (layout_ok) {
      collision_input.objects.insert(collision_input.objects.begin(),
                                     layout_objects.begin(),
                                     layout_objects.end());
    }
    const size_t prefix_size = layout_ok ? layout_objects.size() : 0;
    const auto resolutions =
        ResolveVanillaStaircaseSlots(*rom, collision_input);
    int placement_index = 0;
    for (const auto& resolution : resolutions) {
      if (resolution.object_index < prefix_size) {
        continue;
      }
      if (resolution.slot.has_value() && *resolution.slot >= 0 &&
          *resolution.slot < 4) {
        facts.stair_slot_used[*resolution.slot] = true;
        facts.stair_slot_estimated[*resolution.slot] = false;
      } else if (placement_index < 4 &&
                 !facts.stair_slot_used[placement_index]) {
        // Replay could not place it; fall back to placement order.
        facts.stair_slot_used[placement_index] = true;
        facts.stair_slot_estimated[placement_index] = true;
      }
      ++placement_index;
    }
  }

  for (const auto& door : room.GetDoors()) {
    RoomLinkFacts::Door fact;
    fact.direction = static_cast<int>(door.direction);
    const auto [tile_x, tile_y] = door.GetTileCoords();
    fact.tile_x = tile_x;
    fact.tile_y = tile_y;
    fact.along = (fact.direction <= 1) ? tile_x : tile_y;
    fact.outer = IsOuterDoorPosition(fact.direction, door.position);
    fact.type = static_cast<uint8_t>(door.type);
    if (IsExitDoorType(door.type)) {
      fact.role = RoomDoorRole::kExit;
    } else if (!IsRoomConnectionDoorType(door.type)) {
      continue;  // Layer and dungeon swap markers.
    } else if (IsKeyStairDoor(door.type)) {
      fact.role = RoomDoorRole::kKeyStairs;
    } else if (fact.type == kTeleportDoorType &&
               (fact.direction == kDirectionWest ||
                fact.direction == kDirectionEast)) {
      fact.role = RoomDoorRole::kTeleport;
    }
    facts.doors.push_back(fact);
  }
  return facts;
}

std::vector<RoomLink> CollectRoomLinks(int room_id,
                                       const RoomLinkFactsLookup& lookup,
                                       const std::set<uint8_t>& warp_tag_ids) {
  std::vector<RoomLink> links;
  if (!IsDungeonRoomId(room_id) || !lookup) {
    return links;
  }
  const RoomLinkFacts* self = lookup(room_id);
  if (self == nullptr) {
    return links;
  }
  const RoomLinkFacts& f = *self;
  auto facts_of = [&](int room) -> const RoomLinkFacts* {
    return IsDungeonRoomId(room) ? lookup(room) : nullptr;
  };
  auto make = [&](RoomReferenceKind kind, int raw_to) {
    RoomLink link;
    link.from = room_id;
    link.raw_to = raw_to;
    link.to = IsDungeonRoomId(raw_to) ? raw_to : -1;
    link.kind = kind;
    return link;
  };
  auto same_blockset = [&](int room) {
    const RoomLinkFacts* other = facts_of(room);
    return other != nullptr && other->blockset == f.blockset;
  };
  // Header slots that stair objects in `room` use.
  auto stair_targets = [&](int room) {
    std::vector<int> targets;
    const RoomLinkFacts* other = facts_of(room);
    if (other == nullptr) {
      return targets;
    }
    for (int slot = 0; slot < 4; ++slot) {
      if (other->stair_slot_used[slot]) {
        targets.push_back(
            ResolveHeaderDestinationRoom(room, other->stair_bytes[slot]));
      }
    }
    return targets;
  };

  // ---- Doors ---------------------------------------------------------------
  std::array<bool, 4> teleport_slot{};
  for (size_t index = 0; index < f.doors.size(); ++index) {
    const auto& door = f.doors[index];
    if (!door.outer || (door.role != RoomDoorRole::kNeighbor &&
                        door.role != RoomDoorRole::kTeleport)) {
      continue;
    }
    if (door.role == RoomDoorRole::kTeleport) {
      const int slot = door.direction == kDirectionEast ? 3 : 2;
      RoomLink link =
          make(RoomReferenceKind::kTeleportDoor,
               ResolveHeaderDestinationRoom(room_id, f.stair_bytes[slot]));
      link.slot = slot;
      link.direction = door.direction;
      link.door = static_cast<int>(index);
      link.strong = true;
      link.owner_ok = same_blockset(link.to);
      link.detail = absl::StrFormat(
          "teleport door from %s (%s wall, stair "
          "slot %d)",
          RoomHex(room_id), DirectionName(door.direction), slot + 1);
      teleport_slot[slot] = true;
      links.push_back(std::move(link));
      continue;
    }
    const int neighbor = DungeonNeighborRoom(room_id, door.direction);
    if (neighbor < 0) {
      continue;
    }
    bool mutual = false;
    if (const RoomLinkFacts* other = facts_of(neighbor)) {
      for (const auto& back : other->doors) {
        if (back.outer &&
            (back.role == RoomDoorRole::kNeighbor ||
             back.role == RoomDoorRole::kTeleport) &&
            back.direction == OppositeDirection(door.direction) &&
            std::abs(back.along - door.along) <= 3) {
          mutual = true;
          break;
        }
      }
    }
    RoomLink link = make(RoomReferenceKind::kDoor, neighbor);
    link.direction = door.direction;
    link.door = static_cast<int>(index);
    link.strong = mutual;
    link.owner_ok = mutual;
    link.detail =
        mutual
            ? absl::StrFormat("door from %s (%s wall)", RoomHex(room_id),
                              DirectionName(door.direction))
            : absl::StrFormat("one-sided door from %s (%s wall, no door back)",
                              RoomHex(room_id), DirectionName(door.direction));
    links.push_back(std::move(link));
  }

  // ---- Header stair slots -------------------------------------------------
  const bool warp_tag =
      warp_tag_ids.contains(f.tag1) || warp_tag_ids.contains(f.tag2);
  for (int slot = 0; slot < 4; ++slot) {
    RoomLink link =
        make(RoomReferenceKind::kStair,
             ResolveHeaderDestinationRoom(room_id, f.stair_bytes[slot]));
    link.slot = slot;
    if (f.stair_slot_used[slot]) {
      link.strong = true;
      const auto back = stair_targets(link.to);
      link.owner_ok =
          same_blockset(link.to) ||
          std::find(back.begin(), back.end(), room_id) != back.end();
      link.detail = absl::StrFormat(
          "stair (slot %d) in %s%s", slot + 1, RoomHex(room_id),
          f.stair_slot_estimated[slot] ? ", slot by placement order" : "");
    } else if (warp_tag) {
      link.kind = RoomReferenceKind::kWarpTag;
      link.strong = true;
      link.owner_ok = same_blockset(link.to);
      link.detail = absl::StrFormat("warp tag quadrant %d in %s", slot + 1,
                                    RoomHex(room_id));
    } else if (teleport_slot[slot]) {
      continue;  // The teleport door link already covers this slot.
    } else if (f.stair_bytes[slot] != 0) {
      link.kind = RoomReferenceKind::kHeaderOnly;
      link.detail = absl::StrFormat(
          "header stair slot %d of %s (no stair object uses it)", slot + 1,
          RoomHex(room_id));
    } else {
      continue;  // Byte 0 with no user: not a reference.
    }
    links.push_back(std::move(link));
  }

  // ---- Holewarp -----------------------------------------------------------
  RoomLink hole = make(RoomReferenceKind::kHolewarp,
                       ResolveHeaderDestinationRoom(room_id, f.holewarp_byte));
  hole.owner_ok = same_blockset(hole.to);
  if (f.has_warp_tiles) {
    hole.strong = true;
    hole.detail = absl::StrFormat("warp tiles in %s", RoomHex(room_id));
  } else if (f.has_pits && !f.in_pit_damage_table) {
    hole.strong = true;
    hole.detail = absl::StrFormat("pits in %s", RoomHex(room_id));
  } else if (f.has_pits) {
    hole.detail = absl::StrFormat(
        "holewarp of %s (its pits only cost a heart: RoomsWithPitDamage)",
        RoomHex(room_id));
  } else if (f.has_hole_tag) {
    hole.detail = absl::StrFormat("holewarp of %s (tag-driven holes only)",
                                  RoomHex(room_id));
  } else if (f.holewarp_byte != 0) {
    hole.kind = RoomReferenceKind::kHeaderOnly;
    hole.detail =
        absl::StrFormat("header holewarp of %s (no pits or warp tiles there)",
                        RoomHex(room_id));
  } else {
    return links;  // Byte 0 with no user: not a reference.
  }
  if (hole.strong && !hole.owner_ok) {
    hole.detail += " (other blockset)";
  }
  links.push_back(std::move(hole));
  return links;
}

RoomLinkFactsCache::RoomLinkFactsCache(Rom* rom)
    : rom_(rom), facts_(kDungeonRoomCount) {
  if (rom_ != nullptr && rom_->is_loaded()) {
    auto table = std::make_unique<PitDamageTable>();
    if (PitDamageTable::LoadFromRom(rom_, table.get()).ok()) {
      pit_table_ = std::move(table);
    }
  }
}

RoomLinkFactsCache::~RoomLinkFactsCache() = default;

const RoomLinkFacts* RoomLinkFactsCache::Get(int room_id) {
  if (!IsDungeonRoomId(room_id) || rom_ == nullptr || !rom_->is_loaded()) {
    return nullptr;
  }
  auto& slot = facts_[room_id];
  if (!slot.has_value()) {
    Room room = LoadRoomFromRom(rom_, room_id);
    room.LoadSprites();
    slot = CollectRoomLinkFacts(rom_, room, pit_table_.get());
  }
  return &*slot;
}

RoomLinkFactsLookup RoomLinkFactsCache::AsLookup() {
  return [this](int room_id) {
    return Get(room_id);
  };
}

}  // namespace yaze::zelda3
