// Read-only proposal preview: real room renders side by side with a
// DungeonProposalOverlay drawn on top. Reuses the connected-view composite
// cache; never edits room data.

#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_proposal_overlay.h"
#include "app/editor/dungeon/ui_constants.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas_geometry.h"
#include "app/gui/canvas/canvas_pipelines.h"
#include "app/gui/canvas/canvas_utils.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor {
namespace {

using dungeon_ui::kDungeonRoomPixelSize;

constexpr float kProposalRoomGap = 24.0f;
constexpr float kProposalLabelHeight = 22.0f;

float ProposalHeaderHeight() {
  if (!ImGui::GetCurrentContext())
    return kProposalLabelHeight;
  return std::max(kProposalLabelHeight, ImGui::GetTextLineHeight() + 4.0f);
}

ImU32 ToImColor(uint32_t rgba, float alpha_scale = 1.0f) {
  const int r = static_cast<int>((rgba >> 24) & 0xFF);
  const int g = static_cast<int>((rgba >> 16) & 0xFF);
  const int b = static_cast<int>((rgba >> 8) & 0xFF);
  const int a = std::clamp(
      static_cast<int>(static_cast<float>(rgba & 0xFF) * alpha_scale), 0, 255);
  return IM_COL32(r, g, b, a);
}

void DrawOutlinedText(ImDrawList* draw_list, ImVec2 pos, ImU32 color,
                      const std::string& text) {
  if (text.empty()) {
    return;
  }
  const ImVec2 size = ImGui::CalcTextSize(text.c_str());
  draw_list->AddRectFilled(ImVec2(pos.x - 2, pos.y - 1),
                           ImVec2(pos.x + size.x + 2, pos.y + size.y + 1),
                           IM_COL32(0, 0, 0, 170), 3.0f);
  draw_list->AddText(pos, color, text.c_str());
}

void DrawArrow(ImDrawList* draw_list, ImVec2 a, ImVec2 b, ImU32 color,
               float scale) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.001f)
    return;
  const float ux = dx / len;
  const float uy = dy / len;
  const float head = std::max(5.0f, 6.0f * scale);
  draw_list->AddLine(a, b, color, std::max(2.0f, 2.0f * scale));
  draw_list->AddTriangleFilled(b,
                               ImVec2(b.x - ux * head - uy * head * 0.6f,
                                      b.y - uy * head + ux * head * 0.6f),
                               ImVec2(b.x - ux * head + uy * head * 0.6f,
                                      b.y - uy * head - ux * head * 0.6f),
                               color);
}

// UI-only selection, scoped to the preview window and proposal title. Loading
// an older proposal with no steps continues to show its complete overlay.
int DrawProposalStepSelector(const DungeonProposalOverlay& overlay) {
  if (overlay.steps.empty())
    return -1;
  ImGui::PushID(overlay.title.c_str());
  const ImGuiID key = ImGui::GetID("ProposalSelectedStep");
  ImGuiStorage* storage = ImGui::GetStateStorage();
  int selected = storage->GetInt(key, -1);
  if (selected < -1 || static_cast<size_t>(selected) >= overlay.steps.size())
    selected = -1;
  ImGui::SetNextItemWidth(-1.0f);
  const char* label =
      selected < 0 ? tr("All steps") : overlay.steps[selected].label.c_str();
  if (ImGui::BeginCombo(tr("Preview step"), label)) {
    if (ImGui::Selectable(tr("All steps"), selected < 0))
      selected = -1;
    for (size_t i = 0; i < overlay.steps.size(); ++i) {
      const auto& step = overlay.steps[i];
      ImGui::PushID(step.id.c_str());
      if (ImGui::Selectable(step.label.c_str(),
                            selected == static_cast<int>(i)))
        selected = static_cast<int>(i);
      ImGui::PopID();
    }
    ImGui::EndCombo();
  }
  ImGui::BeginDisabled(selected <= 0);
  if (ImGui::Button(tr("Previous")))
    --selected;
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(selected + 1 >= static_cast<int>(overlay.steps.size()));
  if (ImGui::Button(tr("Next")))
    ++selected;
  ImGui::EndDisabled();
  storage->SetInt(key, selected);
  if (selected >= 0 && !overlay.steps[selected].detail.empty())
    ImGui::TextWrapped("%s", overlay.steps[selected].detail.c_str());
  ImGui::TextDisabled("%s", tr("Preview only. Actor markers and object "
                               "footprints do not change the room."));
  ImGui::PopID();
  return selected;
}

void DrawProposalShape(ImDrawList* draw_list, const ProposalShape& shape,
                       const ProposalLayer& layer, ImVec2 room_origin,
                       float scale) {
  const ImU32 stroke = ToImColor(layer.color_rgba);
  const ImU32 fill = ToImColor(layer.color_rgba, 0.30f);
  const ImU32 text = IM_COL32(255, 255, 255, 255);
  auto to_screen = [&](float x, float y) {
    return ImVec2(room_origin.x + x * scale, room_origin.y + y * scale);
  };
  switch (shape.type) {
    case ProposalShapeType::kActor: {
      const auto r = ProposalFootprintToPixels(shape);
      const ImVec2 p0 = to_screen(r.x0, r.y0);
      const ImVec2 p1 = to_screen(r.x1, r.y1);
      const ImVec2 center =
          to_screen((r.x0 + r.x1) * 0.5f, (r.y0 + r.y1) * 0.5f);
      // Deliberately abstract: do not imply loaded NPC art or collision bounds.
      draw_list->AddRectFilled(p0, p1, fill, 3.0f * scale);
      draw_list->AddRect(p0, p1, stroke, 3.0f * scale, 0, 2.0f);
      ImVec2 direction(0, 0);
      switch (shape.facing) {
        case ProposalFacing::kNorth:
          direction.y = -1;
          break;
        case ProposalFacing::kEast:
          direction.x = 1;
          break;
        case ProposalFacing::kSouth:
          direction.y = 1;
          break;
        case ProposalFacing::kWest:
          direction.x = -1;
          break;
      }
      DrawArrow(draw_list, center,
                ImVec2(center.x + direction.x * 14 * scale,
                       center.y + direction.y * 14 * scale),
                stroke, scale);
      DrawOutlinedText(draw_list, ImVec2(p1.x + 5, p0.y), text, shape.label);
      break;
    }
    case ProposalShapeType::kObject: {
      const auto r = ProposalFootprintToPixels(shape);
      const ImVec2 p0 = to_screen(r.x0, r.y0);
      const ImVec2 p1 = to_screen(r.x1, r.y1);
      draw_list->AddRectFilled(p0, p1, ToImColor(layer.color_rgba, 0.12f));
      // Corner brackets keep the underlying room legible and distinguish a
      // proposed footprint from an already rendered object.
      const float tick =
          std::min(8.0f * scale, std::min(p1.x - p0.x, p1.y - p0.y) * 0.4f);
      for (int corner = 0; corner < 4; ++corner) {
        const bool right = (corner & 1) != 0;
        const bool bottom = (corner & 2) != 0;
        const ImVec2 p(right ? p1.x : p0.x, bottom ? p1.y : p0.y);
        draw_list->AddLine(p, ImVec2(p.x + (right ? -tick : tick), p.y), stroke,
                           2);
        draw_list->AddLine(p, ImVec2(p.x, p.y + (bottom ? -tick : tick)),
                           stroke, 2);
      }
      const std::string label =
          absl::StrFormat("$%03X%s%s", shape.object_id,
                          shape.label.empty() ? "" : " ", shape.label);
      DrawOutlinedText(draw_list, ImVec2(p0.x + 3, p0.y + 2), text, label);
      break;
    }
    case ProposalShapeType::kRect: {
      const auto r = ProposalTileRectToPixels(shape.tiles[0], shape.tiles[1]);
      const ImVec2 p0 = to_screen(r.x0, r.y0);
      const ImVec2 p1 = to_screen(r.x1, r.y1);
      draw_list->AddRectFilled(p0, p1, fill);
      draw_list->AddRect(p0, p1, stroke, 0.0f, 2.0f);
      DrawOutlinedText(draw_list, ImVec2(p0.x + 3, p0.y + 2), text,
                       shape.label);
      break;
    }
    case ProposalShapeType::kPath: {
      std::vector<ImVec2> points;
      points.reserve(shape.tiles.size());
      for (const auto& tile : shape.tiles) {
        float x, y;
        ProposalTileCenter(tile, &x, &y);
        points.push_back(to_screen(x, y));
      }
      const float thickness = std::max(2.0f, 3.0f * scale);
      draw_list->AddPolyline(points.data(), static_cast<int>(points.size()),
                             stroke, thickness);
      // Arrowhead on the last segment shows travel direction.
      const ImVec2 a = points[points.size() - 2];
      const ImVec2 b = points.back();
      DrawArrow(draw_list, a, b, stroke, scale);
      if (!shape.label.empty()) {
        const ImVec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
        DrawOutlinedText(draw_list, ImVec2(mid.x + 4, mid.y + 4), text,
                         shape.label);
      }
      break;
    }
    case ProposalShapeType::kMarker: {
      float x, y;
      ProposalTileCenter(shape.tiles[0], &x, &y);
      const ImVec2 c = to_screen(x, y);
      const float radius = std::max(5.0f, 5.0f * scale);
      draw_list->AddCircleFilled(c, radius, stroke);
      draw_list->AddCircle(c, radius, IM_COL32(255, 255, 255, 230), 0, 1.5f);
      DrawOutlinedText(draw_list, ImVec2(c.x + radius + 3, c.y - 7), text,
                       shape.label);
      break;
    }
    case ProposalShapeType::kRemove: {
      float x, y;
      ProposalTileCenter(shape.tiles[0], &x, &y);
      const ImVec2 c = to_screen(x, y);
      const float r = std::max(6.0f, 6.0f * scale);
      const float thickness = std::max(2.0f, 2.5f * scale);
      draw_list->AddCircle(c, r + 2.0f, stroke, 0, thickness * 0.6f);
      draw_list->AddLine(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r),
                         stroke, thickness);
      draw_list->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x + r, c.y - r),
                         stroke, thickness);
      DrawOutlinedText(draw_list, ImVec2(c.x + r + 4, c.y - 7), text,
                       shape.label);
      break;
    }
  }
}

}  // namespace

ImVec2 DungeonCanvasViewer::GetProposalPreviewContentSize(
    const DungeonProposalOverlay& overlay, float scale) const {
  const int count = static_cast<int>(overlay.rooms.size());
  if (count == 0) {
    return ImVec2(0, 0);
  }
  const float width =
      ProposalRoomOriginX(count - 1, kProposalRoomGap) + kDungeonRoomPixelSize;
  return ImVec2(width * scale,
                ProposalHeaderHeight() + kDungeonRoomPixelSize * scale);
}

void DungeonCanvasViewer::DrawProposalPreview(
    const DungeonProposalOverlay& overlay,
    const std::vector<char>& layer_visible, bool show_overlay,
    gui::Canvas& preview_canvas, bool show_grid, int grid_step,
    bool& auto_fit) {
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
    // Preview focus must not preserve the editable room's keyboard ownership.
    room_canvas_shortcut_focus_ = false;
  }
  if (!rom_ || !rom_->is_loaded() || !rooms_) {
    ImGui::TextDisabled("%s", tr("Load a ROM to render the proposal rooms."));
    return;
  }
  if (overlay.rooms.empty()) {
    return;
  }
  const int selected_step = DrawProposalStepSelector(overlay);

  // Prepare every room composite first, then upload textures once so a newly
  // opened preview draws in the same frame.
  std::vector<gfx::Bitmap*> bitmaps;
  bitmaps.reserve(overlay.rooms.size());
  for (const auto& room : overlay.rooms) {
    // Rooms the user has not opened are materialized but not loaded; load
    // them from the ROM the same way the connected-room view does, or the
    // composite shows placeholder data.
    EnsureRoomLoadedForConnectedView(room.room_id);
    bitmaps.push_back(PrepareConnectedRoomCompositeBitmap(room.room_id));
  }
  gfx::Arena::Get().ProcessTextureQueue(renderer_);

  const ImVec2 content = GetProposalPreviewContentSize(overlay, 1.0f);
  const ImVec2 available = ImGui::GetContentRegionAvail();
  const ImVec2 viewport(std::max(1.0f, available.x),
                        std::max(1.0f, available.y));
  const float header_height = ProposalHeaderHeight();
  if (auto_fit) {
    const auto fit = gui::ComputeZoomToFit(
        ImVec2(content.x, kDungeonRoomPixelSize),
        ImVec2(viewport.x, std::max(1.0f, viewport.y - header_height)), 12.0f);
    preview_canvas.set_global_scale(std::max(0.01f, fit.scale));
    preview_canvas.set_scrolling(fit.scroll);
  }
  gui::CanvasFrameOptions options;
  options.canvas_size = content;
  options.viewport_size = viewport;
  options.grid_step = static_cast<float>(grid_step);
  // Room-local grids below avoid drawing lines through titles/inter-room gaps.
  options.draw_grid = false;
  options.draw_context_menu = false;
  options.draw_overlay = false;
  options.render_popups = false;
  const float scale_before_begin = preview_canvas.global_scale();
  const auto scroll_before_begin = preview_canvas.scrolling();
  auto runtime = gui::BeginCanvas(preview_canvas, options);
  if (preview_canvas.global_scale() != scale_before_begin) {
    // Touch zoom in Canvas::Begin anchors at its viewport origin. The preview
    // keeps a screen-height title above the room, so anchor the room pixels
    // below that band using the same transform as desktop wheel zoom.
    gui::CanvasGeometry geometry;
    geometry.canvas_p0 =
        ImVec2(runtime.canvas_p0.x, runtime.canvas_p0.y + header_height);
    geometry.scrolling = scroll_before_begin;
    preview_canvas.set_scrolling(gui::ComputeScrollForZoomAtScreenPos(
        geometry, scale_before_begin, preview_canvas.global_scale(),
        ImGui::GetMousePos()));
  }
  // BeginCanvas consumes touch wheel gestures itself. Preserve those changes
  // instead of restoring automatic Fit on the following frame.
  if (preview_canvas.global_scale() != scale_before_begin ||
      preview_canvas.scrolling().x != scroll_before_begin.x ||
      preview_canvas.scrolling().y != scroll_before_begin.y)
    auto_fit = false;
  auto& io = ImGui::GetIO();
  if (runtime.hovered) {
    if (io.MouseWheel != 0.0f) {
      gui::CanvasGeometry geometry;
      geometry.canvas_p0 =
          ImVec2(runtime.canvas_p0.x, runtime.canvas_p0.y + header_height);
      geometry.scrolling = runtime.scrolling;
      const float old_scale = preview_canvas.global_scale();
      const float new_scale =
          std::clamp(old_scale * std::pow(1.12f, io.MouseWheel), 0.25f, 4.0f);
      preview_canvas.set_scrolling(gui::ComputeScrollForZoomAtScreenPos(
          geometry, old_scale, new_scale, io.MousePos));
      preview_canvas.set_global_scale(new_scale);
      io.MouseWheel = 0;
      auto_fit = false;
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
      const auto scroll = preview_canvas.scrolling();
      preview_canvas.set_scrolling(
          ImVec2(scroll.x + io.MouseDelta.x, scroll.y + io.MouseDelta.y));
      auto_fit = false;
    }
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
      auto_fit = false;  // Canvas::Begin already applies the maintained drag.
  }
  const float scale = preview_canvas.global_scale();
  const ImVec2 scroll = preview_canvas.scrolling();
  const ImVec2 origin(runtime.canvas_p0.x + scroll.x,
                      runtime.canvas_p0.y + scroll.y);
  const bool surface_hovered = runtime.hovered;
  ImDrawList* draw_list = runtime.draw_list;
  draw_list->PushClipRect(runtime.canvas_p0,
                          ImVec2(runtime.canvas_p0.x + viewport.x,
                                 runtime.canvas_p0.y + viewport.y),
                          true);
  const ImVec2 mouse = ImGui::GetMousePos();
  const float room_size = kDungeonRoomPixelSize * scale;

  std::vector<std::string> hover_lines;
  for (size_t i = 0; i < overlay.rooms.size(); ++i) {
    const auto& room = overlay.rooms[i];
    const float x =
        origin.x +
        ProposalRoomOriginX(static_cast<int>(i), kProposalRoomGap) * scale;
    const ImVec2 room_min(x, origin.y + header_height);
    const ImVec2 room_max(room_min.x + room_size, room_min.y + room_size);

    const std::string title =
        room.label.empty()
            ? absl::StrFormat("Room $%02X", room.room_id)
            : absl::StrFormat("Room $%02X - %s", room.room_id, room.label);
    const ImVec2 title_min(x, origin.y + 2);
    const ImVec2 title_max(x + room_size, origin.y + header_height);
    const ImVec2 title_size = ImGui::CalcTextSize(title.c_str());
    draw_list->PushClipRect(ImVec2(x, origin.y), title_max, true);
    ImGui::RenderTextEllipsis(draw_list, title_min, title_max, title_max.x,
                              title.c_str(), nullptr, &title_size);
    draw_list->PopClipRect();
    if (surface_hovered && mouse.x >= x && mouse.x < title_max.x &&
        mouse.y >= origin.y && mouse.y < title_max.y)
      hover_lines.push_back(title);

    if (bitmaps[i] && bitmaps[i]->texture()) {
      draw_list->AddImage((ImTextureID)(intptr_t)bitmaps[i]->texture(),
                          room_min, room_max, ImVec2(0, 0), ImVec2(1, 1),
                          ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
    } else {
      draw_list->AddRectFilled(room_min, room_max, IM_COL32(40, 40, 48, 255));
      draw_list->AddText(ImVec2(room_min.x + 8, room_min.y + 8),
                         IM_COL32(200, 200, 200, 255), tr("Rendering room..."));
    }
    draw_list->AddRect(room_min, room_max, IM_COL32(255, 255, 255, 90));

    const bool mouse_in_room = surface_hovered && mouse.x >= room_min.x &&
                               mouse.x < room_max.x && mouse.y >= room_min.y &&
                               mouse.y < room_max.y;
    const float local_x = (mouse.x - room_min.x) / scale;
    const float local_y = (mouse.y - room_min.y) / scale;

    if (mouse_in_room) {
      hover_lines.insert(
          hover_lines.begin(),
          absl::StrFormat(
              "Room $%03X  Tile8 (%d, %d)  pixel (%d, %d)", room.room_id,
              static_cast<int>(local_x) / 8, static_cast<int>(local_y) / 8,
              static_cast<int>(local_x), static_cast<int>(local_y)));
    }
    if (show_grid) {
      draw_list->PushClipRect(room_min, room_max, true);
      gui::CanvasUtils::DrawCanvasGridLines(
          draw_list, room_min, room_max, ImVec2(0, 0),
          static_cast<float>(grid_step), scale);
      draw_list->PopClipRect();
    }
    if (!show_overlay) {
      continue;
    }
    draw_list->PushClipRect(room_min, room_max, true);
    for (size_t li = 0; li < overlay.layers.size(); ++li) {
      if (li < layer_visible.size() && !layer_visible[li]) {
        continue;
      }
      const auto& layer = overlay.layers[li];
      if (!ProposalLayerInStep(overlay, layer.id, selected_step))
        continue;
      for (const auto& shape : layer.shapes) {
        if (shape.room_id != room.room_id) {
          continue;
        }
        DrawProposalShape(draw_list, shape, layer, room_min, scale);
        if (mouse_in_room && ProposalShapeHitTest(shape, local_x, local_y,
                                                  6.0f / scale + 2.0f)) {
          std::string line = layer.label;
          if (!shape.label.empty()) {
            line += ": " + shape.label;
          }
          if (!shape.detail.empty()) {
            line += " - " + shape.detail;
          }
          if (shape.type == ProposalShapeType::kActor) {
            const auto r = ProposalFootprintToPixels(shape);
            line +=
                absl::StrFormat(" (actor anchor %d,%d px; preview only)",
                                static_cast<int>(r.x0), static_cast<int>(r.y0));
          } else if (shape.type == ProposalShapeType::kObject) {
            line += absl::StrFormat(
                " ($%03X, size %d, list %d; suggested footprint only)",
                shape.object_id, shape.object_size, shape.object_list);
          }
          hover_lines.push_back(std::move(line));
        }
      }
    }
    draw_list->PopClipRect();
  }
  draw_list->PopClipRect();
  gui::EndCanvas(preview_canvas, runtime, options);

  if (!hover_lines.empty()) {
    const float wrap_width = std::max(
        1.0f,
        std::min(360.0f, ImGui::GetWindowViewport()->WorkSize.x -
                             2.0f * ImGui::GetStyle().WindowPadding.x - 16.0f));
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap_width);
    for (const auto& line : hover_lines) {
      ImGui::TextUnformatted(line.c_str());
    }
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
  }
}

}  // namespace yaze::editor
