#include "app/gui/widgets/resize_handles.h"

#include <gtest/gtest.h>

#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace {

using resize_handles_internal::ApplyDragDelta;
using resize_handles_internal::GetDefaultHandleColor;
using resize_handles_internal::HitTestZone;
using resize_handles_internal::SnapCoord;

class ResizeHandlesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    unsigned char* pixels = nullptr;
    int w = 0, h = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGui::NewFrame();
  }

  void TearDown() override {
    ImGui::EndFrame();
    ImGui::DestroyContext(context_);
    context_ = nullptr;
  }

  ImGuiContext* context_ = nullptr;
};

TEST_F(ResizeHandlesTest, HandleMaskCornersOnlyRejectsEdgeHits) {
  const ImRect rect(100.0f, 100.0f, 200.0f, 200.0f);
  // Mouse in the middle of the top edge (not a corner).
  EXPECT_EQ(
      HitTestZone(ImVec2(150.0f, 100.0f), rect, HandleMask::kCorners, 8.0f),
      HandleZone::kNone);
  // Corners still hit.
  EXPECT_EQ(
      HitTestZone(ImVec2(100.0f, 100.0f), rect, HandleMask::kCorners, 8.0f),
      HandleZone::kNW);
  EXPECT_EQ(
      HitTestZone(ImVec2(200.0f, 200.0f), rect, HandleMask::kCorners, 8.0f),
      HandleZone::kSE);
}

TEST_F(ResizeHandlesTest, HandleMaskEdgesOnlyRejectsCornerHits) {
  const ImRect rect(100.0f, 100.0f, 200.0f, 200.0f);
  // Corner hit under edges-only mask returns none.
  EXPECT_EQ(HitTestZone(ImVec2(100.0f, 100.0f), rect, HandleMask::kEdges, 8.0f),
            HandleZone::kNone);
  // Edge midpoint still hits.
  EXPECT_EQ(HitTestZone(ImVec2(150.0f, 100.0f), rect, HandleMask::kEdges, 8.0f),
            HandleZone::kN);
  EXPECT_EQ(HitTestZone(ImVec2(150.0f, 200.0f), rect, HandleMask::kEdges, 8.0f),
            HandleZone::kS);
  EXPECT_EQ(HitTestZone(ImVec2(100.0f, 150.0f), rect, HandleMask::kEdges, 8.0f),
            HandleZone::kW);
  EXPECT_EQ(HitTestZone(ImVec2(200.0f, 150.0f), rect, HandleMask::kEdges, 8.0f),
            HandleZone::kE);
}

TEST_F(ResizeHandlesTest, HandleMaskAllHitsAllEightZones) {
  const ImRect rect(100.0f, 100.0f, 200.0f, 200.0f);
  EXPECT_EQ(HitTestZone(ImVec2(100.0f, 100.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kNW);
  EXPECT_EQ(HitTestZone(ImVec2(200.0f, 100.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kNE);
  EXPECT_EQ(HitTestZone(ImVec2(100.0f, 200.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kSW);
  EXPECT_EQ(HitTestZone(ImVec2(200.0f, 200.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kSE);
  EXPECT_EQ(HitTestZone(ImVec2(150.0f, 100.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kN);
  EXPECT_EQ(HitTestZone(ImVec2(150.0f, 200.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kS);
  EXPECT_EQ(HitTestZone(ImVec2(100.0f, 150.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kW);
  EXPECT_EQ(HitTestZone(ImVec2(200.0f, 150.0f), rect, HandleMask::kAll, 8.0f),
            HandleZone::kE);
}

TEST_F(ResizeHandlesTest, SnapRoundsToNearestMultiple) {
  EXPECT_FLOAT_EQ(SnapCoord(14.0f, 8.0f), 16.0f);
  EXPECT_FLOAT_EQ(SnapCoord(3.0f, 8.0f), 0.0f);
  // std::round is half-away-from-zero, so 4 → 8 (not 0).
  EXPECT_FLOAT_EQ(SnapCoord(4.0f, 8.0f), 8.0f);
  EXPECT_FLOAT_EQ(SnapCoord(5.0f, 8.0f), 8.0f);
  EXPECT_FLOAT_EQ(SnapCoord(-14.0f, 8.0f), -16.0f);
  // snap <= 0 is a no-op.
  EXPECT_FLOAT_EQ(SnapCoord(7.3f, 0.0f), 7.3f);
  EXPECT_FLOAT_EQ(SnapCoord(7.3f, -1.0f), 7.3f);
}

TEST_F(ResizeHandlesTest, MinSizeClampPreventsInvertedRect) {
  // Drag the NW corner past the SE corner with a huge positive delta.
  ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  ApplyDragDelta(&rect, HandleZone::kNW, ImVec2(300.0f, 300.0f), 0.0f, 4.0f,
                 4.0f);
  // NW is clamped so the rect preserves the 4x4 minimum.
  EXPECT_FLOAT_EQ(rect.Max.x - rect.Min.x, 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.y - rect.Min.y, 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.x, 100.0f);  // SE anchor untouched
  EXPECT_FLOAT_EQ(rect.Max.y, 100.0f);
  EXPECT_FLOAT_EQ(rect.Min.x, 96.0f);  // NW pushed to the min distance
  EXPECT_FLOAT_EQ(rect.Min.y, 96.0f);
}

TEST_F(ResizeHandlesTest, MinSizeClampWorksOnSECorner) {
  // Drag SE inward past NW with a huge negative delta.
  ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  ApplyDragDelta(&rect, HandleZone::kSE, ImVec2(-300.0f, -300.0f), 0.0f, 4.0f,
                 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.x - rect.Min.x, 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.y - rect.Min.y, 4.0f);
  EXPECT_FLOAT_EQ(rect.Min.x, 0.0f);  // NW anchor untouched
  EXPECT_FLOAT_EQ(rect.Min.y, 0.0f);
  EXPECT_FLOAT_EQ(rect.Max.x, 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.y, 4.0f);
}

TEST_F(ResizeHandlesTest, EdgeDragMovesOnlyOneAxis) {
  ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  ApplyDragDelta(&rect, HandleZone::kE, ImVec2(10.0f, 20.0f), 0.0f, 4.0f, 4.0f);
  EXPECT_FLOAT_EQ(rect.Max.x, 110.0f);  // moved
  EXPECT_FLOAT_EQ(rect.Max.y, 100.0f);  // untouched
  EXPECT_FLOAT_EQ(rect.Min.x, 0.0f);
  EXPECT_FLOAT_EQ(rect.Min.y, 0.0f);
}

TEST_F(ResizeHandlesTest, SnapAppliedDuringDrag) {
  ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  ApplyDragDelta(&rect, HandleZone::kSE, ImVec2(3.0f, 11.0f), 8.0f, 4.0f, 4.0f);
  // 100 + 3 = 103 → snaps to 104; 100 + 11 = 111 → snaps to 112.
  EXPECT_FLOAT_EQ(rect.Max.x, 104.0f);
  EXPECT_FLOAT_EQ(rect.Max.y, 112.0f);
}

TEST_F(ResizeHandlesTest, DefaultColorComesFromTheme) {
  const Color expected = ThemeManager::Get().GetCurrentTheme().selection_handle;
  const ImVec4 actual = GetDefaultHandleColor();
  EXPECT_FLOAT_EQ(actual.x, expected.red);
  EXPECT_FLOAT_EQ(actual.y, expected.green);
  EXPECT_FLOAT_EQ(actual.z, expected.blue);
  EXPECT_FLOAT_EQ(actual.w, expected.alpha);
}

TEST_F(ResizeHandlesTest, RectIsUntouchedWhenNoMouseInteraction) {
  ASSERT_TRUE(ImGui::Begin("root"));
  ImRect rect(10.0f, 20.0f, 110.0f, 120.0f);
  const ImRect before = rect;
  const bool committed = ResizeHandles(&rect);
  EXPECT_FALSE(committed);
  EXPECT_FLOAT_EQ(rect.Min.x, before.Min.x);
  EXPECT_FLOAT_EQ(rect.Min.y, before.Min.y);
  EXPECT_FLOAT_EQ(rect.Max.x, before.Max.x);
  EXPECT_FLOAT_EQ(rect.Max.y, before.Max.y);
  ImGui::End();
}

TEST_F(ResizeHandlesTest, ApplyDragWithNoneZoneIsNoop) {
  ImRect rect(0.0f, 0.0f, 100.0f, 100.0f);
  const ImRect before = rect;
  ApplyDragDelta(&rect, HandleZone::kNone, ImVec2(50.0f, 50.0f), 0.0f, 4.0f,
                 4.0f);
  EXPECT_FLOAT_EQ(rect.Min.x, before.Min.x);
  EXPECT_FLOAT_EQ(rect.Min.y, before.Min.y);
  EXPECT_FLOAT_EQ(rect.Max.x, before.Max.x);
  EXPECT_FLOAT_EQ(rect.Max.y, before.Max.y);
}

}  // namespace
}  // namespace gui
}  // namespace yaze
