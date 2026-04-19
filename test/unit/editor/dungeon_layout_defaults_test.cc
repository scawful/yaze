#include "app/editor/layout/layout_presets.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

bool ContainsPanel(const std::vector<std::string>& panels, const char* panel_id) {
  return std::find(panels.begin(), panels.end(), panel_id) != panels.end();
}

int IndexOfPanel(const std::vector<std::string>& panels, const char* panel_id) {
  const auto it = std::find(panels.begin(), panels.end(), panel_id);
  return it == panels.end() ? -1 : static_cast<int>(std::distance(panels.begin(), it));
}

TEST(DungeonLayoutDefaultsTest, WorkbenchDefaultUsesRightSideToolStacks) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kDungeon);

  EXPECT_TRUE(
      ContainsPanel(preset.default_visible_panels, LayoutPresets::Panels::kDungeonWorkbench));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomSelector));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonObjectEditor));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonRoomGraphics));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonRoomMatrix));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonDoorEditor));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonPaletteEditor));

  EXPECT_LT(IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonObjectEditor),
            IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonRoomGraphics));
  EXPECT_LT(IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonRoomMatrix),
            IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonDoorEditor));
  EXPECT_LT(IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonDoorEditor),
            IndexOfPanel(preset.default_visible_panels,
                         LayoutPresets::Panels::kDungeonPaletteEditor));

  auto object_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonObjectEditor);
  ASSERT_NE(object_pos, preset.panel_positions.end());
  EXPECT_EQ(object_pos->second, DockPosition::RightTop);

  auto graphics_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonRoomGraphics);
  ASSERT_NE(graphics_pos, preset.panel_positions.end());
  EXPECT_EQ(graphics_pos->second, DockPosition::RightTop);

  auto matrix_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonRoomMatrix);
  ASSERT_NE(matrix_pos, preset.panel_positions.end());
  EXPECT_EQ(matrix_pos->second, DockPosition::RightBottom);

  auto door_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonDoorEditor);
  ASSERT_NE(door_pos, preset.panel_positions.end());
  EXPECT_EQ(door_pos->second, DockPosition::RightBottom);

  auto palette_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kDungeonPaletteEditor);
  ASSERT_NE(palette_pos, preset.panel_positions.end());
  EXPECT_EQ(palette_pos->second, DockPosition::RightBottom);
}

}  // namespace
}  // namespace yaze::editor
