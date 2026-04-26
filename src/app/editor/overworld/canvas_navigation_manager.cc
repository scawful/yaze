#include "app/editor/overworld/canvas_navigation_manager.h"

#include <algorithm>
#include <optional>

#include "absl/status/status.h"
#include "app/gfx/resource/arena.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::editor {

// =============================================================================
// Anonymous helpers (moved from overworld_editor.cc)
// =============================================================================

namespace {

// Calculate the total canvas content size based on world layout
ImVec2 CalculateOverworldContentSize(float scale) {
  // 8x8 grid of 512x512 maps = 4096x4096 total
  constexpr float kWorldSize = 512.0f * 8.0f;  // 4096
  return ImVec2(kWorldSize * scale, kWorldSize * scale);
}

// Clamp scroll position to valid bounds
ImVec2 ClampScrollPosition(ImVec2 scroll, ImVec2 content_size,
                           ImVec2 visible_size) {
  float max_scroll_x = std::max(0.0f, content_size.x - visible_size.x);
  float max_scroll_y = std::max(0.0f, content_size.y - visible_size.y);

  float clamped_x = std::clamp(scroll.x, -max_scroll_x, 0.0f);
  float clamped_y = std::clamp(scroll.y, -max_scroll_y, 0.0f);

  return ImVec2(clamped_x, clamped_y);
}

int AllocatedRowsForWorld(int world) {
  const int clamped_world = std::clamp(world, 0, 2);
  const int world_start = clamped_world * 0x40;
  const int maps_available =
      std::clamp(zelda3::kNumOverworldMaps - world_start, 0, 0x40);
  return (maps_available + 7) / 8;
}

std::optional<int> MapFromCanvasPosition(const CanvasNavigationContext& ctx,
                                         ImVec2 scaled_position) {
  if (!ctx.ow_map_canvas || !ctx.overworld || !ctx.current_world) {
    return std::nullopt;
  }

  float scale = ctx.ow_map_canvas->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  const int map_x =
      static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
  const int map_y =
      static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;

  if (map_x < 0 || map_x >= 8 || map_y < 0 || map_y >= 8) {
    return std::nullopt;
  }

  if (map_y >= AllocatedRowsForWorld(*ctx.current_world)) {
    return std::nullopt;
  }

  int map_id = map_x + map_y * 8;
  if (*ctx.current_world == 1) {
    map_id += 0x40;
  } else if (*ctx.current_world == 2) {
    map_id += 0x80;
  }

  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps ||
      ctx.overworld->overworld_map(map_id) == nullptr) {
    return std::nullopt;
  }
  return map_id;
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

absl::Status CanvasNavigationManager::CheckForCurrentMap() {
  if (!ctx_.ow_map_canvas || !ctx_.overworld || !ctx_.rom ||
      !ctx_.current_map || !ctx_.current_world || !ctx_.current_parent ||
      !ctx_.current_map_lock || !ctx_.maps_bmp || !ctx_.current_mode) {
    return absl::OkStatus();
  }

  const int large_map_size = 1024;

  const auto hovered_map =
      MapFromCanvasPosition(ctx_, ctx_.ow_map_canvas->hover_mouse_pos());
  if (!hovered_map.has_value()) {
    SetHoveredMap(ctx_, -1);
    return absl::OkStatus();
  }
  SetHoveredMap(ctx_, *hovered_map);

  // Hover is only a preview/loading signal. The editable map is changed by
  // explicit click selection so toolbar/sidebar fields do not retarget while
  // the cursor crosses another area.
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

  if (*ctx_.current_mode == EditingMode::MOUSE &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    if (callbacks_.refresh_tile16_blockset) {
      RETURN_IF_ERROR(callbacks_.refresh_tile16_blockset());
    }
  }

  return absl::OkStatus();
}

// =============================================================================
// Map Interaction
// =============================================================================

void CanvasNavigationManager::HandleMapInteraction() {
  if (!ctx_.ow_map_canvas || !ctx_.current_mode ||
      !ctx_.show_map_properties_panel || !ctx_.current_map_lock ||
      !ctx_.current_map) {
    return;
  }
  if (!ctx_.ow_map_canvas->IsMouseHovering()) {
    return;
  }

  // Paint-mode eyedropper: right-click samples tile16 under cursor.
  if ((*ctx_.current_mode == EditingMode::DRAW_TILE ||
       *ctx_.current_mode == EditingMode::FILL_TILE) &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    if (callbacks_.pick_tile16_from_hovered_canvas) {
      (void)callbacks_.pick_tile16_from_hovered_canvas();
    }
    return;
  }

  if (*ctx_.current_mode != EditingMode::MOUSE) {
    return;
  }
  if (callbacks_.is_entity_hovered && callbacks_.is_entity_hovered()) {
    return;
  }

  auto map_from_cursor = [&]() -> std::optional<int> {
    if (ctx_.hovered_map && *ctx_.hovered_map >= 0) {
      return *ctx_.hovered_map;
    }
    return MapFromCanvasPosition(ctx_, ctx_.ow_map_canvas->hover_mouse_pos());
  };

  const auto hovered_map = map_from_cursor();
  if (!hovered_map.has_value()) {
    return;
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    SelectMapForEditing(ctx_, callbacks_, *hovered_map, true);
  }

  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    SelectMapForEditing(ctx_, callbacks_, *hovered_map, true);
    *ctx_.show_map_properties_panel = true;
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
    if (*ctx_.current_map_lock && *ctx_.current_map == *hovered_map) {
      *ctx_.current_map_lock = false;
      return;
    }
    SelectMapForEditing(ctx_, callbacks_, *hovered_map, false);
    *ctx_.current_map_lock = true;
    *ctx_.show_map_properties_panel = true;
  }
}

// =============================================================================
// Pan and Zoom
// =============================================================================

void CanvasNavigationManager::HandleOverworldPan() {
  // Determine if panning should occur:
  // 1. Middle-click drag always pans (all modes)
  // 2. Left-click drag pans in mouse mode when not hovering over an entity
  bool should_pan = false;

  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    should_pan = true;
  } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
             *ctx_.current_mode == EditingMode::MOUSE) {
    // In mouse mode, left-click pans unless hovering over an entity
    bool over_entity =
        callbacks_.is_entity_hovered && callbacks_.is_entity_hovered();
    // Also don't pan if we're currently dragging an entity
    if (!over_entity && !*ctx_.is_dragging_entity) {
      should_pan = true;
    }
  }

  if (!should_pan) {
    return;
  }

  // Pan by adjusting ImGui's scroll position (scrollbars handle actual scroll)
  ImVec2 delta = ImGui::GetIO().MouseDelta;
  float new_scroll_x = ImGui::GetScrollX() - delta.x;
  float new_scroll_y = ImGui::GetScrollY() - delta.y;

  // Get scroll limits from ImGui
  float max_scroll_x = ImGui::GetScrollMaxX();
  float max_scroll_y = ImGui::GetScrollMaxY();

  // Clamp to valid scroll range
  new_scroll_x = std::clamp(new_scroll_x, 0.0f, max_scroll_x);
  new_scroll_y = std::clamp(new_scroll_y, 0.0f, max_scroll_y);

  ImGui::SetScrollX(new_scroll_x);
  ImGui::SetScrollY(new_scroll_y);
}

void CanvasNavigationManager::HandleOverworldZoom() {
  // Scroll wheel is reserved for canvas navigation/panning
  // Use toolbar buttons or context menu for zoom control
}

void CanvasNavigationManager::ZoomIn() {
  float new_scale =
      std::min(kOverworldMaxZoom,
               ctx_.ow_map_canvas->global_scale() + kOverworldZoomStep);
  ctx_.ow_map_canvas->set_global_scale(new_scale);
  // Scroll will be clamped automatically by ImGui on next frame
}

void CanvasNavigationManager::ZoomOut() {
  float new_scale =
      std::max(kOverworldMinZoom,
               ctx_.ow_map_canvas->global_scale() - kOverworldZoomStep);
  ctx_.ow_map_canvas->set_global_scale(new_scale);
  // Scroll will be clamped automatically by ImGui on next frame
}

void CanvasNavigationManager::ClampOverworldScroll() {
  // ImGui handles scroll clamping automatically via GetScrollMaxX/Y
  // This function is now a no-op but kept for API compatibility
}

void CanvasNavigationManager::ResetOverworldView() {
  // Reset ImGui scroll to top-left
  ImGui::SetScrollX(0);
  ImGui::SetScrollY(0);
  ctx_.ow_map_canvas->set_global_scale(1.0f);
}

void CanvasNavigationManager::CenterOverworldView() {
  // Center the view on the current map
  float scale = ctx_.ow_map_canvas->global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Calculate map position within the world
  int map_in_world = *ctx_.current_map % 0x40;
  int map_x = (map_in_world % 8) * kOverworldMapSize;
  int map_y = (map_in_world / 8) * kOverworldMapSize;

  // Get viewport size
  ImVec2 viewport_px = ImGui::GetContentRegionAvail();

  // Calculate scroll to center the current map (in ImGui's positive scroll
  // space)
  float center_x = (map_x + kOverworldMapSize / 2.0f) * scale;
  float center_y = (map_y + kOverworldMapSize / 2.0f) * scale;

  float scroll_x = center_x - viewport_px.x / 2.0f;
  float scroll_y = center_y - viewport_px.y / 2.0f;

  // Clamp to valid scroll range
  scroll_x = std::clamp(scroll_x, 0.0f, ImGui::GetScrollMaxX());
  scroll_y = std::clamp(scroll_y, 0.0f, ImGui::GetScrollMaxY());

  ImGui::SetScrollX(scroll_x);
  ImGui::SetScrollY(scroll_y);
}

void CanvasNavigationManager::CheckForMousePan() {
  // Legacy wrapper - now calls HandleOverworldPan
  HandleOverworldPan();
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
