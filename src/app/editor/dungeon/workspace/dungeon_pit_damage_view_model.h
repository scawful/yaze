#ifndef YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_PIT_DAMAGE_VIEW_MODEL_H_
#define YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_PIT_DAMAGE_VIEW_MODEL_H_

#include <cstdint>
#include <optional>

#include "absl/status/status.h"

namespace yaze::zelda3 {
class PitDamageTable;
}  // namespace yaze::zelda3

namespace yaze::editor {

struct PitDamageMembershipState {
  bool table_available = false;
  bool room_valid = false;
  uint16_t room_id = 0;
  bool deals_damage = false;
  bool dirty = false;
  std::optional<uint16_t> suggested_replacement_room;
  std::optional<uint16_t> suggested_victim_room;
};

PitDamageMembershipState BuildPitDamageMembershipState(
    const zelda3::PitDamageTable* table, int room_id,
    uint16_t replacement_fallback, uint16_t victim_fallback);

absl::Status AddCurrentRoomToPitDamage(zelda3::PitDamageTable* table,
                                       uint16_t current_room_id,
                                       uint16_t victim_room_id);

absl::Status RemoveCurrentRoomFromPitDamage(zelda3::PitDamageTable* table,
                                            uint16_t current_room_id,
                                            uint16_t replacement_room_id);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_WORKSPACE_DUNGEON_PIT_DAMAGE_VIEW_MODEL_H_
