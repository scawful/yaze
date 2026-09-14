#include "zelda3/dungeon/custom_object.h"

#include <chrono>
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
    previous_state_ = CustomObjectManager::Get().SnapshotState();
    // Use a unique temp directory per test invocation to avoid parallel ctest
    // teardown races across independently spawned gtest processes.
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    temp_dir_ = std::filesystem::temp_directory_path() /
                ("yaze_custom_obj_test_" +
                 std::to_string(static_cast<long long>(nonce)));

    std::error_code cleanup_error;
    std::filesystem::remove_all(temp_dir_, cleanup_error);
    ASSERT_TRUE(std::filesystem::create_directories(temp_dir_ /
                                                    "Sprites/Objects/Data"));

    // Set up manager with temp root
    CustomObjectManager::Get().Initialize(temp_dir_.string());
    CustomObjectManager::Get().ClearObjectFileMap();
  }

  void TearDown() override {
    CustomObjectManager::Get().RestoreState(previous_state_);
    std::error_code cleanup_error;
    std::filesystem::remove_all(temp_dir_, cleanup_error);
  }

  void WriteBinaryFile(const std::string& filename,
                       const std::vector<uint8_t>& data) {
    auto path = temp_dir_ / filename;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
  }

  std::vector<uint8_t> ReadBinaryFile(const std::string& filename) {
    std::ifstream file(temp_dir_ / filename, std::ios::binary);
    return {std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};
  }

  std::filesystem::path temp_dir_;
  CustomObjectManager::State previous_state_;
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

TEST(CustomObjectCodecTest, DenseThirtyTwoTileRowUsesRuntimeCountZeroEncoding) {
  CustomObject object;
  for (int x = 0; x < 32; ++x) {
    object.tiles.push_back({x, 0, static_cast<uint16_t>(0x2800 + x)});
  }

  auto binary_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(binary_or.ok()) << binary_or.status();
  ASSERT_EQ(binary_or->size(), 68u);
  EXPECT_EQ((*binary_or)[0], 0x00);
  EXPECT_EQ((*binary_or)[1], 0x01);

  auto decoded_or = DecodeCustomObjectBinary(*binary_or);
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  EXPECT_EQ(decoded_or->tiles, object.tiles);
}

TEST(CustomObjectCodecTest,
     ThirtyTwoTileSegmentCompensatesForRuntimeJumpBorrow) {
  CustomObject object;
  for (int x = 0; x < 32; ++x) {
    object.tiles.push_back({x, 0, static_cast<uint16_t>(0x2800 + x)});
  }
  object.tiles.push_back({0, 1, 0x2840});

  auto binary_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(binary_or.ok()) << binary_or.status();
  ASSERT_GE(binary_or->size(), 70u);
  EXPECT_EQ((*binary_or)[0], 0x00);
  EXPECT_EQ((*binary_or)[1], 0x81);

  auto decoded_or = DecodeCustomObjectBinary(*binary_or);
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  EXPECT_EQ(decoded_or->tiles, object.tiles);
}

TEST(CustomObjectCodecTest, EmptyObjectUsesTerminatorOnlyEncoding) {
  const CustomObject empty_object;

  auto binary_or = EncodeCustomObjectBinary(empty_object);
  ASSERT_TRUE(binary_or.ok()) << binary_or.status();
  EXPECT_EQ(*binary_or, (std::vector<uint8_t>{0x00, 0x00}));

  auto decoded_or = DecodeCustomObjectBinary(*binary_or);
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  EXPECT_TRUE(decoded_or->IsEmpty());
}

TEST(CustomObjectCodecTest, RepresentableSparseLayoutPreservesCoordinates) {
  CustomObject object;
  object.tiles = {
      {0, 0, 0x2800},
      {3, 0, 0x2803},
      {4, 0, 0x2804},
      {0, 1, 0x2840},
  };

  auto binary_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(binary_or.ok()) << binary_or.status();
  auto decoded_or = DecodeCustomObjectBinary(*binary_or);
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  EXPECT_EQ(decoded_or->tiles, object.tiles);
}

TEST(CustomObjectCodecTest, LeadingAndLongGapsUseRuntimeNoOpBridges) {
  CustomObject object;
  object.tiles = {
      {5, 0, 0x2805},
      {0, 2, 0x2880},
      {63, 63, 0x2FFF},
  };

  auto binary_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(binary_or.ok()) << binary_or.status();
  auto decoded_or = DecodeCustomObjectBinary(*binary_or);
  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();

  std::vector<CustomObject::TileMapEntry> visible_tiles;
  for (const auto& tile : decoded_or->tiles) {
    if (tile.tile_data != 0) {
      visible_tiles.push_back(tile);
    }
  }
  EXPECT_EQ(visible_tiles, object.tiles);
  EXPECT_GT(decoded_or->tiles.size(), object.tiles.size());
}

TEST(CustomObjectCodecTest, RuntimeValidNonCanonicalCountBitsAreAccepted) {
  const std::vector<uint8_t> data = {
      0x21, 0x00,  // Bits 5-7 are ignored by Oracle; count remains one.
      0x34, 0x12, 0x00, 0x00,
  };

  auto decoded_or = DecodeCustomObjectBinary(data);

  ASSERT_TRUE(decoded_or.ok()) << decoded_or.status();
  ASSERT_EQ(decoded_or->tiles.size(), 1u);
  EXPECT_EQ(decoded_or->tiles.front().tile_data, 0x1234);
}

TEST(CustomObjectCodecTest, MalformedStreamsAreRejected) {
  struct MalformedStream {
    const char* name;
    std::vector<uint8_t> bytes;
  };
  const std::vector<MalformedStream> malformed = {
      {"empty", {}},
      {"partial header", {0x01}},
      {"missing terminator", {0x01, 0x00, 0x34, 0x12}},
      {"trailing byte", {0x00, 0x00, 0x7F}},
      {"truncated 32-tile segment", {0x00, 0x01, 0x00, 0x00}},
      {"odd jump", {0x01, 0x01, 0x34, 0x12, 0x00, 0x00}},
      {"partial tile", {0x01, 0x00, 0x34}},
  };

  for (const auto& test_case : malformed) {
    SCOPED_TRACE(test_case.name);
    EXPECT_FALSE(DecodeCustomObjectBinary(test_case.bytes).ok());
  }
}

TEST_F(CustomObjectManagerTest, AssetPathRejectsEscapesAndInvalidNames) {
  EXPECT_TRUE(absl::IsInvalidArgument(
      ResolveCustomObjectAssetPath(temp_dir_.string(), "../escape.bin")
          .status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      ResolveCustomObjectAssetPath(temp_dir_.string(),
                                   (temp_dir_ / "absolute.bin").string())
          .status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      ResolveCustomObjectAssetPath(temp_dir_.string(), "object.dat").status()));
  for (const std::string& invalid_name :
       {"two,files.bin", " line.bin", "line.bin ", "CON.bin", "bad\nline.bin",
        "back\\slash.bin", "question?.bin", "trailingdot./object.bin"}) {
    SCOPED_TRACE(invalid_name);
    EXPECT_TRUE(absl::IsInvalidArgument(
        ResolveCustomObjectAssetPath(temp_dir_.string(), invalid_name)
            .status()));
  }

  const auto asset_root = temp_dir_ / "contained";
  const auto outside = temp_dir_ / "outside";
  ASSERT_TRUE(std::filesystem::create_directories(asset_root));
  ASSERT_TRUE(std::filesystem::create_directories(outside));
  WriteBinaryFile("outside/target.bin", {0x01, 0x00, 0x34, 0x12, 0x00, 0x00});

  std::error_code symlink_error;
  std::filesystem::create_symlink(outside / "target.bin",
                                  asset_root / "target.bin", symlink_error);
  if (!symlink_error) {
    EXPECT_TRUE(absl::IsPermissionDenied(
        ResolveCustomObjectAssetPath(asset_root.string(), "target.bin")
            .status()));
  }

  symlink_error.clear();
  std::filesystem::create_directory_symlink(outside, asset_root / "escape",
                                            symlink_error);
  if (!symlink_error) {
    EXPECT_TRUE(absl::IsPermissionDenied(
        ResolveCustomObjectAssetPath(asset_root.string(), "escape/new.bin")
            .status()));
  }
}

TEST_F(CustomObjectManagerTest, StalePublishPreservesExistingFile) {
  const std::vector<uint8_t> original = {
      0x01, 0x00, 0xAA, 0x2A, 0x00, 0x00,
  };
  WriteBinaryFile("existing.bin", original);
  CustomObject replacement;
  replacement.tiles = {{0, 0, 0x2BBB}};
  const std::vector<uint8_t> stale_snapshot = {
      0x01, 0x00, 0xCC, 0x2C, 0x00, 0x00,
  };
  auto target_or =
      ResolveCustomObjectAssetPath(temp_dir_.string(), "existing.bin");
  ASSERT_TRUE(target_or.ok()) << target_or.status();

  const auto published_or =
      PublishCustomObjectBinary(temp_dir_.string(), "existing.bin", replacement,
                                stale_snapshot, *target_or);

  EXPECT_TRUE(absl::IsAborted(published_or.status()));
  EXPECT_EQ(ReadBinaryFile("existing.bin"), original);
}

TEST_F(CustomObjectManagerTest, PublishHasExactReadbackAndNoTempArtifact) {
  const std::vector<uint8_t> original = {
      0x01, 0x00, 0xAA, 0x2A, 0x00, 0x00,
  };
  WriteBinaryFile("published.bin", original);
  CustomObject object;
  object.tiles = {
      {0, 0, 0x2800},
      {2, 0, 0x2802},
      {0, 1, 0x2840},
  };
  auto expected_or = EncodeCustomObjectBinary(object);
  ASSERT_TRUE(expected_or.ok()) << expected_or.status();
  auto target_or =
      ResolveCustomObjectAssetPath(temp_dir_.string(), "published.bin");
  ASSERT_TRUE(target_or.ok()) << target_or.status();

  const auto published_or = PublishCustomObjectBinary(
      temp_dir_.string(), "published.bin", object, original, *target_or);

  ASSERT_TRUE(published_or.ok()) << published_or.status();
  EXPECT_EQ(*published_or, *expected_or);
  EXPECT_EQ(ReadBinaryFile("published.bin"), *expected_or);
  auto loaded_or = CustomObjectManager::Get().LoadObject("published.bin");
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  EXPECT_EQ((*loaded_or)->tiles, object.tiles);
  for (const auto& entry : std::filesystem::directory_iterator(temp_dir_)) {
    EXPECT_EQ(entry.path().filename().string().find(".yaze-tmp-"),
              std::string::npos);
  }
}

TEST_F(CustomObjectManagerTest, PublishRequiresExactSourceSnapshot) {
  const std::vector<uint8_t> original = {
      0x01, 0x00, 0xAA, 0x2A, 0x00, 0x00,
  };
  WriteBinaryFile("published.bin", original);
  CustomObject object;
  object.tiles = {{0, 0, 0x2BBB}};
  auto target_or =
      ResolveCustomObjectAssetPath(temp_dir_.string(), "published.bin");
  ASSERT_TRUE(target_or.ok()) << target_or.status();

  const auto published_or = PublishCustomObjectBinary(
      temp_dir_.string(), "published.bin", object, {}, *target_or);

  EXPECT_TRUE(absl::IsFailedPrecondition(published_or.status()));
  EXPECT_EQ(ReadBinaryFile("published.bin"), original);
}

TEST_F(CustomObjectManagerTest, RuntimeSubtypeCapacityBoundsMappings) {
  std::vector<std::string> oversized_31(17, "track_LR.bin");
  std::vector<std::string> oversized_32(4, "furnace.bin");
  std::vector<std::string> oversized_54(3, "kydreeok_body.bin");
  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, oversized_31}, {0x32, oversized_32}, {0x54, oversized_54}});

  EXPECT_EQ(CustomObjectManager::RuntimeSubtypeCountForObject(0x31), 16);
  EXPECT_EQ(CustomObjectManager::RuntimeSubtypeCountForObject(0x32), 3);
  EXPECT_EQ(CustomObjectManager::RuntimeSubtypeCountForObject(0x54), 2);
  EXPECT_EQ(CustomObjectManager::Get().GetSubtypeCount(0x31), 16);
  EXPECT_EQ(CustomObjectManager::Get().GetSubtypeCount(0x32), 3);
  EXPECT_EQ(CustomObjectManager::Get().GetSubtypeCount(0x54), 2);
  EXPECT_TRUE(CustomObjectManager::Get().ResolveFilename(0x31, 16).empty());
  EXPECT_TRUE(CustomObjectManager::Get().ResolveFilename(0x32, 3).empty());
  EXPECT_TRUE(CustomObjectManager::Get().ResolveFilename(0x54, 2).empty());
  EXPECT_TRUE(absl::IsOutOfRange(
      CustomObjectManager::Get().GetObjectInternal(0x31, 16).status()));
  EXPECT_TRUE(absl::IsOutOfRange(
      CustomObjectManager::Get().GetObjectInternal(0x32, 3).status()));
  EXPECT_TRUE(absl::IsOutOfRange(
      CustomObjectManager::Get().GetObjectInternal(0x54, 2).status()));
}

TEST_F(CustomObjectManagerTest, SpriteBodyDefaultSubtypeOrderMatchesOracleAbi) {
  const auto& defaults =
      CustomObjectManager::DefaultSubtypeFilenamesForObject(0x54);
  ASSERT_EQ(defaults.size(), 2u);
  EXPECT_EQ(defaults[0], "kydreeok_body.bin");
  EXPECT_EQ(defaults[1], "manhandla_body_1a.bin");
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(0x54, 0), defaults[0]);
  EXPECT_EQ(CustomObjectManager::Get().ResolveFilename(0x54, 1), defaults[1]);
  EXPECT_TRUE(CustomObjectManager::Get().ResolveFilename(0x54, 2).empty());
}

TEST_F(CustomObjectManagerTest,
       SlotBindingDistinguishesDefaultsMappingsAndUnmappedSlots) {
  auto default_binding =
      CustomObjectManager::Get().ResolveSlotBinding(0x31, 13);
  ASSERT_TRUE(default_binding.ok()) << default_binding.status();
  EXPECT_EQ(default_binding->filename, "wall_sword_house.bin");
  EXPECT_EQ(default_binding->origin,
            CustomObjectMappingOrigin::kDefaultFilename);

  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31, {"custom_track.bin", ""}}});
  auto mapped_binding = CustomObjectManager::Get().ResolveSlotBinding(0x31, 0);
  ASSERT_TRUE(mapped_binding.ok()) << mapped_binding.status();
  EXPECT_EQ(mapped_binding->filename, "custom_track.bin");
  EXPECT_EQ(mapped_binding->origin,
            CustomObjectMappingOrigin::kConfiguredFilename);

  auto empty_binding = CustomObjectManager::Get().ResolveSlotBinding(0x31, 1);
  ASSERT_TRUE(empty_binding.ok()) << empty_binding.status();
  EXPECT_TRUE(empty_binding->filename.empty());
  EXPECT_EQ(empty_binding->origin,
            CustomObjectMappingOrigin::kConfiguredSlotUnmapped);

  auto short_binding = CustomObjectManager::Get().ResolveSlotBinding(0x31, 2);
  ASSERT_TRUE(short_binding.ok()) << short_binding.status();
  EXPECT_TRUE(short_binding->filename.empty());
  EXPECT_EQ(short_binding->origin,
            CustomObjectMappingOrigin::kConfiguredSlotUnmapped);

  EXPECT_TRUE(absl::IsNotFound(
      CustomObjectManager::Get().ResolveSlotBinding(0x30, 0).status()));
  EXPECT_TRUE(absl::IsOutOfRange(
      CustomObjectManager::Get().ResolveSlotBinding(0x31, 16).status()));
}

TEST(CustomObjectRuntimeTileWordTest, AppliesSpriteBodyPageMaskAfterZeroCheck) {
  EXPECT_EQ(CustomObjectRuntimeTileWord(0x31, 0x1D32), 0x1D32);
  EXPECT_EQ(CustomObjectRuntimeTileWord(0x32, 0x1D32), 0x1D32);
  EXPECT_EQ(CustomObjectRuntimeTileWord(0x54, 0x1D32), 0x1F32);
  EXPECT_EQ(CustomObjectRuntimeTileWord(0x54, 0x0000), 0x0000);
}

TEST_F(CustomObjectManagerTest, ManagerRejectsMalformedObject) {
  WriteBinaryFile("malformed.bin", {0x01, 0x00, 0x34});

  const auto loaded_or = CustomObjectManager::Get().LoadObject("malformed.bin");

  EXPECT_FALSE(loaded_or.ok());
  EXPECT_TRUE(absl::IsDataLoss(loaded_or.status()));
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

TEST_F(CustomObjectManagerTest,
       WallCornerObjectIdsNeverResolveAsCustomTrackAssets) {
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

  for (const int object_id : {0x100, 0x101, 0x102, 0x103}) {
    SCOPED_TRACE(object_id);
    const auto object =
        CustomObjectManager::Get().GetObjectInternal(object_id, /*subtype=*/0);
    EXPECT_TRUE(absl::IsNotFound(object.status()));
    EXPECT_TRUE(CustomObjectManager::Get()
                    .ResolveFilename(object_id, /*subtype=*/0)
                    .empty());
    EXPECT_EQ(CustomObjectManager::Get().GetSubtypeCount(object_id), 0);
  }
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
