#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {
namespace {

constexpr size_t kSheetBytes = GraphicsSheetStore::kSheetBytes;

// A loaded session: every sheet in the store, filled with `fill`.
std::unique_ptr<GameData> LoadedGameData(uint8_t fill) {
  auto data = std::make_unique<GameData>();
  data->graphics_buffer.assign(GraphicsSheetStore::kSheetCount * kSheetBytes,
                               fill);
  data->sheet_store.MarkAllSheetsChanged();
  return data;
}

std::vector<uint8_t> Sheet(uint8_t fill) {
  return std::vector<uint8_t>(kSheetBytes, fill);
}

// A room showing sheets `first` .. `first + 15`, copied once.
std::unique_ptr<Room> CopiedRoom(int room_id, Rom* rom, GameData* data,
                                 uint8_t first) {
  auto room = std::make_unique<Room>(room_id, rom, data);
  for (int block = 0; block < 16; ++block) {
    room->mutable_blocks()[block] = static_cast<uint8_t>(first + block);
  }
  room->CopyRoomGraphicsToBuffer();
  return room;
}

// The pointer at $6237 names the same US main-group table the overworld
// loader uses. Keep the source tables distinct from the edited session arrays.
std::vector<uint8_t> GroupRom(uint16_t main_pointer = 0xE073) {
  std::vector<uint8_t> bytes(0x200000, 0);
  bytes[kGfxGroupsPointer] = static_cast<uint8_t>(main_pointer);
  bytes[kGfxGroupsPointer + 1] = static_cast<uint8_t>(main_pointer >> 8);
  const auto& us = kVersionConstantsMap.at(zelda3_version::US);
  bytes[us.kOverworldGfxGroups2 + 0x20 * 8] = 0x30;
  bytes[us.kOverworldGfxGroups2 + 0x20 * 8 + 3] = 0x33;
  bytes[us.kOverworldGfxGroups1 + 7 * 4] = 0x43;
  bytes[us.kSpriteBlocksetPointer + 5 * 4] = 0x10;
  bytes[us.kSpriteBlocksetPointer + 6 * 4] = 0x11;
  return bytes;
}

void LoadGroups(Rom& rom, GameData& data) {
  data.version = zelda3_version::US;
  ASSERT_TRUE(LoadGfxGroups(rom, data).ok());
  ASSERT_TRUE(data.gfx_groups_loaded);
}

TEST(GraphicsSheetRevisionRefreshTest, StoreEditDirtiesOnlyRoomsUsingTheSheet) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = LoadedGameData(1);
  auto room_a = CopiedRoom(0, &rom, data.get(), /*first=*/0);     // 0x00-0x0F
  auto room_b = CopiedRoom(1, &rom, data.get(), /*first=*/0x20);  // 0x20-0x2F
  EXPECT_FALSE(room_a->SourceSheetsChanged());
  EXPECT_FALSE(room_b->SourceSheetsChanged());

  // Unchanged pixels change no revision, so nothing rebuilds.
  ASSERT_TRUE(data->sheet_store.WriteSheet(0x05, Sheet(1)).ok());
  EXPECT_FALSE(room_a->SourceSheetsChanged());

  ASSERT_TRUE(data->sheet_store.WriteSheet(0x05, Sheet(4)).ok());
  EXPECT_TRUE(room_a->SourceSheetsChanged());
  EXPECT_FALSE(room_b->SourceSheetsChanged());

  // The rebuild copies the edit and records the new revision.
  room_a->CopyRoomGraphicsToBuffer();
  EXPECT_FALSE(room_a->SourceSheetsChanged());
  EXPECT_EQ(room_a->get_gfx_buffer()[5 * kSheetBytes + 9], 4);
}

TEST(GraphicsSheetRevisionRefreshTest, OverridesWinOverStoreEdits) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = LoadedGameData(1);
  auto room = std::make_unique<Room>(0, &rom, data.get());
  for (int block = 0; block < 16; ++block) {
    room->mutable_blocks()[block] = static_cast<uint8_t>(block);
  }
  room->SetGraphicsSheetOverrides({{0x05, Sheet(6)}});
  room->CopyRoomGraphicsToBuffer();

  // An edit to the overridden sheet is not what the room shows.
  ASSERT_TRUE(data->sheet_store.WriteSheet(0x05, Sheet(3)).ok());
  EXPECT_FALSE(room->SourceSheetsChanged());
  // Another of its sheets still counts.
  ASSERT_TRUE(data->sheet_store.WriteSheet(0x06, Sheet(3)).ok());
  EXPECT_TRUE(room->SourceSheetsChanged());
}

TEST(GraphicsSheetRevisionRefreshTest, OverworldMapNoticesItsSheetEdits) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = LoadedGameData(1);
  OverworldMap map_a(0, &rom, data.get());
  OverworldMap map_b(1, &rom, data.get());
  for (int slot = 0; slot < 16; ++slot) {
    *map_a.mutable_static_graphics(slot) = static_cast<uint8_t>(0x30 + slot);
    *map_b.mutable_static_graphics(slot) = static_cast<uint8_t>(0x50 + slot);
  }
  ASSERT_TRUE(map_a.BuildTileset().ok());
  ASSERT_TRUE(map_b.BuildTileset().ok());
  EXPECT_FALSE(map_a.SourceSheetsChanged());

  ASSERT_TRUE(data->sheet_store.WriteSheet(0x33, Sheet(5)).ok());
  EXPECT_TRUE(map_a.SourceSheetsChanged());
  EXPECT_FALSE(map_b.SourceSheetsChanged());

  ASSERT_TRUE(map_a.BuildTileset().ok());
  EXPECT_FALSE(map_a.SourceSheetsChanged());

  // A map never built from the store has nothing to compare.
  OverworldMap unbuilt(2, &rom, data.get());
  *unbuilt.mutable_static_graphics(0) = 0x33;
  ASSERT_TRUE(data->sheet_store.WriteSheet(0x33, Sheet(6)).ok());
  EXPECT_FALSE(unbuilt.SourceSheetsChanged());
}

// A map filled from the Overworld's tileset cache also notices later edits.
TEST(GraphicsSheetRevisionRefreshTest, CachedTilesetRecordsRevisions) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  auto data = LoadedGameData(1);
  OverworldMap built(0, &rom, data.get());
  OverworldMap cached(1, &rom, data.get());
  for (int slot = 0; slot < 16; ++slot) {
    *built.mutable_static_graphics(slot) = static_cast<uint8_t>(0x30 + slot);
    *cached.mutable_static_graphics(slot) = static_cast<uint8_t>(0x30 + slot);
  }
  ASSERT_TRUE(built.BuildTileset().ok());
  cached.UseCachedTileset(built.current_graphics());
  EXPECT_FALSE(cached.SourceSheetsChanged());

  ASSERT_TRUE(data->sheet_store.WriteSheet(0x3A, Sheet(2)).ok());
  EXPECT_TRUE(cached.SourceSheetsChanged());
}

TEST(GraphicsSheetRevisionRefreshTest,
     UnsavedMainAndAreaAssignmentsDirtyBuiltMapAndZeroInheritsMain) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(GroupRom()).ok());
  auto data = LoadedGameData(1);
  LoadGroups(rom, *data);
  OverworldMap map(0, &rom, data.get());
  map.set_area_graphics(7);
  map.LoadAreaGraphics();
  ASSERT_EQ(map.static_graphics(0), 0x30);
  ASSERT_EQ(map.static_graphics(3), 0x43);
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_FALSE(map.SourceSheetsChanged());

  data->main_blockset_ids[0x20][0] = 0x31;
  data->room_blockset_ids[7][0] = 0x44;
  EXPECT_TRUE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  ASSERT_TRUE(map.BuildTileset().ok());
  EXPECT_EQ(map.static_graphics(0), 0x31);
  EXPECT_EQ(map.static_graphics(3), 0x44);
  EXPECT_FALSE(map.SourceSheetsChanged());

  // Zero in an area group inherits the current, unsaved main-group slot.
  data->main_blockset_ids[0x20][3] = 0x34;
  data->room_blockset_ids[7][0] = 0;
  EXPECT_TRUE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(3), 0x34);
}

TEST(GraphicsSheetRevisionRefreshTest,
     SpriteAssignmentUsesCurrentGameStateAndDirtiesBuiltMap) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(GroupRom()).ok());
  auto data = LoadedGameData(1);
  LoadGroups(rom, *data);
  OverworldMap map(0, &rom, data.get());
  map.set_sprite_graphics(0, 5);
  map.set_sprite_graphics(2, 6);
  map.set_game_state(0);
  map.LoadAreaGraphics();
  ASSERT_EQ(map.static_graphics(12), 0x83);
  ASSERT_TRUE(map.BuildTileset().ok());

  data->spriteset_ids[6][0] = 0x13;
  EXPECT_FALSE(map.SourceSheetsChanged());
  map.set_game_state(2);
  EXPECT_TRUE(map.SourceSheetsChanged());
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(12), 0x86);
  EXPECT_EQ(map.static_graphics(8), 0x73);  // Fixed sprite slots stay fixed.
}

TEST(GraphicsSheetRevisionRefreshTest,
     UnreadyAndClearedGroupsFallBackToRomAndFailedLoadStaysUnready) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(GroupRom()).ok());
  GameData data;
  data.version = zelda3_version::US;
  data.main_blockset_ids[0x20][0] = 0x51;
  data.room_blockset_ids[7][0] = 0x52;
  data.spriteset_ids[5][0] = 0x20;
  OverworldMap map(0, &rom, &data);
  map.set_area_graphics(7);
  map.set_sprite_graphics(0, 5);
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(0), 0x30);
  EXPECT_EQ(map.static_graphics(3), 0x43);
  EXPECT_EQ(map.static_graphics(12), 0x83);

  LoadGroups(rom, data);
  EXPECT_EQ(data.gfx_main_group_address,
            kVersionConstantsMap.at(zelda3_version::US).kOverworldGfxGroups2);
  data.main_blockset_ids[0x20][0] = 0x51;
  data.room_blockset_ids[7][0] = 0x52;
  data.spriteset_ids[5][0] = 0x20;
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(0), 0x51);
  EXPECT_EQ(map.static_graphics(3), 0x52);
  EXPECT_EQ(map.static_graphics(12), 0x93);

  Rom truncated;
  ASSERT_TRUE(truncated.LoadFromData(std::vector<uint8_t>(0x2000, 0)).ok());
  EXPECT_FALSE(LoadGfxGroups(truncated, data).ok());
  EXPECT_FALSE(data.gfx_groups_loaded);
  EXPECT_EQ(data.gfx_main_group_address, 0u);

  LoadGroups(rom, data);
  data.Clear();
  EXPECT_FALSE(data.gfx_groups_loaded);
  EXPECT_EQ(data.gfx_main_group_address, 0u);
  map.LoadAreaGraphics();
  EXPECT_EQ(map.static_graphics(0), 0x30);
  EXPECT_EQ(map.static_graphics(3), 0x43);
  EXPECT_EQ(map.static_graphics(12), 0x83);
}

TEST(GraphicsSheetRevisionRefreshTest,
     IncompatibleMainPointerAndJapaneseAreaTableStayRomBacked) {
  auto relocated_bytes = GroupRom(/*main_pointer=*/0xE273);
  relocated_bytes[0x6273 + 0x20 * 8] = 0x51;
  Rom relocated;
  ASSERT_TRUE(relocated.LoadFromData(relocated_bytes).ok());
  GameData relocated_data;
  LoadGroups(relocated, relocated_data);
  ASSERT_EQ(relocated_data.gfx_main_group_address, 0x6273u);
  OverworldMap relocated_map(0, &relocated, &relocated_data);
  relocated_map.LoadAreaGraphics();
  EXPECT_EQ(relocated_map.static_graphics(0), 0x30);
  relocated_data.main_blockset_ids[0x20][0] = 0x52;
  relocated_map.LoadAreaGraphics();
  EXPECT_EQ(relocated_map.static_graphics(0), 0x30);

  auto jp_bytes = GroupRom(/*main_pointer=*/0xE0B3);
  const auto& jp = kVersionConstantsMap.at(zelda3_version::JP);
  jp_bytes[jp.kOverworldGfxGroups1 + 7 * 4] = 0x45;
  Rom jp_rom;
  ASSERT_TRUE(jp_rom.LoadFromData(jp_bytes).ok());
  GameData jp_data;
  jp_data.version = zelda3_version::JP;
  ASSERT_TRUE(LoadGfxGroups(jp_rom, jp_data).ok());
  OverworldMap jp_map(0, &jp_rom, &jp_data);
  jp_map.set_area_graphics(7);
  jp_map.LoadAreaGraphics();
  EXPECT_EQ(jp_map.static_graphics(3), 0x45);
  jp_data.room_blockset_ids[7][0] = 0x46;
  jp_map.LoadAreaGraphics();
  EXPECT_EQ(jp_map.static_graphics(3), 0x45);
}

}  // namespace
}  // namespace yaze::zelda3
