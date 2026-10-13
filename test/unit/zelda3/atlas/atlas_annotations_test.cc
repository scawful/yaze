// Atlas annotation loader: yaze.atlas/1, the Oracle of Secrets room and
// overworld files (committed copies under test/fixtures/atlas/oracle_b28/),
// merge order, and errors that name the file and the JSON pointer.

#include "zelda3/atlas/atlas_annotations.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "unique_temp_path.h"

namespace yaze::zelda3::atlas {
namespace {

namespace fs = std::filesystem;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

fs::path OracleFixture(const std::string& name) {
  return fs::path(YAZE_TEST_FIXTURE_DIR) / "atlas" / "oracle_b28" / name;
}

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

absl::Status Merge(const std::string& text, AtlasAnnotations* out,
                   AnnotationFormat* format = nullptr) {
  return MergeAtlasAnnotationText(text, "test.json", out, format);
}

std::string ErrorOf(const std::string& text) {
  AtlasAnnotations out;
  const absl::Status status = Merge(text, &out);
  EXPECT_FALSE(status.ok()) << text;
  return std::string(status.message());
}

TEST(AtlasAnnotationsTest, OracleRoomFileLoadsAsIs) {
  AtlasAnnotations out;
  AnnotationFormat format;
  ASSERT_TRUE(
      MergeAtlasAnnotationFile(OracleFixture("room_annotations.json").string(),
                               &out, &format)
          .ok());
  EXPECT_EQ(format, AnnotationFormat::kOracleRoomV0);
  EXPECT_THAT(out.files, ElementsAre("room_annotations.json"));

  ASSERT_EQ(out.dungeons.size(), 11u);
  const auto fos = std::find_if(out.dungeons.begin(), out.dungeons.end(),
                                [](const auto& d) { return d.owner == "FOS"; });
  ASSERT_NE(fos, out.dungeons.end());
  EXPECT_EQ(fos->code, "D8");
  EXPECT_EQ(fos->origin.ToString(), "annotation:room_annotations.json");

  std::vector<std::string> area_keys;
  for (const auto& area : out.areas) {
    area_keys.push_back(area.key);
  }
  EXPECT_THAT(area_keys, ElementsAre("ORIGINS", "HOS", "FINAL", "GANON",
                                     "DREAM", "UWS", "INT"));
  EXPECT_THAT(out.areas[1].rooms,
              ElementsAre(0x11, 0x12, 0x21, 0x22, 0x40, 0x41, 0x42));

  ASSERT_TRUE(out.rooms.contains(0x11D));
  EXPECT_EQ(out.rooms.at(0x11D).name, "Mayor's basement");

  ASSERT_EQ(out.links.size(), 1u);
  EXPECT_EQ(out.links[0].view, AtlasView::kDungeon);
  EXPECT_EQ(out.links[0].kind, "pit");
  EXPECT_EQ(out.links[0].from, 0x81);
  EXPECT_EQ(out.links[0].to, 0x05);

  ASSERT_EQ(out.plans.size(), 4u);
  EXPECT_EQ(out.plans[0].key, "TOWER");
  EXPECT_EQ(out.plans[0].label, "SKY TOWER");
  EXPECT_EQ(out.plans[0].status, "decided");
  EXPECT_EQ(out.plans[0].rooms.size(), 10u);
  EXPECT_EQ(out.plans[2].rooms, (std::vector<int>{0x127, 0x4C}));
}

TEST(AtlasAnnotationsTest, OracleOverworldFileLoadsAsIs) {
  AtlasAnnotations out;
  AnnotationFormat format;
  ASSERT_TRUE(
      MergeAtlasAnnotationFile(
          OracleFixture("overworld_annotations.json").string(), &out, &format)
          .ok());
  EXPECT_EQ(format, AnnotationFormat::kOracleOverworldV0);
  const auto& ow = out.overworld;
  ASSERT_EQ(ow.regions.size(), 26u);
  EXPECT_EQ(ow.regions.front().world, "LW");
  EXPECT_EQ(ow.regions.front().name, "Toto Ranch / Hotel Kaly (northwest)");
  EXPECT_EQ(ow.regions.back().world, "SW");
  EXPECT_EQ(ow.maps.size(), 138u);
  EXPECT_EQ(ow.gates.size(), 26u);
  EXPECT_THAT(ow.closed_border_maps, ElementsAre(0x80, 0x88, 0x91));
  EXPECT_THAT(ow.isolated_maps, ElementsAre(0x80, 0x91, 0x88));
  ASSERT_EQ(out.links.size(), 11u);
  EXPECT_EQ(out.links[0].view, AtlasView::kOverworld);
  EXPECT_EQ(out.links[0].from, 0x3D);
  EXPECT_EQ(out.links[0].label, "WHIRLPOOL to ABYSS");
  EXPECT_EQ(out.links[0].to, -1);
}

// The live Oracle files (read only), so a change there that the loader
// rejects shows up before Oracle points [atlas] at it.
TEST(AtlasAnnotationsTest, LiveOracleFilesLoad) {
  const char* root = std::getenv("YAZE_TEST_ORACLE_ROOT");
  if (root == nullptr || !fs::exists(root)) {
    GTEST_SKIP()
        << "Set YAZE_TEST_ORACLE_ROOT to the Oracle of Secrets checkout";
  }
  const fs::path maps = fs::path(root) / "Scripts" / "Maps";
  auto loaded =
      LoadAtlasAnnotations({(maps / "room_annotations.json").string(),
                            (maps / "overworld_annotations.json").string()},
                           "vanilla");
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->files.size(), 2u);
}

TEST(AtlasAnnotationsTest, MergeOrderDefaultsThenFilesLastWriterWins) {
  auto loaded = LoadAtlasAnnotations({}, "vanilla");
  ASSERT_TRUE(loaded.ok());
  AtlasAnnotations out = *loaded;
  EXPECT_TRUE(out.vanilla_defaults);
  ASSERT_EQ(out.dungeons.size(), 15u);
  EXPECT_EQ(out.dungeons[2].dungeon_id, 0x04);
  EXPECT_EQ(out.dungeons[2].code, "EP");
  EXPECT_EQ(out.dungeons[2].origin.ToString(), "default");

  ASSERT_TRUE(MergeAtlasAnnotationText(R"({
    "schema": "yaze.atlas/1",
    "title": "First",
    "dungeons": [{"dungeon_id": "0x04", "code": "D1", "name": "Forest Temple"}],
    "areas": [{"key": "HUB", "name": "Hub", "rooms": ["0x10", 17]}],
    "links": [{"id": "drop", "view": "dungeon", "kind": "pit", "from": "0x81",
               "to": "0x05"}],
    "plans": [{"key": "D9", "rooms": ["0x90-0x93"], "color": "#5ADCFF"}]
  })",
                                       "a.json", &out)
                  .ok());
  ASSERT_TRUE(MergeAtlasAnnotationText(R"({
    "schema": "yaze.atlas/1",
    "title": "Second",
    "areas": [{"key": "HUB", "code": "HB", "rooms": ["0x11"]}],
    "links": [{"id": "drop", "view": "dungeon", "kind": "warp", "from": "0x81",
               "to": "0x06"},
              {"view": "dungeon", "kind": "dream", "from": "0x10"}],
    "rooms": {"0x31": {"status": "reserved", "note": "dream 3"}}
  })",
                                       "b.json", &out)
                  .ok());
  EXPECT_EQ(out.title, "Second");
  EXPECT_EQ(out.title_origin.ToString(), "annotation:b.json");
  EXPECT_EQ(out.dungeons.size(), 15u);  // Merged by dungeon_id.
  EXPECT_EQ(out.dungeons[2].code, "D1");
  EXPECT_EQ(out.dungeons[2].name, "Forest Temple");
  EXPECT_EQ(out.dungeons[2].origin.ToString(), "annotation:a.json");
  EXPECT_EQ(out.dungeons[2].color_origin.ToString(), "default");
  ASSERT_EQ(out.areas.size(), 1u);
  EXPECT_EQ(out.areas[0].name, "Hub");  // Kept from a.json.
  EXPECT_EQ(out.areas[0].code, "HB");
  EXPECT_THAT(out.areas[0].rooms, ElementsAre(0x11));
  ASSERT_EQ(out.links.size(), 2u);  // "drop" merged by id, one appended.
  EXPECT_EQ(out.links[0].kind, "warp");
  EXPECT_EQ(out.links[0].to, 0x06);
  EXPECT_EQ(out.links[1].to, -1);
  ASSERT_EQ(out.plans.size(), 1u);
  EXPECT_THAT(out.plans[0].rooms, ElementsAre(0x90, 0x91, 0x92, 0x93));
  EXPECT_TRUE(out.rooms.at(0x31).reserved);
  EXPECT_THAT(out.files, ElementsAre("a.json", "b.json"));

  auto none = LoadAtlasAnnotations({}, "none");
  ASSERT_TRUE(none.ok());
  EXPECT_TRUE(none->dungeons.empty());
  EXPECT_FALSE(none->vanilla_defaults);
  const auto bad = LoadAtlasAnnotations({}, "oracle");
  EXPECT_FALSE(bad.ok());
  EXPECT_THAT(std::string(bad.status().message()),
              HasSubstr("defaults must be \"vanilla\" or \"none\""));
}

TEST(AtlasAnnotationsTest, IdsAndCommentKeys) {
  EXPECT_EQ(ParseAtlasId("0x11D"), 0x11D);
  EXPECT_EQ(ParseAtlasId("81"), 0x81);
  EXPECT_EQ(ParseAtlasId("0X0f"), 0x0F);
  EXPECT_FALSE(ParseAtlasId("0x").has_value());
  EXPECT_FALSE(ParseAtlasId("0x1234").has_value());
  EXPECT_FALSE(ParseAtlasId("x1").has_value());

  AtlasAnnotations out;
  ASSERT_TRUE(Merge(R"({"schema": "yaze.atlas/1", "_comment": "ignored",
                        "rooms": {"_note": {}, "18": {"name": "Eighteen"}},
                        "areas": [{"_why": "x", "key": "A", "rooms": [24]}]})",
                    &out)
                  .ok());
  EXPECT_EQ(out.rooms.at(0x18).name, "Eighteen");
  EXPECT_THAT(out.areas[0].rooms, ElementsAre(24));
}

TEST(AtlasAnnotationsTest, ErrorsNameFileAndPointer) {
  EXPECT_THAT(
      ErrorOf(R"({"schema": "yaze.atlas/1",
                  "areas": [{"key": "A", "colour": "#FF0000", "rooms": []}]})"),
      HasSubstr("test.json: /areas/0/colour: unknown key (allowed: key, code, "
                "name, color, rooms, source"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1", "bogus": 1})"),
              HasSubstr("test.json: /bogus: unknown key"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                  "plans": [{"key": "P", "rooms": ["0x10", "0x1G"]}]})"),
              HasSubstr("/plans/0/rooms/1: \"0x1G\" is not a room ID"));
  EXPECT_THAT(
      ErrorOf(R"({"schema": "yaze.atlas/1",
                          "rooms": {"0x128": {"name": "past the end"}}})"),
      HasSubstr("/rooms/0x128: room 0x128 is out of range (0x0-0x127)"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                  "plans": [{"key": "P", "rooms": ["0x96-0x90"]}]})"),
              HasSubstr("ends before it starts"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                          "dungeons": [{"owner": "D1", "color": "red"}]})"),
              HasSubstr("/dungeons/0/color: \"red\" is not a color"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                          "dungeons": [{"code": "LONGER"}]})"),
              HasSubstr("/dungeons/0: needs \"owner\""));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                          "links": [{"view": "dungeon", "kind": "teleport",
                                     "from": 1}]})"),
              HasSubstr("/links/0/kind: \"teleport\" is not one of"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/1",
                          "links": [{"view": "overworld", "kind": "pad",
                                     "from": "0xA0"}]})"),
              HasSubstr("/links/0/from: map 0xA0 is out of range"));
  EXPECT_THAT(ErrorOf(R"({"schema": "yaze.atlas/2"})"),
              HasSubstr("/schema: unknown schema \"yaze.atlas/2\""));
  EXPECT_THAT(ErrorOf(R"({"areas": [], "regions": {}})"), HasSubstr("mixes"));
  EXPECT_THAT(ErrorOf(R"({"title": "no format"})"),
              HasSubstr("cannot tell the format"));
  EXPECT_THAT(ErrorOf("{not json"), HasSubstr("test.json: /: not valid JSON"));
  // Oracle room file: unknown key and a bad room.
  EXPECT_THAT(ErrorOf(R"({"dungeon_codes": {"D1": "D1"}, "room_nmes": {}})"),
              HasSubstr("test.json: /room_nmes: unknown key"));
  EXPECT_THAT(ErrorOf(R"({"runtime_pits": [{"room": "0x81"}]})"),
              HasSubstr("/runtime_pits/0: missing required key \"to\""));
  // Oracle overworld file.
  EXPECT_THAT(ErrorOf(R"({"gate_notes": {"0x04:Feather": "no"}})"),
              HasSubstr("/gate_notes/0x04:Feather: gate \"Feather\""));
  EXPECT_THAT(ErrorOf(R"({"regions": {"Kalyxo": ["0x00"]}})"),
              HasSubstr("/regions/Kalyxo: region names start with the world"));
  EXPECT_THAT(ErrorOf(R"({"regions": {"LW A/B": ["0x00", "zz"]}})"),
              HasSubstr("/regions/LW A~1B/1: \"zz\" is not a map ID"));

  // A failing file leaves the merged result unchanged.
  AtlasAnnotations out;
  ASSERT_TRUE(
      Merge(R"({"schema": "yaze.atlas/1", "title": "kept"})", &out).ok());
  EXPECT_FALSE(
      Merge(R"({"schema": "yaze.atlas/1", "title": "lost", "bogus": 1})", &out)
          .ok());
  EXPECT_EQ(out.title, "kept");
  EXPECT_EQ(out.files.size(), 1u);

  const auto missing = LoadAtlasAnnotations({"/no/such/atlas.json"}, "none");
  EXPECT_FALSE(missing.ok());
  EXPECT_THAT(std::string(missing.status().message()),
              HasSubstr("/no/such/atlas.json"));
}

// The schema file ships next to the loader; keep its key lists in step with
// what the loader accepts.
TEST(AtlasAnnotationsTest, SchemaFileMatchesLoaderKeys) {
  const fs::path schema_path =
      fs::path(YAZE_TEST_FIXTURE_DIR).parent_path().parent_path() / "assets" /
      "atlas" / "atlas-annotations-1.schema.json";
  const auto schema = nlohmann::json::parse(ReadFile(schema_path));
  auto keys = [](const nlohmann::json& object) {
    std::set<std::string> out;
    for (const auto& [key, unused] : object.at("properties").items()) {
      out.insert(key);
    }
    return out;
  };
  const auto& props = schema.at("properties");
  EXPECT_EQ(keys(schema), (std::set<std::string>{
                              "schema", "title", "about", "dungeons", "areas",
                              "rooms", "links", "symbol_links", "plans",
                              "legend_notes", "items", "overworld"}));
  EXPECT_EQ(
      keys(props.at("dungeons").at("items")),
      (std::set<std::string>{"owner", "dungeon_id", "code", "name", "color"}));
  EXPECT_EQ(keys(props.at("areas").at("items")),
            (std::set<std::string>{"key", "code", "name", "color", "rooms",
                                   "source"}));
  EXPECT_EQ(keys(props.at("links").at("items")),
            (std::set<std::string>{"id", "view", "kind", "from", "to", "label",
                                   "source"}));
  EXPECT_EQ(keys(props.at("plans").at("items")),
            (std::set<std::string>{"key", "label", "name", "status", "color",
                                   "rooms", "source"}));
  EXPECT_EQ(keys(props.at("overworld")),
            (std::set<std::string>{"sections", "regions", "maps", "gates",
                                   "edges", "closed_border_maps",
                                   "isolated_maps", "tile_classes",
                                   "gate_role_thresholds", "warp_tile_rule"}));

  // Every example in the user guide's first block loads.
  AtlasAnnotations out;
  ASSERT_TRUE(Merge(R"json({
    "schema": "yaze.atlas/1",
    "title": "My Hack",
    "dungeons": [
      {"dungeon_id": "0x04", "code": "D1", "name": "Forest Temple", "color": "#4CAF50"}
    ],
    "rooms": {"0x0F": {"name": "Secret shop basement"}},
    "plans": [
      {"key": "D9", "label": "D9?", "name": "Bonus dungeon (planned)",
       "status": "provisional", "color": "#5ADCFF", "rooms": ["0x90-0x96"]}
    ]
  })json",
                    &out)
                  .ok());
}

}  // namespace
}  // namespace yaze::zelda3::atlas
