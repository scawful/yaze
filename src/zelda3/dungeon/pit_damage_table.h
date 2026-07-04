#ifndef YAZE_ZELDA3_DUNGEON_PIT_DAMAGE_TABLE_H_
#define YAZE_ZELDA3_DUNGEON_PIT_DAMAGE_TABLE_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"

namespace yaze {
namespace zelda3 {

// ROM-backed membership table for RoomsWithPitDamage (bank $07).
// Falling in a listed room deals damage; unlisted rooms use per-room pit
// destination metadata from the room header instead.
class PitDamageTable {
 public:
  static absl::Status LoadFromRom(Rom* rom, PitDamageTable* out);
  absl::Status SaveToRom(Rom* rom) const;

  const std::vector<uint16_t>& room_ids() const { return room_ids_; }
  bool Contains(uint16_t room_id) const;
  void SetRoomIds(std::vector<uint16_t> room_ids);
  void MarkDirty() { dirty_ = true; }
  bool dirty() const { return dirty_; }

 private:
  std::vector<uint16_t> room_ids_;
  bool dirty_ = false;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_PIT_DAMAGE_TABLE_H_
