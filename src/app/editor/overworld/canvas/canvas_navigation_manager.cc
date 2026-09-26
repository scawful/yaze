// imgui_internal.h (InnerRect) requires the math operators.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "app/editor/overworld/canvas/canvas_navigation_manager.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "absl/status/status.h"
#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/editor/overworld/overworld_map_status.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/platform_keys.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

// =============================================================================
// Anonymous helpers (moved from overworld_editor.cc)
// =============================================================================

namespace {

std::optional<int> MapFromCanvasPosition(const CanvasNavigationContext& ctx,
                                         ImVec2 scaled_position) {
  if (!ctx.ow_map_canvas || !ctx.overworld || !ctx.current_world) {
    return std::nullopt;
  }
  // Use the same bounds and scale validation as right-click targeting. Integer
  // truncation previously mapped small negative positions onto screen zero.
  const auto target = ResolveOverworldContextTarget(
      *ctx.current_world, 0, scaled_position, ImVec2(0, 0), ImVec2(0, 0),
      ctx.ow_map_canvas->global_scale());
  if (!target || !ctx.overworld->overworld_map(target->map_id)) {
    return std::nullopt;
  }
  return target->map_id;
}

void SetHoveredMap(const CanvasNavigationContext& ctx, int map_id) {
  if (ctx.hovered_map) {
    *ctx.hovered_map = map_id;
  }
}

void SelectMapFallback(const CanvasNavigationContext& ctx, int map_id,
                       bool respect_pin) {
  if (!ctx.current_map || !ctx.current_world || !ctx.current_parent ||
      !ctx.current_map_lock || !ctx.overworld) {
    return;
  }
  if (respect_pin && *ctx.current_map_lock) {
    return;
  }
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return;
  }
  const auto* map = ctx.overworld->overworld_map(map_id);
  if (!map) {
    return;
  }
  *ctx.current_map = map_id;
  *ctx.current_world = std::clamp(map_id / 0x40, 0, 2);
  *ctx.current_parent = map->parent();
  ctx.overworld->set_current_map(map_id);
  ctx.overworld->set_current_world(*ctx.current_world);
}

void SelectMapForEditing(const CanvasNavigationContext& ctx,
                         const CanvasNavigationCallbacks& callbacks, int map_id,
                         bool respect_pin) {
  if (callbacks.select_map_for_editing) {
    callbacks.select_map_for_editing(map_id, respect_pin);
    return;
  }
  SelectMapFallback(ctx, map_id, respect_pin);
}

}  // namespace

// =============================================================================
// Initialization
// =============================================================================

void CanvasNavigationManager::Initialize(
    const CanvasNavigationContext& context,
    const CanvasNavigationCallbacks& callbacks) {
  ctx_ = context;
  callbacks_ = callbacks;
}

// =============================================================================
// Map Detection and Loading
// =============================================================================

std::optional<int> CanvasNavigationManager::TrackMapAtCanvasPosition(
    ImVec2 scaled_position) {
  const auto hovered_map = MapFromCanvasPosition(ctx_, scaled_position);
  SetHoveredMap(ctx_, hovered_map.value_or(-1));
  if (hover_follow_suspended_at_) {
    const ImVec2 mouse =
        ImGui::GetCurrentContext() ? ImGui::GetIO().MousePos : ImVec2(0, 0);
    if (mouse.x == hover_follow_suspended_at_->x &&
        mouse.y == hover_follow_suspended_at_->y) {
      return hovered_map;
    }
    hover_follow_suspended_at_.reset();
  }
  if (hovered_map && ctx_.current_map && ctx_.current_map_lock &&
      !*ctx_.current_map_lock && *ctx_.current_map != *hovered_map &&
      !(ctx_.is_dragging_entity && *ctx_.is_dragging_entity)) {
    SelectMapForEditing(ctx_, callbacks_, *hovered_map, true);
  }
  return hovered_map;
}

absl::Status CanvasNavigationManager::CheckForCurrentMap() {
  if (!ctx_.ow_map_canvas || !ctx_.overworld || !ctx_.rom ||
      !ctx_.current_map || !ctx_.current_world || !ctx_.current_parent ||
      !ctx_.current_map_lock || !ctx_.maps_bmp || !ctx_.current_mode) {
    return absl::OkStatus();
  }

  // Leave / no-hover must clear preview state so status falls back to selection.
  // hover_mouse_pos() can retain the last in-canvas point after the cursor
  // exits, so do not trust MapFromCanvasPosition unless we are still hovering.
  if (!ctx_.ow_map_canvas->IsMouseHovering()) {
    SetHoveredMap(ctx_, -1);
    return absl::OkStatus();
  }

  const int large_map_size = 1024;

  const auto hovered_map =
      TrackMapAtCanvasPosition(ctx_.ow_map_canvas->hover_mouse_pos());
  if (!hovered_map.has_value()) {
    SetHoveredMap(ctx_, -1);
    return absl::OkStatus();
  }
  SetHoveredMap(ctx_, *hovered_map);

  // Lightweight hover identity near the cursor when it differs from selection.
  if (ctx_.current_map && *hovered_map != *ctx_.current_map &&
      ctx_.ow_map_canvas && ctx_.ow_map_canvas->IsMouseHovering()) {
    ImGui::SetTooltip(
        "%s", FormatOverworldMapStatusSegment(*ctx_.current_map, *hovered_map)
                  .c_str());
  }

  // Unpinned selection follows the cursor in every editing mode.
  bool should_build = false;
  if (*hovered_map != last_hovered_map_) {
    last_hovered_map_ = *hovered_map;
    hover_time_ = 0.0f;
    should_build = ctx_.overworld->overworld_map(*hovered_map)->is_built();
  } else {
    hover_time_ += ImGui::GetIO().DeltaTime;
    should_build = (hover_time_ >= kHoverBuildDelay) ||
                   ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                   ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  }

  if (should_build) {
    RETURN_IF_ERROR(ctx_.overworld->EnsureMapBuilt(*hovered_map));
  }

  if (hover_time_ >= kPreloadStartDelay && preload_queue_.empty()) {
    QueueAdjacentMapsForPreload(*hovered_map);
  }

  ProcessPreloadQueue();

  const int current_highlighted_map = *ctx_.current_map;
  if (current_highlighted_map < 0 ||
      current_highlighted_map >= zelda3::kNumOverworldMaps ||
      ctx_.overworld->overworld_map(current_highlighted_map) == nullptr) {
    return absl::OkStatus();
  }

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*ctx_.rom);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

  // Get area size for v3+ ROMs, otherwise use legacy logic
  if (use_v3_area_sizes) {
    using zelda3::AreaSizeEnum;
    auto area_size =
        ctx_.overworld->overworld_map(current_highlighted_map)->area_size();
    const int highlight_parent =
        ctx_.overworld->overworld_map(current_highlighted_map)->parent();

    // Calculate parent map coordinates accounting for world offset
    int parent_map_x;
    int parent_map_y;
    if (*ctx_.current_world == 0) {
      parent_map_x = highlight_parent % 8;
      parent_map_y = highlight_parent / 8;
    } else if (*ctx_.current_world == 1) {
      parent_map_x = (highlight_parent - 0x40) % 8;
      parent_map_y = (highlight_parent - 0x40) / 8;
    } else {
      parent_map_x = (highlight_parent - 0x80) % 8;
      parent_map_y = (highlight_parent - 0x80) / 8;
    }

    // Draw outline based on area size
    switch (area_size) {
      case AreaSizeEnum::LargeArea:
        ctx_.ow_map_canvas->DrawOutline(parent_map_x * kOverworldMapSize,
                                        parent_map_y * kOverworldMapSize,
                                        large_map_size, large_map_size);
        break;
      case AreaSizeEnum::WideArea:
        ctx_.ow_map_canvas->DrawOutline(parent_map_x * kOverworldMapSize,
                                        parent_map_y * kOverworldMapSize,
                                        large_map_size, kOverworldMapSize);
        break;
      case AreaSizeEnum::TallArea:
        ctx_.ow_map_canvas->DrawOutline(parent_map_x * kOverworldMapSize,
                                        parent_map_y * kOverworldMapSize,
                                        kOverworldMapSize, large_map_size);
        break;
      case AreaSizeEnum::SmallArea:
      default:
        ctx_.ow_map_canvas->DrawOutline(parent_map_x * kOverworldMapSize,
                                        parent_map_y * kOverworldMapSize,
                                        kOverworldMapSize, kOverworldMapSize);
        break;
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (ctx_.overworld->overworld_map(current_highlighted_map)
            ->is_large_map() ||
        ctx_.overworld->overworld_map(current_highlighted_map)->large_index() !=
            0) {
      const int highlight_parent =
          ctx_.overworld->overworld_map(current_highlighted_map)->parent();

      int parent_map_x;
      int parent_map_y;
      if (*ctx_.current_world == 0) {
        parent_map_x = highlight_parent % 8;
        parent_map_y = highlight_parent / 8;
      } else if (*ctx_.current_world == 1) {
        parent_map_x = (highlight_parent - 0x40) % 8;
        parent_map_y = (highlight_parent - 0x40) / 8;
      } else {
        parent_map_x = (highlight_parent - 0x80) % 8;
        parent_map_y = (highlight_parent - 0x80) / 8;
      }

      ctx_.ow_map_canvas->DrawOutline(parent_map_x * kOverworldMapSize,
                                      parent_map_y * kOverworldMapSize,
                                      large_map_size, large_map_size);
    } else {
      int current_map_x;
      int current_map_y;
      if (*ctx_.current_world == 0) {
        current_map_x = current_highlighted_map % 8;
        current_map_y = current_highlighted_map / 8;
      } else if (*ctx_.current_world == 1) {
        current_map_x = (current_highlighted_map - 0x40) % 8;
        current_map_y = (current_highlighted_map - 0x40) / 8;
      } else {
        current_map_x = (current_highlighted_map - 0x80) % 8;
        current_map_y = (current_highlighted_map - 0x80) / 8;
      }
      ctx_.ow_map_canvas->DrawOutline(current_map_x * kOverworldMapSize,
                                      current_map_y * kOverworldMapSize,
                                      kOverworldMapSize, kOverworldMapSize);
    }
  }

  // Ensure current map has texture created for rendering
  if (callbacks_.ensure_map_texture) {
    callbacks_.ensure_map_texture(*ctx_.current_map);
    if (*hovered_map != *ctx_.current_map) {
      callbacks_.ensure_map_texture(*hovered_map);
    }
  }

  if ((*ctx_.maps_bmp)[*ctx_.current_map].modified()) {
    if (callbacks_.refresh_overworld_map) {
      callbacks_.refresh_overworld_map();
    }
    if (callbacks_.refresh_tile16_blockset) {
      RETURN_IF_ERROR(callbacks_.refresh_tile16_blockset());
    }

    // Ensure tile16 blockset is fully updated before rendering
    if (ctx_.tile16_blockset && ctx_.tile16_blockset->atlas.is_active()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &ctx_.tile16_blockset->atlas);
    }

    // Update map texture with the traditional direct update approach
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE,
        &(*ctx_.maps_bmp)[*ctx_.current_map]);
    (*ctx_.maps_bmp)[*ctx_.current_map].set_modified(false);
  }

  return absl::OkStatus();
}

// =============================================================================
// Map Interaction
// =============================================================================

bool IsMapSelectClick(const MapClickInput& input) {
  switch (input.mode) {
    case EditingMode::DRAW_TILE:
    case EditingMode::FILL_TILE:
      return input.right_clicked && !input.shift;
    case EditingMode::MOUSE:
      return input.left_released && !input.left_dragged &&
             !input.entity_hovered;
  }
  return false;
}

bool CanvasNavigationManager::SelectMapUnderCursor() {
  if (!ctx_.ow_map_canvas || !ctx_.current_map) {
    return false;
  }
  std::optional<int> map;
  if (ctx_.hovered_map && *ctx_.hovered_map >= 0) {
    map = *ctx_.hovered_map;
  } else {
    map = MapFromCanvasPosition(ctx_, ctx_.ow_map_canvas->hover_mouse_pos());
  }
  if (!map) {
    return false;
  }
  if (*ctx_.current_map != *map) {
    SelectMapForEditing(ctx_, callbacks_, *map, /*respect_pin=*/false);
  }
  return true;
}

void CanvasNavigationManager::HandleMapInteraction() {
  if (!ctx_.ow_map_canvas || !ctx_.current_mode || !ctx_.current_map_lock ||
      !ctx_.current_map) {
    return;
  }
  if (!ctx_.ow_map_canvas->IsMouseHovering()) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();
  const float threshold = io.MouseDragThreshold;
  MapClickInput input;
  input.mode = *ctx_.current_mode;
  input.left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  input.left_dragged = io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >=
                       threshold * threshold;
  input.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  input.shift = io.KeyShift;
  input.entity_hovered =
      callbacks_.is_entity_hovered && callbacks_.is_entity_hovered();

  // Paint modes: the right-click Tile16 sample (and right-drag brush capture)
  // stays in TilePaintingManager::CheckForSelectRectangle, which runs after
  // this; sampling here too made every right-click select the tile twice.
  if (IsMapSelectClick(input)) {
    (void)SelectMapUnderCursor();
  }

  if (*ctx_.current_mode == EditingMode::MOUSE && !input.entity_hovered &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      callbacks_.open_map_properties) {
    callbacks_.open_map_properties();
  }

  // Middle-drag is exclusively navigation. Pinning lives in the toolbar,
  // Ctrl+L shortcut, and context menu so panning cannot silently freeze tracking.
}

// =============================================================================
// Pan and Zoom
// =============================================================================

ImVec2 StickyWheelPan::Consume(ImVec2 wheel, float dt, float step_px,
                               float snap_px) {
  if (std::fabs(wheel.x) < kOverworldWheelDeadzone)
    wheel.x = 0.0f;
  if (std::fabs(wheel.y) < kOverworldWheelDeadzone)
    wheel.y = 0.0f;
  if (wheel.x == 0.0f && wheel.y == 0.0f) {
    idle_seconds += dt;
    if (idle_seconds >= kOverworldWheelIdleResetSec) {
      residual = ImVec2(0.0f, 0.0f);
    }
    return ImVec2(0.0f, 0.0f);
  }
  idle_seconds = 0.0f;

  const float snap = std::max(snap_px, 1.0f);
  auto axis = [&](float input, float& rest) -> float {
    if (input == 0.0f)
      return 0.0f;
    // Wheel up/left (positive) scrolls toward the origin.
    const float delta = -input * step_px;
    if ((delta > 0.0f) != (rest > 0.0f) && rest != 0.0f) {
      rest = 0.0f;  // Reversal: respond immediately, no leftover travel.
    }
    rest += delta;
    const float out = std::trunc(rest / snap) * snap;
    rest -= out;
    return out;
  };
  return ImVec2(axis(wheel.x, residual.x), axis(wheel.y, residual.y));
}

ImVec2 ScrollForZoomAtAnchor(ImVec2 scroll, ImVec2 anchor, float old_scale,
                             float new_scale) {
  if (old_scale <= 0.0f || new_scale <= 0.0f) {
    return scroll;
  }
  const float ratio = new_scale / old_scale;
  return ImVec2((scroll.x + anchor.x) * ratio - anchor.x,
                (scroll.y + anchor.y) * ratio - anchor.y);
}

ImVec2 CanvasNavigationManager::ContentSize() const {
  const float scale =
      ctx_.ow_map_canvas ? ctx_.ow_map_canvas->global_scale() : 1.0f;
  constexpr float kWorldSize = kOverworldMapSize * 8.0f;  // 4096
  return ImVec2(kWorldSize * scale, kWorldSize * scale);
}

ImVec2 CanvasNavigationManager::ClampScroll(ImVec2 scroll) const {
  const ImVec2 content = ContentSize();
  const float max_x = std::max(0.0f, content.x - viewport_size_.x);
  const float max_y = std::max(0.0f, content.y - viewport_size_.y);
  // Whole pixels: fractional scroll makes pixel art shimmer while moving.
  return ImVec2(std::round(std::clamp(scroll.x, 0.0f, max_x)),
                std::round(std::clamp(scroll.y, 0.0f, max_y)));
}

void CanvasNavigationManager::SetScaleAboutAnchor(float new_scale,
                                                  ImVec2 anchor) {
  if (!ctx_.ow_map_canvas)
    return;
  const float old_scale = ctx_.ow_map_canvas->global_scale();
  new_scale = std::clamp(new_scale, kOverworldMinZoom, kOverworldMaxZoom);
  if (new_scale == old_scale)
    return;
  const ImVec2 base = pending_scroll_.value_or(last_scroll_);
  ctx_.ow_map_canvas->set_global_scale(new_scale);
  pending_scroll_ = ScrollForZoomAtAnchor(base, anchor, old_scale, new_scale);
}

void CanvasNavigationManager::ZoomBySteps(int steps, ImVec2 anchor) {
  if (!ctx_.ow_map_canvas || steps == 0)
    return;
  SetScaleAboutAnchor(
      ctx_.ow_map_canvas->global_scale() + steps * kOverworldZoomStep, anchor);
}

void CanvasNavigationManager::BeginCanvasViewport() {
  if (!ctx_.ow_map_canvas) {
    return;
  }
  const ImGuiIO& io = ImGui::GetIO();
  std::optional<ImVec2> target = pending_scroll_;
  pending_scroll_.reset();

  // ---- Drag pan -------------------------------------------------------------
  // Only a press that lands on the canvas starts a pan; drags that begin on
  // scrollbars, other panels or popups never move the map.
  const bool over_entity =
      callbacks_.is_entity_hovered && callbacks_.is_entity_hovered();
  const bool dragging_entity =
      ctx_.is_dragging_entity && *ctx_.is_dragging_entity;
  if (pan_button_ < 0 && canvas_item_hovered_) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      pan_button_ = ImGuiMouseButton_Middle;
      pan_active_ = true;  // Middle only pans: follow the cursor at once.
    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
               ctx_.current_mode && *ctx_.current_mode == EditingMode::MOUSE &&
               !over_entity && !dragging_entity) {
      pan_button_ = ImGuiMouseButton_Left;
      pan_active_ = false;  // Becomes a pan once past the drag threshold.
    }
    if (pan_button_ >= 0) {
      pan_anchor_mouse_ = io.MousePos;
      pan_anchor_scroll_ = last_scroll_;
    }
  }
  if (pan_button_ >= 0) {
    if (!ImGui::IsMouseDown(pan_button_)) {
      pan_button_ = -1;
      pan_active_ = false;
    } else {
      if (!pan_active_ && ImGui::IsMouseDragging(pan_button_)) {
        pan_active_ = !dragging_entity;
      }
      if (pan_active_) {
        // Content stays pinned under the grab point: no drift, no inertia.
        target = ImVec2(
            pan_anchor_scroll_.x - (io.MousePos.x - pan_anchor_mouse_.x),
            pan_anchor_scroll_.y - (io.MousePos.y - pan_anchor_mouse_.y));
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
      }
    }
  }

  // ---- Wheel: pan in detents, Cmd/Ctrl+wheel zooms at the cursor -------------
  const bool zoom_modifier =
      io.KeyCtrl || (gui::IsMacPlatform() && io.KeySuper);
  if (canvas_window_hovered_ && !pan_active_) {
    ImVec2 wheel(io.MouseWheelH, io.MouseWheel);
    if (!gui::IsMacPlatform() && io.KeyShift && wheel.x == 0.0f) {
      wheel = ImVec2(wheel.y, 0.0f);  // Shift+wheel scrolls horizontally.
    }
    if (zoom_modifier) {
      if (wheel.y != 0.0f) {
        zoom_idle_seconds_ = 0.0f;
        zoom_residual_ += wheel.y;
        const int steps = static_cast<int>(
            std::trunc(zoom_residual_ / kOverworldWheelZoomUnitsPerStep));
        if (steps != 0) {
          zoom_residual_ -= steps * kOverworldWheelZoomUnitsPerStep;
          if (target) {
            pending_scroll_ = target;
          }
          const ImVec2 anchor(io.MousePos.x - viewport_min_.x,
                              io.MousePos.y - viewport_min_.y);
          ZoomBySteps(steps, anchor);
          target = pending_scroll_;
          pending_scroll_.reset();
        }
      }
    } else {
      const float scale = ctx_.ow_map_canvas->global_scale();
      const float snap = std::max(1.0f, kOverworldPanSnapMapPx * scale);
      const float step =
          std::max(snap, std::round(kOverworldWheelPanPx / snap) * snap);
      const ImVec2 delta = wheel_pan_.Consume(wheel, io.DeltaTime, step, snap);
      if (delta.x != 0.0f || delta.y != 0.0f) {
        const ImVec2 base = target.value_or(last_scroll_);
        target = ImVec2(base.x + delta.x, base.y + delta.y);
      }
    }
  }
  if (!zoom_modifier || io.MouseWheel == 0.0f) {
    zoom_idle_seconds_ += io.DeltaTime;
    if (zoom_idle_seconds_ >= kOverworldWheelIdleResetSec) {
      zoom_residual_ = 0.0f;
    }
  }

  // Explicit content size keeps ImGui's clamp correct on the frame the scale
  // changes (the canvas item that would define it is submitted later).
  ImGui::SetNextWindowContentSize(ContentSize());
  if (target) {
    const ImVec2 clamped = viewport_known_ ? ClampScroll(*target)
                                           : ImVec2(std::max(0.0f, target->x),
                                                    std::max(0.0f, target->y));
    ImGui::SetNextWindowScroll(clamped);
    last_scroll_ = clamped;
  }
}

void CanvasNavigationManager::EndCanvasViewport(bool canvas_item_hovered) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (!window) {
    return;
  }
  viewport_min_ = window->InnerRect.Min;
  viewport_size_ = window->InnerRect.GetSize();
  last_scroll_ = window->Scroll;
  canvas_item_hovered_ = canvas_item_hovered;
  canvas_window_hovered_ = ImGui::IsWindowHovered();
  viewport_known_ = viewport_size_.x > 0.0f && viewport_size_.y > 0.0f;
}

void CanvasNavigationManager::SuspendHoverFollowUntilMouseMoves() {
  hover_follow_suspended_at_ =
      ImGui::GetCurrentContext() ? ImGui::GetIO().MousePos : ImVec2(0, 0);
}

void CanvasNavigationManager::ZoomIn() {
  ZoomBySteps(1, ImVec2(viewport_size_.x * 0.5f, viewport_size_.y * 0.5f));
}

void CanvasNavigationManager::ZoomOut() {
  ZoomBySteps(-1, ImVec2(viewport_size_.x * 0.5f, viewport_size_.y * 0.5f));
}

void CanvasNavigationManager::ZoomToFit() {
  if (!ctx_.ow_map_canvas || !viewport_known_) {
    return;
  }
  constexpr float kWorldSize = kOverworldMapSize * 8.0f;
  // The special world only allocates four rows of screens.
  const bool special_world = ctx_.current_world && *ctx_.current_world == 2;
  const float world_height = special_world ? kWorldSize * 0.5f : kWorldSize;
  const float scale =
      std::min(viewport_size_.x / kWorldSize, viewport_size_.y / world_height);
  ctx_.ow_map_canvas->set_global_scale(
      std::clamp(scale, kOverworldMinZoom, kOverworldMaxZoom));
  pending_scroll_ = ImVec2(0.0f, 0.0f);
}

void CanvasNavigationManager::ResetOverworldView() {
  if (ctx_.ow_map_canvas) {
    ctx_.ow_map_canvas->set_global_scale(1.0f);
  }
  pending_scroll_ = ImVec2(0.0f, 0.0f);
}

void CanvasNavigationManager::CenterOverworldView() {
  if (ctx_.current_map) {
    CenterOnMap(*ctx_.current_map);
  }
}

void CanvasNavigationManager::CenterOnMap(int map_id) {
  if (!ctx_.ow_map_canvas || map_id < 0 ||
      map_id >= zelda3::kNumOverworldMaps) {
    return;
  }
  float scale = ctx_.ow_map_canvas->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  const int map_in_world = map_id % 0x40;
  const float center_x =
      ((map_in_world % 8) * kOverworldMapSize + kOverworldMapSize / 2.0f) *
      scale;
  const float center_y =
      ((map_in_world / 8) * kOverworldMapSize + kOverworldMapSize / 2.0f) *
      scale;
  pending_scroll_ = ImVec2(center_x - viewport_size_.x * 0.5f,
                           center_y - viewport_size_.y * 0.5f);
}

// =============================================================================
// Blockset Selector Synchronization
// =============================================================================

void CanvasNavigationManager::ScrollBlocksetCanvasToCurrentTile() {
  if (*ctx_.blockset_selector) {
    (*ctx_.blockset_selector)->ScrollToTile(*ctx_.current_tile16);
    return;
  }

  // CRITICAL FIX: Do NOT use fallback scrolling from overworld canvas context!
  // The fallback code uses ImGui::SetScrollX/Y which scrolls the CURRENT
  // window, and when called from CheckForSelectRectangle() during overworld
  // canvas rendering, it incorrectly scrolls the overworld canvas instead of
  // the tile16 selector.
  //
  // The blockset_selector_ should always be available in modern code paths.
  // If it's not available, we skip scrolling rather than scroll the wrong
  // window.
}

void CanvasNavigationManager::UpdateBlocksetSelectorState() {
  if (!*ctx_.blockset_selector) {
    return;
  }

  (*ctx_.blockset_selector)->SetTileCount(zelda3::kNumTile16Individual);
  (*ctx_.blockset_selector)->SetSelectedTile(*ctx_.current_tile16);
}

// =============================================================================
// Background Pre-loading
// =============================================================================

void CanvasNavigationManager::QueueAdjacentMapsForPreload(int center_map) {
#ifdef __EMSCRIPTEN__
  // WASM: Skip pre-loading entirely - it blocks the main thread and causes
  // stuttering. The tileset cache and debouncing provide enough optimization.
  return;
#endif

  if (center_map < 0 || center_map >= zelda3::kNumOverworldMaps) {
    return;
  }

  preload_queue_.clear();

  // Calculate grid position (8x8 maps per world)
  int world_offset = (center_map / 64) * 64;
  int local_index = center_map % 64;
  int map_x = local_index % 8;
  int map_y = local_index / 8;
  int max_rows = (center_map >= zelda3::kSpecialWorldMapIdStart) ? 4 : 8;

  // Add adjacent maps (4-connected neighbors)
  static const int dx[] = {-1, 1, 0, 0};
  static const int dy[] = {0, 0, -1, 1};

  for (int i = 0; i < 4; ++i) {
    int nx = map_x + dx[i];
    int ny = map_y + dy[i];

    // Check bounds (world grid; special world is only 4 rows high)
    if (nx >= 0 && nx < 8 && ny >= 0 && ny < max_rows) {
      int neighbor_index = world_offset + ny * 8 + nx;
      // Only queue if not already built
      if (neighbor_index >= 0 && neighbor_index < zelda3::kNumOverworldMaps &&
          !ctx_.overworld->overworld_map(neighbor_index)->is_built()) {
        preload_queue_.push_back(neighbor_index);
      }
    }
  }
}

void CanvasNavigationManager::ProcessPreloadQueue() {
#ifdef __EMSCRIPTEN__
  // WASM: Pre-loading disabled - each EnsureMapBuilt call blocks for 100-200ms
  // which causes unacceptable frame drops. Native builds use this for smoother
  // UX.
  return;
#endif

  if (preload_queue_.empty()) {
    return;
  }

  // Process one map per frame to avoid blocking (native only)
  int map_to_preload = preload_queue_.back();
  preload_queue_.pop_back();

  // Silent build - don't update UI state
  auto status = ctx_.overworld->EnsureMapBuilt(map_to_preload);
  if (!status.ok()) {
    // Log but don't interrupt - this is background work
    LOG_DEBUG("CanvasNavigationManager",
              "Background preload of map %d failed: %s", map_to_preload,
              status.message().data());
  }
}

}  // namespace yaze::editor
