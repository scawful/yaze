#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "absl/status/statusor.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "rom/write_fence.h"

namespace yaze::zelda3 {

using OverworldTerrainPage = std::array<uint16_t, 256>;
struct OverworldMapTiles;

// One deterministic dictionary/index plan shared by ordinary Save and the
// first expanded-map import. Four definitions share each packed 6-byte row.
struct OverworldTile32Packing {
  std::vector<gfx::Tile32> definitions;
  std::vector<OverworldTerrainPage> pages;
  std::array<std::vector<uint8_t>, 4> quadrant_bytes;
};
absl::StatusOr<OverworldTile32Packing> PlanOverworldTile32Packing(
    const OverworldMapTiles& tiles, int map_count, size_t definition_capacity);

struct OverworldTerrainPointerTables {
  uint32_t high_table_pc = 0;
  uint32_t low_table_pc = 0;
  int map_count = 160;
};

// Physical storage supplied by the resolved format, not by canvas selection.
// This does not establish engine or model support for the requested capacity.
struct OverworldTerrainStorage {
  uint32_t high_table_pc = 0;
  uint32_t low_table_pc = 0;
  int map_count = 160;
  std::vector<rom::WriteRange> stream_regions;
  // Existing streams may remain readable without becoming allocation space.
  std::vector<rom::WriteRange> read_only_stream_regions;
  // Retained native readers use these original 00–9F pointer slots. Both
  // copies must agree on load and are published in the same transaction.
  std::optional<OverworldTerrainPointerTables> native_prefix;
  // True only when the format reserves stream_regions for terrain streams, so
  // bytes no surviving pointer covers can be reused whatever they contain.
  bool stream_regions_exclusive = false;
};

struct OverworldTerrainWrite {
  uint32_t pc = 0;
  std::vector<uint8_t> before;
  std::vector<uint8_t> after;
};

absl::StatusOr<OverworldTerrainStorage> ResolveNativeOverworldTerrainStorage(
    const Rom& rom);

struct OverworldTerrainSavePlan {
  OverworldTerrainStorage storage;
  // The complete read set includes other maps' aliases and pointer slots.
  // A change outside the planned writes can invalidate allocation ownership.
  std::vector<uint8_t> baseline;
  std::vector<OverworldTerrainWrite> writes;
  // Import writes are consumed by the complete dictionary/directory image
  // planner; ApplyOverworldTerrainSave must not publish this partial image.
  bool initial_import = false;
};

// Preserve unchanged streams/pointers, share identical decoded planes, and
// place changed streams without overwriting any stream still in use. All
// pointers and independently scanned output pages are checked before mutation.
absl::StatusOr<OverworldTerrainSavePlan> PlanOverworldTerrainSave(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages);

// Read the original 160 streams and allocate the complete desired 160/192-map
// image without inventing old streams for new maps. The editable ROM copy has
// no native directory yet and includes the declared destination storage.
absl::StatusOr<OverworldTerrainSavePlan> PlanInitialOverworldTerrainImport(
    const Rom& rom, const OverworldTerrainStorage& storage,
    std::span<const OverworldTerrainPage> pages);

// Preimages and all active write fences remain effective. Failure restores the
// complete ROM, filename and dirty state through the existing save transaction.
absl::Status ApplyOverworldTerrainSave(Rom& rom,
                                       const OverworldTerrainSavePlan& plan);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_OVERWORLD_TERRAIN_IO_H_
