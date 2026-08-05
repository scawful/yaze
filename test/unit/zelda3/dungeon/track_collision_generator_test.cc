#include "zelda3/dungeon/track_collision_generator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "core/features.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/room.h"

namespace yaze::zelda3 {
namespace {

class ScopedCustomObjectsFlag {
 public:
  explicit ScopedCustomObjectsFlag(bool enabled)
      : previous_(core::FeatureFlags::get().kEnableCustomObjects) {
    core::FeatureFlags::get().kEnableCustomObjects = enabled;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

  ~ScopedCustomObjectsFlag() {
    core::FeatureFlags::get().kEnableCustomObjects = previous_;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

 private:
  bool previous_;
};

int CountExpectedTilesInBounds(const RoomObject& obj) {
  const auto dims = DimensionService::Get().GetDimensions(obj);
  int count = 0;
  for (int dy = 0; dy < std::max(1, dims.height_tiles); ++dy) {
    for (int dx = 0; dx < std::max(1, dims.width_tiles); ++dx) {
      const int x = obj.x_ + dims.offset_x_tiles + dx;
      const int y = obj.y_ + dims.offset_y_tiles + dy;
      if (x >= 0 && x < 64 && y >= 0 && y < 64) {
        ++count;
      }
    }
  }
  return count;
}

CustomCollisionMap MakeCollisionMap(
    std::initializer_list<std::pair<int, uint8_t>> tiles) {
  CustomCollisionMap map;
  map.tiles.fill(0);
  for (const auto& [offset, value] : tiles) {
    map.tiles[static_cast<size_t>(offset)] = value;
  }
  map.has_data = tiles.size() != 0;
  return map;
}

uint32_t ReadCollisionPointerPc(const Rom& rom, int room_id) {
  const int pointer_offset = kCustomCollisionRoomPointers + (room_id * 3);
  const auto& data = rom.vector();
  const uint32_t snes_address = data[pointer_offset] |
                                (data[pointer_offset + 1] << 8) |
                                (data[pointer_offset + 2] << 16);
  return SnesToPc(snes_address);
}

void CopyCollisionPointer(Rom* rom, int source_room_id, int target_room_id) {
  const int source = kCustomCollisionRoomPointers + (source_room_id * 3);
  const int target = kCustomCollisionRoomPointers + (target_room_id * 3);
  for (int i = 0; i < 3; ++i) {
    rom->mutable_data()[target + i] = rom->vector()[source + i];
  }
}

void SetCollisionPointer(Rom* rom, int room_id, uint32_t pc_address) {
  const uint32_t snes_address = PcToSnes(pc_address);
  const int pointer_offset = kCustomCollisionRoomPointers + (room_id * 3);
  rom->mutable_data()[pointer_offset] = snes_address & 0xFF;
  rom->mutable_data()[pointer_offset + 1] = (snes_address >> 8) & 0xFF;
  rom->mutable_data()[pointer_offset + 2] = (snes_address >> 16) & 0xFF;
}

TEST(TrackCollisionGeneratorTest, UsesDimensionServiceNotLegacyWidthHeight) {
  Room room;
  RoomObject track_obj(0x01, 10, 10, 0, 0);

  // Legacy width_/height_ can be stale; generator must ignore them.
  track_obj.width_ = 16;
  track_obj.height_ = 16;
  room.AddTileObject(track_obj);

  const int expected = CountExpectedTilesInBounds(track_obj);
  ASSERT_GT(expected, 0);

  GeneratorOptions options;
  options.track_object_id = 0x01;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  EXPECT_EQ(result.tiles_generated, expected);
}

TEST(TrackCollisionGeneratorTest,
     CanonicalTrackSubtypesFallbackToTwoByTwoWithoutCustomGeometry) {
  ScopedCustomObjectsFlag custom_objects(/*enabled=*/false);
  constexpr std::array<uint8_t, 16> kTrackSubtypes = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

  const auto idx = [](int x, int y) {
    return static_cast<size_t>(y * 64 + x);
  };
  for (const uint8_t subtype : kTrackSubtypes) {
    SCOPED_TRACE(::testing::Message() << "subtype=" << +subtype);
    Room room;
    room.AddTileObject(RoomObject(0x31, 10, 10, subtype, 0));

    auto result_or = GenerateTrackCollision(&room, GeneratorOptions{});
    ASSERT_TRUE(result_or.ok()) << result_or.status();

    const auto& result = result_or.value();
    EXPECT_EQ(result.tiles_generated, 4);
    EXPECT_NE(result.collision_map.tiles[idx(10, 10)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(11, 10)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(10, 11)], 0);
    EXPECT_NE(result.collision_map.tiles[idx(11, 11)], 0);
  }
}

TEST(TrackCollisionGeneratorTest,
     ConfiguredSingleTileTrackIdPreservesOneByOneGeometry) {
  Room room;
  room.AddTileObject(RoomObject(0x35, 10, 10, 0, 0));

  GeneratorOptions options;
  options.track_object_id = 0x35;
  auto result_or = GenerateTrackCollision(&room, options);
  ASSERT_TRUE(result_or.ok()) << result_or.status();

  const auto& result = result_or.value();
  const auto idx = [](int x, int y) {
    return static_cast<size_t>(y * 64 + x);
  };
  EXPECT_EQ(result.tiles_generated, 1);
  EXPECT_NE(result.collision_map.tiles[idx(10, 10)], 0);
  EXPECT_EQ(result.collision_map.tiles[idx(11, 10)], 0);
  EXPECT_EQ(result.collision_map.tiles[idx(10, 11)], 0);
}

TEST(TrackCollisionGeneratorTest, LegacyWidthHeightDoesNotChangeOutput) {
  Room room_a;
  RoomObject obj_a(0x31, 18, 22, 0, 0);
  obj_a.width_ = 16;
  obj_a.height_ = 16;
  room_a.AddTileObject(obj_a);

  Room room_b;
  RoomObject obj_b(0x31, 18, 22, 0, 0);
  obj_b.width_ = 1;
  obj_b.height_ = 1;
  room_b.AddTileObject(obj_b);

  auto res_a_or = GenerateTrackCollision(&room_a, GeneratorOptions{});
  auto res_b_or = GenerateTrackCollision(&room_b, GeneratorOptions{});

  ASSERT_TRUE(res_a_or.ok()) << res_a_or.status();
  ASSERT_TRUE(res_b_or.ok()) << res_b_or.status();

  const auto& res_a = res_a_or.value();
  const auto& res_b = res_b_or.value();

  EXPECT_EQ(res_a.tiles_generated, res_b.tiles_generated);
  EXPECT_EQ(res_a.stop_count, res_b.stop_count);
  EXPECT_EQ(res_a.corner_count, res_b.corner_count);
  EXPECT_EQ(res_a.switch_count, res_b.switch_count);
  EXPECT_EQ(res_a.collision_map.tiles, res_b.collision_map.tiles);
}

TEST(TrackCollisionGeneratorTest,
     WriteReusesUniqueCurrentBlobWhenReplacementFits) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  const CustomCollisionMap initial =
      MakeCollisionMap({{0, 0xB0}, {1, 0xB1}, {2, 0xB2}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, initial).ok());
  const uint32_t initial_pointer = ReadCollisionPointerPc(rom, 0x25);
  ASSERT_EQ(initial_pointer,
            static_cast<uint32_t>(kCustomCollisionDataPosition));

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25), initial_pointer);
  auto loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  EXPECT_EQ(loaded_or->tiles, replacement.tiles);

  constexpr size_t kInitialEncodedSize = 2 + (3 * 3) + 2;
  constexpr size_t kReplacementEncodedSize = 2 + 3 + 2;
  const size_t append_candidate = initial_pointer + kInitialEncodedSize;
  EXPECT_TRUE(std::all_of(
      rom.vector().begin() + append_candidate,
      rom.vector().begin() + append_candidate + kReplacementEncodedSize,
      [](uint8_t value) { return value == 0; }));
}

TEST(TrackCollisionGeneratorTest,
     WriteAppendsWhenReplacementExceedsCurrentBlob) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  ASSERT_TRUE(
      WriteTrackCollision(&rom, 0x25, MakeCollisionMap({{0, 0xB0}})).ok());
  const uint32_t initial_pointer = ReadCollisionPointerPc(rom, 0x25);

  const CustomCollisionMap replacement =
      MakeCollisionMap({{0, 0xB0}, {1, 0xB1}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  constexpr uint32_t kInitialEncodedSize = 2 + 3 + 2;
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25),
            initial_pointer + kInitialEncodedSize);
  auto loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(loaded_or.ok()) << loaded_or.status();
  EXPECT_EQ(loaded_or->tiles, replacement.tiles);
}

TEST(TrackCollisionGeneratorTest, WriteDoesNotReuseBlobSharedByAnotherRoom) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());
  const CustomCollisionMap shared = MakeCollisionMap({{0, 0xB0}, {1, 0xB1}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, shared).ok());
  const uint32_t shared_pointer = ReadCollisionPointerPc(rom, 0x25);
  CopyCollisionPointer(&rom, 0x25, 0x26);

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  EXPECT_NE(ReadCollisionPointerPc(rom, 0x25), shared_pointer);
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x26), shared_pointer);
  auto shared_loaded_or = LoadCustomCollisionMap(&rom, 0x26);
  ASSERT_TRUE(shared_loaded_or.ok()) << shared_loaded_or.status();
  EXPECT_EQ(shared_loaded_or->tiles, shared.tiles);
  auto replacement_loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(replacement_loaded_or.ok()) << replacement_loaded_or.status();
  EXPECT_EQ(replacement_loaded_or->tiles, replacement.tiles);
}

TEST(TrackCollisionGeneratorTest,
     WriteDoesNotReuseBlobContainingAnotherRoomPointer) {
  Rom rom;
  ASSERT_TRUE(
      rom.LoadFromData(std::vector<uint8_t>(kCustomCollisionDataEnd, 0x00))
          .ok());

  // Room 0x25 uses a rectangle whose four payload bytes also form a valid
  // empty single-tile blob for room 0x26. Both streams decode, but their
  // physical spans overlap and therefore cannot be edited in place safely.
  const uint32_t outer_start = kCustomCollisionDataPosition;
  ASSERT_TRUE(rom.WriteVector(outer_start, {0x00, 0x00, 0x04, 0x01, 0xF0, 0xF0,
                                            0xFF, 0xFF, 0xFF, 0xFF})
                  .ok());
  SetCollisionPointer(&rom, 0x25, outer_start);
  SetCollisionPointer(&rom, 0x26, outer_start + 4);

  const CustomCollisionMap replacement = MakeCollisionMap({{65, 0xB7}});
  ASSERT_TRUE(WriteTrackCollision(&rom, 0x25, replacement).ok());

  constexpr uint32_t kOuterEncodedSize = 10;
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x25), outer_start + kOuterEncodedSize);
  EXPECT_EQ(ReadCollisionPointerPc(rom, 0x26), outer_start + 4);
  auto interior_loaded_or = LoadCustomCollisionMap(&rom, 0x26);
  ASSERT_TRUE(interior_loaded_or.ok()) << interior_loaded_or.status();
  EXPECT_TRUE(std::all_of(interior_loaded_or->tiles.begin(),
                          interior_loaded_or->tiles.end(),
                          [](uint8_t value) { return value == 0; }));
  auto replacement_loaded_or = LoadCustomCollisionMap(&rom, 0x25);
  ASSERT_TRUE(replacement_loaded_or.ok()) << replacement_loaded_or.status();
  EXPECT_EQ(replacement_loaded_or->tiles, replacement.tiles);
}

}  // namespace
}  // namespace yaze::zelda3
