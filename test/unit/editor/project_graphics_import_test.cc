#include "app/editor/overworld/project_graphics_import.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "zelda3/game_data.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {
namespace {

std::vector<uint8_t> Glyph(uint8_t index, bool transparent_edge = false) {
  snes_tile8 tile{};
  std::fill(std::begin(tile.data), std::end(tile.data), index);
  if (transparent_edge) {
    std::fill(std::begin(tile.data) + 1, std::end(tile.data), 0);
  }
  return gfx::PackBppTile(tile, 4);
}

gfx::Tile16 Background(uint8_t palette = 7) {
  const gfx::TileInfo q(0, palette, false, false, false);
  return gfx::Tile16(q, q, q, q);
}

class ProjectGraphicsImportTest : public testing::Test {
 protected:
  void SetUp() override {
    gfx::SnesPalette main, aux;
    for (int i = 0; i < 35; ++i)
      main.AddColor(gfx::SnesColor(uint16_t{0}));
    for (int i = 0; i < 21; ++i)
      aux.AddColor(gfx::SnesColor(uint16_t{0}));
    owner_.palette_groups.overworld_main.AddPalette(main);
    owner_.palette_groups.overworld_aux.AddPalette(aux);
    context_.owner = &owner_;
    context_.characters.resize(64);
    context_.characters[0].fill(1);
    context_.palette[7 * 16 + 1] = 0x7C00;  // Exact current blue background.
    context_.definitions.assign(32, Background());
    context_.grid_width = 2;
    context_.grid_height = 1;
    context_.grid = {0, 1};
    for (int p = 1; p < 8; ++p) {
      context_.palette_addresses[5 * 16 + p] = zelda3::PaletteColorAddress{
          zelda3::PaletteAddressGroup::kOverworldMain, 0, 20 + p};
      context_.palette_addresses[5 * 16 + 8 + p] = zelda3::PaletteColorAddress{
          zelda3::PaletteAddressGroup::kOverworldAux, 0, p - 1};
      context_.palette_addresses[6 * 16 + p] = zelda3::PaletteColorAddress{
          zelda3::PaletteAddressGroup::kOverworldMain, 0, 27 + p};
    }
    ProjectImportPalette palette;
    palette.id = "timber";
    palette.colors[1] = 0x001F;
    palette.characters = Glyph(1);
    asset_.width = 1;
    asset_.height = 1;
    asset_.palettes.push_back(palette);
    asset_.cells.resize(1);
    asset_.cells[0].quadrants[0] = {false, "timber", 0, false, false};
    bindings_.resource_first_character = 16;
    bindings_.target_resource.assign(16 * 32, 0);
    bindings_.palettes.push_back({"timber", 5, 16, 8});
    bindings_.first_tile16 = 10;
    bindings_.owned_tile16_count = 8;
  }

  zelda3::GameData owner_;
  ProjectGraphicsImportContext context_;
  ProjectGraphicsImportAsset asset_;
  ProjectGraphicsImportBindings bindings_;
};

TEST_F(ProjectGraphicsImportTest, ExactOpaqueArtPlansWithoutMutatingAnyOwner) {
  auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->definitions.size(), 1);
  EXPECT_EQ(plan->stamp.at(0, 0), 10);
  const auto& q = zelda3::Tile16QuadrantInfo(plan->definitions[0].tile_data, 0);
  EXPECT_EQ(q.id_, 16);
  EXPECT_EQ(q.palette_, 5);
  EXPECT_EQ(std::vector<uint8_t>(plan->resource_after.begin(),
                                 plan->resource_after.begin() + 32),
            Glyph(1));
  ASSERT_EQ(plan->palette_changes.size(), 1);
  EXPECT_EQ(plan->palette_changes[0].group_name, "ow_main");
  EXPECT_EQ(plan->palette_changes[0].palette_index, 0);
  EXPECT_EQ(plan->palette_changes[0].color_index, 21);
  EXPECT_EQ(plan->palette_changes[0].new_color.snes(), 0x001F);
  EXPECT_EQ(owner_.palette_groups.overworld_main[0][21].snes(), 0);
  EXPECT_EQ(context_.definitions[10], Background());
  EXPECT_TRUE(std::all_of(bindings_.target_resource.begin(),
                          bindings_.target_resource.end(),
                          [](uint8_t b) { return b == 0; }));
}

TEST_F(ProjectGraphicsImportTest, FullyTransparentCellsRetainTheirCurrentIds) {
  asset_.width = 2;
  asset_.cells.resize(2);
  auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->stamp.at(1, 0), 1);
  ASSERT_EQ(plan->definitions.size(), 1);
  EXPECT_EQ(plan->definitions[0].tile_id, 10);
}

TEST_F(ProjectGraphicsImportTest,
       AlphaComposesExactResolvedBackgroundInFreeColor) {
  asset_.palettes[0].characters = Glyph(1, true);
  auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->definitions.size(), 1);
  const auto& q = zelda3::Tile16QuadrantInfo(plan->definitions[0].tile_data, 0);
  const auto tile =
      gfx::UnpackBppTile(plan->resource_after,
                         (q.id_ - bindings_.resource_first_character) * 32, 4);
  EXPECT_EQ(tile.data[0], 1);
  EXPECT_TRUE(std::all_of(std::begin(tile.data) + 1, std::end(tile.data),
                          [](uint8_t p) { return p == 2; }));
  const auto blue =
      std::find_if(plan->palette_changes.begin(), plan->palette_changes.end(),
                   [](const auto& change) { return change.color_index == 22; });
  ASSERT_NE(blue, plan->palette_changes.end());
  EXPECT_EQ(blue->new_color.snes(), 0x7C00);
}

TEST_F(ProjectGraphicsImportTest, AlphaRefusesInsteadOfQuantizingFullPalette) {
  asset_.palettes[0].characters = Glyph(1, true);
  for (int p = 1; p < 16; ++p) {
    if (p != 8)
      asset_.palettes[0].colors[p] = 0x0100 + p;
  }
  const auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(plan.status().message()).find("background color"),
            std::string::npos);
  EXPECT_EQ(owner_.palette_groups.overworld_main[0][21].snes(), 0);
}

TEST_F(ProjectGraphicsImportTest, LowAndHighHalvesUseDistinctResolvedRecords) {
  snes_tile8 tile{};
  std::fill(std::begin(tile.data), std::end(tile.data), 1);
  tile.data[0] = 9;
  asset_.palettes[0].characters = gfx::PackBppTile(tile, 4);
  asset_.palettes[0].colors[9] = 0x03E0;
  auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_EQ(plan->palette_changes.size(), 2);
  const auto high = std::find_if(
      plan->palette_changes.begin(), plan->palette_changes.end(),
      [](const auto& change) { return change.group_name == "ow_aux"; });
  ASSERT_NE(high, plan->palette_changes.end());
  EXPECT_EQ(high->palette_index, 0);
  EXPECT_EQ(high->color_index, 0);
  EXPECT_EQ(high->new_color.snes(), 0x03E0);
}

TEST_F(ProjectGraphicsImportTest, ConflictingAliasRefusesBeforeMutation) {
  asset_.palettes[0].colors[9] = 0x03E0;
  context_.palette_addresses[5 * 16 + 9] =
      context_.palette_addresses[5 * 16 + 1];
  const auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(owner_.palette_groups.overworld_main[0][21].snes(), 0);
}

TEST_F(ProjectGraphicsImportTest, ExistingPlacedPaletteConsumerIsProtected) {
  context_.definitions[1] = Background(5);
  const auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(std::string(plan.status().message()).find("existing"),
            std::string::npos);
}

TEST_F(ProjectGraphicsImportTest, OverlappingOwnedCharacterRangesRefuse) {
  auto second = asset_.palettes[0];
  second.id = "leaves";
  asset_.palettes.push_back(second);
  bindings_.palettes.push_back({"leaves", 6, 20, 8});
  const auto plan = PlanProjectGraphicsImport(asset_, bindings_, context_);
  EXPECT_EQ(plan.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST(ProjectGraphicsImportLoaderTest,
     HashMismatchAndEscapingResourceAreRefused) {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("yaze_project_graphics_import_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } cleanup{directory};
  const auto glyph = Glyph(1);
  std::ofstream(directory / "glyph.bin", std::ios::binary)
      .write(reinterpret_cast<const char*>(glyph.data()), glyph.size());
  nlohmann::json catalog = {{"schema_version", 2},
                            {"profiles", nlohmann::json::object()},
                            {"bindings", nlohmann::json::array()}};
  catalog["resources"]["art"] = {{"format", "snes4bpp"},
                                 {"byte_count", 32},
                                 {"path", "glyph.bin"},
                                 {"sha256", std::string(64, '0')}};
  std::ofstream(directory / "catalog.json") << catalog.dump();
  std::ofstream(directory / "logical.json")
      << nlohmann::json({{"catalog", "catalog.json"}}).dump();
  auto loaded = LoadProjectGraphicsImport(directory / "logical.json");
  EXPECT_EQ(loaded.status().code(), absl::StatusCode::kDataLoss);
  catalog["resources"]["art"]["sha256"] =
      core::ComputeSourceArtifactSha256(std::string_view(
          reinterpret_cast<const char*>(glyph.data()), glyph.size()));
  std::ofstream(directory / "catalog.json") << catalog.dump();
  auto words = nlohmann::json::array();
  for (int i = 0; i < 16; ++i)
    words.push_back(nullptr);
  words[1] = 0x001F;
  nlohmann::json logical = {
      {"catalog", "catalog.json"},
      {"tile16_grid_size", {1, 1}},
      {"resources",
       {{{"logical_palette", "timber"},
         {"resource_id", "art"},
         {"characters", 1},
         {"bytes", 32}}}},
      {"palette_groups",
       {{{"logical_palette", "timber"}, {"BGR555_words", words}}}},
      {"tile16_cells",
       {{{"x", 0},
         {"y", 0},
         {"quadrants",
          {{{"logical_palette", "timber"},
            {"logical_character", 0},
            {"hflip", false},
            {"vflip", false}},
           {{"transparent", true}},
           {{"transparent", true}},
           {{"transparent", true}}}}}}}};
  std::ofstream(directory / "logical.json") << logical.dump();
  loaded = LoadProjectGraphicsImport(directory / "logical.json");
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->palettes.size(), 1);
  EXPECT_EQ(loaded->palettes[0].colors[1], uint16_t{0x001F});
  EXPECT_EQ(loaded->palettes[0].characters, glyph);
  catalog["resources"]["art"]["path"] = "../glyph.bin";
  std::ofstream(directory / "catalog.json") << catalog.dump();
  loaded = LoadProjectGraphicsImport(directory / "logical.json");
  EXPECT_FALSE(loaded.ok());
}

}  // namespace
}  // namespace yaze::editor
