#ifndef YAZE_ZELDA3_ATLAS_ATLAS_JSON_H_
#define YAZE_ZELDA3_ATLAS_ATLAS_JSON_H_

// Atlas model -> JSON (schema "yaze.atlas.dungeon/1"). This is the
// `z3ed atlas-render --json` output and the golden-test surface. The output
// is deterministic: no timestamps, and paths only where the caller passes
// them in DungeonAtlasJsonContext.

#include <string>

#include "zelda3/atlas/dungeon_atlas.h"

namespace yaze::zelda3::atlas {

inline constexpr char kDungeonAtlasJsonSchema[] = "yaze.atlas.dungeon/1";

struct DungeonAtlasJsonContext {
  std::string project;    // Project file as given, empty = none.
  std::string generator;  // For example "z3ed 0.8.0".
};

std::string DungeonAtlasToJson(const DungeonAtlas& atlas,
                               const DungeonAtlasJsonContext& context = {},
                               int indent = 1);

}  // namespace yaze::zelda3::atlas

#endif  // YAZE_ZELDA3_ATLAS_ATLAS_JSON_H_
