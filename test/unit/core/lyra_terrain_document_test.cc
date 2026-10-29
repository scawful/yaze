#include "core/lyra_terrain_document.h"

#include <algorithm>
#include <functional>
#include <limits>

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

nlohmann::json AsSchema2(nlohmann::json j) {
  j["schema_version"] = 2;
  j.erase("edge_connections");
  return j;
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

TEST(LyraTerrainDocumentTest,
     DefinitionRebindPreservesDocumentTerrainAndAuthoring) {
  auto created = LyraTerrainDocument::Create(Profile(), 7);
  ASSERT_TRUE(created.ok());
  auto& document = *created;
  const auto* identity = &document;
  ASSERT_TRUE(document.BeginStroke().ok());
  for (int map = 0; map < LyraTerrainDocument::kMapCount; ++map) {
    ASSERT_TRUE(document.Paint((map % 8) * 32, (map / 8) * 32, 100 + map).ok());
  }
  ASSERT_TRUE(document.EndStroke().ok());
  auto authoring = document.authoring();
  authoring.gameplay_areas.push_back(
      {0xA2, LyraGameplayAreaSize::kSmall, {0xA2}});
  ASSERT_TRUE(document.SetAuthoring(document.authoring(), authoring).ok());
  document.MarkSaved();
  const auto maps = document.maps();
  auto changed = Profile();
  changed.tile16_definition_fingerprint = "definitions:paint-f63-q2";

  ASSERT_TRUE(document.RebindTile16Definitions(Profile(), changed).ok());
  EXPECT_EQ(&document, identity);
  EXPECT_EQ(document.profile(), changed);
  EXPECT_EQ(document.maps(), maps);
  EXPECT_EQ(document.authoring(), authoring);
  EXPECT_TRUE(document.profile_dirty());
  EXPECT_TRUE(document.dirty());
  auto reopened = LyraTerrainDocument::Parse(document.Serialize(), changed);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->maps(), maps);
  EXPECT_EQ(reopened->authoring(), authoring);
  EXPECT_FALSE(reopened->dirty());
}

TEST(LyraTerrainDocumentTest, SavedDefinitionRebindUndoRedoTracksBaseline) {
  auto document = LyraTerrainDocument::Create(Profile(), 7);
  ASSERT_TRUE(document.ok());
  auto changed = Profile();
  changed.tile16_definition_fingerprint = "definitions:paint-f63-q2";
  ASSERT_TRUE(document->RebindTile16Definitions(Profile(), changed).ok());
  EXPECT_TRUE(document->profile_dirty());
  document->MarkSaved();
  EXPECT_FALSE(document->profile_dirty());
  EXPECT_FALSE(document->dirty());

  ASSERT_TRUE(document->RebindTile16Definitions(changed, Profile()).ok());
  EXPECT_TRUE(document->profile_dirty());
  EXPECT_TRUE(document->dirty());
  ASSERT_TRUE(document->RebindTile16Definitions(Profile(), changed).ok());
  EXPECT_FALSE(document->profile_dirty());
  EXPECT_FALSE(document->dirty());
}

TEST(LyraTerrainDocumentTest,
     DefinitionRebindRefusesExternalAssetsAndStaleExpectedProfileAtomically) {
  auto document = LyraTerrainDocument::Create(Profile(), 7);
  ASSERT_TRUE(document.ok());
  const auto before = document->Serialize();
  using Mutation = std::function<void(LyraTerrainProfile&)>;
  const std::vector<Mutation> external_changes = {
      [](auto& p) { p.graphics_sheets[0] ^= 1; },
      [](auto& p) { p.main_palette ^= 1; },
      [](auto& p) { p.palette_set ^= 1; },
      [](auto& p) { p.animated_sheet ^= 1; },
      [](auto& p) { ++p.tile_count; },
      [](auto& p) { p.graphics_fingerprint += "-external"; },
      [](auto& p) { p.palette_fingerprint += "-external"; },
      [](auto& p) { p.animation_fingerprint += "-external"; },
  };
  for (size_t i = 0; i < external_changes.size(); ++i) {
    SCOPED_TRACE(i);
    auto changed = Profile();
    changed.tile16_definition_fingerprint = "definitions:paint-f63-q2";
    external_changes[i](changed);
    EXPECT_FALSE(document->RebindTile16Definitions(Profile(), changed).ok());
    EXPECT_EQ(document->Serialize(), before);
    EXPECT_FALSE(document->profile_dirty());
    EXPECT_FALSE(document->dirty());
  }
  auto stale = Profile();
  stale.tile16_definition_fingerprint = "definitions:stale";
  auto changed = Profile();
  changed.tile16_definition_fingerprint = "definitions:paint-f63-q2";
  EXPECT_FALSE(document->RebindTile16Definitions(stale, changed).ok());
  EXPECT_EQ(document->Serialize(), before);
  EXPECT_FALSE(document->dirty());
}

TEST(LyraTerrainDocumentTest,
     FailedPublicationRestoresSavedBaselineWithoutReplacingLiveDrafts) {
  auto created = LyraTerrainDocument::Create(Profile(), 7);
  ASSERT_TRUE(created.ok());
  auto& document = *created;
  const auto* identity = &document;
  const auto baseline = document;
  auto changed = Profile();
  changed.tile16_definition_fingerprint = "definitions:paint-f63-q2";
  ASSERT_TRUE(document.RebindTile16Definitions(Profile(), changed).ok());
  ASSERT_TRUE(document.BeginStroke().ok());
  ASSERT_TRUE(document.Paint(64, 0, 0xF63).ok());
  ASSERT_TRUE(document.EndStroke().ok());
  auto authoring = document.authoring();
  authoring.gameplay_areas.push_back(
      {0xA2, LyraGameplayAreaSize::kSmall, {0xA2}});
  ASSERT_TRUE(document.SetAuthoring(document.authoring(), authoring).ok());
  const auto live_maps = document.maps();
  document.MarkSaved();
  ASSERT_FALSE(document.dirty());

  document.RestoreSavedBaseline(baseline);
  EXPECT_EQ(&document, identity);
  EXPECT_EQ(document.profile(), changed);
  EXPECT_EQ(document.maps(), live_maps);
  EXPECT_EQ(document.authoring(), authoring);
  EXPECT_EQ(document.MapTile(0xA2, 0, 0).value(), 0xF63);
  EXPECT_TRUE(document.profile_dirty());
  EXPECT_TRUE(document.dirty());
}

TEST(LyraTerrainDocumentTest, RefusesMalformedSchemaDimensionsMapsAndTiles) {
  const auto document = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(document.ok());
  const auto original = nlohmann::json::parse(document->Serialize());
  using Mutation = std::function<void(nlohmann::json&)>;
  const std::vector<Mutation> mutations = {
      [](auto& j) {
        j["schema_version"] = LyraTerrainDocument::kSchemaVersion + 1;
      },
      [](auto& j) { j["schema_version"] = uint64_t{4294967298}; },
      [](auto& j) { j["schema_version"] = int64_t{-4294967294}; },
      [](auto& j) {
        j["maps"][0]["authoring"]["properties"]["sprite_graphics"][0] =
            uint64_t{18446744073709551615ULL};
      },
      [](auto& j) {
        j["maps"][0]["authoring"]["properties"]["sprite_palettes"][0] =
            uint64_t{9223372036854775808ULL};
      },
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

TEST(LyraTerrainDocumentTest, Schema1WaterFilledMapsMigrationExact) {
  // Construct a strict schema 1 JSON string with fill_tile = 5 (water) across all 32 maps.
  nlohmann::json j;
  j["schema"] = "yaze.lyra-terrain";
  j["schema_version"] = 1;
  j["columns"] = 8;
  j["rows"] = 4;
  j["map_width"] = 32;
  j["map_height"] = 32;
  j["tile_pixels"] = 16;
  j["profile"] = {{"graphics_sheets", Profile().graphics_sheets},
                  {"main_palette", Profile().main_palette},
                  {"palette_set", Profile().palette_set},
                  {"animated_sheet", Profile().animated_sheet},
                  {"tile_count", Profile().tile_count},
                  {"tile16_definition_fingerprint",
                   Profile().tile16_definition_fingerprint},
                  {"graphics_fingerprint", Profile().graphics_fingerprint},
                  {"palette_fingerprint", Profile().palette_fingerprint},
                  {"animation_fingerprint", Profile().animation_fingerprint}};
  j["maps"] = nlohmann::json::array();
  for (int m = 0; m < 32; ++m) {
    nlohmann::json rows = nlohmann::json::array();
    for (int y = 0; y < 32; ++y) {
      rows.push_back(std::vector<int>(32, 5));
    }
    j["maps"].push_back({{"map_id", 0xA0 + m}, {"tiles16", rows}});
  }

  auto parsed = LyraTerrainDocument::Parse(j.dump(), Profile());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_FALSE(parsed->dirty());

  // Check that every tile in every map is unchanged (5).
  for (int m = 0; m < 32; ++m) {
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 32; ++x) {
        EXPECT_EQ(parsed->MapTile(0xA0 + m, x, y).value(), 5);
      }
    }
    // Check that properties migrated to exact defaults.
    EXPECT_EQ(parsed->authoring().maps[m].properties, LyraMapProperties{});
    EXPECT_TRUE(parsed->authoring().maps[m].sprites.empty());
    EXPECT_TRUE(parsed->authoring().maps[m].doors.empty());
    EXPECT_TRUE(parsed->authoring().maps[m].exits.empty());
    EXPECT_EQ(parsed->authoring().maps[m].asset_bindings,
              (std::array<std::string, 3>{"", "", ""}));
  }
  EXPECT_TRUE(parsed->authoring().flight_points.empty());
  EXPECT_TRUE(parsed->authoring().edge_connections.empty());
  EXPECT_EQ(nlohmann::json::parse(parsed->Serialize())["schema_version"], 3);
}

TEST(LyraTerrainDocumentTest, Schema2FullRoundtripWithAuthoringEntities) {
  auto created = LyraTerrainDocument::Create(Profile(), 1);
  ASSERT_TRUE(created.ok());

  LyraAuthoringState auth_state = created->authoring();
  // Map 0: bound fingerprints and non-default properties
  auth_state.maps[0].asset_bindings = {"bind:defs-1", "bind:gfx-1",
                                       "bind:pal-1"};
  auth_state.maps[0].properties.main_palette = 2;
  auth_state.maps[0].properties.message_id = 0x1234;
  auth_state.maps[0].properties.sprite_graphics = {0x00, 0xFF, -1};
  auth_state.maps[0].sprites.push_back({10, 0x15, 5, 6, 1});
  auth_state.maps[0].doors.push_back({20, 10, 12, 0x50});
  auth_state.maps[0].exits.push_back({30, 0x100, 15, 16, 0x20});

  // Map 1: entity with nullable fields omitted / empty
  auth_state.maps[1].doors.push_back({21, 1, 2, std::nullopt});
  auth_state.maps[1].exits.push_back({31, 10, 3, 4, std::nullopt});

  auth_state.maps[1].properties.main_palette = 1;
  auth_state.maps[1].asset_bindings = {"neighbor:0", "neighbor:1",
                                       "neighbor:2"};
  // Flight points
  auth_state.flight_points.push_back(
      {40, 0xA0, 8, 9, "Village Center", std::nullopt});
  auth_state.flight_points.push_back(
      {41, 0xBF, 31, 31, "Tail Point", std::nullopt});

  ASSERT_TRUE(created->SetAuthoring(created->authoring(), auth_state).ok());
  EXPECT_TRUE(created->dirty());
  created->MarkSaved();
  EXPECT_FALSE(created->dirty());

  std::string serialized = created->Serialize();
  auto roundtripped = LyraTerrainDocument::Parse(serialized, Profile());
  ASSERT_TRUE(roundtripped.ok()) << roundtripped.status();
  EXPECT_EQ(roundtripped->authoring(), auth_state);
  EXPECT_FALSE(roundtripped->dirty());

  // The exact schema 2 shape still opens and migrates to no connections.
  auto legacy = AsSchema2(nlohmann::json::parse(serialized));
  auto migrated = LyraTerrainDocument::Parse(legacy.dump(), Profile());
  ASSERT_TRUE(migrated.ok()) << migrated.status();
  EXPECT_EQ(migrated->authoring(), auth_state);
  EXPECT_TRUE(migrated->authoring().edge_connections.empty());
  EXPECT_FALSE(migrated->dirty());
  EXPECT_EQ(migrated->Serialize(), serialized);
}

TEST(LyraTerrainDocumentTest, ExitRowsAreBoundedBy79NativeRows) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  auto valid = doc->authoring();
  valid.maps[0].exits.push_back({1, 0x105, 0, 0, 78});
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), valid).ok());
  auto invalid = valid;
  invalid.maps[0].exits[0].exit_row = 79;
  EXPECT_FALSE(doc->SetAuthoring(valid, invalid).ok());
  auto json = nlohmann::json::parse(doc->Serialize());
  json["maps"][0]["authoring"]["exits"][0]["exit_row"] = 79;
  EXPECT_FALSE(LyraTerrainDocument::Parse(json.dump(), Profile()).ok());
}

TEST(LyraTerrainDocumentTest, PropertyOnlyDirtySavedUndoViaSetAuthoring) {
  auto doc = LyraTerrainDocument::Create(Profile(), 0);
  ASSERT_TRUE(doc.ok());
  EXPECT_FALSE(doc->dirty());

  const auto baseline = doc->authoring();
  auto modified = baseline;
  modified.maps[0].asset_bindings = {"fp:1", "fp:2", "fp:3"};
  modified.maps[0].properties.message_id = 42;

  // Stale CAS refusal
  LyraAuthoringState wrong_before = baseline;
  wrong_before.maps[0].properties.message_id = 999;
  EXPECT_FALSE(doc->SetAuthoring(wrong_before, modified).ok());
  EXPECT_FALSE(doc->dirty());

  // Active stroke CAS refusal
  ASSERT_TRUE(doc->BeginStroke().ok());
  EXPECT_FALSE(doc->SetAuthoring(baseline, modified).ok());
  doc->CancelStroke();

  // Valid SetAuthoring
  ASSERT_TRUE(doc->SetAuthoring(baseline, modified).ok());
  EXPECT_TRUE(doc->dirty());

  // Save
  doc->MarkSaved();
  EXPECT_FALSE(doc->dirty());

  // Undo back to baseline
  ASSERT_TRUE(doc->SetAuthoring(modified, baseline).ok());
  EXPECT_TRUE(doc->dirty());
  ASSERT_TRUE(doc->SetAuthoring(baseline, modified).ok());
  EXPECT_FALSE(doc->dirty());
}

TEST(LyraTerrainDocumentTest, BackgroundPaletteUsesSchema4OnlyWhenPresent) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  EXPECT_EQ(original["schema_version"], 3);
  for (const auto& map : original["maps"]) {
    EXPECT_FALSE(map["authoring"]["properties"].contains("background_palette"));
  }

  auto state = doc->authoring();
  std::array<uint16_t, 128> palette{};
  for (size_t i = 0; i < palette.size(); ++i) {
    palette[i] = static_cast<uint16_t>(i * 17);
  }
  palette.back() = 0x7FFF;
  state.maps[0].properties.background_palette = palette;
  state.maps[0].asset_bindings = {"definitions:0", "graphics:0", "palette:0"};
  state.maps[31].properties.message_id = 42;
  state.maps[31].asset_bindings = {"definitions:31", "graphics:31",
                                   "palette:31"};
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), state).ok());

  const auto serialized = doc->Serialize();
  const auto json = nlohmann::json::parse(serialized);
  EXPECT_EQ(json["schema_version"], 4);
  EXPECT_EQ(json["maps"][0]["authoring"]["properties"]["background_palette"],
            nlohmann::json(palette));
  EXPECT_FALSE(json["maps"][1]["authoring"]["properties"].contains(
      "background_palette"));
  EXPECT_FALSE(json["maps"][31]["authoring"]["properties"].contains(
      "background_palette"));
  auto schema4_without_override = json;
  schema4_without_override["maps"][0]["authoring"]["properties"].erase(
      "background_palette");
  EXPECT_FALSE(
      LyraTerrainDocument::Parse(schema4_without_override.dump(), Profile())
          .ok());
  auto reopened = LyraTerrainDocument::Parse(serialized, Profile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->authoring(), state);
  EXPECT_EQ(reopened->Serialize(), serialized);
  EXPECT_FALSE(reopened->dirty());

  auto native = state;
  native.maps[0].properties.background_palette.reset();
  ASSERT_TRUE(doc->SetAuthoring(state, native).ok());
  EXPECT_EQ(nlohmann::json::parse(doc->Serialize())["schema_version"], 3);
}

TEST(LyraTerrainDocumentTest,
     BackgroundPaletteRequiresExactBoundedIntegerArray) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  auto state = doc->authoring();
  state.maps[0].asset_bindings = {"definitions:0", "graphics:0", "palette:0"};
  state.maps[0].properties.background_palette = std::array<uint16_t, 128>{};
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), state).ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  ASSERT_TRUE(LyraTerrainDocument::Parse(original.dump(), Profile()).ok());
  using Json = nlohmann::json;
  using Mutation = std::function<void(Json&)>;
  const std::vector<Mutation> mutations = {
      [](Json& p) { p = nullptr; },
      [](Json& p) { p = Json::object(); },
      [](Json& p) { p = "native"; },
      [](Json& p) { p.erase(0); },
      [](Json& p) { p.push_back(0); },
      [](Json& p) { p[0] = -1; },
      [](Json& p) { p[127] = 0x8000; },
      [](Json& p) { p[0] = 0.0; },
      [](Json& p) { p[0] = true; },
      [](Json& p) { p[0] = uint64_t{18446744073709551615ULL}; },
  };
  for (size_t i = 0; i < mutations.size(); ++i) {
    SCOPED_TRACE(i);
    auto malformed = original;
    mutations[i](
        malformed["maps"][0]["authoring"]["properties"]["background_palette"]);
    EXPECT_FALSE(LyraTerrainDocument::Parse(malformed.dump(), Profile()).ok());
  }

  auto invalid = state;
  invalid.maps[0].properties.background_palette->back() = 0x8000;
  doc->MarkSaved();
  EXPECT_FALSE(doc->SetAuthoring(state, invalid).ok());
  EXPECT_EQ(doc->authoring(), state);
  EXPECT_FALSE(doc->dirty());
}

TEST(LyraTerrainDocumentTest, LegacySchemasRefuseBackgroundPaletteOverrides) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  for (const int version : {2, 3}) {
    SCOPED_TRACE(version);
    auto legacy = version == 2 ? AsSchema2(original) : original;
    ASSERT_TRUE(LyraTerrainDocument::Parse(legacy.dump(), Profile()).ok());
    legacy["maps"][0]["authoring"]["properties"]["background_palette"] =
        std::array<uint16_t, 128>{};
    legacy["maps"][0]["authoring"]["asset_bindings"] =
        std::array<std::string, 3>{"definitions:0", "graphics:0", "palette:0"};
    EXPECT_FALSE(LyraTerrainDocument::Parse(legacy.dump(), Profile()).ok());
  }

  auto schema1 = AsSchema2(original);
  schema1["schema_version"] = 1;
  schema1.erase("flight_points");
  for (auto& map : schema1["maps"])
    map.erase("authoring");
  ASSERT_TRUE(LyraTerrainDocument::Parse(schema1.dump(), Profile()).ok());
  schema1["maps"][0]["authoring"] = original["maps"][0]["authoring"];
  schema1["maps"][0]["authoring"]["properties"]["background_palette"] =
      std::array<uint16_t, 128>{};
  EXPECT_FALSE(LyraTerrainDocument::Parse(schema1.dump(), Profile()).ok());
}

TEST(LyraTerrainDocumentTest,
     BackgroundPaletteUndoAndNativeResetTrackDirtyState) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto baseline = doc->authoring();
  auto override_state = baseline;
  override_state.maps[4].asset_bindings = {"defs:4", "gfx:4", "palette:4"};
  std::array<uint16_t, 128> palette{};
  palette[0] = 0x001F;
  palette[127] = 0x7C00;
  override_state.maps[4].properties.background_palette = palette;
  ASSERT_TRUE(doc->SetAuthoring(baseline, override_state).ok());
  EXPECT_TRUE(doc->dirty());
  EXPECT_FALSE(
      doc->authoring().maps[3].properties.background_palette.has_value());
  doc->MarkSaved();
  EXPECT_FALSE(doc->dirty());

  ASSERT_TRUE(doc->SetAuthoring(override_state, baseline).ok());
  EXPECT_TRUE(doc->dirty());
  EXPECT_FALSE(
      doc->authoring().maps[4].properties.background_palette.has_value());
  ASSERT_TRUE(doc->SetAuthoring(baseline, override_state).ok());
  EXPECT_FALSE(doc->dirty());

  auto native = override_state;
  native.maps[4].properties.background_palette.reset();
  ASSERT_TRUE(doc->SetAuthoring(override_state, native).ok());
  EXPECT_TRUE(doc->dirty());
  ASSERT_TRUE(doc->SetAuthoring(native, override_state).ok());
  EXPECT_FALSE(doc->dirty());
  EXPECT_EQ(doc->authoring().maps[4].properties.background_palette, palette);
}

TEST(LyraTerrainDocumentTest,
     RefusesDuplicateIdsPositionsFloatsAndInvalidAuthoring) {
  auto doc = LyraTerrainDocument::Create(Profile(), 0);
  ASSERT_TRUE(doc.ok());
  const auto base = doc->authoring();

  // Duplicate sprite ID
  {
    auto bad = base;
    bad.maps[0].sprites.push_back({1, 0, 0, 0, 0});
    bad.maps[1].sprites.push_back({1, 0, 1, 1, 0});
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // Zero ID
  {
    auto bad = base;
    bad.maps[0].doors.push_back({0, 1, 1, std::nullopt});
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // Duplicate doors at same (map, x, y)
  {
    auto bad = base;
    bad.maps[0].doors.push_back({1, 5, 5, std::nullopt});
    bad.maps[0].doors.push_back({2, 5, 5, std::nullopt});
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // Sprite overlapping positions ARE allowed
  {
    auto good = base;
    good.maps[0].sprites.push_back({1, 0, 5, 5, 0});
    good.maps[0].sprites.push_back({2, 1, 5, 5, 0});
    EXPECT_TRUE(doc->SetAuthoring(base, good).ok());
    ASSERT_TRUE(doc->SetAuthoring(good, base).ok());
  }
  // Duplicate flight names ARE allowed with unique IDs
  {
    auto good = base;
    good.flight_points.push_back({1, 0xA0, 0, 0, "SameName", std::nullopt});
    good.flight_points.push_back({2, 0xA1, 0, 0, "SameName", std::nullopt});
    EXPECT_TRUE(doc->SetAuthoring(base, good).ok());
    ASSERT_TRUE(doc->SetAuthoring(good, base).ok());
  }
  // Non-null runtime_slot in flight point rejected
  {
    auto bad = base;
    bad.flight_points.push_back({1, 0xA0, 0, 0, "Name", 0});
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // Empty bindings with non-default properties rejected
  {
    auto bad = base;
    bad.maps[0].asset_bindings = {"", "", ""};
    bad.maps[0].properties.main_palette = 5;
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // Partial empty bindings rejected
  {
    auto bad = base;
    bad.maps[0].asset_bindings = {"bind:1", "", "bind:3"};
    EXPECT_FALSE(doc->SetAuthoring(base, bad).ok());
  }
  // JSON parse mutations: float fields and unknown keys
  const auto original = nlohmann::json::parse(doc->Serialize());
  {
    auto j = original;
    j["maps"][0]["authoring"]["properties"]["main_palette"] = 1.5;
    EXPECT_FALSE(LyraTerrainDocument::Parse(j.dump(), Profile()).ok());
  }
  {
    auto j = original;
    j["maps"][0]["authoring"]["unknown_key"] = 123;
    EXPECT_FALSE(LyraTerrainDocument::Parse(j.dump(), Profile()).ok());
  }
  {
    auto j = original;
    j["maps"][0]["authoring"]["sprites"].push_back(
        {{"id", 1}, {"sprite_id", 0}, {"x", 0.0}, {"y", 0}, {"state", 0}});
    EXPECT_FALSE(LyraTerrainDocument::Parse(j.dump(), Profile()).ok());
  }
  {
    auto j = original;
    j["maps"][0]["authoring"]["exits"].push_back({{"id", 1},
                                                  {"room_id", 0x128},
                                                  {"x", 0},
                                                  {"y", 0},
                                                  {"exit_row", nullptr}});
    EXPECT_FALSE(LyraTerrainDocument::Parse(j.dump(), Profile()).ok());
  }
}

LyraAuthoringState WithEdgeConnections(const LyraAuthoringState& base) {
  auto state = base;
  state.edge_connections.push_back({1, 0x99, 0xA1, std::nullopt});
  state.edge_connections.push_back({2, 0xA0, 0xA1, LyraEdgePassage{3, 7}});
  state.edge_connections.push_back({5, 0xB7, 0xBF, LyraEdgePassage{31, 31}});
  return state;
}

TEST(LyraTerrainDocumentTest, EdgeAdjacencyIsNaturalCardinalWithinLyraEdges) {
  int pairs = 0;
  for (int a = 0x90; a < 0xC8; ++a) {
    for (int b = a + 1; b < 0xC8; ++b) {
      EXPECT_EQ(LyraEdgeMapsAdjacent(a, b), LyraEdgeMapsAdjacent(b, a));
      pairs += LyraEdgeMapsAdjacent(a, b) ? 1 : 0;
    }
  }
  // 28 east-west and 24 north-south pairs in $A0-$BF, plus $98-$9B south.
  EXPECT_EQ(pairs, 56);
  EXPECT_TRUE(LyraEdgeMapsAdjacent(0x9B, 0xA3));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0x9C, 0xA4));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0x98, 0x99));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0xA7, 0xA8));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0xA0, 0xA9));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0xA0, 0xA0));
  EXPECT_FALSE(LyraEdgeMapsAdjacent(0xB8, 0xC0));
}

TEST(LyraTerrainDocumentTest, Schema3RoundtripsAssignedAndUnassignedPassages) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto state = WithEdgeConnections(doc->authoring());
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), state).ok());

  const std::string serialized = doc->Serialize();
  const auto json = nlohmann::json::parse(serialized);
  EXPECT_EQ(json["schema_version"], 3);
  ASSERT_EQ(json["edge_connections"].size(), 3u);
  EXPECT_EQ(json["edge_connections"][0],
            nlohmann::json::parse(
                R"({"id":1,"map_a":153,"map_b":161,"passage":null})"));
  EXPECT_EQ(json["edge_connections"][1],
            nlohmann::json::parse(R"({"id":2,"map_a":160,"map_b":161,
                "passage":{"first_tile":3,"last_tile":7}})"));

  auto reopened = LyraTerrainDocument::Parse(serialized, Profile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->authoring(), state);
  EXPECT_FALSE(reopened->authoring().edge_connections[0].passage.has_value());
  EXPECT_FALSE(reopened->dirty());
  EXPECT_EQ(reopened->Serialize(), serialized);
}

TEST(LyraTerrainDocumentTest, LegacyAndSchema3RootKeysAreExact) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto current = nlohmann::json::parse(doc->Serialize());

  auto legacy_with_edges = current;
  legacy_with_edges["schema_version"] = 2;
  EXPECT_EQ(LyraTerrainDocument::Parse(legacy_with_edges.dump(), Profile())
                .status()
                .message(),
            "Malformed Lyra schema 2 header keys");

  auto missing_edges = current;
  missing_edges.erase("edge_connections");
  EXPECT_EQ(LyraTerrainDocument::Parse(missing_edges.dump(), Profile())
                .status()
                .message(),
            "Malformed Lyra schema 3 header keys");

  auto legacy = AsSchema2(current);
  auto migrated = LyraTerrainDocument::Parse(legacy.dump(), Profile());
  ASSERT_TRUE(migrated.ok()) << migrated.status();
  EXPECT_TRUE(migrated->authoring().edge_connections.empty());
}

TEST(LyraTerrainDocumentTest, RefusesMalformedEdgeConnectionJsonExactly) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  ASSERT_TRUE(
      doc->SetAuthoring(doc->authoring(), WithEdgeConnections(doc->authoring()))
          .ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  ASSERT_TRUE(LyraTerrainDocument::Parse(original.dump(), Profile()).ok());

  using Json = nlohmann::json;
  const std::string record = "Malformed edge connection";
  const std::string passage = "Malformed edge passage";
  const std::string order =
      "Edge passage requires 0 <= first_tile <= last_tile <= 31";
  const std::string adjacency =
      "Edge connection maps are not adjacent Lyra edge maps";
  struct Case {
    std::function<void(Json&)> mutate;
    std::string message;
  };
  const std::vector<Case> cases = {
      {[](Json& j) { j["edge_connections"][0]["id"] = 0; }, record},
      {[](Json& j) { j["edge_connections"][0]["id"] = 1.0; }, record},
      {[](Json& j) { j["edge_connections"][0]["id"] = true; }, record},
      {[](Json& j) { j["edge_connections"][0]["id"] = uint64_t{4294967296}; },
       record},
      {[](Json& j) { j["edge_connections"][0]["map_a"] = "$99"; }, record},
      {[](Json& j) { j["edge_connections"][0]["map_a"] = 153.0; }, record},
      {[](Json& j) { j["edge_connections"][0]["map_b"] = -161; }, record},
      {[](Json& j) { j["edge_connections"][0]["extra"] = 1; }, record},
      {[](Json& j) { j["edge_connections"][0].erase("passage"); }, record},
      {[](Json& j) { j["edge_connections"][1]["passage"] = Json::object(); },
       passage},
      {[](Json& j) {
         j["edge_connections"][1]["passage"] = Json::array({3, 7});
       },
       passage},
      {[](Json& j) { j["edge_connections"][1]["passage"]["first_tile"] = 3.0; },
       passage},
      {[](Json& j) { j["edge_connections"][1]["passage"]["last_tile"] = 32; },
       passage},
      {[](Json& j) {
         j["edge_connections"][1]["passage"]["last_tile"] = false;
       },
       passage},
      {[](Json& j) { j["edge_connections"][1]["passage"]["note"] = "x"; },
       passage},
      {[](Json& j) { j["edge_connections"][1]["passage"]["first_tile"] = 8; },
       order},
      {[](Json& j) {
         j["edge_connections"][0]["map_a"] = 0xA1;
         j["edge_connections"][0]["map_b"] = 0x99;
       },
       "Edge connection must store map_a < map_b"},
      {[](Json& j) { j["edge_connections"][0]["map_b"] = 0x99; }, adjacency},
      {[](Json& j) {
         j["edge_connections"][1]["map_a"] = 0xA7;
         j["edge_connections"][1]["map_b"] = 0xA8;
       },
       adjacency},
      {[](Json& j) {
         j["edge_connections"][0]["map_a"] = 0x9C;
         j["edge_connections"][0]["map_b"] = 0xA4;
       },
       adjacency},
      {[](Json& j) {
         j["edge_connections"][0]["map_a"] = 0x98;
         j["edge_connections"][0]["map_b"] = 0x99;
       },
       adjacency},
      {[](Json& j) { j["edge_connections"][1]["id"] = 1; },
       "Duplicate edge connection ID"},
      {[](Json& j) {
         j["edge_connections"][1]["map_a"] = 0x99;
         j["edge_connections"][1]["map_b"] = 0xA1;
       },
       "Duplicate edge connection map pair"},
      {[](Json& j) { j["edge_connections"] = Json::object(); },
       "edge_connections must be an array"},
      {[](Json& j) { j["edge_connections"] = nullptr; },
       "edge_connections must be an array"},
      {[](Json& j) {
         for (int i = 0; i < 62; ++i)
           j["edge_connections"].push_back(j["edge_connections"][0]);
       },
       "Max 64 edge connections exceeded"},
  };
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE(i);
    auto malformed = original;
    cases[i].mutate(malformed);
    const auto parsed = LyraTerrainDocument::Parse(malformed.dump(), Profile());
    EXPECT_EQ(parsed.status().code(), absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(parsed.status().message(), cases[i].message);
  }

  auto duplicate_key = original.dump();
  const auto record_start = duplicate_key.find("\"edge_connections\":[{");
  ASSERT_NE(record_start, std::string::npos);
  duplicate_key.insert(record_start + 21, "\"map_b\":161,");
  EXPECT_EQ(
      LyraTerrainDocument::Parse(duplicate_key, Profile()).status().message(),
      "Duplicate JSON keys");
}

TEST(LyraTerrainDocumentTest, RefusesInvalidEdgeConnectionsAtomically) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto base = WithEdgeConnections(doc->authoring());
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), base).ok());
  doc->MarkSaved();

  const std::string adjacency =
      "Edge connection maps are not adjacent Lyra edge maps";
  const std::string order =
      "Edge passage requires 0 <= first_tile <= last_tile <= 31";
  struct Case {
    std::function<void(LyraAuthoringState&)> mutate;
    std::string message;
  };
  const std::vector<Case> cases = {
      {[](auto& s) { s.edge_connections[1].id = 0; },
       "Edge connection ID must be nonzero"},
      {[](auto& s) { s.edge_connections[1].id = 1; },
       "Duplicate edge connection ID"},
      {[](auto& s) { s.edge_connections[1].map_b = 0xA0; }, adjacency},
      {[](auto& s) { s.edge_connections.push_back({9, 0xA7, 0xA8, {}}); },
       adjacency},
      {[](auto& s) { s.edge_connections.push_back({9, 0x9C, 0xA4, {}}); },
       adjacency},
      {[](auto& s) { s.edge_connections.push_back({9, 0x98, 0x99, {}}); },
       adjacency},
      {[](auto& s) { s.edge_connections.push_back({9, 0xA0, 0xA9, {}}); },
       adjacency},
      {[](auto& s) { s.edge_connections.push_back({9, 0xA8, 0xA0, {}}); },
       "Edge connection must store map_a < map_b"},
      {[](auto& s) { s.edge_connections.push_back({9, 0xA0, 0xA1, {}}); },
       "Duplicate edge connection map pair"},
      {[](auto& s) { s.edge_connections[1].passage = LyraEdgePassage{5, 4}; },
       order},
      {[](auto& s) { s.edge_connections[1].passage = LyraEdgePassage{0, 32}; },
       order},
      {[](auto& s) { s.edge_connections[1].passage = LyraEdgePassage{-1, 3}; },
       order},
      {[](auto& s) {
         while (s.edge_connections.size() <= 64)
           s.edge_connections.push_back(
               {static_cast<uint32_t>(s.edge_connections.size() + 10), 0xA2,
                0xA3, std::nullopt});
       },
       "Max 64 edge connections exceeded"},
  };
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE(i);
    auto bad = base;
    cases[i].mutate(bad);
    const auto status = doc->SetAuthoring(base, bad);
    EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(status.message(), cases[i].message);
    EXPECT_EQ(doc->authoring(), base);
    EXPECT_FALSE(doc->dirty());
  }
}

TEST(LyraTerrainDocumentTest, EdgeConnectionHistoryRetainsPairAndDirtyState) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto empty = doc->authoring();
  ASSERT_TRUE(doc->SetAuthoring(empty, empty).ok());
  EXPECT_FALSE(doc->dirty());

  auto unassigned = empty;
  unassigned.edge_connections.push_back({1, 0x9A, 0xA2, std::nullopt});
  auto assigned = unassigned;
  assigned.edge_connections[0].passage = LyraEdgePassage{0, 31};

  ASSERT_TRUE(doc->SetAuthoring(empty, unassigned).ok());
  EXPECT_TRUE(doc->dirty());
  doc->MarkSaved();
  ASSERT_TRUE(doc->SetAuthoring(unassigned, assigned).ok());
  EXPECT_TRUE(doc->dirty());

  // A stale step cannot drop the pair or its passage.
  EXPECT_EQ(doc->SetAuthoring(unassigned, empty).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(doc->authoring(), assigned);

  // Undo the assignment: the pair remains, undecided, and matches the save.
  ASSERT_TRUE(doc->SetAuthoring(assigned, unassigned).ok());
  EXPECT_FALSE(doc->dirty());
  ASSERT_EQ(doc->authoring().edge_connections.size(), 1u);
  EXPECT_FALSE(doc->authoring().edge_connections[0].passage.has_value());
  // Undo the pair, then redo both steps.
  ASSERT_TRUE(doc->SetAuthoring(unassigned, empty).ok());
  EXPECT_TRUE(doc->dirty());
  ASSERT_TRUE(doc->SetAuthoring(empty, unassigned).ok());
  EXPECT_FALSE(doc->dirty());
  ASSERT_TRUE(doc->SetAuthoring(unassigned, assigned).ok());

  auto reopened = LyraTerrainDocument::Parse(doc->Serialize(), Profile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->authoring(), assigned);
}

TEST(LyraTerrainDocumentTest,
     GameplayAreaFootprintsHaveExactOrderAndAllowOddRowAndColumnParents) {
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xA9, LyraGameplayAreaSize::kSmall).value(),
      (std::vector<int>{0xA9}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xA9, LyraGameplayAreaSize::kWide).value(),
      (std::vector<int>{0xA9, 0xAA}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xA9, LyraGameplayAreaSize::kTall).value(),
      (std::vector<int>{0xA9, 0xB1}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xA9, LyraGameplayAreaSize::kLarge).value(),
      (std::vector<int>{0xA9, 0xAA, 0xB1, 0xB2}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xBE, LyraGameplayAreaSize::kWide).value(),
      (std::vector<int>{0xBE, 0xBF}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xB7, LyraGameplayAreaSize::kTall).value(),
      (std::vector<int>{0xB7, 0xBF}));
  EXPECT_EQ(
      LyraGameplayAreaFootprint(0xB6, LyraGameplayAreaSize::kLarge).value(),
      (std::vector<int>{0xB6, 0xB7, 0xBE, 0xBF}));
}

TEST(LyraTerrainDocumentTest,
     GameplayAreaFootprintsBoundEveryParentWithoutWrapOrOverflow) {
  const std::array<LyraGameplayAreaSize, 4> sizes = {
      LyraGameplayAreaSize::kSmall, LyraGameplayAreaSize::kWide,
      LyraGameplayAreaSize::kTall, LyraGameplayAreaSize::kLarge};
  const std::array<int, 4> expected_counts = {32, 28, 24, 21};
  for (size_t index = 0; index < sizes.size(); ++index) {
    int valid = 0;
    for (int parent = 0xA0; parent <= 0xBF; ++parent) {
      const auto footprint = LyraGameplayAreaFootprint(parent, sizes[index]);
      if (!footprint.ok())
        continue;
      ++valid;
      ASSERT_FALSE(footprint->empty());
      EXPECT_EQ(footprint->front(), parent);
      EXPECT_TRUE(
          std::all_of(footprint->begin(), footprint->end(),
                      [](int map) { return map >= 0xA0 && map <= 0xBF; }));
    }
    EXPECT_EQ(valid, expected_counts[index]);
    for (const int outside : {std::numeric_limits<int>::min(), 0x9F, 0xC0,
                              std::numeric_limits<int>::max()}) {
      EXPECT_FALSE(LyraGameplayAreaFootprint(outside, sizes[index]).ok());
    }
  }
  for (const int east_edge : {0xA7, 0xAF, 0xB7, 0xBF}) {
    EXPECT_FALSE(
        LyraGameplayAreaFootprint(east_edge, LyraGameplayAreaSize::kWide).ok());
    EXPECT_FALSE(
        LyraGameplayAreaFootprint(east_edge, LyraGameplayAreaSize::kLarge)
            .ok());
  }
  for (int last_row = 0xB8; last_row <= 0xBF; ++last_row) {
    EXPECT_FALSE(
        LyraGameplayAreaFootprint(last_row, LyraGameplayAreaSize::kTall).ok());
    EXPECT_FALSE(
        LyraGameplayAreaFootprint(last_row, LyraGameplayAreaSize::kLarge).ok());
  }
  EXPECT_FALSE(
      LyraGameplayAreaFootprint(0xA0, static_cast<LyraGameplayAreaSize>(99))
          .ok());
}

TEST(LyraTerrainDocumentTest, LegacySchemasLeaveEveryGameplayMapUnassigned) {
  const auto doc = LyraTerrainDocument::Create(Profile(), 13);
  ASSERT_TRUE(doc.ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  for (const int version : {1, 2, 3, 4}) {
    SCOPED_TRACE(version);
    auto legacy = original;
    legacy["schema_version"] = version;
    if (version <= 2)
      legacy.erase("edge_connections");
    if (version == 1) {
      legacy.erase("flight_points");
      for (auto& map : legacy["maps"])
        map.erase("authoring");
    }
    if (version == 4) {
      legacy["maps"][0]["authoring"]["properties"]["background_palette"] =
          std::array<uint16_t, 128>{};
      legacy["maps"][0]["authoring"]["asset_bindings"] =
          std::array<std::string, 3>{"defs:0", "gfx:0", "palette:0"};
    }
    const auto parsed = LyraTerrainDocument::Parse(legacy.dump(), Profile());
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    EXPECT_TRUE(parsed->authoring().gameplay_areas.empty());
    EXPECT_EQ(parsed->maps(), doc->maps());
    EXPECT_FALSE(parsed->dirty());
    for (int map = 0xA0; map <= 0xBF; ++map)
      EXPECT_EQ(parsed->GameplayAreaForMap(map), nullptr);
    const auto saved = nlohmann::json::parse(parsed->Serialize());
    EXPECT_FALSE(saved.contains("gameplay_areas"));
    EXPECT_EQ(saved["schema_version"], version == 4 ? 4 : 3);
    // An unknown layout field must never be silently dropped by legacy parsing.
    legacy["gameplay_areas"] = nlohmann::json::array();
    EXPECT_FALSE(LyraTerrainDocument::Parse(legacy.dump(), Profile()).ok());
  }
}

TEST(LyraTerrainDocumentTest,
     Schema5RoundtripPreservesGameplayOwnershipAndAllOtherAuthoring) {
  auto doc = LyraTerrainDocument::Create(Profile(), 19);
  ASSERT_TRUE(doc.ok());
  auto state = WithEdgeConnections(doc->authoring());
  state.gameplay_areas = {
      {0xA1, LyraGameplayAreaSize::kLarge, {0xA1, 0xA2, 0xA9, 0xAA}},
      {0xA4, LyraGameplayAreaSize::kWide, {0xA4, 0xA5}},
      {0xAE, LyraGameplayAreaSize::kTall, {0xAE, 0xB6}},
      {0xBF, LyraGameplayAreaSize::kSmall, {0xBF}}};
  state.maps[1].asset_bindings = {"defs:1", "gfx:1", "palette:1"};
  state.maps[1].properties.message_id = 0x1234;
  state.maps[1].properties.background_palette = std::array<uint16_t, 128>{};
  state.maps[1].properties.background_palette->back() = 0x7FFF;
  state.maps[1].sprites.push_back({1, 0x15, 2, 3, 1});
  state.maps[1].doors.push_back({1, 4, 5, std::nullopt});
  state.maps[1].exits.push_back({1, 0x100, 6, 7, 78});
  state.flight_points.push_back({1, 0xA1, 8, 9, "Area flight", std::nullopt});
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), state).ok());
  const auto serialized = doc->Serialize();
  const auto json = nlohmann::json::parse(serialized);
  EXPECT_EQ(json["schema_version"], 5);
  EXPECT_EQ(
      json["gameplay_areas"][0],
      nlohmann::json({{"parent_map_id", 0xA1},
                      {"size", "large"},
                      {"maps", std::vector<int>{0xA1, 0xA2, 0xA9, 0xAA}}}));
  EXPECT_EQ(json["gameplay_areas"][1]["size"], "wide");
  EXPECT_EQ(json["gameplay_areas"][2]["size"], "tall");
  EXPECT_EQ(json["gameplay_areas"][3]["size"], "small");
  const auto parsed = LyraTerrainDocument::Parse(serialized, Profile());
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_EQ(parsed->authoring(), state);
  EXPECT_EQ(parsed->maps(), doc->maps());
  EXPECT_EQ(parsed->profile(), doc->profile());
  EXPECT_EQ(parsed->Serialize(), serialized);
  EXPECT_FALSE(parsed->dirty());
  ASSERT_NE(parsed->GameplayAreaForMap(0xAA), nullptr);
  EXPECT_EQ(parsed->GameplayAreaForMap(0xAA)->parent_map_id, 0xA1);
  EXPECT_EQ(parsed->GameplayAreaForMap(0xA1), parsed->GameplayAreaForMap(0xAA));
  EXPECT_EQ(parsed->GameplayAreaForMap(0xA0), nullptr);
  EXPECT_EQ(parsed->GameplayAreaForMap(0x9F), nullptr);
  EXPECT_EQ(parsed->GameplayAreaForMap(0xC0), nullptr);

  auto native_palette = state;
  native_palette.maps[1].properties.background_palette.reset();
  ASSERT_TRUE(doc->SetAuthoring(state, native_palette).ok());
  const auto no_palette =
      LyraTerrainDocument::Parse(doc->Serialize(), Profile());
  ASSERT_TRUE(no_palette.ok()) << no_palette.status();
  EXPECT_EQ(no_palette->authoring(), native_palette);
  EXPECT_EQ(nlohmann::json::parse(no_palette->Serialize())["schema_version"],
            5);
}

TEST(LyraTerrainDocumentTest, GameplayAreaInvalidStateIsRejectedAtomically) {
  auto doc = LyraTerrainDocument::Create(Profile(), 23);
  ASSERT_TRUE(doc.ok());
  const auto before = doc->authoring();
  const auto before_maps = doc->maps();
  const auto before_json = doc->Serialize();
  using Mutation = std::function<void(LyraAuthoringState&)>;
  const std::vector<Mutation> mutations = {
      [](auto& s) {
        s.gameplay_areas = {{0x9F, LyraGameplayAreaSize::kSmall, {0x9F}}};
      },
      [](auto& s) {
        s.gameplay_areas = {
            {0xA0, static_cast<LyraGameplayAreaSize>(99), {0xA0}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA1, LyraGameplayAreaSize::kWide, {0xA2, 0xA1}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA1, LyraGameplayAreaSize::kWide, {0xA1, 0xA1}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA1, LyraGameplayAreaSize::kWide, {0xA2}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA1, LyraGameplayAreaSize::kSmall, {0xA1, 0xA2}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA7, LyraGameplayAreaSize::kWide, {0xA7, 0xA8}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xB8, LyraGameplayAreaSize::kTall, {0xB8, 0xC0}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA0, LyraGameplayAreaSize::kWide, {0xA0, 0xA1}},
                            {0xA1, LyraGameplayAreaSize::kSmall, {0xA1}}};
      },
      [](auto& s) {
        s.gameplay_areas = {{0xA0, LyraGameplayAreaSize::kSmall, {0xA0}},
                            {0xA0, LyraGameplayAreaSize::kSmall, {0xA0}}};
      },
  };
  for (size_t index = 0; index < mutations.size(); ++index) {
    SCOPED_TRACE(index);
    auto invalid = before;
    mutations[index](invalid);
    EXPECT_EQ(doc->SetAuthoring(before, invalid).code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_EQ(doc->authoring(), before);
    EXPECT_EQ(doc->maps(), before_maps);
    EXPECT_EQ(doc->Serialize(), before_json);
    EXPECT_FALSE(doc->dirty());
  }
}

TEST(LyraTerrainDocumentTest,
     Schema5GameplayAreaJsonRequiresExactBoundedShape) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  auto state = doc->authoring();
  state.gameplay_areas = {{0xA1, LyraGameplayAreaSize::kWide, {0xA1, 0xA2}}};
  ASSERT_TRUE(doc->SetAuthoring(doc->authoring(), state).ok());
  const auto original = nlohmann::json::parse(doc->Serialize());
  using Json = nlohmann::json;
  using Mutation = std::function<void(Json&)>;
  const std::vector<Mutation> mutations = {
      [](auto& j) { j.erase("gameplay_areas"); },
      [](auto& j) { j["unknown_layout"] = true; },
      [](auto& j) { j["gameplay_areas"] = nullptr; },
      [](auto& j) { j["gameplay_areas"] = Json::object(); },
      [](auto& j) { j["gameplay_areas"] = Json::array(); },
      [](auto& j) { j["gameplay_areas"][0] = Json::array(); },
      [](auto& j) { j["gameplay_areas"][0].erase("parent_map_id"); },
      [](auto& j) { j["gameplay_areas"][0].erase("size"); },
      [](auto& j) { j["gameplay_areas"][0].erase("maps"); },
      [](auto& j) { j["gameplay_areas"][0]["unknown"] = 0; },
      [](auto& j) { j["gameplay_areas"][0]["parent_map_id"] = 0x9F; },
      [](auto& j) { j["gameplay_areas"][0]["parent_map_id"] = 0xC0; },
      [](auto& j) { j["gameplay_areas"][0]["parent_map_id"] = 161.0; },
      [](auto& j) { j["gameplay_areas"][0]["size"] = "WIDE"; },
      [](auto& j) { j["gameplay_areas"][0]["size"] = 2; },
      [](auto& j) { j["gameplay_areas"][0]["size"] = nullptr; },
      [](auto& j) { j["gameplay_areas"][0]["maps"] = nullptr; },
      [](auto& j) { j["gameplay_areas"][0]["maps"] = Json::array(); },
      [](auto& j) { j["gameplay_areas"][0]["maps"][0] = true; },
      [](auto& j) { j["gameplay_areas"][0]["maps"][0] = 161.0; },
      [](auto& j) {
        j["gameplay_areas"][0]["maps"][0] = uint64_t{18446744073709551615ULL};
      },
      [](auto& j) {
        j["gameplay_areas"][0]["maps"] = std::vector<int>{0xA2, 0xA1};
      },
      [](auto& j) {
        j["gameplay_areas"][0]["maps"] = std::vector<int>{0xA1, 0xA1};
      },
      [](auto& j) { j["gameplay_areas"].push_back(j["gameplay_areas"][0]); },
  };
  for (size_t index = 0; index < mutations.size(); ++index) {
    SCOPED_TRACE(index);
    auto invalid = original;
    mutations[index](invalid);
    EXPECT_FALSE(LyraTerrainDocument::Parse(invalid.dump(), Profile()).ok());
  }
}

TEST(LyraTerrainDocumentTest,
     GameplayAreaUndoAndRemovalUseAuthoringDirtyState) {
  auto doc = LyraTerrainDocument::Create(Profile());
  ASSERT_TRUE(doc.ok());
  const auto unassigned = doc->authoring();
  auto assigned = unassigned;
  assigned.gameplay_areas = {
      {0xA9, LyraGameplayAreaSize::kLarge, {0xA9, 0xAA, 0xB1, 0xB2}}};
  ASSERT_TRUE(doc->SetAuthoring(unassigned, assigned).ok());
  EXPECT_TRUE(doc->dirty());
  doc->MarkSaved();
  EXPECT_FALSE(doc->dirty());
  EXPECT_EQ(doc->SetAuthoring(unassigned, assigned).code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(doc->authoring(), assigned);
  ASSERT_TRUE(doc->BeginStroke().ok());
  EXPECT_FALSE(doc->SetAuthoring(assigned, unassigned).ok());
  doc->CancelStroke();
  EXPECT_EQ(doc->authoring(), assigned);
  ASSERT_TRUE(doc->SetAuthoring(assigned, unassigned).ok());
  EXPECT_TRUE(doc->dirty());
  EXPECT_EQ(doc->GameplayAreaForMap(0xA9), nullptr);
  EXPECT_EQ(nlohmann::json::parse(doc->Serialize())["schema_version"], 3);
  ASSERT_TRUE(doc->SetAuthoring(unassigned, assigned).ok());
  EXPECT_FALSE(doc->dirty());
  EXPECT_NE(doc->GameplayAreaForMap(0xB2), nullptr);
}

}  // namespace
}  // namespace yaze::core
