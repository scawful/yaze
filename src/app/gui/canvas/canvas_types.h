#ifndef YAZE_APP_GUI_CANVAS_CANVAS_TYPES_H
#define YAZE_APP_GUI_CANVAS_CANVAS_TYPES_H

// POD and option structs for the canvas subsystem.

#include <optional>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

enum class CanvasType { kTile, kBlock, kMap };
enum class CanvasMode { kPaint, kSelect };
enum class CanvasGridSize { k8x8, k16x16, k32x32, k64x64 };

// Describes the role this canvas plays: what it's for, distinct from
// CanvasMode which describes the input behavior the user is currently
// performing. A canvas can have CanvasMode::kSelect while being either a
// kSelectionSource (picker) or a kEditableScratchpad (rect-select-then-edit).
//
// Editors set this once at Init/Begin time. Canvas reads it for cursor and
// hover-hint defaults; it does not gate operations.
enum class CanvasRole : uint8_t {
  kPreviewOnly,         // Read-only display (gfx-group sheet thumbnail).
  kSelectionSource,     // Read-only tile/region picker source.
  kEditableScratchpad,  // User-editable target (default).
  kCompositeOutput,     // Post-render composite output.
};

struct CanvasRuntime {
  ImDrawList* draw_list = nullptr;
  ImVec2 canvas_p0 = ImVec2(0, 0);
  ImVec2 canvas_sz = ImVec2(0, 0);
  ImVec2 scrolling = ImVec2(0, 0);
  ImVec2 mouse_pos_local = ImVec2(0, 0);
  bool hovered = false;
  float grid_step = 16.0f;
  float scale = 1.0f;
  ImVec2 content_size = ImVec2(0, 0);
};

struct CanvasFrameOptions {
  ImVec2 canvas_size = ImVec2(0, 0);
  bool draw_context_menu = true;
  bool draw_grid = true;
  std::optional<float> grid_step;
  bool draw_overlay = true;
  bool render_popups = true;
  bool use_child_window = false;
  bool show_scrollbar = false;
};

struct BitmapPreviewOptions {
  ImVec2 canvas_size = ImVec2(0, 0);
  ImVec2 dest_pos = ImVec2(0, 0);
  ImVec2 dest_size = ImVec2(0, 0);
  ImVec2 src_pos = ImVec2(0, 0);
  ImVec2 src_size = ImVec2(0, 0);
  float scale = 1.0f;
  int alpha = 255;
  bool draw_context_menu = false;
  bool draw_grid = true;
  std::optional<float> grid_step;
  bool draw_overlay = true;
  bool render_popups = true;
  bool ensure_texture = false;
  int selector_tile_size = 0;
  int selector_tile_size_y = 0;
};

struct TileHit {
  int tile_index = -1;
  ImVec2 tile_origin = ImVec2(0, 0);
  ImVec2 tile_size = ImVec2(0, 0);
};

struct BitmapDrawOpts {
  ImVec2 dest_pos = ImVec2(0, 0);
  ImVec2 dest_size = ImVec2(0, 0);
  ImVec2 src_pos = ImVec2(0, 0);
  ImVec2 src_size = ImVec2(0, 0);
  float scale = 1.0f;
  int alpha = 255;
  bool ensure_texture = true;
};

struct SelectorPanelOpts {
  ImVec2 canvas_size = ImVec2(0, 0);
  float grid_step = 16.0f;
  bool show_grid = true;
  bool show_overlay = true;
  bool render_popups = true;
  bool ensure_texture = true;
  int tile_selector_size = 0;
  int tile_selector_size_y = 0;
};

struct PreviewPanelOpts {
  ImVec2 canvas_size = ImVec2(0, 0);
  ImVec2 dest_pos = ImVec2(0, 0);
  ImVec2 dest_size = ImVec2(0, 0);
  float grid_step = 0.0f;
  bool render_popups = false;
  bool ensure_texture = true;
};

struct ZoomToFitResult {
  float scale = 1.0f;
  ImVec2 scroll = ImVec2(0, 0);
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_TYPES_H
