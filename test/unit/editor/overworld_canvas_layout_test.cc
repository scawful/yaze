#include "app/editor/overworld/canvas/overworld_canvas_layout.h"

#include <limits>

#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "gtest/gtest.h"
#include "zelda3/common.h"

namespace yaze::editor {
namespace {

TEST(OverworldCanvasLayoutTest, SingleModeMatchesLegacyMathForAll192Maps) {
  const OverworldCanvasLayout layout(OverworldCanvasLayoutMode::kSingleWorld);
  EXPECT_TRUE(layout.IsSingleWorld());
  EXPECT_FALSE(layout.IsUnified());
  EXPECT_FLOAT_EQ(layout.CanvasSize().x, 4096.0f);
  EXPECT_FLOAT_EQ(layout.CanvasSize().y, 4096.0f);
  EXPECT_FLOAT_EQ(layout.LyraTailOrigin().x, 0.0f);
  EXPECT_FLOAT_EQ(layout.LyraTailOrigin().y, 2048.0f);

  for (int map_id = 0; map_id < zelda3::kExpandedMapCount; ++map_id) {
    const int legacy_world = map_id / 0x40;
    const int legacy_local = map_id % 0x40;
    const float legacy_x = static_cast<float>((legacy_local % 8) * 512);
    const float legacy_y = static_cast<float>((legacy_local / 8) * 512);

    EXPECT_EQ(OverworldCanvasLayout::WorldForMap(map_id), legacy_world);
    EXPECT_EQ(OverworldCanvasLayout::LocalIndex(map_id), legacy_local);
    EXPECT_EQ(OverworldCanvasLayout::MapIdFromLocal(
                  legacy_world, legacy_local % 8, legacy_local / 8),
              map_id);

    const ImVec2 origin = layout.MapOrigin(map_id);
    EXPECT_FLOAT_EQ(origin.x, legacy_x);
    EXPECT_FLOAT_EQ(origin.y, legacy_y);

    for (const ImVec2 offset : {ImVec2(0.0f, 0.0f), ImVec2(511.9f, 0.0f),
                                ImVec2(0.0f, 511.9f), ImVec2(511.9f, 511.9f)}) {
      const auto resolved =
          layout.MapAt(ImVec2(origin.x + offset.x, origin.y + offset.y),
                       legacy_world, zelda3::kExpandedMapCount);
      ASSERT_TRUE(resolved.has_value()) << "map_id=" << map_id;
      EXPECT_EQ(*resolved, map_id);
    }
  }
}

TEST(OverworldCanvasLayoutTest, UnifiedModeMapsBlocksGutterAndEmptyQuadrant) {
  const OverworldCanvasLayout layout(OverworldCanvasLayoutMode::kUnified);
  EXPECT_TRUE(layout.IsUnified());
  EXPECT_EQ(kUnifiedOverworldGutterPx, 32);
  EXPECT_FLOAT_EQ(layout.CanvasSize().x, 8224.0f);
  EXPECT_FLOAT_EQ(layout.CanvasSize().y, 8224.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(0).x, 0.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(0).y, 0.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(1).x, 0.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(1).y, 4128.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(2).x, 4128.0f);
  EXPECT_FLOAT_EQ(layout.BlockOrigin(2).y, 0.0f);
  EXPECT_FLOAT_EQ(layout.LyraTailOrigin().x, 4128.0f);
  EXPECT_FLOAT_EQ(layout.LyraTailOrigin().y, 2048.0f);

  // Every 0x00..0xBF map round-trips regardless of current_world hint.
  for (int map_id = 0; map_id < zelda3::kExpandedMapCount; ++map_id) {
    const ImVec2 origin = layout.MapOrigin(map_id);
    const auto resolved =
        layout.MapAt(ImVec2(origin.x + 256.0f, origin.y + 256.0f),
                     /*current_world=*/0, zelda3::kExpandedMapCount);
    ASSERT_TRUE(resolved.has_value()) << "map_id=" << map_id;
    EXPECT_EQ(*resolved, map_id);
  }

  // Spot-check $B0 (row 6 col 0 in Special Overworld east block).
  const ImVec2 b0_origin = layout.MapOrigin(0xB0);
  EXPECT_FLOAT_EQ(b0_origin.x, 4128.0f);
  EXPECT_FLOAT_EQ(b0_origin.y, 3072.0f);
  EXPECT_EQ(layout.MapAt(ImVec2(b0_origin.x + 16.0f, b0_origin.y + 16.0f), 0,
                         zelda3::kExpandedMapCount),
            0xB0);

  // Gutters, SE empty quadrant, and out-of-bounds return nullopt.
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (const ImVec2 bad_pt :
       {ImVec2(-1.0f, 100.0f), ImVec2(100.0f, -1.0f), ImVec2(4096.0f, 100.0f),
        ImVec2(4127.5f, 100.0f), ImVec2(100.0f, 4096.0f),
        ImVec2(100.0f, 4127.5f), ImVec2(4128.0f, 4128.0f),
        ImVec2(6000.0f, 6000.0f), ImVec2(8224.0f, 100.0f),
        ImVec2(100.0f, 8224.0f), ImVec2(nan, 100.0f)}) {
    EXPECT_FALSE(layout.MapAt(bad_pt, 0, zelda3::kExpandedMapCount).has_value())
        << "point=(" << bad_pt.x << "," << bad_pt.y << ")";
  }
}

TEST(OverworldCanvasLayoutTest,
     UnifiedModeRejectsTailRowsWhenRomHas160MapsOnly) {
  const OverworldCanvasLayout layout(OverworldCanvasLayoutMode::kUnified);
  const ImVec2 special_9f = layout.MapOrigin(0x9F);
  const ImVec2 tail_a0 = layout.MapOrigin(0xA0);

  EXPECT_EQ(layout.MapAt(ImVec2(special_9f.x + 16.0f, special_9f.y + 16.0f), 0,
                         zelda3::kNumOverworldMaps),
            0x9F);
  EXPECT_FALSE(layout
                   .MapAt(ImVec2(tail_a0.x + 16.0f, tail_a0.y + 16.0f), 0,
                          zelda3::kNumOverworldMaps)
                   .has_value());
}

TEST(OverworldCanvasLayoutTest, MinZoomAndFitScaleAllowFullUnifiedOverview) {
  const OverworldCanvasLayout single(OverworldCanvasLayoutMode::kSingleWorld);
  const OverworldCanvasLayout unified(OverworldCanvasLayoutMode::kUnified);
  const ImVec2 viewport(900.0f, 700.0f);

  EXPECT_FLOAT_EQ(single.MinZoom(viewport, 0, zelda3::kNumOverworldMaps),
                  kOverworldMinZoom);
  const float unified_min =
      unified.MinZoom(viewport, 0, zelda3::kExpandedMapCount);
  const float unified_fit =
      unified.FitScale(viewport, 0, zelda3::kExpandedMapCount);
  EXPECT_LT(unified_min, kOverworldMinZoom);
  EXPECT_LE(unified_min, unified_fit);
  EXPECT_LE(unified_fit * 8224.0f, viewport.y + 0.01f);
}

TEST(OverworldCanvasLayoutTest,
     ResolveOverworldContextTargetNormalizesUnifiedCoordinatesToWorldLocal) {
  const OverworldCanvasLayout unified(OverworldCanvasLayoutMode::kUnified);
  // Click at tile (2, 3) on map $B0 inside Special Overworld east block at 0.5x.
  const ImVec2 b0_origin = unified.MapOrigin(0xB0);  // (4128, 3072)
  const ImVec2 canvas_pt(b0_origin.x + 40.0f, b0_origin.y + 56.0f);
  const ImVec2 origin(100.0f, 50.0f);
  const ImVec2 scroll(-20.0f, -10.0f);
  constexpr float scale = 0.5f;
  const ImVec2 screen_pos(origin.x + scroll.x + canvas_pt.x * scale,
                          origin.y + scroll.y + canvas_pt.y * scale);

  const auto target = ResolveOverworldContextTarget(
      unified, /*current_world=*/0, /*game_state=*/1, screen_pos, origin,
      scroll, scale, zelda3::kExpandedMapCount);
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->world, 2);
  EXPECT_EQ(target->map_id, 0xB0);
  EXPECT_FLOAT_EQ(target->world_position.x, 40.0f);
  EXPECT_FLOAT_EQ(target->world_position.y, 3072.0f + 56.0f);
}

}  // namespace
}  // namespace yaze::editor
