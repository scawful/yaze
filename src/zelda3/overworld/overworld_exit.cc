#include "zelda3/overworld/overworld_exit.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<OverworldExit>> LoadExits(Rom* rom) {
  if (rom == nullptr) {
    return absl::InvalidArgumentError("Cannot load exits without a ROM");
  }
  const int NumberOfOverworldExits = 0x4F;
  std::vector<OverworldExit> exits;
  for (int i = 0; i < NumberOfOverworldExits; i++) {
    uint16_t exit_room_id;
    uint8_t exit_map_id;
    uint16_t exit_vram;
    uint16_t exit_y_scroll;
    uint16_t exit_x_scroll;
    uint16_t exit_y_player;
    uint16_t exit_x_player;
    uint16_t exit_y_camera;
    uint16_t exit_x_camera;
    uint8_t exit_scroll_mod_y;
    uint8_t exit_scroll_mod_x;
    uint16_t exit_door_type_1;
    uint16_t exit_door_type_2;
    RETURN_IF_ERROR(rom->ReadTransaction(
        exit_room_id, (OWExitRoomId + (i * 2)), exit_map_id, OWExitMapId + i,
        exit_vram, OWExitVram + (i * 2), exit_y_scroll, OWExitYScroll + (i * 2),
        exit_x_scroll, OWExitXScroll + (i * 2), exit_y_player,
        OWExitYPlayer + (i * 2), exit_x_player, OWExitXPlayer + (i * 2),
        exit_y_camera, OWExitYCamera + (i * 2), exit_x_camera,
        OWExitXCamera + (i * 2), exit_scroll_mod_y, OWExitUnk1 + i,
        exit_scroll_mod_x, OWExitUnk2 + i, exit_door_type_1,
        OWExitDoorType1 + (i * 2), exit_door_type_2,
        OWExitDoorType2 + (i * 2)));

    exits.emplace_back(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, exit_y_player, exit_x_player,
                       exit_y_camera, exit_x_camera, exit_scroll_mod_y,
                       exit_scroll_mod_x, exit_door_type_1, exit_door_type_2,
                       exit_x_player == 0xFFFF && exit_y_player == 0xFFFF);
  }
  return exits;
}

void OverworldExit::UpdateMapProperties(uint16_t map_id, const void* context) {
  // Sync player position from drag system
  // ZScream: ExitMode.cs:229-244 updates PlayerX/PlayerY, then calls
  // UpdateMapStuff
  x_player_ = static_cast<uint16_t>(x_);
  y_player_ = static_cast<uint16_t>(y_);
  map_id_ = map_id;

  // FIX Bug 3: Query actual area size from overworld
  // ZScream: ExitOW.cs:210-212
  int area_size_x = 256;
  int area_size_y = 256;

  // Missing context maps use the same small-area geometry as nullptr.
  // Checked editor operations validate destination ownership separately.
  if (context != nullptr) {
    const auto* overworld = static_cast<const Overworld*>(context);
    if (const auto* map = overworld->overworld_map(map_id)) {
      const auto area_size = map->area_size();
      if (area_size == AreaSizeEnum::LargeArea) {
        area_size_x = area_size_y = 768;
      } else if (area_size == AreaSizeEnum::WideArea) {
        area_size_x = 768;
      } else if (area_size == AreaSizeEnum::TallArea) {
        area_size_y = 768;
      }
    }
  }

  // FIX Bug 5: Normalize map_id FIRST before using for calculations
  // ZScream: ExitOW.cs:214
  uint8_t normalized_map_id = map_id % 0x40;

  // Calculate map grid position
  // ZScream: ExitOW.cs:216-217
  int mapX = normalized_map_id % 8;
  int mapY = normalized_map_id / 8;

  // Calculate game coordinates (map-local tile position)
  // ZScream: ExitOW.cs:219-220
  game_x_ = static_cast<int>((std::abs(x_ - (mapX * 512)) / 16));
  game_y_ = static_cast<int>((std::abs(y_ - (mapY * 512)) / 16));

  // Clamp to valid range based on area size
  // ZScream: ExitOW.cs:222-234
  int max_game_x = (area_size_x == 256) ? 31 : 63;
  int max_game_y = (area_size_y == 256) ? 31 : 63;
  game_x_ = std::clamp(game_x_, 0, max_game_x);
  game_y_ = std::clamp(game_y_, 0, max_game_y);

  // Map base coordinates in world space
  // ZScream: ExitOW.cs:237-238 (mapx, mapy)
  int mapx = (normalized_map_id & 7) << 9;   // * 512
  int mapy = (normalized_map_id & 56) << 6;  // (map_id / 8) * 512

  if (is_automatic_) {
    // Auto-calculate scroll and camera from player position
    // ZScream: ExitOW.cs:256-309

    // Clamp signed values before storing them. Narrowing a negative scroll
    // first wraps it to $FFxx and incorrectly selects the far map edge.
    x_scroll_ = static_cast<uint16_t>(std::clamp(
        static_cast<int>(x_player_) - 120, mapx, mapx + area_size_x));
    y_scroll_ = static_cast<uint16_t>(std::clamp(
        static_cast<int>(y_player_) - 80, mapy, mapy + area_size_y + 32));
    x_camera_ = static_cast<uint16_t>(std::clamp(
        static_cast<int>(x_player_) + 7, mapx + 127, mapx + 127 + area_size_x));
    y_camera_ =
        static_cast<uint16_t>(std::clamp(static_cast<int>(y_player_) + 31,
                                         mapy + 111, mapy + 143 + area_size_y));
  }

  // Calculate VRAM location from scroll values
  // ZScream: ExitOW.cs:312-315
  int16_t vram_x_scroll = static_cast<int16_t>(x_scroll_ - mapx);
  int16_t vram_y_scroll = static_cast<int16_t>(y_scroll_ - mapy);

  map_pos_ = static_cast<uint16_t>(((vram_y_scroll & 0xFFF0) << 3) |
                                   ((vram_x_scroll & 0xFFF0) >> 3));
}

absl::Status SaveExits(Rom* rom, const std::vector<OverworldExit>& exits) {
  if (rom == nullptr) {
    return absl::InvalidArgumentError("Cannot save exits without a ROM");
  }
  if (exits.size() < kNumOverworldExits) {
    return absl::InvalidArgumentError("Saving exits requires all 79 records");
  }
  // Validate every destination before the first mutation, including the
  // auxiliary map bytes used only when a special room destination changes.
  struct TableRange {
    size_t address;
    size_t bytes;
  };
  constexpr TableRange kTables[] = {
      {OWExitRoomId, kNumOverworldExits * 2},
      {OWExitMapId, kNumOverworldExits},
      {OWExitVram, kNumOverworldExits * 2},
      {OWExitYScroll, kNumOverworldExits * 2},
      {OWExitXScroll, kNumOverworldExits * 2},
      {OWExitYPlayer, kNumOverworldExits * 2},
      {OWExitXPlayer, kNumOverworldExits * 2},
      {OWExitYCamera, kNumOverworldExits * 2},
      {OWExitXCamera, kNumOverworldExits * 2},
      {OWExitUnk1, kNumOverworldExits},
      {OWExitUnk2, kNumOverworldExits},
      {OWExitDoorType1, kNumOverworldExits * 2},
      {OWExitDoorType2, kNumOverworldExits * 2},
      {OWExitDoorPosition, 5},
  };
  const size_t rom_size = rom->vector().size();
  for (const auto& table : kTables) {
    if (table.address > rom_size || table.bytes > rom_size - table.address) {
      return absl::OutOfRangeError("ROM does not contain complete exit tables");
    }
  }

  for (int i = 0; i < kNumOverworldExits; i++) {
    ASSIGN_OR_RETURN(auto stored_room, rom->ReadWord(OWExitRoomId + (i * 2)));
    ASSIGN_OR_RETURN(auto stored_map, rom->ReadByte(OWExitMapId + i));
    const bool destination_changed =
        stored_room != exits[i].room_id_ || stored_map != exits[i].map_id_;
    RETURN_IF_ERROR(rom->WriteShort(OWExitRoomId + (i * 2), exits[i].room_id_));
    RETURN_IF_ERROR(rom->WriteByte(OWExitMapId + i, exits[i].map_id_));
    RETURN_IF_ERROR(rom->WriteShort(OWExitVram + (i * 2), exits[i].map_pos_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitYScroll + (i * 2), exits[i].y_scroll_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitXScroll + (i * 2), exits[i].x_scroll_));
    // $FFFF/$FFFF is the existing on-disk unused-record sentinel. Preserve
    // the remaining record fields so deletion can be undone without invention.
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitYPlayer + (i * 2),
                        exits[i].deleted_ ? 0xFFFF : exits[i].y_player_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitXPlayer + (i * 2),
                        exits[i].deleted_ ? 0xFFFF : exits[i].x_player_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitYCamera + (i * 2), exits[i].y_camera_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitXCamera + (i * 2), exits[i].x_camera_));
    RETURN_IF_ERROR(rom->WriteByte(OWExitUnk1 + i, exits[i].scroll_mod_y_));
    RETURN_IF_ERROR(rom->WriteByte(OWExitUnk2 + i, exits[i].scroll_mod_x_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitDoorType1 + (i * 2), exits[i].door_type_1_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitDoorType2 + (i * 2), exits[i].door_type_2_));

    // These auxiliary bytes may intentionally differ from the exit table in a
    // hack. Only an authored destination change may update them.
    if (!destination_changed) {
      continue;
    }
    if (exits[i].room_id_ == 0x0180) {
      RETURN_IF_ERROR(
          rom->WriteByte(OWExitDoorPosition + 0, exits[i].map_id_ & 0xFF));
    } else if (exits[i].room_id_ == 0x0181) {
      RETURN_IF_ERROR(
          rom->WriteByte(OWExitDoorPosition + 2, exits[i].map_id_ & 0xFF));
    } else if (exits[i].room_id_ == 0x0182) {
      RETURN_IF_ERROR(
          rom->WriteByte(OWExitDoorPosition + 4, exits[i].map_id_ & 0xFF));
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
