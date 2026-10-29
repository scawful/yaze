#include "app/editor/dungeon/dungeon_jump_targets.h"

#include <map>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "core/project.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/entrance_layout.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {
namespace {

// Vanilla $040C dungeon IDs advance by two.
constexpr const char* kVanillaDungeonNames[] = {
    "Sewers",          "Hyrule Castle", "Eastern Palace",     "Desert Palace",
    "Agahnim's Tower", "Swamp Palace",  "Palace of Darkness", "Misery Mire",
    "Skull Woods",     "Ice Palace",    "Tower of Hera",      "Thieves' Town",
    "Turtle Rock",     "Ganon's Tower"};
constexpr int kVanillaDungeonCount =
    static_cast<int>(sizeof(kVanillaDungeonNames) / sizeof(char*));
constexpr int kNoDungeon = 0xFF;

std::string DungeonByteName(int dungeon) {
  if (dungeon % 2 == 0 && dungeon / 2 < kVanillaDungeonCount) {
    return kVanillaDungeonNames[dungeon / 2];
  }
  return absl::StrFormat("Dungeon %02X", dungeon);
}

}  // namespace

std::vector<DungeonJumpTarget> BuildDungeonJumpTargets(
    Rom* rom, const project::YazeProject* project) {
  std::vector<DungeonJumpTarget> targets;
  if (project && project->project_opened() &&
      project->hack_manifest.HasProjectRegistry()) {
    for (const auto& dungeon :
         project->hack_manifest.project_registry().dungeons) {
      int room_id = -1;
      for (const auto& room : dungeon.rooms) {
        if (room.id < 0 || room.id >= zelda3::kNumberOfRooms) {
          continue;
        }
        if (room_id < 0) {
          room_id = room.id;
        }
        if (room.type == "entrance") {
          room_id = room.id;
          break;
        }
      }
      if (room_id >= 0) {
        targets.push_back(
            {dungeon_project_labels::FormatDungeonName(dungeon), room_id});
      }
    }
    if (!targets.empty()) {
      return targets;
    }
  }

  if (!rom || !rom->is_loaded() ||
      !zelda3::ResolveEntranceLayout(rom->vector()).ok()) {
    return targets;
  }
  std::map<int, int> first_room_by_dungeon;
  for (int i = 0; i < zelda3::kNumRegularDungeonEntrances; ++i) {
    const zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(i), false);
    if (!entrance.load_status().ok() || entrance.room_ < 0 ||
        entrance.room_ >= zelda3::kNumberOfRooms ||
        entrance.dungeon_id_ == kNoDungeon) {
      continue;
    }
    first_room_by_dungeon.try_emplace(entrance.dungeon_id_, entrance.room_);
  }
  for (const auto& [dungeon, room_id] : first_room_by_dungeon) {
    targets.push_back({DungeonByteName(dungeon), room_id});
  }
  return targets;
}

}  // namespace yaze::editor
