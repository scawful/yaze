// Room census model: synthetic inputs and a synthetic ROM.

#include "zelda3/dungeon/room_census.h"

#include <algorithm>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rom/rom.h"
#include "unit/zelda3/dungeon/room_census_test_rom.h"
#include "zelda3/dungeon/room_census_vanilla_fingerprints.h"

namespace yaze::zelda3 {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

constexpr int kNorth = 0;
constexpr int kSouth = 1;
constexpr int kEast = 3;
constexpr int kWest = 2;

RoomCensusInput EmptyInput() {
  RoomCensusInput input;
  input.rooms.resize(kRoomCensusRoomCount);
  for (int i = 0; i < kRoomCensusRoomCount; ++i) {
    input.rooms[i].room_id = i;
  }
  input.has_vanilla_baseline = true;
  return input;
}

void GiveContent(RoomCensusInput& input, int room, int objects = 20,
                 float vanilla = 0.0f) {
  input.rooms[room].object_count = objects;
  input.rooms[room].sprite_count = 2;
  input.rooms[room].vanilla_similarity = vanilla;
}

void AddEntrance(RoomCensusInput& input, int entrance, int room, int dungeon,
                 RoomReferenceKind kind = RoomReferenceKind::kEntrance) {
  RoomCensusEntranceFact fact;
  fact.entrance_id = entrance;
  fact.room_id = room;
  fact.dungeon_id = dungeon;
  fact.kind = kind;
  fact.placement = "OW 0x00";
  input.entrances.push_back(fact);
}

void AddDoor(RoomCensusInput& input, int room, int direction, int along) {
  input.rooms[room].doors.push_back({direction, along, /*outer=*/true});
}

std::vector<int> RoomsWithStatus(const RoomCensus& census,
                                 RoomCensusStatus status) {
  std::vector<int> out;
  for (const auto& entry : census.rooms) {
    if (entry.status == status) {
      out.push_back(entry.room_id);
    }
  }
  return out;
}

bool HasReason(const RoomCensusEntry& entry, const std::string& text) {
  return std::any_of(
      entry.reasons.begin(), entry.reasons.end(),
      [&](const std::string& r) { return r.find(text) != std::string::npos; });
}

class RoomCensusTest : public ::testing::Test {
 protected:
  void SetUp() override {
    input_ = EmptyInput();
    // Every room has content unless a test clears it, so only the rooms a
    // test cares about can be free.
    for (int i = 0; i < kRoomCensusRoomCount; ++i) {
      GiveContent(input_, i, 20, 0.0f);
    }
    // Entrance 0x04 (Eastern Palace ID) -> 0x10, door to 0x11.
    AddEntrance(input_, 0x04, 0x10, 0x04);
  }
  RoomCensusInput input_;
};

TEST_F(RoomCensusTest, MutualDoorReachesNeighbor) {
  AddDoor(input_, 0x10, kEast, 31);
  AddDoor(input_, 0x11, kWest, 31);
  const auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x10].reached);
  EXPECT_TRUE(census.rooms[0x11].reached);
  EXPECT_EQ(census.rooms[0x11].status, RoomCensusStatus::kInUse);
  EXPECT_TRUE(HasReason(census.rooms[0x11], "door from 0x10"));
}

TEST_F(RoomCensusTest, OneSidedDoorDoesNotReach) {
  AddDoor(input_, 0x10, kEast, 31);
  input_.rooms[0x11].object_count = 0;
  const auto census = BuildRoomCensus(input_);
  EXPECT_FALSE(census.rooms[0x11].reached);
  EXPECT_EQ(census.rooms[0x11].status, RoomCensusStatus::kFree);
  EXPECT_TRUE(HasReason(census.rooms[0x11], "one-sided door from 0x10"));
}

TEST_F(RoomCensusTest, EmptyUnreachedRoomIsFree) {
  input_.rooms[0x93].object_count = 0;
  input_.rooms[0x93].sprite_count = 0;
  // One object and one sprite still counts as empty.
  input_.rooms[0xA0].object_count = 1;
  input_.rooms[0xA0].sprite_count = 1;
  const auto census = BuildRoomCensus(input_);
  EXPECT_THAT(RoomsWithStatus(census, RoomCensusStatus::kFree),
              ElementsAre(0x93, 0xA0));
  EXPECT_TRUE(HasReason(census.rooms[0x93], "empty: 0 objects, 0 sprites"));
}

TEST_F(RoomCensusTest, VanillaLeftoverIsReclaimableAuthoredIsOrphan) {
  input_.rooms[0x01].vanilla_similarity = 0.98f;  // leftover
  input_.rooms[0x30].vanilla_similarity = 0.38f;  // partly edited
  input_.rooms[0x90].vanilla_similarity = 0.00f;  // authored orphan
  const auto census = BuildRoomCensus(input_);
  EXPECT_EQ(census.rooms[0x01].status, RoomCensusStatus::kReclaimable);
  EXPECT_TRUE(HasReason(census.rooms[0x01], "vanilla leftover: 98%"));
  EXPECT_EQ(census.rooms[0x30].status, RoomCensusStatus::kReclaimable);
  EXPECT_TRUE(HasReason(census.rooms[0x30], "partly edited"));
  EXPECT_EQ(census.rooms[0x90].status, RoomCensusStatus::kInUse);
  EXPECT_TRUE(census.rooms[0x90].orphan);
}

TEST_F(RoomCensusTest, HeaderOnlyStairIsACaveatNotAReference) {
  // 0x10 has stair byte 0x93 in slot 0 but no stair object for it.
  input_.rooms[0x10].stair_bytes[0] = 0x93;
  input_.rooms[0x93].object_count = 0;
  auto census = BuildRoomCensus(input_);
  EXPECT_EQ(census.rooms[0x93].status, RoomCensusStatus::kFree);
  EXPECT_TRUE(HasReason(census.rooms[0x93],
                        "header stair slot 1 of 0x10 (no stair object"));

  // With a stair object on that slot it is a real stair.
  input_.rooms[0x10].stair_slot_used[0] = true;
  census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x93].reached);
  EXPECT_EQ(census.rooms[0x93].status, RoomCensusStatus::kInUse);
}

TEST_F(RoomCensusTest, StairInHighPageKeepsHighByte) {
  // Mayor's House: entrance -> 0x119, stair slot 0 byte 0x1D -> 0x11D.
  AddEntrance(input_, 0x61, 0x119, kRoomCensusInteriorDungeonId);
  input_.rooms[0x119].stair_bytes[0] = 0x1D;
  input_.rooms[0x119].stair_slot_used[0] = true;
  const auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x11D].reached);
  EXPECT_FALSE(census.rooms[0x01D].reached);
  EXPECT_TRUE(HasReason(census.rooms[0x11D], "stair (slot 1) in 0x119"));
  EXPECT_TRUE(census.rooms[0x11D].interior);
}

TEST_F(RoomCensusTest, PitsInDamageTableDoNotWarp) {
  input_.rooms[0x10].has_pits = true;
  input_.rooms[0x10].holewarp_byte = 0x20;
  input_.rooms[0x10].in_pit_damage_table = true;
  auto census = BuildRoomCensus(input_);
  EXPECT_FALSE(census.rooms[0x20].reached);
  EXPECT_TRUE(HasReason(census.rooms[0x20], "RoomsWithPitDamage"));

  // Warp tiles use the holewarp even in damage-table rooms.
  input_.rooms[0x10].has_warp_tiles = true;
  census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x20].reached);
}

TEST_F(RoomCensusTest, UnplacedEntranceIsOnlyACaveat) {
  AddEntrance(input_, 0x36, 0x10 + 0x20, 0x02,
              RoomReferenceKind::kUnplacedEntrance);
  input_.rooms[0x30].vanilla_similarity = 1.0f;
  const auto census = BuildRoomCensus(input_);
  EXPECT_EQ(census.rooms[0x30].status, RoomCensusStatus::kReclaimable);
  EXPECT_TRUE(HasReason(census.rooms[0x30],
                        "entrance 0x36 (not placed on the overworld)"));
}

TEST_F(RoomCensusTest, WarpTagUsesHeaderStairSlots) {
  input_.rooms[0x10].tag1 = 0x3A;
  input_.rooms[0x10].stair_bytes = {0x09, 0x00, 0x6A, 0x09};
  input_.warp_tag_ids.insert(0x3A);
  const auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x09].reached);
  EXPECT_TRUE(census.rooms[0x00].reached);
  EXPECT_TRUE(census.rooms[0x6A].reached);
}

TEST_F(RoomCensusTest, DerivedOwnersUseDungeonIdsAndInteriors) {
  // Mostly-vanilla ROM: vanilla dungeon names.
  for (auto& room : input_.rooms) {
    room.vanilla_similarity = 1.0f;
  }
  AddDoor(input_, 0x10, kSouth, 30);
  AddDoor(input_, 0x20, kNorth, 30);
  AddEntrance(input_, 0x50, 0x104, kRoomCensusInteriorDungeonId);
  const auto census = BuildRoomCensus(input_);
  ASSERT_GE(census.rooms[0x20].owner_index, 0);
  EXPECT_EQ(census.owners[census.rooms[0x20].owner_index].name,
            "Eastern Palace");
  EXPECT_FALSE(census.rooms[0x20].interior);
  EXPECT_TRUE(census.rooms[0x104].interior);
  EXPECT_FALSE(census.owners_from_project);
  EXPECT_TRUE(census.vanilla_owner_names);

  // A hack reuses dungeon IDs: no vanilla names.
  for (auto& room : input_.rooms) {
    room.vanilla_similarity = 0.1f;
  }
  const auto hack = BuildRoomCensus(input_);
  EXPECT_FALSE(hack.vanilla_owner_names);
  EXPECT_EQ(hack.owners[hack.rooms[0x20].owner_index].name, "Dungeon ID 0x04");
}

TEST_F(RoomCensusTest, ProjectOwnersWinAndUnreachedListedRoomsStayInUse) {
  input_.project_owners.push_back({"FOS", "Fortress of Secrets", {0x10, 0x4C}});
  input_.rooms[0x4C].object_count = 0;  // empty, unreached, but listed
  const auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.owners_from_project);
  EXPECT_EQ(census.owners[census.rooms[0x10].owner_index].id, "FOS");
  EXPECT_EQ(census.rooms[0x4C].status, RoomCensusStatus::kInUse);
  EXPECT_TRUE(census.rooms[0x4C].orphan);
  EXPECT_TRUE(HasReason(census.rooms[0x4C], "listed in project dungeon FOS"));
}

TEST_F(RoomCensusTest, PauseMapEvidenceOnlyWithoutProject) {
  for (auto& room : input_.rooms) {
    room.vanilla_similarity = 1.0f;
  }
  input_.pause_map_rooms.resize(3);
  input_.pause_map_rooms[2] = {0x10, 0x44};  // dungeon ID 0x04
  input_.rooms[0x44].vanilla_similarity = 1.0f;
  auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x44].reached);
  EXPECT_EQ(census.owners[census.rooms[0x44].owner_index].name,
            "Eastern Palace");

  input_.project_owners.push_back({"D1", "Mushroom Grotto", {0x10}});
  census = BuildRoomCensus(input_);
  EXPECT_FALSE(census.rooms[0x44].reached);
  EXPECT_EQ(census.rooms[0x44].status, RoomCensusStatus::kReclaimable);
  EXPECT_TRUE(HasReason(census.rooms[0x44], "project ownership takes"));
}

// Review finding: the stair reciprocity check indexed facts[target] before
// checking the range. A page-1 stair byte 0x28 resolves to 0x128, one past
// the last room (ASan reports the read); 0xFF resolves to 0x1FF.
TEST_F(RoomCensusTest, OutOfRangeStairDestinationIsIgnored) {
  AddEntrance(input_, 0x61, 0x110, kRoomCensusInteriorDungeonId);
  input_.rooms[0x110].stair_bytes = {0x28, 0xFF, 0x00, 0x00};
  input_.rooms[0x110].stair_slot_used = {true, true, false, false};
  const auto census = BuildRoomCensus(input_);
  EXPECT_TRUE(census.rooms[0x110].reached);
  ASSERT_EQ(static_cast<int>(census.rooms.size()), kRoomCensusRoomCount);
  // No room gains a reference from the out-of-range stairs.
  for (const auto& entry : census.rooms) {
    for (const auto& ref : entry.references) {
      EXPECT_NE(ref.from_room, 0x110) << entry.room_id << ": " << ref.detail;
    }
  }
}

TEST_F(RoomCensusTest, FreeBlocksMergeNearbyRoomsOnOnePage) {
  for (int room : {0x93, 0x94, 0x95, 0x96, 0xA0, 0xA6, 0xB0, 0xE9, 0xF7, 0x1F,
                   0xF5, 0x105}) {
    input_.rooms[room].object_count = 0;
  }
  const auto census = BuildRoomCensus(input_);
  ASSERT_FALSE(census.free_clusters.empty());
  const auto& largest = census.free_clusters.front();
  EXPECT_EQ(largest.summary, "Row 9: 0x93-0x96 + 0xA0/0xA6/0xB0, 7 rooms");
  EXPECT_THAT(largest.core_rooms, ElementsAre(0x93, 0x94, 0x95, 0x96, 0xA6));
  // 0xF5 and 0x105 are one grid row apart but on different $A1 pages.
  for (const auto& cluster : census.free_clusters) {
    const bool has_f5 =
        std::count(cluster.rooms.begin(), cluster.rooms.end(), 0xF5) > 0;
    const bool has_105 =
        std::count(cluster.rooms.begin(), cluster.rooms.end(), 0x105) > 0;
    EXPECT_FALSE(has_f5 && has_105);
  }
}

TEST(RoomCensusFormatTest, BlockSummary) {
  EXPECT_EQ(FormatRoomBlockSummary({0x02}), "Row 0: 0x02, 1 room");
  EXPECT_EQ(FormatRoomBlockSummary({0xE9, 0xF7}),
            "Row E: 0xE9 + 0xF7, 2 rooms");
  EXPECT_EQ(FormatRoomBlockSummary({0x96, 0x93, 0x95, 0x94}),
            "Row 9: 0x93-0x96, 4 rooms");
}

TEST(RoomCensusFingerprintTest, SimilarityIsMultisetJaccard) {
  const std::vector<uint16_t> a = {1, 2, 3, 3};
  const std::vector<uint16_t> b = {1, 2, 3, 4};
  EXPECT_FLOAT_EQ(RoomFingerprintSimilarity(a, a), 1.0f);
  EXPECT_FLOAT_EQ(RoomFingerprintSimilarity(a, b), 3.0f / 5.0f);
  EXPECT_FLOAT_EQ(RoomFingerprintSimilarity({}, {}), 1.0f);
  EXPECT_FLOAT_EQ(RoomFingerprintSimilarity({}, b), 0.0f);
  EXPECT_NE(HashRoomCensusObject(0x0A4, 10, 12, 3),
            HashRoomCensusObject(0x0A4, 10, 13, 3));
}

TEST(RoomCensusFingerprintTest, BuiltinTableCoversAllRooms) {
  ASSERT_TRUE(HasBuiltinVanillaRoomFingerprints());
  int non_empty = 0;
  for (int room = 0; room < kRoomCensusRoomCount; ++room) {
    const auto fingerprint = BuiltinVanillaRoomFingerprint(room);
    EXPECT_TRUE(std::is_sorted(fingerprint.begin(), fingerprint.end()));
    non_empty += fingerprint.empty() ? 0 : 1;
  }
  EXPECT_GT(non_empty, 200);
  EXPECT_TRUE(BuiltinVanillaRoomFingerprint(kRoomCensusRoomCount).empty());
}

class RoomCensusSyntheticRomTest : public ::testing::Test {
 protected:
  std::vector<uint8_t> data_ = test::BuildRoomCensusTestRom();
};

TEST_F(RoomCensusSyntheticRomTest, ReadsWarpsEntrancesAndHighByte) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data_).ok());
  auto input_or = CollectRoomCensusInput(&rom);
  ASSERT_TRUE(input_or.ok()) << input_or.status();
  const auto& input = *input_or;
  EXPECT_TRUE(input.rooms[0x000].has_warp_tiles);
  EXPECT_EQ(input.rooms[0x000].object_count, 1);
  EXPECT_EQ(input.rooms[0x104].holewarp_byte, 0x05);
  EXPECT_EQ(input.rooms[0x093].object_count, 0);

  const RoomCensus census = BuildRoomCensus(input);
  EXPECT_TRUE(census.rooms[0x000].reached);
  EXPECT_TRUE(census.rooms[0x004].reached);
  EXPECT_TRUE(census.rooms[0x104].reached);
  EXPECT_TRUE(census.rooms[0x105].reached);
  EXPECT_FALSE(census.rooms[0x005].reached);
  EXPECT_TRUE(HasReason(census.rooms[0x105], "warp tiles in 0x104"));
  EXPECT_EQ(census.rooms[0x093].status, RoomCensusStatus::kFree);
  EXPECT_EQ(census.free_count, kRoomCensusRoomCount - 4);
  EXPECT_EQ(census.reclaimable_count, 0);
  EXPECT_FALSE(census.expanded_entrance_tables);
}

}  // namespace
}  // namespace yaze::zelda3
