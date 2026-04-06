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
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/core/undo_manager.h"
#include "app/editor/overworld/canvas_navigation_manager.h"
#include "app/editor/overworld/debug_window_card.h"
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/entity_operations.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "app/editor/overworld/overworld_sidebar.h"
#include "app/editor/overworld/overworld_toolbar.h"
#include "app/editor/overworld/overworld_undo_actions.h"
// Note: All overworld panels now self-register via REGISTER_PANEL macro:
// AreaGraphicsPanel, DebugWindowPanel, GfxGroupsPanel, MapPropertiesPanel,
// OverworldCanvasPanel, ScratchSpacePanel, Tile16EditorPanel, Tile16SelectorPanel,
// Tile8SelectorPanel, UsageStatisticsPanel, V3SettingsPanel, OverworldItemListPanel
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/editor/overworld/usage_statistics_card.h"
#include "app/editor/system/hack_manifest_save_validation.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
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

void OverworldEditor::Initialize() {
  // Register panels with PanelManager (dependency injection)
  if (!dependencies_.panel_manager) {
    return;
  }
  auto* panel_manager = dependencies_.panel_manager;

  // Initialize renderer from dependencies
  renderer_ = dependencies_.renderer;

  // Initialize MapRefreshCoordinator (must be before callbacks that use it)
  InitMapRefreshCoordinator();

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

  // Initialize OverworldCanvasRenderer for canvas and panel drawing
  canvas_renderer_ = std::make_unique<OverworldCanvasRenderer>(this);

  InitCanvasNavigationManager();
  InitTilePaintingManager();
  SetupCanvasAutomation();
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
  deps.current_parent = &current_parent_;
  deps.current_mode = &current_mode;
  deps.game_state = &game_state_;
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
  callbacks.refresh_overworld_map = [this]() {
    this->RefreshOverworldMap();
  };
  callbacks.refresh_overworld_map_on_demand = [this](int map_index) {
    this->RefreshOverworldMapOnDemand(map_index);
  };
  callbacks.scroll_blockset_to_current_tile = [this]() {
    this->ScrollBlocksetCanvasToCurrentTile();
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
  ctx.current_mode = &current_mode;
  ctx.current_map_lock = &current_map_lock_;
  ctx.is_dragging_entity = &is_dragging_entity_;
  ctx.show_map_properties_panel = &show_map_properties_panel_;
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
  callbacks.pick_tile16_from_hovered_canvas = [this]() -> bool {
    return this->PickTile16FromHoveredCanvas();
  };
  callbacks.is_entity_hovered = [this]() -> bool {
    return entity_renderer_ && entity_renderer_->hovered_entity() != nullptr;
  };

  canvas_nav_ = std::make_unique<CanvasNavigationManager>();
  canvas_nav_->Initialize(ctx, callbacks);
}

absl::Status OverworldEditor::Load() {
  gfx::ScopedTimer timer("OverworldEditor::Load");

  LOG_DEBUG("OverworldEditor", "Loading overworld.");
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Clear undo/redo state when loading new ROM data
  current_paint_operation_.reset();
  undo_manager_.Clear();

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
    const auto& theme = AgentUI::GetTheme();
    ImGui::TextColored(theme.status_error,
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
  const bool shift_held = ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                          ImGui::IsKeyDown(ImGuiKey_RightShift);
  const bool alt_held =
      ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);

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
    if (ImGui::IsKeyPressed(ImGuiKey_I, false)) {
      (void)PickTile16FromHoveredCanvas();
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

  // Toggle Overworld Item List with Ctrl+Shift+I
  if (ctrl_held && shift_held && ImGui::IsKeyPressed(ImGuiKey_I, false)) {
    if (dependencies_.panel_manager) {
      dependencies_.panel_manager->TogglePanel(0, OverworldPanelIds::kItemList);
    }
  }

  // Item workflow shortcuts (duplicate + nudge) when in item edit mode.
  if (entity_edit_mode_ == EntityEditMode::ITEMS) {
    if (ctrl_held && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
      (void)DuplicateSelectedItem();
    } else if (!ctrl_held) {
      const int step = shift_held ? 16 : 1;
      int delta_x = 0;
      int delta_y = 0;
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false)) {
        delta_x = -step;
      } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false)) {
        delta_x = step;
      } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
        delta_y = -step;
      } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
        delta_y = step;
      }
      if (delta_x != 0 || delta_y != 0) {
        (void)NudgeSelectedItem(delta_x, delta_y);
      }
    }
  }

  // Undo/Redo shortcuts
  HandleUndoRedoShortcuts();
}

bool OverworldEditor::SelectItemByIdentity(
    const zelda3::OverworldItem& item_identity) {
  auto* item = FindItemByIdentity(&overworld_, item_identity);
  if (!item) {
    return false;
  }

  selected_item_identity_ = *item;
  current_item_ = *item;
  current_entity_ = item;
  return true;
}

void OverworldEditor::ClearSelectedItem() {
  selected_item_identity_.reset();
  if (current_entity_ &&
      current_entity_->entity_type_ == zelda3::GameEntity::EntityType::kItem) {
    current_entity_ = nullptr;
  }
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

  current_item_ = *item;
  current_entity_ = item;
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
      current_item_ = *selected_item;
      current_entity_ = selected_item;
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
          "Select an overworld item first (Item Mode: key 5)",
          ToastType::kInfo);
    }
    return false;
  }

  auto before_snapshot = CaptureItemUndoSnapshot();
  auto duplicate_or =
      DuplicateItemByIdentity(&overworld_, *selected_item, offset_x, offset_y);
  if (!duplicate_or.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Failed to duplicate overworld item",
                                        ToastType::kError);
    }
    return false;
  }

  auto* duplicated_item = *duplicate_or;
  selected_item_identity_ = *duplicated_item;
  current_item_ = *duplicated_item;
  current_entity_ = duplicated_item;
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
  current_item_ = *selected_item;
  current_entity_ = selected_item;
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
  auto remove_status = RemoveItemByIdentity(&overworld_, selected_identity);
  if (!remove_status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Failed to delete selected item",
                                        ToastType::kError);
    }
    return false;
  }

  auto* nearest_item =
      FindNearestItemForSelection(&overworld_, selected_identity);
  if (nearest_item) {
    selected_item_identity_ = *nearest_item;
    current_item_ = *nearest_item;
    current_entity_ = nearest_item;
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
      if (!SelectItemByIdentity(
              *static_cast<zelda3::OverworldItem*>(hovered_entity))) {
        current_item_ = *static_cast<zelda3::OverworldItem*>(hovered_entity);
        current_entity_ = hovered_entity;
      }
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
      auto* live_item = static_cast<zelda3::OverworldItem*>(current_entity_);
      if (current_item_.deleted) {
        if (!DeleteSelectedItem()) {
          auto remove_status = RemoveItemByIdentity(&overworld_, current_item_);
          if (!remove_status.ok()) {
            util::logf("Failed to remove overworld item: %s",
                       remove_status.message().data());
          }
        }
      } else {
        *live_item = current_item_;
        selected_item_identity_ = *live_item;
      }
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

// DrawOverworldMaps - now in OverworldCanvasRenderer

// DrawOverworldEdits - now delegated to TilePaintingManager

// RenderUpdatedMapBitmap - now delegated to TilePaintingManager

void OverworldEditor::CheckForOverworldEdits() {
  tile_painting_->CheckForOverworldEdits();
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

  bool any_changed = false;
  for (int dy = 0; dy < height; ++dy) {
    for (int dx = 0; dx < width; ++dx) {
      const int id = ids[dy * width + dx];
      const int gx = tile16_x + dx;
      const int gy = tile16_y + dy;

      const int global_x = superX * 32 + gx;
      const int global_y = superY * 32 + gy;
      if (global_x < 0 || global_x >= 256 || global_y < 0 || global_y >= 256)
        continue;
      const int old_tile_id = selected_world[global_x][global_y];
      if (old_tile_id == id) {
        continue;
      }
      CreateUndoPoint(current_map_, current_world_, global_x, global_y,
                      old_tile_id);
      selected_world[global_x][global_y] = id;
      any_changed = true;
    }
  }

  if (any_changed) {
    FinalizePaintOperation();
    rom_->set_dirty(true);
    RefreshOverworldMap();
  }
  return absl::OkStatus();
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
    gui::CenterText("No ROM loaded");
    return absl::OkStatus();
  }
}

// DrawV3Settings - now in OverworldCanvasRenderer

// DrawMapProperties - now in OverworldCanvasRenderer

absl::Status OverworldEditor::Save() {
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
      std::move(changes), &overworld_, [this]() { RefreshOverworldMap(); });
  undo_manager_.Push(std::move(action));

  current_paint_operation_.reset();
}

absl::Status OverworldEditor::Undo() {
  // Finalize any pending paint operation first
  FinalizePaintOperation();
  return undo_manager_.Undo();
}

absl::Status OverworldEditor::Redo() {
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

  auto try_refresh_map = [&](int map_index) {
    if (map_index < 0 || map_index >= zelda3::kNumOverworldMaps) {
      return;
    }
    if (refresh_count >= max_refreshes_per_frame) {
      return;
    }
    if (maps_bmp_[map_index].modified() && maps_bmp_[map_index].is_active()) {
      RefreshOverworldMapOnDemand(map_index);
      ++refresh_count;
    }
  };

  // Highest priority: current map.
  try_refresh_map(current_map_);

  // Then refresh maps in the active world only, avoiding a full-map scan each frame.
  const int world_start = current_world_ * 0x40;
  const int world_end = std::min(world_start + 0x40, zelda3::kNumOverworldMaps);
  for (int i = world_start;
       i < world_end && refresh_count < max_refreshes_per_frame; ++i) {
    if (i == current_map_) {
      continue;
    }
    try_refresh_map(i);
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
  if (canvas_nav_)
    canvas_nav_->UpdateBlocksetSelectorState();
}

void OverworldEditor::CycleTileSelection(int delta) {
  current_tile16_ = std::max(0, current_tile16_ + delta);
}

}  // namespace yaze::editor
