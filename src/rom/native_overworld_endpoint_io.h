#ifndef YAZE_ROM_NATIVE_OVERWORLD_ENDPOINT_IO_H_
#define YAZE_ROM_NATIVE_OVERWORLD_ENDPOINT_IO_H_

#include <cstdint>
#include <span>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze::rom {

// Runtime table records, not authoring placement IDs or destination room IDs.
// Expected records retain every byte, including unused/deleted row payloads.
struct NativeOverworldEntranceRow {
  uint16_t map_word = 0;
  uint16_t position = 0;
  uint8_t entrance_id = 0;
  bool operator==(const NativeOverworldEntranceRow&) const = default;
};

struct NativeOverworldReturnRow {
  uint16_t room = 0;
  uint8_t map = 0;
  uint16_t vram = 0;
  uint16_t scroll_y = 0;
  uint16_t scroll_x = 0;
  uint16_t player_y = 0;
  uint16_t player_x = 0;
  uint16_t camera_y = 0;
  uint16_t camera_x = 0;
  uint8_t scroll_mod_y = 0;
  uint8_t scroll_mod_x = 0;
  uint16_t door_type_1 = 0;
  uint16_t door_type_2 = 0;
  bool operator==(const NativeOverworldReturnRow&) const = default;
};

struct NativeOverworldEntranceEdit {
  uint16_t row = 0;
  NativeOverworldEntranceRow expected;
  NativeOverworldEntranceRow value;
};
struct NativeOverworldReturnEdit {
  uint16_t row = 0;
  NativeOverworldReturnRow expected;
  NativeOverworldReturnRow value;
};
struct NativeOverworldEndpointEdits {
  std::vector<NativeOverworldEntranceEdit> entrances;
  std::vector<NativeOverworldReturnEdit> returns;
};
struct NativeOverworldEndpointWrite {
  uint32_t address = 0;
  std::vector<uint8_t> bytes;
};

// Plans selected rows in the existing US/Oracle runtime tables. Expansion is
// read from the ROM's existing entrance flag; no addresses or row allocation
// are supplied by callers. Relocated project table overrides are not supported
// by this planner. The model adapter must refuse them rather than drop them.
// final_image supplies proposed geometry only; the sealer passes its validated
// candidate. Row preimages and storage selection always use baseline. An empty
// span uses baseline geometry. Does not mutate either image or bind ID412.
absl::StatusOr<std::vector<NativeOverworldEndpointWrite>>
PlanNativeOverworldEndpointSave(const std::vector<uint8_t>& baseline,
                                const NativeOverworldEndpointEdits& edits,
                                std::span<const uint8_t> final_image = {});

}  // namespace yaze::rom

#endif  // YAZE_ROM_NATIVE_OVERWORLD_ENDPOINT_IO_H_
