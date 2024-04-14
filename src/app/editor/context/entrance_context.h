#ifndef YAZE_APP_EDITOR_CONTEXT_ENTRANCE_CONTEXT_H_
#define YAZE_APP_EDITOR_CONTEXT_ENTRANCE_CONTEXT_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace editor {
namespace context {

class EntranceContext {
 public:
  absl::Status LoadEntranceTileTypes(Rom& rom) {
    int offset_low = 0xDB8BF;
    int offset_high = 0xDB917;

    for (int i = 0; i < 0x2C; i++) {
      // Load entrance tile types
      ASSIGN_OR_RETURN(auto value_low, rom.ReadWord(offset_low + i));
      entrance_tile_types_low_.push_back(value_low);
      ASSIGN_OR_RETURN(auto value_high, rom.ReadWord(offset_high + i));
      entrance_tile_types_low_.push_back(value_high);
    }

    return absl::OkStatus();
  }

 private:
  std::vector<uint16_t> entrance_tile_types_low_;
  std::vector<uint16_t> entrance_tile_types_high_;
};

}  // namespace context
}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CONTEXT_ENTRANCE_CONTEXT_H_