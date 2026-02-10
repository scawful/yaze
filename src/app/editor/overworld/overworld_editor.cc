// Related header
#include "app/editor/overworld/overworld_editor.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

// C system headers
#include <cmath>
#include <cstddef>
#include <cstdint>

// C++ standard library headers
#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/entity_operations.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "app/editor/overworld/overworld_sidebar.h"
#include "app/editor/overworld/overworld_toolbar.h"
// Note: All overworld panels now self-register via REGISTER_PANEL macro:
// AreaGraphicsPanel, DebugWindowPanel, GfxGroupsPanel, MapPropertiesPanel,
// OverworldCanvasPanel, ScratchSpacePanel, Tile16EditorPanel, Tile16SelectorPanel,
// Tile8SelectorPanel, UsageStatisticsPanel, V3SettingsPanel
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/editor/overworld/usage_statistics_card.h"
#include "app/editor/system/panel_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/popup_id.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/imgui_memory_editor.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "app/editor/ui/toast_manager.h"
#include "core/asar_wrapper.h"
#include "core/features.h"
#include "core/project.h"
#include "rom/rom.h"
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
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

void OverworldEditor::Initialize() {
  // Register panels with PanelManager (dependency injection)
  if (!dependencies_.panel_manager) {
    return;
  }
  auto* panel_manager = dependencies_.panel_manager;

  // Initialize renderer from dependencies
  renderer_ = dependencies_.renderer;

  // Register Overworld Canvas (main canvas panel with toolset)

  // Note: All EditorPanel instances now self-register via REGISTER_PANEL macro
  // and use ContentRegistry::Context to access the current editor.
  // See comment at include section for full list of panels.

  // Original initialization code below:
  // Initialize MapPropertiesSystem with canvas and bitmap data
  // Initialize cards
  usage_stats_card_ = std::make_unique<UsageStatisticsCard>(&overworld_);
  debug_window_card_ = std::make_unique<DebugWindowCard>();

  map_properties_system_ = std::make_unique<MapPropertiesSystem>(
      &overworld_, rom_, &maps_bmp_, &ow_map_canvas_);

  // Set up refresh callbacks for MapPropertiesSystem
  map_properties_system_->SetRefreshCallbacks(
      [this]() { this->RefreshMapProperties(); },
      [this]() { this->RefreshOverworldMap(); },
      [this]() -> absl::Status { return this->RefreshMapPalette(); },
      [this]() -> absl::Status { return this->RefreshTile16Blockset(); },
      [this](int map_index) { this->ForceRefreshGraphics(map_index); });

  // Initialize OverworldSidebar
  sidebar_ = std::make_unique<OverworldSidebar>(&overworld_, rom_,
                                                map_properties_system_.get());

  // Initialize OverworldEntityRenderer for entity visualization
  entity_renderer_ = std::make_unique<OverworldEntityRenderer>(
      &overworld_, &ow_map_canvas_, &sprite_previews_);

  // Initialize Toolbar
  toolbar_ = std::make_unique<OverworldToolbar>();
  toolbar_->on_refresh_graphics = [this]() {
    // Invalidate cached graphics for the current map area to force re-render
    // with potentially new palette/graphics settings
    InvalidateGraphicsCache(current_map_);
    RefreshSiblingMapGraphics(current_map_, true);
  };
  toolbar_->on_refresh_map = [this]() {
    RefreshOverworldMap();
  };

  toolbar_->on_save_to_scratch = [this]() {
    SaveCurrentSelectionToScratch();
  };
  toolbar_->on_load_from_scratch = [this]() {
    LoadScratchToSelection();
  };

  SetupCanvasAutomation();
}

absl::Status OverworldEditor::Load() {
  gfx::ScopedTimer timer("OverworldEditor::Load");

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Clear undo/redo state when loading new ROM data
  undo_stack_.clear();
  redo_stack_.clear();
  current_paint_operation_.reset();
  if (dependencies_.undo_manager) {
    dependencies_.undo_manager->Clear();
  }

  RETURN_IF_ERROR(LoadGraphics());
  RETURN_IF_ERROR(
      tile16_editor_.Initialize(tile16_blockset_bmp_, current_gfx_bmp_,
                                *overworld_.mutable_all_tiles_types()));

  // CRITICAL FIX: Initialize tile16 editor with the correct overworld palette
  tile16_editor_.set_palette(palette_);
  tile16_editor_.SetRom(rom_);

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

    // Set up tile16 edit callback for context menu in MOUSE mode
    map_properties_system_->SetTile16EditCallback(
        [this]() { HandleTile16Edit(); });
  }

  ASSIGN_OR_RETURN(entrance_tiletypes_, zelda3::LoadEntranceTileTypes(rom_));

  // Register as palette listener to refresh graphics when palettes change
  if (palette_listener_id_ < 0) {
    palette_listener_id_ = gfx::Arena::Get().RegisterPaletteListener(
        [this](const std::string& group_name, int palette_index) {
          // Only respond to overworld-related palette changes
          if (group_name == "ow_main" || group_name == "ow_animated" ||
              group_name == "ow_aux" || group_name == "grass") {
            LOG_DEBUG("OverworldEditor",
                      "Palette change detected: %s, refreshing current map",
                      group_name.c_str());
            // Refresh current map graphics to reflect palette changes
            if (current_map_ >= 0 && all_gfx_loaded_) {
              RefreshOverworldMap();
            }
          }
        });
    LOG_DEBUG("OverworldEditor", "Registered as palette listener (ID: %d)",
              palette_listener_id_);
  }

  all_gfx_loaded_ = true;
  return absl::OkStatus();
}

absl::Status OverworldEditor::Update() {
  status_ = absl::OkStatus();

  // Safety check: Ensure ROM is loaded and graphics are ready
  if (!rom_ || !rom_->is_loaded()) {
    gui::CenterText("No ROM loaded");
    return absl::OkStatus();
  }

  if (!all_gfx_loaded_) {
    gui::CenterText("Loading graphics...");
    return absl::OkStatus();
  }

  // Process deferred textures for smooth loading
  ProcessDeferredTextures();

  // Update blockset atlas with any pending tile16 changes for live preview
  // Tile cache now uses copy semantics so this is safe to enable
  if (tile16_editor_.has_pending_changes() && map_blockset_loaded_) {
    UpdateBlocksetWithPendingTileChanges();
  }

  // Early return if panel_manager is not available
  // (panels won't be drawn without it, so no point continuing)
  if (!dependencies_.panel_manager) {
    return status_;
  }

  if (overworld_canvas_fullscreen_) {
    return status_;
  }

  // ===========================================================================
  // Main Overworld Canvas
  // ===========================================================================
  // The panels (Tile16 Selector, Area Graphics, etc.) are now managed by
  // EditorPanel/PanelManager and drawn automatically. This section only
  // handles the main canvas and toolbar.

  // ===========================================================================
  // Non-Panel Windows (not managed by EditorPanel system)
  // ===========================================================================
  // These are separate feature windows, not part of the panel system

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

  // Note: Tile16 Editor is now managed as an EditorPanel (Tile16EditorPanel)
  // It uses UpdateAsPanel() which provides a context menu instead of MenuBar

  // ===========================================================================
  // Centralized Entity Interaction Logic (extracted to dedicated method)
  // ===========================================================================
  HandleEntityInteraction();

  // Entity insertion error popup
  if (ImGui::BeginPopupModal("Entity Insert Error", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                       ICON_MD_ERROR " Entity Insertion Failed");
    ImGui::Separator();
    ImGui::TextWrapped("%s", entity_insert_error_message_.c_str());
    ImGui::Separator();
    ImGui::TextDisabled("Tip: Delete an existing entity to free up a slot.");
    ImGui::Spacing();
    if (ImGui::Button("OK", ImVec2(120, 0))) {
      entity_insert_error_message_.clear();
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  // --- END CENTRALIZED LOGIC ---

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

  // All editor windows are now rendered in Update() using either EditorPanel
  // system or MapPropertiesSystem for map-specific panels. This keeps the
  // toolset clean and prevents ImGui ID stack issues.

  // Legacy window code removed - windows rendered in Update() include:
  // - Graphics Groups (EditorPanel)
  // - Area Configuration (MapPropertiesSystem)
  // - Background Color Editor (MapPropertiesSystem)
  // - Visual Effects Editor (MapPropertiesSystem)
  // - Tile16 Editor, Usage Stats, etc. (EditorPanels)

  // Handle keyboard shortcuts (centralized in dedicated method)
  HandleKeyboardShortcuts();

  return absl::OkStatus();
}

void OverworldEditor::HandleKeyboardShortcuts() {
  // Skip processing if any ImGui item is active (e.g., text input)
  if (ImGui::IsAnyItemActive()) {
    return;
  }

  using enum EditingMode;

  const bool ctrl_held = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                         ImGui::IsKeyDown(ImGuiKey_RightCtrl);
  const bool alt_held = ImGui::IsKeyDown(ImGuiKey_LeftAlt) ||
                        ImGui::IsKeyDown(ImGuiKey_RightAlt);

  // Track mode changes for canvas usage mode updates
  EditingMode old_mode = current_mode;

  // Tool shortcuts (1-2 for mode selection)
  if (ImGui::IsKeyPressed(ImGuiKey_1, false)) {
    current_mode = EditingMode::MOUSE;
  } else if (ImGui::IsKeyPressed(ImGuiKey_2, false)) {
    current_mode = EditingMode::DRAW_TILE;
  }

  // Brush/Fill shortcuts (avoid clobbering Ctrl-based shortcuts).
  if (!ctrl_held && !alt_held) {
    if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
      ToggleBrushTool();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
      ActivateFillTool();
    }
  }

  // Update canvas usage mode when mode changes
  if (old_mode != current_mode) {
    if (current_mode == EditingMode::MOUSE) {
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
    } else {
      // DRAW_TILE and FILL_TILE are both tile painting interactions.
      ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
    }
  }

  // Entity editing shortcuts (3-8)
  HandleEntityEditingShortcuts();

  // View shortcuts
  if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
    overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
  }

  // Toggle map lock with Ctrl+L
  if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_L, false)) {
    current_map_lock_ = !current_map_lock_;
  }

  // Toggle Tile16 editor with Ctrl+T
  if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_T, false)) {
    if (dependencies_.panel_manager) {
      dependencies_.panel_manager->TogglePanel(
          0, OverworldPanelIds::kTile16Editor);
    }
  }

  // Undo/Redo shortcuts
  HandleUndoRedoShortcuts();
}

void OverworldEditor::HandleEntityEditingShortcuts() {
  // Entity type selection (3-8 keys)
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
}

void OverworldEditor::HandleUndoRedoShortcuts() {
  // Check for Ctrl key (either left or right)
  bool ctrl_held = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ||
                   ImGui::IsKeyDown(ImGuiKey_RightCtrl);
  if (!ctrl_held) {
    return;
  }

  // Ctrl+Z: Undo (or Ctrl+Shift+Z: Redo)
  if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
    bool shift_held = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                      ImGui::IsKeyDown(ImGuiKey_RightShift);
    if (shift_held) {
      status_ = Redo();  // Ctrl+Shift+Z = Redo
    } else {
      status_ = Undo();  // Ctrl+Z = Undo
    }
  }

  // Ctrl+Y: Redo (Windows style)
  if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
    status_ = Redo();
  }
}

void OverworldEditor::HandleEntityInteraction() {
  // Get hovered entity from previous frame's rendering pass
  zelda3::GameEntity* hovered_entity =
      entity_renderer_ ? entity_renderer_->hovered_entity() : nullptr;

  // Handle all MOUSE mode interactions here
  if (current_mode == EditingMode::MOUSE) {
    HandleEntityContextMenus(hovered_entity);
    HandleEntityDoubleClick(hovered_entity);
  }

  // Process any pending entity insertion from context menu
  // This must be called outside the context menu popup context for OpenPopup
  // to work
  ProcessPendingEntityInsertion();

  // Draw entity editor popups and update entity data
  DrawEntityEditorPopups();
}

void OverworldEditor::HandleEntityContextMenus(
    zelda3::GameEntity* hovered_entity) {
  if (!ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    return;
  }

  if (!hovered_entity) {
    return;
  }

  current_entity_ = hovered_entity;
  switch (hovered_entity->entity_type_) {
    case zelda3::GameEntity::EntityType::kExit:
      current_exit_ = *static_cast<zelda3::OverworldExit*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Exit Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kEntrance:
      current_entrance_ =
          *static_cast<zelda3::OverworldEntrance*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Entrance Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kItem:
      current_item_ = *static_cast<zelda3::OverworldItem*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Item Editor")
              .c_str());
      break;
    case zelda3::GameEntity::EntityType::kSprite:
      current_sprite_ = *static_cast<zelda3::Sprite*>(hovered_entity);
      ImGui::OpenPopup(
          gui::MakePopupId(gui::EditorNames::kOverworld, "Sprite Editor")
              .c_str());
      break;
    default:
      break;
  }
}

void OverworldEditor::HandleEntityDoubleClick(
    zelda3::GameEntity* hovered_entity) {
  if (!hovered_entity || !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    return;
  }

  if (hovered_entity->entity_type_ == zelda3::GameEntity::EntityType::kExit) {
    jump_to_tab_ =
        static_cast<zelda3::OverworldExit*>(hovered_entity)->room_id_;
  } else if (hovered_entity->entity_type_ ==
             zelda3::GameEntity::EntityType::kEntrance) {
    jump_to_tab_ =
        static_cast<zelda3::OverworldEntrance*>(hovered_entity)->entrance_id_;
  }
}

void OverworldEditor::DrawEntityEditorPopups() {
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
}

void OverworldEditor::DrawOverworldMaps() {
  // Get the current zoom scale for positioning and sizing
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (current_world_ * 0x40);

    // Bounds checking to prevent crashes
    if (world_index < 0 || world_index >= static_cast<int>(maps_bmp_.size())) {
      continue;  // Skip invalid map index
    }

    // Apply scale to positions for proper zoom support
    int map_x = static_cast<int>(xx * kOverworldMapSize * scale);
    int map_y = static_cast<int>(yy * kOverworldMapSize * scale);

    // Check if the map has a texture, if not, ensure it gets loaded
    if (!maps_bmp_[world_index].texture() &&
        maps_bmp_[world_index].is_active()) {
      EnsureMapTexture(world_index);
    }

    // Only draw if the map has a valid texture AND is active (has bitmap data)
    // The current_map_ check was causing crashes when hovering over unbuilt maps
    // because the bitmap would be drawn before EnsureMapBuilt() was called
    bool can_draw =
        maps_bmp_[world_index].texture() && maps_bmp_[world_index].is_active();

    if (can_draw) {
      // Draw bitmap at scaled position with scale applied to size
      ow_map_canvas_.DrawBitmap(maps_bmp_[world_index], map_x, map_y, scale);
    } else {
      // Draw a placeholder for maps that haven't loaded yet
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = ow_map_canvas_.zero_point();
      ImVec2 scrolling = ow_map_canvas_.scrolling();
      // Apply scrolling offset and use already-scaled map_x/map_y
      ImVec2 placeholder_pos = ImVec2(canvas_pos.x + scrolling.x + map_x,
                                      canvas_pos.y + scrolling.y + map_y);
      // Scale the placeholder size to match zoomed maps
      float scaled_size = kOverworldMapSize * scale;
      ImVec2 placeholder_size = ImVec2(scaled_size, scaled_size);

      // Modern loading indicator with theme colors
      draw_list->AddRectFilled(
          placeholder_pos,
          ImVec2(placeholder_pos.x + placeholder_size.x,
                 placeholder_pos.y + placeholder_size.y),
          IM_COL32(32, 32, 32, 128));  // Dark gray with transparency

      // Animated loading spinner - scale spinner radius with zoom
      ImVec2 spinner_pos = ImVec2(placeholder_pos.x + placeholder_size.x / 2,
                                  placeholder_pos.y + placeholder_size.y / 2);

      const float spinner_radius = 8.0f * scale;
      const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
      const float start_angle = rotation;
      const float end_angle = rotation + IM_PI * 1.5f;

      draw_list->PathArcTo(spinner_pos, spinner_radius, start_angle, end_angle,
                           12);
      draw_list->PathStroke(IM_COL32(100, 180, 100, 255), 0, 2.5f * scale);
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
  // drawn_tile_position() returns scaled coordinates, need to unscale
  auto scaled_position = ow_map_canvas_.drawn_tile_position();
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Convert scaled position to world coordinates
  ImVec2 mouse_position =
      ImVec2(scaled_position.x / scale, scaled_position.y / scale);

  int map_x = static_cast<int>(mouse_position.x) / kOverworldMapSize;
  int map_y = static_cast<int>(mouse_position.y) / kOverworldMapSize;
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
  const int world_offset = current_world_ * 0x40;
  const int local_map = current_map_ - world_offset;
  const int superY = local_map / 8;
  const int superX = local_map % 8;
  int mouse_x = static_cast<int>(mouse_position.x);
  int mouse_y = static_cast<int>(mouse_position.y);
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

  // Get old tile value for undo tracking
  int old_tile_id = selected_world[index_x][index_y];

  // Only record undo if tile is actually changing
  if (old_tile_id != current_tile16_) {
    CreateUndoPoint(current_map_, current_world_, index_x, index_y,
                    old_tile_id);
    rom_->set_dirty(true);
  }

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
  // Note: With TileSelectorWidget, we check if a valid tile is selected instead
  // of canvas points
  if (current_mode == EditingMode::DRAW_TILE && current_tile16_ >= 0 &&
      !ow_map_canvas_.select_rect_active() &&
      ow_map_canvas_.DrawTilemapPainter(tile16_blockset_, current_tile16_)) {
    DrawOverworldEdits();
  }

  // Fill tool: fill the entire 32x32 tile16 screen under the cursor using the
  // current selection pattern (if any) or the current tile16.
  if (current_mode == EditingMode::FILL_TILE && ow_map_canvas_.IsMouseHovering() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    float scale = ow_map_canvas_.global_scale();
    if (scale <= 0.0f) {
      scale = 1.0f;
    }

    const bool allow_special_tail =
        core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion;
    const auto scaled_position = ow_map_canvas_.hover_mouse_pos();
    const int map_x =
        static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
    const int map_y =
        static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;

    // Bounds guard.
    if (map_x >= 0 && map_x < 8 && map_y >= 0 && map_y < 8) {
      // Special world only renders 4 rows unless tail expansion is enabled.
      if (allow_special_tail || current_world_ != 2 || map_y < 4) {
        const int local_map = map_x + (map_y * 8);
        const int target_map = local_map + (current_world_ * 0x40);
        if (target_map >= 0 && target_map < zelda3::kNumOverworldMaps) {
          // Build pattern from active rectangle selection (if present).
          std::vector<int> pattern_ids;
          int pattern_w = 1;
          int pattern_h = 1;

          if (ow_map_canvas_.select_rect_active() &&
              ow_map_canvas_.selected_points().size() >= 2) {
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

            pattern_w = std::max(1, end_x - start_x + 1);
            pattern_h = std::max(1, end_y - start_y + 1);
            pattern_ids.reserve(pattern_w * pattern_h);

            overworld_.set_current_world(current_world_);
            overworld_.set_current_map(target_map);
            for (int y = start_y; y <= end_y; ++y) {
              for (int x = start_x; x <= end_x; ++x) {
                pattern_ids.push_back(overworld_.GetTile(x, y));
              }
            }
          } else {
            pattern_ids = {current_tile16_};
          }

          auto& world_tiles =
              (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
              : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                                      : overworld_.mutable_map_tiles()->special_world;

          // Apply the fill (repeat pattern across 32x32).
          for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
              const int pattern_x = x % pattern_w;
              const int pattern_y = y % pattern_h;
              const int new_tile_id = pattern_ids[pattern_y * pattern_w + pattern_x];

              const int global_x = map_x * 32 + x;
              const int global_y = map_y * 32 + y;
              if (global_x < 0 || global_x >= 256 || global_y < 0 ||
                  global_y >= 256) {
                continue;
              }

              const int old_tile_id = world_tiles[global_x][global_y];
              if (old_tile_id == new_tile_id) {
                continue;
              }

              CreateUndoPoint(target_map, current_world_, global_x, global_y,
                              old_tile_id);
              world_tiles[global_x][global_y] = new_tile_id;
            }
          }

          rom_->set_dirty(true);
          FinalizePaintOperation();
          current_map_ = target_map;
          RefreshOverworldMapOnDemand(target_map);
        }
      }
    }
  }

  // Rectangle selection stamping (brush mode only).
  if (current_mode == EditingMode::DRAW_TILE && ow_map_canvas_.select_rect_active()) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      LOG_DEBUG("OverworldEditor",
                "CheckForOverworldEdits: About to apply rectangle selection");

      auto& selected_world =
          (current_world_ == 0) ? overworld_.mutable_map_tiles()->light_world
          : (current_world_ == 1)
              ? overworld_.mutable_map_tiles()->dark_world
              : overworld_.mutable_map_tiles()->special_world;
      // selected_points are now stored in world coordinates
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
      // CRITICAL FIX: Use pre-computed tile16_ids_ instead of recalculating
      // from selected_tiles_ This prevents wrapping issues when dragging near
      // boundaries
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
          // Bounds check for the selected world array, accounting for rectangle
          // size Ensure the entire rectangle fits within the world bounds
          int rect_width = ((end_x - start_x) / kTile16Size) + 1;
          int rect_height = ((end_y - start_y) / kTile16Size) + 1;

          // Prevent painting from wrapping around at the edges of large maps
          // Only allow painting if the entire rectangle is within the same
          // 512x512 local map
          int start_local_map_x = start_x / local_map_size;
          int start_local_map_y = start_y / local_map_size;
          int end_local_map_x = end_x / local_map_size;
          int end_local_map_y = end_y / local_map_size;

          bool in_same_local_map = (start_local_map_x == end_local_map_x) &&
                                   (start_local_map_y == end_local_map_y);

          if (in_same_local_map && index_x >= 0 &&
              (index_x + rect_width - 1) < 0x200 && index_y >= 0 &&
              (index_y + rect_height - 1) < 0x200) {
            // Get old tile value for undo tracking
            int old_tile_id = selected_world[index_x][index_y];
            if (old_tile_id != tile16_id) {
              CreateUndoPoint(current_map_, current_world_, index_x, index_y,
                              old_tile_id);
            }

            selected_world[index_x][index_y] = tile16_id;

            // CRITICAL FIX: Also update the bitmap directly like single tile
            // drawing
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

      // Finalize the undo batch operation after all tiles are placed
      FinalizePaintOperation();

      rom_->set_dirty(true);
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
  // Pass the canvas scale for proper zoom handling
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  ow_map_canvas_.DrawSelectRect(current_map_, 0x10, scale);

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
    // selected_points are now stored in world coordinates
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
  // Unscale coordinates to get world position
  const ImVec2 scaled_anchor = ow_map_canvas_.drawn_tile_position();
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  const ImVec2 anchor =
      ImVec2(scaled_anchor.x / scale, scaled_anchor.y / scale);

  // Compute anchor in tile16 grid within the current map
  const int tile16_x =
      (static_cast<int>(anchor.x) % kOverworldMapSize) / kTile16Size;
  const int tile16_y =
      (static_cast<int>(anchor.y) % kOverworldMapSize) / kTile16Size;

  auto& selected_world =
      (current_world_ == 0)   ? overworld_.mutable_map_tiles()->light_world
      : (current_world_ == 1) ? overworld_.mutable_map_tiles()->dark_world
                              : overworld_.mutable_map_tiles()->special_world;

  const int world_offset = current_world_ * 0x40;
  const int local_map = current_map_ - world_offset;
  const int superY = local_map / 8;
  const int superX = local_map % 8;

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
      // TODO: Track undo points for paste operations (treated as a batch).
      selected_world[global_x][global_y] = id;
    }
  }

  rom_->set_dirty(true);
  RefreshOverworldMap();
  return absl::OkStatus();
}

absl::Status OverworldEditor::CheckForCurrentMap() {
  // 4096x4096, 512x512 maps and some are larges maps 1024x1024
  // hover_mouse_pos() returns canvas-local coordinates but they're scaled
  // Unscale to get world coordinates for map detection
  const auto scaled_position = ow_map_canvas_.hover_mouse_pos();
  float scale = ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;
  const int large_map_size = 1024;

  // Calculate which small map the mouse is currently over
  // Unscale coordinates to get world position
  int map_x = static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
  int map_y = static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;

  // Bounds check to prevent out-of-bounds access
  if (map_x < 0 || map_x >= 8 || map_y < 0 || map_y >= 8) {
    return absl::OkStatus();
  }

  const bool allow_special_tail =
      core::FeatureFlags::get().overworld.kEnableSpecialWorldExpansion;
  if (!allow_special_tail && current_world_ == 2 && map_y >= 4) {
    // Special world is only 4 rows high unless expansion is enabled
    return absl::OkStatus();
  }

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

    // Hover debouncing: Only build expensive maps after dwelling on them
    // This prevents lag when rapidly moving mouse across the overworld
    bool should_build = false;
    if (hovered_map != last_hovered_map_) {
      // New map hovered - reset timer
      last_hovered_map_ = hovered_map;
      hover_time_ = 0.0f;
      // Check if already built (instant display)
      should_build = overworld_.overworld_map(hovered_map)->is_built();
    } else {
      // Same map - accumulate hover time
      hover_time_ += ImGui::GetIO().DeltaTime;
      // Build after delay OR if clicking
      should_build = (hover_time_ >= kHoverBuildDelay) ||
                     ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                     ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    }

    // Only trigger expensive build if debounce threshold met
    if (should_build) {
      RETURN_IF_ERROR(overworld_.EnsureMapBuilt(current_map_));
    }

    // After dwelling longer, start pre-loading adjacent maps
    if (hover_time_ >= kPreloadStartDelay && preload_queue_.empty()) {
      QueueAdjacentMapsForPreload(current_map_);
    }

    // Process one preload per frame (background optimization)
    ProcessPreloadQueue();
  }

  const int current_highlighted_map = current_map_;

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

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

      // CRITICAL FIX: Account for world offset when calculating parent
      // coordinates For Dark World (0x40-0x7F), parent IDs are in range
      // 0x40-0x7F For Special World (0x80-0x9F), parent IDs are in range
      // 0x80-0x9F We need to subtract the world offset to get display grid
      // coordinates (0-7)
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
        // Special World (0x80-0x9F) - use display coordinates based on
        // current_world_ The special world maps are displayed in the same 8x8
        // grid as LW/DW
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
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_.atlas);
    }

    // Update map texture with the traditional direct update approach
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &maps_bmp_[current_map_]);
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

void OverworldEditor::CheckForMousePan() {
  // Legacy wrapper - now calls HandleOverworldPan
  HandleOverworldPan();
}

void OverworldEditor::DrawOverworldCanvas() {
  // Simplified map settings - compact row with popup panels for detailed
  // editing
  if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
    const EditingMode old_mode = current_mode;
    bool has_selection = ow_map_canvas_.select_rect_active() &&
                         !ow_map_canvas_.selected_tiles().empty();

    // Check if scratch space has data
    bool scratch_has_data = scratch_space_.in_use;

    // Pass PanelManager to toolbar for panel visibility management
    toolbar_->Draw(current_world_, current_map_, current_map_lock_,
                   current_mode, entity_edit_mode_, dependencies_.panel_manager,
                   has_selection, scratch_has_data, rom_, &overworld_);

    // Toolbar toggles don't currently update canvas usage mode.
    if (old_mode != current_mode) {
      if (current_mode == EditingMode::MOUSE) {
        ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
      } else {
        ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
      }
    }
  }

  // ==========================================================================
  // PHASE 3: Modern BeginCanvas/EndCanvas Pattern
  // ==========================================================================
  // Context menu setup MUST happen BEFORE BeginCanvas (lesson from dungeon)
  bool show_context_menu =
      (current_mode == EditingMode::MOUSE) &&
      (!entity_renderer_ || entity_renderer_->hovered_entity() == nullptr);

  if (rom_->is_loaded() && overworld_.is_loaded() && map_properties_system_) {
    ow_map_canvas_.ClearContextMenuItems();
    map_properties_system_->SetupCanvasContextMenu(
        ow_map_canvas_, current_map_, current_map_lock_,
        show_map_properties_panel_, show_custom_bg_color_editor_,
        show_overlay_editor_, static_cast<int>(current_mode));
  }

  // Configure canvas frame options
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = kOverworldCanvasSize;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 64.0f;  // Map boundaries (512px / 8 maps)
  frame_opts.draw_context_menu = show_context_menu;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;  // CRITICAL: Canvas has own pan logic

  // Wrap in child window for scrollbars
  gui::BeginNoPadding();
  gui::BeginChildBothScrollbars(7);

  // Keep canvas scroll at 0 - ImGui's child window handles all scrolling
  // The scrollbars scroll the child window which moves the entire canvas
  ow_map_canvas_.set_scrolling(ImVec2(0, 0));

  // Begin canvas frame - this handles DrawBackground + DrawContextMenu
  auto canvas_rt = gui::BeginCanvas(ow_map_canvas_, frame_opts);
  gui::EndNoPadding();

  // Handle pan via ImGui scrolling (instead of canvas internal scroll)
  HandleOverworldPan();
  HandleOverworldZoom();

  // Tile painting mode - handle tile edits and right-click tile picking
  if (current_mode == EditingMode::DRAW_TILE ||
      current_mode == EditingMode::FILL_TILE) {
    HandleMapInteraction();
  }

  if (overworld_.is_loaded()) {
    // Draw the 64 overworld map bitmaps
    DrawOverworldMaps();

    // Draw all entities using the new CanvasRuntime-based methods
    if (entity_renderer_) {
      entity_renderer_->DrawExits(canvas_rt, current_world_);
      entity_renderer_->DrawEntrances(canvas_rt, current_world_);
      entity_renderer_->DrawItems(canvas_rt, current_world_);
      entity_renderer_->DrawSprites(canvas_rt, current_world_, game_state_);
    }

    // Draw overlay preview if enabled
    if (show_overlay_preview_ && map_properties_system_) {
      map_properties_system_->DrawOverlayPreviewOnMap(
          current_map_, current_world_, show_overlay_preview_);
    }

    if (current_mode == EditingMode::DRAW_TILE ||
        current_mode == EditingMode::FILL_TILE) {
      CheckForOverworldEdits();
    }

    // Use canvas runtime hover state for map detection
    if (canvas_rt.hovered) {
      status_ = CheckForCurrentMap();
    }

    // --- BEGIN ENTITY DRAG/DROP LOGIC ---
    if (current_mode == EditingMode::MOUSE && entity_renderer_) {
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
        float scale = canvas_rt.scale;
        if (scale > 0.0f) {
          dragged_entity_->x_ += mouse_delta.x / scale;
          dragged_entity_->y_ += mouse_delta.y / scale;
        }
      }

      // 3. End drag
      if (is_dragging_entity_ &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (dragged_entity_) {
          float end_scale = canvas_rt.scale;
          MoveEntityOnGrid(dragged_entity_, canvas_rt.canvas_p0,
                           canvas_rt.scrolling, dragged_entity_free_movement_,
                           end_scale);
          // Pass overworld context for proper area size detection
          dragged_entity_->UpdateMapProperties(dragged_entity_->map_id_,
                                               &overworld_);
          rom_->set_dirty(true);
        }
        is_dragging_entity_ = false;
        dragged_entity_ = nullptr;
        dragged_entity_free_movement_ = false;
      }
    }
    // --- END ENTITY DRAG/DROP LOGIC ---
  }

  // End canvas frame - draws grid/overlay based on frame_opts
  gui::EndCanvas(ow_map_canvas_, canvas_rt, frame_opts);
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
    // Set the current tile in the editor (original behavior)
    auto status = tile16_editor_.SetCurrentTile(current_tile16_);
    if (!status.ok()) {
      util::logf("Failed to set tile16: %s", status.message().data());
    }
    // Note: We do NOT auto-scroll here because it breaks user interaction.
    // The canvas should only scroll when explicitly requested (e.g., when
    // selecting a tile from the overworld canvas via
    // ScrollBlocksetCanvasToCurrentTile).
  }

  if (result.tile_double_clicked) {
    if (dependencies_.panel_manager) {
      dependencies_.panel_manager->ShowPanel(OverworldPanelIds::kTile16Editor);
    }
  }

  ImGui::EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

void OverworldEditor::DrawTile8Selector() {
  // Configure canvas frame options for graphics bin
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = kGraphicsBinCanvasSize;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 16.0f;  // Tile8 grid
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  auto canvas_rt = gui::BeginCanvas(graphics_bin_canvas_, frame_opts);

  if (all_gfx_loaded_) {
    int key = 0;
    for (auto& value : gfx::Arena::Get().gfx_sheets()) {
      int offset = 0x40 * (key + 1);
      int top_left_y = canvas_rt.canvas_p0.y + 2;
      if (key >= 1) {
        top_left_y = canvas_rt.canvas_p0.y + 0x40 * key;
      }
      auto texture = value.texture();
      canvas_rt.draw_list->AddImage(
          (ImTextureID)(intptr_t)texture,
          ImVec2(canvas_rt.canvas_p0.x + 2, top_left_y),
          ImVec2(canvas_rt.canvas_p0.x + 0x100,
                 canvas_rt.canvas_p0.y + offset));
      key++;
    }
  }

  gui::EndCanvas(graphics_bin_canvas_, canvas_rt, frame_opts);
}

void OverworldEditor::InvalidateGraphicsCache(int map_id) {
  if (map_id < 0) {
    // Invalidate all maps - clear both editor cache and Overworld's tileset cache
    current_graphics_set_.clear();
    overworld_.ClearGraphicsConfigCache();
  } else {
    // Invalidate specific map and its siblings in the Overworld's tileset cache
    current_graphics_set_.erase(map_id);
    overworld_.InvalidateSiblingMapCaches(map_id);
  }
}

absl::Status OverworldEditor::DrawAreaGraphics() {
  if (overworld_.is_loaded()) {
    // Always ensure current map graphics are loaded
    if (!current_graphics_set_.contains(current_map_)) {
      overworld_.set_current_map(current_map_);
      palette_ = overworld_.current_area_palette();
      auto bmp = std::make_unique<gfx::Bitmap>();
      bmp->Create(0x80, kOverworldMapSize, 0x08, overworld_.current_graphics());
      bmp->SetPalette(palette_);
      current_graphics_set_[current_map_] = std::move(bmp);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          current_graphics_set_[current_map_].get());
    }
  }

  // Configure canvas frame options for area graphics
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = kCurrentGfxCanvasSize;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 32.0f;  // Tile selector grid
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##AreaGraphicsScrollRegion");

  auto canvas_rt = gui::BeginCanvas(current_gfx_canvas_, frame_opts);
  gui::EndPadding();

  if (current_graphics_set_.contains(current_map_) &&
      current_graphics_set_[current_map_]->is_active()) {
    current_gfx_canvas_.DrawBitmap(*current_graphics_set_[current_map_], 2, 2,
                                   2.0f);
  }
  current_gfx_canvas_.DrawTileSelector(32.0f);

  gui::EndCanvas(current_gfx_canvas_, canvas_rt, frame_opts);
  ImGui::EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

absl::Status OverworldEditor::UpdateGfxGroupEditor() {
  // Delegate to the existing GfxGroupEditor
  if (rom_ && rom_->is_loaded()) {
    return gfx_group_editor_.Update();
  } else {
    gui::CenterText("No ROM loaded");
    return absl::OkStatus();
  }
}

void OverworldEditor::DrawV3Settings() {
  // v3 Settings panel - placeholder for ZSCustomOverworld configuration
  ImGui::TextWrapped("ZSCustomOverworld v3 settings panel");
  ImGui::Separator();

  if (!rom_ || !rom_->is_loaded()) {
    gui::CenterText("No ROM loaded");
    return;
  }

  ImGui::TextWrapped(
      "This panel will contain ZSCustomOverworld configuration options "
      "such as custom map sizes, extended tile sets, and other v3 features.");

  // TODO: Implement v3 settings UI
  // Could include:
  // - Custom map size toggles
  // - Extended tileset configuration
  // - Override settings
  // - Version information display
}

void OverworldEditor::DrawMapProperties() {
  // Area Configuration panel
  static bool show_custom_bg_color_editor = false;
  static bool show_overlay_editor = false;
  static int game_state = 0;  // 0=Beginning, 1=Zelda Saved, 2=Master Sword

  if (sidebar_) {
    sidebar_->Draw(current_world_, current_map_, current_map_lock_, game_state,
                   show_custom_bg_color_editor, show_overlay_editor);
  }

  // Draw popups if triggered from sidebar
  if (show_custom_bg_color_editor) {
    ImGui::OpenPopup("CustomBGColorEditor");
    show_custom_bg_color_editor = false;  // Reset after opening
  }
  if (show_overlay_editor) {
    ImGui::OpenPopup("OverlayEditor");
    show_overlay_editor = false;  // Reset after opening
  }

  if (ImGui::BeginPopup("CustomBGColorEditor")) {
    if (map_properties_system_) {
      map_properties_system_->DrawCustomBackgroundColorEditor(
          current_map_, show_custom_bg_color_editor);
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("OverlayEditor")) {
    if (map_properties_system_) {
      map_properties_system_->DrawOverlayEditor(current_map_,
                                                show_overlay_editor);
    }
    ImGui::EndPopup();
  }
}

absl::Status OverworldEditor::Save() {
  // HACK MANIFEST VALIDATION
  const bool saving_maps = core::FeatureFlags::get().overworld.kSaveOverworldMaps;
  if (saving_maps && dependencies_.project &&
      dependencies_.project->hack_manifest.loaded()) {
    const auto& manifest = dependencies_.project->hack_manifest;
    const auto write_policy = dependencies_.project->rom_metadata.write_policy;

    // Calculate memory ranges that would be written by overworld map saves.
    // `ranges` are PC offsets (ROM file offsets). The hack manifest is in SNES
    // address space (LoROM), so convert before analysis.
    auto ranges = overworld_.GetProjectedWriteRanges();
    auto conflicts = manifest.AnalyzePcWriteRanges(ranges);
    if (!conflicts.empty()) {
      std::string error_msg =
          "Hack manifest write conflicts while saving overworld maps:\n\n";
      for (const auto& conflict : conflicts) {
        absl::StrAppend(
            &error_msg,
            absl::StrFormat("- Address 0x%06X is %s", conflict.address,
                            core::AddressOwnershipToString(conflict.ownership)));
        if (!conflict.module.empty()) {
          absl::StrAppend(&error_msg, " (Module: ", conflict.module, ")");
        }
        absl::StrAppend(&error_msg, "\n");
      }

      if (write_policy == project::RomWritePolicy::kAllow) {
        LOG_DEBUG("OverworldEditor", "%s", error_msg.c_str());
      } else {
        LOG_WARN("OverworldEditor", "%s", error_msg.c_str());
      }

      if (dependencies_.toast_manager &&
          write_policy == project::RomWritePolicy::kWarn) {
        dependencies_.toast_manager->Show(
            "Save warning: write conflict with hack manifest (see log)",
            ToastType::kWarning);
      }

      if (write_policy == project::RomWritePolicy::kBlock) {
        if (dependencies_.toast_manager) {
          dependencies_.toast_manager->Show(
              "Save blocked: write conflict with hack manifest (see log)",
              ToastType::kError);
        }
        return absl::PermissionDeniedError("Write conflict with Hack Manifest");
      }
    }
  }

  if (saving_maps) {
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

// ============================================================================
// Undo/Redo System Implementation
// ============================================================================

auto& OverworldEditor::GetWorldTiles(int world) {
  switch (world) {
    case 0:
      return overworld_.mutable_map_tiles()->light_world;
    case 1:
      return overworld_.mutable_map_tiles()->dark_world;
    default:
      return overworld_.mutable_map_tiles()->special_world;
  }
}

void OverworldEditor::CreateUndoPoint(int map_id, int world, int x, int y,
                                      int old_tile_id) {
  auto now = std::chrono::steady_clock::now();

  // Check if we should batch with current operation (same map, same world,
  // within timeout)
  if (current_paint_operation_.has_value() &&
      current_paint_operation_->map_id == map_id &&
      current_paint_operation_->world == world &&
      (now - last_paint_time_) < kPaintBatchTimeout) {
    // Add to existing operation
    current_paint_operation_->tile_changes.emplace_back(std::make_pair(x, y),
                                                        old_tile_id);
  } else {
    // Finalize any pending operation before starting a new one
    FinalizePaintOperation();

    // Start new operation
    current_paint_operation_ =
        OverworldUndoPoint{.map_id = map_id,
                           .world = world,
                           .tile_changes = {{{x, y}, old_tile_id}},
                           .timestamp = now};
  }

  last_paint_time_ = now;
}

void OverworldEditor::FinalizePaintOperation() {
  if (!current_paint_operation_.has_value()) {
    return;
  }

  // Also push to the UndoManager if available (new framework path).
  // Build the action before we move current_paint_operation_ into the
  // legacy stack, because we need to read the tile changes.
  if (dependencies_.undo_manager) {
    auto& world_tiles = GetWorldTiles(current_paint_operation_->world);
    std::vector<OverworldTileChange> changes;
    changes.reserve(current_paint_operation_->tile_changes.size());
    for (const auto& [coords, old_tile_id] :
         current_paint_operation_->tile_changes) {
      auto [x, y] = coords;
      int new_tile_id = world_tiles[x][y];
      changes.push_back({x, y, old_tile_id, new_tile_id});
    }
    auto action = std::make_unique<OverworldTilePaintAction>(
        current_paint_operation_->map_id, current_paint_operation_->world,
        std::move(changes), &overworld_,
        [this]() { RefreshOverworldMap(); });
    dependencies_.undo_manager->Push(std::move(action));
  }

  // Legacy undo stack path (always maintained for fallback)
  redo_stack_.clear();

  // Add to undo stack
  undo_stack_.push_back(std::move(*current_paint_operation_));
  current_paint_operation_.reset();

  // Limit stack size
  while (undo_stack_.size() > kMaxUndoHistory) {
    undo_stack_.erase(undo_stack_.begin());
  }
}

void OverworldEditor::ApplyUndoPoint(const OverworldUndoPoint& point) {
  auto& world_tiles = GetWorldTiles(point.world);

  // Apply all tile changes
  for (const auto& [coords, tile_id] : point.tile_changes) {
    auto [x, y] = coords;
    world_tiles[x][y] = tile_id;
  }

  // Refresh the map visuals
  RefreshOverworldMap();
}

absl::Status OverworldEditor::Undo() {
  // Finalize any pending paint operation first
  FinalizePaintOperation();

  // Delegate to UndoManager if available (new framework path)
  if (dependencies_.undo_manager) {
    return dependencies_.undo_manager->Undo();
  }

  // Legacy fallback: use local undo_stack_
  if (undo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to undo");
  }

  OverworldUndoPoint point = std::move(undo_stack_.back());
  undo_stack_.pop_back();

  // Create redo point with current tile values before restoring
  OverworldUndoPoint redo_point{.map_id = point.map_id,
                                .world = point.world,
                                .tile_changes = {},
                                .timestamp = std::chrono::steady_clock::now()};

  auto& world_tiles = GetWorldTiles(point.world);

  // Swap tiles and record for redo
  for (const auto& [coords, old_tile_id] : point.tile_changes) {
    auto [x, y] = coords;
    int current_tile_id = world_tiles[x][y];

    // Record current value for redo
    redo_point.tile_changes.emplace_back(coords, current_tile_id);

    // Restore old value
    world_tiles[x][y] = old_tile_id;
  }

  redo_stack_.push_back(std::move(redo_point));

  // Refresh the map visuals
  RefreshOverworldMap();

  return absl::OkStatus();
}

absl::Status OverworldEditor::Redo() {
  // Delegate to UndoManager if available (new framework path)
  if (dependencies_.undo_manager) {
    return dependencies_.undo_manager->Redo();
  }

  // Legacy fallback: use local redo_stack_
  if (redo_stack_.empty()) {
    return absl::FailedPreconditionError("Nothing to redo");
  }

  OverworldUndoPoint point = std::move(redo_stack_.back());
  redo_stack_.pop_back();

  // Create undo point with current tile values
  OverworldUndoPoint undo_point{.map_id = point.map_id,
                                .world = point.world,
                                .tile_changes = {},
                                .timestamp = std::chrono::steady_clock::now()};

  auto& world_tiles = GetWorldTiles(point.world);

  // Swap tiles and record for undo
  for (const auto& [coords, tile_id] : point.tile_changes) {
    auto [x, y] = coords;
    int current_tile_id = world_tiles[x][y];

    // Record current value for undo
    undo_point.tile_changes.emplace_back(coords, current_tile_id);

    // Apply redo value
    world_tiles[x][y] = tile_id;
  }

  undo_stack_.push_back(std::move(undo_point));

  // Refresh the map visuals
  RefreshOverworldMap();

  return absl::OkStatus();
}

// ============================================================================

absl::Status OverworldEditor::LoadGraphics() {
  gfx::ScopedTimer timer("LoadGraphics");

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
  // Load the Link to the Past overworld.
  {
    gfx::ScopedTimer load_timer("Overworld::Load");
    RETURN_IF_ERROR(overworld_.Load(rom_));
  }
  palette_ = overworld_.current_area_palette();

  // Fix: Set transparency for the first color of each 16-color subpalette
  // This ensures the background color (backdrop) shows through
  for (size_t i = 0; i < palette_.size(); i += 16) {
    if (i < palette_.size()) {
      palette_[i].set_transparent(true);
    }
  }

  LOG_DEBUG("OverworldEditor", "Loading overworld graphics (optimized).");

  // Phase 1: Create bitmaps without textures for faster loading
  // This avoids blocking the main thread with GPU texture creation
  {
    gfx::ScopedTimer gfx_timer("CreateBitmapWithoutTexture_Graphics");
    current_gfx_bmp_.Create(0x80, kOverworldMapSize, 0x40,
                            overworld_.current_graphics());
    current_gfx_bmp_.SetPalette(palette_);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &current_gfx_bmp_);
  }

  LOG_DEBUG("OverworldEditor",
            "Loading overworld tileset (deferred textures).");
  {
    gfx::ScopedTimer tileset_timer("CreateBitmapWithoutTexture_Tileset");
    tile16_blockset_bmp_.Create(0x80, 0x2000, 0x08,
                                overworld_.tile16_blockset_data());
    tile16_blockset_bmp_.SetPalette(palette_);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &tile16_blockset_bmp_);
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
  // IMPORTANT: Must match kEssentialMapsPerWorld in overworld.cc
#ifdef __EMSCRIPTEN__
  constexpr int kEssentialMapsPerWorld = 4;  // Match WASM build in overworld.cc
#else
  constexpr int kEssentialMapsPerWorld =
      16;  // Match native build in overworld.cc
#endif
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
      // Queue texture creation/update for initial maps via Arena's deferred
      // system
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
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          &sprite_previews_[sprite.id()]);
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

void OverworldEditor::QueueAdjacentMapsForPreload(int center_map) {
#ifdef __EMSCRIPTEN__
  // WASM: Skip pre-loading entirely - it blocks the main thread and causes
  // stuttering. The tileset cache and debouncing provide enough optimization.
  return;
#endif

  if (center_map < 0 || center_map >= zelda3::kNumOverworldMaps) {
    return;
  }

  preload_queue_.clear();

  // Calculate grid position (8x8 maps per world)
  int world_offset = (center_map / 64) * 64;
  int local_index = center_map % 64;
  int map_x = local_index % 8;
  int map_y = local_index / 8;
  int max_rows = (center_map >= zelda3::kSpecialWorldMapIdStart) ? 4 : 8;

  // Add adjacent maps (4-connected neighbors)
  static const int dx[] = {-1, 1, 0, 0};
  static const int dy[] = {0, 0, -1, 1};

  for (int i = 0; i < 4; ++i) {
    int nx = map_x + dx[i];
    int ny = map_y + dy[i];

    // Check bounds (world grid; special world is only 4 rows high)
    if (nx >= 0 && nx < 8 && ny >= 0 && ny < max_rows) {
      int neighbor_index = world_offset + ny * 8 + nx;
      // Only queue if not already built
      if (neighbor_index >= 0 && neighbor_index < zelda3::kNumOverworldMaps &&
          !overworld_.overworld_map(neighbor_index)->is_built()) {
        preload_queue_.push_back(neighbor_index);
      }
    }
  }
}

void OverworldEditor::ProcessPreloadQueue() {
#ifdef __EMSCRIPTEN__
  // WASM: Pre-loading disabled - each EnsureMapBuilt call blocks for 100-200ms
  // which causes unacceptable frame drops. Native builds use this for smoother UX.
  return;
#endif

  if (preload_queue_.empty()) {
    return;
  }

  // Process one map per frame to avoid blocking (native only)
  int map_to_preload = preload_queue_.back();
  preload_queue_.pop_back();

  // Silent build - don't update UI state
  auto status = overworld_.EnsureMapBuilt(map_to_preload);
  if (!status.ok()) {
    // Log but don't interrupt - this is background work
    LOG_DEBUG("OverworldEditor", "Background preload of map %d failed: %s",
              map_to_preload, status.message().data());
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
    // Mark for deferred refresh - will be processed when the map becomes
    // visible
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
  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

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
 * It always works from the parent perspective to ensure consistent behavior
 * whether the trigger map is the parent or a child.
 *
 * Key improvements:
 * - Uses parameter-based recursion guard instead of static set
 * - Always works from parent perspective for consistent sibling coordination
 * - Respects ZScream area size logic for v3+ ROMs
 * - Falls back to large_map flag for vanilla/v2 ROMs
 */
void OverworldEditor::RefreshMultiAreaMapsSafely(int map_index,
                                                 zelda3::OverworldMap* map) {
  using zelda3::AreaSizeEnum;

  auto area_size = map->area_size();
  if (area_size == AreaSizeEnum::SmallArea) {
    return;  // No siblings to coordinate
  }

  // Always work from parent perspective for consistent coordination
  int parent_id = map->parent();

  // If we're not the parent, get the parent map to work from
  auto* parent_map = overworld_.mutable_overworld_map(parent_id);
  if (!parent_map) {
    LOG_WARN(
        "OverworldEditor",
        "RefreshMultiAreaMapsSafely: Could not get parent map %d for map %d",
        parent_id, map_index);
    return;
  }

  LOG_DEBUG("OverworldEditor",
            "RefreshMultiAreaMapsSafely: Processing %s area from parent %d "
            "(trigger: %d)",
            (area_size == AreaSizeEnum::LargeArea)  ? "large"
            : (area_size == AreaSizeEnum::WideArea) ? "wide"
                                                    : "tall",
            parent_id, map_index);

  // Determine all maps that are part of this multi-area structure
  // based on the parent's position and area size
  std::vector<int> sibling_maps;

  switch (area_size) {
    case AreaSizeEnum::LargeArea:
      // Large Area: 2x2 grid (4 maps total)
      sibling_maps = {parent_id, parent_id + 1, parent_id + 8, parent_id + 9};
      break;

    case AreaSizeEnum::WideArea:
      // Wide Area: 2x1 grid (2 maps total, horizontally adjacent)
      sibling_maps = {parent_id, parent_id + 1};
      break;

    case AreaSizeEnum::TallArea:
      // Tall Area: 1x2 grid (2 maps total, vertically adjacent)
      sibling_maps = {parent_id, parent_id + 8};
      break;

    default:
      LOG_WARN("OverworldEditor",
               "RefreshMultiAreaMapsSafely: Unknown area size %d for map %d",
               static_cast<int>(area_size), map_index);
      return;
  }

  // Refresh all siblings (including self if different from trigger)
  // The trigger map (map_index) was already processed by the caller,
  // so we skip it to avoid double-processing
  for (int sibling : sibling_maps) {
    // Skip the trigger map - it was already processed by RefreshChildMapOnDemand
    if (sibling == map_index) {
      continue;
    }

    // Bounds check
    if (sibling < 0 || sibling >= zelda3::kNumOverworldMaps) {
      continue;
    }

    // Check visibility - only immediately refresh visible maps
    bool is_current_map = (sibling == current_map_);
    bool is_current_world = (sibling / 0x40 == current_world_);

    // Always mark sibling as needing refresh to ensure consistency
    maps_bmp_[sibling].set_modified(true);

    if (is_current_map || is_current_world) {
      LOG_DEBUG("OverworldEditor",
                "RefreshMultiAreaMapsSafely: Refreshing sibling map %d",
                sibling);

      // Direct refresh for visible siblings
      auto* sibling_map = overworld_.mutable_overworld_map(sibling);
      if (!sibling_map)
        continue;

      sibling_map->LoadAreaGraphics();

      auto status = sibling_map->BuildTileset();
      if (!status.ok()) {
        LOG_ERROR("OverworldEditor",
                  "Failed to build tileset for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->BuildTiles16Gfx(*overworld_.mutable_tiles16(),
                                            overworld_.tiles16().size());
      if (!status.ok()) {
        LOG_ERROR("OverworldEditor",
                  "Failed to build tiles16 for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->LoadPalette();
      if (!status.ok()) {
        LOG_ERROR("OverworldEditor",
                  "Failed to load palette for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      status = sibling_map->BuildBitmap(overworld_.GetMapTiles(current_world_));
      if (!status.ok()) {
        LOG_ERROR("OverworldEditor",
                  "Failed to build bitmap for sibling %d: %s", sibling,
                  status.message().data());
        continue;
      }

      // Update bitmap data
      maps_bmp_[sibling].set_data(sibling_map->bitmap_data());

      // Set palette if bitmap has a valid surface
      if (maps_bmp_[sibling].is_active() && maps_bmp_[sibling].surface()) {
        maps_bmp_[sibling].SetPalette(sibling_map->current_palette());
      }
      maps_bmp_[sibling].set_modified(false);

      // Queue texture update/creation
      if (maps_bmp_[sibling].texture()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, &maps_bmp_[sibling]);
      } else {
        EnsureMapTexture(sibling);
      }
    }
    // Non-visible siblings remain marked as modified for deferred refresh
  }
}

absl::Status OverworldEditor::RefreshMapPalette() {
  RETURN_IF_ERROR(
      overworld_.mutable_overworld_map(current_map_)->LoadPalette());
  const auto current_map_palette = overworld_.current_area_palette();
  palette_ = current_map_palette;
  // Keep tile16 editor in sync with the currently active overworld palette
  tile16_editor_.set_palette(current_map_palette);
  // Ensure source graphics bitmap uses the refreshed palette so tile8 selector isn't blank.
  if (current_gfx_bmp_.is_active()) {
    current_gfx_bmp_.SetPalette(palette_);
    current_gfx_bmp_.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_gfx_bmp_);
  }

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

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

      // Update palette for all siblings - each uses its own loaded palette
      for (int sibling_index : sibling_maps) {
        if (sibling_index < 0 || sibling_index >= zelda3::kNumOverworldMaps) {
          continue;
        }
        auto* sibling_map = overworld_.mutable_overworld_map(sibling_index);
        RETURN_IF_ERROR(sibling_map->LoadPalette());
        maps_bmp_[sibling_index].SetPalette(sibling_map->current_palette());
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
        auto* sibling_map = overworld_.mutable_overworld_map(sibling_index);
        RETURN_IF_ERROR(sibling_map->LoadPalette());

        // SAFETY: Only set palette if bitmap has a valid surface
        // Use sibling map's own loaded palette
        if (maps_bmp_[sibling_index].is_active() &&
            maps_bmp_[sibling_index].surface()) {
          maps_bmp_[sibling_index].SetPalette(sibling_map->current_palette());
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

    // Invalidate Overworld's tileset cache for this map and siblings
    // This ensures stale cached tilesets aren't reused after property changes
    overworld_.InvalidateSiblingMapCaches(map_index);

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
      // Call RefreshChildMapOnDemand() directly instead of
      // RefreshOverworldMapOnDemand()
      RefreshChildMapOnDemand(sibling);

      LOG_DEBUG("OverworldEditor",
                "RefreshSiblingMapGraphics: Refreshed sibling map %d", sibling);
    }
  }
}

void OverworldEditor::RefreshMapProperties() {
  const auto& current_ow_map = *overworld_.mutable_overworld_map(current_map_);

  // Use centralized version detection
  auto rom_version = zelda3::OverworldVersionHelper::GetVersion(*rom_);
  bool use_v3_area_sizes =
      zelda3::OverworldVersionHelper::SupportsAreaEnum(rom_version);

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
  tile16_editor_.set_palette(palette_);
  if (current_gfx_bmp_.is_active()) {
    current_gfx_bmp_.SetPalette(palette_);
    current_gfx_bmp_.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &current_gfx_bmp_);
  }

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

void OverworldEditor::UpdateBlocksetWithPendingTileChanges() {
  // Skip if blockset not loaded or no pending changes
  if (!map_blockset_loaded_) {
    return;
  }

  if (!tile16_editor_.has_pending_changes()) {
    return;
  }

  // Validate the atlas bitmap before modifying
  if (!tile16_blockset_.atlas.is_active() ||
      tile16_blockset_.atlas.vector().empty() ||
      tile16_blockset_.atlas.width() == 0 ||
      tile16_blockset_.atlas.height() == 0) {
    return;
  }

  // Calculate tile positions in the atlas (8 tiles per row, each 16x16)
  constexpr int kTilesPerRow = 8;
  constexpr int kTileSize = 16;
  int atlas_width = tile16_blockset_.atlas.width();
  int atlas_height = tile16_blockset_.atlas.height();

  bool atlas_modified = false;

  // Iterate through all possible tile IDs to check for modifications
  // Note: This is a brute-force approach; a more efficient method would
  // maintain a list of modified tile IDs
  for (int tile_id = 0; tile_id < zelda3::kNumTile16Individual; ++tile_id) {
    if (!tile16_editor_.is_tile_modified(tile_id)) {
      continue;
    }

    // Get the pending bitmap for this tile
    const gfx::Bitmap* pending_bmp =
        tile16_editor_.GetPendingTileBitmap(tile_id);
    if (!pending_bmp || !pending_bmp->is_active() ||
        pending_bmp->vector().empty()) {
      continue;
    }

    // Calculate position in the atlas
    int tile_x = (tile_id % kTilesPerRow) * kTileSize;
    int tile_y = (tile_id / kTilesPerRow) * kTileSize;

    // Validate tile position is within atlas bounds
    if (tile_x + kTileSize > atlas_width || tile_y + kTileSize > atlas_height) {
      continue;
    }

    // Copy pending bitmap data into the atlas at the correct position
    auto& atlas_data = tile16_blockset_.atlas.mutable_data();
    const auto& pending_data = pending_bmp->vector();

    for (int y = 0; y < kTileSize && y < pending_bmp->height(); ++y) {
      for (int x = 0; x < kTileSize && x < pending_bmp->width(); ++x) {
        int atlas_idx = (tile_y + y) * atlas_width + (tile_x + x);
        int pending_idx = y * pending_bmp->width() + x;

        if (atlas_idx >= 0 && atlas_idx < static_cast<int>(atlas_data.size()) &&
            pending_idx >= 0 &&
            pending_idx < static_cast<int>(pending_data.size())) {
          atlas_data[atlas_idx] = pending_data[pending_idx];
          atlas_modified = true;
        }
      }
    }
  }

  // Only queue texture update if we actually modified something
  if (atlas_modified && tile16_blockset_.atlas.texture()) {
    tile16_blockset_.atlas.set_modified(true);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_.atlas);
  }
}

void OverworldEditor::HandleMapInteraction() {
  // Handle middle-click for map interaction instead of right-click
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle) &&
      ImGui::IsItemHovered()) {
    // Get the current map from mouse position (unscale coordinates)
    auto scaled_position = ow_map_canvas_.drawn_tile_position();
    float scale = ow_map_canvas_.global_scale();
    if (scale <= 0.0f)
      scale = 1.0f;
    int map_x = static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
    int map_y = static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;
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

  // Handle double-click to open properties panel (original behavior)
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
  // The fallback code uses ImGui::SetScrollX/Y which scrolls the CURRENT
  // window, and when called from CheckForSelectRectangle() during overworld
  // canvas rendering, it incorrectly scrolls the overworld canvas instead of
  // the tile16 selector.
  //
  // The blockset_selector_ should always be available in modern code paths.
  // If it's not available, we skip scrolling rather than scroll the wrong
  // window.
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

absl::Status OverworldEditor::Clear() {
  // Unregister palette listener
  if (palette_listener_id_ >= 0) {
    gfx::Arena::Get().UnregisterPaletteListener(palette_listener_id_);
    palette_listener_id_ = -1;
  }

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
    // Determine which ASM file to apply and use GetResourcePath for proper
    // resolution
    std::string asm_file_name =
        (target_version == 3) ? "asm/yaze.asm"  // Master file with v3
                              : "asm/ZSCustomOverworld.asm";  // v2 standalone

    // Use GetResourcePath to handle app bundles and various deployment
    // scenarios
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
    RETURN_IF_ERROR(rom_->LoadFromData(working_rom_data));

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
    auto restore_result = rom_->LoadFromData(original_rom_data);
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

void OverworldEditor::ToggleBrushTool() {
  if (current_mode == EditingMode::DRAW_TILE) {
    current_mode = EditingMode::MOUSE;
  } else {
    current_mode = EditingMode::DRAW_TILE;
  }

  if (current_mode == EditingMode::MOUSE) {
    ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
  } else {
    ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
  }
}

void OverworldEditor::ActivateFillTool() {
  if (current_mode == EditingMode::FILL_TILE) {
    current_mode = EditingMode::DRAW_TILE;
  } else {
    current_mode = EditingMode::FILL_TILE;
  }

  // Fill tool is still a tile painting interaction.
  ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
}

void OverworldEditor::CycleTileSelection(int delta) {
  current_tile16_ = std::max(0, current_tile16_ + delta);
}

}  // namespace yaze::editor
