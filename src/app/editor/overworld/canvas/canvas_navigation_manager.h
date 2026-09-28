#ifndef YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "rom/rom.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

// Forward declarations
class OverworldEntityRenderer;

/// Turns wheel/trackpad deltas into pan steps that land on whole detents.
///
/// Pure state machine (no ImGui calls) so the feel is unit-testable. Input
/// below the deadzone is ignored, direction reversals drop the residual, and
/// the residual is discarded once input stops for kOverworldWheelIdleResetSec.
struct StickyWheelPan {
  ImVec2 residual{0.0f, 0.0f};
  float idle_seconds = 0.0f;

  /// @param wheel  io.MouseWheelH / io.MouseWheel (positive = left / up).
  /// @param step_px  pixels per wheel unit (a multiple of snap_px).
  /// @param snap_px  detent size in screen pixels.
  /// @return scroll delta in screen pixels, always a multiple of snap_px.
  ImVec2 Consume(ImVec2 wheel, float dt, float step_px, float snap_px);
};

/// Scroll that keeps the content point under @p anchor (viewport-local px)
/// fixed while the scale changes from @p old_scale to @p new_scale.
ImVec2 ScrollForZoomAtAnchor(ImVec2 scroll, ImVec2 anchor, float old_scale,
                             float new_scale);

// =============================================================================
// CanvasNavigationManager
// =============================================================================
//
// Extracted from OverworldEditor to encapsulate all canvas navigation logic:
//   - Map hover detection and lazy loading (CheckForCurrentMap)
//   - Pan and zoom controls
//   - Map interaction (context menus, lock toggle)
//   - Background preloading of adjacent maps
//   - Blockset selector synchronization
//
// The manager holds pointers to shared editor state (via NavigationContext)
// and uses callbacks for operations that remain in the editor (e.g. map
// refresh, texture creation).
// =============================================================================

/// @brief Shared state pointers that the navigation manager reads/writes.
struct CanvasNavigationContext {
  // Canvas references
  gui::Canvas* ow_map_canvas = nullptr;

  // Data model
  zelda3::Overworld* overworld = nullptr;
  Rom* rom = nullptr;

  // Selection state (mutable pointers into editor fields)
  int* current_map = nullptr;
  int* current_world = nullptr;
  int* current_parent = nullptr;
  int* current_tile16 = nullptr;
  int* hovered_map = nullptr;

  // Mode state
  EditingMode* current_mode = nullptr;
  bool* current_map_lock = nullptr;
  bool* is_dragging_entity = nullptr;

  // Graphics
  std::array<gfx::Bitmap, zelda3::kNumOverworldMaps>* maps_bmp = nullptr;
  gfx::Tilemap* tile16_blockset = nullptr;

  // Widgets (read-only pointer to editor's unique_ptr)
  std::unique_ptr<gui::TileSelectorWidget>* blockset_selector = nullptr;
};

/// @brief Callbacks for operations that remain in the OverworldEditor.
struct CanvasNavigationCallbacks {
  std::function<void()> refresh_overworld_map;
  std::function<absl::Status()> refresh_tile16_blockset;
  std::function<void(int)> ensure_map_texture;
  std::function<void(int, bool)> select_map_for_editing;
  std::function<bool()> pick_tile16_from_hovered_canvas;
  /// Returns true if an entity is currently hovered (for pan suppression).
  std::function<bool()> is_entity_hovered;
  /// Opens (and focuses) the Map Properties window. Double-click target.
  std::function<void()> open_map_properties;
};

/// One frame of canvas mouse input, reduced to what map selection needs.
struct MapClickInput {
  EditingMode mode = EditingMode::MOUSE;
  bool left_released = false;
  bool left_dragged = false;  // Moved past the drag threshold while down.
  // The left press this release ends also landed on this canvas, on the map
  // now under the cursor. False for a press that dismissed a popup or menu,
  // or began on another window or another map.
  bool left_press_owned = false;
  bool right_clicked = false;
  bool shift = false;
  bool entity_hovered = false;
};

/// True when this input is an explicit "make the map under the cursor the
/// current map" click: a Select-tool left click that did not pan and whose
/// press also landed on this canvas and map (on release), or a paint-tool
/// right click (which also samples the Tile16). Shift+right-click opens the
/// map menu instead. Explicit clicks select even when the map is pinned; the
/// pin then holds the clicked map.
bool IsMapSelectClick(const MapClickInput& input);

class CanvasNavigationManager {
 public:
  CanvasNavigationManager() = default;

  /// @brief Initialize with shared state and callbacks.
  void Initialize(const CanvasNavigationContext& context,
                  const CanvasNavigationCallbacks& callbacks);

  // ===========================================================================
  // Map Detection and Loading
  // ===========================================================================

  /// @brief Detect which map the mouse is over, trigger lazy loading, draw
  /// the selection outline, and refresh textures if modified.
  absl::Status CheckForCurrentMap();

  // Update hover and unpinned selection using scaled canvas-local coordinates.
  // Separate from rendering so all edit modes share the same tracking policy.
  std::optional<int> TrackMapAtCanvasPosition(ImVec2 scaled_position);

  // ===========================================================================
  // Map Interaction
  // ===========================================================================

  /// @brief Click selection (see IsMapSelectClick) and double-click to open
  /// properties. The paint-tool right-click sample itself happens in
  /// TilePaintingManager. Middle-drag only pans.
  void HandleMapInteraction();

  /// @brief Make the map under the cursor the current map, ignoring the pin
  /// (the pin, if on, then holds this map). Large areas: any quadrant selects
  /// its parent area's properties. Returns false when no map is under it.
  bool SelectMapUnderCursor();

  // ===========================================================================
  // Pan and Zoom
  // ===========================================================================
  //
  // All scrolling of the canvas child goes through this manager:
  //   BeginCanvasViewport()  immediately before BeginChild: applies zoom,
  //                          drag pan and wheel input via SetNextWindowScroll
  //                          so the frame draws with its final scroll.
  //   EndCanvasViewport()    inside the child after BeginCanvas: records the
  //                          viewport rect, scroll and hover for next frame.
  // The child is created with ImGuiWindowFlags_NoScrollWithMouse so ImGui's
  // own fractional wheel scroll never runs on the canvas.

  void BeginCanvasViewport();
  void EndCanvasViewport(bool canvas_item_hovered);

  /// @brief Increase canvas zoom by one step, keeping the view center fixed.
  void ZoomIn();

  /// @brief Decrease canvas zoom by one step, keeping the view center fixed.
  void ZoomOut();

  /// @brief Scale so the whole current world fits the viewport.
  void ZoomToFit();

  /// @brief Reset scroll to top-left and scale to 1.0.
  void ResetOverworldView();

  /// @brief Center the viewport on the current map.
  void CenterOverworldView();

  /// @brief Center the viewport on @p map_id (same world grid).
  void CenterOnMap(int map_id);

  bool is_panning() const { return pan_active_; }

  /// After keyboard map navigation: do not let the (unpinned) hover follow
  /// replace the keyboard choice until the mouse actually moves.
  void SuspendHoverFollowUntilMouseMoves();

  // ===========================================================================
  // Blockset Selector Synchronization
  // ===========================================================================

  /// @brief Scroll the blockset (tile16 selector) to show the currently
  /// selected tile16.
  void ScrollBlocksetCanvasToCurrentTile();

  /// @brief Push current tile count and selection into the blockset widget.
  void UpdateBlocksetSelectorState();

  // ===========================================================================
  // Background Pre-loading
  // ===========================================================================

  /// @brief Queue the 4-connected neighbors of @p center_map for lazy build.
  void QueueAdjacentMapsForPreload(int center_map);

  /// @brief Process one map from the preload queue (call once per frame).
  void ProcessPreloadQueue();

 private:
  CanvasNavigationContext ctx_;
  CanvasNavigationCallbacks callbacks_;

  // Map under the cursor: the hover-tracked map, else the canvas position.
  std::optional<int> MapUnderCursor() const;

  // Left press owned by this canvas (see MapClickInput::left_press_owned):
  // ImGuiIO::MouseClickedTime of that press and the map it landed on.
  double left_press_time_ = -1.0;
  std::optional<int> left_press_map_;

  // Hover debounce state
  int last_hovered_map_ = -1;
  float hover_time_ = 0.0f;
  static constexpr float kHoverBuildDelay = 0.15f;

  // Background pre-loading state
  std::vector<int> preload_queue_;
  static constexpr float kPreloadStartDelay = 0.3f;

  // Zoom by `steps` of kOverworldZoomStep about a viewport-local anchor.
  void ZoomBySteps(int steps, ImVec2 anchor);
  void SetScaleAboutAnchor(float new_scale, ImVec2 anchor);
  ImVec2 ClampScroll(ImVec2 scroll) const;
  ImVec2 ContentSize() const;

  // Viewport state recorded by EndCanvasViewport (previous frame).
  ImVec2 viewport_min_{0.0f, 0.0f};
  ImVec2 viewport_size_{0.0f, 0.0f};
  ImVec2 last_scroll_{0.0f, 0.0f};
  bool canvas_item_hovered_ = false;
  bool canvas_window_hovered_ = false;
  bool viewport_known_ = false;

  std::optional<ImVec2> hover_follow_suspended_at_;

  // Scroll to apply at the next BeginCanvasViewport.
  std::optional<ImVec2> pending_scroll_;

  // Wheel state
  StickyWheelPan wheel_pan_;
  float zoom_residual_ = 0.0f;
  float zoom_idle_seconds_ = 0.0f;

  // Drag pan: content stays pinned to the point grabbed at mouse-down.
  int pan_button_ = -1;
  bool pan_active_ = false;
  ImVec2 pan_anchor_mouse_{0.0f, 0.0f};
  ImVec2 pan_anchor_scroll_{0.0f, 0.0f};
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H
