#include "core/lyra_terrain_document.h"

#include <functional>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
LyraTerrainProfile Profile() {
  LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture-v1";
  profile.graphics_fingerprint = "graphics:fixture-v1";
  profile.palette_fingerprint = "palette:fixture-v1";
  profile.animation_fingerprint = "animation:fixture-v1";
  return profile;
}

TEST(LyraTerrainDocumentTest, OneStrokeCrossesA0A8B0AndUndoReturnsToBaseline) {
  auto created = LyraTerrainDocument::Create(Profile(), 7);
  ASSERT_TRUE(created.ok()) << created.status();
  auto& document = *created;
  ASSERT_TRUE(document.BeginStroke().ok());
  ASSERT_TRUE(document.Paint(3, 31, 101).ok());
  ASSERT_TRUE(document.Paint(3, 32, 102).ok());
  ASSERT_TRUE(document.Paint(3, 64, 103).ok());
  ASSERT_TRUE(document.Paint(3, 32, 104).ok());
  auto stroke = document.EndStroke();
  ASSERT_TRUE(stroke.ok()) << stroke.status();
  ASSERT_EQ(stroke->changes.size(), 3);
  EXPECT_EQ(document.MapTile(0xA0, 3, 31).value(), 101);
  EXPECT_EQ(document.MapTile(0xA8, 3, 0).value(), 104);
  EXPECT_EQ(document.MapTile(0xB0, 3, 0).value(), 103);
  EXPECT_TRUE(document.dirty());
  ASSERT_TRUE(document.ApplyStroke(*stroke, true).ok());
  EXPECT_FALSE(document.dirty());
  EXPECT_EQ(document.MapTile(0xA8, 3, 0).value(), 7);
  ASSERT_TRUE(document.ApplyStroke(*stroke, false).ok());
  EXPECT_TRUE(document.dirty());
  document.MarkSaved();
  EXPECT_FALSE(document.dirty());
  ASSERT_TRUE(document.ApplyStroke(*stroke, true).ok());
  EXPECT_TRUE(document.dirty());
  ASSERT_TRUE(document.ApplyStroke(*stroke, false).ok());
  EXPECT_FALSE(document.dirty());
}

TEST(LyraTerrainDocumentTest, RoundTripRetainsEveryMapAndExactProfile) {
  auto created = LyraTerrainDocument::Create(Profile(), 11);
  ASSERT_TRUE(created.ok());
  ASSERT_TRUE(created->BeginStroke().ok());
  for (int i = 0; i < LyraTerrainDocument::kMapCount; ++i) {
    ASSERT_TRUE(created->Paint((i % 8) * 32, (i / 8) * 32, 100 + i).ok());
  }
  ASSERT_TRUE(created->EndStroke().ok());
  auto reopened = LyraTerrainDocument::Parse(created->Serialize(), Profile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->maps(), created->maps());
  EXPECT_EQ(reopened->profile(), Profile());
  EXPECT_FALSE(reopened->dirty());
  EXPECT_EQ(reopened->MapTile(0xBF, 0, 0).value(), 131);
}

TEST(LyraTerrainDocumentTest, RefusesMalformedSchemaDimensionsMapsAndTiles) {
  const auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  const auto original = nlohmann::json::parse(document->Serialize());
  using Mutation = std::function<void(nlohmann::json&)>;
  const std::vector<Mutation> mutations = {
      [](auto& j) { j["schema_version"] = 2; },
      [](auto& j) { j["columns"] = 4; },
      [](auto& j) { j["rows"] = 8; },
      [](auto& j) { j["map_width"] = 16; },
      [](auto& j) { j["map_height"] = 16; },
      [](auto& j) { j["tile_pixels"] = 32; },
      [](auto& j) { j["columns"] = 8.0; },
      [](auto& j) { j["maps"].erase(0); },
      [](auto& j) { j["maps"][1]["map_id"] = 0xA0; },
      [](auto& j) { j["maps"][0]["map_id"] = 0x9F; },
      [](auto& j) { j["maps"][31]["map_id"] = 0xC0; },
      [](auto& j) { j["maps"][0]["tiles16"].erase(0); },
      [](auto& j) { j["maps"][0]["tiles16"][0].erase(0); },
      [](auto& j) { j["maps"][0]["tiles16"][0][0] = -1; },
      [](auto& j) { j["maps"][0]["tiles16"][0][0] = 4096; },
      [](auto& j) { j["maps"][0]["tiles16"][0][0] = 1.0; },
      [](auto& j) { j["profile"]["main_palette"] = 0.0; },
      [](auto& j) { j["profile"]["graphics_sheets"][0] = 58.0; },
  };
  for (size_t i = 0; i < mutations.size(); ++i) {
    SCOPED_TRACE(i);
    auto malformed = original;
    mutations[i](malformed);
    EXPECT_FALSE(LyraTerrainDocument::Parse(malformed.dump(), Profile()).ok());
  }
  EXPECT_FALSE(LyraTerrainDocument::Parse("{", Profile()).ok());
  EXPECT_FALSE(LyraTerrainDocument::Parse("null", Profile()).ok());
  auto duplicate_key = document->Serialize();
  duplicate_key.insert(1, "\"columns\":8,");
  EXPECT_FALSE(LyraTerrainDocument::Parse(duplicate_key, Profile()).ok());
}

TEST(LyraTerrainDocumentTest, RefusesEveryMismatchedAssetBinding) {
  auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  using Mutation = std::function<void(LyraTerrainProfile&)>;
  const std::vector<Mutation> mutations = {
      [](auto& p) { p.graphics_sheets[0] = 0x38; },
      [](auto& p) { p.main_palette = 1; },
      [](auto& p) { p.palette_set = 10; },
      [](auto& p) { p.animated_sheet = 0x59; },
      [](auto& p) { p.tile_count = 4097; },
      [](auto& p) { p.tile16_definition_fingerprint += "-changed"; },
      [](auto& p) { p.graphics_fingerprint += "-changed"; },
      [](auto& p) { p.palette_fingerprint += "-changed"; },
      [](auto& p) { p.animation_fingerprint += "-changed"; },
  };
  for (size_t i = 0; i < mutations.size(); ++i) {
    SCOPED_TRACE(i);
    auto changed = Profile();
    mutations[i](changed);
    EXPECT_EQ(LyraTerrainDocument::Parse(document->Serialize(), changed)
                  .status()
                  .code(),
              absl::StatusCode::kFailedPrecondition);
  }
}

TEST(LyraTerrainDocumentTest, RefusesUnboundAtlasAndOutOfRangePaint) {
  EXPECT_FALSE(LyraTerrainDocument::Create(LyraTerrainProfile{}).ok());
  EXPECT_FALSE(LyraTerrainDocument::Create(Profile(), 4096).ok());
  auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  EXPECT_FALSE(document->Paint(0, 0, 1).ok());
  EXPECT_FALSE(document->MapTile(0xA0, 32, 0).ok());
  EXPECT_FALSE(document->tile(256, 0).ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  EXPECT_FALSE(document->BeginStroke().ok());
  EXPECT_FALSE(document->Paint(-1, 0, 1).ok());
  EXPECT_FALSE(document->Paint(0, 128, 1).ok());
  EXPECT_FALSE(document->Paint(0, 0, 4096).ok());
  EXPECT_FALSE(document->dirty());
  EXPECT_TRUE(document->EndStroke()->empty());
}

TEST(LyraTerrainDocumentTest, CancelAndNetZeroStrokeDoNotCreateDirtyWork) {
  auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(31, 0, 5).ok());
  ASSERT_TRUE(document->Paint(32, 0, 6).ok());
  document->CancelStroke();
  EXPECT_FALSE(document->dirty());
  EXPECT_FALSE(document->stroke_active());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(0, 0, 5).ok());
  ASSERT_TRUE(document->Paint(0, 0, 0).ok());
  auto stroke = document->EndStroke();
  ASSERT_TRUE(stroke.ok());
  EXPECT_TRUE(stroke->empty());
  EXPECT_FALSE(document->dirty());
}

TEST(LyraTerrainDocumentTest, StaleAndMalformedHistoryAreAtomic) {
  auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(1, 31, 5).ok());
  ASSERT_TRUE(document->Paint(1, 32, 6).ok());
  auto stroke = document->EndStroke();
  ASSERT_TRUE(stroke.ok());
  const auto before = document->maps();
  auto stale = *stroke;
  stale.changes.back().after = 12;
  EXPECT_FALSE(document->ApplyStroke(stale, true).ok());
  EXPECT_EQ(document->maps(), before);
  auto duplicate = *stroke;
  duplicate.changes.push_back(duplicate.changes.front());
  EXPECT_FALSE(document->ApplyStroke(duplicate, true).ok());
  EXPECT_EQ(document->maps(), before);
  auto invalid = *stroke;
  invalid.changes.back().map_id = 0x9F;
  EXPECT_FALSE(document->ApplyStroke(invalid, true).ok());
  EXPECT_EQ(document->maps(), before);
  ASSERT_TRUE(document->BeginStroke().ok());
  EXPECT_FALSE(document->ApplyStroke(*stroke, true).ok());
  document->CancelStroke();
}

}  // namespace
}  // namespace yaze::core
