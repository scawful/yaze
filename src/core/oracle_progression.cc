#include "core/oracle_progression.h"

namespace yaze::core {

OracleProgressionState OracleProgressionState::ParseFromSRAM(
    const uint8_t* data, size_t len) {
  OracleProgressionState state;
  if (!data) return state;

  auto read = [&](uint16_t offset) -> uint8_t {
    return (offset < len) ? data[offset] : 0;
  };

  state.crystal_bitfield = read(kCrystalOffset);
  state.game_state = read(kGameStateOffset);
  state.oosprog = read(kOosProgOffset);
  state.oosprog2 = read(kOosProg2Offset);
  state.side_quest = read(kSideQuestOffset);
  state.pendants = read(kPendantOffset);

  return state;
}

int OracleProgressionState::GetCrystalCount() const {
  // Only count the 7 valid crystal bits (mask off bit 7)
  uint8_t valid = crystal_bitfield & 0x7F;
  int count = 0;
  while (valid) {
    count += (valid & 1);
    valid >>= 1;
  }
  return count;
}

bool OracleProgressionState::IsDungeonComplete(int dungeon_number) const {
  uint8_t mask = GetCrystalMask(dungeon_number);
  return mask != 0 && (crystal_bitfield & mask) != 0;
}

std::string OracleProgressionState::GetGameStateName() const {
  switch (game_state) {
    case 0:
      return "Start";
    case 1:
      return "Loom Beach";
    case 2:
      return "Kydrog Complete";
    case 3:
      return "Farore Rescued";
    default:
      return "Unknown (" + std::to_string(game_state) + ")";
  }
}

std::string OracleProgressionState::GetDungeonName(int dungeon_number) {
  switch (dungeon_number) {
    case 1:
      return "D1 Mushroom Grotto";
    case 2:
      return "D2 Tail Palace";
    case 3:
      return "D3 Kalyxo Castle";
    case 4:
      return "D4 Zora Temple";
    case 5:
      return "D5 Glacia Estate";
    case 6:
      return "D6 Goron Mines";
    case 7:
      return "D7 Dragon Ship";
    default:
      return "Unknown Dungeon";
  }
}

uint8_t OracleProgressionState::GetCrystalMask(int dungeon_number) {
  // Non-sequential mapping (intentional for non-linear design)
  switch (dungeon_number) {
    case 1:
      return kCrystalD1;  // 0x01
    case 2:
      return kCrystalD2;  // 0x10
    case 3:
      return kCrystalD3;  // 0x40
    case 4:
      return kCrystalD4;  // 0x20
    case 5:
      return kCrystalD5;  // 0x04
    case 6:
      return kCrystalD6;  // 0x02
    case 7:
      return kCrystalD7;  // 0x08
    default:
      return 0;
  }
}

}  // namespace yaze::core
