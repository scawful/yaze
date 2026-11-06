#ifndef YAZE_ZELDA3_OVERWORLD_LYRA_TERRAIN_SOURCE_H_
#define YAZE_ZELDA3_OVERWORLD_LYRA_TERRAIN_SOURCE_H_

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/lyra_terrain_document.h"
#include "zelda3/overworld/overworld_geometry.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_terrain_io.h"

namespace yaze::gfx {
class Bitmap;
class SnesPalette;
struct PaletteGroupMap;
}  // namespace yaze::gfx
namespace yaze::zelda3 {
class Overworld;

struct SpecialOverworldTerrainImportPlan {
  OverworldTile32Packing tile32;
  std::vector<OverworldGeometryRow> geometry;
  OverworldTerrainSavePlan terrain;
};
// Load a private model from saved legacy ROM bytes and consume its document once.
// all160 original terrain grids and explicit32-map area assignments. Asset,
// entity and complete-directory publication belong to the full import owner.
absl::StatusOr<SpecialOverworldTerrainImportPlan>
PlanSpecialOverworldTerrainImport(const Rom& source,
                                  const core::LyraTerrainDocument& document,
                                  const OverworldTerrainStorage& destination);

// Resolve the native Loom render profile without changing the active map or ROM.
// The optional atlas is used only by the editor; build verification stays headless.
absl::Status BindLyraTerrainSource(Overworld& overworld,
                                   core::LyraTerrainProfile* profile,
                                   gfx::Bitmap* atlas = nullptr);

// Exact selector/state key and resolved native pixels/palette used by both
// editor previews and build verification. This path creates no GPU textures.
std::string LyraMapAssetKey(const core::LyraMapProperties& properties,
                            int state);
// Apply project-owned BG colors to an already resolved palette copy. Never
// changes native groups, ROM storage, or the upper 128 OBJ colors.
absl::Status ApplyLyraBackgroundPaletteOverride(
    const core::LyraMapProperties& properties, gfx::SnesPalette& palette);
// Array path for native revision-4 table 411 and tests: BG colors 0..127 only.
absl::Status ApplyLyraBackgroundPaletteWords128(
    gfx::SnesPalette& palette, std::span<const uint16_t> words);
// Temporary import data, not a second palette store or allocation grant.
// A match identifies an existing record; selecting a set or reserving new
// storage still belongs to the normal palette assignment service.
struct OverworldBackgroundPaletteComponent {
  std::string group;
  std::vector<uint16_t> colors;
  std::vector<int> matching_records;
};
struct OverworldBackgroundPaletteImportPlan {
  uint16_t background_color = 0;
  bool uniform_background_color = false;
  bool shared_hud_matches = false;
  // Native compositor order: main35, animated7, aux1/aux2 each21 colors.
  std::array<OverworldBackgroundPaletteComponent, 4> components;
};
absl::StatusOr<OverworldBackgroundPaletteImportPlan>
PlanLegacyOverworldBackgroundPalette(
    const std::array<uint16_t, 128>& colors,
    const gfx::PaletteGroupMap& native_palettes);

struct LyraNativeMapAssets {
  OverworldMap source;
  std::string binding;
};
absl::StatusOr<LyraNativeMapAssets> ResolveLyraMapAssets(
    Overworld& overworld, const core::LyraMapProperties& properties, int state);

// Describe a result from the fresh native resolver without loading or changing
// assets. Native-only describes inspection, never an authored runtime/catalog
// assignment. Retains the legacy binding and identifies resolved components.
absl::StatusOr<std::string> DescribeLyraNativeMapAssets(
    const core::LyraMapProperties& properties, int state,
    const LyraNativeMapAssets& assets);
}  // namespace yaze::zelda3

#endif
