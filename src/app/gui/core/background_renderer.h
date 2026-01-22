#ifndef YAZE_APP_EDITOR_UI_BACKGROUND_RENDERER_H
#define YAZE_APP_EDITOR_UI_BACKGROUND_RENDERER_H

#include <memory>
#include <vector>

#include "app/gui/core/color.h"
#include "rom/rom.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class BackgroundRenderer
 * @brief Renders themed background effects for docking windows
 */
class BackgroundRenderer {
 public:
  struct GridSettings {
    float grid_size = 32.0f;        // Size of grid cells
    float line_thickness = 1.0f;    // Thickness of grid lines
    float opacity = 0.12f;          // Subtle but visible opacity
    float fade_distance = 400.0f;   // Distance over which grid fades
    bool enable_animation = false;  // Animation toggle (default off)
    bool enable_breathing =
        false;                 // Color breathing effect toggle (default off)
    bool radial_fade = true;   // Re-enable subtle radial fade
    bool enable_dots = false;  // Use dots instead of lines
    float dot_size = 2.0f;     // Size of grid dots
    float animation_speed = 1.0f;  // Animation speed multiplier
    float breathing_speed = 1.5f;  // Breathing effect speed
    float breathing_intensity =
        0.3f;  // How much color changes during breathing
  };

  static BackgroundRenderer& Get();

  // Main rendering functions
  void RenderDockingBackground(ImDrawList* draw_list, const ImVec2& window_pos,
                               const ImVec2& window_size,
                               const Color& theme_color);
  void RenderGridBackground(ImDrawList* draw_list, const ImVec2& window_pos,
                            const ImVec2& window_size, const Color& grid_color);
  void RenderRadialGradient(ImDrawList* draw_list, const ImVec2& center,
                            float radius, const Color& inner_color,
                            const Color& outer_color);

  // Configuration
  void SetGridSettings(const GridSettings& settings) {
    grid_settings_ = settings;
  }
  const GridSettings& GetGridSettings() const { return grid_settings_; }

  // Animation
  void UpdateAnimation(float delta_time);
  void SetAnimationEnabled(bool enabled) {
    grid_settings_.enable_animation = enabled;
  }

  // Theme integration
  void UpdateForTheme(const Color& primary_color,
                      const Color& background_color);

  // UI for settings
  void DrawSettingsUI();

 private:
  BackgroundRenderer() = default;

  GridSettings grid_settings_;
  float animation_time_ = 0.0f;
  Color cached_grid_color_{0.5f, 0.5f, 0.5f, 0.1f};

  // Helper functions
  float CalculateRadialFade(const ImVec2& pos, const ImVec2& center,
                            float max_distance) const;
  ImU32 BlendColorWithFade(const Color& base_color, float fade_factor) const;
  void DrawGridLine(ImDrawList* draw_list, const ImVec2& start,
                    const ImVec2& end, ImU32 color, float thickness) const;
  void DrawGridDot(ImDrawList* draw_list, const ImVec2& pos, ImU32 color,
                   float size) const;
};

/**
 * @class DockSpaceRenderer
 * @brief Enhanced docking space with themed background effects
 */
class DockSpaceRenderer {
 public:
  static void BeginEnhancedDockSpace(ImGuiID dockspace_id,
                                     const ImVec2& size = ImVec2(0, 0),
                                     ImGuiDockNodeFlags flags = 0);
  static void EndEnhancedDockSpace();

  static ImVec2 GetLastDockspacePos() { return last_dockspace_pos_; }
  static ImVec2 GetLastDockspaceSize() { return last_dockspace_size_; }

  // Configuration
  static void SetBackgroundEnabled(bool enabled) {
    background_enabled_ = enabled;
  }
  static void SetGridEnabled(bool enabled) { grid_enabled_ = enabled; }
  static void SetEffectsEnabled(bool enabled) { effects_enabled_ = enabled; }

 private:
  static bool background_enabled_;
  static bool grid_enabled_;
  static bool effects_enabled_;
  static ImVec2 last_dockspace_pos_;
  static ImVec2 last_dockspace_size_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_BACKGROUND_RENDERER_H
