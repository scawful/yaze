#include "app/zelda3/dungeon/object_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "test/mocks/mock_rom.h"

namespace yaze {
namespace test {

class ObjectParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rom_ = std::make_unique<MockRom>();
    SetupMockData();
    parser_ = std::make_unique<zelda3::ObjectParser>(mock_rom_.get());
  }

  void SetupMockData() {
    std::vector<uint8_t> mock_data(0x100000, 0x00);

    // Set up object subtype tables
    SetupSubtypeTable(mock_data, 0x8000, 0x100);  // Subtype 1 table
    SetupSubtypeTable(mock_data, 0x83F0, 0x80);   // Subtype 2 table
    SetupSubtypeTable(mock_data, 0x84F0, 0x100);  // Subtype 3 table

    // Set up tile data
    SetupTileData(mock_data, 0x1B52, 0x1000);

    static_cast<MockRom*>(mock_rom_.get())->SetTestData(mock_data);
  }

  void SetupSubtypeTable(std::vector<uint8_t>& data, int base_addr, int count) {
    for (int i = 0; i < count; i++) {
      int addr = base_addr + (i * 2);
      if (addr + 1 < (int)data.size()) {
        // Point to tile data at 0x1B52 + (i * 8)
        int tile_offset = (i * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
  }

  void SetupTileData(std::vector<uint8_t>& data, int base_addr, int size) {
    for (int i = 0; i < size; i += 8) {
      int addr = base_addr + i;
      if (addr + 7 < (int)data.size()) {
        // Create simple tile data (4 words per tile)
        for (int j = 0; j < 8; j++) {
          data[addr + j] = (i + j) & 0xFF;
        }
      }
    }
  }

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<zelda3::ObjectParser> parser_;
};

TEST_F(ObjectParserTest, ParseSubtype1Object) {
  auto result = parser_->ParseObject(0x01);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 8);

  // Verify tile data was parsed correctly
  for (const auto& tile : tiles) {
    EXPECT_NE(tile.tile0_.id_, 0);
    EXPECT_NE(tile.tile1_.id_, 0);
    EXPECT_NE(tile.tile2_.id_, 0);
    EXPECT_NE(tile.tile3_.id_, 0);
  }
}

TEST_F(ObjectParserTest, ParseSubtype2Object) {
  auto result = parser_->ParseObject(0x101);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 8);
}

TEST_F(ObjectParserTest, ParseSubtype3Object) {
  auto result = parser_->ParseObject(0x201);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 8);
}

TEST_F(ObjectParserTest, GetObjectSubtype) {
  auto result1 = parser_->GetObjectSubtype(0x01);
  ASSERT_TRUE(result1.ok());
  EXPECT_EQ(result1->subtype, 1);

  auto result2 = parser_->GetObjectSubtype(0x101);
  ASSERT_TRUE(result2.ok());
  EXPECT_EQ(result2->subtype, 2);

  auto result3 = parser_->GetObjectSubtype(0x201);
  ASSERT_TRUE(result3.ok());
  EXPECT_EQ(result3->subtype, 3);
}

TEST_F(ObjectParserTest, ParseObjectSize) {
  auto result = parser_->ParseObjectSize(0x01, 0x12);
  ASSERT_TRUE(result.ok());

  const auto& size_info = result.value();
  EXPECT_EQ(size_info.width_tiles, 4);   // (1 + 1) * 2
  EXPECT_EQ(size_info.height_tiles, 6);  // (2 + 1) * 2
  EXPECT_TRUE(size_info.is_horizontal);
  EXPECT_TRUE(size_info.is_repeatable);
  EXPECT_EQ(size_info.repeat_count, 0x12);
}

TEST_F(ObjectParserTest, ParseObjectRoutine) {
  auto result = parser_->ParseObjectRoutine(0x01);
  ASSERT_TRUE(result.ok());

  const auto& routine_info = result.value();
  EXPECT_NE(routine_info.routine_ptr, 0);
  EXPECT_NE(routine_info.tile_ptr, 0);
  EXPECT_EQ(routine_info.tile_count, 8);
  EXPECT_TRUE(routine_info.is_repeatable);
  EXPECT_TRUE(routine_info.is_orientation_dependent);
}

TEST_F(ObjectParserTest, InvalidObjectId) {
  auto result = parser_->ParseObject(-1);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ObjectParserTest, NullRom) {
  zelda3::ObjectParser null_parser(nullptr);
  auto result = null_parser.ParseObject(0x01);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace test
}  // namespace yaze