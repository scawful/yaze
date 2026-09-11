// Synthetic replay tests for ObjectDrawer draw routines.
//
// IMPORTANT — test tier / what this file does NOT prove:
//   These tests inject sequential dummy tile IDs (0, 1, 2, …) and assert the
//   drawer emits tiles at the coordinates/index slots the *current* C++
//   implementation expects. They are regression guards against accidental drift
//   in our own code, NOT independent proof of 1:1 visual parity with ALTTP.
//
// Stronger parity evidence lives elsewhere:
//   - room_object_rom_parity_test.cc — parser bytes + drawer placement using
//     real ROM tile words (skips without YAZE_TEST_ROM_VANILLA).
//   - dungeon_room_regression_fixtures_test.cc — Mesen2 screenshot ROI
//     baselines for rooms 0x007, 0x012, 0x031, 0x065, and 0x076 (independent
//     emulator truth).
//   - z3ed dungeon-object-validate — bounds vs dimension table across all IDs.
//
// See docs/internal/plans/dungeon-object-rendering-parity-2026-04.md (Phase D/E).

#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/moving_wall_semantics.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {
namespace {

struct ScopedCustomObjectsFlag {
  bool prev = false;
  explicit ScopedCustomObjectsFlag(bool enabled) {
    prev = core::FeatureFlags::get().kEnableCustomObjects;
    core::FeatureFlags::get().kEnableCustomObjects = enabled;
  }
  ~ScopedCustomObjectsFlag() {
    core::FeatureFlags::get().kEnableCustomObjects = prev;
  }
};

class FakeDungeonState : public DungeonState {
 public:
  int open_lock_room_id = -1;
  std::set<std::pair<int, int>> open_chest_slots;
  std::set<std::pair<int, int>> open_big_key_lock_slots;
  mutable std::vector<std::pair<int, int>> chest_queries;
  mutable std::vector<std::pair<int, int>> big_key_lock_queries;
  int water_face_active_room_id = -1;
  int bombed_floor_room_id = -1;
  int cleared_rupee_floor_room_id = -1;
  bool dam_floodgate_open = false;
  bool big_chest_open = false;
  bool wall_moved = false;
  bool door_switch_active = false;

  bool IsChestOpen(int room_id, int chest_index) const override {
    chest_queries.emplace_back(room_id, chest_index);
    return open_chest_slots.contains({room_id, chest_index});
  }
  bool IsBigChestOpen() const override { return big_chest_open; }

  bool IsDoorOpen(int room_id, int door_index) const override {
    if (door_index != 0) {
      return false;
    }
    return room_id == open_lock_room_id;
  }
  bool IsDoorSwitchActive(int /*room_id*/) const override {
    return door_switch_active;
  }
  bool IsBigKeyLockOpen(int room_id, int room_event_index) const override {
    big_key_lock_queries.emplace_back(room_id, room_event_index);
    return open_big_key_lock_slots.contains({room_id, room_event_index});
  }
  bool IsWaterFaceActive(int room_id) const override {
    return room_id == water_face_active_room_id;
  }
  bool IsDamFloodgateOpen(int /*room_id*/) const override {
    return dam_floodgate_open;
  }

  bool IsWallMoved(int /*room_id*/) const override { return wall_moved; }
  bool IsFloorBombable(int room_id) const override {
    return room_id == bombed_floor_room_id;
  }
  bool IsRupeeFloorCleared(int room_id) const override {
    return room_id == cleared_rupee_floor_room_id;
  }

  bool IsCrystalSwitchBlue() const override { return true; }
};

struct SnapshotTileWrite {
  int x = 0;
  int y = 0;
  uint16_t tile_id = 0;
};

std::vector<gfx::TileInfo> MakeSequentialTiles(int count,
                                               uint16_t start_tile_id = 0,
                                               uint8_t palette = 2) {
  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(count);
  for (int i = 0; i < count; ++i) {
    tiles.push_back(gfx::TileInfo(static_cast<uint16_t>(start_tile_id + i),
                                  palette, false, false, false));
  }
  return tiles;
}

std::vector<ObjectDrawer::TileTrace> ReplayObjectTrace(
    int16_t object_id, int x, int y, uint8_t size, RoomObject::LayerType layer,
    const std::vector<gfx::TileInfo>& tiles,
    const DungeonState* state = nullptr,
    const std::vector<std::pair<int, uint16_t>>& rom_words = {},
    int room_id = 0) {
  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (const auto& [address, word] : rom_words) {
    if (address < 0 || address + 1 >= static_cast<int>(dummy_rom.size())) {
      continue;
    }
    dummy_rom[address] = static_cast<uint8_t>(word & 0xFF);
    dummy_rom[address + 1] = static_cast<uint8_t>(word >> 8);
  }
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, room_id, /*room_gfx_buffer=*/nullptr);
  RoomObject obj(object_id, x, y, size, static_cast<int>(layer));
  obj.tiles_loaded_ = true;
  obj.tiles_ = tiles;

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  EXPECT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group, state).ok());
  return trace;
}

std::vector<ObjectDrawer::TileTrace> FilterTraceByLayer(
    const std::vector<ObjectDrawer::TileTrace>& trace,
    RoomObject::LayerType layer) {
  std::vector<ObjectDrawer::TileTrace> filtered;
  for (const auto& t : trace) {
    if (t.layer == static_cast<uint8_t>(layer)) {
      filtered.push_back(t);
    }
  }
  return filtered;
}

std::vector<SnapshotTileWrite> MakeColumnMajorSnapshot(int x, int y, int width,
                                                       int height,
                                                       uint16_t start_tile_id) {
  std::vector<SnapshotTileWrite> out;
  out.reserve(width * height);
  uint16_t tile_id = start_tile_id;
  for (int xx = 0; xx < width; ++xx) {
    for (int yy = 0; yy < height; ++yy) {
      out.push_back({x + xx, y + yy, tile_id++});
    }
  }
  return out;
}

std::vector<SnapshotTileWrite> MakeRowMajorSnapshot(int x, int y, int width,
                                                    int height,
                                                    uint16_t start_tile_id) {
  std::vector<SnapshotTileWrite> out;
  out.reserve(width * height);
  uint16_t tile_id = start_tile_id;
  for (int yy = 0; yy < height; ++yy) {
    for (int xx = 0; xx < width; ++xx) {
      out.push_back({x + xx, y + yy, tile_id++});
    }
  }
  return out;
}

std::vector<SnapshotTileWrite> MakeBigGrayRockSnapshot(int x, int y,
                                                       uint16_t start_tile_id) {
  // USDASM draws four 2x2 quadrants, producing this coordinate layout:
  //   0  2  4  6
  //   1  3  5  7
  //   8 10 12 14
  //   9 11 13 15
  constexpr std::array<std::pair<int, int>, 16> kOffsets = {{
      {0, 0},
      {0, 1},
      {1, 0},
      {1, 1},
      {2, 0},
      {2, 1},
      {3, 0},
      {3, 1},
      {0, 2},
      {0, 3},
      {1, 2},
      {1, 3},
      {2, 2},
      {2, 3},
      {3, 2},
      {3, 3},
  }};

  std::vector<SnapshotTileWrite> out;
  out.reserve(kOffsets.size());
  for (size_t i = 0; i < kOffsets.size(); ++i) {
    out.push_back({x + kOffsets[i].first, y + kOffsets[i].second,
                   static_cast<uint16_t>(start_tile_id + i)});
  }
  return out;
}

std::vector<SnapshotTileWrite> MakeMany32x32BlockGridSnapshot(
    int x, int y, int block_columns, int block_rows, uint16_t start_tile_id) {
  std::vector<SnapshotTileWrite> out;
  out.reserve(block_columns * block_rows * 16);
  for (int block_y = 0; block_y < block_rows; ++block_y) {
    for (int block_x = 0; block_x < block_columns; ++block_x) {
      for (int repeated_rows = 0; repeated_rows < 2; ++repeated_rows) {
        for (int row = 0; row < 2; ++row) {
          for (int column = 0; column < 4; ++column) {
            out.push_back(
                {x + block_x * 4 + column,
                 y + block_y * 4 + repeated_rows * 2 + row,
                 static_cast<uint16_t>(start_tile_id + row * 4 + column)});
          }
        }
      }
    }
  }
  return out;
}

void ExpectTraceMatchesSnapshot(
    const std::vector<ObjectDrawer::TileTrace>& trace,
    const std::vector<SnapshotTileWrite>& expected) {
  ASSERT_EQ(trace.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(trace[i].x_tile, expected[i].x) << "trace idx=" << i;
    EXPECT_EQ(trace[i].y_tile, expected[i].y) << "trace idx=" << i;
    EXPECT_EQ(trace[i].tile_id, expected[i].tile_id) << "trace idx=" << i;
  }
}

void ExpectTraceBounds(const std::vector<ObjectDrawer::TileTrace>& trace,
                       int min_x, int min_y, int max_x, int max_y) {
  ASSERT_FALSE(trace.empty());
  int observed_min_x = trace.front().x_tile;
  int observed_min_y = trace.front().y_tile;
  int observed_max_x = trace.front().x_tile;
  int observed_max_y = trace.front().y_tile;
  for (const auto& tile : trace) {
    observed_min_x = std::min(observed_min_x, static_cast<int>(tile.x_tile));
    observed_min_y = std::min(observed_min_y, static_cast<int>(tile.y_tile));
    observed_max_x = std::max(observed_max_x, static_cast<int>(tile.x_tile));
    observed_max_y = std::max(observed_max_y, static_cast<int>(tile.y_tile));
  }
  EXPECT_EQ(observed_min_x, min_x);
  EXPECT_EQ(observed_min_y, min_y);
  EXPECT_EQ(observed_max_x, max_x);
  EXPECT_EQ(observed_max_y, max_y);
}

bool TraceHasWriteAt(const std::vector<ObjectDrawer::TileTrace>& trace, int x,
                     int y) {
  for (const auto& tile : trace) {
    if (tile.x_tile == x && tile.y_tile == y) {
      return true;
    }
  }
  return false;
}

uint16_t LastTileIdAt(const std::vector<ObjectDrawer::TileTrace>& trace, int x,
                      int y) {
  bool found = false;
  uint16_t tile_id = 0;
  for (const auto& tile : trace) {
    if (tile.x_tile == x && tile.y_tile == y) {
      tile_id = tile.tile_id;
      found = true;
    }
  }
  EXPECT_TRUE(found) << "No trace write at (" << x << ", " << y << ")";
  return tile_id;
}

std::vector<uint8_t> MakeSingleTileCustomObjectBinary(int rel_x, int rel_y,
                                                      uint16_t tile_word) {
  std::vector<uint8_t> data;
  // Advance full rows first (stride 0x80 bytes per row in custom object
  // buffer space), then advance columns (2 bytes per tile), then emit one tile.
  for (int row = 0; row < rel_y; ++row) {
    data.push_back(0x00);
    data.push_back(0x80);
  }
  if (rel_x > 0) {
    data.push_back(0x00);
    data.push_back(static_cast<uint8_t>(rel_x * 2));
  }
  data.push_back(0x01);
  data.push_back(0x00);
  data.push_back(static_cast<uint8_t>(tile_word & 0xFF));
  data.push_back(static_cast<uint8_t>((tile_word >> 8) & 0xFF));
  data.push_back(0x00);
  data.push_back(0x00);
  return data;
}

void WriteBinaryFile(const std::filesystem::path& path,
                     const std::vector<uint8_t>& data) {
  std::ofstream out(path, std::ios::binary);
  ASSERT_TRUE(out.good());
  out.write(reinterpret_cast<const char*>(data.data()), data.size());
  ASSERT_TRUE(out.good());
}

TEST(ObjectDrawerRegistryReplayTest,
     SomariaPathPiecesDrawOneTileAtTheObjectAnchor) {
  ScopedCustomObjectsFlag disable_custom(false);

  // USDASM RoomDraw_SomariaLine performs one load/store and returns. Objects
  // 0x203-0x20C, 0x20E, and 0x20F select distinct one-word tile data entries;
  // their size bits must not be interpreted as a repeated line length.
  constexpr int16_t kObjectIds[] = {0xF83, 0xF84, 0xF85, 0xF86, 0xF87, 0xF88,
                                    0xF89, 0xF8A, 0xF8B, 0xF8C, 0xF8E, 0xF8F};
  constexpr int kX = 20;
  constexpr int kY = 20;
  constexpr uint16_t kTileId = 0x0123;

  for (int16_t object_id : kObjectIds) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);
    auto trace = ReplayObjectTrace(object_id, kX, kY, /*size=*/0x0C,
                                   RoomObject::LayerType::BG1,
                                   MakeSequentialTiles(/*count=*/1, kTileId));
    const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
    ExpectTraceMatchesSnapshot(bg1, {{kX, kY, kTileId}});
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     EnabledStarSwitchAndLitTorchDrawAnchoredTwoByTwo) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 12;
  constexpr int kY = 18;
  constexpr uint16_t kTileId = 0x0240;
  const std::vector<SnapshotTileWrite> expected = {
      {kX, kY, kTileId},
      {kX, kY + 1, kTileId + 1},
      {kX + 1, kY, kTileId + 2},
      {kX + 1, kY + 1, kTileId + 3},
  };

  for (int16_t object_id : {int16_t{0x11F}, int16_t{0x120}}) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);
    const auto trace = ReplayObjectTrace(
        object_id, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
        MakeSequentialTiles(/*count=*/4, kTileId));
    const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

    ExpectTraceMatchesSnapshot(bg1, expected);
    EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     MarioPortraitDrawsAnchoredFourByTwoRowMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 14;
  constexpr int kY = 20;
  constexpr uint16_t kTileId = 0x0260;
  const auto trace = ReplayObjectTrace(
      /*object_id=*/0x12A, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/8, kTileId));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  ExpectTraceMatchesSnapshot(
      bg1, MakeRowMajorSnapshot(kX, kY, /*width=*/4, /*height=*/2, kTileId));
  EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     MagicBatAltarDrawsFixedEightBySevenColumnMajorOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0200;

  for (uint8_t size : {uint8_t{0}, uint8_t{0xFF}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace = ReplayObjectTrace(
          /*object_id=*/0x013F, kX, kY, size, layer,
          MakeSequentialTiles(/*count=*/56, /*start_tile_id=*/kFirstTile));
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
      const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

      ExpectTraceMatchesSnapshot(
          selected, MakeColumnMajorSnapshot(kX, kY, /*width=*/8, /*height=*/7,
                                            /*start_tile_id=*/kFirstTile));
      EXPECT_TRUE(other.empty());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     VitreousGooDamageDrawsFixedFiveByTwoStampGridOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 8;
  constexpr uint16_t kFirstTile = 0x0300;
  FakeDungeonState inactive_state;

  const auto* routine = DrawRoutineRegistry::Get().GetRoutineInfo(
      DrawRoutineIds::kVitreousGooDamage);
  ASSERT_NE(routine, nullptr);
  EXPECT_EQ(routine->base_width, 20);
  EXPECT_EQ(routine->base_height, 8);
  EXPECT_EQ(routine->min_tiles, 8);
  EXPECT_FALSE(routine->draws_to_both_bgs);

  const auto expected = MakeMany32x32BlockGridSnapshot(
      kX, kY, /*block_columns=*/5, /*block_rows=*/2, kFirstTile);
  for (uint8_t size : {uint8_t{0}, uint8_t{0x0E}, uint8_t{0xFF}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      for (const bool use_state : {false, true}) {
        SCOPED_TRACE(::testing::Message()
                     << "size=" << static_cast<int>(size) << " layer="
                     << static_cast<int>(layer) << " use_state=" << use_state);

        const auto trace = ReplayObjectTrace(
            /*object_id=*/0x0FFB, kX, kY, size, layer,
            MakeSequentialTiles(/*count=*/8, /*start_tile_id=*/kFirstTile),
            use_state ? &inactive_state : nullptr);
        const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
        const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
        const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
        const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

        ExpectTraceMatchesSnapshot(selected, expected);
        EXPECT_TRUE(other.empty());
      }
    }
  }
}

std::array<uint8_t, 0x10000> MakeOpaqueDoorGfx() {
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);
  return gfx;
}

void InitializeEmptyDoorBuffer(gfx::BackgroundBuffer& bg) {
  bg.EnsureBitmapInitialized();
  bg.bitmap().Fill(255);
  bg.ClearPriorityBuffer();
  bg.ClearCoverageBuffer();
}

void WriteDoorObjectDataWords(std::vector<uint8_t>& rom_data, int object_offset,
                              uint16_t start_word, int word_count) {
  constexpr int kRoomDrawObjectDataBase = 0x1B52;
  const int base = kRoomDrawObjectDataBase + object_offset;
  for (int i = 0; i < word_count; ++i) {
    const uint16_t word = static_cast<uint16_t>(start_word + i);
    rom_data[base + i * 2] = static_cast<uint8_t>(word & 0xFF);
    rom_data[base + i * 2 + 1] = static_cast<uint8_t>(word >> 8);
  }
}

bool TileHasCoverage(const gfx::BackgroundBuffer& bg, int tile_x, int tile_y) {
  const auto& coverage = bg.coverage_data();
  if (coverage.empty()) {
    return false;
  }
  const int pixel_x = tile_x * 8;
  const int pixel_y = tile_y * 8;
  const int width = bg.bitmap().width();
  const int index = pixel_y * width + pixel_x;
  return index >= 0 && index < static_cast<int>(coverage.size()) &&
         coverage[index] != 0;
}

struct TileRect {
  int x;
  int y;
  int width;
  int height;
};

bool PixelIsInAnyTileRect(int pixel_x, int pixel_y,
                          std::initializer_list<TileRect> rects) {
  for (const auto& rect : rects) {
    if (pixel_x >= rect.x * 8 && pixel_x < (rect.x + rect.width) * 8 &&
        pixel_y >= rect.y * 8 && pixel_y < (rect.y + rect.height) * 8) {
      return true;
    }
  }
  return false;
}

void ExpectOnlyCoverageRects(const gfx::BackgroundBuffer& bg,
                             std::initializer_list<TileRect> rects) {
  const auto& coverage = bg.coverage_data();
  ASSERT_EQ(coverage.size(),
            static_cast<size_t>(bg.bitmap().width() * bg.bitmap().height()));
  for (int y = 0; y < bg.bitmap().height(); ++y) {
    for (int x = 0; x < bg.bitmap().width(); ++x) {
      const bool expected = PixelIsInAnyTileRect(x, y, rects);
      const bool actual = coverage[y * bg.bitmap().width() + x] != 0;
      if (actual != expected) {
        ADD_FAILURE() << "coverage mismatch at pixel (" << x << "," << y
                      << ") expected=" << expected << " actual=" << actual;
        return;
      }
    }
  }
}

void ExpectOnlyCoverageRect(const gfx::BackgroundBuffer& bg, int start_tile_x,
                            int start_tile_y, int width_tiles,
                            int height_tiles) {
  const auto& coverage = bg.coverage_data();
  ASSERT_EQ(coverage.size(),
            static_cast<size_t>(bg.bitmap().width() * bg.bitmap().height()));
  const int start_x = start_tile_x * 8;
  const int start_y = start_tile_y * 8;
  const int end_x = (start_tile_x + width_tiles) * 8;
  const int end_y = (start_tile_y + height_tiles) * 8;
  for (int y = 0; y < bg.bitmap().height(); ++y) {
    for (int x = 0; x < bg.bitmap().width(); ++x) {
      const bool expected =
          x >= start_x && x < end_x && y >= start_y && y < end_y;
      const bool actual = coverage[y * bg.bitmap().width() + x] != 0;
      if (actual != expected) {
        ADD_FAILURE() << "coverage mismatch at pixel (" << x << "," << y
                      << ") expected=" << expected << " actual=" << actual;
        return;
      }
    }
  }
}

void ExpectOnlyPriorityValueRect(const gfx::BackgroundBuffer& bg,
                                 int start_tile_x, int start_tile_y,
                                 int width_tiles, int height_tiles,
                                 uint8_t priority_value) {
  const auto& priority = bg.priority_data();
  ASSERT_EQ(priority.size(),
            static_cast<size_t>(bg.bitmap().width() * bg.bitmap().height()));
  const int start_x = start_tile_x * 8;
  const int start_y = start_tile_y * 8;
  const int end_x = (start_tile_x + width_tiles) * 8;
  const int end_y = (start_tile_y + height_tiles) * 8;
  for (int y = 0; y < bg.bitmap().height(); ++y) {
    for (int x = 0; x < bg.bitmap().width(); ++x) {
      const bool promoted =
          x >= start_x && x < end_x && y >= start_y && y < end_y;
      const uint8_t expected = promoted ? priority_value : 0xFF;
      const uint8_t actual = priority[y * bg.bitmap().width() + x];
      if (actual != expected) {
        ADD_FAILURE() << "priority mismatch at pixel (" << x << "," << y
                      << ") expected=" << static_cast<int>(expected)
                      << " actual=" << static_cast<int>(actual);
        return;
      }
    }
  }
}

void ExpectOnlyPriorityRect(const gfx::BackgroundBuffer& bg, int start_tile_x,
                            int start_tile_y, int width_tiles,
                            int height_tiles) {
  ExpectOnlyPriorityValueRect(bg, start_tile_x, start_tile_y, width_tiles,
                              height_tiles, /*priority_value=*/1);
}

void ExpectPriorityRectSet(const gfx::BackgroundBuffer& bg, int start_tile_x,
                           int start_tile_y, int width_tiles,
                           int height_tiles) {
  for (int y = start_tile_y * 8; y < (start_tile_y + height_tiles) * 8; ++y) {
    for (int x = start_tile_x * 8; x < (start_tile_x + width_tiles) * 8; ++x) {
      ASSERT_EQ(bg.GetPriorityAt(x, y), 1)
          << "priority not promoted at pixel (" << x << "," << y << ")";
    }
  }
}

void ExpectBitmapFilledWith(const gfx::BackgroundBuffer& bg, uint8_t value) {
  const auto& pixels = bg.bitmap().vector();
  for (size_t i = 0; i < pixels.size(); ++i) {
    if (pixels[i] != value) {
      ADD_FAILURE() << "bitmap mismatch at pixel index " << i
                    << " expected=" << static_cast<int>(value)
                    << " actual=" << static_cast<int>(pixels[i]);
      return;
    }
  }
}

void WriteWord(std::vector<uint8_t>& rom_data, int addr, uint16_t value) {
  rom_data[addr] = static_cast<uint8_t>(value & 0xFF);
  rom_data[addr + 1] = static_cast<uint8_t>(value >> 8);
}

TEST(ObjectDrawerRegistryReplayTest,
     FloorCopyObjectsUseRoomHeaderPatternsInsteadOfObjectPayloads) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr uint8_t kFloor1 = 6;
  constexpr uint8_t kFloor2 = 11;
  constexpr uint16_t kFloor1Tile = 0x120;
  constexpr uint16_t kFloor2Tile = 0x1A0;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  const auto write_floor_pattern = [&](uint8_t pattern,
                                       uint16_t first_tile_id) {
    const int offset = static_cast<int>(pattern) << 4;
    for (int index = 0; index < 4; ++index) {
      WriteWord(dummy_rom, kRoomObjectTileAddress + offset + index * 2,
                gfx::TileInfoToWord(
                    gfx::TileInfo(static_cast<uint16_t>(first_tile_id + index),
                                  2, false, false, false)));
      WriteWord(dummy_rom, kRoomObjectTileAddressFloor + offset + index * 2,
                gfx::TileInfoToWord(gfx::TileInfo(
                    static_cast<uint16_t>(first_tile_id + index + 4), 2, false,
                    false, false)));
    }
  };
  write_floor_pattern(kFloor1, kFloor1Tile);
  write_floor_pattern(kFloor2, kFloor2Tile);

  Rom rom;
  rom.LoadFromData(dummy_rom);
  ObjectDrawer drawer(&rom, /*room_id=*/0);
  drawer.SetRoomFloorGraphics(kFloor1, kFloor2);
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  for (const auto [object_id, first_tile_id] :
       {std::pair<int16_t, uint16_t>{0x00C4, kFloor1Tile},
        std::pair<int16_t, uint16_t>{0x00DB, kFloor2Tile}}) {
    RoomObject object(object_id, /*x=*/8, /*y=*/12, /*size=*/0, /*layer=*/0);
    object.tiles_loaded_ = true;
    object.tiles_ = MakeSequentialTiles(8, /*start_tile_id=*/0x300);

    std::vector<ObjectDrawer::TileTrace> trace;
    drawer.SetTraceCollector(&trace, /*trace_only=*/true);
    ASSERT_TRUE(drawer.DrawObject(object, bg1, bg2, palette_group).ok());
    drawer.ClearTraceCollector();

    ASSERT_EQ(trace.size(), 16U);
    for (size_t index = 0; index < trace.size(); ++index) {
      EXPECT_EQ(trace[index].tile_id,
                first_tile_id + static_cast<uint16_t>(index % 8));
      EXPECT_LT(trace[index].tile_id, 0x300);
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest, SuperSquareRendersToBitmap) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  // Provide a simple 8BPP "room gfx" buffer: every pixel is non-zero so any
  // drawn tile must modify the destination bitmap.
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  ASSERT_TRUE(bg1.bitmap().is_active());
  ASSERT_TRUE(bg2.bitmap().is_active());
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  // Object 0xC0 maps to routine 56 (Draw4x4BlocksIn4x4SuperSquare).
  RoomObject obj(0x00C0, /*x=*/1, /*y=*/1, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));

  gfx::PaletteGroup palette_group;
  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());

  const int px = obj.x_ * 8;
  const int py = obj.y_ * 8;
  const int idx = py * bg1.bitmap().width() + px;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(bg1.bitmap().size()));

  // If the routine wrote only to the tile buffer (SetTileAt) and not to the
  // bitmap-backed buffers, this would remain 255.
  EXPECT_NE(bg1.bitmap().data()[idx], 255);
}

TEST(ObjectDrawerRegistryReplayTest,
     ClosedExplodingWallDoesNotDrawGenericNorthDoorFootprint) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  FakeDungeonState state;
  ObjectDrawer::DoorDef door{
      .type = DoorType::ExplodingWall,
      .direction = DoorDirection::North,
      .position = 0,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &state);

  EXPECT_FALSE(TileHasCoverage(bg1, 14, 0));
  EXPECT_FALSE(TileHasCoverage(bg1, 17, 2));
  EXPECT_FALSE(TileHasCoverage(bg2, 14, 0));
}

TEST(ObjectDrawerRegistryReplayTest,
     ClosedNorthCurtainDoorUsesFourByFourFootprint) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/0x078A,
                           /*start_word=*/0x0400, /*word_count=*/16);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  FakeDungeonState state;
  ObjectDrawer::DoorDef door{
      .type = DoorType::CurtainDoor,
      .direction = DoorDirection::North,
      .position = 0,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &state);

  EXPECT_TRUE(TileHasCoverage(bg1, 14, 4));
  EXPECT_TRUE(TileHasCoverage(bg1, 17, 4));
  EXPECT_TRUE(TileHasCoverage(bg1, 14, 7));
  EXPECT_TRUE(TileHasCoverage(bg1, 17, 7));
  EXPECT_FALSE(TileHasCoverage(bg1, 18, 4));
  EXPECT_FALSE(TileHasCoverage(bg1, 14, 8));
  EXPECT_FALSE(TileHasCoverage(bg2, 14, 0));
}

TEST(ObjectDrawerRegistryReplayTest,
     OpenNorthCurtainDoorUsesReplacementFourByFourFootprint) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorwayReplacementDoorGfxBase = 0x1A02;
  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kCurtainDoorType = 0x32;
  constexpr int kOpenCurtainReplacementType = 0x56;
  constexpr int kOpenCurtainObjectOffset = 0x0800;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kDoorwayReplacementDoorGfxBase + kCurtainDoorType,
            kOpenCurtainReplacementType);
  WriteWord(dummy_rom, kDoorGfxNorthTableBase + kCurtainDoorType, 0xFFFF);
  WriteWord(dummy_rom, kDoorGfxNorthTableBase + kOpenCurtainReplacementType,
            kOpenCurtainObjectOffset);
  WriteDoorObjectDataWords(dummy_rom,
                           /*object_offset=*/kOpenCurtainObjectOffset,
                           /*start_word=*/0x0500, /*word_count=*/16);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  FakeDungeonState state;
  state.open_lock_room_id = 0x42;

  ObjectDrawer::DoorDef door{
      .type = DoorType::CurtainDoor,
      .direction = DoorDirection::North,
      .position = 0,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &state);

  EXPECT_TRUE(TileHasCoverage(bg1, 14, 4));
  EXPECT_TRUE(TileHasCoverage(bg1, 17, 4));
  EXPECT_TRUE(TileHasCoverage(bg1, 14, 7));
  EXPECT_TRUE(TileHasCoverage(bg1, 17, 7));
  EXPECT_FALSE(TileHasCoverage(bg1, 18, 4));
  EXPECT_FALSE(TileHasCoverage(bg1, 14, 8));
  EXPECT_FALSE(TileHasCoverage(bg2, 14, 0));
}

TEST(ObjectDrawerRegistryReplayTest,
     OpenGenericDoorUsesRomReplacementWithoutChangingWriterFamily) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorwayReplacementDoorGfxBase = 0x1A02;
  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kClosedObjectOffset = 0x0C00;
  constexpr int kOpenObjectOffset = 0x0C20;
  constexpr uint16_t kClosedFirstWord = 0x0800;
  constexpr uint16_t kOpenFirstWord = 0x0900;
  constexpr DoorType kOpenReplacementType = DoorType::UnusableBombedDoor;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(
      dummy_rom,
      kDoorwayReplacementDoorGfxBase + static_cast<int>(DoorType::BombableDoor),
      static_cast<int>(kOpenReplacementType));
  WriteWord(dummy_rom,
            kDoorGfxWestTableBase + static_cast<int>(DoorType::BombableDoor),
            kClosedObjectOffset);
  WriteWord(dummy_rom,
            kDoorGfxWestTableBase + static_cast<int>(kOpenReplacementType),
            kOpenObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, kClosedObjectOffset, kClosedFirstWord,
                           /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, kOpenObjectOffset, kOpenFirstWord,
                           /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  InitializeEmptyDoorBuffer(bg1);
  InitializeEmptyDoorBuffer(bg2);
  FakeDungeonState open_state;
  open_state.open_lock_room_id = 0x42;

  ObjectDrawer::DoorDef door{
      .type = DoorType::BombableDoor,
      .direction = DoorDirection::West,
      .position = 0,
  };
  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &open_state);

  const auto [tile_x, tile_y] = door.GetTileCoords();
  ExpectOnlyCoverageRect(bg1, tile_x, tile_y, /*width=*/3, /*height=*/4);
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  EXPECT_EQ(bg1.GetTileAt(tile_x, tile_y), kOpenFirstWord);
  EXPECT_NE(bg1.GetTileAt(tile_x, tile_y), kClosedFirstWord);
}

TEST(ObjectDrawerRegistryReplayTest,
     GenericCurtainAndWaterfallFinalTypesSuppressRaster) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kWaterfallObjectOffset = 0x0D00;
  constexpr int kCurtainObjectOffset = 0x0D20;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom,
            kDoorGfxSouthTableBase + static_cast<int>(DoorType::WaterfallDoor),
            kWaterfallObjectOffset);
  WriteWord(dummy_rom,
            kDoorGfxWestTableBase + static_cast<int>(DoorType::CurtainDoor),
            kCurtainObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, kWaterfallObjectOffset,
                           /*start_word=*/0x0A00, /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, kCurtainObjectOffset,
                           /*start_word=*/0x0B00, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  for (const auto& [type, direction] :
       std::array<std::pair<DoorType, DoorDirection>, 2>{
           {{DoorType::WaterfallDoor, DoorDirection::South},
            {DoorType::CurtainDoor, DoorDirection::West}}}) {
    SCOPED_TRACE(::testing::Message()
                 << "type=" << static_cast<int>(type)
                 << " direction=" << static_cast<int>(direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    InitializeEmptyDoorBuffer(bg1);
    InitializeEmptyDoorBuffer(bg2);

    ObjectDrawer::DoorDef door{
        .type = type,
        .direction = direction,
        .position = 0,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

    ExpectOnlyCoverageRect(bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     ActiveShutterControllerKeepsOpenFlaggedShutterClosed) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorwayReplacementDoorGfxBase = 0x1A02;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kClosedObjectOffset = 0x0D40;
  constexpr int kOpenObjectOffset = 0x0D60;
  constexpr uint16_t kClosedFirstWord = 0x0C00;
  constexpr uint16_t kOpenFirstWord = 0x0D00;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom,
            kDoorwayReplacementDoorGfxBase +
                static_cast<int>(DoorType::DoubleSidedShutter),
            static_cast<int>(DoorType::UnusableNormalDoor50));
  WriteWord(
      dummy_rom,
      kDoorGfxSouthTableBase + static_cast<int>(DoorType::DoubleSidedShutter),
      kClosedObjectOffset);
  WriteWord(
      dummy_rom,
      kDoorGfxSouthTableBase + static_cast<int>(DoorType::UnusableNormalDoor50),
      kOpenObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, kClosedObjectOffset, kClosedFirstWord,
                           /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, kOpenObjectOffset, kOpenFirstWord,
                           /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  InitializeEmptyDoorBuffer(bg1);
  InitializeEmptyDoorBuffer(bg2);
  FakeDungeonState state;
  state.open_lock_room_id = 0x42;
  state.door_switch_active = true;

  ObjectDrawer::DoorDef door{
      .type = DoorType::DoubleSidedShutter,
      .direction = DoorDirection::South,
      .position = 0,
  };
  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &state);

  const auto [tile_x, tile_y] = door.GetTileCoords();
  ExpectOnlyCoverageRect(bg1, tile_x, tile_y, /*width=*/4, /*height=*/3);
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  EXPECT_EQ(bg1.GetTileAt(tile_x, tile_y), kClosedFirstWord);
  EXPECT_NE(bg1.GetTileAt(tile_x, tile_y), kOpenFirstWord);
}

TEST(ObjectDrawerRegistryReplayTest,
     OpenExplodingWallUsesUsdasmSpecialPlacement) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kExplodingWallTilemapPositionBase = 0x19DE;
  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kExplodingWallReplacementType = 0x54;
  constexpr int kSouthSegmentObjectOffset = 0x0900;
  constexpr int kNorthSegmentObjectOffset = 0x0920;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kExplodingWallTilemapPositionBase, 0x0D8A);
  WriteWord(dummy_rom, kDoorGfxSouthTableBase + kExplodingWallReplacementType,
            kSouthSegmentObjectOffset);
  WriteWord(dummy_rom, kDoorGfxNorthTableBase + kExplodingWallReplacementType,
            kNorthSegmentObjectOffset);
  WriteDoorObjectDataWords(dummy_rom,
                           /*object_offset=*/kSouthSegmentObjectOffset,
                           /*start_word=*/0x0600, /*word_count=*/13);
  WriteDoorObjectDataWords(dummy_rom,
                           /*object_offset=*/kNorthSegmentObjectOffset,
                           /*start_word=*/0x0700, /*word_count=*/13);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  FakeDungeonState state;
  state.open_lock_room_id = 0x42;

  ObjectDrawer::DoorDef door{
      .type = DoorType::ExplodingWall,
      .direction = DoorDirection::North,
      .position = 0,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &state);

  EXPECT_TRUE(TileHasCoverage(bg1, 5, 23));
  EXPECT_TRUE(TileHasCoverage(bg1, 7, 23));
  EXPECT_TRUE(TileHasCoverage(bg1, 22, 28));
  EXPECT_TRUE(TileHasCoverage(bg1, 5, 29));
  EXPECT_TRUE(TileHasCoverage(bg1, 22, 34));
  EXPECT_FALSE(TileHasCoverage(bg1, 14, 0));
  EXPECT_FALSE(TileHasCoverage(bg2, 5, 23));
}

TEST(ObjectDrawerRegistryReplayTest, NorthMiddleDoorsRenderBothSidesOfTheSeam) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kNorthObjectOffset = 0x0A00;
  constexpr int kSouthObjectOffset = 0x0A20;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kDoorGfxNorthTableBase, kNorthObjectOffset);
  WriteWord(dummy_rom, kDoorGfxSouthTableBase, kSouthObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kNorthObjectOffset,
                           /*start_word=*/0x0800, /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kSouthObjectOffset,
                           /*start_word=*/0x0900, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  ObjectDrawer::DoorDef door{
      .type = DoorType::NormalDoor,
      .direction = DoorDirection::North,
      .position = 6,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  ExpectOnlyCoverageRects(bg1, {{14, 27, 4, 3},    // South-facing counterpart.
                                {14, 36, 4, 3}});  // North-facing current half.
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  EXPECT_EQ(bg1.GetTileAt(14, 27), 0x0900);
  EXPECT_EQ(bg1.GetTileAt(14, 36), 0x0800);
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthDoorsRenderOneTileBelowUsdasmTableAnchor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kSouthObjectOffset = 0x0A30;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kDoorGfxSouthTableBase, kSouthObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kSouthObjectOffset,
                           /*start_word=*/0x0980, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  ObjectDrawer::DoorDef door{
      .type = DoorType::NormalDoor,
      .direction = DoorDirection::South,
      .position = 6,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  EXPECT_FALSE(TileHasCoverage(bg1, 14, 58));
  EXPECT_TRUE(TileHasCoverage(bg1, 14, 59));
  EXPECT_TRUE(TileHasCoverage(bg1, 17, 61));
  EXPECT_FALSE(TileHasCoverage(bg1, 14, 62));
}

TEST(ObjectDrawerRegistryReplayTest,
     EastDoorsRenderOneTileRightOfUsdasmTableAnchor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxEastTableBase = 0x4EC6;
  constexpr int kEastObjectOffset = 0x0A30;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kDoorGfxEastTableBase, kEastObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kEastObjectOffset,
                           /*start_word=*/0x0980, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  ObjectDrawer::DoorDef door{
      .type = DoorType::NormalDoor,
      .direction = DoorDirection::East,
      .position = 9,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  EXPECT_FALSE(TileHasCoverage(bg1, 55, 15));
  EXPECT_TRUE(TileHasCoverage(bg1, 56, 15));
  EXPECT_TRUE(TileHasCoverage(bg1, 58, 18));
  EXPECT_FALSE(TileHasCoverage(bg1, 59, 15));
}

TEST(ObjectDrawerRegistryReplayTest,
     DoorControlMarkersDoNotOverwritePhysicalDoorArt) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kNormalLowerObjectOffset = 0x0A80;
  constexpr uint16_t kFirstNormalLowerWord = 0x0840;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(
      dummy_rom,
      kDoorGfxNorthTableBase + static_cast<int>(DoorType::NormalDoorLower),
      kNormalLowerObjectOffset);
  WriteDoorObjectDataWords(dummy_rom,
                           /*object_offset=*/kNormalLowerObjectOffset,
                           /*start_word=*/kFirstNormalLowerWord,
                           /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x76, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  ObjectDrawer::DoorDef physical_door{
      .type = DoorType::NormalDoorLower,
      .direction = DoorDirection::North,
      .position = 3,
  };
  drawer.DrawDoor(physical_door, /*door_index=*/0, bg1, bg2, nullptr);
  ASSERT_EQ(bg1.GetTileAt(14, 7), kFirstNormalLowerWord);

  ObjectDrawer::DoorDef marker{
      .type = DoorType::LayerSwapMarker,
      .direction = DoorDirection::North,
      .position = 3,
  };
  drawer.DrawDoor(marker, /*door_index=*/1, bg1, bg2, nullptr);

  EXPECT_EQ(bg1.GetTileAt(14, 7), kFirstNormalLowerWord);
}

TEST(ObjectDrawerRegistryReplayTest,
     DoorControlMarkersHaveDirectionAwareZeroRasterSemantics) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  auto marker_writes_pixels = [&](DoorType type, DoorDirection direction) {
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    bg1.EnsureBitmapInitialized();
    bg2.EnsureBitmapInitialized();
    bg1.bitmap().Fill(255);
    bg2.bitmap().Fill(255);
    bg1.ClearCoverageBuffer();
    bg2.ClearCoverageBuffer();

    ObjectDrawer::DoorDef marker{
        .type = type,
        .direction = direction,
        .position = 0,
    };
    drawer.DrawDoor(marker, /*door_index=*/0, bg1, bg2, nullptr);
    const auto [x, y] = marker.GetTileCoords();
    return TileHasCoverage(bg1, x, y) || TileHasCoverage(bg2, x, y);
  };

  for (DoorDirection direction : {DoorDirection::North, DoorDirection::South,
                                  DoorDirection::West, DoorDirection::East}) {
    EXPECT_FALSE(marker_writes_pixels(DoorType::DungeonSwapMarker, direction));
    EXPECT_FALSE(marker_writes_pixels(DoorType::LayerSwapMarker, direction));
  }
  EXPECT_FALSE(
      marker_writes_pixels(DoorType::ExitMarker, DoorDirection::North));
  EXPECT_FALSE(
      marker_writes_pixels(DoorType::ExitMarker, DoorDirection::South));
  EXPECT_TRUE(marker_writes_pixels(DoorType::ExitMarker, DoorDirection::West));
  EXPECT_TRUE(marker_writes_pixels(DoorType::ExitMarker, DoorDirection::East));
}

TEST(ObjectDrawerRegistryReplayTest,
     HighRangeDoorsSplitTilesBetweenUpperAndLowerBackgrounds) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kDoorGfxEastTableBase = 0x4EC6;
  constexpr int kHighRangeType = 0x40;
  constexpr int kObjectOffset = 0x0AC0;
  constexpr uint16_t kFirstWord = 0x0880;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (int table_base : {kDoorGfxNorthTableBase, kDoorGfxSouthTableBase,
                         kDoorGfxWestTableBase, kDoorGfxEastTableBase}) {
    WriteWord(dummy_rom, table_base + kHighRangeType, kObjectOffset);
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/kFirstWord, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  auto verify_direction = [&](DoorDirection direction, int start_x,
                              int start_y) {
    SCOPED_TRACE(static_cast<int>(direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    bg1.EnsureBitmapInitialized();
    bg2.EnsureBitmapInitialized();
    bg1.bitmap().Fill(255);
    bg2.bitmap().Fill(255);
    bg1.ClearCoverageBuffer();
    bg2.ClearCoverageBuffer();

    ObjectDrawer::DoorDef door{
        .type = DoorType::NormalDoorOneSidedShutter,
        .direction = direction,
        .position = 0,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

    const auto dims = door.GetDimensions();
    int tile_idx = 0;
    for (int dx = 0; dx < dims.width_tiles; ++dx) {
      for (int dy = 0; dy < dims.height_tiles; ++dy) {
        bool upper = false;
        switch (direction) {
          case DoorDirection::North:
            upper = dy == 0;
            break;
          case DoorDirection::South:
            upper = dy == dims.height_tiles - 1;
            break;
          case DoorDirection::West:
            upper = dx == 0;
            break;
          case DoorDirection::East:
            upper = dx == dims.width_tiles - 1;
            break;
        }
        const int x = start_x + dx;
        const int y = start_y + dy;
        EXPECT_EQ(TileHasCoverage(bg1, x, y), upper);
        EXPECT_EQ(TileHasCoverage(bg2, x, y), !upper);
        const auto& owner = upper ? bg1 : bg2;
        EXPECT_EQ(owner.GetTileAt(x, y),
                  static_cast<uint16_t>(kFirstWord + tile_idx));
        ++tile_idx;
      }
    }
  };

  verify_direction(DoorDirection::North, /*start_x=*/14, /*start_y=*/4);
  verify_direction(DoorDirection::South, /*start_x=*/14, /*start_y=*/27);
  verify_direction(DoorDirection::West, /*start_x=*/2, /*start_y=*/15);
  verify_direction(DoorDirection::East, /*start_x=*/27, /*start_y=*/15);
}

TEST(ObjectDrawerRegistryReplayTest,
     NormalLowerDoorsPromoteFixedUsdasmRegionsAndStillRaster) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kDoorGfxEastTableBase = 0x4EC6;
  constexpr int kObjectOffset = 0x0AE0;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (int table_base : {kDoorGfxNorthTableBase, kDoorGfxSouthTableBase,
                         kDoorGfxWestTableBase, kDoorGfxEastTableBase}) {
    WriteWord(dummy_rom,
              table_base + static_cast<int>(DoorType::NormalDoorLower),
              kObjectOffset);
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/0x0840, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  struct PriorityCase {
    DoorDirection direction;
    uint8_t position;
    int render_x;
    int render_y;
    int priority_x;
    int priority_y;
    int priority_width;
    int priority_height;
  };
  const std::array<PriorityCase, 4> cases = {{
      {DoorDirection::North, 3, 14, 7, 14, 0, 4, 7},
      {DoorDirection::South, 3, 14, 24, 14, 27, 4, 7},
      {DoorDirection::West, 3, 5, 15, 0, 15, 5, 4},
      {DoorDirection::East, 3, 24, 15, 27, 15, 5, 4},
  }};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(static_cast<int>(test_case.direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = DoorType::NormalDoorLower,
        .direction = test_case.direction,
        .position = test_case.position,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    const auto dims = door.GetDimensions();
    ExpectOnlyCoverageRect(bg1, test_case.render_x, test_case.render_y,
                           dims.width_tiles, dims.height_tiles);
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(layout_bg1, test_case.priority_x,
                           test_case.priority_y, test_case.priority_width,
                           test_case.priority_height);
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
    ExpectPriorityRectSet(bg1, test_case.priority_x, test_case.priority_y,
                          test_case.priority_width, test_case.priority_height);
    ExpectOnlyCoverageRect(layout_bg1, 0, 0, 0, 0);
    ExpectBitmapFilledWith(layout_bg1, 255);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     NormalLowerMiddleDoorsUseSeparatedCounterpartsAndPromoteBothWalls) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr std::array<int, 4> kDoorTableBases = {
      0x4D9E,  // North
      0x4E06,  // South
      0x4E66,  // West
      0x4EC6,  // East
  };
  constexpr int kObjectOffset = 0x0B80;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (int table_base : kDoorTableBases) {
    WriteWord(dummy_rom,
              table_base + static_cast<int>(DoorType::NormalDoorLower),
              kObjectOffset);
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/0x0940, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  auto verify = [&](DoorDirection direction) {
    SCOPED_TRACE(static_cast<int>(direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = DoorType::NormalDoorLower,
        .direction = direction,
        .position = 6,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    if (direction == DoorDirection::North) {
      ExpectOnlyCoverageRects(bg1, {{14, 27, 4, 3}, {14, 36, 4, 3}});
      ExpectOnlyPriorityRect(layout_bg1, 14, 30, 4, 9);
    } else {
      ExpectOnlyCoverageRects(bg1, {{27, 15, 3, 4}, {34, 15, 3, 4}});
      ExpectOnlyPriorityRect(layout_bg1, 30, 15, 7, 4);
    }
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
  };

  verify(DoorDirection::North);
  verify(DoorDirection::West);
}

TEST(ObjectDrawerRegistryReplayTest,
     Type06PromotesBothUpperOwnersWithoutRasterizing) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  struct PriorityCase {
    DoorDirection direction;
    uint8_t position;
    int priority_x;
    int priority_y;
    int priority_width;
    int priority_height;
  };
  const std::array<PriorityCase, 4> cases = {{
      {DoorDirection::North, 3, 14, 0, 4, 8},
      {DoorDirection::South, 3, 14, 25, 4, 7},
      {DoorDirection::West, 3, 0, 15, 6, 4},
      {DoorDirection::East, 3, 25, 15, 7, 4},
  }};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(static_cast<int>(test_case.direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = DoorType::UnusedCaveExit,
        .direction = test_case.direction,
        .position = test_case.position,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    ExpectOnlyCoverageRect(bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(bg1, test_case.priority_x, test_case.priority_y,
                           test_case.priority_width, test_case.priority_height);
    ExpectOnlyPriorityRect(layout_bg1, test_case.priority_x,
                           test_case.priority_y, test_case.priority_width,
                           test_case.priority_height);
    ExpectOnlyPriorityRect(bg2, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
    for (const auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      ExpectBitmapFilledWith(*buffer, 255);
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     HighRangeDoorsPromoteDirectionalUsdasmRegions) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kDoorGfxEastTableBase = 0x4EC6;
  constexpr int kObjectOffset = 0x0B00;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (int table_base : {kDoorGfxNorthTableBase, kDoorGfxSouthTableBase,
                         kDoorGfxWestTableBase, kDoorGfxEastTableBase}) {
    for (int type : {0x40, 0x46, 0x66}) {
      WriteWord(dummy_rom, table_base + type, kObjectOffset);
    }
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/0x0880, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  struct PriorityCase {
    DoorType type;
    DoorDirection direction;
    int priority_x;
    int priority_y;
    int priority_width;
    int priority_height;
  };
  const std::array<PriorityCase, 6> cases = {{
      {DoorType::NormalDoorOneSidedShutter, DoorDirection::North, 14, 0, 4, 4},
      {DoorType::NormalDoorOneSidedShutter, DoorDirection::South, 14, 30, 4, 2},
      {DoorType::NormalDoorOneSidedShutter, DoorDirection::West, 0, 15, 2, 4},
      {DoorType::NormalDoorOneSidedShutter, DoorDirection::East, 30, 15, 2, 4},
      // North explicit-room doors raster across BG1/BG2 but skip priority.
      {DoorType::ExplicitRoomDoor, DoorDirection::North, 0, 0, 0, 0},
      // Verify the upper inclusive endpoint of the supported high range.
      {static_cast<DoorType>(0x66), DoorDirection::North, 14, 0, 4, 4},
  }};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "type=" << static_cast<int>(test_case.type)
                 << " direction=" << static_cast<int>(test_case.direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = test_case.type,
        .direction = test_case.direction,
        .position = 0,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    ExpectOnlyPriorityRect(layout_bg1, test_case.priority_x,
                           test_case.priority_y, test_case.priority_width,
                           test_case.priority_height);
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
    EXPECT_TRUE(TileHasCoverage(bg1, door.GetTileCoords().first,
                                door.GetTileCoords().second) ||
                TileHasCoverage(bg2, door.GetTileCoords().first,
                                door.GetTileCoords().second));
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     HighRangeMiddleDoorsSeparateCounterpartRasterAndPriority) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr std::array<int, 4> kDoorTableBases = {
      0x4D9E,  // North
      0x4E06,  // South
      0x4E66,  // West
      0x4EC6,  // East
  };
  constexpr int kObjectOffset = 0x0BA0;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (int table_base : kDoorTableBases) {
    WriteWord(
        dummy_rom,
        table_base + static_cast<int>(DoorType::NormalDoorOneSidedShutter),
        kObjectOffset);
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/0x0980, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  auto verify = [&](DoorDirection direction) {
    SCOPED_TRACE(static_cast<int>(direction));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = DoorType::NormalDoorOneSidedShutter,
        .direction = direction,
        .position = 6,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    if (direction == DoorDirection::North) {
      ExpectOnlyCoverageRects(bg1, {{14, 29, 4, 1}, {14, 36, 4, 1}});
      ExpectOnlyCoverageRects(bg2, {{14, 27, 4, 2}, {14, 37, 4, 2}});
      ExpectOnlyPriorityRect(layout_bg1, 14, 30, 4, 6);
    } else {
      ExpectOnlyCoverageRects(bg1, {{29, 15, 1, 4}, {34, 15, 1, 4}});
      ExpectOnlyCoverageRects(bg2, {{27, 15, 2, 4}, {35, 15, 2, 4}});
      ExpectOnlyPriorityRect(layout_bg1, 30, 15, 4, 4);
    }
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
  };

  verify(DoorDirection::North);
  verify(DoorDirection::West);
}

TEST(ObjectDrawerRegistryReplayTest,
     NorthLowerKeyStairsRasterOnlyToLowerBackground) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kObjectOffset = 0x0B20;
  constexpr uint16_t kFirstWord = 0x08C0;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (DoorType type :
       {DoorType::SmallKeyStairsUpLower, DoorType::SmallKeyStairsDownLower}) {
    WriteWord(dummy_rom, kDoorGfxNorthTableBase + static_cast<int>(type),
              kObjectOffset);
  }
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kObjectOffset,
                           /*start_word=*/kFirstWord, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  for (DoorType type :
       {DoorType::SmallKeyStairsUpLower, DoorType::SmallKeyStairsDownLower}) {
    SCOPED_TRACE(static_cast<int>(type));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = type,
        .direction = DoorDirection::North,
        .position = 6,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    ExpectOnlyCoverageRect(bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(bg2, 14, 36, 4, 3);
    EXPECT_EQ(bg2.GetTileAt(14, 36), kFirstWord);
    ExpectOnlyPriorityRect(layout_bg1, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);

    gfx::BackgroundBuffer open_bg1(512, 512);
    gfx::BackgroundBuffer open_bg2(512, 512);
    InitializeEmptyDoorBuffer(open_bg1);
    InitializeEmptyDoorBuffer(open_bg2);
    FakeDungeonState open_state;
    open_state.open_lock_room_id = 0x42;
    drawer.DrawDoor(door, /*door_index=*/0, open_bg1, open_bg2, &open_state);
    ExpectOnlyCoverageRect(open_bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(open_bg2, 0, 0, 0, 0);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     OpenNorthUpperKeyStairsSuppressValidReplacementArt) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorwayReplacementDoorGfxBase = 0x1A02;
  constexpr int kDoorGfxNorthTableBase = 0x4D9E;
  constexpr int kReplacementObjectOffset = 0x0B30;
  constexpr uint16_t kReplacementType =
      static_cast<uint16_t>(DoorType::NormalDoor);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  for (DoorType type :
       {DoorType::SmallKeyStairsUp, DoorType::SmallKeyStairsDown}) {
    WriteWord(dummy_rom,
              kDoorwayReplacementDoorGfxBase + static_cast<int>(type),
              kReplacementType);
  }
  WriteWord(dummy_rom, kDoorGfxNorthTableBase + kReplacementType,
            kReplacementObjectOffset);
  WriteDoorObjectDataWords(dummy_rom,
                           /*object_offset=*/kReplacementObjectOffset,
                           /*start_word=*/0x08E0, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  FakeDungeonState open_state;
  open_state.open_lock_room_id = 0x42;

  for (DoorType type :
       {DoorType::SmallKeyStairsUp, DoorType::SmallKeyStairsDown}) {
    SCOPED_TRACE(static_cast<int>(type));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    InitializeEmptyDoorBuffer(bg1);
    InitializeEmptyDoorBuffer(bg2);

    ObjectDrawer::DoorDef door{
        .type = type,
        .direction = DoorDirection::North,
        .position = 0,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &open_state);

    ExpectOnlyCoverageRect(bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthBigKeyDoorUsesNormalDoorTableEntryWhenClosed) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxSouthTableBase = 0x4E06;
  constexpr int kNormalObjectOffset = 0x0B40;
  constexpr int kBigKeyObjectOffset = 0x0B60;
  constexpr uint16_t kNormalFirstWord = 0x0900;
  constexpr uint16_t kBigKeyFirstWord = 0x0A00;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom,
            kDoorGfxSouthTableBase + static_cast<int>(DoorType::NormalDoor),
            kNormalObjectOffset);
  WriteWord(dummy_rom,
            kDoorGfxSouthTableBase + static_cast<int>(DoorType::BigKeyDoor),
            kBigKeyObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kNormalObjectOffset,
                           /*start_word=*/kNormalFirstWord,
                           /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kBigKeyObjectOffset,
                           /*start_word=*/kBigKeyFirstWord,
                           /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  InitializeEmptyDoorBuffer(bg1);
  InitializeEmptyDoorBuffer(bg2);

  ObjectDrawer::DoorDef door{
      .type = DoorType::BigKeyDoor,
      .direction = DoorDirection::South,
      .position = 0,
  };
  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  ExpectOnlyCoverageRect(bg1, 14, 27, 4, 3);
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  EXPECT_EQ(bg1.GetTileAt(14, 27), kNormalFirstWord);
  EXPECT_NE(bg1.GetTileAt(14, 27), kBigKeyFirstWord);
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthFancyExitUsesUsdasmTenByEightRowMajorStamp) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kFancyDungeonExitObjectOffset = 0x2656;
  constexpr uint16_t kFirstWord = 0x0800;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, kFancyDungeonExitObjectOffset, kFirstWord,
                           /*word_count=*/80);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x05, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  InitializeEmptyDoorBuffer(bg1);
  InitializeEmptyDoorBuffer(bg2);

  ObjectDrawer::DoorDef fancy_exit{
      .type = DoorType::FancyDungeonExit,
      .direction = DoorDirection::South,
      .position = 8,
  };
  drawer.DrawDoor(fancy_exit, /*door_index=*/0, bg1, bg2, nullptr);

  // Room $005 uses raw South anchor (46,58). RoomDraw_SomeBigDecors subtracts
  // three columns/four rows and consumes ten words per destination row.
  ExpectOnlyCoverageRect(bg1, 43, 54, 10, 8);
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  ExpectOnlyPriorityValueRect(bg1, 43, 54, 10, 8,
                              /*priority_value=*/0);
  for (int dy = 0; dy < 8; ++dy) {
    for (int dx = 0; dx < 10; ++dx) {
      EXPECT_EQ(bg1.GetTileAt(43 + dx, 54 + dy),
                static_cast<uint16_t>(kFirstWord + dy * 10 + dx));
    }
  }

  ObjectDrawer::DoorDef exit_marker{
      .type = DoorType::ExitMarker,
      .direction = DoorDirection::South,
      .position = 8,
  };
  drawer.DrawDoor(exit_marker, /*door_index=*/1, bg1, bg2, nullptr);

  // The paired marker must preserve both the outer frame and center bottom
  // row of the physical exit.
  ExpectOnlyCoverageRect(bg1, 43, 54, 10, 8);
  EXPECT_EQ(bg1.GetTileAt(43, 54), kFirstWord);
  for (int dx = 3; dx < 7; ++dx) {
    EXPECT_EQ(bg1.GetTileAt(43 + dx, 61), kFirstWord + 70 + dx);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthLowerFancyExitCopiesOnlyPriorityBottomRowToUpperBackground) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kFancyDungeonExitObjectOffset = 0x2656;
  constexpr uint16_t kFirstWord = 0x0900;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, kFancyDungeonExitObjectOffset, kFirstWord,
                           /*word_count=*/80);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  InitializeEmptyDoorBuffer(bg1);
  InitializeEmptyDoorBuffer(bg2);

  ObjectDrawer::DoorDef door{
      .type = DoorType::FancyDungeonExitLower,
      .direction = DoorDirection::South,
      .position = 8,
  };
  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  ExpectOnlyCoverageRect(bg2, 43, 54, 10, 8);
  ExpectOnlyCoverageRect(bg1, 43, 61, 10, 1);
  ExpectOnlyPriorityValueRect(bg2, 43, 54, 10, 8,
                              /*priority_value=*/0);
  ExpectOnlyPriorityRect(bg1, 43, 61, 10, 1);
  for (int dy = 0; dy < 8; ++dy) {
    for (int dx = 0; dx < 10; ++dx) {
      EXPECT_EQ(bg2.GetTileAt(43 + dx, 54 + dy),
                static_cast<uint16_t>(kFirstWord + dy * 10 + dx));
    }
  }
  for (int dx = 0; dx < 10; ++dx) {
    EXPECT_EQ(bg1.GetTileAt(43 + dx, 61),
              static_cast<uint16_t>((kFirstWord + 70 + dx) | 0x2000));
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthExitLowerUsesRawAnchorLowerLayerAndPriorityUpperRowCopy) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kCaveExitLightObjectOffset = 0x26F6;
  constexpr uint16_t kFirstWord = 0x0800;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, kCaveExitLightObjectOffset, kFirstWord,
                           /*word_count=*/16);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  gfx::BackgroundBuffer layout_bg2(512, 512);
  for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
    InitializeEmptyDoorBuffer(*buffer);
  }

  ObjectDrawer::DoorDef door{
      .type = DoorType::ExitLower,
      .direction = DoorDirection::South,
      .position = 0,
  };
  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                  &layout_bg2);

  ExpectOnlyCoverageRect(bg2, 14, 26, 4, 4);
  ExpectOnlyCoverageRect(bg1, 14, 29, 4, 1);
  for (int dx = 0; dx < 4; ++dx) {
    for (int dy = 0; dy < 4; ++dy) {
      EXPECT_EQ(bg2.GetTileAt(14 + dx, 26 + dy),
                static_cast<uint16_t>(kFirstWord + dx * 4 + dy));
    }
    EXPECT_EQ(bg1.GetTileAt(14 + dx, 29),
              static_cast<uint16_t>((kFirstWord + dx * 4 + 3) | 0x2000));
  }
  ExpectOnlyPriorityRect(bg1, 14, 29, 4, 1);
  ExpectOnlyPriorityRect(layout_bg1, 0, 0, 0, 0);
  ExpectOnlyPriorityRect(layout_bg2, 14, 30, 4, 7);
  ExpectPriorityRectSet(bg2, 14, 30, 4, 7);
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthCaveExitAndLitLowerExitUseRawFourByFourAnchor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kCaveExitLightObjectOffset = 0x26F6;
  constexpr uint16_t kFirstWord = 0x0880;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, kCaveExitLightObjectOffset, kFirstWord,
                           /*word_count=*/16);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  for (DoorType type : {DoorType::CaveExit, DoorType::LitCaveExitLower}) {
    SCOPED_TRACE(static_cast<int>(type));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&bg1, &bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer::DoorDef door{
        .type = type,
        .direction = DoorDirection::South,
        .position = 0,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr, &layout_bg1,
                    &layout_bg2);

    ExpectOnlyCoverageRect(bg1, 14, 26, 4, 4);
    ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    for (int dx = 0; dx < 4; ++dx) {
      for (int dy = 0; dy < 4; ++dy) {
        EXPECT_EQ(bg1.GetTileAt(14 + dx, 26 + dy),
                  static_cast<uint16_t>(kFirstWord + dx * 4 + dy));
      }
    }
    if (type == DoorType::LitCaveExitLower) {
      ExpectOnlyPriorityRect(layout_bg1, 14, 30, 4, 7);
      ExpectPriorityRectSet(bg1, 14, 30, 4, 7);
    } else {
      ExpectOnlyPriorityRect(layout_bg1, 0, 0, 0, 0);
    }
    ExpectOnlyPriorityRect(layout_bg2, 0, 0, 0, 0);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SouthSpecialExitStampsAreInvariantWhenDoorStateIsOpen) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kFancyDungeonExitObjectOffset = 0x2656;
  constexpr int kCaveExitLightObjectOffset = 0x26F6;
  constexpr uint16_t kFirstWord = 0x0800;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteDoorObjectDataWords(dummy_rom, kFancyDungeonExitObjectOffset, kFirstWord,
                           /*word_count=*/80);
  WriteDoorObjectDataWords(dummy_rom, kCaveExitLightObjectOffset, kFirstWord,
                           /*word_count=*/16);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());
  FakeDungeonState open_state;
  open_state.open_lock_room_id = 0x42;

  for (DoorType type : {
           DoorType::FancyDungeonExit,
           DoorType::FancyDungeonExitLower,
           DoorType::ExitLower,
           DoorType::CaveExit,
           DoorType::LitCaveExitLower,
       }) {
    SCOPED_TRACE(static_cast<int>(type));
    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    InitializeEmptyDoorBuffer(bg1);
    InitializeEmptyDoorBuffer(bg2);

    ObjectDrawer::DoorDef door{
        .type = type,
        .direction = DoorDirection::South,
        .position = 8,
    };
    drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, &open_state);

    if (type == DoorType::FancyDungeonExit) {
      ExpectOnlyCoverageRect(bg1, 43, 54, 10, 8);
      ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    } else if (type == DoorType::FancyDungeonExitLower) {
      ExpectOnlyCoverageRect(bg1, 43, 61, 10, 1);
      ExpectOnlyCoverageRect(bg2, 43, 54, 10, 8);
    } else if (type == DoorType::ExitLower) {
      ExpectOnlyCoverageRect(bg1, 46, 61, 4, 1);
      ExpectOnlyCoverageRect(bg2, 46, 58, 4, 4);
    } else {
      ExpectOnlyCoverageRect(bg1, 46, 58, 4, 4);
      ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest, WestMiddleDoorsRenderBothSidesOfTheSeam) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kDoorGfxWestTableBase = 0x4E66;
  constexpr int kDoorGfxEastTableBase = 0x4EC6;
  constexpr int kWestObjectOffset = 0x0A40;
  constexpr int kEastObjectOffset = 0x0A60;

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  WriteWord(dummy_rom, kDoorGfxWestTableBase, kWestObjectOffset);
  WriteWord(dummy_rom, kDoorGfxEastTableBase, kEastObjectOffset);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kWestObjectOffset,
                           /*start_word=*/0x0A00, /*word_count=*/12);
  WriteDoorObjectDataWords(dummy_rom, /*object_offset=*/kEastObjectOffset,
                           /*start_word=*/0x0B00, /*word_count=*/12);
  rom.LoadFromData(dummy_rom);

  auto gfx = MakeOpaqueDoorGfx();
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, gfx.data());

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);
  bg1.ClearCoverageBuffer();
  bg2.ClearCoverageBuffer();

  ObjectDrawer::DoorDef door{
      .type = DoorType::NormalDoor,
      .direction = DoorDirection::West,
      .position = 6,
  };

  drawer.DrawDoor(door, /*door_index=*/0, bg1, bg2, nullptr);

  ExpectOnlyCoverageRects(bg1, {{27, 15, 3, 4},    // East-facing counterpart.
                                {34, 15, 3, 4}});  // West-facing current half.
  ExpectOnlyCoverageRect(bg2, 0, 0, 0, 0);
  EXPECT_EQ(bg1.GetTileAt(27, 15), 0x0B00);
  EXPECT_EQ(bg1.GetTileAt(34, 15), 0x0A00);
}

TEST(ObjectDrawerRegistryReplayTest,
     WallTorchesUseUsdasmFourByTwoStampAndTwelveTileStride) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0x55 maps to RoomDraw_RightwardsDecor4x2spaced8_1to16.
  RoomObject obj(0x0055, /*x=*/10, /*y=*/20, /*size=*/1, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ExpectTraceMatchesSnapshot(trace, {
                                        {10, 20, 0},
                                        {11, 20, 1},
                                        {12, 20, 2},
                                        {13, 20, 3},
                                        {10, 21, 4},
                                        {11, 21, 5},
                                        {12, 21, 6},
                                        {13, 21, 7},
                                        {22, 20, 0},
                                        {23, 20, 1},
                                        {24, 20, 2},
                                        {25, 20, 3},
                                        {22, 21, 4},
                                        {23, 21, 5},
                                        {24, 21, 6},
                                        {25, 21, 7},
                                    });
}

TEST(ObjectDrawerRegistryReplayTest,
     CustomObjectPreservesRelativeOffsetsFromBinary) {
  ScopedCustomObjectsFlag enable_custom(true);

  auto& manager = CustomObjectManager::Get();
  const std::string previous_base = manager.GetBasePath();
  std::filesystem::path temp_dir =
      std::filesystem::temp_directory_path() / "yaze_custom_draw_offset_test";
  struct RestoreCustomObjectManagerState {
    CustomObjectManager& manager;
    std::string previous_base;
    std::filesystem::path temp_dir;
    ~RestoreCustomObjectManagerState() {
      manager.Initialize(previous_base);
      manager.ClearObjectFileMap();
      std::filesystem::remove_all(temp_dir);
    }
  } restore{manager, previous_base, temp_dir};

  manager.ClearObjectFileMap();
  std::filesystem::remove_all(temp_dir);
  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));

  // First segment advances by 0x82 bytes with no tiles (x+1, y+1), second
  // segment emits one tile. The renderer should preserve that +1,+1 offset.
  const std::vector<uint8_t> binary = {
      0x00, 0x82,  // Header 1: count=0, jump=0x82
      0x01, 0x00,  // Header 2: count=1, jump=0
      0x42, 0x00,  // Tile word (id=0x42)
      0x00, 0x00,  // Terminator
  };
  {
    std::ofstream out(temp_dir / "track_LR.bin", std::ios::binary);
    ASSERT_TRUE(out.good());
    out.write(reinterpret_cast<const char*>(binary.data()), binary.size());
    ASSERT_TRUE(out.good());
  }
  manager.Initialize(temp_dir.string());

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);
  RoomObject obj(0x0031, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 1u);
  EXPECT_EQ(trace[0].x_tile, 11);
  EXPECT_EQ(trace[0].y_tile, 21);
}

TEST(ObjectDrawerRegistryReplayTest,
     TransparentTileClearsExistingPixelsAndMarksCoverage) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  // Fully transparent tile graphics.
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(0);

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();

  // Seed destination with non-transparent data to ensure transparent source
  // pixels actively clear.
  bg1.bitmap().Fill(42);
  bg1.ClearCoverageBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  RoomObject obj(0x00C0, /*x=*/2, /*y=*/2, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/7, false, false, true));

  gfx::PaletteGroup palette_group;
  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());

  const int x = obj.x_ * 8;
  const int y = obj.y_ * 8;
  const int idx = y * bg1.bitmap().width() + x;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(bg1.bitmap().size()));

  // Transparent tile writes must clear the destination footprint.
  EXPECT_EQ(bg1.bitmap().data()[idx], 255);

  // Coverage must mark the write so compositor can distinguish "clear" from
  // "no write".
  ASSERT_LT(idx, static_cast<int>(bg1.coverage_data().size()));
  EXPECT_EQ(bg1.coverage_data()[idx], 1);

  // Priority for transparent pixels should be cleared.
  ASSERT_LT(idx, static_cast<int>(bg1.priority_data().size()));
  EXPECT_EQ(bg1.priority_data()[idx], 0xFF);

  // Outside the written tile footprint should remain untouched.
  EXPECT_EQ(bg1.bitmap().data()[0], 42);
}

TEST(ObjectDrawerRegistryReplayTest,
     SuperSquare4x4FloorUsesUsdasmRowMajorRows) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0xC8 maps to routine 58 (Draw4x4FloorIn4x4SuperSquare).
  // USDASM RoomDraw_A_Many32x32Blocks writes RoomDrawObjectData offsets
  // +0,+2,+4,+6 through row-0 tilemap pointers, then +8,+10,+12,+14
  // through row-1 pointers; the second pass repeats those rows at y+2/y+3.
  RoomObject obj(0x00C8, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ExpectTraceMatchesSnapshot(trace, {
                                        {10, 20, 0},
                                        {11, 20, 1},
                                        {12, 20, 2},
                                        {13, 20, 3},
                                        {10, 21, 4},
                                        {11, 21, 5},
                                        {12, 21, 6},
                                        {13, 21, 7},
                                        {10, 22, 0},
                                        {11, 22, 1},
                                        {12, 22, 2},
                                        {13, 22, 3},
                                        {10, 23, 4},
                                        {11, 23, 5},
                                        {12, 23, 6},
                                        {13, 23, 7},
                                    });
}

TEST(ObjectDrawerRegistryReplayTest,
     WeirdCornerBottomBothBGMatchesUsdasm3x4ColumnMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // USDASM: RoomDraw_WeirdCornerBottom_BothBG at $01:9854
  // Shape is 3 columns x 4 rows, column-major, written to both BGs.
  RoomObject obj(0x0110, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 12; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 24u);  // 12 tiles × BG1+BG2

  std::vector<ObjectDrawer::TileTrace> bg1_trace;
  for (const auto& t : trace) {
    if (t.layer == static_cast<uint8_t>(RoomObject::LayerType::BG1)) {
      bg1_trace.push_back(t);
    }
  }
  ASSERT_EQ(bg1_trace.size(), 12u);

  struct Expected {
    int x;
    int y;
    uint16_t tile_id;
  };
  const std::vector<Expected> expected = {
      {10, 20, 0}, {10, 21, 1}, {10, 22, 2},  {10, 23, 3},
      {11, 20, 4}, {11, 21, 5}, {11, 22, 6},  {11, 23, 7},
      {12, 20, 8}, {12, 21, 9}, {12, 22, 10}, {12, 23, 11},
  };
  ASSERT_EQ(bg1_trace.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(bg1_trace[i].x_tile, expected[i].x) << "idx=" << i;
    EXPECT_EQ(bg1_trace[i].y_tile, expected[i].y) << "idx=" << i;
    EXPECT_EQ(bg1_trace[i].tile_id, expected[i].tile_id) << "idx=" << i;
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     WeirdCornerTopBothBGMatchesUsdasm4x3ColumnMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // USDASM: RoomDraw_WeirdCornerTop_BothBG at $01:985C
  // Shape is 4 columns x 3 rows, column-major, written to both BGs.
  RoomObject obj(0x0114, /*x=*/8, /*y=*/9, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 12; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 24u);  // 12 tiles × BG1+BG2

  std::vector<ObjectDrawer::TileTrace> bg1_trace;
  for (const auto& t : trace) {
    if (t.layer == static_cast<uint8_t>(RoomObject::LayerType::BG1)) {
      bg1_trace.push_back(t);
    }
  }
  ASSERT_EQ(bg1_trace.size(), 12u);

  struct Expected {
    int x;
    int y;
    uint16_t tile_id;
  };
  const std::vector<Expected> expected = {
      {8, 9, 0},   {8, 10, 1}, {8, 11, 2},   {9, 9, 3},
      {9, 10, 4},  {9, 11, 5}, {10, 9, 6},   {10, 10, 7},
      {10, 11, 8}, {11, 9, 9}, {11, 10, 10}, {11, 11, 11},
  };
  ASSERT_EQ(bg1_trace.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(bg1_trace[i].x_tile, expected[i].x) << "idx=" << i;
    EXPECT_EQ(bg1_trace[i].y_tile, expected[i].y) << "idx=" << i;
    EXPECT_EQ(bg1_trace[i].tile_id, expected[i].tile_id) << "idx=" << i;
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     Corner4x4BothBGMatchesUsdasm4x4ColumnMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0108, /*x=*/6, /*y=*/7, /*size=*/0,
      RoomObject::LayerType::BG1, MakeSequentialTiles(/*count=*/16));

  const auto bg1_trace = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2_trace = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);

  const auto expected =
      MakeColumnMajorSnapshot(/*x=*/6, /*y=*/7, /*width=*/4, /*height=*/4,
                              /*start_tile_id=*/0);

  ExpectTraceMatchesSnapshot(bg1_trace, expected);
  ExpectTraceMatchesSnapshot(bg2_trace, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     CornerAliasOverridesRequireExplicitCustomObjectContext) {
  ScopedCustomObjectsFlag custom_enabled(true);

  auto& manager = CustomObjectManager::Get();
  const auto previous_state = manager.SnapshotState();
  struct RestoreManagerState {
    CustomObjectManager& manager;
    CustomObjectManager::State previous_state;
    ~RestoreManagerState() { manager.RestoreState(previous_state); }
  } restore{manager, previous_state};

  // Simulate an editor/runtime context where custom-object mode is enabled
  // but no project custom-object folder/mapping has been configured.
  manager.ClearObjectFileMap();
  manager.Initialize("");

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0100, /*x=*/20, /*y=*/30, /*size=*/0,
      RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16, /*start_tile_id=*/400));

  // USDASM parity guardrail: without explicit custom-object source
  // configuration, subtype-2 wall corners must stay on the vanilla 4x4
  // column-major path.
  const auto bg1_trace = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto expected =
      MakeColumnMajorSnapshot(/*x=*/20, /*y=*/30, /*width=*/4, /*height=*/4,
                              /*start_tile_id=*/400);
  ExpectTraceMatchesSnapshot(bg1_trace, expected);
  EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     CornerAliasOverridesDoNotActivateFromFolderOnlyContext) {
  ScopedCustomObjectsFlag custom_enabled(true);

  auto& manager = CustomObjectManager::Get();
  const auto previous_state = manager.SnapshotState();
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto temp_dir = std::filesystem::temp_directory_path() /
                        ("yaze_corner_alias_folder_only_" +
                         std::to_string(static_cast<long long>(nonce)));

  struct RestoreManagerStateAndCleanup {
    CustomObjectManager& manager;
    CustomObjectManager::State previous_state;
    std::filesystem::path temp_dir;
    ~RestoreManagerStateAndCleanup() {
      manager.RestoreState(previous_state);
      std::filesystem::remove_all(temp_dir);
    }
  } restore{manager, previous_state, temp_dir};

  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  manager.Initialize(temp_dir.string());
  manager.ClearObjectFileMap();

  // Folder-only custom-object setups should not remap vanilla 0x100..0x103
  // wall corners into track-corner custom payloads.
  WriteBinaryFile(temp_dir / "track_corner_TL.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/0, /*rel_y=*/0, /*tile_word=*/0x0001));
  WriteBinaryFile(temp_dir / "track_corner_TR.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/1, /*rel_y=*/0, /*tile_word=*/0x0002));
  WriteBinaryFile(temp_dir / "track_corner_BL.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/0, /*rel_y=*/1, /*tile_word=*/0x0003));
  WriteBinaryFile(temp_dir / "track_corner_BR.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/1, /*rel_y=*/1, /*tile_word=*/0x0004));

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0100, /*x=*/20, /*y=*/30, /*size=*/0,
      RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16, /*start_tile_id=*/500));
  const auto bg1_trace = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto expected =
      MakeColumnMajorSnapshot(/*x=*/20, /*y=*/30, /*width=*/4, /*height=*/4,
                              /*start_tile_id=*/500);
  ExpectTraceMatchesSnapshot(bg1_trace, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     CornerAliasOverridesUseCustomTrackCornerFiles) {
  ScopedCustomObjectsFlag custom_enabled(true);

  auto& manager = CustomObjectManager::Get();
  const auto previous_state = manager.SnapshotState();
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const auto temp_dir = std::filesystem::temp_directory_path() /
                        ("yaze_corner_alias_override_" +
                         std::to_string(static_cast<long long>(nonce)));

  struct RestoreManagerStateAndCleanup {
    CustomObjectManager& manager;
    CustomObjectManager::State previous_state;
    std::filesystem::path temp_dir;
    ~RestoreManagerStateAndCleanup() {
      manager.RestoreState(previous_state);
      std::filesystem::remove_all(temp_dir);
    }
  } restore{manager, previous_state, temp_dir};

  ASSERT_TRUE(std::filesystem::create_directories(temp_dir));
  manager.Initialize(temp_dir.string());
  manager.ClearObjectFileMap();

  WriteBinaryFile(temp_dir / "track_corner_TL.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/0, /*rel_y=*/0, /*tile_word=*/0x0001));
  WriteBinaryFile(temp_dir / "track_corner_TR.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/1, /*rel_y=*/0, /*tile_word=*/0x0002));
  WriteBinaryFile(temp_dir / "track_corner_BL.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/0, /*rel_y=*/1, /*tile_word=*/0x0003));
  WriteBinaryFile(temp_dir / "track_corner_BR.bin",
                  MakeSingleTileCustomObjectBinary(
                      /*rel_x=*/1, /*rel_y=*/1, /*tile_word=*/0x0004));
  manager.SetObjectFileMap({{0x31,
                             {"track_LR.bin", "track_UD.bin",
                              "track_corner_TL.bin", "track_corner_TR.bin",
                              "track_corner_BL.bin", "track_corner_BR.bin"}}});

  struct CornerAliasCase {
    int16_t object_id;
    const char* filename;
    int rel_x;
    int rel_y;
    uint16_t tile_id;
  };

  const std::vector<CornerAliasCase> cases = {
      {0x0100, "track_corner_TL.bin", 0, 0, 1},
      {0x0101, "track_corner_BL.bin", 0, 1, 3},
      {0x0102, "track_corner_TR.bin", 1, 0, 2},
      {0x0103, "track_corner_BR.bin", 1, 1, 4},
  };

  std::unordered_map<int16_t, std::vector<ObjectDrawer::TileTrace>>
      vanilla_traces;

  {
    ScopedCustomObjectsFlag custom_disabled(false);
    for (const auto& tc : cases) {
      SCOPED_TRACE(tc.object_id);
      EXPECT_EQ(manager.ResolveFilename(tc.object_id, /*subtype=*/0),
                tc.filename);

      auto trace = ReplayObjectTrace(
          tc.object_id, /*x=*/20, /*y=*/30, /*size=*/0,
          RoomObject::LayerType::BG1,
          MakeSequentialTiles(/*count=*/16, /*start_tile_id=*/400));
      // Vanilla path for 0x100..0x103 is a routine-driven multi-tile draw,
      // while the custom override fixture draws a single tile.
      EXPECT_GT(trace.size(), 1u);
      vanilla_traces[tc.object_id] = trace;
    }
  }

  for (const auto& tc : cases) {
    SCOPED_TRACE(tc.object_id);
    auto trace = ReplayObjectTrace(
        tc.object_id, /*x=*/20, /*y=*/30, /*size=*/0,
        RoomObject::LayerType::BG1,
        MakeSequentialTiles(/*count=*/16, /*start_tile_id=*/400));
    ASSERT_EQ(trace.size(), 1u);
    EXPECT_EQ(trace[0].x_tile, 20 + tc.rel_x);
    EXPECT_EQ(trace[0].y_tile, 30 + tc.rel_y);
    EXPECT_EQ(trace[0].tile_id, tc.tile_id);

    const auto it = vanilla_traces.find(tc.object_id);
    ASSERT_NE(it, vanilla_traces.end());
    EXPECT_NE(it->second.size(), trace.size());
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     RightwardsCornerVariantsMatchUsdasmTileOrientation) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 5;
  constexpr int kY = 7;
  constexpr uint8_t kSize = 0;  // count = size + 10
  constexpr int kCount = 10;

  auto top_trace = ReplayObjectTrace(
      /*object_id=*/0x002F, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/6));
  auto bottom_trace = ReplayObjectTrace(
      /*object_id=*/0x0030, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/6));

  const auto top_bg1 =
      FilterTraceByLayer(top_trace, RoomObject::LayerType::BG1);
  const auto bottom_bg1 =
      FilterTraceByLayer(bottom_trace, RoomObject::LayerType::BG1);

  std::vector<SnapshotTileWrite> expected_top;
  std::vector<SnapshotTileWrite> expected_bottom;
  expected_top.reserve((kCount * 2) + 8);
  expected_bottom.reserve((kCount * 2) + 8);

  // USDASM:
  // - $01:8FBD (top corners): body uses top=tile3, bottom=tile0.
  // - $01:9001 (bottom corners): mirrored body uses top=tile0, bottom=tile3.
  // Both variants draw a two-column opening cap, size+10 body columns, and a
  // two-column closing cap. The routine-name suffix describes that size+13
  // extent; it is not an X offset.
  expected_top.push_back({kX, kY, 1});
  expected_top.push_back({kX + 1, kY, 2});
  expected_top.push_back({kX, kY + 1, 0});
  expected_top.push_back({kX + 1, kY + 1, 0});

  expected_bottom.push_back({kX, kY + 1, 1});
  expected_bottom.push_back({kX + 1, kY + 1, 2});
  expected_bottom.push_back({kX, kY, 0});
  expected_bottom.push_back({kX + 1, kY, 0});

  for (int s = 0; s < kCount; ++s) {
    const int x = kX + 2 + s;

    expected_top.push_back({x, kY, 3});
    expected_top.push_back({x, kY + 1, 0});

    expected_bottom.push_back({x, kY + 1, 3});
    expected_bottom.push_back({x, kY, 0});
  }

  expected_top.push_back({kX + kCount + 2, kY, 4});
  expected_top.push_back({kX + kCount + 3, kY, 5});
  expected_top.push_back({kX + kCount + 2, kY + 1, 0});
  expected_top.push_back({kX + kCount + 3, kY + 1, 0});

  expected_bottom.push_back({kX + kCount + 2, kY + 1, 4});
  expected_bottom.push_back({kX + kCount + 3, kY + 1, 5});
  expected_bottom.push_back({kX + kCount + 2, kY, 0});
  expected_bottom.push_back({kX + kCount + 3, kY, 0});

  ExpectTraceMatchesSnapshot(top_bg1, expected_top);
  ExpectTraceMatchesSnapshot(bottom_bg1, expected_bottom);
}

TEST(ObjectDrawerRegistryReplayTest,
     DownwardsCornerVariantsMatchUsdasmTileOrientation) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 8;
  constexpr uint8_t kSize = 0;  // count = size + 10
  constexpr int kCount = 10;

  auto left_trace = ReplayObjectTrace(
      /*object_id=*/0x006C, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/6));
  auto right_trace = ReplayObjectTrace(
      /*object_id=*/0x006D, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/6));

  const auto left_bg1 =
      FilterTraceByLayer(left_trace, RoomObject::LayerType::BG1);
  const auto right_bg1 =
      FilterTraceByLayer(right_trace, RoomObject::LayerType::BG1);

  std::vector<SnapshotTileWrite> expected_left;
  std::vector<SnapshotTileWrite> expected_right;
  expected_left.reserve((kCount * 2) + 8);
  expected_right.reserve((kCount * 2) + 8);

  // USDASM:
  // - $01:9045 (left corners): optional top 2x2 cap, then body uses
  //   left=tile3/right=tile0, then a bottom 2x2 cap.
  // - $01:908F (right corners): mirrored variant with body using
  //   left=tile0/right=tile3 and matching 2x2 caps.
  //
  // On a blank destination the opening cap is emitted, so the body begins two
  // rows below the anchor. The routine-name suffix describes the size+12
  // extent; it is not an X offset.
  expected_left.push_back({kX, kY, 1});
  expected_left.push_back({kX, kY + 1, 2});
  expected_left.push_back({kX + 1, kY, 0});
  expected_left.push_back({kX + 1, kY + 1, 0});

  expected_right.push_back({kX + 1, kY, 1});
  expected_right.push_back({kX + 1, kY + 1, 2});
  expected_right.push_back({kX, kY, 0});
  expected_right.push_back({kX, kY + 1, 0});

  for (int s = 0; s < kCount; ++s) {
    const int y = kY + 2 + s;

    expected_left.push_back({kX, y, 3});
    expected_left.push_back({kX + 1, y, 0});

    expected_right.push_back({kX + 1, y, 3});
    expected_right.push_back({kX, y, 0});
  }

  expected_left.push_back({kX, kY + 2 + kCount, 4});
  expected_left.push_back({kX, kY + 3 + kCount, 5});
  expected_left.push_back({kX + 1, kY + 2 + kCount, 0});
  expected_left.push_back({kX + 1, kY + 3 + kCount, 0});

  expected_right.push_back({kX + 1, kY + 2 + kCount, 4});
  expected_right.push_back({kX + 1, kY + 3 + kCount, 5});
  expected_right.push_back({kX, kY + 2 + kCount, 0});
  expected_right.push_back({kX, kY + 3 + kCount, 0});

  ExpectTraceMatchesSnapshot(left_bg1, expected_left);
  ExpectTraceMatchesSnapshot(right_bg1, expected_right);
}

TEST(ObjectDrawerRoutineSnapshotHarnessTest,
     RepresentativeObjectsMatchRoutineSnapshots) {
  ScopedCustomObjectsFlag disable_custom(false);

  struct SnapshotReplayCase {
    const char* name = nullptr;
    int16_t object_id = 0;
    uint8_t size = 0;
    int x = 0;
    int y = 0;
    RoomObject::LayerType layer = RoomObject::LayerType::BG1;
    int tile_count = 0;
    std::vector<SnapshotTileWrite> expected_bg1;
    bool expect_bg2_mirror = false;
  };

  std::vector<SnapshotReplayCase> cases;
  cases.push_back(
      {.name = "Corner4x4BothBG",
       .object_id = 0x0108,
       .size = 0,
       .x = 4,
       .y = 5,
       .layer = RoomObject::LayerType::BG1,
       .tile_count = 16,
       .expected_bg1 = MakeColumnMajorSnapshot(
           /*x=*/4, /*y=*/5, /*width=*/4, /*height=*/4, /*start_tile_id=*/0),
       .expect_bg2_mirror = true});
  cases.push_back(
      {.name = "WeirdCornerBottomBothBG",
       .object_id = 0x0110,
       .size = 0,
       .x = 8,
       .y = 9,
       .layer = RoomObject::LayerType::BG1,
       .tile_count = 12,
       .expected_bg1 = MakeColumnMajorSnapshot(
           /*x=*/8, /*y=*/9, /*width=*/3, /*height=*/4, /*start_tile_id=*/0),
       .expect_bg2_mirror = true});
  cases.push_back(
      {.name = "WeirdCornerTopBothBG",
       .object_id = 0x0114,
       .size = 0,
       .x = 10,
       .y = 11,
       .layer = RoomObject::LayerType::BG1,
       .tile_count = 12,
       .expected_bg1 = MakeColumnMajorSnapshot(
           /*x=*/10, /*y=*/11, /*width=*/4, /*height=*/3, /*start_tile_id=*/0),
       .expect_bg2_mirror = true});
  cases.push_back(
      {.name = "Bed4x5",
       .object_id = 0x0122,
       .size = 0,
       .x = 6,
       .y = 7,
       .layer = RoomObject::LayerType::BG1,
       .tile_count = 20,
       .expected_bg1 = MakeRowMajorSnapshot(
           /*x=*/6, /*y=*/7, /*width=*/4, /*height=*/5, /*start_tile_id=*/0)});
  cases.push_back({.name = "Rightwards3x6",
                   .object_id = 0x012C,
                   .size = 0,
                   .x = 12,
                   .y = 13,
                   .layer = RoomObject::LayerType::BG1,
                   .tile_count = 18,
                   .expected_bg1 = MakeColumnMajorSnapshot(
                       /*x=*/12, /*y=*/13, /*width=*/6, /*height=*/3,
                       /*start_tile_id=*/0)});
  cases.push_back({.name = "Waterfall48",
                   .object_id = 0x0048,
                   .size = 0,
                   .x = 2,
                   .y = 3,
                   .layer = RoomObject::LayerType::BG1,
                   .tile_count = 9,
                   .expected_bg1 = std::vector<SnapshotTileWrite>{{2, 3, 0},
                                                                  {2, 4, 1},
                                                                  {2, 5, 2},
                                                                  {3, 3, 3},
                                                                  {3, 4, 4},
                                                                  {3, 5, 5},
                                                                  {4, 3, 3},
                                                                  {4, 4, 4},
                                                                  {4, 5, 5},
                                                                  {5, 3, 6},
                                                                  {5, 4, 7},
                                                                  {5, 5, 8}}});

  for (const auto& tc : cases) {
    SCOPED_TRACE(tc.name);
    auto trace = ReplayObjectTrace(tc.object_id, tc.x, tc.y, tc.size, tc.layer,
                                   MakeSequentialTiles(tc.tile_count));
    const auto bg1_trace =
        FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
    ExpectTraceMatchesSnapshot(bg1_trace, tc.expected_bg1);

    const auto bg2_trace =
        FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
    if (tc.expect_bg2_mirror) {
      ExpectTraceMatchesSnapshot(bg2_trace, tc.expected_bg1);
    } else {
      EXPECT_TRUE(bg2_trace.empty());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest, Bed4x5UsesUsdasmRowMajorOrder) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject obj(0x0122, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 20; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 20u);

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  for (int y = 0; y < 5; ++y) {
    for (int x = 0; x < 4; ++x) {
      EXPECT_EQ(by_pos[key(10 + x, 20 + y)], static_cast<uint16_t>(y * 4 + x))
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     Rightwards3x6UsesUsdasmSixByThreeColumnMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject obj(0x012C, /*x=*/8, /*y=*/9, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 18; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 18u);

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  for (int x = 0; x < 6; ++x) {
    for (int y = 0; y < 3; ++y) {
      EXPECT_EQ(by_pos[key(8 + x, 9 + y)], static_cast<uint16_t>(x * 3 + y))
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest, Waterfall47UsesStartMiddleEndTileBlocks) {
  // Mirrors the Waterfall48 test below. Vanilla `RoomDraw_Waterfall47`
  // ($01:9466) draws three vertical 1x5 columns: the left column
  // consumes tile slots 0..4, the middle column block (which repeats
  // by `(size+1)*2`) consumes tile slots 5..9, and the right column
  // consumes tile slots 10..14. With size=0 the middle block repeats
  // twice, producing a 4-column-by-5-row trace = 20 tiles.
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject obj(0x0047, /*x=*/5, /*y=*/6, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 15; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(200 + i),
                                       /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 20u);  // 4 columns x 5 rows

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  // Left column (x=5): tiles 0..4
  for (int row = 0; row < 5; ++row) {
    EXPECT_EQ(by_pos[key(5, 6 + row)], 200 + row) << "left column row " << row;
  }
  // Middle columns (x=6, 7): tiles 5..9 each
  for (int x = 6; x <= 7; ++x) {
    for (int row = 0; row < 5; ++row) {
      EXPECT_EQ(by_pos[key(x, 6 + row)], 200 + 5 + row)
          << "middle column x=" << x << " row " << row;
    }
  }
  // Right column (x=8): tiles 10..14
  for (int row = 0; row < 5; ++row) {
    EXPECT_EQ(by_pos[key(8, 6 + row)], 200 + 10 + row)
        << "right column row " << row;
  }
}

TEST(ObjectDrawerRegistryReplayTest, Waterfall48UsesStartMiddleEndTileBlocks) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject obj(0x0048, /*x=*/5, /*y=*/6, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 9; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(100 + i),
                                       /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 12u);  // 4 columns x 3 rows

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  EXPECT_EQ(by_pos[key(5, 6)], 100);
  EXPECT_EQ(by_pos[key(5, 7)], 101);
  EXPECT_EQ(by_pos[key(5, 8)], 102);

  for (int x = 6; x <= 7; ++x) {
    EXPECT_EQ(by_pos[key(x, 6)], 103);
    EXPECT_EQ(by_pos[key(x, 7)], 104);
    EXPECT_EQ(by_pos[key(x, 8)], 105);
  }

  EXPECT_EQ(by_pos[key(8, 6)], 106);
  EXPECT_EQ(by_pos[key(8, 7)], 107);
  EXPECT_EQ(by_pos[key(8, 8)], 108);
}

TEST(ObjectDrawerRegistryReplayTest,
     RupeeFloorMatchesUsdasmSparseFiveByEightPattern) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 7;
  constexpr int kY = 11;
  constexpr uint16_t kTopTile = 0x500;
  const auto tiles = MakeSequentialTiles(2, kTopTile);

  for (const auto layer :
       {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
    SCOPED_TRACE(static_cast<int>(layer));
    const auto trace = ReplayObjectTrace(
        /*object_id=*/0x0F92, kX, kY, /*size=*/0, layer, tiles);
    ASSERT_EQ(trace.size(), 18u);
    EXPECT_EQ(FilterTraceByLayer(trace, layer).size(), 18u);
    const auto other_layer = layer == RoomObject::LayerType::BG1
                                 ? RoomObject::LayerType::BG2
                                 : RoomObject::LayerType::BG1;
    EXPECT_TRUE(FilterTraceByLayer(trace, other_layer).empty());

    size_t index = 0;
    for (int col = 0; col < 3; ++col) {
      const int x = kX + col * 2;
      for (int row : {0, 3, 6}) {
        ASSERT_LT(index, trace.size());
        EXPECT_EQ(trace[index].x_tile, x);
        EXPECT_EQ(trace[index].y_tile, kY + row);
        EXPECT_EQ(trace[index].tile_id, kTopTile);
        ++index;
      }
      for (int row : {1, 4, 7}) {
        ASSERT_LT(index, trace.size());
        EXPECT_EQ(trace[index].x_tile, x);
        EXPECT_EQ(trace[index].y_tile, kY + row);
        EXPECT_EQ(trace[index].tile_id, kTopTile + 1);
        ++index;
      }
    }
    EXPECT_EQ(index, trace.size());
    ExpectTraceBounds(trace, kX, kY, kX + 4, kY + 7);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     RupeeFloorHidesOnlyWhenCurrentRoomIsCleared) {
  ScopedCustomObjectsFlag disable_custom(false);

  const auto tiles = MakeSequentialTiles(2);
  FakeDungeonState state;

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0F92, /*x=*/3, /*y=*/5, /*size=*/0,
      RoomObject::LayerType::BG1, tiles, &state);
  EXPECT_EQ(trace.size(), 18u) << "uncleared room keeps rupees visible";

  state.cleared_rupee_floor_room_id = 1;
  trace =
      ReplayObjectTrace(/*object_id=*/0x0F92, /*x=*/3, /*y=*/5,
                        /*size=*/0, RoomObject::LayerType::BG1, tiles, &state);
  EXPECT_EQ(trace.size(), 18u) << "another room's flag must not hide rupees";

  state.cleared_rupee_floor_room_id = 0;
  trace =
      ReplayObjectTrace(/*object_id=*/0x0F92, /*x=*/3, /*y=*/5,
                        /*size=*/0, RoomObject::LayerType::BG1, tiles, &state);
  EXPECT_TRUE(trace.empty()) << "current-room cleared state suppresses writes";
}

TEST(ObjectDrawerRegistryReplayTest,
     BombableFloorMatchesUsdasmFourByFourStateMatrices) {
  ScopedCustomObjectsFlag disable_custom(false);

  // Oracle of Secrets relocates this floor from vanilla room 0x65 to 0xAD.
  // The preview state must therefore follow the current room ID.
  constexpr int kBombableFloorRoomId = 0xAD;
  constexpr int kX = 9;
  constexpr int kY = 11;
  constexpr std::array<int, 16> kPayloadIndexByPosition = {
      0, 2, 4, 6, 1, 3, 5, 7, 8, 10, 12, 14, 9, 11, 13, 15};
  const auto tiles = MakeSequentialTiles(32);
  FakeDungeonState state;

  auto assert_state = [&](const std::vector<ObjectDrawer::TileTrace>& trace,
                          int state_offset) {
    ASSERT_EQ(trace.size(), 16u);
    ExpectTraceBounds(trace, kX, kY, kX + 3, kY + 3);
    for (int y = 0; y < 4; ++y) {
      for (int x = 0; x < 4; ++x) {
        SCOPED_TRACE(::testing::Message() << "x=" << x << " y=" << y);
        EXPECT_EQ(LastTileIdAt(trace, kX + x, kY + y),
                  state_offset + kPayloadIndexByPosition[y * 4 + x]);
      }
    }
  };

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FC7, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      tiles, &state, {}, kBombableFloorRoomId);
  assert_state(trace, /*state_offset=*/0);

  state.bombed_floor_room_id = kBombableFloorRoomId;
  trace = ReplayObjectTrace(/*object_id=*/0x0FC7, kX, kY, /*size=*/0,
                            RoomObject::LayerType::BG1, tiles, &state, {},
                            kBombableFloorRoomId);
  assert_state(trace, /*state_offset=*/16);

  trace = ReplayObjectTrace(
      /*object_id=*/0x0FC7, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      tiles, &state, {},
      /*room_id=*/kBombableFloorRoomId - 1);
  assert_state(trace, /*state_offset=*/0);
}

TEST(ObjectDrawerRegistryReplayTest,
     LightBeamOnFloorMatchesUsdasmStackedBlocks) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 20;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FF0, kX, kY, /*size=*/0, RoomObject::LayerType::BG2,
      MakeSequentialTiles(32));
  ASSERT_EQ(trace.size(), 48u);

  for (int tile = 0; tile < 16; ++tile) {
    SCOPED_TRACE(tile);
    const int dx = tile / 4;
    const int dy = tile % 4;

    EXPECT_EQ(trace[tile].x_tile, kX + dx);
    EXPECT_EQ(trace[tile].y_tile, kY + dy);
    EXPECT_EQ(trace[tile].tile_id, tile);

    EXPECT_EQ(trace[16 + tile].x_tile, kX + dx);
    EXPECT_EQ(trace[16 + tile].y_tile, kY + 2 + dy);
    EXPECT_EQ(trace[16 + tile].tile_id, tile)
        << "middle block must reset to obj2376";

    EXPECT_EQ(trace[32 + tile].x_tile, kX + dx);
    EXPECT_EQ(trace[32 + tile].y_tile, kY + 6 + dy);
    EXPECT_EQ(trace[32 + tile].tile_id, 16 + tile)
        << "bottom block must use obj2396";

    EXPECT_EQ(trace[tile].layer,
              static_cast<uint8_t>(RoomObject::LayerType::BG2));
    EXPECT_EQ(trace[16 + tile].layer,
              static_cast<uint8_t>(RoomObject::LayerType::BG2));
    EXPECT_EQ(trace[32 + tile].layer,
              static_cast<uint8_t>(RoomObject::LayerType::BG2));
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     BigLightBeamOnFloorMatchesUsdasmFloorLightGrid) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 20;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FF1, kX, kY, /*size=*/4, RoomObject::LayerType::BG2,
      MakeSequentialTiles(64));
  ASSERT_EQ(trace.size(), 64u);

  for (int block = 0; block < 4; ++block) {
    const int block_x = (block % 2) * 4;
    const int block_y = (block / 2) * 4;
    for (int tile = 0; tile < 16; ++tile) {
      SCOPED_TRACE(::testing::Message()
                   << "block=" << block << " tile=" << tile);
      const auto& write = trace[block * 16 + tile];
      EXPECT_EQ(write.x_tile, kX + block_x + tile / 4);
      EXPECT_EQ(write.y_tile, kY + block_y + tile % 4);
      EXPECT_EQ(write.tile_id, block * 16 + tile);
      EXPECT_EQ(write.layer, static_cast<uint8_t>(RoomObject::LayerType::BG2));
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     BigLightBeamUsesFixedEastAtticBombedFloorStateWhenAvailable) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kEastAtticRoomId = 0x65;
  constexpr int kX = 10;
  constexpr int kY = 20;
  const auto tiles = MakeSequentialTiles(64);
  FakeDungeonState state;

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FF1, kX, kY, /*size=*/4, RoomObject::LayerType::BG1,
      tiles, &state);
  EXPECT_TRUE(trace.empty());

  state.bombed_floor_room_id = kEastAtticRoomId - 1;
  trace = ReplayObjectTrace(/*object_id=*/0x0FF1, kX, kY, /*size=*/4,
                            RoomObject::LayerType::BG1, tiles, &state);
  EXPECT_TRUE(trace.empty());

  state.bombed_floor_room_id = kEastAtticRoomId;
  trace = ReplayObjectTrace(/*object_id=*/0x0FF1, kX, kY, /*size=*/4,
                            RoomObject::LayerType::BG1, tiles, &state);
  ASSERT_EQ(trace.size(), 64u);
  for (const auto& write : trace) {
    EXPECT_EQ(write.layer, static_cast<uint8_t>(RoomObject::LayerType::BG1));
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     FloorLightDrawsUnconditionalSizeInvariantGridOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 20;
  for (const auto layer :
       {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
    for (uint8_t size : {uint8_t{0}, uint8_t{1}, uint8_t{13}, uint8_t{15}}) {
      SCOPED_TRACE(::testing::Message() << "layer=" << static_cast<int>(layer)
                                        << " size=" << static_cast<int>(size));
      auto trace = ReplayObjectTrace(/*object_id=*/0x0FF4, kX, kY, size, layer,
                                     MakeSequentialTiles(64));
      ASSERT_EQ(trace.size(), 64u);

      for (int block = 0; block < 4; ++block) {
        const int block_x = (block % 2) * 4;
        const int block_y = (block / 2) * 4;
        for (int tile = 0; tile < 16; ++tile) {
          SCOPED_TRACE(::testing::Message()
                       << "block=" << block << " tile=" << tile);
          const auto& write = trace[block * 16 + tile];
          EXPECT_EQ(write.x_tile, kX + block_x + tile / 4);
          EXPECT_EQ(write.y_tile, kY + block_y + tile % 4);
          EXPECT_EQ(write.tile_id, block * 16 + tile);
          EXPECT_EQ(write.layer, static_cast<uint8_t>(layer));
        }
      }
    }
  }
}

TEST(ObjectDrawerPillarStrideTest, RightwardsPillar2x4Spaced4Uses6TileStride) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0x3D maps to RoomDraw_RightwardsPillar2x4spaced4_1to16.
  // With size=1 => count=2 pillars. Each pillar is 2x4 (8 writes).
  RoomObject obj(0x003D, /*x=*/10, /*y=*/20, /*size=*/1, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 16u);

  std::set<int> xs;
  for (const auto& t : trace) {
    xs.insert(t.x_tile);
  }

  // Stride is 6 tiles: columns at x=10,11 and x=16,17 (not x=14,15).
  EXPECT_EQ(xs.size(), 4u);
  EXPECT_NE(xs.count(10), 0u);
  EXPECT_NE(xs.count(11), 0u);
  EXPECT_NE(xs.count(16), 0u);
  EXPECT_NE(xs.count(17), 0u);
  EXPECT_EQ(xs.count(14), 0u);
  EXPECT_EQ(xs.count(15), 0u);
}

TEST(ObjectDrawerRegistryReplayTest,
     BuiltInWallRoutingAndDiagonalCountMatchUsdasm) {
  ScopedCustomObjectsFlag disable_custom(false);

  struct Case {
    int16_t object_id;
    size_t expected_bg1_writes;
    size_t expected_bg2_writes;
    bool diagonal;
  };
  const std::array<Case, 6> cases = {{{0x03, 8, 8, false},
                                      {0x05, 0, 8, false},
                                      {0x0C, 0, 30, true},
                                      {0x14, 0, 30, true},
                                      {0x15, 30, 30, true},
                                      {0x20, 30, 30, true}}};

  constexpr int kX = 20;
  constexpr int kY = 20;
  for (const auto& test_case : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "object=0x" << std::hex << test_case.object_id);
    const auto trace = ReplayObjectTrace(test_case.object_id, kX, kY,
                                         /*size=*/0, RoomObject::LayerType::BG2,
                                         MakeSequentialTiles(/*count=*/8));
    const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
    const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
    EXPECT_EQ(bg1.size(), test_case.expected_bg1_writes);
    EXPECT_EQ(bg2.size(), test_case.expected_bg2_writes);

    if (test_case.diagonal) {
      for (const auto* layer_trace : {&bg1, &bg2}) {
        if (layer_trace->empty()) {
          continue;
        }
        const auto [min_it, max_it] =
            std::minmax_element(layer_trace->begin(), layer_trace->end(),
                                [](const auto& lhs, const auto& rhs) {
                                  return lhs.x_tile < rhs.x_tile;
                                });
        EXPECT_EQ(min_it->x_tile, kX);
        EXPECT_EQ(max_it->x_tile, kX + 5);
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     ConditionalEdgeCapsReadTheMatchingLayoutOwner) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(1024 * 1024, 0)).ok());
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  struct Case {
    int16_t object_id;
    uint16_t matching_tile_id;
    int edge_dx;
    int edge_dy;
    std::array<uint16_t, 2> opening_tile_ids;
    int opening_tile_count;
  };
  const std::array<Case, 9> cases = {{{0x22, 0x00E2, 0, 0, {0x300, 0}, 1},
                                      {0x23, 0x01DB, 0, 0, {0x300, 0}, 1},
                                      {0x2F, 0x00E2, 0, 0, {0x301, 0x302}, 2},
                                      {0x30, 0x00E2, 0, 1, {0x301, 0x302}, 2},
                                      {0x5F, 0x00E2, 0, 0, {0x300, 0}, 1},
                                      {0x69, 0x00E3, 0, 0, {0x300, 0}, 1},
                                      {0x6C, 0x00E3, 0, 0, {0x301, 0x302}, 2},
                                      {0x6D, 0x00E3, 1, 0, {0x301, 0x302}, 2},
                                      {0x8A, 0x00E3, 0, 0, {0x300, 0}, 1}}};

  constexpr int kX = 20;
  constexpr int kY = 20;
  for (const auto layer :
       {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
    for (const auto& test_case : cases) {
      SCOPED_TRACE(::testing::Message()
                   << "object=0x" << std::hex << test_case.object_id
                   << " layer=" << std::dec << static_cast<int>(layer));

      gfx::BackgroundBuffer object_bg1(512, 512);
      gfx::BackgroundBuffer object_bg2(512, 512);
      gfx::BackgroundBuffer layout_bg1(512, 512);
      gfx::BackgroundBuffer layout_bg2(512, 512);
      for (auto* buffer :
           {&object_bg1, &object_bg2, &layout_bg1, &layout_bg2}) {
        buffer->EnsureBitmapInitialized();
        buffer->bitmap().Fill(255);
        buffer->ClearBuffer();
      }

      auto& matching_layout =
          layer == RoomObject::LayerType::BG2 ? layout_bg2 : layout_bg1;
      matching_layout.SetTileAt(kX + test_case.edge_dx, kY + test_case.edge_dy,
                                test_case.matching_tile_id);

      ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());
      std::vector<ObjectDrawer::TileTrace> trace;
      drawer.SetTraceCollector(&trace, /*trace_only=*/false);

      RoomObject object(test_case.object_id, kX, kY, /*size=*/0,
                        static_cast<uint8_t>(layer));
      object.tiles_loaded_ = true;
      object.tiles_ = MakeSequentialTiles(/*count=*/6,
                                          /*start_tile_id=*/0x300);
      gfx::PaletteGroup palette_group;
      ASSERT_TRUE(drawer
                      .DrawObject(object, object_bg1, object_bg2, palette_group,
                                  /*state=*/nullptr, &layout_bg1, &layout_bg2)
                      .ok());

      const auto layer_trace = FilterTraceByLayer(trace, layer);
      ASSERT_FALSE(layer_trace.empty());
      for (int index = 0; index < test_case.opening_tile_count; ++index) {
        const uint16_t opening_tile_id = test_case.opening_tile_ids[index];
        EXPECT_TRUE(std::none_of(layer_trace.begin(), layer_trace.end(),
                                 [&](const auto& write) {
                                   return write.tile_id == opening_tile_id;
                                 }));
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     ConditionalEdgeCapsPreferPriorObjectWritesOverLayout) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(1024 * 1024, 0)).ok());
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  constexpr int kX = 20;
  constexpr int kY = 20;
  for (const bool prior_object_matches : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "prior_object_matches=" << prior_object_matches);

    gfx::BackgroundBuffer object_bg1(512, 512);
    gfx::BackgroundBuffer object_bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&object_bg1, &object_bg2, &layout_bg1, &layout_bg2}) {
      buffer->EnsureBitmapInitialized();
      buffer->bitmap().Fill(255);
      buffer->ClearBuffer();
    }
    layout_bg1.SetTileAt(kX, kY, 0x00E2);

    ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());
    gfx::PaletteGroup palette_group;

    RoomObject prior(/*id=*/0x11F, kX, kY, /*size=*/0,
                     RoomObject::LayerType::BG1);
    prior.tiles_loaded_ = true;
    prior.tiles_ = MakeSequentialTiles(/*count=*/4,
                                       /*start_tile_id=*/0x320);
    prior.tiles_[0].id_ = prior_object_matches ? 0x00E2 : 0x0320;
    ASSERT_TRUE(drawer
                    .DrawObject(prior, object_bg1, object_bg2, palette_group,
                                /*state=*/nullptr, &layout_bg1, &layout_bg2)
                    .ok());

    std::vector<ObjectDrawer::TileTrace> trace;
    drawer.SetTraceCollector(&trace, /*trace_only=*/false);
    RoomObject edge(/*id=*/0x22, kX, kY, /*size=*/0,
                    RoomObject::LayerType::BG1);
    edge.tiles_loaded_ = true;
    edge.tiles_ = MakeSequentialTiles(/*count=*/3,
                                      /*start_tile_id=*/0x300);
    ASSERT_TRUE(drawer
                    .DrawObject(edge, object_bg1, object_bg2, palette_group,
                                /*state=*/nullptr, &layout_bg1, &layout_bg2)
                    .ok());

    const bool opening_cap_was_drawn =
        std::any_of(trace.begin(), trace.end(),
                    [](const auto& write) { return write.tile_id == 0x0300; });
    EXPECT_EQ(opening_cap_was_drawn, !prior_object_matches);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     Downwards4x2BothBgUsesUsdasmSizePlusOneRows) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 20;
  for (uint8_t size : {uint8_t{0}, uint8_t{3}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));

    const auto trace = ReplayObjectTrace(
        /*object_id=*/0x0063, kX, kY, size, RoomObject::LayerType::BG1,
        MakeSequentialTiles(/*count=*/8));
    const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
    const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);

    std::vector<SnapshotTileWrite> expected;
    const int count = static_cast<int>(size) + 1;
    expected.reserve(count * 8);
    for (int block = 0; block < count; ++block) {
      for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 4; ++column) {
          expected.push_back({kX + column, kY + block * 2 + row,
                              static_cast<uint16_t>(row * 4 + column)});
        }
      }
    }

    ExpectTraceMatchesSnapshot(bg1, expected);
    ExpectTraceMatchesSnapshot(bg2, expected);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     DownwardsDecor4x2Spaced4UsesRowMajorTileOrder) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0x65 maps to routine 10.
  RoomObject obj(0x0065, /*x=*/10, /*y=*/20, /*size=*/1, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 16u);

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  // Row-major 4x2 at y=20..21
  EXPECT_EQ(by_pos[key(10, 20)], 0);
  EXPECT_EQ(by_pos[key(11, 20)], 1);
  EXPECT_EQ(by_pos[key(12, 20)], 2);
  EXPECT_EQ(by_pos[key(13, 20)], 3);
  EXPECT_EQ(by_pos[key(10, 21)], 4);
  EXPECT_EQ(by_pos[key(11, 21)], 5);
  EXPECT_EQ(by_pos[key(12, 21)], 6);
  EXPECT_EQ(by_pos[key(13, 21)], 7);

  // Second slice uses +6 vertical stride.
  EXPECT_EQ(by_pos[key(10, 26)], 0);
  EXPECT_EQ(by_pos[key(13, 27)], 7);
}

TEST(ObjectDrawerRegistryReplayTest,
     SuperSquare4x4FloorShortTilePayloadFallsBack) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0xC8 uses routine 58. Some hacks provide abbreviated tile payloads.
  RoomObject obj(0x00C8, /*x=*/8, /*y=*/8, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(
      gfx::TileInfo(/*id=*/0x2A, /*pal=*/2, false, false, false));

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  EXPECT_FALSE(trace.empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     BigKeyLockUsesObjectDrawerRoomIdAndColumnMajorTiles) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  // Use trace-only mode so we don't need real gfx data.
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  FakeDungeonState state;

  RoomObject lock(0x0F98, /*x=*/10, /*y=*/10, /*size=*/0, /*layer=*/0);
  lock.tiles_loaded_ = true;
  lock.tiles_.clear();
  for (int i = 0; i < 4; ++i) {
    lock.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                        false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(lock, bg1, bg2, palette_group, &state).ok());
  ASSERT_EQ(trace.size(), 4u);
  EXPECT_EQ(state.big_key_lock_queries,
            (std::vector<std::pair<int, int>>{{0x42, 0}}));

  const auto expected = MakeColumnMajorSnapshot(/*x=*/10, /*y=*/10,
                                                /*width=*/2, /*height=*/2,
                                                /*start_tile_id=*/0);
  ExpectTraceMatchesSnapshot(trace, expected);

  state.open_big_key_lock_slots.insert({0x42, 0});
  state.big_key_lock_queries.clear();
  drawer.ResetChestIndex();
  trace.clear();
  ASSERT_TRUE(drawer.DrawObject(lock, bg1, bg2, palette_group, &state).ok());
  EXPECT_TRUE(trace.empty());
  EXPECT_EQ(state.big_key_lock_queries,
            (std::vector<std::pair<int, int>>{{0x42, 0}}));
}

TEST(ObjectDrawerRegistryReplayTest,
     BigKeyLockSharesOrderedRoomEventSlotsWithChests) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  RoomObject chest(0x0F99, /*x=*/2, /*y=*/2, /*size=*/0, /*layer=*/0);
  chest.tiles_loaded_ = true;
  chest.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/100);

  RoomObject first_lock(0x0F98, /*x=*/10, /*y=*/10, /*size=*/0, /*layer=*/0);
  first_lock.tiles_loaded_ = true;
  first_lock.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/200);

  RoomObject second_lock(0x0F98, /*x=*/20, /*y=*/20, /*size=*/0, /*layer=*/0);
  second_lock.tiles_loaded_ = true;
  second_lock.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/300);

  FakeDungeonState state;
  // Slot 0 is the chest, slot 1 is the first lock, and slot 2 is the second.
  state.open_big_key_lock_slots.insert({0x42, 1});

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;
  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(
      drawer.DrawObjectList({chest}, bg1, bg2, palette_group, &state).ok());
  ASSERT_TRUE(drawer
                  .DrawObjectList({first_lock, second_lock}, bg1, bg2,
                                  palette_group, &state,
                                  /*layout_bg1=*/nullptr,
                                  /*reset_room_event_indices=*/false)
                  .ok());

  EXPECT_EQ(state.big_key_lock_queries,
            (std::vector<std::pair<int, int>>{{0x42, 1}, {0x42, 2}}));
  ASSERT_EQ(trace.size(), 8u);
  for (size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(trace[i].object_id, 0x0F99);
  }
  for (size_t i = 4; i < trace.size(); ++i) {
    EXPECT_EQ(trace[i].object_id, 0x0F98);
  }
  ExpectTraceMatchesSnapshot(
      std::vector<ObjectDrawer::TileTrace>(trace.begin() + 4, trace.end()),
      MakeColumnMajorSnapshot(/*x=*/20, /*y=*/20, /*width=*/2, /*height=*/2,
                              /*start_tile_id=*/300));

  // RoomDraw_Chest maintains a separate chest counter, then copies that next
  // value into the shared room-event counter. A chest after a lock therefore
  // resynchronizes (rather than simply incrementing) the next lock slot.
  state.chest_queries.clear();
  state.big_key_lock_queries.clear();
  trace.clear();
  ASSERT_TRUE(drawer
                  .DrawObjectList({first_lock, chest, second_lock}, bg1, bg2,
                                  palette_group, &state)
                  .ok());
  EXPECT_EQ(state.chest_queries, (std::vector<std::pair<int, int>>{{0x42, 0}}));
  EXPECT_EQ(state.big_key_lock_queries,
            (std::vector<std::pair<int, int>>{{0x42, 0}, {0x42, 1}}));
}

TEST(ObjectDrawerRegistryReplayTest,
     ChestOpcodesConsumeOnlyTheirAuthoritativeRoomEventSlots) {
  ScopedCustomObjectsFlag disable_custom(false);

  struct Case {
    int16_t object_id;
    int tile_count;
    int width;
    int height;
    int expected_lock_slot;
    bool queries_chest;
  };
  const std::array cases = {
      Case{0x0F99, 8, 2, 2, 1, true},
      Case{0x0F9A, 4, 2, 2, 0, false},
      Case{0x0FB1, 24, 4, 3, 1, true},
      Case{0x0FB2, 12, 4, 3, 0, false},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "prefix_id=0x" << std::hex << test_case.object_id);

    Rom rom;
    std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
    rom.LoadFromData(dummy_rom);
    ObjectDrawer drawer(&rom, /*room_id=*/0x42,
                        /*room_gfx_buffer=*/nullptr);

    RoomObject prefix(test_case.object_id, /*x=*/2, /*y=*/2, /*size=*/0,
                      /*layer=*/0);
    prefix.tiles_loaded_ = true;
    prefix.tiles_ =
        MakeSequentialTiles(test_case.tile_count, /*start_tile_id=*/100);

    RoomObject lock(0x0F98, /*x=*/20, /*y=*/20, /*size=*/0, /*layer=*/0);
    lock.tiles_loaded_ = true;
    lock.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/900);

    FakeDungeonState state;
    state.open_big_key_lock_slots.insert({0x42, test_case.expected_lock_slot});

    gfx::BackgroundBuffer bg1(512, 512);
    gfx::BackgroundBuffer bg2(512, 512);
    gfx::PaletteGroup palette_group;
    std::vector<ObjectDrawer::TileTrace> trace;
    drawer.SetTraceCollector(&trace, /*trace_only=*/true);

    ASSERT_TRUE(
        drawer.DrawObjectList({prefix, lock}, bg1, bg2, palette_group, &state)
            .ok());
    EXPECT_EQ(state.chest_queries,
              test_case.queries_chest
                  ? (std::vector<std::pair<int, int>>{{0x42, 0}})
                  : (std::vector<std::pair<int, int>>{}));
    EXPECT_EQ(state.big_key_lock_queries,
              (std::vector<std::pair<int, int>>{
                  {0x42, test_case.expected_lock_slot}}));
    ExpectTraceMatchesSnapshot(
        trace, MakeColumnMajorSnapshot(/*x=*/2, /*y=*/2, test_case.width,
                                       test_case.height,
                                       /*start_tile_id=*/100));
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     StatefulBigChestUsesOpenedTilesAndResynchronizesRoomEventSlot) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  RoomObject big_chest(0x0FB1, /*x=*/2, /*y=*/2, /*size=*/0, /*layer=*/0);
  big_chest.tiles_loaded_ = true;
  big_chest.tiles_ = MakeSequentialTiles(/*count=*/24, /*start_tile_id=*/100);

  RoomObject first_lock(0x0F98, /*x=*/20, /*y=*/20, /*size=*/0, /*layer=*/0);
  first_lock.tiles_loaded_ = true;
  first_lock.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/900);
  RoomObject second_lock(0x0F98, /*x=*/24, /*y=*/24, /*size=*/0, /*layer=*/0);
  second_lock.tiles_loaded_ = true;
  second_lock.tiles_ = MakeSequentialTiles(/*count=*/4, /*start_tile_id=*/904);

  FakeDungeonState state;
  state.open_chest_slots.insert({0x42, 0});
  state.open_big_key_lock_slots.insert({0x42, 0});
  state.open_big_key_lock_slots.insert({0x42, 1});

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;
  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer
                  .DrawObjectList({first_lock, big_chest, second_lock}, bg1,
                                  bg2, palette_group, &state)
                  .ok());
  EXPECT_EQ(state.chest_queries, (std::vector<std::pair<int, int>>{{0x42, 0}}));
  EXPECT_EQ(state.big_key_lock_queries,
            (std::vector<std::pair<int, int>>{{0x42, 0}, {0x42, 1}}));
  ExpectTraceMatchesSnapshot(
      trace, MakeColumnMajorSnapshot(/*x=*/2, /*y=*/2, /*width=*/4,
                                     /*height=*/3, /*start_tile_id=*/112));
}

TEST(ObjectDrawerRegistryReplayTest,
     LegacyBigChestOverrideDoesNotOpenSmallChests) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  RoomObject small_chest(0x0F99, /*x=*/2, /*y=*/2, /*size=*/0, /*layer=*/0);
  small_chest.tiles_loaded_ = true;
  small_chest.tiles_ = MakeSequentialTiles(/*count=*/8, /*start_tile_id=*/100);
  RoomObject big_chest(0x0FB1, /*x=*/10, /*y=*/10, /*size=*/0, /*layer=*/0);
  big_chest.tiles_loaded_ = true;
  big_chest.tiles_ = MakeSequentialTiles(/*count=*/24, /*start_tile_id=*/200);

  FakeDungeonState state;
  state.big_chest_open = true;

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;
  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer
                  .DrawObjectList({small_chest, big_chest}, bg1, bg2,
                                  palette_group, &state)
                  .ok());
  EXPECT_EQ(state.chest_queries,
            (std::vector<std::pair<int, int>>{{0x42, 0}, {0x42, 1}}));
  ASSERT_EQ(trace.size(), 16u);
  ExpectTraceMatchesSnapshot(
      std::vector<ObjectDrawer::TileTrace>(trace.begin(), trace.begin() + 4),
      MakeColumnMajorSnapshot(/*x=*/2, /*y=*/2, /*width=*/2, /*height=*/2,
                              /*start_tile_id=*/100));
  ExpectTraceMatchesSnapshot(
      std::vector<ObjectDrawer::TileTrace>(trace.begin() + 4, trace.end()),
      MakeColumnMajorSnapshot(/*x=*/10, /*y=*/10, /*width=*/4, /*height=*/3,
                              /*start_tile_id=*/212));
}

TEST(ObjectDrawerRegistryReplayTest,
     EmptyWaterFaceUsesStateSensitiveRowsAndTileBlock) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  RoomObject water_face(0x0F80, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  water_face.tiles_loaded_ = true;
  water_face.tiles_.clear();
  for (int i = 0; i < 32; ++i) {
    water_face.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i),
                                              /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  // Default branch: 4x3 using tiles[0..11].
  ASSERT_TRUE(drawer.DrawObject(water_face, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 12u);
  EXPECT_EQ(trace[0].x_tile, 10);
  EXPECT_EQ(trace[0].y_tile, 20);
  EXPECT_EQ(trace[0].tile_id, 0);
  EXPECT_EQ(trace[3].x_tile, 13);
  EXPECT_EQ(trace[3].y_tile, 20);
  EXPECT_EQ(trace[3].tile_id, 3);
  EXPECT_EQ(trace[11].x_tile, 13);
  EXPECT_EQ(trace[11].y_tile, 22);
  EXPECT_EQ(trace[11].tile_id, 11);

  // Door-open state must NOT affect water-face rendering.
  FakeDungeonState unrelated_door_state;
  unrelated_door_state.open_lock_room_id = 0x42;
  trace.clear();
  ASSERT_TRUE(drawer
                  .DrawObject(water_face, bg1, bg2, palette_group,
                              &unrelated_door_state)
                  .ok());
  ASSERT_EQ(trace.size(), 12u);
  EXPECT_EQ(trace[0].tile_id, 0);
  EXPECT_EQ(trace[11].tile_id, 11);

  // Active-water branch: 4x5 using shifted block tiles[12..31].
  FakeDungeonState state;
  state.water_face_active_room_id = 0x42;
  trace.clear();
  ASSERT_TRUE(
      drawer.DrawObject(water_face, bg1, bg2, palette_group, &state).ok());
  ASSERT_EQ(trace.size(), 20u);
  EXPECT_EQ(trace[0].x_tile, 10);
  EXPECT_EQ(trace[0].y_tile, 20);
  EXPECT_EQ(trace[0].tile_id, 12);
  EXPECT_EQ(trace[19].x_tile, 13);
  EXPECT_EQ(trace[19].y_tile, 24);
  EXPECT_EQ(trace[19].tile_id, 31);
}

TEST(ObjectDrawerRegistryReplayTest, SpittingWaterFaceDraws4x5RowMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject water_face(0x0F81, /*x=*/6, /*y=*/7, /*size=*/0, /*layer=*/0);
  water_face.tiles_loaded_ = true;
  water_face.tiles_.clear();
  for (int i = 0; i < 20; ++i) {
    water_face.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(100 + i),
                                              /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(water_face, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 20u);
  EXPECT_EQ(trace[0].x_tile, 6);
  EXPECT_EQ(trace[0].y_tile, 7);
  EXPECT_EQ(trace[0].tile_id, 100);
  EXPECT_EQ(trace[3].x_tile, 9);
  EXPECT_EQ(trace[3].y_tile, 7);
  EXPECT_EQ(trace[3].tile_id, 103);
  EXPECT_EQ(trace[4].x_tile, 6);
  EXPECT_EQ(trace[4].y_tile, 8);
  EXPECT_EQ(trace[4].tile_id, 104);
  EXPECT_EQ(trace[19].x_tile, 9);
  EXPECT_EQ(trace[19].y_tile, 11);
  EXPECT_EQ(trace[19].tile_id, 119);
}

TEST(ObjectDrawerRegistryReplayTest, DrenchingWaterFaceDraws4x7RowMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject water_face(0x0F82, /*x=*/3, /*y=*/5, /*size=*/0, /*layer=*/0);
  water_face.tiles_loaded_ = true;
  water_face.tiles_.clear();
  for (int i = 0; i < 28; ++i) {
    water_face.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(200 + i),
                                              /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(water_face, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 28u);
  EXPECT_EQ(trace[0].x_tile, 3);
  EXPECT_EQ(trace[0].y_tile, 5);
  EXPECT_EQ(trace[0].tile_id, 200);
  EXPECT_EQ(trace[3].x_tile, 6);
  EXPECT_EQ(trace[3].y_tile, 5);
  EXPECT_EQ(trace[3].tile_id, 203);
  EXPECT_EQ(trace[4].x_tile, 3);
  EXPECT_EQ(trace[4].y_tile, 6);
  EXPECT_EQ(trace[4].tile_id, 204);
  EXPECT_EQ(trace[27].x_tile, 6);
  EXPECT_EQ(trace[27].y_tile, 11);
  EXPECT_EQ(trace[27].tile_id, 227);
}

TEST(ObjectDrawerRegistryReplayTest, HammerPegDrawsSingleTwoByTwoAtAnchor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0F96, kX, kY,
      /*size=*/9, RoomObject::LayerType::BG1, MakeSequentialTiles(/*count=*/4));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  ExpectTraceMatchesSnapshot(bg1, MakeColumnMajorSnapshot(kX, kY, 2, 2, 0));
  EXPECT_TRUE(bg2.empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     TableBowlDrawsFixedFourByTwoRowMajorOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0240;

  for (uint8_t size : {uint8_t{0}, uint8_t{10}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace = ReplayObjectTrace(
          /*object_id=*/0x0FDA, kX, kY, size, layer,
          MakeSequentialTiles(/*count=*/8, /*start_tile_id=*/kFirstTile));
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
      const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

      ExpectTraceMatchesSnapshot(
          selected, MakeRowMajorSnapshot(kX, kY, /*width=*/4, /*height=*/2,
                                         /*start_tile_id=*/kFirstTile));
      EXPECT_TRUE(other.empty());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SmithyFurnaceDrawsFixedSixByEightRowMajorOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0200;

  for (uint8_t size : {uint8_t{0}, uint8_t{3}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace = ReplayObjectTrace(
          /*object_id=*/0x0FCC, kX, kY, size, layer,
          MakeSequentialTiles(/*count=*/48, /*start_tile_id=*/kFirstTile));
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
      const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

      ExpectTraceMatchesSnapshot(
          selected, MakeRowMajorSnapshot(kX, kY, /*width=*/6, /*height=*/8,
                                         /*start_tile_id=*/kFirstTile));
      EXPECT_TRUE(other.empty());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     BigGrayRockDrawsFixedFourByFourQuadrantsOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0200;

  for (uint8_t size : {uint8_t{0}, uint8_t{3}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace = ReplayObjectTrace(
          /*object_id=*/0x0FAC, kX, kY, size, layer,
          MakeSequentialTiles(/*count=*/16, /*start_tile_id=*/kFirstTile));
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
      const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

      ExpectTraceMatchesSnapshot(selected,
                                 MakeBigGrayRockSnapshot(kX, kY, kFirstTile));
      EXPECT_TRUE(other.empty());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     AgahnimsAltarDrawsMirroredFourteenByFourteenOnFixedBg1) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0100;
  constexpr std::array<int, 14> kSourceColumns = {
      0, 1, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 1, 0,
  };

  auto tiles = MakeSequentialTiles(/*count=*/84, /*start_tile_id=*/kFirstTile);
  for (size_t i = 0; i < tiles.size(); ++i) {
    tiles[i].horizontal_mirror_ = (i % 3) == 0;
  }

  for (uint8_t size : {uint8_t{0}, uint8_t{3}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace =
          ReplayObjectTrace(/*object_id=*/0x0FAD, kX, kY, size, layer, tiles);
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      ASSERT_EQ(bg1.size(), 196u);
      EXPECT_TRUE(bg2.empty());

      size_t write_index = 0;
      for (int y = 0; y < 14; ++y) {
        for (int x = 0; x < 14; ++x, ++write_index) {
          const size_t source_index = kSourceColumns[x] * 14 + y;
          EXPECT_EQ(bg1[write_index].x_tile, kX + x);
          EXPECT_EQ(bg1[write_index].y_tile, kY + y);
          EXPECT_EQ(bg1[write_index].tile_id,
                    static_cast<uint16_t>(kFirstTile + source_index));

          bool expected_h_flip = tiles[source_index].horizontal_mirror_;
          if (x >= 7 && x <= 12) {
            expected_h_flip = !expected_h_flip;
          } else if (x == 13) {
            expected_h_flip = true;
          }
          EXPECT_EQ((bg1[write_index].flags & 0x1) != 0, expected_h_flip)
              << "x=" << x << " y=" << y;
        }
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     FortuneTellerRoomDrawsSparseFourteenByFourteenOnFixedBg1) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0100;

  auto tiles = MakeSequentialTiles(/*count=*/26, /*start_tile_id=*/kFirstTile);
  for (size_t i = 0; i < tiles.size(); ++i) {
    tiles[i].horizontal_mirror_ = (i % 3) == 0;
  }

  for (uint8_t size : {uint8_t{0}, uint8_t{3}}) {
    for (const auto layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "size=" << static_cast<int>(size)
                   << " layer=" << static_cast<int>(layer));

      const auto trace =
          ReplayObjectTrace(/*object_id=*/0x0FD4, kX, kY, size, layer, tiles);
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      ASSERT_EQ(bg1.size(), 116u);
      EXPECT_TRUE(bg2.empty());
      ExpectTraceBounds(bg1, kX, kY, kX + 13, kY + 13);

      size_t write_index = 0;
      auto expect_write = [&](int dx, int dy, size_t source_index,
                              bool force_horizontal_mirror = false,
                              bool toggle_horizontal_mirror = false) {
        ASSERT_LT(write_index, bg1.size());
        const auto& actual = bg1[write_index++];
        EXPECT_EQ(actual.x_tile, kX + dx);
        EXPECT_EQ(actual.y_tile, kY + dy);
        EXPECT_EQ(actual.tile_id,
                  static_cast<uint16_t>(kFirstTile + source_index));

        bool expected_h_flip = tiles[source_index].horizontal_mirror_;
        if (force_horizontal_mirror) {
          expected_h_flip = true;
        } else if (toggle_horizontal_mirror) {
          expected_h_flip = !expected_h_flip;
        }
        EXPECT_EQ((actual.flags & 0x1) != 0, expected_h_flip)
            << "source_index=" << source_index << " dx=" << dx << " dy=" << dy;
      };

      for (int section = 0; section < 6; ++section) {
        const int x = section * 2;
        expect_write(x + 1, 0, 0);
        expect_write(x + 2, 0, 0);
        expect_write(x + 1, 1, 0);
        expect_write(x + 2, 1, 0);
        expect_write(x + 1, 2, 1);
        expect_write(x + 2, 2, 1, /*force_horizontal_mirror=*/true);
      }

      for (int row = 0; row < 3; ++row) {
        const size_t wall_tile = static_cast<size_t>(2 + row);
        const size_t center_tile = static_cast<size_t>(5 + row);
        const int y = 3 + row;
        for (int x : {0, 2, 10, 12}) {
          expect_write(x, y, wall_tile);
        }
        for (int x : {1, 3, 11, 13}) {
          expect_write(x, y, wall_tile,
                       /*force_horizontal_mirror=*/true);
        }
        for (int x : {4, 6, 8}) {
          expect_write(x, y, center_tile);
        }
        for (int x : {5, 7, 9}) {
          expect_write(x, y, center_tile,
                       /*force_horizontal_mirror=*/true);
        }
      }

      expect_write(0, 0, 8);
      expect_write(0, 1, 8);
      expect_write(13, 0, 8, /*force_horizontal_mirror=*/true);
      expect_write(13, 1, 8, /*force_horizontal_mirror=*/true);
      expect_write(0, 2, 9);
      expect_write(13, 2, 9, /*force_horizontal_mirror=*/true);

      for (int row = 0; row < 4; ++row) {
        const int y = 10 + row;
        expect_write(3, y, static_cast<size_t>(10 + row));
        expect_write(10, y, static_cast<size_t>(10 + row),
                     /*force_horizontal_mirror=*/false,
                     /*toggle_horizontal_mirror=*/true);
        expect_write(4, y, static_cast<size_t>(14 + row));
        expect_write(9, y, static_cast<size_t>(14 + row),
                     /*force_horizontal_mirror=*/false,
                     /*toggle_horizontal_mirror=*/true);
        expect_write(5, y, static_cast<size_t>(18 + row));
        expect_write(8, y, static_cast<size_t>(18 + row),
                     /*force_horizontal_mirror=*/false,
                     /*toggle_horizontal_mirror=*/true);
        expect_write(6, y, static_cast<size_t>(22 + row));
        expect_write(7, y, static_cast<size_t>(22 + row),
                     /*force_horizontal_mirror=*/false,
                     /*toggle_horizontal_mirror=*/true);
      }
      EXPECT_EQ(write_index, bg1.size());
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     BigWallDecorAliasesDrawFixedEightByThreeOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0240;
  struct DecorCase {
    int16_t object_id;
    uint8_t oos_size;
  };
  constexpr DecorCase kCases[] = {
      {0x0FCB, 14},
      {0x0FF6, 9},
      {0x0FF7, 13},
  };

  for (const auto& test_case : kCases) {
    for (uint8_t size : {uint8_t{0}, test_case.oos_size}) {
      for (const auto layer :
           {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
        SCOPED_TRACE(::testing::Message()
                     << "object_id=0x" << std::hex << test_case.object_id
                     << " size=" << std::dec << static_cast<int>(size)
                     << " layer=" << static_cast<int>(layer));

        const auto trace = ReplayObjectTrace(
            test_case.object_id, kX, kY, size, layer,
            MakeSequentialTiles(/*count=*/24,
                                /*start_tile_id=*/kFirstTile));
        const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
        const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
        const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
        const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;

        ExpectTraceMatchesSnapshot(
            selected, MakeColumnMajorSnapshot(kX, kY, /*width=*/8, /*height=*/3,
                                              /*start_tile_id=*/kFirstTile));
        EXPECT_TRUE(other.empty());
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     TurtleRockPipe25CDrawsAllTwentyFourTilesWithoutWrapping) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0240;
  const auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FDC, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/24, /*start_tile_id=*/kFirstTile));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  ExpectTraceMatchesSnapshot(
      bg1, MakeColumnMajorSnapshot(kX, kY, /*width=*/6, /*height=*/4,
                                   /*start_tile_id=*/kFirstTile));
  EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     VerticalJumpLedgesDrawOneTileForSizePlusEightRows) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kTile = 0x0240;
  for (int object_id : {0x008B, 0x008C}) {
    for (uint8_t size : {uint8_t{0}, uint8_t{5}, uint8_t{15}}) {
      SCOPED_TRACE(::testing::Message()
                   << "object_id=0x" << std::hex << object_id
                   << " size=" << std::dec << static_cast<int>(size));
      auto trace = ReplayObjectTrace(
          object_id, kX, kY, size, RoomObject::LayerType::BG1,
          MakeSequentialTiles(/*count=*/3, /*start_tile_id=*/kTile));

      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const int expected_rows = static_cast<int>(size) + 8;
      ASSERT_EQ(static_cast<int>(bg1.size()), expected_rows);
      EXPECT_TRUE(bg2.empty());
      for (int row = 0; row < expected_rows; ++row) {
        EXPECT_EQ(bg1[row].x_tile, kX);
        EXPECT_EQ(bg1[row].y_tile, kY + row);
        EXPECT_EQ(bg1[row].tile_id, kTile);
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     ArcheryCurtainsRepeatTwoByFourStampOnSelectedLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 10;
  constexpr int kY = 12;
  constexpr uint16_t kFirstTile = 0x0240;
  for (const auto layer :
       {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
    for (uint8_t size : {uint8_t{0}, uint8_t{5}, uint8_t{15}}) {
      SCOPED_TRACE(::testing::Message() << "layer=" << static_cast<int>(layer)
                                        << " size=" << static_cast<int>(size));
      auto trace = ReplayObjectTrace(
          /*object_id=*/0x00B5, kX, kY, size, layer,
          MakeSequentialTiles(/*count=*/8, /*start_tile_id=*/kFirstTile));

      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
      const auto& selected = layer == RoomObject::LayerType::BG1 ? bg1 : bg2;
      const auto& other = layer == RoomObject::LayerType::BG1 ? bg2 : bg1;
      const int block_count = static_cast<int>(size) + 1;
      ASSERT_EQ(static_cast<int>(selected.size()), block_count * 8);
      EXPECT_TRUE(other.empty());

      for (int block = 0; block < block_count; ++block) {
        for (int column = 0; column < 2; ++column) {
          for (int row = 0; row < 4; ++row) {
            const int trace_index = block * 8 + column * 4 + row;
            EXPECT_EQ(selected[trace_index].x_tile, kX + block * 2 + column);
            EXPECT_EQ(selected[trace_index].y_tile, kY + row);
            EXPECT_EQ(selected[trace_index].tile_id,
                      kFirstTile + column * 4 + row);
          }
        }
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     ArcheryGameTargetDoorDrawsTwoStacked3x3Sections) {
  ScopedCustomObjectsFlag disable_custom(false);

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FE0, /*x=*/8, /*y=*/9, /*size=*/0,
      RoomObject::LayerType::BG1, MakeSequentialTiles(/*count=*/18));

  const auto bg1_trace = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto expected_top =
      MakeColumnMajorSnapshot(/*x=*/8, /*y=*/9, /*width=*/3, /*height=*/3,
                              /*start_tile_id=*/0);
  const auto expected_bottom =
      MakeColumnMajorSnapshot(/*x=*/8, /*y=*/12, /*width=*/3, /*height=*/3,
                              /*start_tile_id=*/9);

  ASSERT_EQ(bg1_trace.size(), expected_top.size() + expected_bottom.size());
  for (size_t i = 0; i < expected_top.size(); ++i) {
    EXPECT_EQ(bg1_trace[i].x_tile, expected_top[i].x) << "top idx=" << i;
    EXPECT_EQ(bg1_trace[i].y_tile, expected_top[i].y) << "top idx=" << i;
    EXPECT_EQ(bg1_trace[i].tile_id, expected_top[i].tile_id) << "top idx=" << i;
  }
  for (size_t i = 0; i < expected_bottom.size(); ++i) {
    const size_t trace_index = expected_top.size() + i;
    EXPECT_EQ(bg1_trace[trace_index].x_tile, expected_bottom[i].x)
        << "bottom idx=" << i;
    EXPECT_EQ(bg1_trace[trace_index].y_tile, expected_bottom[i].y)
        << "bottom idx=" << i;
    EXPECT_EQ(bg1_trace[trace_index].tile_id, expected_bottom[i].tile_id)
        << "bottom idx=" << i;
  }
}

constexpr uint8_t kBG2ObjectRevealMask =
    static_cast<uint8_t>(gfx::BG1RevealMaskSource::kBG2Objects);

TEST(ObjectDrawerMaskPropagationTest, Layer2PitMaskRecordsBG1Reveal) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  // Provide non-zero gfx so any draw calls can write pixels if needed.
  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  obj_bg1.EnsureBitmapInitialized();
  obj_bg2.EnsureBitmapInitialized();
  layout_bg1.EnsureBitmapInitialized();

  // Fill BG1 buffers with a non-transparent value so we can observe the mask.
  obj_bg1.bitmap().Fill(10);
  layout_bg1.bitmap().Fill(11);
  obj_bg2.bitmap().Fill(255);
  obj_bg1.ClearPriorityBuffer();
  obj_bg2.ClearPriorityBuffer();
  layout_bg1.ClearPriorityBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  // 0xC2 is "Layer 2 pit mask (large)" and should clear BG1 to transparent
  // in the object's covered area when drawn on BG2.
  RoomObject obj(0x00C2, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));

  gfx::PaletteGroup palette_group;

  const int px = obj.x_ * 8;
  const int py = obj.y_ * 8;
  const int idx = py * obj_bg1.bitmap().width() + px;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(obj_bg1.bitmap().size()));
  ASSERT_NE(obj_bg1.bitmap().data()[idx], 255);
  ASSERT_NE(layout_bg1.bitmap().data()[idx], 255);

  ASSERT_TRUE(drawer
                  .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 10);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 11);
  EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
  EXPECT_NE(layout_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
}

TEST(ObjectDrawerMaskPropagationTest, Layer2LargeCeilingMasksBG1Transparent) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  obj_bg1.EnsureBitmapInitialized();
  obj_bg2.EnsureBitmapInitialized();
  layout_bg1.EnsureBitmapInitialized();

  obj_bg1.bitmap().Fill(10);
  layout_bg1.bitmap().Fill(11);
  obj_bg2.bitmap().Fill(255);
  obj_bg1.ClearPriorityBuffer();
  obj_bg2.ClearPriorityBuffer();
  layout_bg1.ClearPriorityBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  RoomObject obj(0x00C0, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));

  gfx::PaletteGroup palette_group;

  const int px = obj.x_ * 8;
  const int py = obj.y_ * 8;
  const int idx = py * obj_bg1.bitmap().width() + px;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(obj_bg1.bitmap().size()));

  ASSERT_TRUE(drawer
                  .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 10);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 11);
  EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
  EXPECT_NE(layout_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
}

TEST(ObjectDrawerMaskPropagationTest, Layer2WaterFloorMasksBG1Transparent) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  obj_bg1.EnsureBitmapInitialized();
  obj_bg2.EnsureBitmapInitialized();
  layout_bg1.EnsureBitmapInitialized();

  obj_bg1.bitmap().Fill(10);
  layout_bg1.bitmap().Fill(11);
  obj_bg2.bitmap().Fill(255);
  obj_bg1.ClearPriorityBuffer();
  obj_bg2.ClearPriorityBuffer();
  layout_bg1.ClearPriorityBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  RoomObject obj(0x00C8, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  gfx::PaletteGroup palette_group;

  const int px = obj.x_ * 8;
  const int py = obj.y_ * 8;
  const int idx = py * obj_bg1.bitmap().width() + px;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(obj_bg1.bitmap().size()));

  ASSERT_TRUE(drawer
                  .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 10);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 11);
  EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
  EXPECT_NE(layout_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0);
}

TEST(ObjectDrawerMaskPropagationTest, Layer2FloodWaterMasksBG1Transparent) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::PaletteGroup palette_group;

  for (const int object_id : {0x00D8, 0x00DA}) {
    gfx::BackgroundBuffer obj_bg1(512, 512);
    gfx::BackgroundBuffer obj_bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    obj_bg1.EnsureBitmapInitialized();
    obj_bg2.EnsureBitmapInitialized();
    layout_bg1.EnsureBitmapInitialized();

    obj_bg1.bitmap().Fill(10);
    layout_bg1.bitmap().Fill(11);
    obj_bg2.bitmap().Fill(255);
    obj_bg1.ClearPriorityBuffer();
    obj_bg2.ClearPriorityBuffer();
    layout_bg1.ClearPriorityBuffer();

    ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

    RoomObject obj(object_id, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
    obj.tiles_loaded_ = true;
    obj.tiles_.clear();
    for (int i = 0; i < 8; ++i) {
      obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                         false, false, false));
    }

    const int px = obj.x_ * 8;
    const int py = obj.y_ * 8;
    const int idx = py * obj_bg1.bitmap().width() + px;
    ASSERT_GE(idx, 0);
    ASSERT_LT(idx, static_cast<int>(obj_bg1.bitmap().size()));

    ASSERT_TRUE(drawer
                    .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                                /*state=*/nullptr,
                                /*layout_bg1=*/&layout_bg1)
                    .ok());

    EXPECT_EQ(obj_bg1.bitmap().data()[idx], 10) << object_id;
    EXPECT_EQ(layout_bg1.bitmap().data()[idx], 11) << object_id;
    EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0)
        << object_id;
    EXPECT_NE(layout_bg1.bg1_reveal_mask_data()[idx] & kBG2ObjectRevealMask, 0)
        << object_id;
  }
}

TEST(ObjectDrawerMaskPropagationTest,
     Layer2OverlayUsesPerPixelMaskInsteadOfRectangularClear) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(0);
  gfx[0] = 1;  // Only the tile's top-left pixel is opaque.

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  obj_bg1.EnsureBitmapInitialized();
  obj_bg2.EnsureBitmapInitialized();
  layout_bg1.EnsureBitmapInitialized();

  obj_bg1.bitmap().Fill(10);
  layout_bg1.bitmap().Fill(11);
  obj_bg2.bitmap().Fill(255);
  obj_bg1.ClearPriorityBuffer();
  obj_bg2.ClearPriorityBuffer();
  layout_bg1.ClearPriorityBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  RoomObject obj(0x0034, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  obj.tiles_.push_back(gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));

  gfx::PaletteGroup palette_group;

  const int base_x = (obj.x_ + 3) * 8;
  const int base_y = obj.y_ * 8;
  const int opaque_idx = base_y * obj_bg1.bitmap().width() + base_x;
  const int transparent_idx = opaque_idx + 1;
  ASSERT_LT(transparent_idx, static_cast<int>(obj_bg1.bitmap().size()));
  const auto obj_bg1_before = obj_bg1.bitmap().vector();
  const auto layout_bg1_before = layout_bg1.bitmap().vector();

  ASSERT_TRUE(drawer
                  .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());

  EXPECT_EQ(obj_bg1.bitmap().vector(), obj_bg1_before);
  EXPECT_EQ(layout_bg1.bitmap().vector(), layout_bg1_before);
  EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask,
            0);
  EXPECT_NE(
      layout_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask, 0);
  EXPECT_EQ(
      obj_bg1.bg1_reveal_mask_data()[transparent_idx] & kBG2ObjectRevealMask,
      0);
  EXPECT_EQ(
      layout_bg1.bg1_reveal_mask_data()[transparent_idx] & kBG2ObjectRevealMask,
      0);
}

TEST(ObjectDrawerMaskPropagationTest,
     LaterBG1WriteClearsOnlyItsStreamRevealBit) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(1024 * 1024, 0)).ok());

  std::array<uint8_t, 0x10000> gfx{};
  gfx[0] = 1;

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  for (auto* buffer : {&obj_bg1, &obj_bg2, &layout_bg1}) {
    buffer->EnsureBitmapInitialized();
    buffer->bitmap().Fill(255);
  }

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());
  RoomObject lower(0x0034, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  lower.tiles_loaded_ = true;
  lower.tiles_ = {gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false)};
  gfx::PaletteGroup palette_group;

  const int pixel_x = (lower.x_ + 3) * 8;
  const int pixel_y = lower.y_ * 8;
  const int bitmap_width = obj_bg1.bitmap().width();
  const int opaque_index = pixel_y * bitmap_width + pixel_x;
  const int transparent_index = opaque_index + 1;
  const int outside_index = opaque_index + 8;
  ASSERT_TRUE(drawer
                  .DrawObject(lower, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());
  ASSERT_NE(obj_bg1.bg1_reveal_mask_data()[opaque_index] & kBG2ObjectRevealMask,
            0);

  obj_bg1.SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Objects, pixel_x,
                               pixel_y, 8, 8);
  obj_bg1.SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Objects,
                               pixel_x + 8, pixel_y, 1, 1);
  obj_bg1.SetBG1RevealMaskRect(gfx::BG1RevealMaskSource::kBG2Layout, pixel_x,
                               pixel_y, 8, 8);
  ASSERT_NE(
      obj_bg1.bg1_reveal_mask_data()[transparent_index] & kBG2ObjectRevealMask,
      0);
  ASSERT_NE(
      obj_bg1.bg1_reveal_mask_data()[outside_index] & kBG2ObjectRevealMask, 0);

  RoomObject later_upper = lower;
  later_upper.layer_ = RoomObject::LayerType::BG1;
  ASSERT_TRUE(
      drawer.DrawObject(later_upper, obj_bg1, obj_bg2, palette_group).ok());

  const uint8_t layout_reveal_mask =
      static_cast<uint8_t>(gfx::BG1RevealMaskSource::kBG2Layout);
  for (int dy = 0; dy < 8; ++dy) {
    for (int dx = 0; dx < 8; ++dx) {
      const int inside_index = (pixel_y + dy) * bitmap_width + pixel_x + dx;
      const uint8_t remaining = obj_bg1.bg1_reveal_mask_data()[inside_index];
      EXPECT_EQ(remaining & kBG2ObjectRevealMask, 0)
          << "pixel=(" << dx << "," << dy << ")";
      EXPECT_NE(remaining & layout_reveal_mask, 0)
          << "pixel=(" << dx << "," << dy << ")";
    }
  }
  EXPECT_NE(
      obj_bg1.bg1_reveal_mask_data()[outside_index] & kBG2ObjectRevealMask, 0);
  EXPECT_NE(obj_bg1.bitmap().data()[opaque_index], 255);
  EXPECT_EQ(obj_bg1.bitmap().data()[transparent_index], 255);
  EXPECT_NE(
      layout_bg1.bg1_reveal_mask_data()[opaque_index] & kBG2ObjectRevealMask,
      0);
}

TEST(ObjectDrawerMaskPropagationTest, StoredBg2SpiralStairsUsePerPixelMasking) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(0);
  gfx[0] = 1;  // Only the tile's top-left pixel is opaque.

  gfx::BackgroundBuffer obj_bg1(512, 512);
  gfx::BackgroundBuffer obj_bg2(512, 512);
  gfx::BackgroundBuffer layout_bg1(512, 512);
  obj_bg1.EnsureBitmapInitialized();
  obj_bg2.EnsureBitmapInitialized();
  layout_bg1.EnsureBitmapInitialized();

  obj_bg1.bitmap().Fill(10);
  layout_bg1.bitmap().Fill(11);
  obj_bg2.bitmap().Fill(255);
  obj_bg1.ClearPriorityBuffer();
  obj_bg2.ClearPriorityBuffer();
  layout_bg1.ClearPriorityBuffer();

  ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

  RoomObject obj(0x013B, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 12; ++i) {
    obj.tiles_.push_back(
        gfx::TileInfo(/*id=*/0, /*pal=*/2, false, false, false));
  }

  gfx::PaletteGroup palette_group;

  const int base_x = obj.x_ * 8;
  const int base_y = obj.y_ * 8;
  const int opaque_idx = base_y * obj_bg1.bitmap().width() + base_x;
  const int transparent_idx = opaque_idx + 1;
  ASSERT_LT(transparent_idx, static_cast<int>(obj_bg1.bitmap().size()));
  ASSERT_TRUE(drawer
                  .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                              /*state=*/nullptr, /*layout_bg1=*/&layout_bg1)
                  .ok());

  EXPECT_EQ(obj_bg1.bitmap().data()[opaque_idx], 10);
  EXPECT_EQ(layout_bg1.bitmap().data()[opaque_idx], 11);
  EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask,
            0);
  EXPECT_NE(
      layout_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask, 0);
  EXPECT_EQ(
      obj_bg1.bg1_reveal_mask_data()[transparent_idx] & kBG2ObjectRevealMask,
      0);
  EXPECT_EQ(
      layout_bg1.bg1_reveal_mask_data()[transparent_idx] & kBG2ObjectRevealMask,
      0);
}

TEST(ObjectDrawerRegistryReplayTest,
     AutoStairsNorthMultiLayerDrawsToBothLayersColumnMajor) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 4;
  constexpr int kY = 6;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0130, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 4, 4, 0);

  ExpectTraceMatchesSnapshot(bg1, expected);
  ExpectTraceMatchesSnapshot(bg2, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     AutoStairsMergedLayerKeepsTargetLayerAndColumnMajorOrder) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 5;
  constexpr int kY = 7;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0132, kX, kY, /*size=*/0, RoomObject::LayerType::BG2,
      MakeSequentialTiles(/*count=*/16));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 4, 4, 0);

  EXPECT_TRUE(bg1.empty());
  ExpectTraceMatchesSnapshot(bg2, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     SpiralStairsRastersFollowStoredLayerInColumnMajorOrder) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 3;
  constexpr int kY = 5;
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 4, 3, 0);

  for (const int object_id : {0x0138, 0x0139, 0x013A, 0x013B}) {
    for (const auto stored_layer :
         {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
      SCOPED_TRACE(::testing::Message()
                   << "object=0x" << std::hex << object_id
                   << " layer=" << static_cast<int>(stored_layer));
      const auto trace =
          ReplayObjectTrace(object_id, kX, kY, /*size=*/0, stored_layer,
                            MakeSequentialTiles(/*count=*/12));
      const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
      const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);

      if (stored_layer == RoomObject::LayerType::BG1) {
        ExpectTraceMatchesSnapshot(bg1, expected);
        EXPECT_TRUE(bg2.empty());
      } else {
        EXPECT_TRUE(bg1.empty());
        ExpectTraceMatchesSnapshot(bg2, expected);
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     SpiralStairsPromoteFixedMapFlanksWithoutRasterOrCoverage) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> room_gfx{};
  room_gfx.fill(1);
  ObjectDrawer drawer(&rom, /*room_id=*/0x77, room_gfx.data());
  gfx::PaletteGroup palette_group;

  struct SpiralCase {
    int object_id;
    RoomObject::LayerType stored_layer;
    bool priority_on_upper;
  };
  const std::array<SpiralCase, 4> cases = {{
      {0x0138, RoomObject::LayerType::BG2, true},
      {0x0139, RoomObject::LayerType::BG2, true},
      {0x013A, RoomObject::LayerType::BG1, false},
      {0x013B, RoomObject::LayerType::BG1, false},
  }};

  constexpr int kX = 3;
  constexpr int kY = 5;
  for (const auto& test_case : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "object=0x" << std::hex << test_case.object_id);
    gfx::BackgroundBuffer object_upper(512, 512);
    gfx::BackgroundBuffer object_lower(512, 512);
    gfx::BackgroundBuffer layout_upper(512, 512);
    gfx::BackgroundBuffer layout_lower(512, 512);
    for (auto* buffer :
         {&object_upper, &object_lower, &layout_upper, &layout_lower}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    RoomObject object(test_case.object_id, kX, kY, /*size=*/0,
                      static_cast<int>(test_case.stored_layer));
    object.tiles_loaded_ = true;
    object.tiles_ = MakeSequentialTiles(/*count=*/12);

    ASSERT_TRUE(drawer
                    .DrawObject(object, object_upper, object_lower,
                                palette_group, /*state=*/nullptr, &layout_upper,
                                &layout_lower)
                    .ok());

    const auto& raster = test_case.stored_layer == RoomObject::LayerType::BG1
                             ? object_upper
                             : object_lower;
    const auto& other_object =
        test_case.stored_layer == RoomObject::LayerType::BG1 ? object_lower
                                                             : object_upper;
    ExpectOnlyCoverageRect(raster, kX, kY, /*width_tiles=*/4,
                           /*height_tiles=*/3);
    ExpectOnlyCoverageRect(other_object, 0, 0, 0, 0);

    const auto& fixed_object =
        test_case.priority_on_upper ? object_upper : object_lower;
    const auto& fixed_layout =
        test_case.priority_on_upper ? layout_upper : layout_lower;
    const auto& opposite_layout =
        test_case.priority_on_upper ? layout_lower : layout_upper;
    for (const int flank_x : {kX - 1, kX + 4}) {
      ExpectPriorityRectSet(fixed_object, flank_x, kY, 1, 1);
      ExpectPriorityRectSet(fixed_layout, flank_x, kY, 1, 1);
    }
    ExpectOnlyCoverageRect(fixed_layout, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(opposite_layout, 0, 0, 0, 0);
    ExpectOnlyPriorityRect(opposite_layout, 0, 0, 0, 0);
    ExpectBitmapFilledWith(fixed_layout, 255);
    ExpectBitmapFilledWith(opposite_layout, 255);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     StraightInterroomUpperAlwaysRendersOnBg1InColumnMajorOrder) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 4;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0F9E, kX, kY, /*size=*/0, RoomObject::LayerType::BG2,
      MakeSequentialTiles(/*count=*/16));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 4, 4, 0);

  ExpectTraceMatchesSnapshot(bg1, expected);
  EXPECT_TRUE(bg2.empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     StraightInterroomNorthLowerRendersBodyOnBg2AndFrontEdgeOnBg1) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 4;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FA6, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected_bg2 = MakeColumnMajorSnapshot(kX, kY, 4, 4, 0);
  const std::vector<SnapshotTileWrite> expected_bg1 = {
      {kX + 0, kY + 0, 0},
      {kX + 1, kY + 0, 4},
      {kX + 2, kY + 0, 8},
      {kX + 3, kY + 0, 12},
  };

  ExpectTraceMatchesSnapshot(bg2, expected_bg2);
  ExpectTraceMatchesSnapshot(bg1, expected_bg1);
}

TEST(ObjectDrawerRegistryReplayTest,
     StraightInterroomSouthLowerRendersBodyOnBg2AndFrontEdgeOnBg1) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 4;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FA8, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected_bg2 = MakeColumnMajorSnapshot(kX, kY, 4, 4, 0);
  const std::vector<SnapshotTileWrite> expected_bg1 = {
      {kX + 0, kY + 3, 3},
      {kX + 1, kY + 3, 7},
      {kX + 2, kY + 3, 11},
      {kX + 3, kY + 3, 15},
  };

  ExpectTraceMatchesSnapshot(bg2, expected_bg2);
  ExpectTraceMatchesSnapshot(bg1, expected_bg1);
}

TEST(ObjectDrawerRegistryReplayTest,
     StraightInterroomLowerPromotesFixedBg1ColumnWithoutPaintingIt) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(1024 * 1024, 0)).ok());
  std::array<uint8_t, 0x10000> room_gfx{};
  room_gfx.fill(1);
  gfx::PaletteGroup palette_group;

  struct Case {
    int16_t object_id;
    int priority_start_y;
    int bg1_raster_y;
  };
  constexpr int kX = 8;
  constexpr int kY = 8;
  const std::array<Case, 2> cases = {
      {{0x0FA6, kY - 4, kY}, {0x0FA8, kY + 4, kY + 3}}};

  for (const auto& test_case : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "object=0x" << std::hex << test_case.object_id);
    gfx::BackgroundBuffer object_bg1(512, 512);
    gfx::BackgroundBuffer object_bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    gfx::BackgroundBuffer layout_bg2(512, 512);
    for (auto* buffer : {&object_bg1, &object_bg2, &layout_bg1, &layout_bg2}) {
      InitializeEmptyDoorBuffer(*buffer);
    }

    ObjectDrawer drawer(&rom, /*room_id=*/0x51, room_gfx.data());
    RoomObject object(test_case.object_id, kX, kY, /*size=*/0,
                      RoomObject::LayerType::BG2);
    object.tiles_loaded_ = true;
    object.tiles_ = MakeSequentialTiles(/*count=*/16);
    ASSERT_TRUE(drawer
                    .DrawObject(object, object_bg1, object_bg2, palette_group,
                                /*state=*/nullptr, &layout_bg1, &layout_bg2)
                    .ok());

    ExpectOnlyCoverageRect(object_bg1, kX, test_case.bg1_raster_y,
                           /*width_tiles=*/4, /*height_tiles=*/1);
    ExpectOnlyCoverageRect(object_bg2, kX, kY, /*width_tiles=*/4,
                           /*height_tiles=*/4);
    ExpectOnlyCoverageRect(layout_bg1, 0, 0, 0, 0);
    ExpectOnlyCoverageRect(layout_bg2, 0, 0, 0, 0);

    ExpectPriorityRectSet(object_bg1, kX, test_case.priority_start_y,
                          /*width_tiles=*/1, /*height_tiles=*/4);
    ExpectPriorityRectSet(layout_bg1, kX, test_case.priority_start_y,
                          /*width_tiles=*/1, /*height_tiles=*/4);
    EXPECT_EQ(object_bg2.GetPriorityAt(kX * 8, test_case.priority_start_y * 8),
              0xFF);
    EXPECT_EQ(layout_bg2.GetPriorityAt(kX * 8, test_case.priority_start_y * 8),
              0xFF);

    const int priority_pixel = test_case.priority_start_y * 8 * 512 + kX * 8;
    ASSERT_LT(priority_pixel,
              static_cast<int>(object_bg1.bitmap().vector().size()));
    EXPECT_EQ(object_bg1.bitmap().vector()[priority_pixel], 255);
    EXPECT_EQ(layout_bg1.bitmap().vector()[priority_pixel], 255);
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     WaterHopStairsNorthUseSingleLayerRowMajor4x2Order) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 8;
  constexpr int kY = 9;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0135, kX, kY, /*size=*/0, RoomObject::LayerType::BG2,
      MakeSequentialTiles(/*count=*/8));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeRowMajorSnapshot(kX, kY, 4, 2, 0);

  EXPECT_TRUE(bg1.empty());
  ExpectTraceMatchesSnapshot(bg2, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     WaterHopStairsSouthDrawToBothLayersInRowMajor4x2Order) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 8;
  constexpr int kY = 9;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0136, kX, kY, /*size=*/0, RoomObject::LayerType::BG2,
      MakeSequentialTiles(/*count=*/8));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeRowMajorSnapshot(kX, kY, 4, 2, 0);

  ExpectTraceMatchesSnapshot(bg1, expected);
  ExpectTraceMatchesSnapshot(bg2, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     DamFloodGateUsesClosedTilesByDefaultInColumnMajor10x4Order) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 3;
  constexpr int kY = 4;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0137, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/80));

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 10, 4, 0);

  ExpectTraceMatchesSnapshot(bg1, expected);
  EXPECT_TRUE(bg2.empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     DamFloodGateUsesOpenTilesWhenStateIsActive) {
  ScopedCustomObjectsFlag disable_custom(false);

  FakeDungeonState state;
  state.dam_floodgate_open = true;

  constexpr int kX = 3;
  constexpr int kY = 4;
  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0137, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/80), &state);

  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);
  const auto expected = MakeColumnMajorSnapshot(kX, kY, 10, 4, 40);

  ExpectTraceMatchesSnapshot(bg1, expected);
}

TEST(ObjectDrawerMaskPropagationTest,
     DiagonalMaskBObjectsUsePerPixelBg1Masking) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(0);
  gfx[0] = 1;  // Only the top-left pixel of each tile is opaque.

  gfx::PaletteGroup palette_group;

  for (const int object_id : {0x00A9, 0x00AA, 0x00AB, 0x00AC}) {
    auto trace = ReplayObjectTrace(object_id, /*x=*/2, /*y=*/3, /*size=*/0,
                                   RoomObject::LayerType::BG2,
                                   MakeSequentialTiles(/*count=*/1));
    const auto bg2_trace =
        FilterTraceByLayer(trace, RoomObject::LayerType::BG2);
    ASSERT_FALSE(bg2_trace.empty()) << object_id;

    gfx::BackgroundBuffer obj_bg1(512, 512);
    gfx::BackgroundBuffer obj_bg2(512, 512);
    gfx::BackgroundBuffer layout_bg1(512, 512);
    obj_bg1.EnsureBitmapInitialized();
    obj_bg2.EnsureBitmapInitialized();
    layout_bg1.EnsureBitmapInitialized();

    obj_bg1.bitmap().Fill(10);
    layout_bg1.bitmap().Fill(11);
    obj_bg2.bitmap().Fill(255);
    obj_bg1.ClearPriorityBuffer();
    obj_bg2.ClearPriorityBuffer();
    layout_bg1.ClearPriorityBuffer();

    ObjectDrawer drawer(&rom, /*room_id=*/0, gfx.data());

    RoomObject obj(object_id, /*x=*/2, /*y=*/3, /*size=*/0, /*layer=*/1);
    obj.tiles_loaded_ = true;
    obj.tiles_ = MakeSequentialTiles(/*count=*/1);

    ASSERT_TRUE(drawer
                    .DrawObject(obj, obj_bg1, obj_bg2, palette_group,
                                /*state=*/nullptr,
                                /*layout_bg1=*/&layout_bg1)
                    .ok())
        << object_id;

    const int opaque_x = bg2_trace.front().x_tile * 8;
    const int opaque_y = bg2_trace.front().y_tile * 8;
    const int opaque_idx = opaque_y * obj_bg1.bitmap().width() + opaque_x;
    const int transparent_idx = opaque_idx + 1;

    ASSERT_LT(transparent_idx, static_cast<int>(obj_bg1.bitmap().size()));
    EXPECT_EQ(obj_bg1.bitmap().data()[opaque_idx], 10) << object_id;
    EXPECT_EQ(layout_bg1.bitmap().data()[opaque_idx], 11) << object_id;
    EXPECT_NE(obj_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask,
              0)
        << object_id;
    EXPECT_NE(
        layout_bg1.bg1_reveal_mask_data()[opaque_idx] & kBG2ObjectRevealMask, 0)
        << object_id;
    EXPECT_EQ(
        obj_bg1.bg1_reveal_mask_data()[transparent_idx] & kBG2ObjectRevealMask,
        0)
        << object_id;
    EXPECT_EQ(layout_bg1.bg1_reveal_mask_data()[transparent_idx] &
                  kBG2ObjectRevealMask,
              0)
        << object_id;
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     PrisonCellDrawsOnlyToObjectStreamTargetLayer) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  std::array<uint8_t, 0x10000> gfx{};
  gfx.fill(1);

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  bg1.EnsureBitmapInitialized();
  bg2.EnsureBitmapInitialized();
  bg1.bitmap().Fill(255);
  bg2.bitmap().Fill(255);

  ObjectDrawer drawer(&rom, /*room_id=*/0x10, gfx.data());

  // PrisonCell maps to routine 97. ZScream "0x20D" corresponds to 0xF8D in
  // our decoded Type 3 ID space (0xF80 + 0x0D).
  gfx::PaletteGroup palette_group;
  for (const auto layer :
       {RoomObject::LayerType::BG1, RoomObject::LayerType::BG2}) {
    SCOPED_TRACE(static_cast<int>(layer));
    RoomObject cell(0x0F8D, /*x=*/2, /*y=*/2, /*size=*/0,
                    static_cast<int>(layer));
    cell.tiles_loaded_ = true;
    cell.tiles_.clear();
    for (int i = 0; i < 6; ++i) {
      cell.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                          /*vertical=*/false,
                                          /*horizontal=*/true, /*over=*/false));
    }

    std::vector<ObjectDrawer::TileTrace> trace;
    drawer.SetTraceCollector(&trace, /*trace_only=*/true);
    ASSERT_TRUE(drawer.DrawObject(cell, bg1, bg2, palette_group).ok());
    ASSERT_EQ(trace.size(), 48u);
    for (const auto& write : trace) {
      EXPECT_EQ(write.layer, static_cast<uint8_t>(layer));
      EXPECT_NE(write.flags & 0x1, 0)
          << "USDASM ORA #$4000 must not toggle an existing H-flip off";
    }
    drawer.ClearTraceCollector();
  }
}

TEST(ObjectDrawerCannonHoleTest,
     RightwardsRepeatsLeftSegmentAndDrawsRightEdgeOnce) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  // Objects 0x51/0x52 use RoomDraw_RightwardsCannonHole4x3_1to16:
  // - Repeat left 2 columns (6 tiles) "count" times (count = size + 1)
  // - Then draw right 2-column edge once.
  //
  // With size=1 => count=2 => total columns = 2*count + 2 = 6 columns
  // Total tiles = 6 cols * 3 rows = 18 writes.
  RoomObject obj(0x0051, /*x=*/10, /*y=*/20, /*size=*/1, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 12; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                       false, false, false));
  }

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 18u);

  struct Expected {
    int x = 0;
    int y = 0;
    uint16_t tile_id = 0;
  };

  const std::vector<Expected> expected = {
      // Segment 0 (left 2 columns)
      {10, 20, 0},
      {10, 21, 1},
      {10, 22, 2},
      {11, 20, 3},
      {11, 21, 4},
      {11, 22, 5},
      // Segment 1 (repeat left 2 columns)
      {12, 20, 0},
      {12, 21, 1},
      {12, 22, 2},
      {13, 20, 3},
      {13, 21, 4},
      {13, 22, 5},
      // Right edge (last 2 columns)
      {14, 20, 6},
      {14, 21, 7},
      {14, 22, 8},
      {15, 20, 9},
      {15, 21, 10},
      {15, 22, 11},
  };

  ASSERT_EQ(trace.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(trace[i].x_tile, expected[i].x) << "trace idx=" << i;
    EXPECT_EQ(trace[i].y_tile, expected[i].y) << "trace idx=" << i;
    EXPECT_EQ(trace[i].tile_id, expected[i].tile_id) << "trace idx=" << i;
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     RightwardsBigRailUsesUsdasmStartMiddleEndColumns) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 4;
  constexpr int kY = 6;
  constexpr uint8_t kSize = 1;  // middle columns = size + 2 = 3

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x005D, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/15));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  std::vector<SnapshotTileWrite> expected;
  expected.reserve(21);

  auto append_column = [&](int x, uint16_t top, uint16_t mid, uint16_t bot) {
    expected.push_back({x, kY + 0, top});
    expected.push_back({x, kY + 1, mid});
    expected.push_back({x, kY + 2, bot});
  };

  append_column(kX + 0, 0, 1, 2);
  append_column(kX + 1, 3, 4, 5);
  append_column(kX + 2, 6, 7, 8);
  append_column(kX + 3, 6, 7, 8);
  append_column(kX + 4, 6, 7, 8);
  append_column(kX + 5, 9, 10, 11);
  append_column(kX + 6, 12, 13, 14);

  ExpectTraceMatchesSnapshot(bg1, expected);
}

std::vector<SnapshotTileWrite> MakeBigHoleSnapshot(int x, int y, uint8_t size) {
  const int max = static_cast<int>(size & 0x0F) + 3;
  std::vector<SnapshotTileWrite> out;
  out.reserve(16);

  out.push_back({x, y, 8});
  out.push_back({x + max, y, 14});
  out.push_back({x, y + max, 17});
  out.push_back({x + max, y + max, 23});

  for (int xx = 1; xx < max; ++xx) {
    for (int yy = 1; yy < max; ++yy) {
      out.push_back({x + xx, y + yy, 0});
    }
    out.push_back({x + xx, y, 10});
    out.push_back({x + xx, y + max, 19});
  }
  for (int yy = 1; yy < max; ++yy) {
    out.push_back({x, y + yy, 9});
    out.push_back({x + max, y + yy, 15});
  }
  return out;
}

std::vector<SnapshotTileWrite> MakeTableRockSnapshot(int x, int y,
                                                     uint8_t size) {
  const int size_x = (size >> 2) & 0x03;
  const int size_y = size & 0x03;
  const int right_x = x + (3 + (size_x * 2));
  const int bottom_y = y + (3 + (size_y * 2));

  std::vector<SnapshotTileWrite> out;
  out.reserve(16);

  for (int xx = 0; xx < size_x + 1; ++xx) {
    for (int yy = 0; yy < size_y + 1; ++yy) {
      const int base_x = x + (xx * 2);
      const int base_y = y + (yy * 2);
      out.push_back({base_x + 1, base_y + 1, 5});
      out.push_back({base_x + 2, base_y + 1, 6});
      out.push_back({base_x + 1, base_y + 2, 9});
      out.push_back({base_x + 2, base_y + 2, 10});
    }
  }

  for (int yy = 0; yy < size_y + 1; ++yy) {
    const int base_y = y + (yy * 2);
    out.push_back({x, base_y + 1, 4});
    out.push_back({x, base_y + 2, 8});
    out.push_back({right_x, base_y + 1, 7});
    out.push_back({right_x, base_y + 2, 11});
  }

  for (int xx = 0; xx < size_x + 1; ++xx) {
    const int base_x = x + (xx * 2);
    out.push_back({base_x + 1, y, 1});
    out.push_back({base_x + 2, y, 2});
    out.push_back({base_x + 1, bottom_y, 13});
    out.push_back({base_x + 2, bottom_y, 14});
  }

  out.push_back({x, y, 0});
  out.push_back({x, bottom_y, 12});
  out.push_back({right_x, y, 3});
  out.push_back({right_x, bottom_y, 15});
  return out;
}

std::vector<SnapshotTileWrite> MakeWaterOverlaySnapshot(int x, int y,
                                                        uint8_t size) {
  const int size_x = (size >> 2) & 0x03;
  const int size_y = size & 0x03;
  const int count_x = size_x + 2;
  const int count_y = size_y + 2;

  std::vector<SnapshotTileWrite> out;
  out.reserve(count_x * count_y * 16);

  for (int yy = 0; yy < count_y; ++yy) {
    for (int xx = 0; xx < count_x; ++xx) {
      const int base_x = x + (xx * 4);
      const int base_y = y + (yy * 4);
      for (int tile_x = 0; tile_x < 4; ++tile_x) {
        out.push_back({base_x + tile_x, base_y, static_cast<uint16_t>(tile_x)});
        out.push_back(
            {base_x + tile_x, base_y + 2, static_cast<uint16_t>(tile_x)});
        out.push_back(
            {base_x + tile_x, base_y + 1, static_cast<uint16_t>(4 + tile_x)});
        out.push_back(
            {base_x + tile_x, base_y + 3, static_cast<uint16_t>(4 + tile_x)});
      }
    }
  }
  return out;
}

TEST(ObjectDrawerRegistryReplayTest,
     BigHoleDrawsBorderedInteriorWithUsdasmTileIndices) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 7;
  constexpr int kY = 9;
  constexpr uint8_t kSize = 0;

  const auto trace = ReplayObjectTrace(
      /*object_id=*/0x00A4, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/24));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  ExpectTraceMatchesSnapshot(bg1, MakeBigHoleSnapshot(kX, kY, kSize));
  EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     TableRockDrawsFourByFourStampWithUsdasmTileIndices) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 5;
  constexpr int kY = 6;
  constexpr uint8_t kSize = 0;

  const auto trace = ReplayObjectTrace(
      /*object_id=*/0x00DD, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/16));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  ExpectTraceMatchesSnapshot(bg1, MakeTableRockSnapshot(kX, kY, kSize));
  EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG2).empty());
}

TEST(ObjectDrawerRegistryReplayTest,
     FloodWaterOverlayDrawsEightByEightStampGridOnBg2) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 3;
  constexpr int kY = 4;
  constexpr uint8_t kSize = 0;

  for (const int object_id : {0x00D8, 0x00DA}) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);

    const auto trace =
        ReplayObjectTrace(object_id, kX, kY, kSize, RoomObject::LayerType::BG2,
                          MakeSequentialTiles(/*count=*/8));
    const auto bg2 = FilterTraceByLayer(trace, RoomObject::LayerType::BG2);

    ExpectTraceMatchesSnapshot(bg2, MakeWaterOverlaySnapshot(kX, kY, kSize));
    EXPECT_TRUE(FilterTraceByLayer(trace, RoomObject::LayerType::BG1).empty());
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     LongHorizontalAndVerticalRailsUseMatchingCornerMiddleEndSpans) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 4;
  constexpr int kY = 6;
  constexpr uint8_t kSize = 2;  // middle span = size + 21 = 23

  const auto horizontal = ReplayObjectTrace(
      /*object_id=*/0x005F, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/3));
  const auto vertical = ReplayObjectTrace(
      /*object_id=*/0x008A, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/3));

  const auto h_bg1 = FilterTraceByLayer(horizontal, RoomObject::LayerType::BG1);
  const auto v_bg1 = FilterTraceByLayer(vertical, RoomObject::LayerType::BG1);

  EXPECT_EQ(h_bg1.size(), v_bg1.size());
  const int expected_span = static_cast<int>(kSize) + 23;
  ExpectTraceBounds(h_bg1, kX, kY, kX + expected_span - 1, kY);
  ExpectTraceBounds(v_bg1, kX, kY, kX, kY + expected_span - 1);

  for (int offset = 0; offset < expected_span; ++offset) {
    EXPECT_TRUE(TraceHasWriteAt(h_bg1, kX + offset, kY)) << "offset=" << offset;
    EXPECT_TRUE(TraceHasWriteAt(v_bg1, kX, kY + offset)) << "offset=" << offset;
  }

  EXPECT_EQ(h_bg1.front().tile_id, 0);
  EXPECT_EQ(v_bg1.front().tile_id, 0);
  EXPECT_EQ(h_bg1.back().tile_id, 2);
  EXPECT_EQ(v_bg1.back().tile_id, 2);
}

TEST(ObjectDrawerRegistryReplayTest,
     DownwardsBigRailUsesUsdasmTopMiddleBottomSegments) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 6;
  constexpr int kY = 5;
  constexpr uint8_t kSize = 1;  // middle rows = size + 1 = 2

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0088, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/12));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  const std::vector<SnapshotTileWrite> expected = {
      // Top 2x2 cap (column-major).
      {kX + 0, kY + 0, 0},
      {kX + 0, kY + 1, 1},
      {kX + 1, kY + 0, 2},
      {kX + 1, kY + 1, 3},
      // Middle repeated 2x1 rows.
      {kX + 0, kY + 2, 4},
      {kX + 1, kY + 2, 5},
      {kX + 0, kY + 3, 4},
      {kX + 1, kY + 3, 5},
      // Bottom 2 columns x 3 rows.
      {kX + 0, kY + 4, 6},
      {kX + 0, kY + 5, 7},
      {kX + 0, kY + 6, 8},
      {kX + 1, kY + 4, 9},
      {kX + 1, kY + 5, 10},
      {kX + 1, kY + 6, 11},
  };

  ExpectTraceMatchesSnapshot(bg1, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     DownwardsCannonHoleUsesUsdasmSegmentRepeatAndEdge) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 8;
  constexpr int kY = 9;
  constexpr uint8_t kSize = 1;  // repeat left segment size+1 times.

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0085, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/12));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  const std::vector<SnapshotTileWrite> expected = {
      // Repeated left segment #0 (3x2, row-major).
      {kX + 0, kY + 0, 0},
      {kX + 1, kY + 0, 1},
      {kX + 2, kY + 0, 2},
      {kX + 0, kY + 1, 3},
      {kX + 1, kY + 1, 4},
      {kX + 2, kY + 1, 5},
      // Repeated left segment #1.
      {kX + 0, kY + 2, 0},
      {kX + 1, kY + 2, 1},
      {kX + 2, kY + 2, 2},
      {kX + 0, kY + 3, 3},
      {kX + 1, kY + 3, 4},
      {kX + 2, kY + 3, 5},
      // Final edge segment.
      {kX + 0, kY + 4, 6},
      {kX + 1, kY + 4, 7},
      {kX + 2, kY + 4, 8},
      {kX + 0, kY + 5, 9},
      {kX + 1, kY + 5, 10},
      {kX + 2, kY + 5, 11},
  };

  ExpectTraceMatchesSnapshot(bg1, expected);
}

TEST(ObjectDrawerRegistryReplayTest,
     RightwardsDecor4x3Spaced4UsesEightTileStride) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 5;
  constexpr int kY = 7;
  constexpr uint8_t kSize = 2;  // count = size + 1 = 3 stamps.

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x0FF9, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/12));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  ASSERT_EQ(bg1.size(), 36u);
  ExpectTraceBounds(bg1, kX, kY, kX + 19, kY + 2);

  // usdasm RoomDraw_RightwardsDecor4x3spaced4_1to16 ($01:9387)
  // draws a 4x3 stamp, then ADC #$0008 after RoomDraw_1x3N_rightwards.
  // The helper already advanced by four tile columns, so the next stamp starts
  // eight tile columns after the previous one.
  EXPECT_TRUE(TraceHasWriteAt(bg1, kX + 0, kY));
  EXPECT_TRUE(TraceHasWriteAt(bg1, kX + 8, kY));
  EXPECT_TRUE(TraceHasWriteAt(bg1, kX + 16, kY));
  EXPECT_FALSE(TraceHasWriteAt(bg1, kX + 6, kY));
  EXPECT_FALSE(TraceHasWriteAt(bg1, kX + 12, kY));
}

TEST(ObjectDrawerRegistryReplayTest,
     OpenChestPlatformUsesTwoBitSizeFieldsAndFullSegmentHelper) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 4;
  constexpr int kY = 6;
  constexpr uint8_t kSize = 0x05;  // size_x=1, size_y=1.

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x00DC, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/21));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  // Width = 2*size_x + 10, height = 2*size_y + 7.
  ASSERT_EQ(bg1.size(), 108u);
  ExpectTraceBounds(bg1, kX, kY, kX + 11, kY + 8);

  EXPECT_EQ(LastTileIdAt(bg1, kX + 0, kY + 0), 0);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 1, kY + 0), 3);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 2, kY + 0), 3);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 3, kY + 0), 6);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 4, kY + 0), 9);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 7, kY + 0), 9);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 8, kY + 0), 12);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 9, kY + 0), 15);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 10, kY + 0), 15);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 11, kY + 0), 18);

  EXPECT_EQ(LastTileIdAt(bg1, kX + 0, kY + 7), 1);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 11, kY + 7), 19);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 0, kY + 8), 2);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 11, kY + 8), 20);
}

TEST(ObjectDrawerRegistryReplayTest,
     ClosedChestPlatformUsesTwoBitSizeFieldsAndCenterOverlay) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 2;
  constexpr int kY = 4;
  constexpr uint8_t kSize = 0x03;  // size_x=0, size_y=3.

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x00C1, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/68));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  // Width = 2*size_x + 14, height = 2*size_y + 8.
  ExpectTraceBounds(bg1, kX, kY, kX + 13, kY + 13);

  EXPECT_EQ(LastTileIdAt(bg1, kX + 0, kY + 0), 0);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 2, kY + 2), 8);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 11, kY + 0), 15);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 13, kY + 2), 23);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 0, kY + 11), 40);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 13, kY + 13), 63);

  // The center 2x2 is drawn after the repeated center carpet and must
  // overwrite those positions with tiles 64..67.
  EXPECT_EQ(LastTileIdAt(bg1, kX + 6, kY + 6), 64);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 7, kY + 6), 66);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 6, kY + 7), 65);
  EXPECT_EQ(LastTileIdAt(bg1, kX + 7, kY + 7), 67);
}

TEST(ObjectDrawerRegistryReplayTest, MovingWallsDrawNothingAfterWallMoved) {
  ScopedCustomObjectsFlag disable_custom(false);

  FakeDungeonState state;
  state.wall_moved = true;

  for (const int16_t object_id : {int16_t{0x00CD}, int16_t{0x00CE}}) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);
    auto trace = ReplayObjectTrace(object_id, /*x=*/36, /*y=*/4, /*size=*/0,
                                   RoomObject::LayerType::BG1,
                                   MakeSequentialTiles(/*count=*/24), &state);
    EXPECT_TRUE(trace.empty());
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     MovingWallsUseIndependentDirectionAndCountSelectors) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kWestX = 36;
  constexpr int kEastX = 4;
  constexpr int kY = 4;
  for (size_t direction_index = 0;
       direction_index < moving_wall::kDirections.size(); ++direction_index) {
    for (size_t count_index = 0;
         count_index < moving_wall::kObjectCounts.size(); ++count_index) {
      const uint8_t size =
          static_cast<uint8_t>((direction_index << 2) | count_index);
      const int direction = moving_wall::kDirections[direction_index];
      const int count = moving_wall::kObjectCounts[count_index];
      const int height = direction * 2 + 6;
      const size_t expected_writes = count * height + 18 + direction * 6;

      for (const int16_t object_id : {int16_t{0x00CD}, int16_t{0x00CE}}) {
        SCOPED_TRACE(::testing::Message()
                     << "object_id=0x" << std::hex << object_id << " size=0x"
                     << static_cast<int>(size));
        const int object_x = object_id == 0x00CD ? kWestX : kEastX;
        auto trace = ReplayObjectTrace(object_id, object_x, kY, size,
                                       RoomObject::LayerType::BG1,
                                       MakeSequentialTiles(/*count=*/24));
        const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

        ASSERT_EQ(bg1.size(), expected_writes);
        if (object_id == 0x00CD) {
          ExpectTraceBounds(bg1, object_x - count, kY, object_x + 2,
                            kY + height - 1);
        } else {
          ExpectTraceBounds(bg1, object_x, kY, object_x + count + 2,
                            kY + height - 1);
        }
      }
    }
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     MovingWallsUseUsdasmCornerVerticalAndFillTileLayout) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 36;
  constexpr int kY = 4;
  constexpr int kFillDataOffset = kRoomObjectTileAddress + 0x03D8;
  const std::vector<std::pair<int, uint16_t>> fill_words = {
      {kFillDataOffset + 0, 0x0411},
      {kFillDataOffset + 2, 0x0822},
      {kFillDataOffset + 4, 0x0C33},
  };

  auto west_trace = ReplayObjectTrace(
      /*object_id=*/0x00CD, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/24), nullptr, fill_words);
  const auto west = FilterTraceByLayer(west_trace, RoomObject::LayerType::BG1);

  // West fill grows eight columns left from the anchor. Its top/middle/bottom
  // tiles come from obj03D8, while the 3x3 corners and repeated 3x2 vertical
  // wall use object payload slots 0..23.
  EXPECT_EQ(LastTileIdAt(west, kX - 8, kY + 0), 0x011);
  EXPECT_EQ(LastTileIdAt(west, kX - 8, kY + 1), 0x022);
  EXPECT_EQ(LastTileIdAt(west, kX - 8, kY + 15), 0x033);
  EXPECT_EQ(LastTileIdAt(west, kX + 0, kY + 0), 0);
  EXPECT_EQ(LastTileIdAt(west, kX + 2, kY + 2), 8);
  EXPECT_EQ(LastTileIdAt(west, kX + 0, kY + 3), 9);
  EXPECT_EQ(LastTileIdAt(west, kX + 2, kY + 4), 14);
  EXPECT_EQ(LastTileIdAt(west, kX + 0, kY + 13), 15);
  EXPECT_EQ(LastTileIdAt(west, kX + 1, kY + 14), 19);
  EXPECT_EQ(LastTileIdAt(west, kX + 2, kY + 15), 23);

  auto east_trace = ReplayObjectTrace(
      /*object_id=*/0x00CE, kX, kY, /*size=*/0, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/24), nullptr, fill_words);
  const auto east = FilterTraceByLayer(east_trace, RoomObject::LayerType::BG1);

  // East keeps the wall at the anchor and grows the fill rightward. Like west,
  // it resets X to obj03D8 before drawing the fill; payload slots 0..23 remain
  // dedicated to the corners and vertical wall.
  EXPECT_EQ(LastTileIdAt(east, kX + 0, kY + 0), 0);
  EXPECT_EQ(LastTileIdAt(east, kX + 2, kY + 2), 8);
  EXPECT_EQ(LastTileIdAt(east, kX + 0, kY + 3), 9);
  EXPECT_EQ(LastTileIdAt(east, kX + 2, kY + 4), 14);
  EXPECT_EQ(LastTileIdAt(east, kX + 0, kY + 13), 15);
  EXPECT_EQ(LastTileIdAt(east, kX + 1, kY + 14), 19);
  EXPECT_EQ(LastTileIdAt(east, kX + 2, kY + 15), 23);
  EXPECT_EQ(LastTileIdAt(east, kX + 3, kY + 0), 0x011);
  EXPECT_EQ(LastTileIdAt(east, kX + 3, kY + 1), 0x022);
  EXPECT_EQ(LastTileIdAt(east, kX + 3, kY + 15), 0x033);
}

TEST(ObjectDrawerRegistryReplayTest, DownwardsBarUsesUsdasmTopThenBodyRows) {
  ScopedCustomObjectsFlag disable_custom(false);

  constexpr int kX = 3;
  constexpr int kY = 4;
  constexpr uint8_t kSize = 1;  // body rows = 2 * (size + 2) = 6

  auto trace = ReplayObjectTrace(
      /*object_id=*/0x008F, kX, kY, kSize, RoomObject::LayerType::BG1,
      MakeSequentialTiles(/*count=*/4));
  const auto bg1 = FilterTraceByLayer(trace, RoomObject::LayerType::BG1);

  std::vector<SnapshotTileWrite> expected;
  expected.reserve(14);

  expected.push_back({kX + 0, kY + 0, 0});
  expected.push_back({kX + 1, kY + 0, 1});
  for (int row = 0; row < 6; ++row) {
    expected.push_back({kX + 0, kY + 1 + row, 2});
    expected.push_back({kX + 1, kY + 1 + row, 3});
  }

  ExpectTraceMatchesSnapshot(bg1, expected);
}

}  // namespace
}  // namespace yaze::zelda3
