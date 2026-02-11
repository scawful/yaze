#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H

#include <string>
#include <vector>

#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

struct ValidationResult {
  bool is_valid = true;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
};

class DungeonValidator {
 public:
  ValidationResult ValidateRoom(const Room& room);

 private:
  // Limits from ALTTP hardware and engine
  static constexpr int kMaxSprites = 16;       // Active sprites limit (hardware/engine constraint)
  static constexpr int kMaxTotalSprites = 64;  // Total sprites in room list (arbitrary safety limit)
  static constexpr int kMaxChests = 6;         // Limit for item collection flags (per room)
  static constexpr int kMaxDoors = 16;         // Practical limit for door objects
  static constexpr int kMaxObjects = 400;      // Limit before processing lag might occur
  static constexpr int kMaxBg3Objects = 128;   // Guardrail for unstable BG3-heavy rooms
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H
