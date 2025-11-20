#include "zelda3/overworld/overworld_exit.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<OverworldExit>> LoadExits(Rom* rom) {
  const int NumberOfOverworldExits = 0x4F;
  std::vector<OverworldExit> exits;
  for (int i = 0; i < NumberOfOverworldExits; i++) {
    const auto* rom_data = rom->data();

    uint16_t exit_room_id;
    uint16_t exit_map_id;
    uint16_t exit_vram;
    uint16_t exit_y_scroll;
    uint16_t exit_x_scroll;
    uint16_t exit_y_player;
    uint16_t exit_x_player;
    uint16_t exit_y_camera;
    uint16_t exit_x_camera;
    uint16_t exit_scroll_mod_y;
    uint16_t exit_scroll_mod_x;
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

    uint16_t player_y =
        static_cast<uint16_t>((rom_data[OWExitYPlayer + (i * 2) + 1] << 8) +
                              rom_data[OWExitYPlayer + (i * 2)]);
    uint16_t player_x =
        static_cast<uint16_t>((rom_data[OWExitXPlayer + (i * 2) + 1] << 8) +
                              rom_data[OWExitXPlayer + (i * 2)]);

    exits.emplace_back(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, player_y, player_x, exit_y_camera,
                       exit_x_camera, exit_scroll_mod_y, exit_scroll_mod_x,
                       exit_door_type_1, exit_door_type_2,
                       (player_x & player_y) == 0xFFFF);
  }
  return exits;
}

void OverworldExit::UpdateMapProperties(uint16_t map_id, const void* context) {
  // Sync player position from drag system
  // ZScream: ExitMode.cs:229-244 updates PlayerX/PlayerY, then calls UpdateMapStuff
  x_player_ = static_cast<uint16_t>(x_);
  y_player_ = static_cast<uint16_t>(y_);
  map_id_ = map_id;

  // FIX Bug 3: Query actual area size from overworld
  // ZScream: ExitOW.cs:210-212
  int area_size_x = 256;
  int area_size_y = 256;

  if (context != nullptr) {
    const auto* overworld = static_cast<const Overworld*>(context);
    auto area_size = overworld->overworld_map(map_id)->area_size();

    // Calculate area dimensions based on size enum
    if (area_size == AreaSizeEnum::LargeArea) {
      area_size_x = area_size_y = 768;
    } else if (area_size == AreaSizeEnum::WideArea) {
      area_size_x = 768;
      area_size_y = 256;
    } else if (area_size == AreaSizeEnum::TallArea) {
      area_size_x = 256;
      area_size_y = 768;
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

    // Base scroll calculation (player centered in screen)
    x_scroll_ = x_player_ - 120;
    y_scroll_ = y_player_ - 80;

    // Clamp scroll to map bounds using actual area size
    if (x_scroll_ < mapx) {
      x_scroll_ = mapx;
    }

    if (y_scroll_ < mapy) {
      y_scroll_ = mapy;
    }

    if (x_scroll_ > mapx + area_size_x) {
      x_scroll_ = mapx + area_size_x;
    }

    if (y_scroll_ > mapy + area_size_y + 32) {
      y_scroll_ = mapy + area_size_y + 32;
    }

    // Camera position (offset from player)
    x_camera_ = x_player_ + 0x07;
    y_camera_ = y_player_ + 0x1F;

    // Clamp camera to valid range
    if (x_camera_ < mapx + 127) {
      x_camera_ = mapx + 127;
    }

    if (y_camera_ < mapy + 111) {
      y_camera_ = mapy + 111;
    }

    if (x_camera_ > mapx + 127 + area_size_x) {
      x_camera_ = mapx + 127 + area_size_x;
    }

    if (y_camera_ > mapy + 143 + area_size_y) {
      y_camera_ = mapy + 143 + area_size_y;
    }
  }

  // Calculate VRAM location from scroll values
  // ZScream: ExitOW.cs:312-315
  int16_t vram_x_scroll = static_cast<int16_t>(x_scroll_ - mapx);
  int16_t vram_y_scroll = static_cast<int16_t>(y_scroll_ - mapy);

  map_pos_ = static_cast<uint16_t>(((vram_y_scroll & 0xFFF0) << 3) |
                                   ((vram_x_scroll & 0xFFF0) >> 3));
}

absl::Status SaveExits(Rom* rom, const std::vector<OverworldExit>& exits) {

  // ASM version 0x03 added SW support and the exit leading to Zora's Domain specifically
  // needs to be updated because its camera values are incorrect.
  // We only update it if it was a vanilla ROM though because we don't know if the
  // user has already adjusted it or not.
  uint8_t asm_version = (*rom)[OverworldCustomASMHasBeenApplied];
  if (asm_version == 0x00) {
    // Apply special fix for Zora's Domain exit (index 0x4D)
    // TODO(scawful): Implement SpecialUpdatePosition for OverworldExit
    // Similar to ZScream Save.cs:1034-1039
    // if (all_exits_.size() > 0x4D) {
    //   all_exits_[0x4D].SpecialUpdatePosition();
    // }
  }

  for (int i = 0; i < kNumOverworldExits; i++) {
    RETURN_IF_ERROR(rom->WriteShort(OWExitRoomId + (i * 2), exits[i].room_id_));
    RETURN_IF_ERROR(rom->WriteByte(OWExitMapId + i, exits[i].map_id_));
    RETURN_IF_ERROR(rom->WriteShort(OWExitVram + (i * 2), exits[i].map_pos_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitYScroll + (i * 2), exits[i].y_scroll_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitXScroll + (i * 2), exits[i].x_scroll_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitYPlayer + (i * 2), exits[i].y_player_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitXPlayer + (i * 2), exits[i].x_player_));
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