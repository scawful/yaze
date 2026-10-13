#include "zelda3/atlas/atlas_defaults.h"

namespace yaze::zelda3::atlas {

const std::vector<AtlasColor>& AtlasAreaPalette() {
  static const std::vector<AtlasColor> palette = {
      {226, 118, 118}, {238, 160, 88},  {228, 204, 106}, {96, 158, 232},
      {150, 212, 238}, {182, 130, 92},  {84, 184, 140},  {158, 112, 210},
      {116, 126, 226}, {216, 110, 168}, {144, 206, 96},  {234, 210, 160},
      {246, 196, 64},  {150, 66, 74}};
  return palette;
}

// Names match the census's vanilla owner names; colors are the palette in
// order (design doc section 6.4).
const std::vector<VanillaDungeonDefault>& VanillaDungeonDefaults() {
  static const std::vector<VanillaDungeonDefault> defaults = [] {
    const auto& palette = AtlasAreaPalette();
    std::vector<VanillaDungeonDefault> out = {
        {0x00, "SEW", "Sewers", palette[0]},
        {0x02, "HC", "Hyrule Castle", palette[1]},
        {0x04, "EP", "Eastern Palace", palette[2]},
        {0x06, "DP", "Desert Palace", palette[3]},
        {0x08, "AT", "Agahnim's Tower", palette[4]},
        {0x0A, "SP", "Swamp Palace", palette[5]},
        {0x0C, "PD", "Palace of Darkness", palette[6]},
        {0x0E, "MM", "Misery Mire", palette[7]},
        {0x10, "SW", "Skull Woods", palette[8]},
        {0x12, "IP", "Ice Palace", palette[9]},
        {0x14, "TH", "Tower of Hera", palette[10]},
        {0x16, "TT", "Thieves' Town", palette[11]},
        {0x18, "TR", "Turtle Rock", palette[12]},
        {0x1A, "GT", "Ganon's Tower", palette[13]},
        {0xFF, "INT", "Interiors (houses, caves)", kAtlasInteriorColor},
    };
    return out;
  }();
  return defaults;
}

}  // namespace yaze::zelda3::atlas
