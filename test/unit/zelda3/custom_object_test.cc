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
    auto path = temp_dir_ / "Sprites/Objects/Data" / filename;
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
  
  std::vector<uint8_t> data = {
      0x02, 0x80, // Header
      0x40, 0x28, // Tile 1
      0x41, 0x28, // Tile 2
      0x00, 0x00  // Terminator
  };
  
  WriteBinaryFile("track_LR.bin", data); // Subtype 0 maps to track_LR.bin

  auto result = CustomObjectManager::Get().GetObjectInternal(0x31, 0); // ID 0x31, Subtype 0
  ASSERT_TRUE(result.ok());
  auto obj = result.value();
  ASSERT_NE(obj, nullptr);
  ASSERT_FALSE(obj->IsEmpty());

  ASSERT_EQ(obj->rows.size(), 1);
  EXPECT_EQ(obj->rows[0].stride, 0x80);
  EXPECT_EQ(obj->rows[0].tiles.size(), 2);

  EXPECT_EQ(obj->rows[0].tiles[0].id_, 0x40);
  EXPECT_EQ(obj->rows[0].tiles[0].palette_, 2);
  
  EXPECT_EQ(obj->rows[0].tiles[1].id_, 0x41);
}

TEST_F(CustomObjectManagerTest, LoadEmpty) {
  // Empty file or just terminator
  std::vector<uint8_t> data = { 0x00, 0x00 };
  WriteBinaryFile("track_UD.bin", data); // Subtype 1 maps to track_UD.bin

  auto result = CustomObjectManager::Get().GetObjectInternal(0x31, 1);
  ASSERT_TRUE(result.ok());
  auto obj = result.value();
  EXPECT_TRUE(obj->IsEmpty());
}

TEST_F(CustomObjectManagerTest, MissingFile) {
  auto result = CustomObjectManager::Get().GetObjectInternal(0x31, 99); // Invalid subtype
  EXPECT_FALSE(result.ok()); // File won't exist
}

} // namespace
} // namespace yaze::zelda3
