#include "Data/rom.h"

#include <gtest/gtest.h>

#include "Data/rom.h"
#include "Graphics/tile.h"

namespace YazeTests {
namespace ROMTestSuite {

TEST(ROMTest, decompress_basic_test) {
  yaze::Application::Data::ROM rom;
  rom.LoadFromFile("C:/alttp.sfc");
  yaze::Application::Graphics::TilePreset current_set;
  current_set.bpp = 4;
  current_set.length = 28672;
  current_set.pcTilesLocation = 0x80000;
  current_set.SNESTilesLocation = 0x0000;
  current_set.pcPaletteLocation = 0xDD326;
  current_set.SNESPaletteLocation = 0x0000;
  auto tiles_ = rom.ExtractTiles(current_set);
  auto current_palette_ = rom.ExtractPalette(current_set);
}

}  // namespace ROMTestSuite
}  // namespace YazeTests