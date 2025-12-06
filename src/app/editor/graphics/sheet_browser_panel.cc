#include "app/editor/graphics/sheet_browser_panel.h"

#include <cstdlib>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void SheetBrowserPanel::Initialize() {
  // Initialize with sensible defaults
  thumbnail_scale_ = 2.0f;
  columns_ = 2;
}

void SheetBrowserPanel::Draw(bool* p_open) {
  // EditorPanel interface - delegate to existing Update() logic
  DrawSearchBar();
  ImGui::Separator();
  DrawBatchOperations();
  ImGui::Separator();
  DrawSheetGrid();
}

absl::Status SheetBrowserPanel::Update() {
  DrawSearchBar();
  ImGui::Separator();
  DrawBatchOperations();
  ImGui::Separator();
  DrawSheetGrid();
  return absl::OkStatus();
}

void SheetBrowserPanel::DrawSearchBar() {
  ImGui::Text("Search:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  if (ImGui::InputText("##SheetSearch", search_buffer_, sizeof(search_buffer_),
                       ImGuiInputTextFlags_CharsHexadecimal)) {
    // Parse hex input for sheet number
    if (strlen(search_buffer_) > 0) {
      int value = static_cast<int>(strtol(search_buffer_, nullptr, 16));
      if (value >= 0 && value <= 222) {
        state_->SelectSheet(static_cast<uint16_t>(value));
      }
    }
  }
  HOVER_HINT("Enter hex sheet number (00-DE)");

  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::DragInt("##FilterMin", &filter_min_, 1.0f, 0, 222, "%02X");
  ImGui::SameLine();
  ImGui::Text("-");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(60);
  ImGui::DragInt("##FilterMax", &filter_max_, 1.0f, 0, 222, "%02X");

  ImGui::SameLine();
  ImGui::Checkbox("Modified", &show_only_modified_);
  HOVER_HINT("Show only modified sheets");
}

void SheetBrowserPanel::DrawBatchOperations() {
  if (ImGui::Button(ICON_MD_SELECT_ALL " Select All")) {
    for (int i = filter_min_; i <= filter_max_; i++) {
      state_->selected_sheets.insert(static_cast<uint16_t>(i));
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DESELECT " Clear")) {
    state_->selected_sheets.clear();
  }

  if (!state_->selected_sheets.empty()) {
    ImGui::SameLine();
    ImGui::Text("(%zu selected)", state_->selected_sheets.size());
  }

  // Thumbnail size slider
  ImGui::SameLine();
  ImGui::SetNextItemWidth(100);
  ImGui::SliderFloat("##Scale", &thumbnail_scale_, 1.0f, 4.0f, "%.1fx");
}

void SheetBrowserPanel::DrawSheetGrid() {
  ImGui::BeginChild("##SheetGridChild", ImVec2(0, 0), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);

  auto& sheets = gfx::Arena::Get().gfx_sheets();

  // Calculate thumbnail size
  const float thumb_width = 128 * thumbnail_scale_;
  const float thumb_height = 32 * thumbnail_scale_;
  const float padding = 4.0f;

  // Calculate columns based on available width
  float available_width = ImGui::GetContentRegionAvail().x;
  columns_ = std::max(1, static_cast<int>(available_width / (thumb_width + padding * 2)));

  int col = 0;
  for (int i = filter_min_; i <= filter_max_ && i < zelda3::kNumGfxSheets; i++) {
    // Filter by modification state if enabled
    if (show_only_modified_ &&
        state_->modified_sheets.find(static_cast<uint16_t>(i)) ==
            state_->modified_sheets.end()) {
      continue;
    }

    if (col > 0) {
      ImGui::SameLine();
    }

    ImGui::PushID(i);
    DrawSheetThumbnail(i, sheets[i]);
    ImGui::PopID();

    col++;
    if (col >= columns_) {
      col = 0;
    }
  }

  ImGui::EndChild();
}

void SheetBrowserPanel::DrawSheetThumbnail(int sheet_id, gfx::Bitmap& bitmap) {
  const float thumb_width = 128 * thumbnail_scale_;
  const float thumb_height = 32 * thumbnail_scale_;

  bool is_selected = state_->current_sheet_id == static_cast<uint16_t>(sheet_id);
  bool is_multi_selected =
      state_->selected_sheets.count(static_cast<uint16_t>(sheet_id)) > 0;
  bool is_modified =
      state_->modified_sheets.count(static_cast<uint16_t>(sheet_id)) > 0;

  // Selection highlight
  if (is_selected) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.5f, 0.8f, 0.3f));
  } else if (is_multi_selected) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.5f, 0.5f, 0.2f, 0.3f));
  }

  ImGui::BeginChild(absl::StrFormat("##Sheet%02X", sheet_id).c_str(),
                    ImVec2(thumb_width + 8, thumb_height + 24), true,
                    ImGuiWindowFlags_NoScrollbar);

  gui::BitmapPreviewOptions preview_opts;
  preview_opts.canvas_size = ImVec2(thumb_width + 1, thumb_height + 1);
  preview_opts.dest_pos = ImVec2(2, 2);
  preview_opts.dest_size = ImVec2(thumb_width - 2, thumb_height - 2);
  preview_opts.grid_step = 8.0f * thumbnail_scale_;
  preview_opts.draw_context_menu = false;
  preview_opts.ensure_texture = true;

  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = preview_opts.canvas_size;
  frame_opts.draw_context_menu = preview_opts.draw_context_menu;
  frame_opts.draw_grid = preview_opts.draw_grid;
  frame_opts.grid_step = preview_opts.grid_step;
  frame_opts.draw_overlay = preview_opts.draw_overlay;
  frame_opts.render_popups = preview_opts.render_popups;

  {
    auto rt = gui::BeginCanvas(thumbnail_canvas_, frame_opts);
    gui::DrawBitmapPreview(rt, bitmap, preview_opts);

    // Sheet label with modification indicator
    std::string label = absl::StrFormat("%02X", sheet_id);
    if (is_modified) {
      label += "*";
    }

    // Draw label with background
    ImVec2 text_pos = ImGui::GetCursorScreenPos();
    ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
    thumbnail_canvas_.AddRectFilledAt(
        ImVec2(2, 2), ImVec2(text_size.x + 4, text_size.y + 2),
        is_modified ? IM_COL32(180, 100, 0, 200) : IM_COL32(0, 100, 0, 180));

    thumbnail_canvas_.AddTextAt(ImVec2(4, 2), label,
                                is_modified ? IM_COL32(255, 200, 100, 255)
                                            : IM_COL32(150, 255, 150, 255));
    gui::EndCanvas(thumbnail_canvas_, rt, frame_opts);
  }

  // Click handling
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
    if (ImGui::GetIO().KeyCtrl) {
      // Ctrl+click for multi-select
      if (is_multi_selected) {
        state_->selected_sheets.erase(static_cast<uint16_t>(sheet_id));
      } else {
        state_->selected_sheets.insert(static_cast<uint16_t>(sheet_id));
      }
    } else {
      // Normal click to select
      state_->SelectSheet(static_cast<uint16_t>(sheet_id));
    }
  }

  // Double-click to open in new tab
  if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    state_->open_sheets.insert(static_cast<uint16_t>(sheet_id));
  }

  ImGui::EndChild();

  if (is_selected || is_multi_selected) {
    ImGui::PopStyleColor();
  }

  // Tooltip with sheet info
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Sheet: 0x%02X (%d)", sheet_id, sheet_id);
    if (bitmap.is_active()) {
      ImGui::Text("Size: %dx%d", bitmap.width(), bitmap.height());
      ImGui::Text("Depth: %d bpp", bitmap.depth());
    } else {
      ImGui::Text("(Inactive)");
    }
    if (is_modified) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Modified");
    }
    ImGui::EndTooltip();
  }
}

}  // namespace editor
}  // namespace yaze
