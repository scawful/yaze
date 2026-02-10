#include "app/editor/graphics/pixel_editor_panel.h"

#include <algorithm>
#include <cmath>
#include <queue>

#include "absl/strings/str_format.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void PixelEditorPanel::Initialize() {
  // Canvas is initialized via member initializer list
}

void PixelEditorPanel::Draw(bool* p_open) {
  // EditorPanel interface - delegate to Update()
  Update().IgnoreError();
}

absl::Status PixelEditorPanel::Update() {
  // Top toolbar
  DrawToolbar();
  ImGui::SameLine();
  DrawViewControls();

  ImGui::Separator();

  constexpr float kColorPickerWidth = 200.0f;
  constexpr float kStatusBarHeight = 24.0f;

  // Main content area with canvas and side panels
  ImGui::BeginChild("##PixelEditorContent", ImVec2(0, -kStatusBarHeight),
                    false);

  // Color picker on the left
  ImGui::BeginChild("##ColorPickerSide", ImVec2(kColorPickerWidth, 0), true);
  DrawColorPicker();
  ImGui::Separator();
  DrawMiniMap();
  ImGui::EndChild();

  ImGui::SameLine();

  // Main canvas
  ImGui::BeginChild("##CanvasArea", ImVec2(0, 0), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  DrawCanvas();
  ImGui::EndChild();

  ImGui::EndChild();

  // Status bar
  DrawStatusBar();

  return absl::OkStatus();
}

void PixelEditorPanel::DrawToolbar() {
  // Tool selection buttons
  auto tool_button = [this](PixelTool tool, const char* icon,
                            const char* tooltip) {
    bool is_selected = state_->current_tool == tool;
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Button, gui::GetPrimaryVec4());
    }
    if (ImGui::Button(icon)) {
      state_->SetTool(tool);
    }
    if (is_selected) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", tooltip);
    }
    ImGui::SameLine();
  };

  tool_button(PixelTool::kSelect, ICON_MD_SELECT_ALL, "Select (V)");
  tool_button(PixelTool::kPencil, ICON_MD_DRAW, "Pencil (B)");
  tool_button(PixelTool::kBrush, ICON_MD_BRUSH, "Brush (B)");
  tool_button(PixelTool::kEraser, ICON_MD_AUTO_FIX_HIGH, "Eraser (E)");
  tool_button(PixelTool::kFill, ICON_MD_FORMAT_COLOR_FILL, "Fill (G)");
  tool_button(PixelTool::kLine, ICON_MD_HORIZONTAL_RULE, "Line");
  tool_button(PixelTool::kRectangle, ICON_MD_CROP_SQUARE, "Rectangle");
  tool_button(PixelTool::kEyedropper, ICON_MD_COLORIZE, "Eyedropper (I)");

  ImGui::SameLine();
  ImGui::Text("|");
  ImGui::SameLine();

  // Brush size for pencil/brush/eraser
  if (state_->current_tool == PixelTool::kPencil ||
      state_->current_tool == PixelTool::kBrush ||
      state_->current_tool == PixelTool::kEraser) {
    ImGui::SetNextItemWidth(80);
    int brush = state_->brush_size;
    if (ImGui::SliderInt("##BrushSize", &brush, 1, 8, "%d px")) {
      state_->brush_size = static_cast<uint8_t>(brush);
    }
    HOVER_HINT("Brush size");
    ImGui::SameLine();
  }

  // Undo/Redo buttons
  ImGui::Text("|");
  ImGui::SameLine();

  ImGui::BeginDisabled(!state_->CanUndo());
  if (ImGui::Button(ICON_MD_UNDO)) {
    PixelEditorSnapshot snapshot;
    if (state_->PopUndoState(snapshot)) {
      // Apply undo state
      auto& sheet =
          gfx::Arena::Get().mutable_gfx_sheets()->at(snapshot.sheet_id);
      sheet.set_data(snapshot.pixel_data);
      gfx::Arena::Get().NotifySheetModified(snapshot.sheet_id);
    }
  }
  ImGui::EndDisabled();
  HOVER_HINT("Undo (Ctrl+Z)");

  ImGui::SameLine();

  ImGui::BeginDisabled(!state_->CanRedo());
  if (ImGui::Button(ICON_MD_REDO)) {
    PixelEditorSnapshot snapshot;
    if (state_->PopRedoState(snapshot)) {
      // Apply redo state
      auto& sheet =
          gfx::Arena::Get().mutable_gfx_sheets()->at(snapshot.sheet_id);
      sheet.set_data(snapshot.pixel_data);
      gfx::Arena::Get().NotifySheetModified(snapshot.sheet_id);
    }
  }
  ImGui::EndDisabled();
  HOVER_HINT("Redo (Ctrl+Y)");
}

void PixelEditorPanel::DrawViewControls() {
  // Zoom controls
  if (ImGui::Button(ICON_MD_ZOOM_OUT)) {
    state_->ZoomOut();
  }
  HOVER_HINT("Zoom out (-)");
  ImGui::SameLine();

  ImGui::SetNextItemWidth(100);
  float zoom = state_->zoom_level;
  if (ImGui::SliderFloat("##Zoom", &zoom, 1.0f, 16.0f, "%.0fx")) {
    state_->SetZoom(zoom);
  }
  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_ZOOM_IN)) {
    state_->ZoomIn();
  }
  HOVER_HINT("Zoom in (+)");

  ImGui::SameLine();
  ImGui::Text("|");
  ImGui::SameLine();

  // View overlay toggles
  ImGui::Checkbox(ICON_MD_GRID_ON, &state_->show_grid);
  HOVER_HINT("Toggle grid (Ctrl+G)");
  ImGui::SameLine();

  ImGui::Checkbox(ICON_MD_ADD, &state_->show_cursor_crosshair);
  HOVER_HINT("Toggle cursor crosshair");
  ImGui::SameLine();

  ImGui::Checkbox(ICON_MD_BRUSH, &state_->show_brush_preview);
  HOVER_HINT("Toggle brush preview");
  ImGui::SameLine();

  ImGui::Checkbox(ICON_MD_TEXTURE, &state_->show_transparency_grid);
  HOVER_HINT("Toggle transparency grid");
}

void PixelEditorPanel::DrawCanvas() {
  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled("No sheet selected. Select a sheet from the browser.");
    return;
  }

  // Tab bar for open sheets
  if (ImGui::BeginTabBar("##SheetTabs",
                         ImGuiTabBarFlags_AutoSelectNewTabs |
                             ImGuiTabBarFlags_Reorderable |
                             ImGuiTabBarFlags_TabListPopupButton)) {
    std::vector<uint16_t> sheets_to_close;

    for (uint16_t sheet_id : state_->open_sheets) {
      bool open = true;
      std::string tab_label = absl::StrFormat("%02X", sheet_id);
      if (state_->modified_sheets.count(sheet_id) > 0) {
        tab_label += "*";
      }

      if (ImGui::BeginTabItem(tab_label.c_str(), &open)) {
        state_->current_sheet_id = sheet_id;

        // Get the current sheet bitmap
        auto& sheet = gfx::Arena::Get().mutable_gfx_sheets()->at(
            state_->current_sheet_id);

        if (!sheet.is_active()) {
          ImGui::TextDisabled("Sheet %02X is not active", sheet_id);
          ImGui::EndTabItem();
          continue;
        }

        // Calculate canvas size based on zoom
        float canvas_width = sheet.width() * state_->zoom_level;
        float canvas_height = sheet.height() * state_->zoom_level;

        // Draw canvas background
        canvas_.DrawBackground(ImVec2(canvas_width, canvas_height));

        // Draw transparency checkerboard background if enabled
        if (state_->show_transparency_grid) {
          DrawTransparencyGrid(canvas_width, canvas_height);
        }

        // Draw the sheet texture
        if (sheet.texture()) {
          canvas_.draw_list()->AddImage(
              (ImTextureID)(intptr_t)sheet.texture(), canvas_.zero_point(),
              ImVec2(canvas_.zero_point().x + canvas_width,
                     canvas_.zero_point().y + canvas_height));
        }

        // Draw grid if enabled
        if (state_->show_grid) {
          canvas_.DrawGrid(8.0f * state_->zoom_level);
        }

        // Draw transient tile highlight (e.g., from "Edit Graphics" jump)
        DrawTileHighlight(sheet);

        // Draw selection rectangle if active
        if (state_->selection.is_active) {
          ImVec2 sel_min =
              PixelToScreen(state_->selection.x, state_->selection.y);
          ImVec2 sel_max =
              PixelToScreen(state_->selection.x + state_->selection.width,
                            state_->selection.y + state_->selection.height);
          canvas_.draw_list()->AddRect(
              sel_min, sel_max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);

          // Marching ants effect (simplified)
          canvas_.draw_list()->AddRect(sel_min, sel_max, IM_COL32(0, 0, 0, 128),
                                       0.0f, 0, 1.0f);
        }

        // Draw tool preview (line/rectangle)
        if (show_tool_preview_ && is_drawing_) {
          ImVec2 start = PixelToScreen(static_cast<int>(tool_start_pixel_.x),
                                       static_cast<int>(tool_start_pixel_.y));
          ImVec2 end = PixelToScreen(static_cast<int>(preview_end_.x),
                                     static_cast<int>(preview_end_.y));

          if (state_->current_tool == PixelTool::kLine) {
            canvas_.draw_list()->AddLine(start, end, IM_COL32(255, 255, 0, 200),
                                         2.0f);
          } else if (state_->current_tool == PixelTool::kRectangle) {
            canvas_.draw_list()->AddRect(start, end, IM_COL32(255, 255, 0, 200),
                                         0.0f, 0, 2.0f);
          }
        }

        // Draw cursor crosshair overlay if enabled and cursor in canvas
        if (state_->show_cursor_crosshair && cursor_in_canvas_) {
          DrawCursorCrosshair();
        }

        // Draw brush preview if using brush/eraser tool
        if (state_->show_brush_preview && cursor_in_canvas_ &&
            (state_->current_tool == PixelTool::kBrush ||
             state_->current_tool == PixelTool::kEraser)) {
          DrawBrushPreview();
        }

        canvas_.DrawOverlay();

        // Handle mouse input
        HandleCanvasInput();

        // Show pixel info tooltip if enabled
        if (state_->show_pixel_info_tooltip && cursor_in_canvas_) {
          DrawPixelInfoTooltip(sheet);
        }

        ImGui::EndTabItem();
      }

      if (!open) {
        sheets_to_close.push_back(sheet_id);
      }
    }

    // Close tabs that were requested
    for (uint16_t sheet_id : sheets_to_close) {
      state_->CloseSheet(sheet_id);
    }

    ImGui::EndTabBar();
  }
}

void PixelEditorPanel::DrawTransparencyGrid(float canvas_width,
                                            float canvas_height) {
  const float cell_size = 8.0f;  // Checkerboard cell size
  const ImU32 color1 = IM_COL32(180, 180, 180, 255);
  const ImU32 color2 = IM_COL32(220, 220, 220, 255);

  ImVec2 origin = canvas_.zero_point();
  int cols = static_cast<int>(canvas_width / cell_size) + 1;
  int rows = static_cast<int>(canvas_height / cell_size) + 1;

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      bool is_light = (row + col) % 2 == 0;
      ImVec2 p_min(origin.x + col * cell_size, origin.y + row * cell_size);
      ImVec2 p_max(std::min(p_min.x + cell_size, origin.x + canvas_width),
                   std::min(p_min.y + cell_size, origin.y + canvas_height));
      canvas_.draw_list()->AddRectFilled(p_min, p_max,
                                         is_light ? color1 : color2);
    }
  }
}

void PixelEditorPanel::DrawCursorCrosshair() {
  ImVec2 cursor_screen = PixelToScreen(cursor_x_, cursor_y_);
  float pixel_size = state_->zoom_level;

  // Vertical line through cursor pixel
  ImVec2 v_start(cursor_screen.x + pixel_size / 2, canvas_.zero_point().y);
  ImVec2 v_end(cursor_screen.x + pixel_size / 2,
               canvas_.zero_point().y + canvas_.canvas_size().y);
  canvas_.draw_list()->AddLine(v_start, v_end, IM_COL32(255, 100, 100, 100),
                               1.0f);

  // Horizontal line through cursor pixel
  ImVec2 h_start(canvas_.zero_point().x, cursor_screen.y + pixel_size / 2);
  ImVec2 h_end(canvas_.zero_point().x + canvas_.canvas_size().x,
               cursor_screen.y + pixel_size / 2);
  canvas_.draw_list()->AddLine(h_start, h_end, IM_COL32(255, 100, 100, 100),
                               1.0f);

  // Highlight current pixel with a bright outline
  ImVec2 pixel_min = cursor_screen;
  ImVec2 pixel_max(cursor_screen.x + pixel_size, cursor_screen.y + pixel_size);
  canvas_.draw_list()->AddRect(pixel_min, pixel_max,
                               IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
}

void PixelEditorPanel::DrawBrushPreview() {
  ImVec2 cursor_screen = PixelToScreen(cursor_x_, cursor_y_);
  float pixel_size = state_->zoom_level;
  int brush = state_->brush_size;
  int half = brush / 2;

  // Draw preview of brush area
  ImVec2 brush_min(cursor_screen.x - half * pixel_size,
                   cursor_screen.y - half * pixel_size);
  ImVec2 brush_max(cursor_screen.x + (brush - half) * pixel_size,
                   cursor_screen.y + (brush - half) * pixel_size);

  // Fill with semi-transparent color preview
  ImU32 preview_color = (state_->current_tool == PixelTool::kEraser)
                            ? IM_COL32(255, 0, 0, 50)
                            : IM_COL32(0, 255, 0, 50);
  canvas_.draw_list()->AddRectFilled(brush_min, brush_max, preview_color);

  // Outline
  ImU32 outline_color = (state_->current_tool == PixelTool::kEraser)
                            ? IM_COL32(255, 100, 100, 200)
                            : IM_COL32(100, 255, 100, 200);
  canvas_.draw_list()->AddRect(brush_min, brush_max, outline_color, 0.0f, 0,
                               1.0f);
}

void PixelEditorPanel::DrawPixelInfoTooltip(const gfx::Bitmap& sheet) {
  if (cursor_x_ < 0 || cursor_x_ >= sheet.width() || cursor_y_ < 0 ||
      cursor_y_ >= sheet.height()) {
    return;
  }

  uint8_t color_index = sheet.GetPixel(cursor_x_, cursor_y_);
  auto palette = sheet.palette();

  ImGui::BeginTooltip();
  ImGui::Text("Pos: %d, %d", cursor_x_, cursor_y_);
  ImGui::Text("Tile: %d, %d", cursor_x_ / 8, cursor_y_ / 8);
  ImGui::Text("Index: %d", color_index);

  if (color_index < palette.size()) {
    ImGui::Text("SNES: $%04X", palette[color_index].snes());
    ImVec4 color(palette[color_index].rgb().x / 255.0f,
                 palette[color_index].rgb().y / 255.0f,
                 palette[color_index].rgb().z / 255.0f, 1.0f);
    ImGui::ColorButton("##ColorPreview", color, ImGuiColorEditFlags_NoTooltip,
                       ImVec2(24, 24));
    if (color_index == 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("(Transparent)");
    }
  }
  ImGui::EndTooltip();
}

void PixelEditorPanel::DrawTileHighlight(const gfx::Bitmap& sheet) {
  if (!state_->tile_highlight.active) {
    return;
  }
  if (state_->tile_highlight.sheet_id != state_->current_sheet_id) {
    return;
  }

  const double now = ImGui::GetTime();
  const double elapsed = now - state_->tile_highlight.start_time;
  if (elapsed > state_->tile_highlight.duration) {
    state_->tile_highlight.Clear();
    return;
  }

  const int tiles_per_row = sheet.width() / 8;
  if (tiles_per_row <= 0) {
    return;
  }
  const uint16_t tile_index = state_->tile_highlight.tile_index;
  const int tile_x = static_cast<int>(tile_index % tiles_per_row);
  const int tile_y = static_cast<int>(tile_index / tiles_per_row);

  const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(elapsed) * 6.0f);
  const float alpha = 0.25f + (pulse * 0.35f);

  ImVec2 min = PixelToScreen(tile_x * 8, tile_y * 8);
  ImVec2 max = PixelToScreen(tile_x * 8 + 8, tile_y * 8 + 8);

  ImVec4 sel = gui::GetSelectedColor();
  sel.w = alpha;
  const ImU32 fill_color = ImGui::GetColorU32(sel);
  ImVec4 sel_outline = gui::GetSelectedColor();
  sel_outline.w = 0.9f;
  const ImU32 outline_color = ImGui::GetColorU32(sel_outline);
  canvas_.draw_list()->AddRectFilled(min, max, fill_color);
  canvas_.draw_list()->AddRect(min, max, outline_color, 0.0f, 0, 2.0f);

  if (ImGui::IsMouseHoveringRect(min, max)) {
    ImGui::BeginTooltip();
    ImGui::Text("Focus tile: %d (sheet %02X)", tile_index,
                state_->tile_highlight.sheet_id);
    if (!state_->tile_highlight.label.empty()) {
      ImGui::Text("%s", state_->tile_highlight.label.c_str());
    }
    ImGui::EndTooltip();
  }
}

void PixelEditorPanel::DrawColorPicker() {
  ImGui::Text("Colors");

  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled("No sheet");
    return;
  }

  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);
  auto palette = sheet.palette();

  // Draw palette colors in 4x4 grid (16 colors)
  for (int i = 0; i < static_cast<int>(palette.size()) && i < 16; i++) {
    if (i > 0 && i % 4 == 0) {
      // New row
    } else if (i > 0) {
      ImGui::SameLine();
    }

    ImVec4 color(palette[i].rgb().x / 255.0f, palette[i].rgb().y / 255.0f,
                 palette[i].rgb().z / 255.0f, 1.0f);

    bool is_selected = state_->current_color_index == i;
    if (is_selected) {
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
      ImGui::PushStyleColor(ImGuiCol_Border, gui::GetWarningColor());
    }

    std::string id = absl::StrFormat("##Color%d", i);
    if (ImGui::ColorButton(
            id.c_str(), color,
            ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
            ImVec2(24, 24))) {
      state_->current_color_index = static_cast<uint8_t>(i);
      state_->current_color = color;
    }

    if (is_selected) {
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("Index: %d", i);
      ImGui::Text("SNES: $%04X", palette[i].snes());
      ImGui::Text("RGB: %d, %d, %d", static_cast<int>(palette[i].rgb().x),
                  static_cast<int>(palette[i].rgb().y),
                  static_cast<int>(palette[i].rgb().z));
      if (i == 0) {
        ImGui::Text("(Transparent)");
      }
      ImGui::EndTooltip();
    }
  }

  ImGui::Separator();

  // Current color preview
  ImGui::Text("Current:");
  ImGui::ColorButton("##CurrentColor", state_->current_color,
                     ImGuiColorEditFlags_NoTooltip, ImVec2(40, 40));
  ImGui::SameLine();
  ImGui::Text("Index: %d", state_->current_color_index);
}

void PixelEditorPanel::DrawMiniMap() {
  ImGui::Text("Navigator");

  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled("No sheet");
    return;
  }

  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);
  if (!sheet.texture())
    return;

  // Draw mini version of the sheet
  float mini_scale = 0.5f;
  float mini_width = sheet.width() * mini_scale;
  float mini_height = sheet.height() * mini_scale;

  ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::GetWindowDrawList()->AddImage(
      (ImTextureID)(intptr_t)sheet.texture(), pos,
      ImVec2(pos.x + mini_width, pos.y + mini_height));

  // Draw viewport rectangle
  // TODO: Calculate actual viewport bounds based on scroll position

  ImGui::Dummy(ImVec2(mini_width, mini_height));
}

void PixelEditorPanel::DrawStatusBar() {
  ImGui::Separator();

  // Tool name
  ImGui::Text("%s", state_->GetToolName());
  ImGui::SameLine();

  // Cursor position
  if (cursor_in_canvas_) {
    ImGui::Text("Pos: %d, %d", cursor_x_, cursor_y_);
    ImGui::SameLine();

    // Tile coordinates
    int tile_x = cursor_x_ / 8;
    int tile_y = cursor_y_ / 8;
    ImGui::Text("Tile: %d, %d", tile_x, tile_y);
    ImGui::SameLine();
  }

  // Sheet info
  ImGui::Text("Sheet: %02X", state_->current_sheet_id);
  ImGui::SameLine();

  if (state_->tile_highlight.active &&
      state_->tile_highlight.sheet_id == state_->current_sheet_id) {
    ImGui::TextColored(gui::GetWarningColor(), "Focus: %d",
                       state_->tile_highlight.tile_index);
    ImGui::SameLine();
  }

  // Modified indicator
  if (state_->modified_sheets.count(state_->current_sheet_id) > 0) {
    ImGui::TextColored(gui::GetModifiedColor(), "(Modified)");
  }

  // Zoom level
  ImGui::SameLine();
  ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
  ImGui::Text("Zoom: %.0fx", state_->zoom_level);
}

void PixelEditorPanel::HandleCanvasInput() {
  if (!ImGui::IsItemHovered()) {
    cursor_in_canvas_ = false;
    return;
  }

  cursor_in_canvas_ = true;
  ImVec2 mouse_pos = ImGui::GetMousePos();
  ImVec2 pixel_pos = ScreenToPixel(mouse_pos);

  cursor_x_ = static_cast<int>(pixel_pos.x);
  cursor_y_ = static_cast<int>(pixel_pos.y);

  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  // Clamp to sheet bounds
  cursor_x_ = std::clamp(cursor_x_, 0, sheet.width() - 1);
  cursor_y_ = std::clamp(cursor_y_, 0, sheet.height() - 1);

  // Mouse button handling
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    is_drawing_ = true;
    tool_start_pixel_ =
        ImVec2(static_cast<float>(cursor_x_), static_cast<float>(cursor_y_));
    last_mouse_pixel_ = tool_start_pixel_;

    // Save undo state before starting to draw
    SaveUndoState();

    // Handle tools that need start position
    switch (state_->current_tool) {
      case PixelTool::kPencil:
        ApplyPencil(cursor_x_, cursor_y_);
        break;
      case PixelTool::kBrush:
        ApplyBrush(cursor_x_, cursor_y_);
        break;
      case PixelTool::kEraser:
        ApplyEraser(cursor_x_, cursor_y_);
        break;
      case PixelTool::kFill:
        ApplyFill(cursor_x_, cursor_y_);
        break;
      case PixelTool::kEyedropper:
        ApplyEyedropper(cursor_x_, cursor_y_);
        break;
      case PixelTool::kSelect:
        BeginSelection(cursor_x_, cursor_y_);
        break;
      case PixelTool::kLine:
      case PixelTool::kRectangle:
        show_tool_preview_ = true;
        break;
      default:
        break;
    }
  }

  if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && is_drawing_) {
    preview_end_ =
        ImVec2(static_cast<float>(cursor_x_), static_cast<float>(cursor_y_));

    switch (state_->current_tool) {
      case PixelTool::kPencil:
        ApplyPencil(cursor_x_, cursor_y_);
        break;
      case PixelTool::kBrush:
        ApplyBrush(cursor_x_, cursor_y_);
        break;
      case PixelTool::kEraser:
        ApplyEraser(cursor_x_, cursor_y_);
        break;
      case PixelTool::kSelect:
        UpdateSelection(cursor_x_, cursor_y_);
        break;
      default:
        break;
    }

    last_mouse_pixel_ =
        ImVec2(static_cast<float>(cursor_x_), static_cast<float>(cursor_y_));
  }

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && is_drawing_) {
    is_drawing_ = false;

    switch (state_->current_tool) {
      case PixelTool::kLine:
        DrawLine(static_cast<int>(tool_start_pixel_.x),
                 static_cast<int>(tool_start_pixel_.y), cursor_x_, cursor_y_);
        break;
      case PixelTool::kRectangle:
        DrawRectangle(static_cast<int>(tool_start_pixel_.x),
                      static_cast<int>(tool_start_pixel_.y), cursor_x_,
                      cursor_y_, false);
        break;
      case PixelTool::kSelect:
        EndSelection();
        break;
      default:
        break;
    }

    show_tool_preview_ = false;
  }
}

void PixelEditorPanel::ApplyPencil(int x, int y) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  if (x >= 0 && x < sheet.width() && y >= 0 && y < sheet.height()) {
    sheet.WriteToPixel(x, y, state_->current_color_index);
    state_->MarkSheetModified(state_->current_sheet_id);
    gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
  }
}

void PixelEditorPanel::ApplyBrush(int x, int y) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);
  int size = state_->brush_size;
  int half = size / 2;

  for (int dy = -half; dy < size - half; dy++) {
    for (int dx = -half; dx < size - half; dx++) {
      int px = x + dx;
      int py = y + dy;
      if (px >= 0 && px < sheet.width() && py >= 0 && py < sheet.height()) {
        sheet.WriteToPixel(px, py, state_->current_color_index);
      }
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::ApplyEraser(int x, int y) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);
  int size = state_->brush_size;
  int half = size / 2;

  for (int dy = -half; dy < size - half; dy++) {
    for (int dx = -half; dx < size - half; dx++) {
      int px = x + dx;
      int py = y + dy;
      if (px >= 0 && px < sheet.width() && py >= 0 && py < sheet.height()) {
        sheet.WriteToPixel(px, py, 0);  // Index 0 = transparent
      }
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::ApplyFill(int x, int y) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  if (x < 0 || x >= sheet.width() || y < 0 || y >= sheet.height())
    return;

  uint8_t target_color = sheet.GetPixel(x, y);
  uint8_t fill_color = state_->current_color_index;

  if (target_color == fill_color)
    return;  // Nothing to fill

  // BFS flood fill
  std::queue<std::pair<int, int>> queue;
  std::vector<bool> visited(sheet.width() * sheet.height(), false);

  queue.push({x, y});
  visited[y * sheet.width() + x] = true;

  while (!queue.empty()) {
    auto [cx, cy] = queue.front();
    queue.pop();

    sheet.WriteToPixel(cx, cy, fill_color);

    // Check 4-connected neighbors
    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};

    for (int i = 0; i < 4; i++) {
      int nx = cx + dx[i];
      int ny = cy + dy[i];

      if (nx >= 0 && nx < sheet.width() && ny >= 0 && ny < sheet.height()) {
        int idx = ny * sheet.width() + nx;
        if (!visited[idx] && sheet.GetPixel(nx, ny) == target_color) {
          visited[idx] = true;
          queue.push({nx, ny});
        }
      }
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::ApplyEyedropper(int x, int y) {
  auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];

  if (x >= 0 && x < sheet.width() && y >= 0 && y < sheet.height()) {
    state_->current_color_index = sheet.GetPixel(x, y);

    // Update current color display
    auto palette = sheet.palette();
    if (state_->current_color_index < palette.size()) {
      auto& color = palette[state_->current_color_index];
      state_->current_color =
          ImVec4(color.rgb().x / 255.0f, color.rgb().y / 255.0f,
                 color.rgb().z / 255.0f, 1.0f);
    }
  }
}

void PixelEditorPanel::DrawLine(int x1, int y1, int x2, int y2) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  // Bresenham's line algorithm
  int dx = std::abs(x2 - x1);
  int dy = std::abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = dx - dy;

  while (true) {
    if (x1 >= 0 && x1 < sheet.width() && y1 >= 0 && y1 < sheet.height()) {
      sheet.WriteToPixel(x1, y1, state_->current_color_index);
    }

    if (x1 == x2 && y1 == y2)
      break;

    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::DrawRectangle(int x1, int y1, int x2, int y2,
                                     bool filled) {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  int min_x = std::min(x1, x2);
  int max_x = std::max(x1, x2);
  int min_y = std::min(y1, y2);
  int max_y = std::max(y1, y2);

  if (filled) {
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        if (x >= 0 && x < sheet.width() && y >= 0 && y < sheet.height()) {
          sheet.WriteToPixel(x, y, state_->current_color_index);
        }
      }
    }
  } else {
    // Top and bottom edges
    for (int x = min_x; x <= max_x; x++) {
      if (x >= 0 && x < sheet.width()) {
        if (min_y >= 0 && min_y < sheet.height())
          sheet.WriteToPixel(x, min_y, state_->current_color_index);
        if (max_y >= 0 && max_y < sheet.height())
          sheet.WriteToPixel(x, max_y, state_->current_color_index);
      }
    }
    // Left and right edges
    for (int y = min_y; y <= max_y; y++) {
      if (y >= 0 && y < sheet.height()) {
        if (min_x >= 0 && min_x < sheet.width())
          sheet.WriteToPixel(min_x, y, state_->current_color_index);
        if (max_x >= 0 && max_x < sheet.width())
          sheet.WriteToPixel(max_x, y, state_->current_color_index);
      }
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::BeginSelection(int x, int y) {
  state_->selection.x = x;
  state_->selection.y = y;
  state_->selection.width = 1;
  state_->selection.height = 1;
  state_->selection.is_active = true;
  state_->is_selecting = true;
}

void PixelEditorPanel::UpdateSelection(int x, int y) {
  int start_x = static_cast<int>(tool_start_pixel_.x);
  int start_y = static_cast<int>(tool_start_pixel_.y);

  state_->selection.x = std::min(start_x, x);
  state_->selection.y = std::min(start_y, y);
  state_->selection.width = std::abs(x - start_x) + 1;
  state_->selection.height = std::abs(y - start_y) + 1;
}

void PixelEditorPanel::EndSelection() {
  state_->is_selecting = false;

  // Copy pixel data for the selection
  if (state_->selection.width > 0 && state_->selection.height > 0) {
    auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
    state_->selection.pixel_data.resize(state_->selection.width *
                                        state_->selection.height);

    for (int y = 0; y < state_->selection.height; y++) {
      for (int x = 0; x < state_->selection.width; x++) {
        int src_x = state_->selection.x + x;
        int src_y = state_->selection.y + y;
        if (src_x >= 0 && src_x < sheet.width() && src_y >= 0 &&
            src_y < sheet.height()) {
          state_->selection.pixel_data[y * state_->selection.width + x] =
              sheet.GetPixel(src_x, src_y);
        }
      }
    }

    state_->selection.palette = sheet.palette();
  }
}

void PixelEditorPanel::CopySelection() {
  // Selection data is already in state_->selection
}

void PixelEditorPanel::PasteSelection(int x, int y) {
  if (state_->selection.pixel_data.empty())
    return;

  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);

  SaveUndoState();

  for (int dy = 0; dy < state_->selection.height; dy++) {
    for (int dx = 0; dx < state_->selection.width; dx++) {
      int dest_x = x + dx;
      int dest_y = y + dy;
      if (dest_x >= 0 && dest_x < sheet.width() && dest_y >= 0 &&
          dest_y < sheet.height()) {
        uint8_t pixel =
            state_->selection.pixel_data[dy * state_->selection.width + dx];
        sheet.WriteToPixel(dest_x, dest_y, pixel);
      }
    }
  }

  state_->MarkSheetModified(state_->current_sheet_id);
  gfx::Arena::Get().NotifySheetModified(state_->current_sheet_id);
}

void PixelEditorPanel::FlipSelectionHorizontal() {
  if (state_->selection.pixel_data.empty())
    return;

  std::vector<uint8_t> flipped(state_->selection.pixel_data.size());
  for (int y = 0; y < state_->selection.height; y++) {
    for (int x = 0; x < state_->selection.width; x++) {
      int src_idx = y * state_->selection.width + x;
      int dst_idx =
          y * state_->selection.width + (state_->selection.width - 1 - x);
      flipped[dst_idx] = state_->selection.pixel_data[src_idx];
    }
  }
  state_->selection.pixel_data = std::move(flipped);
}

void PixelEditorPanel::FlipSelectionVertical() {
  if (state_->selection.pixel_data.empty())
    return;

  std::vector<uint8_t> flipped(state_->selection.pixel_data.size());
  for (int y = 0; y < state_->selection.height; y++) {
    for (int x = 0; x < state_->selection.width; x++) {
      int src_idx = y * state_->selection.width + x;
      int dst_idx =
          (state_->selection.height - 1 - y) * state_->selection.width + x;
      flipped[dst_idx] = state_->selection.pixel_data[src_idx];
    }
  }
  state_->selection.pixel_data = std::move(flipped);
}

void PixelEditorPanel::SaveUndoState() {
  auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  state_->PushUndoState(state_->current_sheet_id, sheet.vector(),
                        sheet.palette());
}

ImVec2 PixelEditorPanel::ScreenToPixel(ImVec2 screen_pos) {
  float px = (screen_pos.x - canvas_.zero_point().x) / state_->zoom_level;
  float py = (screen_pos.y - canvas_.zero_point().y) / state_->zoom_level;
  return ImVec2(px, py);
}

ImVec2 PixelEditorPanel::PixelToScreen(int x, int y) {
  return ImVec2(canvas_.zero_point().x + x * state_->zoom_level,
                canvas_.zero_point().y + y * state_->zoom_level);
}

}  // namespace editor
}  // namespace yaze
