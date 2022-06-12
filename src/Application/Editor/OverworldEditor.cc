#include "OverworldEditor.h"
#include "Core/Icons.h"
#include "Graphics/Bitmap.h"
#include "Graphics/Tile.h"
#include "imgui.h"
#include <cmath>

namespace yaze {
namespace Application {
namespace Editor {
void OverworldEditor::Update() {

  if (rom_.isLoaded()) {
    if (!doneLoaded) {
      overworld.Load(rom_);
      Graphics::CreateAllGfxData(rom_.GetRawData(), allGfx16Ptr);
      doneLoaded = true;
    }
  }

  if (show_changelist_) {
    DrawChangelist();
  }

  DrawToolset();
  ImGui::Separator();
  if (ImGui::BeginTable("#owEditTable", 2, ow_edit_flags, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#overworldCanvas");
    ImGui::TableSetupColumn("#tileSelector");
    ImGui::TableNextColumn();
    DrawOverworldCanvas();
    ImGui::TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
}

void OverworldEditor::DrawToolset() {
  if (ImGui::BeginTable("Toolset", 14, toolset_table_flags, ImVec2(0, 0))) {

    ImGui::TableSetupColumn("#undoTool");
    ImGui::TableSetupColumn("#redoTool");
    ImGui::TableSetupColumn("#drawTool");
    ImGui::TableSetupColumn("#separator2");
    ImGui::TableSetupColumn("#zoomOutTool");
    ImGui::TableSetupColumn("#zoomInTool");
    ImGui::TableSetupColumn("#separator");
    ImGui::TableSetupColumn("#history");
    ImGui::TableSetupColumn("#entranceTool");
    ImGui::TableSetupColumn("#exitTool");
    ImGui::TableSetupColumn("#itemTool");
    ImGui::TableSetupColumn("#spriteTool");
    ImGui::TableSetupColumn("#transportTool");
    ImGui::TableSetupColumn("#musicTool");

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_UNDO);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_REDO);

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_MANAGE_HISTORY)) {
      if (!show_changelist_)
        show_changelist_ = true;
      else
        show_changelist_ = false;
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ZOOM_OUT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ZOOM_IN);

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DRAW);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DOOR_FRONT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_DOOR_BACK);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_GRASS);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_PEST_CONTROL_RODENT);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_ADD_LOCATION);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_MUSIC_NOTE);

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldMapSettings() {
  if (ImGui::BeginTable("#mapSettings", 7, ow_map_settings_flags, ImVec2(0, 0),
                        -1)) {

    ImGui::TableSetupColumn("##1stCol");
    ImGui::TableSetupColumn("##gfxCol");
    ImGui::TableSetupColumn("##palCol");
    ImGui::TableSetupColumn("##sprgfxCol");
    ImGui::TableSetupColumn("##sprpalCol");
    ImGui::TableSetupColumn("##msgidCol");
    ImGui::TableSetupColumn("##2ndCol");

    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##world", &current_world_,
                 "Light World\0Dark World\0Extra World\0");

    ImGui::TableNextColumn();
    ImGui::Text("GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapGFX", map_gfx_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapPal", map_palette_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Spr GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprGFX", spr_gfx_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Spr Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprPal", spr_palette_, kByteSize);

    ImGui::TableNextColumn();
    ImGui::Text("Msg ID");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.f);
    ImGui::InputText("##msgid", spr_palette_, kMessageIdSize);

    ImGui::TableNextColumn();
    ImGui::Checkbox("Show grid", &opt_enable_grid);
    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  DrawOverworldMapSettings();
  ImGui::Separator();
  static ImVector<ImVec2> points;
  static ImVec2 scrolling(0.0f, 0.0f);
  static bool opt_enable_context_menu = true;
  static bool adding_line = false;
  ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
  ImVec2 canvas_p1 =
      ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

  // Draw border and background color
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
  draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

  // This will catch our interactions
  ImGui::InvisibleButton("canvas", canvas_sz,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight);
  const bool is_hovered = ImGui::IsItemHovered(); // Hovered
  const bool is_active = ImGui::IsItemActive();   // Held
  const ImVec2 origin(canvas_p0.x + scrolling.x,
                      canvas_p0.y + scrolling.y); // Lock scrolled origin
  const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                   io.MousePos.y - origin.y);

  // Add first and second point
  if (is_hovered && !adding_line &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    points.push_back(mouse_pos_in_canvas);
    points.push_back(mouse_pos_in_canvas);
    adding_line = true;
  }
  if (adding_line) {
    points.back() = mouse_pos_in_canvas;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
      adding_line = false;
  }

  // Pan (we use a zero mouse threshold when there's no context menu)
  const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
  if (is_active &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan)) {
    scrolling.x += io.MouseDelta.x;
    scrolling.y += io.MouseDelta.y;
  }

  // Context menu (under default mouse threshold)
  ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
  if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
  if (ImGui::BeginPopup("context")) {
    if (adding_line)
      points.resize(points.size() - 2);
    adding_line = false;
    if (ImGui::MenuItem("Remove one", NULL, false, points.Size > 0)) {
      points.resize(points.size() - 2);
    }
    if (ImGui::MenuItem("Remove all", NULL, false, points.Size > 0)) {
      points.clear();
    }
    ImGui::EndPopup();
  }

  // Draw grid + all lines in the canvas
  draw_list->PushClipRect(canvas_p0, canvas_p1, true);
  if (opt_enable_grid) {
    const float GRID_STEP = 64.0f;
    for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x;
         x += GRID_STEP)
      draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y),
                         ImVec2(canvas_p0.x + x, canvas_p1.y),
                         IM_COL32(200, 200, 200, 40));
    for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y;
         y += GRID_STEP)
      draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y),
                         ImVec2(canvas_p1.x, canvas_p0.y + y),
                         IM_COL32(200, 200, 200, 40));
  }

  for (int n = 0; n < points.Size; n += 2)
    draw_list->AddLine(
        ImVec2(origin.x + points[n].x, origin.y + points[n].y),
        ImVec2(origin.x + points[n + 1].x, origin.y + points[n + 1].y),
        IM_COL32(255, 255, 0, 255), 2.0f);

  draw_list->PopClipRect();
}
void OverworldEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar")) {
    if (ImGui::BeginTabItem("Tile8")) {
      if (rom_.isLoaded()) {
        ImGui::Image((void *)(intptr_t)overworld_texture,
                     ImVec2(overworld.overworldMapBitmap->GetWidth(),
                            overworld.overworldMapBitmap->GetHeight()));
      }

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tile16")) {

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void OverworldEditor::DrawChangelist() {
  if (!ImGui::Begin("Changelist")) {

    ImGui::End();
  }

  ImGui::Text("Test");
  ImGui::End();
}

} // namespace Editor
} // namespace Application
} // namespace yaze