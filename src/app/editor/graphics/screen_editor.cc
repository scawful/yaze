#include "screen_editor.h"
#include "app/editor/system/editor_card_registry.h"

#include <fstream>
#include <iostream>
#include <string>

#include "absl/strings/str_format.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

constexpr uint32_t kRedPen = 0xFF0000FF;

void ScreenEditor::Initialize() {
  if (!dependencies_.card_registry)
    return;
  auto* card_registry = dependencies_.card_registry;

  card_registry->RegisterCard({.card_id = "screen.dungeon_maps",
                               .display_name = "Dungeon Maps",
                               .icon = ICON_MD_MAP,
                               .category = "Screen",
                               .shortcut_hint = "Alt+1",
                               .priority = 10});
  card_registry->RegisterCard({.card_id = "screen.inventory_menu",
                               .display_name = "Inventory Menu",
                               .icon = ICON_MD_INVENTORY,
                               .category = "Screen",
                               .shortcut_hint = "Alt+2",
                               .priority = 20});
  card_registry->RegisterCard({.card_id = "screen.overworld_map",
                               .display_name = "Overworld Map",
                               .icon = ICON_MD_PUBLIC,
                               .category = "Screen",
                               .shortcut_hint = "Alt+3",
                               .priority = 30});
  card_registry->RegisterCard({.card_id = "screen.title_screen",
                               .display_name = "Title Screen",
                               .icon = ICON_MD_TITLE,
                               .category = "Screen",
                               .shortcut_hint = "Alt+4",
                               .priority = 40});
  card_registry->RegisterCard({.card_id = "screen.naming_screen",
                               .display_name = "Naming Screen",
                               .icon = ICON_MD_EDIT,
                               .category = "Screen",
                               .shortcut_hint = "Alt+5",
                               .priority = 50});

  // Show title screen by default
  card_registry->ShowCard("screen.title_screen");
}

absl::Status ScreenEditor::Load() {
  gfx::ScopedTimer timer("ScreenEditor::Load");

  ASSIGN_OR_RETURN(dungeon_maps_,
                   zelda3::LoadDungeonMaps(*rom(), dungeon_map_labels_));
  RETURN_IF_ERROR(zelda3::LoadDungeonMapTile16(
      tile16_blockset_, *rom(), rom()->graphics_buffer(), false));

  // Load graphics sheets and apply dungeon palette
  sheets_.try_emplace(0, gfx::Arena::Get().gfx_sheets()[212]);
  sheets_.try_emplace(1, gfx::Arena::Get().gfx_sheets()[213]);
  sheets_.try_emplace(2, gfx::Arena::Get().gfx_sheets()[214]);
  sheets_.try_emplace(3, gfx::Arena::Get().gfx_sheets()[215]);

  // Apply dungeon palette to all sheets
  for (int i = 0; i < 4; i++) {
    sheets_[i].SetPalette(*rom()->mutable_dungeon_palette(3));
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &sheets_[i]);
  }

  // Create a single tilemap for tile8 graphics with on-demand texture creation
  // Combine all 4 sheets (128x32 each) into one bitmap (128x128)
  // This gives us 16 tiles per row × 16 rows = 256 tiles total
  const int tile8_width = 128;
  const int tile8_height = 128;  // 4 sheets × 32 pixels each
  std::vector<uint8_t> tile8_data(tile8_width * tile8_height);

  // Copy data from all 4 sheets into the combined bitmap
  for (int sheet_idx = 0; sheet_idx < 4; sheet_idx++) {
    const auto& sheet = sheets_[sheet_idx];
    int dest_y_offset = sheet_idx * 32;  // Each sheet is 32 pixels tall

    for (int y = 0; y < 32; y++) {
      for (int x = 0; x < 128; x++) {
        int src_index = y * 128 + x;
        int dest_index = (dest_y_offset + y) * 128 + x;

        if (src_index < sheet.size() && dest_index < tile8_data.size()) {
          tile8_data[dest_index] = sheet.data()[src_index];
        }
      }
    }
  }

  // Create tilemap with 8x8 tile size
  tile8_tilemap_.tile_size = {8, 8};
  tile8_tilemap_.map_size = {256, 256};  // Logical size for tile count
  tile8_tilemap_.atlas.Create(tile8_width, tile8_height, 8, tile8_data);
  tile8_tilemap_.atlas.SetPalette(*rom()->mutable_dungeon_palette(3));

  // Queue single texture creation for the atlas (not individual tiles)
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tile8_tilemap_.atlas);
  return absl::OkStatus();
}

absl::Status ScreenEditor::Update() {
  if (!dependencies_.card_registry)
    return absl::OkStatus();
  auto* card_registry = dependencies_.card_registry;

  static gui::EditorCard dungeon_maps_card("Dungeon Maps", ICON_MD_MAP);
  static gui::EditorCard inventory_menu_card("Inventory Menu",
                                             ICON_MD_INVENTORY);
  static gui::EditorCard overworld_map_card("Overworld Map", ICON_MD_PUBLIC);
  static gui::EditorCard title_screen_card("Title Screen", ICON_MD_TITLE);
  static gui::EditorCard naming_screen_card("Naming Screen",
                                            ICON_MD_EDIT_ATTRIBUTES);

  dungeon_maps_card.SetDefaultSize(800, 600);
  inventory_menu_card.SetDefaultSize(800, 600);
  overworld_map_card.SetDefaultSize(600, 500);
  title_screen_card.SetDefaultSize(600, 500);
  naming_screen_card.SetDefaultSize(500, 400);

  // Dungeon Maps Card - Check visibility flag exists and is true before rendering
  bool* dungeon_maps_visible =
      card_registry->GetVisibilityFlag("screen.dungeon_maps");
  if (dungeon_maps_visible && *dungeon_maps_visible) {
    if (dungeon_maps_card.Begin(dungeon_maps_visible)) {
      DrawDungeonMapsEditor();
    }
    dungeon_maps_card.End();
  }

  // Inventory Menu Card - Check visibility flag exists and is true before rendering
  bool* inventory_menu_visible =
      card_registry->GetVisibilityFlag("screen.inventory_menu");
  if (inventory_menu_visible && *inventory_menu_visible) {
    if (inventory_menu_card.Begin(inventory_menu_visible)) {
      DrawInventoryMenuEditor();
    }
    inventory_menu_card.End();
  }

  // Overworld Map Card - Check visibility flag exists and is true before rendering
  bool* overworld_map_visible =
      card_registry->GetVisibilityFlag("screen.overworld_map");
  if (overworld_map_visible && *overworld_map_visible) {
    if (overworld_map_card.Begin(overworld_map_visible)) {
      DrawOverworldMapEditor();
    }
    overworld_map_card.End();
  }

  // Title Screen Card - Check visibility flag exists and is true before rendering
  bool* title_screen_visible =
      card_registry->GetVisibilityFlag("screen.title_screen");
  if (title_screen_visible && *title_screen_visible) {
    if (title_screen_card.Begin(title_screen_visible)) {
      DrawTitleScreenEditor();
    }
    title_screen_card.End();
  }

  // Naming Screen Card - Check visibility flag exists and is true before rendering
  bool* naming_screen_visible =
      card_registry->GetVisibilityFlag("screen.naming_screen");
  if (naming_screen_visible && *naming_screen_visible) {
    if (naming_screen_card.Begin(naming_screen_visible)) {
      DrawNamingScreenEditor();
    }
    naming_screen_card.End();
  }

  return status_;
}

void ScreenEditor::DrawToolset() {
  // Sidebar is now drawn by EditorManager for card-based editors
  // This method kept for compatibility but sidebar handles card toggles
}

void ScreenEditor::DrawInventoryMenuEditor() {
  static bool create = false;
  if (!create && rom()->is_loaded()) {
    status_ = inventory_.Create(rom());
    if (status_.ok()) {
      palette_ = inventory_.palette();
      create = true;
    } else {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error loading inventory: %s",
                         status_.message().data());
      return;
    }
  }

  DrawInventoryToolset();

  if (ImGui::BeginTable("InventoryScreen", 4, ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Canvas");
    ImGui::TableSetupColumn("Tilesheet");
    ImGui::TableSetupColumn("Item Icons");
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
    DrawInventoryItemIcons();

    ImGui::TableNextColumn();
    gui::DisplayPalette(palette_, create);

    ImGui::EndTable();
  }
  ImGui::Separator();

  // TODO(scawful): Future Oracle of Secrets menu editor integration
  // - Full inventory screen layout editor
  // - Item slot assignment and positioning
  // - Heart container and magic meter editor
  // - Equipment display customization
  // - A/B button equipment quick-select editor
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

void ScreenEditor::DrawInventoryItemIcons() {
  if (ImGui::BeginChild("##ItemIconsList", ImVec2(0, 0), true,
                        ImGuiWindowFlags_HorizontalScrollbar)) {
    ImGui::Text("Item Icons (2x2 tiles each)");
    ImGui::Separator();

    auto& icons = inventory_.item_icons();
    if (icons.empty()) {
      ImGui::TextWrapped(
          "No item icons loaded. Icons will be loaded when the "
          "inventory is initialized.");
      ImGui::EndChild();
      return;
    }

    // Display icons in a table format
    if (ImGui::BeginTable("##IconsTable", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Icon Name");
      ImGui::TableSetupColumn("Tile Data");
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < icons.size(); i++) {
        const auto& icon = icons[i];

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        // Display icon name with selectable row
        if (ImGui::Selectable(icon.name.c_str(), false,
                              ImGuiSelectableFlags_SpanAllColumns)) {
          // TODO: Select this icon for editing
        }

        ImGui::TableNextColumn();
        // Display tile word data in hex format
        ImGui::Text("TL:%04X TR:%04X", icon.tile_tl, icon.tile_tr);
        ImGui::SameLine();
        ImGui::Text("BL:%04X BR:%04X", icon.tile_bl, icon.tile_br);
      }

      ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextWrapped(
        "NOTE: Individual icon editing will be implemented in the future "
        "Oracle of Secrets menu editor. Each icon is composed of 4 tile words "
        "representing a 2x2 arrangement of 8x8 tiles in SNES tile format "
        "(vhopppcc cccccccc).");
  }
  ImGui::EndChild();
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

    // Extract tile data from the atlas directly
    const int tiles_per_row = tile16_blockset_.atlas.width() / 16;
    const int tile_x = (tile16_id % tiles_per_row) * 16;
    const int tile_y = (tile16_id / tiles_per_row) * 16;

    std::vector<uint8_t> tile_data(16 * 16);
    int tile_data_offset = 0;
    tile16_blockset_.atlas.Get16x16Tile(tile_x, tile_y, tile_data,
                                        tile_data_offset);

    // Create or update cached tile
    auto* cached_tile = tile16_blockset_.tile_cache.GetTile(tile16_id);
    if (!cached_tile) {
      // Create new cached tile
      gfx::Bitmap new_tile(16, 16, 8, tile_data);
      new_tile.SetPalette(tile16_blockset_.atlas.palette());
      tile16_blockset_.tile_cache.CacheTile(tile16_id, std::move(new_tile));
      cached_tile = tile16_blockset_.tile_cache.GetTile(tile16_id);
    } else {
      // Update existing cached tile data
      cached_tile->set_data(tile_data);
    }

    if (cached_tile && cached_tile->is_active()) {
      // Ensure the cached tile has a valid texture
      if (!cached_tile->texture()) {
        // Queue texture creation via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, cached_tile);
      }
      screen_canvas_.DrawBitmap(*cached_tile, pos.x, pos.y, 4.0F, 255);
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
      gfx::RenderTile16(nullptr, tile16_blockset_, selected_tile16_);
      std::ranges::copy(tile16_blockset_.tile_info[selected_tile16_],
                        current_tile16_info.begin());
    }
    // Use direct bitmap rendering for tilesheet
    tilesheet_canvas_.DrawBitmap(tile16_blockset_.atlas, 1, 1, 2.0F, 255);
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

    // Get tile8 from cache on-demand (only create texture when needed)
    if (selected_tile8_ >= 0 && selected_tile8_ < 256) {
      auto* cached_tile8 = tile8_tilemap_.tile_cache.GetTile(selected_tile8_);

      if (!cached_tile8) {
        // Extract tile from atlas and cache it
        const int tiles_per_row =
            tile8_tilemap_.atlas.width() / 8;  // 128 / 8 = 16
        const int tile_x = (selected_tile8_ % tiles_per_row) * 8;
        const int tile_y = (selected_tile8_ / tiles_per_row) * 8;

        // Extract 8x8 tile data from atlas
        std::vector<uint8_t> tile_data(64);
        for (int py = 0; py < 8; py++) {
          for (int px = 0; px < 8; px++) {
            int src_x = tile_x + px;
            int src_y = tile_y + py;
            int src_index = src_y * tile8_tilemap_.atlas.width() + src_x;
            int dst_index = py * 8 + px;

            if (src_index < tile8_tilemap_.atlas.size() && dst_index < 64) {
              tile_data[dst_index] = tile8_tilemap_.atlas.data()[src_index];
            }
          }
        }

        gfx::Bitmap new_tile8(8, 8, 8, tile_data);
        new_tile8.SetPalette(tile8_tilemap_.atlas.palette());
        tile8_tilemap_.tile_cache.CacheTile(selected_tile8_,
                                            std::move(new_tile8));
        cached_tile8 = tile8_tilemap_.tile_cache.GetTile(selected_tile8_);
      }

      if (cached_tile8 && cached_tile8->is_active()) {
        // Create texture on-demand only when needed
        if (!cached_tile8->texture()) {
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, cached_tile8);
        }

        if (current_tile_canvas_.DrawTilePainter(*cached_tile8, 16)) {
          // Modify the tile16 based on the selected tile and current_tile16_info
          gfx::ModifyTile16(tile16_blockset_, rom()->graphics_buffer(),
                            current_tile16_info[0], current_tile16_info[1],
                            current_tile16_info[2], current_tile16_info[3], 212,
                            selected_tile16_);
          gfx::UpdateTile16(nullptr, tile16_blockset_, selected_tile16_);
        }
      }
    }
    // Get selected tile from cache
    auto* selected_tile = tile16_blockset_.tile_cache.GetTile(selected_tile16_);
    if (selected_tile && selected_tile->is_active()) {
      // Ensure the selected tile has a valid texture
      if (!selected_tile->texture()) {
        // Queue texture creation via Arena's deferred system
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, selected_tile);
      }
      current_tile_canvas_.DrawBitmap(*selected_tile, 2, 2, 4.0f, 255);
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
      gfx::UpdateTile16(nullptr, tile16_blockset_, selected_tile16_);
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
  std::string bin_file = util::FileDialogWrapper::ShowOpenFileDialog();
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
          // Queue texture creation via Arena's deferred system
          gfx::Arena::Get().QueueTextureCommand(
              gfx::Arena::TextureCommandType::CREATE, &sheets_[i]);
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
  // Initialize title screen on first draw
  if (!title_screen_loaded_ && rom()->is_loaded()) {
    status_ = title_screen_.Create(rom());
    if (!status_.ok()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error loading title screen: %s",
                         status_.message().data());
      return;
    }
    title_screen_loaded_ = true;
  }

  if (!title_screen_loaded_) {
    ImGui::Text("Title screen not loaded. Ensure ROM is loaded.");
    return;
  }

  // Toolbar with mode controls
  if (ImGui::Button(ICON_MD_DRAW)) {
    current_mode_ = EditingMode::DRAW;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE)) {
    status_ = title_screen_.Save(rom());
    if (status_.ok()) {
      ImGui::OpenPopup("SaveSuccess");
    }
  }
  ImGui::SameLine();
  ImGui::Text("Selected Tile: %d", selected_title_tile16_);

  // Save success popup
  if (ImGui::BeginPopup("SaveSuccess")) {
    ImGui::Text("Title screen saved successfully!");
    ImGui::EndPopup();
  }

  // Layer visibility controls
  bool prev_bg1 = show_title_bg1_;
  bool prev_bg2 = show_title_bg2_;
  ImGui::Checkbox("Show BG1", &show_title_bg1_);
  ImGui::SameLine();
  ImGui::Checkbox("Show BG2", &show_title_bg2_);

  // Re-render composite if visibility changed
  if (prev_bg1 != show_title_bg1_ || prev_bg2 != show_title_bg2_) {
    status_ =
        title_screen_.RenderCompositeLayer(show_title_bg1_, show_title_bg2_);
    if (status_.ok()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE,
          &title_screen_.composite_bitmap());
    }
  }

  // Layout: 2-column table (composite view + tile selector)
  if (ImGui::BeginTable("TitleScreenTable", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Title Screen (Composite)");
    ImGui::TableSetupColumn("Tile Selector");
    ImGui::TableHeadersRow();

    // Column 1: Composite Canvas (BG1+BG2 stacked)
    ImGui::TableNextColumn();
    DrawTitleScreenCompositeCanvas();

    // Column 2: Blockset Selector
    ImGui::TableNextColumn();
    DrawTitleScreenBlocksetSelector();

    ImGui::EndTable();
  }
}

void ScreenEditor::DrawTitleScreenCompositeCanvas() {
  title_bg1_canvas_.DrawBackground();
  title_bg1_canvas_.DrawContextMenu();

  // Draw composite tilemap (BG1+BG2 stacked with transparency)
  auto& composite_bitmap = title_screen_.composite_bitmap();
  if (composite_bitmap.is_active()) {
    title_bg1_canvas_.DrawBitmap(composite_bitmap, 0, 0, 2.0f, 255);
  }

  // Handle tile painting - always paint to BG1 layer
  if (current_mode_ == EditingMode::DRAW && selected_title_tile16_ >= 0) {
    if (title_bg1_canvas_.DrawTileSelector(8.0f)) {
      if (!title_bg1_canvas_.points().empty()) {
        auto click_pos = title_bg1_canvas_.points().front();
        int tile_x = static_cast<int>(click_pos.x) / 8;
        int tile_y = static_cast<int>(click_pos.y) / 8;

        if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 32) {
          int tilemap_index = tile_y * 32 + tile_x;

          // Create tile word: tile_id | (palette << 10) | h_flip | v_flip
          uint16_t tile_word = selected_title_tile16_ & 0x3FF;
          tile_word |= (title_palette_ & 0x07) << 10;
          if (title_h_flip_)
            tile_word |= 0x4000;
          if (title_v_flip_)
            tile_word |= 0x8000;

          // Update BG1 buffer and re-render both layers and composite
          title_screen_.mutable_bg1_buffer()[tilemap_index] = tile_word;
          status_ = title_screen_.RenderBG1Layer();
          if (status_.ok()) {
            // Update BG1 texture
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::UPDATE,
                &title_screen_.bg1_bitmap());

            // Re-render and update composite
            status_ = title_screen_.RenderCompositeLayer(show_title_bg1_,
                                                         show_title_bg2_);
            if (status_.ok()) {
              gfx::Arena::Get().QueueTextureCommand(
                  gfx::Arena::TextureCommandType::UPDATE, &composite_bitmap);
            }
          }
        }
      }
    }
  }

  title_bg1_canvas_.DrawGrid();
  title_bg1_canvas_.DrawOverlay();
}

void ScreenEditor::DrawTitleScreenBG1Canvas() {
  title_bg1_canvas_.DrawBackground();
  title_bg1_canvas_.DrawContextMenu();

  // Draw BG1 tilemap
  auto& bg1_bitmap = title_screen_.bg1_bitmap();
  if (bg1_bitmap.is_active()) {
    title_bg1_canvas_.DrawBitmap(bg1_bitmap, 0, 0, 2.0f, 255);
  }

  // Handle tile painting
  if (current_mode_ == EditingMode::DRAW && selected_title_tile16_ >= 0) {
    if (title_bg1_canvas_.DrawTileSelector(8.0f)) {
      if (!title_bg1_canvas_.points().empty()) {
        auto click_pos = title_bg1_canvas_.points().front();
        int tile_x = static_cast<int>(click_pos.x) / 8;
        int tile_y = static_cast<int>(click_pos.y) / 8;

        if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 32) {
          int tilemap_index = tile_y * 32 + tile_x;

          // Create tile word: tile_id | (palette << 10) | h_flip | v_flip
          uint16_t tile_word = selected_title_tile16_ & 0x3FF;
          tile_word |= (title_palette_ & 0x07) << 10;
          if (title_h_flip_)
            tile_word |= 0x4000;
          if (title_v_flip_)
            tile_word |= 0x8000;

          // Update buffer and re-render
          title_screen_.mutable_bg1_buffer()[tilemap_index] = tile_word;
          status_ = title_screen_.RenderBG1Layer();
          if (status_.ok()) {
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::UPDATE, &bg1_bitmap);
          }
        }
      }
    }
  }

  title_bg1_canvas_.DrawGrid(8.0f);
  title_bg1_canvas_.DrawOverlay();
}

void ScreenEditor::DrawTitleScreenBG2Canvas() {
  title_bg2_canvas_.DrawBackground();
  title_bg2_canvas_.DrawContextMenu();

  // Draw BG2 tilemap
  auto& bg2_bitmap = title_screen_.bg2_bitmap();
  if (bg2_bitmap.is_active()) {
    title_bg2_canvas_.DrawBitmap(bg2_bitmap, 0, 0, 2.0f, 255);
  }

  // Handle tile painting
  if (current_mode_ == EditingMode::DRAW && selected_title_tile16_ >= 0) {
    if (title_bg2_canvas_.DrawTileSelector(8.0f)) {
      if (!title_bg2_canvas_.points().empty()) {
        auto click_pos = title_bg2_canvas_.points().front();
        int tile_x = static_cast<int>(click_pos.x) / 8;
        int tile_y = static_cast<int>(click_pos.y) / 8;

        if (tile_x >= 0 && tile_x < 32 && tile_y >= 0 && tile_y < 32) {
          int tilemap_index = tile_y * 32 + tile_x;

          // Create tile word: tile_id | (palette << 10) | h_flip | v_flip
          uint16_t tile_word = selected_title_tile16_ & 0x3FF;
          tile_word |= (title_palette_ & 0x07) << 10;
          if (title_h_flip_)
            tile_word |= 0x4000;
          if (title_v_flip_)
            tile_word |= 0x8000;

          // Update buffer and re-render
          title_screen_.mutable_bg2_buffer()[tilemap_index] = tile_word;
          status_ = title_screen_.RenderBG2Layer();
          if (status_.ok()) {
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::UPDATE, &bg2_bitmap);
          }
        }
      }
    }
  }

  title_bg2_canvas_.DrawGrid(8.0f);
  title_bg2_canvas_.DrawOverlay();
}

void ScreenEditor::DrawTitleScreenBlocksetSelector() {
  title_blockset_canvas_.DrawBackground();
  title_blockset_canvas_.DrawContextMenu();

  // Draw tile8 bitmap (8x8 tiles used to compose tile16)
  auto& tiles8_bitmap = title_screen_.tiles8_bitmap();
  if (tiles8_bitmap.is_active()) {
    title_blockset_canvas_.DrawBitmap(tiles8_bitmap, 0, 0, 2.0f, 255);
  }

  // Handle tile selection (8x8 tiles)
  if (title_blockset_canvas_.DrawTileSelector(8.0f)) {
    // Calculate selected tile ID from click position
    if (!title_blockset_canvas_.points().empty()) {
      auto click_pos = title_blockset_canvas_.points().front();
      int tile_x = static_cast<int>(click_pos.x) / 8;
      int tile_y = static_cast<int>(click_pos.y) / 8;
      int tiles_per_row = 128 / 8;  // 16 tiles per row for 8x8 tiles
      selected_title_tile16_ = tile_x + (tile_y * tiles_per_row);
    }
  }

  title_blockset_canvas_.DrawGrid(8.0f);
  title_blockset_canvas_.DrawOverlay();

  // Show selected tile preview and controls
  if (selected_title_tile16_ >= 0) {
    ImGui::Text("Selected Tile: %d", selected_title_tile16_);

    // Flip controls
    ImGui::Checkbox("H Flip", &title_h_flip_);
    ImGui::SameLine();
    ImGui::Checkbox("V Flip", &title_v_flip_);

    // Palette selector (0-7 for 3BPP graphics)
    ImGui::SetNextItemWidth(100);
    ImGui::SliderInt("Palette", &title_palette_, 0, 7);
  }
}

void ScreenEditor::DrawNamingScreenEditor() {}

void ScreenEditor::DrawOverworldMapEditor() {
  // Initialize overworld map on first draw
  if (!ow_map_loaded_ && rom()->is_loaded()) {
    status_ = ow_map_screen_.Create(rom());
    if (!status_.ok()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error loading overworld map: %s",
                         status_.message().data());
      return;
    }
    ow_map_loaded_ = true;
  }

  if (!ow_map_loaded_) {
    ImGui::Text("Overworld map not loaded. Ensure ROM is loaded.");
    return;
  }

  // Toolbar with mode controls
  if (ImGui::Button(ICON_MD_DRAW)) {
    current_mode_ = EditingMode::DRAW;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_SAVE)) {
    status_ = ow_map_screen_.Save(rom());
    if (status_.ok()) {
      ImGui::OpenPopup("OWSaveSuccess");
    }
  }
  ImGui::SameLine();

  // World toggle
  if (ImGui::Button(ow_show_dark_world_ ? "Dark World" : "Light World")) {
    ow_show_dark_world_ = !ow_show_dark_world_;
    // Re-render map with new world
    status_ = ow_map_screen_.RenderMapLayer(ow_show_dark_world_);
    if (status_.ok()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &ow_map_screen_.map_bitmap());
    }
  }
  ImGui::SameLine();

  // Custom map load/save buttons
  if (ImGui::Button("Load Custom Map...")) {
    std::string path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!path.empty()) {
      status_ = ow_map_screen_.LoadCustomMap(path);
      if (!status_.ok()) {
        ImGui::OpenPopup("CustomMapLoadError");
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save Custom Map...")) {
    std::string path = util::FileDialogWrapper::ShowSaveFileDialog();
    if (!path.empty()) {
      status_ = ow_map_screen_.SaveCustomMap(path, ow_show_dark_world_);
      if (status_.ok()) {
        ImGui::OpenPopup("CustomMapSaveSuccess");
      }
    }
  }

  ImGui::SameLine();
  ImGui::Text("Selected Tile: %d", selected_ow_tile_);

  // Custom map error/success popups
  if (ImGui::BeginPopup("CustomMapLoadError")) {
    ImGui::Text("Error loading custom map: %s", status_.message().data());
    ImGui::EndPopup();
  }
  if (ImGui::BeginPopup("CustomMapSaveSuccess")) {
    ImGui::Text("Custom map saved successfully!");
    ImGui::EndPopup();
  }

  // Save success popup
  if (ImGui::BeginPopup("OWSaveSuccess")) {
    ImGui::Text("Overworld map saved successfully!");
    ImGui::EndPopup();
  }

  // Layout: 3-column table
  if (ImGui::BeginTable("OWMapTable", 3,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders)) {
    ImGui::TableSetupColumn("Map Canvas");
    ImGui::TableSetupColumn("Tileset");
    ImGui::TableSetupColumn("Palette");
    ImGui::TableHeadersRow();

    // Column 1: Map Canvas
    ImGui::TableNextColumn();
    ow_map_canvas_.DrawBackground();
    ow_map_canvas_.DrawContextMenu();

    auto& map_bitmap = ow_map_screen_.map_bitmap();
    if (map_bitmap.is_active()) {
      ow_map_canvas_.DrawBitmap(map_bitmap, 0, 0, 1.0f, 255);
    }

    // Handle tile painting
    if (current_mode_ == EditingMode::DRAW && selected_ow_tile_ >= 0) {
      if (ow_map_canvas_.DrawTileSelector(8.0f)) {
        if (!ow_map_canvas_.points().empty()) {
          auto click_pos = ow_map_canvas_.points().front();
          int tile_x = static_cast<int>(click_pos.x) / 8;
          int tile_y = static_cast<int>(click_pos.y) / 8;

          if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
            int tile_index = tile_x + (tile_y * 64);

            // Update appropriate world's tile data
            if (ow_show_dark_world_) {
              ow_map_screen_.mutable_dw_tiles()[tile_index] = selected_ow_tile_;
            } else {
              ow_map_screen_.mutable_lw_tiles()[tile_index] = selected_ow_tile_;
            }

            // Re-render map
            status_ = ow_map_screen_.RenderMapLayer(ow_show_dark_world_);
            if (status_.ok()) {
              gfx::Arena::Get().QueueTextureCommand(
                  gfx::Arena::TextureCommandType::UPDATE, &map_bitmap);
            }
          }
        }
      }
    }

    ow_map_canvas_.DrawGrid(8.0f);
    ow_map_canvas_.DrawOverlay();

    // Column 2: Tileset Selector
    ImGui::TableNextColumn();
    ow_tileset_canvas_.DrawBackground();
    ow_tileset_canvas_.DrawContextMenu();

    auto& tiles8_bitmap = ow_map_screen_.tiles8_bitmap();
    if (tiles8_bitmap.is_active()) {
      ow_tileset_canvas_.DrawBitmap(tiles8_bitmap, 0, 0, 2.0f, 255);
    }

    // Handle tile selection
    if (ow_tileset_canvas_.DrawTileSelector(8.0f)) {
      if (!ow_tileset_canvas_.points().empty()) {
        auto click_pos = ow_tileset_canvas_.points().front();
        int tile_x = static_cast<int>(click_pos.x) / 8;
        int tile_y = static_cast<int>(click_pos.y) / 8;
        selected_ow_tile_ = tile_x + (tile_y * 16);  // 16 tiles per row
      }
    }

    ow_tileset_canvas_.DrawGrid(8.0f);
    ow_tileset_canvas_.DrawOverlay();

    // Column 3: Palette Display
    ImGui::TableNextColumn();
    auto& palette = ow_show_dark_world_ ? ow_map_screen_.dw_palette()
                                        : ow_map_screen_.lw_palette();
    // Use inline palette editor for full 128-color palette
    gui::InlinePaletteEditor(palette, "Overworld Map Palette");

    ImGui::EndTable();
  }
}

void ScreenEditor::DrawDungeonMapToolset() {
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
