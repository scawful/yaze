#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "app/service/render_service.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "core/project.h"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"

namespace yaze::cli {
namespace {

std::filesystem::path UniqueDir(const std::string& tag) {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_ow_agent_" + tag + "_" + std::to_string(stamp));
}

TEST(OverworldSpriteEditGuardTest, DetectsRomInsideOracleCheckout) {
  const auto root = UniqueDir("guard");
  std::filesystem::create_directories(root / "checkout" / "Roms");
  std::filesystem::create_directories(root / "copy" / "Roms");
  std::ofstream(root / "checkout" / "Oracle_main.asm") << "; main\n";
  std::ofstream(root / "checkout" / "Roms" / "oos168.sfc") << "rom";
  std::ofstream(root / "copy" / "Roms" / "oos168.sfc") << "rom";

  EXPECT_TRUE(handlers::IsOracleProjectRomPath(root / "checkout" / "Roms" /
                                               "oos168.sfc"));
  // Relative-looking paths with .. still resolve to the checkout.
  EXPECT_TRUE(handlers::IsOracleProjectRomPath(
      root / "copy" / ".." / "checkout" / "Roms" / "oos168.sfc"));
  EXPECT_FALSE(
      handlers::IsOracleProjectRomPath(root / "copy" / "Roms" / "oos168.sfc"));
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

TEST(OverworldRenderServiceTest, RejectsBadRequestsBeforeLoading) {
  app::service::RenderService service(nullptr, nullptr);
  app::service::OverworldRenderRequest request;
  request.scale = 0.0f;
  EXPECT_TRUE(
      absl::IsInvalidArgument(service.RenderOverworldArea(request).status()));
  request.scale = 1.0f;
  EXPECT_TRUE(absl::IsFailedPrecondition(
      service.RenderOverworldArea(request).status()));
}

// A project whose manifest DMAs one 4bpp tile to VRAM word 0x2C00 (BG
// character 0xC0) in the area keyed by parent_map, like Oracle's Dragon Ship.
class OverworldRenderGraphicsPreviewTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = UniqueDir("preview");
    std::filesystem::create_directories(root_ / "Data" / "gfx");
    std::vector<char> tile(32, 0);
    // Row 0 sets all four bitplanes: pixel value 15 across the row.
    tile[0] = tile[1] = tile[16] = tile[17] = static_cast<char>(0xFF);
    std::ofstream(root_ / "Data" / "gfx" / "boat.bin", std::ios::binary)
        .write(tile.data(), static_cast<std::streamsize>(tile.size()));
    WriteManifest(0x30);
    project_.filepath = (root_ / "Game.yaze").string();
    project_.overworld_graphics_preview = "Data/preview.json";
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  void WriteManifest(int parent_map) {
    const nlohmann::json document = {{"schema_version", 1},
                                     {"areas",
                                      {{{"parent_map", parent_map},
                                        {"bg_character_base_word", 0x2000},
                                        {"transfers",
                                         {{{"path", "gfx/boat.bin"},
                                           {"format", "snes4bpp"},
                                           {"byte_count", 32},
                                           {"vram_word_address", 0x2C00}}}}}}}};
    std::ofstream(root_ / "Data" / "preview.json") << document.dump();
  }
  std::string Manifest() const {
    return (root_ / "Data" / "preview.json").lexically_normal().string();
  }
  std::filesystem::path root_;
  project::YazeProject project_;
};

TEST_F(OverworldRenderGraphicsPreviewTest, ProjectManifestLoadsByDefault) {
  for (const auto& flag :
       {std::optional<std::string>(), std::optional<std::string>("on")}) {
    auto resolved =
        handlers::ResolveOverworldRenderGraphicsPreview(&project_, flag);
    ASSERT_TRUE(resolved.ok()) << resolved.status();
    EXPECT_EQ(resolved->mode, "project");
    EXPECT_EQ(resolved->manifest, Manifest());
    ASSERT_TRUE(resolved->preview.contains(0x30));
    const auto& tiles = resolved->preview.at(0x30);
    ASSERT_EQ(tiles.size(), 1u);
    EXPECT_EQ(tiles.at(0xC0)[0], 15);
    EXPECT_EQ(tiles.at(0xC0)[8], 0);
  }
}

TEST_F(OverworldRenderGraphicsPreviewTest, AreaKeyIsManifestData) {
  // Moving the ship to a wide map at $38/$39 only edits the manifest.
  WriteManifest(0x38);
  auto resolved =
      handlers::ResolveOverworldRenderGraphicsPreview(&project_, std::nullopt);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_FALSE(resolved->preview.contains(0x30));
  EXPECT_TRUE(resolved->preview.contains(0x38));
}

TEST_F(OverworldRenderGraphicsPreviewTest, OffSkipsTheManifestEntirely) {
  project_.overworld_graphics_preview = "Data/missing.json";
  auto resolved = handlers::ResolveOverworldRenderGraphicsPreview(
      &project_, std::string("off"));
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->mode, "off");
  EXPECT_TRUE(resolved->manifest.empty());
  EXPECT_TRUE(resolved->preview.empty());
}

TEST_F(OverworldRenderGraphicsPreviewTest, ExplicitManifestNeedsNoProject) {
  auto resolved =
      handlers::ResolveOverworldRenderGraphicsPreview(nullptr, Manifest());
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_EQ(resolved->mode, "file");
  EXPECT_TRUE(resolved->preview.contains(0x30));
}

TEST_F(OverworldRenderGraphicsPreviewTest, MissingConfigurationFailsOnRequest) {
  auto none =
      handlers::ResolveOverworldRenderGraphicsPreview(nullptr, std::nullopt);
  ASSERT_TRUE(none.ok()) << none.status();
  EXPECT_EQ(none->mode, "none");
  EXPECT_TRUE(none->preview.empty());

  EXPECT_TRUE(absl::IsFailedPrecondition(
      handlers::ResolveOverworldRenderGraphicsPreview(nullptr,
                                                      std::string("on"))
          .status()));
  project::YazeProject unconfigured;
  unconfigured.filepath = project_.filepath;
  EXPECT_TRUE(absl::IsFailedPrecondition(
      handlers::ResolveOverworldRenderGraphicsPreview(&unconfigured,
                                                      std::string("on"))
          .status()));

  project_.overworld_graphics_preview = "Data/missing.json";
  auto broken =
      handlers::ResolveOverworldRenderGraphicsPreview(&project_, std::nullopt);
  ASSERT_FALSE(broken.ok());
  EXPECT_NE(broken.status().message().find("missing.json"), std::string::npos);
}

TEST(OverworldRenderServiceTest, GraphicsPreviewRedrawsOnlyItsArea) {
  const auto rom_path = test::FindOracleRomFixture();
  if (rom_path.empty()) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of an Oracle ROM";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  zelda3::GameData game_data;
  ASSERT_TRUE(zelda3::LoadGameData(rom, game_data).ok());
  app::service::RenderService service(&rom, &game_data);

  zelda3::PreviewTile8 pixels{};
  pixels.fill(13);
  zelda3::AreaGraphicsPreview tiles;
  for (uint16_t id = 0xC0; id < 0x1C0; ++id) {
    tiles.emplace(id, pixels);
  }
  app::service::OverworldRenderRequest plain;
  plain.area_overlay = false;
  auto previewed = plain;
  previewed.graphics_preview = {{0x30, tiles}};

  // A child screen resolves to parent $30 and takes the preview.
  plain.screen_id = previewed.screen_id = 0x39;
  auto without = service.RenderOverworldArea(plain);
  auto with = service.RenderOverworldArea(previewed);
  ASSERT_TRUE(without.ok()) << without.status();
  ASSERT_TRUE(with.ok()) << with.status();
  ASSERT_EQ(with->parent_screen, 0x30);
  EXPECT_FALSE(without->graphics_preview);
  EXPECT_TRUE(with->graphics_preview);
  EXPECT_NE(with->png_data, without->png_data);

  // Areas outside the manifest render exactly as before.
  plain.screen_id = previewed.screen_id = 0x00;
  without = service.RenderOverworldArea(plain);
  with = service.RenderOverworldArea(previewed);
  ASSERT_TRUE(without.ok()) << without.status();
  ASSERT_TRUE(with.ok()) << with.status();
  ASSERT_NE(with->parent_screen, 0x30);
  EXPECT_FALSE(with->graphics_preview);
  EXPECT_EQ(with->png_data, without->png_data);
}

}  // namespace
}  // namespace yaze::cli
