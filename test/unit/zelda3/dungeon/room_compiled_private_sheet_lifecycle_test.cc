#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3 {

// Friend of Room: installs stale compiled overrides the way a prior
// LoadRoomGraphics used to leave them in graphics_sheet_overrides_.
class RoomCompiledPrivateSheetLifecycleTest : public ::testing::Test {
 protected:
  static constexpr int kSheetBytes = 4096;

  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    game_data_.graphics_buffer.assign(256 * kSheetBytes, 2);
    game_data_.gfx_sheet_table.count = 255;
    for (int i = 0; i < 8; ++i) {
      game_data_.main_blockset_ids[0][i] = static_cast<uint8_t>(i);
    }
  }

  static void InstallCompiledOverrides(
      Room& room, std::map<uint16_t, std::vector<uint8_t>> overrides) {
    room.compiled_graphics_sheet_overrides_ = std::move(overrides);
  }

  static const uint8_t* SheetSource(const Room& room, int sheet_id) {
    return room.GraphicsSheetSource(sheet_id);
  }

  Rom rom_;
  GameData game_data_;
};

TEST_F(RoomCompiledPrivateSheetLifecycleTest,
       SameRoomActiveToInactiveClearsCompiledOverrides) {
  Room room(/*room_id=*/0x54, &rom_, &game_data_);
  for (int block = 0; block < 16; ++block) {
    room.mutable_blocks()[block] = static_cast<uint8_t>(block);
  }

  InstallCompiledOverrides(room,
                           {{0x08, std::vector<uint8_t>(kSheetBytes, 9)}});
  ASSERT_NE(SheetSource(room, 0x08), nullptr);
  EXPECT_EQ(SheetSource(room, 0x08)[0], 9);
  EXPECT_EQ(room.compiled_graphics_sheet_overrides().size(), 1u);
  EXPECT_TRUE(room.graphics_sheet_overrides().empty());

  // No WSP1 / inactive profile: LoadRoomGraphics must clear compiled map so
  // the next copy uses ordinary GameData sheet 0x08 (= fill value 2).
  room.LoadRoomGraphics();
  EXPECT_TRUE(room.compiled_graphics_sheet_overrides().empty());
  EXPECT_TRUE(room.graphics_sheet_overrides().empty());
  ASSERT_NE(SheetSource(room, 0x08), nullptr);
  EXPECT_EQ(SheetSource(room, 0x08)[0], 2);
}

TEST_F(RoomCompiledPrivateSheetLifecycleTest,
       SameRoomChangedStreamRebuildsCompiledOverrides) {
  Room room(/*room_id=*/0x54, &rom_, &game_data_);
  InstallCompiledOverrides(room,
                           {{0x08, std::vector<uint8_t>(kSheetBytes, 9)},
                            {0xA9, std::vector<uint8_t>(kSheetBytes, 9)}});
  EXPECT_EQ(SheetSource(room, 0x08)[0], 9);

  // Simulate a later load that would apply a different private stream: clear
  // via LoadRoomGraphics, then install the changed pixels. Caller overrides
  // remain untouched and still win.
  room.SetGraphicsSheetOverrides(
      {{0x08, std::vector<uint8_t>(kSheetBytes, 7)}});
  room.LoadRoomGraphics();
  EXPECT_TRUE(room.compiled_graphics_sheet_overrides().empty());
  InstallCompiledOverrides(room,
                           {{0x08, std::vector<uint8_t>(kSheetBytes, 5)},
                            {0xA9, std::vector<uint8_t>(kSheetBytes, 5)}});
  EXPECT_EQ(SheetSource(room, 0x08)[0], 7);  // caller wins
  EXPECT_EQ(SheetSource(room, 0xA9)[0], 5);  // compiled changed stream
  EXPECT_EQ(room.graphics_sheet_overrides().size(), 1u);
  EXPECT_EQ(room.compiled_graphics_sheet_overrides().size(), 2u);
}

TEST_F(RoomCompiledPrivateSheetLifecycleTest,
       Room64Slot6CompiledOverridesClearOnInactiveReload) {
  Room room(/*room_id=*/0x64, &rom_, &game_data_);
  InstallCompiledOverrides(room,
                           {{0x08, std::vector<uint8_t>(kSheetBytes, 4)},
                            {0x6F, std::vector<uint8_t>(kSheetBytes, 6)}});
  EXPECT_EQ(room.compiled_graphics_sheet_overrides().count(0x6F), 1u);
  EXPECT_EQ(SheetSource(room, 0x6F)[0], 6);

  // No WSP1 / inactive profile: long-lived room64 must not keep stale slot6.
  room.LoadRoomGraphics();
  EXPECT_TRUE(room.compiled_graphics_sheet_overrides().empty());
  ASSERT_NE(SheetSource(room, 0x6F), nullptr);
  EXPECT_EQ(SheetSource(room, 0x6F)[0], 2);
}

}  // namespace yaze::zelda3
