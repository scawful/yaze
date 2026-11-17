#ifndef YAZE_ROM_NATIVE_OVERWORLD_ITEM_IO_H_
#define YAZE_ROM_NATIVE_OVERWORLD_ITEM_IO_H_

#include <cstdint>
#include <span>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::rom {

struct NativeOverworldItemRecord {
  uint16_t map_pos = 0;  // ((game_y << 6) | game_x) << 1
  uint8_t item_id = 0;
  bool operator==(const NativeOverworldItemRecord&) const = default;
};

struct NativeOverworldItemMapEdit {
  uint16_t map_id = 0;
  std::vector<NativeOverworldItemRecord> items;
  bool operator==(const NativeOverworldItemMapEdit&) const = default;
};

struct NativeOverworldItemEdits {
  std::vector<NativeOverworldItemMapEdit> maps;
};

struct NativeOverworldItemWrite {
  uint32_t address = 0;
  std::vector<uint8_t> bytes;
};

// Plans minimal-churn (in-place / free-gap, falling back to deduplicated
// compact repack) writes for bank $1B secret-item streams [0x0DC2F9, 0x0DC89C),
// descriptor entry 23 (and 0x012784 mirror for maps < $A0), and descriptor
// entry 22 bomb-door locations (and 0x012644 mirror for maps < $A0).
// final_image supplies proposed parent/size geometry only; an empty span uses
// baseline geometry. Never mutates baseline or final_image.
absl::StatusOr<std::vector<NativeOverworldItemWrite>>
PlanNativeOverworldItemSave(const std::vector<uint8_t>& baseline,
                            const NativeOverworldItemEdits& edits,
                            std::span<const uint8_t> final_image = {});

}  // namespace yaze::rom

#endif  // YAZE_ROM_NATIVE_OVERWORLD_ITEM_IO_H_
