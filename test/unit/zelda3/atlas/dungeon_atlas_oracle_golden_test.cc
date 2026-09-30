// Dungeon Atlas vs the Oracle of Secrets map scripts on the same ROM.
//
// YAZE_TEST_ROM_OOS_B28: a COPY of Roms/Playtest/oos-v0.9.0-b28.sfc (SHA-1
// pinned in the fixture) with its .sym next to it. The reference is
// test/fixtures/atlas/oracle_b28_room_matrix.json (Scripts/Maps/room_data.py
// output, trimmed). Owners come from the committed dungeons.json copy, the
// annotations from the committed copies of Oracle's two files, and the dream
// warps from the symbol link design doc section 6.5 says Oracle will add.
//
// Differences that are known and explained live in
// test/fixtures/atlas/oracle_b28_known_diffs.json. The test fails on any
// other difference and on an allowlist entry that no longer reproduces, so a
// fix removes its entries in the same change.

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "core/hack_manifest.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "util/rom_hash.h"
#include "zelda3/atlas/atlas_annotations.h"
#include "zelda3/atlas/atlas_json.h"
#include "zelda3/atlas/dungeon_atlas.h"

namespace yaze::zelda3::atlas {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

fs::path AtlasFixtures() {
  return fs::path(YAZE_TEST_FIXTURE_DIR) / "atlas";
}

Json ReadJson(const fs::path& path) {
  std::ifstream file(path);
  return Json::parse(file);
}

struct Diff {
  std::string id;  // Room ("0x81") or edge ("0x01-0x02-E").
  std::string field;
  Json oracle;
  Json yaze;
  std::string Key() const {
    return id + "|" + field + "|" + oracle.dump() + "|" + yaze.dump();
  }
};

Json SortedStrings(Json list) {
  std::vector<std::string> items;
  for (const auto& item : list) {
    items.push_back(item.get<std::string>());
  }
  std::sort(items.begin(), items.end());
  return Json(items);
}

// Badges from room_data.py chests / pot_keys / drop_keys, with the same rule
// as the renderer (render_room_matrix.py badges()).
Json OracleBadges(const Json& room) {
  int k = 0, c = 0, m = 0, bk = 0, big = 0, other = 0;
  for (const auto& chest : room.value("chests", Json::array())) {
    const int id = std::stoi(chest[0].get<std::string>(), nullptr, 16);
    const bool is_big = chest[1].get<bool>();
    const bool known = id == 0x24 || id == 0x25 || id == 0x32 || id == 0x33;
    big += is_big ? 1 : 0;
    k += id == 0x24;
    c += id == 0x25;
    bk += id == 0x32;
    m += id == 0x33;
    other += (!known && !is_big) ? 1 : 0;
  }
  int k_drop = 0, bk_drop = 0;
  for (const auto& drop : room.value("drop_keys", Json::array())) {
    k_drop += drop == "K";
    bk_drop += drop == "BK";
  }
  return {{"k_chest", k},
          {"c", c},
          {"m", m},
          {"bk_chest", bk},
          {"big_chest", big},
          {"other", other},
          {"k_pot", room.value("pot_keys", 0)},
          {"k_drop", k_drop},
          {"bk_drop", bk_drop}};
}

std::map<std::string, Json> OracleFields(const Json& room) {
  std::map<std::string, Json> out;
  out["area"] = room.at("area");
  out["status"] = room.at("status");
  if (room.contains("name")) {
    out["name"] = room.at("name");
  }
  out["doors"] = room.value("doors", Json::array());
  out["stairs"] = SortedStrings(room.value("stairs", Json::array()));
  out["pit_to"] = room.value("pit_to", Json());
  out["warp_tiles_to"] = room.value("warp_tiles_to", Json());
  out["tag_hole_to"] = room.value("tag_hole_to", Json());
  out["warp_tag"] = room.value("warp_tag", Json::array());
  out["pits"] = room.value("pits", false);
  out["pit_damage"] = room.value("pit_damage", false);
  out["entrances"] = room.value("entrances", Json::array());
  out["ow_holes"] = room.value("ow_holes", Json::array());
  out["spawns"] = room.value("spawns", Json::array());
  out["dream_warps_in"] = room.value("dream_warps_in", Json::array());
  out["badges"] = OracleBadges(room);
  out["plans"] = room.value("plans", Json::array());
  out["pit_to_cross"] = room.value("pit_to_cross", false);
  out["warp_tiles_to_cross"] = room.value("warp_tiles_to_cross", false);
  return out;
}

std::map<std::string, Json> YazeFields(const Json& room) {
  std::map<std::string, Json> out;
  out["area"] = room.at("area");
  out["status"] = room.at("status");
  out["name"] = room.at("name");
  Json doors = Json::array();
  for (const auto& door : room.at("doors")) {
    doors.push_back({door.at("wall"), door.at("pos"), door.at("category")});
  }
  out["doors"] = doors;
  Json stairs = Json::array();
  Json warp_tag = Json::array();
  Json pit_to, warp_to, tag_to;
  bool pits = false, pit_cross = false, warp_cross = false;
  for (const auto& link : room.at("links_out")) {
    const std::string kind = link.at("kind").get<std::string>();
    const std::string style = link.value("style", std::string());
    if (kind == "stair") {
      stairs.push_back(link.at("to"));
    } else if (kind == "warp_tag") {
      warp_tag.push_back(link.at("to"));
    } else if (kind == "pit" ||
               (kind == "annotation" && style == "pit" && pit_to.is_null())) {
      pit_to = link.at("to");
      pit_cross = link.at("cross_area").get<bool>();
      pits = true;
    } else if (kind == "pit_damage") {
      pits = true;
    } else if (kind == "warp_tiles") {
      warp_to = link.at("to");
      warp_cross = link.at("cross_area").get<bool>();
    } else if (kind == "tag_pit") {
      tag_to = link.at("to");
    }
  }
  out["stairs"] = SortedStrings(stairs);
  out["pit_to"] = pit_to;
  out["warp_tiles_to"] = warp_to;
  out["tag_hole_to"] = tag_to;
  out["warp_tag"] = warp_tag;
  out["pits"] = pits;
  out["pit_damage"] = room.at("pit_damage");
  Json entrances = Json::array(), holes = Json::array(), spawns = Json::array(),
       dreams = Json::array();
  for (const auto& link : room.at("links_in")) {
    const std::string kind = link.at("kind").get<std::string>();
    if (kind == "entrance") {
      entrances.push_back({link.at("entrance"), link.at("ow")});
    } else if (kind == "hole") {
      holes.push_back({link.at("entrance"), link.at("ow")});
    } else if (kind == "spawn") {
      spawns.push_back(link.at("spawn"));
    } else if (kind == "symbol" &&
               link.value("style", std::string()) == "dream") {
      dreams.push_back(link.at("n"));
    }
  }
  out["entrances"] = entrances;
  out["ow_holes"] = holes;
  out["spawns"] = spawns;
  out["dream_warps_in"] = dreams;
  out["badges"] = room.at("badges");
  out["plans"] = room.at("plans");
  out["pit_to_cross"] = pit_cross;
  out["warp_tiles_to_cross"] = warp_cross;
  return out;
}

std::vector<Diff> CompareWithOracle(const Json& oracle, const Json& yaze) {
  std::vector<Diff> diffs;
  std::map<std::string, const Json*> yaze_rooms;
  for (const auto& room : yaze.at("rooms")) {
    yaze_rooms[room.at("room").get<std::string>()] = &room;
  }
  for (const auto& room : oracle.at("rooms")) {
    const std::string id = room.at("room").get<std::string>();
    const auto it = yaze_rooms.find(id);
    if (it == yaze_rooms.end()) {
      diffs.push_back({id, "room", room, Json()});
      continue;
    }
    const auto expected = OracleFields(room);
    const auto actual = YazeFields(*it->second);
    for (const auto& [field, value] : expected) {
      if (actual.at(field) != value) {
        diffs.push_back({id, field, value, actual.at(field)});
      }
    }
  }
  // Two rooms can share several doors on one wall: the position is part of
  // the key.
  auto edge_id = [](const Json& edge) {
    return edge[0].get<std::string>() + "-" + edge[1].get<std::string>() + "-" +
           edge[2].get<std::string>() + "@" +
           std::to_string(edge[3].get<int>());
  };
  std::map<std::string, Json> oracle_edges;
  for (const auto& edge : oracle.at("edges")) {
    oracle_edges[edge_id(edge)] = edge;
  }
  std::map<std::string, Json> yaze_edges;
  for (const auto& edge : yaze.at("edges")) {
    const Json flat = {edge.at("a"),
                       edge.at("b"),
                       edge.at("dir"),
                       edge.at("pos"),
                       edge.at("category"),
                       edge.at("sides"),
                       edge.at("one_sided_type")};
    yaze_edges[edge_id(flat)] = flat;
  }
  for (const auto& [id, edge] : oracle_edges) {
    const auto it = yaze_edges.find(id);
    const Json actual = it == yaze_edges.end() ? Json() : it->second;
    if (actual != edge) {
      diffs.push_back({id, "edge", edge, actual});
    }
  }
  for (const auto& [id, edge] : yaze_edges) {
    if (!oracle_edges.contains(id)) {
      diffs.push_back({id, "edge", Json(), edge});
    }
  }
  return diffs;
}

TEST(DungeonAtlasOracleGoldenTest, MatchesOracleMapScriptsOnB28) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_OOS_B28");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_OOS_B28 to a copy of "
                    "Roms/Playtest/oos-v0.9.0-b28.sfc (with its .sym)";
  }
  const Json oracle = ReadJson(AtlasFixtures() / "oracle_b28_room_matrix.json");
  const std::string pinned = oracle.at("rom").at("sha1").get<std::string>();
  if (util::ComputeFileSha1Hex(rom_path) != pinned) {
    GTEST_SKIP() << rom_path << " is not the pinned b28 ROM (SHA-1 " << pinned
                 << ")";
  }
  const fs::path sym = fs::path(rom_path).replace_extension(".sym");
  if (!fs::exists(sym)) {
    GTEST_SKIP() << "b28 golden needs " << sym.string()
                 << " for the dream warps";
  }

  // Registry: the committed dungeons.json copy as Docs/Dev/Planning/.
  const fs::path project = yaze::test::UniqueTempPath("atlas_b28_project");
  fs::create_directories(project / "Docs" / "Dev" / "Planning");
  fs::copy_file(fs::path(YAZE_TEST_FIXTURE_DIR) / "room_census" /
                    "oracle_dungeon_owners.json",
                project / "Docs" / "Dev" / "Planning" / "dungeons.json");
  core::HackManifest manifest;
  ASSERT_TRUE(manifest.LoadProjectRegistry(project.string()).ok());
  fs::remove_all(project);
  ASSERT_FALSE(manifest.project_registry().dungeons.empty());

  const fs::path oracle_files = AtlasFixtures() / "oracle_b28";
  auto annotations = LoadAtlasAnnotations(
      {(oracle_files / "room_annotations.json").string(),
       (oracle_files / "overworld_annotations.json").string()},
      "vanilla");
  ASSERT_TRUE(annotations.ok()) << annotations.status();
  ASSERT_TRUE(MergeAtlasAnnotationText(
                  R"({"schema": "yaze.atlas/1", "symbol_links": [
                       {"symbol": "Oracle_Link_WarpToRoom_room", "count": 3,
                        "width": 1, "kind": "dream",
                        "label": "DREAM {n} IN"}]})",
                  "oracle_symbol_links.json", &*annotations)
                  .ok());

  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  DungeonAtlasCollectOptions options;
  options.registry = &manifest.project_registry();
  options.symbol_links = annotations->symbol_links;
  options.symbols_path = sym.string();
  auto input = CollectDungeonAtlasInput(&rom, options);
  ASSERT_TRUE(input.ok()) << input.status();
  // Oracle's hack manifest names room tag 0x3A "WarpTag".
  input->census.warp_tag_ids = {0x3A};
  const DungeonAtlas atlas = BuildDungeonAtlas(*input, *annotations);
  const Json yaze = Json::parse(DungeonAtlasToJson(atlas));
  EXPECT_EQ(yaze.at("rom").at("sha1"), pinned);

  const Json known = ReadJson(AtlasFixtures() / "oracle_b28_known_diffs.json");
  std::map<std::string, Json> allowed;
  for (const auto& entry : known.at("diffs")) {
    const Diff diff{entry.at("id").get<std::string>(),
                    entry.at("field").get<std::string>(), entry.at("oracle"),
                    entry.at("yaze")};
    allowed[diff.Key()] = entry;
  }

  const auto diffs = CompareWithOracle(oracle, yaze);
  std::set<std::string> seen;
  Json unexpected = Json::array();
  for (const auto& diff : diffs) {
    seen.insert(diff.Key());
    if (!allowed.contains(diff.Key())) {
      unexpected.push_back({{"id", diff.id},
                            {"field", diff.field},
                            {"oracle", diff.oracle},
                            {"yaze", diff.yaze}});
    }
  }
  Json stale = Json::array();
  for (const auto& [key, entry] : allowed) {
    if (!seen.contains(key)) {
      stale.push_back(entry);
    }
  }
  EXPECT_TRUE(unexpected.empty())
      << unexpected.size() << " difference(s) not in "
      << "oracle_b28_known_diffs.json:\n"
      << unexpected.dump(1);
  EXPECT_TRUE(stale.empty())
      << stale.size() << " known difference(s) no longer reproduce; remove "
      << "them:\n"
      << stale.dump(1);
}

// Vanilla US ROM, no project: built-in defaults name the dungeons by entrance
// dungeon ID. Structural checks only (no Oracle-style reference exists).
TEST(DungeonAtlasVanillaTest, VanillaDefaultsNameTheDungeons) {
  const char* rom_path = std::getenv("YAZE_TEST_ROM_VANILLA");
  if (rom_path == nullptr || !fs::exists(rom_path)) {
    GTEST_SKIP() << "Set YAZE_TEST_ROM_VANILLA to a vanilla US ROM copy";
  }
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(rom_path).ok());
  auto input = CollectDungeonAtlasInput(&rom, {});
  ASSERT_TRUE(input.ok()) << input.status();
  auto annotations = LoadAtlasAnnotations({}, "vanilla");
  ASSERT_TRUE(annotations.ok());
  const DungeonAtlas atlas = BuildDungeonAtlas(*input, *annotations);
  const RoomCensus census = BuildRoomCensus(input->census);
  ASSERT_TRUE(census.vanilla_owner_names);
  EXPECT_FALSE(atlas.owners_from_project);
  EXPECT_TRUE(atlas.warnings.empty()) << Json(atlas.warnings).dump(1);

  const auto ep = std::find_if(atlas.areas.begin(), atlas.areas.end(),
                               [](const auto& a) { return a.key == "EP"; });
  ASSERT_NE(ep, atlas.areas.end());
  EXPECT_EQ(ep->name, "Eastern Palace");
  EXPECT_EQ(ep->origin.ToString(), "default");
  EXPECT_GT(ep->rooms, 0);
  EXPECT_EQ(atlas.rooms[0xC9].area, "EP");    // Eastern Palace entrance hall.
  EXPECT_EQ(atlas.rooms[0x104].area, "INT");  // Link's house.

  // Statuses follow the census one to one (no annotations, no symbols).
  EXPECT_EQ(atlas.counts.free, census.free_count);
  EXPECT_EQ(atlas.counts.reclaimable, census.reclaimable_count);
  EXPECT_EQ(atlas.counts.in_use + atlas.counts.orphan, census.in_use_count);
  EXPECT_EQ(atlas.counts.orphan, census.orphan_count);
  EXPECT_EQ(atlas.counts.reserved, 0);
  EXPECT_FALSE(atlas.edges.empty());
  // Big keys: a chest in EP, DP, TH, PD, SP, SW, TT, IP, MM, TR and GT;
  // Hyrule Castle's comes from the ball-and-chain guard (a key drop).
  int big_key_chests = 0;
  int big_key_drops = 0;
  for (const auto& room : atlas.rooms) {
    big_key_chests += room.badges.bk_chest;
    big_key_drops += room.badges.bk_drop;
  }
  EXPECT_EQ(big_key_chests, 11);
  EXPECT_EQ(big_key_drops, 1);
}

}  // namespace
}  // namespace yaze::zelda3::atlas
