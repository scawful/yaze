#ifndef YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H
#define YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/overworld/ui_constants.h"
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

  // Mode state
  EditingMode* current_mode = nullptr;
  bool* current_map_lock = nullptr;
  bool* is_dragging_entity = nullptr;
  bool* show_map_properties_panel = nullptr;

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
  std::function<bool()> pick_tile16_from_hovered_canvas;
  /// Returns true if an entity is currently hovered (for pan suppression).
  std::function<bool()> is_entity_hovered;
};

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

  // ===========================================================================
  // Map Interaction
  // ===========================================================================

  /// @brief Handle tile-mode right-click (eyedropper) and middle-click
  /// (lock/properties toggle), plus double-click to open properties.
  void HandleMapInteraction();

  // ===========================================================================
  // Pan and Zoom
  // ===========================================================================

  /// @brief Pan the overworld canvas via middle-click or left-click drag
  /// (in MOUSE mode when not hovering an entity).
  void HandleOverworldPan();

  /// @brief No-op stub preserved for API compatibility.
  void HandleOverworldZoom();

  /// @brief Increase canvas zoom by one step.
  void ZoomIn();

  /// @brief Decrease canvas zoom by one step.
  void ZoomOut();

  /// @brief No-op stub -- ImGui handles scroll clamping automatically.
  void ClampOverworldScroll();

  /// @brief Reset scroll to top-left and scale to 1.0.
  void ResetOverworldView();

  /// @brief Center the viewport on the current map.
  void CenterOverworldView();

  /// @brief Legacy wrapper -- delegates to HandleOverworldPan().
  void CheckForMousePan();

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

  // Hover debounce state
  int last_hovered_map_ = -1;
  float hover_time_ = 0.0f;
  static constexpr float kHoverBuildDelay = 0.15f;

  // Background pre-loading state
  std::vector<int> preload_queue_;
  static constexpr float kPreloadStartDelay = 0.3f;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_CANVAS_NAVIGATION_MANAGER_H
