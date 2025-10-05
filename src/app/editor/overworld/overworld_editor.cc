#include "overworld_editor.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/core/asar_wrapper.h"
#include "app/core/features.h"
#include "app/core/window.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/arena.h"
#include "app/gfx/bitmap.h"
#include "app/gfx/performance_profiler.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/tilemap.h"
#include "app/gui/canvas.h"
#include "app/gui/editor_layout.h"
#include "app/gui/icons.h"
#include "app/gui/style.h"
#include "app/gui/ui_helpers.h"
#include "app/rom.h"
#include "app/zelda3/common.h"
#include "app/zelda3/overworld/overworld.h"
#include "app/zelda3/overworld/overworld_map.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"

namespace yaze::editor {

using core::Renderer;
using namespace ImGui;

constexpr float kInputFieldSize = 30.f;

void OverworldEditor::Initialize() {
  // Initialize MapPropertiesSystem with canvas and bitmap data
  map_properties_system_ = std::make_unique<MapPropertiesSystem>(
      &overworld_, rom_, &maps_bmp_, &ow_map_canvas_);

  // Set up refresh callbacks for MapPropertiesSystem
  map_properties_system_->SetRefreshCallbacks(
      [this]() { this->RefreshMapProperties(); },
      [this]() { this->RefreshOverworldMap(); },
      [this]() -> absl::Status { return this->RefreshMapPalette(); }
  );

  // Initialize OverworldEditorManager for v3 features
  overworld_manager_ =
      std::make_unique<OverworldEditorManager>(&overworld_, rom_, this);

  // Setup overworld canvas context menu
  SetupOverworldCanvasContextMenu();
  
  // Old toolset initialization removed - using modern CompactToolbar instead
}

absl::Status OverworldEditor::Load() {
  gfx::ScopedTimer timer("OverworldEditor::Load");

  LOG_INFO("OverworldEditor", "Loading overworld.");
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  RETURN_IF_ERROR(LoadGraphics());
  RETURN_IF_ERROR(
      tile16_editor_.Initialize(tile16_blockset_bmp_, current_gfx_bmp_,
                                *overworld_.mutable_all_tiles_types()));

  // CRITICAL FIX: Initialize tile16 editor with the correct overworld palette
  tile16_editor_.set_palette(palette_);
  tile16_editor_.set_rom(rom_);

  // Set up callback for when tile16 changes are committed
  tile16_editor_.set_on_changes_committed([this]() -> absl::Status {
    // Regenerate the overworld editor's tile16 blockset
    RETURN_IF_ERROR(RefreshTile16Blockset());

    // Force refresh of the current overworld map to show changes
    RefreshOverworldMap();

    LOG_INFO("OverworldEditor", "Overworld editor refreshed after Tile16 changes");
    return absl::OkStatus();
  });

  ASSIGN_OR_RETURN(entrance_tiletypes_, zelda3::LoadEntranceTileTypes(rom_));
  all_gfx_loaded_ = true;
  return absl::OkStatus();
}

absl::Status OverworldEditor::Update() {
  status_ = absl::OkStatus();

  // Process deferred textures for smooth loading
  ProcessDeferredTextures();

  if (overworld_canvas_fullscreen_) {
    DrawFullscreenCanvas();
    return status_;
  }

  // Modern layout - no tabs, just toolbar + canvas + floating cards
  DrawToolset();
  gui::VerticalSpacing(2.0f);

  // Initialize cards on first run
  static gui::EditorCard tile16_card("Tile16 Selector", ICON_MD_GRID_3X3);
  static gui::EditorCard tile8_card("Tile8 Selector", ICON_MD_GRID_4X4);
  static gui::EditorCard area_gfx_card("Area Graphics", ICON_MD_IMAGE);
  static gui::EditorCard scratch_card("Scratch Space", ICON_MD_BRUSH);
  static gui::EditorCard tile16_editor_card("Tile16 Editor", ICON_MD_GRID_ON);
  static gui::EditorCard gfx_groups_card("Graphics Groups", ICON_MD_COLLECTIONS);
  static gui::EditorCard usage_stats_card("Usage Statistics", ICON_MD_ANALYTICS);
  static gui::EditorCard v3_settings_card("v3 Settings", ICON_MD_TUNE);
  static bool cards_initialized = false;
  
  if (!cards_initialized) {
    // Position cards for optimal workflow
    tile16_card.SetDefaultSize(300, 600);
    tile16_card.SetPosition(gui::EditorCard::Position::Right);
    
    tile8_card.SetDefaultSize(280, 500);
    tile8_card.SetPosition(gui::EditorCard::Position::Right);
    
    area_gfx_card.SetDefaultSize(300, 400);
    area_gfx_card.SetPosition(gui::EditorCard::Position::Right);
    
    scratch_card.SetDefaultSize(350, 500);
    scratch_card.SetPosition(gui::EditorCard::Position::Right);
    
    tile16_editor_card.SetDefaultSize(800, 600);
    tile16_editor_card.SetPosition(gui::EditorCard::Position::Floating);
    
    gfx_groups_card.SetDefaultSize(700, 550);
    gfx_groups_card.SetPosition(gui::EditorCard::Position::Floating);
    
    usage_stats_card.SetDefaultSize(600, 500);
    usage_stats_card.SetPosition(gui::EditorCard::Position::Floating);
    
    v3_settings_card.SetDefaultSize(500, 600);
    v3_settings_card.SetPosition(gui::EditorCard::Position::Floating);
    
    cards_initialized = true;
  }
  
  // Main canvas (full width when cards are docked)
  DrawOverworldCanvas();
  
  // Floating tile selector cards (4 tabs converted to separate cards)
  if (show_tile16_selector_ && tile16_card.Begin(&show_tile16_selector_)) {
    status_ = DrawTile16Selector();
    tile16_card.End();
  }
  
  if (show_tile8_selector_ && tile8_card.Begin(&show_tile8_selector_)) {
    gui::BeginPadding(3);
    gui::BeginChildWithScrollbar("##Tile8SelectorScrollRegion");
    DrawTile8Selector();
    ImGui::EndChild();
    gui::EndNoPadding();
    tile8_card.End();
  }
  
  if (show_area_gfx_ && area_gfx_card.Begin(&show_area_gfx_)) {
    status_ = DrawAreaGraphics();
    area_gfx_card.End();
  }
  
  if (show_scratch_ && scratch_card.Begin(&show_scratch_)) {
    status_ = DrawScratchSpace();
    scratch_card.End();
  }
  
  // Tile16 Editor popup-only (no tab)
  if (show_tile16_editor_ && tile16_editor_card.Begin(&show_tile16_editor_)) {
    if (rom_->is_loaded()) {
      status_ = tile16_editor_.Update();
    } else {
      gui::CenterText("No ROM loaded");
    }
    tile16_editor_card.End();
  }
  
  // Graphics Groups popup
  if (show_gfx_groups_ && gfx_groups_card.Begin(&show_gfx_groups_)) {
    if (rom_->is_loaded()) {
      status_ = gfx_group_editor_.Update();
    } else {
      gui::CenterText("No ROM loaded");
    }
    gfx_groups_card.End();
  }
  
  // Usage Statistics popup
  if (show_usage_stats_ && usage_stats_card.Begin(&show_usage_stats_)) {
    if (rom_->is_loaded()) {
      status_ = UpdateUsageStats();
    } else {
      gui::CenterText("No ROM loaded");
    }
    usage_stats_card.End();
  }
  
  // v3 Settings popup
  if (show_v3_settings_ && v3_settings_card.Begin(&show_v3_settings_)) {
    if (rom_->is_loaded()) {
      status_ = overworld_manager_->DrawV3SettingsPanel();
    }
    v3_settings_card.End();
  }

  // Area Configuration Panel (detailed editing)
  if (show_map_properties_panel_) {
    ImGui::SetNextWindowSize(ImVec2(650, 750), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_TUNE " Area Configuration###AreaConfig", &show_map_properties_panel_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
        map_properties_system_->DrawMapPropertiesPanel(current_map_, show_map_properties_panel_);
      }
    }
    ImGui::End();
  }

  // Custom Background Color Editor
  if (show_custom_bg_color_editor_) {
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_FORMAT_COLOR_FILL " Background Color", &show_custom_bg_color_editor_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
        map_properties_system_->DrawCustomBackgroundColorEditor(current_map_, show_custom_bg_color_editor_);
      }
    }
    ImGui::End();
  }

  // Visual Effects Editor (Subscreen Overlays)
  if (show_overlay_editor_) {
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_LAYERS " Visual Effects Editor###OverlayEditor", &show_overlay_editor_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
        map_properties_system_->DrawOverlayEditor(current_map_, show_overlay_editor_);
      }
    }
    ImGui::End();
  }

  return status_;
}

void OverworldEditor::DrawFullscreenCanvas() {
  static bool use_work_area = true;
  static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoSavedSettings;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
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
  // Modern adaptive toolbar with inline mode switching and properties
  static gui::Toolset toolbar;
  static uint8_t asm_version = 0xFF;
  
  // Don't use WidgetIdScope here - it conflicts with ImGui::Begin/End ID stack in cards
  // Widgets register themselves individually instead
  
  toolbar.Begin();
  
  // Mode buttons (editing tools) - compact inline row
  toolbar.BeginModeGroup();
  
  if (toolbar.ModeButton(ICON_MD_PAN_TOOL_ALT, current_mode == EditingMode::PAN, "Pan (1)")) {
    current_mode = EditingMode::PAN;
    ow_map_canvas_.set_draggable(true);
  }
  
  if (toolbar.ModeButton(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE, "Draw (2)")) {
    current_mode = EditingMode::DRAW_TILE;
  }
  
  if (toolbar.ModeButton(ICON_MD_DOOR_FRONT, current_mode == EditingMode::ENTRANCES, "Entrances (3)")) {
    current_mode = EditingMode::ENTRANCES;
  }
  
  if (toolbar.ModeButton(ICON_MD_DOOR_BACK, current_mode == EditingMode::EXITS, "Exits (4)")) {
    current_mode = EditingMode::EXITS;
  }
  
  if (toolbar.ModeButton(ICON_MD_GRASS, current_mode == EditingMode::ITEMS, "Items (5)")) {
    current_mode = EditingMode::ITEMS;
  }
  
  if (toolbar.ModeButton(ICON_MD_PEST_CONTROL_RODENT, current_mode == EditingMode::SPRITES, "Sprites (6)")) {
    current_mode = EditingMode::SPRITES;
  }
  
  if (toolbar.ModeButton(ICON_MD_ADD_LOCATION, current_mode == EditingMode::TRANSPORTS, "Transports (7)")) {
    current_mode = EditingMode::TRANSPORTS;
  }
  
  if (toolbar.ModeButton(ICON_MD_MUSIC_NOTE, current_mode == EditingMode::MUSIC, "Music (8)")) {
    current_mode = EditingMode::MUSIC;
  }
  
  toolbar.EndModeGroup();
  
  // ROM version badge
  asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  toolbar.AddRomBadge(asm_version, [this]() {
    ImGui::OpenPopup("UpgradeROMVersion");
  });
  
  // World selector
  const char* worlds[] = {"Light", "Dark", "Extra"};
  if (toolbar.AddCombo(ICON_MD_PUBLIC, &current_world_, worlds, 3)) {
    RefreshMapProperties();
    RefreshOverworldMap();
  }
  
  toolbar.AddSeparator();
  
  // Inline map properties with icon labels - use toolbar methods for consistency
  if (toolbar.AddProperty(ICON_MD_IMAGE, "##gfx", 
                         overworld_.mutable_overworld_map(current_map_)->mutable_area_graphics(),
                         [this]() {
    RefreshMapProperties();
    RefreshOverworldMap();
  })) {
    // Property changed
  }
  
  if (toolbar.AddProperty(ICON_MD_PALETTE, "##pal",
                         overworld_.mutable_overworld_map(current_map_)->mutable_area_palette(),
                         [this]() {
    RefreshMapProperties();
    status_ = RefreshMapPalette();
    RefreshOverworldMap();
  })) {
    // Property changed
  }
  
  toolbar.AddSeparator();
  
  // Quick actions
  if (toolbar.AddAction(ICON_MD_ZOOM_OUT, "Zoom Out")) {
    ow_map_canvas_.ZoomOut();
  }
  
  if (toolbar.AddAction(ICON_MD_ZOOM_IN, "Zoom In")) {
    ow_map_canvas_.ZoomIn();
  }
  
  if (toolbar.AddToggle(ICON_MD_OPEN_IN_FULL, &overworld_canvas_fullscreen_, "Fullscreen (F11)")) {
    // Toggled by helper
  }
  
  toolbar.AddSeparator();
  
  // Card visibility toggles (with automation-friendly paths)
  if (toolbar.AddAction(ICON_MD_GRID_3X3, "Toggle Tile16 Selector")) {
    show_tile16_selector_ = !show_tile16_selector_;
  }
  
  if (toolbar.AddAction(ICON_MD_GRID_4X4, "Toggle Tile8 Selector")) {
    show_tile8_selector_ = !show_tile8_selector_;
  }
  
  if (toolbar.AddAction(ICON_MD_IMAGE, "Toggle Area Graphics")) {
    show_area_gfx_ = !show_area_gfx_;
  }
  
  if (toolbar.AddAction(ICON_MD_BRUSH, "Toggle Scratch Space")) {
    show_scratch_ = !show_scratch_;
  }
  
  toolbar.AddSeparator();
  
  if (toolbar.AddAction(ICON_MD_GRID_VIEW, "Open Tile16 Editor")) {
    show_tile16_editor_ = !show_tile16_editor_;
  }
  
  if (toolbar.AddAction(ICON_MD_COLLECTIONS, "Open Graphics Groups")) {
    show_gfx_groups_ = !show_gfx_groups_;
  }
  
  toolbar.AddSeparator();
  
  // v3 Settings and Usage Statistics
  toolbar.AddV3StatusBadge(asm_version, [this]() {
    show_v3_settings_ = !show_v3_settings_;
  });
  
  if (toolbar.AddUsageStatsButton("Open Usage Statistics")) {
    show_usage_stats_ = !show_usage_stats_;
  }
  
  if (toolbar.AddAction(ICON_MD_TUNE, "Open Area Configuration")) {
    show_map_properties_panel_ = !show_map_properties_panel_;
  }
  
  toolbar.End();

  // All editor windows are now rendered in Update() using either EditorCard system
  // or MapPropertiesSystem for map-specific panels. This keeps the toolset clean
  // and prevents ImGui ID stack issues.

  // Legacy window code removed - windows rendered in Update() include:
  // - Graphics Groups (EditorCard)
  // - Area Configuration (MapPropertiesSystem)
  // - Background Color Editor (MapPropertiesSystem)
  // - Visual Effects Editor (MapPropertiesSystem)
  // - Tile16 Editor, Usage Stats, etc. (EditorCards)

  // Keyboard shortcuts for the Overworld Editor
  if (!ImGui::IsAnyItemActive()) {
    using enum EditingMode;

    // Tool shortcuts
    if (ImGui::IsKeyDown(ImGuiKey_1)) {
      current_mode = PAN;
    } else if (ImGui::IsKeyDown(ImGuiKey_2)) {
      current_mode = DRAW_TILE;
    } else if (ImGui::IsKeyDown(ImGuiKey_3)) {
      current_mode = ENTRANCES;
    } else if (ImGui::IsKeyDown(ImGuiKey_4)) {
      current_mode = EXITS;
    } else if (ImGui::IsKeyDown(ImGuiKey_5)) {
      current_mode = ITEMS;
    } else if (ImGui::IsKeyDown(ImGuiKey_6)) {
      current_mode = SPRITES;
    } else if (ImGui::IsKeyDown(ImGuiKey_7)) {
      current_mode = TRANSPORTS;
    } else if (ImGui::IsKeyDown(ImGuiKey_8)) {
      current_mode = MUSIC;
    }

    // View shortcuts
    if (ImGui::IsKeyDown(ImGuiKey_F11)) {
      overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
    }

    // Toggle map lock with L key
    if (ImGui::IsKeyDown(ImGuiKey_L) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      current_map_lock_ = !current_map_lock_;
    }

    // Toggle Tile16 editor with T key
    if (ImGui::IsKeyDown(ImGuiKey_T) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
      show_tile16_editor_ = !show_tile16_editor_;
    }
  }
}

void OverworldEditor::DrawOverworldMaps() {
  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);

    // Bounds checking to prevent crashes
    if (world_index < 0 || world_index >= static_cast<int>(maps_bmp_.size())) {
      continue;  // Skip invalid map index
    }

    int scale = static_cast<int>(ow_map_canvas_.global_scale());
    int map_x = (xx * kOverworldMapSize * scale);
    int map_y = (yy * kOverworldMapSize * scale);

    // Check if the map has a texture, if not, ensure it gets loaded
    if (!maps_bmp_[world_index].texture() &&
        maps_bmp_[world_index].is_active()) {
      EnsureMapTexture(world_index);
    }

    // Only draw if the map has a texture or is the currently selected map
    if (maps_bmp_[world_index].texture() || world_index == current_map_) {
      ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y,
                                ow_map_canvas_.global_scale());
    } else {
      // Draw a placeholder for maps that haven't loaded yet
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = ow_map_canvas_.zero_point();
      ImVec2 placeholder_pos =
          ImVec2(canvas_pos.x + map_x, canvas_pos.y + map_y);
      ImVec2 placeholder_size =
          ImVec2(kOverworldMapSize * scale, kOverworldMapSize * scale);

      // Modern loading indicator with theme colors
      draw_list->AddRectFilled(
          placeholder_pos,
          ImVec2(placeholder_pos.x + placeholder_size.x,
                 placeholder_pos.y + placeholder_size.y),
          IM_COL32(32, 32, 32, 128));  // Dark gray with transparency

      // Animated loading spinner
      ImVec2 spinner_pos = ImVec2(
        placeholder_pos.x + placeholder_size.x / 2,
        placeholder_pos.y + placeholder_size.y / 2
      );
      
      const float spinner_radius = 8.0f;
      const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
      const float start_angle = rotation;
      const float end_angle = rotation + IM_PI * 1.5f;
      
      draw_list->PathArcTo(spinner_pos, spinner_radius, start_angle, end_angle, 12);
      draw_list->PathStroke(IM_COL32(100, 180, 100, 255), 0, 2.5f);
    }

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

  // Bounds checking to prevent crashes
  if (current_map_ < 0 || current_map_ >= static_cast<int>(maps_bmp_.size())) {
    return;  // Invalid map index, skip drawing
  }

  // Validate tile16_blockset_ before calling GetTilemapData
  if (!tile16_blockset_.atlas.is_active() ||
      tile16_blockset_.atlas.vector().empty()) {
    LOG_ERROR("OverworldEditor",
              "Error: tile16_blockset_ is not properly initialized (active: %s, "
              "size: %zu)",
              tile16_blockset_.atlas.is_active() ? "true" : "false",
              tile16_blockset_.atlas.vector().size());
    return;  // Skip drawing if blockset is invalid
  }

  // Render the updated map bitmap.
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, current_tile16_);
  RenderUpdatedMapBitmap(mouse_position, tile_data);

  // Calculate the correct superX and superY values
  int superY = current_map_ / 8;
  int superX = current_map_ % 8;
  int mouse_x = mouse_position.x;
  int mouse_y = mouse_position.y;
  // Calculate the correct tile16_x and tile16_y positions
  int tile16_x = (mouse_x % kOverworldMapSize) / (kOverworldMapSize / 32);
  int tile16_y = (mouse_y % kOverworldMapSize) / (kOverworldMapSize / 32);

  // Update the overworld_.map_tiles() based on tile16 ID and current world
  auto& selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  int index_x = superX * 32 + tile16_x;
  int index_y = superY * 32 + tile16_y;

  selected_world[index_x][index_y] = current_tile16_;
}

void OverworldEditor::RenderUpdatedMapBitmap(
    const ImVec2& click_position, const std::vector<uint8_t>& tile_data) {

  // Bounds checking to prevent crashes
  if (current_map_ < 0 || current_map_ >= static_cast<int>(maps_bmp_.size())) {
    LOG_ERROR("OverworldEditor",
              "ERROR: RenderUpdatedMapBitmap - Invalid current_map_ %d "
              "(maps_bmp_.size()=%zu)",
              current_map_, maps_bmp_.size());
    return;  // Invalid map index, skip rendering
  }

  // Calculate the tile index for x and y based on the click_position
  int tile_index_x =
      (static_cast<int>(click_position.x) % kOverworldMapSize) / kTile16Size;
  int tile_index_y =
      (static_cast<int>(click_position.y) % kOverworldMapSize) / kTile16Size;

  // Calculate the pixel start position based on tile index and tile size
  ImVec2 start_position;
  start_position.x = static_cast<float>(tile_index_x * kTile16Size);
  start_position.y = static_cast<float>(tile_index_y * kTile16Size);

  // Update the bitmap's pixel data based on the start_position and tile_data
  gfx::Bitmap& current_bitmap = maps_bmp_[current_map_];

  // Validate bitmap state before writing
  if (!current_bitmap.is_active() || current_bitmap.size() == 0) {
    LOG_ERROR("OverworldEditor",
              "ERROR: RenderUpdatedMapBitmap - Bitmap %d is not active or has no "
              "data (active=%s, size=%zu)",
              current_map_, current_bitmap.is_active() ? "true" : "false",
              current_bitmap.size());
    return;
  }

  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      int pixel_index =
          (start_position.y + y) * kOverworldMapSize + (start_position.x + x);

      // Bounds check for pixel index
      if (pixel_index < 0 ||
          pixel_index >= static_cast<int>(current_bitmap.size())) {
        LOG_ERROR("OverworldEditor",
                  "ERROR: RenderUpdatedMapBitmap - pixel_index %d out of bounds "
                  "(bitmap size=%zu)",
                  pixel_index, current_bitmap.size());
        continue;
      }

      // Bounds check for tile data
      int tile_data_index = y * kTile16Size + x;
      if (tile_data_index < 0 ||
          tile_data_index >= static_cast<int>(tile_data.size())) {
        LOG_ERROR("OverworldEditor",
                  "ERROR: RenderUpdatedMapBitmap - tile_data_index %d out of bounds "
                  "(tile_data size=%zu)",
                  tile_data_index, tile_data.size());
        continue;
      }

      current_bitmap.WriteToPixel(pixel_index, tile_data[tile_data_index]);
    }
  }

  current_bitmap.set_modified(true);

  // Immediately update the texture to reflect changes
  core::Renderer::Get().UpdateBitmap(&current_bitmap);
}

void OverworldEditor::CheckForOverworldEdits() {
  LOG_DEBUG("OverworldEditor", "CheckForOverworldEdits: Frame %d",
            ImGui::GetFrameCount());

  CheckForSelectRectangle();

  // User has selected a tile they want to draw from the blockset
  // and clicked on the canvas.
  if (!blockset_canvas_.points().empty() &&
      !ow_map_canvas_.select_rect_active() &&
      ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
    DrawOverworldEdits();
  }

  if (ow_map_canvas_.select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      LOG_DEBUG("OverworldEditor", "CheckForOverworldEdits: About to apply rectangle selection");

      auto& selected_world =
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

      if (start_x > end_x)
        std::swap(start_x, end_x);
      if (start_y > end_y)
        std::swap(start_y, end_y);

      constexpr int local_map_size = 512;  // Size of each local map
      // Number of tiles per local map (since each tile is 16x16)
      constexpr int tiles_per_local_map = local_map_size / kTile16Size;

      LOG_DEBUG("OverworldEditor",
                "CheckForOverworldEdits: About to fill rectangle with "
                "current_tile16_=%d",
                current_tile16_);

      // Apply the selected tiles to each position in the rectangle
      // CRITICAL FIX: Use pre-computed tile16_ids_ instead of recalculating from selected_tiles_
      // This prevents wrapping issues when dragging near boundaries
      int i = 0;
      for (int y = start_y; y <= end_y && i < static_cast<int>(selected_tile16_ids_.size()); y += kTile16Size) {
        for (int x = start_x; x <= end_x && i < static_cast<int>(selected_tile16_ids_.size()); x += kTile16Size, ++i) {
          
          // Determine which local map (512x512) the tile is in
          int local_map_x = x / local_map_size;
          int local_map_y = y / local_map_size;

          // Calculate the tile's position within its local map
          int tile16_x = (x % local_map_size) / kTile16Size;
          int tile16_y = (y % local_map_size) / kTile16Size;

          // Calculate the index within the overall map structure
          int index_x = local_map_x * tiles_per_local_map + tile16_x;
          int index_y = local_map_y * tiles_per_local_map + tile16_y;

          // FIXED: Use pre-computed tile ID from the ORIGINAL selection
          int tile16_id = selected_tile16_ids_[i];
          // Bounds check for the selected world array, accounting for rectangle size
          // Ensure the entire rectangle fits within the world bounds
          int rect_width = ((end_x - start_x) / kTile16Size) + 1;
          int rect_height = ((end_y - start_y) / kTile16Size) + 1;

          // Prevent painting from wrapping around at the edges of large maps
          // Only allow painting if the entire rectangle is within the same 512x512 local map
          int start_local_map_x = start_x / local_map_size;
          int start_local_map_y = start_y / local_map_size;
          int end_local_map_x = end_x / local_map_size;
          int end_local_map_y = end_y / local_map_size;

          bool in_same_local_map = (start_local_map_x == end_local_map_x) && (start_local_map_y == end_local_map_y);

          if (in_same_local_map &&
              index_x >= 0 && (index_x + rect_width - 1) < 0x200 &&
              index_y >= 0 && (index_y + rect_height - 1) < 0x200) {
            selected_world[index_x][index_y] = tile16_id;

            // CRITICAL FIX: Also update the bitmap directly like single tile drawing
            ImVec2 tile_position(x, y);
            auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile16_id);
            if (!tile_data.empty()) {
              RenderUpdatedMapBitmap(tile_position, tile_data);
              LOG_INFO("OverworldEditor",
                       "CheckForOverworldEdits: Updated bitmap at position (%d,%d) "
                       "with tile16_id=%d",
                       x, y, tile16_id);
            } else {
              LOG_ERROR("OverworldEditor", "ERROR: Failed to get tile data for tile16_id=%d",
                         tile16_id);
            }
          }
        }
      }

      RefreshOverworldMap();
      // Clear the rectangle selection after applying
      // This is commented out for now, will come back to later.
      // ow_map_canvas_.mutable_selected_tiles()->clear();
      // ow_map_canvas_.mutable_points()->clear();
      LOG_INFO("OverworldEditor",
               "CheckForOverworldEdits: Rectangle selection applied and cleared");
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
    
    // Scroll blockset canvas to show the selected tile
    ScrollBlocksetCanvasToCurrentTile();
  }

  // Rectangle selection case - use member variable instead of static local
  if (ow_map_canvas_.select_rect_active()) {
    // Get the tile16 IDs from the selected tile ID positions
    selected_tile16_ids_.clear();

    if (ow_map_canvas_.selected_tiles().size() > 0) {
      // Set the current world and map in overworld for proper tile lookup
      overworld_.set_current_world(current_world_);
      overworld_.set_current_map(current_map_);
      for (auto& each : ow_map_canvas_.selected_tiles()) {
        selected_tile16_ids_.push_back(overworld_.GetTileFromPosition(each));
      }
    }
  }
  // Create a composite image of all the tile16s selected
  ow_map_canvas_.DrawBitmapGroup(selected_tile16_ids_, tile16_blockset_, 0x10,
                                 ow_map_canvas_.global_scale());
}

absl::Status OverworldEditor::Copy() {
  if (!context_)
    return absl::FailedPreconditionError("No editor context");
  // If a rectangle selection exists, copy its tile16 IDs into shared clipboard
  if (ow_map_canvas_.select_rect_active() &&
      !ow_map_canvas_.selected_points().empty()) {
    std::vector<int> ids;
    const auto start = ow_map_canvas_.selected_points()[0];
    const auto end = ow_map_canvas_.selected_points()[1];
    const int start_x =
        static_cast<int>(std::floor(std::min(start.x, end.x) / 16.0f));
    const int end_x =
        static_cast<int>(std::floor(std::max(start.x, end.x) / 16.0f));
    const int start_y =
        static_cast<int>(std::floor(std::min(start.y, end.y) / 16.0f));
    const int end_y =
        static_cast<int>(std::floor(std::max(start.y, end.y) / 16.0f));
    const int width = end_x - start_x + 1;
    const int height = end_y - start_y + 1;
    ids.reserve(width * height);
    overworld_.set_current_world(current_world_);
    overworld_.set_current_map(current_map_);
    for (int y = start_y; y <= end_y; ++y) {
      for (int x = start_x; x <= end_x; ++x) {
        ids.push_back(overworld_.GetTile(x, y));
      }
    }

    context_->shared_clipboard.overworld_tile16_ids = std::move(ids);
    context_->shared_clipboard.overworld_width = width;
    context_->shared_clipboard.overworld_height = height;
    context_->shared_clipboard.has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  // Single tile copy fallback
  if (current_tile16_ >= 0) {
    context_->shared_clipboard.overworld_tile16_ids = {current_tile16_};
    context_->shared_clipboard.overworld_width = 1;
    context_->shared_clipboard.overworld_height = 1;
    context_->shared_clipboard.has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  return absl::FailedPreconditionError("Nothing selected to copy");
}

absl::Status OverworldEditor::Paste() {
  if (!context_)
    return absl::FailedPreconditionError("No editor context");
  if (!context_->shared_clipboard.has_overworld_tile16) {
    return absl::FailedPreconditionError("Clipboard empty");
  }
  if (ow_map_canvas_.points().empty() &&
      ow_map_canvas_.selected_tile_pos().x == -1) {
    return absl::FailedPreconditionError("No paste target");
  }

  // Determine paste anchor position (use current mouse drawn tile position)
  const ImVec2 anchor = ow_map_canvas_.drawn_tile_position();

  // Compute anchor in tile16 grid within the current map
  const int tile16_x =
      (static_cast<int>(anchor.x) % kOverworldMapSize) / kTile16Size;
  const int tile16_y =
      (static_cast<int>(anchor.y) % kOverworldMapSize) / kTile16Size;

  auto& selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  const int superY = current_map_ / 8;
  const int superX = current_map_ % 8;
  const int tiles_per_local_map = 512 / kTile16Size;

  const int width = context_->shared_clipboard.overworld_width;
  const int height = context_->shared_clipboard.overworld_height;
  const auto& ids = context_->shared_clipboard.overworld_tile16_ids;

  // Guard
  if (width * height != static_cast<int>(ids.size())) {
    return absl::InternalError("Clipboard dimensions mismatch");
  }

  for (int dy = 0; dy < height; ++dy) {
    for (int dx = 0; dx < width; ++dx) {
      const int id = ids[dy * width + dx];
      const int gx = tile16_x + dx;
      const int gy = tile16_y + dy;

      const int global_x = superX * 32 + gx;
      const int global_y = superY * 32 + gy;
      if (global_x < 0 || global_x >= 256 || global_y < 0 || global_y >= 256)
        continue;
      selected_world[global_x][global_y] = id;
    }
  }

  RefreshOverworldMap();
  return absl::OkStatus();
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
  int hovered_map = map_x + map_y * 8;
  if (current_world_ == 1) {
    hovered_map += 0x40;
  } else if (current_world_ == 2) {
    hovered_map += 0x80;
  }

  // Only update current_map_ if not locked
  if (!current_map_lock_) {
    current_map_ = hovered_map;
    current_parent_ = overworld_.overworld_map(current_map_)->parent();

    // Ensure the current map is built (on-demand loading)
    RETURN_IF_ERROR(overworld_.EnsureMapBuilt(current_map_));
  }

  const int current_highlighted_map = current_map_;

  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3);

  // Get area size for v3+ ROMs, otherwise use legacy logic
  if (use_v3_area_sizes) {
    using zelda3::AreaSizeEnum;
    auto area_size = overworld_.overworld_map(current_map_)->area_size();
    const int highlight_parent =
        overworld_.overworld_map(current_highlighted_map)->parent();

    // Calculate parent map coordinates accounting for world offset
    int parent_map_x;
    int parent_map_y;
    if (current_world_ == 0) {
      // Light World (0x00-0x3F)
      parent_map_x = highlight_parent % 8;
      parent_map_y = highlight_parent / 8;
    } else if (current_world_ == 1) {
      // Dark World (0x40-0x7F)
      parent_map_x = (highlight_parent - 0x40) % 8;
      parent_map_y = (highlight_parent - 0x40) / 8;
    } else {
      // Special World (0x80-0x9F)
      parent_map_x = (highlight_parent - 0x80) % 8;
      parent_map_y = (highlight_parent - 0x80) / 8;
    }

    // Draw outline based on area size
    switch (area_size) {
      case AreaSizeEnum::LargeArea:
        // 2x2 grid (1024x1024)
        ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                   parent_map_y * kOverworldMapSize,
                                   large_map_size, large_map_size);
        break;
      case AreaSizeEnum::WideArea:
        // 2x1 grid (1024x512) - horizontal
        ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                   parent_map_y * kOverworldMapSize,
                                   large_map_size, kOverworldMapSize);
        break;
      case AreaSizeEnum::TallArea:
        // 1x2 grid (512x1024) - vertical
        ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                   parent_map_y * kOverworldMapSize,
                                   kOverworldMapSize, large_map_size);
        break;
      case AreaSizeEnum::SmallArea:
      default:
        // 1x1 grid (512x512)
        ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                   parent_map_y * kOverworldMapSize,
                                   kOverworldMapSize, kOverworldMapSize);
        break;
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (overworld_.overworld_map(current_map_)->is_large_map() ||
        overworld_.overworld_map(current_map_)->large_index() != 0) {
      const int highlight_parent =
          overworld_.overworld_map(current_highlighted_map)->parent();
      const int parent_map_x = highlight_parent % 8;
      const int parent_map_y = highlight_parent / 8;
      ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                 parent_map_y * kOverworldMapSize,
                                 large_map_size, large_map_size);
    } else {
      // Calculate map coordinates accounting for world offset
      int current_map_x;
      int current_map_y;
      if (current_world_ == 0) {
        // Light World (0x00-0x3F)
        current_map_x = current_highlighted_map % 8;
        current_map_y = current_highlighted_map / 8;
      } else if (current_world_ == 1) {
        // Dark World (0x40-0x7F)
        current_map_x = (current_highlighted_map - 0x40) % 8;
        current_map_y = (current_highlighted_map - 0x40) / 8;
      } else {
        // Special World (0x80-0x9F) - use display coordinates based on current_world_
        // The special world maps are displayed in the same 8x8 grid as LW/DW
        current_map_x = (current_highlighted_map - 0x80) % 8;
        current_map_y = (current_highlighted_map - 0x80) / 8;
      }
      ow_map_canvas_.DrawOutline(current_map_x * kOverworldMapSize,
                                 current_map_y * kOverworldMapSize,
                                 kOverworldMapSize, kOverworldMapSize);
    }
  }

  // Ensure current map has texture created for rendering
  EnsureMapTexture(current_map_);

  if (maps_bmp_[current_map_].modified()) {
    RefreshOverworldMap();
    RETURN_IF_ERROR(RefreshTile16Blockset());

    // Ensure tile16 blockset is fully updated before rendering
    if (tile16_blockset_.atlas.is_active()) {
      Renderer::Get().UpdateBitmap(&tile16_blockset_.atlas);
    }

    // Update map texture with the traditional direct update approach
    Renderer::Get().UpdateBitmap(&maps_bmp_[current_map_]);
    maps_bmp_[current_map_].set_modified(false);
  }

  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    RETURN_IF_ERROR(RefreshTile16Blockset());
  }

  // If double clicked, toggle the current map
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right)) {
    current_map_lock_ = !current_map_lock_;
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
  // Simplified map settings - compact row with popup panels for detailed editing
  if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
    map_properties_system_->DrawSimplifiedMapSettings(
        current_world_, current_map_, current_map_lock_, 
        show_map_properties_panel_, show_custom_bg_color_editor_,
        show_overlay_editor_, show_overlay_preview_, 
        game_state_, (int&)current_mode);
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
    // Handle map interaction with middle-click instead of right-click
    HandleMapInteraction();
  }

  if (overworld_.is_loaded()) {
    DrawOverworldMaps();
    DrawOverworldExits(ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
    DrawOverworldEntrances(ow_map_canvas_.zero_point(), ow_map_canvas_.scrolling());
    DrawOverworldItems();
    DrawOverworldSprites();

    // Draw overlay preview if enabled
    if (show_overlay_preview_) {
      map_properties_system_->DrawOverlayPreviewOnMap(
          current_map_, current_world_, show_overlay_preview_);
    }

    if (current_mode == EditingMode::DRAW_TILE) {
      CheckForOverworldEdits();
    }
    if (IsItemHovered())
      status_ = CheckForCurrentMap();
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
  gui::EndPadding();  // Fixed: was EndNoPadding()

  blockset_canvas_.DrawContextMenu();
  blockset_canvas_.DrawBitmap(tile16_blockset_.atlas, /*x_offset=*/2,
                              map_blockset_loaded_, /*scale=*/2);
  bool tile_selected = false;

  // Call DrawTileSelector after event detection for visual feedback
  if (blockset_canvas_.DrawTileSelector(32.0f)) {
    tile_selected = true;
    show_tile16_editor_ = true;
  }

  // Then check for single click (if not double-click)
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      blockset_canvas_.IsMouseHovering()) {
    tile_selected = true;
  }

  if (tile_selected) {
    // Get mouse position relative to canvas
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 canvas_pos = blockset_canvas_.zero_point();
    ImVec2 mouse_pos =
        ImVec2(io.MousePos.x - canvas_pos.x, io.MousePos.y - canvas_pos.y);

    // Calculate grid position (32x32 tiles in blockset)
    int x_offset = static_cast<int>(mouse_pos.x / 32);
    int grid_y = static_cast<int>(mouse_pos.y / 32);
    int id = x_offset + grid_y * 8;  // 8 tiles per row in blockset

    if (id != current_tile16_ && id >= 0 && id < 512) {
      current_tile16_ = id;
      RETURN_IF_ERROR(tile16_editor_.SetCurrentTile(id));

      // Scroll blockset canvas to show the selected tile
      ScrollBlocksetCanvasToCurrentTile();
    }
  }

  blockset_canvas_.DrawGrid();
  blockset_canvas_.DrawOverlay();

  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

void OverworldEditor::DrawTile8Selector() {
  graphics_bin_canvas_.DrawBackground();
  graphics_bin_canvas_.DrawContextMenu();
  if (all_gfx_loaded_) {
    int key = 0;
    for (auto& value : gfx::Arena::Get().gfx_sheets()) {
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
  if (overworld_.is_loaded()) {
    // Always ensure current map graphics are loaded
    if (!current_graphics_set_.contains(current_map_)) {
      overworld_.set_current_map(current_map_);
      palette_ = overworld_.current_area_palette();
      gfx::Bitmap bmp;
      Renderer::Get().CreateAndRenderBitmap(0x80, kOverworldMapSize, 0x08,
                                            overworld_.current_graphics(), bmp,
                                            palette_);
      current_graphics_set_[current_map_] = bmp;
    }
  }

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##AreaGraphicsScrollRegion");
  current_gfx_canvas_.DrawBackground();
  gui::EndPadding();
  {
    current_gfx_canvas_.DrawContextMenu();
    if (current_graphics_set_.contains(current_map_) &&
        current_graphics_set_[current_map_].is_active()) {
      current_gfx_canvas_.DrawBitmap(current_graphics_set_[current_map_], 2, 2,
                                     2.0f);
    }
    current_gfx_canvas_.DrawTileSelector(32.0f);
    current_gfx_canvas_.DrawGrid();
    current_gfx_canvas_.DrawOverlay();
  }
  EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

// DrawTileSelector() removed - replaced by individual card system in Update()

void OverworldEditor::DrawOverworldEntrances(ImVec2 canvas_p0, ImVec2 scrolling) {
  int i = 0;
  for (auto& each : overworld_.entrances()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted) {
      // Use theme-aware color with proper transparency
      ImVec4 entrance_color = gui::GetEntranceColor();
      if (each.is_hole_) {
        // Holes are more opaque for visibility
        entrance_color.w = 0.78f;  // 200/255 alpha
      }
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16, entrance_color);
      std::string str = util::HexByte(each.entrance_id_);

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
    auto& entrance = overworld_.entrances()[deleted_entrance_id];
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
  for (auto& each : *overworld_.mutable_exits()) {
    if (each.map_id_ < 0x40 + (current_world_ * 0x40) &&
        each.map_id_ >= (current_world_ * 0x40) && !each.deleted_) {
      ow_map_canvas_.DrawRect(each.x_, each.y_, 16, 16,
                              gui::GetExitColor());
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

      std::string str = util::HexByte(i);
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
  for (auto& item : *overworld_.mutable_all_items()) {
    // Get the item's bitmap and real X and Y positions
    if (item.room_map_id_ < 0x40 + (current_world_ * 0x40) &&
        item.room_map_id_ >= (current_world_ * 0x40) && !item.deleted) {
      ow_map_canvas_.DrawRect(item.x_, item.y_, 16, 16, gui::GetItemColor());

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
      if (item.id_ < zelda3::kSecretItemNames.size()) {
        item_name = zelda3::kSecretItemNames[item.id_];
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
  for (auto& sprite : *overworld_.mutable_sprites(game_state_)) {
    // Filter sprites by current world - only show sprites for the current world
    if (!sprite.deleted() && sprite.map_id() < 0x40 + (current_world_ * 0x40) &&
        sprite.map_id() >= (current_world_ * 0x40)) {
      // Sprites are already stored with global coordinates (realX, realY from
      // ROM loading) So we can use sprite.x_ and sprite.y_ directly
      int sprite_x = sprite.x_;
      int sprite_y = sprite.y_;

      // Temporarily update sprite coordinates for entity interaction
      int original_x = sprite.x_;
      int original_y = sprite.y_;

      ow_map_canvas_.DrawRect(sprite_x, sprite_y, kTile16Size, kTile16Size,
                              gui::GetSpriteColor());
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
      if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
        if (sprite_previews_[sprite.id()].is_active()) {
          ow_map_canvas_.DrawBitmap(sprite_previews_[sprite.id()], sprite_x,
                                    sprite_y, 2.0f);
        }
      }

      ow_map_canvas_.DrawText(absl::StrFormat("%s", sprite.name()), sprite_x,
                              sprite_y);

      // Restore original coordinates
      sprite.x_ = original_x;
      sprite.y_ = original_y;
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
  if (core::FeatureFlags::get().overworld.kSaveOverworldMaps) {
    RETURN_IF_ERROR(overworld_.CreateTile32Tilemap());
    RETURN_IF_ERROR(overworld_.SaveMap32Tiles());
    RETURN_IF_ERROR(overworld_.SaveMap16Tiles());
    RETURN_IF_ERROR(overworld_.SaveOverworldMaps());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldEntrances) {
    RETURN_IF_ERROR(overworld_.SaveEntrances());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldExits) {
    RETURN_IF_ERROR(overworld_.SaveExits());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldItems) {
    RETURN_IF_ERROR(overworld_.SaveItems());
  }
  if (core::FeatureFlags::get().overworld.kSaveOverworldProperties) {
    RETURN_IF_ERROR(overworld_.SaveMapProperties());
    RETURN_IF_ERROR(overworld_.SaveMusic());
  }
  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadGraphics() {
  gfx::ScopedTimer timer("LoadGraphics");

  LOG_INFO("OverworldEditor", "Loading overworld.");
  // Load the Link to the Past overworld.
  {
    gfx::ScopedTimer load_timer("Overworld::Load");
    RETURN_IF_ERROR(overworld_.Load(rom_));
  }
  palette_ = overworld_.current_area_palette();

  LOG_INFO("OverworldEditor", "Loading overworld graphics (optimized).");

  // Phase 1: Create bitmaps without textures for faster loading
  // This avoids blocking the main thread with GPU texture creation
  {
    gfx::ScopedTimer gfx_timer("CreateBitmapWithoutTexture_Graphics");
    Renderer::Get().CreateBitmapWithoutTexture(0x80, kOverworldMapSize, 0x40,
                                               overworld_.current_graphics(),
                                               current_gfx_bmp_, palette_);
  }

  LOG_INFO("OverworldEditor", "Loading overworld tileset (deferred textures).");
  {
    gfx::ScopedTimer tileset_timer("CreateBitmapWithoutTexture_Tileset");
    Renderer::Get().CreateBitmapWithoutTexture(
        0x80, 0x2000, 0x08, overworld_.tile16_blockset_data(),
        tile16_blockset_bmp_, palette_);
  }
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_blockset_data = overworld_.tile16_blockset_data();
  LOG_INFO("OverworldEditor", "Loading overworld tile16 graphics.");

  {
    gfx::ScopedTimer tilemap_timer("CreateTilemap");
    tile16_blockset_ =
        gfx::CreateTilemap(tile16_blockset_data, 0x80, 0x2000, kTile16Size,
                           zelda3::kNumTile16Individual, palette_);
  }

  // Phase 2: Create bitmaps only for essential maps initially
  // Non-essential maps will be created on-demand when accessed
  constexpr int kEssentialMapsPerWorld = 8;
  constexpr int kLightWorldEssential = kEssentialMapsPerWorld;
  constexpr int kDarkWorldEssential =
      zelda3::kDarkWorldMapIdStart + kEssentialMapsPerWorld;
  constexpr int kSpecialWorldEssential =
      zelda3::kSpecialWorldMapIdStart + kEssentialMapsPerWorld;

  LOG_INFO("OverworldEditor",
           "Creating bitmaps for essential maps only (first %d maps per world)",
           kEssentialMapsPerWorld);

  std::vector<gfx::Bitmap*> maps_to_texture;
  maps_to_texture.reserve(kEssentialMapsPerWorld *
                          3);  // 8 maps per world * 3 worlds

  {
    gfx::ScopedTimer maps_timer("CreateEssentialOverworldMaps");
    for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
      bool is_essential = false;

      // Check if this is an essential map
      if (i < kLightWorldEssential) {
        is_essential = true;
      } else if (i >= zelda3::kDarkWorldMapIdStart && i < kDarkWorldEssential) {
        is_essential = true;
      } else if (i >= zelda3::kSpecialWorldMapIdStart &&
                 i < kSpecialWorldEssential) {
        is_essential = true;
      }

      if (is_essential) {
        overworld_.set_current_map(i);
        auto palette = overworld_.current_area_palette();
        try {
          // Create bitmap data and surface but defer texture creation
          maps_bmp_[i].Create(kOverworldMapSize, kOverworldMapSize, 0x80,
                              overworld_.current_map_bitmap_data());
          maps_bmp_[i].SetPalette(palette);
          maps_to_texture.push_back(&maps_bmp_[i]);
        } catch (const std::bad_alloc& e) {
          std::cout << "Error allocating map " << i << ": " << e.what()
                    << std::endl;
          continue;
        }
      }
      // Non-essential maps will be created on-demand when accessed
    }
  }

  // Phase 3: Create textures only for currently visible maps
  // Only create textures for the first few maps initially
  const int initial_texture_count =
      std::min(4, static_cast<int>(maps_to_texture.size()));
  {
    gfx::ScopedTimer initial_textures_timer("CreateInitialTextures");
    for (int i = 0; i < initial_texture_count; ++i) {
      Renderer::Get().RenderBitmap(maps_to_texture[i]);
    }
  }

  // Store remaining maps for lazy texture creation
  deferred_map_textures_.assign(maps_to_texture.begin() + initial_texture_count,
                                maps_to_texture.end());

  if (core::FeatureFlags::get().overworld.kDrawOverworldSprites) {
    {
      gfx::ScopedTimer sprites_timer("LoadSpriteGraphics");
      RETURN_IF_ERROR(LoadSpriteGraphics());
    }
  }

  return absl::OkStatus();
}

absl::Status OverworldEditor::LoadSpriteGraphics() {
  // Render the sprites for each Overworld map
  const int depth = 0x10;
  for (int i = 0; i < 3; i++)
    for (auto const& sprite : *overworld_.mutable_sprites(i)) {
      int width = sprite.width();
      int height = sprite.height();
      if (width == 0 || height == 0) {
        continue;
      }
      if (sprite_previews_.size() < sprite.id()) {
        sprite_previews_.resize(sprite.id() + 1);
      }
      sprite_previews_[sprite.id()].Create(width, height, depth,
                                           *sprite.preview_graphics());
      sprite_previews_[sprite.id()].SetPalette(palette_);
      Renderer::Get().RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

void OverworldEditor::ProcessDeferredTextures() {
  std::lock_guard<std::mutex> lock(deferred_textures_mutex_);

  if (deferred_map_textures_.empty()) {
    return;
  }

  // Priority-based loading: process more textures for visible maps
  const int textures_per_frame = 8;  // Increased from 2 to 8 for faster loading
  int processed = 0;

  // First pass: prioritize textures for the current world
  auto it = deferred_map_textures_.begin();
  while (it != deferred_map_textures_.end() && processed < textures_per_frame) {
    if (*it && !(*it)->texture()) {
      // Check if this texture belongs to the current world
      int map_index = -1;
      for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
        if (&maps_bmp_[i] == *it) {
          map_index = i;
          break;
        }
      }

      bool is_current_world = false;
      if (map_index >= 0) {
        int map_world = map_index / 0x40;  // 64 maps per world
        is_current_world = (map_world == current_world_);
      }

      // Prioritize current world maps, but also process others if we have capacity
      if (is_current_world || processed < textures_per_frame / 2) {
        Renderer::Get().RenderBitmap(*it);
        processed++;
        it = deferred_map_textures_.erase(
            it);  // Remove immediately after processing
      } else {
        ++it;
      }
    } else {
      ++it;
    }
  }

  // Second pass: process remaining textures if we still have capacity
  if (processed < textures_per_frame) {
    it = deferred_map_textures_.begin();
    while (it != deferred_map_textures_.end() &&
           processed < textures_per_frame) {
      if (*it && !(*it)->texture()) {
        Renderer::Get().RenderBitmap(*it);
        processed++;
        it = deferred_map_textures_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Third pass: process deferred map refreshes for visible maps
  if (processed < textures_per_frame) {
    for (int i = 0;
         i < zelda3::kNumOverworldMaps && processed < textures_per_frame; ++i) {
      if (maps_bmp_[i].modified() && maps_bmp_[i].is_active()) {
        // Check if this map is visible
        bool is_visible = (i == current_map_) || (i / 0x40 == current_world_);
        if (is_visible) {
          RefreshOverworldMapOnDemand(i);
          processed++;
        }
      }
    }
  }
}

void OverworldEditor::EnsureMapTexture(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  // Ensure the map is built first (on-demand loading)
  auto status = overworld_.EnsureMapBuilt(map_index);
  if (!status.ok()) {
    LOG_ERROR("OverworldEditor", "Failed to build map %d: %s", map_index,
              status.message());
    return;
  }

  auto& bitmap = maps_bmp_[map_index];

  // If bitmap doesn't exist yet (non-essential map), create it now
  if (!bitmap.is_active()) {
    overworld_.set_current_map(map_index);
    auto palette = overworld_.current_area_palette();
    try {
      bitmap.Create(kOverworldMapSize, kOverworldMapSize, 0x80,
                    overworld_.current_map_bitmap_data());
      bitmap.SetPalette(palette);
    } catch (const std::bad_alloc& e) {
      LOG_ERROR("OverworldEditor", "Error allocating bitmap for map %d: %s",
                map_index, e.what());
      return;
    }
  }

  if (!bitmap.texture() && bitmap.is_active()) {
    Renderer::Get().RenderBitmap(&bitmap);

    // Remove from deferred list if it was there
    std::lock_guard<std::mutex> lock(deferred_textures_mutex_);
    auto it = std::find(deferred_map_textures_.begin(),
                        deferred_map_textures_.end(), &bitmap);
    if (it != deferred_map_textures_.end()) {
      deferred_map_textures_.erase(it);
    }
  }
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
  // Use the new on-demand refresh system
  RefreshOverworldMapOnDemand(current_map_);
}

/**
 * @brief On-demand map refresh that only updates what's actually needed
 * 
 * This method intelligently determines what needs to be refreshed based on
 * the type of change and only updates the necessary components, avoiding
 * expensive full rebuilds when possible.
 */
void OverworldEditor::RefreshOverworldMapOnDemand(int map_index) {
  if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
    return;
  }

  // Check if the map is actually visible or being edited
  bool is_current_map = (map_index == current_map_);
  bool is_current_world = (map_index / 0x40 == current_world_);

  // For non-current maps in non-current worlds, defer the refresh
  if (!is_current_map && !is_current_world) {
    // Mark for deferred refresh - will be processed when the map becomes visible
    maps_bmp_[map_index].set_modified(true);
    return;
  }

  // For visible maps, do immediate refresh
  RefreshChildMapOnDemand(map_index);
}

/**
 * @brief On-demand child map refresh with selective updates
 */
void OverworldEditor::RefreshChildMapOnDemand(int map_index) {
  auto* map = overworld_.mutable_overworld_map(map_index);

  // Check what actually needs to be refreshed
  bool needs_graphics_rebuild = maps_bmp_[map_index].modified();
  bool needs_palette_rebuild = false;  // Could be tracked more granularly

  if (needs_graphics_rebuild) {
    // Only rebuild what's actually changed
    map->LoadAreaGraphics();

    // Rebuild tileset only if graphics changed
    auto status = map->BuildTileset();
    if (!status.ok()) {
      LOG_ERROR("OverworldEditor", "Failed to build tileset for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Rebuild tiles16 graphics
    status = map->BuildTiles16Gfx(*overworld_.mutable_tiles16(),
                                  overworld_.tiles16().size());
    if (!status.ok()) {
      LOG_ERROR("OverworldEditor", "Failed to build tiles16 graphics for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Rebuild bitmap
    status = map->BuildBitmap(overworld_.GetMapTiles(current_world_));
    if (!status.ok()) {
      LOG_ERROR("OverworldEditor", "Failed to build bitmap for map %d: %s",
                map_index, status.message().data());
      return;
    }

    // Update bitmap data
    maps_bmp_[map_index].set_data(map->bitmap_data());
    maps_bmp_[map_index].set_modified(false);

    // Validate surface synchronization to help debug crashes
    if (!maps_bmp_[map_index].ValidateDataSurfaceSync()) {
      LOG_WARN("OverworldEditor", "Warning: Surface synchronization issue detected for map %d",
               map_index);
    }

    // Update texture on main thread
    if (maps_bmp_[map_index].texture()) {
      Renderer::Get().UpdateBitmap(&maps_bmp_[map_index]);
    } else {
      // Create texture if it doesn't exist
      EnsureMapTexture(map_index);
    }
  }

  // Handle multi-area maps (large, wide, tall) with safe coordination
  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3 && asm_version != 0xFF);

  if (use_v3_area_sizes) {
    // Use v3 multi-area coordination
    RefreshMultiAreaMapsSafely(map_index, map);
  } else {
    // Legacy logic: only handle large maps for vanilla/v2
    if (map->is_large_map()) {
      RefreshMultiAreaMapsSafely(map_index, map);
    }
  }
}

/**
 * @brief Safely refresh multi-area maps without recursion
 * 
 * This function handles the coordination of large, wide, and tall area maps
 * by using a non-recursive approach with explicit map list processing.
 * It respects the ZScream area size logic and prevents infinite recursion.
 */
void OverworldEditor::RefreshMultiAreaMapsSafely(int map_index,
                                                 zelda3::OverworldMap* map) {
  using zelda3::AreaSizeEnum;

  // Skip if this is already a processed sibling to avoid double-processing
  static std::set<int> currently_processing;
  if (currently_processing.count(map_index)) {
    return;
  }

  auto area_size = map->area_size();
  if (area_size == AreaSizeEnum::SmallArea) {
    return;  // No siblings to coordinate
  }

  LOG_DEBUG("OverworldEditor",
            "RefreshMultiAreaMapsSafely: Processing %s area map %d (parent: %d)",
            (area_size == AreaSizeEnum::LargeArea)  ? "large"
            : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                    : "tall",
            map_index, map->parent());

  // Determine all maps that are part of this multi-area structure
  std::vector<int> sibling_maps;
  int parent_id = map->parent();

  // Use the same logic as ZScream for area coordination
  switch (area_size) {
    case AreaSizeEnum::LargeArea: {
      // Large Area: 2x2 grid (4 maps total)
      // Parent is top-left (quadrant 0), siblings are:
      // +1 (top-right, quadrant 1), +8 (bottom-left, quadrant 2), +9 (bottom-right, quadrant 3)
      sibling_maps = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      LOG_DEBUG("OverworldEditor",
                "RefreshMultiAreaMapsSafely: Large area siblings: %d, %d, %d, %d",
                parent_id, parent_id + 1, parent_id + 8, parent_id + 9);
      break;
    }

    case AreaSizeEnum::WideArea: {
      // Wide Area: 2x1 grid (2 maps total, horizontally adjacent)
      // Parent is left, sibling is +1 (right)
      sibling_maps = {parent_id, parent_id + 1};
      LOG_DEBUG("OverworldEditor",
                "RefreshMultiAreaMapsSafely: Wide area siblings: %d, %d",
                parent_id, parent_id + 1);
      break;
    }

    case AreaSizeEnum::TallArea: {
      // Tall Area: 1x2 grid (2 maps total, vertically adjacent)
      // Parent is top, sibling is +8 (bottom)
      sibling_maps = {parent_id, parent_id + 8};
      LOG_DEBUG("OverworldEditor",
                "RefreshMultiAreaMapsSafely: Tall area siblings: %d, %d",
                parent_id, parent_id + 8);
      break;
    }

    default:
      LOG_WARN("OverworldEditor",
               "RefreshMultiAreaMapsSafely: Unknown area size %d for map %d",
               static_cast<int>(area_size), map_index);
      return;
  }

  // Mark all siblings as being processed to prevent recursion
  for (int sibling : sibling_maps) {
    currently_processing.insert(sibling);
  }

  // Only refresh siblings that are visible/current and need updating
  for (int sibling : sibling_maps) {
    if (sibling == map_index) {
      continue;  // Skip self (already processed above)
    }

    // Bounds check
    if (sibling < 0 || sibling >= zelda3::kNumOverworldMaps) {
      continue;
    }

    // Only refresh if it's visible or current
    bool is_current_map = (sibling == current_map_);
    bool is_current_world = (sibling / 0x40 == current_world_);
    bool needs_refresh = maps_bmp_[sibling].modified();

    if ((is_current_map || is_current_world) && needs_refresh) {
      LOG_DEBUG("OverworldEditor",
                "RefreshMultiAreaMapsSafely: Refreshing %s area sibling map %d "
                "(parent: %d)",
                (area_size == AreaSizeEnum::LargeArea)  ? "large"
                : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                        : "tall",
                sibling, parent_id);

      // Direct refresh without calling RefreshChildMapOnDemand to avoid recursion
      auto* sibling_map = overworld_.mutable_overworld_map(sibling);
      if (sibling_map && maps_bmp_[sibling].modified()) {
        sibling_map->LoadAreaGraphics();

        auto status = sibling_map->BuildTileset();
        if (status.ok()) {
          status = sibling_map->BuildTiles16Gfx(*overworld_.mutable_tiles16(),
                                                overworld_.tiles16().size());
          if (status.ok()) {
            // Load palette for the sibling map
            status = sibling_map->LoadPalette();
            if (status.ok()) {
              status = sibling_map->BuildBitmap(
                  overworld_.GetMapTiles(current_world_));
              if (status.ok()) {
                maps_bmp_[sibling].set_data(sibling_map->bitmap_data());
                maps_bmp_[sibling].SetPalette(
                    overworld_.current_area_palette());
                maps_bmp_[sibling].set_modified(false);

                // Update texture if it exists
                if (maps_bmp_[sibling].texture()) {
                  core::Renderer::Get().UpdateBitmap(&maps_bmp_[sibling]);
                } else {
                  EnsureMapTexture(sibling);
                }
              }
            }
          }
        }

        if (!status.ok()) {
          LOG_ERROR("OverworldEditor",
                    "RefreshMultiAreaMapsSafely: Failed to refresh sibling map %d: "
                    "%s",
                    sibling, status.message().data());
        }
      }
    } else if (!is_current_map && !is_current_world) {
      // Mark non-visible siblings for deferred refresh
      maps_bmp_[sibling].set_modified(true);
    }
  }

  // Clear processing set after completion
  for (int sibling : sibling_maps) {
    currently_processing.erase(sibling);
  }
}

absl::Status OverworldEditor::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_.mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_.current_area_palette();

  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3 && asm_version != 0xFF);

  if (use_v3_area_sizes) {
    // Use v3 area size system
    using zelda3::AreaSizeEnum;
    auto area_size = overworld_.overworld_map(current_map_)->area_size();

    if (area_size != AreaSizeEnum::SmallArea) {
      // Get all sibling maps that need palette updates
      std::vector<int> sibling_maps;
      int parent_id = overworld_.overworld_map(current_map_)->parent();

      switch (area_size) {
        case AreaSizeEnum::LargeArea:
          // 2x2 grid: parent, parent+1, parent+8, parent+9
          sibling_maps = {parent_id, parent_id + 1, parent_id + 8,
                          parent_id + 9};
          break;
        case AreaSizeEnum::WideArea:
          // 2x1 grid: parent, parent+1
          sibling_maps = {parent_id, parent_id + 1};
          break;
        case AreaSizeEnum::TallArea:
          // 1x2 grid: parent, parent+8
          sibling_maps = {parent_id, parent_id + 8};
          break;
        default:
          break;
      }

      // Update palette for all siblings
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        RETURN_IF_ERROR(
            overworld_.mutable_overworld_map(sibling_index)->LoadPalette());
        maps_bmp_[sibling_index].SetPalette(current_map_palette);
      }
    } else {
      // Small area - only update current map
      maps_bmp_[current_map_].SetPalette(current_map_palette);
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (overworld_.overworld_map(current_map_)->is_large_map()) {
      // We need to update the map and its siblings if it's a large map
      for (int i = 1; i < 4; i++) {
        int sibling_index =
            overworld_.overworld_map(current_map_)->parent() + i;
        if (i >= 2)
          sibling_index += 6;
        RETURN_IF_ERROR(
            overworld_.mutable_overworld_map(sibling_index)->LoadPalette());
        maps_bmp_[sibling_index].SetPalette(current_map_palette);
      }
    }
    maps_bmp_[current_map_].SetPalette(current_map_palette);
  }

  return absl::OkStatus();
}

void OverworldEditor::RefreshMapProperties() {
  const auto& current_ow_map = *overworld_.mutable_overworld_map(current_map_);

  // Check if ZSCustomOverworld v3 is present
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  bool use_v3_area_sizes = (asm_version >= 3);

  if (use_v3_area_sizes) {
    // Use v3 area size system
    using zelda3::AreaSizeEnum;
    auto area_size = current_ow_map.area_size();

    if (area_size != AreaSizeEnum::SmallArea) {
      // Get all sibling maps that need property updates
      std::vector<int> sibling_maps;
      int parent_id = current_ow_map.parent();

      switch (area_size) {
        case AreaSizeEnum::LargeArea:
          // 2x2 grid: parent+1, parent+8, parent+9 (skip parent itself)
          sibling_maps = {parent_id + 1, parent_id + 8, parent_id + 9};
          break;
        case AreaSizeEnum::WideArea:
          // 2x1 grid: parent+1 (skip parent itself)
          sibling_maps = {parent_id + 1};
          break;
        case AreaSizeEnum::TallArea:
          // 1x2 grid: parent+8 (skip parent itself)
          sibling_maps = {parent_id + 8};
          break;
        default:
          break;
      }

      // Copy properties from parent map to all siblings
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        auto& map = *overworld_.mutable_overworld_map(sibling_index);
        map.set_area_graphics(current_ow_map.area_graphics());
        map.set_area_palette(current_ow_map.area_palette());
        map.set_sprite_graphics(game_state_,
                                current_ow_map.sprite_graphics(game_state_));
        map.set_sprite_palette(game_state_,
                               current_ow_map.sprite_palette(game_state_));
        map.set_message_id(current_ow_map.message_id());
      }
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    if (current_ow_map.is_large_map()) {
      // We need to copy the properties from the parent map to the children
      for (int i = 1; i < 4; i++) {
        int sibling_index = current_ow_map.parent() + i;
        if (i >= 2) {
          sibling_index += 6;
        }
        auto& map = *overworld_.mutable_overworld_map(sibling_index);
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
}

absl::Status OverworldEditor::RefreshTile16Blockset() {
  LOG_DEBUG("OverworldEditor", "RefreshTile16Blockset called");
  if (current_blockset_ ==
      overworld_.overworld_map(current_map_)->area_graphics()) {
    return absl::OkStatus();
  }
  current_blockset_ = overworld_.overworld_map(current_map_)->area_graphics();

  overworld_.set_current_map(current_map_);
  palette_ = overworld_.current_area_palette();

  const auto tile16_data = overworld_.tile16_blockset_data();

  gfx::UpdateTilemap(tile16_blockset_, tile16_data);
  tile16_blockset_.atlas.SetPalette(palette_);
  return absl::OkStatus();
}

void OverworldEditor::HandleMapInteraction() {
  // Handle middle-click for map interaction instead of right-click
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) &&
      ImGui::IsItemHovered()) {
    // Get the current map from mouse position
    auto mouse_position = ow_map_canvas_.drawn_tile_position();
    int map_x = mouse_position.x / kOverworldMapSize;
    int map_y = mouse_position.y / kOverworldMapSize;
    int hovered_map = map_x + map_y * 8;
    if (current_world_ == 1) {
      hovered_map += 0x40;
    } else if (current_world_ == 2) {
      hovered_map += 0x80;
    }

    // Only interact if we're hovering over a valid map
    if (hovered_map >= 0 && hovered_map < 0xA0) {
      // Toggle map lock or open properties panel
      if (current_map_lock_ && current_map_ == hovered_map) {
        current_map_lock_ = false;
      } else {
        current_map_lock_ = true;
        current_map_ = hovered_map;
        show_map_properties_panel_ = true;
      }
    }
  }

  // Handle double-click to open properties panel
  if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      ImGui::IsItemHovered()) {
    show_map_properties_panel_ = true;
  }
}

void OverworldEditor::SetupOverworldCanvasContextMenu() {
  // Clear any existing context menu items
  ow_map_canvas_.ClearContextMenuItems();

  // Add overworld-specific context menu items
  gui::Canvas::ContextMenuItem lock_item;
  lock_item.label = current_map_lock_ ? "Unlock Map" : "Lock to This Map";
  lock_item.callback = [this]() {
    current_map_lock_ = !current_map_lock_;
    if (current_map_lock_) {
      // Get the current map from mouse position
      auto mouse_position = ow_map_canvas_.drawn_tile_position();
      int map_x = mouse_position.x / kOverworldMapSize;
      int map_y = mouse_position.y / kOverworldMapSize;
      int hovered_map = map_x + map_y * 8;
      if (current_world_ == 1) {
        hovered_map += 0x40;
      } else if (current_world_ == 2) {
        hovered_map += 0x80;
      }
      if (hovered_map >= 0 && hovered_map < 0xA0) {
        current_map_ = hovered_map;
      }
    }
  };
  ow_map_canvas_.AddContextMenuItem(lock_item);

  // Map Properties
  gui::Canvas::ContextMenuItem properties_item;
  properties_item.label = "Map Properties";
  properties_item.callback = [this]() {
    show_map_properties_panel_ = true;
  };
  ow_map_canvas_.AddContextMenuItem(properties_item);

  // Custom overworld features (only show if v3+)
  static uint8_t asm_version =
      (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (asm_version >= 3 && asm_version != 0xFF) {
    // Custom Background Color
    gui::Canvas::ContextMenuItem bg_color_item;
    bg_color_item.label = "Custom Background Color";
    bg_color_item.callback = [this]() {
      show_custom_bg_color_editor_ = true;
    };
    ow_map_canvas_.AddContextMenuItem(bg_color_item);

    // Overlay Settings
    gui::Canvas::ContextMenuItem overlay_item;
    overlay_item.label = "Overlay Settings";
    overlay_item.callback = [this]() {
      show_overlay_editor_ = true;
    };
    ow_map_canvas_.AddContextMenuItem(overlay_item);
  }

  // Map editing controls
  gui::Canvas::ContextMenuItem refresh_map_item;
  refresh_map_item.label = "Refresh Map Changes";
  refresh_map_item.callback = [this]() {
    RefreshOverworldMap();
    auto status = RefreshTile16Blockset();
    if (!status.ok()) {
      LOG_ERROR("OverworldEditor", "Failed to refresh tile16 blockset: %s",
                status.message().data());
    }
  };
  ow_map_canvas_.AddContextMenuItem(refresh_map_item);

  // Canvas controls
  gui::Canvas::ContextMenuItem reset_pos_item;
  reset_pos_item.label = "Reset Canvas Position";
  reset_pos_item.callback = [this]() {
    ow_map_canvas_.set_scrolling(ImVec2(0, 0));
  };
  ow_map_canvas_.AddContextMenuItem(reset_pos_item);

  gui::Canvas::ContextMenuItem zoom_fit_item;
  zoom_fit_item.label = "Zoom to Fit";
  zoom_fit_item.callback = [this]() {
    ow_map_canvas_.set_global_scale(1.0f);
    ow_map_canvas_.set_scrolling(ImVec2(0, 0));
  };
  ow_map_canvas_.AddContextMenuItem(zoom_fit_item);
}

void OverworldEditor::ScrollBlocksetCanvasToCurrentTile() {
  // Calculate the position of the current tile in the blockset canvas
  // Blockset is arranged in an 8-tile-per-row grid, each tile is 16x16 pixels
  constexpr int kTilesPerRow = 8;
  constexpr int kTileDisplaySize =
      32;  // Each tile displayed at 32x32 (16x16 at 2x scale)

  // Calculate tile position in canvas coordinates (absolute position in the grid)
  int tile_col = current_tile16_ % kTilesPerRow;
  int tile_row = current_tile16_ / kTilesPerRow;
  float tile_x = static_cast<float>(tile_col * kTileDisplaySize);
  float tile_y = static_cast<float>(tile_row * kTileDisplaySize);

  // Get the canvas dimensions
  ImVec2 canvas_size = blockset_canvas_.canvas_size();

  // Calculate the scroll position to center the tile in the viewport
  float scroll_x = tile_x - (canvas_size.x / 2.0F) + (kTileDisplaySize / 2.0F);
  float scroll_y = tile_y - (canvas_size.y / 2.0F) + (kTileDisplaySize / 2.0F);

  // Clamp scroll to valid ranges (don't scroll beyond bounds)
  if (scroll_x < 0)
    scroll_x = 0;
  if (scroll_y < 0)
    scroll_y = 0;

  // Update the blockset canvas scrolling position first
  blockset_canvas_.set_scrolling(ImVec2(-1, -scroll_y));

  // Set the points to draw the white outline box around the current tile
  // Points are in canvas coordinates (not screen coordinates)
  // blockset_canvas_.mutable_points()->clear();
  // blockset_canvas_.mutable_points()->push_back(ImVec2(tile_x, tile_y));
  // blockset_canvas_.mutable_points()->push_back(ImVec2(tile_x + kTileDisplaySize, tile_y + kTileDisplaySize));
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
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_GFX);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  Text("Sprite Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART1);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART2);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART2);
  ImGui::Separator();

  Text("Area Pal LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_PAL);
  SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
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
  if (BeginTable("UsageStatsTable", 3, 
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter, 
                 ImVec2(0, 0))) {
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
        auto entrance_name = rom_->resource_label()->CreateOrGetLabel(
            "Dungeon Entrance Names", util::HexByte(i),
            zelda3::kEntranceNames[i]);
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
  int total_squares = 128;
  int squares_wide = 8;
  int squares_tall = (total_squares + squares_wide - 1) /
                     squares_wide;  // Ceiling of total_squares/squares_wide

  // Loop through each row
  for (int row = 0; row < squares_tall; ++row) {
    NewLine();

    for (int col = 0; col < squares_wide; ++col) {
      if (row * squares_wide + col >= total_squares) {
        break;
      }
      // Determine if this square should be highlighted
      bool highlight = selected_usage_map_ == (row * squares_wide + col);

      // Set highlight color if needed
      if (highlight) {
        PushStyleColor(ImGuiCol_Button, gui::GetSelectedColor());
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

absl::Status OverworldEditor::Clear() {
  overworld_.Destroy();
  current_graphics_set_.clear();
  all_gfx_loaded_ = false;
  map_blockset_loaded_ = false;
  return absl::OkStatus();
}

absl::Status OverworldEditor::ApplyZSCustomOverworldASM(int target_version) {
  if (!core::FeatureFlags::get().overworld.kApplyZSCustomOverworldASM) {
    return absl::FailedPreconditionError(
        "ZSCustomOverworld ASM application is disabled in feature flags");
  }

  // Validate target version
  if (target_version < 2 || target_version > 3) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Invalid target version: %d. Must be 2 or 3.", target_version));
  }

  // Check current ROM version
  uint8_t current_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  if (current_version != 0xFF && current_version >= target_version) {
    return absl::AlreadyExistsError(absl::StrFormat(
        "ROM is already version %d or higher", current_version));
  }

  LOG_INFO("OverworldEditor", "Applying ZSCustomOverworld ASM v%d to ROM...",
           target_version);

  // Initialize Asar wrapper
  auto asar_wrapper = std::make_unique<core::AsarWrapper>();
  RETURN_IF_ERROR(asar_wrapper->Initialize());

  // Create backup of ROM data
  std::vector<uint8_t> original_rom_data = rom_->vector();
  std::vector<uint8_t> working_rom_data = original_rom_data;

  try {
    // Determine which ASM file to apply
    std::string asm_file_path;
    if (target_version == 3) {
      asm_file_path = "assets/asm/yaze.asm";  // Master file with v3
    } else {
      asm_file_path = "assets/asm/ZSCustomOverworld.asm";  // v2 standalone
    }

    // Check if ASM file exists
    if (!std::filesystem::exists(asm_file_path)) {
      return absl::NotFoundError(
          absl::StrFormat("ASM file not found: %s", asm_file_path));
    }

    // Apply the ASM patch
    auto patch_result =
        asar_wrapper->ApplyPatch(asm_file_path, working_rom_data);
    if (!patch_result.ok()) {
      return absl::InternalError(absl::StrFormat(
          "Failed to apply ASM patch: %s", patch_result.status().message()));
    }

    const auto& result = patch_result.value();
    if (!result.success) {
      std::string error_details = "ASM patch failed with errors:\n";
      for (const auto& error : result.errors) {
        error_details += "  - " + error + "\n";
      }
      if (!result.warnings.empty()) {
        error_details += "Warnings:\n";
        for (const auto& warning : result.warnings) {
          error_details += "  - " + warning + "\n";
        }
      }
      return absl::InternalError(error_details);
    }

    // Update ROM with patched data
    RETURN_IF_ERROR(rom_->LoadFromData(working_rom_data, false));

    // Update version marker and feature flags
    RETURN_IF_ERROR(UpdateROMVersionMarkers(target_version));

    // Log symbols found during patching
    LOG_INFO("OverworldEditor", "ASM patch applied successfully. Found %zu symbols:",
             result.symbols.size());
    for (const auto& symbol : result.symbols) {
      LOG_INFO("OverworldEditor", "  %s @ $%06X", symbol.name.c_str(),
               symbol.address);
    }

    // Refresh overworld data to reflect changes
    RETURN_IF_ERROR(overworld_.Load(rom_));

    LOG_INFO("OverworldEditor", "ZSCustomOverworld v%d successfully applied to ROM",
             target_version);
    return absl::OkStatus();

  } catch (const std::exception& e) {
    // Restore original ROM data on any exception
    auto restore_result = rom_->LoadFromData(original_rom_data, false);
    if (!restore_result.ok()) {
      LOG_ERROR("OverworldEditor", "Failed to restore ROM data: %s",
                restore_result.message().data());
    }
    return absl::InternalError(
        absl::StrFormat("Exception during ASM application: %s", e.what()));
  }
}

absl::Status OverworldEditor::UpdateROMVersionMarkers(int target_version) {
  // Set the main version marker
  (*rom_)[zelda3::OverworldCustomASMHasBeenApplied] =
      static_cast<uint8_t>(target_version);

  // Enable feature flags based on target version
  if (target_version >= 2) {
    // v2+ features
    (*rom_)[zelda3::OverworldCustomAreaSpecificBGEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomMainPaletteEnabled] = 0x01;

    LOG_INFO("OverworldEditor", "Enabled v2+ features: Custom BG colors, Main palettes");
  }

  if (target_version >= 3) {
    // v3 features
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomAnimatedGFXEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomTileGFXGroupEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomMosaicEnabled] = 0x01;

    LOG_INFO("OverworldEditor",
             "Enabled v3+ features: Subscreen overlays, Animated GFX, Tile GFX "
             "groups, Mosaic");

    // Initialize area size data for v3 (set all areas to small by default)
    for (int i = 0; i < 0xA0; i++) {
      (*rom_)[zelda3::kOverworldScreenSize + i] =
          static_cast<uint8_t>(zelda3::AreaSizeEnum::SmallArea);
    }

    // Set appropriate sizes for known large areas
    const std::vector<int> large_areas = {
        0x00, 0x02, 0x05, 0x07, 0x0A, 0x0B, 0x0F, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1D,
        0x1E, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x30,
        0x32, 0x33, 0x34, 0x35, 0x37, 0x3A, 0x3B, 0x3C, 0x3F};

    for (int area_id : large_areas) {
      if (area_id < 0xA0) {
        (*rom_)[zelda3::kOverworldScreenSize + area_id] =
            static_cast<uint8_t>(zelda3::AreaSizeEnum::LargeArea);
      }
    }

    LOG_INFO("OverworldEditor", "Initialized area size data for %zu areas",
             large_areas.size());
  }

  LOG_INFO("OverworldEditor", "ROM version markers updated to v%d", target_version);
  return absl::OkStatus();
}

}  // namespace yaze::editor