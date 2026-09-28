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
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

using dungeon_ui::kDungeonRoomPixelSize;

constexpr float kProposalRoomGap = 24.0f;
constexpr float kProposalLabelHeight = 22.0f;

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
    case ProposalShapeType::kRect: {
      const auto r = ProposalTileRectToPixels(shape.tiles[0], shape.tiles[1]);
      const ImVec2 p0 = to_screen(r.x0, r.y0);
      const ImVec2 p1 = to_screen(r.x1, r.y1);
      draw_list->AddRectFilled(p0, p1, fill);
      draw_list->AddRect(p0, p1, stroke, 0.0f, 0, 2.0f);
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
                             stroke, ImDrawFlags_None, thickness);
      // Arrowhead on the last segment shows travel direction.
      const ImVec2 a = points[points.size() - 2];
      const ImVec2 b = points.back();
      const float dx = b.x - a.x;
      const float dy = b.y - a.y;
      const float len = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
      const float ux = dx / len;
      const float uy = dy / len;
      const float head = std::max(7.0f, 7.0f * scale);
      draw_list->AddTriangleFilled(b,
                                   ImVec2(b.x - ux * head - uy * head * 0.6f,
                                          b.y - uy * head + ux * head * 0.6f),
                                   ImVec2(b.x - ux * head + uy * head * 0.6f,
                                          b.y - uy * head - ux * head * 0.6f),
                                   stroke);
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
                kProposalLabelHeight + kDungeonRoomPixelSize * scale);
}

void DungeonCanvasViewer::DrawProposalPreview(
    const DungeonProposalOverlay& overlay,
    const std::vector<char>& layer_visible, bool show_overlay, float scale) {
  if (!rom_ || !rom_->is_loaded() || !rooms_) {
    ImGui::TextDisabled("%s", tr("Load a ROM to render the proposal rooms."));
    return;
  }
  if (overlay.rooms.empty()) {
    return;
  }

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

  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 content = GetProposalPreviewContentSize(overlay, scale);
  ImGui::InvisibleButton("##DungeonProposalPreviewSurface", content);
  const bool surface_hovered = ImGui::IsItemHovered();
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 mouse = ImGui::GetMousePos();
  const float room_size = kDungeonRoomPixelSize * scale;

  std::vector<std::string> hover_lines;
  for (size_t i = 0; i < overlay.rooms.size(); ++i) {
    const auto& room = overlay.rooms[i];
    const float x =
        origin.x +
        ProposalRoomOriginX(static_cast<int>(i), kProposalRoomGap) * scale;
    const ImVec2 room_min(x, origin.y + kProposalLabelHeight);
    const ImVec2 room_max(room_min.x + room_size, room_min.y + room_size);

    const std::string title =
        room.label.empty()
            ? absl::StrFormat("Room $%02X", room.room_id)
            : absl::StrFormat("Room $%02X - %s", room.room_id, room.label);
    draw_list->AddText(ImVec2(x, origin.y + 2),
                       ImGui::GetColorU32(ImGuiCol_Text), title.c_str());

    if (bitmaps[i] && bitmaps[i]->texture()) {
      draw_list->AddImage((ImTextureID)(intptr_t)bitmaps[i]->texture(),
                          room_min, room_max);
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

    if (!show_overlay) {
      continue;
    }
    draw_list->PushClipRect(room_min, room_max, true);
    for (size_t li = 0; li < overlay.layers.size(); ++li) {
      if (li < layer_visible.size() && !layer_visible[li]) {
        continue;
      }
      const auto& layer = overlay.layers[li];
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
          hover_lines.push_back(std::move(line));
        }
      }
    }
    draw_list->PopClipRect();

    if (mouse_in_room) {
      hover_lines.insert(
          hover_lines.begin(),
          absl::StrFormat("Room $%02X  tile (%d, %d)", room.room_id,
                          static_cast<int>(local_x) / kProposalTilePixels,
                          static_cast<int>(local_y) / kProposalTilePixels));
    }
  }

  if (!hover_lines.empty()) {
    ImGui::BeginTooltip();
    for (const auto& line : hover_lines) {
      ImGui::TextUnformatted(line.c_str());
    }
    ImGui::EndTooltip();
  }
}

}  // namespace yaze::editor
