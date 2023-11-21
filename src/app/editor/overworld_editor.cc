#include "overworld_editor.h"

#include <imgui/imgui.h>

#include <cmath>
#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/core/pipeline.h"
#include "app/editor/resources/palette_editor.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_tile.h"
#include "app/gui/canvas.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/style.h"
#include "app/gui/widgets.h"
#include "app/rom.h"
#include "app/zelda3/overworld.h"

namespace yaze {
namespace app {
namespace editor {

using ImGui::Separator;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

absl::Status OverworldEditor::Update() {
  if (rom()->isLoaded() && !all_gfx_loaded_) {
    // Initialize overworld graphics, maps, and palettes
    RETURN_IF_ERROR(LoadGraphics())
    RETURN_IF_ERROR(tile16_editor_.InitBlockset(
        tile16_blockset_bmp_, tile16_individual_, tile8_individual_));
    gfx_group_editor_.InitBlockset(tile16_blockset_bmp_);
    all_gfx_loaded_ = true;
  } else if (!rom()->isLoaded() && all_gfx_loaded_) {
    // Reset the editor if the ROM is unloaded
    Shutdown();
    all_gfx_loaded_ = false;
    map_blockset_loaded_ = false;
  }

  // Draws the toolset for editing the Overworld.
  RETURN_IF_ERROR(DrawToolset())

  Separator();
  if (ImGui::BeginTable(kOWEditTable.data(), 2, kOWEditFlags, ImVec2(0, 0))) {
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Tile Selector", ImGuiTableColumnFlags_WidthFixed, 256);
    TableHeadersRow();
    TableNextRow();
    TableNextColumn();
    DrawOverworldCanvas();
    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }

  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::DrawToolset() {
  static bool show_gfx_group = false;
  static bool show_tile16 = false;

  if (ImGui::BeginTable("OWToolset", 19, kToolsetTableFlags, ImVec2(0, 0))) {
    for (const auto &name : kToolsetColumnNames)
      ImGui::TableSetupColumn(name.data());

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_UNDO)) {
      RETURN_IF_ERROR(Undo())
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_REDO)) {
      RETURN_IF_ERROR(Redo())
    }

    TEXT_COLUMN(ICON_MD_MORE_VERT)   // Separator
    BUTTON_COLUMN(ICON_MD_ZOOM_OUT)  // Zoom Out
    BUTTON_COLUMN(ICON_MD_ZOOM_IN)   // Zoom In
    TEXT_COLUMN(ICON_MD_MORE_VERT)   // Separator

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_DRAW)) {
      current_mode = EditingMode::DRAW_TILE;
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_DOOR_FRONT)) {
      current_mode = EditingMode::ENTRANCES;
    }

    NEXT_COLUMN()
    if (ImGui::Button(ICON_MD_DOOR_BACK)) {
      current_mode = EditingMode::EXITS;
    }

    BUTTON_COLUMN(ICON_MD_GRASS)                // Items
    BUTTON_COLUMN(ICON_MD_PEST_CONTROL_RODENT)  // Sprites
    BUTTON_COLUMN(ICON_MD_ADD_LOCATION)         // Transports
    BUTTON_COLUMN(ICON_MD_MUSIC_NOTE)           // Music

    TableNextColumn();
    if (ImGui::Button(ICON_MD_GRID_VIEW)) {
      show_tile16 = !show_tile16;
    }

    TableNextColumn();
    if (ImGui::Button(ICON_MD_TABLE_CHART)) {
      show_gfx_group = !show_gfx_group;
    }

    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator

    TableNextColumn();  // Palette
    palette_editor_.DisplayPalette(palette_, overworld_.isLoaded());
    TEXT_COLUMN(ICON_MD_MORE_VERT)  // Separator
    TableNextColumn();              // Experimental
    ImGui::Checkbox("Experimental", &show_experimental);

    ImGui::EndTable();
  }

  if (show_experimental) {
    RETURN_IF_ERROR(DrawExperimentalModal())
  }

  if (show_tile16) {
    // Create a table in ImGui for the Tile16 Editor
    ImGui::Begin("Tile16 Editor", &show_tile16);
    RETURN_IF_ERROR(tile16_editor_.Update())
    ImGui::End();
  }

  if (show_gfx_group) {
    ImGui::Begin("Gfx Group Editor", &show_gfx_group);
    RETURN_IF_ERROR(gfx_group_editor_.Update())
    ImGui::End();
  }
  return absl::OkStatus();
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldMapSettings() {
  if (ImGui::BeginTable(kOWMapTable.data(), 8, kOWMapFlags, ImVec2(0, 0), -1)) {
    for (const auto &name : kOverworldSettingsColumnNames)
      ImGui::TableSetupColumn(name.data());

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    ImGui::Combo("##world", &current_world_, kWorldList.data(), 3);

    TableNextColumn();
    Text("GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapGFX", map_gfx_, kByteSize);

    TableNextColumn();
    Text("Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##mapPal", map_palette_, kByteSize);

    TableNextColumn();
    Text("Spr GFX");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprGFX", spr_gfx_, kByteSize);

    TableNextColumn();
    Text("Spr Palette");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kInputFieldSize);
    ImGui::InputText("##sprPal", spr_palette_, kByteSize);

    TableNextColumn();
    Text("Msg ID");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50.f);
    ImGui::InputText("##msgid", spr_palette_, kMessageIdSize);

    TableNextColumn();
    ImGui::SetNextItemWidth(100.f);
    // ImGui::Combo("##world", &game_state_, "Part 0\0Part 1\0Part 2\0", 3);
    ImGui::Combo("##World", &game_state_, kGamePartComboString, 3);

    TableNextColumn();
    ImGui::Checkbox("Show grid", &opt_enable_grid);  // TODO
    ImGui::EndTable();
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldEntrances(ImVec2 canvas_p0,
                                             ImVec2 scrolling) {
  for (auto &each : overworld_.Entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40)) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              ImVec4(210, 24, 210, 150));
      std::string str = absl::StrFormat("%#x", each.entrance_id_);
      ow_map_canvas_.DrawText(str, each.x_ - 4, each.y_ - 2);

      // Check if this entrance is being clicked and dragged
      if (IsMouseHoveringOverEntrance(each, canvas_p0, scrolling) &&
          ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        dragged_entrance_ = &each;
        is_dragging_entrance_ = true;
        if (ImGui::BeginDragDropSource()) {
          ImGui::SetDragDropPayload("ENTRANCE_PAYLOAD", &each,
                                    sizeof(zelda3::OverworldEntrance));
          Text("Moving Entrance ID: %s", str.c_str());
          ImGui::EndDragDropSource();
        }
      } else if (is_dragging_entrance_ && dragged_entrance_ == &each &&
                 ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        // Adjust the X and Y position of the entrance rectangle based on the
        // mouse position when it is released after being dragged
        const ImGuiIO &io = ImGui::GetIO();
        const ImVec2 origin(canvas_p0.x + scrolling.x,
                            canvas_p0.y + scrolling.y);
        dragged_entrance_->x_ = io.MousePos.x - origin.x - 8;
        dragged_entrance_->y_ = io.MousePos.y - origin.y - 8;
        is_dragging_entrance_ = false;
      }
    }
  }
}

bool OverworldEditor::IsMouseHoveringOverEntrance(
    const zelda3::OverworldEntrance &entrance, ImVec2 canvas_p0,
    ImVec2 scrolling) {
  // Get the mouse position relative to the canvas
  const ImGuiIO &io = ImGui::GetIO();
  const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y);
  const ImVec2 mouse_pos(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

  // Check if the mouse is hovering over the entrance
  if (mouse_pos.x >= entrance.x_ && mouse_pos.x <= entrance.x_ + 16 &&
      mouse_pos.y >= entrance.y_ && mouse_pos.y <= entrance.y_ + 16) {
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);
    int map_x = (xx * 0x200);
    int map_y = (yy * 0x200);
    ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y);
    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
    }
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldSprites() {
  for (const auto &sprite : overworld_.Sprites(game_state_)) {
    // Get the sprite's bitmap and real X and Y positions
    auto id = sprite.id();
    const gfx::Bitmap &sprite_bitmap = sprite_previews_[id];
    int realX = sprite.GetRealX();
    int realY = sprite.GetRealY();

    // Draw the sprite's bitmap onto the canvas at its real X and Y positions
    ow_map_canvas_.DrawBitmap(sprite_bitmap, realX, realY);
    ow_map_canvas_.DrawRect(realX, realY, sprite.Width(), sprite.Height(),
                            ImVec4(255, 0, 0, 150));
    std::string str = absl::StrFormat("%s", sprite.Name());
    ow_map_canvas_.DrawText(str, realX - 4, realY - 2);
  }
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawOverworldEdits() {
  auto mouse_position = ow_map_canvas_.GetCurrentDrawnTilePosition();
  auto canvas_size = ow_map_canvas_.GetCanvasSize();
  int x = mouse_position.x / canvas_size.x;
  int y = mouse_position.y / canvas_size.y;
  auto index = x + (y * 8);

  // Determine which overworld map the user is currently editing.
  DetermineActiveMap(mouse_position);

  // Render the updated map bitmap.
  RenderUpdatedMapBitmap(mouse_position,
                         tile16_individual_data_[current_tile16_]);

  // Queue up the raw ROM changes.
  QueueROMChanges(index, current_tile16_);
}

void OverworldEditor::DetermineActiveMap(const ImVec2 &mouse_position) {
  // Assuming each small map is 256x256 pixels (adjust as needed)
  constexpr int small_map_size = 512;

  // Calculate which small map the mouse is currently over
  int map_x = mouse_position.x / small_map_size;
  int map_y = mouse_position.y / small_map_size;

  // Calculate the index of the map in the `maps_bmp_` vector
  current_map_ = map_x + map_y * 8;
}

void OverworldEditor::RenderUpdatedMapBitmap(const ImVec2 &click_position,
                                             const Bytes &tile_data) {
  // Calculate the tile position relative to the current active map
  constexpr int tile_size = 16;  // Tile size is 16x16 pixels

  // Calculate the tile index for x and y based on the click_position
  int tile_index_x = (static_cast<int>(click_position.x) % 512) / tile_size;
  int tile_index_y = (static_cast<int>(click_position.y) % 512) / tile_size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = tile_index_x * tile_size;
  start_position.y = tile_index_y * tile_size;

  // Get the current map's bitmap from the BitmapTable
  gfx::Bitmap &current_bitmap = maps_bmp_[current_map_];

  // Update the bitmap's pixel data based on the start_position and tile_data
  for (int y = 0; y < tile_size; ++y) {
    for (int x = 0; x < tile_size; ++x) {
      int pixel_index = (start_position.y + y) * current_bitmap.width() +
                        (start_position.x + x);
      current_bitmap.WriteToPixel(pixel_index, tile_data[y * tile_size + x]);
    }
  }

  // Render the updated bitmap to the canvas
  rom()->RenderBitmap(&current_bitmap);
}

void OverworldEditor::QueueROMChanges(int index, ushort new_tile16) {
  // Store the changes made by the user to the ROM (or project file)
  rom()->QueueChanges([&]() {
    PRINT_IF_ERROR(overworld_.SaveOverworldMaps());
    if (!overworld_.CreateTile32Tilemap()) {
      // overworld_.SaveMap16Tiles();
      PRINT_IF_ERROR(overworld_.SaveMap32Tiles());
    } else {
      std::cout << "Failed to create tile32 tilemap" << std::endl;
    }
  });
}

// ----------------------------------------------------------------------------

void OverworldEditor::CheckForOverworldEdits() {
  if (!blockset_canvas_.Points().empty()) {
    // User has selected a tile they want to draw from the blockset.
    int x = blockset_canvas_.Points().front().x / 32;
    int y = blockset_canvas_.Points().front().y / 32;
    current_tile16_ = x + (y * 8);
    if (ow_map_canvas_.DrawTilePainter(tile16_individual_[current_tile16_],
                                       16)) {
      // Update the overworld map.
      DrawOverworldEdits();
    }
  }
}

// Overworld Editor canvas
// Allows the user to make changes to the overworld map.
void OverworldEditor::DrawOverworldCanvas() {
  DrawOverworldMapSettings();
  Separator();
  if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)7);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar)) {
    ow_map_canvas_.DrawBackground(ImVec2(0x200 * 8, 0x200 * 8));
    ow_map_canvas_.DrawContextMenu();
    if (overworld_.isLoaded()) {
      DrawOverworldMaps();
      DrawOverworldEntrances(ow_map_canvas_.GetZeroPoint(),
                             ow_map_canvas_.Scrolling());
      if (flags()->kDrawOverworldSprites) {
        DrawOverworldSprites();
      }
      CheckForOverworldEdits();
    }
    ow_map_canvas_.DrawGrid(64.0f);
    ow_map_canvas_.DrawOverlay();
  }
  ImGui::EndChild();
}

// ----------------------------------------------------------------------------

// Tile 8 Selector
// Displays all the individual tiles that make up a tile16.
void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground(
      ImVec2(0x100 + 1, kNumSheetsToLoad * 0x40 + 1));
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    // for (const auto &[key, value] : graphics_bin_) {
    for (auto &[key, value] : rom()->bitmap_manager()) {
      int offset = 0x40 * (key + 1);
      int top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 2;
      if (key >= 1) {
        top_left_y = graphics_bin_canvas_.GetZeroPoint().y + 0x40 * key;
      }
      auto texture = value.get()->texture();
      graphics_bin_canvas_.GetDrawList()->AddImage(
          (void *)texture,
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 2, top_left_y),
          ImVec2(graphics_bin_canvas_.GetZeroPoint().x + 0x100,
                 graphics_bin_canvas_.GetZeroPoint().y + offset));
    }
  }
  graphics_bin_canvas_.DrawGrid(16.0f);
  graphics_bin_canvas_.DrawOverlay();
}

// ----------------------------------------------------------------------------

void OverworldEditor::DrawTileSelector() {
  if (ImGui::BeginTabBar(kTileSelectorTab.data(),
                         ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Tile16")) {
      core::BitmapCanvasPipeline(blockset_canvas_, tile16_blockset_bmp_, 0x100,
                                 (8192 * 2), 0x20, map_blockset_loaded_, true,
                                 1);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Tile8")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)1);
          ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                            ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawTile8Selector();
      }
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Area Graphics")) {
      core::BitmapCanvasPipeline(current_gfx_canvas_, current_gfx_bmp_, 256,
                                 0x10 * 0x40, 0x20, overworld_.isLoaded(), true,
                                 3);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

// ----------------------------------------------------------------------------

absl::Status OverworldEditor::LoadGraphics() {
  // Load all of the graphics data from the game.
  PRINT_IF_ERROR(rom()->LoadAllGraphicsData())
  graphics_bin_ = rom()->graphics_bin();

  // Load the Link to the Past overworld.
  RETURN_IF_ERROR(overworld_.Load(*rom()))
  palette_ = overworld_.AreaPalette();

  // Create the area graphics image
  core::BuildAndRenderBitmapPipeline(0x80, 0x200, 0x40,
                                     overworld_.AreaGraphics(), *rom(),
                                     current_gfx_bmp_, palette_);

  // Create the tile16 blockset image
  core::BuildAndRenderBitmapPipeline(0x80, 0x2000, 0x80,
                                     overworld_.Tile16Blockset(), *rom(),
                                     tile16_blockset_bmp_, palette_);
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_data = overworld_.Tile16Blockset();

  std::cout << tile16_data.size() << std::endl;

  // Loop through the tiles and copy their pixel data into separate vectors
  for (int i = 0; i < 4096; i++) {
    // Create a new vector for the pixel data of the current tile
    Bytes tile_data(16 * 16, 0x00);  // More efficient initialization

    // Copy the pixel data for the current tile into the vector
    for (int ty = 0; ty < 16; ty++) {
      for (int tx = 0; tx < 16; tx++) {
        int position = tx + (ty * 0x10);
        uchar value =
            tile16_data[(i % 8 * 16) + (i / 8 * 16 * 0x80) + (ty * 0x80) + tx];
        tile_data[position] = value;
      }
    }

    // Add the vector for the current tile to the vector of tile pixel data
    tile16_individual_data_.push_back(tile_data);
  }

  // Render the bitmaps of each tile.
  for (int id = 0; id < 4096; id++) {
    tile16_individual_.emplace_back();
    core::BuildAndRenderBitmapPipeline(0x10, 0x10, 0x80,
                                       tile16_individual_data_[id], *rom(),
                                       tile16_individual_[id], palette_);
  }

  // Render the overworld maps loaded from the ROM.
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    overworld_.SetCurrentMap(i);
    auto palette = overworld_.AreaPalette();
    core::BuildAndRenderBitmapPipeline(0x200, 0x200, 0x200,
                                       overworld_.BitmapData(), *rom(),
                                       maps_bmp_[i], palette);
  }

  if (flags()->kDrawOverworldSprites) {
    RETURN_IF_ERROR(LoadSpriteGraphics());
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  for (int i = 0; i < 3; i++)
    for (auto const &sprite : overworld_.Sprites(i)) {
      int width = sprite.Width();
      int height = sprite.Height();
      int depth = 0x40;
      auto spr_gfx = sprite.PreviewGraphics();
      sprite_previews_[sprite.id()].Create(width, height, depth, spr_gfx);
      sprite_previews_[sprite.id()].ApplyPalette(palette_);
      rom()->RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

absl::Status OverworldEditor::DrawExperimentalModal() {
  ImGui::Begin("Experimental", &show_experimental);

  gui::TextWithSeparators("PROTOTYPE OVERWORLD TILEMAP LOADER");
  Text("Please provide two files:");
  Text("One based on MAPn.DAT, which represents the overworld tilemap");
  Text("One based on MAPDATn.DAT, which is the tile32 configurations.");
  Text("Currently, loading CGX for this component is NOT supported. ");
  Text("Please load a US ROM of LTTP (JP ROM support coming soon).");
  Text(
      "Once you've loaded the files, you can click the button below to load "
      "the tilemap into the editor");

  ImGui::InputText("##TilemapFile", &ow_tilemap_filename_);
  ImGui::SameLine();
  core::FileDialogPipeline(
      "ImportTilemapsKey", ".DAT,.dat\0", "Tilemap Hex File", [this]() {
        ow_tilemap_filename_ = ImGuiFileDialog::Instance()->GetFilePathName();
      });

  ImGui::InputText("##Tile32ConfigurationFile",
                   &tile32_configuration_filename_);
  ImGui::SameLine();
  core::FileDialogPipeline("ImportTile32Key", ".DAT,.dat\0", "Tile32 Hex File",
                           [this]() {
                             tile32_configuration_filename_ =
                                 ImGuiFileDialog::Instance()->GetFilePathName();
                           });

  if (ImGui::Button("Load Prototype Overworld with ROM graphics")) {
    RETURN_IF_ERROR(LoadGraphics())
    all_gfx_loaded_ = true;
  }

  gui::TextWithSeparators("Configuration");

  gui::InputHexShort("Tilemap File Offset (High)", &tilemap_file_offset_high_);
  gui::InputHexShort("Tilemap File Offset (Low)", &tilemap_file_offset_low_);

  gui::InputHexShort("LW Maps to Load", &light_maps_to_load_);
  gui::InputHexShort("DW Maps to Load", &dark_maps_to_load_);
  gui::InputHexShort("SP Maps to Load", &sp_maps_to_load_);

  ImGui::End();
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze