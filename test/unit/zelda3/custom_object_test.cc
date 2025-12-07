#include "zelda3/dungeon/custom_object.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace yaze::zelda3 {
namespace {

class CustomObjectManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create a temporary directory for testing
    temp_dir_ = std::filesystem::temp_directory_path() / "yaze_custom_obj_test";
    std::filesystem::create_directories(temp_dir_ / "Sprites/Objects/Data");
    
    // Set up manager with temp root
    CustomObjectManager::Get().Initialize(temp_dir_.string());
  }

  void TearDown() override {
    std::filesystem::remove_all(temp_dir_);
  }

  void WriteBinaryFile(const std::string& filename, const std::vector<uint8_t>& data) {
    auto path = temp_dir_ / filename;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
  }

  std::filesystem::path temp_dir_;
};

TEST_F(CustomObjectManagerTest, LoadSimpleObject) {
  // Simple object: 1 Row, 2 Tiles
  // Row Header: Count=2, Stride=0x80 -> Word 0x8002 -> LE: 02 80
  // Tile 1: ID=0x40, Palette=2, Prio=1 -> 00101000 01000000 -> 0x2840 -> LE: 40 28
  // Tile 2: ID=0x41, Palette=2, Prio=1 -> 00101000 01000001 -> 0x2841 -> LE: 41 28
  // Terminator: 00 00
  // Note: Stride 0x80 is largely ignored by "rel_x/rel_y" calculation in new logic 
  // unless we actually increment current_buffer_pos.
  // In ParseBinaryData: 
  // current_buffer_pos += (count * 2) + jump_offset
  // For this test: count=2 (4 bytes), jump_offset=0x80 (128 bytes)
  // End pos = 4 + 128 = 132.
  
  std::vector<uint8_t> data = {
      0x02, 0x80, // Header: Count=2, Jump=0x80
      0x40, 0x28, // Tile 1
      0x41, 0x28, // Tile 2
      0x00, 0x00  // Terminator
  };
  
  // CustomObjectManager expects files relative to base_path_
  WriteBinaryFile("track_LR.bin", data); 

  auto result = CustomObjectManager::Get().GetObjectInternal(0x31, 0); // ID 0x31, Subtype 0 -> track_LR.bin
  ASSERT_TRUE(result.ok());
  auto obj = result.value();
  ASSERT_NE(obj, nullptr);
  ASSERT_FALSE(obj->IsEmpty());

  ASSERT_EQ(obj->tiles.size(), 2);
  
  // First tile (pos 0) -> x=0, y=0
  EXPECT_EQ(obj->tiles[0].rel_x, 0);
  EXPECT_EQ(obj->tiles[0].rel_y, 0);
  EXPECT_EQ(obj->tiles[0].tile_data, 0x2840);

  // Second tile (pos 2) -> x=1, y=0
  EXPECT_EQ(obj->tiles[1].rel_x, 1);
  EXPECT_EQ(obj->tiles[1].rel_y, 0);
  EXPECT_EQ(obj->tiles[1].tile_data, 0x2841);
}

TEST_F(CustomObjectManagerTest, LoadComplexLayout) {
  // Two rows of 2 tiles
  // Row 1: 0xAAAA, 0xBBBB. Jump to next row (stride 64 bytes - 4 bytes used = 60 bytes jump)
  // Header 1: Count=2, Jump=60 (0x3C). 0x3C02 -> LE: 02 3C
  // Row 2: 0xCCCC, 0xDDDD.
  // Header 2: Count=2, Jump=0. 0x0002 -> LE: 02 00
  // Terminator
  
  std::vector<uint8_t> data = {
    0x02, 0x3C,             // Header 1
    0xAA, 0xAA, 0xBB, 0xBB, // Row 1 Tiles (LE: 0xAAAA, 0xBBBB)
    0x02, 0x00,             // Header 2
    0xCC, 0xCC, 0xDD, 0xDD, // Row 2 Tiles (LE: 0xCCCC, 0xDDDD)
    0x00, 0x00              // Terminator
  };
  
  WriteBinaryFile("complex.bin", data);

  auto result = CustomObjectManager::Get().LoadObject("complex.bin");
  ASSERT_TRUE(result.ok());
  auto obj = result.value();
  
  ASSERT_EQ(obj->tiles.size(), 4);
  
  // Row 1
  EXPECT_EQ(obj->tiles[0].tile_data, 0xAAAA);
  EXPECT_EQ(obj->tiles[0].rel_y, 0);
  EXPECT_EQ(obj->tiles[1].tile_data, 0xBBBB);
  EXPECT_EQ(obj->tiles[1].rel_y, 0);
  
  // Row 2 (Should be at offset 64 = 1 line down)
  // Logic: 
  // Initial pos = 0
  // After row 1 tiles: pos = 4
  // After jump: pos = 4 + 60 = 64
  // Row 2 Tile 1: pos 64 -> y=1, x=0
  
  EXPECT_EQ(obj->tiles[2].tile_data, 0xCCCC);
  EXPECT_EQ(obj->tiles[2].rel_y, 1);
  EXPECT_EQ(obj->tiles[2].rel_x, 0);
  
  EXPECT_EQ(obj->tiles[3].tile_data, 0xDDDD);
  EXPECT_EQ(obj->tiles[3].rel_y, 1);
  EXPECT_EQ(obj->tiles[3].rel_x, 1);
}

TEST_F(CustomObjectManagerTest, MissingFile) {
  auto result = CustomObjectManager::Get().LoadObject("nonexistent.bin");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
}

} // namespace
} // namespace yaze::zelda3
