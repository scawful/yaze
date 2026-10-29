#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "absl/cleanup/cleanup.h"
#include "app/service/render_service.h"
#include "cli/handlers/game/overworld_render_commands.h"
#include "cli/handlers/game/overworld_sprite_edit_commands.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/lyra_terrain_source.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::cli {
namespace {

template <size_t N>
std::string WordFingerprint(const std::array<uint16_t, N>& words) {
  std::string bytes;
  bytes.reserve(N * 2);
  for (const auto word : words) {
    bytes.push_back(static_cast<char>(word));
    bytes.push_back(static_cast<char>(word >> 8));
  }
  return core::ComputeSourceArtifactSha256(bytes);
}

std::string ByteFingerprint(const std::vector<uint8_t>& bytes, size_t offset,
                            size_t count) {
  return core::ComputeSourceArtifactSha256(
      {reinterpret_cast<const char*>(bytes.data() + offset), count});
}

TEST(LyraNativeAssetDescriptionTest, RejectsIncompleteSourceWithoutRom) {
  core::LyraMapProperties properties;
  zelda3::LyraNativeMapAssets missing{zelda3::OverworldMap{}, ""};
  for (const int state : {-1, 0, 3}) {
    EXPECT_FALSE(
        zelda3::DescribeLyraNativeMapAssets(properties, state, missing).ok())
        << "state=" << state;
  }
}

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
  const auto source_status = source->Load(&rom);
  ASSERT_TRUE(source_status.ok()) << source_status;
  core::LyraTerrainProfile profile;
  ASSERT_TRUE(zelda3::BindLyraTerrainSource(*source, &profile).ok());
  auto document = core::LyraTerrainDocument::Create(profile);
  ASSERT_TRUE(document.ok()) << document.status();
  auto authoring = document->authoring();
  authoring.maps[0].properties.message_id = 0x42;
  authoring.maps[0].properties.graphics_sheets[3] = 0;
  // Keep the static lower half distinct from the animated sheet so the
  // descriptor cannot label the entire composite slot as animation bytes.
  authoring.maps[0].properties.graphics_sheets[7] = 0x3E;
  std::array<uint16_t, 128> background;
  for (size_t i = 0; i < background.size(); ++i)
    background[i] = static_cast<uint16_t>((i * 47 + 0x123) & 0x7FFF);
  authoring.maps[0].properties.background_palette = background;
  auto native_properties = authoring.maps[0].properties;
  native_properties.background_palette.reset();
  std::array<std::array<uint16_t, 256>, 3> expected_palettes;
  for (int state = 0; state < 3; ++state) {
    const auto native =
        zelda3::ResolveLyraMapAssets(*source, native_properties, state);
    ASSERT_TRUE(native.ok()) << native.status();
    const auto assets = zelda3::ResolveLyraMapAssets(
        *source, authoring.maps[0].properties, state);
    ASSERT_TRUE(assets.ok()) << assets.status();
    ASSERT_EQ(assets->source.current_palette().size(), 256);
    ASSERT_EQ(native->source.current_palette().size(), 256);
    for (size_t color = 0; color < 256; ++color) {
      expected_palettes[state][color] =
          color < 128 ? background[color]
                      : native->source.current_palette()[color].snes();
      EXPECT_EQ(assets->source.current_palette()[color].snes(),
                expected_palettes[state][color]);
    }
    const auto description = zelda3::DescribeLyraNativeMapAssets(
        authoring.maps[0].properties, state, *assets);
    ASSERT_TRUE(description.ok()) << description.status();
    const auto described = nlohmann::json::parse(*description);
    EXPECT_EQ(described["palette"]["words"],
              nlohmann::json(expected_palettes[state]));
    EXPECT_EQ(described["palette"]["uint16le_sha256"],
              WordFingerprint(expected_palettes[state]));
    EXPECT_EQ(described["palette"]["background_override_words"],
              nlohmann::json(background));
    EXPECT_EQ(described["palette"]["background_override_uint16le_sha256"],
              WordFingerprint(background));
    EXPECT_EQ(described["palette"]["runtime_consumer"], "unsupported");
    EXPECT_EQ(described["effective_sprite_graphics"],
              assets->source.sprite_graphics(state));
    EXPECT_EQ(described["effective_sprite_palette"],
              assets->source.sprite_palette(state));
    const auto& pixels = assets->source.current_graphics();
    ASSERT_EQ(pixels.size(), 0x10000);
    EXPECT_EQ(described["graphics"]["sha256"],
              ByteFingerprint(pixels, 0, pixels.size()));
    ASSERT_EQ(described["slots"].size(), 16);
    for (int slot = 0; slot < 16; ++slot) {
      const auto& row = described["slots"][slot];
      const auto sheet = row["sheet_id"].get<uint16_t>();
      EXPECT_EQ(row["slot"], slot);
      EXPECT_EQ(sheet, slot == 3 ? 0 : assets->source.static_graphics(slot));
      EXPECT_EQ(row["loader_sheet_id"], assets->source.static_graphics(slot));
      EXPECT_EQ(row["decoded_byte_count"], 4096);
      EXPECT_EQ(row["decoded_format"], "indexed8");
      EXPECT_EQ(row["decoded_sheet_sha256"],
                ByteFingerprint(data.graphics_buffer, sheet * 0x1000, 0x1000));
      EXPECT_EQ(row["resolved_offset"], slot * 0x1000);
      EXPECT_EQ(row["resolved_byte_count"], 4096);
      EXPECT_EQ(row["resolved_slot_sha256"],
                ByteFingerprint(pixels, slot * 0x1000, 0x1000));
      EXPECT_EQ(row["composite"], slot == 7);
    }
    const auto& animation = described["animation"];
    const auto animated = assets->source.animated_sheet();
    EXPECT_EQ(animation["sheet_id"], animated);
    EXPECT_EQ(animation["decoded_sheet_sha256"],
              ByteFingerprint(data.graphics_buffer, animated * 0x1000, 0x1000));
    EXPECT_EQ(animation["frame"], 0);
    EXPECT_EQ(animation["source_offset"], 0);
    EXPECT_EQ(animation["destination_offset"], 0x7000);
    EXPECT_EQ(animation["byte_count"], 0x800);
    EXPECT_EQ(animation["frame0_sha256"],
              ByteFingerprint(data.graphics_buffer, animated * 0x1000, 0x800));
    EXPECT_EQ(animation["composite_slot7_sha256"],
              ByteFingerprint(pixels, 0x7000, 0x1000));
    EXPECT_EQ(animation["static_lower_sheet_id"], 0x3E);
    EXPECT_EQ(animation["static_lower_source_offset"], 0x800);
    EXPECT_EQ(animation["static_lower_destination_offset"], 0x7800);
    EXPECT_EQ(animation["static_lower_byte_count"], 0x800);
    EXPECT_EQ(
        ByteFingerprint(pixels, 0x7800, 0x800),
        ByteFingerprint(data.graphics_buffer, 0x3E * 0x1000 + 0x800, 0x800));
    if (state == 0) {
      auto incomplete = *assets;
      incomplete.source.mutable_current_graphics()->pop_back();
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, incomplete)
                       .ok());
      incomplete = *assets;
      incomplete.source.mutable_current_palette()->set_size(255);
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, incomplete)
                       .ok());
      auto inconsistent = *assets;
      (*inconsistent.source.mutable_current_graphics())[0] ^= 1;
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, inconsistent)
                       .ok());
      inconsistent = *assets;
      auto* palette = inconsistent.source.mutable_current_palette();
      (*palette)[0] =
          gfx::SnesColor(static_cast<uint16_t>((*palette)[0].snes() ^ 1));
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, inconsistent)
                       .ok());
      inconsistent = *assets;
      inconsistent.binding.assign(64, '0');
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, inconsistent)
                       .ok());
      auto wrong_properties = authoring.maps[0].properties;
      wrong_properties.palette_set ^= 1;
      EXPECT_FALSE(
          zelda3::DescribeLyraNativeMapAssets(wrong_properties, state, *assets)
              .ok());
      for (const int wrong_state : {-1, 1, 3})
        EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                         authoring.maps[0].properties, wrong_state, *assets)
                         .ok());
      {
        auto& decoded_pixel =
            data.graphics_buffer
                [authoring.maps[0].properties.graphics_sheets[0] * 0x1000];
        const uint8_t original_pixel = decoded_pixel;
        absl::Cleanup restore_pixel = [&]() {
          decoded_pixel = original_pixel;
        };
        decoded_pixel ^= 1;
        const auto drifted = zelda3::DescribeLyraNativeMapAssets(
            authoring.maps[0].properties, state, *assets);
        EXPECT_TRUE(absl::IsFailedPrecondition(drifted.status()));
      }
      {
        auto* donor = source->mutable_overworld_map(0x3A);
        ASSERT_NE(donor, nullptr);
        const uint16_t sheet = authoring.maps[0].properties.graphics_sheets[0];
        auto begin = data.graphics_buffer.begin() + sheet * 0x1000;
        std::vector<uint8_t> preview(begin, begin + 0x1000);
        preview[0] ^= 1;
        donor->SetGraphicsSheetOverrides({{sheet, std::move(preview)}});
        absl::Cleanup reset_overrides = [&]() {
          donor->SetGraphicsSheetOverrides({});
        };
        const auto overridden = zelda3::ResolveLyraMapAssets(
            *source, authoring.maps[0].properties, state);
        ASSERT_TRUE(overridden.ok()) << overridden.status();
        EXPECT_NE(overridden->binding, assets->binding);
        const auto unsafe_description = zelda3::DescribeLyraNativeMapAssets(
            authoring.maps[0].properties, state, *overridden);
        EXPECT_TRUE(absl::IsFailedPrecondition(unsafe_description.status()));
      }
      // A complete composite raster cannot conceal a missing decoded source.
      auto decoded = std::move(data.graphics_buffer);
      absl::Cleanup restore_decoded = [&]() {
        data.graphics_buffer = std::move(decoded);
      };
      EXPECT_FALSE(zelda3::DescribeLyraNativeMapAssets(
                       authoring.maps[0].properties, state, *assets)
                       .ok());
    }
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

  handlers::LyraInspectProjectCommandHandler inspection;
  inspection.SetProjectContext(&project);
  resources::OutputFormatter inspected_output(
      resources::OutputFormatter::Format::kJson);
  inspected_output.BeginObject();
  const auto inspect_status = inspection.Execute(&rom, args, inspected_output);
  ASSERT_TRUE(inspect_status.ok()) << inspect_status;
  inspected_output.EndObject();
  const auto inspected = nlohmann::json::parse(inspected_output.GetOutput());
  EXPECT_EQ(inspected["inspection_only"], true);
  EXPECT_EQ(inspected["asset_binding_mismatches"], 1);
  EXPECT_TRUE(inspected["project_descriptor_sha256"].is_null());
  ASSERT_TRUE(inspected["rom_address_overrides"].is_object());
  EXPECT_FALSE(inspected["rom_address_overrides"].empty());
  EXPECT_EQ(inspected["rom_address_overrides_sha256"],
            core::ComputeSourceArtifactSha256(
                inspected["rom_address_overrides"].dump()));
  ASSERT_EQ(inspected["map_assets"].size(), 96);
  for (int map = 0; map < core::LyraTerrainDocument::kMapCount; ++map) {
    for (int state = 0; state < 3; ++state) {
      SCOPED_TRACE("map=" + std::to_string(map + 0xA0) +
                   " state=" + std::to_string(state));
      const auto& row = inspected["map_assets"][map * 3 + state];
      EXPECT_EQ(row["map_id"], map + 0xA0);
      EXPECT_EQ(row["state"], state);
      EXPECT_EQ(row["stored_binding"], changed.maps[map].asset_bindings[state]);
      EXPECT_EQ(row["binding_changed"], map == 0 && state == 1);
      EXPECT_EQ(row["runtime_assignment"], "unassigned");
      const auto& native = row["native_assets"];
      EXPECT_EQ(native["schema"], "yaze.lyra-native-resolution.v1");
      EXPECT_EQ(native["resolution_mode"], "native-only");
      EXPECT_EQ(native["resolver_seed_map"], 0x3A);
      EXPECT_EQ(native["state"], state);
      EXPECT_EQ(native["inspection_only"], true);
      EXPECT_EQ(native["runtime_assignment"], "unassigned");
      EXPECT_TRUE(native["custom_catalog_binding"].is_null());
      EXPECT_EQ(native["custom_transfers"], nlohmann::json::array());
      EXPECT_EQ(native["native_base_binding"], row["current_binding"]);
      EXPECT_EQ(row["native_base_binding"], row["current_binding"]);
      EXPECT_FALSE(native.contains("gameplay_parent"));
      EXPECT_FALSE(native.contains("footprint"));
      const auto& properties = native["saved_properties"];
      EXPECT_EQ(properties["graphics_sheets"],
                nlohmann::json(changed.maps[map].properties.graphics_sheets));
      EXPECT_EQ(properties["sprite_graphics"],
                nlohmann::json::array({-1, -1, -1}));
      EXPECT_EQ(properties["sprite_palettes"],
                nlohmann::json::array({-1, -1, -1}));
      ASSERT_EQ(native["slots"].size(), 16);
      EXPECT_EQ(native["graphics"]["format"], "indexed8");
      EXPECT_EQ(native["graphics"]["byte_count"], 65536);
      EXPECT_EQ(native["palette"]["words"], row["palette_words"]);
      ASSERT_EQ(row["palette_words"].size(), 256);
      if (map == 0) {
        EXPECT_EQ(native["native_base_binding"],
                  authoring.maps[0].asset_bindings[state]);
        EXPECT_EQ(native["palette"]["words"],
                  nlohmann::json(expected_palettes[state]));
        EXPECT_EQ(native["palette"]["uint16le_sha256"],
                  WordFingerprint(expected_palettes[state]));
        EXPECT_EQ(native["palette"]["background_override_words"],
                  nlohmann::json(background));
        EXPECT_EQ(native["palette"]["background_override_uint16le_sha256"],
                  WordFingerprint(background));
        EXPECT_EQ(native["palette"]["runtime_consumer"], "unsupported");
      } else {
        EXPECT_TRUE(native["palette"]["background_override_words"].is_null());
        EXPECT_TRUE(
            native["palette"]["background_override_uint16le_sha256"].is_null());
        EXPECT_EQ(native["palette"]["runtime_consumer"], "unassigned");
      }
    }
  }
  EXPECT_EQ(rom.vector(), before);
  EXPECT_EQ(core::RomSettings::Get().address_overrides().addresses,
            unrelated.addresses);
  std::ifstream inspected_sidecar(project.lyra_terrain_file);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(inspected_sidecar), {}),
            document->Serialize());

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
