#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"
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
  int water_face_active_room_id = -1;

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
  bool IsWaterFaceActive(int room_id) const override {
    return room_id == water_face_active_room_id;
  }

  bool IsWallMoved(int /*room_id*/) const override { return false; }
  bool IsFloorBombable(int /*room_id*/) const override { return false; }
  bool IsRupeeFloorActive(int /*room_id*/) const override { return false; }

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
    const DungeonState* state = nullptr) {
  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);
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
  expected_top.reserve(kCount * 2);
  expected_bottom.reserve(kCount * 2);

  // USDASM:
  // - $01:8FBD (top corners): body uses top=tile3, bottom=tile0.
  // - $01:9001 (bottom corners): mirrored body uses top=tile0, bottom=tile3.
  // Endpoints consume cap tiles (tile1 at start, tile4 at end).
  for (int s = 0; s < kCount; ++s) {
    const uint16_t top_cap = (s == 0) ? 1 : ((s == kCount - 1) ? 4 : 3);
    const uint16_t bottom_cap = (s == 0) ? 1 : ((s == kCount - 1) ? 4 : 3);
    const int x = kX + 13 + s;

    expected_top.push_back({x, kY, top_cap});
    expected_top.push_back({x, kY + 1, 0});

    expected_bottom.push_back({x, kY + 1, 0});
    expected_bottom.push_back({x, kY + 2, bottom_cap});
  }

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
  expected_left.reserve(kCount * 2);
  expected_right.reserve(kCount * 2);

  // USDASM:
  // - $01:9045 (left corners): body uses left=tile3, right=tile0.
  // - $01:908F (right corners): mirrored body uses left=tile0, right=tile3.
  // Endpoints consume cap tiles (tile1 at start, tile4 at end).
  for (int s = 0; s < kCount; ++s) {
    const uint16_t left_cap = (s == 0) ? 1 : ((s == kCount - 1) ? 4 : 3);
    const uint16_t right_cap = (s == 0) ? 1 : ((s == kCount - 1) ? 4 : 3);
    const int y = kY + s;

    expected_left.push_back({kX + 12, y, left_cap});
    expected_left.push_back({kX + 13, y, 0});

    expected_right.push_back({kX + 12, y, 0});
    expected_right.push_back({kX + 13, y, right_cap});
  }

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
     LightBeamOnFloorMatchesUsdasmStackedBlocks) {
  ScopedCustomObjectsFlag disable_custom(false);

  Rom rom;
  std::vector<uint8_t> dummy_rom(1024 * 1024, 0);
  rom.LoadFromData(dummy_rom);

  ObjectDrawer drawer(&rom, /*room_id=*/0, /*room_gfx_buffer=*/nullptr);

  RoomObject obj(0x0FF0, /*x=*/10, /*y=*/20, /*size=*/0, /*layer=*/0);
  obj.tiles_loaded_ = true;
  obj.tiles_.clear();
  for (int i = 0; i < 48; ++i) {
    obj.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i),
                                       /*pal=*/2, false, false, false));
  }

  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);

  ASSERT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  ASSERT_EQ(trace.size(), 48u);

  auto key = [](int x, int y) {
    return (y << 8) | x;
  };
  std::unordered_map<int, uint16_t> final_by_pos;
  final_by_pos.reserve(trace.size());
  for (const auto& t : trace) {
    final_by_pos[key(t.x_tile, t.y_tile)] = t.tile_id;
  }

  EXPECT_EQ(final_by_pos[key(10, 20)], 0);   // top block
  EXPECT_EQ(final_by_pos[key(10, 22)], 16);  // middle block overwrites overlap
  EXPECT_EQ(final_by_pos[key(13, 25)], 31);
  EXPECT_EQ(final_by_pos[key(10, 26)], 32);  // bottom block
  EXPECT_EQ(final_by_pos[key(13, 29)], 47);
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
     RegistryRoutinesUseObjectDrawerRoomIdForStateQueries) {
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

  // When opened, BigKeyLock draws the second 2x2 tile set (tiles[4..7]).
  EXPECT_EQ(trace[0].tile_id, lock.tiles_[4].id_);
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

  EXPECT_EQ(obj_bg1.bitmap().data()[idx], 255);
  EXPECT_EQ(layout_bg1.bitmap().data()[idx], 255);
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

    EXPECT_EQ(obj_bg1.bitmap().data()[idx], 255) << object_id;
    EXPECT_EQ(layout_bg1.bitmap().data()[idx], 255) << object_id;
  }
}

TEST(ObjectDrawerRegistryReplayTest,
     PrisonCellDrawsToBothBuffersWhenMarkedBothBG) {
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
    cell.tiles_.push_back(gfx::TileInfo(static_cast<uint16_t>(i), /*pal=*/2,
                                        false, false, false));
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
