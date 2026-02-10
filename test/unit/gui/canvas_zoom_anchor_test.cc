#include <gtest/gtest.h>

#include "app/gui/canvas/canvas_geometry.h"
#include "testing.h"

namespace yaze::test {

TEST(CanvasGeometryTest, ComputeScrollForZoomAtScreenPos_LocksPointUnderMouse) {
  gui::CanvasGeometry geom;
  geom.canvas_p0 = ImVec2(10.0f, 20.0f);
  geom.scrolling = ImVec2(5.0f, -5.0f);

  const float old_scale = 1.0f;
  const float new_scale = 2.0f;
  const ImVec2 mouse_screen_pos(123.0f, 77.0f);

  const ImVec2 new_scroll =
      gui::ComputeScrollForZoomAtScreenPos(geom, old_scale, new_scale,
                                           mouse_screen_pos);

  // Verify: the canvas-space coordinate under the mouse is invariant.
  const ImVec2 old_origin = gui::GetCanvasOrigin(geom);
  const ImVec2 canvas_coord((mouse_screen_pos.x - old_origin.x) / old_scale,
                            (mouse_screen_pos.y - old_origin.y) / old_scale);

  gui::CanvasGeometry geom_zoomed = geom;
  geom_zoomed.scrolling = new_scroll;
  const ImVec2 new_origin = gui::GetCanvasOrigin(geom_zoomed);

  const ImVec2 mouse_after(new_origin.x + canvas_coord.x * new_scale,
                           new_origin.y + canvas_coord.y * new_scale);

  EXPECT_FLOAT_EQ(mouse_after.x, mouse_screen_pos.x);
  EXPECT_FLOAT_EQ(mouse_after.y, mouse_screen_pos.y);
}

}  // namespace yaze::test

