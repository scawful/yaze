#include "app/editor/dungeon/workspace/dungeon_pit_damage_view_model.h"

#include "absl/strings/str_format.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/pit_damage_table.h"

namespace yaze::editor {
namespace {

bool IsValidRoomId(uint16_t room_id) {
  return room_id < zelda3::kNumberOfRooms;
}

std::optional<uint16_t> FirstPitDamageMember(
    const zelda3::PitDamageTable& table) {
  for (uint16_t room_id : table.room_ids()) {
    if (IsValidRoomId(room_id)) {
      return room_id;
    }
  }
  return std::nullopt;
}

std::optional<uint16_t> FirstNonPitDamageRoom(
    const zelda3::PitDamageTable& table, uint16_t fallback) {
  if (IsValidRoomId(fallback) && !table.Contains(fallback)) {
    return fallback;
  }
  for (uint16_t room_id = 0; room_id < zelda3::kNumberOfRooms; ++room_id) {
    if (!table.Contains(room_id)) {
      return room_id;
    }
  }
  return std::nullopt;
}

absl::Status ValidateTableAndRoom(zelda3::PitDamageTable* table,
                                  uint16_t room_id, const char* room_label) {
  if (table == nullptr) {
    return absl::InvalidArgumentError("Pit damage table is unavailable");
  }
  if (!IsValidRoomId(room_id)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("%s room 0x%03X is outside the dungeon room range",
                        room_label, room_id));
  }
  return absl::OkStatus();
}

}  // namespace

PitDamageMembershipState BuildPitDamageMembershipState(
    const zelda3::PitDamageTable* table, int room_id,
    uint16_t replacement_fallback, uint16_t victim_fallback) {
  PitDamageMembershipState state;
  state.table_available = table != nullptr;
  state.room_valid = room_id >= 0 && room_id < zelda3::kNumberOfRooms;
  if (!state.table_available || !state.room_valid) {
    return state;
  }

  state.room_id = static_cast<uint16_t>(room_id);
  state.deals_damage = table->Contains(state.room_id);
  state.dirty = table->dirty();
  state.suggested_replacement_room =
      FirstNonPitDamageRoom(*table, replacement_fallback);
  state.suggested_victim_room = FirstPitDamageMember(*table);
  if (table->Contains(victim_fallback)) {
    state.suggested_victim_room = victim_fallback;
  }
  return state;
}

absl::Status AddCurrentRoomToPitDamage(zelda3::PitDamageTable* table,
                                       uint16_t current_room_id,
                                       uint16_t victim_room_id) {
  RETURN_IF_ERROR(ValidateTableAndRoom(table, current_room_id, "Current"));
  RETURN_IF_ERROR(ValidateTableAndRoom(table, victim_room_id, "Victim"));
  if (table->Contains(current_room_id)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room 0x%03X already deals pit damage", current_room_id));
  }
  if (!table->Contains(victim_room_id)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Victim room 0x%03X is not in RoomsWithPitDamage", victim_room_id));
  }
  return table->ReplaceRoomId(victim_room_id, current_room_id);
}

absl::Status RemoveCurrentRoomFromPitDamage(zelda3::PitDamageTable* table,
                                            uint16_t current_room_id,
                                            uint16_t replacement_room_id) {
  RETURN_IF_ERROR(ValidateTableAndRoom(table, current_room_id, "Current"));
  RETURN_IF_ERROR(
      ValidateTableAndRoom(table, replacement_room_id, "Replacement"));
  if (!table->Contains(current_room_id)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Room 0x%03X is not in RoomsWithPitDamage", current_room_id));
  }
  if (table->Contains(replacement_room_id)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Replacement room 0x%03X already deals pit damage",
                        replacement_room_id));
  }
  return table->ReplaceRoomId(current_room_id, replacement_room_id);
}

}  // namespace yaze::editor
