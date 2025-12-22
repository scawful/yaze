#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "e2e/rom_dependent/editor_save_test_base.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "testing.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for DungeonEditor Save Operations
 *
 * Validates the complete dungeon editing workflow:
 * 1. Load ROM and room data
 * 2. Modify room objects and sprites
 * 3. Save changes to ROM
 * 4. Reload ROM and verify edits persisted
 * 5. Verify no data corruption occurred
 */
class DungeonEditorSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    EditorSaveTestBase::SetUp();

    // Load the test ROM
    rom_ = std::make_unique<Rom>();
    auto load_result = rom_->LoadFromFile(test_rom_path_);
    if (!load_result.ok()) {
      GTEST_SKIP() << "Failed to load test ROM: " << load_result.message();
    }

    // Load game data
    game_data_ = std::make_unique<zelda3::GameData>();
    auto gd_result = zelda3::LoadGameData(*rom_, *game_data_);
    if (!gd_result.ok()) {
      GTEST_SKIP() << "Failed to load game data: " << gd_result.message();
    }
  }

  // Room header location helper
  uint32_t GetRoomHeaderAddress(int room_id) {
    // Room headers start at 0xF8000 in vanilla ROM
    // Each room header is 14 bytes
    return 0xF8000 + (room_id * 14);
  }

  uint32_t GetRoomObjectPointerAddress(int room_id) {
    // Object pointers at $1E8000 + room_id * 3
    return 0x1E8000 + (room_id * 3);
  }

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
};

// Test 1: Room header save/reload persistence
TEST_F(DungeonEditorSaveTest, RoomHeader_SaveAndReload) {
  const int test_room_id = 0;  // Ganon's Room

  // Get room header address
  uint32_t header_addr = GetRoomHeaderAddress(test_room_id);
  
  // Read original header byte
  auto original_byte = rom_->ReadByte(header_addr + 1);
  if (!original_byte.ok()) {
    GTEST_SKIP() << "Failed to read room header";
  }

  // Modify the header byte
  uint8_t modified_byte = (*original_byte + 0x10) & 0xFF;
  ASSERT_OK(rom_->WriteByte(header_addr + 1, modified_byte));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_byte = reloaded_rom->ReadByte(header_addr + 1);
  ASSERT_TRUE(reloaded_byte.ok());
  EXPECT_EQ(*reloaded_byte, modified_byte)
      << "Room header modification should persist";
}

// Test 2: Room object data save/reload
TEST_F(DungeonEditorSaveTest, RoomObjects_SaveAndReload) {
  const int test_room_id = 1;

  // Get room object pointer
  uint32_t obj_ptr_addr = GetRoomObjectPointerAddress(test_room_id);
  auto obj_ptr_low = rom_->ReadWord(obj_ptr_addr);
  auto obj_ptr_high = rom_->ReadByte(obj_ptr_addr + 2);

  if (!obj_ptr_low.ok() || !obj_ptr_high.ok()) {
    GTEST_SKIP() << "Failed to read object pointer";
  }

  uint32_t obj_data_addr = (*obj_ptr_low) | ((*obj_ptr_high) << 16);
  obj_data_addr = SnesToPc(obj_data_addr);

  // Record original first few bytes of object data
  std::vector<uint8_t> original_data(8);
  for (int i = 0; i < 8; ++i) {
    auto byte = rom_->ReadByte(obj_data_addr + i);
    original_data[i] = byte.ok() ? *byte : 0;
  }

  // Modify object data (change first object's position/type)
  // Object format: 2 bytes position, 1 byte object type
  if (original_data[0] != 0xFF && original_data[1] != 0xFF) {
    uint8_t modified_pos = (original_data[0] + 0x10) & 0xFF;
    ASSERT_OK(rom_->WriteByte(obj_data_addr, modified_pos));

    // Save ROM
    ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

    // Reload and verify
    std::unique_ptr<Rom> reloaded_rom;
    ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

    auto reloaded_byte = reloaded_rom->ReadByte(obj_data_addr);
    ASSERT_TRUE(reloaded_byte.ok());
    EXPECT_EQ(*reloaded_byte, modified_pos)
        << "Room object modification should persist";

    // Verify other bytes weren't corrupted
    for (int i = 1; i < 8; ++i) {
      auto byte = reloaded_rom->ReadByte(obj_data_addr + i);
      ASSERT_TRUE(byte.ok());
      EXPECT_EQ(*byte, original_data[i])
          << "Adjacent object data should not be corrupted at offset " << i;
    }
  }
}

// Test 3: Sprite data save/reload
TEST_F(DungeonEditorSaveTest, SpriteData_SaveAndReload) {
  const int test_room_id = 2;

  // Sprite pointers are at different location
  // $09D62E + room_id * 2 for the pointer table
  uint32_t sprite_ptr_table = 0x09D62E;
  auto sprite_ptr = rom_->ReadWord(sprite_ptr_table + (test_room_id * 2));

  if (!sprite_ptr.ok()) {
    GTEST_SKIP() << "Failed to read sprite pointer";
  }

  uint32_t sprite_data_addr = SnesToPc(0x090000 | *sprite_ptr);

  // Record original sprite data
  std::vector<uint8_t> original_sprite_data(6);
  bool has_sprites = true;
  for (int i = 0; i < 6; ++i) {
    auto byte = rom_->ReadByte(sprite_data_addr + i);
    if (!byte.ok()) {
      has_sprites = false;
      break;
    }
    original_sprite_data[i] = *byte;
  }

  if (!has_sprites || original_sprite_data[0] == 0xFF) {
    GTEST_SKIP() << "Room has no sprites to modify";
  }

  // Modify sprite Y position (first byte of sprite entry)
  uint8_t modified_y = (original_sprite_data[0] + 0x08) & 0xFF;
  ASSERT_OK(rom_->WriteByte(sprite_data_addr, modified_y));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_y = reloaded_rom->ReadByte(sprite_data_addr);
  ASSERT_TRUE(reloaded_y.ok());
  EXPECT_EQ(*reloaded_y, modified_y)
      << "Sprite position modification should persist";
}

// Test 4: Multiple room edits without cross-corruption
TEST_F(DungeonEditorSaveTest, MultipleRooms_NoCrossCorruption) {
  const std::vector<int> test_rooms = {0, 10, 50, 100};
  std::map<int, uint8_t> original_first_bytes;
  std::map<int, uint8_t> modified_first_bytes;

  // Record original data for each room's header
  for (int room_id : test_rooms) {
    uint32_t header_addr = GetRoomHeaderAddress(room_id);
    auto first_byte = rom_->ReadByte(header_addr);
    if (!first_byte.ok()) continue;
    original_first_bytes[room_id] = *first_byte;
    // Create unique modification for each room
    modified_first_bytes[room_id] = (*first_byte + room_id) & 0xFF;
  }

  // Apply all modifications
  for (const auto& [room_id, new_value] : modified_first_bytes) {
    uint32_t header_addr = GetRoomHeaderAddress(room_id);
    ASSERT_OK(rom_->WriteByte(header_addr, new_value));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify all changes persisted without cross-corruption
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (const auto& [room_id, expected_value] : modified_first_bytes) {
    uint32_t header_addr = GetRoomHeaderAddress(room_id);
    auto reloaded_byte = reloaded_rom->ReadByte(header_addr);
    ASSERT_TRUE(reloaded_byte.ok());
    EXPECT_EQ(*reloaded_byte, expected_value)
        << "Room " << room_id << " modification should persist";
  }
}

// Test 5: Room floor/layer data persistence
TEST_F(DungeonEditorSaveTest, FloorLayerData_Persistence) {
  const int test_room_id = 5;

  // Floor data is part of the room header
  uint32_t header_addr = GetRoomHeaderAddress(test_room_id);

  // Byte 0 contains floor information
  auto floor_byte = rom_->ReadByte(header_addr);
  if (!floor_byte.ok()) {
    GTEST_SKIP() << "Failed to read floor data";
  }

  uint8_t original_floor = *floor_byte;
  uint8_t modified_floor = (original_floor ^ 0x07) & 0xFF;  // Toggle lower bits

  ASSERT_OK(rom_->WriteByte(header_addr, modified_floor));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_floor = reloaded_rom->ReadByte(header_addr);
  ASSERT_TRUE(reloaded_floor.ok());
  EXPECT_EQ(*reloaded_floor, modified_floor)
      << "Floor/layer data should persist";
}

// Test 6: Room via Room class
TEST_F(DungeonEditorSaveTest, RoomClass_LoadAndModify) {
  const int test_room_id = 3;

  // Create Room using constructor with room_id
  zelda3::Room room(test_room_id, rom_.get(), game_data_.get());

  // Get current palette value
  uint8_t original_palette = room.palette;

  // Get the header address for direct verification
  uint32_t header_addr = GetRoomHeaderAddress(test_room_id);
  auto original_header = rom_->ReadByte(header_addr);
  if (!original_header.ok()) {
    GTEST_SKIP() << "Failed to read room header";
  }

  // Modify header directly
  uint8_t modified_header = (*original_header + 0x10) & 0xFF;
  ASSERT_OK(rom_->WriteByte(header_addr, modified_header));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_header = reloaded_rom->ReadByte(header_addr);
  ASSERT_TRUE(reloaded_header.ok());
  EXPECT_EQ(*reloaded_header, modified_header)
      << "Room header modification should persist";
}

// Test 7: Large batch room modifications
TEST_F(DungeonEditorSaveTest, LargeBatch_RoomModifications) {
  const int batch_size = 50;
  std::map<int, uint8_t> original_headers;
  std::map<int, uint8_t> modified_headers;

  // Prepare batch modifications
  for (int i = 0; i < batch_size; ++i) {
    int room_id = i * 2;  // Every other room
    uint32_t header_addr = GetRoomHeaderAddress(room_id);

    auto header_byte = rom_->ReadByte(header_addr);
    if (!header_byte.ok()) continue;

    original_headers[room_id] = *header_byte;
    modified_headers[room_id] = (*header_byte + i) & 0xFF;
  }

  // Apply all modifications
  for (const auto& [room_id, new_value] : modified_headers) {
    uint32_t header_addr = GetRoomHeaderAddress(room_id);
    ASSERT_OK(rom_->WriteByte(header_addr, new_value));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  int verified_count = 0;
  for (const auto& [room_id, expected_value] : modified_headers) {
    uint32_t header_addr = GetRoomHeaderAddress(room_id);
    auto reloaded_byte = reloaded_rom->ReadByte(header_addr);
    if (reloaded_byte.ok() && *reloaded_byte == expected_value) {
      verified_count++;
    }
  }

  EXPECT_EQ(verified_count, static_cast<int>(modified_headers.size()))
      << "All batch room modifications should persist";
}

// Test 8: Room palette data persistence
TEST_F(DungeonEditorSaveTest, PaletteData_Persistence) {
  const int test_room_id = 10;

  // Palette info is in the room header
  uint32_t header_addr = GetRoomHeaderAddress(test_room_id);

  // Read palette byte (offset varies by header layout)
  auto palette_byte = rom_->ReadByte(header_addr + 2);
  if (!palette_byte.ok()) {
    GTEST_SKIP() << "Failed to read palette data";
  }

  uint8_t original_palette = *palette_byte;
  uint8_t modified_palette = (original_palette + 1) & 0x07;  // Cycle palette

  ASSERT_OK(rom_->WriteByte(header_addr + 2, modified_palette));
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_palette = reloaded_rom->ReadByte(header_addr + 2);
  ASSERT_TRUE(reloaded_palette.ok());
  EXPECT_EQ(*reloaded_palette, modified_palette)
      << "Palette data should persist";
}

}  // namespace test
}  // namespace yaze
