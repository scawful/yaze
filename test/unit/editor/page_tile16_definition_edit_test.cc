#include <algorithm>

#include "app/gfx/types/snes_tile.h"
#include "gtest/gtest.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "test_utils/overworld_model_test_peer.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace {

gfx::Tile16 MakeSolidTile16(uint8_t palette) {
  const gfx::TileInfo info(0, palette, false, false, false);
  return gfx::Tile16(info, info, info, info);
}

class PageTile16DefinitionEditTest : public ::testing::Test {
 protected:
  void SetUp() override {
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    zelda3::OverworldModelTestPeer::InitializeMapSlots(
        *overworld_, zelda3::kNumOverworldMaps);
    auto* global = overworld_->mutable_tiles16();
    global->assign(4096, MakeSolidTile16(0));
  }

  Rom rom_;
  std::unique_ptr<zelda3::Overworld> overworld_;
};

TEST_F(PageTile16DefinitionEditTest, HasPageTile16AndEncodeRoundTrip) {
  const auto encoded = zelda3::Overworld::MakePageTile16Definition(
      0x90, 0xFD2, MakeSolidTile16(1));
  EXPECT_EQ(encoded.map_id, 0x90);
  EXPECT_EQ(encoded.tile_id, 0xFD2);
  zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, {encoded});
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_FALSE(overworld_->HasPageTile16(0x91, 0xFD2));
  EXPECT_NE(overworld_->Tiles16ForMap(0x90)[0xFD2],
            overworld_->tiles16()[0xFD2]);
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2],
            overworld_->tiles16()[0xFD2]);
}

TEST_F(PageTile16DefinitionEditTest,
       PageLocalUpsertKeepsNeighborGlobalAndSupportsRestore) {
  zelda3::OverworldModelTestPeer::SetPageTile16(
      *overworld_, {zelda3::Overworld::MakePageTile16Definition(
                       0x90, 0xFD2, MakeSolidTile16(1))});
  const auto before = overworld_->page_tile16();
  const auto global_before = overworld_->tiles16()[0xFD2];

  std::vector<rom::NativePageTile16Definition> rows = before;
  rows.erase(std::remove_if(rows.begin(), rows.end(),
                            [](const auto& row) {
                              return row.map_id == 0x90 && row.tile_id == 0xFD2;
                            }),
             rows.end());
  rows.push_back(zelda3::Overworld::MakePageTile16Definition(
      0x90, 0xFD2, MakeSolidTile16(3)));
  zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, rows);

  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(3));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2], global_before);
  EXPECT_EQ(overworld_->tiles16()[0xFD2], global_before);

  zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, before);
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(1));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x91)[0xFD2], global_before);
  EXPECT_EQ(overworld_->tiles16()[0xFD2], global_before);
}

TEST_F(PageTile16DefinitionEditTest,
       ClearingPageOverrideRevealsUnchangedGlobal) {
  zelda3::OverworldModelTestPeer::SetPageTile16(
      *overworld_, {zelda3::Overworld::MakePageTile16Definition(
                        0x90, 0xFD2, MakeSolidTile16(2)),
                    zelda3::Overworld::MakePageTile16Definition(
                        0x90, 0x010, MakeSolidTile16(4))});
  (*overworld_->mutable_tiles16())[0xFD2] = MakeSolidTile16(7);

  std::vector<rom::NativePageTile16Definition> kept;
  for (const auto& row : overworld_->page_tile16()) {
    if (!(row.map_id == 0x90 && row.tile_id == 0xFD2))
      kept.push_back(row);
  }
  zelda3::OverworldModelTestPeer::SetPageTile16(*overworld_, kept);
  EXPECT_FALSE(overworld_->HasPageTile16(0x90, 0xFD2));
  EXPECT_TRUE(overworld_->HasPageTile16(0x90, 0x010));
  EXPECT_EQ(overworld_->Tiles16ForMap(0x90)[0xFD2], MakeSolidTile16(7));
}

}  // namespace
}  // namespace yaze
