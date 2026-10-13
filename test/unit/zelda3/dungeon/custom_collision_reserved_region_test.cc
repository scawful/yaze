#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze {
namespace zelda3 {

namespace {

std::unique_ptr<Rom> MakeDummyRom(size_t size_bytes) {
  auto rom = std::make_unique<Rom>();
  std::vector<uint8_t> dummy(size_bytes, 0);
  auto status = rom->LoadFromData(dummy);
  EXPECT_TRUE(status.ok()) << status.message();
  return rom;
}

}  // namespace

TEST(CustomCollisionReservedRegionTest,
     LoadRejectsWhenPointerOverlapsReserved) {
  auto rom = MakeDummyRom(0x200000);

  // Make room 0 custom collision pointer point into the reserved region.
  const uint32_t snes = PcToSnes(static_cast<uint32_t>(kWaterFillTableStart));
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  auto loaded_or = LoadCustomCollisionMap(rom.get(), /*room_id=*/0);
  EXPECT_FALSE(loaded_or.ok());
  if (!loaded_or.ok()) {
    EXPECT_EQ(loaded_or.status().code(), absl::StatusCode::kFailedPrecondition);
  }
}

TEST(CustomCollisionReservedRegionTest,
     LoadRejectsWhenBlobUnterminatedBeforeReservedBoundary) {
  auto rom = MakeDummyRom(0x200000);

  // Create a minimal single-tile blob that ends exactly at the reserved
  // WaterFill boundary, without an end marker. This should be treated as
  // unterminated collision data (and rejected) when the reserved region exists.
  const uint32_t start_pc = static_cast<uint32_t>(kWaterFillTableStart - 5);
  const uint32_t snes = PcToSnes(start_pc);
  const int ptr_offset = kCustomCollisionRoomPointers + 0;
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 0, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr_offset + 2, (snes >> 16) & 0xFF).ok());

  // Blob: F0 F0 (single tile mode), 00 00 (offset), 08 (tile). No terminator.
  std::vector<uint8_t> blob = {0xF0, 0xF0, 0x00, 0x00, 0x08};
  ASSERT_TRUE(rom->WriteVector(static_cast<int>(start_pc), blob).ok());

  auto loaded_or = LoadCustomCollisionMap(rom.get(), /*room_id=*/0);
  EXPECT_FALSE(loaded_or.ok());
  if (!loaded_or.ok()) {
    EXPECT_EQ(loaded_or.status().code(), absl::StatusCode::kFailedPrecondition);
  }
}

TEST(CustomCollisionMapTest, RectanglesAndSinglesRetainExplicitFloorPresence) {
  auto rom = MakeDummyRom(0x200000);
  const uint32_t pc = kCustomCollisionDataPosition;
  const uint32_t snes = PcToSnes(pc);
  const int ptr = kCustomCollisionRoomPointers + 0x83 * 3;
  ASSERT_TRUE(rom->WriteByte(ptr, snes & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr + 1, (snes >> 8) & 0xFF).ok());
  ASSERT_TRUE(rom->WriteByte(ptr + 2, snes >> 16).ok());
  // Rectangle at (46,7), 2x2, then a single floor at (43,22).
  ASSERT_TRUE(
      rom->WriteVector(pc, {0xEE, 0x01, 0x02, 0x02, 0x00, 0xFF, 0x08, 0x00,
                            0xF0, 0xF0, 0xAB, 0x05, 0x00, 0xFF, 0xFF})
          .ok());
  const auto map = LoadCustomCollisionMap(rom.get(), 0x83);
  ASSERT_TRUE(map.ok()) << map.status();
  EXPECT_TRUE(map->has_data);
  EXPECT_EQ(map->overrides.count(), 5u);
  for (size_t offset : {494u, 495u, 558u, 559u, 1451u}) {
    EXPECT_TRUE(map->HasOverride(offset)) << offset;
  }
  EXPECT_EQ(map->tiles[494], 0x00);
  EXPECT_EQ(map->tiles[495], 0xFF);
  EXPECT_EQ(map->tiles[558], 0x08);
  EXPECT_EQ(map->tiles[559], 0x00);
  EXPECT_EQ(map->tiles[1451], 0x00);
  EXPECT_FALSE(map->HasOverride(493));
  EXPECT_FALSE(map->HasOverride(1452));
}

TEST(CustomCollisionMapTest,
     EqualityDistinguishesFloorAndNormalizesLegacyNonzeroPresence) {
  CustomCollisionMap legacy;
  legacy.has_data = true;
  legacy.tiles[65] = 0x08;
  CustomCollisionMap explicit_map;
  explicit_map.SetOverride(65, 0x08);
  EXPECT_EQ(legacy, explicit_map);

  explicit_map.SetOverride(66, 0x00);
  EXPECT_NE(legacy, explicit_map);
  EXPECT_TRUE(explicit_map.HasOverride(66));
  EXPECT_EQ(explicit_map.tiles[66], legacy.tiles[66]);
  explicit_map.ClearOverride(66);
  EXPECT_EQ(legacy, explicit_map);
  EXPECT_FALSE(explicit_map.HasOverride(66));
  EXPECT_FALSE(explicit_map.HasOverride(explicit_map.tiles.size()));
}

}  // namespace zelda3
}  // namespace yaze
