// Related header
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/overworld_map_status.h"
#include "rom/transaction.h"
#include "util/i18n/tr.h"

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
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/canvas/canvas_navigation_manager.h"
#include "app/editor/overworld/entity/entity.h"
#include "app/editor/overworld/entity/entity_operations.h"
#include "app/editor/overworld/entity/overworld_entity_renderer.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "app/editor/overworld/maps/map_texture_coordinator.h"
#include "app/editor/overworld/maps/overworld_map_metadata.h"
#include "app/editor/overworld/overworld_undo_actions.h"
#include "app/editor/overworld/ui/debug/debug_window_card.h"
#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
// Note: All overworld panels now self-register via REGISTER_PANEL macro:
// AreaGraphicsPanel, DebugWindowPanel, GfxGroupsPanel, MapPropertiesPanel,
// OverworldCanvasPanel, ScratchSpacePanel, Tile16EditorPanel, Tile16SelectorPanel,
// Tile8SelectorPanel, UsageStatisticsPanel, V3SettingsPanel, OverworldItemListPanel
#include "app/editor/menu/status_bar.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui/debug/usage_statistics_card.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/session/hack_manifest_save_validation.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/debug/performance/performance_profiler.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/popup_id.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/imgui_memory_editor.h"
#include "app/gui/widgets/empty_state.h"
#include "app/gui/widgets/tile_selector_widget.h"
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

namespace {

bool ItemIdentityMatchesForUndo(const zelda3::OverworldItem& lhs,
                                const zelda3::OverworldItem& rhs) {
  return lhs.id_ == rhs.id_ && lhs.room_map_id_ == rhs.room_map_id_ &&
         lhs.x_ == rhs.x_ && lhs.y_ == rhs.y_ && lhs.bg2_ == rhs.bg2_;
}

bool ItemSnapshotsEqual(const OverworldItemsSnapshot& lhs,
                        const OverworldItemsSnapshot& rhs) {
  if (lhs.items.size() != rhs.items.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.items.size(); ++i) {
    if (!ItemIdentityMatchesForUndo(lhs.items[i], rhs.items[i])) {
      return false;
    }
  }

  if (lhs.selected_item_identity.has_value() !=
      rhs.selected_item_identity.has_value()) {
    return false;
  }
  if (!lhs.selected_item_identity.has_value()) {
    return true;
  }

  return ItemIdentityMatchesForUndo(*lhs.selected_item_identity,
                                    *rhs.selected_item_identity);
}

}  // namespace

OverworldEditor::~OverworldEditor() {
  if (palette_listener_id_ >= 0) {
    gfx::Arena::Get().UnregisterPaletteListener(palette_listener_id_);
    palette_listener_id_ = -1;
  }

  ow_map_canvas_.ClearContextMenuItems();
  current_gfx_canvas_.ClearContextMenuItems();
  blockset_canvas_.ClearContextMenuItems();
  graphics_bin_canvas_.ClearContextMenuItems();
  properties_canvas_.ClearContextMenuItems();
  scratch_canvas_.ClearContextMenuItems();

  // These store callbacks and raw pointers into the editor. Release them while
  // all editor-owned state is still alive rather than during member teardown.
  sidebar_.reset();
  map_properties_system_.reset();
}

bool OverworldEditor::NormalizeMapSelection(int& current_world,
                                            int& current_map) {
  const int clamped_world = std::clamp(current_world, 0, 2);
  int normalized_map = current_map;
  if (normalized_map < 0 || normalized_map >= zelda3::kNumOverworldMaps) {
    normalized_map =
        std::min(clamped_world * 0x40, zelda3::kNumOverworldMaps - 1);
  }

  const int normalized_world = std::clamp(normalized_map / 0x40, 0, 2);
  const bool changed =
      normalized_world != current_world || normalized_map != current_map;
  current_world = normalized_world;
  current_map = normalized_map;
  return changed;
}

void OverworldEditor::Initialize() {
  // Initialize renderer from dependencies
  renderer_ = dependencies_.renderer;

  gfx_group_editor_.SetHostSurfaceHint(
      "Gfx Groups: selection syncs with the Graphics editor for this ROM "
      "session "
      "(this surface uses its own preview canvases).");

  InitMapTextureCoordinator();

  // Initialize MapRefreshCoordinator (must be before callbacks that use it)
  InitMapRefreshCoordinator();

  if (rom_) {
    upgrade_system_ = std::make_unique<zelda3::OverworldUpgradeSystem>(*rom_);
  }
  entity_mutation_service_ =
      std::make_unique<EntityMutationService>(overworld_);

  InitInteractionCoordinator();

  // Note: All WindowContent instances now self-register via REGISTER_PANEL macro
  // and use ContentRegistry::Context to access the current editor.
  // See comment at include section for the current set of view wrappers.

  // Original initialization code below:
  // Initialize MapPropertiesSystem with canvas and bitmap data
  // Initialize cards
  usage_stats_card_ = std::make_unique<UsageStatisticsCard>(&overworld_);
  debug_window_card_ = std::make_unique<DebugWindowCard>();

  map_properties_system_ = std::make_unique<MapPropertiesSystem>(
      &overworld_, rom_, &maps_bmp_, &ow_map_canvas_, &game_state_);

  // Set up refresh callbacks for MapPropertiesSystem
  map_properties_system_->SetRefreshCallbacks(
      [this]() { this->RefreshMapProperties(); },
      [this]() { this->RefreshOverworldMap(); },
      [this]() -> absl::Status { return this->RefreshMapPalette(); },
      [this]() -> absl::Status { return this->RefreshTile16Blockset(); },
      [this](int map_index) { this->ForceRefreshGraphics(map_index); });
  map_properties_system_->SetMapSelectionCallback(
      [this](int map_index, bool respect_pin) {
        this->SelectMapForEditing(map_index, respect_pin);
      });
  map_properties_system_->SetPropertyEditCallback(
      [this](const OverworldPropertyEdit& edit) {
        return this->ApplyOverworldPropertyEdit(edit);
      });
  map_properties_system_->SetPropertyEditBatchCallback(
      [this](const std::vector<OverworldPropertyEdit>& edits,
             const std::string& description) {
        return this->ApplyOverworldPropertyEdits(edits, description);
      });
  map_properties_system_->SetResourceLabelEditCallback(
      [this](const std::string& type, int id, const std::string& label) {
        return this->RenameProjectResourceLabelWithUndo(type, id, label);
      });

  // Initialize OverworldSidebar
  sidebar_ = std::make_unique<OverworldSidebar>(&overworld_, rom_,
                                                map_properties_system_.get());

  // Initialize OverworldEntityRenderer for entity visualization
  entity_renderer_ = std::make_unique<OverworldEntityRenderer>(
      &overworld_, &ow_map_canvas_, &sprite_previews_);

  sidebar_->SetRenameLabelCallback(
      [this](const std::string& type, int id, const std::string& label) {
        return this->RenameProjectResourceLabelWithUndo(type, id, label);
      });

  // Initialize Toolbar: navigation, tools and view only. Per-map data is
  // edited in the Map Properties panel (sidebar_).
  toolbar_ = std::make_unique<OverworldToolbar>();
  toolbar_->on_world_changed = [this](int world) {
    SwitchToWorld(world);
  };
  toolbar_->on_set_mode = [this](EditingMode mode) {
    SetEditingMode(mode);
  };
  toolbar_->on_set_entity_mode = [this](EntityEditMode mode) {
    SetEntityEditMode(mode);
  };
  toolbar_->on_open_map_properties = [this]() {
    OpenMapPropertiesWindow();
  };
  toolbar_->on_toggle_overlay_preview = [this]() {
    show_overlay_preview_ = !show_overlay_preview_;
  };
  toolbar_->is_overlay_preview_enabled = [this]() {
    return show_overlay_preview_;
  };
  toolbar_->on_toggle_grid = [this]() {
    ToggleGrid();
  };
  toolbar_->is_grid_visible = [this]() {
    return grid_visible();
  };
  toolbar_->on_toggle_entities = [this]() {
    ToggleEntityVisibility();
  };
  toolbar_->are_entities_visible = [this]() {
    return entities_visible();
  };
  toolbar_->on_zoom_in = [this]() {
    ZoomIn();
  };
  toolbar_->on_zoom_out = [this]() {
    ZoomOut();
  };
  toolbar_->on_zoom_fit = [this]() {
    ZoomToFit();
  };
  toolbar_->on_center_map = [this]() {
    CenterOverworldView();
  };
  toolbar_->get_zoom = [this]() {
    return ow_map_canvas_.global_scale();
  };
  toolbar_->on_toggle_overlay_preview = [this]() {
    show_overlay_preview_ = !show_overlay_preview_;
  };
  toolbar_->is_overlay_preview_enabled = [this]() {
    return show_overlay_preview_;
  };
  toolbar_->on_upgrade_rom_version = [this](int) {
    ImGui::OpenPopup("UpgradeROMVersion");
  };

  // Initialize OverworldCanvasRenderer for canvas and panel drawing
  canvas_renderer_ = std::make_unique<OverworldCanvasRenderer>(this);
  map_properties_system_->SetOpenMapPropertiesCallback(
      [this]() { OpenMapPropertiesWindow(); });
  map_properties_system_->SetContextNavigationCallbacks(
      [this]() { ResetOverworldView(); }, [this]() { ZoomIn(); },
      [this]() { ZoomOut(); }, [this]() { ZoomToFit(); },
      [this]() { CenterOverworldView(); });
  map_properties_system_->SetContextViewToggles(
      {[this]() { return grid_visible(); }, [this]() { ToggleGrid(); }},
      {[this]() { return entities_visible(); },
       [this]() { ToggleEntityVisibility(); }},
      {[this]() { return show_overlay_preview_; },
       [this]() { show_overlay_preview_ = !show_overlay_preview_; }});
  map_properties_system_->SetCurrentMapProvider(
      [this]() { return current_map_; });
  map_properties_system_->SetMapJumpCallback(
      [this](int map_id) { JumpToMap(map_id); });
  map_properties_system_->SetShortcutHintProvider(
      [this](const char* name) -> std::string {
        return dependencies_.shortcut_manager
                   ? dependencies_.shortcut_manager->GetDisplayString(name)
                   : std::string();
      });

  InitCanvasNavigationManager();
  InitTilePaintingManager();
  SetupCanvasAutomation();
}

void OverworldEditor::InitInteractionCoordinator() {
  OverworldCommandSink sink;
  sink.on_set_editor_mode = [this](EditingMode mode) {
    SetEditingMode(mode);
  };
  sink.on_set_entity_mode = [this](EntityEditMode mode) {
    SetEntityEditMode(mode);
  };
  sink.on_pick_tile_from_hover = [this]() {
    (void)PickTile16FromHoveredCanvas();
  };
  sink.on_duplicate_selected = [this]() {
    (void)DuplicateSelectedItem();
  };
  sink.on_nudge_selected = [this](int delta_x, int delta_y, bool shift_held) {
    const int step = shift_held ? 16 : 1;
    (void)this->NudgeSelectedItem(delta_x * step, delta_y * step);
  };
  sink.on_entity_context_menu = [this](zelda3::GameEntity* entity) {
    if (dependencies_.window_manager) {
      auto* workbench = static_cast<OverworldEntityWorkbench*>(
          dependencies_.window_manager->GetWindowContent(
              "overworld.entity_workbench"));
      if (workbench) {
        workbench->OpenContextMenuFor(entity);
      }
    }
  };
  sink.on_entity_double_click = [this](zelda3::GameEntity* entity) {
    if (!entity) {
      return;
    }
    switch (entity->entity_type_) {
      case zelda3::GameEntity::EntityType::kExit:
        this->jump_to_tab_ =
            static_cast<zelda3::OverworldExit*>(entity)->room_id_;
        break;
      case zelda3::GameEntity::EntityType::kEntrance:
        this->jump_to_tab_ =
            static_cast<zelda3::OverworldEntrance*>(entity)->entrance_id_;
        break;
      default:
        break;
    }
  };
  sink.can_edit_items = [this]() {
    // Items mode, or no entity focus with an item selected, in select mode.
    if (current_mode != EditingMode::MOUSE) {
      return false;
    }
    if (entity_edit_mode_ == EntityEditMode::ITEMS) {
      return true;
    }
    return entity_edit_mode_ == EntityEditMode::NONE &&
           GetSelectedItem() != nullptr;
  };
  sink.on_undo = [this]() {
    status_ = Undo();
  };
  sink.on_redo = [this]() {
    status_ = Redo();
  };
  interaction_coordinator_ =
      std::make_unique<OverworldInteractionCoordinator>(std::move(sink));
}

void OverworldEditor::InitTilePaintingManager() {
  TilePaintingDependencies deps;
  deps.ow_map_canvas = &ow_map_canvas_;
  deps.overworld = &overworld_;
  deps.maps_bmp = &maps_bmp_;
  deps.tile16_blockset = &tile16_blockset_;
  deps.current_tile16 = &current_tile16_;
  deps.selected_tile16_ids = &selected_tile16_ids_;
  deps.current_map = &current_map_;
  deps.current_world = &current_world_;
  deps.current_mode = &current_mode;
  deps.rom = rom_;
  deps.tile16_editor = &tile16_editor_;

  TilePaintingCallbacks callbacks;
  callbacks.create_undo_point = [this](int map_id, int world, int x, int y,
                                       int old_tile_id) {
    this->CreateUndoPoint(map_id, world, x, y, old_tile_id);
  };
  callbacks.finalize_paint_operation = [this]() {
    this->FinalizePaintOperation();
  };
  callbacks.refresh_overworld_map_on_demand = [this](int map_index) {
    this->RefreshOverworldMapOnDemand(map_index);
  };
  callbacks.scroll_blockset_to_current_tile = [this]() {
    this->ScrollBlocksetCanvasToCurrentTile();
  };
  callbacks.request_tile16_selection = [this](int tile_id) {
    this->RequestTile16Selection(tile_id);
  };

  tile_painting_ = std::make_unique<TilePaintingManager>(deps, callbacks);
}

void OverworldEditor::InitCanvasNavigationManager() {
  CanvasNavigationContext ctx;
  ctx.ow_map_canvas = &ow_map_canvas_;
  ctx.overworld = &overworld_;
  ctx.rom = rom_;
  ctx.current_map = &current_map_;
  ctx.current_world = &current_world_;
  ctx.current_parent = &current_parent_;
  ctx.current_tile16 = &current_tile16_;
  ctx.hovered_map = &hovered_map_;
  ctx.current_mode = &current_mode;
  ctx.current_map_lock = &current_map_lock_;
  ctx.is_dragging_entity = &is_dragging_entity_;
  ctx.maps_bmp = &maps_bmp_;
  ctx.tile16_blockset = &tile16_blockset_;
  ctx.blockset_selector = &blockset_selector_;

  CanvasNavigationCallbacks callbacks;
  callbacks.refresh_overworld_map = [this]() {
    this->RefreshOverworldMap();
  };
  callbacks.refresh_tile16_blockset = [this]() -> absl::Status {
    return this->RefreshTile16Blockset();
  };
  callbacks.ensure_map_texture = [this](int map_index) {
    this->EnsureMapTexture(map_index);
  };
  callbacks.select_map_for_editing = [this](int map_index, bool respect_pin) {
    this->SelectMapForEditing(map_index, respect_pin);
  };
  callbacks.pick_tile16_from_hovered_canvas = [this]() -> bool {
    return this->PickTile16FromHoveredCanvas();
  };
  callbacks.is_entity_hovered = [this]() -> bool {
    return entity_renderer_ && entity_renderer_->hovered_entity() != nullptr;
  };
  callbacks.open_map_properties = [this]() {
    OpenMapPropertiesWindow();
  };

  canvas_nav_ = std::make_unique<CanvasNavigationManager>();
  canvas_nav_->Initialize(ctx, callbacks);
}

void OverworldEditor::SetCurrentEntity(zelda3::GameEntity* entity) {
  current_entity_ = entity;
  if (!entity) {
    selected_item_identity_.reset();
    return;
  }
  if (entity->entity_type_ == zelda3::GameEntity::EntityType::kItem) {
    selected_item_identity_ = *static_cast<zelda3::OverworldItem*>(entity);
  } else {
    selected_item_identity_.reset();
  }
}

void OverworldEditor::NotifyEntityModified(zelda3::GameEntity*) {
  if (rom_) {
    rom_->set_dirty(true);
  }
}

absl::Status OverworldEditor::Load() {
  pending_entity_insertion_.reset();
  ow_map_canvas_.ClearContextMenuItems();
  gfx::ScopedTimer timer("OverworldEditor::Load");

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (dependencies_.window_manager) {
    dependencies_.window_manager->RegisterPanelAlias("overworld.debug_window",
                                                     "overworld.debug");
  }

  // Clear undo/redo state when loading new ROM data
  current_paint_operation_.reset();
  undo_manager_.Clear();

  RETURN_IF_ERROR(LoadGraphics());
  tile16_editor_.BindDocument(overworld_.mutable_tiles16(), &undo_manager_,
                              [this]() { FinalizePaintOperation(); });
  RETURN_IF_ERROR(
      tile16_editor_.Initialize(tile16_blockset_bmp_, current_gfx_bmp_,
                                *overworld_.mutable_all_tiles_types()));

  // CRITICAL FIX: Initialize tile16 editor with the correct overworld palette
  tile16_editor_.set_palette(palette_);
  tile16_editor_.SetRom(rom_);
  tile16_editor_.set_on_current_tile_changed(
      [this](int id) { current_tile16_ = id; });

  tile16_editor_.set_on_document_changed(
      [this](const std::vector<Tile16Commit>&) {
        if (map_refresh_) {
          map_refresh_->InvalidateTile16Definitions();
          RefreshOverworldMap();
          status_ = RefreshTile16Blockset();
        }
      });

  // Set up entity insertion callback for MapPropertiesSystem
  if (map_properties_system_) {
    map_properties_system_->SetEntityCallbacks(
        [this](const std::string& entity_type,
               const OverworldContextTarget& target) {
          HandleEntityInsertion(entity_type, target);
        });

    // Set up tile16 edit callback for context menu in MOUSE mode
    map_properties_system_->SetTile16SampleCallback(
        [this](const OverworldContextTarget& target) {
          return SampleContextTile16(target);
        });
    map_properties_system_->SetTile16EditCallback(
        [this](const OverworldContextTarget& target) {
          HandleTile16Edit(target);
        });
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

  auto scratch_status = LoadScratchPad();
  if (!scratch_status.ok()) {
    LOG_WARN("OverworldEditor", "Failed to load scratch pad: %s",
             std::string(scratch_status.message()).c_str());
  }

  if (pending_tile16_selection_after_gfx_) {
    const int pending = *pending_tile16_selection_after_gfx_;
    pending_tile16_selection_after_gfx_.reset();
    RequestTile16Selection(pending);
  }

  return absl::OkStatus();
}

void OverworldEditor::RefreshMapsForSheetEdits() {
  for (int map = 0; map < zelda3::kNumOverworldMaps; ++map) {
    auto* overworld_map = overworld_.mutable_overworld_map(map);
    // A map already marked for refresh waits for its rebuild, which records
    // the new sheet revisions; marking it again every frame would keep
    // clearing the blockset cache.
    if (overworld_map != nullptr && !maps_bmp_[map].modified() &&
        overworld_map->SourceSheetsChanged()) {
      ForceRefreshGraphics(map);
    }
  }
}

absl::Status OverworldEditor::Update() {
  status_ = absl::OkStatus();

  // Safety check: Ensure ROM is loaded and graphics are ready
  if (!rom_ || !rom_->is_loaded()) {
    gui::DrawEmptyState(gui::EmptyNoRom());
    return absl::OkStatus();
  }

  if (!all_gfx_loaded_) {
    gui::DrawEmptyState(gui::EmptyLoading("Loading overworld graphics…"));
    return absl::OkStatus();
  }

  NormalizeCurrentSelectionState();

  // Process deferred textures for smooth loading
  ProcessDeferredTextures();

  // Show unsaved Graphics editor edits in the maps that use those sheets.
  RefreshMapsForSheetEdits();

  // Early return if window_manager is not available
  // (panels won't be drawn without it, so no point continuing)
  if (!dependencies_.window_manager) {
    return status_;
  }

  if (overworld_canvas_fullscreen_) {
    return status_;
  }

  // ===========================================================================
  // Main Overworld Canvas
  // ===========================================================================
  // The panels (Tile16 Selector, Area Graphics, etc.) are now managed by
  // WindowContent/WorkspaceWindowManager and drawn automatically. This section only
  // handles the main canvas and toolbar.

  // ===========================================================================
  // Non-Panel Windows (not managed by WindowContent system)
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

  // Note: Tile16 Editor is now managed as an WindowContent (Tile16EditorPanel)
  // It uses UpdateAsPanel() which provides a context menu instead of MenuBar

  if (auto* workbench = GetWorkbench()) {
    workbench->ProcessPendingInsertion(entity_mutation_service_.get());
    workbench->DrawPopups();
  }

  if (ImGui::BeginPopupModal("UpgradeROMVersion", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(ICON_MD_UPGRADE " Upgrade ROM to ZSCustomOverworld");
    ImGui::Separator();
    ImGui::TextWrapped(
        tr("This will apply the ZSCustomOverworld ASM patch to your ROM,\n"
           "enabling advanced overworld features."));
    ImGui::Separator();

    const uint8_t current_version =
        rom_ != nullptr ? (*rom_)[zelda3::OverworldCustomASMHasBeenApplied]
                        : 0xFF;
    ImGui::Text(tr("Current Version: %s"),
                current_version == 0xFF
                    ? "Vanilla"
                    : absl::StrFormat("v%d", current_version).c_str());

    static int target_version = 3;
    ImGui::RadioButton(tr("v2 (Basic features)"), &target_version, 2);
    ImGui::SameLine();
    ImGui::RadioButton(tr("v3 (All features)"), &target_version, 3);

    ImGui::Separator();

    if (ImGui::Button(ICON_MD_CHECK " Apply Upgrade", ImVec2(150.0f, 0.0f))) {
      auto upgrade_status =
          upgrade_system_ != nullptr
              ? upgrade_system_->ApplyZSCustomOverworldASM(target_version)
              : absl::FailedPreconditionError("Upgrade system not initialized");
      if (upgrade_status.ok()) {
        status_ = Clear();
        if (status_.ok()) {
          status_ = Load();
        }
        if (status_.ok()) {
          ImGui::CloseCurrentPopup();
        }
      } else {
        LOG_ERROR("OverworldEditor", "Upgrade failed: %s",
                  upgrade_status.message().data());
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel", ImVec2(150.0f, 0.0f))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  // ===========================================================================
  // Centralized Entity Interaction Logic (extracted to dedicated method)
  // ===========================================================================
  zelda3::GameEntity* hovered_entity =
      entity_renderer_ ? entity_renderer_->hovered_entity() : nullptr;
  if (interaction_coordinator_) {
    interaction_coordinator_->Update(hovered_entity);
  }

  return absl::OkStatus();
}

void OverworldEditor::DrawOverworldCanvas() {
  NormalizeCurrentSelectionState();

  if (canvas_renderer_) {
    canvas_renderer_->DrawOverworldCanvas();
  }
}

bool OverworldEditor::NormalizeCurrentSelectionState() {
  const bool changed = NormalizeMapSelection(current_world_, current_map_);

  overworld_.set_current_world(current_world_);
  overworld_.set_current_map(current_map_);
  if (const auto* map = overworld_.overworld_map(current_map_)) {
    current_parent_ = map->parent();
  } else {
    current_parent_ = current_map_;
  }

  if (changed) {
    LOG_WARN("OverworldEditor",
             "Normalized stale overworld selection to world=%d map=%d",
             current_world_, current_map_);
  }
  return changed;
}

void OverworldEditor::SelectMapForEditing(int map_id, bool respect_pin) {
  if (respect_pin && current_map_lock_) {
    return;
  }
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return;
  }
  const auto* map = overworld_.overworld_map(map_id);
  if (!map) {
    return;
  }

  // Cursor tracking may cross screens during one paint stroke. Preserve its
  // undo batch within the same world; explicit selection still finalizes it.
  const bool tracking_paint = respect_pin &&
                              (current_mode == EditingMode::DRAW_TILE ||
                               current_mode == EditingMode::FILL_TILE) &&
                              map_id / 0x40 == current_world_;
  if (!tracking_paint) {
    FinalizePaintOperation();
  }
  current_map_ = map_id;
  current_world_ = std::clamp(map_id / 0x40, 0, 2);
  current_parent_ = map->parent();
  overworld_.set_current_map(current_map_);
  overworld_.set_current_world(current_world_);

  status_ = overworld_.EnsureMapBuilt(current_map_);
  if (!status_.ok()) {
    PRINT_IF_ERROR(status_);
    return;
  }

  EnsureMapTexture(current_map_);
  if (all_gfx_loaded_) {
    PRINT_IF_ERROR(RefreshTile16Blockset());
    RefreshMapProperties();
  }
}

void OverworldEditor::PrimeWorldMaps(int world, bool process_texture_queue) {
  if (map_texture_)
    map_texture_->PrimeWorldMaps(world, process_texture_queue);
}

void OverworldEditor::SwitchToWorld(int world) {
  FinalizePaintOperation();

  const int clamped_world = std::clamp(world, 0, 2);
  const int local_map = current_map_ & 0x3F;
  const int world_start = clamped_world * 0x40;
  const int maps_remaining = zelda3::kNumOverworldMaps - world_start;
  if (maps_remaining <= 0) {
    return;
  }

  hovered_map_ = -1;
  SelectMapForEditing(world_start + std::min(local_map, maps_remaining - 1),
                      false);
  NormalizeCurrentSelectionState();

  status_ = overworld_.EnsureMapBuilt(current_map_);
  if (!status_.ok()) {
    PRINT_IF_ERROR(status_);
    return;
  }

  EnsureMapTexture(current_map_);
  ForceRefreshGraphics(current_map_);
  RefreshOverworldMapOnDemand(current_map_);
  PRINT_IF_ERROR(RefreshTile16Blockset());
  PrimeWorldMaps(current_world_);
}

bool OverworldEditor::SelectItemByIdentity(
    const zelda3::OverworldItem& item_identity) {
  auto* item = FindItemByIdentity(&overworld_, item_identity);
  if (!item) {
    return false;
  }

  selected_item_identity_ = *item;
  current_entity_ = item;
  if (dependencies_.window_manager) {
    auto* workbench = static_cast<OverworldEntityWorkbench*>(
        dependencies_.window_manager->GetWindowContent(
            "overworld.entity_workbench"));
    if (workbench) {
      workbench->SetActiveEntity(item);
    }
  }
  return true;
}

void OverworldEditor::ClearSelectedItem() {
  selected_item_identity_.reset();
  current_entity_ = nullptr;
  if (dependencies_.window_manager) {
    auto* workbench = static_cast<OverworldEntityWorkbench*>(
        dependencies_.window_manager->GetWindowContent(
            "overworld.entity_workbench"));
    if (workbench) {
      workbench->SetActiveEntity(nullptr);
    }
  }
}

OverworldEntityWorkbench* OverworldEditor::GetWorkbench() {
  if (!dependencies_.window_manager)
    return nullptr;
  return static_cast<OverworldEntityWorkbench*>(
      dependencies_.window_manager->GetWindowContent(
          "overworld.entity_workbench"));
}

zelda3::OverworldItem* OverworldEditor::GetSelectedItem() {
  if (!selected_item_identity_.has_value()) {
    return nullptr;
  }

  auto* item = FindItemByIdentity(&overworld_, *selected_item_identity_);
  if (!item) {
    ClearSelectedItem();
    return nullptr;
  }

  current_entity_ = item;
  if (auto* workbench = GetWorkbench()) {
    workbench->SetActiveEntity(item);
  }
  return item;
}

const zelda3::OverworldItem* OverworldEditor::GetSelectedItem() const {
  return const_cast<OverworldEditor*>(this)->GetSelectedItem();
}

OverworldItemsSnapshot OverworldEditor::CaptureItemUndoSnapshot() const {
  OverworldItemsSnapshot snapshot;
  snapshot.items = overworld_.all_items();
  snapshot.selected_item_identity = selected_item_identity_;
  return snapshot;
}

void OverworldEditor::RestoreItemUndoSnapshot(
    const OverworldItemsSnapshot& snapshot) {
  auto* items = overworld_.mutable_all_items();
  if (!items) {
    return;
  }

  *items = snapshot.items;
  selected_item_identity_ = snapshot.selected_item_identity;
  if (selected_item_identity_.has_value()) {
    auto* selected_item =
        FindItemByIdentity(&overworld_, *selected_item_identity_);
    if (selected_item) {
      current_entity_ = selected_item;
      if (auto* workbench = GetWorkbench()) {
        workbench->SetActiveEntity(selected_item);
      }
    } else {
      ClearSelectedItem();
    }
  } else {
    ClearSelectedItem();
  }

  if (rom_) {
    rom_->set_dirty(true);
  }
  RefreshOverworldMap();
}

void OverworldEditor::PushItemUndoAction(OverworldItemsSnapshot before,
                                         std::string description) {
  OverworldItemsSnapshot after = CaptureItemUndoSnapshot();
  if (ItemSnapshotsEqual(before, after)) {
    return;
  }

  undo_manager_.Push(std::make_unique<OverworldItemsEditAction>(
      std::move(before), std::move(after),
      [this](const OverworldItemsSnapshot& snapshot) {
        RestoreItemUndoSnapshot(snapshot);
      },
      std::move(description)));
}

bool OverworldEditor::DuplicateSelectedItem(int offset_x, int offset_y) {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          "Select an overworld item first (click one, or Items mode: 5)",
          ToastType::kInfo);
    }
    return false;
  }

  auto before_snapshot = CaptureItemUndoSnapshot();
  EntityMutationService::MutationResult duplicate_result;
  if (entity_mutation_service_) {
    duplicate_result = entity_mutation_service_->DuplicateItem(
        *selected_item, offset_x, offset_y);
  } else {
    auto duplicate_or = DuplicateItemByIdentity(&overworld_, *selected_item,
                                                offset_x, offset_y);
    if (duplicate_or.ok()) {
      duplicate_result.entity = *duplicate_or;
      duplicate_result.status = absl::OkStatus();
    } else {
      duplicate_result.status = duplicate_or.status();
      duplicate_result.error_message =
          "Failed to duplicate overworld item: " +
          std::string(duplicate_or.status().message());
    }
  }
  if (!duplicate_result.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          duplicate_result.error_message.empty()
              ? "Failed to duplicate overworld item"
              : duplicate_result.error_message,
          ToastType::kError);
    }
    return false;
  }

  auto* duplicated_item =
      static_cast<zelda3::OverworldItem*>(duplicate_result.entity);
  selected_item_identity_ = *duplicated_item;
  current_entity_ = duplicated_item;
  if (auto* workbench = GetWorkbench()) {
    workbench->SetActiveEntity(duplicated_item);
  }
  PushItemUndoAction(std::move(before_snapshot),
                     absl::StrFormat("Duplicate overworld item 0x%02X",
                                     static_cast<int>(duplicated_item->id_)));
  rom_->set_dirty(true);
  if (dependencies_.toast_manager) {
    dependencies_.toast_manager->Show(
        absl::StrFormat("Duplicated item 0x%02X",
                        static_cast<int>(duplicated_item->id_)),
        ToastType::kSuccess);
  }
  return true;
}

bool OverworldEditor::NudgeSelectedItem(int delta_x, int delta_y) {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    return false;
  }

  auto before_snapshot = CaptureItemUndoSnapshot();
  auto status = NudgeItem(selected_item, delta_x, delta_y);
  if (!status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Failed to move selected item",
                                        ToastType::kError);
    }
    return false;
  }

  selected_item_identity_ = *selected_item;
  current_entity_ = selected_item;
  if (auto* workbench = GetWorkbench()) {
    workbench->SetActiveEntity(selected_item);
  }
  PushItemUndoAction(
      std::move(before_snapshot),
      absl::StrFormat("Move overworld item 0x%02X (%+d,%+d)",
                      static_cast<int>(selected_item->id_), delta_x, delta_y));
  rom_->set_dirty(true);
  return true;
}

bool OverworldEditor::DeleteSelectedItem() {
  auto* selected_item = GetSelectedItem();
  if (!selected_item) {
    return false;
  }

  auto before_snapshot = CaptureItemUndoSnapshot();
  const zelda3::OverworldItem selected_identity = *selected_item;
  const uint8_t deleted_item_id = selected_identity.id_;
  absl::Status remove_status =
      absl::FailedPreconditionError("Entity mutation service not initialized");
  if (entity_mutation_service_) {
    remove_status =
        entity_mutation_service_->DeleteItem(selected_identity).status;
  } else {
    remove_status = RemoveItemByIdentity(&overworld_, selected_identity);
  }
  if (!remove_status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Failed to delete selected item",
                                        ToastType::kError);
    }
    return false;
  }

  auto* nearest_item =
      entity_mutation_service_
          ? entity_mutation_service_->ResolveNextSelection(selected_identity)
          : FindNearestItemForSelection(&overworld_, selected_identity);
  if (nearest_item) {
    selected_item_identity_ = *nearest_item;
    current_entity_ = nearest_item;
    if (auto* workbench = GetWorkbench()) {
      workbench->SetActiveEntity(nearest_item);
    }
  } else {
    ClearSelectedItem();
  }

  PushItemUndoAction(std::move(before_snapshot),
                     absl::StrFormat("Delete overworld item 0x%02X",
                                     static_cast<int>(deleted_item_id)));
  rom_->set_dirty(true);
  if (dependencies_.toast_manager) {
    if (nearest_item) {
      dependencies_.toast_manager->Show(
          absl::StrFormat("Deleted item 0x%02X (selected nearest 0x%02X)",
                          static_cast<int>(deleted_item_id),
                          static_cast<int>(nearest_item->id_)),
          ToastType::kSuccess);
    } else {
      dependencies_.toast_manager->Show(
          absl::StrFormat("Deleted overworld item 0x%02X",
                          static_cast<int>(deleted_item_id)),
          ToastType::kSuccess);
    }
  }
  return true;
}

void OverworldEditor::CheckForOverworldEdits() {
  tile_painting_->CheckForOverworldEdits();
}

absl::Status OverworldEditor::Copy() {
  if (!dependencies_.shared_clipboard) {
    return absl::FailedPreconditionError("Clipboard unavailable");
  }
  if (tile_painting_) {
    if (const auto* brush = tile_painting_->selection_brush()) {
      dependencies_.shared_clipboard->overworld_tile16_ids = brush->tile_ids;
      dependencies_.shared_clipboard->overworld_width = brush->width;
      dependencies_.shared_clipboard->overworld_height = brush->height;
      dependencies_.shared_clipboard->has_overworld_tile16 = true;
      return absl::OkStatus();
    }
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
  if (!tile_painting_) {
    return absl::FailedPreconditionError("Tile painting is unavailable");
  }
  const auto& clipboard = *dependencies_.shared_clipboard;
  return tile_painting_->PasteBrush({clipboard.overworld_width,
                                     clipboard.overworld_height,
                                     clipboard.overworld_tile16_ids});
}

absl::Status OverworldEditor::CheckForCurrentMap() {
  if (canvas_nav_)
    return canvas_nav_->CheckForCurrentMap();
  return absl::OkStatus();
}

absl::Status OverworldEditor::UpdateGfxGroupEditor() {
  // Delegate to the existing GfxGroupEditor
  if (rom_ && rom_->is_loaded()) {
    return gfx_group_editor_.Update();
  } else {
    gui::DrawEmptyState(gui::EmptyNoRom(/*compact=*/true));
    return absl::OkStatus();
  }
}

// DrawV3Settings - now in OverworldCanvasRenderer

// DrawMapProperties - now in OverworldCanvasRenderer

absl::Status OverworldEditor::Save() {
  // Validate sprite capacity before any editor serializer changes ROM bytes.
  ASSIGN_OR_RETURN(auto sprite_plan, overworld_.PrepareSpriteSave());
  if (!sprite_plan.empty() && dependencies_.project &&
      dependencies_.project->hack_manifest.loaded()) {
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    for (const auto& write : sprite_plan) {
      ranges.emplace_back(write.address, write.address + write.bytes.size());
    }
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        dependencies_.project->hack_manifest,
        dependencies_.project->rom_metadata.write_policy, ranges,
        "overworld sprites", "OverworldEditor", dependencies_.toast_manager));
  }

  ScopedRomTransaction save_transaction(*rom_);
  // HACK MANIFEST VALIDATION
  const bool saving_maps =
      core::FeatureFlags::get().overworld.kSaveOverworldMaps;
  if (saving_maps && dependencies_.project &&
      dependencies_.project->hack_manifest.loaded()) {
    const auto& manifest = dependencies_.project->hack_manifest;
    const auto write_policy = dependencies_.project->rom_metadata.write_policy;

    // Calculate memory ranges that would be written by overworld map saves.
    // `ranges` are PC offsets (ROM file offsets). The hack manifest is in SNES
    // address space (LoROM), so convert before analysis.
    auto ranges = overworld_.GetProjectedWriteRanges();
    RETURN_IF_ERROR(ValidateHackManifestSaveConflicts(
        manifest, write_policy, ranges, "overworld maps", "OverworldEditor",
        dependencies_.toast_manager));
  }

  if (saving_maps) {
    const auto profile = zelda3::DetectOverworldRomProfile(*rom_);
    RETURN_IF_ERROR(overworld_.CreateTile32Tilemap());
    if (profile.has_expanded_tile32) {
      RETURN_IF_ERROR(overworld_.SaveMap32Expanded());
    } else {
      RETURN_IF_ERROR(overworld_.SaveMap32Tiles());
    }
    if (profile.has_expanded_tile16) {
      RETURN_IF_ERROR(overworld_.SaveMap16Expanded());
    } else {
      RETURN_IF_ERROR(overworld_.SaveMap16Tiles());
    }
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
    RETURN_IF_ERROR(overworld_.SaveCustomOverworldData());
  }
  RETURN_IF_ERROR(zelda3::ApplyOverworldSpriteSave(*rom_, sprite_plan));
  save_transaction.Commit();
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

  // A stroke can cross screen boundaries. Keep one batch within the same world
  // until release; the undo action refreshes every screen touched by its tiles.
  if (current_paint_operation_.has_value() &&
      current_paint_operation_->world == world) {
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
}

void OverworldEditor::FinalizePaintOperation() {
  if (!current_paint_operation_.has_value()) {
    return;
  }

  // Push to the UndoManager (new framework path).
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
      std::move(changes), &overworld_, std::function<void()>{},
      [this](int map_id) {
        maps_bmp_[map_id].set_modified(true);
        RefreshOverworldMapOnDemand(map_id);
      },
      /*allow_merge=*/false);
  undo_manager_.Push(std::move(action));

  current_paint_operation_.reset();
}

absl::Status OverworldEditor::ApplyOverworldPropertyEdit(
    const OverworldPropertyEdit& edit, bool record_undo) {
  if (!map_properties_system_) {
    return absl::FailedPreconditionError("MapPropertiesSystem not initialized");
  }

  if (!record_undo) {
    return map_properties_system_->ApplyPropertyEditDirect(edit);
  }

  FinalizePaintOperation();
  ASSIGN_OR_RETURN(const int before_value,
                   map_properties_system_->ReadPropertyValue(edit));

  OverworldPropertyEdit before = edit;
  before.value = before_value;
  RETURN_IF_ERROR(map_properties_system_->ApplyPropertyEditDirect(edit));

  ASSIGN_OR_RETURN(const int after_value,
                   map_properties_system_->ReadPropertyValue(edit));
  if (after_value == before_value) {
    return absl::OkStatus();
  }

  OverworldPropertyEdit after = edit;
  after.value = after_value;
  undo_manager_.Push(std::make_unique<OverworldMapPropertyEditAction>(
      before, after,
      [this](const OverworldPropertyEdit& replay) {
        return this->ApplyOverworldPropertyEdit(replay, false);
      },
      DescribeOverworldPropertyEdit(after)));
  return absl::OkStatus();
}

absl::Status OverworldEditor::ApplyOverworldPropertyEdits(
    const std::vector<OverworldPropertyEdit>& edits,
    const std::string& description, bool record_undo) {
  if (!map_properties_system_) {
    return absl::FailedPreconditionError("MapPropertiesSystem not initialized");
  }
  if (edits.empty()) {
    return absl::OkStatus();
  }

  if (!record_undo) {
    for (const auto& edit : edits) {
      RETURN_IF_ERROR(map_properties_system_->ApplyPropertyEditDirect(edit));
    }
    return absl::OkStatus();
  }

  FinalizePaintOperation();

  std::vector<OverworldPropertyEdit> before_edits;
  std::vector<OverworldPropertyEdit> after_edits;
  before_edits.reserve(edits.size());
  after_edits.reserve(edits.size());

  for (const auto& edit : edits) {
    if (!map_properties_system_->CheckPropertyEditSupported(edit).ok()) {
      continue;
    }

    auto before_value = map_properties_system_->ReadPropertyValue(edit);
    if (!before_value.ok()) {
      for (auto it = before_edits.rbegin(); it != before_edits.rend(); ++it) {
        (void)map_properties_system_->ApplyPropertyEditDirect(*it);
      }
      return before_value.status();
    }

    OverworldPropertyEdit before = edit;
    before.value = *before_value;
    const absl::Status apply_status =
        map_properties_system_->ApplyPropertyEditDirect(edit);
    if (!apply_status.ok()) {
      for (auto it = before_edits.rbegin(); it != before_edits.rend(); ++it) {
        (void)map_properties_system_->ApplyPropertyEditDirect(*it);
      }
      return apply_status;
    }

    auto after_value = map_properties_system_->ReadPropertyValue(edit);
    if (!after_value.ok()) {
      (void)map_properties_system_->ApplyPropertyEditDirect(before);
      for (auto it = before_edits.rbegin(); it != before_edits.rend(); ++it) {
        (void)map_properties_system_->ApplyPropertyEditDirect(*it);
      }
      return after_value.status();
    }
    if (*after_value == *before_value) {
      continue;
    }

    OverworldPropertyEdit after = edit;
    after.value = *after_value;
    before_edits.push_back(std::move(before));
    after_edits.push_back(std::move(after));
  }

  if (after_edits.empty()) {
    return absl::OkStatus();
  }

  const std::string action_description =
      description.empty()
          ? absl::StrFormat("Edit %d overworld map properties",
                            static_cast<int>(after_edits.size()))
          : description;
  undo_manager_.Push(std::make_unique<OverworldMapPropertyBatchEditAction>(
      before_edits, after_edits,
      [this](const OverworldPropertyEdit& replay) {
        return this->ApplyOverworldPropertyEdit(replay, false);
      },
      action_description));
  return absl::OkStatus();
}

absl::Status OverworldEditor::RenameProjectResourceLabelWithUndo(
    const std::string& type, int id, const std::string& label) {
  if (!dependencies_.project) {
    return absl::FailedPreconditionError("No project is open");
  }

  FinalizePaintOperation();
  const std::string before =
      GetProjectResourceLabel(dependencies_.project, type, id);
  RETURN_IF_ERROR(
      RenameProjectResourceLabel(dependencies_.project, type, id, label));
  const std::string after =
      GetProjectResourceLabel(dependencies_.project, type, id);
  if (after == before) {
    return absl::OkStatus();
  }

  undo_manager_.Push(std::make_unique<OverworldProjectLabelEditAction>(
      before, after,
      [this, type, id](const std::string& value) {
        return RenameProjectResourceLabel(dependencies_.project, type, id,
                                          value);
      },
      absl::StrFormat("Rename %s 0x%02X", type, id)));
  return absl::OkStatus();
}

absl::Status OverworldEditor::Undo() {
  // Finalize any pending paint operation first
  FinalizePaintOperation();
  return undo_manager_.Undo();
}

absl::Status OverworldEditor::Redo() {
  FinalizePaintOperation();
  return undo_manager_.Redo();
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
    current_gfx_bmp_.Create(0x80, kOverworldMapSize, 0x08,
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
  const auto& tile16_blockset_data = overworld_.tile16_blockset_data();
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

  // Phase 2: reset map bitmaps and leave them fully on-demand.
  // The canvas/navigation path will materialize only the visible/current maps.
  if (map_texture_) {
    map_texture_->ResetMapBitmaps();
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
  if (map_texture_)
    map_texture_->ProcessDeferredTextures();
}

const gfx::Bitmap* OverworldEditor::AreaScreenBitmap(int map_id) {
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return nullptr;
  }
  EnsureMapTexture(map_id);
  const gfx::Bitmap& bitmap = maps_bmp_[map_id];
  return bitmap.is_active() && bitmap.texture() ? &bitmap : nullptr;
}

void OverworldEditor::EnsureMapTexture(int map_index) {
  if (map_texture_)
    map_texture_->EnsureMapTexture(map_index);
}

void OverworldEditor::InitMapTextureCoordinator() {
  MapTextureContext ctx;
  ctx.overworld = &overworld_;
  ctx.maps_bmp = &maps_bmp_;
  ctx.renderer = renderer_;
  ctx.current_world = &current_world_;
  ctx.current_map = &current_map_;
  ctx.refresh_map_on_demand = [this](int map_index) {
    this->RefreshOverworldMapOnDemand(map_index);
  };
  map_texture_ = std::make_unique<OverworldMapTextureCoordinator>(ctx);
}

void OverworldEditor::InitMapRefreshCoordinator() {
  MapRefreshContext ctx;
  ctx.rom = rom_;
  ctx.overworld = &overworld_;
  ctx.maps_bmp = &maps_bmp_;
  ctx.tile16_blockset = &tile16_blockset_;
  ctx.current_gfx_bmp = &current_gfx_bmp_;
  ctx.current_graphics_set = &current_graphics_set_;
  ctx.palette = &palette_;
  ctx.renderer = renderer_;
  ctx.tile16_editor = &tile16_editor_;
  ctx.current_world = &current_world_;
  ctx.current_map = &current_map_;
  ctx.current_blockset = &current_blockset_;
  ctx.game_state = &game_state_;
  ctx.map_blockset_loaded = &map_blockset_loaded_;
  ctx.status = &status_;
  ctx.ensure_map_texture = [this](int map_index) {
    this->EnsureMapTexture(map_index);
  };
  map_refresh_ = std::make_unique<MapRefreshCoordinator>(ctx);
}

void OverworldEditor::HandleMapInteraction() {
  if (canvas_nav_)
    canvas_nav_->HandleMapInteraction();
}

void OverworldEditor::ScrollBlocksetCanvasToCurrentTile() {
  if (canvas_nav_)
    canvas_nav_->ScrollBlocksetCanvasToCurrentTile();
}

void OverworldEditor::RequestTile16Selection(int tile_id) {
  if (tile_id < 0 || tile_id >= zelda3::kNumTile16Individual) {
    return;
  }

  if (!rom_ || !rom_->is_loaded() || !all_gfx_loaded_ ||
      !map_blockset_loaded_) {
    current_tile16_ = tile_id;
    pending_tile16_selection_after_gfx_ = tile_id;
    return;
  }

  pending_tile16_selection_after_gfx_.reset();

  if (tile_id == tile16_editor_.current_tile16()) {
    current_tile16_ = tile_id;
    auto status = tile16_editor_.SetCurrentTile(tile_id);
    if (!status.ok()) {
      LOG_WARN("OverworldEditor", "Failed to refresh selected Tile16 %d: %s",
               tile_id, status.message().data());
    }
    return;
  }

  tile16_editor_.RequestTileSwitch(tile_id);
  current_tile16_ = tile16_editor_.current_tile16();
}

absl::Status OverworldEditor::Clear() {
  pending_tile16_selection_after_gfx_.reset();

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

void OverworldEditor::UpdateBlocksetSelectorState() {
  if (canvas_nav_)
    canvas_nav_->UpdateBlocksetSelectorState();
}

void OverworldEditor::CycleTileSelection(int delta) {
  const int next =
      std::clamp(current_tile16_ + delta, 0, zelda3::kNumTile16Individual - 1);
  RequestTile16Selection(next);
}

void OverworldEditor::ToggleMapLock() {
  current_map_lock_ = !current_map_lock_;
}

void OverworldEditor::ToggleCanvasFullscreen() {
  overworld_canvas_fullscreen_ = !overworld_canvas_fullscreen_;
}

void OverworldEditor::ToggleTile16EditorWindow() {
  if (!dependencies_.window_manager) {
    return;
  }
  dependencies_.window_manager->ToggleWindow(
      dependencies_.window_manager->GetActiveSessionId(),
      OverworldPanelIds::kTile16Editor);
}

void OverworldEditor::ToggleItemListWindow() {
  if (!dependencies_.window_manager) {
    return;
  }
  dependencies_.window_manager->ToggleWindow(
      dependencies_.window_manager->GetActiveSessionId(),
      OverworldPanelIds::kItemList);
}

void OverworldEditor::SelectAdjacentMap(int dx, int dy) {
  const int world = std::clamp(current_world_, 0, 2);
  const int local = current_map_ - world * 0x40;
  const int rows = world == 2 ? 4 : 8;  // Special world: four rows.
  const int x = local % 8 + dx;
  const int y = local / 8 + dy;
  if (x < 0 || x >= 8 || y < 0 || y >= rows) {
    return;
  }
  const int target = world * 0x40 + y * 8 + x;
  if (target >= zelda3::kNumOverworldMaps) {
    return;
  }
  SelectMapForEditing(target, /*respect_pin=*/false);
  if (canvas_nav_) {
    // Keep the keyboard choice until the mouse moves, then resume following
    // the cursor (unless pinned).
    canvas_nav_->SuspendHoverFollowUntilMouseMoves();
    canvas_nav_->CenterOnMap(target);
  }
}

void OverworldEditor::JumpToMap(int map_id) {
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return;
  }
  const int world = std::clamp(map_id / 0x40, 0, 2);
  if (world != current_world_) {
    SwitchToWorld(world);
  }
  SelectMapForEditing(map_id, /*respect_pin=*/false);
  if (canvas_nav_) {
    // Same as keyboard navigation: keep the choice until the mouse moves.
    canvas_nav_->SuspendHoverFollowUntilMouseMoves();
    canvas_nav_->CenterOnMap(map_id);
  }
}

void OverworldEditor::SetEditingMode(EditingMode mode) {
  current_mode = mode;
  entity_edit_mode_ = EntityEditMode::NONE;
  ow_map_canvas_.SetUsageMode(mode == EditingMode::MOUSE
                                  ? gui::CanvasUsage::kEntityManipulation
                                  : gui::CanvasUsage::kTilePainting);
}

void OverworldEditor::SetEntityEditMode(EntityEditMode mode) {
  entity_edit_mode_ = mode;
  if (mode != EntityEditMode::NONE) {
    // Entity work happens with the select tool.
    current_mode = EditingMode::MOUSE;
    ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kEntityManipulation);
  }
}

void OverworldEditor::ToggleGrid() {
  auto config = ow_map_canvas_.GetConfig();
  config.enable_grid = !config.enable_grid;
  ow_map_canvas_.ApplyConfigSnapshot(config);
}

bool OverworldEditor::grid_visible() const {
  return ow_map_canvas_.GetConfig().enable_grid;
}

void OverworldEditor::OpenMapPropertiesWindow() {
  auto* window_manager = dependencies_.window_manager;
  if (!window_manager) {
    return;
  }
  const size_t session_id = window_manager->GetActiveSessionId();
  window_manager->OpenWindow(session_id, OverworldPanelIds::kMapProperties);
  window_manager->MarkWindowRecentlyUsed(OverworldPanelIds::kMapProperties);
  // Bring a docked-but-hidden tab to front. No-op until the window exists.
  const std::string window_name = window_manager->GetWorkspaceWindowName(
      session_id, OverworldPanelIds::kMapProperties);
  if (!window_name.empty()) {
    ImGui::SetWindowFocus(window_name.c_str());
  }
}

void OverworldEditor::ContributeStatus(StatusBar* status_bar) {
  if (!status_bar)
    return;
  status_bar->SetCustomSegment(
      "Map",
      FormatOverworldMapStatusSegment(
          current_map_, EffectiveOverworldHoverMap(
                            hovered_map_, ow_map_canvas_.IsMouseHovering())));
  status_bar->SetCustomSegment("Tile16",
                               absl::StrFormat("0x%03X", current_tile16_));

  const char* mode_label;
  switch (entity_edit_mode_) {
    case EntityEditMode::ENTRANCES:
      mode_label = "Entrances";
      break;
    case EntityEditMode::EXITS:
      mode_label = "Exits";
      break;
    case EntityEditMode::ITEMS:
      mode_label = "Items";
      break;
    case EntityEditMode::SPRITES:
      mode_label = "Sprites";
      break;
    case EntityEditMode::TRANSPORTS:
      mode_label = "Transports";
      break;
    case EntityEditMode::MUSIC:
      mode_label = "Music";
      break;
    case EntityEditMode::NONE:
      switch (current_mode) {
        case EditingMode::MOUSE:
          mode_label = "Mouse";
          break;
        case EditingMode::DRAW_TILE:
          mode_label = "Draw";
          break;
        case EditingMode::FILL_TILE:
          mode_label = "Fill";
          break;
        default:
          mode_label = "Draw";
          break;
      }
      break;
    default:
      mode_label = "Draw";
      break;
  }
  status_bar->SetEditorMode(mode_label);
}

EditorContextSnapshot OverworldEditor::BuildContextSnapshot() const {
  EditorContextSnapshot snapshot;
  snapshot.category = "Overworld";
  snapshot.semantic_owner = absl::StrFormat("overworld.map.%03X", current_map_);

  if (!overworld_.is_loaded() || current_map_ < 0) {
    snapshot.title = "Overworld";
    snapshot.subtitle = "Load a ROM to inspect map context";
    return snapshot;
  }

  const OverworldMapMetadata map = BuildOverworldMapMetadata(
      overworld_, rom_, dependencies_.project, current_map_, game_state_);
  snapshot.title = map.map_title;
  snapshot.subtitle =
      absl::StrFormat("%s · %s", map.world_label, map.area_size_label);
  snapshot.metadata = {
      {.id = "map", .label = "Map", .value = map.map_id_label},
      {.id = "parent", .label = "Parent", .value = map.parent_label},
      {.id = "tile16",
       .label = "Tile16",
       .value = absl::StrFormat("0x%03X", current_tile16_)},
      {.id = "graphics", .label = "Graphics", .value = map.area_gfx_label},
      {.id = "palette", .label = "Palette", .value = map.area_palette_label},
  };

  const int owner_map = current_parent_;
  const auto belongs_to_area = [owner_map](const auto& entity) {
    return static_cast<int>(entity.map_id_) == owner_map;
  };
  const auto sprites = overworld_.sprites(game_state_);
  const auto items = overworld_.all_items();
  const auto* exits = overworld_.exits();
  snapshot.counts = {
      {.id = "sprites",
       .label = "Sprites",
       .value = std::to_string(std::count_if(
           sprites.begin(), sprites.end(),
           [owner_map](const zelda3::Sprite& sprite) {
             return static_cast<int>(sprite.map_id()) == owner_map;
           }))},
      {.id = "items",
       .label = "Items",
       .value = std::to_string(
           std::count_if(items.begin(), items.end(), belongs_to_area))},
      {.id = "entrances",
       .label = "Entrances",
       .value = std::to_string(std::count_if(overworld_.entrances().begin(),
                                             overworld_.entrances().end(),
                                             belongs_to_area))},
      {.id = "exits",
       .label = "Exits",
       .value = exits ? std::to_string(std::count_if(
                            exits->begin(), exits->end(), belongs_to_area))
                      : "0"},
  };

  snapshot.has_pending_changes = rom_ != nullptr && rom_->dirty();
  snapshot.pending_label =
      snapshot.has_pending_changes ? "ROM buffer has pending changes" : "";
  if (current_map_lock_) {
    snapshot.diagnostics.push_back({
        .id = "map_pinned",
        .severity = EditorContextDiagnosticSeverity::kInfo,
        .message = "Map context is pinned; cursor hover will not replace it.",
    });
  }
  if (!selected_tile16_ids_.empty()) {
    snapshot.diagnostics.push_back({
        .id = "tile_selection",
        .severity = EditorContextDiagnosticSeverity::kInfo,
        .message = absl::StrFormat("%zu Tile16 cells selected",
                                   selected_tile16_ids_.size()),
    });
  }

  snapshot.capabilities = {
      "overworld.properties",
      "overworld.entities",
      "overworld.tile16",
  };
  snapshot.actions = {
      {.id = "open_properties",
       .label = "Map Properties",
       .target = "overworld.properties"},
      {.id = "open_entities",
       .label = "Entity Workbench",
       .target = "overworld.entity_workbench"},
      {.id = "open_tile16",
       .label = "Tile16 Selector",
       .target = "overworld.tile16_selector"},
  };
  return snapshot;
}

}  // namespace yaze::editor
