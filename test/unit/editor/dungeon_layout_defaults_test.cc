#include "app/editor/layout/layout_presets.h"

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

bool ContainsPanel(const std::vector<std::string>& panels,
                   const char* panel_id) {
  return std::find(panels.begin(), panels.end(), panel_id) != panels.end();
}

TEST(DungeonLayoutDefaultsTest, WorkbenchIsSoleDefaultSurface) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kDungeon);

  ASSERT_EQ(preset.default_visible_panels.size(), 1U);
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonWorkbench));
  EXPECT_TRUE(preset.dock_only_default_visible_panels);

  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomSelector));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonObjectSelector));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonObjectEditor));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomGraphics));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomMatrix));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonDoorEditor));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonPaletteEditor));

  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonRoomSelector));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonObjectSelector));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonRoomGraphics));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonRoomMatrix));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonDoorEditor));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kDungeonPaletteEditor));

  auto selector_pos = preset.panel_positions.find(
      LayoutPresets::Panels::kDungeonObjectSelector);
  ASSERT_NE(selector_pos, preset.panel_positions.end());
  EXPECT_EQ(selector_pos->second, DockPosition::RightTop);

  EXPECT_EQ(
      preset.panel_positions.count(LayoutPresets::Panels::kDungeonObjectEditor),
      0U);

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
