#include "app/editor/ui/dashboard_panel.h"

#include <algorithm>
#include <cfloat>
#include <fstream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/file_util.h"

namespace yaze {
namespace editor {

namespace {

constexpr float kDashboardCardBaseWidth = 180.0f;
constexpr float kDashboardCardBaseHeight = 120.0f;
constexpr float kDashboardCardWidthMaxFactor = 1.35f;
constexpr float kDashboardCardHeightMaxFactor = 1.35f;
constexpr float kDashboardCardMinWidthFactor = 0.9f;
constexpr float kDashboardCardMinHeightFactor = 0.9f;
constexpr int kDashboardMaxColumns = 5;
constexpr float kDashboardRecentBaseWidth = 150.0f;
constexpr float kDashboardRecentBaseHeight = 35.0f;
constexpr float kDashboardRecentWidthMaxFactor = 1.3f;
constexpr int kDashboardMaxRecentColumns = 4;

struct FlowLayout {
  int columns = 1;
  float item_width = 0.0f;
  float item_height = 0.0f;
  float spacing = 0.0f;
};

FlowLayout ComputeFlowLayout(float avail_width, float min_width,
                             float max_width, float min_height,
                             float max_height, float aspect_ratio,
                             float spacing, int max_columns, int item_count) {
  FlowLayout layout;
  layout.spacing = spacing;
  if (avail_width <= 0.0f) {
    layout.columns = 1;
    layout.item_width = min_width;
    layout.item_height = min_height;
    return layout;
  }

  const int clamped_max =
      std::max(1, max_columns > 0 ? max_columns : item_count);
  const auto width_for_columns = [avail_width, spacing](int columns) {
    return (avail_width - spacing * static_cast<float>(columns - 1)) /
           static_cast<float>(columns);
  };

  int columns = std::max(
      1, static_cast<int>((avail_width + spacing) / (min_width + spacing)));
  columns = std::min(columns, clamped_max);
  if (item_count > 0) {
    columns = std::min(columns, item_count);
  }
  columns = std::max(columns, 1);

  float width = width_for_columns(columns);
  while (columns < clamped_max && width > max_width) {
    columns += 1;
    width = width_for_columns(columns);
  }

  const float clamped_max_width = std::min(max_width, avail_width);
  const float clamped_min_width = std::min(min_width, clamped_max_width);
  layout.item_width = std::clamp(width, clamped_min_width, clamped_max_width);
  layout.item_height =
      std::clamp(layout.item_width * aspect_ratio, min_height, max_height);

  layout.columns = columns;

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

DashboardPanel::DashboardPanel(EditorManager* editor_manager)
    : editor_manager_(editor_manager), window_("Dashboard", ICON_MD_DASHBOARD) {
  window_.SetSaveSettings(false);
  window_.SetDefaultSize(950, 650);
  window_.SetPosition(gui::PanelWindow::Position::Center);

  // Use platform-aware shortcut strings (Cmd on macOS, Ctrl elsewhere)
  const char* ctrl = gui::GetCtrlDisplayName();
  editors_ = {
      {"Overworld", ICON_MD_MAP,
       "Edit overworld maps, entrances, and properties",
       absl::StrFormat("%s+1", ctrl), EditorType::kOverworld},
      {"Dungeon", ICON_MD_CASTLE,
       "Design dungeon rooms, layouts, and mechanics",
       absl::StrFormat("%s+2", ctrl), EditorType::kDungeon},
      {"Graphics", ICON_MD_PALETTE, "Modify tiles, palettes, and graphics sets",
       absl::StrFormat("%s+3", ctrl), EditorType::kGraphics},
      {"Sprites", ICON_MD_EMOJI_EMOTIONS, "Edit sprite graphics and properties",
       absl::StrFormat("%s+4", ctrl), EditorType::kSprite},
      {"Messages", ICON_MD_CHAT_BUBBLE, "Edit dialogue, signs, and text",
       absl::StrFormat("%s+5", ctrl), EditorType::kMessage},
      {"Music", ICON_MD_MUSIC_NOTE, "Configure music and sound effects",
       absl::StrFormat("%s+6", ctrl), EditorType::kMusic},
      {"Palettes", ICON_MD_COLOR_LENS, "Edit color palettes and animations",
       absl::StrFormat("%s+7", ctrl), EditorType::kPalette},
      {"Screens", ICON_MD_TV, "Edit title screen and ending screens",
       absl::StrFormat("%s+8", ctrl), EditorType::kScreen},
      {"Assembly", ICON_MD_CODE, "Write and edit assembly code",
       absl::StrFormat("%s+9", ctrl), EditorType::kAssembly},
      {"Hex Editor", ICON_MD_DATA_ARRAY,
       "Direct ROM memory editing and comparison",
       absl::StrFormat("%s+0", ctrl), EditorType::kHex},
      {"Emulator", ICON_MD_VIDEOGAME_ASSET,
       "Test and debug your ROM in real-time",
       absl::StrFormat("%s+Shift+E", ctrl), EditorType::kEmulator},
      {"AI Agent", ICON_MD_SMART_TOY,
       "Configure AI agent, collaboration, and automation",
       absl::StrFormat("%s+Shift+A", ctrl), EditorType::kAgent, false, false},
  };

  LoadRecentEditors();
}

void DashboardPanel::Draw() {
  if (!show_)
    return;

  // Set window properties immediately before Begin
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 center = viewport->GetCenter();
  ImVec2 view_size = viewport->WorkSize;
  float target_width = std::clamp(view_size.x * 0.9f, 520.0f, 1000.0f);
  float target_height = std::clamp(view_size.y * 0.88f, 420.0f, 780.0f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(target_width, target_height),
                           ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(
      ImVec2(420.0f, 360.0f),
      ImVec2(view_size.x * 0.98f, view_size.y * 0.95f));

  has_rom_ = editor_manager_ && editor_manager_->GetCurrentRom() &&
             editor_manager_->GetCurrentRom()->is_loaded();

  if (window_.Begin(&show_)) {
    DrawWelcomeHeader();
    ImGui::Separator();
    ImGui::Spacing();

    if (!has_rom_) {
      const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
      ImVec4 info_color = gui::ConvertColorToImVec4(theme.text_secondary);
      ImGui::TextColored(info_color,
                         ICON_MD_INFO " Load a ROM to enable editors.");
      ImGui::Spacing();
    }

    DrawRecentEditors();
    if (!recent_editors_.empty()) {
      ImGui::Separator();
      ImGui::Spacing();
    }
    DrawEditorGrid();
  }
  window_.End();
}

void DashboardPanel::DrawWelcomeHeader() {
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

void DashboardPanel::DrawRecentEditors() {
  if (recent_editors_.empty())
    return;

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  ImGui::TextColored(accent, ICON_MD_HISTORY " Recently Used");
  ImGui::Spacing();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float avail_width = ImGui::GetContentRegionAvail().x;
  const float min_width = kDashboardRecentBaseWidth;
  const float max_width =
      kDashboardRecentBaseWidth * kDashboardRecentWidthMaxFactor;
  const float height =
      std::max(kDashboardRecentBaseHeight, ImGui::GetFrameHeight());
  const float spacing = style.ItemSpacing.x;
  const bool stack_items = avail_width < min_width * 1.6f;
  FlowLayout row_layout{};
  if (stack_items) {
    row_layout.columns = 1;
    row_layout.item_width = avail_width;
    row_layout.item_height = height;
    row_layout.spacing = spacing;
  } else {
    row_layout = ComputeFlowLayout(avail_width, min_width, max_width, height,
                                   height, height / std::max(min_width, 1.0f),
                                   spacing, kDashboardMaxRecentColumns,
                                   static_cast<int>(recent_editors_.size()));
  }

  ImGuiTableFlags table_flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;
  const ImVec2 cell_padding(row_layout.spacing * 0.5f,
                            style.ItemSpacing.y * 0.4f);
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
  if (ImGui::BeginTable("DashboardRecentGrid", row_layout.columns,
                        table_flags)) {
    for (EditorType type : recent_editors_) {
      // Find editor info
      auto it = std::find_if(
          editors_.begin(), editors_.end(),
          [type](const EditorInfo& info) { return info.type == type; });

      if (it == editors_.end()) {
        continue;
      }

      ImGui::TableNextColumn();

      const bool enabled = has_rom_ || !it->requires_rom;
      const float alpha = enabled ? 1.0f : 0.35f;
      const ImVec4 base_color = GetEditorAccentColor(it->type, theme);
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ScaleColor(base_color, 0.5f, 0.7f * alpha));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ScaleColor(base_color, 0.7f, 0.9f * alpha));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            WithAlpha(base_color, 1.0f * alpha));

      ImVec2 button_size(
          stack_items ? avail_width : row_layout.item_width,
          row_layout.item_height > 0.0f ? row_layout.item_height : height);
      if (!enabled) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(absl::StrCat(it->icon, " ", it->name).c_str(),
                        button_size)) {
        if (enabled && editor_manager_) {
          MarkRecentlyUsed(type);
          editor_manager_->SwitchToEditor(type);
          show_ = false;
        }
      }
      if (!enabled) {
        ImGui::EndDisabled();
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        if (!enabled) {
          ImGui::SetTooltip("Load a ROM to open %s", it->name.c_str());
        } else {
          ImGui::SetTooltip("%s", it->description.c_str());
        }
      }
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
}

void DashboardPanel::DrawEditorGrid() {
  ImGui::Text(ICON_MD_APPS " All Editors");
  ImGui::Spacing();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float avail_width = ImGui::GetContentRegionAvail().x;
  const float scale = ImGui::GetFontSize() / 16.0f;
  const float compact_scale = avail_width < 620.0f ? 0.85f : 1.0f;
  const float min_width =
      kDashboardCardBaseWidth * kDashboardCardMinWidthFactor * scale *
      compact_scale;
  const float max_width =
      kDashboardCardBaseWidth * kDashboardCardWidthMaxFactor * scale *
      compact_scale;
  const float min_height =
      std::max(kDashboardCardBaseHeight * kDashboardCardMinHeightFactor * scale *
                   compact_scale,
               ImGui::GetFrameHeight() * 3.2f);
  const float max_height =
      kDashboardCardBaseHeight * kDashboardCardHeightMaxFactor * scale *
      compact_scale;
  const float aspect_ratio =
      kDashboardCardBaseHeight / std::max(kDashboardCardBaseWidth, 1.0f);
  const float spacing = style.ItemSpacing.x;

  FlowLayout layout = ComputeFlowLayout(
      avail_width, min_width, max_width, min_height, max_height, aspect_ratio,
      spacing, kDashboardMaxColumns, static_cast<int>(editors_.size()));

  ImGuiTableFlags table_flags =
      ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX;
  const ImVec2 cell_padding(layout.spacing * 0.5f, style.ItemSpacing.y * 0.5f);
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
  if (ImGui::BeginTable("DashboardEditorGrid", layout.columns, table_flags)) {
    for (size_t i = 0; i < editors_.size(); ++i) {
      ImGui::TableNextColumn();
      const bool enabled = has_rom_ || !editors_[i].requires_rom;
      DrawEditorPanel(editors_[i], static_cast<int>(i),
                      ImVec2(layout.item_width, layout.item_height), enabled);
    }
    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
}

void DashboardPanel::DrawEditorPanel(const EditorInfo& info, int index,
                                     const ImVec2& card_size, bool enabled) {
  ImGui::PushID(index);

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const float disabled_alpha = enabled ? 1.0f : 0.35f;
  const ImVec4 base_color = GetEditorAccentColor(info.type, theme);
  ImVec4 text_primary = gui::ConvertColorToImVec4(theme.text_primary);
  ImVec4 text_secondary = gui::ConvertColorToImVec4(theme.text_secondary);
  const ImVec4 accent = gui::ConvertColorToImVec4(theme.accent);
  text_primary.w *= enabled ? 1.0f : 0.5f;
  text_secondary.w *= enabled ? 1.0f : 0.5f;
  ImFont* text_font = ImGui::GetFont();
  const float text_font_size = ImGui::GetFontSize();

  const ImGuiStyle& style = ImGui::GetStyle();
  const float line_height = ImGui::GetTextLineHeight();
  const float padding_x = std::max(style.FramePadding.x, card_size.x * 0.06f);
  const float padding_y = std::max(style.FramePadding.y, card_size.y * 0.08f);

  const float footer_height = info.shortcut.empty() ? 0.0f : line_height;
  const float footer_spacing =
      info.shortcut.empty() ? 0.0f : style.ItemSpacing.y;
  const float available_icon_height = card_size.y - padding_y * 2.0f -
                                      line_height - footer_height -
                                      footer_spacing;
  const float min_icon_radius = line_height * 0.9f;
  float max_icon_radius = card_size.y * 0.24f;
  max_icon_radius = std::max(max_icon_radius, min_icon_radius);
  const float icon_radius = std::clamp(available_icon_height * 0.5f,
                                       min_icon_radius, max_icon_radius);

  const ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 icon_center(cursor_pos.x + card_size.x * 0.5f,
                           cursor_pos.y + padding_y + icon_radius);
  float title_y = icon_center.y + icon_radius + style.ItemSpacing.y;
  const float footer_y = cursor_pos.y + card_size.y - padding_y - footer_height;
  if (title_y + line_height > footer_y - style.ItemSpacing.y) {
    title_y = footer_y - line_height - style.ItemSpacing.y;
  }

  bool is_recent = std::find(recent_editors_.begin(), recent_editors_.end(),
                             info.type) != recent_editors_.end();

  // Create gradient background
  ImU32 color_top =
      ImGui::GetColorU32(ScaleColor(base_color, 0.4f, 0.85f * disabled_alpha));
  ImU32 color_bottom =
      ImGui::GetColorU32(ScaleColor(base_color, 0.2f, 0.9f * disabled_alpha));

  // Draw gradient card background
  draw_list->AddRectFilledMultiColor(
      cursor_pos,
      ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y), color_top,
      color_top, color_bottom, color_bottom);

  // Colored border
  ImU32 border_color = is_recent
                           ? ImGui::GetColorU32(
                                 WithAlpha(base_color, 1.0f * disabled_alpha))
                           : ImGui::GetColorU32(
                                 ScaleColor(base_color, 0.6f,
                                            0.7f * disabled_alpha));
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
    const ImU32 star_color = ImGui::GetColorU32(text_primary);
    const ImVec2 star_size =
        text_font->CalcTextSizeA(text_font_size, FLT_MAX, 0.0f, ICON_MD_STAR);
    const ImVec2 star_pos(badge_pos.x - star_size.x * 0.5f,
                          badge_pos.y - star_size.y * 0.5f);
    draw_list->AddText(text_font, text_font_size, star_pos, star_color,
                       ICON_MD_STAR);
  }

  // Make button transparent (we draw our own background)
  ImVec4 button_bg = ImGui::GetStyleColorVec4(ImGuiCol_Button);
  button_bg.w = 0.0f;
  ImGui::PushStyleColor(ImGuiCol_Button, button_bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ScaleColor(base_color, 0.3f,
                                   enabled ? 0.5f : 0.2f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ScaleColor(base_color, 0.5f,
                                   enabled ? 0.7f : 0.2f));

  if (!enabled) {
    ImGui::BeginDisabled();
  }
  bool clicked =
      ImGui::Button(absl::StrCat("##", info.name).c_str(), card_size);
  if (!enabled) {
    ImGui::EndDisabled();
  }
  bool is_hovered = ImGui::IsItemHovered();

  ImGui::PopStyleColor(3);

  // Draw icon with colored background circle
  ImU32 icon_bg =
      ImGui::GetColorU32(WithAlpha(base_color, 1.0f * disabled_alpha));
  draw_list->AddCircleFilled(icon_center, icon_radius, icon_bg, 32);

  // Draw icon
  ImFont* icon_font = ImGui::GetFont();
  if (ImGui::GetIO().Fonts->Fonts.size() > 2 &&
      card_size.y >= kDashboardCardBaseHeight) {
    icon_font = ImGui::GetIO().Fonts->Fonts[2];
  } else if (ImGui::GetIO().Fonts->Fonts.size() > 1) {
    icon_font = ImGui::GetIO().Fonts->Fonts[1];
  }
  ImGui::PushFont(icon_font);
  const float icon_font_size = ImGui::GetFontSize();
  const ImVec2 icon_size = icon_font->CalcTextSizeA(icon_font_size, FLT_MAX,
                                                    0.0f, info.icon.c_str());
  ImGui::PopFont();
  const ImVec2 icon_text_pos(icon_center.x - icon_size.x * 0.5f,
                             icon_center.y - icon_size.y * 0.5f);
  draw_list->AddText(icon_font, icon_font_size, icon_text_pos,
                     ImGui::GetColorU32(text_primary), info.icon.c_str());

  // Draw name
  const ImVec2 name_size = text_font->CalcTextSizeA(text_font_size, FLT_MAX,
                                                    0.0f, info.name.c_str());
  float name_x = cursor_pos.x + (card_size.x - name_size.x) * 0.5f;
  const float name_min_x = cursor_pos.x + padding_x;
  const float name_max_x = cursor_pos.x + card_size.x - padding_x;
  name_x = std::clamp(name_x, name_min_x, name_max_x);
  const ImVec2 name_pos(name_x, title_y);
  const ImVec4 name_clip(name_min_x, cursor_pos.y + padding_y, name_max_x,
                         footer_y);
  draw_list->AddText(
      text_font, text_font_size, name_pos,
      ImGui::GetColorU32(WithAlpha(base_color, disabled_alpha)),
      info.name.c_str(), nullptr, 0.0f, &name_clip);

  // Draw shortcut hint if available
  if (!info.shortcut.empty()) {
    const ImVec2 shortcut_pos(cursor_pos.x + padding_x, footer_y);
    const ImVec4 shortcut_clip(cursor_pos.x + padding_x, footer_y,
                               cursor_pos.x + card_size.x - padding_x,
                               cursor_pos.y + card_size.y - padding_y);
    draw_list->AddText(text_font, text_font_size, shortcut_pos,
                       ImGui::GetColorU32(text_secondary),
                       info.shortcut.c_str(), nullptr, 0.0f, &shortcut_clip);
  }

  // Hover glow effect
  if (is_hovered) {
    ImU32 glow_color =
        ImGui::GetColorU32(ScaleColor(base_color, 1.0f,
                                      enabled ? 0.18f : 0.08f));
    draw_list->AddRectFilled(
        cursor_pos,
        ImVec2(cursor_pos.x + card_size.x, cursor_pos.y + card_size.y),
        glow_color, rounding);
  }

  // Enhanced tooltip
  if (is_hovered) {
    const float tooltip_width = std::clamp(card_size.x * 1.4f, 240.0f, 340.0f);
    ImGui::SetNextWindowSize(ImVec2(tooltip_width, 0), ImGuiCond_Always);
    ImGui::BeginTooltip();
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);  // Medium font
    ImGui::TextColored(WithAlpha(base_color, disabled_alpha), "%s %s",
                       info.icon.c_str(), info.name.c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + tooltip_width - 20.0f);
    if (!enabled) {
      ImGui::TextWrapped("Load a ROM to open this editor.");
    } else {
      ImGui::TextWrapped("%s", info.description.c_str());
    }
    ImGui::PopTextWrapPos();
    if (enabled && !info.shortcut.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(WithAlpha(base_color, disabled_alpha),
                         ICON_MD_KEYBOARD " %s", info.shortcut.c_str());
    }
    if (is_recent) {
      ImGui::Spacing();
      ImGui::TextColored(accent, ICON_MD_STAR " Recently used");
    }
    ImGui::EndTooltip();
  }

  if (clicked && enabled) {
    if (editor_manager_) {
      MarkRecentlyUsed(info.type);
      editor_manager_->SwitchToEditor(info.type);
      show_ = false;
    }
  }

  ImGui::PopID();
}

void DashboardPanel::MarkRecentlyUsed(EditorType type) {
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

void DashboardPanel::LoadRecentEditors() {
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
    // Ignore errors
  }
}

void DashboardPanel::SaveRecentEditors() {
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

void DashboardPanel::ClearRecentEditors() {
  recent_editors_.clear();
  SaveRecentEditors();
}

}  // namespace editor
}  // namespace yaze
