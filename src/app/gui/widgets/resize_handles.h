#ifndef YAZE_APP_GUI_WIDGETS_RESIZE_HANDLES_H_
#define YAZE_APP_GUI_WIDGETS_RESIZE_HANDLES_H_

#include <cstdint>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

// Which handle zones to draw and hit-test. Corners take priority over edges
// when the mouse overlaps both.
enum class HandleMask : std::uint8_t {
  kNone = 0,
  kNW = 1 << 0,
  kNE = 1 << 1,
  kSW = 1 << 2,
  kSE = 1 << 3,
  kN = 1 << 4,
  kE = 1 << 5,
  kS = 1 << 6,
  kW = 1 << 7,
  kCorners = kNW | kNE | kSW | kSE,
  kEdges = kN | kE | kS | kW,
  kAll = kCorners | kEdges,
};

constexpr HandleMask operator|(HandleMask a, HandleMask b) {
  return static_cast<HandleMask>(static_cast<std::uint8_t>(a) |
                                 static_cast<std::uint8_t>(b));
}
constexpr HandleMask operator&(HandleMask a, HandleMask b) {
  return static_cast<HandleMask>(static_cast<std::uint8_t>(a) &
                                 static_cast<std::uint8_t>(b));
}
constexpr bool Has(HandleMask all, HandleMask flag) {
  return (static_cast<std::uint8_t>(all) & static_cast<std::uint8_t>(flag)) !=
         0;
}

// Which zone the user is hitting. At most one value is returned per hit test.
enum class HandleZone : std::uint8_t {
  kNone = 0,
  kNW,
  kN,
  kNE,
  kW,
  kE,
  kSW,
  kS,
  kSE,
};

struct ResizeHandleStyle {
  float handle_size = 8.0f;
  float min_width = 4.0f;
  float min_height = 4.0f;
  // If alpha == 0, the theme's selection_handle color is used at draw time.
  ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
};

// Draws eight resize handles around `*rect` and services mouse drag.
//
// Returns true on the frame the user commits a drag (mouse release inside an
// active handle). Mid-drag frames mutate `*rect` but return false. Frames
// with no active drag leave `*rect` untouched.
//
// `mask`  — which zones are active (corners only, edges only, or all).
// `snap`  — when > 0, each moving coordinate is rounded to a multiple of this.
// `id`    — scope for ImGui hit-test state. Pass 0 to auto-derive from the
//           rect position.
// `style` — sizing + minimum-rect + color overrides.
bool ResizeHandles(ImRect* rect, HandleMask mask = HandleMask::kAll,
                   float snap = 0.0f, ImGuiID id = 0,
                   const ResizeHandleStyle& style = {});

// ---------------------------------------------------------------------------
// Internal helpers exposed for tests.
// ---------------------------------------------------------------------------
namespace resize_handles_internal {

// Round `v` to the nearest multiple of `snap`. When `snap <= 0` returns `v`
// unchanged.
float SnapCoord(float v, float snap);

// Returns the zone under `mouse` given `rect`, mask, and handle size.
// Corners take priority over edges when they overlap. Returns `kNone` when
// no zone is hit or when the hit zone is disabled by `mask`.
HandleZone HitTestZone(ImVec2 mouse, const ImRect& rect, HandleMask mask,
                       float handle_size);

// Applies a drag delta to `*rect` at the given zone, snapping each moving
// coordinate and enforcing minimum size. When `zone == kNone` the rect is
// not touched.
void ApplyDragDelta(ImRect* rect, HandleZone zone, ImVec2 delta, float snap,
                    float min_width, float min_height);

// Returns the theme's selection_handle color as an ImVec4. Read at call time
// so theme swaps take effect immediately.
ImVec4 GetDefaultHandleColor();

}  // namespace resize_handles_internal

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGETS_RESIZE_HANDLES_H_
