#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "rom/rom.h"
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

}  // namespace
}  // namespace yaze::zelda3
