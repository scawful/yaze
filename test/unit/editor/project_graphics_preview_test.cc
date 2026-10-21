#include "app/editor/overworld/project_graphics_preview.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "zelda3/game_data.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {
namespace {

// Character $C0 of the area tileset: BG base $2000, VRAM word $2C00.
constexpr size_t kPreviewPixel = (0xC0 / 16 * 8) * 128 + (0xC0 % 16) * 8;

class ProjectGraphicsPreviewSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = std::filesystem::temp_directory_path() /
            ("yaze-project-graphics-" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root_ / "a");
    std::filesystem::create_directories(root_ / "b");
    std::vector<uint8_t> data(0x200000, 0);
    data[zelda3::OverworldCustomASMHasBeenApplied] = 3;
    for (int id = 0; id < zelda3::kNumOverworldMaps; ++id)
      data[zelda3::kOverworldMapParentIdExpanded + id] = id;
    ASSERT_TRUE(rom_.LoadFromData(data).ok());
    game_.graphics_buffer.resize(0x100000, 0);
    overworld_ = std::make_unique<zelda3::Overworld>(&rom_);
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        overworld_->overworld_maps());
    maps.reserve(zelda3::kNumOverworldMaps);
    for (int id = 0; id < zelda3::kNumOverworldMaps; ++id)
      maps.emplace_back(id, &rom_, &game_);
    ordinary_pixel_ = RenderedPixel(0x30);
  }
  void TearDown() override { std::filesystem::remove_all(root_); }

  // One 4bpp character whose first pixel has `color`, bound to `parents`.
  // `listed_color` lets a test publish bytes that differ from the catalog's
  // recorded identity, as a half-written pack build would.
  std::string WriteCatalog(const std::string& project, uint8_t color,
                           const std::vector<int>& parents,
                           uint8_t listed_color = 0) {
    const auto dir = root_ / project;
    std::ofstream(dir / "chars.bin", std::ios::binary) << Character(color);
    nlohmann::json catalog = {{"schema_version", 2}};
    catalog["resources"]["chars"] = {
        {"path", "chars.bin"},
        {"format", "snes4bpp"},
        {"byte_count", 32},
        {"sha256", core::ComputeSourceArtifactSha256(
                       Character(listed_color ? listed_color : color))}};
    catalog["profiles"]["area"] = {{"bg_character_base_word", 0x2000},
                                   {"transfers",
                                    {{{"resource_id", "chars"},
                                      {"byte_count", 32},
                                      {"vram_word_address", 0x2C00}}}}};
    catalog["bindings"] = nlohmann::json::array(
        {{{"parent_maps", parents}, {"profile_id", "area"}}});
    const auto path = (dir / "catalog.json").string();
    std::ofstream(path) << catalog.dump();
    return path;
  }

  static std::string Character(uint8_t color) {
    std::string bytes(32, '\0');
    for (int plane = 0; plane < 4; ++plane) {
      if (color & (1 << plane))
        bytes[(plane / 2) * 16 + plane % 2] = static_cast<char>(0x80);
    }
    return bytes;
  }

  uint8_t RenderedPixel(int map_id) {
    auto* map = overworld_->mutable_overworld_map(map_id);
    EXPECT_TRUE(map->BuildTileset().ok());
    return map->current_graphics()[kPreviewPixel];
  }

  std::filesystem::path root_;
  uint8_t ordinary_pixel_ = 0;
  Rom rom_;
  zelda3::GameData game_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  ProjectGraphicsPreviewSource source_;
};

TEST_F(ProjectGraphicsPreviewSourceTest,
       FailedReloadKeepsArtUntilCorrectedFileAtSamePathIsAccepted) {
  const auto path = WriteCatalog("a", 3, {0x30});
  std::set<int> changed;
  ASSERT_TRUE(source_.Bind(path, *overworld_, &changed).ok());
  EXPECT_EQ(changed, std::set<int>({0x30}));
  ASSERT_EQ(RenderedPixel(0x30), 3);
  const auto shown = overworld_->overworld_map(0x30)->current_graphics();

  // New bytes under the old identity, then a truncated catalog.
  WriteCatalog("a", 5, {0x30}, /*listed_color=*/3);
  EXPECT_FALSE(source_.LoadReplacement().ok());
  std::ofstream(path) << "{";
  EXPECT_FALSE(source_.LoadReplacement().ok());
  EXPECT_EQ(overworld_->overworld_map(0x30)->current_graphics(), shown);
  EXPECT_TRUE(overworld_->overworld_map(0x30)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x30), 3);

  WriteCatalog("a", 5, {0x30});
  auto replacement = source_.LoadReplacement();
  ASSERT_TRUE(replacement.ok()) << replacement.status();
  EXPECT_EQ(replacement->changed_parents, std::set<int>({0x30}));
  EXPECT_EQ(RenderedPixel(0x30), 3) << "validation must not apply";
  ASSERT_TRUE(source_.Commit(std::move(*replacement), *overworld_).ok());
  EXPECT_EQ(RenderedPixel(0x30), 5);
  EXPECT_FALSE(overworld_->overworld_map(0x00)->HasGraphicsPreview());
  EXPECT_EQ(source_.path(), path);
}

TEST_F(ProjectGraphicsPreviewSourceTest, ReloadReportsOnlyChangedParents) {
  const auto path = WriteCatalog("a", 3, {0x30, 0x32});
  ASSERT_TRUE(source_.Bind(path, *overworld_, nullptr).ok());

  auto same = source_.LoadReplacement();
  ASSERT_TRUE(same.ok()) << same.status();
  EXPECT_TRUE(same->changed_parents.empty());

  WriteCatalog("a", 3, {0x32, 0x38});
  auto moved = source_.LoadReplacement();
  ASSERT_TRUE(moved.ok()) << moved.status();
  EXPECT_EQ(moved->changed_parents, std::set<int>({0x30, 0x38}));
  ASSERT_TRUE(source_.Commit(std::move(*moved), *overworld_).ok());
  EXPECT_FALSE(overworld_->overworld_map(0x30)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x32), 3);
  EXPECT_EQ(RenderedPixel(0x38), 3);
}

TEST_F(ProjectGraphicsPreviewSourceTest,
       ProjectSwitchNeverRetainsOrLaterAppliesPreviousProjectArt) {
  const auto first = WriteCatalog("a", 3, {0x30});
  ASSERT_TRUE(source_.Bind(first, *overworld_, nullptr).ok());
  ASSERT_EQ(RenderedPixel(0x30), 3);
  auto stale = source_.LoadReplacement();
  ASSERT_TRUE(stale.ok()) << stale.status();

  // The second project's catalog is missing.
  const auto second = (root_ / "b" / "catalog.json").string();
  std::set<int> changed;
  EXPECT_FALSE(source_.Bind(second, *overworld_, &changed).ok());
  EXPECT_EQ(changed, std::set<int>({0x30}));
  EXPECT_FALSE(overworld_->overworld_map(0x30)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x30), ordinary_pixel_);
  EXPECT_FALSE(source_.LoadReplacement().ok());

  EXPECT_FALSE(source_.Commit(std::move(*stale), *overworld_).ok());
  EXPECT_FALSE(overworld_->overworld_map(0x30)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x30), ordinary_pixel_);

  WriteCatalog("b", 6, {0x38});
  auto fixed = source_.LoadReplacement();
  ASSERT_TRUE(fixed.ok()) << fixed.status();
  ASSERT_TRUE(source_.Commit(std::move(*fixed), *overworld_).ok());
  EXPECT_FALSE(overworld_->overworld_map(0x30)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x38), 6);
}

TEST_F(ProjectGraphicsPreviewSourceTest,
       CommitInverseRestoresPreviousArtOnlyBeforeAnotherCommit) {
  const auto path = WriteCatalog("a", 3, {0x30});
  ASSERT_TRUE(source_.Bind(path, *overworld_, nullptr).ok());
  WriteCatalog("a", 5, {0x30, 0x38});
  auto replacement = source_.LoadReplacement();
  ASSERT_TRUE(replacement.ok()) << replacement.status();
  auto inverse = source_.Commit(std::move(*replacement), *overworld_);
  ASSERT_TRUE(inverse.ok()) << inverse.status();
  ASSERT_EQ(RenderedPixel(0x30), 5);
  ASSERT_EQ(RenderedPixel(0x38), 5);

  auto restored = source_.Commit(std::move(*inverse), *overworld_);
  ASSERT_TRUE(restored.ok()) << restored.status();
  EXPECT_EQ(RenderedPixel(0x30), 3);
  EXPECT_FALSE(overworld_->overworld_map(0x38)->HasGraphicsPreview());
  EXPECT_EQ(RenderedPixel(0x38), ordinary_pixel_);
  EXPECT_EQ(source_.path(), path);

  // An inverse is bound to the catalog it was taken from.
  auto again = source_.LoadReplacement();
  ASSERT_TRUE(again.ok()) << again.status();
  ASSERT_TRUE(source_.Commit(std::move(*again), *overworld_).ok());
  EXPECT_FALSE(source_.Commit(std::move(*restored), *overworld_).ok());
  EXPECT_EQ(RenderedPixel(0x30), 5);
  EXPECT_EQ(RenderedPixel(0x38), 5);
}

}  // namespace
}  // namespace yaze::editor
