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

}  // namespace
}  // namespace yaze::core
