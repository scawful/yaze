#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::test {
namespace {

constexpr uint32_t kSpritesetTable = 0x5B57;

struct LoadedGroups {
  Rom rom;
  zelda3::GameData data;
};

LoadedGroups LoadFixtureGroups() {
  LoadedGroups loaded;
  auto fixture = BuildGfxSheetTestRom();
  EXPECT_TRUE(loaded.rom.LoadFromData(fixture.bytes).ok());
  loaded.data.version = zelda3_version::US;
  EXPECT_TRUE(zelda3::LoadGfxGroups(loaded.rom, loaded.data).ok());
  return loaded;
}

LoadedGroups LoadRoomsetUsageGroups(
    bool custom = false, zelda3_version version = zelda3_version::US) {
  LoadedGroups loaded;
  auto bytes = BuildGfxSheetTestRom().bytes;
  bytes.resize(0x200000, 0);
  loaded.data.version = version;
  for (int i = 0; i < 0x40; ++i) {
    bytes[zelda3::kOverworldMapParentId + i] = static_cast<uint8_t>(i);
  }
  if (custom) {
    bytes[zelda3::OverworldCustomASMHasBeenApplied] = 0x03;
    bytes[zelda3::OverworldCustomTileGFXGroupEnabled] = 0xFF;
    for (int i = 0; i < zelda3::kNumOverworldAreas; ++i) {
      bytes[zelda3::kOverworldMapParentIdExpanded + i] =
          static_cast<uint8_t>(i);
      bytes[zelda3::kOverworldScreenSize + i] = 0;
    }
  }
  EXPECT_TRUE(loaded.rom.LoadFromData(bytes).ok());
  EXPECT_TRUE(zelda3::LoadGfxGroups(loaded.rom, loaded.data).ok());
  return loaded;
}

bool HasId(const std::vector<int>& ids, int id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

TEST(GfxGroupPersistenceTest, FreshlyLoadedGroupsHaveNoDiff) {
  auto loaded = LoadFixtureGroups();
  auto diff = zelda3::DiffGfxGroups(loaded.rom, loaded.data);
  ASSERT_TRUE(diff.ok()) << diff.status();
  EXPECT_FALSE(diff->any());
}

TEST(GfxGroupPersistenceTest, SaveWritesOnlyChangedBytesAndReadsBack) {
  auto loaded = LoadFixtureGroups();
  loaded.data.spriteset_ids[5][2] = 0x1D;
  loaded.data.room_blockset_ids[7][0] = 0x44;
  loaded.data.paletteset_ids[2][1] = 0x10;

  auto diff = zelda3::DiffGfxGroups(loaded.rom, loaded.data);
  ASSERT_TRUE(diff.ok());
  EXPECT_TRUE(diff->spritesets);
  EXPECT_TRUE(diff->room_blocksets);
  EXPECT_TRUE(diff->palettesets);
  EXPECT_FALSE(diff->main_blocksets);
  EXPECT_EQ(diff->changed_bytes, 3);

  const auto before = loaded.rom.vector();
  ASSERT_TRUE(zelda3::SaveGfxGroups(loaded.rom, loaded.data).ok());
  EXPECT_EQ(loaded.rom.vector()[kSpritesetTable + 5 * 4 + 2], 0x1D);
  EXPECT_EQ(loaded.rom.vector()[zelda3::kEntranceGfxGroup + 7 * 4], 0x44);

  int changed = 0;
  for (size_t i = 0; i < before.size(); ++i) {
    changed += before[i] != loaded.rom.vector()[i] ? 1 : 0;
  }
  EXPECT_EQ(changed, 3);
  auto after = zelda3::DiffGfxGroups(loaded.rom, loaded.data);
  ASSERT_TRUE(after.ok());
  EXPECT_FALSE(after->any());
}

TEST(GfxGroupPersistenceTest, UnknownVersionIsRefused) {
  auto loaded = LoadFixtureGroups();
  loaded.data.version = zelda3_version::SD;
  EXPECT_EQ(zelda3::DiffGfxGroups(loaded.rom, loaded.data).status().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(zelda3::SaveGfxGroups(loaded.rom, loaded.data).ok());
}

TEST(GfxGroupPersistenceTest, FindsSpritesetAndRoomsetUsage) {
  std::map<int, zelda3::OverworldAreaGfxInfo> areas;
  areas[0x00].sprite_graphics = {0x09, 0x09, 0x0C};
  areas[0x00].area_graphics = 0x20;
  areas[0x2D].sprite_graphics = {0x0C, 0x09, 0x09};
  areas[0x40].sprite_graphics = {0x10, 0x10, 0x10};
  areas[0x40].area_graphics = 0x20;
  std::map<int, zelda3::RoomGfxInfo> rooms = {
      {0x12, {0x20, 0x09}}, {0x13, {0x05, 0x0C}}, {0x14, {0x20, 0x09}}};

  const auto set09 = zelda3::FindSpritesetUsage(0x09, areas, rooms);
  EXPECT_EQ(set09.ow_areas_by_state[0], (std::vector<int>{0x00}));
  EXPECT_EQ(set09.ow_areas_by_state[1], (std::vector<int>{0x00, 0x2D}));
  EXPECT_EQ(set09.ow_areas_by_state[2], (std::vector<int>{0x2D}));
  EXPECT_TRUE(set09.rooms.empty());  // rooms use sets 0x40+

  const auto set49 = zelda3::FindSpritesetUsage(0x49, areas, rooms);
  EXPECT_EQ(set49.rooms, (std::vector<int>{0x12, 0x14}));

  const auto roomset = zelda3::FindRoomsetUsage(0x20, areas, rooms);
  EXPECT_EQ(roomset.ow_areas, (std::vector<int>{0x00, 0x40}));
  EXPECT_EQ(roomset.rooms, (std::vector<int>{0x12, 0x14}));
}

TEST(GfxGroupPersistenceTest,
     RoomsetReportCountsSharedZeroEntriesAndMissingRooms) {
  auto loaded = LoadRoomsetUsageGroups();
  loaded.rom[zelda3::kAreaGfxIdPtr + 1] = 7;
  loaded.data.room_blockset_ids[7] = {0, 0, 0, 0};
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {7, 0}}, {4, {3, 0}}};

  auto report = zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, rooms);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_EQ(report->rooms, (std::vector<int>{2}));
  EXPECT_EQ(report->ow_areas, (std::vector<int>{1}));
  EXPECT_TRUE(report->overridden_ow_areas.empty());
  EXPECT_TRUE(report->independent_ow_areas.empty());
  EXPECT_EQ(report->unscanned_rooms, 294);
  EXPECT_EQ(report->unscanned_ow_areas, 0);
  EXPECT_FALSE(report->uses_live_overworld);
}

TEST(GfxGroupPersistenceTest, RoomsetReportSeparatesCustomWorldDefaultAreas) {
  auto loaded = LoadRoomsetUsageGroups(/*custom=*/true);
  loaded.rom[zelda3::kOverworldMapParentIdExpanded + 0x41] = 0x40;
  loaded.rom[zelda3::kAreaGfxIdPtr + 0x40] = 7;
  for (int slot = 0; slot < 8; ++slot) {
    loaded.rom[zelda3::OverworldCustomTileGFXGroupArray + 0x40 * 8 + slot] =
        0xFF;
  }
  loaded.data.room_blockset_ids[7][0] = 0x42;
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {7, 0}}};

  auto report = zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, rooms);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_TRUE(report->ow_areas.empty());
  EXPECT_EQ(report->overridden_ow_areas, (std::vector<int>{0x40, 0x41}));
  EXPECT_TRUE(report->independent_ow_areas.empty());
  EXPECT_EQ(report->rooms, (std::vector<int>{2}));
}

TEST(GfxGroupPersistenceTest, RoomsetReportSeparatesJapaneseRomAreaTable) {
  auto loaded = LoadRoomsetUsageGroups(/*custom=*/false, zelda3_version::JP);
  loaded.rom[zelda3::kAreaGfxIdPtr + 1] = 7;
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {7, 0}}};

  auto report = zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, rooms);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_TRUE(report->ow_areas.empty());
  EXPECT_TRUE(report->overridden_ow_areas.empty());
  EXPECT_EQ(report->independent_ow_areas, (std::vector<int>{1}));
  EXPECT_EQ(report->rooms, (std::vector<int>{2}));
}

TEST(GfxGroupPersistenceTest, RoomsetReportFallsBackForUnloadedLiveModel) {
  auto loaded = LoadRoomsetUsageGroups();
  loaded.rom[zelda3::kAreaGfxIdPtr + 1] = 7;
  zelda3::Overworld lazy_overworld(&loaded.rom, &loaded.data);
  ASSERT_FALSE(lazy_overworld.is_loaded());
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {7, 0}}};

  auto report = zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, rooms,
                                             &lazy_overworld);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_FALSE(report->uses_live_overworld);
  EXPECT_EQ(report->ow_areas, (std::vector<int>{1}));
  EXPECT_EQ(report->unscanned_ow_areas, 0);
}

TEST(GfxGroupPersistenceTest, RoomsetReportRefusesUnavailableGroupState) {
  auto loaded = LoadRoomsetUsageGroups();
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {7, 0}}};
  loaded.data.Clear();
  EXPECT_EQ(zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, rooms)
                .status()
                .code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 82, rooms)
                .status()
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(GfxGroupPersistenceTest, RoomsetReportRefusesAnotherRomSession) {
  auto loaded = LoadRoomsetUsageGroups();
  auto other = LoadRoomsetUsageGroups();
  zelda3::Overworld foreign(&other.rom, &other.data);
  EXPECT_EQ(
      zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, {}, &foreign)
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
}

TEST(GfxGroupPersistenceTest,
     RoomsetReportMarksTruncatedExpandedMapsUnavailable) {
  auto loaded = LoadRoomsetUsageGroups(/*custom=*/true);
  auto bytes = loaded.rom.vector();
  bytes.resize(0x141000);  // The expanded message table is beyond this image.
  for (uint8_t version : {1, 2, 3}) {
    SCOPED_TRACE(version);
    bytes[zelda3::OverworldCustomASMHasBeenApplied] = version;
    ASSERT_TRUE(loaded.rom.LoadFromData(bytes).ok());
    auto report =
        zelda3::DescribeRoomsetUsage(loaded.rom, loaded.data, 7, {{2, {7, 0}}});
    ASSERT_TRUE(report.ok()) << report.status();
    EXPECT_EQ(report->rooms, (std::vector<int>{2}));
    EXPECT_EQ(report->unscanned_ow_areas, zelda3::kNumOverworldAreas);
    EXPECT_TRUE(report->ow_areas.empty());
    EXPECT_TRUE(report->overridden_ow_areas.empty());
  }
}

TEST(GfxGroupPersistenceTest, MainGroupResolverMatchesRoomPreview) {
  auto loaded = LoadRoomsetUsageGroups();
  loaded.data.room_default_entrances.resize(zelda3::kNumDungeonRooms);
  loaded.data.room_default_entrances[2] = {
      1, 7, zelda3::RoomEntranceSource::kDirect, -1};
  loaded.data.room_default_entrances[3] = {
      1, 7, zelda3::RoomEntranceSource::kNeighbour, 2};
  loaded.data.room_default_entrances[4] = {
      1, 7, zelda3::RoomEntranceSource::kDungeonMap, -1};
  loaded.data.room_blockset_ids[9] = {0x44, 0, 0, 0};
  loaded.data.main_blockset_ids[7][0] = 0x31;
  loaded.data.main_blockset_ids[8][0] = 0x32;

  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {9, 0}}, {3, {9, 0}},
                                                    {4, {9, 0}}, {5, {7, 0}},
                                                    {6, {9, 0}}, {7, {9, 0}}};
  const std::map<int, uint8_t> explicit_main = {{6, 7}, {7, 0xFE}};
  auto report = zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 7, 0,
                                               rooms, explicit_main);
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_EQ(report->direct_default_rooms, (std::vector<int>{2}));
  EXPECT_EQ(report->inferred_default_rooms, (std::vector<int>{3, 4}));
  EXPECT_EQ(report->fallback_rooms, (std::vector<int>{5}));
  EXPECT_EQ(report->explicit_preview_rooms, (std::vector<int>{6}));
  EXPECT_EQ(report->unscanned_rooms, zelda3::kNumDungeonRooms - 6);

  zelda3::Room room(2, &loaded.rom, &loaded.data);
  room.SetBlockset(9);
  room.LoadRoomGraphics();
  EXPECT_EQ(room.resolved_main_blockset(), 7);
  EXPECT_EQ(room.blocks()[0], 0x31);
  room.SetRenderEntranceBlockset(8);
  room.LoadRoomGraphics();
  EXPECT_EQ(room.resolved_main_blockset(), 8);
  EXPECT_EQ(room.blocks()[0], 0x32);
  const auto resolved = zelda3::ResolveRoomMainBlockset(2, 9, 8, loaded.data);
  EXPECT_EQ(resolved.main_blockset, room.resolved_main_blockset());
  EXPECT_EQ(resolved.source, zelda3::RoomMainBlocksetSource::kExplicitPreview);
}

TEST(GfxGroupPersistenceTest, MainGroupReportSeparatesRoomSlotOverrides) {
  auto loaded = LoadRoomsetUsageGroups();
  loaded.data.room_default_entrances.resize(zelda3::kNumDungeonRooms);
  loaded.data.room_default_entrances[2] = {
      1, 7, zelda3::RoomEntranceSource::kDirect, -1};
  loaded.data.room_blockset_ids[9] = {0x44, 0, 0, 0};
  loaded.data.main_blockset_ids[7][3] = 0x33;
  loaded.data.main_blockset_ids[7][7] = 0x51;
  const std::map<int, zelda3::RoomGfxInfo> rooms = {{2, {9, 0}}};

  auto slot3 =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 7, 3, rooms);
  ASSERT_TRUE(slot3.ok()) << slot3.status();
  EXPECT_EQ(slot3->overridden_rooms, (std::vector<int>{2}));
  EXPECT_TRUE(slot3->direct_default_rooms.empty());
  auto slot7 =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 7, 7, rooms);
  ASSERT_TRUE(slot7.ok()) << slot7.status();
  EXPECT_EQ(slot7->direct_default_rooms, (std::vector<int>{2}));
  EXPECT_TRUE(slot7->overridden_rooms.empty());
  EXPECT_TRUE(slot7->slot7_bottom_half_only);

  zelda3::Room room(2, &loaded.rom, &loaded.data);
  room.SetBlockset(9);
  room.LoadRoomGraphics();
  EXPECT_EQ(room.blocks()[3], 0x44);
  EXPECT_EQ(room.blocks()[7], 0x51);
}

TEST(GfxGroupPersistenceTest, MainGroupReportUsesSharedAndIndependentTables) {
  auto loaded = LoadRoomsetUsageGroups();
  loaded.data.gfx_main_group_address = zelda3_us_pointers.kOverworldGfxGroups2;
  loaded.rom[zelda3::kAreaGfxIdPtr + 1] = 7;
  loaded.data.room_blockset_ids[7][0] = 0x42;
  auto shared =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 0x20, 3, {});
  ASSERT_TRUE(shared.ok()) << shared.status();
  EXPECT_TRUE(HasId(shared->ow_areas, 0));
  EXPECT_TRUE(HasId(shared->overridden_ow_areas, 1));
  EXPECT_EQ(shared->unscanned_ow_areas, 0);
  auto slot7 =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 0x20, 7, {});
  ASSERT_TRUE(slot7.ok()) << slot7.status();
  EXPECT_TRUE(HasId(slot7->ow_areas, 1));
  EXPECT_TRUE(slot7->slot7_bottom_half_only);

  loaded.data.version = zelda3_version::JP;
  auto independent =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 0x20, 0, {});
  ASSERT_TRUE(independent.ok()) << independent.status();
  EXPECT_TRUE(HasId(independent->independent_ow_areas, 0));
  EXPECT_FALSE(HasId(independent->ow_areas, 0));
}

TEST(GfxGroupPersistenceTest, MainGroupReportMarksCustomSlot7Override) {
  auto loaded = LoadRoomsetUsageGroups(/*custom=*/true);
  loaded.data.gfx_main_group_address = zelda3_us_pointers.kOverworldGfxGroups2;
  auto report =
      zelda3::DescribeMainGroupUsage(loaded.rom, loaded.data, 0x20, 7, {});
  ASSERT_TRUE(report.ok()) << report.status();
  EXPECT_TRUE(HasId(report->overridden_ow_areas, 0));
  EXPECT_FALSE(HasId(report->ow_areas, 0));
  EXPECT_TRUE(report->slot7_bottom_half_only);
}

}  // namespace
}  // namespace yaze::test
