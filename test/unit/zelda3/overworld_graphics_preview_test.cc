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

class OverworldGraphicsCatalogTest : public OverworldGraphicsPreviewFileTest {
 protected:
  void SetUp() override {
    OverworldGraphicsPreviewFileTest::SetUp();
    // Two distinct characters in one resource. Profiles can use the complete
    // pack or aligned slices, without maintaining separate character files.
    bytes.assign(64, '\0');
    bytes[0] = static_cast<char>(0x80);
    bytes[32 + 16] = static_cast<char>(0x80);
    std::ofstream(root / "packed.bin", std::ios::binary)
        .write(bytes.data(), bytes.size());
    document = {{"schema_version", 2}};
    document["resources"]["fort.characters"] = {
        {"path", "packed.bin"},
        {"format", "snes4bpp"},
        {"byte_count", bytes.size()},
        {"sha256", core::ComputeSourceArtifactSha256(bytes)}};
    document["profiles"]["fort"] = {{"bg_character_base_word", 0x2000},
                                    {"transfers",
                                     {{{"resource_id", "fort.characters"},
                                       {"source_offset", 32},
                                       {"byte_count", 32},
                                       {"vram_word_address", 0x2C00}},
                                      {{"resource_id", "fort.characters"},
                                       {"byte_count", 32},
                                       {"vram_word_address", 0x2C10}}}}};
    document["bindings"] = nlohmann::json::array(
        {{{"parent_maps", {0x30, 0x32}}, {"profile_id", "fort"}},
         {{"parent_maps", {0x38}}, {"profile_id", "fort"}}});
  }
  absl::StatusOr<OverworldGraphicsPreview> Load() {
    Write();
    return core::LoadOverworldGraphicsPreview((root / "preview.json").string());
  }
  std::string bytes;
};

TEST_F(OverworldGraphicsCatalogTest,
       SharedProfilesAndPackedSlicesMatchVersionOnePixels) {
  auto catalog = Load();
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  ASSERT_EQ(catalog->size(), 3u);
  EXPECT_EQ(catalog->at(0x30), catalog->at(0x32));
  EXPECT_EQ(catalog->at(0x30), catalog->at(0x38));
  EXPECT_EQ(catalog->at(0x30).at(0xC0)[0], 4);
  EXPECT_EQ(catalog->at(0x30).at(0xC1)[0], 1);
  // Version1 remains the same resolved API and can describe identical pixels.
  std::ofstream(root / "first.bin", std::ios::binary)
      .write(bytes.data() + 32, 32);
  std::ofstream(root / "second.bin", std::ios::binary).write(bytes.data(), 32);
  document = {{"schema_version", 1}, {"areas", nlohmann::json::array()}};
  for (int parent : {0x30, 0x32, 0x38}) {
    document["areas"].push_back({{"parent_map", parent},
                                 {"bg_character_base_word", 0x2000},
                                 {"transfers",
                                  {{{"path", "first.bin"},
                                    {"format", "snes4bpp"},
                                    {"byte_count", 32},
                                    {"vram_word_address", 0x2C00}},
                                   {{"path", "second.bin"},
                                    {"format", "snes4bpp"},
                                    {"byte_count", 32},
                                    {"vram_word_address", 0x2C10}}}}});
  }
  auto legacy = Load();
  ASSERT_TRUE(legacy.ok()) << legacy.status();
  EXPECT_EQ(*legacy, *catalog);
}

TEST_F(OverworldGraphicsCatalogTest, ResourceIdentityAndSizesFailClosed) {
  const auto valid = document;
  auto& resource = document["resources"]["fort.characters"];
  for (const auto& bad_hash : std::vector<std::string>{
           "short", std::string(64, 'g'), std::string(64, '0')}) {
    resource["sha256"] = bad_hash;
    EXPECT_FALSE(Load().ok()) << bad_hash;
  }
  document = valid;
  document["resources"]["fort.characters"]["byte_count"] = 32;
  EXPECT_FALSE(Load().ok());
  document = valid;
  document["resources"]["fort.characters"]["format"] = "snes3bpp";
  EXPECT_FALSE(Load().ok());
  document = valid;
  document["resources"]["fort.characters"]["path"] = "missing.bin";
  EXPECT_FALSE(Load().ok());
  document = valid;
  bytes[1] = static_cast<char>(0xFF);  // Same byte count, stale identity.
  std::ofstream(root / "packed.bin", std::ios::binary)
      .write(bytes.data(), bytes.size());
  EXPECT_FALSE(Load().ok());
}

TEST_F(OverworldGraphicsCatalogTest, RejectsInvalidSlicesAndVramTargets) {
  const auto valid = document;
  for (const auto& [field, value] :
       std::vector<std::pair<std::string, nlohmann::json>>{
           {"source_offset", -32},
           {"source_offset", 1},
           {"source_offset", 64},
           {"source_offset", 2147483648ULL},
           {"source_offset", 0.0},
           {"byte_count", 0},
           {"byte_count", 31},
           {"byte_count", 64},
           {"vram_word_address", 0x2C01},
           {"vram_word_address", 0x1000},
           {"vram_word_address", 0x6000}}) {
    document = valid;
    document["profiles"]["fort"]["transfers"][0][field] = value;
    EXPECT_FALSE(Load().ok()) << field << "=" << value;
  }
}

TEST_F(OverworldGraphicsCatalogTest,
       RejectsUnknownReferencesAndDuplicateParents) {
  const auto valid = document;
  document["profiles"]["fort"]["transfers"][0]["resource_id"] = "unknown";
  EXPECT_FALSE(Load().ok());
  document = valid;
  document["bindings"][0]["profile_id"] = "unknown";
  EXPECT_FALSE(Load().ok());
  for (auto parents :
       {nlohmann::json::array({0x30, 0x30}), nlohmann::json::array({0x38}),
        nlohmann::json::array({160}), nlohmann::json::array({-1}),
        nlohmann::json::array({48.0}), nlohmann::json::array()}) {
    document = valid;
    document["bindings"][0]["parent_maps"] = parents;
    EXPECT_FALSE(Load().ok()) << parents;
  }
}

TEST_F(OverworldGraphicsCatalogTest,
       RejectsOverlappingTransfersAndDuplicateKeys) {
  document["profiles"]["fort"]["transfers"][1]["vram_word_address"] = 0x2C00;
  EXPECT_FALSE(Load().ok());
  const auto path = (root / "preview.json").string();
  std::ofstream(path)
      << R"({"schema_version":2,"resources":{"same":{},"same":{}},
    "profiles":{},"bindings":[]})";
  auto duplicate = core::LoadOverworldGraphicsPreview(path);
  EXPECT_FALSE(duplicate.ok());
  EXPECT_NE(duplicate.status().message().find("Duplicate"), std::string::npos);
  std::ofstream(path) << R"({"schema_version":2,"resources":{},
    "profiles":{"same":{},"same":{}},"bindings":[]})";
  EXPECT_FALSE(core::LoadOverworldGraphicsPreview(path).ok());
}

TEST_F(OverworldGraphicsCatalogTest, ExistingProjectBindingSurvivesRelocation) {
  ASSERT_TRUE(Load().ok());
  const auto path = root / "unified.yaze";
  std::ofstream(path) << "[project]\nname=Unified\n[files]\n"
                         "overworld_graphics_preview=preview.json\n";
  project::YazeProject project;
  ASSERT_TRUE(project.Open(path.string()).ok());
  ASSERT_TRUE(project.Save().ok());
  const auto moved = root / "moved";
  std::filesystem::create_directory(moved);
  for (const auto& name : {"unified.yaze", "preview.json", "packed.bin"}) {
    std::filesystem::copy_file(root / name, moved / name);
  }
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open((moved / "unified.yaze").string()).ok());
  auto result =
      core::LoadOverworldGraphicsPreview(reopened.overworld_graphics_preview);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->at(0x30).at(0xC0)[0], 4);
  EXPECT_EQ(reopened.overworld_graphics_preview,
            (moved / "preview.json").string());
}

}  // namespace
}  // namespace yaze::zelda3
