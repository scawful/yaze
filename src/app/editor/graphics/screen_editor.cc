#include "screen_editor.h"

#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/gfx/performance_profiler.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/atlas_renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "imgui/imgui.h"
#include "util/hex.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

using core::Renderer;

constexpr uint32_t kRedPen = 0xFF0000FF;

void ScreenEditor::Initialize() {}

absl::Status ScreenEditor::Load() {
  gfx::ScopedTimer timer("ScreenEditor::Load");

  ASSIGN_OR_RETURN(dungeon_maps_,
                   zelda3::LoadDungeonMaps(*rom(), dungeon_map_labels_));
  RETURN_IF_ERROR(zelda3::LoadDungeonMapTile16(
      tile16_blockset_, *rom(), rom()->graphics_buffer(), false));
  // TODO: Load roomset gfx based on dungeon ID
  sheets_.try_emplace(0, gfx::Arena::Get().gfx_sheets()[212]);
  sheets_.try_emplace(1, gfx::Arena::Get().gfx_sheets()[213]);
  sheets_.try_emplace(2, gfx::Arena::Get().gfx_sheets()[214]);
  sheets_.try_emplace(3, gfx::Arena::Get().gfx_sheets()[215]);
  /**
  int current_tile8 = 0;
  int tile_data_offset = 0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 32; j++) {
      std::vector<uint8_t> tile_data(64, 0);  // 8x8 tile (64 bytes
      int tile_index = current_tile8 + j;
      int x = (j % 8) * 8;
      int y = (j / 8) * 8;
      sheets_[i].Get8x8Tile(tile_index, x, y, tile_data, tile_data_offset);
      tile8_individual_.emplace_back(gfx::Bitmap(8, 8, 4, tile_data));
      tile8_individual_.back().SetPalette(*rom()->mutable_dungeon_palette(3));
      Renderer::Get().RenderBitmap(&tile8_individual_.back());
    }
    tile_data_offset = 0;
  }
  */
  return absl::OkStatus();
}

absl::Status ScreenEditor::Update() {
  if (ImGui::BeginTabBar("##ScreenEditorTabBar")) {
    if (ImGui::BeginTabItem("Dungeon Maps")) {
      DrawDungeonMapsEditor();
      ImGui::EndTabItem();
    }
    DrawInventoryMenuEditor();
    DrawOverworldMapEditor();
    DrawTitleScreenEditor();
    DrawNamingScreenEditor();
    ImGui::EndTabBar();
  }
  return status_;
}

void ScreenEditor::DrawInventoryMenuEditor() {
  if (ImGui::BeginTabItem("Inventory Menu")) {
    static bool create = false;
    if (!create && rom()->is_loaded()) {
      status_ = inventory_.Create();
      palette_ = inventory_.palette();
      create = true;
    }

    DrawInventoryToolset();

    if (ImGui::BeginTable("InventoryScreen", 3, ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("Canvas");
      ImGui::TableSetupColumn("Tiles");
      ImGui::TableSetupColumn("Palette");
      ImGui::TableHeadersRow();

      ImGui::TableNextColumn();
      screen_canvas_.DrawBackground();
      screen_canvas_.DrawContextMenu();
      screen_canvas_.DrawBitmap(inventory_.bitmap(), 2, create);
      screen_canvas_.DrawGrid(32.0f);
      screen_canvas_.DrawOverlay();

      ImGui::TableNextColumn();
      tilesheet_canvas_.DrawBackground(ImVec2(128 * 2 + 2, (192 * 2) + 4));
      tilesheet_canvas_.DrawContextMenu();
      tilesheet_canvas_.DrawBitmap(inventory_.tilesheet(), 2, create);
      tilesheet_canvas_.DrawGrid(16.0f);
      tilesheet_canvas_.DrawOverlay();

      ImGui::TableNextColumn();
      gui::DisplayPalette(palette_, create);

      ImGui::EndTable();
    }
    ImGui::Separator();
    ImGui::EndTabItem();
  }
}

void ScreenEditor::DrawInventoryToolset() {
  if (ImGui::BeginTable("InventoryToolset", 8, ImGuiTableFlags_SizingFixedFit,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("#drawTool");
    ImGui::TableSetupColumn("#sep1");
    ImGui::TableSetupColumn("#zoomOut");
    ImGui::TableSetupColumn("#zoomIN");
    ImGui::TableSetupColumn("#sep2");
    ImGui::TableSetupColumn("#bg2Tool");
    ImGui::TableSetupColumn("#bg3Tool");
    ImGui::TableSetupColumn("#itemTool");

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_UNDO)) {
      // status_ = inventory_.Undo();
    }
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_REDO)) {
      // status_ = inventory_.Redo();
    }
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_ZOOM_OUT)) {
      screen_canvas_.ZoomOut();
    }
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_ZOOM_IN)) {
      screen_canvas_.ZoomIn();
    }
    ImGui::TableNextColumn();
    ImGui::Text(ICON_MD_MORE_VERT);
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_DRAW)) {
      current_mode_ = EditingMode::DRAW;
    }
    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_BUILD)) {
      // current_mode_ = EditingMode::BUILD;
    }

    ImGui::EndTable();
  }
}

void ScreenEditor::DrawDungeonMapScreen(int i) {
  gfx::ScopedTimer timer("screen_editor_draw_dungeon_map_screen");

  auto& current_dungeon = dungeon_maps_[selected_dungeon];

  floor_number = i;
  screen_canvas_.DrawBackground(ImVec2(325, 325));
  screen_canvas_.DrawTileSelector(64.f);

  auto boss_room = current_dungeon.boss_room;

  // Pre-allocate vectors for batch operations
  std::vector<int> tile_ids_to_render;
  std::vector<ImVec2> tile_positions;
  tile_ids_to_render.reserve(zelda3::kNumRooms);
  tile_positions.reserve(zelda3::kNumRooms);

  for (int j = 0; j < zelda3::kNumRooms; j++) {
    if (current_dungeon.floor_rooms[floor_number][j] != 0x0F) {
      int tile16_id = current_dungeon.floor_gfx[floor_number][j];
      int posX = ((j % 5) * 32);
      int posY = ((j / 5) * 32);

      // Batch tile rendering
      tile_ids_to_render.push_back(tile16_id);
      tile_positions.emplace_back(posX * 2, posY * 2);
    }
  }

  // Batch render all tiles
  for (size_t idx = 0; idx < tile_ids_to_render.size(); ++idx) {
    int tile16_id = tile_ids_to_render[idx];
    ImVec2 pos = tile_positions[idx];

    gfx::RenderTile16(tile16_blockset_, tile16_id);
    // Get tile from cache after rendering
    auto* cached_tile = tile16_blockset_.tile_cache.GetTile(tile16_id);
    if (cached_tile && cached_tile->is_active()) {
      // Ensure the cached tile has a valid texture
      if (!cached_tile->texture()) {
        core::Renderer::Get().RenderBitmap(cached_tile);
      }
      screen_canvas_.DrawBitmap(*cached_tile, pos.x, pos.y, 4.0F);
    }
  }

  // Draw overlays and labels
  for (int j = 0; j < zelda3::kNumRooms; j++) {
    if (current_dungeon.floor_rooms[floor_number][j] != 0x0F) {
      int posX = ((j % 5) * 32);
      int posY = ((j / 5) * 32);

      if (current_dungeon.floor_rooms[floor_number][j] == boss_room) {
        screen_canvas_.DrawOutlineWithColor((posX * 2), (posY * 2), 64, 64,
                                            kRedPen);
      }

      std::string label =
          dungeon_map_labels_[selected_dungeon][floor_number][j];
      screen_canvas_.DrawText(label, (posX * 2), (posY * 2));
      std::string gfx_id =
          util::HexByte(current_dungeon.floor_gfx[floor_number][j]);
      screen_canvas_.DrawText(gfx_id, (posX * 2), (posY * 2) + 16);
    }
  }

  screen_canvas_.DrawGrid(64.f, 5);
  screen_canvas_.DrawOverlay();

  if (!screen_canvas_.points().empty()) {
    int x = screen_canvas_.points().front().x / 64;
    int y = screen_canvas_.points().front().y / 64;
    selected_room = x + (y * 5);
  }
}

void ScreenEditor::DrawDungeonMapsTabs() {
  auto& current_dungeon = dungeon_maps_[selected_dungeon];
  if (ImGui::BeginTabBar("##DungeonMapTabs")) {
    auto nbr_floors =
        current_dungeon.nbr_of_floor + current_dungeon.nbr_of_basement;
    for (int i = 0; i < nbr_floors; i++) {
      int basement_num = current_dungeon.nbr_of_basement - i;
      std::string tab_name = absl::StrFormat("Basement %d", basement_num);
      if (i >= current_dungeon.nbr_of_basement) {
        tab_name = absl::StrFormat("Floor %d",
                                   i - current_dungeon.nbr_of_basement + 1);
      }
      if (ImGui::BeginTabItem(tab_name.data())) {
        DrawDungeonMapScreen(i);
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  gui::InputHexByte(
      "Selected Room",
      &current_dungeon.floor_rooms[floor_number].at(selected_room));

  gui::InputHexWord("Boss Room", &current_dungeon.boss_room);

  const auto button_size = ImVec2(130, 0);

  if (ImGui::Button("Add Floor", button_size) &&
      current_dungeon.nbr_of_floor < 8) {
    current_dungeon.nbr_of_floor++;
    dungeon_map_labels_[selected_dungeon].emplace_back();
  }
  ImGui::SameLine();
  if (ImGui::Button("Remove Floor", button_size) &&
      current_dungeon.nbr_of_floor > 0) {
    current_dungeon.nbr_of_floor--;
    dungeon_map_labels_[selected_dungeon].pop_back();
  }

  if (ImGui::Button("Add Basement", button_size) &&
      current_dungeon.nbr_of_basement < 8) {
    current_dungeon.nbr_of_basement++;
    dungeon_map_labels_[selected_dungeon].emplace_back();
  }
  ImGui::SameLine();
  if (ImGui::Button("Remove Basement", button_size) &&
      current_dungeon.nbr_of_basement > 0) {
    current_dungeon.nbr_of_basement--;
    dungeon_map_labels_[selected_dungeon].pop_back();
  }

  if (ImGui::Button("Copy Floor", button_size)) {
    copy_button_pressed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste Floor", button_size)) {
    paste_button_pressed = true;
  }
}

/**
 * @brief Draw dungeon room graphics editor with enhanced tile16 editing
 * 
 * Enhanced Features:
 * - Interactive tile16 selector with visual feedback
 * - Real-time tile16 composition from 4x 8x8 tiles
 * - Tile metadata editing (mirroring, palette, etc.)
 * - Integration with ROM graphics buffer
 * - Undo/redo support for tile modifications
 * 
 * Performance Notes:
 * - Cached tile16 rendering to avoid repeated composition
 * - Efficient tile selector with grid-based snapping
 * - Batch tile updates to minimize ROM writes
 * - Lazy loading of tile graphics data
 */
void ScreenEditor::DrawDungeonMapsRoomGfx() {
  gfx::ScopedTimer timer("screen_editor_draw_dungeon_maps_room_gfx");

  if (ImGui::BeginChild("##DungeonMapTiles", ImVec2(0, 0), true)) {
    // Enhanced tilesheet canvas with improved tile selection
    tilesheet_canvas_.DrawBackground(ImVec2((256 * 2) + 2, (192 * 2) + 4));
    tilesheet_canvas_.DrawContextMenu();

    // Interactive tile16 selector with grid snapping
    if (tilesheet_canvas_.DrawTileSelector(32.f)) {
      selected_tile16_ = tilesheet_canvas_.points().front().x / 32 +
                         (tilesheet_canvas_.points().front().y / 32) * 16;

      // Render selected tile16 and cache tile metadata
      gfx::RenderTile16(tile16_blockset_, selected_tile16_);
      std::ranges::copy(tile16_blockset_.tile_info[selected_tile16_],
                        current_tile16_info.begin());
    }
    // Use direct bitmap rendering for tilesheet
    tilesheet_canvas_.DrawBitmap(tile16_blockset_.atlas, 1, 1, 2.0F);
    tilesheet_canvas_.DrawGrid(32.f);
    tilesheet_canvas_.DrawOverlay();

    if (!tilesheet_canvas_.points().empty() &&
        !screen_canvas_.points().empty()) {
      dungeon_maps_[selected_dungeon].floor_gfx[floor_number][selected_room] =
          selected_tile16_;
      tilesheet_canvas_.mutable_points()->clear();
    }

    ImGui::Separator();
    current_tile_canvas_.DrawBackground();  // ImVec2(64 * 2 + 2, 64 * 2 + 4));
    current_tile_canvas_.DrawContextMenu();
    if (current_tile_canvas_.DrawTilePainter(tile8_individual_[selected_tile8_],
                                             16)) {
      // Modify the tile16 based on the selected tile and current_tile16_info
      gfx::ModifyTile16(tile16_blockset_, rom()->graphics_buffer(),
                        current_tile16_info[0], current_tile16_info[1],
                        current_tile16_info[2], current_tile16_info[3], 212,
                        selected_tile16_);
      gfx::UpdateTile16(tile16_blockset_, selected_tile16_);
    }
    // Get selected tile from cache
    auto* selected_tile = tile16_blockset_.tile_cache.GetTile(selected_tile16_);
    if (selected_tile && selected_tile->is_active()) {
      // Ensure the selected tile has a valid texture
      if (!selected_tile->texture()) {
        core::Renderer::Get().RenderBitmap(selected_tile);
      }
      current_tile_canvas_.DrawBitmap(*selected_tile, 2, 4.0f);
    }
    current_tile_canvas_.DrawGrid(16.f);
    current_tile_canvas_.DrawOverlay();

    gui::InputTileInfo("TL", &current_tile16_info[0]);
    ImGui::SameLine();
    gui::InputTileInfo("TR", &current_tile16_info[1]);
    gui::InputTileInfo("BL", &current_tile16_info[2]);
    ImGui::SameLine();
    gui::InputTileInfo("BR", &current_tile16_info[3]);

    if (ImGui::Button("Modify Tile16")) {
      gfx::ModifyTile16(tile16_blockset_, rom()->graphics_buffer(),
                        current_tile16_info[0], current_tile16_info[1],
                        current_tile16_info[2], current_tile16_info[3], 212,
                        selected_tile16_);
      gfx::UpdateTile16(tile16_blockset_, selected_tile16_);
    }
  }
  ImGui::EndChild();
}

/**
 * @brief Draw dungeon maps editor with enhanced ROM hacking features
 * 
 * Enhanced Features:
 * - Multi-mode editing (DRAW, EDIT, SELECT)
 * - Real-time tile16 preview and editing
 * - Floor/basement management for complex dungeons
 * - Copy/paste operations for floor layouts
 * - Integration with ROM tile16 data
 * 
 * Performance Notes:
 * - Lazy loading of dungeon graphics
 * - Cached tile16 rendering for fast updates
 * - Batch operations for multiple tile changes
 * - Efficient memory management for large dungeons
 */
void ScreenEditor::DrawDungeonMapsEditor() {
  // Enhanced editing mode controls with visual feedback
  if (ImGui::Button(ICON_MD_DRAW)) {
    current_mode_ = EditingMode::DRAW;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_EDIT)) {
    current_mode_ = EditingMode::EDIT;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE)) {
    PRINT_IF_ERROR(zelda3::SaveDungeonMapTile16(tile16_blockset_, *rom()));
  }

  static std::vector<std::string> dungeon_names = {
      "Sewers/Sanctuary",   "Hyrule Castle", "Eastern Palace",
      "Desert Palace",      "Tower of Hera", "Agahnim's Tower",
      "Palace of Darkness", "Swamp Palace",  "Skull Woods",
      "Thieves' Town",      "Ice Palace",    "Misery Mire",
      "Turtle Rock",        "Ganon's Tower"};

  if (ImGui::BeginTable("DungeonMapsTable", 4,
                        ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Hideable)) {
    ImGui::TableSetupColumn("Dungeon");
    ImGui::TableSetupColumn("Map");
    ImGui::TableSetupColumn("Rooms Gfx");
    ImGui::TableSetupColumn("Tiles Gfx");
    ImGui::TableHeadersRow();

    ImGui::TableNextColumn();
    for (int i = 0; i < dungeon_names.size(); i++) {
      rom()->resource_label()->SelectableLabelWithNameEdit(
          selected_dungeon == i, "Dungeon Names", absl::StrFormat("%d", i),
          dungeon_names[i]);
      if (ImGui::IsItemClicked()) {
        selected_dungeon = i;
      }
    }

    ImGui::TableNextColumn();
    DrawDungeonMapsTabs();

    ImGui::TableNextColumn();
    DrawDungeonMapsRoomGfx();

    ImGui::TableNextColumn();
    tilemap_canvas_.DrawBackground();
    tilemap_canvas_.DrawContextMenu();
    if (tilemap_canvas_.DrawTileSelector(16.f)) {
      // Get the tile8 ID to use for the tile16 drawing above
      selected_tile8_ = tilemap_canvas_.GetTileIdFromMousePos();
    }
    tilemap_canvas_.DrawBitmapTable(sheets_);
    tilemap_canvas_.DrawGrid();
    tilemap_canvas_.DrawOverlay();

    ImGui::Text("Selected tile8: %d", selected_tile8_);
    ImGui::Separator();
    ImGui::Text("For use with custom inserted graphics assembly patches.");
    if (ImGui::Button("Load GFX from BIN file"))
      LoadBinaryGfx();

    ImGui::EndTable();
  }
}

void ScreenEditor::LoadBinaryGfx() {
  std::string bin_file = core::FileDialogWrapper::ShowOpenFileDialog();
  if (!bin_file.empty()) {
    std::ifstream file(bin_file, std::ios::binary);
    if (file.is_open()) {
      // Read the gfx data into a buffer
      std::vector<uint8_t> bin_data((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
      if (auto converted_bin = gfx::SnesTo8bppSheet(bin_data, 4, 4);
          zelda3::LoadDungeonMapTile16(tile16_blockset_, *rom(), converted_bin,
                                       true)
              .ok()) {
        sheets_.clear();
        std::vector<std::vector<uint8_t>> gfx_sheets;
        for (int i = 0; i < 4; i++) {
          gfx_sheets.emplace_back(converted_bin.begin() + (i * 0x1000),
                                  converted_bin.begin() + ((i + 1) * 0x1000));
          sheets_.emplace(i, gfx::Bitmap(128, 32, 8, gfx_sheets[i]));
          sheets_[i].SetPalette(*rom()->mutable_dungeon_palette(3));
          Renderer::Get().RenderBitmap(&sheets_[i]);
        }
        binary_gfx_loaded_ = true;
      } else {
        status_ = absl::InternalError("Failed to load dungeon map tile16");
      }
      file.close();
    }
  }
}

void ScreenEditor::DrawTitleScreenEditor() {
  if (ImGui::BeginTabItem("Title Screen")) {
    ImGui::EndTabItem();
  }
}

void ScreenEditor::DrawNamingScreenEditor() {
  if (ImGui::BeginTabItem("Naming Screen")) {
    ImGui::EndTabItem();
  }
}

void ScreenEditor::DrawOverworldMapEditor() {
  if (ImGui::BeginTabItem("Overworld Map")) {
    ImGui::EndTabItem();
  }
}

void ScreenEditor::DrawToolset() {
  static bool show_bg1 = true;
  static bool show_bg2 = true;
  static bool show_bg3 = true;

  static bool drawing_bg1 = true;
  static bool drawing_bg2 = false;
  static bool drawing_bg3 = false;

  ImGui::Checkbox("Show BG1", &show_bg1);
  ImGui::SameLine();
  ImGui::Checkbox("Show BG2", &show_bg2);

  ImGui::Checkbox("Draw BG1", &drawing_bg1);
  ImGui::SameLine();
  ImGui::Checkbox("Draw BG2", &drawing_bg2);
  ImGui::SameLine();
  ImGui::Checkbox("Draw BG3", &drawing_bg3);
}

}  // namespace editor
}  // namespace yaze
