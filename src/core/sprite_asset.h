#ifndef YAZE_CORE_SPRITE_ASSET_H_
#define YAZE_CORE_SPRITE_ASSET_H_

#include <array>
#include <cstdint>
#include <string>

#include "core/sprite_behavior.h"

namespace yaze::project {
struct SpritePaletteBinding {
  std::string group = "auto";
  int index = 0;
};

// Stored in the project, leaving the upstream ZSM binary format unchanged.
// zsm_path identifies the saved asset; catalog_key is optional provenance.
struct SpriteAssetBinding {
  std::string zsm_path;
  std::string catalog_key;
  std::string draw_adapter = "literal_v1";
  std::string source_path;
  std::string source_label;
  std::string source_sha256;
  std::array<uint8_t, 8> sheets = {0, 10, 6, 7, 0, 0, 0, 0};
  std::array<SpritePaletteBinding, 8> palette_rows;
  SpriteBehavior behavior;
};

}  // namespace yaze::project
#endif
