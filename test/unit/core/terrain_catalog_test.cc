#include "core/terrain_catalog.h"

#include "gtest/gtest.h"

namespace yaze::core {
namespace {
nlohmann::json Example() {
  return {{"schema_version", 1},
          {"game_id", "any-game"},
          {"entries",
           {{{"id", "palm.canopy"},
             {"name", "Palm canopy"},
             {"category", "vegetation"},
             {"review_state", "suggested"},
             {"bindings",
              {{{"adapter", "example.tiles.v1"},
                {"tile_id", 42},
                {"definition_fingerprint", "definition-A"},
                {"profile_fingerprint", "sand-A"}}}}}}}};
}

TEST(TerrainCatalogTest, LabelsRequireExactAdapterDefinitionAndProfile) {
  auto catalog = TerrainCatalog::Parse(Example().dump());
  ASSERT_TRUE(catalog.ok()) << catalog.status();
  EXPECT_EQ(
      catalog->Resolve("example.tiles.v1", 42, "definition-A", "sand-A").size(),
      1);
  EXPECT_TRUE(catalog->Resolve("alttp", 42, "definition-A", "sand-A").empty());
  EXPECT_TRUE(catalog->Resolve("example.tiles.v1", 43, "definition-A", "sand-A")
                  .empty());
  EXPECT_TRUE(catalog->Resolve("example.tiles.v1", 42, "definition-B", "sand-A")
                  .empty());
  EXPECT_TRUE(
      catalog->Resolve("example.tiles.v1", 42, "definition-A", "grass-A")
          .empty());
}

TEST(TerrainCatalogTest, KeepsSuggestionsDistinctAndRoundTripsExtensions) {
  auto j = Example();
  j["entries"][0]["part"] = "northwest";
  j["entries"][0]["tags"] = {"sand", "foreground"};
  auto catalog = TerrainCatalog::Parse(j.dump());
  ASSERT_TRUE(catalog.ok());
  EXPECT_EQ(catalog->document(), j);
  EXPECT_EQ(catalog->Resolve("example.tiles.v1", 42, "definition-A",
                             "sand-A")[0]["review_state"],
            "suggested");
}

TEST(TerrainCatalogTest, RejectsMissingAndMalformedIdentity) {
  EXPECT_FALSE(TerrainCatalog::Parse("not json").ok());
  EXPECT_FALSE(TerrainCatalog::Parse("[]").ok());
  for (const char* field : {"adapter", "tile_id", "definition_fingerprint",
                            "profile_fingerprint"}) {
    auto j = Example();
    j["entries"][0]["bindings"][0].erase(field);
    EXPECT_FALSE(TerrainCatalog::Parse(j.dump()).ok()) << field;
  }
  for (auto id : {nlohmann::json(-1), nlohmann::json(3.5), nlohmann::json("42"),
                  nlohmann::json(1ULL << 40)}) {
    auto j = Example();
    j["entries"][0]["bindings"][0]["tile_id"] = id;
    EXPECT_FALSE(TerrainCatalog::Parse(j.dump()).ok());
  }
}

TEST(TerrainCatalogTest, RejectsDuplicatesUnsupportedVersionAndReviewState) {
  auto j = Example();
  j["entries"].push_back(j["entries"][0]);
  EXPECT_FALSE(TerrainCatalog::Parse(j.dump()).ok());
  j = Example();
  j["schema_version"] = 2;
  EXPECT_FALSE(TerrainCatalog::Parse(j.dump()).ok());
  j = Example();
  j["entries"][0]["review_state"] = "guessed-but-safe";
  EXPECT_FALSE(TerrainCatalog::Parse(j.dump()).ok());
}

TEST(TerrainCatalogTest, SupportsEmptyProjectCatalogAndKnownFingerprintVector) {
  EXPECT_TRUE(TerrainCatalog::Parse(
                  R"({"schema_version":1,"game_id":"custom","entries":[]})")
                  .ok());
  EXPECT_EQ(TerrainFingerprint("hello"), "fnv1a64:a430d84680aabd0b");
  EXPECT_NE(TerrainFingerprint("hello"), TerrainFingerprint("Hello"));
}
}  // namespace
}  // namespace yaze::core
