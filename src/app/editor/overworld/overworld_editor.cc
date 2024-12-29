#include "overworld_editor.h"

#include <cmath>
#include <future>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/constants.h"
#include "app/core/platform/clipboard.h"
#include "app/core/platform/renderer.h"
#include "app/editor/graphics/palette_editor.h"
#include "app/editor/overworld/entity.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/gui/zeml.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::BeginTooltip;
using ImGui::Button;
using ImGui::Checkbox;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::EndTable;
using ImGui::EndTooltip;
using ImGui::IsItemHovered;
using ImGui::NewLine;
using ImGui::PopStyleColor;
using ImGui::PushStyleColor;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

constexpr int kTile16Size = 0x10;

absl::Status OverworldEditor::Update() {
  status_ = absl::OkStatus();
  if (rom()->is_loaded() && !all_gfx_loaded_) {
    RETURN_IF_ERROR(tile16_editor_.InitBlockset(
        tile16_blockset_bmp_, current_gfx_bmp_, tile16_individual_,
        *overworld_.mutable_all_tiles_types()));
    RETURN_IF_ERROR(LoadEntranceTileTypes(*rom()));
    all_gfx_loaded_ = true;
  }

  if (overworld_canvas_fullscreen_) {
    DrawFullscreenCanvas();
  }

  // Draw the overworld editor layout from the ZEML file
  gui::zeml::Render(layout_node_);
  return status_;
}

void OverworldEditor::DrawFullscreenCanvas() {
  static bool use_work_area = true;
  static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings;
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
  ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);
  if (ImGui::Begin("Fullscreen Overworld Editor", &overworld_canvas_fullscreen_,
                   flags)) {
    // Draws the toolset for editing the Overworld.
    DrawToolset();
    DrawOverworldCanvas();
  }
  ImGui::End();
}

void OverworldEditor::DrawToolset() {
  static bool show_gfx_group = false;
  static bool show_properties = false;

  if (BeginTable("OWToolset", 22, kToolsetTableFlags, ImVec2(0, 0))) {
    for (const auto &name : kToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    NEXT_COLUMN()
    if (Button(ICON_MD_UNDO)) {
      status_ = Undo();
    }

    NEXT_COLUMN()
    if (Button(ICON_MD_REDO)) {
      status_ = Redo();
    }

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    NEXT_COLUMN()
    if (Button(ICON_MD_ZOOM_OUT)) {
      ow_map_canvas_.ZoomOut();
    }

    NEXT_COLUMN()
    if (Button(ICON_MD_ZOOM_IN)) {
      ow_map_canvas_.ZoomIn();
    }

    NEXT_COLUMN()
    if (Button(ICON_MD_OPEN_IN_FULL)) {
      overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
    }
    HOVER_HINT("Fullscreen Canvas")

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    NEXT_COLUMN()
    if (Selectable(ICON_MD_PAN_TOOL_ALT, current_mode == EditingMode::PAN)) {
      current_mode = EditingMode::PAN;
      ow_map_canvas_.set_draggable(true);
    }
    HOVER_HINT("Pan (Right click and drag)")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE)) {
      current_mode = EditingMode::DRAW_TILE;
    }
    HOVER_HINT("Draw Tile")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_DOOR_FRONT, current_mode == EditingMode::ENTRANCES))
      current_mode = EditingMode::ENTRANCES;
    HOVER_HINT("Entrances")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_DOOR_BACK, current_mode == EditingMode::EXITS))
      current_mode = EditingMode::EXITS;
    HOVER_HINT("Exits")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_GRASS, current_mode == EditingMode::ITEMS))
      current_mode = EditingMode::ITEMS;
    HOVER_HINT("Items")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_PEST_CONTROL_RODENT,
                   current_mode == EditingMode::SPRITES))
      current_mode = EditingMode::SPRITES;
    HOVER_HINT("Sprites")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_ADD_LOCATION,
                   current_mode == EditingMode::TRANSPORTS))
      current_mode = EditingMode::TRANSPORTS;
    HOVER_HINT("Transports")

    NEXT_COLUMN()
    if (Selectable(ICON_MD_MUSIC_NOTE, current_mode == EditingMode::MUSIC))
      current_mode = EditingMode::MUSIC;
    HOVER_HINT("Music")

    TableNextColumn();
    if (Button(ICON_MD_GRID_VIEW)) {
      show_tile16_editor_ = !show_tile16_editor_;
    }
    HOVER_HINT("Tile16 Editor")

    TableNextColumn();
    if (Button(ICON_MD_TABLE_CHART)) {
      show_gfx_group = !show_gfx_group;
    }
    HOVER_HINT("Gfx Group Editor")

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    TableNextColumn();
    if (Button(ICON_MD_CONTENT_COPY)) {
      std::vector<uint8_t> png_data;
      if (gfx::ConvertSurfaceToPNG(maps_bmp_[current_map_].surface(),
                                   png_data)) {
        core::CopyImageToClipboard(png_data);
      } else {
        status_ = absl::InternalError(
            "Failed to convert overworld map surface to PNG");
      }
    }
    HOVER_HINT("Copy Map to Clipboard");

    TableNextColumn();  // Palette
    status_ = DisplayPalette(palette_, overworld_.is_loaded());

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    TableNextColumn();
    Checkbox("Properties", &show_properties);

    ImGui::EndTable();
  }

  if (show_tile16_editor_) {
    // Create a table in ImGui for the Tile16 Editor
    ImGui::Begin("Tile16 Editor", &show_tile16_editor_,
                 ImGuiWindowFlags_MenuBar);
    status_ = tile16_editor_.Update();
    ImGui::End();
  }

  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    status_ = gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }

  if (show_properties) {
    ImGui::Begin("Properties", &show_properties);
    DrawOverworldProperties();
    ImGui::End();
  }

  // TODO: Customizable shortcuts for the Overworld Editor
  if (!ImGui::IsAnyItemActive()) {
    if (ImGui::IsKeyDown(ImGuiKey_1)) {
      current_mode = EditingMode::PAN;
    } else if (ImGui::IsKeyDown(ImGuiKey_2)) {
      current_mode = EditingMode::DRAW_TILE;
    } else if (ImGui::IsKeyDown(ImGuiKey_3)) {
      current_mode = EditingMode::ENTRANCES;
    } else if (ImGui::IsKeyDown(ImGuiKey_4)) {
      current_mode = EditingMode::EXITS;
    } else if (ImGui::IsKeyDown(ImGuiKey_5)) {
      current_mode = EditingMode::ITEMS;
    } else if (ImGui::IsKeyDown(ImGuiKey_6)) {
      current_mode = EditingMode::SPRITES;
    } else if (ImGui::IsKeyDown(ImGuiKey_7)) {
      current_mode = EditingMode::TRANSPORTS;
    } else if (ImGui::IsKeyDown(ImGuiKey_8)) {
      current_mode = EditingMode::MUSIC;
    }
  }
}

constexpr std::array<const char *, 8> kMapSettingsColumnNames = {
    "##WorldId", "##GfxId",  "##PalId",  "##SprGfxId",
    "##5thCol",  "##6thCol", "##7thCol", "##8thCol"};

void OverworldEditor::DrawOverworldMapSettings() {
  if (BeginTable(kOWMapTable.data(), 8, kOWMapFlags, ImVec2(0, 0), -1)) {
    for (const auto &name : kMapSettingsColumnNames)
      ImGui::TableSetupColumn(name);

    TableNextColumn();
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("##world", &current_world_, kWorldList.data(), 3);

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Gfx",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_graphics(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Palette",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      status_ = RefreshMapPalette();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Gfx",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_graphics(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Palette",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_palette(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexWord(
        "Msg Id",
        overworld_.mutable_overworld_map(current_map_)->mutable_message_id(),
        kInputFieldSize + 20);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##World", &game_state_, kGamePartComboString.data(), 3);

    TableNextColumn();
    ImGui::Checkbox(
        "##mosaic",
        overworld_.mutable_overworld_map(current_map_)->mutable_mosaic());
    HOVER_HINT("Enable Mosaic effect for the current map");

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawCustomOverworldMapSettings() {
  if (BeginTable(kOWMapTable.data(), 15, kOWMapFlags, ImVec2(0, 0), -1)) {
    for (const auto &name : kMapSettingsColumnNames)
      ImGui::TableSetupColumn(name);

    TableNextColumn();
    ImGui::SetNextItemWidth(120.f);
    ImGui::Combo("##world", &current_world_, kWorldList.data(), 3);

    static const std::array<std::string, 8> kCustomMapSettingsColumnNames = {
        "TileGfx0", "TileGfx1", "TileGfx2", "TileGfx3",
        "TileGfx4", "TileGfx5", "TileGfx6", "TileGfx7"};
    for (int i = 0; i < 8; ++i) {
      TableNextColumn();
      ImGui::BeginGroup();
      if (gui::InputHexByte(kCustomMapSettingsColumnNames[i].data(),
                            overworld_.mutable_overworld_map(current_map_)
                                ->mutable_custom_tileset(i),
                            kInputFieldSize)) {
        RefreshMapProperties();
        RefreshOverworldMap();
      }
      ImGui::EndGroup();
    }

    TableNextColumn();
    ImGui::BeginGroup();
    if (gui::InputHexByte("Palette",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          kInputFieldSize)) {
      RefreshMapProperties();
      status_ = RefreshMapPalette();
      RefreshOverworldMap();
    }
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Gfx",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_graphics(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexByte("Spr Palette",
                      overworld_.mutable_overworld_map(current_map_)
                          ->mutable_sprite_palette(game_state_),
                      kInputFieldSize);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::BeginGroup();
    gui::InputHexWord(
        "Msg Id",
        overworld_.mutable_overworld_map(current_map_)->mutable_message_id(),
        kInputFieldSize + 20);
    ImGui::EndGroup();

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##World", &game_state_, kGamePartComboString.data(), 3);

    TableNextColumn();
    ImGui::Checkbox(
        "##mosaic",
        overworld_.mutable_overworld_map(current_map_)->mutable_mosaic());
    HOVER_HINT("Enable Mosaic effect for the current map");

    ImGui::EndTable();
  }
}

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);
    int map_x = (xx * kOverworldMapSize * ow_map_canvas_.global_scale());
    int map_y = (yy * kOverworldMapSize * ow_map_canvas_.global_scale());
    ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y,
                              ow_map_canvas_.global_scale());
    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
    }
  }
}

void OverworldEditor::DrawOverworldEdits() {
  // Determine which overworld map the user is currently editing.
  auto mouse_position = ow_map_canvas_.drawn_tile_position();
  int map_x = mouse_position.x / kOverworldMapSize;
  int map_y = mouse_position.y / kOverworldMapSize;
  current_map_ = map_x + map_y * 8;
  if (current_world_ == 1) {
    current_map_ += 0x40;
  } else if (current_world_ == 2) {
    current_map_ += 0x80;
  }

  // Render the updated map bitmap.
  RenderUpdatedMapBitmap(mouse_position,
                         tile16_individual_data_[current_tile16_]);

  // Calculate the correct superX and superY values
  int superY = current_map_ / 8;
  int superX = current_map_ % 8;
  int mouse_x = mouse_position.x;
  int mouse_y = mouse_position.y;
  // Calculate the correct tile16_x and tile16_y positions
  int tile16_x = (mouse_x % kOverworldMapSize) / (kOverworldMapSize / 32);
  int tile16_y = (mouse_y % kOverworldMapSize) / (kOverworldMapSize / 32);

  // Update the overworld_.map_tiles() based on tile16 ID and current world
  auto &selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  int index_x = superX * 32 + tile16_x;
  int index_y = superY * 32 + tile16_y;

  selected_world[index_x][index_y] = current_tile16_;
}

void OverworldEditor::RenderUpdatedMapBitmap(
    const ImVec2 &click_position, const std::vector<uint8_t> &tile_data) {
  // Calculate the tile index for x and y based on the click_position
  int tile_index_x =
      (static_cast<int>(click_position.x) % kOverworldMapSize) / kTile16Size;
  int tile_index_y =
      (static_cast<int>(click_position.y) % kOverworldMapSize) / kTile16Size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = tile_index_x * kTile16Size;
  start_position.y = tile_index_y * kTile16Size;

  // Update the bitmap's pixel data based on the start_position and tile_data
  gfx::Bitmap &current_bitmap = maps_bmp_[current_map_];
  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      int pixel_index =
          (start_position.y + y) * kOverworldMapSize + (start_position.x + x);
      current_bitmap.WriteToPixel(pixel_index, tile_data[y * kTile16Size + x]);
    }
  }

  current_bitmap.set_modified(true);
}

void OverworldEditor::CheckForOverworldEdits() {
  CheckForSelectRectangle();

  // User has selected a tile they want to draw from the blockset.
  if (!blockset_canvas_.points().empty() &&
      !ow_map_canvas_.select_rect_active()) {
    // Left click is pressed
    if (ow_map_canvas_.DrawTilePainter(tile16_individual_[current_tile16_],
                                       kTile16Size)) {
      DrawOverworldEdits();
    }
  }

  if (ow_map_canvas_.select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      auto &selected_world =
          (current_world_ == 0) ? overworld_.mutable_map_tiles()->light_world
          : (current_world_ == 1)
              ? overworld_.mutable_map_tiles()->dark_world
              : overworld_.mutable_map_tiles()->special_world;
      // new_start_pos and new_end_pos
      auto start = ow_map_canvas_.selected_points()[0];
      auto end = ow_map_canvas_.selected_points()[1];

      // Calculate the bounds of the rectangle in terms of 16x16 tile indices
      int start_x = std::floor(start.x / kTile16Size) * kTile16Size;
      int start_y = std::floor(start.y / kTile16Size) * kTile16Size;
      int end_x = std::floor(end.x / kTile16Size) * kTile16Size;
      int end_y = std::floor(end.y / kTile16Size) * kTile16Size;

      if (start_x > end_x) std::swap(start_x, end_x);
      if (start_y > end_y) std::swap(start_y, end_y);

      constexpr int local_map_size = 512;  // Size of each local map
      // Number of tiles per local map (since each tile is 16x16)
      constexpr int tiles_per_local_map = local_map_size / kTile16Size;

      for (int y = start_y, i = 0; y <= end_y; y += kTile16Size) {
        for (int x = start_x; x <= end_x; x += kTile16Size, ++i) {
          // Determine which local map (512x512) the tile is in
          int local_map_x = x / local_map_size;
          int local_map_y = y / local_map_size;

          // Calculate the tile's position within its local map
          int tile16_x = (x % local_map_size) / kTile16Size;
          int tile16_y = (y % local_map_size) / kTile16Size;

          // Calculate the index within the overall map structure
          int index_x = local_map_x * tiles_per_local_map + tile16_x;
          int index_y = local_map_y * tiles_per_local_map + tile16_y;
          int tile16_id = overworld_.GetTileFromPosition(
              ow_map_canvas_.selected_tiles()[i]);
          selected_world[index_x][index_y] = tile16_id;
        }
      }

      RefreshOverworldMap();
    }
  }
}

void OverworldEditor::CheckForSelectRectangle() {
  ow_map_canvas_.DrawSelectRect(current_map_);

  // Single tile case
  if (ow_map_canvas_.selected_tile_pos().x != -1) {
    current_tile16_ =
        overworld_.GetTileFromPosition(ow_map_canvas_.selected_tile_pos());
    ow_map_canvas_.set_selected_tile_pos(ImVec2(-1, -1));
  }

  static std::vector<int> tile16_ids;
  if (ow_map_canvas_.select_rect_active()) {
    // Get the tile16 IDs from the selected tile ID positions
    if (tile16_ids.size() != 0) {
      tile16_ids.clear();
    }

    if (ow_map_canvas_.selected_tiles().size() > 0) {
      for (auto &each : ow_map_canvas_.selected_tiles()) {
        tile16_ids.push_back(overworld_.GetTileFromPosition(each));
      }
    }
  }
  // Create a composite image of all the tile16s selected
  ow_map_canvas_.DrawBitmapGroup(tile16_ids, tile16_individual_, 0x10);
}

absl::Status OverworldEditor::CheckForCurrentMap() {
  // 4096x4096, 512x512 maps and some are larges maps 1024x1024
  const auto mouse_position = ImGui::GetIO().MousePos;
  const int large_map_size = 1024;
  const auto canvas_zero_point = ow_map_canvas_.zero_point();

  // Calculate which small map the mouse is currently over
  int map_x = (mouse_position.x - canvas_zero_point.x) / kOverworldMapSize;
  int map_y = (mouse_position.y - canvas_zero_point.y) / kOverworldMapSize;

  // Calculate the index of the map in the `maps_bmp_` vector
  current_map_ = map_x + map_y * 8;
  const int current_highlighted_map = current_map_;
  if (current_world_ == 1) {
    current_map_ += 0x40;
  } else if (current_world_ == 2) {
    current_map_ += 0x80;
  }

  current_parent_ = overworld_.overworld_map(current_map_)->parent();

  if (overworld_.overworld_map(current_map_)->is_large_map() ||
      overworld_.overworld_map(current_map_)->large_index() != 0) {
    const int highlight_parent =
        overworld_.overworld_map(current_highlighted_map)->parent();
    const int parent_map_x = highlight_parent % 8;
    const int parent_map_y = highlight_parent / 8;
    ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                               parent_map_y * kOverworldMapSize, large_map_size,
                               large_map_size);
  } else {
    const int current_map_x = current_highlighted_map % 8;
    const int current_map_y = current_highlighted_map / 8;
    ow_map_canvas_.DrawOutline(current_map_x * kOverworldMapSize,
                               current_map_y * kOverworldMapSize,
                               kOverworldMapSize, kOverworldMapSize);
  }

  if (maps_bmp_[current_map_].modified() ||
      ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    RefreshOverworldMap();
    RETURN_IF_ERROR(RefreshTile16Blockset());
    Renderer::GetInstance().UpdateBitmap(&maps_bmp_[current_map_]);
    maps_bmp_[current_map_].set_modified(false);
  }

  return absl::OkStatus();
}

void OverworldEditor::CheckForMousePan() {
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    previous_mode = current_mode;
    current_mode = EditingMode::PAN;
    ow_map_canvas_.set_draggable(true);
    middle_mouse_dragging_ = true;
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) &&
      current_mode == EditingMode::PAN && middle_mouse_dragging_) {
    current_mode = previous_mode;
    ow_map_canvas_.set_draggable(false);
    middle_mouse_dragging_ = false;
  }
}

void OverworldEditor::DrawOverworldCanvas() {
  if (all_gfx_loaded_) {
    if (flags()->overworld.kLoadCustomOverworld) {
      DrawCustomOverworldMapSettings();
    } else {
      DrawOverworldMapSettings();
    }
    Separator();
  }

  gui::BeginNoPadding();
  gui::BeginChildBothScrollbars(7);
  ow_map_canvas_.DrawBackground();
  gui::EndNoPadding();

  CheckForMousePan();
  if (current_mode == EditingMode::PAN) {
    ow_map_canvas_.DrawContextMenu();
  } else {
    ow_map_canvas_.set_draggable(false);
  }

  if (overworld_.is_loaded()) {
    DrawOverworldMaps();
    DrawOverworldExits(ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
    DrawOverworldEntrances(ow_map_canvas_.zero_point(),
                           ow_map_canvas_.scrolling());
    DrawOverworldItems();
    DrawOverworldSprites();
    if (current_mode == EditingMode::DRAW_TILE) {
      CheckForOverworldEdits();
    }
    if (IsItemHovered()) status_ = CheckForCurrentMap();
  }

  ow_map_canvas_.DrawGrid();
  ow_map_canvas_.DrawOverlay();
  EndChild();

  // Handle mouse wheel activity
  if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    ImGui::SetScrollX(ImGui::GetScrollX() + ImGui::GetIO().MouseWheelH * 16.0f);
    ImGui::SetScrollY(ImGui::GetScrollY() + ImGui::GetIO().MouseWheel * 16.0f);
  }
}

absl::Status OverworldEditor::DrawTile16Selector() {
  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##Tile16SelectorScrollRegion");
  blockset_canvas_.DrawBackground();
  gui::EndNoPadding();
  {
    blockset_canvas_.DrawContextMenu();
    blockset_canvas_.DrawBitmap(tile16_blockset_bmp_, /*border_offset=*/2,
                                map_blockset_loaded_);

    if (blockset_canvas_.DrawTileSelector(32.0f)) {
      // Open the tile16 editor to the tile
      auto tile_pos = blockset_canvas_.points().front();
      int grid_x = static_cast<int>(tile_pos.x / 32);
      int grid_y = static_cast<int>(tile_pos.y / 32);
      int id = grid_x + grid_y * 8;
      RETURN_IF_ERROR(tile16_editor_.SetCurrentTile(id));
      show_tile16_editor_ = true;
    }

    if (ImGui::IsItemClicked() && !blockset_canvas_.points().empty()) {
      int x = blockset_canvas_.points().front().x / 32;
      int y = blockset_canvas_.points().front().y / 32;
      current_tile16_ = x + (y * 8);
    }

    blockset_canvas_.DrawGrid();
    blockset_canvas_.DrawOverlay();
  }
  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground();
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    int key = 0;
    for (auto &value : rom()->gfx_sheets()) {
      int offset = 0x40 * (key + 1);
      int top_left_y = graphics_bin_canvas_.zero_point().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.zero_point().y + 0x40 * key;
      }
      auto texture = value.texture();
      graphics_bin_canvas_.draw_list()->AddImage(
          (ImTextureID)(intptr_t)texture,
          ImVec2(graphics_bin_canvas_.zero_point().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.zero_point().x + 0x100,
                 graphics_bin_canvas_.zero_point().y + offset));
      key++;
    }
  }
  graphics_bin_canvas_.DrawGrid();
  graphics_bin_canvas_.DrawOverlay();
}

absl::Status OverworldEditor::DrawAreaGraphics() {
  if (overworld_.is_loaded() &&
      current_graphics_set_.count(current_map_) == 0) {
    overworld_.set_current_map(current_map_);
    palette_ = overworld_.current_area_palette();
    gfx::Bitmap bmp;
    RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
        0x80, kOverworldMapSize, 0x08, overworld_.current_graphics(), bmp,
        palette_));
    current_graphics_set_[current_map_] = bmp;
  }

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##AreaGraphicsScrollRegion");
  current_gfx_canvas_.DrawBackground();
  gui::EndPadding();
  {
    current_gfx_canvas_.DrawContextMenu();
    current_gfx_canvas_.DrawBitmap(current_graphics_set_[current_map_],
                                   /*border_offset=*/2, overworld_.is_loaded());
    current_gfx_canvas_.DrawTileSelector(32.0f);
    current_gfx_canvas_.DrawGrid();
    current_gfx_canvas_.DrawOverlay();
  }
  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawTileSelector() {
  if (BeginTabBar(kTileSelectorTab.data(),
                  ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (BeginTabItem("Tile16")) {
      status_ = DrawTile16Selector();
      EndTabItem();
    }
    if (BeginTabItem("Tile8")) {
      gui::BeginPadding(3);
      gui::BeginChildWithScrollbar("##Tile8SelectorScrollRegion");
      DrawTile8Selector();
      EndChild();
      gui::EndNoPadding();
      EndTabItem();
    }
    if (BeginTabItem("Area Graphics")) {
      status_ = DrawAreaGraphics();
      EndTabItem();
    }
    EndTabBar();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldEntrances(ImVec2 canvas_p0, ImVec2 scrolling,
                                             bool holes) {
  int i = 0;
  for (auto &each : overworld_.entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted) {
      auto color = ImVec4(255, 255, 0, 100);
      if (each.is_hole_) {
        color = ImVec4(255, 255, 255, 200);
      }
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16, color);
      std::string str = core::UppercaseHexByte(each.entrance_id_);

      if (current_mode == EditingMode::ENTRANCES) {
        HandleEntityDragging(&each, canvas_p0, scrolling, is_dragging_entity_,
                             dragged_entity_, current_entity_);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.entrance_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_entrance_id_ = i;
          current_entrance_ = each;
        }
      }

      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  if (DrawEntranceInserterPopup()) {
    // Get the deleted entrance ID and insert it at the mouse position
    auto deleted_entrance_id = overworld_.deleted_entrances().back();
    overworld_.deleted_entrances().pop_back();
    auto &entrance = overworld_.entrances()[deleted_entrance_id];
    entrance.map_id_ = current_map_;
    entrance.entrance_id_ = deleted_entrance_id;
    entrance.x_ = ow_map_canvas_.hover_mouse_pos().x;
    entrance.y_ = ow_map_canvas_.hover_mouse_pos().y;
    entrance.deleted = false;
  }

  if (current_mode == EditingMode::ENTRANCES) {
    const auto is_hovering =
        IsMouseHoveringOverEntity(current_entrance_, canvas_p0, scrolling);

    if (!is_hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Entrance Inserter");
    } else {
      if (DrawOverworldEntrancePopup(
              overworld_.entrances()[current_entrance_id_])) {
        overworld_.entrances()[current_entrance_id_] = current_entrance_;
      }

      if (overworld_.entrances()[current_entrance_id_].deleted) {
        overworld_.mutable_deleted_entrances()->emplace_back(
            current_entrance_id_);
      }
    }
  }
}

void OverworldEditor::DrawOverworldExits(ImVec2 canvas_p0, ImVec2 scrolling) {
  int i = 0;
  for (auto &each : *overworld_.mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted_) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              ImVec4(255, 255, 255, 150));
      if (current_mode == EditingMode::EXITS) {
        each.entity_id_ = i;
        HandleEntityDragging(&each, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_, true);

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          jump_to_tab_ = each.room_id_;
        }

        if (IsMouseHoveringOverEntity(each, canvas_p0, scrolling) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_exit_id_ = i;
          current_exit_ = each;
          current_entity_ = &each;
          current_entity_->entity_id_ = i;
          ImGui::OpenPopup("Exit editor");
        }
      }

      std::string str = core::UppercaseHexByte(i);
      ow_map_canvas_.DrawText(str, each.x_, each.y_);
    }
    i++;
  }

  DrawExitInserterPopup();
  if (current_mode == EditingMode::EXITS) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_exits()->at(current_exit_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Exit Inserter");
    } else {
      if (DrawExitEditorPopup(
              overworld_.mutable_exits()->at(current_exit_id_))) {
        overworld_.mutable_exits()->at(current_exit_id_) = current_exit_;
      }
    }
  }
}

void OverworldEditor::DrawOverworldItems() {
  int i = 0;
  for (auto &item : *overworld_.mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world_ * 0x40) &&
        item.room_map_id_ >= (current_world_ * 0x40) && !item.deleted) {
      ow_map_canvas_.DrawRect(item.x_, item.y_, 16, 16, ImVec4(255, 0, 0, 150));

      if (current_mode == EditingMode::ITEMS) {
        // Check if this item is being clicked and dragged
        HandleEntityDragging(&item, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);

        const auto hovering = IsMouseHoveringOverEntity(
            item, ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
        if (hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_item_id_ = i;
          current_item_ = item;
          current_entity_ = &item;
        }
      }
      std::string item_name = "";
      if (item.id_ < zelda3::overworld::kSecretItemNames.size()) {
        item_name = zelda3::overworld::kSecretItemNames[item.id_];
      } else {
        item_name = absl::StrFormat("0x%02X", item.id_);
      }
      ow_map_canvas_.DrawText(item_name, item.x_, item.y_);
    }
    i++;
  }

  DrawItemInsertPopup();
  if (current_mode == EditingMode::ITEMS) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_all_items()->at(current_item_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Item Inserter");
    } else {
      if (DrawItemEditorPopup(
              overworld_.mutable_all_items()->at(current_item_id_))) {
        overworld_.mutable_all_items()->at(current_item_id_) = current_item_;
      }
    }
  }
}

void OverworldEditor::DrawOverworldSprites() {
  int i = 0;
  for (auto &sprite : *overworld_.mutable_sprites(game_state_)) {
    if (!sprite.deleted()) {
      // int map_id = sprite.map_id();
      // map x and map y are relative to the map
      // So we need to check if the map is large or small then add the offset

      // Calculate the superX and superY values
      // int superY = map_id / 8;
      // int superX = map_id % 8;

      // Calculate the map_x and map_y values
      int map_x = sprite.map_x();
      int map_y = sprite.map_y();

      // Calculate the actual map_x and map_y values
      // map_x += superX * 512;
      // map_y += superY * 512;

      ow_map_canvas_.DrawRect(map_x, map_y, kTile16Size, kTile16Size,
                              /*magenta*/ ImVec4(255, 0, 255, 150));
      if (current_mode == EditingMode::SPRITES) {
        HandleEntityDragging(&sprite, ow_map_canvas_.zero_point(),
                             ow_map_canvas_.scrolling(), is_dragging_entity_,
                             dragged_entity_, current_entity_);
        if (IsMouseHoveringOverEntity(sprite, ow_map_canvas_.zero_point(),
                                      ow_map_canvas_.scrolling()) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
          current_sprite_id_ = i;
          current_sprite_ = sprite;
        }
      }
      if (sprite_previews_[sprite.id()].is_active()) {
        ow_map_canvas_.DrawBitmap(sprite_previews_[sprite.id()], map_x, map_y,
                                  2.0f);
      }

      ow_map_canvas_.DrawText(absl::StrFormat("%s", sprite.name()), map_x,
                              map_y);
    }
    i++;
  }

  DrawSpriteInserterPopup();
  if (current_mode == EditingMode::SPRITES) {
    const auto hovering = IsMouseHoveringOverEntity(
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_),
        ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());

    if (!hovering && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      ImGui::OpenPopup("Sprite Inserter");
    } else {
      if (DrawSpriteEditorPopup(overworld_.mutable_sprites(game_state_)
                                    ->at(current_sprite_id_))) {
        overworld_.mutable_sprites(game_state_)->at(current_sprite_id_) =
            current_sprite_;
      }
    }
  }
}

absl::Status OverworldEditor::Save() {
  if (flags()->overworld.kSaveOverworldMaps) {
    RETURN_IF_ERROR(overworld_.CreateTile32Tilemap());
    RETURN_IF_ERROR(overworld_.SaveMap32Tiles());
    RETURN_IF_ERROR(overworld_.SaveMap16Tiles());
    RETURN_IF_ERROR(overworld_.SaveOverworldMaps());
  }
  if (flags()->overworld.kSaveOverworldEntrances) {
    RETURN_IF_ERROR(overworld_.SaveEntrances());
  }
  if (flags()->overworld.kSaveOverworldExits) {
    RETURN_IF_ERROR(overworld_.SaveExits());
  }
  if (flags()->overworld.kSaveOverworldItems) {
    RETURN_IF_ERROR(overworld_.SaveItems());
  }
  if (flags()->overworld.kSaveOverworldProperties) {
    RETURN_IF_ERROR(overworld_.SaveMapProperties());
  }
  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadGraphics() {
  // Load the Link to the Past overworld.
  RETURN_IF_ERROR(overworld_.Load(*rom()))
  palette_ = overworld_.current_area_palette();

  // Create the area graphics image
  RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
      0x80, kOverworldMapSize, 0x40, overworld_.current_graphics(),
      current_gfx_bmp_, palette_));

  // Create the tile16 blockset image
  RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
      0x80, 0x2000, 0x08, overworld_.tile16_blockset_data(),
      tile16_blockset_bmp_, palette_));
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_data = overworld_.tile16_blockset_data();
  tile16_individual_.reserve(zelda3::overworld::kNumTile16Individual);

  // Loop through the tiles and copy their pixel data into separate vectors
  for (uint i = 0; i < zelda3::overworld::kNumTile16Individual; i++) {
    std::vector<uint8_t> tile_data(kTile16Size * kTile16Size, 0x00);

    // Copy the pixel data for the current tile into the vector
    for (int ty = 0; ty < kTile16Size; ty++) {
      for (int tx = 0; tx < kTile16Size; tx++) {
        int position = tx + (ty * kTile16Size);
        uint8_t value =
            tile16_data[(i % 8 * kTile16Size) + (i / 8 * kTile16Size * 0x80) +
                        (ty * 0x80) + tx];
        tile_data[position] = value;
      }
    }

    // Add the vector for the current tile to the vector of tile pixel data
    tile16_individual_data_.push_back(tile_data);
    tile16_individual_.emplace_back();
    RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
        kTile16Size, kTile16Size, 0x80, tile16_individual_data_[i],
        tile16_individual_[i], palette_));
  }

  // Render the overworld maps loaded from the ROM.
  for (int i = 0; i < zelda3::overworld::kNumOverworldMaps; ++i) {
    overworld_.set_current_map(i);
    auto palette = overworld_.current_area_palette();
    RETURN_IF_ERROR(Renderer::GetInstance().CreateAndRenderBitmap(
        kOverworldMapSize, kOverworldMapSize, 0x200,
        overworld_.current_map_bitmap_data(), maps_bmp_[i], palette));
  }

  if (flags()->overworld.kDrawOverworldSprites) {
    RETURN_IF_ERROR(LoadSpriteGraphics());
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  for (int i = 0; i < 3; i++)
    for (auto const &sprite : overworld_.sprites(i)) {
      int width = sprite.width();
      int height = sprite.height();
      int depth = 0x10;
      auto spr_gfx = sprite.PreviewGraphics();
      if (spr_gfx.empty() || width == 0 || height == 0) {
        continue;
      }
      sprite_previews_[sprite.id()].Create(width, height, depth, spr_gfx);
      RETURN_IF_ERROR(sprite_previews_[sprite.id()].ApplyPalette(palette_));
      Renderer::GetInstance().RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

void OverworldEditor::RefreshChildMap(int map_index) {
  overworld_.mutable_overworld_map(map_index)->LoadAreaGraphics();
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTileset();
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildTiles16Gfx(
      *overworld_.mutable_tiles16(), overworld_.tiles16().size());
  PRINT_IF_ERROR(status_);
  status_ = overworld_.mutable_overworld_map(map_index)->BuildBitmap(
      overworld_.GetMapTiles(current_world_));
  maps_bmp_[map_index].set_data(
      overworld_.mutable_overworld_map(map_index)->bitmap_data());
  maps_bmp_[map_index].set_modified(true);
  PRINT_IF_ERROR(status_);
}

void OverworldEditor::RefreshOverworldMap() {
  std::vector<std::future<void>> futures;
  int indices[4];

  auto refresh_map_async = [this](int map_index) {
    RefreshChildMap(map_index);
  };

  int source_map_id = current_map_;
  bool is_large = overworld_.overworld_map(current_map_)->is_large_map();
  if (is_large) {
    source_map_id = current_parent_;
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(source_map_id)->parent() + i;
      if (i >= 2) sibling_index += 6;
      futures.push_back(
          std::async(std::launch::async, refresh_map_async, sibling_index));
      indices[i] = sibling_index;
    }
  }
  indices[0] = source_map_id;
  futures.push_back(
      std::async(std::launch::async, refresh_map_async, source_map_id));

  for (auto &each : futures) {
    each.get();
  }
  int n = is_large ? 4 : 1;
  // We do texture updating on the main thread
  for (int i = 0; i < n; ++i) {
    Renderer::GetInstance().UpdateBitmap(&maps_bmp_[indices[i]]);
  }
}

absl::Status OverworldEditor::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_.mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_.current_area_palette();

  if (overworld_.overworld_map(current_map_)->is_large_map()) {
    // We need to update the map and its siblings if it's a large map
    for (int i = 1; i < 4; i++) {
      int sibling_index = overworld_.overworld_map(current_map_)->parent() + i;
      if (i >= 2) sibling_index += 6;
      RETURN_IF_ERROR(
          overworld_.mutable_overworld_map(sibling_index)->LoadPalette());
      RETURN_IF_ERROR(
          maps_bmp_[sibling_index].ApplyPalette(current_map_palette));
    }
  }

  RETURN_IF_ERROR(maps_bmp_[current_map_].ApplyPalette(current_map_palette));
  return absl::OkStatus();
}

void OverworldEditor::RefreshMapProperties() {
  auto &current_ow_map = *overworld_.mutable_overworld_map(current_map_);
  if (current_ow_map.is_large_map()) {
    // We need to copy the properties from the parent map to the children
    for (int i = 1; i < 4; i++) {
      int sibling_index = current_ow_map.parent() + i;
      if (i >= 2) {
        sibling_index += 6;
      }
      auto &map = *overworld_.mutable_overworld_map(sibling_index);
      map.set_area_graphics(current_ow_map.area_graphics());
      map.set_area_palette(current_ow_map.area_palette());
      map.set_sprite_graphics(game_state_,
                              current_ow_map.sprite_graphics(game_state_));
      map.set_sprite_palette(game_state_,
                             current_ow_map.sprite_palette(game_state_));
      map.set_message_id(current_ow_map.message_id());
    }
  }
}

absl::Status OverworldEditor::RefreshTile16Blockset() {
  if (current_blockset_ ==
      overworld_.overworld_map(current_map_)->area_graphics()) {
    return absl::OkStatus();
  }
  current_blockset_ = overworld_.overworld_map(current_map_)->area_graphics();

  overworld_.set_current_map(current_map_);
  palette_ = overworld_.current_area_palette();
  // Create the tile16 blockset image
  Renderer::GetInstance().UpdateBitmap(&tile16_blockset_bmp_);
  RETURN_IF_ERROR(tile16_blockset_bmp_.ApplyPalette(palette_));

  // Copy the tile16 data into individual tiles.
  const auto tile16_data = overworld_.tile16_blockset_data();

  std::vector<std::future<void>> futures;
  // Loop through the tiles and copy their pixel data into separate vectors
  for (uint i = 0; i < zelda3::overworld::kNumTile16Individual; i++) {
    futures.push_back(std::async(
        std::launch::async,
        [&](int index) {
          std::vector<uint8_t> tile_data(16 * 16, 0x00);
          for (int ty = 0; ty < 16; ty++) {
            for (int tx = 0; tx < 16; tx++) {
              int position = tx + (ty * 0x10);
              uint8_t value =
                  tile16_data[(index % 8 * 16) + (index / 8 * 16 * 0x80) +
                              (ty * 0x80) + tx];
              tile_data[position] = value;
            }
          }
          tile16_individual_[index].set_data(tile_data);
        },
        i));
  }

  for (auto &future : futures) {
    future.get();
  }

  // Render the bitmaps of each tile.
  for (uint id = 0; id < zelda3::overworld::kNumTile16Individual; id++) {
    RETURN_IF_ERROR(tile16_individual_[id].ApplyPalette(palette_));
    Renderer::GetInstance().UpdateBitmap(&tile16_individual_[id]);
  }

  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldProperties() {
  static bool init_properties = false;

  if (!init_properties) {
    for (int i = 0; i < 0x40; i++) {
      std::string area_graphics_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->area_graphics());
      properties_canvas_.mutable_labels(OverworldProperty::LW_AREA_GFX)
          ->push_back(area_graphics_str);

      area_graphics_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->area_graphics());
      properties_canvas_.mutable_labels(OverworldProperty::DW_AREA_GFX)
          ->push_back(area_graphics_str);

      std::string area_palette_str =
          absl::StrFormat("%02hX", overworld_.overworld_map(i)->area_palette());
      properties_canvas_.mutable_labels(OverworldProperty::LW_AREA_PAL)
          ->push_back(area_palette_str);

      area_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->area_palette());
      properties_canvas_.mutable_labels(OverworldProperty::DW_AREA_PAL)
          ->push_back(area_palette_str);
      std::string sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_graphics(1));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_graphics(2));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_graphics(1));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_graphics(2));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      std::string sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_palette(1));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i)->sprite_palette(2));
      properties_canvas_.mutable_labels(OverworldProperty::LW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_palette(1));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", overworld_.overworld_map(i + 0x40)->sprite_palette(2));
      properties_canvas_.mutable_labels(OverworldProperty::DW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);
    }
    init_properties = true;
  }

  Text("Area Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::LW_AREA_GFX);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  Text("Sprite Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::LW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::DW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::LW_SPR_GFX_PART2);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::DW_SPR_GFX_PART2);
  ImGui::Separator();

  Text("Area Pal LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::LW_AREA_PAL);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 8, 2.0f, 32,
                                    OverworldProperty::DW_AREA_PAL);

  static bool show_gfx_group = false;
  Checkbox("Show Gfx Group Editor", &show_gfx_group);
  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    status_ = gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }
}

absl::Status OverworldEditor::UpdateUsageStats() {
  if (BeginTable("UsageStatsTable", 3, kOWEditFlags, ImVec2(0, 0))) {
    TableSetupColumn("Entrances");
    TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    if (BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
                   ImGuiWindowFlags_HorizontalScrollbar)) {
      for (int i = 0; i < 0x81; i++) {
        auto entrance_name = rom()->resource_label()->GetLabel(
            "Dungeon Entrance Names", core::UppercaseHexByte(i));
        std::string str = absl::StrFormat("%#x - %s", i, entrance_name);
        if (Selectable(str.c_str(), selected_entrance_ == i,
                       overworld_.entrances().at(i).deleted
                           ? ImGuiSelectableFlags_Disabled
                           : 0)) {
          selected_entrance_ = i;
          selected_usage_map_ = overworld_.entrances().at(i).map_id_;
          properties_canvas_.set_highlight_tile_id(selected_usage_map_);
        }
        if (IsItemHovered()) {
          BeginTooltip();
          Text("Entrance ID: %d", i);
          Text("Map ID: %d", overworld_.entrances().at(i).map_id_);
          Text("Entrance ID: %d", overworld_.entrances().at(i).entrance_id_);
          Text("X: %d", overworld_.entrances().at(i).x_);
          Text("Y: %d", overworld_.entrances().at(i).y_);
          Text("Deleted? %s",
               overworld_.entrances().at(i).deleted ? "Yes" : "No");
          EndTooltip();
        }
      }
      EndChild();
    }

    TableNextColumn();
    DrawUsageGrid();

    TableNextColumn();
    DrawOverworldProperties();

    EndTable();
  }
  return absl::OkStatus();
}

void OverworldEditor::DrawUsageGrid() {
  // Create a grid of 8x8 squares
  int totalSquares = 128;
  int squaresWide = 8;
  int squaresTall = (totalSquares + squaresWide - 1) /
                    squaresWide;  // Ceiling of totalSquares/squaresWide

  // Loop through each row
  for (int row = 0; row < squaresTall; ++row) {
    NewLine();

    for (int col = 0; col < squaresWide; ++col) {
      if (row * squaresWide + col >= totalSquares) {
        break;
      }
      // Determine if this square should be highlighted
      bool highlight = selected_usage_map_ == (row * squaresWide + col);

      // Set highlight color if needed
      if (highlight) {
        PushStyleColor(ImGuiCol_Button,
                       ImVec4(1.0f, 0.5f, 0.0f,
                              1.0f));  // Or any highlight color
      }

      // Create a button or selectable for each square
      if (Button("##square", ImVec2(20, 20))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }

      // Reset style if it was highlighted
      if (highlight) {
        PopStyleColor();
      }

      // Check if the square is hovered
      if (IsItemHovered()) {
        // Display a tooltip with all the room properties
      }

      // Keep squares in the same line
      SameLine();
    }
  }
}

void OverworldEditor::DrawDebugWindow() {
  Text("Current Map: %d", current_map_);
  Text("Current Tile16: %d", current_tile16_);
  int relative_x = (int)ow_map_canvas_.drawn_tile_position().x % 512;
  int relative_y = (int)ow_map_canvas_.drawn_tile_position().y % 512;
  Text("Current Tile16 Drawn Position (Relative): %d, %d", relative_x,
       relative_y);

  // Print the size of the overworld map_tiles per world
  Text("Light World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->light_world.size());
  Text("Dark World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->dark_world.size());
  Text("Special World Map Tiles: %d",
       (int)overworld_.mutable_map_tiles()->special_world.size());

  static bool view_lw_map_tiles = false;
  static MemoryEditor mem_edit;
  // Let's create buttons which let me view containers in the memory editor
  if (Button("View Light World Map Tiles")) {
    view_lw_map_tiles = !view_lw_map_tiles;
  }

  if (view_lw_map_tiles) {
    mem_edit.DrawContents(
        overworld_.mutable_map_tiles()->light_world[current_map_].data(),
        overworld_.mutable_map_tiles()->light_world[current_map_].size());
  }
}

void OverworldEditor::InitializeZeml() {
  // Load zeml string from layouts/overworld.zeml
  std::string layout = gui::zeml::LoadFile("overworld.zeml");
  // Parse the zeml string into a Node object
  layout_node_ = gui::zeml::Parse(layout);

  gui::zeml::Bind(&*layout_node_.GetNode("OverworldCanvas"),
                  [this]() { DrawOverworldCanvas(); });
  gui::zeml::Bind(&*layout_node_.GetNode("OverworldTileSelector"),
                  [this]() { status_ = DrawTileSelector(); });
  gui::zeml::Bind(&*layout_node_.GetNode("OwUsageStats"), [this]() {
    if (rom()->is_loaded()) {
      status_ = UpdateUsageStats();
    }
  });
  gui::zeml::Bind(&*layout_node_.GetNode("owToolset"),
                  [this]() { DrawToolset(); });
  gui::zeml::Bind(&*layout_node_.GetNode("OwTile16Editor"), [this]() {
    if (rom()->is_loaded()) {
      status_ = tile16_editor_.Update();
    }
  });
  gui::zeml::Bind(&*layout_node_.GetNode("OwGfxGroupEditor"), [this]() {
    if (rom()->is_loaded()) {
      status_ = gfx_group_editor_.Update();
    }
  });
}

}  // namespace editor
}  // namespace yaze
