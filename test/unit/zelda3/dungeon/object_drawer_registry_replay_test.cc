#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>

#include "core/features.h"
#include "rom/rom.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "zelda3/dungeon/dungeon_state.h"
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

  bool IsChestOpen(int /*room_id*/, int /*chest_index*/) const override {
    return false;
  }
  bool IsBigChestOpen() const override { return false; }

  bool IsDoorOpen(int room_id, int door_index) const override {
    if (door_index != 0) {
      return false;
    }
    return room_id == open_lock_room_id;
  }
  bool IsDoorSwitchActive(int /*room_id*/) const override { return false; }

  bool IsWallMoved(int /*room_id*/) const override { return false; }
  bool IsFloorBombable(int /*room_id*/) const override { return false; }
  bool IsRupeeFloorActive(int /*room_id*/) const override { return false; }

  bool IsCrystalSwitchBlue() const override { return true; }
};

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

TEST(ObjectDrawerRegistryReplayTest, SuperSquare4x4FloorUsesColumnMajorTiles) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  // Object 0xD9 maps to routine 58 (Draw4x4FloorIn4x4SuperSquare).
  RoomObject obj(0x00D9, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    obj.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 16u);

  auto key = [](int x, int y) { return (y << 8) | x; };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());

  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }
  ASSERT_EQ(by_pos.size(), 16u);

  // Tiles are column-major 4x2, and rows 2/3 repeat rows 0/1.
  EXPECT_EQ(by_pos[key(10, 20)], 0);
  EXPECT_EQ(by_pos[key(10, 21)], 1);
  EXPECT_EQ(by_pos[key(10, 22)], 0);
  EXPECT_EQ(by_pos[key(10, 23)], 1);

  EXPECT_EQ(by_pos[key(11, 20)], 2);
  EXPECT_EQ(by_pos[key(11, 21)], 3);
  EXPECT_EQ(by_pos[key(11, 22)], 2);
  EXPECT_EQ(by_pos[key(11, 23)], 3);

  EXPECT_EQ(by_pos[key(12, 20)], 4);
  EXPECT_EQ(by_pos[key(12, 21)], 5);
  EXPECT_EQ(by_pos[key(12, 22)], 4);
  EXPECT_EQ(by_pos[key(12, 23)], 5);

  EXPECT_EQ(by_pos[key(13, 20)], 6);
  EXPECT_EQ(by_pos[key(13, 21)], 7);
  EXPECT_EQ(by_pos[key(13, 22)], 6);
  EXPECT_EQ(by_pos[key(13, 23)], 7);
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
    obj.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
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
     DownwardsDecor4x2Spaced4UsesColumnMajorTileOrder) {
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
    obj.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 16u);

  auto key = [](int x, int y) { return (y << 8) | x; };
  std::unordered_map<int, uint16_t> by_pos;
  by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  // Column-major 4x2 at y=20..21
  EXPECT_EQ(by_pos[key(10, 20)], 0);
  EXPECT_EQ(by_pos[key(10, 21)], 1);
  EXPECT_EQ(by_pos[key(11, 20)], 2);
  EXPECT_EQ(by_pos[key(11, 21)], 3);
  EXPECT_EQ(by_pos[key(12, 20)], 4);
  EXPECT_EQ(by_pos[key(12, 21)], 5);
  EXPECT_EQ(by_pos[key(13, 20)], 6);
  EXPECT_EQ(by_pos[key(13, 21)], 7);

  // Second slice uses +6 vertical stride.
  EXPECT_EQ(by_pos[key(10, 26)], 0);
  EXPECT_EQ(by_pos[key(13, 27)], 7);
}

TEST(ObjectDrawerRegistryReplayTest, SuperSquare4x4FloorShortTilePayloadFallsBack) {
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

TEST(ObjectDrawerRegistryReplayTest, RegistryRoutinesUseObjectDrawerRoomIdForStateQueries) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  // Use trace-only mode so we don't need real gfx data.
  ObjectDrawer drawer(&rom, /*room_id=*/0x42, /*room_gfx_buffer=*/nullptr);

  FakeDungeonState state;
  state.open_lock_room_id = 0x42;

  RoomObject lock(0x0F98, /*x=*/10, /*y=*/10, /*size=*/0, /*layer=*/0);
  lock.tiles_loaded_ = true;
  lock.tiles_.clear();
  for (int i = 0; i < 8; ++i) {
    lock.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(lock, bg1, bg2, palette_group, &state).ok());
  ASSERT_EQ(trace.size(), 4u);

  // When opened, BigKeyLock draws the second 2x2 tile set (tiles[4..7]).
  EXPECT_EQ(trace[0].tile_id, lock.tiles_[4].id_);
}

TEST(ObjectDrawerMaskPropagationTest, Layer2PitMaskMarksBG1Transparent) {
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

  // Both the object BG1 buffer and the layout BG1 buffer should be masked.
  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 255);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 255);
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

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 255);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 255);
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
    obj.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
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

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 255);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 255);
}

TEST(ObjectDrawerRegistryReplayTest, PrisonCellDrawsToBothBuffersWhenMarkedBothBG) {
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
  RoomObject cell(0x0F8D, /*x=*/2, /*y=*/2, /*size=*/0, /*layer=*/0);
  cell.tiles_loaded_ = true;
  cell.tiles_.clear();
  for (int i = 0; i < 6; ++i) {
    cell.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
  }

  gfx::PaletteGroup palette_group;
  ASSERT_TRUE(drawer.DrawObject(cell, bg1, bg2, palette_group).ok());

  const int px = cell.x_ * 8;
  const int py = cell.y_ * 8;
  const int idx = py * bg1.bitmap().width() + px;
  ASSERT_GE(idx, 0);
  ASSERT_LT(idx, static_cast<int>(bg1.bitmap().size()));

  // Routine 97 is marked draws_to_both_bgs=true in the registry, so ObjectDrawer
  // should execute it once for BG1 and once for BG2.
  EXPECT_NE(bg1.bitmap().data()[idx], 255);
  EXPECT_NE(bg2.bitmap().data()[idx], 255);
}

TEST(ObjectDrawerCannonHoleTest, RightwardsRepeatsLeftSegmentAndDrawsRightEdgeOnce) {
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
    obj.tiles_.push_back(
        gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2, false, false, false));
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
      {10, 20, 0}, {10, 21, 1}, {10, 22, 2},
      {11, 20, 3}, {11, 21, 4}, {11, 22, 5},
      // Segment 1 (repeat left 2 columns)
      {12, 20, 0}, {12, 21, 1}, {12, 22, 2},
      {13, 20, 3}, {13, 21, 4}, {13, 22, 5},
      // Right edge (last 2 columns)
      {14, 20, 6}, {14, 21, 7}, {14, 22, 8},
      {15, 20, 9}, {15, 21, 10}, {15, 22, 11},
  };

  ASSERT_EQ(trace.size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(trace[i].x_tile, expected[i].x) << "trace idx=" << i;
    EXPECT_EQ(trace[i].y_tile, expected[i].y) << "trace idx=" << i;
    EXPECT_EQ(trace[i].tile_id, expected[i].tile_id) << "trace idx=" << i;
  }
}

}  // namespace
}  // namespace yaze::zelda3
