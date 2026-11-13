#include "app/editor/graphics/catalog_tile_transfer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>

#include "app/platform/asset_loader.h"

namespace yaze::editor {
namespace {

TEST(BundledCatalogAssetsTest, LoadsAllPagesAndAssemblesCompleteAssets) {
  const auto path =
      AssetLoader::FindAssetFile("graphics/catalog/minish/catalog.json");
  ASSERT_TRUE(path.ok()) << path.status();
  const auto sources = LoadCatalogSheetSources(path->string());
  ASSERT_TRUE(sources.ok()) << sources.status();
  EXPECT_EQ(sources->size(), 35);
  std::set<std::string> assets;
  std::set<std::string> categories;
  for (const auto& source : *sources) {
    assets.insert(source.asset_id);
    categories.insert(source.category);
    EXPECT_FALSE(source.resources_bound);
    EXPECT_EQ(source.bpp, 3);
    EXPECT_EQ(source.palette.size(), 8);
    EXPECT_EQ(source.indexed_pixels.size(), 4096);
    EXPECT_EQ(source.source_sha256.size(), 64);
    EXPECT_EQ(source.page_sha256.size(), 64);
    EXPECT_FALSE(source.credit.empty());
    EXPECT_FALSE(source.preview_path.empty());
    EXPECT_FALSE(source.game.empty());
    EXPECT_TRUE(std::all_of(source.indexed_pixels.begin(),
                            source.indexed_pixels.end(),
                            [](uint8_t pixel) { return pixel < 8; }));
  }
  EXPECT_EQ(assets.size(), 34);
  EXPECT_TRUE(categories.contains("arch"));
  EXPECT_TRUE(categories.contains("prop"));
  EXPECT_TRUE(categories.contains("architecture"));
  EXPECT_TRUE(categories.contains("wall-panel"));
  for (const auto& id : assets) {
    const auto art = AssembleCatalogAsset(*sources, id);
    ASSERT_TRUE(art.ok()) << id << ": " << art.status();
    EXPECT_EQ(art->indexed_pixels.size(),
              static_cast<size_t>(art->width * art->height));
    EXPECT_EQ(art->palette.size(), 8);
  }
  const auto dark = AssembleCatalogAsset(*sources, "minish.dark-castle-arch");
  ASSERT_TRUE(dark.ok()) << dark.status();
  EXPECT_EQ(dark->width, 80);
  EXPECT_EQ(dark->height, 96);
  const auto bright =
      AssembleCatalogAsset(*sources, "minish.bright-hyrule-castle-arch");
  ASSERT_TRUE(bright.ok()) << bright.status();
  EXPECT_EQ(bright->width, 96);
  EXPECT_EQ(bright->height, 48);
}

}  // namespace
}  // namespace yaze::editor
