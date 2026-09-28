#include "app/editor/dungeon/dungeon_entrance_camera.h"

#include <algorithm>
#include <cstddef>

#include "absl/strings/str_format.h"
#include "zelda3/dungeon/dungeon_spawn_point.h"
#include "zelda3/dungeon/room_census.h"
#include "zelda3/dungeon/room_entrance.h"

namespace yaze::editor {

namespace {

constexpr int kSpecialQuadrantRoom = 0x104;

int AlignDown8(int value) {
  return value & ~0x07;
}

void RestoreDirtyState(bool dirty, zelda3::RoomEntrance* entrance) {
  if (dirty) {
    entrance->MarkDirty();
  } else {
    entrance->ClearDirty();
  }
}

void RestoreDirtyState(bool dirty, zelda3::DungeonSpawnPoint* spawn) {
  if (dirty) {
    spawn->MarkDirty();
  } else {
    spawn->ClearDirty();
  }
}

}  // namespace

DungeonEntranceCameraDerived DeriveDungeonEntranceCamera(int room_id,
                                                         uint16_t player_x,
                                                         uint16_t player_y) {
  const int room_base_x = (room_id & 0x0F) * 0x200;
  const int room_base_y = (room_id >> 4) * 0x200;

  const int camera_x = std::clamp(AlignDown8(static_cast<int>(player_x) - 0x78),
                                  room_base_x, room_base_x + 0x100);
  const int camera_y = std::clamp(AlignDown8(static_cast<int>(player_y) - 0x68),
                                  room_base_y + 0x10, room_base_y + 0x110);

  const int page_x = camera_x >> 8;
  const bool right_half = camera_x - room_base_x >= 0x100;
  const int page_y = camera_y >> 8;
  const bool lower_half = camera_y - room_base_y >= 0x100;

  DungeonEntranceCameraDerived derived;
  derived.camera_x = static_cast<uint16_t>(camera_x);
  derived.camera_y = static_cast<uint16_t>(camera_y);
  derived.trigger_x = static_cast<uint16_t>((camera_x - room_base_x) + 0x7F);
  derived.trigger_y = static_cast<uint16_t>((camera_y - room_base_y) + 0x77);
  derived.boundaries = {
      static_cast<uint8_t>(page_y),
      static_cast<uint8_t>(lower_half ? page_y - 1 : page_y),
      static_cast<uint8_t>(page_y),
      static_cast<uint8_t>(lower_half ? page_y : page_y + 1),
      static_cast<uint8_t>(page_x),
      static_cast<uint8_t>(right_half ? page_x - 1 : page_x),
      static_cast<uint8_t>(page_x),
      static_cast<uint8_t>(right_half ? page_x : page_x + 1),
  };
  const int local_x = static_cast<int>(player_x) - room_base_x;
  const int local_y = static_cast<int>(player_y) - room_base_y;
  derived.quadrant = static_cast<uint8_t>((local_x >= 0x100 ? 0x10 : 0) |
                                          (local_y >= 0x100 ? 0x02 : 0));
  return derived;
}

DungeonEntranceCameraValidation ValidateDungeonEntranceCamera(
    const DungeonEntranceCameraState& state) {
  DungeonEntranceCameraValidation validation;
  validation.quadrant_repair_safe = state.room_id != kSpecialQuadrantRoom;
  if (state.room_id < 0 || state.room_id >= zelda3::kRoomCensusRoomCount) {
    validation.repair_source_valid = false;
    validation.errors.push_back(absl::StrFormat(
        "Room 0x%X is outside the dungeon room table.", state.room_id));
    return validation;
  }

  const int room_base_x = (state.room_id & 0x0F) * 0x200;
  const int room_base_y = (state.room_id >> 4) * 0x200;
  const int player_x = state.player_x;
  const int player_y = state.player_y;
  if (player_x < room_base_x || player_x > room_base_x + 0x1FF) {
    validation.repair_source_valid = false;
    validation.errors.push_back(absl::StrFormat(
        "Player X 0x%04X is outside room X range 0x%04X-0x%04X.", player_x,
        room_base_x, room_base_x + 0x1FF));
  }
  if (player_y < room_base_y || player_y > room_base_y + 0x1FF) {
    validation.repair_source_valid = false;
    validation.errors.push_back(absl::StrFormat(
        "Player Y 0x%04X is outside room Y range 0x%04X-0x%04X.", player_y,
        room_base_y, room_base_y + 0x1FF));
  }

  if (state.camera_x < room_base_x || state.camera_x > room_base_x + 0x100) {
    validation.errors.push_back(
        absl::StrFormat("Camera X 0x%04X is outside room range 0x%04X-0x%04X.",
                        state.camera_x, room_base_x, room_base_x + 0x100));
  }
  if (state.camera_y < room_base_y + 0x10 ||
      state.camera_y > room_base_y + 0x110) {
    validation.errors.push_back(absl::StrFormat(
        "Camera Y 0x%04X is outside room range 0x%04X-0x%04X.", state.camera_y,
        room_base_y + 0x10, room_base_y + 0x110));
  }
  if ((state.camera_x & 0x07) != 0 || (state.camera_y & 0x07) != 0) {
    validation.errors.push_back(
        "Camera coordinates must be aligned to an 8-pixel boundary.");
  }
  if (state.trigger_x < 0x007F || state.trigger_x > 0x017F) {
    validation.errors.push_back(absl::StrFormat(
        "Camera trigger X 0x%04X is outside safe range 0x007F-0x017F.",
        state.trigger_x));
  }
  if (state.trigger_y < 0x0087 || state.trigger_y > 0x0187) {
    validation.errors.push_back(absl::StrFormat(
        "Camera trigger Y 0x%04X is outside safe range 0x0087-0x0187.",
        state.trigger_y));
  }
  for (size_t index = 0; index < 4; ++index) {
    if (state.boundaries[index] > 0x25) {
      validation.errors.push_back(
          absl::StrFormat("Vertical boundary %zu (0x%02X) exceeds page 0x25.",
                          index, state.boundaries[index]));
    }
  }
  for (size_t index = 4; index < state.boundaries.size(); ++index) {
    if (state.boundaries[index] > 0x1F) {
      validation.errors.push_back(
          absl::StrFormat("Horizontal boundary %zu (0x%02X) exceeds page 0x1F.",
                          index, state.boundaries[index]));
    }
  }

  validation.expected = DeriveDungeonEntranceCamera(
      state.room_id, state.player_x, state.player_y);
  if (state.camera_x != validation.expected.camera_x ||
      state.camera_y != validation.expected.camera_y) {
    validation.has_repairable_difference = true;
    validation.differences.push_back(absl::StrFormat(
        "Camera scroll differs (current %04X,%04X; derived %04X,%04X).",
        state.camera_x, state.camera_y, validation.expected.camera_x,
        validation.expected.camera_y));
  }
  if (state.trigger_x != validation.expected.trigger_x ||
      state.trigger_y != validation.expected.trigger_y) {
    validation.has_repairable_difference = true;
    validation.differences.push_back(absl::StrFormat(
        "Camera triggers differ (current %04X,%04X; derived %04X,%04X).",
        state.trigger_x, state.trigger_y, validation.expected.trigger_x,
        validation.expected.trigger_y));
  }
  if (state.boundaries != validation.expected.boundaries) {
    validation.has_repairable_difference = true;
    validation.differences.push_back(
        "Camera boundary pages differ from the player-derived values.");
  }
  if (state.quadrant != validation.expected.quadrant) {
    validation.has_repairable_difference =
        validation.has_repairable_difference || validation.quadrant_repair_safe;
    validation.differences.push_back(
        state.room_id == kSpecialQuadrantRoom
            ? "Quadrant differs from the ordinary-room formula; room 0x104 "
              "is a known vanilla exception and will be preserved."
            : absl::StrFormat("Quadrant differs (current %02X; derived %02X).",
                              state.quadrant, validation.expected.quadrant));
  }
  return validation;
}

DungeonEntranceCameraState BuildRepairedDungeonEntranceCamera(
    const DungeonEntranceCameraState& state) {
  DungeonEntranceCameraState repaired = state;
  const auto derived = DeriveDungeonEntranceCamera(
      state.room_id, state.player_x, state.player_y);
  repaired.camera_x = derived.camera_x;
  repaired.camera_y = derived.camera_y;
  repaired.trigger_x = derived.trigger_x;
  repaired.trigger_y = derived.trigger_y;
  repaired.boundaries = derived.boundaries;
  if (state.room_id != kSpecialQuadrantRoom) {
    repaired.quadrant = derived.quadrant;
  }
  return repaired;
}

DungeonEntranceCameraState CaptureDungeonEntranceCamera(
    const zelda3::RoomEntrance& entrance) {
  return {
      .kind = DungeonEntranceRecordKind::kRegularEntrance,
      .room_id = entrance.room_,
      .player_x = entrance.x_position_,
      .player_y = entrance.y_position_,
      .camera_x = entrance.camera_x_,
      .camera_y = entrance.camera_y_,
      .trigger_x = entrance.camera_trigger_x_,
      .trigger_y = entrance.camera_trigger_y_,
      .boundaries = {entrance.camera_boundary_qn_, entrance.camera_boundary_fn_,
                     entrance.camera_boundary_qs_, entrance.camera_boundary_fs_,
                     entrance.camera_boundary_qw_, entrance.camera_boundary_fw_,
                     entrance.camera_boundary_qe_,
                     entrance.camera_boundary_fe_},
      .quadrant = entrance.scroll_quadrant_,
      .dirty = entrance.dirty(),
  };
}

DungeonEntranceCameraState CaptureDungeonSpawnCamera(
    const zelda3::DungeonSpawnPoint& spawn) {
  return {
      .kind = DungeonEntranceRecordKind::kSpawnPoint,
      .room_id = spawn.room_id,
      .player_x = spawn.x_coordinate,
      .player_y = spawn.y_coordinate,
      .camera_x = spawn.horizontal_scroll,
      .camera_y = spawn.vertical_scroll,
      .trigger_x = spawn.camera_trigger_x,
      .trigger_y = spawn.camera_trigger_y,
      .boundaries = spawn.camera_scroll_boundaries,
      .quadrant = spawn.quadrant,
      .dirty = spawn.dirty(),
  };
}

void ApplyDungeonEntranceCameraState(const DungeonEntranceCameraState& state,
                                     zelda3::RoomEntrance* entrance) {
  if (entrance == nullptr) {
    return;
  }
  entrance->camera_x_ = state.camera_x;
  entrance->camera_y_ = state.camera_y;
  entrance->camera_trigger_x_ = state.trigger_x;
  entrance->camera_trigger_y_ = state.trigger_y;
  entrance->camera_boundary_qn_ = state.boundaries[0];
  entrance->camera_boundary_fn_ = state.boundaries[1];
  entrance->camera_boundary_qs_ = state.boundaries[2];
  entrance->camera_boundary_fs_ = state.boundaries[3];
  entrance->camera_boundary_qw_ = state.boundaries[4];
  entrance->camera_boundary_fw_ = state.boundaries[5];
  entrance->camera_boundary_qe_ = state.boundaries[6];
  entrance->camera_boundary_fe_ = state.boundaries[7];
  entrance->scroll_quadrant_ = state.quadrant;
  RestoreDirtyState(state.dirty, entrance);
}

void ApplyDungeonSpawnCameraState(const DungeonEntranceCameraState& state,
                                  zelda3::DungeonSpawnPoint* spawn) {
  if (spawn == nullptr) {
    return;
  }
  spawn->horizontal_scroll = state.camera_x;
  spawn->vertical_scroll = state.camera_y;
  spawn->camera_trigger_x = state.trigger_x;
  spawn->camera_trigger_y = state.trigger_y;
  spawn->camera_scroll_boundaries = state.boundaries;
  spawn->quadrant = state.quadrant;
  RestoreDirtyState(state.dirty, spawn);
}

}  // namespace yaze::editor
