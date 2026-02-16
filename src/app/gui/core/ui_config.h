#ifndef YAZE_APP_GUI_CORE_UI_CONFIG_H_
#define YAZE_APP_GUI_CORE_UI_CONFIG_H_

#include <algorithm>

#include "imgui/imgui.h"

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
  static constexpr float kPanelWidthNarrow = 300.0f;
  static constexpr float kPanelWidthMedium = 360.0f;
  static constexpr float kPanelWidthWide = 400.0f;

  // Specific panel defaults
  static constexpr float kPanelWidthAgentChat = kPanelWidthWide;
  static constexpr float kPanelWidthProposals = kPanelWidthWide;
  static constexpr float kPanelWidthSettings = kPanelWidthMedium;
  static constexpr float kPanelWidthHelp = 340.0f;
  static constexpr float kPanelWidthNotifications = kPanelWidthMedium;
  static constexpr float kPanelWidthProperties = 340.0f;
  static constexpr float kPanelWidthProject = kPanelWidthMedium;

  // Panel layout
  static constexpr float kPanelHeaderHeight = 44.0f;
  static constexpr float kMaxPanelWidthRatio = 0.35f;

  // Minimum panel widths (for right panel and similar; used with SetPanelWidth /
  // clamp)
  static constexpr float kPanelMinWidthAbsolute = 180.0f;
  static constexpr float kPanelMinWidthAgentChat = 360.0f;
  static constexpr float kPanelMinWidthProposals = 360.0f;
  static constexpr float kPanelMinWidthSettings = 340.0f;
  static constexpr float kPanelMinWidthHelp = 340.0f;
  static constexpr float kPanelMinWidthNotifications = 300.0f;
  static constexpr float kPanelMinWidthProperties = 340.0f;
  static constexpr float kPanelMinWidthProject = 320.0f;

  // Content region minimums (for child windows so they never collapse to zero)
  static constexpr float kContentMinHeightChat = 120.0f;  // Agent chat body
  static constexpr float kContentMinHeightList =
      80.0f;  // Lists, notification list
  static constexpr float kContentMinHeightCanvas =
      200.0f;  // Canvas areas (dungeon, overworld)
  static constexpr float kContentMinWidthSidebar = 160.0f;  // Sidebar content

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

  // Splitter / header button spacing
  static constexpr float kSplitterWidth = 6.0f;
  static constexpr float kHeaderButtonSpacing = 6.0f;
  static constexpr float kHeaderButtonGap = 2.0f;

  // Animation
  static constexpr float kAnimationSpeed = 8.0f;  // Lerp speed multiplier
  static constexpr float kAnimationSnapThreshold = 0.01f;
};

// ---------------------------------------------------------------------------
// Viewport-relative sizing helpers
//
// Use these instead of hardcoded ImVec2 sizes for dialogs and windows so that
// the UI adapts to different display resolutions and viewport dimensions.
// All helpers read from ImGui::GetMainViewport()->WorkSize at call time.
// ---------------------------------------------------------------------------

/**
 * @brief Returns an ImVec2 sized as a fraction of the main viewport WorkSize.
 *
 * Example: ViewportRelativeSize(0.5f, 0.4f) on a 1920x1080 viewport gives
 * ImVec2(960, 432).
 */
inline ImVec2 ViewportRelativeSize(float width_factor, float height_factor) {
  const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
  return ImVec2(work.x * width_factor, work.y * height_factor);
}

/**
 * @brief Returns a standard dialog size as a fraction of the viewport.
 *
 * Defaults to 60% width and 65% height which works well for most modal
 * dialogs. Callers can override either factor.
 */
inline ImVec2 DialogSize(float width_factor = 0.6f,
                         float height_factor = 0.65f) {
  return ViewportRelativeSize(width_factor, height_factor);
}

/**
 * @brief Returns min/max size pair suitable for ImGui::SetNextWindowSizeConstraints.
 *
 * The minimum is clamped so the window never shrinks below |min_size| and
 * the maximum is capped at |max_factor| of the viewport in each dimension.
 *
 * Usage:
 *   auto [lo, hi] = ConstrainToViewport(ImVec2(300, 200));
 *   ImGui::SetNextWindowSizeConstraints(lo, hi);
 */
struct SizeConstraints {
  ImVec2 min_size;
  ImVec2 max_size;
};

inline SizeConstraints ConstrainToViewport(ImVec2 min_size,
                                           float max_factor = 0.95f) {
  const ImVec2 work = ImGui::GetMainViewport()->WorkSize;
  ImVec2 max_size(work.x * max_factor, work.y * max_factor);
  // Ensure min never exceeds max.
  max_size.x = std::max(max_size.x, min_size.x);
  max_size.y = std::max(max_size.y, min_size.y);
  return {min_size, max_size};
}

/**
 * @brief Returns a DPI-scaled version of a fixed pixel size.
 *
 * Multiplies the base dimensions by ImGui's FontGlobalScale, which serves
 * as the application-wide scale factor in yaze (set via user preferences).
 * Use this when you have a known pixel size that should grow with the UI
 * scale (e.g., canvas sizes, icon grids).
 */
inline ImVec2 ScaledSize(float base_width, float base_height) {
  const float scale = ImGui::GetIO().FontGlobalScale;
  return ImVec2(base_width * scale, base_height * scale);
}

// ---------------------------------------------------------------------------
// Content region sizing (for BeginChild and panel content areas)
// ---------------------------------------------------------------------------

/**
 * @brief Returns content region available size clamped to a minimum.
 *
 * Use before BeginChild when the child should fill available space but never
 * shrink below min_size. Preserves ImGui layout and scrollbars when content
 * exceeds available space.
 *
 * Usage:
 *   ImVec2 size = GetContentRegionAvailClamped(ImVec2(0, UIConfig::kContentMinHeightChat));
 *   if (ImGui::BeginChild("id", size, true)) { ... }
 */
inline ImVec2 GetContentRegionAvailClamped(const ImVec2& min_size) {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  return ImVec2(std::max(avail.x, min_size.x), std::max(avail.y, min_size.y));
}

}  // namespace yaze::gui

#endif  // YAZE_APP_GUI_CORE_UI_CONFIG_H_
