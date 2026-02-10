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

  // Icon button size presets
  static constexpr float kIconButtonSmall = 24.0f;
  static constexpr float kIconButtonMedium = 32.0f;
  static constexpr float kIconButtonLarge = 48.0f;
  static constexpr float kIconButtonToolbar = 28.0f;

  // Activity bar icon dimensions
  static constexpr float kActivityBarIconWidth = 48.0f;
  static constexpr float kActivityBarIconHeight = 40.0f;

  // Padding presets
  static constexpr float kPanelPaddingSmall = 4.0f;
  static constexpr float kPanelPaddingMedium = 8.0f;
  static constexpr float kPanelPaddingLarge = 12.0f;

  // Animation
  static constexpr float kAnimationSpeed = 8.0f;  // Lerp speed multiplier
  static constexpr float kAnimationSnapThreshold = 0.01f;
};

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_CORE_UI_CONFIG_H_
