#include "overworld_editor.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <ostream>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/entity_operations.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "app/rom.h"
#include "canvas/canvas_usage_tracker.h"
#include "core/asar_wrapper.h"
#include "core/features.h"
#include "editor/overworld/overworld_entity_renderer.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/file_util.h"
#include "util/hex.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

void OverworldEditor::Initialize() {
  // Register cards with EditorCardRegistry (dependency injection)
  if (!dependencies_.card_registry) {
    return;
  }
  auto* card_registry = dependencies_.card_registry;

  // Register Overworld Canvas (main canvas card with toolset)
  card_registry->RegisterCard({
      .card_id = MakeCardId("overworld.canvas"),
      .display_name = "Overworld Canvas",
      .icon = ICON_MD_MAP,
      .category = "Overworld",
      .shortcut_hint = "Ctrl+Shift+O",
      .visibility_flag = &show_overworld_canvas_,
      .priority = 5  // Show first, most important
  });

  card_registry->RegisterCard(
      {.card_id = MakeCardId("overworld.tile16_selector"),
       .display_name = "Tile16 Selector",
       .icon = ICON_MD_GRID_ON,
       .category = "Overworld",
       .shortcut_hint = "Ctrl+Alt+1",
       .visibility_flag = &show_tile16_selector_,
       .priority = 10});

  card_registry->RegisterCard(
      {.card_id = MakeCardId("overworld.tile8_selector"),
       .display_name = "Tile8 Selector",
       .icon = ICON_MD_GRID_3X3,
       .category = "Overworld",
       .shortcut_hint = "Ctrl+Alt+2",
       .visibility_flag = &show_tile8_selector_,
       .priority = 20});

  card_registry->RegisterCard({.card_id = MakeCardId("overworld.area_graphics"),
                               .display_name = "Area Graphics",
                               .icon = ICON_MD_IMAGE,
                               .category = "Overworld",
                               .shortcut_hint = "Ctrl+Alt+3",
                               .visibility_flag = &show_area_gfx_,
                               .priority = 30});

  card_registry->RegisterCard({.card_id = MakeCardId("overworld.scratch"),
                               .display_name = "Scratch Workspace",
                               .icon = ICON_MD_DRAW,
                               .category = "Overworld",
                               .shortcut_hint = "Ctrl+Alt+4",
                               .visibility_flag = &show_scratch_,
                               .priority = 40});

  card_registry->RegisterCard({.card_id = MakeCardId("overworld.gfx_groups"),
                               .display_name = "GFX Groups",
                               .icon = ICON_MD_FOLDER,
                               .category = "Overworld",
                               .shortcut_hint = "Ctrl+Alt+5",
                               .visibility_flag = &show_gfx_groups_,
                               .priority = 50});

  card_registry->RegisterCard({.card_id = MakeCardId("overworld.usage_stats"),
                               .display_name = "Usage Statistics",
                               .icon = ICON_MD_ANALYTICS,
                               .category = "Overworld",
                               .shortcut_hint = "Ctrl+Alt+6",
                               .visibility_flag = &show_usage_stats_,
                               .priority = 60});

  card_registry->RegisterCard({.card_id = MakeCardId("overworld.v3_settings"),
                               .display_name = "v3 Settings",
                               .icon = ICON_MD_SETTINGS,
                               .category = "Overworld",
                               .shortcut_hint = "Ctrl+Alt+7",
                               .visibility_flag = &show_v3_settings_,
                               .priority = 70});

  // Original initialization code below:
  // Initialize MapPropertiesSystem with canvas and bitmap data
  map_properties_system_ = std::make_unique<MapPropertiesSystem>(
      &overworld_, rom_, &maps_bmp_, &ow_map_canvas_);

  // Set up refresh callbacks for MapPropertiesSystem
  map_properties_system_->SetRefreshCallbacks(
      [this]() { this->RefreshMapProperties(); },
      [this]() { this->RefreshOverworldMap(); },
      [this]() -> absl::Status { return this->RefreshMapPalette(); },
      [this]() -> absl::Status { return this->RefreshTile16Blockset(); },
      [this](int map_index) { this->ForceRefreshGraphics(map_index); });

  // Initialize OverworldEntityRenderer for entity visualization
  entity_renderer_ = std::make_unique<OverworldEntityRenderer>(
      &overworld_, &ow_map_canvas_, &sprite_previews_);

  // Setup Canvas Automation API callbacks (Phase 4)
  SetupCanvasAutomation();

}

absl::Status OverworldEditor::Load() {
  gfx::ScopedTimer timer("OverworldEditor::Load");

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
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

    LOG_DEBUG("OverworldEditor",
              "Overworld editor refreshed after Tile16 changes");
    return absl::OkStatus();
  });

  // Set up entity insertion callback for MapPropertiesSystem
  if (map_properties_system_) {
    map_properties_system_->SetEntityCallbacks(
        [this](const std::string& entity_type) {
          HandleEntityInsertion(entity_type);
        });
  }

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

  // Create session-aware cards (non-static for multi-session support)
  gui::EditorCard overworld_canvas_card(
      MakeCardTitle("Overworld Canvas").c_str(), ICON_MD_PUBLIC);
  gui::EditorCard tile16_card(MakeCardTitle("Tile16 Selector").c_str(),
                              ICON_MD_GRID_3X3);
  gui::EditorCard tile8_card(MakeCardTitle("Tile8 Selector").c_str(),
                             ICON_MD_GRID_4X4);
  gui::EditorCard area_gfx_card(MakeCardTitle("Area Graphics").c_str(),
                                ICON_MD_IMAGE);
  gui::EditorCard scratch_card(MakeCardTitle("Scratch Space").c_str(),
                               ICON_MD_BRUSH);
  gui::EditorCard tile16_editor_card(MakeCardTitle("Tile16 Editor").c_str(),
                                     ICON_MD_GRID_ON);
  gui::EditorCard gfx_groups_card(MakeCardTitle("Graphics Groups").c_str(),
                                  ICON_MD_COLLECTIONS);
  gui::EditorCard usage_stats_card(MakeCardTitle("Usage Statistics").c_str(),
                                   ICON_MD_ANALYTICS);
  gui::EditorCard v3_settings_card(MakeCardTitle("v3 Settings").c_str(),
                                   ICON_MD_TUNE);

  // Configure card positions (these settings persist via imgui.ini)
  static bool cards_configured = false;
  if (!cards_configured) {
    // Position cards for optimal workflow
    overworld_canvas_card.SetDefaultSize(1000, 700);
    overworld_canvas_card.SetPosition(gui::EditorCard::Position::Floating);

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

    cards_configured = true;
  }

  // Main canvas (full width when cards are docked)
  if (show_overworld_canvas_) {
    if (overworld_canvas_card.Begin(&show_overworld_canvas_)) {
      DrawToolset();
      DrawOverworldCanvas();
    }
    overworld_canvas_card.End();  // ALWAYS call End after Begin
  }

  // Floating tile selector cards (4 tabs converted to separate cards)
  if (show_tile16_selector_) {
    if (tile16_card.Begin(&show_tile16_selector_)) {
      status_ = DrawTile16Selector();
    }
    tile16_card.End();  // ALWAYS call End after Begin
  }

  if (show_tile8_selector_) {
    if (tile8_card.Begin(&show_tile8_selector_)) {
      gui::BeginPadding(3);
      gui::BeginChildWithScrollbar("##Tile8SelectorScrollRegion");
      DrawTile8Selector();
      ImGui::EndChild();
      gui::EndNoPadding();
    }
    tile8_card.End();  // ALWAYS call End after Begin
  }

  if (show_area_gfx_) {
    if (area_gfx_card.Begin(&show_area_gfx_)) {
      status_ = DrawAreaGraphics();
    }
    area_gfx_card.End();  // ALWAYS call End after Begin
  }

  if (show_scratch_) {
    if (scratch_card.Begin(&show_scratch_)) {
      status_ = DrawScratchSpace();
    }
    scratch_card.End();  // ALWAYS call End after Begin
  }

  // Tile16 Editor popup-only (no tab)
  if (show_tile16_editor_) {
    if (tile16_editor_card.Begin(&show_tile16_editor_)) {
      if (rom_->is_loaded()) {
        status_ = tile16_editor_.Update();
      } else {
        gui::CenterText("No ROM loaded");
      }
    }
    tile16_editor_card.End();  // ALWAYS call End after Begin
  }

  // Graphics Groups popup
  if (show_gfx_groups_) {
    if (gfx_groups_card.Begin(&show_gfx_groups_)) {
      if (rom_->is_loaded()) {
        status_ = gfx_group_editor_.Update();
      } else {
        gui::CenterText("No ROM loaded");
      }
    }
    gfx_groups_card.End();  // ALWAYS call End after Begin
  }

  // Usage Statistics popup
  if (show_usage_stats_) {
    if (usage_stats_card.Begin(&show_usage_stats_)) {
      if (rom_->is_loaded()) {
        status_ = UpdateUsageStats();
      } else {
        gui::CenterText("No ROM loaded");
      }
    }
    usage_stats_card.End();  // ALWAYS call End after Begin
  }

  // Area Configuration Panel (detailed editing)
  if (show_map_properties_panel_) {
    ImGui::SetNextWindowSize(ImVec2(650, 750), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_TUNE " Area Configuration###AreaConfig",
                     &show_map_properties_panel_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() &&
          map_properties_system_) {
        map_properties_system_->DrawMapPropertiesPanel(
            current_map_, show_map_properties_panel_);
      }
    }
    ImGui::End();
  }

  // Custom Background Color Editor
  if (show_custom_bg_color_editor_) {
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_FORMAT_COLOR_FILL " Background Color",
                     &show_custom_bg_color_editor_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() &&
          map_properties_system_) {
        map_properties_system_->DrawCustomBackgroundColorEditor(
            current_map_, show_custom_bg_color_editor_);
      }
    }
    ImGui::End();
  }

  // Visual Effects Editor (Subscreen Overlays)
  if (show_overlay_editor_) {
    ImGui::SetNextWindowSize(ImVec2(500, 450), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(ICON_MD_LAYERS " Visual Effects Editor###OverlayEditor",
                     &show_overlay_editor_)) {
      if (rom_->is_loaded() && overworld_.is_loaded() &&
          map_properties_system_) {
        map_properties_system_->DrawOverlayEditor(current_map_,
                                                  show_overlay_editor_);
      }
    }
    ImGui::End();
  }

  // --- BEGIN CENTRALIZED INTERACTION LOGIC ---
  auto* hovered_entity = entity_renderer_->hovered_entity();

  // Handle all MOUSE mode interactions here
  if (current_mode == EditingMode::MOUSE) {
    // --- CONTEXT MENUS & POPOVERS ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      if (hovered_entity) {
        current_entity_ = hovered_entity;
        switch (hovered_entity->entity_type_) {
          case zelda3::GameEntity::EntityType::kExit:
            current_exit_ =
                *static_cast<zelda3::OverworldExit*>(hovered_entity);
            ImGui::OpenPopup("Exit editor");
            break;
          case zelda3::GameEntity::EntityType::kEntrance:
            current_entrance_ =
                *static_cast<zelda3::OverworldEntrance*>(hovered_entity);
            ImGui::OpenPopup("Entrance Editor");
            break;
          case zelda3::GameEntity::EntityType::kItem:
            current_item_ =
                *static_cast<zelda3::OverworldItem*>(hovered_entity);
            ImGui::OpenPopup("Item editor");
            break;
          case zelda3::GameEntity::EntityType::kSprite:
            current_sprite_ = *static_cast<zelda3::Sprite*>(hovered_entity);
            ImGui::OpenPopup("Sprite editor");
            break;
          default:
            break;
        }
      }
    }

    // --- DOUBLE-CLICK ACTIONS ---
    if (hovered_entity && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      if (hovered_entity->entity_type_ ==
          zelda3::GameEntity::EntityType::kExit) {
        jump_to_tab_ =
            static_cast<zelda3::OverworldExit*>(hovered_entity)->room_id_;
      } else if (hovered_entity->entity_type_ ==
                 zelda3::GameEntity::EntityType::kEntrance) {
        jump_to_tab_ = static_cast<zelda3::OverworldEntrance*>(hovered_entity)
                           ->entrance_id_;
      }
    }
  }

  // --- DRAW POPUPS ---
  if (DrawExitEditorPopup(current_exit_)) {
    if (current_entity_ && current_entity_->entity_type_ ==
                               zelda3::GameEntity::EntityType::kExit) {
      *static_cast<zelda3::OverworldExit*>(current_entity_) = current_exit_;
      rom_->set_dirty(true);
    }
  }
  if (DrawOverworldEntrancePopup(current_entrance_)) {
    if (current_entity_ && current_entity_->entity_type_ ==
                               zelda3::GameEntity::EntityType::kEntrance) {
      *static_cast<zelda3::OverworldEntrance*>(current_entity_) =
          current_entrance_;
      rom_->set_dirty(true);
    }
  }
  if (DrawItemEditorPopup(current_item_)) {
    if (current_entity_ && current_entity_->entity_type_ ==
                               zelda3::GameEntity::EntityType::kItem) {
      *static_cast<zelda3::OverworldItem*>(current_entity_) = current_item_;
      rom_->set_dirty(true);
    }
  }
  if (DrawSpriteEditorPopup(current_sprite_)) {
    if (current_entity_ && current_entity_->entity_type_ ==
                               zelda3::GameEntity::EntityType::kSprite) {
      *static_cast<zelda3::Sprite*>(current_entity_) = current_sprite_;
      rom_->set_dirty(true);
    }
  }
  // --- END CENTRALIZED LOGIC ---

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

  // IMPORTANT: Don't make asm_version static - it needs to update after ROM upgrade
  uint8_t asm_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];

  // Don't use WidgetIdScope here - it conflicts with ImGui::Begin/End ID stack in cards
  // Widgets register themselves individually instead

  toolbar.Begin();

  // Mode buttons - simplified to 2 modes
  toolbar.BeginModeGroup();

  if (toolbar.ModeButton(
          ICON_MD_MOUSE, current_mode == EditingMode::MOUSE,
          "Mouse Mode (1)\nNavigate, pan, and manage entities")) {
    if (current_mode != EditingMode::MOUSE) {
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    }
  }

  if (toolbar.ModeButton(ICON_MD_DRAW, current_mode == EditingMode::DRAW_TILE,
                         "Tile Paint Mode (2)\nDraw tiles on the map")) {
    if (current_mode != EditingMode::DRAW_TILE) {
      current_mode = EditingMode::DRAW_TILE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
    }
  }

  toolbar.EndModeGroup();

  // Entity editing indicator (shows current entity mode if active)
  if (entity_edit_mode_ != EntityEditMode::NONE) {
    toolbar.AddSeparator();
    const char* entity_label = "";
    const char* entity_icon = "";
    switch (entity_edit_mode_) {
      case EntityEditMode::ENTRANCES:
        entity_icon = ICON_MD_DOOR_FRONT;
        entity_label = "Entrances";
        break;
      case EntityEditMode::EXITS:
        entity_icon = ICON_MD_DOOR_BACK;
        entity_label = "Exits";
        break;
      case EntityEditMode::ITEMS:
        entity_icon = ICON_MD_GRASS;
        entity_label = "Items";
        break;
      case EntityEditMode::SPRITES:
        entity_icon = ICON_MD_PEST_CONTROL_RODENT;
        entity_label = "Sprites";
        break;
      case EntityEditMode::TRANSPORTS:
        entity_icon = ICON_MD_ADD_LOCATION;
        entity_label = "Transports";
        break;
      case EntityEditMode::MUSIC:
        entity_icon = ICON_MD_MUSIC_NOTE;
        entity_label = "Music";
        break;
      default:
        break;
    }
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s Editing: %s",
                       entity_icon, entity_label);
  }

  // ROM version badge (already read above)
  toolbar.AddRomBadge(asm_version,
                      [this]() { ImGui::OpenPopup("UpgradeROMVersion"); });

  // Inline map properties with icon labels - use toolbar methods for consistency
  if (toolbar.AddProperty(ICON_MD_IMAGE, " Gfx",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_graphics(),
                          [this]() {
                            // CORRECT ORDER: Properties first, then graphics reload

                            // 1. Propagate properties to siblings FIRST (this also calls LoadAreaGraphics on siblings)
                            RefreshMapProperties();

                            // 2. Force immediate refresh of current map and all siblings
                            maps_bmp_[current_map_].set_modified(true);
                            RefreshChildMapOnDemand(current_map_);
                            RefreshSiblingMapGraphics(current_map_);

                            // 3. Update tile selector
                            RefreshTile16Blockset();
                          })) {
    // Property changed
  }

  if (toolbar.AddProperty(ICON_MD_PALETTE, " Pal",
                          overworld_.mutable_overworld_map(current_map_)
                              ->mutable_area_palette(),
                          [this]() {
                            // Palette changes also need to propagate to siblings
                            RefreshSiblingMapGraphics(current_map_);
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

  if (toolbar.AddToggle(ICON_MD_OPEN_IN_FULL, &overworld_canvas_fullscreen_,
                        "Fullscreen (F11)")) {
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

  if (toolbar.AddUsageStatsButton("Open Usage Statistics")) {
    show_usage_stats_ = !show_usage_stats_;
  }

  if (toolbar.AddAction(ICON_MD_TUNE, "Open Area Configuration")) {
    show_map_properties_panel_ = !show_map_properties_panel_;
  }

  toolbar.End();

  // ROM Upgrade Popup (rendered outside toolbar to avoid ID conflicts)
  if (ImGui::BeginPopupModal("UpgradeROMVersion", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(ICON_MD_UPGRADE " Upgrade ROM to ZSCustomOverworld");
    ImGui::Separator();
    ImGui::TextWrapped(
        "This will apply the ZSCustomOverworld ASM patch to your ROM,\n"
        "enabling advanced features like custom tile graphics, animated GFX,\n"
        "wide/tall areas, and more.");
    ImGui::Separator();

    uint8_t current_version = (*rom_)[zelda3::OverworldCustomASMHasBeenApplied];
    ImGui::Text("Current Version: %s",
                current_version == 0xFF
                    ? "Vanilla"
                    : absl::StrFormat("v%d", current_version).c_str());

    static int target_version = 3;
    ImGui::RadioButton("v2 (Basic features)", &target_version, 2);
    ImGui::SameLine();
    ImGui::RadioButton("v3 (All features)", &target_version, 3);

    ImGui::Separator();

    if (ImGui::Button(ICON_MD_CHECK " Apply Upgrade", ImVec2(150, 0))) {
      auto status = ApplyZSCustomOverworldASM(target_version);
      if (status.ok()) {
        // CRITICAL: Reload the editor to reflect changes
        status_ = Clear();
        status_ = Load();
        ImGui::CloseCurrentPopup();
      } else {
        LOG_ERROR("OverworldEditor", "Upgrade failed: %s",
                  status.message().data());
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel", ImVec2(150, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

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

    EditingMode old_mode = current_mode;

    // Tool shortcuts (simplified)
    if (ImGui::IsKeyDown(ImGuiKey_1)) {
      current_mode = EditingMode::MOUSE;
    } else if (ImGui::IsKeyDown(ImGuiKey_2)) {
      current_mode = EditingMode::DRAW_TILE;
    }

    // Update canvas usage mode when mode changes
    if (old_mode != current_mode) {
      if (current_mode == EditingMode::MOUSE) {
        ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
      } else if (current_mode == EditingMode::DRAW_TILE) {
        ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
      }
    }

    // Entity editing shortcuts (3-8)
    if (ImGui::IsKeyDown(ImGuiKey_3)) {
      entity_edit_mode_ = EntityEditMode::ENTRANCES;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else if (ImGui::IsKeyDown(ImGuiKey_4)) {
      entity_edit_mode_ = EntityEditMode::EXITS;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else if (ImGui::IsKeyDown(ImGuiKey_5)) {
      entity_edit_mode_ = EntityEditMode::ITEMS;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else if (ImGui::IsKeyDown(ImGuiKey_6)) {
      entity_edit_mode_ = EntityEditMode::SPRITES;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else if (ImGui::IsKeyDown(ImGuiKey_7)) {
      entity_edit_mode_ = EntityEditMode::TRANSPORTS;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else if (ImGui::IsKeyDown(ImGuiKey_8)) {
      entity_edit_mode_ = EntityEditMode::MUSIC;
      current_mode = EditingMode::MOUSE;
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
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

    // Don't apply scale to coordinates - scale is applied to canvas rendering
    int map_x = xx * kOverworldMapSize;
    int map_y = yy * kOverworldMapSize;

    // Check if the map has a texture, if not, ensure it gets loaded
    if (!maps_bmp_[world_index].texture() &&
        maps_bmp_[world_index].is_active()) {
      EnsureMapTexture(world_index);
    }

    // Only draw if the map has a texture or is the currently selected map
    if (maps_bmp_[world_index].texture() || world_index == current_map_) {
      // Draw without applying scale here - canvas handles zoom uniformly
      ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y, 1.0f);
    } else {
      // Draw a placeholder for maps that haven't loaded yet
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = ow_map_canvas_.zero_point();
      ImVec2 placeholder_pos =
          ImVec2(canvas_pos.x + map_x, canvas_pos.y + map_y);
      ImVec2 placeholder_size = ImVec2(kOverworldMapSize, kOverworldMapSize);

      // Modern loading indicator with theme colors
      draw_list->AddRectFilled(
          placeholder_pos,
          ImVec2(placeholder_pos.x + placeholder_size.x,
                 placeholder_pos.y + placeholder_size.y),
          IM_COL32(32, 32, 32, 128));  // Dark gray with transparency

      // Animated loading spinner
      ImVec2 spinner_pos = ImVec2(placeholder_pos.x + placeholder_size.x / 2,
                                  placeholder_pos.y + placeholder_size.y / 2);

      const float spinner_radius = 8.0f;
      const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
      const float start_angle = rotation;
      const float end_angle = rotation + IM_PI * 1.5f;

      draw_list->PathArcTo(spinner_pos, spinner_radius, start_angle, end_angle,
                           12);
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
    LOG_ERROR(
        "OverworldEditor",
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
    LOG_ERROR(
        "OverworldEditor",
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
        LOG_ERROR(
            "OverworldEditor",
            "ERROR: RenderUpdatedMapBitmap - pixel_index %d out of bounds "
            "(bitmap size=%zu)",
            pixel_index, current_bitmap.size());
        continue;
      }

      // Bounds check for tile data
      int tile_data_index = y * kTile16Size + x;
      if (tile_data_index < 0 ||
          tile_data_index >= static_cast<int>(tile_data.size())) {
        LOG_ERROR(
            "OverworldEditor",
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
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::UPDATE,
                                        &current_bitmap);
}

void OverworldEditor::CheckForOverworldEdits() {
  LOG_DEBUG("OverworldEditor", "CheckForOverworldEdits: Frame %d",
            ImGui::GetFrameCount());

  CheckForSelectRectangle();

  // User has selected a tile they want to draw from the blockset
  // and clicked on the canvas.
  // Note: With TileSelectorWidget, we check if a valid tile is selected instead of canvas points
  if (current_tile16_ >= 0 && !ow_map_canvas_.select_rect_active() &&
      ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
    DrawOverworldEdits();
  }

  if (ow_map_canvas_.select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      LOG_DEBUG("OverworldEditor",
                "CheckForOverworldEdits: About to apply rectangle selection");

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
      for (int y = start_y;
           y <= end_y && i < static_cast<int>(selected_tile16_ids_.size());
           y += kTile16Size) {
        for (int x = start_x;
             x <= end_x && i < static_cast<int>(selected_tile16_ids_.size());
             x += kTile16Size, ++i) {

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

          bool in_same_local_map = (start_local_map_x == end_local_map_x) &&
                                   (start_local_map_y == end_local_map_y);

          if (in_same_local_map && index_x >= 0 &&
              (index_x + rect_width - 1) < 0x200 && index_y >= 0 &&
              (index_y + rect_height - 1) < 0x200) {
            selected_world[index_x][index_y] = tile16_id;

            // CRITICAL FIX: Also update the bitmap directly like single tile drawing
            ImVec2 tile_position(x, y);
            auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile16_id);
            if (!tile_data.empty()) {
              RenderUpdatedMapBitmap(tile_position, tile_data);
              LOG_DEBUG(
                  "OverworldEditor",
                  "CheckForOverworldEdits: Updated bitmap at position (%d,%d) "
                  "with tile16_id=%d",
                  x, y, tile16_id);
            } else {
              LOG_ERROR("OverworldEditor",
                        "ERROR: Failed to get tile data for tile16_id=%d",
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
      LOG_DEBUG(
          "OverworldEditor",
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
  if (!dependencies_.shared_clipboard) {
    return absl::FailedPreconditionError("Clipboard unavailable");
  }
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

    dependencies_.shared_clipboard->overworld_tile16_ids = std::move(ids);
    dependencies_.shared_clipboard->overworld_width = width;
    dependencies_.shared_clipboard->overworld_height = height;
    dependencies_.shared_clipboard->has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  // Single tile copy fallback
  if (current_tile16_ >= 0) {
    dependencies_.shared_clipboard->overworld_tile16_ids = {current_tile16_};
    dependencies_.shared_clipboard->overworld_width = 1;
    dependencies_.shared_clipboard->overworld_height = 1;
    dependencies_.shared_clipboard->has_overworld_tile16 = true;
    return absl::OkStatus();
  }
  return absl::FailedPreconditionError("Nothing selected to copy");
}

absl::Status OverworldEditor::Paste() {
  if (!dependencies_.shared_clipboard) {
    return absl::FailedPreconditionError("Clipboard unavailable");
  }
  if (!dependencies_.shared_clipboard->has_overworld_tile16) {
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

  const int width = dependencies_.shared_clipboard->overworld_width;
  const int height = dependencies_.shared_clipboard->overworld_height;
  const auto& ids = dependencies_.shared_clipboard->overworld_tile16_ids;

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
  // CRITICAL FIX: Use canvas hover position (not raw ImGui mouse) for proper coordinate sync
  // hover_mouse_pos() already returns canvas-local coordinates (world space, not screen space)
  const auto mouse_position = ow_map_canvas_.hover_mouse_pos();
  const int large_map_size = 1024;

  // Calculate which small map the mouse is currently over
  // No need to subtract canvas_zero_point - mouse_position is already in world coordinates
  int map_x = mouse_position.x / kOverworldMapSize;
  int map_y = mouse_position.y / kOverworldMapSize;

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
        ow_map_canvas_.DrawOutline(parent_map_x * kOverworldMapSize,
                                   parent_map_y * kOverworldMapSize,
                                   kOverworldMapSize, kOverworldMapSize);
        break;
    }
  } else {
    // Legacy logic for vanilla and v2 ROMs
    int world_offset = current_world_ * 0x40;
    if (overworld_.overworld_map(current_map_)->is_large_map() ||
        overworld_.overworld_map(current_map_)->large_index() != 0) {
      const int highlight_parent =
          overworld_.overworld_map(current_highlighted_map)->parent();

      // CRITICAL FIX: Account for world offset when calculating parent coordinates
      // For Dark World (0x40-0x7F), parent IDs are in range 0x40-0x7F
      // For Special World (0x80-0x9F), parent IDs are in range 0x80-0x9F
      // We need to subtract the world offset to get display grid coordinates (0-7)
      int parent_map_x;
      int parent_map_y;
      if (current_world_ == 0) {
        // Light World (0x00-0x3F)
        parent_map_x = highlight_parent % 8;
        parent_map_y = highlight_parent / 8;
      } else if (current_world_ == 1) {
        // Dark World (0x40-0x7F) - subtract 0x40 to get display coordinates
        parent_map_x = (highlight_parent - 0x40) % 8;
        parent_map_y = (highlight_parent - 0x40) / 8;
      } else {
        // Special World (0x80-0x9F) - subtract 0x80 to get display coordinates
        parent_map_x = (highlight_parent - 0x80) % 8;
        parent_map_y = (highlight_parent - 0x80) / 8;
      }

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
      // TODO: Queue texture for later rendering.
      // Renderer::Get().UpdateBitmap(&tile16_blockset_.atlas);
    }

    // Update map texture with the traditional direct update approach
    // TODO: Queue texture for later rendering.
    // Renderer::Get().UpdateBitmap(&maps_bmp_[current_map_]);
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

// Overworld Canvas Pan/Zoom Helpers

namespace {

// Calculate the total canvas content size based on world layout
ImVec2 CalculateOverworldContentSize(float scale) {
  // 8x8 grid of 512x512 maps = 4096x4096 total
  constexpr float kWorldSize = 512.0f * 8.0f;  // 4096
  return ImVec2(kWorldSize * scale, kWorldSize * scale);
}

// Clamp scroll position to valid bounds
ImVec2 ClampScrollPosition(ImVec2 scroll, ImVec2 content_size,
                           ImVec2 visible_size) {
  // Calculate maximum scroll values
  float max_scroll_x = std::max(0.0f, content_size.x - visible_size.x);
  float max_scroll_y = std::max(0.0f, content_size.y - visible_size.y);

  // Clamp to valid range [min_scroll, 0]
  // Note: Canvas uses negative scrolling for right/down
  float clamped_x = std::clamp(scroll.x, -max_scroll_x, 0.0f);
  float clamped_y = std::clamp(scroll.y, -max_scroll_y, 0.0f);

  return ImVec2(clamped_x, clamped_y);
}

}  // namespace

void OverworldEditor::HandleOverworldPan() {
  // Middle mouse button panning (works in all modes)
  if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) &&
      ImGui::IsItemHovered()) {
    middle_mouse_dragging_ = true;

    // Get mouse delta and apply to scroll
    ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
    ImVec2 current_scroll = ow_map_canvas_.scrolling();
    ImVec2 new_scroll = ImVec2(current_scroll.x + mouse_delta.x,
                               current_scroll.y + mouse_delta.y);

    // Clamp scroll to boundaries
    ImVec2 content_size =
        CalculateOverworldContentSize(ow_map_canvas_.global_scale());
    ImVec2 visible_size = ow_map_canvas_.canvas_size();
    new_scroll = ClampScrollPosition(new_scroll, content_size, visible_size);

    ow_map_canvas_.set_scrolling(new_scroll);
  }

  if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle) &&
      middle_mouse_dragging_) {
    middle_mouse_dragging_ = false;
  }
}

void OverworldEditor::HandleOverworldZoom() {
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) {
    return;
  }

  const ImGuiIO& io = ImGui::GetIO();

  // Mouse wheel zoom with Ctrl key
  if (io.MouseWheel != 0.0f && io.KeyCtrl) {
    float current_scale = ow_map_canvas_.global_scale();
    float zoom_delta = io.MouseWheel * 0.1f;
    float new_scale = current_scale + zoom_delta;

    // Clamp zoom range (0.25x to 2.0x)
    new_scale = std::clamp(new_scale, 0.25f, 2.0f);

    if (new_scale != current_scale) {
      // Get mouse position relative to canvas
      ImVec2 mouse_pos_canvas =
          ImVec2(io.MousePos.x - ow_map_canvas_.zero_point().x,
                 io.MousePos.y - ow_map_canvas_.zero_point().y);

      // Calculate content position under mouse before zoom
      ImVec2 scroll = ow_map_canvas_.scrolling();
      ImVec2 content_pos_before =
          ImVec2((mouse_pos_canvas.x - scroll.x) / current_scale,
                 (mouse_pos_canvas.y - scroll.y) / current_scale);

      // Apply new scale
      ow_map_canvas_.set_global_scale(new_scale);

      // Calculate new scroll to keep same content under mouse
      ImVec2 new_scroll =
          ImVec2(mouse_pos_canvas.x - (content_pos_before.x * new_scale),
                 mouse_pos_canvas.y - (content_pos_before.y * new_scale));

      // Clamp scroll to boundaries with new scale
      ImVec2 content_size = CalculateOverworldContentSize(new_scale);
      ImVec2 visible_size = ow_map_canvas_.canvas_size();
      new_scroll = ClampScrollPosition(new_scroll, content_size, visible_size);

      ow_map_canvas_.set_scrolling(new_scroll);
    }
  }
}

void OverworldEditor::ResetOverworldView() {
  ow_map_canvas_.set_global_scale(1.0f);
  ow_map_canvas_.set_scrolling(ImVec2(0, 0));
}

void OverworldEditor::CenterOverworldView() {
  float scale = ow_map_canvas_.global_scale();
  ImVec2 content_size = CalculateOverworldContentSize(scale);
  ImVec2 visible_size = ow_map_canvas_.canvas_size();

  // Center the view
  ImVec2 centered_scroll = ImVec2(-(content_size.x - visible_size.x) / 2.0f,
                                  -(content_size.y - visible_size.y) / 2.0f);

  ow_map_canvas_.set_scrolling(centered_scroll);
}

void OverworldEditor::CheckForMousePan() {
  // Legacy wrapper - now calls HandleOverworldPan
  HandleOverworldPan();
}

void OverworldEditor::DrawOverworldCanvas() {
  // Simplified map settings - compact row with popup panels for detailed editing
  if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
    map_properties_system_->DrawSimplifiedMapSettings(
        current_world_, current_map_, current_map_lock_,
        show_map_properties_panel_, show_custom_bg_color_editor_,
        show_overlay_editor_, show_overlay_preview_, game_state_,
        (int&)current_mode);
  }

  gui::BeginNoPadding();
  gui::BeginChildBothScrollbars(7);
  ow_map_canvas_.DrawBackground();
  gui::EndNoPadding();

  // Setup dynamic context menu based on current map state (Phase 3B)
  if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
    map_properties_system_->SetupCanvasContextMenu(
        ow_map_canvas_, current_map_, current_map_lock_,
        show_map_properties_panel_, show_custom_bg_color_editor_,
        show_overlay_editor_, static_cast<int>(current_mode));
  }

  // Handle pan and zoom (works in all modes)
  HandleOverworldPan();
  HandleOverworldZoom();

  // Context menu only in MOUSE mode
  if (current_mode == EditingMode::MOUSE) {
    if (entity_renderer_->hovered_entity() == nullptr) {
      ow_map_canvas_.DrawContextMenu();
    }
  } else if (current_mode == EditingMode::DRAW_TILE) {
    // Tile painting mode - handle tile edits and right-click tile picking
    HandleMapInteraction();
  }

  if (overworld_.is_loaded()) {
    DrawOverworldMaps();

    // Draw all entities using the entity renderer
    // Convert entity_edit_mode_ to legacy mode int for entity renderer
    int entity_mode_int = static_cast<int>(entity_edit_mode_);
    entity_renderer_->DrawExits(ow_map_canvas_.zero_point(),
                                ow_map_canvas_.scrolling(), current_world_,
                                entity_mode_int);
    entity_renderer_->DrawEntrances(ow_map_canvas_.zero_point(),
                                    ow_map_canvas_.scrolling(), current_world_,
                                    entity_mode_int);
    entity_renderer_->DrawItems(current_world_, entity_mode_int);
    entity_renderer_->DrawSprites(current_world_, game_state_, entity_mode_int);

    // Draw overlay preview if enabled
    if (show_overlay_preview_) {
      map_properties_system_->DrawOverlayPreviewOnMap(
          current_map_, current_world_, show_overlay_preview_);
    }

    if (current_mode == EditingMode::DRAW_TILE) {
      CheckForOverworldEdits();
    }
    // CRITICAL FIX: Use canvas hover state, not ImGui::IsItemHovered()
    // IsItemHovered() checks the LAST drawn item, which could be entities/overlay,
    // not the canvas InvisibleButton. ow_map_canvas_.IsMouseHovering() correctly
    // tracks whether mouse is over the canvas area.
    if (ow_map_canvas_.IsMouseHovering())
      status_ = CheckForCurrentMap();

    // --- BEGIN NEW DRAG/DROP LOGIC ---
    if (current_mode == EditingMode::MOUSE) {
      auto hovered_entity = entity_renderer_->hovered_entity();

      // 1. Initiate drag
      if (!is_dragging_entity_ && hovered_entity &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        dragged_entity_ = hovered_entity;
        is_dragging_entity_ = true;
        if (dragged_entity_->entity_type_ ==
            zelda3::GameEntity::EntityType::kExit) {
          dragged_entity_free_movement_ = true;
        }
      }

      // 2. Update drag
      if (is_dragging_entity_ && dragged_entity_ &&
          ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
        float scale = ow_map_canvas_.global_scale();
        if (scale > 0.0f) {
          dragged_entity_->x_ += mouse_delta.x / scale;
          dragged_entity_->y_ += mouse_delta.y / scale;
        }
      }

      // 3. End drag
      if (is_dragging_entity_ &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (dragged_entity_) {
          MoveEntityOnGrid(dragged_entity_, ow_map_canvas_.zero_point(),
                           ow_map_canvas_.scrolling(),
                           dragged_entity_free_movement_);
          dragged_entity_->UpdateMapProperties(dragged_entity_->map_id_);
          rom_->set_dirty(true);
        }
        is_dragging_entity_ = false;
        dragged_entity_ = nullptr;
        dragged_entity_free_movement_ = false;
      }
    }
    // --- END NEW DRAG/DROP LOGIC ---
  }

  ow_map_canvas_.DrawGrid();
  ow_map_canvas_.DrawOverlay();
  ImGui::EndChild();
  
}

absl::Status OverworldEditor::DrawTile16Selector() {
  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##Tile16SelectorScrollRegion");
  gui::EndPadding();

  if (!blockset_selector_) {
    gui::TileSelectorWidget::Config selector_config;
    selector_config.tile_size = 16;
    selector_config.display_scale = 2.0f;
    selector_config.tiles_per_row = 8;
    selector_config.total_tiles = zelda3::kNumTile16Individual;
    selector_config.draw_offset = ImVec2(2.0f, 0.0f);
    selector_config.highlight_color = ImVec4(0.95f, 0.75f, 0.3f, 1.0f);

    blockset_selector_ = std::make_unique<gui::TileSelectorWidget>(
        "OwBlocksetSelector", selector_config);
    blockset_selector_->AttachCanvas(&blockset_canvas_);
  }

  UpdateBlocksetSelectorState();

  gfx::Bitmap& atlas = tile16_blockset_.atlas;
  bool atlas_ready = map_blockset_loaded_ && atlas.is_active();
  auto result = blockset_selector_->Render(atlas, atlas_ready);

  if (result.selection_changed) {
    current_tile16_ = result.selected_tile;
    auto status = tile16_editor_.SetCurrentTile(current_tile16_);
    if (!status.ok()) {
      // Store error but ensure we close the child before returning
      ImGui::EndChild();
      ImGui::EndGroup();
      return status;
    }
    // Note: We do NOT auto-scroll here because it breaks user interaction.
    // The canvas should only scroll when explicitly requested (e.g., when
    // selecting a tile from the overworld canvas via ScrollBlocksetCanvasToCurrentTile).
  }

  if (result.tile_double_clicked) {
    show_tile16_editor_ = true;
  }

  ImGui::EndChild();
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
      // TODO: Queue texture for later rendering.
      // Renderer::Get().CreateAndRenderBitmap(0x80, kOverworldMapSize, 0x08,
      // overworld_.current_graphics(), bmp,
      // palette_);
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
  ImGui::EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
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

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
  // Load the Link to the Past overworld.
  {
    gfx::ScopedTimer load_timer("Overworld::Load");
    RETURN_IF_ERROR(overworld_.Load(rom_));
  }
  palette_ = overworld_.current_area_palette();

  LOG_DEBUG("OverworldEditor", "Loading overworld graphics (optimized).");

  // Phase 1: Create bitmaps without textures for faster loading
  // This avoids blocking the main thread with GPU texture creation
  {
    gfx::ScopedTimer gfx_timer("CreateBitmapWithoutTexture_Graphics");
    // TODO: Queue texture for later rendering.
    // Renderer::Get().CreateBitmapWithoutTexture(0x80, kOverworldMapSize, 0x40,
    //                                            overworld_.current_graphics(),
    //                                            current_gfx_bmp_, palette_);
  }

  LOG_DEBUG("OverworldEditor",
            "Loading overworld tileset (deferred textures).");
  {
    gfx::ScopedTimer tileset_timer("CreateBitmapWithoutTexture_Tileset");
    // TODO: Queue texture for later rendering.
    // Renderer::Get().CreateBitmapWithoutTexture(
    // 0x80, 0x2000, 0x08, overworld_.tile16_blockset_data(),
    // tile16_blockset_bmp_, palette_);
  }
  map_blockset_loaded_ = true;

  // Copy the tile16 data into individual tiles.
  auto tile16_blockset_data = overworld_.tile16_blockset_data();
  LOG_DEBUG("OverworldEditor", "Loading overworld tile16 graphics.");

  {
    gfx::ScopedTimer tilemap_timer("CreateTilemap");
    tile16_blockset_ =
        gfx::CreateTilemap(renderer_, tile16_blockset_data, 0x80, 0x2000,
                           kTile16Size, zelda3::kNumTile16Individual, palette_);

    // Queue texture creation for the tile16 blockset atlas
    if (tile16_blockset_.atlas.is_active() &&
        tile16_blockset_.atlas.surface()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &tile16_blockset_.atlas);
    }
  }

  // Phase 2: Create bitmaps only for essential maps initially
  // Non-essential maps will be created on-demand when accessed
  constexpr int kEssentialMapsPerWorld = 8;
  constexpr int kLightWorldEssential = kEssentialMapsPerWorld;
  constexpr int kDarkWorldEssential =
      zelda3::kDarkWorldMapIdStart + kEssentialMapsPerWorld;
  constexpr int kSpecialWorldEssential =
      zelda3::kSpecialWorldMapIdStart + kEssentialMapsPerWorld;

  LOG_DEBUG(
      "OverworldEditor",
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
      // Queue texture creation/update for initial maps via Arena's deferred system
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, maps_to_texture[i]);
    }
  }

  // Queue remaining maps for progressive loading via Arena
  // Priority based on current world (0 = current world, 11+ = other worlds)
  for (size_t i = initial_texture_count; i < maps_to_texture.size(); ++i) {
    // Determine priority based on which world this map belongs to
    int map_index = -1;
    for (int j = 0; j < zelda3::kNumOverworldMaps; ++j) {
      if (&maps_bmp_[j] == maps_to_texture[i]) {
        map_index = j;
        break;
      }
    }

    int priority = 15;  // Default low priority
    if (map_index >= 0) {
      int map_world = map_index / 0x40;
      priority = (map_world == current_world_)
                     ? 5
                     : 15;  // Current world = priority 5, others = 15
    }

    // Queue texture creation for remaining maps via Arena's deferred system
    // Note: Priority system to be implemented in future enhancement
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, maps_to_texture[i]);
  }

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
      // TODO: Queue texture for later rendering.
      // Renderer::Get().RenderBitmap(&(sprite_previews_[sprite.id()]));
    }
  return absl::OkStatus();
}

void OverworldEditor::ProcessDeferredTextures() {
  // Process queued texture commands via Arena's deferred system
  if (renderer_) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }

  // Also process deferred map refreshes for modified maps
  int refresh_count = 0;
  const int max_refreshes_per_frame = 2;

  for (int i = 0;
       i < zelda3::kNumOverworldMaps && refresh_count < max_refreshes_per_frame;
       ++i) {
    if (maps_bmp_[i].modified() && maps_bmp_[i].is_active()) {
      // Check if this map is in current world (prioritize)
      bool is_current_world = (i / 0x40 == current_world_);
      bool is_current_map = (i == current_map_);

      if (is_current_map || is_current_world) {
        RefreshOverworldMapOnDemand(i);
        refresh_count++;
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
    // Queue texture creation for this map
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
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
      LOG_ERROR("OverworldEditor",
                "Failed to build tiles16 graphics for map %d: %s", map_index,
                status.message().data());
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
      LOG_WARN("OverworldEditor",
               "Warning: Surface synchronization issue detected for map %d",
               map_index);
    }

    // Queue texture update to ensure changes are visible
    if (maps_bmp_[map_index].texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &maps_bmp_[map_index]);
    } else {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &maps_bmp_[map_index]);
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

  LOG_DEBUG(
      "OverworldEditor",
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
      LOG_DEBUG(
          "OverworldEditor",
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

                // SAFETY: Only set palette if bitmap has a valid surface
                if (maps_bmp_[sibling].is_active() &&
                    maps_bmp_[sibling].surface()) {
                  maps_bmp_[sibling].SetPalette(
                      overworld_.current_area_palette());
                }
                maps_bmp_[sibling].set_modified(false);

                // Queue texture update/creation
                if (maps_bmp_[sibling].texture()) {
                  gfx::Arena::Get().QueueTextureCommand(
                      gfx::Arena::TextureCommandType::UPDATE,
                      &maps_bmp_[sibling]);
                } else {
                  EnsureMapTexture(sibling);
                }
              }
            }
          }
        }

        if (!status.ok()) {
          LOG_ERROR(
              "OverworldEditor",
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

        // SAFETY: Only set palette if bitmap has a valid surface
        if (maps_bmp_[sibling_index].is_active() &&
            maps_bmp_[sibling_index].surface()) {
          maps_bmp_[sibling_index].SetPalette(current_map_palette);
        }
      }
    }

    // SAFETY: Only set palette if bitmap has a valid surface
    if (maps_bmp_[current_map_].is_active() &&
        maps_bmp_[current_map_].surface()) {
      maps_bmp_[current_map_].SetPalette(current_map_palette);
    }
  }

  return absl::OkStatus();
}

void OverworldEditor::ForceRefreshGraphics(int map_index) {
  // Mark the bitmap as modified to force refresh on next update
  if (map_index >= 0 && map_index < static_cast<int>(maps_bmp_.size())) {
    maps_bmp_[map_index].set_modified(true);

    // Clear blockset cache
    current_blockset_ = 0xFF;

    LOG_DEBUG("OverworldEditor",
              "ForceRefreshGraphics: Map %d marked for refresh", map_index);
  }
}

void OverworldEditor::RefreshSiblingMapGraphics(int map_index,
                                                bool include_self) {
  if (map_index < 0 || map_index >= static_cast<int>(maps_bmp_.size())) {
    return;
  }

  auto* map = overworld_.mutable_overworld_map(map_index);
  if (map->area_size() == zelda3::AreaSizeEnum::SmallArea) {
    return;  // No siblings for small areas
  }

  int parent_id = map->parent();
  std::vector<int> siblings;

  switch (map->area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      siblings = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;
    case zelda3::AreaSizeEnum::WideArea:
      siblings = {parent_id, parent_id + 1};
      break;
    case zelda3::AreaSizeEnum::TallArea:
      siblings = {parent_id, parent_id + 8};
      break;
    default:
      return;
  }

  for (int sibling : siblings) {
    if (sibling >= 0 && sibling < 0xA0) {
      // Skip self unless include_self is true
      if (sibling == map_index && !include_self) {
        continue;
      }

      // Mark as modified FIRST before loading
      maps_bmp_[sibling].set_modified(true);

      // Load graphics from ROM
      overworld_.mutable_overworld_map(sibling)->LoadAreaGraphics();

      // CRITICAL FIX: Bypass visibility check - force immediate refresh
      // Call RefreshChildMapOnDemand() directly instead of RefreshOverworldMapOnDemand()
      RefreshChildMapOnDemand(sibling);

      LOG_DEBUG("OverworldEditor",
                "RefreshSiblingMapGraphics: Refreshed sibling map %d", sibling);
    }
  }
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

        // CRITICAL FIX: Reload graphics after changing properties
        map.LoadAreaGraphics();
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

        // CRITICAL FIX: Reload graphics after changing properties
        map.LoadAreaGraphics();
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

  gfx::UpdateTilemap(renderer_, tile16_blockset_, tile16_data);
  tile16_blockset_.atlas.SetPalette(palette_);

  // Queue texture update for the atlas
  if (tile16_blockset_.atlas.texture() && tile16_blockset_.atlas.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_.atlas);
  } else if (!tile16_blockset_.atlas.texture() &&
             tile16_blockset_.atlas.is_active()) {
    // Create texture if it doesn't exist yet
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &tile16_blockset_.atlas);
  }

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

// Note: SetupOverworldCanvasContextMenu has been removed (Phase 3B).
// Context menu is now setup dynamically in DrawOverworldCanvas() via
// MapPropertiesSystem::SetupCanvasContextMenu() for context-aware menu items.

void OverworldEditor::ScrollBlocksetCanvasToCurrentTile() {
  if (blockset_selector_) {
    blockset_selector_->ScrollToTile(current_tile16_);
    return;
  }

  // CRITICAL FIX: Do NOT use fallback scrolling from overworld canvas context!
  // The fallback code uses ImGui::SetScrollX/Y which scrolls the CURRENT window,
  // and when called from CheckForSelectRectangle() during overworld canvas rendering,
  // it incorrectly scrolls the overworld canvas instead of the tile16 selector.
  //
  // The blockset_selector_ should always be available in modern code paths.
  // If it's not available, we skip scrolling rather than scroll the wrong window.
  //
  // This fixes the bug where right-clicking to select tiles on the Dark World
  // causes the overworld canvas to scroll unexpectedly.
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

  ImGui::Text("Area Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_GFX);
  ImGui::SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  ImGui::Text("Sprite Gfx LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART1);
  ImGui::SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART1);
  ImGui::SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_SPR_GFX_PART2);
  ImGui::SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_SPR_GFX_PART2);
  ImGui::Separator();

  ImGui::Text("Area Pal LW/DW");
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::LW_AREA_PAL);
  ImGui::SameLine();
  properties_canvas_.UpdateInfoGrid(ImVec2(256, 256), 32,
                                    OverworldProperty::DW_AREA_PAL);

  static bool show_gfx_group = false;
  ImGui::Checkbox("Show Gfx Group Editor", &show_gfx_group);
  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    status_ = gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }
}

absl::Status OverworldEditor::UpdateUsageStats() {
  if (ImGui::BeginTable(
          "UsageStatsTable", 3,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersOuter,
          ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Entrances");
    ImGui::TableSetupColumn("Grid", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Usage", ImGuiTableColumnFlags_WidthFixed, 256);
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    if (ImGui::BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
      for (int i = 0; i < 0x81; i++) {
        auto entrance_name = rom_->resource_label()->CreateOrGetLabel(
            "Dungeon Entrance Names", util::HexByte(i),
            zelda3::kEntranceNames[i]);
        std::string str = absl::StrFormat("%#x - %s", i, entrance_name);
        if (ImGui::Selectable(str.c_str(), selected_entrance_ == i,
                              overworld_.entrances().at(i).deleted
                                  ? ImGuiSelectableFlags_Disabled
                                  : 0)) {
          selected_entrance_ = i;
          selected_usage_map_ = overworld_.entrances().at(i).map_id_;
          properties_canvas_.set_highlight_tile_id(selected_usage_map_);
        }
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::Text("Entrance ID: %d", i);
          ImGui::Text("Map ID: %d", overworld_.entrances().at(i).map_id_);
          ImGui::Text("Entrance ID: %d",
                      overworld_.entrances().at(i).entrance_id_);
          ImGui::Text("X: %d", overworld_.entrances().at(i).x_);
          ImGui::Text("Y: %d", overworld_.entrances().at(i).y_);
          ImGui::Text("Deleted? %s",
                      overworld_.entrances().at(i).deleted ? "Yes" : "No");
          ImGui::EndTooltip();
        }
      }
      ImGui::EndChild();
    }

    ImGui::TableNextColumn();
    DrawUsageGrid();

    ImGui::TableNextColumn();
    DrawOverworldProperties();

    ImGui::EndTable();
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
    ImGui::NewLine();

    for (int col = 0; col < squares_wide; ++col) {
      if (row * squares_wide + col >= total_squares) {
        break;
      }
      // Determine if this square should be highlighted
      bool highlight = selected_usage_map_ == (row * squares_wide + col);

      // Set highlight color if needed
      if (highlight) {
        ImGui::PushStyleColor(ImGuiCol_Button, gui::GetSelectedColor());
      }

      // Create a button or selectable for each square
      if (ImGui::Button("##square", ImVec2(20, 20))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }

      // Reset style if it was highlighted
      if (highlight) {
        ImGui::PopStyleColor();
      }

      // Check if the square is hovered
      if (ImGui::IsItemHovered()) {
        // Display a tooltip with all the room properties
      }

      // Keep squares in the same line
      ImGui::SameLine();
    }
  }
}

void OverworldEditor::DrawDebugWindow() {
  ImGui::Text("Current Map: %d", current_map_);
  ImGui::Text("Current Tile16: %d", current_tile16_);
  int relative_x = (int)ow_map_canvas_.drawn_tile_position().x % 512;
  int relative_y = (int)ow_map_canvas_.drawn_tile_position().y % 512;
  ImGui::Text("Current Tile16 Drawn Position (Relative): %d, %d", relative_x,
              relative_y);

  // Print the size of the overworld map_tiles per world
  ImGui::Text("Light World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->light_world.size());
  ImGui::Text("Dark World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->dark_world.size());
  ImGui::Text("Special World Map Tiles: %d",
              (int)overworld_.mutable_map_tiles()->special_world.size());

  static bool view_lw_map_tiles = false;
  static MemoryEditor mem_edit;
  // Let's create buttons which let me view containers in the memory editor
  if (ImGui::Button("View Light World Map Tiles")) {
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
  // Feature flag deprecated - ROM version gating is sufficient
  // User explicitly clicked upgrade button, so respect their request

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

  LOG_DEBUG("OverworldEditor", "Applying ZSCustomOverworld ASM v%d to ROM...",
            target_version);

  // Initialize Asar wrapper
  auto asar_wrapper = std::make_unique<core::AsarWrapper>();
  RETURN_IF_ERROR(asar_wrapper->Initialize());

  // Create backup of ROM data
  std::vector<uint8_t> original_rom_data = rom_->vector();
  std::vector<uint8_t> working_rom_data = original_rom_data;

  try {
    // Determine which ASM file to apply and use GetResourcePath for proper resolution
    std::string asm_file_name =
        (target_version == 3) ? "asm/yaze.asm"  // Master file with v3
                              : "asm/ZSCustomOverworld.asm";  // v2 standalone

    // Use GetResourcePath to handle app bundles and various deployment scenarios
    std::string asm_file_path = util::GetResourcePath(asm_file_name);

    LOG_DEBUG("OverworldEditor", "Using ASM file: %s", asm_file_path.c_str());

    // Verify file exists
    if (!std::filesystem::exists(asm_file_path)) {
      return absl::NotFoundError(
          absl::StrFormat("ASM file not found at: %s\n\n"
                          "Expected location: assets/%s\n"
                          "Make sure the assets directory is accessible.",
                          asm_file_path, asm_file_name));
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
    LOG_DEBUG("OverworldEditor",
              "ASM patch applied successfully. Found %zu symbols:",
              result.symbols.size());
    for (const auto& symbol : result.symbols) {
      LOG_DEBUG("OverworldEditor", "  %s @ $%06X", symbol.name.c_str(),
                symbol.address);
    }

    // Refresh overworld data to reflect changes
    RETURN_IF_ERROR(overworld_.Load(rom_));

    LOG_DEBUG("OverworldEditor",
              "ZSCustomOverworld v%d successfully applied to ROM",
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

    LOG_DEBUG("OverworldEditor",
              "Enabled v2+ features: Custom BG colors, Main palettes");
  }

  if (target_version >= 3) {
    // v3 features
    (*rom_)[zelda3::OverworldCustomSubscreenOverlayEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomAnimatedGFXEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomTileGFXGroupEnabled] = 0x01;
    (*rom_)[zelda3::OverworldCustomMosaicEnabled] = 0x01;

    LOG_DEBUG(
        "OverworldEditor",
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

    LOG_DEBUG("OverworldEditor", "Initialized area size data for %zu areas",
              large_areas.size());
  }

  LOG_DEBUG("OverworldEditor", "ROM version markers updated to v%d",
            target_version);
  return absl::OkStatus();
}

void OverworldEditor::UpdateBlocksetSelectorState() {
  if (!blockset_selector_) {
    return;
  }

  blockset_selector_->SetTileCount(zelda3::kNumTile16Individual);
  blockset_selector_->SetSelectedTile(current_tile16_);
}

// ============================================================================
// Canvas Automation API Integration (Phase 4)
// ============================================================================

void OverworldEditor::SetupCanvasAutomation() {
  auto* api = ow_map_canvas_.GetAutomationAPI();

  // Set tile paint callback
  api->SetTilePaintCallback([this](int x, int y, int tile_id) {
    return AutomationSetTile(x, y, tile_id);
  });

  // Set tile query callback
  api->SetTileQueryCallback(
      [this](int x, int y) { return AutomationGetTile(x, y); });
}

bool OverworldEditor::AutomationSetTile(int x, int y, int tile_id) {
  if (!overworld_.is_loaded()) {
    return false;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return false;
  }

  // Set current world based on current_map_
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  // Set the tile in the overworld data structure
  overworld_.SetTile(x, y, static_cast<uint16_t>(tile_id));

  // Update the bitmap
  auto tile_data = gfx::GetTilemapData(tile16_blockset_, tile_id);
  if (!tile_data.empty()) {
    RenderUpdatedMapBitmap(
        ImVec2(static_cast<float>(x * 16), static_cast<float>(y * 16)),
        tile_data);
    return true;
  }

  return false;
}

int OverworldEditor::AutomationGetTile(int x, int y) {
  if (!overworld_.is_loaded()) {
    return -1;
  }

  // Bounds check
  if (x < 0 || y < 0 || x >= 512 || y >= 512) {
    return -1;
  }

  // Set current world
  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);

  return overworld_.GetTile(x, y);
}

void OverworldEditor::HandleEntityInsertion(const std::string& entity_type) {
  if (!overworld_.is_loaded()) {
    LOG_ERROR("OverworldEditor", "Cannot insert entity: overworld not loaded");
    return;
  }

  // Get mouse position from canvas (in world coordinates)
  ImVec2 mouse_pos = ow_map_canvas_.hover_mouse_pos();

  LOG_DEBUG("OverworldEditor",
            "HandleEntityInsertion called: type='%s' at pos=(%.0f,%.0f) map=%d",
            entity_type.c_str(), mouse_pos.x, mouse_pos.y, current_map_);

  if (entity_type == "entrance") {
    auto result = InsertEntrance(&overworld_, mouse_pos, current_map_, false);
    if (result.ok()) {
      current_entrance_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup("Entrance Editor");
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Entrance inserted successfully at map=%d",
                current_map_);
    } else {
      LOG_ERROR("OverworldEditor", "Failed to insert entrance: %s",
                result.status().message().data());
    }

  } else if (entity_type == "hole") {
    auto result = InsertEntrance(&overworld_, mouse_pos, current_map_, true);
    if (result.ok()) {
      current_entrance_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup("Entrance Editor");
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Hole inserted successfully at map=%d",
                current_map_);
    } else {
      LOG_ERROR("OverworldEditor", "Failed to insert hole: %s",
                result.status().message().data());
    }

  } else if (entity_type == "exit") {
    auto result = InsertExit(&overworld_, mouse_pos, current_map_);
    if (result.ok()) {
      current_exit_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup("Exit editor");
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Exit inserted successfully at map=%d",
                current_map_);
    } else {
      LOG_ERROR("OverworldEditor", "Failed to insert exit: %s",
                result.status().message().data());
    }

  } else if (entity_type == "item") {
    auto result = InsertItem(&overworld_, mouse_pos, current_map_, 0x00);
    if (result.ok()) {
      current_item_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup("Item editor");
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Item inserted successfully at map=%d",
                current_map_);
    } else {
      LOG_ERROR("OverworldEditor", "Failed to insert item: %s",
                result.status().message().data());
    }

  } else if (entity_type == "sprite") {
    auto result =
        InsertSprite(&overworld_, mouse_pos, current_map_, game_state_, 0x00);
    if (result.ok()) {
      current_sprite_ = **result;
      current_entity_ = *result;
      ImGui::OpenPopup("Sprite editor");
      rom_->set_dirty(true);
      LOG_DEBUG("OverworldEditor", "Sprite inserted successfully at map=%d",
                current_map_);
    } else {
      LOG_ERROR("OverworldEditor", "Failed to insert sprite: %s",
                result.status().message().data());
    }

  } else {
    LOG_WARN("OverworldEditor", "Unknown entity type: %s", entity_type.c_str());
  }
}

}  // namespace yaze::editor