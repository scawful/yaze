#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/cleanup/cleanup.h"
#include "app/service/render_service.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::cli {
namespace {

TEST(LyraVerifyProjectCommandTest, RequiresLoadedRomAndProjectContext) {
  handlers::LyraVerifyProjectCommandHandler command;
  resources::ArgumentParser args({});
  resources::OutputFormatter out(resources::OutputFormatter::Format::kJson);
  EXPECT_TRUE(absl::IsFailedPrecondition(command.Execute(nullptr, args, out)));
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  const auto before = rom.vector();
  auto status = command.Execute(&rom, args, out);
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(status.message().find("--project-context"), std::string::npos);
  EXPECT_EQ(rom.vector(), before);
}

TEST(LyraVerifyProjectCommandTest,
     RequiresSavedManagedTerrainBeforeLoadingAssets) {
  handlers::LyraVerifyProjectCommandHandler command;
  resources::ArgumentParser args({});
  resources::OutputFormatter out(resources::OutputFormatter::Format::kJson);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  const auto before = rom.vector();
  project::YazeProject project;
  project.name = "Native build inputs";
  project.filepath = "/nonexistent/lyra-project/Game.yaze";
  command.SetProjectContext(&project);
  auto status = command.Execute(&rom, args, out);
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(status.message().find("before building"), std::string::npos);
  project.lyra_terrain_file = "Game.lyra-terrain.json";
  status = command.Execute(&rom, args, out);
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
  EXPECT_NE(status.message().find("missing"), std::string::npos);
  EXPECT_EQ(rom.vector(), before);
}

std::filesystem::path UniqueDir(const std::string& tag) {
  const auto stamp =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("yaze_ow_agent_" + tag + "_" + std::to_string(stamp));
}

TEST(LyraVerifyProjectCommandTest,
     VerifiesNativeBindingsAndScopedRelocatedDefinitionsWithoutWrites) {
  const auto fixture = test::FindOracleRomFixture();
  if (fixture.empty())
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to the main-project Oracle ROM";
  const auto root = UniqueDir("lyra_verify");
  std::filesystem::create_directories(root);
  const auto previous_addresses = core::RomSettings::Get().address_overrides();
  const auto previous_flags = core::FeatureFlags::get();
  absl::Cleanup cleanup = [&]() {
    core::RomSettings::Get().SetAddressOverrides(previous_addresses);
    core::FeatureFlags::get() = previous_flags;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  };
  core::RomSettings::Get().ClearOverrides();
  core::FeatureFlags::get().overworld.kLoadCustomOverworld = true;
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(fixture).ok());
  const auto layout = zelda3::DetectOverworldRomProfile(rom);
  ASSERT_TRUE(layout.has_expanded_tile32);
  const auto original_tr = zelda3::GetMap32TileTRExpanded();
  const auto table_bytes = zelda3::Map32StorageBytesForProfile(layout);
  auto image = rom.vector();
  ASSERT_LE(static_cast<size_t>(original_tr + table_bytes), image.size());
  const size_t relocated_tr = (image.size() + 0x7FFF) & ~size_t{0x7FFF};
  image.resize(relocated_tr + table_bytes);
  std::copy_n(image.begin() + original_tr, table_bytes,
              image.begin() + relocated_tr);
  // The first definition's TR quadrant differs only in the project-owned table.
  image[relocated_tr] ^= 1;
  ASSERT_TRUE(rom.LoadFromData(image).ok());
  project::YazeProject project;
  project.name = "Lyra native verification";
  project.filepath = (root / "Game.yaze").string();
  project.lyra_terrain_file = (root / "Game.lyra-terrain.json").string();
  project.rom_address_overrides
      .addresses[core::RomAddressKey::kOverworldMap32TrExpanded] =
      static_cast<uint32_t>(relocated_tr);
  core::RomSettings::Get().SetAddressOverrides(project.rom_address_overrides);
  zelda3::GameData data;
  zelda3::LoadOptions options;
  options.expand_rom = false;
  ASSERT_TRUE(zelda3::LoadGameData(rom, data, options).ok());
  auto source = std::make_unique<zelda3::Overworld>(&rom, &data);
  ASSERT_TRUE(source->Load(&rom).ok());
  core::LyraTerrainProfile profile;
  ASSERT_TRUE(zelda3::BindLyraTerrainSource(*source, &profile).ok());
  auto document = core::LyraTerrainDocument::Create(profile);
  ASSERT_TRUE(document.ok()) << document.status();
  auto authoring = document->authoring();
  authoring.maps[0].properties.message_id = 0x42;
  for (int state = 0; state < 3; ++state) {
    const auto assets = zelda3::ResolveLyraMapAssets(
        *source, authoring.maps[0].properties, state);
    ASSERT_TRUE(assets.ok()) << assets.status();
    authoring.maps[0].asset_bindings[state] = assets->binding;
  }
  ASSERT_TRUE(document->SetAuthoring(document->authoring(), authoring).ok());
  const auto terrain_bytes = document->Serialize();
  std::ofstream(project.lyra_terrain_file) << terrain_bytes;

  core::RomAddressOverrides unrelated;
  unrelated.addresses[core::RomAddressKey::kOverworldMap32TrExpanded] =
      rom.size() + 0x20000;
  core::RomSettings::Get().SetAddressOverrides(unrelated);
  const auto before = rom.vector();
  handlers::LyraVerifyProjectCommandHandler command;
  command.SetProjectContext(&project);
  resources::ArgumentParser args({});
  resources::OutputFormatter out(resources::OutputFormatter::Format::kJson);
  out.BeginObject();
  const auto status = command.Execute(&rom, args, out);
  ASSERT_TRUE(status.ok()) << status;
  out.EndObject();
  const auto verified = nlohmann::json::parse(out.GetOutput());
  EXPECT_EQ(verified["map_count"], 32);
  EXPECT_EQ(verified["profile"]["tile16_definition_fingerprint"],
            profile.tile16_definition_fingerprint);
  EXPECT_EQ(verified["verified_asset_bindings"][0],
            nlohmann::json(authoring.maps[0].asset_bindings));
  EXPECT_EQ(verified["verified_asset_bindings"][1],
            nlohmann::json::array({"", "", ""}));
  const auto definitions = source->tiles32_unique();
  EXPECT_EQ(verified["native_tile32_capacity"], definitions.size());
  EXPECT_EQ(verified["native_definitions"][0][1], definitions[0].tile1_);
  EXPECT_EQ(core::RomSettings::Get().address_overrides().addresses,
            unrelated.addresses);
  EXPECT_EQ(rom.vector(), before);
  std::ifstream saved(project.lyra_terrain_file);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(saved), {}),
            terrain_bytes);

  auto changed = document->authoring();
  auto& stale = changed.maps[0].asset_bindings[1];
  stale[0] = stale[0] == '0' ? '1' : '0';
  ASSERT_TRUE(document->SetAuthoring(document->authoring(), changed).ok());
  saved.close();
  std::ofstream(project.lyra_terrain_file) << document->Serialize();
  resources::OutputFormatter rejected(
      resources::OutputFormatter::Format::kJson);
  const auto stale_status = command.Execute(&rom, args, rejected);
  EXPECT_TRUE(absl::IsFailedPrecondition(stale_status));
  EXPECT_NE(stale_status.message().find("state 1 native asset binding changed"),
            std::string::npos);
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(core::RomSettings::Get().address_overrides().addresses,
            unrelated.addresses);

  // LoadGameData's normal expansion must stay disabled on a failing input too.
  auto small_image = before;
  small_image.resize(0x100000);
  Rom small;
  ASSERT_TRUE(small.LoadFromData(small_image).ok());
  const auto small_before = small.vector();
  resources::OutputFormatter failed(resources::OutputFormatter::Format::kJson);
  EXPECT_FALSE(command.Execute(&small, args, failed).ok());
  EXPECT_EQ(small.vector(), small_before);
  EXPECT_EQ(core::RomSettings::Get().address_overrides().addresses,
            unrelated.addresses);
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
  // Resolve the fixture's parent: the relocated wide ship uses $38/$39,
  // while older Oracle fixtures used the large $30 area.
  plain.screen_id = 0x39;
  auto without = service.RenderOverworldArea(plain);
  ASSERT_TRUE(without.ok()) << without.status();
  const int parent = without->parent_screen;
  ASSERT_NE(parent, plain.screen_id);
  ASSERT_NE(parent, 0x00);
  auto previewed = plain;
  previewed.graphics_preview = {{parent, tiles}};
  auto with = service.RenderOverworldArea(previewed);
  ASSERT_TRUE(with.ok()) << with.status();
  ASSERT_EQ(with->parent_screen, parent);
  EXPECT_FALSE(without->graphics_preview);
  EXPECT_TRUE(with->graphics_preview);
  EXPECT_NE(with->png_data, without->png_data);

  // Areas outside the manifest render exactly as before.
  plain.screen_id = previewed.screen_id = 0x00;
  without = service.RenderOverworldArea(plain);
  with = service.RenderOverworldArea(previewed);
  ASSERT_TRUE(without.ok()) << without.status();
  ASSERT_TRUE(with.ok()) << with.status();
  ASSERT_NE(with->parent_screen, parent);
  EXPECT_FALSE(with->graphics_preview);
  EXPECT_EQ(with->png_data, without->png_data);
}

}  // namespace
}  // namespace yaze::cli
