#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "app/editor/graphics/catalog_tile_transfer.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "yaze_test_paths.h"

namespace yaze::editor {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

// Subset of the V30 GBC collection (catalog.json 2ec09085...) with every
// reference copied verbatim, including research fields the reader ignores.
fs::path FixtureDir() {
  return fs::path(YAZE_TEST_FIXTURE_DIR) / "graphics" / "gbc_catalog";
}

std::map<std::string, std::vector<CatalogSheetSource>> ByAsset(
    const std::vector<CatalogSheetSource>& sources) {
  std::map<std::string, std::vector<CatalogSheetSource>> assets;
  for (const auto& source : sources)
    assets[source.asset_id].push_back(source);
  return assets;
}

size_t CountMatches(const std::vector<CatalogSheetSource>& sources,
                    const CatalogSourceFilter& filter) {
  return std::count_if(sources.begin(), sources.end(), [&](const auto& source) {
    return CatalogSourceMatches(source, filter);
  });
}

TEST(CatalogGbcCollectionTest, LoadsTileMotifsAndSpritePosesAtTheirDepth) {
  const auto sources =
      LoadCatalogSheetSources((FixtureDir() / "catalog.json").string());
  ASSERT_TRUE(sources.ok()) << sources.status();
  ASSERT_EQ(sources->size(), 6u);
  const auto assets = ByAsset(*sources);
  ASSERT_EQ(assets.size(), 5u);
  for (const auto& source : *sources) {
    SCOPED_TRACE(source.id);
    EXPECT_FALSE(source.resources_bound);
    EXPECT_EQ(source.source_kind, "indexed-png");
    EXPECT_EQ(source.palette.size(), size_t{1} << source.bpp);
    EXPECT_TRUE(
        std::all_of(source.indexed_pixels.begin(), source.indexed_pixels.end(),
                    [&](uint8_t pixel) { return pixel < (1 << source.bpp); }));
    EXPECT_FALSE(source.game.empty());
    EXPECT_FALSE(source.credit.empty());
    EXPECT_EQ(source.source_sha256.size(), 64u);
    EXPECT_FALSE(source.conversion.empty());
    EXPECT_FALSE(source.behavior_status.empty());
    EXPECT_FALSE(source.binary_path.empty());
    EXPECT_EQ(source.binary_sha256.size(), 64u);
    EXPECT_EQ(fs::file_size(source.binary_path),
              static_cast<uintmax_t>(64 * 8 * source.bpp));
  }

  const auto& keese = assets.at("gbc.gbc-keese").front();
  EXPECT_EQ(keese.bpp, 4);
  EXPECT_EQ(keese.resource_kind, CatalogResourceKind::kSpritePose);
  EXPECT_EQ(fs::path(keese.binary_path).extension(), ".4bpp");
  EXPECT_FALSE(ValidateCatalogSourceForRomSheet(keese).ok());

  const auto& moblin = assets.at("gbc.gbc-moblin-king");
  ASSERT_EQ(moblin.size(), 2u);
  EXPECT_EQ(moblin[0].page_count, 2);
  EXPECT_EQ(moblin[1].bpp, 4);

  const auto& pot = assets.at("gbc.la-dungeon-pot").front();
  EXPECT_EQ(pot.bpp, 3);
  EXPECT_EQ(pot.resource_kind, CatalogResourceKind::kDungeonTile);
  EXPECT_EQ(fs::path(pot.binary_path).extension(), ".3bpp");
  EXPECT_TRUE(ValidateCatalogSourceForRomSheet(pot).ok());
  EXPECT_EQ(assets.at("gbc.season-round-tree").front().resource_kind,
            CatalogResourceKind::kOverworldTile);
  EXPECT_EQ(assets.at("gbc.la-barrel").front().resource_kind,
            CatalogResourceKind::kInteriorTile);

  for (const auto& [id, pages] : assets) {
    const auto art = AssembleCatalogAsset(*sources, id);
    ASSERT_TRUE(art.ok()) << id << ": " << art.status();
    EXPECT_EQ(art->bpp, pages.front().bpp);
    EXPECT_EQ(art->palette.size(), size_t{1} << art->bpp);
  }
}

TEST(CatalogGbcCollectionTest, FiltersByResourceKindPaletteAndProvenance) {
  const auto sources =
      LoadCatalogSheetSources((FixtureDir() / "catalog.json").string());
  ASSERT_TRUE(sources.ok()) << sources.status();
  const auto count = [&](CatalogSourceFilter filter) {
    return CountMatches(*sources, filter);
  };
  EXPECT_EQ(count({}), 6u);
  EXPECT_EQ(count({CatalogResourceKind::kSpritePose}), 3u);
  EXPECT_EQ(count({CatalogResourceKind::kOverworldTile}), 1u);
  EXPECT_EQ(count({CatalogResourceKind::kDungeonTile}), 1u);
  EXPECT_EQ(count({CatalogResourceKind::kInteriorTile}), 1u);
  EXPECT_EQ(count({CatalogResourceKind::kOtherSheet}), 0u);
  EXPECT_EQ(count({std::nullopt, true}), 6u);
  EXPECT_EQ(count({std::nullopt, false, "", "", "KEESE"}), 1u);
  EXPECT_EQ(count({CatalogResourceKind::kSpritePose, false, "", "", "keese"}),
            1u);
  EXPECT_EQ(count({CatalogResourceKind::kDungeonTile, false, "", "", "keese"}),
            0u);
  EXPECT_EQ(count({std::nullopt, false, "", "sprite"}), 3u);
  EXPECT_STREQ(CatalogResourceKindName(CatalogResourceKind::kSpritePose),
               "Sprite family poses");
}

class CatalogGbcMutationTest : public testing::Test {
 protected:
  void SetUp() override {
    folder_ =
        fs::temp_directory_path() /
        ("yaze-gbc-catalog-" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::copy(FixtureDir(), folder_, fs::copy_options::recursive);
    std::ifstream input(folder_ / "catalog.json");
    catalog_ = Json::parse(input);
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(folder_, ec);
  }

  Json& Reference(const std::string& id) {
    for (auto& reference : catalog_["references"])
      if (reference["id"] == id)
        return reference;
    ADD_FAILURE() << "missing reference " << id;
    return catalog_;
  }

  absl::StatusOr<std::vector<CatalogSheetSource>> Load() {
    std::ofstream(folder_ / "catalog.json") << catalog_.dump(1);
    return LoadCatalogSheetSources((folder_ / "catalog.json").string());
  }

  void RewriteBinary(Json& page,
                     const std::function<void(std::string&)>& edit) {
    const auto path = folder_ / page["binary_path"].get<std::string>();
    std::ifstream input(path, std::ios::binary);
    std::string bytes(std::istreambuf_iterator<char>(input), {});
    input.close();
    edit(bytes);
    std::ofstream(path, std::ios::binary | std::ios::trunc) << bytes;
    page["binary_sha256"] = core::ComputeSourceArtifactSha256(bytes);
  }

  fs::path folder_;
  Json catalog_;
};

TEST_F(CatalogGbcMutationTest, ToleratesUnknownAndMalformedResearchFields) {
  catalog_["research_packet"] = {{"version", 30}, {"receipts", {1, 2, 3}}};
  for (auto& reference : catalog_["references"]) {
    reference["future_binding_hint"] = {{"slots", Json::array()},
                                        {"x", nullptr}};
    reference["frame_layout_verified"] = false;
    for (auto& page : reference["pages"])
      page["oam_preview"] = {{"cells", 4}};
  }
  Reference("gbc.la-dungeon-pot")["notes"] = nullptr;
  Reference("gbc.la-dungeon-pot")["conversion"] = 7;
  Reference("gbc.la-dungeon-pot")["resource_kind"] = "tilemap-word";
  Reference("gbc.la-barrel")["color_reduction"] = "maybe";
  Reference("gbc.gbc-keese")["resource_kind"] = "other-sheet";
  const auto sources = Load();
  ASSERT_TRUE(sources.ok()) << sources.status();
  ASSERT_EQ(sources->size(), 6u);
  const auto assets = ByAsset(*sources);
  const auto& pot = assets.at("gbc.la-dungeon-pot").front();
  EXPECT_TRUE(pot.notes.empty());
  EXPECT_TRUE(pot.conversion.empty());
  EXPECT_EQ(pot.resource_kind, CatalogResourceKind::kDungeonTile);
  EXPECT_FALSE(assets.at("gbc.la-barrel").front().color_reduction);
  EXPECT_EQ(assets.at("gbc.gbc-keese").front().resource_kind,
            CatalogResourceKind::kOtherSheet);
}

TEST_F(CatalogGbcMutationTest, RefusesBoundResources) {
  Reference("gbc.gbc-keese")["resources_bound"] = true;
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kFailedPrecondition);
  Reference("gbc.gbc-keese")["resources_bound"] = "no";
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  Reference("gbc.gbc-keese")["resources_bound"] = false;
  EXPECT_TRUE(Load().ok());
}

TEST_F(CatalogGbcMutationTest, PlanarPageMustDecodeAtTheDeclaredDepth) {
  auto& page = Reference("gbc.gbc-keese")["pages"][0];
  // Plane 3 of tile 0, row 0: raises a pixel's ID above the PNG's.
  RewriteBinary(page, [](std::string& bytes) { bytes[17] ^= 0x80; });
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
  RewriteBinary(page, [](std::string& bytes) {
    bytes[17] ^= 0x80;
    bytes.resize(1536);
  });
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(CatalogGbcMutationTest, PlanarPageRequiresItsHash) {
  auto& page = Reference("gbc.la-dungeon-pot")["pages"][0];
  page.erase("binary_sha256");
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kInvalidArgument);
  page["binary_sha256"] = std::string(64, 'a');
  EXPECT_EQ(Load().status().code(), absl::StatusCode::kDataLoss);
}

}  // namespace
}  // namespace yaze::editor
