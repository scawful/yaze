#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
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

TEST(SaveAllCollisionRomPresenceTest, NoOpWhenRegionMissingAndNoDirtyCollision) {
  auto rom = MakeDummyRom(0x100000);  // Smaller than custom collision pointers

  std::vector<Room> rooms;
  rooms.reserve(1);
  rooms.emplace_back(/*room_id=*/0, rom.get());
  rooms[0].SetLoaded(true);

  auto status = SaveAllCollision(rom.get(), absl::MakeSpan(rooms));
  EXPECT_TRUE(status.ok()) << status;
}

TEST(SaveAllCollisionRomPresenceTest, RejectsWhenRegionMissingAndDirtyCollision) {
  auto rom = MakeDummyRom(0x100000);  // Smaller than custom collision pointers

  std::vector<Room> rooms;
  rooms.reserve(1);
  rooms.emplace_back(/*room_id=*/0, rom.get());
  rooms[0].SetLoaded(true);

  // Simulate user-authored collision edits.
  rooms[0].SetCollisionTile(/*x=*/0, /*y=*/0, /*tile=*/0x08);
  ASSERT_TRUE(rooms[0].custom_collision_dirty());

  auto status = SaveAllCollision(rom.get(), absl::MakeSpan(rooms));
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

}  // namespace yaze::zelda3

