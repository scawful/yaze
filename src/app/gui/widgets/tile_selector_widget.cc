#include "app/gui/widgets/tile_selector_widget.h"

#include <algorithm>
#include <cstdio>

#include "app/gui/core/drag_drop.h"

namespace yaze::gui {

TileSelectorWidget::TileSelectorWidget(std::string widget_id)
    : config_(),
      total_tiles_(config_.total_tiles),
      widget_id_(std::move(widget_id)) {}

TileSelectorWidget::TileSelectorWidget(std::string widget_id, Config config)
    : config_(config),
      total_tiles_(config.total_tiles),
      widget_id_(std::move(widget_id)) {}

void TileSelectorWidget::AttachCanvas(Canvas* canvas) {
  canvas_ = canvas;
}

void TileSelectorWidget::SetTileCount(int total_tiles) {
  total_tiles_ = std::max(total_tiles, 0);
  if (!IsValidTileId(selected_tile_id_)) {
    selected_tile_id_ = 0;
  }
}

void TileSelectorWidget::SetSelectedTile(int tile_id) {
  if (IsValidTileId(tile_id)) {
    selected_tile_id_ = tile_id;
  }
}

TileSelectorWidget::RenderResult TileSelectorWidget::Render(gfx::Bitmap& atlas,
                                                            bool atlas_ready) {
  RenderResult result;

  if (!canvas_) {
    return result;
  }

  const int tile_display_size =
      static_cast<int>(config_.tile_size * config_.display_scale);

  // Calculate total content size for ImGui child window scrolling
  const int num_rows =
      (total_tiles_ + config_.tiles_per_row - 1) / config_.tiles_per_row;
  const ImVec2 content_size(
      config_.tiles_per_row * tile_display_size + config_.draw_offset.x * 2,
      num_rows * tile_display_size + config_.draw_offset.y * 2);

  // Set content size for ImGui child window (must be called before
  // DrawBackground)
  ImGui::SetCursorPos(ImVec2(0, 0));
  ImGui::Dummy(content_size);
  ImGui::SetCursorPos(ImVec2(0, 0));

  // Handle pending scroll (deferred from ScrollToTile call outside render
  // context)
  if (pending_scroll_tile_id_ >= 0) {
    if (IsValidTileId(pending_scroll_tile_id_)) {
      const ImVec2 target = TileOrigin(pending_scroll_tile_id_);
      if (pending_scroll_use_imgui_) {
        const ImVec2 window_size = ImGui::GetWindowSize();
        float scroll_x =
            target.x - (window_size.x / 2.0f) + (tile_display_size / 2.0f);
        float scroll_y =
            target.y - (window_size.y / 2.0f) + (tile_display_size / 2.0f);
        scroll_x = std::max(0.0f, scroll_x);
        scroll_y = std::max(0.0f, scroll_y);
        ImGui::SetScrollX(scroll_x);
        ImGui::SetScrollY(scroll_y);
      }
    }
    pending_scroll_tile_id_ = -1;  // Clear pending scroll
  }

  canvas_->DrawBackground();
  canvas_->DrawContextMenu();

  if (atlas_ready && atlas.is_active()) {
    canvas_->DrawBitmap(atlas, static_cast<int>(config_.draw_offset.x),
                        static_cast<int>(config_.draw_offset.y),
                        config_.display_scale);

    result = HandleInteraction(tile_display_size);

    // Hover tooltip: show tile ID and zoomed preview
    if (config_.show_hover_tooltip && ImGui::IsItemHovered()) {
      int hovered_tile = ResolveTileAtCursor(tile_display_size);
      if (IsValidTileId(hovered_tile)) {
        ImGui::BeginTooltip();
        ImGui::Text("Tile %d (0x%03X)", hovered_tile, hovered_tile);

        // Extract and draw a zoomed preview of the hovered tile
        int tile_col = hovered_tile % config_.tiles_per_row;
        int tile_row = hovered_tile / config_.tiles_per_row;
        int src_x = tile_col * config_.tile_size;
        int src_y = tile_row * config_.tile_size;

        // Use ImGui texture coords for the atlas to show the tile zoomed
        auto* texture_id = atlas.texture();
        if (texture_id != nullptr) {
          float atlas_w = static_cast<float>(atlas.width());
          float atlas_h = static_cast<float>(atlas.height());
          if (atlas_w > 0 && atlas_h > 0) {
            ImVec2 uv0(src_x / atlas_w, src_y / atlas_h);
            ImVec2 uv1((src_x + config_.tile_size) / atlas_w,
                       (src_y + config_.tile_size) / atlas_h);
            float preview_size = config_.tile_size * 4.0f;
            ImGui::Image((ImTextureID)(intptr_t)texture_id,
                         ImVec2(preview_size, preview_size), uv0, uv1);
          }
        }

        ImGui::EndTooltip();
      }
    }

    if (config_.show_tile_ids) {
      DrawTileIdLabels(tile_display_size);
    }

    DrawHighlight(tile_display_size);
  }

  canvas_->DrawGrid();
  canvas_->DrawOverlay();

  return result;
}

TileSelectorWidget::RenderResult TileSelectorWidget::HandleInteraction(
    int tile_display_size) {
  RenderResult result;

  if (!ImGui::IsItemHovered()) {
    return result;
  }

  const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  const bool double_clicked =
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

  if (clicked || double_clicked) {
    const int hovered_tile = ResolveTileAtCursor(tile_display_size);
    if (IsValidTileId(hovered_tile)) {
      result.tile_clicked = clicked;
      result.tile_double_clicked = double_clicked;
      if (hovered_tile != selected_tile_id_) {
        selected_tile_id_ = hovered_tile;
        result.selection_changed = true;
      }
      result.selected_tile = selected_tile_id_;
    }
  }

  // Emit drag source for the selected tile when dragging is enabled
  if (config_.enable_drag && IsValidTileId(selected_tile_id_)) {
    result.tile_dragging = BeginTileDragSource(selected_tile_id_,
                                               config_.drag_source_map_id);
  }

  return result;
}

int TileSelectorWidget::ResolveTileAtCursor(int tile_display_size) const {
  if (!canvas_) {
    return -1;
  }

  const ImVec2 screen_pos = ImGui::GetIO().MousePos;
  const ImVec2 origin = canvas_->zero_point();
  const ImVec2 scroll = canvas_->scrolling();

  // Convert screen position to canvas content position (accounting for scroll)
  ImVec2 local =
      ImVec2(screen_pos.x - origin.x - config_.draw_offset.x - scroll.x,
             screen_pos.y - origin.y - config_.draw_offset.y - scroll.y);

  if (local.x < 0.0f || local.y < 0.0f) {
    return -1;
  }

  const int column = static_cast<int>(local.x / tile_display_size);
  const int row = static_cast<int>(local.y / tile_display_size);

  return row * config_.tiles_per_row + column;
}

void TileSelectorWidget::DrawHighlight(int tile_display_size) const {
  if (!canvas_ || !IsValidTileId(selected_tile_id_)) {
    return;
  }

  const int column = selected_tile_id_ % config_.tiles_per_row;
  const int row = selected_tile_id_ / config_.tiles_per_row;

  const float x = config_.draw_offset.x + column * tile_display_size;
  const float y = config_.draw_offset.y + row * tile_display_size;

  canvas_->DrawOutlineWithColor(static_cast<int>(x), static_cast<int>(y),
                                tile_display_size, tile_display_size,
                                config_.highlight_color);
}

void TileSelectorWidget::DrawTileIdLabels(int) const {
  // Future enhancement: draw ImGui text overlay with tile indices.
}

void TileSelectorWidget::ScrollToTile(int tile_id, bool use_imgui_scroll) {
  if (!canvas_ || !IsValidTileId(tile_id)) {
    return;
  }

  // Defer scroll until next render (when we're in the correct ImGui window
  // context)
  pending_scroll_tile_id_ = tile_id;
  pending_scroll_use_imgui_ = use_imgui_scroll;
}

ImVec2 TileSelectorWidget::TileOrigin(int tile_id) const {
  if (!IsValidTileId(tile_id)) {
    return ImVec2(-1, -1);
  }
  const int tile_display_size =
      static_cast<int>(config_.tile_size * config_.display_scale);
  const int column = tile_id % config_.tiles_per_row;
  const int row = tile_id / config_.tiles_per_row;
  return ImVec2(config_.draw_offset.x + column * tile_display_size,
                config_.draw_offset.y + row * tile_display_size);
}

bool TileSelectorWidget::DrawFilterBar() {
  bool jumped = false;
  const int max_tile_id = GetMaxTileId();

  ImGui::PushItemWidth(80);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Go:");
  ImGui::SameLine();

  constexpr ImGuiInputTextFlags kHexFlags =
      ImGuiInputTextFlags_CharsHexadecimal |
      ImGuiInputTextFlags_EnterReturnsTrue |
      ImGuiInputTextFlags_AutoSelectAll;

  ImGui::PushID(widget_id_.c_str());
  if (ImGui::InputText("##TileFilterID", filter_buf_, sizeof(filter_buf_),
                       kHexFlags)) {
    unsigned int parsed = 0;
    if (std::sscanf(filter_buf_, "%x", &parsed) == 1) {
      int tile_id = static_cast<int>(parsed);
      if (IsValidTileId(tile_id)) {
        selected_tile_id_ = tile_id;
        ScrollToTile(tile_id, true);
        jumped = true;
      }
    }
  }
  ImGui::PopID();
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enter hex tile ID and press Enter to jump");
  }

  ImGui::SameLine();
  ImGui::TextDisabled("/ max %d (0x%03X)", max_tile_id, max_tile_id);
  ImGui::PopItemWidth();

  return jumped;
}

bool TileSelectorWidget::IsValidTileId(int tile_id) const {
  return tile_id >= 0 && tile_id < total_tiles_;
}

}  // namespace yaze::gui
