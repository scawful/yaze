#include "app/editor/screen_editor.h"

#include <imgui/imgui.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_tile.h"
#include "app/gfx/tilesheet.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/zelda3/dungeon/room.h"

namespace yaze {
namespace app {
namespace editor {

ScreenEditor::ScreenEditor() { screen_canvas_.SetCanvasSize(ImVec2(512, 512)); }

void ScreenEditor::Update() {
  TAB_BAR("##TabBar")
  TAB_ITEM("Dungeon Maps")
  if (rom()->is_loaded()) {
    DrawDungeonMapsEditor();
  }
  END_TAB_ITEM()
  DrawInventoryMenuEditor();
  DrawOverworldMapEditor();
  DrawTitleScreenEditor();
  DrawNamingScreenEditor();
  END_TAB_BAR()
}

void ScreenEditor::DrawInventoryMenuEditor() {
  TAB_ITEM("Inventory Menu")

  static bool create = false;
  if (!create && rom()->is_loaded()) {
    inventory_.Create();
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
    gui::DisplayPalette(palette_, create);

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
    ASSIGN_OR_RETURN(int ptr,
                     rom()->ReadWord(zelda3::kDungeonMapRoomsPtr + (d * 2)));
    ASSIGN_OR_RETURN(int ptrGFX,
                     rom()->ReadWord(zelda3::kDungeonMapRoomsPtr + (d * 2)));
    ptr |= 0x0A0000;                  // Add bank to the short ptr
    ptrGFX |= 0x0A0000;               // Add bank to the short ptr
    int pcPtr = core::SnesToPc(ptr);  // Contains data for the next 25 rooms
    int pcPtrGFX =
        core::SnesToPc(ptrGFX);  // Contains data for the next 25 rooms

    ASSIGN_OR_RETURN(ushort bossRoomD,
                     rom()->ReadWord(zelda3::kDungeonMapBossRooms + (d * 2)));

    ASSIGN_OR_RETURN(nbr_basement_d,
                     rom()->ReadByte(zelda3::kDungeonMapFloors + (d * 2)));
    nbr_basement_d &= 0x0F;

    ASSIGN_OR_RETURN(nbr_floor_d,
                     rom()->ReadByte(zelda3::kDungeonMapFloors + (d * 2)));
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
        // rdata[j] = 0x0F;
        gdata[j] = 0xFF;
        rdata[j] = rom()->data()[pcPtr + j + (i * 25)];  // Set the rooms

        if (rdata[j] == 0x0F) {
          gdata[j] = 0xFF;
        } else {
          gdata[j] = rom()->data()[pcPtrGFX++];
        }

        std::string label = core::UppercaseHexByte(rdata[j]);
        dungeon_map_labels_[d][i][j] = label;
      }

      current_floor_gfx_d.push_back(gdata);    // Add new floor gfx data
      current_floor_rooms_d.push_back(rdata);  // Add new floor data
    }

    dungeon_maps_.emplace_back(bossRoomD, nbr_floor_d, nbr_basement_d,
                               current_floor_rooms_d, current_floor_gfx_d);
  }

  return absl::OkStatus();
}

absl::Status ScreenEditor::SaveDungeonMaps() {
  for (int d = 0; d < 14; d++) {
    int ptr = zelda3::kDungeonMapRoomsPtr + (d * 2);
    int ptrGFX = zelda3::kDungeonMapGfxPtr + (d * 2);
    int pcPtr = core::SnesToPc(ptr);
    int pcPtrGFX = core::SnesToPc(ptrGFX);

    const int nbr_floors = dungeon_maps_[d].nbr_of_floor;
    const int nbr_basements = dungeon_maps_[d].nbr_of_basement;
    for (int i = 0; i < nbr_floors + nbr_basements; i++) {
      for (int j = 0; j < 25; j++) {
        // rom()->data()[pcPtr + j + (i * 25)] =
        //     dungeon_maps_[d].floor_rooms[i][j];
        // rom()->data()[pcPtrGFX++] = dungeon_maps_[d].floor_gfx[i][j];

        RETURN_IF_ERROR(rom()->WriteByte(ptr + j + (i * 25),
                                         dungeon_maps_[d].floor_rooms[i][j]));
        RETURN_IF_ERROR(rom()->WriteByte(ptrGFX + j + (i * 25),
                                         dungeon_maps_[d].floor_gfx[i][j]));
        pcPtrGFX++;
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ScreenEditor::LoadDungeonMapTile16() {
  tile16_sheet_.Init(256, 192, gfx::TileType::Tile16);

  for (int i = 0; i < 186; i++) {
    int addr = zelda3::kDungeonMapTile16;
    if (rom()->data()[zelda3::kDungeonMapExpCheck] != 0xB9) {
      addr = zelda3::kDungeonMapTile16Expanded;
    }

    ASSIGN_OR_RETURN(auto tl, rom()->ReadWord(addr + (i * 8)));
    gfx::TileInfo t1 = gfx::WordToTileInfo(tl);  // Top left

    ASSIGN_OR_RETURN(auto tr, rom()->ReadWord(addr + 2 + (i * 8)));
    gfx::TileInfo t2 = gfx::WordToTileInfo(tr);  // Top right

    ASSIGN_OR_RETURN(auto bl, rom()->ReadWord(addr + 4 + (i * 8)));
    gfx::TileInfo t3 = gfx::WordToTileInfo(bl);  // Bottom left

    ASSIGN_OR_RETURN(auto br, rom()->ReadWord(addr + 6 + (i * 8)));
    gfx::TileInfo t4 = gfx::WordToTileInfo(br);  // Bottom right

    tile16_sheet_.ComposeTile16(rom()->graphics_buffer(), t1, t2, t3, t4);
  }

  tile16_sheet_.mutable_bitmap()->ApplyPalette(
      *rom()->mutable_dungeon_palette(3));
  rom()->RenderBitmap(&*tile16_sheet_.mutable_bitmap().get());

  for (int i = 0; i < tile16_sheet_.num_tiles(); ++i) {
    if (tile16_individual_.count(i) == 0) {
      auto tile = tile16_sheet_.GetTile16(i);
      tile16_individual_[i] = tile;
      rom()->RenderBitmap(&tile16_individual_[i]);
    }
  }

  return absl::OkStatus();
}

void ScreenEditor::DrawDungeonMapsTabs() {
  auto current_dungeon = dungeon_maps_[selected_dungeon];
  if (ImGui::BeginTabBar("##DungeonMapTabs")) {
    auto nbr_floors =
        current_dungeon.nbr_of_floor + current_dungeon.nbr_of_basement;
    for (int i = 0; i < nbr_floors; i++) {
      std::string tab_name = absl::StrFormat("Floor %d", i + 1);
      if (i >= current_dungeon.nbr_of_floor) {
        tab_name = absl::StrFormat("Basement %d",
                                   i - current_dungeon.nbr_of_floor + 1);
      }

      if (ImGui::BeginTabItem(tab_name.c_str())) {
        floor_number = i;
        // screen_canvas_.LoadCustomLabels(dungeon_map_labels_[selected_dungeon]);
        // screen_canvas_.set_current_labels(floor_number);
        screen_canvas_.DrawBackground(ImVec2(325, 325));
        screen_canvas_.DrawTileSelector(64.f);

        auto boss_room = current_dungeon.boss_room;
        for (int j = 0; j < 25; j++) {
          if (current_dungeon.floor_rooms[floor_number][j] != 0x0F) {
            int tile16_id = current_dungeon.floor_rooms[floor_number][j];
            int tile_x = (tile16_id % 16) * 16;
            int tile_y = (tile16_id / 16) * 16;
            int posX = ((j % 5) * 32);
            int posY = ((j / 5) * 32);

            if (tile16_individual_.count(tile16_id) == 0) {
              auto tile = tile16_sheet_.GetTile16(tile16_id);
              std::cout << "Tile16: " << tile16_id << std::endl;
              rom()->RenderBitmap(&tile);
              tile16_individual_[tile16_id] = tile;
            }
            screen_canvas_.DrawBitmap(tile16_individual_[tile16_id], (posX * 2),
                                      (posY * 2), 4.0f);

            if (current_dungeon.floor_rooms[floor_number][j] == boss_room) {
              screen_canvas_.DrawOutlineWithColor((posX * 2), (posY * 2), 64,
                                                  64, core::kRedPen);
            }

            std::string label =
                dungeon_map_labels_[selected_dungeon][floor_number][j];
            screen_canvas_.DrawText(label, (posX * 2), (posY * 2));
            // GFX.drawText(
            //     e.Graphics, 16 + ((i % 5) * 32), 20 + ((i / 5) * 32),
          }

          // if (dungmapSelectedTile == i)
          //  Constants.AzurePen2,
          //       10 + ((i % 5) * 32), 12 + ((i / 5) * 32), 32, 32));
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

  if (ImGui::Button("Copy Floor", ImVec2(100, 0))) {
    copy_button_pressed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("Paste Floor", ImVec2(100, 0))) {
    paste_button_pressed = true;
  }
}

void ScreenEditor::DrawDungeonMapsEditor() {
  if (!dungeon_maps_loaded_) {
    if (LoadDungeonMaps().ok()) {
      if (LoadDungeonMapTile16().ok()) {
        auto bitmap_manager = rom()->mutable_bitmap_manager();
        sheets_.emplace(0, *bitmap_manager->mutable_bitmap(212));
        sheets_.emplace(1, *bitmap_manager->mutable_bitmap(213));
        sheets_.emplace(2, *bitmap_manager->mutable_bitmap(214));
        sheets_.emplace(3, *bitmap_manager->mutable_bitmap(215));
        dungeon_maps_loaded_ = true;
      } else {
        ImGui::Text("Failed to load dungeon map tile16");
      }
    } else {
      ImGui::Text("Failed to load dungeon maps");
    }
  }

  static std::vector<std::string> dungeon_names = {
      "Sewers/Sanctuary",   "Hyrule Castle", "Eastern Palace",
      "Desert Palace",      "Tower of Hera", "Agahnim's Tower",
      "Palace of Darkness", "Swamp Palace",  "Skull Woods",
      "Thieves' Town",      "Ice Palace",    "Misery Mire",
      "Turtle Rock",        "Ganon's Tower"};

  if (ImGui::BeginTable("DungeonMapsTable", 4, ImGuiTableFlags_Resizable)) {
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
      }
    }
    ImGui::EndChild();

    ImGui::TableNextColumn();
    tilemap_canvas_.DrawBackground(ImVec2(128 * 2 + 2, (192 * 2) + 4));
    tilemap_canvas_.DrawContextMenu();
    tilemap_canvas_.DrawBitmapTable(sheets_);
    tilemap_canvas_.DrawGrid();
    tilemap_canvas_.DrawOverlay();

    ImGui::EndTable();
  }
}

void ScreenEditor::DrawTitleScreenEditor() {
  TAB_ITEM("Title Screen")
  END_TAB_ITEM()
}
void ScreenEditor::DrawNamingScreenEditor() {
  TAB_ITEM("Naming Screen")
  END_TAB_ITEM()
}
void ScreenEditor::DrawOverworldMapEditor() {
  TAB_ITEM("Overworld Map")
  END_TAB_ITEM()
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
}  // namespace app
}  // namespace yaze