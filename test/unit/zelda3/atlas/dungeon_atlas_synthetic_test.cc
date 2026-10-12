// Dungeon Atlas model without a real ROM: door categories and pairing, 9-bit
// header targets, reserved rooms, cross-area rings, owner display and badges
// from plain input data, plus one pass over the synthetic census ROM.

#include "zelda3/atlas/dungeon_atlas.h"

#include <algorithm>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "rom/rom.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/atlas/atlas_json.h"

namespace yaze::zelda3::atlas {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

AtlasDoor Door(char wall, int pos, DoorCategory category,
               const std::string& type = "") {
  AtlasDoor door;
  door.wall = wall;
  door.pos = pos;
  door.category = category;
  door.type_name = type;
  return door;
}

TEST(DungeonDoorEdgesTest, CategoriesAndOuterWall) {
  EXPECT_EQ(DoorCategoryForType(0x1E), DoorCategory::kBigKey);
  EXPECT_EQ(DoorCategoryForType(0x28), DoorCategory::kBomb);
  EXPECT_EQ(DoorCategoryForType(0x12), DoorCategory::kExit);
  EXPECT_EQ(DoorCategoryForType(0x16), DoorCategory::kIgnore);
  EXPECT_EQ(DoorCategoryForType(0x2A), DoorCategory::kOther);

  const std::vector<AtlasDoorFact> facts = {
      {0, 14, 4, 0x00, "Normal Door"},     // N outer, pos 16
      {0, 14, 20, 0x00, "Normal Door"},    // N inner: dropped
      {1, 30, 58, 0x12, "Exit Marker"},    // S outer exit, pos 32
      {1, 31, 60, 0x00, "Normal Door"},    // S, within 2 of the exit: exit
      {3, 60, 40, 0x16, "Layer Swap"},     // Layer swap: dropped
      {2, 2, 14, 0x1C, "Small Key Door"},  // W outer key, pos 16
  };
  const auto doors = OuterWallDoors(facts);
  ASSERT_EQ(doors.size(), 4u);
  EXPECT_EQ(doors[0].wall, 'N');
  EXPECT_EQ(doors[0].pos, 16);
  EXPECT_EQ(doors[0].category, DoorCategory::kOpen);
  EXPECT_EQ(doors[1].category, DoorCategory::kExit);
  EXPECT_EQ(doors[2].pos, 33);
  EXPECT_EQ(doors[2].category, DoorCategory::kExit);
  EXPECT_EQ(doors[3].wall, 'W');
  EXPECT_EQ(doors[3].category, DoorCategory::kKey);
}

TEST(DungeonDoorEdgesTest, PairsFacingWallsStrongestCategoryWins) {
  std::vector<std::vector<AtlasDoor>> doors(kAtlasRoomCount);
  // 0x01 E <-> 0x02 W: open + key -> key, both sides.
  doors[0x01] = {Door('E', 17, DoorCategory::kOpen, "Normal Door")};
  doors[0x02] = {Door('W', 19, DoorCategory::kKey, "Small Key Door")};
  // 0x10 S with no partner: hollow (one side), one-sided type -> open.
  doors[0x10] = {Door('S', 40, DoorCategory::kOneSided, "One-sided")};
  // 0x21 N <-> 0x11 S, 4 tiles apart: not a pair (two hollow edges).
  doors[0x21] = {Door('N', 10, DoorCategory::kShutter)};
  doors[0x11] = {Door('S', 14, DoorCategory::kBomb)};
  // Exits never pair.
  doors[0x30] = {Door('E', 30, DoorCategory::kExit)};
  doors[0x31] = {Door('W', 30, DoorCategory::kOpen)};
  // Rows run across the page: 0xF5 south is 0x105.
  doors[0xF5] = {Door('S', 20, DoorCategory::kBigKey)};
  doors[0x105] = {Door('N', 20, DoorCategory::kOpen)};

  const auto edges = PairDoorEdges(doors);
  auto find = [&](int a, int b) -> const DoorEdge* {
    for (const auto& edge : edges) {
      if (edge.a == a && edge.b == b) {
        return &edge;
      }
    }
    return nullptr;
  };
  const DoorEdge* key = find(0x01, 0x02);
  ASSERT_NE(key, nullptr);
  EXPECT_EQ(key->category, DoorCategory::kKey);
  EXPECT_TRUE(key->both_sides);
  EXPECT_THAT(key->types, ElementsAre("Normal Door", "Small Key Door"));
  EXPECT_EQ(find(0x02, 0x01), nullptr);  // Used by the pair above.

  const DoorEdge* hollow = find(0x10, 0x20);
  ASSERT_NE(hollow, nullptr);
  EXPECT_FALSE(hollow->both_sides);
  EXPECT_EQ(hollow->category, DoorCategory::kOpen);
  EXPECT_TRUE(hollow->one_sided_type);

  ASSERT_NE(find(0x11, 0x21), nullptr);
  EXPECT_FALSE(find(0x11, 0x21)->both_sides);
  ASSERT_NE(find(0x21, 0x11), nullptr);
  EXPECT_EQ(find(0x30, 0x31), nullptr);
  ASSERT_NE(find(0x31, 0x30), nullptr);
  EXPECT_FALSE(find(0x31, 0x30)->both_sides);

  const DoorEdge* page = find(0xF5, 0x105);
  ASSERT_NE(page, nullptr);
  EXPECT_TRUE(page->both_sides);
  EXPECT_EQ(page->category, DoorCategory::kBigKey);
}

// A plain-data input: every room empty and unreached unless set below.
DungeonAtlasInput EmptyInput() {
  DungeonAtlasInput input;
  input.census.rooms.resize(kAtlasRoomCount);
  input.extras.resize(kAtlasRoomCount);
  for (int r = 0; r < kAtlasRoomCount; ++r) {
    input.census.rooms[r].room_id = r;
  }
  input.rom.name = "synthetic.sfc";
  return input;
}

void AddEntrance(DungeonAtlasInput* input, int entrance, int room,
                 int dungeon_id, int ow_map) {
  RoomCensusEntranceFact fact;
  fact.entrance_id = entrance;
  fact.room_id = room;
  fact.dungeon_id = dungeon_id;
  fact.kind = RoomReferenceKind::kEntrance;
  fact.ow_map = ow_map;
  fact.placement = "OW";
  input->census.entrances.push_back(fact);
  input->census.rooms[room].object_count = 10;
}

const AtlasLink* FindLink(const std::vector<AtlasLink>& links, LinkKind kind) {
  for (const auto& link : links) {
    if (link.kind == kind) {
      return &link;
    }
  }
  return nullptr;
}

TEST(DungeonAtlasSyntheticTest, RedefinedHoleTagDoesNotInventARoute) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  input.census.warp_tag_ids = {0x3A};
  auto& facts = input.census.rooms[0x10];
  facts.tag1 = 0x3A;
  facts.has_hole_tag = true;
  facts.holewarp_byte = 0x20;

  auto atlas = BuildDungeonAtlas(input, {});
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kTagPit), nullptr);
  EXPECT_FALSE(atlas.rooms[0x20].reached);
  EXPECT_EQ(atlas.rooms[0x20].status, RoomStatus::kFree);

  // Ordinary pits remain live without a project tag override.
  facts.tag1 = 0;
  facts.has_hole_tag = false;
  facts.has_pits = true;
  atlas = BuildDungeonAtlas(input, {});
  const auto* pit = FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit);
  ASSERT_NE(pit, nullptr);
  EXPECT_EQ(pit->to, 0x20);
  EXPECT_TRUE(atlas.rooms[0x20].reached);
}

TEST(DungeonAtlasSyntheticTest, WarpTagSlotsReplaceTheRawWarpTileTarget) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  input.census.warp_tag_ids = {0x3A};
  auto& facts = input.census.rooms[0x10];
  facts.tag1 = 0x3A;
  facts.has_warp_tiles = true;
  facts.holewarp_byte = 0x20;
  facts.stair_bytes = {0x30, 0x31, 0x32, 0x33};

  const auto atlas = BuildDungeonAtlas(input, {});
  const auto& links = atlas.rooms[0x10].links_out;
  EXPECT_EQ(FindLink(links, LinkKind::kWarpTiles), nullptr);
  EXPECT_FALSE(atlas.rooms[0x20].reached);
  EXPECT_EQ(std::count_if(links.begin(), links.end(),
                          [](const auto& link) {
                            return link.kind == LinkKind::kWarpTag;
                          }),
            4);

  // Without the tag override, the warp tile keeps its header destination.
  facts.tag1 = 0;
  const auto plain = BuildDungeonAtlas(input, {});
  const auto* warp =
      FindLink(plain.rooms[0x10].links_out, LinkKind::kWarpTiles);
  ASSERT_NE(warp, nullptr);
  EXPECT_EQ(warp->to, 0x20);
  EXPECT_EQ(FindLink(plain.rooms[0x10].links_out, LinkKind::kWarpTag), nullptr);
}

TEST(DungeonAtlasSyntheticTest, DamageOnlyHoleSourcesDoNotClaimTraversal) {
  for (int cause = 0; cause < 3; ++cause) {
    DungeonAtlasInput input = EmptyInput();
    AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
    auto& facts = input.census.rooms[0x10];
    facts.has_pits = cause == 0;
    facts.has_hole_tag = cause == 1;
    facts.hole_sprite = cause == 2 ? "Falling-floor overlord" : "";
    facts.holewarp_byte = 0x20;
    facts.in_pit_damage_table = true;

    auto atlas = BuildDungeonAtlas(input, {});
    const auto* damage =
        FindLink(atlas.rooms[0x10].links_out, LinkKind::kPitDamage);
    ASSERT_NE(damage, nullptr) << cause;
    EXPECT_EQ(damage->to, 0x20) << cause;
    EXPECT_FALSE(atlas.rooms[0x20].reached) << cause;

    // Warp tiles remain a route even in a damage-and-respawn room.
    facts.has_warp_tiles = true;
    atlas = BuildDungeonAtlas(input, {});
    const auto* warp =
        FindLink(atlas.rooms[0x10].links_out, LinkKind::kWarpTiles);
    ASSERT_NE(warp, nullptr) << cause;
    EXPECT_EQ(warp->to, 0x20) << cause;
    EXPECT_TRUE(atlas.rooms[0x20].reached) << cause;
    EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kPitDamage),
              nullptr)
        << cause;
  }
}

TEST(DungeonAtlasSyntheticTest, FallingFloorSpriteShowsItsPitDestination) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  auto& facts = input.census.rooms[0x10];
  facts.hole_sprite = "Falling-floor overlord";
  facts.holewarp_byte = 0x20;

  const auto atlas = BuildDungeonAtlas(input, {});
  const auto* pit = FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit);
  ASSERT_NE(pit, nullptr);
  EXPECT_EQ(pit->to, 0x20);
  EXPECT_EQ(pit->note, facts.hole_sprite);
  EXPECT_TRUE(atlas.rooms[0x20].reached);
}

TEST(DungeonAtlasSyntheticTest, SharedHoleCauseKeepsTagAndSpritePrecedence) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  auto& facts = input.census.rooms[0x10];
  facts.has_hole_tag = true;
  facts.tag1 = 0x22;
  facts.hole_sprite = "Falling-floor overlord";
  facts.holewarp_byte = 0x20;

  auto atlas = BuildDungeonAtlas(input, {});
  const auto* tag = FindLink(atlas.rooms[0x10].links_out, LinkKind::kTagPit);
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->to, 0x20);
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit), nullptr);

  // An active project warp tag replaces the sprite's raw destination too.
  input.census.warp_tag_ids = {0x22};
  atlas = BuildDungeonAtlas(input, {});
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit), nullptr);
  EXPECT_FALSE(atlas.rooms[0x20].reached);

  // With no tag override, the sprite remains a live floor-drop source.
  facts.tag1 = 0;
  facts.has_hole_tag = false;
  atlas = BuildDungeonAtlas(input, {});
  const auto* pit = FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit);
  ASSERT_NE(pit, nullptr);
  EXPECT_EQ(pit->to, 0x20);
  EXPECT_EQ(pit->note, facts.hole_sprite);
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kTagPit), nullptr);
}

TEST(DungeonAtlasSyntheticTest, OnlyActiveHoleSourcesWarnAboutInvalidTargets) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x104, 0x04, 0x00);
  auto& facts = input.census.rooms[0x104];
  facts.tag1 = 0x3A;
  facts.has_hole_tag = true;
  facts.holewarp_byte = 0x40;
  input.census.warp_tag_ids = {0x3A};

  auto atlas = BuildDungeonAtlas(input, {});
  EXPECT_THAT(atlas.warnings,
              ::testing::Not(::testing::Contains(HasSubstr("holewarp"))));
  facts.tag1 = 0;
  facts.has_hole_tag = false;
  facts.has_pits = true;
  atlas = BuildDungeonAtlas(input, {});
  EXPECT_THAT(atlas.warnings,
              ::testing::Contains(HasSubstr("holewarp points past $127")));
  EXPECT_EQ(FindLink(atlas.rooms[0x104].links_out, LinkKind::kPit), nullptr);
}

TEST(DungeonAtlasSyntheticTest, WarpTagDoesNotReachTheUnusedRawPitRoom) {
  auto input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  auto& facts = input.census.rooms[0x10];
  facts.tag2 = 0x3A;
  facts.has_pits = true;
  facts.holewarp_byte = 0x30;
  facts.stair_bytes = {0x20, 0x21, 0x22, 0x23};
  input.census.warp_tag_ids = {0x3A};
  const auto atlas = BuildDungeonAtlas(input, {});
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_out, LinkKind::kPit), nullptr);
  EXPECT_FALSE(atlas.rooms[0x30].reached);
  EXPECT_EQ(atlas.rooms[0x30].status, RoomStatus::kFree);
  for (int target = 0x20; target <= 0x23; ++target) {
    EXPECT_TRUE(atlas.rooms[target].reached);
  }
}

TEST(DungeonAtlasSyntheticTest, HeaderTargetsKeepTheHighByte) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x10, 0x104, 0x04, 0x20);
  auto& facts = input.census.rooms[0x104];
  facts.has_warp_tiles = true;
  facts.holewarp_byte = 0x05;  // 0x104 -> 0x105, not 0x005.
  facts.stair_bytes = {0x20, 0x00, 0x00, 0x00};
  facts.stair_slot_used = {true, false, false, false};
  facts.stair_slot_estimated = {true, false, false, false};
  // A page-1 byte >= 0x28 resolves past $127: dropped with a warning.
  auto& far = input.census.rooms[0x105];
  far.has_pits = true;
  far.holewarp_byte = 0x40;

  const auto atlas = BuildDungeonAtlas(input, {});
  const auto& room = atlas.rooms[0x104];
  const AtlasLink* warp = FindLink(room.links_out, LinkKind::kWarpTiles);
  ASSERT_NE(warp, nullptr);
  EXPECT_EQ(warp->to, 0x105);
  const AtlasLink* stair = FindLink(room.links_out, LinkKind::kStair);
  ASSERT_NE(stair, nullptr);
  EXPECT_EQ(stair->to, 0x120);
  EXPECT_EQ(stair->index, 1);
  EXPECT_EQ(stair->note, "slot by placement order");
  EXPECT_EQ(FindLink(atlas.rooms[0x105].links_out, LinkKind::kPit), nullptr);
  EXPECT_THAT(atlas.warnings,
              ::testing::Contains(HasSubstr("room 0x105 holewarp points past "
                                            "$127 (0x140)")));

  const AtlasLink* entrance = FindLink(room.links_in, LinkKind::kEntrance);
  ASSERT_NE(entrance, nullptr);
  EXPECT_EQ(entrance->entrance, 0x10);
  EXPECT_EQ(entrance->ow_map, 0x20);
}

TEST(DungeonAtlasSyntheticTest, OwnersAreasReservedAndCrossAreaRings) {
  DungeonAtlasInput input = EmptyInput();
  // Project dungeon "D1" owns 0x10 and 0x11; 0x10 has the entrance.
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  input.census.project_owners = {{"D1", "Forest Temple", {0x10, 0x11}}};
  input.census.rooms[0x11].object_count = 5;
  // Pits in 0x10 drop into 0x11 (same area).
  input.census.rooms[0x10].has_pits = true;
  input.census.rooms[0x10].holewarp_byte = 0x11;
  // Chests: a small key, a big chest with the big key, an unknown item.
  input.extras[0x10].chests = {{0x24, false}, {0x32, true}, {0x40, false}};
  input.extras[0x10].pot_items = {0x08, 0x08, 0x01};
  input.extras[0x10].key_drops_small = 1;
  // Symbol link lands in free 0x30 (reserved); plan on 0x31.
  input.symbol_reads = {{true, {0x30, 0x10}}};
  input.symbols_path = "hack.sym";

  AtlasAnnotations annotations;
  ApplyVanillaAtlasDefaults(&annotations);
  ASSERT_TRUE(MergeAtlasAnnotationText(R"({
    "dungeon_codes": {"D1": "DG1"},
    "areas": [{"key": "HUB", "name": "Hub", "rooms": ["0x20"]}],
    "runtime_pits": [{"room": "0x20", "to": "0x11", "source": "emulator"}],
    "symbol_links": [{"symbol": "Hack_Dream", "count": 2, "kind": "dream",
                      "label": "DREAM {n} IN"}],
    "plans": [{"key": "P", "label": "PLAN", "rooms": ["0x31"]}]
  })",
                                       "room_annotations.json", &annotations)
                  .ok());

  const auto atlas = BuildDungeonAtlas(input, annotations);
  EXPECT_TRUE(atlas.owners_from_project);
  const auto& d1 = atlas.rooms[0x10];
  EXPECT_EQ(d1.area, "DG1");
  EXPECT_EQ(d1.area_origin.ToString(), "registry");
  EXPECT_EQ(d1.owner, "D1");
  EXPECT_EQ(d1.status, RoomStatus::kInUse);
  const AtlasLink* pit = FindLink(d1.links_out, LinkKind::kPit);
  ASSERT_NE(pit, nullptr);
  EXPECT_EQ(pit->to, 0x11);
  EXPECT_FALSE(pit->cross_area);
  EXPECT_EQ(pit->origin.ToString(), "rom");
  EXPECT_EQ(d1.badges.k_chest, 1);
  EXPECT_EQ(d1.badges.bk_chest, 1);
  EXPECT_EQ(d1.badges.big_chest, 1);
  EXPECT_EQ(d1.badges.other, 1);
  EXPECT_EQ(d1.badges.k_pot, 2);
  EXPECT_EQ(d1.badges.k_drop, 1);

  const auto& hub = atlas.rooms[0x20];
  EXPECT_EQ(hub.area, "HUB");
  EXPECT_EQ(hub.area_origin.ToString(), "annotation:room_annotations.json");
  const AtlasLink* runtime = FindLink(hub.links_out, LinkKind::kAnnotation);
  ASSERT_NE(runtime, nullptr);
  EXPECT_EQ(runtime->style, "pit");
  EXPECT_EQ(runtime->to, 0x11);
  EXPECT_TRUE(runtime->cross_area);  // HUB -> DG1: red ring.
  EXPECT_EQ(runtime->note, "emulator");

  const auto& reserved = atlas.rooms[0x30];
  EXPECT_EQ(reserved.status, RoomStatus::kReserved);
  EXPECT_EQ(reserved.status_origin.ToString(), "symbol:Hack_Dream");
  const AtlasLink* dream = FindLink(reserved.links_in, LinkKind::kSymbol);
  ASSERT_NE(dream, nullptr);
  EXPECT_EQ(dream->label, "DREAM 1 IN");
  EXPECT_EQ(dream->style, "dream");
  // A symbol link into an in-use room does not change its status.
  EXPECT_EQ(atlas.rooms[0x10].status, RoomStatus::kInUse);
  EXPECT_EQ(FindLink(atlas.rooms[0x10].links_in, LinkKind::kSymbol)->label,
            "DREAM 2 IN");

  EXPECT_EQ(atlas.rooms[0x31].status, RoomStatus::kFree);
  EXPECT_THAT(atlas.rooms[0x31].plans, ElementsAre("P"));
  EXPECT_EQ(atlas.rooms[0x40].area, "NONE");

  // Areas: the registry dungeon, the annotation area, NONE.
  ASSERT_GE(atlas.areas.size(), 3u);
  EXPECT_EQ(atlas.areas[0].key, "DG1");
  EXPECT_EQ(atlas.areas[0].name, "Forest Temple");
  EXPECT_EQ(atlas.areas[0].origin.ToString(),
            "annotation:room_annotations.json");
  EXPECT_EQ(atlas.areas[0].rooms, 2);
  EXPECT_EQ(atlas.areas[1].key, "HUB");
  ASSERT_FALSE(atlas.key_table.empty());
  EXPECT_EQ(atlas.key_table[0].area, "DG1");
  EXPECT_EQ(atlas.key_table[0].bk, 1);
  EXPECT_EQ(atlas.counts.reserved, 1);

  // JSON carries the provenance strings.
  const auto json = nlohmann::json::parse(DungeonAtlasToJson(atlas));
  EXPECT_EQ(json.at("schema"), "yaze.atlas.dungeon/1");
  EXPECT_EQ(json.at("rooms").size(), static_cast<size_t>(kAtlasRoomCount));
  EXPECT_EQ(json.at("rooms").at(0x30).at("status_source"), "symbol:Hack_Dream");
  EXPECT_EQ(json.at("rooms").at(0x20).at("links_out").at(0).at("source"),
            "annotation:room_annotations.json");
}

TEST(DungeonAtlasSyntheticTest, ReservedByAnnotationAndDuplicateWarning) {
  DungeonAtlasInput input = EmptyInput();
  AddEntrance(&input, 0x01, 0x10, 0x04, 0x00);
  input.census.rooms[0x10].has_pits = true;
  input.census.rooms[0x10].holewarp_byte = 0x11;
  AtlasAnnotations annotations;
  ASSERT_TRUE(MergeAtlasAnnotationText(R"({
    "schema": "yaze.atlas/1",
    "rooms": {"0x50": {"status": "reserved", "name": "Kept"}},
    "links": [{"view": "dungeon", "kind": "pit", "from": "0x10", "to": "0x11"}]
  })",
                                       "atlas.json", &annotations)
                  .ok());
  const auto atlas = BuildDungeonAtlas(input, annotations);
  EXPECT_EQ(atlas.rooms[0x50].status, RoomStatus::kReserved);
  EXPECT_EQ(atlas.rooms[0x50].status_origin.ToString(),
            "annotation:atlas.json");
  EXPECT_EQ(atlas.rooms[0x50].name, "Kept");
  EXPECT_THAT(atlas.warnings,
              ::testing::Contains(HasSubstr("pit link 0x10 -> 0x11 repeats a "
                                            "ROM fact")));
  // Without a registry, a derived owner gets its own area, named by the
  // owner ID when no annotation names its dungeon ID.
  EXPECT_EQ(atlas.rooms[0x10].area, "ID04");
}

TEST(DungeonAtlasSyntheticTest, CollectsFromTheSyntheticCensusRom) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(zelda3::test::BuildRoomCensusTestRom()).ok());
  const std::vector<uint8_t> before = rom.vector();
  auto input = CollectDungeonAtlasInput(&rom, {});
  ASSERT_TRUE(input.ok()) << input.status();
  EXPECT_EQ(input->extras.size(), static_cast<size_t>(kAtlasRoomCount));
  EXPECT_EQ(input->rom.sha1.size(), 40u);
  auto annotations = LoadAtlasAnnotations({}, "vanilla");
  ASSERT_TRUE(annotations.ok());
  const auto atlas = BuildDungeonAtlas(*input, *annotations);
  EXPECT_EQ(atlas.counts.free, kAtlasRoomCount - 4);
  const AtlasLink* warp =
      FindLink(atlas.rooms[0x000].links_out, LinkKind::kWarpTiles);
  ASSERT_NE(warp, nullptr);
  EXPECT_EQ(warp->to, 0x004);
  const AtlasLink* high =
      FindLink(atlas.rooms[0x104].links_out, LinkKind::kWarpTiles);
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->to, 0x105);
  const AtlasLink* spawn =
      FindLink(atlas.rooms[0x104].links_in, LinkKind::kSpawn);
  ASSERT_NE(spawn, nullptr);
  EXPECT_EQ(spawn->index, 0);
  EXPECT_EQ(rom.vector(), before);  // Read-only.
}

}  // namespace
}  // namespace yaze::zelda3::atlas
