#include "app/editor/layout/layout_presets.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(DungeonLayoutDefaultsTest,
     DefaultPlacesPaletteEditorUnderRoomGraphics) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kDungeon);

  auto graphics_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonRoomGraphics);
  ASSERT_NE(graphics_pos, preset.panel_positions.end());
  EXPECT_EQ(graphics_pos->second, DockPosition::RightTop);

  auto palette_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonPaletteEditor);
  ASSERT_NE(palette_pos, preset.panel_positions.end());
  EXPECT_EQ(palette_pos->second, DockPosition::RightBottom);
}

}  // namespace
}  // namespace yaze::editor
