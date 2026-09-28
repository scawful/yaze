#include "zelda3/dungeon/room_census.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <utility>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "core/hack_manifest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_census_vanilla_fingerprints.h"
#include "zelda3/dungeon/room_collision.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_header_destination.h"
#include "zelda3/dungeon/room_layout.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/screen/dungeon_map.h"

namespace yaze::zelda3 {

namespace {

constexpr int kRooms = kRoomCensusRoomCount;
// Free-room blocks: a contiguous core of at least kFreeBlockMinCore rooms
// takes in smaller free cores within kFreeBlockMergeDistance grid cells
// (Chebyshev distance, same $A1 page).
constexpr int kFreeBlockMinCore = 3;
constexpr int kFreeBlockMergeDistance = 3;
// Custom collision (ZScream) tile types.
constexpr uint8_t kCollisionPit = 0x20;
constexpr uint8_t kCollisionWarp = 0x4B;
// ZScream's expanded entrance tables: the loader at $02:D99F becomes
// JSL $0FF008 and reads rooms from $0F:8000 and dungeon IDs from $0F:9800.
constexpr int kEntranceLoaderHookPc = 0x1599F;
constexpr int kExpandedEntranceRoomPc = 0x078000;
constexpr int kExpandedEntranceDungeonPc = 0x079800;
constexpr int kVanillaEntranceCount = 0x85;

bool IsEncodedStreamObject(const RoomObject& object) {
  return (object.options() & ObjectOption::Torch) == ObjectOption::Nothing &&
         (object.options() & ObjectOption::Block) == ObjectOption::Nothing;
}

// Pits, pit edges, layer-2 pit masks, and bombable floor (bombing it opens a
// hole that uses the holewarp, e.g. Oracle D5 0xAD -> boss room 0xAC).
bool IsPitObject(int id) {
  return (id >= 0x023 && id <= 0x02E) || id == 0x06A || id == 0x06B ||
         id == 0x0A4 || id == 0x0C2 || id == 0x0C3 || id == 0xFC7 ||
         id == 0xFE6;
}

bool IsWarpTileObject(int id) {
  return id == 0xFCA;
}

bool IsHeaderStairObject(int id) {
  return id == 0x12D || id == 0x12E || id == 0x12F ||
         (id >= 0x138 && id <= 0x13B) || (id >= 0xF9E && id <= 0xFA1) ||
         (id >= 0xFA6 && id <= 0xFA9);
}

bool IsHoleTag(uint8_t tag) {
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

// Edge transitions change only $A0; $A1 is unchanged, and the room grid
// is 16 wide. Returns -1 when the neighbor would leave the page.
int NeighborRoom(int room_id, int direction) {
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
  return (target >= 0 && target < kRooms) ? target : -1;
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

std::string VanillaDungeonName(int dungeon_id) {
  switch (dungeon_id & 0xFE) {
    case 0x00:
      return "Sewers";
    case 0x02:
      return "Hyrule Castle";
    case 0x04:
      return "Eastern Palace";
    case 0x06:
      return "Desert Palace";
    case 0x08:
      return "Agahnim's Tower";
    case 0x0A:
      return "Swamp Palace";
    case 0x0C:
      return "Palace of Darkness";
    case 0x0E:
      return "Misery Mire";
    case 0x10:
      return "Skull Woods";
    case 0x12:
      return "Ice Palace";
    case 0x14:
      return "Tower of Hera";
    case 0x16:
      return "Thieves' Town";
    case 0x18:
      return "Turtle Rock";
    case 0x1A:
      return "Ganon's Tower";
  }
  return "";
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

// Room and dungeon ID of an entrance, from the table the game reads.
std::optional<std::pair<int, int>> ReadEntrance(const Rom& rom, int entrance,
                                                bool expanded) {
  const auto& data = rom.vector();
  const int room_pc = expanded ? kExpandedEntranceRoomPc + entrance * 2
                               : kEntranceRoom + entrance * 2;
  const int dungeon_pc = expanded ? kExpandedEntranceDungeonPc + entrance
                                  : kEntranceDungeon + entrance;
  if (room_pc + 1 >= static_cast<int>(data.size()) ||
      dungeon_pc >= static_cast<int>(data.size())) {
    return std::nullopt;
  }
  const int room = data[room_pc] | (data[room_pc + 1] << 8);
  return std::make_pair(room, static_cast<int>(data[dungeon_pc]));
}

std::vector<uint16_t> FingerprintRoom(const Room& room) {
  std::vector<uint16_t> hashes;
  for (const auto& object : room.GetTileObjects()) {
    if (!IsEncodedStreamObject(object)) {
      continue;
    }
    hashes.push_back(HashRoomCensusObject(object.id_, object.x(), object.y(),
                                          object.size()));
  }
  std::sort(hashes.begin(), hashes.end());
  return hashes;
}

struct Edge {
  int from = -1;
  int to = -1;
  RoomReferenceKind kind = RoomReferenceKind::kDoor;
  bool strong = false;
  bool owner_ok = false;  // May carry ownership (same area).
  std::string detail;
};

}  // namespace

const char* RoomCensusStatusName(RoomCensusStatus status) {
  switch (status) {
    case RoomCensusStatus::kFree:
      return "free";
    case RoomCensusStatus::kReclaimable:
      return "reclaimable";
    case RoomCensusStatus::kInUse:
      return "in_use";
  }
  return "in_use";
}

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
  }
  return "?";
}

uint16_t HashRoomCensusObject(int id, int x, int y, int size) {
  uint32_t key = ((static_cast<uint32_t>(id) & 0xFFFu) << 20) |
                 ((static_cast<uint32_t>(x) & 0x3Fu) << 14) |
                 ((static_cast<uint32_t>(y) & 0x3Fu) << 8) |
                 (static_cast<uint32_t>(size) & 0xFFu);
  key ^= 0x5A17C3E5u;
  key *= 0x9E3779B1u;
  key ^= key >> 15;
  key *= 0x85EBCA77u;
  key ^= key >> 13;
  return static_cast<uint16_t>(key >> 16);
}

float RoomFingerprintSimilarity(const std::vector<uint16_t>& a,
                                const std::vector<uint16_t>& b) {
  // Multiset Jaccard over sorted inputs.
  if (a.empty() && b.empty()) {
    return 1.0f;
  }
  size_t i = 0;
  size_t j = 0;
  size_t common = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      ++common;
      ++i;
      ++j;
    } else if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  const size_t total = a.size() + b.size() - common;
  return total == 0 ? 1.0f
                    : static_cast<float>(common) / static_cast<float>(total);
}

absl::StatusOr<std::vector<std::vector<uint16_t>>> ComputeRoomFingerprints(
    Rom* rom) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }
  std::vector<std::vector<uint16_t>> out(kRooms);
  for (int room_id = 0; room_id < kRooms; ++room_id) {
    Room room = LoadRoomFromRom(rom, room_id);
    out[room_id] = FingerprintRoom(room);
  }
  return out;
}

std::vector<RoomCensusOwnerGroup> RoomCensusOwnersFromProject(
    const core::ProjectRegistry& registry) {
  std::vector<RoomCensusOwnerGroup> groups;
  for (const auto& dungeon : registry.dungeons) {
    RoomCensusOwnerGroup group;
    group.id = dungeon.id;
    group.name = dungeon.name.empty() ? dungeon.id : dungeon.name;
    for (const auto& room : dungeon.rooms) {
      if (room.id >= 0 && room.id < kRooms) {
        group.rooms.push_back(room.id);
      }
    }
    if (!group.rooms.empty()) {
      groups.push_back(std::move(group));
    }
  }
  return groups;
}

std::set<uint8_t> RoomCensusWarpTagsFromManifest(
    const core::HackManifest& manifest) {
  std::set<uint8_t> tags;
  for (const auto& tag : manifest.room_tags()) {
    std::string lowered = tag.name;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lowered.find("warptag") != std::string::npos || lowered == "warp_tag") {
      tags.insert(tag.tag_id);
    }
  }
  return tags;
}

absl::StatusOr<RoomCensusInput> CollectRoomCensusInput(
    Rom* rom, const RoomCensusOptions& options) {
  if (rom == nullptr || !rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }
  RoomCensusInput input;
  input.rooms.resize(kRooms);

  PitDamageTable pit_table;
  const bool has_pit_table = PitDamageTable::LoadFromRom(rom, &pit_table).ok();

  std::vector<std::vector<uint16_t>> vanilla_from_rom;
  if (options.vanilla_rom != nullptr) {
    auto fingerprints = ComputeRoomFingerprints(options.vanilla_rom);
    if (!fingerprints.ok()) {
      return fingerprints.status();
    }
    vanilla_from_rom = std::move(fingerprints).value();
    input.has_vanilla_baseline = true;
    input.vanilla_baseline_source =
        "vanilla ROM " + options.vanilla_rom->filename();
  } else if (HasBuiltinVanillaRoomFingerprints()) {
    input.has_vanilla_baseline = true;
    input.vanilla_baseline_source = "built-in US 1.0 fingerprints";
  }

  for (int room_id = 0; room_id < kRooms; ++room_id) {
    Room room = LoadRoomFromRom(rom, room_id);
    room.LoadSprites();
    RoomCensusRoomFacts& facts = input.rooms[room_id];
    facts.room_id = room_id;
    facts.blockset = room.blockset();
    facts.tag1 = static_cast<uint8_t>(room.tag1());
    facts.tag2 = static_cast<uint8_t>(room.tag2());
    facts.sprite_count = static_cast<int>(room.GetSprites().size());
    facts.chest_count = static_cast<int>(room.GetChests().size());
    facts.holewarp_byte = room.holewarp();
    for (int slot = 0; slot < 4; ++slot) {
      facts.stair_bytes[slot] = room.staircase_room(slot);
    }
    facts.in_pit_damage_table =
        has_pit_table && pit_table.Contains(static_cast<uint16_t>(room_id));
    facts.has_hole_tag = IsHoleTag(facts.tag1) || IsHoleTag(facts.tag2);

    bool has_stairs = false;
    for (const auto& object : room.GetTileObjects()) {
      if (!IsEncodedStreamObject(object)) {
        continue;
      }
      ++facts.object_count;
      facts.has_pits |= IsPitObject(object.id_);
      facts.has_warp_tiles |= IsWarpTileObject(object.id_);
      has_stairs |= IsHeaderStairObject(object.id_);
    }
    if (room.has_custom_collision()) {
      for (uint8_t tile : room.custom_collision().tiles) {
        facts.has_pits |= tile == kCollisionPit;
        facts.has_warp_tiles |= tile == kCollisionWarp;
      }
    }

    // Stair objects -> header slots, same replay as
    // `dungeon-describe-room --include-staircase-resolution`.
    if (has_stairs) {
      RoomLayout layout(rom);
      const bool layout_ok = layout.LoadLayout(room.layout_id()).ok();
      auto collision_input = MakeRoomCollisionInput(room);
      const auto& prefix = layout.GetObjects();
      if (layout_ok) {
        collision_input.objects.insert(collision_input.objects.begin(),
                                       prefix.begin(), prefix.end());
      }
      const size_t prefix_size = layout_ok ? prefix.size() : 0;
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
      if (!IsRoomConnectionDoorType(door.type) || IsKeyStairDoor(door.type)) {
        continue;
      }
      RoomCensusRoomFacts::Door fact;
      fact.direction = static_cast<int>(door.direction);
      const auto [tile_x, tile_y] = door.GetTileCoords();
      fact.along = (fact.direction <= 1) ? tile_x : tile_y;
      fact.outer = IsOuterDoorPosition(fact.direction, door.position);
      facts.doors.push_back(fact);
    }

    if (input.has_vanilla_baseline) {
      const auto fingerprint = FingerprintRoom(room);
      const std::vector<uint16_t> vanilla =
          !vanilla_from_rom.empty() ? vanilla_from_rom[room_id]
                                    : BuiltinVanillaRoomFingerprint(room_id);
      facts.vanilla_similarity =
          RoomFingerprintSimilarity(fingerprint, vanilla);
    }
  }

  // Entrances. The game reads ZScream's bank $0F tables when present.
  input.expanded_entrance_tables = UsesExpandedEntranceTables(*rom);
  std::set<int> placed_entrances;
  auto add_entrance = [&](int entrance_id, RoomReferenceKind kind,
                          std::string placement) {
    const auto entry =
        ReadEntrance(*rom, entrance_id, input.expanded_entrance_tables);
    if (!entry.has_value() || entry->first < 0 || entry->first >= kRooms) {
      return;
    }
    RoomCensusEntranceFact fact;
    fact.entrance_id = entrance_id;
    fact.room_id = entry->first;
    fact.dungeon_id = entry->second;
    fact.kind = kind;
    fact.placement = std::move(placement);
    input.entrances.push_back(std::move(fact));
  };
  if (auto entrances = LoadEntrances(rom); entrances.ok()) {
    for (const auto& entrance : *entrances) {
      if (entrance.map_pos_ == 0xFFFF) {
        continue;
      }
      placed_entrances.insert(entrance.entrance_id_);
      add_entrance(entrance.entrance_id_, RoomReferenceKind::kEntrance,
                   absl::StrFormat("OW 0x%02X", entrance.map_id_ & 0xFF));
    }
  }
  if (auto holes = LoadHoles(rom); holes.ok()) {
    for (const auto& hole : *holes) {
      placed_entrances.insert(hole.entrance_id_);
      add_entrance(hole.entrance_id_, RoomReferenceKind::kHole,
                   absl::StrFormat("OW 0x%02X", hole.map_id_ & 0xFF));
    }
  }
  for (int entrance_id = 0; entrance_id < kVanillaEntranceCount;
       ++entrance_id) {
    if (!placed_entrances.contains(entrance_id)) {
      add_entrance(entrance_id, RoomReferenceKind::kUnplacedEntrance, "");
    }
  }
  for (int spawn_id = 0; spawn_id < kNumDungeonSpawnPoints; ++spawn_id) {
    auto spawn = DungeonSpawnPoint::Load(*rom, spawn_id);
    if (!spawn.ok() || spawn->room_id >= kRooms) {
      continue;
    }
    RoomCensusEntranceFact fact;
    fact.entrance_id = spawn_id;
    fact.room_id = spawn->room_id;
    fact.dungeon_id = spawn->dungeon_id;
    fact.kind = RoomReferenceKind::kSpawn;
    input.entrances.push_back(std::move(fact));
  }

  {
    DungeonMapLabels labels;
    if (auto maps = LoadDungeonMaps(*rom, labels); maps.ok()) {
      for (const auto& map : *maps) {
        std::vector<int> rooms;
        for (const auto& floor : map.floor_rooms) {
          for (uint8_t room : floor) {
            // 0x0F marks an empty map cell.
            if (room != 0x0F &&
                std::find(rooms.begin(), rooms.end(), room) == rooms.end()) {
              rooms.push_back(room);
            }
          }
        }
        input.pause_map_rooms.push_back(std::move(rooms));
      }
    }
  }

  if (options.project != nullptr) {
    input.project_owners = RoomCensusOwnersFromProject(*options.project);
  }
  if (options.manifest != nullptr) {
    input.warp_tag_ids = RoomCensusWarpTagsFromManifest(*options.manifest);
  }
  return input;
}

std::string FormatRoomBlockSummary(const std::vector<int>& rooms_in) {
  if (rooms_in.empty()) {
    return "";
  }
  std::vector<int> rooms = rooms_in;
  std::sort(rooms.begin(), rooms.end());
  rooms.erase(std::unique(rooms.begin(), rooms.end()), rooms.end());

  std::map<int, int> per_row;
  for (int room : rooms) {
    ++per_row[room >> 4];
  }
  int main_row = rooms.front() >> 4;
  for (const auto& [row, count] : per_row) {
    if (count > per_row[main_row]) {
      main_row = row;
    }
  }
  auto format_runs = [](const std::vector<int>& ids) {
    std::vector<std::string> parts;
    for (size_t i = 0; i < ids.size();) {
      size_t j = i;
      while (j + 1 < ids.size() && ids[j + 1] == ids[j] + 1 &&
             (ids[j + 1] >> 4) == (ids[i] >> 4)) {
        ++j;
      }
      parts.push_back(
          j > i ? absl::StrFormat("%s-%s", RoomHex(ids[i]), RoomHex(ids[j]))
                : RoomHex(ids[i]));
      i = j + 1;
    }
    return absl::StrJoin(parts, "/");
  };
  std::vector<int> main_ids;
  std::vector<int> other_ids;
  for (int room : rooms) {
    ((room >> 4) == main_row ? main_ids : other_ids).push_back(room);
  }
  std::string text =
      absl::StrFormat("Row %X: %s", main_row, format_runs(main_ids));
  if (!other_ids.empty()) {
    text += " + " + format_runs(other_ids);
  }
  text += absl::StrFormat(", %d room%s", static_cast<int>(rooms.size()),
                          rooms.size() == 1 ? "" : "s");
  return text;
}

RoomCensus BuildRoomCensus(const RoomCensusInput& input) {
  RoomCensus census;
  census.rooms.resize(kRooms);
  census.has_vanilla_baseline = input.has_vanilla_baseline;
  census.vanilla_baseline_source = input.vanilla_baseline_source;
  census.expanded_entrance_tables = input.expanded_entrance_tables;

  std::vector<RoomCensusRoomFacts> facts = input.rooms;
  facts.resize(kRooms);
  for (int i = 0; i < kRooms; ++i) {
    facts[i].room_id = i;
  }

  // ---- Edges -------------------------------------------------------------
  std::vector<std::vector<Edge>> out_edges(kRooms);
  auto add_edge = [&](Edge edge) {
    if (edge.to < 0 || edge.to >= kRooms) {
      return;
    }
    out_edges[edge.from].push_back(std::move(edge));
  };

  // Header stair slots that a stair object uses; used for the reciprocity
  // check below. A header byte can resolve past the last room (a page-1 byte
  // >= 0x28 gives 0x128-0x1FF), so check the range before indexing `facts`.
  auto stair_targets = [&](int room) {
    std::vector<int> targets;
    if (room < 0 || room >= kRooms) {
      return targets;
    }
    for (int slot = 0; slot < 4; ++slot) {
      if (facts[room].stair_slot_used[slot]) {
        targets.push_back(
            ResolveHeaderDestinationRoom(room, facts[room].stair_bytes[slot]));
      }
    }
    return targets;
  };

  for (int room = 0; room < kRooms; ++room) {
    const auto& f = facts[room];
    for (const auto& door : f.doors) {
      if (!door.outer) {
        continue;
      }
      const int neighbor = NeighborRoom(room, door.direction);
      if (neighbor < 0) {
        continue;
      }
      bool mutual = false;
      for (const auto& other : facts[neighbor].doors) {
        if (other.outer &&
            other.direction == OppositeDirection(door.direction) &&
            std::abs(other.along - door.along) <= 3) {
          mutual = true;
          break;
        }
      }
      Edge edge;
      edge.from = room;
      edge.to = neighbor;
      edge.kind = RoomReferenceKind::kDoor;
      edge.strong = mutual;
      edge.owner_ok = mutual;
      edge.detail =
          mutual ? absl::StrFormat("door from %s (%s wall)", RoomHex(room),
                                   DirectionName(door.direction))
                 : absl::StrFormat(
                       "one-sided door from %s (%s wall, no door back)",
                       RoomHex(room), DirectionName(door.direction));
      add_edge(std::move(edge));
    }

    const bool warp_tag = input.warp_tag_ids.contains(f.tag1) ||
                          input.warp_tag_ids.contains(f.tag2);
    for (int slot = 0; slot < 4; ++slot) {
      const int target =
          ResolveHeaderDestinationRoom(room, f.stair_bytes[slot]);
      Edge edge;
      edge.from = room;
      edge.to = target;
      if (f.stair_slot_used[slot]) {
        edge.kind = RoomReferenceKind::kStair;
        edge.strong = true;
        const auto back = stair_targets(target);
        edge.owner_ok =
            (target < kRooms && facts[target].blockset == f.blockset) ||
            std::find(back.begin(), back.end(), room) != back.end();
        edge.detail = absl::StrFormat(
            "stair (slot %d) in %s%s", slot + 1, RoomHex(room),
            f.stair_slot_estimated[slot] ? ", slot by placement order" : "");
      } else if (warp_tag) {
        edge.kind = RoomReferenceKind::kWarpTag;
        edge.strong = true;
        edge.owner_ok = target < kRooms && facts[target].blockset == f.blockset;
        edge.detail = absl::StrFormat("warp tag quadrant %d in %s", slot + 1,
                                      RoomHex(room));
      } else if (f.stair_bytes[slot] != 0) {
        edge.kind = RoomReferenceKind::kHeaderOnly;
        edge.detail = absl::StrFormat(
            "header stair slot %d of %s (no stair object uses it)", slot + 1,
            RoomHex(room));
      } else {
        continue;
      }
      add_edge(std::move(edge));
    }

    const int hole_target = ResolveHeaderDestinationRoom(room, f.holewarp_byte);
    Edge hole;
    hole.from = room;
    hole.to = hole_target;
    hole.kind = RoomReferenceKind::kHolewarp;
    hole.owner_ok =
        hole_target < kRooms && facts[hole_target].blockset == f.blockset;
    if (f.has_warp_tiles) {
      hole.strong = true;
      hole.detail = absl::StrFormat("warp tiles in %s", RoomHex(room));
    } else if (f.has_pits && !f.in_pit_damage_table) {
      hole.strong = true;
      hole.detail = absl::StrFormat("pits in %s", RoomHex(room));
    } else if (f.has_pits) {
      hole.detail = absl::StrFormat(
          "holewarp of %s (its pits only cost a heart: RoomsWithPitDamage)",
          RoomHex(room));
    } else if (f.has_hole_tag) {
      hole.detail = absl::StrFormat("holewarp of %s (tag-driven holes only)",
                                    RoomHex(room));
    } else if (f.holewarp_byte != 0) {
      hole.kind = RoomReferenceKind::kHeaderOnly;
      hole.detail = absl::StrFormat(
          "header holewarp of %s (no pits or warp tiles there)", RoomHex(room));
    } else {
      hole.to = -1;
    }
    if (hole.strong) {
      if (!hole.owner_ok) {
        hole.detail += " (other blockset)";
      }
    }
    add_edge(std::move(hole));
  }

  // Inbound references.
  for (int room = 0; room < kRooms; ++room) {
    for (const auto& edge : out_edges[room]) {
      if (edge.to == room) {
        continue;  // A room does not reach itself.
      }
      RoomReference ref;
      ref.kind = edge.kind;
      ref.strong = edge.strong;
      ref.from_room = edge.from;
      ref.detail = edge.detail;
      census.rooms[edge.to].references.push_back(std::move(ref));
    }
  }
  std::vector<int> roots;
  for (const auto& entrance : input.entrances) {
    if (entrance.room_id < 0 || entrance.room_id >= kRooms) {
      continue;
    }
    RoomReference ref;
    ref.kind = entrance.kind;
    ref.entrance_id = entrance.entrance_id;
    ref.strong = entrance.kind != RoomReferenceKind::kUnplacedEntrance;
    switch (entrance.kind) {
      case RoomReferenceKind::kSpawn:
        ref.detail = absl::StrFormat("spawn point %d", entrance.entrance_id);
        break;
      case RoomReferenceKind::kHole:
        ref.detail = absl::StrFormat("overworld hole -> entrance 0x%02X (%s)",
                                     entrance.entrance_id, entrance.placement);
        break;
      case RoomReferenceKind::kUnplacedEntrance:
        ref.detail =
            absl::StrFormat("entrance 0x%02X (not placed on the overworld)",
                            entrance.entrance_id);
        break;
      default:
        ref.detail = absl::StrFormat("entrance 0x%02X (%s)",
                                     entrance.entrance_id, entrance.placement);
        break;
    }
    if (ref.strong) {
      roots.push_back(entrance.room_id);
    }
    census.rooms[entrance.room_id].references.push_back(std::move(ref));
  }

  // ---- Reachability ------------------------------------------------------
  auto is_empty = [&](int room) {
    return facts[room].object_count == 0 ||
           (facts[room].object_count <= 1 && facts[room].sprite_count <= 1);
  };
  std::vector<bool> reached(kRooms, false);
  std::deque<int> queue;
  for (int root : roots) {
    if (!reached[root]) {
      reached[root] = true;
      queue.push_back(root);
    }
  }
  auto drain = [&]() {
    while (!queue.empty()) {
      const int room = queue.front();
      queue.pop_front();
      for (const auto& edge : out_edges[room]) {
        if (edge.strong && !reached[edge.to]) {
          reached[edge.to] = true;
          queue.push_back(edge.to);
        }
      }
    }
  };
  drain();
  // Pause maps: big halls and open floors change rooms without a door
  // object. A non-empty room on the map of a reached dungeon is in use.
  // With project ownership the maps only add caveats: the project registry
  // is the declared structure, and hacks keep stale grids (Oracle's bank $0A
  // grids still list 0x01 under Hyrule Castle and 0x95/0x96 under FoS).
  const bool maps_are_evidence = input.project_owners.empty();
  std::vector<int> map_dungeon(kRooms, -1);
  std::vector<bool> map_ref_added(kRooms, false);
  for (bool changed = true; changed;) {
    changed = false;
    for (size_t map = 0; map < input.pause_map_rooms.size(); ++map) {
      const auto& rooms = input.pause_map_rooms[map];
      const bool active = std::any_of(rooms.begin(), rooms.end(),
                                      [&](int room) { return reached[room]; });
      for (int room : rooms) {
        if (room < 0 || room >= kRooms) {
          continue;
        }
        if (map_dungeon[room] < 0) {
          map_dungeon[room] = static_cast<int>(map) * 2;
        }
        const bool strong = maps_are_evidence && active && !is_empty(room);
        if (!map_ref_added[room] || (strong && !reached[room])) {
          RoomReference ref;
          ref.kind = RoomReferenceKind::kDungeonMap;
          ref.strong = strong;
          ref.detail = absl::StrFormat(
              "on the pause map of dungeon ID 0x%02X%s",
              static_cast<int>(map) * 2,
              !maps_are_evidence ? " (project ownership takes precedence)"
              : !active          ? " (no room of that map is reached)"
              : strong           ? ""
                                 : " (room is empty)");
          if (!map_ref_added[room]) {
            census.rooms[room].references.push_back(ref);
            map_ref_added[room] = true;
          } else {
            for (auto& existing : census.rooms[room].references) {
              if (existing.kind == RoomReferenceKind::kDungeonMap) {
                existing = ref;
                break;
              }
            }
          }
        }
        if (strong && !reached[room]) {
          reached[room] = true;
          queue.push_back(room);
          changed = true;
        }
      }
    }
    drain();
  }

  // ---- Owners ------------------------------------------------------------
  std::vector<int> owner_of(kRooms, -1);
  std::vector<bool> project_listed(kRooms, false);
  census.owners_from_project = !input.project_owners.empty();
  for (const auto& group : input.project_owners) {
    RoomCensusOwner owner;
    owner.id = group.id;
    owner.name = group.name;
    owner.from_project = true;
    const int index = static_cast<int>(census.owners.size());
    census.owners.push_back(owner);
    for (int room : group.rooms) {
      if (room >= 0 && room < kRooms && owner_of[room] < 0) {
        owner_of[room] = index;
        project_listed[room] = true;
      }
    }
  }

  // Vanilla dungeon names only fit a ROM that still looks vanilla; a hack
  // reuses the IDs for its own dungeons.
  int authored_rooms = 0;
  int vanilla_rooms = 0;
  for (const auto& f : facts) {
    if (f.object_count > 0) {
      ++authored_rooms;
      vanilla_rooms += f.vanilla_similarity >= kReclaimableVanillaSimilarity;
    }
  }
  const bool vanilla_names = input.has_vanilla_baseline && authored_rooms > 0 &&
                             vanilla_rooms * 5 >= authored_rooms * 4;
  census.vanilla_owner_names = vanilla_names;

  std::map<int, int> derived_owner;  // dungeon id -> owner index
  auto derived_owner_index = [&](int dungeon_id) {
    auto it = derived_owner.find(dungeon_id);
    if (it != derived_owner.end()) {
      return it->second;
    }
    RoomCensusOwner owner;
    owner.dungeon_id = dungeon_id;
    if (dungeon_id == kRoomCensusInteriorDungeonId) {
      owner.id = "INT";
      owner.name = "Interiors";
      owner.interior = true;
    } else {
      owner.id = absl::StrFormat("ID%02X", dungeon_id);
      const std::string vanilla = VanillaDungeonName(dungeon_id);
      if (census.owners_from_project) {
        owner.name =
            absl::StrFormat("Dungeon ID 0x%02X (not in project)", dungeon_id);
      } else if (!vanilla_names || vanilla.empty() || (dungeon_id & 1) != 0) {
        owner.name = absl::StrFormat("Dungeon ID 0x%02X", dungeon_id);
      } else {
        owner.name = vanilla;
      }
    }
    const int index = static_cast<int>(census.owners.size());
    census.owners.push_back(owner);
    derived_owner[dungeon_id] = index;
    return index;
  };

  // Dungeon entrances first (by dungeon ID), interiors last.
  std::vector<const RoomCensusEntranceFact*> ordered;
  for (const auto& entrance : input.entrances) {
    if (entrance.kind != RoomReferenceKind::kUnplacedEntrance &&
        entrance.room_id >= 0 && entrance.room_id < kRooms) {
      ordered.push_back(&entrance);
    }
  }
  std::stable_sort(
      ordered.begin(), ordered.end(), [](const auto* a, const auto* b) {
        const bool a_int = a->dungeon_id == kRoomCensusInteriorDungeonId;
        const bool b_int = b->dungeon_id == kRoomCensusInteriorDungeonId;
        if (a_int != b_int) {
          return !a_int;
        }
        if (a->dungeon_id != b->dungeon_id) {
          return a->dungeon_id < b->dungeon_id;
        }
        return a->entrance_id < b->entrance_id;
      });
  auto flood = [&](bool owner_edges_only) {
    for (const auto* entrance : ordered) {
      const int start = entrance->room_id;
      if (project_listed[start]) {
        continue;
      }
      int owner = owner_of[start];
      if (owner < 0) {
        owner = derived_owner_index(entrance->dungeon_id);
        owner_of[start] = owner;
      }
      std::deque<int> q{start};
      std::vector<bool> seen(kRooms, false);
      seen[start] = true;
      while (!q.empty()) {
        const int room = q.front();
        q.pop_front();
        for (const auto& edge : out_edges[room]) {
          if (!edge.strong || (owner_edges_only && !edge.owner_ok) ||
              seen[edge.to] || project_listed[edge.to]) {
            continue;
          }
          seen[edge.to] = true;
          if (owner_of[edge.to] < 0) {
            owner_of[edge.to] = owner;
          } else if (owner_of[edge.to] != owner) {
            continue;  // Another area owns it; do not walk through.
          }
          q.push_back(edge.to);
        }
      }
    }
  };
  flood(/*owner_edges_only=*/true);
  // Rooms on a dungeon's pause map belong to that dungeon.
  for (int room = 0; room < kRooms; ++room) {
    if (owner_of[room] < 0 && reached[room] && map_dungeon[room] >= 0) {
      owner_of[room] = derived_owner_index(map_dungeon[room]);
    }
  }
  flood(/*owner_edges_only=*/false);

  // ---- Classify ----------------------------------------------------------
  for (int room = 0; room < kRooms; ++room) {
    const auto& f = facts[room];
    RoomCensusEntry& entry = census.rooms[room];
    entry.room_id = room;
    entry.reached = reached[room];
    entry.owner_index = owner_of[room];
    entry.interior =
        entry.owner_index >= 0 && census.owners[entry.owner_index].interior;
    entry.object_count = f.object_count;
    entry.sprite_count = f.sprite_count;
    entry.vanilla_similarity = f.vanilla_similarity;
    entry.empty =
        f.object_count == 0 || (f.object_count <= 1 && f.sprite_count <= 1);
    const std::string content =
        absl::StrFormat("%d object%s, %d sprite%s", f.object_count,
                        f.object_count == 1 ? "" : "s", f.sprite_count,
                        f.sprite_count == 1 ? "" : "s");
    const int vanilla_percent =
        static_cast<int>(f.vanilla_similarity * 100.0f + 0.5f);

    // Several overworld slots can place the same entrance; list it once.
    std::vector<RoomReference> unique_refs;
    for (auto& ref : entry.references) {
      const bool duplicate = std::any_of(
          unique_refs.begin(), unique_refs.end(), [&](const auto& seen) {
            return seen.kind == ref.kind && seen.detail == ref.detail;
          });
      if (!duplicate) {
        unique_refs.push_back(std::move(ref));
      }
    }
    entry.references = std::move(unique_refs);
    std::vector<std::string> strong_refs;
    std::vector<std::string> weak_refs;
    for (const auto& ref : entry.references) {
      (ref.strong ? strong_refs : weak_refs).push_back(ref.detail);
    }
    auto join_capped = [](const std::vector<std::string>& items) {
      constexpr size_t kMaxListed = 4;
      std::vector<std::string> shown(
          items.begin(), items.begin() + std::min(items.size(), kMaxListed));
      std::string text = absl::StrJoin(shown, "; ");
      if (items.size() > kMaxListed) {
        text += absl::StrFormat("; +%d more",
                                static_cast<int>(items.size() - kMaxListed));
      }
      return text;
    };

    if (entry.reached) {
      entry.status = RoomCensusStatus::kInUse;
      entry.reasons.push_back(strong_refs.empty()
                                  ? "reached from an entrance"
                                  : "reached via " + join_capped(strong_refs));
    } else if (project_listed[room]) {
      entry.status = RoomCensusStatus::kInUse;
      entry.orphan = true;
      entry.reasons.push_back(absl::StrFormat(
          "listed in project dungeon %s, but no entrance, stair, hole, warp "
          "or door path reaches it",
          census.owners[owner_of[room]].id));
    } else if (entry.empty) {
      entry.status = RoomCensusStatus::kFree;
      entry.reasons.push_back(
          "no entrance, stair, hole, warp or door path reaches it");
      entry.reasons.push_back("empty: " + content);
    } else if (f.vanilla_similarity >= kReclaimableVanillaSimilarity) {
      entry.status = RoomCensusStatus::kReclaimable;
      entry.reasons.push_back(
          "no entrance, stair, hole, warp or door path reaches it");
      entry.reasons.push_back(absl::StrFormat(
          "vanilla leftover: %d%% of its objects match vanilla (%s)",
          vanilla_percent, content));
    } else if (f.vanilla_similarity >= kPartlyEditedVanillaSimilarity) {
      entry.status = RoomCensusStatus::kReclaimable;
      entry.reasons.push_back(
          "no entrance, stair, hole, warp or door path reaches it");
      entry.reasons.push_back(absl::StrFormat(
          "partly edited vanilla leftover: %d%% of its objects match vanilla "
          "(%s); check the edits before reclaiming",
          vanilla_percent, content));
    } else {
      entry.status = RoomCensusStatus::kInUse;
      entry.orphan = true;
      entry.reasons.push_back(
          "orphan: authored content but no entrance, stair, hole, warp or "
          "door path reaches it; owner decision needed");
      entry.reasons.push_back(
          f.vanilla_similarity < 0.0f
              ? content
              : absl::StrFormat("%s, %d%% vanilla", content, vanilla_percent));
    }
    if (!entry.reached && !weak_refs.empty()) {
      entry.reasons.push_back("caveat: " + join_capped(weak_refs));
    }
    if (entry.status != RoomCensusStatus::kInUse && f.in_pit_damage_table) {
      entry.reasons.push_back("caveat: listed in RoomsWithPitDamage");
    }

    switch (entry.status) {
      case RoomCensusStatus::kFree:
        ++census.free_count;
        break;
      case RoomCensusStatus::kReclaimable:
        ++census.reclaimable_count;
        break;
      case RoomCensusStatus::kInUse:
        ++census.in_use_count;
        break;
    }
    census.orphan_count += entry.orphan ? 1 : 0;
    census.interior_count += entry.interior ? 1 : 0;
    if (entry.owner_index >= 0) {
      ++census.owners[entry.owner_index].room_count;
    }
  }

  // ---- Free blocks -------------------------------------------------------
  // Contiguous runs ("cores") are 4-connected free rooms on one $A1 page. A
  // core of at least kFreeBlockMinCore rooms also takes in smaller free cores
  // within kFreeBlockMergeDistance cells (not transitively), so a block reads
  // like "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0" while scattered single rooms
  // stay separate.
  std::vector<int> free_rooms;
  for (const auto& entry : census.rooms) {
    if (entry.status == RoomCensusStatus::kFree) {
      free_rooms.push_back(entry.room_id);
    }
  }
  auto grid_distance = [](int a, int b) {
    if ((a >> 8) != (b >> 8)) {
      return 1 << 20;
    }
    const int dr = std::abs(((a & 0xFF) >> 4) - ((b & 0xFF) >> 4));
    const int dc = std::abs((a & 0x0F) - (b & 0x0F));
    return std::max(dr, dc);
  };
  auto adjacent = [](int a, int b) {
    if ((a >> 8) != (b >> 8)) {
      return false;
    }
    const int dr = std::abs(((a & 0xFF) >> 4) - ((b & 0xFF) >> 4));
    const int dc = std::abs((a & 0x0F) - (b & 0x0F));
    return dr + dc == 1;
  };
  std::vector<std::vector<int>> cores;
  std::vector<bool> assigned(free_rooms.size(), false);
  for (size_t i = 0; i < free_rooms.size(); ++i) {
    if (assigned[i]) {
      continue;
    }
    std::vector<int> core;
    std::deque<size_t> pending{i};
    assigned[i] = true;
    while (!pending.empty()) {
      const size_t at = pending.front();
      pending.pop_front();
      core.push_back(free_rooms[at]);
      for (size_t j = 0; j < free_rooms.size(); ++j) {
        if (!assigned[j] && adjacent(free_rooms[at], free_rooms[j])) {
          assigned[j] = true;
          pending.push_back(j);
        }
      }
    }
    std::sort(core.begin(), core.end());
    cores.push_back(std::move(core));
  }
  std::stable_sort(cores.begin(), cores.end(),
                   [](const auto& a, const auto& b) {
                     if (a.size() != b.size()) {
                       return a.size() > b.size();
                     }
                     return a.front() < b.front();
                   });
  std::vector<bool> absorbed(cores.size(), false);
  for (size_t i = 0; i < cores.size(); ++i) {
    if (absorbed[i]) {
      continue;
    }
    absorbed[i] = true;
    RoomCensusCluster cluster;
    cluster.core_rooms = cores[i];
    cluster.rooms = cores[i];
    if (static_cast<int>(cores[i].size()) >= kFreeBlockMinCore) {
      for (size_t j = i + 1; j < cores.size(); ++j) {
        if (absorbed[j] || cores[j].size() >= cores[i].size()) {
          continue;
        }
        bool near = false;
        for (int a : cores[i]) {
          for (int b : cores[j]) {
            near |= grid_distance(a, b) <= kFreeBlockMergeDistance;
          }
        }
        if (near) {
          absorbed[j] = true;
          cluster.rooms.insert(cluster.rooms.end(), cores[j].begin(),
                               cores[j].end());
        }
      }
    }
    std::sort(cluster.rooms.begin(), cluster.rooms.end());
    cluster.summary = FormatRoomBlockSummary(cluster.rooms);
    census.free_clusters.push_back(std::move(cluster));
  }
  std::stable_sort(census.free_clusters.begin(), census.free_clusters.end(),
                   [](const auto& a, const auto& b) {
                     if (a.rooms.size() != b.rooms.size()) {
                       return a.rooms.size() > b.rooms.size();
                     }
                     return a.rooms.front() < b.rooms.front();
                   });
  return census;
}

absl::StatusOr<RoomCensus> ComputeRoomCensus(Rom* rom,
                                             const RoomCensusOptions& options) {
  auto input = CollectRoomCensusInput(rom, options);
  if (!input.ok()) {
    return input.status();
  }
  return BuildRoomCensus(*input);
}

}  // namespace yaze::zelda3
