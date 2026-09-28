#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <vector>

#include "rom/rom.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"

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

}  // namespace
}  // namespace yaze::test
