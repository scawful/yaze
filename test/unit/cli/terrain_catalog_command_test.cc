#include "cli/handlers/game/terrain_catalog_commands.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "oracle_rom_fixture.h"
#include "rom/rom.h"

namespace yaze::cli {
namespace {

using nlohmann::json;

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream text;
  text << input.rdbuf();
  return text.str();
}

class TerrainCatalogCommandTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto rom_path = test::FindOracleRomFixture();
    if (rom_path.empty()) {
      GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS to a copy of an Oracle ROM";
    }
    ASSERT_TRUE(rom_.LoadFromFile(rom_path).ok());
    root_ = std::filesystem::temp_directory_path() /
            ("yaze-terrain-catalog-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root_);
    overrides_ = core::RomSettings::Get().address_overrides();
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }

  absl::Status Run(std::vector<std::string> args, const std::string& name,
                   json* out = nullptr) {
    const auto path = root_ / (name + ".json");
    args.insert(args.begin(), {"--map=0x33", "--out=" + path.string()});
    args.push_back("--format=json");
    handlers::OverworldTerrainCatalogCommandHandler handler;
    std::string output;
    const auto status = handler.Run(args, &rom_, &output);
    if (status.ok() && out != nullptr) {
      *out = json::parse(ReadFile(path));
    }
    return status;
  }

  // One 4bpp transfer that replaces every character of `slot`. Its first
  // character is all zero (transparent); the rest are color 15.
  std::filesystem::path WriteManifest(int parent, int slot,
                                      const std::string& asset = "dock.bin") {
    std::vector<char> bytes(64 * 32, static_cast<char>(0xFF));
    std::fill(bytes.begin(), bytes.begin() + 32, 0);
    std::ofstream(root_ / "dock.bin", std::ios::binary)
        .write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const json document = {
        {"schema_version", 1},
        {"areas",
         {{{"parent_map", parent},
           {"bg_character_base_word", 0x2000},
           {"transfers",
            {{{"path", asset},
              {"format", "snes4bpp"},
              {"byte_count", 64 * 32},
              {"vram_word_address", 0x2000 + slot * 64 * 16}}}}}}}};
    std::ofstream(root_ / "preview.json") << document.dump();
    return root_ / "preview.json";
  }

  Rom rom_;
  std::filesystem::path root_;
  core::RomAddressOverrides overrides_;
};

// The most used BG slot whose characters are all ordinary sheet pixels.
int BusiestSheetSlot(const json& catalog) {
  std::map<int, int> uses;
  for (const auto& record : catalog.at("records")) {
    for (const auto& quadrant : record.at("quadrants")) {
      const auto& source = quadrant.at("graphics_source");
      const int slot = source.at("slot");
      if (source.at("kind") == "sheet" && slot < 7) {
        ++uses[slot];
      }
    }
  }
  int best = -1;
  for (const auto& [slot, count] : uses) {
    if (best < 0 || count > uses[best])
      best = slot;
  }
  return best;
}

TEST_F(TerrainCatalogCommandTest, PreviewTransfersReplaceOnlyTheirCharacters) {
  const auto rom_before = rom_.vector();
  json none;
  json off;
  ASSERT_TRUE(Run({}, "none", &none).ok());
  ASSERT_TRUE(Run({"--graphics-preview=off"}, "off", &off).ok());
  // Control: without a manifest the export is the ROM's sheets only.
  EXPECT_EQ(none.at("records"), off.at("records"));
  EXPECT_EQ(none.at("profile_fingerprint"), off.at("profile_fingerprint"));
  EXPECT_EQ(off.at("profile").at("runtime_overrides"), "not_applied");
  EXPECT_EQ(none.at("graphics_composition").at("graphics_preview").at("mode"),
            "none");
  EXPECT_EQ(off.at("graphics_composition").at("graphics_preview").at("mode"),
            "off");
  for (const auto& record : off.at("records")) {
    for (const auto& quadrant : record.at("quadrants")) {
      const auto& source = quadrant.at("graphics_source");
      const int id = quadrant.at("tile8_id");
      EXPECT_EQ(source.at("slot"), id / 64);
      EXPECT_NE(source.at("kind"), "graphics_preview");
      if (source.at("kind") == "sheet") {
        const int slot = id / 64;
        EXPECT_EQ(
            source.at("palette_half"),
            slot == 0 || slot == 3 || slot == 4 || slot == 5 ? "high" : "low");
        EXPECT_EQ(source.at("sheet"), off.at("profile").at("sheets").at(slot));
      }
    }
  }

  const int slot = BusiestSheetSlot(off);
  ASSERT_GE(slot, 0);
  const auto manifest = WriteManifest(off.at("parent_area"), slot);
  json on;
  ASSERT_TRUE(Run({"--graphics-preview=" + manifest.string()}, "on", &on).ok());

  const auto& preview = on.at("graphics_composition").at("graphics_preview");
  EXPECT_EQ(preview.at("mode"), "file");
  EXPECT_TRUE(preview.at("applied"));
  EXPECT_EQ(preview.at("manifest_sha256"),
            core::ComputeSourceArtifactSha256(ReadFile(manifest)));
  EXPECT_EQ(
      preview.at("characters"),
      json::array(
          {{{"first", slot * 64}, {"last", slot * 64 + 63}, {"count", 64}}}));
  EXPECT_EQ(on.at("graphics_composition")
                .at("slots")
                .at(slot)
                .at("characters")
                .at("graphics_preview"),
            64);
  EXPECT_EQ(on.at("profile").at("runtime_overrides"), "graphics_preview");
  EXPECT_NE(on.at("profile_fingerprint"), off.at("profile_fingerprint"));

  ASSERT_EQ(on.at("records").size(), off.at("records").size());
  int replaced = 0;
  bool pixels_changed = false;
  for (size_t r = 0; r < off.at("records").size(); ++r) {
    const auto& before = off.at("records").at(r);
    const auto& after = on.at("records").at(r);
    ASSERT_EQ(after.at("tile_id"), before.at("tile_id"));
    EXPECT_EQ(after.at("definition_fingerprint"),
              before.at("definition_fingerprint"));
    bool uses_slot = false;
    for (size_t q = 0; q < 4; ++q) {
      const auto& old_quadrant = before.at("quadrants").at(q);
      const auto& quadrant = after.at("quadrants").at(q);
      const int id = quadrant.at("tile8_id");
      if (id / 64 == slot) {
        uses_slot = true;
        ++replaced;
        EXPECT_EQ(quadrant.at("graphics_source").at("kind"),
                  "graphics_preview");
        EXPECT_TRUE(quadrant.at("graphics_source").at("sheet").is_null());
        EXPECT_EQ(quadrant.at("transparent_pixels"), id == slot * 64 ? 64 : 0);
      } else {
        EXPECT_EQ(quadrant.at("graphics_source"),
                  old_quadrant.at("graphics_source"));
        EXPECT_EQ(quadrant.at("transparent_pixels"),
                  old_quadrant.at("transparent_pixels"));
      }
    }
    if (uses_slot) {
      pixels_changed |= after.at("pixels") != before.at("pixels");
    } else {
      EXPECT_EQ(after.at("pixels"), before.at("pixels"));
      EXPECT_EQ(after.at("transparent_mask"), before.at("transparent_mask"));
    }
  }
  EXPECT_GT(replaced, 0);
  EXPECT_TRUE(pixels_changed);
  EXPECT_EQ(rom_.vector(), rom_before);
}

TEST_F(TerrainCatalogCommandTest, ProjectContextSuppliesThePreview) {
  json off;
  ASSERT_TRUE(Run({"--graphics-preview=off"}, "off", &off).ok());
  const int slot = BusiestSheetSlot(off);
  ASSERT_GE(slot, 0);
  WriteManifest(off.at("parent_area"), slot);
  project::YazeProject project;
  ASSERT_TRUE(project.Create("terrain", root_.string()).ok());
  project.overworld_graphics_preview = "preview.json";
  ASSERT_TRUE(project.Save().ok());
  const std::string context = "--project-context=" + project.filepath;

  json on;
  ASSERT_TRUE(Run({context}, "project", &on).ok());
  const auto& preview = on.at("graphics_composition").at("graphics_preview");
  EXPECT_EQ(preview.at("mode"), "project");
  EXPECT_TRUE(preview.at("applied"));
  EXPECT_EQ(preview.at("characters").at(0).at("first"), slot * 64);

  json project_off;
  ASSERT_TRUE(
      Run({context, "--graphics-preview=off"}, "project_off", &project_off)
          .ok());
  EXPECT_EQ(project_off.at("records"), off.at("records"));
  EXPECT_FALSE(project_off.at("graphics_composition")
                   .at("graphics_preview")
                   .at("applied"));
  EXPECT_EQ(core::RomSettings::Get().address_overrides().addresses,
            overrides_.addresses);
}

TEST_F(TerrainCatalogCommandTest, UnavailablePreviewsRefuseWithoutOutput) {
  const auto rom_before = rom_.vector();
  EXPECT_TRUE(absl::IsFailedPrecondition(
      Run({"--graphics-preview=on"}, "on_without_project")));
  EXPECT_FALSE(std::filesystem::exists(root_ / "on_without_project.json"));

  const auto manifest = WriteManifest(0x33, 3, "missing.bin");
  EXPECT_FALSE(
      Run({"--graphics-preview=" + manifest.string()}, "missing_asset").ok());
  EXPECT_FALSE(std::filesystem::exists(root_ / "missing_asset.json"));

  EXPECT_TRUE(absl::IsInvalidArgument(
      Run({"--project-file=a.yaze", "--project-context=b.yaze"}, "both")));
  EXPECT_FALSE(std::filesystem::exists(root_ / "both.json"));
  EXPECT_EQ(rom_.vector(), rom_before);
}

}  // namespace
}  // namespace yaze::cli
