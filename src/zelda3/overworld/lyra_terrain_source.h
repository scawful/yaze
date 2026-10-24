#ifndef YAZE_ZELDA3_OVERWORLD_LYRA_TERRAIN_SOURCE_H_
#define YAZE_ZELDA3_OVERWORLD_LYRA_TERRAIN_SOURCE_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/lyra_terrain_document.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::gfx {
class Bitmap;
}
namespace yaze::zelda3 {
class Overworld;

// Resolve the native Loom render profile without changing the active map or ROM.
// The optional atlas is used only by the editor; build verification stays headless.
absl::Status BindLyraTerrainSource(Overworld& overworld,
                                   core::LyraTerrainProfile* profile,
                                   gfx::Bitmap* atlas = nullptr);

// Exact selector/state key and resolved native pixels/palette used by both
// editor previews and build verification. This path creates no GPU textures.
std::string LyraMapAssetKey(const core::LyraMapProperties& properties,
                            int state);
struct LyraNativeMapAssets {
  OverworldMap source;
  std::string binding;
};
absl::StatusOr<LyraNativeMapAssets> ResolveLyraMapAssets(
    Overworld& overworld, const core::LyraMapProperties& properties, int state);
}  // namespace yaze::zelda3

#endif
