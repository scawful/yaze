#include "screen_editor.h"

#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/constants.h"
#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gfx/tilesheet.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using core::Renderer;

constexpr uint32_t kRedPen = 0xFF0000FF;

absl::Status ScreenEditor::Update() {
  if (ImGui::BeginTabBar("##ScreenEditorTabBar")) {
    if (ImGui::BeginTabItem("Dungeon Maps")) {
      if (rom()->is_loaded()) {
        DrawDungeonMapsEditor();
      }
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
  TAB_ITEM("Inventory Menu")

  static bool create = false;
  if (!create && rom()->is_loaded()) {
    status_ = inventory_.Create();
    palette_ = inventory_.Palette();
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
    screen_canvas_.DrawBitmap(inventory_.Bitmap(), 2, create);
    screen_canvas_.DrawGrid(32.0f);
    screen_canvas_.DrawOverlay();

    ImGui::TableNextColumn();
    tilesheet_canvas_.DrawBackground(ImVec2(128 * 2 + 2, (192 * 2) + 4));
    tilesheet_canvas_.DrawContextMenu();
    tilesheet_canvas_.DrawBitmap(inventory_.Tilesheet(), 2, create);
    tilesheet_canvas_.DrawGrid(16.0f);
    tilesheet_canvas_.DrawOverlay();

    ImGui::TableNextColumn();
    status_ = gui::DisplayPalette(palette_, create);

    ImGui::EndTable();
  }
  ImGui::Separator();
  END_TAB_ITEM()
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

    BUTTON_COLUMN(ICON_MD_UNDO)
    BUTTON_COLUMN(ICON_MD_REDO)
    TEXT_COLUMN(ICON_MD_MORE_VERT)
    BUTTON_COLUMN(ICON_MD_ZOOM_OUT)
    BUTTON_COLUMN(ICON_MD_ZOOM_IN)
    TEXT_COLUMN(ICON_MD_MORE_VERT)
    BUTTON_COLUMN(ICON_MD_DRAW)
    BUTTON_COLUMN(ICON_MD_BUILD)

    ImGui::EndTable();
  }
}

absl::Status ScreenEditor::LoadDungeonMaps() {
  std::vector<std::array<uint8_t, 25>> current_floor_rooms_d;
  std::vector<std::array<uint8_t, 25>> current_floor_gfx_d;
  int total_floors_d;
  uint8_t nbr_floor_d;
  uint8_t nbr_basement_d;

  for (int d = 0; d < 14; d++) {
    current_floor_rooms_d.clear();
    current_floor_gfx_d.clear();
    ASSIGN_OR_RETURN(
        int ptr,
        rom()->ReadWord(zelda3::screen::kDungeonMapRoomsPtr + (d * 2)));
    ASSIGN_OR_RETURN(
        int ptr_gfx,
        rom()->ReadWord(zelda3::screen::kDungeonMapGfxPtr + (d * 2)));
    ptr |= 0x0A0000;                   // Add bank to the short ptr
    ptr_gfx |= 0x0A0000;               // Add bank to the short ptr
    int pc_ptr = core::SnesToPc(ptr);  // Contains data for the next 25 rooms
    int pc_ptr_gfx =
        core::SnesToPc(ptr_gfx);  // Contains data for the next 25 rooms

    ASSIGN_OR_RETURN(
        ushort boss_room_d,
        rom()->ReadWord(zelda3::screen::kDungeonMapBossRooms + (d * 2)));

    ASSIGN_OR_RETURN(
        nbr_basement_d,
        rom()->ReadByte(zelda3::screen::kDungeonMapFloors + (d * 2)));
    nbr_basement_d &= 0x0F;

    ASSIGN_OR_RETURN(
        nbr_floor_d,
        rom()->ReadByte(zelda3::screen::kDungeonMapFloors + (d * 2)));
    nbr_floor_d &= 0xF0;
    nbr_floor_d = nbr_floor_d >> 4;

    total_floors_d = nbr_basement_d + nbr_floor_d;

    dungeon_map_labels_.emplace_back();

    // for each floor in the dungeon
    for (int i = 0; i < total_floors_d; i++) {
      dungeon_map_labels_[d].emplace_back();

      std::array<uint8_t, 25> rdata;
      std::array<uint8_t, 25> gdata;

      // for each room on the floor
      for (int j = 0; j < 25; j++) {
        gdata[j] = 0xFF;
        rdata[j] = rom()->data()[pc_ptr + j + (i * 25)];  // Set the rooms

        if (rdata[j] == 0x0F) {
          gdata[j] = 0xFF;
        } else {
          gdata[j] = rom()->data()[pc_ptr_gfx++];
        }

        std::string label = core::HexByte(rdata[j]);
        dungeon_map_labels_[d][i][j] = label;
      }

      current_floor_gfx_d.push_back(gdata);    // Add new floor gfx data
      current_floor_rooms_d.push_back(rdata);  // Add new floor data
    }

    dungeon_maps_.emplace_back(boss_room_d, nbr_floor_d, nbr_basement_d,
                               current_floor_rooms_d, current_floor_gfx_d);
  }

  return absl::OkStatus();
}

absl::Status ScreenEditor::SaveDungeonMaps() {
  for (int d = 0; d < 14; d++) {
    int ptr = zelda3::screen::kDungeonMapRoomsPtr + (d * 2);
    int ptr_gfx = zelda3::screen::kDungeonMapGfxPtr + (d * 2);
    int pc_ptr = core::SnesToPc(ptr);
    int pc_ptr_gfx = core::SnesToPc(ptr_gfx);

    const int nbr_floors = dungeon_maps_[d].nbr_of_floor;
    const int nbr_basements = dungeon_maps_[d].nbr_of_basement;
    for (int i = 0; i < nbr_floors + nbr_basements; i++) {
      for (int j = 0; j < 25; j++) {
        RETURN_IF_ERROR(rom()->WriteByte(pc_ptr + j + (i * 25),
                                         dungeon_maps_[d].floor_rooms[i][j]));
        RETURN_IF_ERROR(rom()->WriteByte(pc_ptr_gfx + j + (i * 25),
                                         dungeon_maps_[d].floor_gfx[i][j]));
        pc_ptr_gfx++;
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ScreenEditor::LoadDungeonMapTile16(
    const std::vector<uint8_t>& gfx_data, bool bin_mode) {
  tile16_sheet_.Init(256, 192, gfx::TileType::Tile16);

  for (int i = 0; i < 186; i++) {
    int addr = zelda3::screen::kDungeonMapTile16;
    if (rom()->data()[zelda3::screen::kDungeonMapExpCheck] != 0xB9) {
      addr = zelda3::screen::kDungeonMapTile16Expanded;
    }

    ASSIGN_OR_RETURN(auto tl, rom()->ReadWord(addr + (i * 8)));
    gfx::TileInfo t1 = gfx::WordToTileInfo(tl);  // Top left

    ASSIGN_OR_RETURN(auto tr, rom()->ReadWord(addr + 2 + (i * 8)));
    gfx::TileInfo t2 = gfx::WordToTileInfo(tr);  // Top right

    ASSIGN_OR_RETURN(auto bl, rom()->ReadWord(addr + 4 + (i * 8)));
    gfx::TileInfo t3 = gfx::WordToTileInfo(bl);  // Bottom left

    ASSIGN_OR_RETURN(auto br, rom()->ReadWord(addr + 6 + (i * 8)));
    gfx::TileInfo t4 = gfx::WordToTileInfo(br);  // Bottom right

    int sheet_offset = 212;
    if (bin_mode) {
      sheet_offset = 0;
    }
    tile16_sheet_.ComposeTile16(gfx_data, t1, t2, t3, t4, sheet_offset);
  }

  RETURN_IF_ERROR(tile16_sheet_.mutable_bitmap()->ApplyPalette(
      *rom()->mutable_dungeon_palette(3)));
  Renderer::GetInstance().RenderBitmap(&*tile16_sheet_.mutable_bitmap().get());

  for (int i = 0; i < tile16_sheet_.num_tiles(); ++i) {
    auto tile = tile16_sheet_.GetTile16(i);
    tile16_individual_[i] = tile;
    RETURN_IF_ERROR(
        tile16_individual_[i].ApplyPalette(*rom()->mutable_dungeon_palette(3)));
    Renderer::GetInstance().RenderBitmap(&tile16_individual_[i]);
  }

  return absl::OkStatus();
}

absl::Status ScreenEditor::SaveDungeonMapTile16() {
  for (int i = 0; i < 186; i++) {
    int addr = zelda3::screen::kDungeonMapTile16;
    if (rom()->data()[zelda3::screen::kDungeonMapExpCheck] != 0xB9) {
      addr = zelda3::screen::kDungeonMapTile16Expanded;
    }

    gfx::TileInfo t1 = tile16_sheet_.tile_info()[i].tiles[0];
    gfx::TileInfo t2 = tile16_sheet_.tile_info()[i].tiles[1];
    gfx::TileInfo t3 = tile16_sheet_.tile_info()[i].tiles[2];
    gfx::TileInfo t4 = tile16_sheet_.tile_info()[i].tiles[3];

    auto tl = gfx::TileInfoToWord(t1);
    RETURN_IF_ERROR(rom()->WriteWord(addr + (i * 8), tl));

    auto tr = gfx::TileInfoToWord(t2);
    RETURN_IF_ERROR(rom()->WriteWord(addr + 2 + (i * 8), tr));

    auto bl = gfx::TileInfoToWord(t3);
    RETURN_IF_ERROR(rom()->WriteWord(addr + 4 + (i * 8), bl));

    auto br = gfx::TileInfoToWord(t4);
    RETURN_IF_ERROR(rom()->WriteWord(addr + 6 + (i * 8), br));
  }
  return absl::OkStatus();
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

      if (ImGui::BeginTabItem(tab_name.c_str())) {
        floor_number = i;
        screen_canvas_.DrawBackground(ImVec2(325, 325));
        screen_canvas_.DrawTileSelector(64.f);

        auto boss_room = current_dungeon.boss_room;
        for (int j = 0; j < 25; j++) {
          if (current_dungeon.floor_rooms[floor_number][j] != 0x0F) {
            int tile16_id = current_dungeon.floor_gfx[floor_number][j];
            int posX = ((j % 5) * 32);
            int posY = ((j / 5) * 32);

            if (tile16_individual_.count(tile16_id) == 0) {
              tile16_individual_[tile16_id] =
                  tile16_sheet_.GetTile16(tile16_id);
              Renderer::GetInstance().RenderBitmap(
                  &tile16_individual_[tile16_id]);
            }
            screen_canvas_.DrawBitmap(tile16_individual_[tile16_id], (posX * 2),
                                      (posY * 2), 4.0f);

            if (current_dungeon.floor_rooms[floor_number][j] == boss_room) {
              screen_canvas_.DrawOutlineWithColor((posX * 2), (posY * 2), 64,
                                                  64, kRedPen);
            }

            std::string label =
                dungeon_map_labels_[selected_dungeon][floor_number][j];
            screen_canvas_.DrawText(label, (posX * 2), (posY * 2));
            std::string gfx_id = core::HexByte(tile16_id);
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
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  gui::InputHexByte(
      "Selected Room",
      &current_dungeon.floor_rooms[floor_number].at(selected_room));

  gui::InputHexWord("Boss Room", &current_dungeon.boss_room);

  const ImVec2 button_size = ImVec2(130, 0);

  // Add Floor Button
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

  // Add Basement Button
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

void ScreenEditor::DrawDungeonMapsEditor() {
  if (!dungeon_maps_loaded_) {
    if (!LoadDungeonMaps().ok()) {
      ImGui::Text("Failed to load dungeon maps");
    }

    if (LoadDungeonMapTile16(rom()->graphics_buffer()).ok()) {
      // TODO: Load roomset gfx based on dungeon ID
      sheets_.emplace(0, rom()->gfx_sheets()[212]);
      sheets_.emplace(1, rom()->gfx_sheets()[213]);
      sheets_.emplace(2, rom()->gfx_sheets()[214]);
      sheets_.emplace(3, rom()->gfx_sheets()[215]);
      int current_tile8 = 0;
      int tile_data_offset = 0;
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 32; j++) {
          std::vector<uint8_t> tile_data(64, 0);  // 8x8 tile (64 bytes
          int tile_index = current_tile8 + j;
          int x = (j % 8) * 8;
          int y = (j / 8) * 8;
          sheets_[i].Get8x8Tile(tile_index, 0, 0, tile_data, tile_data_offset);
          tile8_individual_.emplace_back(gfx::Bitmap(8, 8, 4, tile_data));
          RETURN_VOID_IF_ERROR(tile8_individual_.back().ApplyPalette(
              *rom()->mutable_dungeon_palette(3)));
          Renderer::GetInstance().RenderBitmap(&tile8_individual_.back());
        }
        tile_data_offset = 0;
      }
      dungeon_maps_loaded_ = true;
    } else {
      ImGui::Text("Failed to load dungeon map tile16");
    }
  }

  if (ImGui::BeginTable("##DungeonMapToolset", 2,
                        ImGuiTableFlags_SizingFixedFit)) {
    ImGui::TableSetupColumn("Draw Mode");
    ImGui::TableSetupColumn("Edit Mode");

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_DRAW)) {
      current_mode_ = EditingMode::DRAW;
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_EDIT)) {
      current_mode_ = EditingMode::EDIT;
    }

    ImGui::EndTable();
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

    // Dungeon column
    ImGui::TableNextColumn();
    for (int i = 0; i < dungeon_names.size(); i++) {
      rom()->resource_label()->SelectableLabelWithNameEdit(
          selected_dungeon == i, "Dungeon Names", absl::StrFormat("%d", i),
          dungeon_names[i]);
      if (ImGui::IsItemClicked()) {
        selected_dungeon = i;
      }
    }

    // Map column
    ImGui::TableNextColumn();
    DrawDungeonMapsTabs();

    ImGui::TableNextColumn();
    if (ImGui::BeginChild("##DungeonMapTiles", ImVec2(0, 0), true)) {
      tilesheet_canvas_.DrawBackground(ImVec2((256 * 2) + 2, (192 * 2) + 4));
      tilesheet_canvas_.DrawContextMenu();
      tilesheet_canvas_.DrawTileSelector(32.f);
      tilesheet_canvas_.DrawBitmap(*tile16_sheet_.bitmap(), 2, true);
      tilesheet_canvas_.DrawGrid(32.f);
      tilesheet_canvas_.DrawOverlay();

      if (!tilesheet_canvas_.points().empty()) {
        selected_tile16_ = tilesheet_canvas_.points().front().x / 32 +
                           (tilesheet_canvas_.points().front().y / 32) * 16;
        current_tile16_info = tile16_sheet_.tile_info().at(selected_tile16_);

        // Draw the selected tile
        if (!screen_canvas_.points().empty()) {
          dungeon_maps_[selected_dungeon]
              .floor_gfx[floor_number][selected_room] = selected_tile16_;
          tilesheet_canvas_.mutable_points()->clear();
        }
      }

      ImGui::Separator();
      current_tile_canvas_
          .DrawBackground();  // ImVec2(64 * 2 + 2, 64 * 2 + 4));
      current_tile_canvas_.DrawContextMenu();
      if (current_tile_canvas_.DrawTilePainter(
              tile8_individual_[selected_tile8_], 16)) {
        // Modify the tile16 based on the selected tile and current_tile16_info
      }
      current_tile_canvas_.DrawBitmap(tile16_individual_[selected_tile16_], 2,
                                      4.0f);
      current_tile_canvas_.DrawGrid(16.f);
      current_tile_canvas_.DrawOverlay();

      gui::InputTileInfo("TL", &current_tile16_info.tiles[0]);
      ImGui::SameLine();
      gui::InputTileInfo("TR", &current_tile16_info.tiles[1]);
      gui::InputTileInfo("BL", &current_tile16_info.tiles[2]);
      ImGui::SameLine();
      gui::InputTileInfo("BR", &current_tile16_info.tiles[3]);

      if (ImGui::Button("Modify Tile16")) {
        tile16_sheet_.ModifyTile16(
            rom()->graphics_buffer(), current_tile16_info.tiles[0],
            current_tile16_info.tiles[1], current_tile16_info.tiles[2],
            current_tile16_info.tiles[3], selected_tile16_, 212);
        tile16_individual_[selected_tile16_] =
            tile16_sheet_.GetTile16(selected_tile16_);
        RETURN_VOID_IF_ERROR(tile16_individual_[selected_tile16_].ApplyPalette(
            *rom()->mutable_dungeon_palette(3)));
        Renderer::GetInstance().RenderBitmap(
            &tile16_individual_[selected_tile16_]);
      }
    }
    ImGui::EndChild();

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
    if (ImGui::Button("Load GFX from BIN file")) LoadBinaryGfx();

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
      auto converted_bin = gfx::SnesTo8bppSheet(bin_data, 4, 4);
      gfx_bin_data_ = converted_bin;
      tile16_sheet_.clear();
      if (LoadDungeonMapTile16(converted_bin, true).ok()) {
        sheets_.clear();
        std::vector<std::vector<uint8_t>> gfx_sheets;
        for (int i = 0; i < 4; i++) {
          gfx_sheets.emplace_back(converted_bin.begin() + (i * 0x1000),
                                  converted_bin.begin() + ((i + 1) * 0x1000));
          sheets_.emplace(i, gfx::Bitmap(128, 32, 8, gfx_sheets[i]));
          status_ = sheets_[i].ApplyPalette(*rom()->mutable_dungeon_palette(3));
          if (status_.ok()) {
            Renderer::GetInstance().RenderBitmap(&sheets_[i]);
          }
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
