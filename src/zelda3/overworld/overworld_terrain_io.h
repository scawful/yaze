#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "rom/write_fence.h"

namespace yaze::zelda3 {

using OverworldTerrainPage = std::array<uint16_t, 256>;

// Physical storage supplied by the resolved format, not by canvas selection.
// This does not establish engine or model support for the requested capacity.
struct OverworldTerrainStorage {
  uint32_t high_table_pc = 0;
  uint32_t low_table_pc = 0;
  int map_count = 160;
  std::vector<rom::WriteRange> stream_regions;
};

struct OverworldTerrainWrite {
  uint32_t pc = 0;
  std::vector<uint8_t> before;
  std::vector<uint8_t> after;
};

struct OverworldTerrainSavePlan {
  OverworldTerrainStorage storage;
  // The complete read set includes other maps' aliases and pointer slots.
  // A change outside the planned writes can invalidate allocation ownership.
  std::vector<uint8_t> baseline;
  std::vector<OverworldTerrainWrite> writes;
};

// Preserve unchanged streams/pointers, share identical decoded planes, and
// place changed streams without overwriting any stream still in use. All
// pointers and independently scanned output pages are checked before mutation.
absl::StatusOr<OverworldTerrainSavePlan> PlanOverworldTerrainSave(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages);

// Preimages and all active write fences remain effective. Failure restores the
// complete ROM, filename and dirty state through the existing save transaction.
absl::Status ApplyOverworldTerrainSave(Rom& rom,
                                       const OverworldTerrainSavePlan& plan);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_
