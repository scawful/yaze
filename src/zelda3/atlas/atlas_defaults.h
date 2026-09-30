#ifndef YAZE_ZELDA3_ATLAS_ATLAS_DEFAULTS_H_
#define YAZE_ZELDA3_ATLAS_ATLAS_DEFAULTS_H_

// Built-in vanilla Atlas defaults. Style and vanilla ALTTP facts only;
// project data (names, codes, colors of a hack) comes from annotations.

#include <string>
#include <vector>

#include "zelda3/atlas/atlas_annotations.h"

namespace yaze::zelda3::atlas {

struct VanillaDungeonDefault {
  int dungeon_id = 0;  // Entrance dungeon ID byte (0xFF = interiors).
  const char* code = "";
  const char* name = "";
  AtlasColor color;
};

// Sewers (0x00) ... Ganon's Tower (0x1A), then interiors (0xFF).
const std::vector<VanillaDungeonDefault>& VanillaDungeonDefaults();

// Fill colors for owners and areas without an annotated color, in order.
const std::vector<AtlasColor>& AtlasAreaPalette();

inline constexpr AtlasColor kAtlasOtherColor{120, 120, 132};
inline constexpr AtlasColor kAtlasNoOwnerColor{30, 33, 40};
inline constexpr AtlasColor kAtlasInteriorColor{86, 90, 102};

}  // namespace yaze::zelda3::atlas

#endif  // YAZE_ZELDA3_ATLAS_ATLAS_DEFAULTS_H_
