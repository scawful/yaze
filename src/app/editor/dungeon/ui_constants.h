#ifndef YAZE_APP_EDITOR_DUNGEON_UI_CONSTANTS_H
#define YAZE_APP_EDITOR_DUNGEON_UI_CONSTANTS_H

// Shared layout constants for the dungeon editor UI. Mirrors
// src/app/editor/overworld/ui_constants.h: each editor family keeps its layout
// tuning in one header so panels, widgets, and tests reference a single source
// of truth instead of redefining literals in .cc files.
//
// Scope: only constants that are load-bearing for layout math or that are
// meaningful to external callers (tests, sibling panels). Purely file-local
// IDs (ImGui child/popup strings) should stay as anonymous-namespace locals
// in their owning translation unit.

namespace yaze::editor::dungeon_ui {

// Logical pixel footprint of a single dungeon room as rendered on the canvas.
// 64 tiles * 8 px. Used by the connected-room graph and overview.
inline constexpr float kDungeonRoomPixelSize = 512.0f;

// Gap (in logical pixels) between rooms in the connected-room graph layout.
inline constexpr float kConnectedRoomGap = 48.0f;

// Connected-canvas zoom bounds + default.
inline constexpr float kConnectedCanvasDefaultScale = 0.18f;
inline constexpr float kConnectedCanvasMinScale = 0.12f;
inline constexpr float kConnectedCanvasMaxScale = 0.60f;

// Overview / current-room preview side-panel geometry.
inline constexpr float kConnectedSidePanelGap = 12.0f;
inline constexpr float kConnectedSidePanelApproxMinWidth = 196.0f;
// Hysteresis band. Once the side panel is visible it stays visible down to
// `min - kConnectedSidePanelHideHysteresis` pixels of viewport width. Below
// that the panel collapses. Prevents a sudden ~220 px horizontal snap when
// toggling the panel near the visibility threshold.
inline constexpr float kConnectedSidePanelHideHysteresis = 40.0f;
inline constexpr float kConnectedControlsApproxHeight = 62.0f;
inline constexpr float kConnectedOverviewApproxHeight = 168.0f;
inline constexpr float kConnectedPreviewApproxHeight = 212.0f;

// Overview panel sizing.
inline constexpr float kConnectedOverviewMaxWidth = 196.0f;
inline constexpr float kConnectedOverviewMaxHeight = 152.0f;

// Current-room preview HUD sizing.
inline constexpr float kConnectedCurrentRoomPreviewWidth = 160.0f;
inline constexpr float kConnectedCurrentRoomPreviewPadding = 12.0f;

// Room-label chrome on the connected matrix.
inline constexpr float kConnectedRoomLabelPadding = 10.0f;
inline constexpr float kConnectedRoomLabelHeight = 28.0f;
inline constexpr float kConnectedRoomHighlightThickness = 3.0f;
inline constexpr float kConnectedRoomOutlineThickness = 1.5f;

// Returns true when the side panel should remain visible, with hysteresis to
// avoid layout bounce. `viewport_width` is the unreserved width of the
// connected canvas viewport (i.e. the body child's width).
constexpr bool ShouldShowConnectedSidePanel(float viewport_width,
                                            bool currently_visible) {
  const float hide_threshold = currently_visible
                                   ? kConnectedSidePanelApproxMinWidth -
                                         kConnectedSidePanelHideHysteresis
                                   : kConnectedSidePanelApproxMinWidth;
  return viewport_width >= hide_threshold;
}

}  // namespace yaze::editor::dungeon_ui

#endif  // YAZE_APP_EDITOR_DUNGEON_UI_CONSTANTS_H
