#include "theme_manager.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"  // For ColorsYaze function
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "util/file_util.h"
#include "util/log.h"
#include "util/platform_paths.h"

namespace yaze {
namespace gui {

// Helper function to create Color from RGB values
Color RGB(float r, float g, float b, float a = 1.0f) {
  return {r / 255.0f, g / 255.0f, b / 255.0f, a};
}

Color RGBA(int r, int g, int b, int a = 255) {
  return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
}

// Theme Implementation
void EnhancedTheme::ApplyToImGui() const {
  ImGuiStyle* style = &ImGui::GetStyle();
  ImVec4* colors = style->Colors;

  // Apply colors
  colors[ImGuiCol_Text] = ConvertColorToImVec4(text_primary);
  colors[ImGuiCol_TextDisabled] = ConvertColorToImVec4(text_disabled);
  colors[ImGuiCol_WindowBg] = ConvertColorToImVec4(window_bg);
  colors[ImGuiCol_ChildBg] = ConvertColorToImVec4(child_bg);
  colors[ImGuiCol_PopupBg] = ConvertColorToImVec4(popup_bg);
  colors[ImGuiCol_Border] = ConvertColorToImVec4(border);
  colors[ImGuiCol_BorderShadow] = ConvertColorToImVec4(border_shadow);
  colors[ImGuiCol_FrameBg] = ConvertColorToImVec4(frame_bg);
  colors[ImGuiCol_FrameBgHovered] = ConvertColorToImVec4(frame_bg_hovered);
  colors[ImGuiCol_FrameBgActive] = ConvertColorToImVec4(frame_bg_active);
  colors[ImGuiCol_TitleBg] = ConvertColorToImVec4(title_bg);
  colors[ImGuiCol_TitleBgActive] = ConvertColorToImVec4(title_bg_active);
  colors[ImGuiCol_TitleBgCollapsed] = ConvertColorToImVec4(title_bg_collapsed);
  colors[ImGuiCol_MenuBarBg] = ConvertColorToImVec4(menu_bar_bg);
  colors[ImGuiCol_ScrollbarBg] = ConvertColorToImVec4(scrollbar_bg);
  colors[ImGuiCol_ScrollbarGrab] = ConvertColorToImVec4(scrollbar_grab);
  colors[ImGuiCol_ScrollbarGrabHovered] =
      ConvertColorToImVec4(scrollbar_grab_hovered);
  colors[ImGuiCol_ScrollbarGrabActive] =
      ConvertColorToImVec4(scrollbar_grab_active);
  colors[ImGuiCol_Button] = ConvertColorToImVec4(button);
  colors[ImGuiCol_ButtonHovered] = ConvertColorToImVec4(button_hovered);
  colors[ImGuiCol_ButtonActive] = ConvertColorToImVec4(button_active);
  colors[ImGuiCol_Header] = ConvertColorToImVec4(header);
  colors[ImGuiCol_HeaderHovered] = ConvertColorToImVec4(header_hovered);
  colors[ImGuiCol_HeaderActive] = ConvertColorToImVec4(header_active);
  colors[ImGuiCol_Separator] = ConvertColorToImVec4(separator);
  colors[ImGuiCol_SeparatorHovered] = ConvertColorToImVec4(separator_hovered);
  colors[ImGuiCol_SeparatorActive] = ConvertColorToImVec4(separator_active);
  colors[ImGuiCol_ResizeGrip] = ConvertColorToImVec4(resize_grip);
  colors[ImGuiCol_ResizeGripHovered] =
      ConvertColorToImVec4(resize_grip_hovered);
  colors[ImGuiCol_ResizeGripActive] = ConvertColorToImVec4(resize_grip_active);
  colors[ImGuiCol_Tab] = ConvertColorToImVec4(tab);
  colors[ImGuiCol_TabHovered] = ConvertColorToImVec4(tab_hovered);
  colors[ImGuiCol_TabSelected] = ConvertColorToImVec4(tab_active);
  colors[ImGuiCol_TabUnfocused] = ConvertColorToImVec4(tab_unfocused);
  colors[ImGuiCol_TabUnfocusedActive] =
      ConvertColorToImVec4(tab_unfocused_active);
  colors[ImGuiCol_DockingPreview] = ConvertColorToImVec4(docking_preview);
  colors[ImGuiCol_DockingEmptyBg] = ConvertColorToImVec4(docking_empty_bg);

  // Complete ImGui color support
  colors[ImGuiCol_CheckMark] = ConvertColorToImVec4(check_mark);
  colors[ImGuiCol_SliderGrab] = ConvertColorToImVec4(slider_grab);
  colors[ImGuiCol_SliderGrabActive] = ConvertColorToImVec4(slider_grab_active);
  colors[ImGuiCol_InputTextCursor] = ConvertColorToImVec4(input_text_cursor);
  colors[ImGuiCol_NavCursor] = ConvertColorToImVec4(nav_cursor);
  colors[ImGuiCol_NavWindowingHighlight] =
      ConvertColorToImVec4(nav_windowing_highlight);
  colors[ImGuiCol_NavWindowingDimBg] =
      ConvertColorToImVec4(nav_windowing_dim_bg);
  colors[ImGuiCol_ModalWindowDimBg] = ConvertColorToImVec4(modal_window_dim_bg);
  colors[ImGuiCol_TextSelectedBg] = ConvertColorToImVec4(text_selected_bg);
  colors[ImGuiCol_DragDropTarget] = ConvertColorToImVec4(drag_drop_target);
  colors[ImGuiCol_TableHeaderBg] = ConvertColorToImVec4(table_header_bg);
  colors[ImGuiCol_TableBorderStrong] =
      ConvertColorToImVec4(table_border_strong);
  colors[ImGuiCol_TableBorderLight] = ConvertColorToImVec4(table_border_light);
  colors[ImGuiCol_TableRowBg] = ConvertColorToImVec4(table_row_bg);
  colors[ImGuiCol_TableRowBgAlt] = ConvertColorToImVec4(table_row_bg_alt);
  colors[ImGuiCol_TextLink] = ConvertColorToImVec4(text_link);
  colors[ImGuiCol_PlotLines] = ConvertColorToImVec4(plot_lines);
  colors[ImGuiCol_PlotLinesHovered] = ConvertColorToImVec4(plot_lines_hovered);
  colors[ImGuiCol_PlotHistogram] = ConvertColorToImVec4(plot_histogram);
  colors[ImGuiCol_PlotHistogramHovered] =
      ConvertColorToImVec4(plot_histogram_hovered);
  colors[ImGuiCol_TreeLines] = ConvertColorToImVec4(tree_lines);

  // Additional ImGui colors for complete coverage
  colors[ImGuiCol_TabDimmed] = ConvertColorToImVec4(tab_dimmed);
  colors[ImGuiCol_TabDimmedSelected] =
      ConvertColorToImVec4(tab_dimmed_selected);
  colors[ImGuiCol_TabDimmedSelectedOverline] =
      ConvertColorToImVec4(tab_dimmed_selected_overline);
  colors[ImGuiCol_TabSelectedOverline] =
      ConvertColorToImVec4(tab_selected_overline);

  // Apply style parameters
  style->WindowRounding = window_rounding;
  style->FrameRounding = frame_rounding;
  style->ScrollbarRounding = scrollbar_rounding;
  style->GrabRounding = grab_rounding;
  style->TabRounding = tab_rounding;
  style->WindowBorderSize = window_border_size;
  style->FrameBorderSize = frame_border_size;
}

// ThemeManager Implementation
ThemeManager& ThemeManager::Get() {
  static ThemeManager instance;
  return instance;
}

void ThemeManager::InitializeBuiltInThemes() {
  // Always create fallback theme first
  CreateFallbackYazeClassic();

  // Create the Classic YAZE theme during initialization
  ApplyClassicYazeTheme();

  // Load all available theme files dynamically
  auto status = LoadAllAvailableThemes();
  if (!status.ok()) {
    LOG_ERROR("Theme Manager", "Failed to load some theme files");
  }

  // Ensure we have a valid current theme (Classic is already set above)
  // Only fallback to file themes if Classic creation failed
  if (current_theme_name_ != "Classic YAZE") {
    if (themes_.find("YAZE Tre") != themes_.end()) {
      current_theme_ = themes_["YAZE Tre"];
      current_theme_name_ = "YAZE Tre";
    }
  }
}

void ThemeManager::CreateFallbackYazeClassic() {
  // Fallback theme that matches the original ColorsYaze() function colors but in theme format
  EnhancedTheme theme;
  theme.name = "YAZE Tre";
  theme.description = "YAZE theme resource edition";
  theme.author = "YAZE Team";

  // Use the exact original ColorsYaze colors
  theme.primary = RGBA(92, 115, 92);   // allttpLightGreen
  theme.secondary = RGBA(71, 92, 71);  // alttpMidGreen
  theme.accent = RGBA(89, 119, 89);    // TabActive
  theme.background =
      RGBA(8, 8, 8);  // Very dark gray for better grid visibility

  theme.text_primary = RGBA(230, 230, 230);   // 0.90f, 0.90f, 0.90f
  theme.text_disabled = RGBA(153, 153, 153);  // 0.60f, 0.60f, 0.60f
  theme.window_bg = RGBA(8, 8, 8, 217);       // Very dark gray with same alpha
  theme.child_bg = RGBA(0, 0, 0, 0);          // Transparent
  theme.popup_bg = RGBA(28, 28, 36, 235);     // 0.11f, 0.11f, 0.14f, 0.92f

  theme.button = RGBA(71, 92, 71);             // alttpMidGreen
  theme.button_hovered = RGBA(125, 146, 125);  // allttpLightestGreen
  theme.button_active = RGBA(92, 115, 92);     // allttpLightGreen

  theme.header = RGBA(46, 66, 46);           // alttpDarkGreen
  theme.header_hovered = RGBA(92, 115, 92);  // allttpLightGreen
  theme.header_active = RGBA(71, 92, 71);    // alttpMidGreen

  theme.menu_bar_bg = RGBA(46, 66, 46);    // alttpDarkGreen
  theme.tab = RGBA(46, 66, 46);            // alttpDarkGreen
  theme.tab_hovered = RGBA(71, 92, 71);    // alttpMidGreen
  theme.tab_active = RGBA(89, 119, 89);    // TabActive
  theme.tab_unfocused = RGBA(37, 52, 37);  // Darker version of tab
  theme.tab_unfocused_active =
      RGBA(62, 83, 62);  // Darker version of tab_active

  // Complete all remaining ImGui colors from original ColorsYaze() function
  theme.title_bg = RGBA(71, 92, 71);            // alttpMidGreen
  theme.title_bg_active = RGBA(46, 66, 46);     // alttpDarkGreen
  theme.title_bg_collapsed = RGBA(71, 92, 71);  // alttpMidGreen

  // Initialize missing fields that were added to the struct
  theme.surface = theme.background;
  theme.error = RGBA(220, 50, 50);
  theme.warning = RGBA(255, 200, 50);
  theme.success = theme.primary;
  theme.info = RGBA(70, 170, 255);
  theme.text_secondary = RGBA(200, 200, 200);
  theme.modal_bg = theme.popup_bg;

  // Borders and separators
  theme.border = RGBA(92, 115, 92);               // allttpLightGreen
  theme.border_shadow = RGBA(0, 0, 0, 0);         // Transparent
  theme.separator = RGBA(128, 128, 128, 153);     // 0.50f, 0.50f, 0.50f, 0.60f
  theme.separator_hovered = RGBA(153, 153, 178);  // 0.60f, 0.60f, 0.70f
  theme.separator_active = RGBA(178, 178, 230);   // 0.70f, 0.70f, 0.90f

  // Scrollbars
  theme.scrollbar_bg = RGBA(92, 115, 92, 153);   // 0.36f, 0.45f, 0.36f, 0.60f
  theme.scrollbar_grab = RGBA(92, 115, 92, 76);  // 0.36f, 0.45f, 0.36f, 0.30f
  theme.scrollbar_grab_hovered =
      RGBA(92, 115, 92, 102);  // 0.36f, 0.45f, 0.36f, 0.40f
  theme.scrollbar_grab_active =
      RGBA(92, 115, 92, 153);  // 0.36f, 0.45f, 0.36f, 0.60f

  // Resize grips (from original - light blue highlights)
  theme.resize_grip = RGBA(255, 255, 255, 26);  // 1.00f, 1.00f, 1.00f, 0.10f
  theme.resize_grip_hovered =
      RGBA(199, 209, 255, 153);  // 0.78f, 0.82f, 1.00f, 0.60f
  theme.resize_grip_active =
      RGBA(199, 209, 255, 230);  // 0.78f, 0.82f, 1.00f, 0.90f

  // ENHANCED: Complete ImGui colors with theme-aware smart defaults
  // Use theme colors instead of hardcoded values for consistency
  theme.check_mark =
      RGBA(125, 255, 125, 255);  // Bright green checkmark (highly visible!)
  theme.slider_grab = RGBA(92, 115, 92, 255);  // Theme green (solid)
  theme.slider_grab_active =
      RGBA(125, 146, 125, 255);  // Lighter green when active
  theme.input_text_cursor =
      RGBA(255, 255, 255, 255);                 // White cursor (always visible)
  theme.nav_cursor = RGBA(125, 146, 125, 255);  // Light green for navigation
  theme.nav_windowing_highlight =
      RGBA(89, 119, 89, 200);  // Accent with high visibility
  theme.nav_windowing_dim_bg =
      RGBA(0, 0, 0, 150);  // Darker overlay for better contrast
  theme.modal_window_dim_bg = RGBA(0, 0, 0, 128);  // 50% alpha for modals
  theme.text_selected_bg = RGBA(
      92, 115, 92, 128);  // Theme green with 50% alpha (visible selection!)
  theme.drag_drop_target =
      RGBA(125, 146, 125, 200);  // Bright green for drop zones

  // Table colors (from original)
  theme.table_header_bg = RGBA(46, 66, 46);      // alttpDarkGreen
  theme.table_border_strong = RGBA(71, 92, 71);  // alttpMidGreen
  theme.table_border_light = RGBA(66, 66, 71);   // 0.26f, 0.26f, 0.28f
  theme.table_row_bg = RGBA(0, 0, 0, 0);         // Transparent
  theme.table_row_bg_alt =
      RGBA(255, 255, 255, 18);  // 1.00f, 1.00f, 1.00f, 0.07f

  // Links and plots - use accent colors intelligently
  theme.text_link = theme.accent;                    // Accent for links
  theme.plot_lines = RGBA(255, 255, 255);            // White for plots
  theme.plot_lines_hovered = RGBA(230, 178, 0);      // 0.90f, 0.70f, 0.00f
  theme.plot_histogram = RGBA(230, 178, 0);          // Same as above
  theme.plot_histogram_hovered = RGBA(255, 153, 0);  // 1.00f, 0.60f, 0.00f

  // Docking colors
  theme.docking_preview =
      RGBA(92, 115, 92, 180);  // Light green with transparency
  theme.docking_empty_bg = RGBA(46, 66, 46, 255);  // Dark green

  // Apply original style settings
  theme.window_rounding = 0.0f;
  theme.frame_rounding = 5.0f;
  theme.scrollbar_rounding = 5.0f;
  theme.tab_rounding = 0.0f;
  theme.enable_glow_effects = false;

  themes_["YAZE Tre"] = theme;
  current_theme_ = theme;
  current_theme_name_ = "YAZE Tre";
}

absl::Status ThemeManager::LoadTheme(const std::string& theme_name) {
  auto it = themes_.find(theme_name);
  if (it == themes_.end()) {
    return absl::NotFoundError(
        absl::StrFormat("Theme '%s' not found", theme_name));
  }

  current_theme_ = it->second;
  current_theme_name_ = theme_name;
  current_theme_.ApplyToImGui();

  return absl::OkStatus();
}

absl::Status ThemeManager::LoadThemeFromFile(const std::string& filepath) {
  // Try multiple possible paths where theme files might be located
  std::vector<std::string> possible_paths = {
      filepath,                        // Absolute path
      "assets/themes/" + filepath,     // Relative from build dir
      "../assets/themes/" + filepath,  // Relative from bin dir
      util::GetResourcePath("assets/themes/" +
                            filepath),  // Platform-specific resource path
  };

  std::ifstream file;
  std::string successful_path;

  for (const auto& path : possible_paths) {
    file.open(path);
    if (file.is_open()) {
      successful_path = path;
      break;
    } else {
      file.clear();  // Clear any error flags before trying next path
    }
  }

  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Cannot open theme file: %s (tried %zu paths)",
                        filepath, possible_paths.size()));
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  file.close();

  if (content.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Theme file is empty: %s", successful_path));
  }

  EnhancedTheme theme;
  auto parse_status = ParseThemeFile(content, theme);
  if (!parse_status.ok()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse theme file %s: %s", successful_path,
                        parse_status.message()));
  }

  if (theme.name.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Theme file missing name: %s", successful_path));
  }

  themes_[theme.name] = theme;
  return absl::OkStatus();
}

std::vector<std::string> ThemeManager::GetAvailableThemes() const {
  std::vector<std::string> theme_names;
  for (const auto& [name, theme] : themes_) {
    theme_names.push_back(name);
  }
  return theme_names;
}

const EnhancedTheme* ThemeManager::GetTheme(const std::string& name) const {
  auto it = themes_.find(name);
  return (it != themes_.end()) ? &it->second : nullptr;
}

void ThemeManager::ApplyTheme(const std::string& theme_name) {
  auto status = LoadTheme(theme_name);
  if (!status.ok()) {
    // Fallback to YAZE Tre if theme not found
    auto fallback_status = LoadTheme("YAZE Tre");
    if (!fallback_status.ok()) {
      LOG_ERROR("Theme Manager", "Failed to load fallback theme");
    }
  }
}

void ThemeManager::ApplyTheme(const EnhancedTheme& theme) {
  current_theme_ = theme;
  current_theme_name_ = theme.name;  // CRITICAL: Update the name tracking
  current_theme_.ApplyToImGui();
}

Color ThemeManager::GetWelcomeScreenBackground() const {
  // Create a darker version of the window background for welcome screen
  Color bg = current_theme_.window_bg;
  return {bg.red * 0.8f, bg.green * 0.8f, bg.blue * 0.8f, bg.alpha};
}

Color ThemeManager::GetWelcomeScreenBorder() const {
  return current_theme_.accent;
}

Color ThemeManager::GetWelcomeScreenAccent() const {
  return current_theme_.primary;
}

void ThemeManager::ShowThemeSelector(bool* p_open) {
  if (!p_open || !*p_open)
    return;

  if (ImGui::Begin(
          absl::StrFormat("%s Theme Selector", ICON_MD_PALETTE).c_str(),
          p_open)) {

    // Add subtle particle effects to theme selector
    static float theme_animation_time = 0.0f;
    theme_animation_time += ImGui::GetIO().DeltaTime;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Subtle corner particles for theme selector
    for (int i = 0; i < 4; ++i) {
      float corner_offset = i * 1.57f;  // 90 degrees apart
      float x = window_pos.x + window_size.x * 0.5f +
                cosf(theme_animation_time * 0.8f + corner_offset) *
                    (window_size.x * 0.4f);
      float y = window_pos.y + window_size.y * 0.5f +
                sinf(theme_animation_time * 0.8f + corner_offset) *
                    (window_size.y * 0.4f);

      float alpha =
          0.1f + 0.1f * sinf(theme_animation_time * 1.2f + corner_offset);
      auto current_theme = GetCurrentTheme();
      ImU32 particle_color = ImGui::ColorConvertFloat4ToU32(
          ImVec4(current_theme.accent.red, current_theme.accent.green,
                 current_theme.accent.blue, alpha));

      draw_list->AddCircleFilled(ImVec2(x, y), 3.0f, particle_color);
    }

    ImGui::Text("%s Available Themes", ICON_MD_COLOR_LENS);
    ImGui::Separator();

    // Add Classic YAZE button first (direct ColorsYaze() application)
    bool is_classic_active = (current_theme_name_ == "Classic YAZE");
    if (is_classic_active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.45f, 0.36f,
                                                    1.0f));  // allttpLightGreen
    }

    if (ImGui::Button(
            absl::StrFormat("%s YAZE Classic (Original)",
                            is_classic_active ? ICON_MD_CHECK : ICON_MD_STAR)
                .c_str(),
            ImVec2(-1, 50))) {
      ApplyClassicYazeTheme();
    }

    if (is_classic_active) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Original YAZE theme using ColorsYaze() function");
      ImGui::Text("This is the authentic classic look - direct function call");
      ImGui::EndTooltip();
    }

    ImGui::Separator();

    // Sort themes alphabetically for consistent ordering (by name only)
    std::vector<std::string> sorted_theme_names;
    for (const auto& [name, theme] : themes_) {
      sorted_theme_names.push_back(name);
    }
    std::sort(sorted_theme_names.begin(), sorted_theme_names.end());

    for (const auto& name : sorted_theme_names) {
      const auto& theme = themes_.at(name);
      bool is_current = (name == current_theme_name_);

      if (is_current) {
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ConvertColorToImVec4(theme.accent));
      }

      if (ImGui::Button(
              absl::StrFormat("%s %s",
                              is_current ? ICON_MD_CHECK : ICON_MD_CIRCLE,
                              name.c_str())
                  .c_str(),
              ImVec2(-1, 40))) {
        auto status = LoadTheme(
            name);  // Use LoadTheme instead of ApplyTheme to ensure correct tracking
        if (!status.ok()) {
          LOG_ERROR("Theme Manager", "Failed to load theme %s", name.c_str());
        }
      }

      if (is_current) {
        ImGui::PopStyleColor();
      }

      // Show theme preview colors
      ImGui::SameLine();
      ImGui::ColorButton(absl::StrFormat("##primary_%s", name.c_str()).c_str(),
                         ConvertColorToImVec4(theme.primary),
                         ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
      ImGui::SameLine();
      ImGui::ColorButton(
          absl::StrFormat("##secondary_%s", name.c_str()).c_str(),
          ConvertColorToImVec4(theme.secondary), ImGuiColorEditFlags_NoTooltip,
          ImVec2(20, 20));
      ImGui::SameLine();
      ImGui::ColorButton(absl::StrFormat("##accent_%s", name.c_str()).c_str(),
                         ConvertColorToImVec4(theme.accent),
                         ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", theme.description.c_str());
        ImGui::Text("Author: %s", theme.author.c_str());
        ImGui::EndTooltip();
      }
    }

    ImGui::Separator();
    if (ImGui::Button(
            absl::StrFormat("%s Refresh Themes", ICON_MD_REFRESH).c_str())) {
      auto status = RefreshAvailableThemes();
      if (!status.ok()) {
        LOG_ERROR("Theme Manager", "Failed to refresh themes");
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(
            absl::StrFormat("%s Load Custom Theme", ICON_MD_FOLDER_OPEN)
                .c_str())) {
      auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!file_path.empty()) {
        auto status = LoadThemeFromFile(file_path);
        if (!status.ok()) {
          // Show error toast (would need access to toast manager)
        }
      }
    }

    ImGui::SameLine();
    static bool show_simple_editor = false;
    if (ImGui::Button(
            absl::StrFormat("%s Theme Editor", ICON_MD_EDIT).c_str())) {
      show_simple_editor = true;
    }

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Edit and save custom themes");
      ImGui::Text("Includes 'Save to File' functionality");
      ImGui::EndTooltip();
    }

    if (show_simple_editor) {
      ShowSimpleThemeEditor(&show_simple_editor);
    }
  }
  ImGui::End();
}

absl::Status ThemeManager::ParseThemeFile(const std::string& content,
                                          EnhancedTheme& theme) {
  std::istringstream stream(content);
  std::string line;
  std::string current_section = "";

  while (std::getline(stream, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
      continue;

    // Check for section headers [section_name]
    if (line[0] == '[' && line.back() == ']') {
      current_section = line.substr(1, line.length() - 2);
      continue;
    }

    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos)
      continue;

    std::string key = line.substr(0, eq_pos);
    std::string value = line.substr(eq_pos + 1);

    // Trim whitespace and comments
    key.erase(0, key.find_first_not_of(" \t"));
    key.erase(key.find_last_not_of(" \t") + 1);
    value.erase(0, value.find_first_not_of(" \t"));

    // Remove inline comments
    size_t comment_pos = value.find('#');
    if (comment_pos != std::string::npos) {
      value = value.substr(0, comment_pos);
    }
    value.erase(value.find_last_not_of(" \t") + 1);

    // Parse based on section
    if (current_section == "colors") {
      Color color = ParseColorFromString(value);

      if (key == "primary")
        theme.primary = color;
      else if (key == "secondary")
        theme.secondary = color;
      else if (key == "accent")
        theme.accent = color;
      else if (key == "background")
        theme.background = color;
      else if (key == "surface")
        theme.surface = color;
      else if (key == "error")
        theme.error = color;
      else if (key == "warning")
        theme.warning = color;
      else if (key == "success")
        theme.success = color;
      else if (key == "info")
        theme.info = color;
      else if (key == "text_primary")
        theme.text_primary = color;
      else if (key == "text_secondary")
        theme.text_secondary = color;
      else if (key == "text_disabled")
        theme.text_disabled = color;
      else if (key == "window_bg")
        theme.window_bg = color;
      else if (key == "child_bg")
        theme.child_bg = color;
      else if (key == "popup_bg")
        theme.popup_bg = color;
      else if (key == "button")
        theme.button = color;
      else if (key == "button_hovered")
        theme.button_hovered = color;
      else if (key == "button_active")
        theme.button_active = color;
      else if (key == "frame_bg")
        theme.frame_bg = color;
      else if (key == "frame_bg_hovered")
        theme.frame_bg_hovered = color;
      else if (key == "frame_bg_active")
        theme.frame_bg_active = color;
      else if (key == "header")
        theme.header = color;
      else if (key == "header_hovered")
        theme.header_hovered = color;
      else if (key == "header_active")
        theme.header_active = color;
      else if (key == "tab")
        theme.tab = color;
      else if (key == "tab_hovered")
        theme.tab_hovered = color;
      else if (key == "tab_active")
        theme.tab_active = color;
      else if (key == "menu_bar_bg")
        theme.menu_bar_bg = color;
      else if (key == "title_bg")
        theme.title_bg = color;
      else if (key == "title_bg_active")
        theme.title_bg_active = color;
      else if (key == "title_bg_collapsed")
        theme.title_bg_collapsed = color;
      else if (key == "separator")
        theme.separator = color;
      else if (key == "separator_hovered")
        theme.separator_hovered = color;
      else if (key == "separator_active")
        theme.separator_active = color;
      else if (key == "scrollbar_bg")
        theme.scrollbar_bg = color;
      else if (key == "scrollbar_grab")
        theme.scrollbar_grab = color;
      else if (key == "scrollbar_grab_hovered")
        theme.scrollbar_grab_hovered = color;
      else if (key == "scrollbar_grab_active")
        theme.scrollbar_grab_active = color;
      else if (key == "border")
        theme.border = color;
      else if (key == "border_shadow")
        theme.border_shadow = color;
      else if (key == "resize_grip")
        theme.resize_grip = color;
      else if (key == "resize_grip_hovered")
        theme.resize_grip_hovered = color;
      else if (key == "resize_grip_active")
        theme.resize_grip_active = color;
      else if (key == "check_mark")
        theme.check_mark = color;
      else if (key == "slider_grab")
        theme.slider_grab = color;
      else if (key == "slider_grab_active")
        theme.slider_grab_active = color;
      else if (key == "input_text_cursor")
        theme.input_text_cursor = color;
      else if (key == "nav_cursor")
        theme.nav_cursor = color;
      else if (key == "nav_windowing_highlight")
        theme.nav_windowing_highlight = color;
      else if (key == "nav_windowing_dim_bg")
        theme.nav_windowing_dim_bg = color;
      else if (key == "modal_window_dim_bg")
        theme.modal_window_dim_bg = color;
      else if (key == "text_selected_bg")
        theme.text_selected_bg = color;
      else if (key == "drag_drop_target")
        theme.drag_drop_target = color;
      else if (key == "table_header_bg")
        theme.table_header_bg = color;
      else if (key == "table_border_strong")
        theme.table_border_strong = color;
      else if (key == "table_border_light")
        theme.table_border_light = color;
      else if (key == "table_row_bg")
        theme.table_row_bg = color;
      else if (key == "table_row_bg_alt")
        theme.table_row_bg_alt = color;
      else if (key == "text_link")
        theme.text_link = color;
      else if (key == "plot_lines")
        theme.plot_lines = color;
      else if (key == "plot_lines_hovered")
        theme.plot_lines_hovered = color;
      else if (key == "plot_histogram")
        theme.plot_histogram = color;
      else if (key == "plot_histogram_hovered")
        theme.plot_histogram_hovered = color;
      else if (key == "tree_lines")
        theme.tree_lines = color;
      else if (key == "tab_dimmed")
        theme.tab_dimmed = color;
      else if (key == "tab_dimmed_selected")
        theme.tab_dimmed_selected = color;
      else if (key == "tab_dimmed_selected_overline")
        theme.tab_dimmed_selected_overline = color;
      else if (key == "tab_selected_overline")
        theme.tab_selected_overline = color;
      else if (key == "docking_preview")
        theme.docking_preview = color;
      else if (key == "docking_empty_bg")
        theme.docking_empty_bg = color;
    } else if (current_section == "style") {
      if (key == "window_rounding")
        theme.window_rounding = std::stof(value);
      else if (key == "frame_rounding")
        theme.frame_rounding = std::stof(value);
      else if (key == "scrollbar_rounding")
        theme.scrollbar_rounding = std::stof(value);
      else if (key == "grab_rounding")
        theme.grab_rounding = std::stof(value);
      else if (key == "tab_rounding")
        theme.tab_rounding = std::stof(value);
      else if (key == "window_border_size")
        theme.window_border_size = std::stof(value);
      else if (key == "frame_border_size")
        theme.frame_border_size = std::stof(value);
      else if (key == "enable_animations")
        theme.enable_animations = (value == "true");
      else if (key == "enable_glow_effects")
        theme.enable_glow_effects = (value == "true");
      else if (key == "animation_speed")
        theme.animation_speed = std::stof(value);
    } else if (current_section == "" || current_section == "metadata") {
      // Top-level metadata
      if (key == "name")
        theme.name = value;
      else if (key == "description")
        theme.description = value;
      else if (key == "author")
        theme.author = value;
    }
  }

  return absl::OkStatus();
}

Color ThemeManager::ParseColorFromString(const std::string& color_str) const {
  std::vector<std::string> components = absl::StrSplit(color_str, ',');
  if (components.size() != 4) {
    return RGBA(255, 255, 255, 255);  // White fallback
  }

  try {
    int r = std::stoi(components[0]);
    int g = std::stoi(components[1]);
    int b = std::stoi(components[2]);
    int a = std::stoi(components[3]);
    return RGBA(r, g, b, a);
  } catch (...) {
    return RGBA(255, 255, 255, 255);  // White fallback
  }
}

std::string ThemeManager::SerializeTheme(const EnhancedTheme& theme) const {
  std::ostringstream ss;

  // Helper function to convert color to RGB string
  auto colorToString = [](const Color& c) -> std::string {
    int r = static_cast<int>(c.red * 255.0f);
    int g = static_cast<int>(c.green * 255.0f);
    int b = static_cast<int>(c.blue * 255.0f);
    int a = static_cast<int>(c.alpha * 255.0f);
    return std::to_string(r) + "," + std::to_string(g) + "," +
           std::to_string(b) + "," + std::to_string(a);
  };

  ss << "# yaze Theme File\n";
  ss << "# Generated by YAZE Theme Editor\n";
  ss << "name=" << theme.name << "\n";
  ss << "description=" << theme.description << "\n";
  ss << "author=" << theme.author << "\n";
  ss << "version=1.0\n";
  ss << "\n[colors]\n";

  // Primary colors
  ss << "# Primary colors\n";
  ss << "primary=" << colorToString(theme.primary) << "\n";
  ss << "secondary=" << colorToString(theme.secondary) << "\n";
  ss << "accent=" << colorToString(theme.accent) << "\n";
  ss << "background=" << colorToString(theme.background) << "\n";
  ss << "surface=" << colorToString(theme.surface) << "\n";
  ss << "\n";

  // Status colors
  ss << "# Status colors\n";
  ss << "error=" << colorToString(theme.error) << "\n";
  ss << "warning=" << colorToString(theme.warning) << "\n";
  ss << "success=" << colorToString(theme.success) << "\n";
  ss << "info=" << colorToString(theme.info) << "\n";
  ss << "\n";

  // Text colors
  ss << "# Text colors\n";
  ss << "text_primary=" << colorToString(theme.text_primary) << "\n";
  ss << "text_secondary=" << colorToString(theme.text_secondary) << "\n";
  ss << "text_disabled=" << colorToString(theme.text_disabled) << "\n";
  ss << "\n";

  // Window colors
  ss << "# Window colors\n";
  ss << "window_bg=" << colorToString(theme.window_bg) << "\n";
  ss << "child_bg=" << colorToString(theme.child_bg) << "\n";
  ss << "popup_bg=" << colorToString(theme.popup_bg) << "\n";
  ss << "\n";

  // Interactive elements
  ss << "# Interactive elements\n";
  ss << "button=" << colorToString(theme.button) << "\n";
  ss << "button_hovered=" << colorToString(theme.button_hovered) << "\n";
  ss << "button_active=" << colorToString(theme.button_active) << "\n";
  ss << "frame_bg=" << colorToString(theme.frame_bg) << "\n";
  ss << "frame_bg_hovered=" << colorToString(theme.frame_bg_hovered) << "\n";
  ss << "frame_bg_active=" << colorToString(theme.frame_bg_active) << "\n";
  ss << "\n";

  // Navigation
  ss << "# Navigation\n";
  ss << "header=" << colorToString(theme.header) << "\n";
  ss << "header_hovered=" << colorToString(theme.header_hovered) << "\n";
  ss << "header_active=" << colorToString(theme.header_active) << "\n";
  ss << "tab=" << colorToString(theme.tab) << "\n";
  ss << "tab_hovered=" << colorToString(theme.tab_hovered) << "\n";
  ss << "tab_active=" << colorToString(theme.tab_active) << "\n";
  ss << "menu_bar_bg=" << colorToString(theme.menu_bar_bg) << "\n";
  ss << "title_bg=" << colorToString(theme.title_bg) << "\n";
  ss << "title_bg_active=" << colorToString(theme.title_bg_active) << "\n";
  ss << "title_bg_collapsed=" << colorToString(theme.title_bg_collapsed)
     << "\n";
  ss << "\n";

  // Borders and separators
  ss << "# Borders and separators\n";
  ss << "border=" << colorToString(theme.border) << "\n";
  ss << "border_shadow=" << colorToString(theme.border_shadow) << "\n";
  ss << "separator=" << colorToString(theme.separator) << "\n";
  ss << "separator_hovered=" << colorToString(theme.separator_hovered) << "\n";
  ss << "separator_active=" << colorToString(theme.separator_active) << "\n";
  ss << "\n";

  // Scrollbars and controls
  ss << "# Scrollbars and controls\n";
  ss << "scrollbar_bg=" << colorToString(theme.scrollbar_bg) << "\n";
  ss << "scrollbar_grab=" << colorToString(theme.scrollbar_grab) << "\n";
  ss << "scrollbar_grab_hovered=" << colorToString(theme.scrollbar_grab_hovered)
     << "\n";
  ss << "scrollbar_grab_active=" << colorToString(theme.scrollbar_grab_active)
     << "\n";
  ss << "resize_grip=" << colorToString(theme.resize_grip) << "\n";
  ss << "resize_grip_hovered=" << colorToString(theme.resize_grip_hovered)
     << "\n";
  ss << "resize_grip_active=" << colorToString(theme.resize_grip_active)
     << "\n";
  ss << "check_mark=" << colorToString(theme.check_mark) << "\n";
  ss << "slider_grab=" << colorToString(theme.slider_grab) << "\n";
  ss << "slider_grab_active=" << colorToString(theme.slider_grab_active)
     << "\n";
  ss << "\n";

  // Navigation and special elements
  ss << "# Navigation and special elements\n";
  ss << "input_text_cursor=" << colorToString(theme.input_text_cursor) << "\n";
  ss << "nav_cursor=" << colorToString(theme.nav_cursor) << "\n";
  ss << "nav_windowing_highlight="
     << colorToString(theme.nav_windowing_highlight) << "\n";
  ss << "nav_windowing_dim_bg=" << colorToString(theme.nav_windowing_dim_bg)
     << "\n";
  ss << "modal_window_dim_bg=" << colorToString(theme.modal_window_dim_bg)
     << "\n";
  ss << "text_selected_bg=" << colorToString(theme.text_selected_bg) << "\n";
  ss << "drag_drop_target=" << colorToString(theme.drag_drop_target) << "\n";
  ss << "docking_preview=" << colorToString(theme.docking_preview) << "\n";
  ss << "docking_empty_bg=" << colorToString(theme.docking_empty_bg) << "\n";
  ss << "\n";

  // Table colors
  ss << "# Table colors\n";
  ss << "table_header_bg=" << colorToString(theme.table_header_bg) << "\n";
  ss << "table_border_strong=" << colorToString(theme.table_border_strong)
     << "\n";
  ss << "table_border_light=" << colorToString(theme.table_border_light)
     << "\n";
  ss << "table_row_bg=" << colorToString(theme.table_row_bg) << "\n";
  ss << "table_row_bg_alt=" << colorToString(theme.table_row_bg_alt) << "\n";
  ss << "\n";

  // Links and plots
  ss << "# Links and plots\n";
  ss << "text_link=" << colorToString(theme.text_link) << "\n";
  ss << "plot_lines=" << colorToString(theme.plot_lines) << "\n";
  ss << "plot_lines_hovered=" << colorToString(theme.plot_lines_hovered)
     << "\n";
  ss << "plot_histogram=" << colorToString(theme.plot_histogram) << "\n";
  ss << "plot_histogram_hovered=" << colorToString(theme.plot_histogram_hovered)
     << "\n";
  ss << "tree_lines=" << colorToString(theme.tree_lines) << "\n";
  ss << "\n";

  // Tab variations
  ss << "# Tab variations\n";
  ss << "tab_dimmed=" << colorToString(theme.tab_dimmed) << "\n";
  ss << "tab_dimmed_selected=" << colorToString(theme.tab_dimmed_selected)
     << "\n";
  ss << "tab_dimmed_selected_overline="
     << colorToString(theme.tab_dimmed_selected_overline) << "\n";
  ss << "tab_selected_overline=" << colorToString(theme.tab_selected_overline)
     << "\n";
  ss << "\n";

  // Enhanced semantic colors
  ss << "# Enhanced semantic colors\n";
  ss << "text_highlight=" << colorToString(theme.text_highlight) << "\n";
  ss << "link_hover=" << colorToString(theme.link_hover) << "\n";
  ss << "code_background=" << colorToString(theme.code_background) << "\n";
  ss << "success_light=" << colorToString(theme.success_light) << "\n";
  ss << "warning_light=" << colorToString(theme.warning_light) << "\n";
  ss << "error_light=" << colorToString(theme.error_light) << "\n";
  ss << "info_light=" << colorToString(theme.info_light) << "\n";
  ss << "\n";

  // UI state colors
  ss << "# UI state colors\n";
  ss << "active_selection=" << colorToString(theme.active_selection) << "\n";
  ss << "hover_highlight=" << colorToString(theme.hover_highlight) << "\n";
  ss << "focus_border=" << colorToString(theme.focus_border) << "\n";
  ss << "disabled_overlay=" << colorToString(theme.disabled_overlay) << "\n";
  ss << "\n";

  // Editor-specific colors
  ss << "# Editor-specific colors\n";
  ss << "editor_background=" << colorToString(theme.editor_background) << "\n";
  ss << "editor_grid=" << colorToString(theme.editor_grid) << "\n";
  ss << "editor_cursor=" << colorToString(theme.editor_cursor) << "\n";
  ss << "editor_selection=" << colorToString(theme.editor_selection) << "\n";
  ss << "\n";

  // Style settings
  ss << "[style]\n";
  ss << "window_rounding=" << theme.window_rounding << "\n";
  ss << "frame_rounding=" << theme.frame_rounding << "\n";
  ss << "scrollbar_rounding=" << theme.scrollbar_rounding << "\n";
  ss << "tab_rounding=" << theme.tab_rounding << "\n";
  ss << "enable_animations=" << (theme.enable_animations ? "true" : "false")
     << "\n";
  ss << "enable_glow_effects=" << (theme.enable_glow_effects ? "true" : "false")
     << "\n";

  return ss.str();
}

absl::Status ThemeManager::SaveThemeToFile(const EnhancedTheme& theme,
                                           const std::string& filepath) const {
  std::string theme_content = SerializeTheme(theme);

  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(
        absl::StrFormat("Failed to open file for writing: %s", filepath));
  }

  file << theme_content;
  file.close();

  if (file.fail()) {
    return absl::InternalError(
        absl::StrFormat("Failed to write theme file: %s", filepath));
  }

  return absl::OkStatus();
}

void ThemeManager::ApplyClassicYazeTheme() {
  // Apply the original ColorsYaze() function directly
  ColorsYaze();
  current_theme_name_ = "Classic YAZE";

  // Create a complete Classic theme object that matches what ColorsYaze() sets
  EnhancedTheme classic_theme;
  classic_theme.name = "Classic YAZE";
  classic_theme.description =
      "Original YAZE theme (direct ColorsYaze() function)";
  classic_theme.author = "YAZE Team";

  // Extract ALL the colors that ColorsYaze() sets (copy from CreateFallbackYazeClassic)
  classic_theme.primary = RGBA(92, 115, 92);   // allttpLightGreen
  classic_theme.secondary = RGBA(71, 92, 71);  // alttpMidGreen
  classic_theme.accent = RGBA(89, 119, 89);    // TabActive
  classic_theme.background =
      RGBA(8, 8, 8);  // Very dark gray for better grid visibility

  classic_theme.text_primary = RGBA(230, 230, 230);   // 0.90f, 0.90f, 0.90f
  classic_theme.text_disabled = RGBA(153, 153, 153);  // 0.60f, 0.60f, 0.60f
  classic_theme.window_bg =
      RGBA(8, 8, 8, 217);                     // Very dark gray with same alpha
  classic_theme.child_bg = RGBA(0, 0, 0, 0);  // Transparent
  classic_theme.popup_bg = RGBA(28, 28, 36, 235);  // 0.11f, 0.11f, 0.14f, 0.92f

  classic_theme.button = RGBA(71, 92, 71);             // alttpMidGreen
  classic_theme.button_hovered = RGBA(125, 146, 125);  // allttpLightestGreen
  classic_theme.button_active = RGBA(92, 115, 92);     // allttpLightGreen

  classic_theme.header = RGBA(46, 66, 46);           // alttpDarkGreen
  classic_theme.header_hovered = RGBA(92, 115, 92);  // allttpLightGreen
  classic_theme.header_active = RGBA(71, 92, 71);    // alttpMidGreen

  classic_theme.menu_bar_bg = RGBA(46, 66, 46);    // alttpDarkGreen
  classic_theme.tab = RGBA(46, 66, 46);            // alttpDarkGreen
  classic_theme.tab_hovered = RGBA(71, 92, 71);    // alttpMidGreen
  classic_theme.tab_active = RGBA(89, 119, 89);    // TabActive
  classic_theme.tab_unfocused = RGBA(37, 52, 37);  // Darker version of tab
  classic_theme.tab_unfocused_active =
      RGBA(62, 83, 62);  // Darker version of tab_active

  // Complete all remaining ImGui colors from original ColorsYaze() function
  classic_theme.title_bg = RGBA(71, 92, 71);            // alttpMidGreen
  classic_theme.title_bg_active = RGBA(46, 66, 46);     // alttpDarkGreen
  classic_theme.title_bg_collapsed = RGBA(71, 92, 71);  // alttpMidGreen

  // Borders and separators
  classic_theme.border = RGBA(92, 115, 92);        // allttpLightGreen
  classic_theme.border_shadow = RGBA(0, 0, 0, 0);  // Transparent
  classic_theme.separator =
      RGBA(128, 128, 128, 153);  // 0.50f, 0.50f, 0.50f, 0.60f
  classic_theme.separator_hovered = RGBA(153, 153, 178);  // 0.60f, 0.60f, 0.70f
  classic_theme.separator_active = RGBA(178, 178, 230);   // 0.70f, 0.70f, 0.90f

  // Scrollbars
  classic_theme.scrollbar_bg =
      RGBA(92, 115, 92, 153);  // 0.36f, 0.45f, 0.36f, 0.60f
  classic_theme.scrollbar_grab =
      RGBA(92, 115, 92, 76);  // 0.36f, 0.45f, 0.36f, 0.30f
  classic_theme.scrollbar_grab_hovered =
      RGBA(92, 115, 92, 102);  // 0.36f, 0.45f, 0.36f, 0.40f
  classic_theme.scrollbar_grab_active =
      RGBA(92, 115, 92, 153);  // 0.36f, 0.45f, 0.36f, 0.60f

  // ENHANCED: Frame colors for inputs/widgets
  classic_theme.frame_bg =
      RGBA(46, 66, 46, 140);  // Darker green with some transparency
  classic_theme.frame_bg_hovered =
      RGBA(71, 92, 71, 170);  // Mid green when hovered
  classic_theme.frame_bg_active =
      RGBA(92, 115, 92, 200);  // Light green when active

  // FIXED: Resize grips with better visibility
  classic_theme.resize_grip = RGBA(92, 115, 92, 80);  // Theme green, subtle
  classic_theme.resize_grip_hovered =
      RGBA(125, 146, 125, 180);  // Brighter when hovered
  classic_theme.resize_grip_active =
      RGBA(125, 146, 125, 255);  // Solid when active

  // FIXED: Checkmark - bright green for high visibility!
  classic_theme.check_mark =
      RGBA(125, 255, 125, 255);  // Bright green (clearly visible)

  // FIXED: Sliders with theme colors
  classic_theme.slider_grab = RGBA(92, 115, 92, 255);  // Theme green (solid)
  classic_theme.slider_grab_active =
      RGBA(125, 146, 125, 255);  // Lighter when grabbed

  // FIXED: Input cursor - white for maximum visibility
  classic_theme.input_text_cursor =
      RGBA(255, 255, 255, 255);  // White cursor (always visible)

  // FIXED: Navigation with theme colors
  classic_theme.nav_cursor =
      RGBA(125, 146, 125, 255);  // Light green navigation
  classic_theme.nav_windowing_highlight =
      RGBA(92, 115, 92, 200);  // Theme green highlight
  classic_theme.nav_windowing_dim_bg = RGBA(0, 0, 0, 150);  // Darker overlay

  // FIXED: Modals with better dimming
  classic_theme.modal_window_dim_bg = RGBA(0, 0, 0, 128);  // 50% alpha

  // FIXED: Text selection - visible and theme-appropriate!
  classic_theme.text_selected_bg =
      RGBA(92, 115, 92, 128);  // Theme green with 50% alpha (visible!)

  // FIXED: Drag/drop target with high visibility
  classic_theme.drag_drop_target = RGBA(125, 146, 125, 200);  // Bright green
  classic_theme.table_header_bg = RGBA(46, 66, 46);
  classic_theme.table_border_strong = RGBA(71, 92, 71);
  classic_theme.table_border_light = RGBA(66, 66, 71);
  classic_theme.table_row_bg = RGBA(0, 0, 0, 0);
  classic_theme.table_row_bg_alt = RGBA(255, 255, 255, 18);
  classic_theme.text_link = classic_theme.accent;
  classic_theme.plot_lines = RGBA(255, 255, 255);
  classic_theme.plot_lines_hovered = RGBA(230, 178, 0);
  classic_theme.plot_histogram = RGBA(230, 178, 0);
  classic_theme.plot_histogram_hovered = RGBA(255, 153, 0);
  classic_theme.docking_preview = RGBA(92, 115, 92, 180);
  classic_theme.docking_empty_bg = RGBA(46, 66, 46, 255);
  classic_theme.tree_lines =
      classic_theme.separator;  // Use separator color for tree lines

  // Tab dimmed colors (for unfocused tabs)
  classic_theme.tab_dimmed = RGBA(37, 52, 37);  // Darker version of tab
  classic_theme.tab_dimmed_selected =
      RGBA(62, 83, 62);  // Darker version of tab_active
  classic_theme.tab_dimmed_selected_overline = classic_theme.accent;
  classic_theme.tab_selected_overline = classic_theme.accent;

  // Enhanced semantic colors for better theming
  classic_theme.text_highlight =
      RGBA(255, 255, 150);  // Light yellow for highlights
  classic_theme.link_hover =
      RGBA(140, 220, 255);  // Brighter blue for link hover
  classic_theme.code_background =
      RGBA(40, 60, 40);  // Slightly darker green for code
  classic_theme.success_light = RGBA(140, 195, 140);  // Light green
  classic_theme.warning_light = RGBA(255, 220, 100);  // Light yellow
  classic_theme.error_light = RGBA(255, 150, 150);    // Light red
  classic_theme.info_light = RGBA(150, 200, 255);     // Light blue

  // UI state colors
  classic_theme.active_selection =
      classic_theme.accent;  // Use accent color for active selection
  classic_theme.hover_highlight =
      RGBA(92, 115, 92, 100);                          // Semi-transparent green
  classic_theme.focus_border = classic_theme.primary;  // Use primary for focus
  classic_theme.disabled_overlay = RGBA(50, 50, 50, 128);  // Gray overlay

  // Editor-specific colors
  classic_theme.editor_background = RGBA(30, 45, 30);  // Dark green background
  classic_theme.editor_grid = RGBA(80, 100, 80, 100);  // Subtle grid lines
  classic_theme.editor_cursor = RGBA(255, 255, 255);   // White cursor
  classic_theme.editor_selection =
      RGBA(110, 145, 110, 100);  // Semi-transparent selection

  // Apply original style settings
  classic_theme.window_rounding = 0.0f;
  classic_theme.frame_rounding = 5.0f;
  classic_theme.scrollbar_rounding = 5.0f;
  classic_theme.tab_rounding = 0.0f;
  classic_theme.enable_glow_effects = false;

  // DON'T add Classic theme to themes map - keep it as a special case
  // themes_["Classic YAZE"] = classic_theme; // REMOVED to prevent off-by-one
  current_theme_ = classic_theme;
}

void ThemeManager::ShowSimpleThemeEditor(bool* p_open) {
  if (!p_open || !*p_open)
    return;

  ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(absl::StrFormat("%s Theme Editor", ICON_MD_PALETTE).c_str(),
                   p_open, ImGuiWindowFlags_MenuBar)) {

    // Add gentle particle effects to theme editor background
    static float editor_animation_time = 0.0f;
    editor_animation_time += ImGui::GetIO().DeltaTime;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();

    // Floating color orbs representing different color categories
    auto current_theme = GetCurrentTheme();
    std::vector<gui::Color> theme_colors = {
        current_theme.primary, current_theme.secondary, current_theme.accent,
        current_theme.success, current_theme.warning,   current_theme.error};

    for (size_t i = 0; i < theme_colors.size(); ++i) {
      float time_offset = i * 1.0f;
      float orbit_radius = 60.0f + i * 8.0f;
      float x = window_pos.x + window_size.x * 0.8f +
                cosf(editor_animation_time * 0.3f + time_offset) * orbit_radius;
      float y = window_pos.y + window_size.y * 0.3f +
                sinf(editor_animation_time * 0.3f + time_offset) * orbit_radius;

      float alpha =
          0.15f + 0.1f * sinf(editor_animation_time * 1.5f + time_offset);
      ImU32 orb_color = ImGui::ColorConvertFloat4ToU32(
          ImVec4(theme_colors[i].red, theme_colors[i].green,
                 theme_colors[i].blue, alpha));

      float radius =
          4.0f + sinf(editor_animation_time * 2.0f + time_offset) * 1.0f;
      draw_list->AddCircleFilled(ImVec2(x, y), radius, orb_color);
    }

    // Menu bar for theme operations
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem(
                absl::StrFormat("%s New Theme", ICON_MD_ADD).c_str())) {
          // Reset to default theme
          ApplyClassicYazeTheme();
        }
        if (ImGui::MenuItem(
                absl::StrFormat("%s Load Theme", ICON_MD_FOLDER_OPEN)
                    .c_str())) {
          auto file_path = util::FileDialogWrapper::ShowOpenFileDialog();
          if (!file_path.empty()) {
            LoadThemeFromFile(file_path);
          }
        }
        ImGui::Separator();
        if (ImGui::MenuItem(
                absl::StrFormat("%s Save Theme", ICON_MD_SAVE).c_str())) {
          // Save current theme to its existing file
          std::string current_file_path = GetCurrentThemeFilePath();
          if (!current_file_path.empty()) {
            auto status = SaveThemeToFile(current_theme_, current_file_path);
            if (!status.ok()) {
              LOG_ERROR("Theme Manager", "Failed to save theme");
            }
          } else {
            // No existing file, prompt for new location
            auto file_path = util::FileDialogWrapper::ShowSaveFileDialog(
                current_theme_.name, "theme");
            if (!file_path.empty()) {
              auto status = SaveThemeToFile(current_theme_, file_path);
              if (!status.ok()) {
                LOG_ERROR("Theme Manager", "Failed to save theme");
              }
            }
          }
        }
        if (ImGui::MenuItem(
                absl::StrFormat("%s Save As...", ICON_MD_SAVE_AS).c_str())) {
          // Save theme to new file
          auto file_path = util::FileDialogWrapper::ShowSaveFileDialog(
              current_theme_.name, "theme");
          if (!file_path.empty()) {
            auto status = SaveThemeToFile(current_theme_, file_path);
            if (!status.ok()) {
              LOG_ERROR("Theme Manager", "Failed to save theme");
            }
          }
        }
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Presets")) {
        if (ImGui::MenuItem("YAZE Classic")) {
          ApplyClassicYazeTheme();
        }

        auto available_themes = GetAvailableThemes();
        if (!available_themes.empty()) {
          ImGui::Separator();
          for (const auto& theme_name : available_themes) {
            if (ImGui::MenuItem(theme_name.c_str())) {
              LoadTheme(theme_name);
            }
          }
        }
        ImGui::EndMenu();
      }

      ImGui::EndMenuBar();
    }

    static EnhancedTheme edit_theme = current_theme_;
    static char theme_name[128];
    static char theme_description[256];
    static char theme_author[128];
    static bool live_preview = true;
    static EnhancedTheme
        original_theme;  // Store original theme for restoration
    static bool theme_backup_made = false;

    // Helper lambda for live preview application
    auto apply_live_preview = [&]() {
      if (live_preview) {
        if (!theme_backup_made) {
          original_theme = current_theme_;
          theme_backup_made = true;
        }
        // Apply the edit theme directly to ImGui without changing theme manager state
        edit_theme.ApplyToImGui();
      }
    };

    // Live preview toggle
    ImGui::Checkbox("Live Preview", &live_preview);
    ImGui::SameLine();
    ImGui::Text("| Changes apply immediately when enabled");

    // If live preview was just disabled, restore original theme
    static bool prev_live_preview = live_preview;
    if (prev_live_preview && !live_preview && theme_backup_made) {
      ApplyTheme(original_theme);
      theme_backup_made = false;
    }
    prev_live_preview = live_preview;

    ImGui::Separator();

    // Theme metadata in a table for better layout
    if (ImGui::BeginTable("ThemeMetadata", 2,
                          ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Name:");
      ImGui::TableNextColumn();
      if (ImGui::InputText("##theme_name", theme_name, sizeof(theme_name))) {
        edit_theme.name = std::string(theme_name);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Description:");
      ImGui::TableNextColumn();
      if (ImGui::InputText("##theme_description", theme_description,
                           sizeof(theme_description))) {
        edit_theme.description = std::string(theme_description);
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Author:");
      ImGui::TableNextColumn();
      if (ImGui::InputText("##theme_author", theme_author,
                           sizeof(theme_author))) {
        edit_theme.author = std::string(theme_author);
      }

      ImGui::EndTable();
    }

    ImGui::Separator();

    // Enhanced theme editing with tabs for better organization
    if (ImGui::BeginTabBar("ThemeEditorTabs", ImGuiTabBarFlags_None)) {

      // Apply live preview on first frame if enabled
      static bool first_frame = true;
      if (first_frame && live_preview) {
        apply_live_preview();
        first_frame = false;
      }

      // Primary Colors Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Primary", ICON_MD_COLOR_LENS).c_str())) {
        if (ImGui::BeginTable("PrimaryColorsTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                  0.4f);
          ImGui::TableHeadersRow();

          // Primary color
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::AlignTextToFramePadding();
          ImGui::Text("Primary:");
          ImGui::TableNextColumn();
          ImVec4 primary = ConvertColorToImVec4(edit_theme.primary);
          if (ImGui::ColorEdit3("##primary", &primary.x)) {
            edit_theme.primary = {primary.x, primary.y, primary.z, primary.w};
            apply_live_preview();
          }
          ImGui::TableNextColumn();
          ImGui::Button("Primary Preview", ImVec2(-1, 30));

          // Secondary color
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::AlignTextToFramePadding();
          ImGui::Text("Secondary:");
          ImGui::TableNextColumn();
          ImVec4 secondary = ConvertColorToImVec4(edit_theme.secondary);
          if (ImGui::ColorEdit3("##secondary", &secondary.x)) {
            edit_theme.secondary = {secondary.x, secondary.y, secondary.z,
                                    secondary.w};
            apply_live_preview();
          }
          ImGui::TableNextColumn();
          ImGui::PushStyleColor(ImGuiCol_Button, secondary);
          ImGui::Button("Secondary Preview", ImVec2(-1, 30));
          ImGui::PopStyleColor();

          // Accent color
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::AlignTextToFramePadding();
          ImGui::Text("Accent:");
          ImGui::TableNextColumn();
          ImVec4 accent = ConvertColorToImVec4(edit_theme.accent);
          if (ImGui::ColorEdit3("##accent", &accent.x)) {
            edit_theme.accent = {accent.x, accent.y, accent.z, accent.w};
            apply_live_preview();
          }
          ImGui::TableNextColumn();
          ImGui::PushStyleColor(ImGuiCol_Button, accent);
          ImGui::Button("Accent Preview", ImVec2(-1, 30));
          ImGui::PopStyleColor();

          // Background color
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::AlignTextToFramePadding();
          ImGui::Text("Background:");
          ImGui::TableNextColumn();
          ImVec4 background = ConvertColorToImVec4(edit_theme.background);
          if (ImGui::ColorEdit4("##background", &background.x)) {
            edit_theme.background = {background.x, background.y, background.z,
                                     background.w};
            apply_live_preview();
          }
          ImGui::TableNextColumn();
          ImGui::Text("Background preview shown in window");

          ImGui::EndTable();
        }
        ImGui::EndTabItem();
      }

      // Text Colors Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Text", ICON_MD_TEXT_FIELDS).c_str())) {
        if (ImGui::BeginTable("TextColorsTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                  0.4f);
          ImGui::TableHeadersRow();

          // Text colors with live preview
          auto text_colors = std::vector<std::pair<const char*, Color*>>{
              {"Primary Text", &edit_theme.text_primary},
              {"Secondary Text", &edit_theme.text_secondary},
              {"Disabled Text", &edit_theme.text_disabled},
              {"Link Text", &edit_theme.text_link},
              {"Text Highlight", &edit_theme.text_highlight},
              {"Link Hover", &edit_theme.link_hover},
              {"Text Selected BG", &edit_theme.text_selected_bg},
              {"Input Text Cursor", &edit_theme.input_text_cursor}};

          for (auto& [label, color_ptr] : text_colors) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s:", label);

            ImGui::TableNextColumn();
            ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
            std::string id = absl::StrFormat("##%s", label);
            if (ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
              *color_ptr = {color_vec.x, color_vec.y, color_vec.z, color_vec.w};
              apply_live_preview();
            }

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, color_vec);
            ImGui::Text("Sample %s", label);
            ImGui::PopStyleColor();
          }

          ImGui::EndTable();
        }
        ImGui::EndTabItem();
      }

      // Interactive Elements Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Interactive", ICON_MD_TOUCH_APP).c_str())) {
        if (ImGui::BeginTable("InteractiveColorsTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                  0.4f);
          ImGui::TableHeadersRow();

          // Interactive element colors
          auto interactive_colors =
              std::vector<std::tuple<const char*, Color*, ImGuiCol>>{
                  {"Button", &edit_theme.button, ImGuiCol_Button},
                  {"Button Hovered", &edit_theme.button_hovered,
                   ImGuiCol_ButtonHovered},
                  {"Button Active", &edit_theme.button_active,
                   ImGuiCol_ButtonActive},
                  {"Frame Background", &edit_theme.frame_bg, ImGuiCol_FrameBg},
                  {"Frame BG Hovered", &edit_theme.frame_bg_hovered,
                   ImGuiCol_FrameBgHovered},
                  {"Frame BG Active", &edit_theme.frame_bg_active,
                   ImGuiCol_FrameBgActive},
                  {"Check Mark", &edit_theme.check_mark, ImGuiCol_CheckMark},
                  {"Slider Grab", &edit_theme.slider_grab, ImGuiCol_SliderGrab},
                  {"Slider Grab Active", &edit_theme.slider_grab_active,
                   ImGuiCol_SliderGrabActive}};

          for (auto& [label, color_ptr, imgui_col] : interactive_colors) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s:", label);

            ImGui::TableNextColumn();
            ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
            std::string id = absl::StrFormat("##%s", label);
            if (ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
              *color_ptr = {color_vec.x, color_vec.y, color_vec.z, color_vec.w};
              apply_live_preview();
            }

            ImGui::TableNextColumn();
            ImGui::PushStyleColor(imgui_col, color_vec);
            ImGui::Button(absl::StrFormat("Preview %s", label).c_str(),
                          ImVec2(-1, 30));
            ImGui::PopStyleColor();
          }

          ImGui::EndTable();
        }
        ImGui::EndTabItem();
      }

      // Style Parameters Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Style", ICON_MD_TUNE).c_str())) {
        ImGui::Text("Rounding and Border Settings:");

        if (ImGui::SliderFloat("Window Rounding", &edit_theme.window_rounding,
                               0.0f, 20.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (ImGui::SliderFloat("Frame Rounding", &edit_theme.frame_rounding,
                               0.0f, 20.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (ImGui::SliderFloat("Scrollbar Rounding",
                               &edit_theme.scrollbar_rounding, 0.0f, 20.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (ImGui::SliderFloat("Tab Rounding", &edit_theme.tab_rounding, 0.0f,
                               20.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (ImGui::SliderFloat("Grab Rounding", &edit_theme.grab_rounding, 0.0f,
                               20.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }

        ImGui::Separator();
        ImGui::Text("Border Sizes:");

        if (ImGui::SliderFloat("Window Border Size",
                               &edit_theme.window_border_size, 0.0f, 3.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (ImGui::SliderFloat("Frame Border Size",
                               &edit_theme.frame_border_size, 0.0f, 3.0f)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }

        ImGui::Separator();
        ImGui::Text("Animation & Effects:");

        if (ImGui::Checkbox("Enable Animations",
                            &edit_theme.enable_animations)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }
        if (edit_theme.enable_animations) {
          if (ImGui::SliderFloat("Animation Speed", &edit_theme.animation_speed,
                                 0.1f, 3.0f)) {
            apply_live_preview();
          }
        }
        if (ImGui::Checkbox("Enable Glow Effects",
                            &edit_theme.enable_glow_effects)) {
          if (live_preview)
            ApplyTheme(edit_theme);
        }

        ImGui::EndTabItem();
      }

      // Navigation & Windows Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Navigation", ICON_MD_NAVIGATION).c_str())) {
        if (ImGui::BeginTable("NavigationTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch,
                                  0.4f);
          ImGui::TableHeadersRow();

          // Window colors
          auto window_colors =
              std::vector<std::tuple<const char*, Color*, const char*>>{
                  {"Window Background", &edit_theme.window_bg,
                   "Main window background"},
                  {"Child Background", &edit_theme.child_bg,
                   "Child window background"},
                  {"Popup Background", &edit_theme.popup_bg,
                   "Popup window background"},
                  {"Modal Background", &edit_theme.modal_bg,
                   "Modal window background"},
                  {"Menu Bar BG", &edit_theme.menu_bar_bg,
                   "Menu bar background"}};

          for (auto& [label, color_ptr, description] : window_colors) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s:", label);

            ImGui::TableNextColumn();
            ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
            std::string id = absl::StrFormat("##window_%s", label);
            if (ImGui::ColorEdit4(id.c_str(), &color_vec.x)) {
              *color_ptr = {color_vec.x, color_vec.y, color_vec.z, color_vec.w};
              apply_live_preview();
            }

            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", description);
          }

          ImGui::EndTable();
        }

        ImGui::Separator();

        // Header and Tab colors
        if (ImGui::CollapsingHeader("Headers & Tabs",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::BeginTable("HeaderTabTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Preview",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            auto header_tab_colors =
                std::vector<std::pair<const char*, Color*>>{
                    {"Header", &edit_theme.header},
                    {"Header Hovered", &edit_theme.header_hovered},
                    {"Header Active", &edit_theme.header_active},
                    {"Tab", &edit_theme.tab},
                    {"Tab Hovered", &edit_theme.tab_hovered},
                    {"Tab Active", &edit_theme.tab_active},
                    {"Tab Unfocused", &edit_theme.tab_unfocused},
                    {"Tab Unfocused Active", &edit_theme.tab_unfocused_active},
                    {"Tab Dimmed", &edit_theme.tab_dimmed},
                    {"Tab Dimmed Selected", &edit_theme.tab_dimmed_selected},
                    {"Title Background", &edit_theme.title_bg},
                    {"Title BG Active", &edit_theme.title_bg_active},
                    {"Title BG Collapsed", &edit_theme.title_bg_collapsed}};

            for (auto& [label, color_ptr] : header_tab_colors) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();
              ImGui::Text("%s:", label);

              ImGui::TableNextColumn();
              ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
              std::string id = absl::StrFormat("##header_%s", label);
              if (ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
                *color_ptr = {color_vec.x, color_vec.y, color_vec.z,
                              color_vec.w};
                apply_live_preview();
              }

              ImGui::TableNextColumn();
              ImGui::PushStyleColor(ImGuiCol_Button, color_vec);
              ImGui::Button(absl::StrFormat("Preview %s", label).c_str(),
                            ImVec2(-1, 25));
              ImGui::PopStyleColor();
            }

            ImGui::EndTable();
          }
        }

        // Navigation and Special Elements
        if (ImGui::CollapsingHeader("Navigation & Special")) {
          if (ImGui::BeginTable("NavSpecialTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Description",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            auto nav_special_colors =
                std::vector<std::tuple<const char*, Color*, const char*>>{
                    {"Nav Cursor", &edit_theme.nav_cursor,
                     "Navigation cursor color"},
                    {"Nav Win Highlight", &edit_theme.nav_windowing_highlight,
                     "Window selection highlight"},
                    {"Nav Win Dim BG", &edit_theme.nav_windowing_dim_bg,
                     "Background dimming for navigation"},
                    {"Modal Win Dim BG", &edit_theme.modal_window_dim_bg,
                     "Background dimming for modals"},
                    {"Drag Drop Target", &edit_theme.drag_drop_target,
                     "Drag and drop target highlight"},
                    {"Docking Preview", &edit_theme.docking_preview,
                     "Docking area preview"},
                    {"Docking Empty BG", &edit_theme.docking_empty_bg,
                     "Empty docking space background"}};

            for (auto& [label, color_ptr, description] : nav_special_colors) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();
              ImGui::Text("%s:", label);

              ImGui::TableNextColumn();
              ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
              std::string id = absl::StrFormat("##nav_%s", label);
              if (ImGui::ColorEdit4(id.c_str(), &color_vec.x)) {
                *color_ptr = {color_vec.x, color_vec.y, color_vec.z,
                              color_vec.w};
                apply_live_preview();
              }

              ImGui::TableNextColumn();
              ImGui::TextWrapped("%s", description);
            }

            ImGui::EndTable();
          }
        }

        ImGui::EndTabItem();
      }

      // Tables & Data Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Tables", ICON_MD_TABLE_CHART).c_str())) {
        if (ImGui::BeginTable("TablesDataTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Description",
                                  ImGuiTableColumnFlags_WidthStretch, 0.4f);
          ImGui::TableHeadersRow();

          auto table_colors =
              std::vector<std::tuple<const char*, Color*, const char*>>{
                  {"Table Header BG", &edit_theme.table_header_bg,
                   "Table column headers"},
                  {"Table Border Strong", &edit_theme.table_border_strong,
                   "Outer table borders"},
                  {"Table Border Light", &edit_theme.table_border_light,
                   "Inner table borders"},
                  {"Table Row BG", &edit_theme.table_row_bg,
                   "Normal table rows"},
                  {"Table Row BG Alt", &edit_theme.table_row_bg_alt,
                   "Alternating table rows"},
                  {"Tree Lines", &edit_theme.tree_lines,
                   "Tree view connection lines"}};

          for (auto& [label, color_ptr, description] : table_colors) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s:", label);

            ImGui::TableNextColumn();
            ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
            std::string id = absl::StrFormat("##table_%s", label);
            if (ImGui::ColorEdit4(id.c_str(), &color_vec.x)) {
              *color_ptr = {color_vec.x, color_vec.y, color_vec.z, color_vec.w};
              apply_live_preview();
            }

            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", description);
          }

          ImGui::EndTable();
        }

        ImGui::Separator();

        // Plots and Graphs
        if (ImGui::CollapsingHeader("Plots & Graphs")) {
          if (ImGui::BeginTable("PlotsTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Description",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            auto plot_colors =
                std::vector<std::tuple<const char*, Color*, const char*>>{
                    {"Plot Lines", &edit_theme.plot_lines, "Line plot color"},
                    {"Plot Lines Hovered", &edit_theme.plot_lines_hovered,
                     "Line plot hover color"},
                    {"Plot Histogram", &edit_theme.plot_histogram,
                     "Histogram fill color"},
                    {"Plot Histogram Hovered",
                     &edit_theme.plot_histogram_hovered,
                     "Histogram hover color"}};

            for (auto& [label, color_ptr, description] : plot_colors) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();
              ImGui::Text("%s:", label);

              ImGui::TableNextColumn();
              ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
              std::string id = absl::StrFormat("##plot_%s", label);
              if (ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
                *color_ptr = {color_vec.x, color_vec.y, color_vec.z,
                              color_vec.w};
                apply_live_preview();
              }

              ImGui::TableNextColumn();
              ImGui::TextWrapped("%s", description);
            }

            ImGui::EndTable();
          }
        }

        ImGui::EndTabItem();
      }

      // Borders & Controls Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Borders", ICON_MD_BORDER_ALL).c_str())) {
        if (ImGui::BeginTable("BordersControlsTable", 3,
                              ImGuiTableFlags_SizingStretchProp)) {
          ImGui::TableSetupColumn("Element", ImGuiTableColumnFlags_WidthFixed,
                                  120.0f);
          ImGui::TableSetupColumn("Picker", ImGuiTableColumnFlags_WidthStretch,
                                  0.6f);
          ImGui::TableSetupColumn("Description",
                                  ImGuiTableColumnFlags_WidthStretch, 0.4f);
          ImGui::TableHeadersRow();

          auto border_control_colors =
              std::vector<std::tuple<const char*, Color*, const char*>>{
                  {"Border", &edit_theme.border, "General border color"},
                  {"Border Shadow", &edit_theme.border_shadow,
                   "Border shadow/depth"},
                  {"Separator", &edit_theme.separator,
                   "Horizontal/vertical separators"},
                  {"Separator Hovered", &edit_theme.separator_hovered,
                   "Separator hover state"},
                  {"Separator Active", &edit_theme.separator_active,
                   "Separator active/dragged state"},
                  {"Scrollbar BG", &edit_theme.scrollbar_bg,
                   "Scrollbar track background"},
                  {"Scrollbar Grab", &edit_theme.scrollbar_grab,
                   "Scrollbar handle"},
                  {"Scrollbar Grab Hovered", &edit_theme.scrollbar_grab_hovered,
                   "Scrollbar handle hover"},
                  {"Scrollbar Grab Active", &edit_theme.scrollbar_grab_active,
                   "Scrollbar handle active"},
                  {"Resize Grip", &edit_theme.resize_grip,
                   "Window resize grip"},
                  {"Resize Grip Hovered", &edit_theme.resize_grip_hovered,
                   "Resize grip hover"},
                  {"Resize Grip Active", &edit_theme.resize_grip_active,
                   "Resize grip active"}};

          for (auto& [label, color_ptr, description] : border_control_colors) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s:", label);

            ImGui::TableNextColumn();
            ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
            std::string id = absl::StrFormat("##border_%s", label);
            if (ImGui::ColorEdit4(id.c_str(), &color_vec.x)) {
              *color_ptr = {color_vec.x, color_vec.y, color_vec.z, color_vec.w};
              apply_live_preview();
            }

            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", description);
          }

          ImGui::EndTable();
        }

        ImGui::EndTabItem();
      }

      // Enhanced Colors Tab
      if (ImGui::BeginTabItem(
              absl::StrFormat("%s Enhanced", ICON_MD_AUTO_AWESOME).c_str())) {
        ImGui::Text(
            "Enhanced semantic colors and editor-specific customization");
        ImGui::Separator();

        // Enhanced semantic colors section
        if (ImGui::CollapsingHeader("Enhanced Semantic Colors",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          if (ImGui::BeginTable("EnhancedSemanticTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Description",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            auto enhanced_colors =
                std::vector<std::tuple<const char*, Color*, const char*>>{
                    {"Code Background", &edit_theme.code_background,
                     "Code blocks background"},
                    {"Success Light", &edit_theme.success_light,
                     "Light success variant"},
                    {"Warning Light", &edit_theme.warning_light,
                     "Light warning variant"},
                    {"Error Light", &edit_theme.error_light,
                     "Light error variant"},
                    {"Info Light", &edit_theme.info_light,
                     "Light info variant"}};

            for (auto& [label, color_ptr, description] : enhanced_colors) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();
              ImGui::Text("%s:", label);

              ImGui::TableNextColumn();
              ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
              std::string id = absl::StrFormat("##enhanced_%s", label);
              if (ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
                *color_ptr = {color_vec.x, color_vec.y, color_vec.z,
                              color_vec.w};
                apply_live_preview();
              }

              ImGui::TableNextColumn();
              ImGui::TextWrapped("%s", description);
            }

            ImGui::EndTable();
          }
        }

        // UI State colors section
        if (ImGui::CollapsingHeader("UI State Colors")) {
          if (ImGui::BeginTable("UIStateTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Description",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            // UI state colors with alpha support where needed
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Active Selection:");
            ImGui::TableNextColumn();
            ImVec4 active_selection =
                ConvertColorToImVec4(edit_theme.active_selection);
            if (ImGui::ColorEdit4("##active_selection", &active_selection.x)) {
              edit_theme.active_selection = {
                  active_selection.x, active_selection.y, active_selection.z,
                  active_selection.w};
              apply_live_preview();
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("Active/selected UI elements");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Hover Highlight:");
            ImGui::TableNextColumn();
            ImVec4 hover_highlight =
                ConvertColorToImVec4(edit_theme.hover_highlight);
            if (ImGui::ColorEdit4("##hover_highlight", &hover_highlight.x)) {
              edit_theme.hover_highlight = {
                  hover_highlight.x, hover_highlight.y, hover_highlight.z,
                  hover_highlight.w};
              apply_live_preview();
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("General hover state highlighting");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Focus Border:");
            ImGui::TableNextColumn();
            ImVec4 focus_border = ConvertColorToImVec4(edit_theme.focus_border);
            if (ImGui::ColorEdit3("##focus_border", &focus_border.x)) {
              edit_theme.focus_border = {focus_border.x, focus_border.y,
                                         focus_border.z, focus_border.w};
              apply_live_preview();
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped("Border for focused input elements");

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Disabled Overlay:");
            ImGui::TableNextColumn();
            ImVec4 disabled_overlay =
                ConvertColorToImVec4(edit_theme.disabled_overlay);
            if (ImGui::ColorEdit4("##disabled_overlay", &disabled_overlay.x)) {
              edit_theme.disabled_overlay = {
                  disabled_overlay.x, disabled_overlay.y, disabled_overlay.z,
                  disabled_overlay.w};
              apply_live_preview();
            }
            ImGui::TableNextColumn();
            ImGui::TextWrapped(
                "Semi-transparent overlay for disabled elements");

            ImGui::EndTable();
          }
        }

        // Editor-specific colors section
        if (ImGui::CollapsingHeader("Editor-Specific Colors")) {
          if (ImGui::BeginTable("EditorColorsTable", 3,
                                ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed,
                                    120.0f);
            ImGui::TableSetupColumn("Picker",
                                    ImGuiTableColumnFlags_WidthStretch, 0.6f);
            ImGui::TableSetupColumn("Description",
                                    ImGuiTableColumnFlags_WidthStretch, 0.4f);
            ImGui::TableHeadersRow();

            auto editor_colors =
                std::vector<std::tuple<const char*, Color*, const char*, bool>>{
                    {"Editor Background", &edit_theme.editor_background,
                     "Main editor canvas background", false},
                    {"Editor Grid", &edit_theme.editor_grid,
                     "Grid lines in map/graphics editors", true},
                    {"Editor Cursor", &edit_theme.editor_cursor,
                     "Cursor color in editors", false},
                    {"Editor Selection", &edit_theme.editor_selection,
                     "Selection highlight in editors", true}};

            for (auto& [label, color_ptr, description, use_alpha] :
                 editor_colors) {
              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();
              ImGui::Text("%s:", label);

              ImGui::TableNextColumn();
              ImVec4 color_vec = ConvertColorToImVec4(*color_ptr);
              std::string id = absl::StrFormat("##editor_%s", label);
              if (use_alpha ? ImGui::ColorEdit4(id.c_str(), &color_vec.x)
                            : ImGui::ColorEdit3(id.c_str(), &color_vec.x)) {
                *color_ptr = {color_vec.x, color_vec.y, color_vec.z,
                              color_vec.w};
                apply_live_preview();
              }

              ImGui::TableNextColumn();
              ImGui::TextWrapped("%s", description);
            }

            ImGui::EndTable();
          }
        }

        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }

    ImGui::Separator();

    if (ImGui::Button("Preview Theme")) {
      ApplyTheme(edit_theme);
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset to Current")) {
      edit_theme = current_theme_;
      // Safe string copy with bounds checking
      size_t name_len =
          std::min(current_theme_.name.length(), sizeof(theme_name) - 1);
      std::memcpy(theme_name, current_theme_.name.c_str(), name_len);
      theme_name[name_len] = '\0';

      size_t desc_len = std::min(current_theme_.description.length(),
                                 sizeof(theme_description) - 1);
      std::memcpy(theme_description, current_theme_.description.c_str(),
                  desc_len);
      theme_description[desc_len] = '\0';

      size_t author_len =
          std::min(current_theme_.author.length(), sizeof(theme_author) - 1);
      std::memcpy(theme_author, current_theme_.author.c_str(), author_len);
      theme_author[author_len] = '\0';

      // Reset backup state since we're back to current theme
      if (theme_backup_made) {
        theme_backup_made = false;
        current_theme_
            .ApplyToImGui();  // Apply current theme to clear any preview changes
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Theme")) {
      edit_theme.name = std::string(theme_name);
      edit_theme.description = std::string(theme_description);
      edit_theme.author = std::string(theme_author);

      // Add to themes map and apply
      themes_[edit_theme.name] = edit_theme;
      ApplyTheme(edit_theme);

      // Reset backup state since theme is now applied
      theme_backup_made = false;
    }

    ImGui::SameLine();

    // Save Over Current button - overwrites the current theme file
    std::string current_file_path = GetCurrentThemeFilePath();
    bool can_save_over = !current_file_path.empty();

    if (!can_save_over) {
      ImGui::BeginDisabled();
    }

    if (ImGui::Button("Save Over Current")) {
      edit_theme.name = std::string(theme_name);
      edit_theme.description = std::string(theme_description);
      edit_theme.author = std::string(theme_author);

      auto status = SaveThemeToFile(edit_theme, current_file_path);
      if (status.ok()) {
        // Update themes map and apply
        themes_[edit_theme.name] = edit_theme;
        ApplyTheme(edit_theme);
        theme_backup_made =
            false;  // Reset backup state since theme is now applied
      } else {
        LOG_ERROR("Theme Manager", "Failed to save over current theme");
      }
    }

    if (!can_save_over) {
      ImGui::EndDisabled();
    }

    if (ImGui::IsItemHovered() && can_save_over) {
      ImGui::BeginTooltip();
      ImGui::Text("Save over current theme file:");
      ImGui::Text("%s", current_file_path.c_str());
      ImGui::EndTooltip();
    } else if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("No current theme file to overwrite");
      ImGui::Text("Use 'Save to File...' to create a new theme file");
      ImGui::EndTooltip();
    }

    ImGui::SameLine();
    if (ImGui::Button("Save to File...")) {
      edit_theme.name = std::string(theme_name);
      edit_theme.description = std::string(theme_description);
      edit_theme.author = std::string(theme_author);

      // Use save file dialog with proper defaults
      std::string safe_name =
          edit_theme.name.empty() ? "custom_theme" : edit_theme.name;
      auto file_path =
          util::FileDialogWrapper::ShowSaveFileDialog(safe_name, "theme");

      if (!file_path.empty()) {
        // Ensure .theme extension
        if (file_path.find(".theme") == std::string::npos) {
          file_path += ".theme";
        }

        auto status = SaveThemeToFile(edit_theme, file_path);
        if (status.ok()) {
          // Also add to themes map for immediate use
          themes_[edit_theme.name] = edit_theme;
          ApplyTheme(edit_theme);
        } else {
          LOG_ERROR("Theme Manager", "Failed to save theme");
        }
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Save theme to a .theme file");
      ImGui::Text("Saved themes can be shared and loaded later");
      ImGui::EndTooltip();
    }
  }
  ImGui::End();
}

std::vector<std::string> ThemeManager::GetThemeSearchPaths() const {
  std::vector<std::string> search_paths;

  // Development path (relative to build directory)
  search_paths.push_back("assets/themes/");
  search_paths.push_back("../assets/themes/");

  // Platform-specific resource paths
#ifdef __APPLE__
  // macOS bundle resource path (this should be the primary path for bundled apps)
  std::string bundle_themes = util::GetResourcePath("assets/themes/");
  if (!bundle_themes.empty()) {
    search_paths.push_back(bundle_themes);
  }

  // Alternative bundle locations
  std::string bundle_root = util::GetBundleResourcePath();

  search_paths.push_back(bundle_root + "Contents/Resources/themes/");
  search_paths.push_back(bundle_root + "Contents/Resources/assets/themes/");
  search_paths.push_back(bundle_root + "assets/themes/");
  search_paths.push_back(bundle_root + "themes/");
#else
  // Linux/Windows relative paths
  search_paths.push_back("./assets/themes/");
  search_paths.push_back("./themes/");
#endif

  // User config directory
  auto config_dir = util::PlatformPaths::GetConfigDirectory();
  if (config_dir.ok()) {
    search_paths.push_back((*config_dir / "themes/").string());
  }

  return search_paths;
}

std::string ThemeManager::GetThemesDirectory() const {
  auto search_paths = GetThemeSearchPaths();

  // Try each search path and return the first one that exists
  for (const auto& path : search_paths) {
    std::ifstream test_file(
        path + ".");  // Test if directory exists by trying to access it
    if (test_file.good()) {
      return path;
    }

    // Also try with platform-specific directory separators
    std::string normalized_path = path;
    if (!normalized_path.empty() && normalized_path.back() != '/' &&
        normalized_path.back() != '\\') {
      normalized_path += "/";
    }

    std::ifstream test_file2(normalized_path + ".");
    if (test_file2.good()) {
      return normalized_path;
    }
  }

  return search_paths.empty() ? "assets/themes/" : search_paths[0];
}

std::vector<std::string> ThemeManager::DiscoverAvailableThemeFiles() const {
  std::vector<std::string> theme_files;
  auto search_paths = GetThemeSearchPaths();

  for (const auto& search_path : search_paths) {

    try {
      // Use platform-specific file discovery instead of glob
#ifdef __APPLE__
      auto files_in_folder =
          util::FileDialogWrapper::GetFilesInFolder(search_path);
      for (const auto& file : files_in_folder) {
        if (file.length() > 6 && file.substr(file.length() - 6) == ".theme") {
          std::string full_path = search_path + file;
          theme_files.push_back(full_path);
        }
      }
#else
      // For Linux/Windows, use filesystem directory iteration
      // (could be extended with platform-specific implementations if needed)
      std::vector<std::string> known_themes = {
          "yaze_tre.theme", "cyberpunk.theme", "sunset.theme", "forest.theme",
          "midnight.theme"};

      for (const auto& theme_name : known_themes) {
        std::string full_path = search_path + theme_name;
        std::ifstream test_file(full_path);
        if (test_file.good()) {
          theme_files.push_back(full_path);
        }
      }
#endif
    } catch (const std::exception& e) {
      LOG_ERROR("Theme Manager", "Error scanning directory %s",
                search_path.c_str());
    }
  }

  // Remove duplicates while preserving order
  std::vector<std::string> unique_files;
  std::set<std::string> seen_basenames;

  for (const auto& file : theme_files) {
    std::string basename = util::GetFileName(file);
    if (seen_basenames.find(basename) == seen_basenames.end()) {
      unique_files.push_back(file);
      seen_basenames.insert(basename);
    }
  }

  return unique_files;
}

absl::Status ThemeManager::LoadAllAvailableThemes() {
  auto theme_files = DiscoverAvailableThemeFiles();

  int successful_loads = 0;
  int failed_loads = 0;

  for (const auto& theme_file : theme_files) {
    auto status = LoadThemeFromFile(theme_file);
    if (status.ok()) {
      successful_loads++;
    } else {
      failed_loads++;
    }
  }

  if (successful_loads == 0 && failed_loads > 0) {
    return absl::InternalError(absl::StrFormat(
        "Failed to load any themes (%d failures)", failed_loads));
  }

  return absl::OkStatus();
}

absl::Status ThemeManager::RefreshAvailableThemes() {
  return LoadAllAvailableThemes();
}

std::string ThemeManager::GetCurrentThemeFilePath() const {
  if (current_theme_name_ == "Classic YAZE") {
    return "";  // Classic theme doesn't have a file
  }

  // Try to find the current theme file in the search paths
  auto search_paths = GetThemeSearchPaths();
  std::string theme_filename = current_theme_name_ + ".theme";

  // Convert theme name to safe filename (replace spaces and special chars)
  for (char& c : theme_filename) {
    if (!std::isalnum(c) && c != '.' && c != '_') {
      c = '_';
    }
  }

  for (const auto& search_path : search_paths) {
    std::string full_path = search_path + theme_filename;
    std::ifstream test_file(full_path);
    if (test_file.good()) {
      return full_path;
    }
  }

  // If not found, return path in the first search directory (for new saves)
  return search_paths.empty() ? theme_filename
                              : search_paths[0] + theme_filename;
}

}  // namespace gui
}  // namespace yaze
