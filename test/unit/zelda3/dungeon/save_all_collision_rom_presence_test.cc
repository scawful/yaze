#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {

namespace {

std::unique_ptr<Rom> MakeDummyRom(size_t size_bytes) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy(size_bytes, 0);
  auto status = rom->LoadFromData(dummy);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

}  // namespace

TEST(SaveAllCollisionRomPresenceTest,
     NoOpWhenRegionMissingAndNoDirtyCollision) {
  auto rom = MakeDummyRom(0x100000);  // Smaller than custom collision pointers

  std::vector<Room> rooms;
  rooms.reserve(1);
  rooms.emplace_back(/*room_id=*/0, rom.get());

  auto status = SaveAllCollision(rom.get(), absl::MakeSpan(rooms));
  EXPECT_TRUE(status.ok()) << status;
}

TEST(SaveAllCollisionRomPresenceTest,
     RejectsWhenRegionMissingAndDirtyCollision) {
  auto rom = MakeDummyRom(0x100000);  // Smaller than custom collision pointers

  std::vector<Room> rooms;
  rooms.reserve(1);
  rooms.emplace_back(/*room_id=*/0, rom.get());

  // Simulate user-authored collision edits.
  rooms[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  ASSERT_TRUE(rooms[0].custom_collision_dirty());

  auto status = SaveAllCollision(rom.get(), absl::MakeSpan(rooms));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(SaveAllCollisionRomPresenceTest,
     RejectsWhenPointerTablePresentButDataRegionMissingAndDirtyCollision) {
  // Big enough for the pointer table, too small for the collision data region.
  const size_t ptr_table_end =
      static_cast<size_t>(kCustomCollisionRoomPointers + (kNumberOfRooms * 3));
  auto rom = MakeDummyRom(ptr_table_end);

  std::vector<Room> rooms;
  rooms.reserve(1);
  rooms.emplace_back(/*room_id=*/0, rom.get());

  rooms[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  ASSERT_TRUE(rooms[0].custom_collision_dirty());

  auto status = SaveAllCollision(rom.get(), absl::MakeSpan(rooms));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST(SaveAllCollisionRomPresenceTest,
     FloorOnlyMapSurvivesSaveAndReopenWithoutPopulatingAbsentCells) {
  auto rom = MakeDummyRom(0x200000);
  ASSERT_TRUE(rom->WriteByte(kWaterFillTableStart, 0xA5).ok());
  const auto before = rom->vector();
  std::vector<Room> rooms;
  rooms.emplace_back(0x83, rom.get());
  rooms[0].SetCollisionTile(46, 7, 0x00);
  ASSERT_TRUE(rooms[0].HasCollisionOverride(46, 7));
  ASSERT_TRUE(rooms[0].custom_collision_dirty());
  ASSERT_TRUE(SaveAllCollision(rom.get(), absl::MakeSpan(rooms)).ok());
  EXPECT_FALSE(rooms[0].custom_collision_dirty());

  Rom reopened;
  ASSERT_TRUE(reopened.LoadFromData(rom->vector()).ok());
  const auto loaded = LoadCustomCollisionMap(&reopened, 0x83);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_TRUE(loaded->has_data);
  EXPECT_TRUE(loaded->HasOverride(494));
  EXPECT_EQ(loaded->tiles[494], 0x00);
  EXPECT_FALSE(loaded->HasOverride(493));
  EXPECT_EQ(loaded->overrides.count(), 1u);

  const size_t pointer = kCustomCollisionRoomPointers + 0x83 * 3;
  auto expected = before;
  std::copy_n(reopened.vector().begin() + pointer, 3,
              expected.begin() + pointer);
  const std::vector<uint8_t> blob{0xF0, 0xF0, 0xEE, 0x01, 0x00, 0xFF, 0xFF};
  std::copy(blob.begin(), blob.end(),
            expected.begin() + kCustomCollisionDataPosition);
  EXPECT_EQ(reopened.vector(), expected);
#if defined(YAZE_WITH_JSON)
  const auto source = DumpCustomCollisionSourceFromRom(&reopened);
  ASSERT_TRUE(source.ok()) << source.status();
  const auto entries = LoadCustomCollisionRoomsFromJsonString(*source);
  ASSERT_TRUE(entries.ok()) << entries.status();
  ASSERT_EQ(entries->size(), 1u);
  EXPECT_EQ((*entries)[0].room_id, 0x83);
  ASSERT_EQ((*entries)[0].tiles.size(), 1u);
  EXPECT_EQ((*entries)[0].tiles[0].offset, 494);
  EXPECT_EQ((*entries)[0].tiles[0].value, 0x00);
#endif
}

TEST(SaveAllCollisionRomPresenceTest,
     ClearingLastFloorOverrideDisablesOnlyThatRoom) {
  auto rom = MakeDummyRom(0x200000);
  std::vector<Room> rooms;
  rooms.emplace_back(0x83, rom.get());
  rooms[0].SetCollisionTile(46, 7, 0x00);
  ASSERT_TRUE(SaveAllCollision(rom.get(), absl::MakeSpan(rooms)).ok());
  const auto before_clear = rom->vector();
  rooms[0].ClearCollisionTile(46, 7);
  EXPECT_FALSE(rooms[0].HasCollisionOverride(46, 7));
  ASSERT_TRUE(rooms[0].custom_collision_dirty());
  ASSERT_TRUE(SaveAllCollision(rom.get(), absl::MakeSpan(rooms)).ok());
  EXPECT_FALSE(rooms[0].custom_collision_dirty());

  const auto loaded = LoadCustomCollisionMap(rom.get(), 0x83);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_FALSE(loaded->has_data);
  EXPECT_FALSE(loaded->HasOverrides());
  const size_t pointer = kCustomCollisionRoomPointers + 0x83 * 3;
  auto expected = before_clear;
  std::fill_n(expected.begin() + pointer, 3, 0);
  EXPECT_EQ(rom->vector(), expected);
}

}  // namespace yaze::zelda3
