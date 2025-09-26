#include "theme_manager.h"

#include <cctype>
#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/core/platform/file_dialog.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"  // For ColorsYaze function
#include "imgui/imgui.h"
#include "util/log.h"

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
  colors[ImGuiCol_ScrollbarGrabHovered] = ConvertColorToImVec4(scrollbar_grab_hovered);
  colors[ImGuiCol_ScrollbarGrabActive] = ConvertColorToImVec4(scrollbar_grab_active);
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
  colors[ImGuiCol_ResizeGripHovered] = ConvertColorToImVec4(resize_grip_hovered);
  colors[ImGuiCol_ResizeGripActive] = ConvertColorToImVec4(resize_grip_active);
  colors[ImGuiCol_Tab] = ConvertColorToImVec4(tab);
  colors[ImGuiCol_TabHovered] = ConvertColorToImVec4(tab_hovered);
  colors[ImGuiCol_TabSelected] = ConvertColorToImVec4(tab_active);
  colors[ImGuiCol_DockingPreview] = ConvertColorToImVec4(docking_preview);
  colors[ImGuiCol_DockingEmptyBg] = ConvertColorToImVec4(docking_empty_bg);
  
  // Complete ImGui color support
  colors[ImGuiCol_CheckMark] = ConvertColorToImVec4(check_mark);
  colors[ImGuiCol_SliderGrab] = ConvertColorToImVec4(slider_grab);
  colors[ImGuiCol_SliderGrabActive] = ConvertColorToImVec4(slider_grab_active);
  colors[ImGuiCol_InputTextCursor] = ConvertColorToImVec4(input_text_cursor);
  colors[ImGuiCol_NavCursor] = ConvertColorToImVec4(nav_cursor);
  colors[ImGuiCol_NavWindowingHighlight] = ConvertColorToImVec4(nav_windowing_highlight);
  colors[ImGuiCol_NavWindowingDimBg] = ConvertColorToImVec4(nav_windowing_dim_bg);
  colors[ImGuiCol_ModalWindowDimBg] = ConvertColorToImVec4(modal_window_dim_bg);
  colors[ImGuiCol_TextSelectedBg] = ConvertColorToImVec4(text_selected_bg);
  colors[ImGuiCol_DragDropTarget] = ConvertColorToImVec4(drag_drop_target);
  colors[ImGuiCol_TableHeaderBg] = ConvertColorToImVec4(table_header_bg);
  colors[ImGuiCol_TableBorderStrong] = ConvertColorToImVec4(table_border_strong);
  colors[ImGuiCol_TableBorderLight] = ConvertColorToImVec4(table_border_light);
  colors[ImGuiCol_TableRowBg] = ConvertColorToImVec4(table_row_bg);
  colors[ImGuiCol_TableRowBgAlt] = ConvertColorToImVec4(table_row_bg_alt);
  colors[ImGuiCol_TextLink] = ConvertColorToImVec4(text_link);
  colors[ImGuiCol_PlotLines] = ConvertColorToImVec4(plot_lines);
  colors[ImGuiCol_PlotLinesHovered] = ConvertColorToImVec4(plot_lines_hovered);
  colors[ImGuiCol_PlotHistogram] = ConvertColorToImVec4(plot_histogram);
  colors[ImGuiCol_PlotHistogramHovered] = ConvertColorToImVec4(plot_histogram_hovered);
  
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
  
  // Try to load themes from files (will override fallback if successful)
  std::vector<std::string> theme_files = {
    "yaze_tre.theme",
    "cyberpunk.theme", 
    "sunset.theme",
    "forest.theme",
    "midnight.theme"
  };
  
  for (const auto& theme_file : theme_files) {
    auto status = LoadThemeFromFile(theme_file);
    if (!status.ok()) {
      util::logf("Failed to load theme file %s: %s", theme_file.c_str(), status.message().data());
    }
  }
  
  // Ensure we have a valid current theme (prefer file-based theme)
  if (themes_.find("YAZE Classic") != themes_.end()) {
    current_theme_ = themes_["YAZE Classic"];
    current_theme_name_ = "YAZE Classic";
  } else if (themes_.find("YAZE Tre") != themes_.end()) {
    current_theme_ = themes_["YAZE Tre"];
    current_theme_name_ = "YAZE Tre";
  }
}

void ThemeManager::CreateFallbackYazeClassic() {
  // Fallback theme that matches the original ColorsYaze() function colors but in theme format
  EnhancedTheme theme;
  theme.name = "YAZE Tre";
  theme.description = "YAZE theme resource edition";
  theme.author = "YAZE Team";
  
  // Use the exact original ColorsYaze colors
  theme.primary = RGBA(92, 115, 92);          // allttpLightGreen
  theme.secondary = RGBA(71, 92, 71);         // alttpMidGreen  
  theme.accent = RGBA(89, 119, 89);           // TabActive
  theme.background = RGBA(8, 8, 8);           // Very dark gray for better grid visibility
  
  theme.text_primary = RGBA(230, 230, 230);   // 0.90f, 0.90f, 0.90f
  theme.text_disabled = RGBA(153, 153, 153);  // 0.60f, 0.60f, 0.60f
  theme.window_bg = RGBA(8, 8, 8, 217);       // Very dark gray with same alpha
  theme.child_bg = RGBA(0, 0, 0, 0);          // Transparent
  theme.popup_bg = RGBA(28, 28, 36, 235);     // 0.11f, 0.11f, 0.14f, 0.92f
  
  theme.button = RGBA(71, 92, 71);            // alttpMidGreen
  theme.button_hovered = RGBA(125, 146, 125); // allttpLightestGreen
  theme.button_active = RGBA(92, 115, 92);    // allttpLightGreen
  
  theme.header = RGBA(46, 66, 46);            // alttpDarkGreen
  theme.header_hovered = RGBA(92, 115, 92);   // allttpLightGreen
  theme.header_active = RGBA(71, 92, 71);     // alttpMidGreen
  
  theme.menu_bar_bg = RGBA(46, 66, 46);       // alttpDarkGreen
  theme.tab = RGBA(46, 66, 46);               // alttpDarkGreen
  theme.tab_hovered = RGBA(71, 92, 71);       // alttpMidGreen
  theme.tab_active = RGBA(89, 119, 89);       // TabActive
  
  // Complete all remaining ImGui colors from original ColorsYaze() function
  theme.title_bg = RGBA(71, 92, 71);          // alttpMidGreen
  theme.title_bg_active = RGBA(46, 66, 46);   // alttpDarkGreen
  theme.title_bg_collapsed = RGBA(71, 92, 71); // alttpMidGreen
  
  // Borders and separators
  theme.border = RGBA(92, 115, 92);           // allttpLightGreen
  theme.border_shadow = RGBA(0, 0, 0, 0);     // Transparent
  theme.separator = RGBA(128, 128, 128, 153); // 0.50f, 0.50f, 0.50f, 0.60f
  theme.separator_hovered = RGBA(153, 153, 178); // 0.60f, 0.60f, 0.70f
  theme.separator_active = RGBA(178, 178, 230);  // 0.70f, 0.70f, 0.90f
  
  // Scrollbars
  theme.scrollbar_bg = RGBA(92, 115, 92, 153);    // 0.36f, 0.45f, 0.36f, 0.60f
  theme.scrollbar_grab = RGBA(92, 115, 92, 76);   // 0.36f, 0.45f, 0.36f, 0.30f
  theme.scrollbar_grab_hovered = RGBA(92, 115, 92, 102); // 0.36f, 0.45f, 0.36f, 0.40f
  theme.scrollbar_grab_active = RGBA(92, 115, 92, 153);  // 0.36f, 0.45f, 0.36f, 0.60f
  
  // Resize grips (from original - light blue highlights)
  theme.resize_grip = RGBA(255, 255, 255, 26);      // 1.00f, 1.00f, 1.00f, 0.10f
  theme.resize_grip_hovered = RGBA(199, 209, 255, 153); // 0.78f, 0.82f, 1.00f, 0.60f
  theme.resize_grip_active = RGBA(199, 209, 255, 230);  // 0.78f, 0.82f, 1.00f, 0.90f
  
  // Complete ImGui colors with smart defaults using accent colors
  theme.check_mark = RGBA(230, 230, 230, 128);      // 0.90f, 0.90f, 0.90f, 0.50f
  theme.slider_grab = RGBA(255, 255, 255, 77);      // 1.00f, 1.00f, 1.00f, 0.30f
  theme.slider_grab_active = RGBA(92, 115, 92, 153); // Same as scrollbar for consistency
  theme.input_text_cursor = theme.text_primary;      // Use primary text color
  theme.nav_cursor = theme.accent;                   // Use accent color for navigation
  theme.nav_windowing_highlight = theme.accent;      // Accent for window switching
  theme.nav_windowing_dim_bg = RGBA(0, 0, 0, 128);  // Semi-transparent overlay
  theme.modal_window_dim_bg = RGBA(0, 0, 0, 89);    // 0.35f alpha
  theme.text_selected_bg = RGBA(89, 119, 89, 89);   // Accent color with transparency
  theme.drag_drop_target = theme.accent;             // Use accent for drag targets
  
  // Table colors (from original)
  theme.table_header_bg = RGBA(46, 66, 46);         // alttpDarkGreen
  theme.table_border_strong = RGBA(71, 92, 71);     // alttpMidGreen  
  theme.table_border_light = RGBA(66, 66, 71);      // 0.26f, 0.26f, 0.28f
  theme.table_row_bg = RGBA(0, 0, 0, 0);            // Transparent
  theme.table_row_bg_alt = RGBA(255, 255, 255, 18); // 1.00f, 1.00f, 1.00f, 0.07f
  
  // Links and plots - use accent colors intelligently
  theme.text_link = theme.accent;                   // Accent for links
  theme.plot_lines = RGBA(255, 255, 255);          // White for plots
  theme.plot_lines_hovered = RGBA(230, 178, 0);    // 0.90f, 0.70f, 0.00f
  theme.plot_histogram = RGBA(230, 178, 0);        // Same as above
  theme.plot_histogram_hovered = RGBA(255, 153, 0); // 1.00f, 0.60f, 0.00f
  
  // Docking colors
  theme.docking_preview = RGBA(92, 115, 92, 180);   // Light green with transparency
  theme.docking_empty_bg = RGBA(46, 66, 46, 255);   // Dark green
  
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
    return absl::NotFoundError(absl::StrFormat("Theme '%s' not found", theme_name));
  }
  
  current_theme_ = it->second;
  current_theme_name_ = theme_name;
  current_theme_.ApplyToImGui();
  
  return absl::OkStatus();
}

absl::Status ThemeManager::LoadThemeFromFile(const std::string& filepath) {
  // Try multiple possible paths where theme files might be located
  std::vector<std::string> possible_paths = {
    filepath,                                    // Absolute path
    "assets/themes/" + filepath,                 // Relative from build dir  
    "../assets/themes/" + filepath,              // Relative from bin dir
    core::GetResourcePath("assets/themes/" + filepath), // Platform-specific resource path
  };
  
  std::ifstream file;
  std::string successful_path;
  
  for (const auto& path : possible_paths) {
    util::logf("Trying to open theme file: %s", path.c_str());
    file.open(path);
    if (file.is_open()) {
      successful_path = path;
      util::logf("✅ Successfully opened theme file: %s", path.c_str());
      break;
    } else {
      util::logf("❌ Failed to open theme file: %s", path.c_str());
      file.clear(); // Clear any error flags before trying next path
    }
  }
  
  if (!file.is_open()) {
    return absl::InvalidArgumentError(absl::StrFormat("Cannot open theme file: %s (tried %zu paths)", 
                                                      filepath, possible_paths.size()));
  }
  
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  file.close();
  
  if (content.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat("Theme file is empty: %s", successful_path));
  }
  
  EnhancedTheme theme;
  auto parse_status = ParseThemeFile(content, theme);
  if (!parse_status.ok()) {
    return absl::InvalidArgumentError(absl::StrFormat("Failed to parse theme file %s: %s", 
                                                     successful_path, parse_status.message()));
  }
  
  if (theme.name.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat("Theme file missing name: %s", successful_path));
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
      util::logf("Failed to load fallback theme: %s", fallback_status.message().data());
    }
  }
}

void ThemeManager::ApplyTheme(const EnhancedTheme& theme) {
  current_theme_ = theme;
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
  if (!p_open || !*p_open) return;
  
  if (ImGui::Begin(absl::StrFormat("%s Theme Selector", ICON_MD_PALETTE).c_str(), p_open)) {
    ImGui::Text("%s Available Themes", ICON_MD_COLOR_LENS);
    ImGui::Separator();
    
    // Add Classic YAZE button first (direct ColorsYaze() application)
    bool is_classic_active = (current_theme_name_ == "Classic YAZE");
    if (is_classic_active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.36f, 0.45f, 0.36f, 1.0f)); // allttpLightGreen
    }
    
    if (ImGui::Button(absl::StrFormat("%s YAZE Classic (Original)", 
                                     is_classic_active ? ICON_MD_CHECK : ICON_MD_STAR).c_str(), 
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
        ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.accent));
      }
      
      if (ImGui::Button(absl::StrFormat("%s %s", 
                                       is_current ? ICON_MD_CHECK : ICON_MD_CIRCLE,
                                       name.c_str()).c_str(), ImVec2(-1, 40))) {
        auto status = LoadTheme(name); // Use LoadTheme instead of ApplyTheme to ensure correct tracking
        if (!status.ok()) {
          util::logf("Failed to load theme %s: %s", name.c_str(), status.message().data());
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
      ImGui::ColorButton(absl::StrFormat("##secondary_%s", name.c_str()).c_str(), 
                        ConvertColorToImVec4(theme.secondary), 
                        ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
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
    if (ImGui::Button(absl::StrFormat("%s Load Custom Theme", ICON_MD_FOLDER_OPEN).c_str())) {
      auto file_path = core::FileDialogWrapper::ShowOpenFileDialog();
      if (!file_path.empty()) {
        auto status = LoadThemeFromFile(file_path);
        if (!status.ok()) {
          // Show error toast (would need access to toast manager)
        }
      }
    }
    
    ImGui::SameLine();
    static bool show_simple_editor = false;
    if (ImGui::Button(absl::StrFormat("%s Theme Editor", ICON_MD_EDIT).c_str())) {
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

absl::Status ThemeManager::ParseThemeFile(const std::string& content, EnhancedTheme& theme) {
  std::istringstream stream(content);
  std::string line;
  std::string current_section = "";
  
  while (std::getline(stream, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') continue;
    
    // Check for section headers [section_name]
    if (line[0] == '[' && line.back() == ']') {
      current_section = line.substr(1, line.length() - 2);
      continue;
    }
    
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) continue;
    
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
      
      if (key == "primary") theme.primary = color;
      else if (key == "secondary") theme.secondary = color;
      else if (key == "accent") theme.accent = color;
      else if (key == "background") theme.background = color;
      else if (key == "surface") theme.surface = color;
      else if (key == "error") theme.error = color;
      else if (key == "warning") theme.warning = color;
      else if (key == "success") theme.success = color;
      else if (key == "info") theme.info = color;
      else if (key == "text_primary") theme.text_primary = color;
      else if (key == "text_secondary") theme.text_secondary = color;
      else if (key == "text_disabled") theme.text_disabled = color;
      else if (key == "window_bg") theme.window_bg = color;
      else if (key == "child_bg") theme.child_bg = color;
      else if (key == "popup_bg") theme.popup_bg = color;
      else if (key == "button") theme.button = color;
      else if (key == "button_hovered") theme.button_hovered = color;
      else if (key == "button_active") theme.button_active = color;
      else if (key == "frame_bg") theme.frame_bg = color;
      else if (key == "frame_bg_hovered") theme.frame_bg_hovered = color;
      else if (key == "frame_bg_active") theme.frame_bg_active = color;
      else if (key == "header") theme.header = color;
      else if (key == "header_hovered") theme.header_hovered = color;
      else if (key == "header_active") theme.header_active = color;
      else if (key == "tab") theme.tab = color;
      else if (key == "tab_hovered") theme.tab_hovered = color;
      else if (key == "tab_active") theme.tab_active = color;
      else if (key == "menu_bar_bg") theme.menu_bar_bg = color;
      else if (key == "title_bg") theme.title_bg = color;
      else if (key == "title_bg_active") theme.title_bg_active = color;
      else if (key == "title_bg_collapsed") theme.title_bg_collapsed = color;
      else if (key == "separator") theme.separator = color;
      else if (key == "separator_hovered") theme.separator_hovered = color;
      else if (key == "separator_active") theme.separator_active = color;
      else if (key == "scrollbar_bg") theme.scrollbar_bg = color;
      else if (key == "scrollbar_grab") theme.scrollbar_grab = color;
      else if (key == "scrollbar_grab_hovered") theme.scrollbar_grab_hovered = color;
      else if (key == "scrollbar_grab_active") theme.scrollbar_grab_active = color;
      else if (key == "border") theme.border = color;
      else if (key == "border_shadow") theme.border_shadow = color;
      else if (key == "resize_grip") theme.resize_grip = color;
      else if (key == "resize_grip_hovered") theme.resize_grip_hovered = color;
      else if (key == "resize_grip_active") theme.resize_grip_active = color;
      // Note: Additional colors like check_mark, slider_grab, table colors
      // are handled by the fallback or can be added to EnhancedTheme struct as needed
    }
    else if (current_section == "style") {
      if (key == "window_rounding") theme.window_rounding = std::stof(value);
      else if (key == "frame_rounding") theme.frame_rounding = std::stof(value);
      else if (key == "scrollbar_rounding") theme.scrollbar_rounding = std::stof(value);
      else if (key == "grab_rounding") theme.grab_rounding = std::stof(value);
      else if (key == "tab_rounding") theme.tab_rounding = std::stof(value);
      else if (key == "window_border_size") theme.window_border_size = std::stof(value);
      else if (key == "frame_border_size") theme.frame_border_size = std::stof(value);
      else if (key == "enable_animations") theme.enable_animations = (value == "true");
      else if (key == "enable_glow_effects") theme.enable_glow_effects = (value == "true");
      else if (key == "animation_speed") theme.animation_speed = std::stof(value);
    }
    else if (current_section == "" || current_section == "metadata") {
      // Top-level metadata
      if (key == "name") theme.name = value;
      else if (key == "description") theme.description = value;
      else if (key == "author") theme.author = value;
    }
  }
  
  return absl::OkStatus();
}

Color ThemeManager::ParseColorFromString(const std::string& color_str) const {
  std::vector<std::string> components = absl::StrSplit(color_str, ',');
  if (components.size() != 4) {
    return RGBA(255, 255, 255, 255); // White fallback
  }
  
  try {
    int r = std::stoi(components[0]);
    int g = std::stoi(components[1]);
    int b = std::stoi(components[2]);
    int a = std::stoi(components[3]);
    return RGBA(r, g, b, a);
  } catch (...) {
    return RGBA(255, 255, 255, 255); // White fallback
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
    return std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "," + std::to_string(a);
  };
  
  ss << "# YAZE Theme File\n";
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
  ss << "title_bg_collapsed=" << colorToString(theme.title_bg_collapsed) << "\n";
  ss << "\n";
  
  // Borders and separators
  ss << "# Borders and separators\n";
  ss << "border=" << colorToString(theme.border) << "\n";
  ss << "border_shadow=" << colorToString(theme.border_shadow) << "\n";
  ss << "separator=" << colorToString(theme.separator) << "\n";
  ss << "separator_hovered=" << colorToString(theme.separator_hovered) << "\n";
  ss << "separator_active=" << colorToString(theme.separator_active) << "\n";
  ss << "\n";
  
  // Scrollbars
  ss << "# Scrollbars\n";
  ss << "scrollbar_bg=" << colorToString(theme.scrollbar_bg) << "\n";
  ss << "scrollbar_grab=" << colorToString(theme.scrollbar_grab) << "\n";
  ss << "scrollbar_grab_hovered=" << colorToString(theme.scrollbar_grab_hovered) << "\n";
  ss << "scrollbar_grab_active=" << colorToString(theme.scrollbar_grab_active) << "\n";
  ss << "\n";
  
  // Style settings
  ss << "[style]\n";
  ss << "window_rounding=" << theme.window_rounding << "\n";
  ss << "frame_rounding=" << theme.frame_rounding << "\n";
  ss << "scrollbar_rounding=" << theme.scrollbar_rounding << "\n";
  ss << "tab_rounding=" << theme.tab_rounding << "\n";
  ss << "enable_animations=" << (theme.enable_animations ? "true" : "false") << "\n";
  ss << "enable_glow_effects=" << (theme.enable_glow_effects ? "true" : "false") << "\n";
  
  return ss.str();
}

absl::Status ThemeManager::SaveThemeToFile(const EnhancedTheme& theme, const std::string& filepath) const {
  std::string theme_content = SerializeTheme(theme);
  
  std::ofstream file(filepath);
  if (!file.is_open()) {
    return absl::InternalError(absl::StrFormat("Failed to open file for writing: %s", filepath));
  }
  
  file << theme_content;
  file.close();
  
  if (file.fail()) {
    return absl::InternalError(absl::StrFormat("Failed to write theme file: %s", filepath));
  }
  
  util::logf("✅ Successfully saved theme '%s' to file: %s", theme.name.c_str(), filepath.c_str());
  return absl::OkStatus();
}

void ThemeManager::ApplyClassicYazeTheme() {
  // Apply the original ColorsYaze() function directly
  ColorsYaze();
  current_theme_name_ = "Classic YAZE";
  
  // Update current_theme_ to reflect the applied colors for consistency
  // (This creates a temporary theme object that matches what ColorsYaze() sets)
  EnhancedTheme classic_theme;
  classic_theme.name = "Classic YAZE";
  classic_theme.description = "Original YAZE theme (direct ColorsYaze() function)";
  classic_theme.author = "YAZE Team";
  
  // Extract the basic colors that ColorsYaze() sets (adjusted for grid visibility)
  classic_theme.primary = RGBA(92, 115, 92);     // allttpLightGreen 
  classic_theme.secondary = RGBA(71, 92, 71);    // alttpMidGreen
  classic_theme.accent = RGBA(89, 119, 89);      // TabActive color
  classic_theme.background = RGBA(8, 8, 8);      // Very dark gray for better grid visibility
  
  current_theme_ = classic_theme;
}

void ThemeManager::ShowSimpleThemeEditor(bool* p_open) {
  if (!p_open || !*p_open) return;
  
  if (ImGui::Begin(absl::StrFormat("%s Simple Theme Editor", ICON_MD_PALETTE).c_str(), p_open)) {
    ImGui::Text("%s Create or modify themes with basic controls", ICON_MD_EDIT);
    ImGui::Separator();
    
    static EnhancedTheme edit_theme = current_theme_;
    static char theme_name[128];
    static char theme_description[256];
    static char theme_author[128];
    
    // Basic theme info
    ImGui::InputText("Theme Name", theme_name, sizeof(theme_name));
    ImGui::InputText("Description", theme_description, sizeof(theme_description));
    ImGui::InputText("Author", theme_author, sizeof(theme_author));
    
    ImGui::Separator();
    
    // Primary Colors
    if (ImGui::CollapsingHeader("Primary Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImVec4 primary = ConvertColorToImVec4(edit_theme.primary);
      ImVec4 secondary = ConvertColorToImVec4(edit_theme.secondary);
      ImVec4 accent = ConvertColorToImVec4(edit_theme.accent);
      ImVec4 background = ConvertColorToImVec4(edit_theme.background);
      
      if (ImGui::ColorEdit3("Primary", &primary.x)) {
        edit_theme.primary = {primary.x, primary.y, primary.z, primary.w};
      }
      if (ImGui::ColorEdit3("Secondary", &secondary.x)) {
        edit_theme.secondary = {secondary.x, secondary.y, secondary.z, secondary.w};
      }
      if (ImGui::ColorEdit3("Accent", &accent.x)) {
        edit_theme.accent = {accent.x, accent.y, accent.z, accent.w};
      }
      if (ImGui::ColorEdit3("Background", &background.x)) {
        edit_theme.background = {background.x, background.y, background.z, background.w};
      }
    }
    
    // Text Colors
    if (ImGui::CollapsingHeader("Text Colors")) {
      ImVec4 text_primary = ConvertColorToImVec4(edit_theme.text_primary);
      ImVec4 text_secondary = ConvertColorToImVec4(edit_theme.text_secondary);
      ImVec4 text_disabled = ConvertColorToImVec4(edit_theme.text_disabled);
      
      if (ImGui::ColorEdit3("Primary Text", &text_primary.x)) {
        edit_theme.text_primary = {text_primary.x, text_primary.y, text_primary.z, text_primary.w};
      }
      if (ImGui::ColorEdit3("Secondary Text", &text_secondary.x)) {
        edit_theme.text_secondary = {text_secondary.x, text_secondary.y, text_secondary.z, text_secondary.w};
      }
      if (ImGui::ColorEdit3("Disabled Text", &text_disabled.x)) {
        edit_theme.text_disabled = {text_disabled.x, text_disabled.y, text_disabled.z, text_disabled.w};
      }
    }
    
    // Window Colors
    if (ImGui::CollapsingHeader("Window Colors")) {
      ImVec4 window_bg = ConvertColorToImVec4(edit_theme.window_bg);
      ImVec4 popup_bg = ConvertColorToImVec4(edit_theme.popup_bg);
      
      if (ImGui::ColorEdit4("Window Background", &window_bg.x)) {
        edit_theme.window_bg = {window_bg.x, window_bg.y, window_bg.z, window_bg.w};
      }
      if (ImGui::ColorEdit4("Popup Background", &popup_bg.x)) {
        edit_theme.popup_bg = {popup_bg.x, popup_bg.y, popup_bg.z, popup_bg.w};
      }
    }
    
    // Interactive Elements
    if (ImGui::CollapsingHeader("Interactive Elements")) {
      ImVec4 button = ConvertColorToImVec4(edit_theme.button);
      ImVec4 button_hovered = ConvertColorToImVec4(edit_theme.button_hovered);
      ImVec4 button_active = ConvertColorToImVec4(edit_theme.button_active);
      
      if (ImGui::ColorEdit3("Button", &button.x)) {
        edit_theme.button = {button.x, button.y, button.z, button.w};
      }
      if (ImGui::ColorEdit3("Button Hovered", &button_hovered.x)) {
        edit_theme.button_hovered = {button_hovered.x, button_hovered.y, button_hovered.z, button_hovered.w};
      }
      if (ImGui::ColorEdit3("Button Active", &button_active.x)) {
        edit_theme.button_active = {button_active.x, button_active.y, button_active.z, button_active.w};
      }
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Preview Theme")) {
      ApplyTheme(edit_theme);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Reset to Current")) {
      edit_theme = current_theme_;
      strncpy(theme_name, current_theme_.name.c_str(), sizeof(theme_name));
      strncpy(theme_description, current_theme_.description.c_str(), sizeof(theme_description));
      strncpy(theme_author, current_theme_.author.c_str(), sizeof(theme_author));
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save Theme")) {
      edit_theme.name = std::string(theme_name);
      edit_theme.description = std::string(theme_description);
      edit_theme.author = std::string(theme_author);
      
      // Add to themes map and apply
      themes_[edit_theme.name] = edit_theme;
      ApplyTheme(edit_theme);
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save to File...")) {
      edit_theme.name = std::string(theme_name);
      edit_theme.description = std::string(theme_description);
      edit_theme.author = std::string(theme_author);
      
      // Use folder dialog to choose save location
      auto folder_path = core::FileDialogWrapper::ShowOpenFolderDialog();
      if (!folder_path.empty()) {
        // Create filename from theme name (sanitize it)
        std::string safe_name = edit_theme.name;
        // Replace spaces and special chars with underscores
        for (char& c : safe_name) {
          if (!std::isalnum(c)) {
            c = '_';
          }
        }
        
        std::string file_path = folder_path + "/" + safe_name + ".theme";
        
        auto status = SaveThemeToFile(edit_theme, file_path);
        if (status.ok()) {
          // Also add to themes map for immediate use
          themes_[edit_theme.name] = edit_theme;
          ApplyTheme(edit_theme);
          util::logf("Theme saved successfully to: %s", file_path.c_str());
        } else {
          util::logf("Failed to save theme: %s", status.message().data());
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

} // namespace gui
} // namespace yaze
