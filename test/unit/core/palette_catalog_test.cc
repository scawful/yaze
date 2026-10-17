#include "core/palette_catalog.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
using nlohmann::json;
json Document() {
  return {
      {"schema_version", 1},
      {"game_id", "oracle"},
      {"entries",
       json::array(
           {{{"key", "oracle.sprites.lw"},
             {"name", "Light World sprites"},
             {"group", "global_sprites"},
             {"index", 0},
             {"world", "light_world"},
             {"use", "mixed"},
             {"description", "Four OBJ rows"},
             {"provenance", "Palettes_Load_SpriteMain $1B:EC9E"},
             {"ownership", "rom"},
             {"usages",
              json::array({{{"name", "Impa"},
                            {"context", "Follower $01, Light World"},
                            {"use", "npcs"},
                            {"obj_row", 4},
                            {"sprite_sheets", {0x64}},
                            {"source", "Base sheet $64; Follower_Draw"}}})}},
            {{"key", "oracle.sprites.dw"},
             {"name", "Dark World sprites"},
             {"group", "global_sprites"},
             {"index", 1},
             {"world", "dark_world"},
             {"use", "mixed"},
             {"description", "Four OBJ rows"},
             {"provenance", "Palettes_Load_SpriteMain $1B:EC9E"},
             {"ownership", "rom"},
             {"usages", json::array()}}})}};
}

TEST(PaletteCatalogTest, FiltersActorContextAndKeepsSheetNumberingExplicit) {
  auto parsed = PaletteCatalog::Parse(Document().dump());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  ASSERT_EQ(parsed->Filter("IMPA").size(), 1);
  EXPECT_EQ(parsed->Filter("Impa", "dark_world").size(), 0);
  EXPECT_EQ(parsed->Filter("", "light_world", "npcs").size(), 1);
  EXPECT_EQ(parsed->Filter("", "dark_world", "mixed").size(), 1);
  const auto& usage = parsed->entries()[0].usages[0];
  EXPECT_EQ(usage.obj_row, 4);
  EXPECT_EQ(usage.sprite_sheets, std::vector<int>({0x64}));
  EXPECT_EQ(usage.sprite_sheets[0] + 0x73, 0xD7);
}

TEST(PaletteCatalogTest, RefusesUnsupportedSchemaFieldsAndBadIdentities) {
  const auto original = Document();
  for (const auto* field : {"schema_version", "game_id", "entries"}) {
    auto bad = original;
    bad.erase(field);
    EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok()) << field;
  }
  auto bad = original;
  bad["schema_version"] = 2;
  EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok());
  bad = original;
  bad["silent_future_behavior"] = true;
  EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok());
  for (const auto& mutation : json::array({{{"group", "typo"}},
                                           {{"index", 2}},
                                           {{"index", -1}},
                                           {{"world", "unknown"}},
                                           {{"ownership", "writable"}},
                                           {{"use", "unknown"}},
                                           {{"extra", 0}},
                                           {{"provenance", ""}}})) {
    bad = original;
    bad["entries"][0].update(mutation);
    EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok()) << mutation;
  }
  bad = original;
  bad["entries"].push_back(bad["entries"][0]);
  EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok());
  bad = original;
  bad["entries"][1]["key"] = bad["entries"][0]["key"];
  EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok());
}

TEST(PaletteCatalogTest, RefusesContextlessRowsAndAmbiguousSheetFields) {
  for (const auto& mutation : json::array({{{"obj_row", 8}},
                                           {{"obj_row", -1}},
                                           {{"context", ""}},
                                           {{"use", "unknown"}},
                                           {{"sprite_sheets", {0x6C}}},
                                           {{"sprite_sheets", {-1}}},
                                           {{"gfx_sheets", {0xD7}}},
                                           {{"sprite_sheets", "64"}}})) {
    auto bad = Document();
    bad["entries"][0]["usages"][0].update(mutation);
    EXPECT_FALSE(PaletteCatalog::Parse(bad.dump()).ok()) << mutation;
  }
}

TEST(PaletteCatalogTest, BoundsInputSize) {
  EXPECT_FALSE(PaletteCatalog::Parse(std::string(1024 * 1024 + 1, ' ')).ok());
  EXPECT_FALSE(PaletteCatalog::Parse("not JSON").ok());
}

TEST(PaletteCatalogTest, ContainsResolvedPathsAndNeverChangesCatalogFiles) {
  namespace fs = std::filesystem;
  const auto root =
      fs::temp_directory_path() /
      ("yaze_palette_catalog_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  fs::create_directories(root / "project");
  const auto catalog = root / "project/catalog.json";
  const auto text = Document().dump();
  std::ofstream(catalog) << text;
  const auto project = (root / "project/game.yaze").string();
  EXPECT_TRUE(PaletteCatalog::LoadForProject(project, "catalog.json").ok());
  EXPECT_TRUE(PaletteCatalog::LoadForProject(project, catalog.string()).ok());
  EXPECT_FALSE(PaletteCatalog::LoadForProject(project, "../catalog.json").ok());
  std::ofstream(root / "outside.json") << text;
  EXPECT_FALSE(
      PaletteCatalog::LoadForProject(project, (root / "outside.json").string())
          .ok());
  fs::create_symlink(root / "outside.json", root / "project/escape.json");
  EXPECT_FALSE(PaletteCatalog::LoadForProject(project, "escape.json").ok());
  EXPECT_FALSE(
      PaletteCatalog::LoadForProject(project, "C:\\outside.json").ok());
  std::ifstream stream(catalog);
  EXPECT_EQ(std::string(std::istreambuf_iterator<char>(stream), {}), text);
  fs::remove_all(root);
}
}  // namespace
}  // namespace yaze::core
