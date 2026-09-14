// Object-to-routine mapping and focused draw-behavior regressions. Independent
// pixel evidence lives in dungeon_room_regression_fixtures_test.cc.

#include <algorithm>
#include <vector>

#include "app/gfx/render/background_buffer.h"
#include "core/features.h"
#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

class DrawRoutineMappingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Minimal ROM needed for ObjectDrawer construction
    rom_ = std::make_unique<Rom>();
    // No data needed for mapping logic as it's hardcoded in InitializeDrawRoutines
  }

  std::unique_ptr<Rom> rom_;
};

namespace {

struct TilePoint {
  int x;
  int y;
};

gfx::TileInfo MakeTile(uint16_t id, uint8_t palette = 0) {
  return gfx::TileInfo(id, palette, false, false, false);
}

std::vector<TilePoint> CollectNonZeroTiles(const gfx::BackgroundBuffer& bg) {
  std::vector<TilePoint> points;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      if (bg.GetTileAt(x, y) != 0) {
        points.push_back({x, y});
      }
    }
  }
  return points;
}

bool ContainsPoint(const std::vector<TilePoint>& points, int x, int y) {
  for (const auto& point : points) {
    if (point.x == x && point.y == y) {
      return true;
    }
  }
  return false;
}

struct WaterObjectCase {
  int object_id;
  int routine_id;
  int column_height;
  bool vertical;
  std::vector<uint16_t> words;
};

const std::vector<WaterObjectCase>& WaterObjectCases() {
  // USDASM bank_00 obj091A..obj096C ($00A46C..$00A4CE), in source order.
  // Keep the actual mirrored words: sequential IDs cannot catch lost flips.
  static const std::vector<WaterObjectCase> cases = {
      {0x3F, 22, 1, false, {0x1DFE, 0x1DFC, 0x5DFE}},
      {0x40, 22, 1, false, {0x9DFE, 0x9DFC, 0xDDFE}},
      {0x41, 22, 1, false, {0xDDFF, 0x9DFC, 0x9DFF}},
      {0x42, 22, 1, false, {0x5DFF, 0x1DFC, 0x1DFF}},
      {0x43, 22, 1, false, {0xDDFF, 0x9DFC, 0xDDFE}},
      {0x44, 22, 1, false, {0x9DFE, 0x9DFC, 0x9DFF}},
      {0x45, 22, 1, false, {0x5DFF, 0x1DFC, 0x5DFE}},
      {0x46, 22, 1, false, {0x1DFE, 0x1DFC, 0x1DFF}},
      {0x47,
       DrawRoutineIds::kWaterfall47,
       5,
       false,
       {0x1DF7, 0x1C40, 0x1C41, 0x1C42, 0x1DB5, 0x1DB2, 0x1DB3, 0x1DB3, 0x1DB4,
        0x1DB5, 0x5DF7, 0x5C40, 0x5C41, 0x5C42, 0x5DB5}},
      {0x48,
       DrawRoutineIds::kWaterfall48,
       3,
       false,
       {0x1DF7, 0x1C40, 0x1DB5, 0x1DB2, 0x1DB3, 0x1DB5, 0x5DF7, 0x5C40,
        0x5DB5}},
      {0x79, 13, 1, true, {0x1DFD}},
      {0x7A, 13, 1, true, {0x5DFD}},
  };
  return cases;
}

}  // namespace

TEST_F(DrawRoutineMappingTest,
       WaterEdgesAndWaterfallsPreserveSourceWordsAcrossSizesAndBounds) {
  auto& registry = DrawRoutineRegistry::Get();
  for (const auto& tc : WaterObjectCases()) {
    ASSERT_EQ(registry.GetRoutineIdForObject(tc.object_id), tc.routine_id);
    const auto* info = registry.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);
    EXPECT_FALSE(info->draws_to_both_bgs);
    // The second pass exercises all attribute bits, including priority, without
    // changing the source character IDs. It is synthetic, not a ROM capture.
    for (const uint16_t attribute_xor : {uint16_t{0}, uint16_t{0xFC00}}) {
      std::vector<gfx::TileInfo> tiles;
      for (const uint16_t word : tc.words) {
        tiles.push_back(gfx::WordToTileInfo(word ^ attribute_xor));
      }
      for (int size = 0; size < 16; ++size) {
        // $018F62 / $018F8A: cap + size+1 body + cap, or size+1 rows.
        // $019466 / $019488: cap + 2*(size+1) body columns + cap.
        const int width = tc.vertical             ? 1
                          : tc.column_height == 1 ? size + 3
                                                  : 2 * size + 4;
        const int height = tc.vertical ? size + 1 : tc.column_height;
        for (const TilePoint origin :
             {TilePoint{4, 6}, TilePoint{31, 31},
              TilePoint{64 - width, 64 - height}, TilePoint{63, 63}}) {
          SCOPED_TRACE(::testing::Message()
                       << "object=" << tc.object_id << " size=" << size
                       << " attributes=" << attribute_xor << " origin=("
                       << origin.x << "," << origin.y << ")");
          gfx::BackgroundBuffer target;
          gfx::BackgroundBuffer secondary;
          const RoomObject object(tc.object_id, origin.x, origin.y, size, 0);
          DrawContext ctx{target,     object, tiles,   nullptr,
                          rom_.get(), 0,      nullptr, &secondary};
          info->function(ctx);

          std::vector<uint16_t> expected(64 * 64, 0);
          for (int x = 0; x < width && origin.x + x < 64; ++x) {
            for (int y = 0; y < height && origin.y + y < 64; ++y) {
              const int source_column = x == 0 ? 0 : x == width - 1 ? 2 : 1;
              const int source_index =
                  tc.vertical ? 0 : source_column * tc.column_height + y;
              expected[(origin.y + y) * 64 + origin.x + x] =
                  tc.words[source_index] ^ attribute_xor;
            }
          }
          // Out-of-room positions clip to the editor canvas; this does not
          // claim parity with SNES address wrapping for malformed placements.
          EXPECT_EQ(target.buffer(), expected);
          EXPECT_TRUE(secondary.buffer().empty());
        }
      }
    }
  }
}

TEST_F(DrawRoutineMappingTest, WaterEdgeCapsPreserveOnlyCompatibleCorners) {
  for (const auto& tc : WaterObjectCases()) {
    if (tc.object_id < 0x3F || tc.object_id > 0x46) {
      continue;
    }
    const auto* info = DrawRoutineRegistry::Get().GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);
    std::vector<gfx::TileInfo> tiles;
    for (const uint16_t word : tc.words) {
      tiles.push_back(gfx::WordToTileInfo(word));
    }
    // $018F65..$018F7F compares the low ten bits only. Other water caps
    // (0x1FE/0x1FF) are not protected corners and must be replaced.
    for (const uint16_t existing_id :
         {0x01DB, 0x01A6, 0x01DD, 0x01FC, 0x01FE, 0x01FF, 0x0000}) {
      SCOPED_TRACE(::testing::Message()
                   << "object=" << tc.object_id << " existing=" << existing_id);
      constexpr int kX = 4;
      constexpr int kY = 6;
      constexpr int kSize = 3;
      const uint16_t existing_word = existing_id | 0xE800;
      const bool keep_corner = existing_id == 0x01DB || existing_id == 0x01A6 ||
                               existing_id == 0x01DD || existing_id == 0x01FC;
      gfx::BackgroundBuffer target;
      target.SetTileAt(kX, kY, existing_word);
      const RoomObject object(tc.object_id, kX, kY, kSize, 0);
      DrawContext ctx{target,     object, tiles,   nullptr,
                      rom_.get(), 0,      nullptr, nullptr};
      info->function(ctx);

      std::vector<uint16_t> expected(64 * 64, 0);
      expected[kY * 64 + kX] = keep_corner ? existing_word : tc.words[0];
      for (int x = 1; x <= kSize + 1; ++x) {
        expected[kY * 64 + kX + x] = tc.words[1];
      }
      expected[kY * 64 + kX + kSize + 2] = tc.words[2];
      EXPECT_EQ(target.buffer(), expected);
    }
  }
}

TEST_F(DrawRoutineMappingTest,
       CustomFeatureRoutesAllOracleFixedFamiliesThroughCustomRoutine) {
  const bool previous_custom_objects =
      core::FeatureFlags::get().kEnableCustomObjects;
  struct RestoreFeatureFlag {
    bool previous;
    ~RestoreFeatureFlag() {
      core::FeatureFlags::get().kEnableCustomObjects = previous;
      DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    }
  } restore{previous_custom_objects};

  auto& registry = DrawRoutineRegistry::Get();
  core::FeatureFlags::get().kEnableCustomObjects = true;
  registry.RefreshFeatureFlagMappings();
  for (const int object_id : {0x31, 0x32, 0x54}) {
    EXPECT_EQ(registry.GetRoutineIdForObject(object_id),
              DrawRoutineIds::kCustomObject);
  }

  core::FeatureFlags::get().kEnableCustomObjects = false;
  registry.RefreshFeatureFlagMappings();
  EXPECT_EQ(registry.GetRoutineIdForObject(0x31), DrawRoutineIds::kNothing);
  EXPECT_EQ(registry.GetRoutineIdForObject(0x32), DrawRoutineIds::kNothing);
  EXPECT_EQ(registry.GetRoutineIdForObject(0x54), DrawRoutineIds::kNothing);
}

TEST_F(DrawRoutineMappingTest,
       HorizontalRailRoutinesKeepExistingSmallCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x00E2, 0), MakeTile(0x0240, 1), MakeTile(0x0241, 2)};

  struct Case {
    int routine_id;
    RoomObject object;
    int end_x;
  };

  const std::vector<Case> cases = {
      {DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus3,
       RoomObject(0x22, 5, 7, 0, 0), 8},
      {DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23,
       RoomObject(0x5F, 5, 7, 0, 0), 27},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E2, 6));
    bg.SetTileAt(tc.object.x_, tc.object.y_, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(bg.GetTileAt(tc.object.x_, tc.object.y_), preexisting_corner);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_ + 1, tc.object.y_),
              tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.end_x, tc.object.y_),
              tiles[2].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       HorizontalTrimRailRoutineKeepsCompatibleExistingCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* info =
      reg.GetRoutineInfo(DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus2);
  ASSERT_NE(info, nullptr);

  gfx::BackgroundBuffer bg;
  const RoomObject object(0x23, 9, 11, 0, 0);
  const uint16_t compatible_corner = gfx::TileInfoToWord(MakeTile(0x01A6, 5));
  bg.SetTileAt(object.x_, object.y_, compatible_corner);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x01DB, 0), MakeTile(0x0260, 1), MakeTile(0x0261, 2)};
  DrawContext ctx{bg,
                  object,
                  std::span<const gfx::TileInfo>(tiles),
                  /*state=*/nullptr,
                  rom_.get(),
                  /*room_id=*/0,
                  /*room_gfx_buffer=*/nullptr,
                  /*secondary_bg=*/nullptr};
  info->function(ctx);

  EXPECT_EQ(bg.GetTileAt(object.x_, object.y_), compatible_corner);
  EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, object.x_ + 1, object.y_),
            tiles[1].id_);
  EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, object.x_ + 2, object.y_),
            tiles[2].id_);
}

TEST_F(DrawRoutineMappingTest,
       VerticalRailRoutinesKeepExistingSmallCornerTile) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x00E3, 0), MakeTile(0x0280, 1), MakeTile(0x0281, 2)};

  struct Case {
    int routine_id;
    RoomObject object;
    int end_y;
  };

  // ASM RoomDraw_DownwardsHasEdge1x1_1to16 ($01:8EC6) calls
  // GetSize_1to16_timesA, so middle count = composite_size + A. _plus3 sets
  // A=2 (count = size + 2) and _plus23 sets A=21 (count = size + 21). With
  // size=0 + corner suppression the rendered span is corner_y + (count) middle
  // rows + 1 end row, so end_y = object.y + count + 1.
  const std::vector<Case> cases = {
      {DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus3,
       RoomObject(0x69, 13, 3, 0, 0), 6},
      {DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23,
       RoomObject(0x8A, 13, 3, 0, 0), 25},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E3, 7));
    bg.SetTileAt(tc.object.x_, tc.object.y_, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(bg.GetTileAt(tc.object.x_, tc.object.y_), preexisting_corner);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_, tc.object.y_ + 1),
              tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.object.x_, tc.end_y),
              tiles[2].id_);
  }
}

// Pins horizontal/vertical _plus3 parity. Both rail routines branch through
// RoomDraw_GetSize_1to16_timesA with A=2 (RoomDraw_RightwardsHasEdge1x1_1to16
// at $01:8EF0 and RoomDraw_DownwardsHasEdge1x1_1to16 at $01:8EC3), so the
// middle-tile count formula is identical (count = composite_size + 2). Total
// rendered length is 1 corner + count middles + 1 end = size + 4 tiles.
TEST_F(DrawRoutineMappingTest,
       HorizontalAndVerticalPlus3RailsRenderEqualLengthSpans) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* h_info =
      reg.GetRoutineInfo(DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus3);
  const DrawRoutineInfo* v_info =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus3);
  ASSERT_NE(h_info, nullptr);
  ASSERT_NE(v_info, nullptr);

  // Use distinct, non-corner tile IDs so the corner-suppression path stays
  // off and every emit position writes a tile we can identify.
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0200, 0), MakeTile(0x0201, 1), MakeTile(0x0202, 2)};

  for (uint8_t size : {uint8_t{0}, uint8_t{1}, uint8_t{5}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));

    constexpr int kAnchorX = 4;
    constexpr int kAnchorY = 6;
    const int expected_count = static_cast<int>(size) + 2;
    const int expected_total = expected_count + 2;  // corner + middles + end

    auto run_routine = [&](const DrawRoutineInfo& info,
                           const RoomObject& object) {
      gfx::BackgroundBuffer bg;
      DrawContext ctx{bg,
                      object,
                      std::span<const gfx::TileInfo>(tiles),
                      /*state=*/nullptr,
                      rom_.get(),
                      /*room_id=*/0,
                      /*room_gfx_buffer=*/nullptr,
                      /*secondary_bg=*/nullptr};
      info.function(ctx);
      return CollectNonZeroTiles(bg);
    };

    const RoomObject horizontal(0x22, kAnchorX, kAnchorY, size, 0);
    const auto h_points = run_routine(*h_info, horizontal);
    EXPECT_EQ(static_cast<int>(h_points.size()), expected_total)
        << "horizontal _plus3 footprint";
    for (int i = 0; i < expected_total; ++i) {
      const int x = kAnchorX + i;
      EXPECT_TRUE(ContainsPoint(h_points, x, kAnchorY))
          << "horizontal expects tile at (" << x << "," << kAnchorY << ")";
    }

    const RoomObject vertical(0x69, kAnchorX, kAnchorY, size, 0);
    const auto v_points = run_routine(*v_info, vertical);
    EXPECT_EQ(static_cast<int>(v_points.size()), expected_total)
        << "vertical _plus3 footprint";
    for (int i = 0; i < expected_total; ++i) {
      const int y = kAnchorY + i;
      EXPECT_TRUE(ContainsPoint(v_points, kAnchorX, y))
          << "vertical expects tile at (" << kAnchorX << "," << y << ")";
    }

    EXPECT_EQ(h_points.size(), v_points.size())
        << "horizontal/vertical _plus3 spans must match length";
  }
}

// Re-asserts the middle tile is the *same slot* (tile[1]) at every interior
// position for the vertical _plus3 rail. This guards against a regression
// where the routine could accidentally consume more tile slots (e.g. by
// indexing tiles[1+s]) and re-introduce the asymmetry that 2025-12-07
// "Issue 1: Vertical rails may not be updated to match horizontal rails"
// flagged.
TEST_F(DrawRoutineMappingTest,
       HorizontalAndVerticalPlus23RailsRenderEqualLengthSpans) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* h_info =
      reg.GetRoutineInfo(DrawRoutineIds::kRightwardsHasEdge1x1_1to16_plus23);
  const DrawRoutineInfo* v_info =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23);
  ASSERT_NE(h_info, nullptr);
  ASSERT_NE(v_info, nullptr);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0200, 0), MakeTile(0x0201, 1), MakeTile(0x0202, 2)};

  for (uint8_t size : {uint8_t{0}, uint8_t{1}, uint8_t{5}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));

    constexpr int kAnchorX = 4;
    constexpr int kAnchorY = 6;
    const int expected_count = static_cast<int>(size) + 21;
    const int expected_total = expected_count + 2;  // corner + middles + end

    auto run_routine = [&](const DrawRoutineInfo& info,
                           const RoomObject& object) {
      gfx::BackgroundBuffer bg;
      DrawContext ctx{bg,
                      object,
                      std::span<const gfx::TileInfo>(tiles),
                      /*state=*/nullptr,
                      rom_.get(),
                      /*room_id=*/0,
                      /*room_gfx_buffer=*/nullptr,
                      /*secondary_bg=*/nullptr};
      info.function(ctx);
      return CollectNonZeroTiles(bg);
    };

    const RoomObject horizontal(0x5F, kAnchorX, kAnchorY, size, 0);
    const auto h_points = run_routine(*h_info, horizontal);
    EXPECT_EQ(static_cast<int>(h_points.size()), expected_total)
        << "horizontal _plus23 footprint";
    for (int i = 0; i < expected_total; ++i) {
      const int x = kAnchorX + i;
      EXPECT_TRUE(ContainsPoint(h_points, x, kAnchorY))
          << "horizontal expects tile at (" << x << "," << kAnchorY << ")";
    }

    const RoomObject vertical(0x8A, kAnchorX, kAnchorY, size, 0);
    const auto v_points = run_routine(*v_info, vertical);
    EXPECT_EQ(static_cast<int>(v_points.size()), expected_total)
        << "vertical _plus23 footprint";
    for (int i = 0; i < expected_total; ++i) {
      const int y = kAnchorY + i;
      EXPECT_TRUE(ContainsPoint(v_points, kAnchorX, y))
          << "vertical expects tile at (" << kAnchorX << "," << y << ")";
    }

    EXPECT_EQ(h_points.size(), v_points.size())
        << "horizontal/vertical _plus23 spans must match length";
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsRailPlus23RepeatsMiddleTileAtEveryInteriorRow) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* info =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23);
  ASSERT_NE(info, nullptr);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0200, 0), MakeTile(0x0201, 1), MakeTile(0x0202, 2)};
  constexpr int kAnchorX = 11;
  constexpr int kAnchorY = 4;

  for (uint8_t size : {uint8_t{0}, uint8_t{3}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));
    const int middle_count = static_cast<int>(size) + 21;
    gfx::BackgroundBuffer bg;
    const RoomObject vertical(0x8A, kAnchorX, kAnchorY, size, 0);
    DrawContext ctx{bg,
                    vertical,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY), tiles[0].id_);
    for (int s = 0; s < middle_count; ++s) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY + 1 + s),
                tiles[1].id_)
          << "middle row offset=" << s;
    }
    EXPECT_EQ(
        DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY + 1 + middle_count),
        tiles[2].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsRailPlus3RepeatsMiddleTileAtEveryInteriorRow) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* info =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus3);
  ASSERT_NE(info, nullptr);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0200, 0), MakeTile(0x0201, 1), MakeTile(0x0202, 2)};
  constexpr int kAnchorX = 11;
  constexpr int kAnchorY = 4;

  for (uint8_t size : {uint8_t{0}, uint8_t{3}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));
    const int middle_count = static_cast<int>(size) + 2;
    gfx::BackgroundBuffer bg;
    const RoomObject vertical(0x69, kAnchorX, kAnchorY, size, 0);
    DrawContext ctx{bg,
                    vertical,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY), tiles[0].id_);
    for (int s = 0; s < middle_count; ++s) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY + 1 + s),
                tiles[1].id_)
          << "middle row offset=" << s;
    }
    EXPECT_EQ(
        DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY + 1 + middle_count),
        tiles[2].id_);
  }
}

TEST_F(DrawRoutineMappingTest, SolidPlus3RoutinesExtendFromTheObjectOrigin) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* horizontal =
      reg.GetRoutineInfo(DrawRoutineIds::kRightwards1x1Solid_1to16_plus3);
  const DrawRoutineInfo* vertical =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwards1x1Solid_1to16_plus3);
  ASSERT_NE(horizontal, nullptr);
  ASSERT_NE(vertical, nullptr);

  const std::vector<gfx::TileInfo> tiles = {MakeTile(0x0310, 3)};
  constexpr int kAnchorX = 9;
  constexpr int kAnchorY = 6;

  for (uint8_t size : {uint8_t{0}, uint8_t{5}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));
    const int expected_count = static_cast<int>(size) + 4;

    auto draw = [&](const DrawRoutineInfo& info, const RoomObject& object) {
      gfx::BackgroundBuffer bg;
      DrawContext ctx{bg,
                      object,
                      std::span<const gfx::TileInfo>(tiles),
                      /*state=*/nullptr,
                      rom_.get(),
                      /*room_id=*/0,
                      /*room_gfx_buffer=*/nullptr,
                      /*secondary_bg=*/nullptr};
      info.function(ctx);
      return bg;
    };

    const auto horizontal_bg =
        draw(*horizontal, RoomObject(0x34, kAnchorX, kAnchorY, size, 0));
    const auto horizontal_points = CollectNonZeroTiles(horizontal_bg);
    ASSERT_EQ(static_cast<int>(horizontal_points.size()), expected_count);
    for (int x = kAnchorX; x < kAnchorX + expected_count; ++x) {
      EXPECT_TRUE(ContainsPoint(horizontal_points, x, kAnchorY));
    }

    const auto vertical_bg =
        draw(*vertical, RoomObject(0x71, kAnchorX, kAnchorY, size, 0));
    const auto vertical_points = CollectNonZeroTiles(vertical_bg);
    ASSERT_EQ(static_cast<int>(vertical_points.size()), expected_count);
    for (int y = kAnchorY; y < kAnchorY + expected_count; ++y) {
      EXPECT_TRUE(ContainsPoint(vertical_points, kAnchorX, y));
    }
  }
}

TEST_F(DrawRoutineMappingTest,
       ThinStripObjectsPreserveSizeAttributesAndClipAtRoomBoundary) {
  // USDASM $019120/$019136 use size+4 from the stored origin; $018F8A
  // uses size+1. These are single-tile repeats, not conditional-cap routines.
  struct Case {
    int object_id;
    int added_count;
    bool horizontal;
  };
  const std::vector<Case> cases = {
      {0x34, 4, true}, {0x71, 4, false}, {0x8D, 1, false}, {0x8E, 1, false}};
  const gfx::TileInfo tile(0x02A7, 5, true, true, true);
  const std::vector<gfx::TileInfo> tiles = {tile};
  const uint16_t expected_word = gfx::TileInfoToWord(tile);
  auto& registry = DrawRoutineRegistry::Get();

  for (const auto& test_case : cases) {
    const auto* info = registry.GetRoutineInfo(
        registry.GetRoutineIdForObject(test_case.object_id));
    ASSERT_NE(info, nullptr);
    for (uint8_t size : {uint8_t{0}, uint8_t{1}, uint8_t{15}}) {
      const int count = size + test_case.added_count;
      for (const int anchor : {9, 31, 64 - count, 63}) {
        SCOPED_TRACE(::testing::Message() << "object=" << test_case.object_id
                                          << " size=" << static_cast<int>(size)
                                          << " anchor=" << anchor);
        const int start_x = test_case.horizontal ? anchor : 63;
        const int start_y = test_case.horizontal ? 63 : anchor;
        const RoomObject object(test_case.object_id, start_x, start_y, size, 0);
        gfx::BackgroundBuffer bg;
        // A solid strip must replace even a preexisting rail corner.
        bg.SetTileAt(start_x, start_y, 0xE0E3);
        DrawContext ctx{
            bg,      object,     std::span<const gfx::TileInfo>(tiles),
            nullptr, rom_.get(), 0,
            nullptr, nullptr};
        info->function(ctx);

        for (int y = 0; y < 64; ++y) {
          for (int x = 0; x < 64; ++x) {
            const bool in_strip =
                test_case.horizontal
                    ? y == start_y && x >= start_x && x < start_x + count
                    : x == start_x && y >= start_y && y < start_y + count;
            EXPECT_EQ(bg.GetTileAt(x, y), in_strip ? expected_word : 0)
                << "tile=(" << x << "," << y << ")";
          }
        }
      }
    }
  }
}

TEST_F(DrawRoutineMappingTest,
       ThinTrimObjectsKeepOnlyUsdasmCompatibleCapsWithoutShiftingBody) {
  // $018F65-$018F7F compares the low ten tile bits with four compatible
  // corners. $01B2CA advances one column even when the opening cap is kept.
  const std::vector<gfx::TileInfo> tiles = {
      gfx::TileInfo(0x0300, 2, false, true, false),
      gfx::TileInfo(0x0301, 5, true, false, true),
      gfx::TileInfo(0x0302, 6, true, true, true)};
  auto& registry = DrawRoutineRegistry::Get();

  for (const int object_id : {0xB3, 0xB4}) {
    const auto* info =
        registry.GetRoutineInfo(registry.GetRoutineIdForObject(object_id));
    ASSERT_NE(info, nullptr);
    for (uint8_t size : {uint8_t{0}, uint8_t{1}, uint8_t{15}}) {
      for (const uint16_t existing_id :
           {0x01DB, 0x01A6, 0x01DD, 0x01FC, 0x00E2, 0x02DB}) {
        const bool keep_cap = existing_id == 0x01DB || existing_id == 0x01A6 ||
                              existing_id == 0x01DD || existing_id == 0x01FC;
        const uint16_t existing_word = existing_id | 0xFC00;
        const int width = size + 3;
        for (const int anchor : {9, 31, 64 - width, 63}) {
          SCOPED_TRACE(::testing::Message()
                       << "object=" << object_id
                       << " size=" << static_cast<int>(size)
                       << " existing=" << existing_id << " anchor=" << anchor);
          constexpr int kY = 63;
          const RoomObject object(object_id, anchor, kY, size, 0);
          gfx::BackgroundBuffer bg;
          bg.SetTileAt(anchor, kY, existing_word);
          DrawContext ctx{
              bg,      object,     std::span<const gfx::TileInfo>(tiles),
              nullptr, rom_.get(), 0,
              nullptr, nullptr};
          info->function(ctx);

          for (int y = 0; y < 64; ++y) {
            for (int x = 0; x < 64; ++x) {
              uint16_t expected = 0;
              if (y == kY && x >= anchor && x < anchor + width) {
                const int index = x == anchor               ? 0
                                  : x == anchor + width - 1 ? 2
                                                            : 1;
                expected = x == anchor && keep_cap
                               ? existing_word
                               : gfx::TileInfoToWord(tiles[index]);
              }
              EXPECT_EQ(bg.GetTileAt(x, y), expected)
                  << "tile=(" << x << "," << y << ")";
            }
          }
        }
      }
    }
  }
}

TEST_F(DrawRoutineMappingTest,
       HorizontalCornerRoutinesUseUsdasmCapsAndObjectOrigin) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0300, 0), MakeTile(0x0301, 1), MakeTile(0x0302, 2),
      MakeTile(0x0303, 3), MakeTile(0x0304, 4), MakeTile(0x0305, 5)};
  constexpr int kAnchorX = 6;
  constexpr int kAnchorY = 8;
  constexpr int kBodyCount = 10;

  struct Case {
    int routine_id;
    int object_id;
    bool top_corner;
  };
  for (const auto& tc :
       {Case{DrawRoutineIds::kRightwardsTopCorners1x2_1to16_plus13, 0x2F, true},
        Case{DrawRoutineIds::kRightwardsBottomCorners1x2_1to16_plus13, 0x30,
             false}}) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);
    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const RoomObject object(tc.object_id, kAnchorX, kAnchorY, 0, 0);
    DrawContext ctx{bg,
                    object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    const int edge_y = tc.top_corner ? kAnchorY : kAnchorY + 1;
    const int fill_y = tc.top_corner ? kAnchorY + 1 : kAnchorY;
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, edge_y), tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX + 1, edge_y),
              tiles[2].id_);
    for (int x = kAnchorX; x < kAnchorX + kBodyCount + 4; ++x) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, x, fill_y), tiles[0].id_)
          << "x=" << x;
    }
    for (int x = kAnchorX + 2; x < kAnchorX + 2 + kBodyCount; ++x) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, x, edge_y), tiles[3].id_)
          << "x=" << x;
    }
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX + kBodyCount + 2, edge_y),
              tiles[4].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX + kBodyCount + 3, edge_y),
              tiles[5].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsCornerRoutinesUseUsdasmCapsAndObjectOrigin) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0300, 0), MakeTile(0x0301, 1), MakeTile(0x0302, 2),
      MakeTile(0x0303, 3), MakeTile(0x0304, 4), MakeTile(0x0305, 5)};
  constexpr int kAnchorX = 6;
  constexpr int kAnchorY = 8;
  constexpr int kBodyCount = 10;

  struct Case {
    int routine_id;
    int object_id;
    bool left_corner;
  };
  for (const auto& tc :
       {Case{DrawRoutineIds::kDownwardsLeftCorners2x1_1to16_plus12, 0x6C, true},
        Case{DrawRoutineIds::kDownwardsRightCorners2x1_1to16_plus12, 0x6D,
             false}}) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);
    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const RoomObject object(tc.object_id, kAnchorX, kAnchorY, 0, 0);
    DrawContext ctx{bg,
                    object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    const int edge_x = tc.left_corner ? kAnchorX : kAnchorX + 1;
    const int fill_x = tc.left_corner ? kAnchorX + 1 : kAnchorX;
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, edge_x, kAnchorY), tiles[1].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, edge_x, kAnchorY + 1),
              tiles[2].id_);
    for (int y = kAnchorY; y < kAnchorY + kBodyCount + 4; ++y) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, fill_x, y), tiles[0].id_)
          << "y=" << y;
    }
    for (int y = kAnchorY + 2; y < kAnchorY + 2 + kBodyCount; ++y) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, edge_x, y), tiles[3].id_)
          << "y=" << y;
    }
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, edge_x, kAnchorY + kBodyCount + 2),
              tiles[4].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, edge_x, kAnchorY + kBodyCount + 3),
              tiles[5].id_);
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsEdgePlus7RepeatsOneTileForSizePlusEightRows) {
  auto& reg = DrawRoutineRegistry::Get();
  const DrawRoutineInfo* info =
      reg.GetRoutineInfo(DrawRoutineIds::kDownwardsEdge1x1_1to16plus7);
  ASSERT_NE(info, nullptr);

  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0310, 3), MakeTile(0x0311, 4), MakeTile(0x0312, 5)};
  constexpr int kAnchorX = 9;
  constexpr int kAnchorY = 6;

  for (uint8_t size : {uint8_t{0}, uint8_t{5}, uint8_t{15}}) {
    SCOPED_TRACE(::testing::Message() << "size=" << static_cast<int>(size));
    gfx::BackgroundBuffer bg;
    const RoomObject object(0x8B, kAnchorX, kAnchorY, size, 0);
    DrawContext ctx{bg,
                    object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    const int expected_rows = static_cast<int>(size) + 8;
    const auto points = CollectNonZeroTiles(bg);
    ASSERT_EQ(static_cast<int>(points.size()), expected_rows);
    for (int row = 0; row < expected_rows; ++row) {
      EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, kAnchorX, kAnchorY + row),
                tiles[0].id_)
          << "row=" << row;
    }
  }
}

TEST_F(DrawRoutineMappingTest,
       DownwardsCornerVariantsSkipOpeningCapWhenCornerAlreadyExists) {
  auto& reg = DrawRoutineRegistry::Get();
  const std::vector<gfx::TileInfo> tiles = {
      MakeTile(0x0300, 0), MakeTile(0x0301, 1), MakeTile(0x0302, 2),
      MakeTile(0x0303, 3), MakeTile(0x0304, 4), MakeTile(0x0305, 5)};

  struct Case {
    int routine_id;
    RoomObject object;
    int corner_x;
    int corner_y;
    int body_x;
    int body_y;
    int fill_x;
    int fill_y;
    int end_top_x;
    int end_bottom_x;
  };

  const std::vector<Case> cases = {
      {DrawRoutineIds::kDownwardsLeftCorners2x1_1to16_plus12,
       RoomObject(0x6C, 6, 8, 0, 0), 6, 8, 6, 8, 7, 8, 6, 6},
      {DrawRoutineIds::kDownwardsRightCorners2x1_1to16_plus12,
       RoomObject(0x6D, 6, 8, 0, 0), 7, 8, 7, 8, 6, 8, 7, 7},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    const uint16_t preexisting_corner =
        gfx::TileInfoToWord(MakeTile(0x00E3, 7));
    bg.SetTileAt(tc.corner_x, tc.corner_y, preexisting_corner);

    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.body_x, tc.body_y),
              tiles[3].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.fill_x, tc.fill_y),
              tiles[0].id_);
    EXPECT_EQ(DrawRoutineUtils::TileIdAt(bg, tc.end_top_x, tc.object.y_ + 10),
              tiles[4].id_);
    EXPECT_EQ(
        DrawRoutineUtils::TileIdAt(bg, tc.end_bottom_x, tc.object.y_ + 11),
        tiles[5].id_);
  }
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype1Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Test a few key mappings from bank_01.asm analysis

  // 0x00 -> Routine 0 (Rightwards2x2_1to15or32)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x00), 0);

  // 0x01-0x02 -> Routine 1 (Rightwards2x4_1to15or26)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x01), 1);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x02), 1);

  // 0x09 -> Routine 5 (DiagonalAcute_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x09), 5);

  // 0x15 -> Routine 17 (DiagonalAcute_BothBG)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x15), 17);

  // 0x33 -> Routine 16 (4x4)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x33), 16);
}

TEST_F(DrawRoutineMappingTest, MapsDownwardAndHorizontalVariantFamilies) {
  ObjectDrawer drawer(rom_.get(), 0);

  // 0x81-0x84: routine 65 (DownwardsDecor3x4spaced2_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x81), 65);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x84), 65);

  // 0x88: routine 66 (DownwardsBigRail3x1_1to16plus5)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x88), 66);

  // 0x89: routine 67 (DownwardsBlock2x2spaced2_1to16)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x89), 67);

  // 0xB0-0xB1: routine 72 (RightwardsEdge1x1_1to16plus7)
  EXPECT_EQ(drawer.GetDrawRoutineId(0xB0), 72);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xB1), 72);

  // USDASM $0197DC-$0197EC: 0xB5 repeats a 2x4 stamp horizontally for
  // size + 1 blocks through the current (single-layer) tilemap pointers.
  EXPECT_EQ(drawer.GetDrawRoutineId(0xB5), DrawRoutineIds::kWeird2x4_1to16);
  const DrawRoutineInfo* curtains = DrawRoutineRegistry::Get().GetRoutineInfo(
      DrawRoutineIds::kWeird2x4_1to16);
  ASSERT_NE(curtains, nullptr);
  EXPECT_EQ(curtains->base_width, 2);
  EXPECT_EQ(curtains->base_height, 4);
  EXPECT_EQ(curtains->min_tiles, 8);
  EXPECT_FALSE(curtains->draws_to_both_bgs);
  EXPECT_TRUE(DrawRoutineRegistry::Get().RoutineDrawsToBothBGs(
      DrawRoutineIds::kRightwards2x4_1to16));

  // USDASM $018314-$018318: only 0x8A is the +23 long rail; 0x8B/0x8C
  // use the single-tile downwards +7 routine.
  EXPECT_EQ(drawer.GetDrawRoutineId(0x8A),
            DrawRoutineIds::kDownwardsHasEdge1x1_1to16_plus23);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x8B),
            DrawRoutineIds::kDownwardsEdge1x1_1to16plus7);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x8C),
            DrawRoutineIds::kDownwardsEdge1x1_1to16plus7);
}

TEST_F(DrawRoutineMappingTest, MapsDiagonalCeilingFamilies) {
  ObjectDrawer drawer(rom_.get(), 0);

  // DiagonalCeilingTopLeft: 0xA0, 0xA5, 0xA9 -> routine 75
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA0), 75);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA5), 75);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA9), 75);

  // DiagonalCeilingBottomLeft: 0xA1, 0xA6, 0xAA -> routine 76
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA1), 76);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA6), 76);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAA), 76);

  // DiagonalCeilingTopRight: 0xA2, 0xA7, 0xAB -> routine 77
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA2), 77);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA7), 77);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAB), 77);

  // DiagonalCeilingBottomRight: 0xA3, 0xA8, 0xAC -> routine 78
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA3), 78);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xA8), 78);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xAC), 78);
}

TEST_F(DrawRoutineMappingTest,
       DiagonalCeilingRoutinesRenderExpectedOrientationAndArea) {
  auto& reg = DrawRoutineRegistry::Get();

  gfx::TileInfo fill_tile;
  fill_tile.id_ = 1;
  fill_tile.palette_ = 0;
  std::vector<gfx::TileInfo> tiles = {fill_tile};

  struct Case {
    int routine_id;
    RoomObject object;
    int expected_min_x;
    int expected_max_x;
    int expected_min_y;
    int expected_max_y;
    TilePoint anchor;
    TilePoint should_exist;
    TilePoint should_not_exist;
  };

  const std::vector<Case> cases = {
      {75,
       RoomObject(0xA0, 10, 10, 0, 0),
       10,
       13,
       10,
       13,
       {10, 10},
       {13, 10},
       {13, 13}},
      {76,
       RoomObject(0xA1, 10, 10, 0, 0),
       10,
       13,
       10,
       13,
       {10, 10},
       {13, 13},
       {13, 10}},
      {77,
       RoomObject(0xA2, 10, 10, 0, 0),
       10,
       13,
       10,
       13,
       {10, 10},
       {13, 13},
       {10, 13}},
      {78,
       RoomObject(0xA3, 10, 10, 0, 0),
       10,
       13,
       7,
       10,
       {10, 10},
       {13, 7},
       {10, 7}},
  };

  for (const auto& tc : cases) {
    SCOPED_TRACE(::testing::Message() << "routine=" << tc.routine_id);

    const DrawRoutineInfo* info = reg.GetRoutineInfo(tc.routine_id);
    ASSERT_NE(info, nullptr);

    gfx::BackgroundBuffer bg;
    DrawContext ctx{bg,
                    tc.object,
                    std::span<const gfx::TileInfo>(tiles),
                    /*state=*/nullptr,
                    rom_.get(),
                    /*room_id=*/0,
                    /*room_gfx_buffer=*/nullptr,
                    /*secondary_bg=*/nullptr};
    info->function(ctx);

    const auto points = CollectNonZeroTiles(bg);
    ASSERT_FALSE(points.empty());

    // size nibble=0 => side=(0+4), triangle area = 4+3+2+1 = 10 tiles.
    EXPECT_EQ(points.size(), 10u);

    int min_x = points.front().x;
    int max_x = points.front().x;
    int min_y = points.front().y;
    int max_y = points.front().y;
    for (const auto& point : points) {
      min_x = std::min(min_x, point.x);
      max_x = std::max(max_x, point.x);
      min_y = std::min(min_y, point.y);
      max_y = std::max(max_y, point.y);
    }

    EXPECT_EQ(min_x, tc.expected_min_x);
    EXPECT_EQ(max_x, tc.expected_max_x);
    EXPECT_EQ(min_y, tc.expected_min_y);
    EXPECT_EQ(max_y, tc.expected_max_y);

    EXPECT_TRUE(ContainsPoint(points, tc.anchor.x, tc.anchor.y));
    EXPECT_TRUE(ContainsPoint(points, tc.should_exist.x, tc.should_exist.y));
    EXPECT_FALSE(
        ContainsPoint(points, tc.should_not_exist.x, tc.should_not_exist.y));
  }
}

TEST_F(DrawRoutineMappingTest, MapsMovingWallAndChestPlatformFamilies) {
  ObjectDrawer drawer(rom_.get(), 0);

  // ClosedChestPlatform: 0xC1 -> routine 79
  EXPECT_EQ(drawer.GetDrawRoutineId(0xC1), 79);

  // MovingWallWest: 0xCD -> routine 80
  EXPECT_EQ(drawer.GetDrawRoutineId(0xCD), 80);

  // MovingWallEast: 0xCE -> routine 81
  EXPECT_EQ(drawer.GetDrawRoutineId(0xCE), 81);

  // OpenChestPlatform: 0xDC -> routine 82
  EXPECT_EQ(drawer.GetDrawRoutineId(0xDC), 82);

  // CheckIfWallIsMoved: 0xD3-0xD6 -> routine 38 (Nothing) - logic-only objects
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD3), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD4), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD5), 38);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xD6), 38);
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype2Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // 0x100-0x107 -> fixed RoomDraw_4x4, not the repeated subtype-1 wrapper.
  EXPECT_EQ(drawer.GetDrawRoutineId(0x100), DrawRoutineIds::kActual4x4);

  // 0x108 -> Routine 35 (4x4 Corner BothBG)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x108), 35);

  // 0x110 -> Routine 36 (Weird Corner Bottom)
  EXPECT_EQ(drawer.GetDrawRoutineId(0x110), 36);

  // Type-2 specials
  EXPECT_EQ(drawer.GetDrawRoutineId(0x122), DrawRoutineIds::kBed4x5);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x12C), DrawRoutineIds::kRightwards3x6);
  EXPECT_EQ(drawer.GetDrawRoutineId(0x13E), DrawRoutineIds::kUtility6x3);
}

TEST_F(DrawRoutineMappingTest,
       FixedCornersUseUsdasmSourcesAndBackgroundMetadata) {
  // Subtype-2 source offsets, in ID order, from $0183F0-$01841E. Keep the
  // non-monotonic order: the single-BG and dual-BG corner sets interleave.
  const std::vector<uint16_t> offsets = {
      0x0B66, 0x0B86, 0x0BA6, 0x0BC6, 0x0C66, 0x0C86, 0x0CA6, 0x0CC6,
      0x0BE6, 0x0C06, 0x0C26, 0x0C46, 0x0CE6, 0x0D06, 0x0D26, 0x0D46,
      0x0D66, 0x0D7E, 0x0D96, 0x0DAE, 0x0DC6, 0x0DDE, 0x0DF6, 0x0E0E,
  };
  for (int index = 0; index < static_cast<int>(offsets.size()); ++index) {
    SCOPED_TRACE(index + 0x100);
    const int id = index + 0x100;
    const int width = id >= 0x110 && id <= 0x113 ? 3 : 4;
    const int height = id >= 0x114 ? 3 : 4;
    const int source = kRoomObjectTileAddress + offsets[index];
    std::vector<uint8_t> data(1024 * 1024, 0);
    data[kRoomObjectSubtype2 + index * 2] = offsets[index] & 0xFF;
    data[kRoomObjectSubtype2 + index * 2 + 1] = offsets[index] >> 8;
    for (int slot = 0; slot <= width * height; ++slot) {
      data[source + slot * 2] = slot;
      data[source + slot * 2 + 1] = 0x29;
    }
    ASSERT_TRUE(rom_->LoadFromData(data).ok());
    ObjectParser parser(rom_.get());
    const auto tiles = parser.ParseObject(id);
    ASSERT_TRUE(tiles.ok()) << tiles.status();
    ASSERT_EQ(tiles->size(), width * height);
    for (int slot = 0; slot < width * height; ++slot) {
      EXPECT_EQ(gfx::TileInfoToWord((*tiles)[slot]), 0x2900 + slot);
    }
    const auto ranges = parser.ResolveTileReadRanges(id);
    ASSERT_TRUE(ranges.ok()) << ranges.status();
    ASSERT_EQ(ranges->size(), 1u);
    EXPECT_EQ(ranges->front().begin, source);
    EXPECT_EQ(ranges->front().end, source + width * height * 2);

    const auto& registry = DrawRoutineRegistry::Get();
    const auto* routine =
        registry.GetRoutineInfo(registry.GetRoutineIdForObject(id));
    ASSERT_NE(routine, nullptr);
    EXPECT_EQ(routine->draws_to_both_bgs, id >= 0x108);
    EXPECT_EQ(routine->base_width, width);
    EXPECT_EQ(routine->base_height, height);
    for (int size : {0, 1, 15}) {
      EXPECT_EQ(ObjectDimensionTable::Get().GetDimensions(id, size),
                std::make_pair(width, height));
    }
  }
}

TEST_F(DrawRoutineMappingTest, SanctuaryWallUsesUsdasmPayloadAndFootprint) {
  // $018468 selects obj1458; $019B56 consumes two six-word facade columns
  // and a four-column, three-row center pattern: 24 source words total.
  std::vector<uint8_t> data(1024 * 1024, 0);
  constexpr int kPointer = kRoomObjectSubtype2 + 0x3C * 2;
  constexpr int kSource = kRoomObjectTileAddress + 0x1458;
  data[kPointer] = 0x58;
  data[kPointer + 1] = 0x14;
  for (int i = 0; i < 25; ++i) {
    data[kSource + i * 2] = static_cast<uint8_t>(i);
    data[kSource + i * 2 + 1] = 0x1D;
  }
  ASSERT_TRUE(rom_->LoadFromData(data).ok());
  ObjectParser parser(rom_.get());
  const auto tiles = parser.ParseObject(0x13C);
  ASSERT_TRUE(tiles.ok()) << tiles.status();
  ASSERT_EQ(tiles->size(), 24u);
  EXPECT_EQ(gfx::TileInfoToWord(tiles->back()), 0x1D17);
  const auto ranges = parser.ResolveTileReadRanges(0x13C);
  ASSERT_TRUE(ranges.ok()) << ranges.status();
  ASSERT_EQ(ranges->size(), 1u);
  EXPECT_EQ(ranges->front().begin, kSource);
  EXPECT_EQ(ranges->front().end, kSource + 48);

  const auto& registry = DrawRoutineRegistry::Get();
  const auto* routine =
      registry.GetRoutineInfo(registry.GetRoutineIdForObject(0x13C));
  ASSERT_NE(routine, nullptr);
  EXPECT_EQ(routine->base_width, 24);
  EXPECT_EQ(routine->base_height, 6);
  EXPECT_EQ(routine->min_tiles, 24);
  EXPECT_FALSE(routine->draws_to_both_bgs);  // Only the center uses active BG.
  for (int size : {0, 1, 15}) {
    EXPECT_EQ(ObjectDimensionTable::Get().GetDimensions(0x13C, size),
              std::make_pair(24, 6));
  }
  for (int layer : {0, 1, 2}) {
    const RoomObject object(0x13C, 10, 20, 0, layer);
    const auto semantics = GetObjectLayerSemantics(object);
    EXPECT_FALSE(semantics.draws_to_both_bgs);
    EXPECT_EQ(semantics.effective_bg_layer, layer == 1
                                                ? EffectiveBgLayer::kBothBg1Bg2
                                                : EffectiveBgLayer::kBg1);
    EXPECT_EQ(semantics.render_routing, layer == 1
                                            ? ObjectRenderRouting::kMixedBg1Bg2
                                            : ObjectRenderRouting::kFixedBg1);
  }
}

TEST_F(DrawRoutineMappingTest, VerifiesSubtype3Mappings) {
  ObjectDrawer drawer(rom_.get(), 0);

  // Type-3 key mappings from usdasm routine table.
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF80), DrawRoutineIds::kEmptyWaterFace);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF83), DrawRoutineIds::kSomariaLine);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF8D), DrawRoutineIds::kPrisonCell);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF90), DrawRoutineIds::kSingle2x2);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF92), DrawRoutineIds::kRupeeFloor);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xF96), DrawRoutineIds::kSingle2x2);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFAB), DrawRoutineIds::kSingle2x2);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFAC), DrawRoutineIds::kBigGrayRock);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFAD), DrawRoutineIds::kAgahnimsAltar);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFD4), DrawRoutineIds::kFortuneTellerRoom);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFB1), DrawRoutineIds::kSingle4x3);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFD5), DrawRoutineIds::kUtility3x5);
  for (int object_id : {0xFD6, 0xFD7, 0xFD8, 0xFD9}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(object_id), DrawRoutineIds::kSingle2x2);
  }
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFCC), DrawRoutineIds::kSmithyFurnace);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFDA), DrawRoutineIds::kTableBowl);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFDB), DrawRoutineIds::kUtility3x5);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFBA),
            DrawRoutineIds::kVerticalTurtleRockPipe);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFBC),
            DrawRoutineIds::kHorizontalTurtleRockPipe);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE0),
            DrawRoutineIds::kArcheryGameTargetDoor);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE6), DrawRoutineIds::kActual4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFE9), DrawRoutineIds::kSolidWallDecor3x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFEB), DrawRoutineIds::kSingle4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF0), DrawRoutineIds::kLightBeam);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF1), DrawRoutineIds::kBigLightBeam);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF2), DrawRoutineIds::kBossShell4x4);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF4), DrawRoutineIds::kFloorLight);
  EXPECT_EQ(drawer.GetDrawRoutineId(0xFF8),
            DrawRoutineIds::kGanonTriforceFloorDecor);
  for (int object_id : {0xFCB, 0xFF6, 0xFF7}) {
    EXPECT_EQ(drawer.GetDrawRoutineId(object_id),
              DrawRoutineIds::kBigWallDecor);
  }

  const DrawRoutineInfo* floor_light =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kFloorLight);
  ASSERT_NE(floor_light, nullptr);
  EXPECT_EQ(floor_light->base_width, 8);
  EXPECT_EQ(floor_light->base_height, 8);
  EXPECT_EQ(floor_light->min_tiles, 64);
  EXPECT_FALSE(floor_light->draws_to_both_bgs);

  const DrawRoutineInfo* big_wall_decor =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kBigWallDecor);
  ASSERT_NE(big_wall_decor, nullptr);
  EXPECT_EQ(big_wall_decor->base_width, 8);
  EXPECT_EQ(big_wall_decor->base_height, 3);
  EXPECT_EQ(big_wall_decor->min_tiles, 24);
  EXPECT_FALSE(big_wall_decor->draws_to_both_bgs);

  const DrawRoutineInfo* table_bowl =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kTableBowl);
  ASSERT_NE(table_bowl, nullptr);
  EXPECT_EQ(table_bowl->name, "TableBowl");
  EXPECT_TRUE(static_cast<bool>(table_bowl->function));
  EXPECT_EQ(table_bowl->base_width, 4);
  EXPECT_EQ(table_bowl->base_height, 2);
  EXPECT_EQ(table_bowl->min_tiles, 8);
  EXPECT_FALSE(table_bowl->draws_to_both_bgs);
  EXPECT_EQ(table_bowl->category, DrawRoutineInfo::Category::Special);

  const DrawRoutineInfo* smithy_furnace =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kSmithyFurnace);
  ASSERT_NE(smithy_furnace, nullptr);
  EXPECT_EQ(smithy_furnace->name, "SmithyFurnace");
  EXPECT_TRUE(static_cast<bool>(smithy_furnace->function));
  EXPECT_EQ(smithy_furnace->base_width, 6);
  EXPECT_EQ(smithy_furnace->base_height, 8);
  EXPECT_EQ(smithy_furnace->min_tiles, 48);
  EXPECT_FALSE(smithy_furnace->draws_to_both_bgs);
  EXPECT_EQ(smithy_furnace->category, DrawRoutineInfo::Category::Special);

  const DrawRoutineInfo* big_gray_rock =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kBigGrayRock);
  ASSERT_NE(big_gray_rock, nullptr);
  EXPECT_EQ(big_gray_rock->name, "BigGrayRock");
  EXPECT_TRUE(static_cast<bool>(big_gray_rock->function));
  EXPECT_EQ(big_gray_rock->base_width, 4);
  EXPECT_EQ(big_gray_rock->base_height, 4);
  EXPECT_EQ(big_gray_rock->min_tiles, 16);
  EXPECT_FALSE(big_gray_rock->draws_to_both_bgs);
  EXPECT_EQ(big_gray_rock->category, DrawRoutineInfo::Category::Special);

  const DrawRoutineInfo* agahnims_altar =
      DrawRoutineRegistry::Get().GetRoutineInfo(DrawRoutineIds::kAgahnimsAltar);
  ASSERT_NE(agahnims_altar, nullptr);
  EXPECT_EQ(agahnims_altar->name, "AgahnimsAltar");
  EXPECT_TRUE(static_cast<bool>(agahnims_altar->function));
  EXPECT_EQ(agahnims_altar->base_width, 14);
  EXPECT_EQ(agahnims_altar->base_height, 14);
  EXPECT_EQ(agahnims_altar->min_tiles, 84);
  EXPECT_FALSE(agahnims_altar->draws_to_both_bgs);
  EXPECT_EQ(agahnims_altar->category, DrawRoutineInfo::Category::Special);

  const DrawRoutineInfo* fortune_teller_room =
      DrawRoutineRegistry::Get().GetRoutineInfo(
          DrawRoutineIds::kFortuneTellerRoom);
  ASSERT_NE(fortune_teller_room, nullptr);
  EXPECT_EQ(fortune_teller_room->name, "FortuneTellerRoom");
  EXPECT_TRUE(static_cast<bool>(fortune_teller_room->function));
  EXPECT_EQ(fortune_teller_room->base_width, 14);
  EXPECT_EQ(fortune_teller_room->base_height, 14);
  EXPECT_EQ(fortune_teller_room->min_tiles, 26);
  EXPECT_FALSE(fortune_teller_room->draws_to_both_bgs);
  EXPECT_EQ(fortune_teller_room->category, DrawRoutineInfo::Category::Special);

  const auto& routines = DrawRoutineRegistry::Get().GetAllRoutines();
  EXPECT_EQ(std::count_if(routines.begin(), routines.end(),
                          [](const DrawRoutineInfo& info) {
                            return info.id == DrawRoutineIds::kTableBowl;
                          }),
            1);
  EXPECT_EQ(std::count_if(routines.begin(), routines.end(),
                          [](const DrawRoutineInfo& info) {
                            return info.id == DrawRoutineIds::kSmithyFurnace;
                          }),
            1);
  EXPECT_EQ(std::count_if(routines.begin(), routines.end(),
                          [](const DrawRoutineInfo& info) {
                            return info.id == DrawRoutineIds::kBigGrayRock;
                          }),
            1);
  EXPECT_EQ(std::count_if(routines.begin(), routines.end(),
                          [](const DrawRoutineInfo& info) {
                            return info.id == DrawRoutineIds::kAgahnimsAltar;
                          }),
            1);
  EXPECT_EQ(std::count_if(routines.begin(), routines.end(),
                          [](const DrawRoutineInfo& info) {
                            return info.id ==
                                   DrawRoutineIds::kFortuneTellerRoom;
                          }),
            1);
}

}  // namespace zelda3
}  // namespace yaze
