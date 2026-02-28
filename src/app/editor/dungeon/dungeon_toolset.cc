#include "dungeon_toolset.h"

#include <algorithm>
#include <array>

#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze::editor {

namespace {

struct ToolDef {
  const char* icon;
  const char* tooltip;
  const char* shortcut;
  DungeonToolset::PlacementType type;
};

constexpr std::array<ToolDef, 7> kToolDefs = {{
    {ICON_MD_NEAR_ME, "Select", "Esc", DungeonToolset::kNoType},
    {ICON_MD_WIDGETS, "Objects", "O", DungeonToolset::kObject},
    {ICON_MD_PEST_CONTROL, "Sprites", "S", DungeonToolset::kSprite},
    {ICON_MD_GRASS, "Items", "I", DungeonToolset::kItem},
    {ICON_MD_SENSOR_DOOR, "Doors", "D", DungeonToolset::kDoor},
    {ICON_MD_INVENTORY, "Chests", "C", DungeonToolset::kChest},
    {ICON_MD_NAVIGATION, "Entrances", "E", DungeonToolset::kEntrance},
}};

}  // namespace

void DungeonToolset::Draw() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Undo / Redo buttons
  if (gui::ThemedIconButton(ICON_MD_UNDO, "Undo (Ctrl+Z)")) {
    if (undo_callback_)
      undo_callback_();
  }
  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_REDO, "Redo (Ctrl+Y)")) {
    if (redo_callback_)
      redo_callback_();
  }

  ImGui::SameLine(0, 12.0f);
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, 12.0f);

  // Background layer radio buttons (compact)
  auto bg_radio = [&](const char* label, BackgroundType type, const char* tip) {
    if (ImGui::RadioButton(label, background_type_ == type)) {
      background_type_ = type;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tip);
    }
    ImGui::SameLine();
  };

  bg_radio("All", kBackgroundAny, "Show all layers");
  bg_radio("BG1", kBackground1, "Background 1 only");
  bg_radio("BG2", kBackground2, "Background 2 only");
  bg_radio("BG3", kBackground3, "Background 3 only");

  ImGui::SameLine(0, 12.0f);
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, 12.0f);

  // Placement mode tool strip — themed icon buttons with active highlighting
  for (const auto& tool : kToolDefs) {
    const bool is_active = (placement_type_ == tool.type);

    if (gui::ThemedIconButton(tool.icon, nullptr, ImVec2(0, 0), is_active)) {
      placement_type_ = tool.type;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s (%s)", tool.tooltip, tool.shortcut);
    }
    ImGui::SameLine();
  }

  ImGui::SameLine(0, 12.0f);
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, 12.0f);

  // Palette toggle
  if (gui::ThemedIconButton(ICON_MD_PALETTE, "Palette Editor")) {
    if (palette_toggle_callback_)
      palette_toggle_callback_();
  }

  ImGui::NewLine();
}

const char* DungeonToolset::GetToolModeName() const {
  for (const auto& tool : kToolDefs) {
    if (tool.type == placement_type_) {
      return tool.tooltip;
    }
  }
  return "Select";
}

}  // namespace yaze::editor
