// Pins which theme token each overworld entity marker box uses. The markers
// used to read the status colors through the theme defaults (exits red,
// sprites blue); each type now has its own token, and this test keeps the
// renderer from pointing a type at the wrong one.
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/overworld/entity/overworld_entity_renderer.h"
#include "app/gui/core/theme_manager.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "zelda3/common.h"

namespace yaze::editor {
namespace {

using EntityType = zelda3::GameEntity::EntityType;

void ExpectSameVec4(const ImVec4& actual, const ImVec4& expected,
                    const char* what) {
  EXPECT_FLOAT_EQ(actual.x, expected.x) << what;
  EXPECT_FLOAT_EQ(actual.y, expected.y) << what;
  EXPECT_FLOAT_EQ(actual.z, expected.z) << what;
  EXPECT_FLOAT_EQ(actual.w, expected.w) << what;
}

TEST(OverworldEntityMarkerColorTest, EachTypeReadsItsOwnToken) {
  AgentUITheme theme;
  theme.entrance_color = ImVec4(0.1f, 0.0f, 0.0f, 1.0f);
  theme.hole_color = ImVec4(0.2f, 0.0f, 0.0f, 1.0f);
  theme.exit_color = ImVec4(0.3f, 0.0f, 0.0f, 1.0f);
  theme.item_color = ImVec4(0.4f, 0.0f, 0.0f, 1.0f);
  theme.sprite_color = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);
  theme.status_error = ImVec4(0.9f, 0.0f, 0.0f, 1.0f);
  theme.status_active = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);

  ExpectSameVec4(EntityMarkerColor(theme, EntityType::kEntrance),
                 theme.entrance_color, "entrance");
  ExpectSameVec4(EntityMarkerColor(theme, EntityType::kEntrance, true),
                 theme.hole_color, "hole");
  ExpectSameVec4(EntityMarkerColor(theme, EntityType::kExit), theme.exit_color,
                 "exit");
  ExpectSameVec4(EntityMarkerColor(theme, EntityType::kItem), theme.item_color,
                 "item");
  ExpectSameVec4(EntityMarkerColor(theme, EntityType::kSprite),
                 theme.sprite_color, "sprite");
}

// End to end under Classic YAZE: theme -> AgentUITheme -> marker color ->
// the ImU32 the canvas draws. IM_COL32 keeps red in the low byte.
TEST(OverworldEntityMarkerColorTest, ClassicYazeMarkersPackToDocumentedRgb) {
  ImGuiContext* context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);
  gui::ThemeManager::Get().ApplyClassicYazeTheme();
  AgentUI::RefreshTheme();
  const AgentUITheme& theme = AgentUI::GetTheme();

  struct Case {
    EntityType type;
    bool is_hole;
    ImU32 expected;
    const char* name;
  };
  const Case cases[] = {
      {EntityType::kEntrance, false, IM_COL32(255, 204, 0, 255), "entrance"},
      {EntityType::kEntrance, true, IM_COL32(255, 150, 0, 255), "hole"},
      {EntityType::kExit, false, IM_COL32(150, 235, 255, 255), "exit"},
      {EntityType::kItem, false, IM_COL32(235, 45, 45, 255), "item"},
      {EntityType::kSprite, false, IM_COL32(235, 60, 235, 255), "sprite"},
  };
  for (const auto& c : cases) {
    const ImVec4 color = EntityMarkerColor(theme, c.type, c.is_hole);
    // Same packing as CanvasUtils::DrawCanvasRect.
    const ImU32 packed =
        IM_COL32(color.x * 255, color.y * 255, color.z * 255, color.w * 255);
    const ImU32 rounded = ImGui::ColorConvertFloat4ToU32(color);
    EXPECT_EQ(rounded, c.expected) << c.name;
    EXPECT_NEAR(static_cast<int>(packed & 0xFF),
                static_cast<int>(c.expected & 0xFF), 1)
        << c.name << " red channel";
    EXPECT_NEAR(static_cast<int>((packed >> 16) & 0xFF),
                static_cast<int>((c.expected >> 16) & 0xFF), 1)
        << c.name << " blue channel";
  }

  ImGui::DestroyContext(context);
}

}  // namespace
}  // namespace yaze::editor
