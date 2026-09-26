#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <vector>

#include "rom/rom.h"
#include "rom/snes.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {

namespace {

constexpr int kSheetBytes = 4096;

}  // namespace

// A preview hands the room edited sheet pixels without touching the ROM or
// GameData. The override must replace exactly that sheet, keep the right
// palette expansion, and leave the shared buffer unchanged.
TEST(RoomGraphicsSheetOverrideTest, OverrideReplacesOnlyThatSheet) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData game_data;
  game_data.graphics_buffer.assign(16 * kSheetBytes, 1);

  Room room(/*room_id=*/0, &rom, &game_data);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }

  std::vector<uint8_t> edited(kSheetBytes, 1);
  edited[5] = 6;
  std::vector<uint8_t> edited_right(kSheetBytes, 1);
  edited_right[5] = 6;
  room.SetGraphicsSheetOverrides({{4, edited}, {3, edited_right}});
  room.CopyRoomGraphicsToBuffer();

  const auto& gfx = room.get_gfx_buffer();
  // Block 4 is a left-palette UW slot: pixel copied as-is.
  EXPECT_EQ(gfx[4 * kSheetBytes + 5], 6);
  EXPECT_EQ(gfx[4 * kSheetBytes + 4], 1);
  // Block 3 is a right-palette UW slot: override still gets the +8 shift.
  EXPECT_EQ(gfx[3 * kSheetBytes + 5], 14);
  // Sheets without an override come from GameData.
  EXPECT_EQ(gfx[5 * kSheetBytes + 5], 1);
  // The shared buffer is untouched.
  EXPECT_EQ(game_data.graphics_buffer[4 * kSheetBytes + 5], 1);
}

TEST(RoomGraphicsSheetOverrideTest, WrongSizedOverrideFallsBackToGameData) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData game_data;
  game_data.graphics_buffer.assign(16 * kSheetBytes, 2);

  Room room(0, &rom, &game_data);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }
  room.SetGraphicsSheetOverrides({{4, std::vector<uint8_t>(100, 7)}});
  room.CopyRoomGraphicsToBuffer();

  EXPECT_EQ(room.get_gfx_buffer()[4 * kSheetBytes], 2);
}

// An override can cover a sheet the GameData buffer does not (a buffer that
// was trimmed or failed to load past some sheet).
TEST(RoomGraphicsSheetOverrideTest, OverrideCoversSheetOutsideBuffer) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData game_data;
  game_data.graphics_buffer.assign(8 * kSheetBytes, 0);

  Room room(0, &rom, &game_data);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }
  room.SetGraphicsSheetOverrides({{9, std::vector<uint8_t>(kSheetBytes, 3)}});
  room.CopyRoomGraphicsToBuffer();

  EXPECT_EQ(room.get_gfx_buffer()[9 * kSheetBytes + 10], 3);
}

TEST(RoomGraphicsSheetOverrideTest, SettingOverridesMarksGraphicsDirty) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  GameData game_data;
  Room room(0, &rom, &game_data);
  const uint64_t before = room.composite_source_revision();
  room.SetGraphicsSheetOverrides({{1, std::vector<uint8_t>(kSheetBytes, 1)}});
  EXPECT_NE(room.composite_source_revision(), before);
  EXPECT_EQ(room.graphics_sheet_overrides().size(), 1u);
}

}  // namespace yaze::zelda3::test
