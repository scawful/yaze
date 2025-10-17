#include "zelda3/overworld/overworld_exit.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "util/macro.h"
#include <cstdint>
#include <vector>
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<OverworldExit>> LoadExits(Rom* rom) {
  const int NumberOfOverworldExits = 0x4F;
  std::vector<OverworldExit> exits;
  for (int i = 0; i < NumberOfOverworldExits; i++) {
    auto rom_data = rom->data();

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

    uint16_t py = (uint16_t)((rom_data[OWExitYPlayer + (i * 2) + 1] << 8) +
                             rom_data[OWExitYPlayer + (i * 2)]);
    uint16_t px = (uint16_t)((rom_data[OWExitXPlayer + (i * 2) + 1] << 8) +
                             rom_data[OWExitXPlayer + (i * 2)]);

    exits.emplace_back(exit_room_id, exit_map_id, exit_vram, exit_y_scroll,
                       exit_x_scroll, py, px, exit_y_camera, exit_x_camera,
                       exit_scroll_mod_y, exit_scroll_mod_x, exit_door_type_1,
                       exit_door_type_2, (px & py) == 0xFFFF);
  }
  return exits;
}


absl::Status SaveExits(Rom* rom, const std::vector<OverworldExit>& exits) {

  // ASM version 0x03 added SW support and the exit leading to Zora's Domain specifically
  // needs to be updated because its camera values are incorrect.
  // We only update it if it was a vanilla ROM though because we don't know if the
  // user has already adjusted it or not.
  uint8_t asm_version = (*rom)[OverworldCustomASMHasBeenApplied];
  if (asm_version == 0x00) {
    // Apply special fix for Zora's Domain exit (index 0x4D)
    // TODO: Implement SpecialUpdatePosition for OverworldExit
    // if (all_exits_.size() > 0x4D) {
    //   all_exits_[0x4D].SpecialUpdatePosition();
    // }
  }

  for (int i = 0; i < kNumOverworldExits; i++) {
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitRoomId + (i * 2), exits[i].room_id_));
    RETURN_IF_ERROR(rom->WriteByte(OWExitMapId + i, exits[i].map_id_));
    RETURN_IF_ERROR(
        rom->WriteShort(OWExitVram + (i * 2), exits[i].map_pos_));
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
    RETURN_IF_ERROR(
        rom->WriteByte(OWExitUnk1 + i, exits[i].scroll_mod_y_));
    RETURN_IF_ERROR(
        rom->WriteByte(OWExitUnk2 + i, exits[i].scroll_mod_x_));
    RETURN_IF_ERROR(rom->WriteShort(OWExitDoorType1 + (i * 2),
                                      exits[i].door_type_1_));
    RETURN_IF_ERROR(rom->WriteShort(OWExitDoorType2 + (i * 2),
                                      exits[i].door_type_2_));

    if (exits[i].room_id_ == 0x0180) {
      RETURN_IF_ERROR(rom->WriteByte(OWExitDoorPosition + 0,
                                       exits[i].map_id_ & 0xFF));
    } else if (exits[i].room_id_ == 0x0181) {
      RETURN_IF_ERROR(rom->WriteByte(OWExitDoorPosition + 2,
                                       exits[i].map_id_ & 0xFF));
    } else if (exits[i].room_id_ == 0x0182) {
      RETURN_IF_ERROR(rom->WriteByte(OWExitDoorPosition + 4,
                                       exits[i].map_id_ & 0xFF));
    }
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3