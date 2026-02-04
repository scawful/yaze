#include "gtest/gtest.h"

#include "app/gui/app/editor_layout.h"

namespace yaze::gui {
namespace {

TEST(PanelWindowTest, StableIdAppendsHiddenSuffix) {
  PanelWindow window("Room List", "ICON");
  window.SetStableId("panel.room_list");

  EXPECT_STREQ(window.GetWindowName(), "ICON Room List##panel.room_list");
}

TEST(PanelWindowTest, EmptyStableIdKeepsLabel) {
  PanelWindow window("Room List", "ICON");

  EXPECT_STREQ(window.GetWindowName(), "ICON Room List");
}

}  // namespace
}  // namespace yaze::gui
