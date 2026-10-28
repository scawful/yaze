#define IMGUI_DEFINE_MATH_OPERATORS
#include "app/editor/graphics/pixel_editor_panel.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <queue>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_sheet_sync.h"
#include "app/editor/graphics/graphics_undo_actions.h"
#include "app/editor/graphics/palette_controls_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/centered_icon_button.h"
#include "app/gui/widgets/themed_widgets.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/image_clipboard.h"

namespace yaze {
namespace editor {

void PixelEditorPanel::Initialize() {
  // Canvas is initialized via member initializer list
}

void PixelEditorPanel::Draw(bool* p_open) {
  // WindowContent interface - delegate to Update()
  Update().IgnoreError();
}

absl::Status PixelEditorPanel::Update() {
  HandleFloatingPasteKeys();
  DrawSourceContext();

  // Match every control to the current font/frame size. Mixing toolbar icon
  // presets and normal buttons made Undo taller than the drawing tools.
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(4, ImGui::GetStyle().ItemSpacing.y));
  if (sheet_browser_visible_ && set_sheet_browser_visible_) {
    const bool visible = sheet_browser_visible_();
    std::optional<gui::StyleColorGuard> selected;
    if (visible)
      selected.emplace(ImGuiCol_Button, gui::GetPrimaryVec4());
    if (ImGui::Button(tr("Sheets")))
      set_sheet_browser_visible_(!visible);
    gui::AutoRegisterLastItem("button", "Toggle Sheet Browser");
    HOVER_HINT(visible ? "Hide Sheet Browser to expand the canvas"
                       : "Show Sheet Browser in its previous position");
    ImGui::SameLine();
  }
  DrawToolbar();
  ImGui::SameLine();
  const float button_size = ImGui::GetFrameHeight();
  if (gui::CenteredIconButton(ICON_MD_TUNE "##ViewOptions", button_size)) {
    ImGui::OpenPopup("View options");
  }
  gui::AutoRegisterLastItem("button", "View options");
  HOVER_HINT("Display overlays and sheet overview");
  if (ImGui::BeginPopup("View options")) {
    DrawViewControls();
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (gui::CenteredIconButton(ICON_MD_MORE_HORIZ "##PixelEditOptions",
                              button_size)) {
    ImGui::OpenPopup("Edit options");
  }
  gui::AutoRegisterLastItem("button", "Edit options");
  HOVER_HINT("Copy and paste a PNG");
  if (ImGui::BeginPopup("Edit options")) {
    DrawClipboardControls();
    ImGui::EndPopup();
  }
  ImGui::PopStyleVar();
  if (state_->selection.is_floating) {
    DrawClipboardControls();
  }

  DrawColorPicker();

  const float status_height =
      ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y + 1;
  ImGui::BeginChild("##PixelEditorContent", ImVec2(0, -status_height), false);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const float font = ImGui::GetFontSize();
  const bool preview = show_source_preview_ && draw_source_preview_ &&
                       state_->source_context &&
                       state_->source_context->owner == state_->game_data &&
                       state_->source_context->render_preview;
  const bool beside = preview && available.x >= 48 * font;
  const float preview_width =
      std::clamp(available.x * 0.30f, 14 * font, 22 * font);
  const float preview_height = std::min(available.y * 0.38f, 18 * font);
  const float gap = ImGui::GetStyle().ItemSpacing.x;
  const ImVec2 canvas_size(
      beside ? std::max(1.0f, available.x - preview_width - gap) : 0,
      preview && !beside ? std::max(1.0f, available.y - preview_height -
                                              ImGui::GetStyle().ItemSpacing.y)
                         : 0);
  if (fit_canvas_requested_) {
    ImGui::SetNextWindowScroll(ImVec2(0, 0));
  }
  ImGui::BeginChild("##CanvasArea", canvas_size, false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  DrawCanvas();
  ImGui::EndChild();
  if (preview) {
    if (beside)
      ImGui::SameLine();
    ImGui::BeginChild(
        "##SourceUsagePreview",
        beside ? ImVec2(preview_width, 0) : ImVec2(0, preview_height), false);
    draw_source_preview_();
    ImGui::EndChild();
  }
  ImGui::EndChild();

  FinishReleasedGesture();
  // Status bar
  DrawStatusBar();

  return absl::OkStatus();
}

gfx::SnesPalette PixelEditorPanel::DisplayPalette() const {
  if (const auto* target = state_->SourceTarget()) {
    return target->palette;
  }
  const auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  const auto* palette = platform::GetSurfacePalette(sheet.surface());
  if (!palette) {
    return {};
  }

  // Bitmap::palette() retains the full source bank. Manual row selection
  // applies a slice to the surface, inserting transparent index zero. Pixel
  // tools and PNG transfer must use those displayed three-bit indices.
  std::vector<gfx::SnesColor> colors;
  const int count = std::min(8, palette->ncolors);
  colors.reserve(count);
  for (int i = 0; i < count; ++i) {
    const auto& color = palette->colors[i];
    colors.emplace_back(color.r, color.g, color.b);
    colors.back().set_transparent(i == 0);
  }
  return gfx::SnesPalette(colors);
}

gfx::Bitmap& PixelEditorPanel::DisplaySheet() {
  auto& sheet =
      gfx::Arena::Get().mutable_gfx_sheets()->at(state_->current_sheet_id);
  if (state_->SourceTarget() == nullptr || !sheet.is_active()) {
    return sheet;
  }
  bool changed = !context_sheet_.is_active();
  if (!context_sheet_.is_active()) {
    context_sheet_.Create(sheet.width(), sheet.height(), 8, sheet.vector());
  } else if (context_sheet_.vector() != sheet.vector()) {
    context_sheet_.set_data(sheet.vector());
    changed = true;
  }
  const auto& palette = DisplayPalette();
  const auto& old = context_sheet_.palette();
  bool palette_changed = old.size() != palette.size();
  for (size_t i = 0; !palette_changed && i < palette.size(); ++i) {
    palette_changed = old[i].snes() != palette[i].snes();
  }
  if (palette_changed) {
    context_sheet_.SetPaletteWithTransparent(palette, 1, 7);
  }
  if (changed || palette_changed || !context_sheet_.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        context_sheet_.texture() ? gfx::Arena::TextureCommandType::UPDATE
                                 : gfx::Arena::TextureCommandType::CREATE,
        &context_sheet_);
  }
  return context_sheet_;
}

void PixelEditorPanel::DrawSourceContext() {
  if (!state_->source_context ||
      state_->source_context->owner != state_->game_data) {
    return;
  }
  auto& context = *state_->source_context;
  ImGui::BeginDisabled(!context.return_to_source);
  const bool return_to_source = ImGui::SmallButton(tr("Return to source"));
  gui::AutoRegisterLastItem("button", "Return to source");
  ImGui::EndDisabled();
  if (return_to_source) {
    // Copy before invoking: navigation may replace editor state.
    auto navigate = context.return_to_source;
    FinishGesture();
    navigate();
    return;
  }
  if (draw_source_preview_ && context.render_preview) {
    ImGui::SameLine();
    ImGui::Checkbox(tr("Preview"), &show_source_preview_);
    gui::AutoRegisterLastItem("checkbox", "Show source preview");
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_INFO_OUTLINE "##SourceDetails")) {
    ImGui::OpenPopup("Source details");
  }
  gui::AutoRegisterLastItem("button", "Source details");
  HOVER_HINT("Source, drawing pattern and palette details");
  if (ImGui::BeginPopup("Source details")) {
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 28 * ImGui::GetFontSize());
    ImGui::TextWrapped("%s", context.label.c_str());
    if (!context.drawing_description.empty()) {
      ImGui::TextWrapped(tr("Drawing: %s"),
                         context.drawing_description.c_str());
    }
    if (const auto* target = state_->SourceTarget();
        target && target->source_orientation_mask != 0) {
      ImGui::TextWrapped(tr("Pattern orientations: %s"),
                         target->SourceOrientationLabel().c_str());
      ImGui::TextWrapped("%s",
                         tr("Paint the unflipped source; all uses of this "
                            "sheet share its pixels."));
    }
    if (!context.limitation.empty()) {
      ImGui::TextWrapped("%s", context.limitation.c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    ImGui::BeginDisabled(state_->selection.is_floating);
    const bool manual = ImGui::SmallButton(tr("Use manual palette"));
    gui::AutoRegisterLastItem("button", "Use manual palette");
    ImGui::EndDisabled();
    if (manual)
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    if (manual) {
      state_->ClearSourceContext();
      return;
    }
  }
  ImGui::SameLine();
  const ImVec2 label_pos = ImGui::GetCursorScreenPos();
  const ImVec2 label_size(std::max(1.0f, ImGui::GetContentRegionAvail().x),
                          ImGui::GetFrameHeight());
  const ImVec2 text_pos(label_pos.x,
                        label_pos.y + ImGui::GetStyle().FramePadding.y);
  const bool clipped =
      ImGui::CalcTextSize(context.label.c_str()).x > label_size.x;
  const float ellipsis_width = clipped ? ImGui::CalcTextSize("...").x : 0;
  const float text_right =
      label_pos.x + std::max(0.0f, label_size.x - ellipsis_width);
  auto* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(label_pos, ImVec2(text_right, label_pos.y + label_size.y),
                     true);
  draw->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text),
                context.label.c_str());
  draw->PopClipRect();
  if (clipped && label_size.x >= ellipsis_width) {
    draw->AddText(ImVec2(text_right, text_pos.y),
                  ImGui::GetColorU32(ImGuiCol_Text), "...");
  }
  ImGui::Dummy(label_size);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", context.label.c_str());

  if (!context.palette_error.empty()) {
    ImGui::TextWrapped(tr("Source colors could not refresh: %s"),
                       context.palette_error.c_str());
  }
  if (context.targets.empty())
    return;
  const auto& target = context.targets[context.selected_target];
  ImGui::BeginDisabled(state_->selection.is_floating);
  ImGui::SetNextItemWidth(-1);
  const bool open = ImGui::BeginCombo("##SourceTile", target.label.c_str());
  gui::AutoRegisterLastItem("combo", "Source tile");
  if (open) {
    for (size_t i = 0; i < context.targets.size(); ++i) {
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::Selectable(context.targets[i].label.c_str(),
                            i == context.selected_target)) {
        state_->SelectSourceTarget(i);
      }
      gui::AutoRegisterLastItem("selectable",
                                absl::StrFormat("Source tile %zu", i));
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::EndDisabled();
}

void PixelEditorPanel::DrawToolbar() {
  if (open_scratch_) {
    if (ImGui::Button("Scratch assets"))
      open_scratch_();
    gui::AutoRegisterLastItem("button", "Scratch assets");
    HOVER_HINT(
        "Keep editable sheets and BIN assets before assigning them to the ROM");
  }
  struct Tool {
    PixelTool tool;
    const char* icon;
    const char* label;
  };
  const Tool tools[] = {
      {PixelTool::kSelect, ICON_MD_SELECT_ALL, "Select (M)"},
      {PixelTool::kHand, ICON_MD_PAN_TOOL, "Hand (H)"},
      {PixelTool::kPencil, ICON_MD_DRAW, "Pencil (B)"},
      {PixelTool::kBrush, ICON_MD_BRUSH, "Brush (P)"},
      {PixelTool::kEraser, ICON_MD_AUTO_FIX_HIGH, "Eraser (E)"},
      {PixelTool::kFill, ICON_MD_FORMAT_COLOR_FILL, "Fill (G)"},
      {PixelTool::kLine, ICON_MD_HORIZONTAL_RULE, "Line (L)"},
      {PixelTool::kRectangle, ICON_MD_CROP_SQUARE, "Rectangle (R)"},
      {PixelTool::kEyedropper, ICON_MD_COLORIZE, "Eyedropper (I)"},
  };
  const float button_size = ImGui::GetFrameHeight();
  const bool uses_brush = state_->current_tool == PixelTool::kPencil ||
                          state_->current_tool == PixelTool::kBrush ||
                          state_->current_tool == PixelTool::kEraser;
  // Nine tools, Undo/Redo, View and Edit. Only collapse on genuinely narrow
  // detached panels; normal laptop workspaces keep the drawing tools visible.
  const float toolbar_width =
      13 * button_size + 12 * ImGui::GetStyle().ItemSpacing.x +
      (uses_brush ? 60 + ImGui::GetStyle().ItemSpacing.x : 0);
  if (ImGui::GetContentRegionAvail().x < toolbar_width) {
    ImGui::SetNextItemWidth(125);
    if (ImGui::BeginCombo("##PixelTool", state_->GetToolName())) {
      for (const auto& tool : tools) {
        if (ImGui::Selectable(tool.label, state_->current_tool == tool.tool)) {
          state_->SetTool(tool.tool);
        }
      }
      ImGui::EndCombo();
    }
  } else {
    for (const auto& tool : tools) {
      std::optional<gui::StyleColorGuard> selected;
      if (state_->current_tool == tool.tool) {
        selected.emplace(ImGuiCol_Button, gui::GetPrimaryVec4());
      }
      if (gui::CenteredIconButton(tool.icon, button_size)) {
        state_->SetTool(tool.tool);
      }
      gui::AutoRegisterLastItem("button", tool.label);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", tool.label);
      if (&tool != &tools[8])
        ImGui::SameLine();
    }
  }
  ImGui::SameLine();

  // Brush size for pencil/brush/eraser
  if (uses_brush) {
    ImGui::SetNextItemWidth(60);
    int brush = state_->brush_size;
    if (ImGui::SliderInt("##BrushSize", &brush, 1, 8, "%d px")) {
      state_->brush_size = static_cast<uint8_t>(brush);
    }
    HOVER_HINT("Brush size");
    ImGui::SameLine();
  }

  // Undo/Redo buttons
  ImGui::BeginDisabled(HasActiveStroke() || !undo_manager_ ||
                       !undo_manager_->CanUndo());
  if (gui::CenteredIconButton(ICON_MD_UNDO, button_size) && undo_manager_) {
    const auto status = undo_ ? undo_() : undo_manager_->Undo();
    history_error_ = status.ok() ? "" : std::string(status.message());
  }
  gui::AutoRegisterLastItem("button", "Undo pixels");
  HOVER_HINT("Undo (Cmd/Ctrl+Z)");
  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(HasActiveStroke() || !undo_manager_ ||
                       !undo_manager_->CanRedo());
  if (gui::CenteredIconButton(ICON_MD_REDO, button_size) && undo_manager_) {
    const auto status = redo_ ? redo_() : undo_manager_->Redo();
    history_error_ = status.ok() ? "" : std::string(status.message());
  }
  gui::AutoRegisterLastItem("button", "Redo pixels");
  HOVER_HINT("Redo (Cmd/Ctrl+Shift+Z)");
  ImGui::EndDisabled();
}

void PixelEditorPanel::DrawViewControls() {
  ImGui::Checkbox(tr("Pixel grid (6x zoom)"), &state_->show_pixel_grid);
  ImGui::Checkbox(tr("8x8 tile grid"), &state_->show_grid);
  ImGui::Checkbox(tr("16x16 block grid"), &state_->show_tile_boundaries);
  ImGui::Checkbox(tr("Cursor crosshair"), &state_->show_cursor_crosshair);
  ImGui::Checkbox(tr("Brush preview"), &state_->show_brush_preview);
  ImGui::Checkbox(tr("Transparency grid"), &state_->show_transparency_grid);
  if (ImGui::CollapsingHeader(tr("Sheet overview"))) {
    DrawMiniMap();
  }
}

void PixelEditorPanel::DrawCanvas() {
  // Show the sheet the browser has selected rather than an empty editor.
  if (state_->open_sheets.empty() &&
      gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id].is_active()) {
    state_->open_sheets.insert(state_->current_sheet_id);
  }
  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled(
        tr("No sheet selected. Select a sheet from the browser."));
    return;
  }

  // Tab bar for open sheets
  if (gui::BeginThemedTabBar("##SheetTabs",
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

      const bool requested = state_->requested_sheet_tab == sheet_id;
      if (ImGui::BeginTabItem(tab_label.c_str(), &open,
                              requested ? ImGuiTabItemFlags_SetSelected : 0)) {
        if (state_->current_sheet_id != sheet_id)
          FinishGesture();
        state_->current_sheet_id = sheet_id;
        if (requested) {
          state_->requested_sheet_tab.reset();
        }

        // Get the current sheet bitmap
        auto& sheet = DisplaySheet();

        if (!sheet.is_active()) {
          ImGui::TextDisabled(tr("Sheet %02X is not active"), sheet_id);
          ImGui::EndTabItem();
          continue;
        }

        if (fit_canvas_requested_) {
          // Fit once, after the sheet tabs establish the actual canvas space.
          // Resizing the window must not silently change an artist's zoom.
          const ImVec2 available = ImGui::GetContentRegionAvail();
          state_->SetZoom(std::floor(std::min(available.x / sheet.width(),
                                              available.y / sheet.height())));
          fit_canvas_requested_ = false;
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
                     canvas_.zero_point().y + canvas_height),
              ImVec2(0, 0), ImVec2(1, 1),
              ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
        }

        // Draw grid if enabled
        if (state_->show_grid) {
          canvas_.DrawGrid(8.0f * state_->zoom_level);
        }
        DrawPixelGrids(canvas_width, canvas_height);

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
              sel_min, sel_max, ImGui::GetColorU32(IM_COL32(255, 255, 0, 255)),
              0.0f, 2.0f);

          // Marching ants effect (simplified)
          canvas_.draw_list()->AddRect(
              sel_min, sel_max, ImGui::GetColorU32(IM_COL32(0, 0, 0, 128)),
              0.0f, 1.0f);
        }

        DrawFloatingPaste();

        // Draw tool preview (line/rectangle)
        if (show_tool_preview_ && is_drawing_) {
          ImVec2 start = PixelToScreen(static_cast<int>(tool_start_pixel_.x),
                                       static_cast<int>(tool_start_pixel_.y));
          ImVec2 end = PixelToScreen(static_cast<int>(preview_end_.x),
                                     static_cast<int>(preview_end_.y));

          if (state_->current_tool == PixelTool::kLine) {
            canvas_.draw_list()->AddLine(
                start, end, ImGui::GetColorU32(IM_COL32(255, 255, 0, 200)),
                2.0f);
          } else if (state_->current_tool == PixelTool::kRectangle) {
            // Dragging up or left puts end before start; ImGui 1.93 draws
            // nothing for an inverted rect.
            canvas_.draw_list()->AddRect(
                ImMin(start, end), ImMax(start, end),
                ImGui::GetColorU32(IM_COL32(255, 255, 0, 200)), 0.0f, 2.0f);
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

    gui::EndThemedTabBar();
  }
}

void PixelEditorPanel::DrawTransparencyGrid(float canvas_width,
                                            float canvas_height) {
  const float cell_size = 8.0f;  // Checkerboard cell size
  const ImU32 color1 = ImGui::GetColorU32(IM_COL32(180, 180, 180, 255));
  const ImU32 color2 = ImGui::GetColorU32(IM_COL32(220, 220, 220, 255));

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
  canvas_.draw_list()->AddLine(
      v_start, v_end, ImGui::GetColorU32(IM_COL32(255, 100, 100, 100)), 1.0f);

  // Horizontal line through cursor pixel
  ImVec2 h_start(canvas_.zero_point().x, cursor_screen.y + pixel_size / 2);
  ImVec2 h_end(canvas_.zero_point().x + canvas_.canvas_size().x,
               cursor_screen.y + pixel_size / 2);
  canvas_.draw_list()->AddLine(
      h_start, h_end, ImGui::GetColorU32(IM_COL32(255, 100, 100, 100)), 1.0f);

  // Highlight current pixel with a bright outline
  ImVec2 pixel_min = cursor_screen;
  ImVec2 pixel_max(cursor_screen.x + pixel_size, cursor_screen.y + pixel_size);
  canvas_.draw_list()->AddRect(pixel_min, pixel_max,
                               ImGui::GetColorU32(IM_COL32(255, 255, 255, 200)),
                               0.0f, 2.0f);
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
                            ? ImGui::GetColorU32(IM_COL32(255, 0, 0, 50))
                            : ImGui::GetColorU32(IM_COL32(0, 255, 0, 50));
  canvas_.draw_list()->AddRectFilled(brush_min, brush_max, preview_color);

  // Outline
  ImU32 outline_color = (state_->current_tool == PixelTool::kEraser)
                            ? ImGui::GetColorU32(IM_COL32(255, 100, 100, 200))
                            : ImGui::GetColorU32(IM_COL32(100, 255, 100, 200));
  canvas_.draw_list()->AddRect(brush_min, brush_max, outline_color, 0.0f, 1.0f);
}

void PixelEditorPanel::DrawPixelInfoTooltip(const gfx::Bitmap& sheet) {
  if (cursor_x_ < 0 || cursor_x_ >= sheet.width() || cursor_y_ < 0 ||
      cursor_y_ >= sheet.height()) {
    return;
  }

  uint8_t color_index = sheet.GetPixel(cursor_x_, cursor_y_);
  const auto& palette = DisplayPalette();

  ImGui::BeginTooltip();
  ImGui::Text(tr("Pos: %d, %d"), cursor_x_, cursor_y_);
  ImGui::Text(tr("Tile: %d, %d"), cursor_x_ / 8, cursor_y_ / 8);
  ImGui::Text(tr("Index: %d"), color_index);

  if (color_index < palette.size()) {
    ImGui::Text(tr("SNES: $%04X"), palette[color_index].snes());
    ImVec4 color(palette[color_index].rgb().x / 255.0f,
                 palette[color_index].rgb().y / 255.0f,
                 palette[color_index].rgb().z / 255.0f, 1.0f);
    ImGui::ColorButton("##ColorPreview", color, ImGuiColorEditFlags_NoTooltip,
                       ImVec2(24, 24));
    if (color_index == 0) {
      ImGui::SameLine();
      ImGui::TextDisabled(tr("(Transparent)"));
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

  const float pulse =
      0.5f + 0.5f * std::sin(static_cast<float>(elapsed) * 6.0f);
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
  canvas_.draw_list()->AddRect(min, max, outline_color, 0.0f, 2.0f);

  if (ImGui::IsMouseHoveringRect(min, max)) {
    ImGui::BeginTooltip();
    ImGui::Text(tr("Focus tile: %d (sheet %02X)"), tile_index,
                state_->tile_highlight.sheet_id);
    if (!state_->tile_highlight.label.empty()) {
      ImGui::Text("%s", state_->tile_highlight.label.c_str());
    }
    ImGui::EndTooltip();
  }
}

void PixelEditorPanel::DrawColorPicker() {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(4, ImGui::GetStyle().ItemSpacing.y));
  const float right =
      ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
  ImGui::BeginGroup();
  DrawPaletteRowPicker();
  ImGui::EndGroup();
  const float swatch_size = ImGui::GetFrameHeight();
  const float swatch_width =
      9 * swatch_size + 9 * ImGui::GetStyle().ItemSpacing.x;
  if (ImGui::GetItemRectMax().x + swatch_width +
          ImGui::GetStyle().ItemSpacing.x <=
      right) {
    ImGui::SameLine();
  }
  ImGui::BeginGroup();
  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled(tr("No sheet"));
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    return;
  }

  const auto& palette = DisplayPalette();

  // Selection is an index, not a cached RGB value. Resolve it again when the
  // sheet, palette row, or source context changes. Index zero stays transparent.
  const auto display_color = [&palette](int index) {
    ImVec4 color = gui::ConvertSnesColorToImVec4(palette[index]);
    color.w = index == 0 ? 0.0f : 1.0f;
    return color;
  };
  constexpr auto kSwatchFlags = ImGuiColorEditFlags_NoTooltip |
                                ImGuiColorEditFlags_NoBorder |
                                ImGuiColorEditFlags_AlphaPreview;

  // Sheet editing is indexed 3bpp; rendered 4bpp expansion is not authored.
  for (int i = 0; i < static_cast<int>(palette.size()) && i < 8; i++) {
    if (i > 0) {
      ImGui::SameLine();
    }

    std::string id = absl::StrFormat("##Color%d", i);
    if (ImGui::ColorButton(id.c_str(), display_color(i), kSwatchFlags,
                           ImVec2(swatch_size, swatch_size))) {
      state_->current_color_index = static_cast<uint8_t>(i);
    }
    gui::AutoRegisterLastItem("color", absl::StrFormat("Palette index %d", i));

    if (state_->current_color_index == i) {
      const auto min = ImGui::GetItemRectMin();
      const auto max = ImGui::GetItemRectMax();
      ImGui::GetWindowDrawList()->AddRect(
          ImVec2(min.x - 1, min.y - 1), ImVec2(max.x + 1, max.y + 1),
          ImGui::GetColorU32(gui::GetSelectedColor()),
          ImGui::GetStyle().FrameRounding, 1.5f);
    }

    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text(tr("Index: %d"), i);
      ImGui::Text(tr("SNES: $%04X"), palette[i].snes());
      ImGui::Text(tr("RGB: %d, %d, %d"), static_cast<int>(palette[i].rgb().x),
                  static_cast<int>(palette[i].rgb().y),
                  static_cast<int>(palette[i].rgb().z));
      if (i == 0) {
        ImGui::Text(tr("(Transparent)"));
      }
      ImGui::EndTooltip();
    }
  }

  // Keep the foreground swatch adjacent to the indexed colors.
  ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x * 2);
  const int index = state_->current_color_index;
  if (index >= 8 || index >= palette.size()) {
    ImGui::TextDisabled("?");
    HOVER_HINT("Selected palette index is unavailable");
    ImGui::EndGroup();
    ImGui::PopStyleVar();
    return;
  }
  ImGui::ColorButton("##CurrentColor", display_color(index), kSwatchFlags,
                     ImVec2(swatch_size, swatch_size));
  gui::AutoRegisterLastItem("color", "Current color");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Current color: %d%s"), index,
                      index == 0 ? tr(" (transparent)") : "");
  }
  ImGui::EndGroup();
  ImGui::PopStyleVar();
  palette_reorder_.Draw(*state_, apply_palette_reorder_, HasActiveStroke());
}

void PixelEditorPanel::DrawMiniMap() {
  if (state_->open_sheets.empty()) {
    ImGui::TextDisabled(tr("No sheet"));
    return;
  }

  auto& sheet = DisplaySheet();
  if (!sheet.texture())
    return;

  // Draw mini version of the sheet
  float mini_scale = 0.5f;
  float mini_width = sheet.width() * mini_scale;
  float mini_height = sheet.height() * mini_scale;

  ImVec2 pos = ImGui::GetCursorScreenPos();

  ImGui::GetWindowDrawList()->AddImage(
      (ImTextureID)(intptr_t)sheet.texture(), pos,
      ImVec2(pos.x + mini_width, pos.y + mini_height), ImVec2(0, 0),
      ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));

  // Draw viewport rectangle
  // TODO: Calculate actual viewport bounds based on scroll position

  ImGui::Dummy(ImVec2(mini_width, mini_height));
}

void PixelEditorPanel::DrawStatusBar() {
  ImGui::Separator();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(4, ImGui::GetStyle().ItemSpacing.y));
  const float button_size = ImGui::GetFrameHeight();
  if (gui::CenteredIconButton(ICON_MD_ZOOM_OUT, button_size)) {
    state_->ZoomOut();
  }
  gui::AutoRegisterLastItem("button", "Zoom out");
  HOVER_HINT("Zoom out (-)");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::CalcTextSize("16x").x + button_size + 8);
  if (ImGui::BeginCombo("##PixelZoom",
                        absl::StrFormat("%.0fx", state_->zoom_level).c_str())) {
    for (int zoom = 1; zoom <= 16; ++zoom) {
      if (ImGui::Selectable(absl::StrFormat("%dx", zoom).c_str(),
                            state_->zoom_level == zoom)) {
        state_->SetZoom(zoom);
      }
    }
    ImGui::EndCombo();
  }
  ImGui::SameLine();
  if (gui::CenteredIconButton(ICON_MD_ZOOM_IN, button_size)) {
    state_->ZoomIn();
  }
  gui::AutoRegisterLastItem("button", "Zoom in");
  HOVER_HINT("Zoom in (+)");
  ImGui::SameLine();
  if (ImGui::Button(tr("Fit"))) {
    fit_canvas_requested_ = true;
  }
  gui::AutoRegisterLastItem("button", "Fit sheet");
  HOVER_HINT("Fit this sheet at an integer zoom and reset panning");
  ImGui::SameLine();

  std::string status = absl::StrFormat(
      "%s | %02X | %s %d", state_->GetToolName(), state_->current_sheet_id,
      tr("Color"), state_->current_color_index);
  if (cursor_in_canvas_) {
    absl::StrAppendFormat(&status, " | %d,%d | %s %d", cursor_x_, cursor_y_,
                          tr("Tile"), cursor_y_ / 8 * 16 + cursor_x_ / 8);
  }
  if (state_->tile_highlight.active &&
      state_->tile_highlight.sheet_id == state_->current_sheet_id) {
    absl::StrAppendFormat(&status, " | %s %d", tr("Focus"),
                          state_->tile_highlight.tile_index);
  }
  if (state_->modified_sheets.contains(state_->current_sheet_id)) {
    status += tr(" | Modified");
  }
  if (!history_error_.empty())
    status += " | " + history_error_;
  // Long cursor/source metadata must not push view controls off a small panel.
  ImGui::BeginChild(
      "##PixelStatus", ImVec2(0, button_size), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(status.c_str());
  if (ImGui::IsWindowHovered()) {
    ImGui::SetTooltip("%s", status.c_str());
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
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

  if (HandlePanInput() || HandleFloatingPasteInput()) {
    return;
  }

  // Mouse button handling
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    is_drawing_ = true;
    gesture_tool_ = state_->current_tool;
    tool_start_pixel_ =
        ImVec2(static_cast<float>(cursor_x_), static_cast<float>(cursor_y_));
    last_mouse_pixel_ = tool_start_pixel_;

    // Pixel writes until mouse-up form one stroke (one undo step).
    BeginStroke();

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

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && is_drawing_)
    FinishGesture();
}

void PixelEditorPanel::FinishReleasedGesture() {
  if (is_drawing_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    // The canvas release path already handles the current endpoint. This is
    // the outside/hidden-panel fallback, so retain the last valid drag point.
    cursor_x_ = static_cast<int>(last_mouse_pixel_.x);
    cursor_y_ = static_cast<int>(last_mouse_pixel_.y);
    FinishGesture();
  }
}

void PixelEditorPanel::FinishGesture() {
  if ((is_drawing_ || stroke_) && ActiveSheetStore(*state_) == nullptr)
    return;
  if (is_drawing_) {
    // Use the last point inside the canvas, never the control released over.
    // Explicit sheet navigation calls this before changing the selected sheet.
    switch (gesture_tool_) {
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
  }
  is_drawing_ = false;
  show_tool_preview_ = false;
  EndStroke();
}

void PixelEditorPanel::ApplyPencil(int x, int y) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }
  ShowStrokeChanges(edit->Set(x, y, state_->current_color_index));
}

void PixelEditorPanel::ApplyBrush(int x, int y) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }
  int size = state_->brush_size;
  int half = size / 2;
  bool changed = false;
  for (int dy = -half; dy < size - half; dy++) {
    for (int dx = -half; dx < size - half; dx++) {
      changed |= edit->Set(x + dx, y + dy, state_->current_color_index);
    }
  }
  ShowStrokeChanges(changed);
}

void PixelEditorPanel::ApplyEraser(int x, int y) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }
  int size = state_->brush_size;
  int half = size / 2;
  bool changed = false;
  for (int dy = -half; dy < size - half; dy++) {
    for (int dx = -half; dx < size - half; dx++) {
      changed |= edit->Set(x + dx, y + dy, 0);  // Index 0 = transparent
    }
  }
  ShowStrokeChanges(changed);
}

void PixelEditorPanel::ApplyFill(int x, int y) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }
  const int width = edit->width();
  const int height = edit->height();
  if (x < 0 || x >= width || y < 0 || y >= height)
    return;

  uint8_t target_color = edit->Get(x, y);
  uint8_t fill_color = state_->current_color_index;

  if (target_color == fill_color)
    return;  // Nothing to fill

  // BFS flood fill
  std::queue<std::pair<int, int>> queue;
  std::vector<bool> visited(width * height, false);

  queue.push({x, y});
  visited[y * width + x] = true;

  while (!queue.empty()) {
    auto [cx, cy] = queue.front();
    queue.pop();

    edit->Set(cx, cy, fill_color);

    // Check 4-connected neighbors
    const int dx[] = {0, 0, -1, 1};
    const int dy[] = {-1, 1, 0, 0};

    for (int i = 0; i < 4; i++) {
      int nx = cx + dx[i];
      int ny = cy + dy[i];

      if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
        int idx = ny * width + nx;
        if (!visited[idx] && edit->Get(nx, ny) == target_color) {
          visited[idx] = true;
          queue.push({nx, ny});
        }
      }
    }
  }

  ShowStrokeChanges(true);
}

void PixelEditorPanel::ApplyEyedropper(int x, int y) {
  auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];

  if (x >= 0 && x < sheet.width() && y >= 0 && y < sheet.height()) {
    state_->current_color_index = sheet.GetPixel(x, y);
  }
}

void PixelEditorPanel::DrawLine(int x1, int y1, int x2, int y2) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }

  // Bresenham's line algorithm
  int dx = std::abs(x2 - x1);
  int dy = std::abs(y2 - y1);
  int sx = x1 < x2 ? 1 : -1;
  int sy = y1 < y2 ? 1 : -1;
  int err = dx - dy;
  bool changed = false;

  while (true) {
    changed |= edit->Set(x1, y1, state_->current_color_index);

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

  ShowStrokeChanges(changed);
}

void PixelEditorPanel::DrawRectangle(int x1, int y1, int x2, int y2,
                                     bool filled) {
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }

  int min_x = std::min(x1, x2);
  int max_x = std::max(x1, x2);
  int min_y = std::min(y1, y2);
  int max_y = std::max(y1, y2);
  const uint8_t color = state_->current_color_index;
  bool changed = false;

  if (filled) {
    for (int y = min_y; y <= max_y; y++) {
      for (int x = min_x; x <= max_x; x++) {
        changed |= edit->Set(x, y, color);
      }
    }
  } else {
    // Top and bottom edges
    for (int x = min_x; x <= max_x; x++) {
      changed |= edit->Set(x, min_y, color);
      changed |= edit->Set(x, max_y, color);
    }
    // Left and right edges
    for (int y = min_y; y <= max_y; y++) {
      changed |= edit->Set(min_x, y, color);
      changed |= edit->Set(max_x, y, color);
    }
  }

  ShowStrokeChanges(changed);
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

    state_->selection.palette = DisplayPalette();
  }
}

void PixelEditorPanel::CopySelection() {
  // Selection data is already in state_->selection
}

void PixelEditorPanel::PasteSelection(int x, int y) {
  if (state_->selection.pixel_data.empty())
    return;

  BeginStroke();
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    return;
  }
  bool changed = false;
  for (int dy = 0; dy < state_->selection.height; dy++) {
    for (int dx = 0; dx < state_->selection.width; dx++) {
      uint8_t pixel =
          state_->selection.pixel_data[dy * state_->selection.width + dx];
      changed |= edit->Set(x + dx, y + dy, pixel);
    }
  }
  ShowStrokeChanges(changed);
  EndStroke();
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

void PixelEditorPanel::BeginStroke() {
  EndStroke();  // a stroke left open ends here
  auto* store = ActiveSheetStore(*state_);
  if (store == nullptr) {
    return;
  }
  stroke_.emplace(*store, state_->current_sheet_id);
  if (!stroke_->ok()) {
    stroke_.reset();
  }
}

zelda3::GraphicsSheetStore::Edit* PixelEditorPanel::StrokeEdit() {
  if (!stroke_ || stroke_->sheet() != state_->current_sheet_id) {
    BeginStroke();
  }
  return stroke_ ? &*stroke_ : nullptr;
}

void PixelEditorPanel::ShowStrokeChanges(bool changed) {
  if (!stroke_ || !changed) {
    return;
  }
  RefreshArenaSheet(*state_, stroke_->sheet());
  state_->MarkSheetModified(stroke_->sheet());
}

void PixelEditorPanel::EndStroke() {
  if (!stroke_) {
    return;
  }
  const uint16_t sheet = stroke_->sheet();
  SheetPixelDiff diff =
      MakeSheetPixelDiff(sheet, stroke_->before(), stroke_->pixels());
  stroke_.reset();  // commits: one revision for the whole stroke
  RefreshArenaSheet(*state_, sheet);
  if (diff.empty()) {
    return;
  }
  state_->MarkSheetModified(sheet);
  if (undo_manager_ != nullptr) {
    undo_manager_->Push(std::make_unique<GraphicsPixelEditAction>(
        state_, std::move(diff),
        absl::StrFormat("Edit pixels on sheet %02X", sheet)));
  }
}

bool PixelEditorPanel::HandlePanInput() {
  const bool hand = state_->current_tool == PixelTool::kHand;
  const bool middle = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  if (!middle && !(hand && ImGui::IsMouseDown(ImGuiMouseButton_Left))) {
    return hand;  // the Hand tool never paints
  }
  // DrawCanvas runs inside the scrolling "##CanvasArea" child.
  const ImVec2 delta = ImGui::GetIO().MouseDelta;
  ImGui::SetScrollX(ImGui::GetScrollX() - delta.x);
  ImGui::SetScrollY(ImGui::GetScrollY() - delta.y);
  return true;
}

void PixelEditorPanel::DrawPixelGrids(float canvas_width, float canvas_height) {
  auto* draw_list = canvas_.draw_list();
  const ImVec2 origin = canvas_.zero_point();
  const float zoom = state_->zoom_level;
  auto lines = [&](float step, ImU32 color, float thickness) {
    for (float x = step; x < canvas_width; x += step) {
      draw_list->AddLineV(origin.x + x, origin.y, origin.y + canvas_height,
                          color, thickness);
    }
    for (float y = step; y < canvas_height; y += step) {
      draw_list->AddLineH(origin.x, origin.x + canvas_width, origin.y + y,
                          color, thickness);
    }
  };
  if (state_->show_pixel_grid && zoom >= 6.0f) {
    lines(zoom, ImGui::GetColorU32(IM_COL32(255, 255, 255, 28)), 1.0f);
  }
  if (state_->show_tile_boundaries) {
    lines(16.0f * zoom, ImGui::GetColorU32(gui::GetInfoColor()), 1.5f);
  }
}

void PixelEditorPanel::DrawPaletteRowPicker() {
  if (state_->SourceTarget()) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", tr("Source colors"));
    HOVER_HINT("Colors resolved by the source editor");
    return;
  }
  DrawGraphicsPreviewPaletteSelector(*state_,
                                     GraphicsPaletteSelectorLayout::kInline);
}

SheetColors PixelEditorPanel::CurrentSheetColors() const {
  SheetColors colors{};
  const auto& palette = DisplayPalette();
  for (size_t i = 0; i < colors.size(); ++i) {
    if (i < palette.size()) {
      const ImVec4 rgb = palette[i].rgb();  // 0-255 components
      colors[i] = {static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                   static_cast<uint8_t>(rgb.z)};
    } else {
      const auto gray = static_cast<uint8_t>(i * 255 / 7);
      colors[i] = {gray, gray, gray};
    }
  }
  return colors;
}

// Cmd+C/X/V reach Copy/Cut/Paste through the editor shortcuts
// (GraphicsEditor::Copy and friends); only the floating paste's own keys are
// handled here.
void PixelEditorPanel::HandleFloatingPasteKeys() {
  if (!state_->selection.is_floating ||
      !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) ||
      ImGui::GetIO().WantTextInput) {
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Enter) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
    CommitFloatingPaste();
    return;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    CancelFloatingPaste();
    return;
  }
  const int step = ImGui::GetIO().KeyShift ? 8 : 1;
  auto& selection = state_->selection;
  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    selection.x -= step;
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    selection.x += step;
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    selection.y -= step;
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    selection.y += step;
  ClampFloatingPaste();
}

void PixelEditorPanel::DrawClipboardControls() {
  const bool image_clipboard = util::ImageClipboardSupported();
  ImGui::BeginDisabled(!image_clipboard);
  if (gui::ToolbarIconButton(ICON_MD_CONTENT_COPY,
                             "Copy selection, or the whole sheet, as a PNG "
                             "(Cmd+C)")) {
    CopyToSystemClipboard();
  }
  ImGui::SameLine();
  if (gui::ToolbarIconButton(ICON_MD_CONTENT_PASTE,
                             "Paste a PNG from the clipboard as a floating "
                             "selection (Cmd+V)")) {
    PasteFromSystemClipboard();
  }
  ImGui::EndDisabled();
  if (!image_clipboard) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("(image clipboard: macOS only)"));
  }
  if (state_->selection.is_floating) {
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CHECK " Commit")) {
      CommitFloatingPaste();
    }
    HOVER_HINT("Write the floating paste into the sheet (Enter)");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CLOSE " Cancel")) {
      CancelFloatingPaste();
    }
    HOVER_HINT("Drop the floating paste (Esc)");
    ImGui::SameLine();
    ImGui::Checkbox(tr("Skip color 0"), &paste_skip_color0_);
    HOVER_HINT("Leave pixels under color 0 of the paste unchanged");
  }
  if (!clipboard_status_.empty()) {
    ImGui::TextColored(
        clipboard_status_is_error_ ? gui::GetErrorColor() : gui::GetInfoColor(),
        "%s", clipboard_status_.c_str());
  }
}

void PixelEditorPanel::CopyToSystemClipboard() {
  const auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  if (!sheet.is_active()) {
    clipboard_status_ = "No sheet to copy";
    clipboard_status_is_error_ = true;
    return;
  }
  const auto& selection = state_->selection;
  int x0 = 0;
  int y0 = 0;
  int width = sheet.width();
  int height = sheet.height();
  if (selection.is_active && !selection.is_floating && selection.width > 0 &&
      selection.height > 0) {
    x0 = selection.x;
    y0 = selection.y;
    width = selection.width;
    height = selection.height;
  }
  // Read the current pixels; the selection's copy may predate later edits.
  std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, 0);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int sx = x0 + x;
      const int sy = y0 + y;
      if (sx >= 0 && sx < sheet.width() && sy >= 0 && sy < sheet.height()) {
        pixels[y * width + x] = sheet.GetPixel(sx, sy);
      }
    }
  }
  auto png = EncodeClipboardPng(pixels, width, height, CurrentSheetColors());
  const absl::Status status =
      png.ok() ? util::SetClipboardPng(*png) : png.status();
  clipboard_status_ =
      status.ok() ? absl::StrFormat("Copied %dx%d from sheet %02X", width,
                                    height, state_->current_sheet_id)
                  : std::string(status.message());
  clipboard_status_is_error_ = !status.ok();
}

void PixelEditorPanel::CutToSystemClipboard() {
  const auto& selection = state_->selection;
  if (!selection.is_active || selection.is_floating || selection.width <= 0 ||
      selection.height <= 0) {
    clipboard_status_ = "Select pixels to cut";
    clipboard_status_is_error_ = true;
    return;
  }
  CopyToSystemClipboard();
  if (clipboard_status_is_error_) {
    return;
  }
  BeginStroke();
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    clipboard_status_ = "Copied, but this sheet cannot be edited here";
    clipboard_status_is_error_ = true;
    return;
  }
  bool changed = false;
  for (int y = selection.y; y < selection.y + selection.height; ++y) {
    for (int x = selection.x; x < selection.x + selection.width; ++x) {
      changed |= edit->Set(x, y, 0);
    }
  }
  ShowStrokeChanges(changed);
  EndStroke();
  clipboard_status_ =
      absl::StrFormat("Cut %dx%d from sheet %02X", selection.width,
                      selection.height, state_->current_sheet_id);
}

void PixelEditorPanel::PasteFromSystemClipboard() {
  const auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  if (!sheet.is_active()) {
    clipboard_status_ = "Open a sheet before pasting";
    clipboard_status_is_error_ = true;
    return;
  }
  auto png = util::GetClipboardPng();
  auto paste = png.ok() ? MapClipboardPng(*png, CurrentSheetColors(),
                                          sheet.width(), sheet.height())
                        : absl::StatusOr<ClipboardPaste>(png.status());
  if (!paste.ok()) {
    clipboard_status_ = std::string(paste.status().message());
    clipboard_status_is_error_ = true;
    return;
  }
  auto& selection = state_->selection;
  // Float over the current selection's corner, or the sheet's top left.
  const int x = selection.is_active ? selection.x : 0;
  const int y = selection.is_active ? selection.y : 0;
  selection.pixel_data = paste->indices;
  selection.width = paste->width;
  selection.height = paste->height;
  selection.x = x;
  selection.y = y;
  selection.is_active = true;
  selection.is_floating = true;
  ClampFloatingPaste();
  clipboard_status_ =
      "Paste " + DescribeClipboardPaste(*paste) +
      (paste->above_depth > 0 ? " (colors above 7 were mapped by color)" : "") +
      ". Drag or use arrows, Enter to commit, Esc to cancel.";
  clipboard_status_is_error_ = false;
}

void PixelEditorPanel::ClampFloatingPaste() {
  const auto& sheet = gfx::Arena::Get().gfx_sheets()[state_->current_sheet_id];
  auto& selection = state_->selection;
  selection.x =
      std::clamp(selection.x, 0, std::max(0, sheet.width() - selection.width));
  selection.y = std::clamp(selection.y, 0,
                           std::max(0, sheet.height() - selection.height));
}

void PixelEditorPanel::CommitFloatingPaste() {
  auto& selection = state_->selection;
  if (!selection.is_floating) {
    return;
  }
  BeginStroke();
  auto* edit = StrokeEdit();
  if (edit == nullptr) {
    clipboard_status_ = "This sheet cannot be edited here";
    clipboard_status_is_error_ = true;
    return;
  }
  bool changed = false;
  for (int dy = 0; dy < selection.height; ++dy) {
    for (int dx = 0; dx < selection.width; ++dx) {
      const uint8_t pixel = selection.pixel_data[dy * selection.width + dx];
      if (paste_skip_color0_ && pixel == 0) {
        continue;
      }
      changed |= edit->Set(selection.x + dx, selection.y + dy, pixel);
    }
  }
  ShowStrokeChanges(changed);
  EndStroke();
  selection.is_floating = false;
  dragging_paste_ = false;
  clipboard_status_ =
      absl::StrFormat("Pasted %dx%d at %d,%d", selection.width,
                      selection.height, selection.x, selection.y);
  clipboard_status_is_error_ = false;
}

void PixelEditorPanel::CancelFloatingPaste() {
  state_->selection.Clear();
  dragging_paste_ = false;
  clipboard_status_ = "Paste cancelled";
  clipboard_status_is_error_ = false;
}

bool PixelEditorPanel::HandleFloatingPasteInput() {
  auto& selection = state_->selection;
  if (!selection.is_floating) {
    return false;
  }
  const bool inside =
      cursor_x_ >= selection.x && cursor_x_ < selection.x + selection.width &&
      cursor_y_ >= selection.y && cursor_y_ < selection.y + selection.height;
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (inside) {
      dragging_paste_ = true;
      paste_drag_dx_ = cursor_x_ - selection.x;
      paste_drag_dy_ = cursor_y_ - selection.y;
    } else {
      CommitFloatingPaste();
    }
    return true;
  }
  if (dragging_paste_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    selection.x = cursor_x_ - paste_drag_dx_;
    selection.y = cursor_y_ - paste_drag_dy_;
    ClampFloatingPaste();
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    dragging_paste_ = false;
  }
  return true;
}

void PixelEditorPanel::DrawFloatingPaste() {
  const auto& selection = state_->selection;
  if (!selection.is_floating) {
    return;
  }
  const SheetColors colors = CurrentSheetColors();
  auto* draw_list = canvas_.draw_list();
  for (int dy = 0; dy < selection.height; ++dy) {
    for (int dx = 0; dx < selection.width; ++dx) {
      const uint8_t pixel = selection.pixel_data[dy * selection.width + dx];
      if (paste_skip_color0_ && pixel == 0) {
        continue;
      }
      const auto& rgb = colors[pixel & 7];
      const ImVec2 p0 = PixelToScreen(selection.x + dx, selection.y + dy);
      const ImVec2 p1 =
          PixelToScreen(selection.x + dx + 1, selection.y + dy + 1);
      // Keep adjacent pixels hard-edged at fractional zoom, as in dungeon
      // sprite previews. AddRectFilled adds an AA fringe to fractional bounds.
      draw_list->PrimReserve(6, 4);
      draw_list->PrimRect(
          p0, p1, ImGui::GetColorU32(IM_COL32(rgb[0], rgb[1], rgb[2], 230)));
    }
  }
  draw_list->AddRect(PixelToScreen(selection.x, selection.y),
                     PixelToScreen(selection.x + selection.width,
                                   selection.y + selection.height),
                     ImGui::GetColorU32(gui::GetInfoColor()), 0.0f, 2.0f);
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
