#include "app/editor/layout/layout_presets.h"

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

bool ContainsPanel(const std::vector<std::string>& panels, const char* panel_id) {
  return std::find(panels.begin(), panels.end(), panel_id) != panels.end();
}

TEST(LayoutPresetsTest, OverworldDefaultUsesBalancedPanelArrangement) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kOverworld);

  EXPECT_TRUE(
      ContainsPanel(preset.default_visible_panels, LayoutPresets::Panels::kOverworldCanvas));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldTile16Selector));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldTile16Editor));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldMapProperties));

  auto canvas_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kOverworldCanvas);
  ASSERT_NE(canvas_pos, preset.panel_positions.end());
  EXPECT_EQ(canvas_pos->second, DockPosition::Center);

  auto selector_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kOverworldTile16Selector);
  ASSERT_NE(selector_pos, preset.panel_positions.end());
  EXPECT_EQ(selector_pos->second, DockPosition::LeftTop);

  auto editor_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kOverworldTile16Editor);
  ASSERT_NE(editor_pos, preset.panel_positions.end());
  EXPECT_EQ(editor_pos->second, DockPosition::RightTop);

  auto properties_pos = preset.panel_positions.find(
      LayoutPresets::Panels::kOverworldMapProperties);
  ASSERT_NE(properties_pos, preset.panel_positions.end());
  EXPECT_EQ(properties_pos->second, DockPosition::RightBottom);
}

TEST(LayoutPresetsTest, OverworldExpertPositionsMapPropertiesAndGraphicsEditor) {
  auto preset = LayoutPresets::GetOverworldExpertPreset();

  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldMapProperties));

  auto properties_pos = preset.panel_positions.find(
      LayoutPresets::Panels::kOverworldMapProperties);
  ASSERT_NE(properties_pos, preset.panel_positions.end());
  EXPECT_EQ(properties_pos->second, DockPosition::RightBottom);

  auto gfx_editor_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kGraphicsSheetEditor);
  ASSERT_NE(gfx_editor_pos, preset.panel_positions.end());
  EXPECT_EQ(gfx_editor_pos->second, DockPosition::LeftBottom);
}

}  // namespace
}  // namespace yaze::editor
