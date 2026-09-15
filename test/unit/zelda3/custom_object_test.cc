#include "zelda3/dungeon/custom_object.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "core/source_artifact_publisher.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

struct OracleRuntimeReplay {
  CustomObject payload;
  std::array<uint16_t, 64 * 64> tilemap;
};

// Source-backed model of object_handler.asm's .lineLoop: decrement the complete
// 16-bit header, then XBA/AND to obtain the jump from the saved segment origin.
// This does not use the production decoder or its count/jump calculation. It
// is an ASM contract check, not independent emulator or pixel-parity evidence.
absl::StatusOr<OracleRuntimeReplay> ReplayOracleCustomObject(
    int object_id, const std::vector<uint8_t>& bytes) {
  OracleRuntimeReplay replay;
  replay.tilemap.fill(
      0xA55A);  // A zero source word must preserve existing tiles.
  size_t cursor = 0;
  size_t destination = 0;
  auto read_word = [&]() -> absl::StatusOr<uint16_t> {
    if (cursor + 1 >= bytes.size()) {
      return absl::DataLossError("Oracle replay reached a truncated word");
    }
    const uint16_t word = bytes[cursor] | (bytes[cursor + 1] << 8);
    cursor += 2;
    return word;
  };
  while (true) {
    auto header = read_word();
    if (!header.ok()) {
      return header.status();
    }
    if (*header == 0) {
      if (cursor != bytes.size()) {
        return absl::DataLossError("Oracle replay found trailing asset bytes");
      }
      return replay;
    }
    const size_t segment_origin = destination;
    uint16_t counter = *header;
    do {
      auto word = read_word();
      if (!word.ok()) {
        return word.status();
      }
      if ((destination & 1) != 0 || destination / 2 >= replay.tilemap.size()) {
        return absl::OutOfRangeError("Oracle replay destination is invalid");
      }
      replay.payload.tiles.push_back({static_cast<int>((destination % 128) / 2),
                                      static_cast<int>(destination / 128),
                                      *word});
      if (*word != 0) {
        replay.tilemap[destination / 2] =
            object_id == 0x54 ? (*word | 0x0300) : *word;
      }
      destination += 2;
      --counter;
    } while ((counter & 0x001F) != 0);
    destination = segment_origin + (counter >> 8);
  }
}

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
  // The jump is relative to the saved segment origin, not the final tile.

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

TEST(CustomObjectCodecTest, SparseWordsPreserveBackgroundAndAllTileAttributes) {
  const std::vector<uint8_t> source = {
      0x04, 0x80, 0x00, 0x00, 0x55, 0xA9, 0xAA, 0x56, 0xFF,
      0xFF, 0x02, 0x00, 0x00, 0x00, 0x23, 0x01, 0x00, 0x00,
  };
  const auto decoded = DecodeCustomObjectBinary(source);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  const auto encoded = EncodeCustomObjectBinary(*decoded);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  const auto reopened = DecodeCustomObjectBinary(*encoded);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->tiles, decoded->tiles);

  for (const int object_id : CustomObjectManager::RuntimeObjectIds()) {
    SCOPED_TRACE(object_id);
    const auto original = ReplayOracleCustomObject(object_id, source);
    ASSERT_TRUE(original.ok()) << original.status();
    const auto replayed = ReplayOracleCustomObject(object_id, *encoded);
    ASSERT_TRUE(replayed.ok()) << replayed.status();
    EXPECT_EQ(original->payload.tiles, decoded->tiles);
    EXPECT_EQ(replayed->tilemap, original->tilemap);
    EXPECT_EQ(replayed->tilemap[0], 0xA55A);
    EXPECT_EQ(replayed->tilemap[64], 0xA55A);
    EXPECT_EQ(replayed->tilemap[1], object_id == 0x54 ? 0xAB55 : 0xA955);
    EXPECT_EQ(replayed->tilemap[2], object_id == 0x54 ? 0x57AA : 0x56AA);
    EXPECT_EQ(replayed->tilemap[3], 0xFFFF);
  }
}

// Opt-in real-source audit. No Oracle payloads are vendored, no source paths
// are guessed, and only copies inside the fixture's temporary folder are
// published. An explicitly configured but incomplete corpus fails the tests.
class CustomObjectOracleAssetTest
    : public CustomObjectManagerTest,
      public ::testing::WithParamInterface<std::pair<int, int>> {};

TEST_P(CustomObjectOracleAssetTest, DecodePublishReloadPreservesRuntimeLayout) {
  const char* asset_root = std::getenv("YAZE_TEST_ORACLE_CUSTOM_OBJECTS");
  if (asset_root == nullptr) {
    GTEST_SKIP() << "Set YAZE_TEST_ORACLE_CUSTOM_OBJECTS to Oracle's "
                    "Dungeons/Objects/Data folder for the 21-asset audit";
  }
  const auto [object_id, subtype] = GetParam();
  auto& manager = CustomObjectManager::Get();
  const std::string filename = manager.ResolveFilename(object_id, subtype);
  ASSERT_FALSE(filename.empty());
  SCOPED_TRACE(filename);
  const auto source = LoadCustomObjectAsset(asset_root, filename);
  ASSERT_TRUE(source.ok()) << source.status();
  RecordProperty("source_sha256", core::ComputeSourceArtifactSha256(
                                      std::string(source->source_bytes.begin(),
                                                  source->source_bytes.end())));
  RecordProperty("source_path", source->resolved_path.string());
  const auto original =
      ReplayOracleCustomObject(object_id, source->source_bytes);
  ASSERT_TRUE(original.ok()) << original.status();
  ASSERT_EQ(source->object.tiles, original->payload.tiles);
  ASSERT_FALSE(source->object.IsEmpty());

  WriteBinaryFile(filename, source->source_bytes);
  const auto cached = manager.GetObjectInternal(object_id, subtype);
  ASSERT_TRUE(cached.ok()) << cached.status();
  EXPECT_EQ((*cached)->tiles, original->payload.tiles);
  const auto target =
      ResolveCustomObjectAssetPath(temp_dir_.string(), filename);
  ASSERT_TRUE(target.ok()) << target.status();
  const auto published =
      PublishCustomObjectBinary(temp_dir_.string(), filename, source->object,
                                source->source_bytes, *target);
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_EQ(ReadBinaryFile(filename), *published);
  manager.ReloadAll();
  const auto reopened = manager.GetObjectInternal(object_id, subtype);
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_NE(reopened->get(), cached->get());
  EXPECT_EQ((*reopened)->tiles, original->payload.tiles);
  const auto replayed = ReplayOracleCustomObject(object_id, *published);
  ASSERT_TRUE(replayed.ok()) << replayed.status();
  EXPECT_EQ(replayed->tilemap, original->tilemap);

  // The native source publisher is not allowed to mutate the input corpus.
  const auto untouched_source = LoadCustomObjectAsset(asset_root, filename);
  ASSERT_TRUE(untouched_source.ok()) << untouched_source.status();
  EXPECT_EQ(untouched_source->source_bytes, source->source_bytes);
}

std::vector<std::pair<int, int>> OracleRuntimeAssetCases() {
  std::vector<std::pair<int, int>> cases;
  for (const int object_id : CustomObjectManager::RuntimeObjectIds()) {
    for (int subtype = 0;
         subtype < CustomObjectManager::RuntimeSubtypeCountForObject(object_id);
         ++subtype) {
      cases.emplace_back(object_id, subtype);
    }
  }
  return cases;
}

INSTANTIATE_TEST_SUITE_P(
    OracleRuntimeAssets, CustomObjectOracleAssetTest,
    ::testing::ValuesIn(OracleRuntimeAssetCases()),
    [](const ::testing::TestParamInfo<std::pair<int, int>>& info) {
      const auto& filename =
          CustomObjectManager::DefaultSubtypeFilenamesForObject(
              info.param.first)[info.param.second];
      return std::filesystem::path(filename).stem().string();
    });

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

TEST_F(CustomObjectManagerTest, NestedAssetPathsRequireForwardSlashes) {
  ASSERT_TRUE(std::filesystem::create_directory(temp_dir_ / "nested"));
  WriteBinaryFile("nested/object.bin", {0x01, 0x00, 0x34, 0x12, 0x00, 0x00});

  auto existing =
      ResolveCustomObjectAssetPath(temp_dir_.string(), "nested/object.bin");
  ASSERT_TRUE(existing.ok()) << existing.status();
  EXPECT_EQ(*existing,
            std::filesystem::canonical(temp_dir_ / "nested/object.bin"));
  auto missing =
      ResolveCustomObjectAssetPath(temp_dir_.string(), "nested/new.bin");
  ASSERT_TRUE(missing.ok()) << missing.status();
  EXPECT_EQ(*missing,
            std::filesystem::canonical(temp_dir_ / "nested") / "new.bin");

  for (const std::string& invalid_name :
       {"nested\\object.bin", "nested\\new.bin", "nested/..\\object.bin"}) {
    SCOPED_TRACE(invalid_name);
    EXPECT_TRUE(absl::IsInvalidArgument(
        ResolveCustomObjectAssetPath(temp_dir_.string(), invalid_name)
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

TEST_F(CustomObjectManagerTest, TrackAndIceSubtypeOrderMatchesOracleAbi) {
  EXPECT_THAT(CustomObjectManager::RuntimeObjectIds(),
              ::testing::ElementsAre(0x31, 0x32, 0x54));
  EXPECT_THAT(CustomObjectManager::DefaultSubtypeFilenamesForObject(0x31),
              ::testing::ElementsAre(
                  "track_LR.bin", "track_UD.bin", "track_corner_TL.bin",
                  "track_corner_TR.bin", "track_corner_BL.bin",
                  "track_corner_BR.bin", "track_floor_UD.bin",
                  "track_floor_LR.bin", "track_floor_corner_TL.bin",
                  "track_floor_corner_TR.bin", "track_floor_corner_BL.bin",
                  "track_floor_corner_BR.bin", "track_floor_any.bin",
                  "wall_sword_house.bin", "track_any.bin", "small_statue.bin"));
  EXPECT_THAT(
      CustomObjectManager::DefaultSubtypeFilenamesForObject(0x32),
      ::testing::ElementsAre("furnace.bin", "firewood.bin", "ice_chair.bin"));
  EXPECT_EQ(OracleRuntimeAssetCases().size(), 21u);
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

  // Row 2 starts 128 bytes (64 tiles) after the saved origin, at x=0, y=1.

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

TEST_F(CustomObjectManagerTest, MissingFileStaysCachedUntilAssetReload) {
  auto& manager = CustomObjectManager::Get();
  const auto missing = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(absl::IsNotFound(missing.status()));
  const uint64_t generation = manager.asset_generation();

  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x40, 0x28, 0x00, 0x00});
  for (int lookup = 0; lookup < 3; ++lookup) {
    EXPECT_EQ(manager.GetObjectInternal(0x31, 0).status(), missing.status());
  }
  EXPECT_EQ(manager.asset_generation(), generation);

  manager.ReloadAll();
  EXPECT_NE(manager.asset_generation(), generation);
  const auto reloaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(reloaded.ok()) << reloaded.status();
  ASSERT_EQ((*reloaded)->tiles.size(), 1u);
  EXPECT_EQ((*reloaded)->tiles.front().tile_data, 0x2840);
}

TEST_F(CustomObjectManagerTest, MalformedFileStaysCachedUntilAssetReload) {
  auto& manager = CustomObjectManager::Get();
  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x40});
  const auto malformed = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(absl::IsDataLoss(malformed.status()));

  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x41, 0x28, 0x00, 0x00});
  for (int lookup = 0; lookup < 3; ++lookup) {
    EXPECT_EQ(manager.GetObjectInternal(0x31, 0).status(), malformed.status());
  }

  manager.ReloadAll();
  const auto reloaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(reloaded.ok()) << reloaded.status();
  ASSERT_EQ((*reloaded)->tiles.size(), 1u);
  EXPECT_EQ((*reloaded)->tiles.front().tile_data, 0x2841);
}

TEST_F(CustomObjectManagerTest, SuccessfulFileStaysCachedUntilAssetReload) {
  auto& manager = CustomObjectManager::Get();
  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x40, 0x28, 0x00, 0x00});
  const auto original = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(original.ok()) << original.status();

  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x41, 0x28, 0x00, 0x00});
  const auto cached = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(cached.ok()) << cached.status();
  EXPECT_EQ(cached->get(), original->get());
  ASSERT_EQ((*cached)->tiles.size(), 1u);
  EXPECT_EQ((*cached)->tiles.front().tile_data, 0x2840);

  manager.ReloadAll();
  const auto reloaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(reloaded.ok()) << reloaded.status();
  EXPECT_NE(reloaded->get(), original->get());
  ASSERT_EQ((*reloaded)->tiles.size(), 1u);
  EXPECT_EQ((*reloaded)->tiles.front().tile_data, 0x2841);
}

TEST_F(CustomObjectManagerTest, MappingChangeRetriesCachedFailure) {
  auto& manager = CustomObjectManager::Get();
  const auto missing = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(absl::IsNotFound(missing.status()));
  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x40, 0x28, 0x00, 0x00});

  manager.SetObjectFileMap({{0x31, {"track_LR.bin"}}});

  const auto loaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ((*loaded)->tiles.size(), 1u);
  EXPECT_EQ((*loaded)->tiles.front().tile_data, 0x2840);
}

TEST_F(CustomObjectManagerTest, RuntimeContextsKeepIndependentLoadResults) {
  auto& manager = CustomObjectManager::Get();
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const uint64_t first_id = static_cast<uint64_t>(nonce) | (uint64_t{1} << 63);
  const uint64_t second_id = first_id ^ (uint64_t{1} << 62);
  struct RestoreRuntimeContext {
    CustomObjectManager& manager;
    CustomObjectManager::State state;
    std::optional<uint64_t> active_id;
    uint64_t first_id;
    uint64_t second_id;
    ~RestoreRuntimeContext() {
      manager.RemoveRuntimeContext(first_id);
      manager.RemoveRuntimeContext(second_id);
      if (active_id.has_value()) {
        manager.ActivateRuntimeContext(*active_id, state);
      } else {
        manager.ActivateStandaloneContext();
      }
    }
  } restore{manager, manager.SnapshotState(),
            manager.active_runtime_context_id(), first_id, second_id};
  const auto state = manager.SnapshotState();

  manager.ActivateRuntimeContext(first_id, state);
  const auto missing = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(absl::IsNotFound(missing.status()));
  const uint64_t first_generation = manager.asset_generation();
  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x40, 0x28, 0x00, 0x00});

  manager.ActivateRuntimeContext(second_id, state);
  const auto second_loaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(second_loaded.ok()) << second_loaded.status();
  const uint64_t second_generation = manager.asset_generation();
  EXPECT_NE(second_generation, first_generation);

  manager.ActivateRuntimeContext(first_id, state);
  EXPECT_EQ(manager.asset_generation(), first_generation);
  EXPECT_EQ(manager.GetObjectInternal(0x31, 0).status(), missing.status());

  auto changed_state = state;
  changed_state.custom_file_map = {{0x31, {"track_LR.bin"}}};
  manager.ActivateRuntimeContext(first_id, changed_state);
  EXPECT_NE(manager.asset_generation(), first_generation);
  ASSERT_TRUE(manager.GetObjectInternal(0x31, 0).ok());

  WriteBinaryFile("track_LR.bin", {0x01, 0x00, 0x41, 0x28, 0x00, 0x00});
  manager.ReloadAll();
  const auto first_reloaded = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(first_reloaded.ok()) << first_reloaded.status();
  ASSERT_EQ((*first_reloaded)->tiles.size(), 1u);
  EXPECT_EQ((*first_reloaded)->tiles.front().tile_data, 0x2841);

  manager.ActivateRuntimeContext(second_id, state);
  EXPECT_EQ(manager.asset_generation(), second_generation);
  const auto second_cached = manager.GetObjectInternal(0x31, 0);
  ASSERT_TRUE(second_cached.ok()) << second_cached.status();
  EXPECT_EQ(second_cached->get(), second_loaded->get());
  ASSERT_EQ((*second_cached)->tiles.size(), 1u);
  EXPECT_EQ((*second_cached)->tiles.front().tile_data, 0x2840);
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
