#ifndef YAZE_APP_GUI_THEME_MANAGER_H
#define YAZE_APP_GUI_THEME_MANAGER_H

#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gui/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @struct EnhancedTheme
 * @brief Comprehensive theme structure for YAZE
 */
struct EnhancedTheme {
  std::string name;
  std::string description;
  std::string author;
  
  // Primary colors
  Color primary;
  Color secondary;
  Color accent;
  Color background;
  Color surface;
  Color error;
  Color warning;
  Color success;
  Color info;
  
  // Text colors
  Color text_primary;
  Color text_secondary;
  Color text_disabled;
  
  // Window colors
  Color window_bg;
  Color child_bg;
  Color popup_bg;
  Color modal_bg;
  
  // Interactive elements
  Color button;
  Color button_hovered;
  Color button_active;
  Color frame_bg;
  Color frame_bg_hovered;
  Color frame_bg_active;
  
  // Navigation and selection
  Color header;
  Color header_hovered;
  Color header_active;
  Color tab;
  Color tab_hovered;
  Color tab_active;
  Color menu_bar_bg;
  Color title_bg;
  Color title_bg_active;
  Color title_bg_collapsed;
  
  // Borders and separators
  Color border;
  Color border_shadow;
  Color separator;
  Color separator_hovered;
  Color separator_active;
  
  // Scrollbars and controls
  Color scrollbar_bg;
  Color scrollbar_grab;
  Color scrollbar_grab_hovered;
  Color scrollbar_grab_active;
  
  // Special elements
  Color resize_grip;
  Color resize_grip_hovered;
  Color resize_grip_active;
  Color docking_preview;
  Color docking_empty_bg;
  
  // Complete ImGui color support
  Color check_mark;
  Color slider_grab;
  Color slider_grab_active;
  Color input_text_cursor;
  Color nav_cursor;
  Color nav_windowing_highlight;
  Color nav_windowing_dim_bg;
  Color modal_window_dim_bg;
  Color text_selected_bg;
  Color drag_drop_target;
  Color table_header_bg;
  Color table_border_strong;
  Color table_border_light;
  Color table_row_bg;
  Color table_row_bg_alt;
  Color text_link;
  Color plot_lines;
  Color plot_lines_hovered;
  Color plot_histogram;
  Color plot_histogram_hovered;
  Color tree_lines;
  
  // Additional ImGui colors for complete coverage
  Color tab_dimmed;
  Color tab_dimmed_selected;
  Color tab_dimmed_selected_overline;
  Color tab_selected_overline;
  
  // Enhanced theme system - semantic colors
  Color text_highlight;     // For selected text, highlighted items
  Color link_hover;        // For hover state of links 
  Color code_background;   // For code blocks, monospace text backgrounds
  Color success_light;     // Lighter variant of success color
  Color warning_light;     // Lighter variant of warning color  
  Color error_light;       // Lighter variant of error color
  Color info_light;        // Lighter variant of info color
  
  // UI state colors
  Color active_selection;  // For active/selected UI elements
  Color hover_highlight;   // General hover state
  Color focus_border;      // For focused input elements
  Color disabled_overlay;  // Semi-transparent overlay for disabled elements
  
  // Editor-specific colors
  Color editor_background; // Main editor canvas background
  Color editor_grid;       // Grid lines in editors
  Color editor_cursor;     // Cursor/selection in editors
  Color editor_selection;  // Selected area in editors
  
  // Style parameters
  float window_rounding = 0.0f;
  float frame_rounding = 5.0f;
  float scrollbar_rounding = 5.0f;
  float grab_rounding = 3.0f;
  float tab_rounding = 0.0f;
  float window_border_size = 0.0f;
  float frame_border_size = 0.0f;
  
  // Animation and effects
  bool enable_animations = true;
  float animation_speed = 1.0f;
  bool enable_glow_effects = false;
  
  // Helper methods
  void ApplyToImGui() const;
};

/**
 * @class ThemeManager
 * @brief Manages themes, loading, saving, and switching
 */
class ThemeManager {
public:
  static ThemeManager& Get();
  
  // Theme management
  absl::Status LoadTheme(const std::string& theme_name);
  absl::Status SaveTheme(const EnhancedTheme& theme, const std::string& filename);
  absl::Status LoadThemeFromFile(const std::string& filepath);
  absl::Status SaveThemeToFile(const EnhancedTheme& theme, const std::string& filepath) const;
  
  // Dynamic theme discovery - replaces hardcoded theme lists with automatic discovery
  // This works across development builds, macOS app bundles, and other deployment scenarios
  std::vector<std::string> DiscoverAvailableThemeFiles() const;
  absl::Status LoadAllAvailableThemes();
  absl::Status RefreshAvailableThemes(); // Public method to refresh at runtime
  
  // Built-in themes
  void InitializeBuiltInThemes();
  std::vector<std::string> GetAvailableThemes() const;
  const EnhancedTheme* GetTheme(const std::string& name) const;
  const EnhancedTheme& GetCurrentTheme() const { return current_theme_; }
  const std::string& GetCurrentThemeName() const { return current_theme_name_; }
  
  // Theme application
  void ApplyTheme(const std::string& theme_name);
  void ApplyTheme(const EnhancedTheme& theme);
  void ApplyClassicYazeTheme(); // Apply original ColorsYaze() function
  
  // Theme creation and editing
  EnhancedTheme CreateCustomTheme(const std::string& name);
  void ShowThemeEditor(bool* p_open);
  void ShowThemeSelector(bool* p_open);
  void ShowSimpleThemeEditor(bool* p_open);
  
  // Integration with welcome screen
  Color GetWelcomeScreenBackground() const;
  Color GetWelcomeScreenBorder() const;
  Color GetWelcomeScreenAccent() const;
  
private:
  ThemeManager() { InitializeBuiltInThemes(); }
  
  std::map<std::string, EnhancedTheme> themes_;
  EnhancedTheme current_theme_;
  std::string current_theme_name_ = "Classic YAZE";
  
  void CreateFallbackYazeClassic();
  absl::Status ParseThemeFile(const std::string& content, EnhancedTheme& theme);
  Color ParseColorFromString(const std::string& color_str) const;
  std::string SerializeTheme(const EnhancedTheme& theme) const;
  
  // Helper methods for path resolution
  std::vector<std::string> GetThemeSearchPaths() const;
  std::string GetThemesDirectory() const;
  std::string GetCurrentThemeFilePath() const;
};

} // namespace gui
} // namespace yaze

#endif // YAZE_APP_GUI_THEME_MANAGER_H
