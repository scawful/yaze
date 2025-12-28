#include "app/editor/ui/editor_selection_dialog.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/color.h"
#include "imgui/imgui.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

namespace {

constexpr float kEditorSelectBaseFontSize = 16.0f;
constexpr float kEditorSelectCardBaseWidth = 180.0f;
constexpr float kEditorSelectCardBaseHeight = 120.0f;
constexpr float kEditorSelectCardWidthMaxFactor = 1.35f;
constexpr float kEditorSelectCardHeightMaxFactor = 1.35f;
constexpr float kEditorSelectRecentBaseWidth = 150.0f;
constexpr float kEditorSelectRecentBaseHeight = 35.0f;
constexpr float kEditorSelectRecentWidthMaxFactor = 1.3f;

struct GridLayout {
  int columns = 1;
  float item_width = 0.0f;
  float item_height = 0.0f;
  float spacing = 0.0f;
  float row_start_x = 0.0f;
};

float GetEditorSelectScale() {
  const float font_size = ImGui::GetFontSize();
  if (font_size <= 0.0f) {
    return 1.0f;
  }
  return font_size / kEditorSelectBaseFontSize;
}

GridLayout ComputeGridLayout(float avail_width, float min_width,
                             float max_width, float min_height,
                             float max_height, float preferred_width,
                             float aspect_ratio, float spacing) {
  GridLayout layout;
  layout.spacing = spacing;
  const auto width_for_columns = [avail_width, spacing](int columns) {
    return (avail_width - spacing * static_cast<float>(columns - 1)) /
           static_cast<float>(columns);
  };

  layout.columns =
      std::max(1, static_cast<int>((avail_width + spacing) /
                                   (preferred_width + spacing)));

  layout.item_width = width_for_columns(layout.columns);
  while (layout.columns > 1 && layout.item_width < min_width) {
    layout.columns -= 1;
    layout.item_width = width_for_columns(layout.columns);
  }

  layout.item_width = std::min(layout.item_width, max_width);
  layout.item_width = std::min(layout.item_width, avail_width);
  layout.item_height =
      std::clamp(layout.item_width * aspect_ratio, min_height, max_height);

  const float row_width =
      layout.item_width * static_cast<float>(layout.columns) +
      spacing * static_cast<float>(layout.columns - 1);
  layout.row_start_x = ImGui::GetCursorPosX();
  if (row_width < avail_width) {
    layout.row_start_x += (avail_width - row_width) * 0.5f;
  }

  return layout;
}

ImVec4 ScaleColor(const ImVec4& color, float scale, float alpha) {
  return ImVec4(color.x * scale, color.y * scale, color.z * scale, alpha);
}

ImVec4 ScaleColor(const ImVec4& color, float scale) {
  return ScaleColor(color, scale, color.w);
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
  color.w = alpha;
  return color;
}

ImVec4 GetEditorAccentColor(EditorType type, const gui::Theme& theme) {
  switch (type) {
    case EditorType::kOverworld:
      return gui::ConvertColorToImVec4(theme.success);
    case EditorType::kDungeon:
      return gui::ConvertColorToImVec4(theme.secondary);
    case EditorType::kGraphics:
      return gui::ConvertColorToImVec4(theme.warning);
    case EditorType::kSprite:
      return gui::ConvertColorToImVec4(theme.info);
    case EditorType::kMessage:
      return gui::ConvertColorToImVec4(theme.primary);
    case EditorType::kMusic:
      return gui::ConvertColorToImVec4(theme.accent);
    case EditorType::kPalette:
      return gui::ConvertColorToImVec4(theme.error);
    case EditorType::kScreen:
      return gui::ConvertColorToImVec4(theme.info);
    case EditorType::kAssembly:
      return gui::ConvertColorToImVec4(theme.text_secondary);
    case EditorType::kHex:
      return gui::ConvertColorToImVec4(theme.success);
    case EditorType::kEmulator:
      return gui::ConvertColorToImVec4(theme.info);
    case EditorType::kAgent:
      return gui::ConvertColorToImVec4(theme.accent);
    case EditorType::kSettings:
    case EditorType::kUnknown:
    default:
      return gui::ConvertColorToImVec4(theme.text_primary);
  }
}

}  // namespace

EditorSelectionDialog::EditorSelectionDialog() {
  // Use platform-aware shortcut strings (Cmd on macOS, Ctrl elsewhere)
  const char* ctrl = gui::GetCtrlDisplayName();
  editors_ = {
      {EditorType::kOverworld, "Overworld", ICON_MD_MAP,
       "Edit overworld maps, entrances, and properties",
       absl::StrFormat("%s+1", ctrl), false, true},

      {EditorType::kDungeon, "Dungeon", ICON_MD_CASTLE,
       "Design dungeon rooms, layouts, and mechanics",
       absl::StrFormat("%s+2", ctrl), false, true},

      {EditorType::kGraphics, "Graphics", ICON_MD_PALETTE,
       "Modify tiles, palettes, and graphics sets",
       absl::StrFormat("%s+3", ctrl), false, true},

      {EditorType::kSprite, "Sprites", ICON_MD_EMOJI_EMOTIONS,
       "Edit sprite graphics and properties",
       absl::StrFormat("%s+4", ctrl), false, true},

      {EditorType::kMessage, "Messages", ICON_MD_CHAT_BUBBLE,
       "Edit dialogue, signs, and text",
       absl::StrFormat("%s+5", ctrl), false, true},

      {EditorType::kMusic, "Music", ICON_MD_MUSIC_NOTE,
       "Configure music and sound effects",
       absl::StrFormat("%s+6", ctrl), false, true},

      {EditorType::kPalette, "Palettes", ICON_MD_COLOR_LENS,
       "Edit color palettes and animations",
       absl::StrFormat("%s+7", ctrl), false, true},

      {EditorType::kScreen, "Screens", ICON_MD_TV,
       "Edit title screen and ending screens",
       absl::StrFormat("%s+8", ctrl), false, true},

      {EditorType::kAssembly, "Assembly", ICON_MD_CODE,
       "Write and edit assembly code",
       absl::StrFormat("%s+9", ctrl), false, false},

      {EditorType::kHex, "Hex Editor", ICON_MD_DATA_ARRAY,
       "Direct ROM memory editing and comparison",
       absl::StrFormat("%s+0", ctrl), false, true},

      {EditorType::kEmulator, "Emulator", ICON_MD_VIDEOGAME_ASSET,
       "Test and debug your ROM in real-time with live debugging",
       absl::StrFormat("%s+Shift+E", ctrl), false, true},

      {EditorType::kAgent, "AI Agent", ICON_MD_SMART_TOY,
       "Configure AI agent, collaboration, and automation",
       absl::StrFormat("%s+Shift+A", ctrl), false, false},
  };

  LoadRecentEditors();
}

bool EditorSelectionDialog::Show(bool* p_open) {
  // Sync internal state with external flag
  if (p_open && *p_open && !is_open_) {
    is_open_ = true;
  }

  if (!is_open_) {
    if (p_open)
      *p_open = false;
    return false;
  }

  bool editor_selected = false;
  bool* window_open = p_open ? p_open : &is_open_;

  // Set window properties immediately before Begin to prevent them from
  // affecting tooltips
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(950, 650),
      ImGuiCond_Appearing);  // Slightly larger for better layout

  if (ImGui::Begin("Editor Selection", window_open,
                   ImGuiWindowFlags_NoCollapse)) {
    DrawWelcomeHeader();

    ImGui::Separator();
    ImGui::Spacing();

    // Quick access buttons for recently used
    if (!recent_editors_.empty()) {
      DrawQuickAccessButtons();
      ImGui::Separator();
      ImGui::Spacing();
    }

    // Main editor grid
    ImGui::Text(ICON_MD_APPS " All Editors");
    ImGui::Spacing();

    const float scale = GetEditorSelectScale();
    const float min_width = kEditorSelectCardBaseWidth * scale;
    const float max_width =
        kEditorSelectCardBaseWidth * kEditorSelectCardWidthMaxFactor * scale;
    const float min_height = kEditorSelectCardBaseHeight * scale;
    const float max_height =
        kEditorSelectCardBaseHeight * kEditorSelectCardHeightMaxFactor * scale;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float aspect_ratio = min_height / std::max(min_width, 1.0f);
    GridLayout layout = ComputeGridLayout(ImGui::GetContentRegionAvail().x,
                                          min_width, max_width, min_height,
                                          max_height, min_width, aspect_ratio,
                                          spacing);

    int column = 0;
    for (size_t i = 0; i < editors_.size(); ++i) {
      if (column == 0) {
        ImGui::SetCursorPosX(layout.row_start_x);
      }

      EditorType prev_selection = selected_editor_;
      DrawEditorPanel(editors_[i], static_cast<int>(i),
                      ImVec2(layout.item_width, layout.item_height));

      // Check if an editor was just selected
      if (selected_editor_ != prev_selection) {
        editor_selected = true;
        MarkRecentlyUsed(selected_editor_);
        if (selection_callback_) {
          selection_callback_(selected_editor_);
        }
        // Auto-dismiss after selection
        is_open_ = false;
        if (p_open) {
          *p_open = false;
        }
      }

      column += 1;
      if (column < layout.columns) {
        ImGui::SameLine(0.0f, layout.spacing);
      } else {
        column = 0;
        ImGui::Spacing();
      }
    }

    if (column != 0) {
      ImGui::NewLine();
    }
  }
  ImGui::End();

  // Sync state back
  if (p_open && !(*p_open)) {
    is_open_ = false;
  }

  // DO NOT auto-dismiss here. Let the callback/EditorManager handle it.
  // This allows the dialog to be used as a persistent switcher if desired.

  return editor_selected;
}

void EditorSelectionDialog::DrawWelcomeHeader() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  const ImVec4 text_secondary = gui::ConvertColorToImVec4(theme.text_secondary);

  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Large font

  ImGui::TextColored(accent, ICON_MD_EDIT " Select an Editor");

  ImGui::PopFont();

  ImGui::TextColored(text_secondary,
                     "Choose an editor to begin working on your ROM. "
                     "You can open multiple editors simultaneously.");
}

void EditorSelectionDialog::DrawQuickAccessButtons() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  ImGui::TextColored(accent, ICON_MD_HISTORY " Recently Used");
  ImGui::Spacing();

  const float scale = GetEditorSelectScale();
  const float min_width = kEditorSelectRecentBaseWidth * scale;
  const float max_width =
      kEditorSelectRecentBaseWidth * kEditorSelectRecentWidthMaxFactor * scale;
  const float height = kEditorSelectRecentBaseHeight * scale;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  GridLayout layout = ComputeGridLayout(
      ImGui::GetContentRegionAvail().x, min_width, max_width, height, height,
      min_width, height / std::max(min_width, 1.0f), spacing);

  int column = 0;
  for (EditorType type : recent_editors_) {
    // Find editor info
    auto it = std::find_if(
        editors_.begin(), editors_.end(),
        [type](const EditorInfo& info) { return info.type == type; });

    if (it != editors_.end()) {
      if (column == 0) {
        ImGui::SetCursorPosX(layout.row_start_x);
      }

      const ImVec4 base_color = GetEditorAccentColor(it->type, theme);
      ImGui::PushStyleColor(
          ImGuiCol_Button,
          ScaleColor(base_color, 0.5f, 0.7f));
      ImGui::PushStyleColor(
          ImGuiCol_ButtonHovered,
          ScaleColor(base_color, 0.7f, 0.9f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(base_color, 1.0f));

      if (ImGui::Button(absl::StrCat(it->icon, " ", it->name).c_str(),
                        ImVec2(layout.item_width, layout.item_height))) {
        selected_editor_ = type;
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", it->description);
      }

      column += 1;
      if (column < layout.columns) {
        ImGui::SameLine(0.0f, layout.spacing);
      } else {
        column = 0;
        ImGui::Spacing();
      }
    }
  }

  if (column != 0) {
    ImGui::NewLine();
  }
}

void EditorSelectionDialog::DrawEditorPanel(const EditorInfo& info, int index,
                                            const ImVec2& card_size) {
  ImGui::PushID(index);

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 base_color = GetEditorAccentColor(info.type, theme);
  const ImVec4 text_primary = gui::ConvertColorToImVec4(theme.text_primary);
  const ImVec4 text_secondary = gui::ConvertColorToImVec4(theme.text_secondary);
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);

  const ImGuiStyle& style = ImGui::GetStyle();
  const float line_height = ImGui::GetTextLineHeight();
  const float padding_x = std::max(style.FramePadding.x, card_size.x * 0.06f);
  const float padding_y = std::max(style.FramePadding.y, card_size.y * 0.08f);

  const float footer_height = info.shortcut.empty() ? 0.0f : line_height;
  const float footer_spacing = info.shortcut.empty() ? 0.0f : style.ItemSpacing.y;
  const float available_icon_height =
      card_size.y - padding_y * 2.0f - line_height - footer_height - footer_spacing;
  const float min_icon_radius = line_height * 0.9f;
  float max_icon_radius = card_size.y * 0.24f;
  max_icon_radius = std::max(max_icon_radius, min_icon_radius);
  const float icon_radius =
      std::clamp(available_icon_height * 0.5f, min_icon_radius, max_icon_radius);

  const ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 icon_center(cursor_pos.x + card_size.x * 0.5f,
                           cursor_pos.y + padding_y + icon_radius);
  float title_y = icon_center.y + icon_radius + style.ItemSpacing.y;
  const float footer_y = cursor_pos.y + card_size.y - padding_y - footer_height;
  if (title_y + line_height > footer_y - style.ItemSpacing.y) {
    title_y = footer_y - line_height - style.ItemSpacing.y;
  }

  // Panel styling with gradients
  bool is_recent = std::find(recent_editors_.begin(), recent_editors_.end(),
                             info.type) != recent_editors_.end();

  // Create gradient background
  ImU32 color_top = ImGui::GetColorU32(ScaleColor(base_color, 0.4f, 0.85f));
  ImU32 color_bottom =
      ImGui::GetColorU32(ScaleColor(base_color, 0.2f, 0.9f));

  // Draw gradient card background
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
      color_top, color_top, color_bottom, color_bottom);

  // Colored border
  ImU32 border_color =
      is_recent
          ? ImGui::GetColorU32(
                WithAlpha(base_color, 1.0f))
          : ImGui::GetColorU32(ScaleColor(base_color, 0.6f, 0.7f));
  const float rounding = std::max(style.FrameRounding, card_size.y * 0.05f);
  const float border_thickness =
      is_recent ? std::max(2.0f, style.FrameBorderSize + 1.0f)
                : std::max(1.0f, style.FrameBorderSize);
  draw_list->AddRect(
      cursor_pos,
      ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
      border_color, rounding, 0, border_thickness);

  // Recent indicator badge
  if (is_recent) {
    const float badge_radius =
        std::clamp(line_height * 0.6f, line_height * 0.4f, line_height);
    ImVec2 badge_pos(cursor_pos.x + card_size.x - padding_x - badge_radius,
                     cursor_pos.y + padding_y + badge_radius);
    draw_list->AddCircleFilled(badge_pos, badge_radius,
                               ImGui::GetColorU32(base_color), 16);
    ImVec2 star_size = ImGui::CalcTextSize(ICON_MD_STAR);
    ImGui::SetCursorScreenPos(
        ImVec2(badge_pos.x - star_size.x * 0.5f,
               badge_pos.y - star_size.y * 0.5f));
    ImGui::TextColored(text_primary, ICON_MD_STAR);
  }

  // Make button transparent (we draw our own background)
  ImVec4 button_bg = ImGui::GetStyleColorVec4(ImGuiCol_Button);
  button_bg.w = 0.0f;
  ImGui::PushStyleColor(ImGuiCol_Button, button_bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ScaleColor(base_color, 0.3f, 0.5f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ScaleColor(base_color, 0.5f, 0.7f));

  ImGui::SetCursorScreenPos(cursor_pos);
  bool clicked =
      ImGui::Button(absl::StrCat("##", info.name).c_str(), card_size);
  bool is_hovered = ImGui::IsItemHovered();
  const ImVec2 after_button = ImGui::GetCursorScreenPos();

  ImGui::PopStyleColor(3);

  // Draw icon with colored background circle
  ImU32 icon_bg = ImGui::GetColorU32(base_color);
  draw_list->AddCircleFilled(icon_center, icon_radius, icon_bg, 32);

  // Draw icon
  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);  // Larger font for icon
  ImVec2 icon_size = ImGui::CalcTextSize(info.icon);
  ImGui::SetCursorScreenPos(
      ImVec2(icon_center.x - icon_size.x / 2, icon_center.y - icon_size.y / 2));
  ImGui::TextColored(text_primary, "%s", info.icon);
  ImGui::PopFont();

  // Draw name
  const float name_wrap_width = card_size.x - padding_x * 2.0f;
  ImGui::PushTextWrapPos(cursor_pos.x + card_size.x - padding_x);
  ImVec2 name_size =
      ImGui::CalcTextSize(info.name, nullptr, false, name_wrap_width);
  ImGui::SetCursorScreenPos(ImVec2(
      cursor_pos.x + (card_size.x - name_size.x) / 2.0f, title_y));
  ImGui::TextColored(base_color, "%s", info.name);
  ImGui::PopTextWrapPos();

  // Draw shortcut hint if available
  if (!info.shortcut.empty()) {
    ImGui::SetCursorScreenPos(
        ImVec2(cursor_pos.x + padding_x, footer_y));
    ImGui::TextColored(text_secondary, "%s", info.shortcut.c_str());
  }

  // Hover glow effect
  if (is_hovered) {
    ImU32 glow_color =
        ImGui::GetColorU32(ScaleColor(base_color, 1.0f, 0.18f));
    draw_list->AddRectFilled(
        cursor_pos,
        ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
        glow_color, rounding);
  }

  // Enhanced tooltip with fixed sizing
  if (is_hovered) {
    const float tooltip_width = std::clamp(card_size.x * 1.4f, 240.0f, 340.0f);
    ImGui::SetNextWindowSize(ImVec2(tooltip_width, 0), ImGuiCond_Always);
    ImGui::BeginTooltip();
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);  // Medium font
    ImGui::TextColored(base_color, "%s %s", info.icon, info.name);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + tooltip_width - 20.0f);
    ImGui::TextWrapped("%s", info.description);
    ImGui::PopTextWrapPos();
    if (!info.shortcut.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(base_color, ICON_MD_KEYBOARD " %s", info.shortcut.c_str());
    }
    if (is_recent) {
      ImGui::Spacing();
      ImGui::TextColored(accent, ICON_MD_STAR " Recently used");
    }
    ImGui::EndTooltip();
  }

  if (clicked) {
    selected_editor_ = info.type;
  }

  ImGui::SetCursorScreenPos(after_button);
  ImGui::PopID();
}

void EditorSelectionDialog::MarkRecentlyUsed(EditorType type) {
  // Remove if already in list
  auto it = std::find(recent_editors_.begin(), recent_editors_.end(), type);
  if (it != recent_editors_.end()) {
    recent_editors_.erase(it);
  }

  // Add to front
  recent_editors_.insert(recent_editors_.begin(), type);

  // Limit size
  if (recent_editors_.size() > kMaxRecentEditors) {
    recent_editors_.resize(kMaxRecentEditors);
  }

  SaveRecentEditors();
}

void EditorSelectionDialog::LoadRecentEditors() {
  try {
    auto data = util::LoadFileFromConfigDir("recent_editors.txt");
    if (!data.empty()) {
      std::istringstream ss(data);
      std::string line;
      while (std::getline(ss, line) &&
             recent_editors_.size() < kMaxRecentEditors) {
        int type_int = std::stoi(line);
        if (type_int >= 0 &&
            type_int < static_cast<int>(EditorType::kSettings)) {
          recent_editors_.push_back(static_cast<EditorType>(type_int));
        }
      }
    }
  } catch (...) {
    // Ignore errors, just start with empty recent list
  }
}

void EditorSelectionDialog::SaveRecentEditors() {
  try {
    std::ostringstream ss;
    for (EditorType type : recent_editors_) {
      ss << static_cast<int>(type) << "\n";
    }
    util::SaveFile("recent_editors.txt", ss.str());
  } catch (...) {
    // Ignore save errors
  }
}

}  // namespace editor
}  // namespace yaze
