#include "overworld_editor.h"

#include <imgui.h>

#include <cmath>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/zelda3/overworld.h"
#include "gui/icons.h"

/**
 * Drawing the Overworld
 * Tips by Zarby
 *
 * 1) Find the graphics pointers (constants.h)
 * 2) Convert the 3bpp SNES data into PC 4bpp (Hard)
 * 3) Get the tiles32 data
 * 4) Get the tiles16 data
 * 5) Get the map32 data using lz2 variant decompression
 * 6) Get the graphics data of the map
 * 7) Load 4bpp into Pseudo VRAM for rendering tiles to screen
 * 8) Render the tiles to a bitmap with a B&W palette to start
 * 9) Get the palette data and find how it's loaded in game
 *
 */
namespace yaze {
namespace app {
namespace editor {

void OverworldEditor::SetupROM(ROM &rom) { rom_ = rom; }

void OverworldEditor::Update() {
  if (rom_.isLoaded() && !all_gfx_loaded_) {
    LoadGraphics();
    all_gfx_loaded_ = true;
  }

  DrawToolset();
  ImGui::Separator();
  if (ImGui::BeginTable("#owEditTable", 2, ow_edit_flags, ImVec2(0, 0))) {
    ImGui::TableSetupColumn(" Canvas", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn(" Tile Selector");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawOverworldCanvas();
    ImGui::TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
}

void OverworldEditor::DrawToolset() {
  if (ImGui::BeginTable("Toolset", 17, toolset_table_flags, ImVec2(0, 0))) {
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
    ImGui::TableSetupColumn("#separator3");
    ImGui::TableSetupColumn("#reloadTool");

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_UNDO);

    ImGui::TableNextColumn();
    ImGui::Button(ICON_MD_REDO);

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

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_UPDATE)) {
      overworld_.Load(rom_, allgfxBitmap.GetData());
    }

    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);

    ImGui::TableNextColumn();
    ImGui::Text("Palette:");
    for (int i = 0; i < 8; i++) {
      std::string id = "##PaletteColor" + std::to_string(i);
      ImGui::SameLine();
      ImGui::ColorEdit4(id.c_str(), &current_palette_[i].x,
                        ImGuiColorEditFlags_NoInputs |
                            ImGuiColorEditFlags_DisplayRGB |
                            ImGuiColorEditFlags_DisplayHex);
    }

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldMapSettings() {
  if (ImGui::BeginTable("#mapSettings", 7, ow_map_flags, ImVec2(0, 0), -1)) {
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
  auto canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

  // Draw border and background color
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
  draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

  // This will catch our interactions
  ImGui::InvisibleButton(
      "canvas", canvas_sz,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool is_hovered = ImGui::IsItemHovered();  // Hovered
  const bool is_active = ImGui::IsItemActive();    // Held
  const ImVec2 origin(canvas_p0.x + scrolling.x,
                      canvas_p0.y + scrolling.y);  // Lock scrolled origin
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
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) adding_line = false;
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
    if (adding_line) points.resize(points.size() - 2);
    adding_line = false;
    if (ImGui::MenuItem("Remove one", nullptr, false, points.Size > 0)) {
      points.resize(points.size() - 2);
    }
    if (ImGui::MenuItem("Remove all", nullptr, false, points.Size > 0)) {
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
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Tile16")) {
      bool child_is_visible =
          ImGui::BeginChild("#Tile16Child", ImGui::GetContentRegionAvail(),
                            true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
      if (child_is_visible) DrawTile16Selector();

      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tile8")) {
      ImGuiID child_id = ImGui::GetID((void *)(intptr_t)1);
      bool child_is_visible =
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar);
      if (child_is_visible) {
        DrawTile8Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void OverworldEditor::DrawTile16Selector() const {
  static ImVec2 scrolling(0.0f, 0.0f);
  ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  auto canvas_sz = ImVec2(256 + 1, kNumSheetsToLoad * 64 + 1);
  auto canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

  // Draw border and background color
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
  draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

  // This will catch our interactions
  ImGui::InvisibleButton(
      "Tile16SelectorCanvas", canvas_sz,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const ImVec2 origin(canvas_p0.x + scrolling.x,
                      canvas_p0.y + scrolling.y);  // Lock scrolled origin
  const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                   io.MousePos.y - origin.y);

  // Context menu (under default mouse threshold)
  ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
  if (drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("contextTile16",
                                ImGuiPopupFlags_MouseButtonRight);
  if (ImGui::BeginPopup("context")) {
    ImGui::EndPopup();
  }

  if (map_blockset_loaded_) {
    draw_list->AddImage(
        (void *)mapblockset16Bitmap.GetTexture(),
        ImVec2(canvas_p0.x + 2, canvas_p0.y + 2),
        ImVec2(canvas_p0.x + (mapblockset16Bitmap.GetWidth() * 2),
               canvas_p0.y + (mapblockset16Bitmap.GetHeight() * 2)));
  }

  // Draw grid + all lines in the canvas
  draw_list->PushClipRect(canvas_p0, canvas_p1, true);
  if (opt_enable_grid) {
    const float GRID_STEP = 32.0f;
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

  draw_list->PopClipRect();
}

void OverworldEditor::DrawTile8Selector() const {
  static ImVec2 scrolling(0.0f, 0.0f);
  ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
  auto canvas_sz = ImVec2(256 + 1, kNumSheetsToLoad * 64 + 1);
  auto canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

  // Draw border and background color
  const ImGuiIO &io = ImGui::GetIO();
  ImDrawList *draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(32, 32, 32, 255));
  draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

  // This will catch our interactions
  ImGui::InvisibleButton(
      "Tile8SelectorCanvas", canvas_sz,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const ImVec2 origin(canvas_p0.x + scrolling.x,
                      canvas_p0.y + scrolling.y);  // Lock scrolled origin
  const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x,
                                   io.MousePos.y - origin.y);

  // Context menu (under default mouse threshold)
  ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
  if (drag_delta.x == 0.0f && drag_delta.y == 0.0f)
    ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
  if (ImGui::BeginPopup("context")) {
    ImGui::EndPopup();
  }

  if (all_gfx_loaded_) {
    for (const auto &[key, value] : all_texture_sheet_) {
      int offset = 64 * (key + 1);
      int top_left_y = canvas_p0.y + 2;
      if (key >= 1) {
        top_left_y = canvas_p0.y + 64 * key;
      }
      draw_list->AddImage((void *)value, ImVec2(canvas_p0.x + 2, top_left_y),
                          ImVec2(canvas_p0.x + 256, canvas_p0.y + offset));
    }
  }

  // Draw grid + all lines in the canvas
  draw_list->PushClipRect(canvas_p0, canvas_p1, true);
  if (opt_enable_grid) {
    const float GRID_STEP = 16.0f;
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

  draw_list->PopClipRect();
}

void OverworldEditor::LoadGraphics() {
  for (int i = 0; i < kNumSheetsToLoad; i++) {
    all_texture_sheet_[i] = rom_.DrawGraphicsSheet(i);
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze