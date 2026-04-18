#include "app/editor/layout/layout_presets.h"

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

bool ContainsPanel(const std::vector<std::string>& panels, const char* panel_id) {
  return std::find(panels.begin(), panels.end(), panel_id) != panels.end();
}

TEST(LayoutPresetsTest, OverworldDefaultUsesMinimalWorkbenchArrangement) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kOverworld);

  EXPECT_TRUE(
      ContainsPanel(preset.default_visible_panels, LayoutPresets::Panels::kOverworldCanvas));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldTile16Selector));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kOverworldTile16Editor));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kOverworldMapProperties));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kOverworldItemList));
  EXPECT_TRUE(preset.dock_only_default_visible_panels);

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

TEST(LayoutPresetsTest, WorkspaceAliasesMirrorWindowTerminology) {
  WorkspaceLayoutPreset preset =
      LayoutPresets::GetDefaultWorkspacePreset(EditorType::kOverworld);

  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Windows::kOverworldCanvas));
  EXPECT_TRUE(LayoutPresets::IsDefaultWindow(
      EditorType::kOverworld, LayoutPresets::Windows::kOverworldCanvas));

  const auto all_windows =
      LayoutPresets::GetAllWindowsForEditor(EditorType::kOverworld);
  EXPECT_TRUE(
      ContainsPanel(all_windows, LayoutPresets::Windows::kOverworldMapProperties));
}

TEST(LayoutPresetsTest, AssemblyDefaultIncludesSupportingToolWindows) {
  auto preset = LayoutPresets::GetDefaultPreset(EditorType::kAssembly);

  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kAssemblyEditor));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kAssemblyFileBrowser));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kAssemblyDisassembly));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kAssemblySymbols));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kAssemblyBuildOutput));
  EXPECT_TRUE(ContainsPanel(preset.optional_panels,
                            LayoutPresets::Panels::kAssemblyToolbar));

  auto browser_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kAssemblyFileBrowser);
  ASSERT_NE(browser_pos, preset.panel_positions.end());
  EXPECT_EQ(browser_pos->second, DockPosition::LeftTop);

  auto symbols_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kAssemblySymbols);
  ASSERT_NE(symbols_pos, preset.panel_positions.end());
  EXPECT_EQ(symbols_pos->second, DockPosition::RightTop);

  auto disassembly_pos =
      preset.panel_positions.find(LayoutPresets::Panels::kAssemblyDisassembly);
  ASSERT_NE(disassembly_pos, preset.panel_positions.end());
  EXPECT_EQ(disassembly_pos->second, DockPosition::RightBottom);
}

}  // namespace
}  // namespace yaze::editor
