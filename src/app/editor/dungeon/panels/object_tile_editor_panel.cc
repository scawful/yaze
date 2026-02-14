#include "app/editor/dungeon/panels/object_tile_editor_panel.h"

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {

ObjectTileEditorPanel::ObjectTileEditorPanel(gfx::IRenderer* renderer,
                                             Rom* rom)
    : renderer_(renderer), rom_(rom) {
  tile_editor_ = std::make_unique<zelda3::ObjectTileEditor>(rom);
}

void ObjectTileEditorPanel::OpenForObject(
    int16_t object_id, int room_id,
    std::array<zelda3::Room, 0x128>* rooms) {
  current_object_id_ = object_id;
  current_room_id_ = room_id;
  rooms_ = rooms;
  selected_cell_index_ = -1;
  selected_source_tile_ = -1;
  preview_dirty_ = true;
  atlas_dirty_ = true;
  is_open_ = true;

  if (!rooms_ || current_room_id_ < 0 ||
      current_room_id_ >= static_cast<int>(rooms_->size())) {
    return;
  }

  auto& room = (*rooms_)[current_room_id_];
  auto layout_or = tile_editor_->CaptureObjectLayout(
      object_id, room, current_palette_group_);
  if (layout_or.ok()) {
    current_layout_ = std::move(layout_or.value());
  }
}

void ObjectTileEditorPanel::OpenForNewObject(
    int width, int height, const std::string& filename,
    int16_t object_id, int room_id,
    std::array<zelda3::Room, 0x128>* rooms) {
  current_object_id_ = object_id;
  current_room_id_ = room_id;
  rooms_ = rooms;
  selected_cell_index_ = -1;
  selected_source_tile_ = -1;
  preview_dirty_ = true;
  atlas_dirty_ = true;
  is_open_ = true;
  is_new_object_ = true;

  current_layout_ = zelda3::ObjectTileLayout::CreateEmpty(
      width, height, object_id, filename);
}

void ObjectTileEditorPanel::Close() {
  is_open_ = false;
  is_new_object_ = false;
  current_layout_ = {};
  selected_cell_index_ = -1;
  selected_source_tile_ = -1;
}

void ObjectTileEditorPanel::SetCurrentPaletteGroup(
    const gfx::PaletteGroup& group) {
  current_palette_group_ = group;
}

void ObjectTileEditorPanel::Draw(bool* p_open) {
  if (!is_open_ || current_layout_.cells.empty()) return;

  std::string title;
  if (is_new_object_) {
    title = absl::StrFormat(
        ICON_MD_ADD_BOX " New Object (%dx%d) - %s###ObjTileEditor",
        current_layout_.bounds_width, current_layout_.bounds_height,
        current_layout_.custom_filename.c_str());
  } else {
    title = absl::StrFormat(
        ICON_MD_GRID_ON " Object 0x%03X - %s###ObjTileEditor",
        current_object_id_,
        zelda3::GetObjectName(current_object_id_).c_str());
  }

  ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(title.c_str(), &is_open_)) {
    ImGui::End();
    return;
  }

  if (!is_open_) {
    Close();
    ImGui::End();
    return;
  }

  // Two-column layout: tile grid + source sheet
  if (ImGui::BeginTable("##TileEditorLayout", 2,
                        ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Tile Grid", ImGuiTableColumnFlags_WidthFixed,
                            280.0f);
    ImGui::TableSetupColumn("Source Sheet", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawTileGrid();

    ImGui::TableNextColumn();
    DrawSourceSheet();

    ImGui::EndTable();
  }

  ImGui::Separator();
  DrawTileProperties();
  ImGui::Separator();
  DrawActionBar();

  HandleKeyboardShortcuts();

  // Shared tile data confirmation modal
  if (show_shared_confirm_) {
    ImGui::OpenPopup("Shared Tile Data");
    show_shared_confirm_ = false;
  }
  if (ImGui::BeginPopupModal("Shared Tile Data", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("This tile data is shared by %d objects.", shared_object_count_);
    ImGui::Text("Changes will affect all of them.");
    ImGui::Spacing();
    if (ImGui::Button("Apply Anyway", ImVec2(120, 0))) {
      ApplyChanges(/*confirm_shared=*/false);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

void ObjectTileEditorPanel::RenderObjectPreview() {
  if (!rooms_ || current_room_id_ < 0) return;
  auto& room = (*rooms_)[current_room_id_];

  auto status = tile_editor_->RenderLayoutToBitmap(
      current_layout_, object_preview_bmp_,
      room.get_gfx_buffer().data(), current_palette_group_);
  if (status.ok()) {
    object_preview_bmp_.UpdateTexture();
    preview_dirty_ = false;
  }
}

void ObjectTileEditorPanel::RenderTile8Atlas() {
  if (!rooms_ || current_room_id_ < 0) return;
  auto& room = (*rooms_)[current_room_id_];

  auto status = tile_editor_->BuildTile8Atlas(
      tile8_atlas_bmp_, room.get_gfx_buffer().data(),
      current_palette_group_, source_palette_);
  if (status.ok()) {
    tile8_atlas_bmp_.UpdateTexture();
    atlas_dirty_ = false;
  }
}

void ObjectTileEditorPanel::DrawTileGrid() {
  if (preview_dirty_) {
    RenderObjectPreview();
  }

  ImGui::Text("Object Tiles (%dx%d)", current_layout_.bounds_width,
              current_layout_.bounds_height);

  constexpr float kScale = 4.0f;
  float grid_width = current_layout_.bounds_width * 8 * kScale;
  float grid_height = current_layout_.bounds_height * 8 * kScale;

  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size(grid_width, grid_height);

  // Draw the preview bitmap as background
  if (object_preview_bmp_.is_active() && object_preview_bmp_.texture()) {
    ImGui::Image(
        (ImTextureID)(intptr_t)object_preview_bmp_.texture(),
        canvas_size);
  } else {
    ImGui::Dummy(canvas_size);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw 8px grid overlay
  for (int gx = 0; gx <= current_layout_.bounds_width; ++gx) {
    float line_x = canvas_pos.x + gx * 8 * kScale;
    draw_list->AddLine(ImVec2(line_x, canvas_pos.y),
                       ImVec2(line_x, canvas_pos.y + grid_height),
                       IM_COL32(128, 128, 128, 80));
  }
  for (int gy = 0; gy <= current_layout_.bounds_height; ++gy) {
    float line_y = canvas_pos.y + gy * 8 * kScale;
    draw_list->AddLine(ImVec2(canvas_pos.x, line_y),
                       ImVec2(canvas_pos.x + grid_width, line_y),
                       IM_COL32(128, 128, 128, 80));
  }

  // Highlight selected cell
  if (selected_cell_index_ >= 0 &&
      selected_cell_index_ < static_cast<int>(current_layout_.cells.size())) {
    const auto& cell = current_layout_.cells[selected_cell_index_];
    ImVec2 cell_min(canvas_pos.x + cell.rel_x * 8 * kScale,
                    canvas_pos.y + cell.rel_y * 8 * kScale);
    ImVec2 cell_max(cell_min.x + 8 * kScale, cell_min.y + 8 * kScale);
    draw_list->AddRect(cell_min, cell_max, IM_COL32(255, 255, 0, 255), 0, 0,
                       2.0f);
  }

  // Handle clicks on the grid
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
    ImVec2 mouse = ImGui::GetMousePos();
    int click_tile_x = static_cast<int>((mouse.x - canvas_pos.x) / (8 * kScale));
    int click_tile_y = static_cast<int>((mouse.y - canvas_pos.y) / (8 * kScale));

    // Find the cell at this position
    for (int idx = 0; idx < static_cast<int>(current_layout_.cells.size());
         ++idx) {
      if (current_layout_.cells[idx].rel_x == click_tile_x &&
          current_layout_.cells[idx].rel_y == click_tile_y) {
        selected_cell_index_ = idx;
        break;
      }
    }
  }

  // Show cell count
  int modified_count = 0;
  for (const auto& cell : current_layout_.cells) {
    if (cell.modified) ++modified_count;
  }
  ImGui::Text("%zu tiles, %d modified", current_layout_.cells.size(),
              modified_count);
}

void ObjectTileEditorPanel::DrawSourceSheet() {
  if (atlas_dirty_) {
    RenderTile8Atlas();
  }

  ImGui::Text("Source Tiles (Palette %d)", source_palette_);

  // Palette selector
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80);
  if (ImGui::SliderInt("##SrcPal", &source_palette_, 0, 7)) {
    atlas_dirty_ = true;
  }

  constexpr float kAtlasScale = 2.0f;
  float display_width = zelda3::ObjectTileEditor::kAtlasWidthPx * kAtlasScale;
  float display_height =
      zelda3::ObjectTileEditor::kAtlasHeightPx * kAtlasScale;

  ImVec2 atlas_pos = ImGui::GetCursorScreenPos();
  ImVec2 atlas_size(display_width, display_height);

  // Scrollable child for the atlas
  ImGui::BeginChild("##AtlasScroll", ImVec2(display_width + 16, 300), true,
                    ImGuiWindowFlags_HorizontalScrollbar);

  atlas_pos = ImGui::GetCursorScreenPos();

  if (tile8_atlas_bmp_.is_active() && tile8_atlas_bmp_.texture()) {
    ImGui::Image(
        (ImTextureID)(intptr_t)tile8_atlas_bmp_.texture(),
        atlas_size);
  } else {
    ImGui::Dummy(atlas_size);
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw 8px grid
  for (int gx = 0; gx <= zelda3::ObjectTileEditor::kAtlasTilesPerRow; ++gx) {
    float line_x = atlas_pos.x + gx * 8 * kAtlasScale;
    draw_list->AddLine(ImVec2(line_x, atlas_pos.y),
                       ImVec2(line_x, atlas_pos.y + display_height),
                       IM_COL32(64, 64, 64, 60));
  }
  for (int gy = 0; gy <= zelda3::ObjectTileEditor::kAtlasTileRows; ++gy) {
    float line_y = atlas_pos.y + gy * 8 * kAtlasScale;
    draw_list->AddLine(ImVec2(atlas_pos.x, line_y),
                       ImVec2(atlas_pos.x + display_width, line_y),
                       IM_COL32(64, 64, 64, 60));
  }

  // Highlight selected source tile
  if (selected_source_tile_ >= 0) {
    int src_col =
        selected_source_tile_ % zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    int src_row =
        selected_source_tile_ / zelda3::ObjectTileEditor::kAtlasTilesPerRow;
    ImVec2 sel_min(atlas_pos.x + src_col * 8 * kAtlasScale,
                   atlas_pos.y + src_row * 8 * kAtlasScale);
    ImVec2 sel_max(sel_min.x + 8 * kAtlasScale, sel_min.y + 8 * kAtlasScale);
    draw_list->AddRect(sel_min, sel_max, IM_COL32(0, 255, 255, 255), 0, 0,
                       2.0f);
  }

  // Handle clicks on the atlas
  if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
    ImVec2 mouse = ImGui::GetMousePos();
    int click_col =
        static_cast<int>((mouse.x - atlas_pos.x) / (8 * kAtlasScale));
    int click_row =
        static_cast<int>((mouse.y - atlas_pos.y) / (8 * kAtlasScale));

    if (click_col >= 0 &&
        click_col < zelda3::ObjectTileEditor::kAtlasTilesPerRow &&
        click_row >= 0 &&
        click_row < zelda3::ObjectTileEditor::kAtlasTileRows) {
      int tile_id = click_row * zelda3::ObjectTileEditor::kAtlasTilesPerRow +
                    click_col;
      selected_source_tile_ = tile_id;

      // If a cell is selected, replace its tile
      if (selected_cell_index_ >= 0 &&
          selected_cell_index_ <
              static_cast<int>(current_layout_.cells.size())) {
        auto& cell = current_layout_.cells[selected_cell_index_];
        cell.tile_info.id_ = static_cast<uint16_t>(tile_id);
        cell.tile_info.palette_ = static_cast<uint8_t>(source_palette_);
        cell.modified = true;
        preview_dirty_ = true;
      }
    }
  }

  ImGui::EndChild();

  if (selected_source_tile_ >= 0) {
    ImGui::Text("Tile: 0x%03X", selected_source_tile_);
  }
}

void ObjectTileEditorPanel::DrawTileProperties() {
  if (selected_cell_index_ < 0 ||
      selected_cell_index_ >= static_cast<int>(current_layout_.cells.size())) {
    ImGui::TextDisabled("Select a tile cell to edit properties");
    return;
  }

  auto& cell = current_layout_.cells[selected_cell_index_];
  ImGui::Text("Cell (%d, %d)", cell.rel_x, cell.rel_y);
  ImGui::SameLine();

  // Tile ID
  int tile_id = cell.tile_info.id_;
  ImGui::SetNextItemWidth(80);
  if (ImGui::InputInt("ID", &tile_id, 1, 16)) {
    cell.tile_info.id_ = static_cast<uint16_t>(tile_id & 0x3FF);
    cell.modified = true;
    preview_dirty_ = true;
  }
  ImGui::SameLine();

  // Palette
  int pal = cell.tile_info.palette_;
  ImGui::SetNextItemWidth(60);
  if (ImGui::SliderInt("Pal", &pal, 0, 7)) {
    cell.tile_info.palette_ = static_cast<uint8_t>(pal);
    cell.modified = true;
    preview_dirty_ = true;
  }
  ImGui::SameLine();

  // Flip flags
  if (ImGui::Checkbox("H", &cell.tile_info.horizontal_mirror_)) {
    cell.modified = true;
    preview_dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("V", &cell.tile_info.vertical_mirror_)) {
    cell.modified = true;
    preview_dirty_ = true;
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Pri", &cell.tile_info.over_)) {
    cell.modified = true;
    preview_dirty_ = true;
  }
}

void ObjectTileEditorPanel::ApplyChanges(bool confirm_shared) {
  // Check for shared tile data and ask for confirmation
  if (confirm_shared && current_layout_.tile_data_address >= 0 &&
      !current_layout_.is_custom) {
    int shared_count =
        tile_editor_->CountObjectsSharingTileData(current_object_id_);
    if (shared_count > 1) {
      shared_object_count_ = shared_count;
      show_shared_confirm_ = true;
      return;
    }
  }

  auto status = tile_editor_->WriteBack(current_layout_);
  if (status.ok()) {
    // Re-render room after applying changes
    if (rooms_ && current_room_id_ >= 0 &&
        current_room_id_ < static_cast<int>(rooms_->size())) {
      auto& room = (*rooms_)[current_room_id_];
      room.MarkObjectsDirty();
      room.RenderRoomGraphics();
    }
    // Update original words to match current state
    for (auto& cell : current_layout_.cells) {
      if (cell.modified) {
        cell.original_word = gfx::TileInfoToWord(cell.tile_info);
        cell.modified = false;
      }
    }

    // Fire creation callback on first save of a new object
    if (is_new_object_ && on_object_created_) {
      on_object_created_(current_layout_.object_id,
                         current_layout_.custom_filename);
      is_new_object_ = false;
    }
  }
}

void ObjectTileEditorPanel::DrawActionBar() {
  int modified_count = 0;
  for (const auto& cell : current_layout_.cells) {
    if (cell.modified) ++modified_count;
  }

  bool has_mods = modified_count > 0;

  if (has_mods) {
    ImGui::Text("%d tile(s) modified", modified_count);
    ImGui::SameLine();
  }

  // Shared tile data warning
  if (current_layout_.tile_data_address >= 0 && !current_layout_.is_custom) {
    int shared_count =
        tile_editor_->CountObjectsSharingTileData(current_object_id_);
    if (shared_count > 1) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                         ICON_MD_WARNING " Shared by %d objects", shared_count);
      ImGui::SameLine();
    }
  }

  // Apply button
  if (!has_mods) ImGui::BeginDisabled();
  if (ImGui::Button(ICON_MD_SAVE " Apply")) {
    ApplyChanges();
  }
  if (!has_mods) ImGui::EndDisabled();

  ImGui::SameLine();

  // Revert button
  if (!has_mods) ImGui::BeginDisabled();
  if (ImGui::Button(ICON_MD_UNDO " Revert")) {
    current_layout_.RevertAll();
    preview_dirty_ = true;
  }
  if (!has_mods) ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button(ICON_MD_CLOSE " Close")) {
    Close();
  }
}

void ObjectTileEditorPanel::HandleKeyboardShortcuts() {
  // Only handle shortcuts when this window is focused
  if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;
  if (current_layout_.cells.empty()) return;

  int cell_count = static_cast<int>(current_layout_.cells.size());

  // Arrow keys: navigate selected cell by spatial position
  auto find_neighbor = [&](int dx, int dy) -> int {
    if (selected_cell_index_ < 0) return 0;
    const auto& cur = current_layout_.cells[selected_cell_index_];
    int target_x = cur.rel_x + dx;
    int target_y = cur.rel_y + dy;
    for (int i = 0; i < cell_count; ++i) {
      if (current_layout_.cells[i].rel_x == target_x &&
          current_layout_.cells[i].rel_y == target_y) {
        return i;
      }
    }
    return selected_cell_index_;
  };

  if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
    selected_cell_index_ = find_neighbor(-1, 0);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
    selected_cell_index_ = find_neighbor(1, 0);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
    selected_cell_index_ = find_neighbor(0, -1);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
    selected_cell_index_ = find_neighbor(0, 1);
  }

  // Number keys 0-7: set palette on selected cell
  if (selected_cell_index_ >= 0 && selected_cell_index_ < cell_count) {
    auto& cell = current_layout_.cells[selected_cell_index_];
    for (int key = 0; key <= 7; ++key) {
      if (ImGui::IsKeyPressed(
              static_cast<ImGuiKey>(ImGuiKey_0 + key), false)) {
        cell.tile_info.palette_ = static_cast<uint8_t>(key);
        cell.modified = true;
        preview_dirty_ = true;
      }
    }

    // H: toggle horizontal flip
    if (ImGui::IsKeyPressed(ImGuiKey_H, false)) {
      cell.tile_info.horizontal_mirror_ = !cell.tile_info.horizontal_mirror_;
      cell.modified = true;
      preview_dirty_ = true;
    }

    // V: toggle vertical flip
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) {
      cell.tile_info.vertical_mirror_ = !cell.tile_info.vertical_mirror_;
      cell.modified = true;
      preview_dirty_ = true;
    }

    // P: toggle priority
    if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
      cell.tile_info.over_ = !cell.tile_info.over_;
      cell.modified = true;
      preview_dirty_ = true;
    }
  }

  // Escape: deselect or close
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    if (selected_cell_index_ >= 0) {
      selected_cell_index_ = -1;
    } else {
      Close();
    }
  }

  // Tab: cycle to next cell
  if (ImGui::IsKeyPressed(ImGuiKey_Tab, false)) {
    if (cell_count > 0) {
      selected_cell_index_ = (selected_cell_index_ + 1) % cell_count;
    }
  }
}

}  // namespace editor
}  // namespace yaze
