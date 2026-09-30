#include "core/overworld_graphics_preview.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>

#include "core/project.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::zelda3 {
namespace {

TEST(OverworldGraphicsPreviewTest, RawFourBitPlanesAndVramWordOffsets) {
  std::vector<uint8_t> bytes(0x2000, 0);
  bytes[0] = 0x80;
  bytes[1] = 0x40;
  bytes[16] = 0x20;
  bytes[17] = 0x10;
  auto boat = DecodeGraphicsPreviewTransfer(bytes, 0x2C00, 0x2000);
  ASSERT_TRUE(boat.ok());
  ASSERT_EQ(boat->size(), 256u);
  EXPECT_EQ(boat->begin()->first, 0xC0);
  EXPECT_EQ(boat->rbegin()->first, 0x1BF);
  EXPECT_EQ(boat->at(0xC0)[0], 1);
  EXPECT_EQ(boat->at(0xC0)[1], 2);
  EXPECT_EQ(boat->at(0xC0)[2], 4);
  EXPECT_EQ(boat->at(0xC0)[3], 8);
  auto boat2 = DecodeGraphicsPreviewTransfer(bytes, 0x5000, 0x2000);
  ASSERT_TRUE(boat2.ok());
  EXPECT_EQ(boat2->begin()->first, 0x300);
  EXPECT_EQ(boat2->rbegin()->first, 0x3FF);
}

TEST(OverworldGraphicsPreviewTest, RejectsUnalignedTruncatedAndOutOfRangeDma) {
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer({}, 0x2C00, 0x2000).ok());
  std::vector<uint8_t> bytes(32, 0);
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer(bytes, 0x2C01, 0x2000).ok());
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer(bytes, 0x1000, 0x2000).ok());
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer(bytes, 0x6000, 0x2000).ok());
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer(bytes, INT32_MAX, INT32_MIN).ok());
  bytes.resize(31);
  EXPECT_FALSE(DecodeGraphicsPreviewTransfer(bytes, 0x2C00, 0x2000).ok());
}

TEST(OverworldGraphicsPreviewTest,
     AreaShapesShareOverridesWithoutChangingRomOrSheets) {
  for (const auto dimensions : {std::pair{1, 1}, {2, 2}, {2, 1}, {1, 2}}) {
    SCOPED_TRACE(testing::Message()
                 << dimensions.first << "x" << dimensions.second);
    Rom rom;
    std::vector<uint8_t> data(0x200000, 0);
    data[OverworldCustomASMHasBeenApplied] = 3;
    for (int id = 0; id < kNumOverworldMaps; ++id) {
      data[kOverworldMapParentIdExpanded + id] = id;
    }
    for (int y = 0; y < dimensions.second; ++y) {
      for (int x = 0; x < dimensions.first; ++x) {
        data[kOverworldMapParentIdExpanded + 0x30 + x + y * 8] = 0x30;
      }
    }
    ASSERT_TRUE(rom.LoadFromData(data).ok());
    GameData game;
    game.graphics_buffer.resize(0x100000, 0);
    Overworld overworld(&rom);
    auto& maps =
        const_cast<std::vector<OverworldMap>&>(overworld.overworld_maps());
    maps.reserve(kNumOverworldMaps);
    for (int id = 0; id < kNumOverworldMaps; ++id)
      maps.emplace_back(id, &rom, &game);
    // Ordinary graphics can contain palette offsets even when source pixels
    // are zero. Clearing a preview must restore those exact loaded graphics.
    std::map<int, std::vector<uint8_t>> ordinary_graphics;
    for (int id : {0x30, 0x31, 0x38, 0x39, 0x32}) {
      maps[id].LoadAreaGraphics();
      ASSERT_TRUE(maps[id].BuildTileset().ok());
      ordinary_graphics[id] = maps[id].current_graphics();
    }
    PreviewTile8 pixels{};
    pixels.fill(13);
    const bool dirty = rom.dirty();
    const auto before = game.graphics_buffer;
    overworld.SetGraphicsPreview({{0x30, {{0xC0, pixels}}}});
    for (int id : {0x30, 0x31, 0x38, 0x39, 0x32}) {
      auto& map = maps[id];
      ASSERT_TRUE(map.BuildTileset().ok());
      const bool belongs = map.parent() == 0x30;
      EXPECT_EQ(map.HasGraphicsPreview(), belongs);
      EXPECT_EQ(map.current_graphics()[0xC0 / 16 * 8 * 128],
                belongs ? 13 : ordinary_graphics[id][0xC0 / 16 * 8 * 128]);
    }
    EXPECT_EQ(game.graphics_buffer, before);
    EXPECT_EQ(rom.dirty(), dirty);
    overworld.SetGraphicsPreview({});
    for (int id : {0x30, 0x31, 0x38, 0x39}) {
      EXPECT_FALSE(maps[id].HasGraphicsPreview());
      ASSERT_TRUE(maps[id].BuildTileset().ok());
      EXPECT_EQ(maps[id].current_graphics(), ordinary_graphics[id]);
    }
  }
}

class OverworldGraphicsPreviewFileTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root = std::filesystem::temp_directory_path() /
           ("yaze-preview-test-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
  }
  void TearDown() override { std::filesystem::remove_all(root); }
  void Write() { std::ofstream(root / "preview.json") << document.dump(); }
  std::filesystem::path root;
  nlohmann::json document = {{"schema_version", 1},
                             {"areas",
                              {{{"parent_map", 48},
                                {"bg_character_base_word", 8192},
                                {"transfers",
                                 {{{"format", "snes4bpp"},
                                   {"path", "boat.bin"},
                                   {"vram_word_address", 11264},
                                   {"byte_count", 32}}}}}}}};
};

TEST_F(OverworldGraphicsPreviewFileTest,
       ExplicitAssetsResolveRelativeToManifest) {
  std::ofstream asset(root / "boat.bin", std::ios::binary);
  std::vector<char> bytes(32, static_cast<char>(0xFF));
  asset.write(bytes.data(), bytes.size());
  asset.close();
  Write();
  auto result =
      core::LoadOverworldGraphicsPreview((root / "preview.json").string());
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->at(48).at(0xC0)[0], 15);
  EXPECT_TRUE(core::LoadOverworldGraphicsPreview("")->empty());
  document["areas"][0]["parent_map"] = 56;
  Write();
  result = core::LoadOverworldGraphicsPreview((root / "preview.json").string());
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(result->contains(48));
  EXPECT_TRUE(result->contains(56));
}

TEST_F(OverworldGraphicsPreviewFileTest,
       MissingMalformedAndWrongSizeFailClosed) {
  const auto path = (root / "preview.json").string();
  EXPECT_FALSE(core::LoadOverworldGraphicsPreview(path).ok());
  std::ofstream(path) << "{";
  EXPECT_FALSE(core::LoadOverworldGraphicsPreview(path).ok());
  Write();
  EXPECT_FALSE(core::LoadOverworldGraphicsPreview(path).ok());
  std::ofstream(root / "boat.bin") << "short";
  EXPECT_FALSE(core::LoadOverworldGraphicsPreview(path).ok());
}

TEST_F(OverworldGraphicsPreviewFileTest,
       NativeProjectPathSurvivesSaveAndReopen) {
  const auto path = root / "ship.yaze";
  std::ofstream(path) << "[project]\nname=Ship Preview\n[files]\n"
                         "overworld_graphics_preview=preview.json\n";
  project::YazeProject project;
  ASSERT_TRUE(project.Open(path.string()).ok());
  EXPECT_EQ(project.overworld_graphics_preview,
            (root / "preview.json").string());
  ASSERT_TRUE(project.Save().ok());
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(path.string()).ok());
  EXPECT_EQ(reopened.overworld_graphics_preview,
            project.overworld_graphics_preview);
  std::ifstream saved(path);
  std::string text((std::istreambuf_iterator<char>(saved)), {});
  EXPECT_NE(text.find("overworld_graphics_preview=preview.json"),
            std::string::npos);
  project::YazeProject ordinary;
  EXPECT_TRUE(ordinary.overworld_graphics_preview.empty());
}

TEST_F(OverworldGraphicsPreviewFileTest, DuplicateAndOverlappingTransfersFail) {
  std::ofstream asset(root / "boat.bin", std::ios::binary);
  const std::string bytes(32, '\0');
  asset.write(bytes.data(), bytes.size());
  asset.close();
  auto& transfers = document["areas"][0]["transfers"];
  transfers.push_back(transfers[0]);
  Write();
  EXPECT_FALSE(
      core::LoadOverworldGraphicsPreview((root / "preview.json").string())
          .ok());
  transfers.erase(1);
  document["areas"].push_back(document["areas"][0]);
  Write();
  EXPECT_FALSE(
      core::LoadOverworldGraphicsPreview((root / "preview.json").string())
          .ok());
}

}  // namespace
}  // namespace yaze::zelda3
