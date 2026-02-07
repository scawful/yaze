#ifndef YAZE_APP_GUI_CORE_UI_CONFIG_H_
#define YAZE_APP_GUI_CORE_UI_CONFIG_H_

namespace yaze::gui {

/**
 * @brief Centralized UI dimension constants
 *
 * Replaces scattered magic numbers across the UI layer. All values are in
 * logical pixels and may be scaled by density presets at runtime.
 */
struct UIConfig {
  // Activity bar
  static constexpr float kActivityBarWidth = 48.0f;

  // Status bar
  static constexpr float kStatusBarHeight = 24.0f;

  // Right panel defaults
  static constexpr float kPanelWidthNarrow = 320.0f;
  static constexpr float kPanelWidthMedium = 380.0f;
  static constexpr float kPanelWidthWide = 420.0f;

  // Panel layout
  static constexpr float kPanelHeaderHeight = 44.0f;
  static constexpr float kMaxPanelWidthRatio = 0.35f;

  // Animation
  static constexpr float kAnimationSpeed = 8.0f;  // Lerp speed multiplier
  static constexpr float kAnimationSnapThreshold = 0.01f;
};

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_CORE_UI_CONFIG_H_
