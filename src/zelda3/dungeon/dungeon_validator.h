#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H

#include <string>
#include <vector>

#include "zelda3/dungeon/dungeon_limits.h"
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
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_VALIDATOR_H
