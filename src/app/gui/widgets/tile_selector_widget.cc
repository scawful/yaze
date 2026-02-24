#include "app/gui/widgets/tile_selector_widget.h"

#include <algorithm>
#include <cstdio>

#include "app/gui/core/drag_drop.h"

namespace yaze::gui {

namespace {

std::string_view TrimAsciiWhitespace(std::string_view s) {
  while (!s.empty() &&
         (s.front() == ' ' || s.front() == '\t' || s.front() == '\n' ||
          s.front() == '\r')) {
    s.remove_prefix(1);
  }
  while (!s.empty() &&
         (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' ||
          s.back() == '\r')) {
    s.remove_suffix(1);
  }
  return s;
}

// Parse tile-id text using hex by default, with explicit decimal support via
// "d:<value>".
bool ParseTileIdText(std::string_view input, int* out_value) {
  if (!out_value) {
    return false;
  }

  std::string_view trimmed = TrimAsciiWhitespace(input);
  if (trimmed.empty()) {
    return false;
  }

  bool decimal_mode = false;
  if (trimmed.size() >= 2 &&
      (trimmed[0] == 'd' || trimmed[0] == 'D') &&
      trimmed[1] == ':') {
    decimal_mode = true;
    trimmed.remove_prefix(2);
  } else if (trimmed.size() >= 2 && trimmed[0] == '0' &&
             (trimmed[1] == 'x' || trimmed[1] == 'X')) {
    trimmed.remove_prefix(2);
  }

  if (trimmed.empty()) {
    return false;
  }

  std::string text(trimmed);
  unsigned int parsed = 0;
  char trailing = '\0';
  const char* format = decimal_mode ? "%u%c" : "%x%c";
  if (std::sscanf(text.c_str(), format, &parsed, &trailing) != 1) {
    return false;
  }

  *out_value = static_cast<int>(parsed);
  return true;
}

}  // namespace

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
  if (last_jump_result_ == JumpToTileResult::kOutOfRange &&
      IsValidTileId(selected_tile_id_)) {
    last_jump_result_ = JumpToTileResult::kSuccess;
  }
}

void TileSelectorWidget::SetSelectedTile(int tile_id) {
  if (IsValidTileId(tile_id)) {
    selected_tile_id_ = tile_id;
    last_jump_result_ = JumpToTileResult::kSuccess;
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
    if (IsValidTileId(hovered_tile) && IsInFilterRange(hovered_tile)) {
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

TileSelectorWidget::JumpToTileResult TileSelectorWidget::JumpToTileFromInput(
    std::string_view input) {
  int tile_id = -1;
  if (!ParseTileIdText(input, &tile_id)) {
    last_jump_result_ = JumpToTileResult::kInvalidFormat;
    return last_jump_result_;
  }

  if (!IsValidTileId(tile_id)) {
    last_jump_result_ = JumpToTileResult::kOutOfRange;
    return last_jump_result_;
  }

  selected_tile_id_ = tile_id;
  ScrollToTile(tile_id, true);
  last_jump_result_ = JumpToTileResult::kSuccess;
  return last_jump_result_;
}

bool TileSelectorWidget::DrawFilterBar() {
  bool jumped = false;
  const int max_tile_id = GetMaxTileId();

  constexpr ImGuiInputTextFlags kHexFlags =
      ImGuiInputTextFlags_CharsHexadecimal |
      ImGuiInputTextFlags_EnterReturnsTrue |
      ImGuiInputTextFlags_AutoSelectAll;

  ImGui::PushID(widget_id_.c_str());

  // Jump-to-ID input
  ImGui::PushItemWidth(64);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted("Go:");
  ImGui::SameLine();

  if (ImGui::InputText("##TileFilterID", filter_buf_, sizeof(filter_buf_),
                       kHexFlags)) {
    switch (JumpToTileFromInput(filter_buf_)) {
      case JumpToTileResult::kSuccess:
        jumped = true;
        break;
      case JumpToTileResult::kInvalidFormat:
      case JumpToTileResult::kOutOfRange:
        break;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Enter tile ID and press Enter:\n"
        "hex: 1A or 0x1A\n"
        "decimal: d:26");
  }

  ImGui::SameLine();
  ImGui::TextDisabled("/ 0x%03X", max_tile_id);
  if (last_jump_result_ == JumpToTileResult::kInvalidFormat) {
    ImGui::SameLine(0, 8.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Invalid hex ID");
  } else if (last_jump_result_ == JumpToTileResult::kOutOfRange) {
    ImGui::SameLine(0, 8.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "Out of range (max: 0x%03X)", max_tile_id);
  }

  // Range filter inputs
  ImGui::SameLine(0, 12.0f);
  ImGui::TextUnformatted("Range:");
  ImGui::SameLine();

  bool range_changed = false;
  if (ImGui::InputText("##RangeMin", filter_min_buf_, sizeof(filter_min_buf_),
                       kHexFlags)) {
    range_changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Min tile ID. Press Enter to apply.\n"
        "hex: 1A or 0x1A\n"
        "decimal: d:26");
  }
  ImGui::SameLine();
  ImGui::TextUnformatted("-");
  ImGui::SameLine();
  if (ImGui::InputText("##RangeMax", filter_max_buf_, sizeof(filter_max_buf_),
                       kHexFlags)) {
    range_changed = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Max tile ID. Press Enter to apply.\n"
        "hex: 1A or 0x1A\n"
        "decimal: d:26");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(hex, d:dec)");

  if (range_changed) {
    int parsed_min = 0;
    int parsed_max = 0;
    bool has_min = ParseTileIdText(filter_min_buf_, &parsed_min);
    bool has_max = ParseTileIdText(filter_max_buf_, &parsed_max);

    if (has_min && has_max && parsed_min <= parsed_max) {
      SetRangeFilter(parsed_min, parsed_max);
      filter_range_error_ = false;
      if (filter_range_active_) {
        filter_out_of_range_ = false;
        ScrollToTile(filter_range_min_, true);
      } else {
        // SetRangeFilter returned early: both values exceeded total_tiles_.
        filter_out_of_range_ = true;
      }
    } else if (!has_min && !has_max) {
      ClearRangeFilter();
      filter_range_error_ = false;
      filter_out_of_range_ = false;
    } else if (has_min && has_max && parsed_min > parsed_max) {
      // Invalid range: min must be ≤ max
      filter_range_error_ = true;
      filter_out_of_range_ = false;
    }
  }

  // Clear button when range is active
  if (filter_range_active_) {
    ImGui::SameLine();
    if (ImGui::SmallButton("X##ClearRange")) {
      ClearRangeFilter();
      filter_min_buf_[0] = '\0';
      filter_max_buf_[0] = '\0';
      filter_range_error_ = false;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Clear range filter");
    }
  }

  // Validation feedback (shown inline after the filter inputs)
  if (filter_range_error_) {
    ImGui::SameLine(0, 8.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Min must be <= Max");
  } else if (filter_out_of_range_) {
    ImGui::SameLine(0, 8.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       "Out of range (max: 0x%03X)", GetMaxTileId());
  } else if (filter_range_active_) {
    int range_count = filter_range_max_ - filter_range_min_ + 1;
    if (range_count <= 0) {
      ImGui::SameLine(0, 8.0f);
      ImGui::TextDisabled("(no tiles in range)");
    }
  }

  ImGui::PopItemWidth();
  ImGui::PopID();

  return jumped;
}

void TileSelectorWidget::SetRangeFilter(int min_id, int max_id) {
  if (min_id < 0) min_id = 0;
  if (max_id >= total_tiles_) max_id = total_tiles_ - 1;
  if (min_id > max_id) return;

  filter_range_active_ = true;
  filter_range_min_ = min_id;
  filter_range_max_ = max_id;
  filter_range_error_ = false;
}

void TileSelectorWidget::ClearRangeFilter() {
  filter_range_active_ = false;
  filter_range_min_ = 0;
  filter_range_max_ = 0;
  filter_range_error_ = false;
  filter_out_of_range_ = false;
}

bool TileSelectorWidget::IsValidTileId(int tile_id) const {
  return tile_id >= 0 && tile_id < total_tiles_;
}

bool TileSelectorWidget::IsInFilterRange(int tile_id) const {
  if (!filter_range_active_) return true;
  return tile_id >= filter_range_min_ && tile_id <= filter_range_max_;
}

}  // namespace yaze::gui
