#include "app/gui/widgets/resize_handles.h"

#include <algorithm>
#include <cmath>

#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace resize_handles_internal {

float SnapCoord(float v, float snap) {
  if (snap <= 0.0f)
    return v;
  return std::round(v / snap) * snap;
}

HandleZone HitTestZone(ImVec2 mouse, const ImRect& rect, HandleMask mask,
                       float handle_size) {
  const float s = handle_size;
  const float half = s * 0.5f;
  const ImVec2 mn = rect.Min;
  const ImVec2 mx = rect.Max;

  auto contains = [&](float x0, float y0, float x1, float y1) {
    return mouse.x >= x0 && mouse.x <= x1 && mouse.y >= y0 && mouse.y <= y1;
  };

  // Corner hit-tests first (priority over edges).
  if (Has(mask, HandleMask::kNW) &&
      contains(mn.x - half, mn.y - half, mn.x + half, mn.y + half)) {
    return HandleZone::kNW;
  }
  if (Has(mask, HandleMask::kNE) &&
      contains(mx.x - half, mn.y - half, mx.x + half, mn.y + half)) {
    return HandleZone::kNE;
  }
  if (Has(mask, HandleMask::kSW) &&
      contains(mn.x - half, mx.y - half, mn.x + half, mx.y + half)) {
    return HandleZone::kSW;
  }
  if (Has(mask, HandleMask::kSE) &&
      contains(mx.x - half, mx.y - half, mx.x + half, mx.y + half)) {
    return HandleZone::kSE;
  }

  // Edge strips (inset from corners).
  if (Has(mask, HandleMask::kN) &&
      contains(mn.x + half, mn.y - half, mx.x - half, mn.y + half)) {
    return HandleZone::kN;
  }
  if (Has(mask, HandleMask::kS) &&
      contains(mn.x + half, mx.y - half, mx.x - half, mx.y + half)) {
    return HandleZone::kS;
  }
  if (Has(mask, HandleMask::kW) &&
      contains(mn.x - half, mn.y + half, mn.x + half, mx.y - half)) {
    return HandleZone::kW;
  }
  if (Has(mask, HandleMask::kE) &&
      contains(mx.x - half, mn.y + half, mx.x + half, mx.y - half)) {
    return HandleZone::kE;
  }

  return HandleZone::kNone;
}

void ApplyDragDelta(ImRect* rect, HandleZone zone, ImVec2 delta, float snap,
                    float min_width, float min_height) {
  if (!rect || zone == HandleZone::kNone)
    return;

  const bool move_min_x = zone == HandleZone::kNW || zone == HandleZone::kW ||
                          zone == HandleZone::kSW;
  const bool move_max_x = zone == HandleZone::kNE || zone == HandleZone::kE ||
                          zone == HandleZone::kSE;
  const bool move_min_y = zone == HandleZone::kNW || zone == HandleZone::kN ||
                          zone == HandleZone::kNE;
  const bool move_max_y = zone == HandleZone::kSW || zone == HandleZone::kS ||
                          zone == HandleZone::kSE;

  if (move_min_x)
    rect->Min.x = SnapCoord(rect->Min.x + delta.x, snap);
  if (move_max_x)
    rect->Max.x = SnapCoord(rect->Max.x + delta.x, snap);
  if (move_min_y)
    rect->Min.y = SnapCoord(rect->Min.y + delta.y, snap);
  if (move_max_y)
    rect->Max.y = SnapCoord(rect->Max.y + delta.y, snap);

  // Enforce minimum size by pushing the dragged side, not the anchor side.
  if (move_min_x && rect->Max.x - rect->Min.x < min_width)
    rect->Min.x = rect->Max.x - min_width;
  if (move_max_x && rect->Max.x - rect->Min.x < min_width)
    rect->Max.x = rect->Min.x + min_width;
  if (move_min_y && rect->Max.y - rect->Min.y < min_height)
    rect->Min.y = rect->Max.y - min_height;
  if (move_max_y && rect->Max.y - rect->Min.y < min_height)
    rect->Max.y = rect->Min.y + min_height;
}

ImVec4 GetDefaultHandleColor() {
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return ConvertColorToImVec4(theme.selection_handle);
}

}  // namespace resize_handles_internal

namespace {

ImGuiMouseCursor CursorForZone(HandleZone zone) {
  switch (zone) {
    case HandleZone::kN:
    case HandleZone::kS:
      return ImGuiMouseCursor_ResizeNS;
    case HandleZone::kW:
    case HandleZone::kE:
      return ImGuiMouseCursor_ResizeEW;
    case HandleZone::kNE:
    case HandleZone::kSW:
      return ImGuiMouseCursor_ResizeNESW;
    case HandleZone::kNW:
    case HandleZone::kSE:
      return ImGuiMouseCursor_ResizeNWSE;
    default:
      return ImGuiMouseCursor_Arrow;
  }
}

ImRect HandleRect(HandleZone zone, const ImRect& rect, float handle_size) {
  const float half = handle_size * 0.5f;
  const ImVec2 mn = rect.Min;
  const ImVec2 mx = rect.Max;
  switch (zone) {
    case HandleZone::kNW:
      return ImRect(mn.x - half, mn.y - half, mn.x + half, mn.y + half);
    case HandleZone::kNE:
      return ImRect(mx.x - half, mn.y - half, mx.x + half, mn.y + half);
    case HandleZone::kSW:
      return ImRect(mn.x - half, mx.y - half, mn.x + half, mx.y + half);
    case HandleZone::kSE:
      return ImRect(mx.x - half, mx.y - half, mx.x + half, mx.y + half);
    case HandleZone::kN:
      return ImRect(mn.x + half, mn.y - half, mx.x - half, mn.y + half);
    case HandleZone::kS:
      return ImRect(mn.x + half, mx.y - half, mx.x - half, mx.y + half);
    case HandleZone::kW:
      return ImRect(mn.x - half, mn.y + half, mn.x + half, mx.y - half);
    case HandleZone::kE:
      return ImRect(mx.x - half, mn.y + half, mx.x + half, mx.y - half);
    default:
      return ImRect();
  }
}

}  // namespace

bool ResizeHandles(ImRect* rect, HandleMask mask, float snap, ImGuiID id,
                   const ResizeHandleStyle& style) {
  if (!rect)
    return false;

  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx == nullptr)
    return false;

  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window == nullptr || window->SkipItems)
    return false;

  if (id == 0) {
    id = window->GetID(rect);
  }

  const ImVec2 mouse = ImGui::GetMousePos();
  const HandleZone hovered_zone = resize_handles_internal::HitTestZone(
      mouse, *rect, mask, style.handle_size);

  // Per-zone button behavior: tracks which zone owns the active drag.
  static thread_local HandleZone s_active_zone = HandleZone::kNone;
  static thread_local ImGuiID s_active_id = 0;
  static thread_local ImVec2 s_last_mouse = ImVec2(0, 0);

  bool committed = false;

  if (s_active_id == id && s_active_zone != HandleZone::kNone) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      const ImVec2 delta(mouse.x - s_last_mouse.x, mouse.y - s_last_mouse.y);
      resize_handles_internal::ApplyDragDelta(
          rect, s_active_zone, delta, snap, style.min_width, style.min_height);
      s_last_mouse = mouse;
      ImGui::SetMouseCursor(CursorForZone(s_active_zone));
    } else {
      // Mouse released — commit the drag.
      committed = true;
      s_active_zone = HandleZone::kNone;
      s_active_id = 0;
    }
  } else if (hovered_zone != HandleZone::kNone &&
             ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    s_active_id = id;
    s_active_zone = hovered_zone;
    s_last_mouse = mouse;
    ImGui::SetMouseCursor(CursorForZone(hovered_zone));
  } else if (hovered_zone != HandleZone::kNone) {
    ImGui::SetMouseCursor(CursorForZone(hovered_zone));
  }

  // Draw handles.
  ImVec4 color = style.color;
  if (color.w == 0.0f) {
    color = resize_handles_internal::GetDefaultHandleColor();
  }
  const ImU32 color_u32 = ImGui::ColorConvertFloat4ToU32(color);

  ImDrawList* draw_list = window->DrawList;
  static constexpr HandleZone kAllZones[] = {
      HandleZone::kNW, HandleZone::kN,  HandleZone::kNE, HandleZone::kW,
      HandleZone::kE,  HandleZone::kSW, HandleZone::kS,  HandleZone::kSE,
  };
  static constexpr HandleMask kZoneMask[] = {
      HandleMask::kNW, HandleMask::kN,  HandleMask::kNE, HandleMask::kW,
      HandleMask::kE,  HandleMask::kSW, HandleMask::kS,  HandleMask::kSE,
  };
  for (std::size_t i = 0; i < 8; ++i) {
    if (!Has(mask, kZoneMask[i]))
      continue;
    const ImRect r = HandleRect(kAllZones[i], *rect, style.handle_size);
    draw_list->AddRectFilled(r.Min, r.Max, color_u32);
  }

  return committed;
}

}  // namespace gui
}  // namespace yaze
