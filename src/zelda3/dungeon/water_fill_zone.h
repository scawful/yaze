#ifndef YAZE_APP_ZELDA3_DUNGEON_WATER_FILL_ZONE_H
#define YAZE_APP_ZELDA3_DUNGEON_WATER_FILL_ZONE_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze {
class Rom;
}

namespace yaze::zelda3 {

struct WaterFillZoneEntry {
  int room_id = -1;
  uint8_t sram_bit_mask = 0;           // Bit in $7EF411 (e.g. 0x01)
  std::vector<uint16_t> fill_offsets;  // Each offset = Y*64 + X (0..4095)
};

// Load the editor-authored water fill table from the reserved ROM region.
// Returns an empty list if the table is not present.
absl::StatusOr<std::vector<WaterFillZoneEntry>> LoadWaterFillTable(Rom* rom);

// Write the editor-authored water fill table into the reserved ROM region.
// This overwrites the reserved region (kWaterFillTableStart..kWaterFillTableEnd)
// and leaves collision data untouched.
absl::Status WriteWaterFillTable(Rom* rom,
                                 const std::vector<WaterFillZoneEntry>& zones);

// Best-effort import of legacy WaterGate_* tables from an asar-generated .sym
// file (WLA symbol format). This is intended as a one-time migration aid.
//
// Returns an empty list if the symbol file cannot be read or the symbols are
// not present.
absl::StatusOr<std::vector<WaterFillZoneEntry>> LoadLegacyWaterGateZones(
    Rom* rom, const std::string& symbol_path);

}  // namespace yaze::zelda3

#endif  // YAZE_APP_ZELDA3_DUNGEON_WATER_FILL_ZONE_H

