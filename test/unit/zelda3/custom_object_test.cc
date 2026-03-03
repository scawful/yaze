#include "zelda3/dungeon/custom_object.h"

#include <filesystem>
#include <fstream>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

  void TearDown() override { std::filesystem::remove_all(temp_dir_); }

  void WriteBinaryFile(const std::string& filename,
                       const std::vector<uint8_t>& data) {
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
      0x02, 0x80,  // Header: Count=2, Jump=0x80
      0x40, 0x28,  // Tile 1
      0x41, 0x28,  // Tile 2
      0x00, 0x00   // Terminator
  };

  // CustomObjectManager expects files relative to base_path_
  WriteBinaryFile("track_LR.bin", data);

  auto result = CustomObjectManager::Get().GetObjectInternal(
      0x31, 0);  // ID 0x31, Subtype 0 -> track_LR.bin
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
  // Row 1: 0xAAAA, 0xBBBB. Jump to next row (stride 128 bytes, jump 128)
  // Header 1: Count=2, Jump=128 (0x80). 0x8002 -> LE: 02 80
  // Row 2: 0xCCCC, 0xDDDD.
  // Header 2: Count=2, Jump=0. 0x0002 -> LE: 02 00
  // Terminator

  std::vector<uint8_t> data = {
      0x02, 0x80,              // Header 1
      0xAA, 0xAA, 0xBB, 0xBB,  // Row 1 Tiles (LE: 0xAAAA, 0xBBBB)
      0x02, 0x00,              // Header 2
      0xCC, 0xCC, 0xDD, 0xDD,  // Row 2 Tiles (LE: 0xCCCC, 0xDDDD)
      0x00, 0x00               // Terminator
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

TEST_F(CustomObjectManagerTest, CornerObjectIdsMapToTrackCornerOverrides) {
  auto one_tile_object = [](uint16_t tile_word) {
    return std::vector<uint8_t>{
        0x01,
        0x00,                                    // count=1, jump=0
        static_cast<uint8_t>(tile_word & 0xFF),  // tile low
        static_cast<uint8_t>(tile_word >> 8),    // tile high
        0x00,
        0x00  // terminator
    };
  };

  // These file names are the canonical object 0x31 corner slots:
  // index 2=TL, 3=TR, 4=BL, 5=BR.
  WriteBinaryFile("track_corner_TL.bin", one_tile_object(0x1111));
  WriteBinaryFile("track_corner_TR.bin", one_tile_object(0x2222));
  WriteBinaryFile("track_corner_BL.bin", one_tile_object(0x3333));
  WriteBinaryFile("track_corner_BR.bin", one_tile_object(0x4444));
  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31,
        {"track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
         "track_corner_TR.bin", "track_corner_BL.bin",
         "track_corner_BR.bin"}}});

  auto tl = CustomObjectManager::Get().GetObjectInternal(/*object_id=*/0x100,
                                                         /*subtype=*/0);
  ASSERT_TRUE(tl.ok());
  ASSERT_EQ(tl.value()->tiles.size(), 1u);
  EXPECT_EQ(tl.value()->tiles[0].tile_data, 0x1111);

  auto bl = CustomObjectManager::Get().GetObjectInternal(/*object_id=*/0x101,
                                                         /*subtype=*/0);
  ASSERT_TRUE(bl.ok());
  ASSERT_EQ(bl.value()->tiles.size(), 1u);
  EXPECT_EQ(bl.value()->tiles[0].tile_data, 0x3333);

  auto tr = CustomObjectManager::Get().GetObjectInternal(/*object_id=*/0x102,
                                                         /*subtype=*/0);
  ASSERT_TRUE(tr.ok());
  ASSERT_EQ(tr.value()->tiles.size(), 1u);
  EXPECT_EQ(tr.value()->tiles[0].tile_data, 0x2222);

  auto br = CustomObjectManager::Get().GetObjectInternal(/*object_id=*/0x103,
                                                         /*subtype=*/0);
  ASSERT_TRUE(br.ok());
  ASSERT_EQ(br.value()->tiles.size(), 1u);
  EXPECT_EQ(br.value()->tiles[0].tile_data, 0x4444);
}

TEST_F(CustomObjectManagerTest,
       CornerObjectIdsRequireExplicitTrackMappingForOverrides) {
  auto one_tile_object = [](uint16_t tile_word) {
    return std::vector<uint8_t>{
        0x01,
        0x00,                                    // count=1, jump=0
        static_cast<uint8_t>(tile_word & 0xFF),  // tile low
        static_cast<uint8_t>(tile_word >> 8),    // tile high
        0x00,
        0x00  // terminator
    };
  };

  // Files exist on disk, but without an explicit object 0x31 mapping the
  // subtype-2 corner aliases must stay disabled.
  WriteBinaryFile("track_corner_TL.bin", one_tile_object(0x1111));
  WriteBinaryFile("track_corner_TR.bin", one_tile_object(0x2222));
  WriteBinaryFile("track_corner_BL.bin", one_tile_object(0x3333));
  WriteBinaryFile("track_corner_BR.bin", one_tile_object(0x4444));
  CustomObjectManager::Get().ClearObjectFileMap();

  auto tl = CustomObjectManager::Get().GetObjectInternal(/*object_id=*/0x100,
                                                         /*subtype=*/0);
  EXPECT_FALSE(tl.ok());
  EXPECT_EQ(tl.status().code(), absl::StatusCode::kNotFound);
}

TEST_F(CustomObjectManagerTest,
       CornerObjectIdsResolveExpectedOverrideFilenames) {
  // For corner aliases, filename selection must be independent of subtype.
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(/*object_id=*/0x100,
                                                       /*subtype=*/0),
            "track_corner_TL.bin");
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(/*object_id=*/0x100,
                                                       /*subtype=*/0x12),
            "track_corner_TL.bin");
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(/*object_id=*/0x101,
                                                       /*subtype=*/0),
            "track_corner_BL.bin");
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(/*object_id=*/0x102,
                                                       /*subtype=*/0),
            "track_corner_TR.bin");
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(/*object_id=*/0x103,
                                                       /*subtype=*/0),
            "track_corner_BR.bin");
}

// ============================================================================
// GetBoundingBox Tests
// ============================================================================

TEST(CustomObjectBoundingBox, EmptyObjectReturnsZeroBox) {
  CustomObject obj;
  auto bb = obj.GetBoundingBox();
  EXPECT_EQ(bb.min_x, 0);
  EXPECT_EQ(bb.min_y, 0);
  EXPECT_EQ(bb.max_x, 0);
  EXPECT_EQ(bb.max_y, 0);
  EXPECT_EQ(bb.width(), 1);
  EXPECT_EQ(bb.height(), 1);
}

TEST(CustomObjectBoundingBox, SingleTileReturnsUnitBox) {
  CustomObject obj;
  obj.tiles.push_back({3, 5, 0x2840});
  auto bb = obj.GetBoundingBox();
  EXPECT_EQ(bb.min_x, 3);
  EXPECT_EQ(bb.min_y, 5);
  EXPECT_EQ(bb.max_x, 3);
  EXPECT_EQ(bb.max_y, 5);
  EXPECT_EQ(bb.width(), 1);
  EXPECT_EQ(bb.height(), 1);
}

TEST(CustomObjectBoundingBox, HorizontalRowOfTiles) {
  CustomObject obj;
  obj.tiles.push_back({0, 0, 0x0001});
  obj.tiles.push_back({1, 0, 0x0002});
  obj.tiles.push_back({2, 0, 0x0003});
  auto bb = obj.GetBoundingBox();
  EXPECT_EQ(bb.min_x, 0);
  EXPECT_EQ(bb.min_y, 0);
  EXPECT_EQ(bb.max_x, 2);
  EXPECT_EQ(bb.max_y, 0);
  EXPECT_EQ(bb.width(), 3);
  EXPECT_EQ(bb.height(), 1);
}

TEST(CustomObjectBoundingBox, TwoByTwoBlock) {
  CustomObject obj;
  obj.tiles.push_back({0, 0, 0x0001});
  obj.tiles.push_back({1, 0, 0x0002});
  obj.tiles.push_back({0, 1, 0x0003});
  obj.tiles.push_back({1, 1, 0x0004});
  auto bb = obj.GetBoundingBox();
  EXPECT_EQ(bb.min_x, 0);
  EXPECT_EQ(bb.min_y, 0);
  EXPECT_EQ(bb.max_x, 1);
  EXPECT_EQ(bb.max_y, 1);
  EXPECT_EQ(bb.width(), 2);
  EXPECT_EQ(bb.height(), 2);
}

TEST(CustomObjectBoundingBox, NonZeroOriginTiles) {
  // Tiles that don't start at (0,0) - bounding box should reflect actual range
  CustomObject obj;
  obj.tiles.push_back({5, 3, 0x0001});
  obj.tiles.push_back({7, 3, 0x0002});
  obj.tiles.push_back({5, 5, 0x0003});
  obj.tiles.push_back({7, 5, 0x0004});
  auto bb = obj.GetBoundingBox();
  EXPECT_EQ(bb.min_x, 5);
  EXPECT_EQ(bb.min_y, 3);
  EXPECT_EQ(bb.max_x, 7);
  EXPECT_EQ(bb.max_y, 5);
  EXPECT_EQ(bb.width(), 3);
  EXPECT_EQ(bb.height(), 3);
}

TEST_F(CustomObjectManagerTest, GetBoundingBoxFromLoadedObject) {
  // Two rows of 2 tiles: a 2x2 block at origin
  std::vector<uint8_t> data = {
      0x02, 0x80,              // Header 1: Count=2, Jump=0x80
      0xAA, 0xAA, 0xBB, 0xBB,  // Row 1 Tiles
      0x02, 0x00,              // Header 2: Count=2, Jump=0x00
      0xCC, 0xCC, 0xDD, 0xDD,  // Row 2 Tiles
      0x00, 0x00               // Terminator
  };

  WriteBinaryFile("bbox_test.bin", data);
  auto result = CustomObjectManager::Get().LoadObject("bbox_test.bin");
  ASSERT_TRUE(result.ok());
  auto obj = result.value();
  ASSERT_EQ(obj->tiles.size(), 4);

  auto bb = obj->GetBoundingBox();
  EXPECT_EQ(bb.min_x, 0);
  EXPECT_EQ(bb.min_y, 0);
  EXPECT_EQ(bb.max_x, 1);
  EXPECT_EQ(bb.max_y, 1);
  EXPECT_EQ(bb.width(), 2);
  EXPECT_EQ(bb.height(), 2);
}

}  // namespace
}  // namespace yaze::zelda3
