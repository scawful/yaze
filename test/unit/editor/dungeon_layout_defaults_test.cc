#include "app/editor/layout/layout_presets.h"

#include <algorithm>
#include <vector>

#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "core/features.h"
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

TEST(DungeonLayoutDefaultsTest,
     StandaloneWorkflowDefaultsToNavigationWithoutToolClutter) {
  auto& use_workbench = core::FeatureFlags::get().dungeon.kUseWorkbench;
  const bool previous = use_workbench;
  use_workbench = false;
  const auto preset = LayoutPresets::GetDefaultPreset(EditorType::kDungeon);
  use_workbench = previous;

  ASSERT_EQ(preset.default_visible_panels.size(), 2U);
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonRoomMatrix));
  EXPECT_TRUE(ContainsPanel(preset.default_visible_panels,
                            LayoutPresets::Panels::kDungeonEntrances));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomSelector));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonObjectSelector));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonPaletteEditor));
  EXPECT_FALSE(ContainsPanel(preset.default_visible_panels,
                             LayoutPresets::Panels::kDungeonRoomGraphics));
}

TEST(DungeonLayoutDefaultsTest, ContextSnapshotExposesStableDungeonActions) {
  DungeonEditorV2 editor;
  const EditorContextSnapshot snapshot = editor.BuildContextSnapshot();
  EXPECT_EQ(snapshot.category, "Dungeon");
  EXPECT_NE(snapshot.title.find("Room 0x000"), std::string::npos);
  EXPECT_FALSE(snapshot.diagnostics.empty());
  EXPECT_TRUE(std::any_of(snapshot.actions.begin(), snapshot.actions.end(),
                          [](const EditorContextAction& action) {
                            return action.target == "dungeon.room_matrix";
                          }));
}

}  // namespace
}  // namespace yaze::editor
