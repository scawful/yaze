#ifndef YAZE_ZELDA3_DUNGEON_DOOR_SURROUND_POLICY_H_
#define YAZE_ZELDA3_DUNGEON_DOOR_SURROUND_POLICY_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "rom/snes.h"
#include "zelda3/dungeon/door_types.h"

namespace yaze::zelda3 {

// Read-only ABI for Oracle's default-off upper-door surround prototype.
// An unpatched ROM, unknown version, or malformed descriptor has no policy.
// The native helper and editor consume the same room/physical-face overrides.
class DoorSurroundPolicy {
 public:
  static std::optional<DoorSurroundPolicy> Decode(
      const std::vector<uint8_t>& data) {
    constexpr size_t kHook = 0x890D;
    constexpr size_t kBridge = 0xB558;
    if (data.size() < kBridge + 8 || data[kHook] != 0x22 ||
        data[kHook + 4] != 0xEA || data[kHook + 5] != 0xEA ||
        data[kHook + 6] != 0xEA || data[kBridge] != 0x20 ||
        data[kBridge + 1] != 0x16 || data[kBridge + 2] != 0x89 ||
        data[kBridge + 3] != 0x6B || data[kBridge + 7] != 1) {
      return std::nullopt;
    }
    const uint32_t address = data[kBridge + 4] | (data[kBridge + 5] << 8) |
                             (data[kBridge + 6] << 16);
    if ((address & 0xFF0000) != 0x2C0000 || (address & 0x8000) == 0) {
      return std::nullopt;
    }
    const size_t table = SnesToPc(address);
    constexpr size_t kRoomCount = 296;
    constexpr size_t kRecordsOffset = 4 + 2 + kRoomCount + 2;
    if (table > data.size() || data.size() - table < kRecordsOffset + 4 ||
        data[table] != 'D' || data[table + 1] != 'S' ||
        data[table + 2] != 'R' || data[table + 3] != '1' ||
        (data[table + 4] | (data[table + 5] << 8)) != kRoomCount) {
      return std::nullopt;
    }
    const size_t count_offset = table + kRecordsOffset - 2;
    const size_t count = data[count_offset] | (data[count_offset + 1] << 8);
    // Maximum 12 positions on four walls in each of 296 native rooms.
    if (count > kRoomCount * 48 ||
        count > (data.size() - table - kRecordsOffset - 4) / 4 ||
        (address & 0xFFFF) + kRecordsOffset + count * 4 + 4 > 0x10000) {
      return std::nullopt;
    }
    const size_t end = table + kRecordsOffset + count * 4;
    if (data[end] != 0xFF || data[end + 1] != 0xFF || data[end + 2] != 0xFF ||
        data[end + 3] != 0xFF) {
      return std::nullopt;
    }
    DoorSurroundPolicy policy;
    for (size_t i = 0; i < kRoomCount; ++i) {
      if (data[table + 6 + i] > 1) {
        return std::nullopt;
      }
      policy.enabled_rooms_.push_back(data[table + 6 + i] != 0);
    }
    for (size_t i = 0; i < count; ++i) {
      const size_t entry = table + kRecordsOffset + i * 4;
      const uint16_t room = data[entry] | (data[entry + 1] << 8);
      const uint16_t face = data[entry + 2] | (data[entry + 3] << 8);
      if (room >= 296 || (face & 0xFF0C) != 0 || (face >> 4) >= 12) {
        return std::nullopt;
      }
      policy.exceptions_.emplace_back(room, face);
    }
    return policy;
  }

  bool Applies(int room, DoorType type, DoorDirection direction,
               uint8_t position) const {
    if (room < 0 || room >= static_cast<int>(enabled_rooms_.size()) ||
        !enabled_rooms_[room] || position >= 12 ||
        (type != DoorType::NormalDoor && type != DoorType::DoubleSidedShutter &&
         type != DoorType::SmallKeyDoor && type != DoorType::BigKeyDoor)) {
      return false;
    }
    const uint16_t face = (position << 4) | static_cast<uint8_t>(direction);
    for (const auto& [excluded_room, excluded_face] : exceptions_) {
      if (excluded_room == room && excluded_face == face) {
        return false;
      }
    }
    return true;
  }

 private:
  std::vector<bool> enabled_rooms_;
  std::vector<std::pair<uint16_t, uint16_t>> exceptions_;
};

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_DUNGEON_DOOR_SURROUND_POLICY_H_
