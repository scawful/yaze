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

TEST(CustomCollisionReservedRegionTest, LoadRejectsWhenPointerOverlapsReserved) {
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
    EXPECT_EQ(loaded_or.status().code(),
              absl::StatusCode::kFailedPrecondition);
  }
}

TEST(CustomCollisionReservedRegionTest,
     LoadRejectsWhenBlobUnterminatedBeforeReservedBoundary) {
  auto rom = MakeDummyRom(0x200000);

  // Create a minimal single-tile blob that ends exactly at the reserved
  // WaterFill boundary, without an end marker. This should be treated as
  // unterminated collision data (and rejected) when the reserved region exists.
  const uint32_t start_pc =
      static_cast<uint32_t>(kWaterFillTableStart - 5);
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
    EXPECT_EQ(loaded_or.status().code(),
              absl::StatusCode::kFailedPrecondition);
  }
}

}  // namespace zelda3
}  // namespace yaze

