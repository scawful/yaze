// Related header
#include "app/editor/overworld/canvas/overworld_canvas_renderer.h"
#include "app/editor/overworld/canvas/overworld_canvas_layout.h"
#include "app/editor/overworld/canvas/overworld_context_target.h"
#include "app/gui/canvas/canvas_utils.h"
#include "util/i18n/tr.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

// C++ standard library headers
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// Third-party library headers
#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/overworld/entity/entity.h"
#include "app/editor/overworld/entity/overworld_entity_renderer.h"
#include "app/editor/overworld/maps/map_properties.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui/navigation/overworld_sidebar.h"
#include "app/editor/overworld/ui/navigation/overworld_toolbar.h"
#include "app/editor/overworld/ui/ui_constants.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/automation/widget_id_registry.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "core/lyra_terrain_document.h"
#include "rom/rom.h"
#include "util/log.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {
namespace {

int EntityMapId(const zelda3::GameEntity& entity) {
  switch (entity.entity_type_) {
    case zelda3::GameEntity::EntityType::kItem:
      return static_cast<const zelda3::OverworldItem&>(entity).room_map_id_;
    case zelda3::GameEntity::EntityType::kSprite:
      return static_cast<const zelda3::Sprite&>(entity).map_id();
    case zelda3::GameEntity::EntityType::kEntrance:
    case zelda3::GameEntity::EntityType::kExit:
    default:
      return entity.map_id_;
  }
}

ImVec2 ClampUnifiedEntityLocalPos(const zelda3::Overworld& overworld,
                                  const zelda3::GameEntity& entity,
                                  int entity_world, int displayed_map_count,
                                  ImVec2 local_pos, int grid_size) {
  if (entity.entity_type_ == zelda3::GameEntity::EntityType::kSprite) {
    const int owner = static_cast<const zelda3::Sprite&>(entity).map_id();
    const auto* parent = overworld.overworld_map(owner);
    const int min_x = OverworldCanvasLayout::GridX(owner) * kOverworldMapSizePx;
    const int min_y = OverworldCanvasLayout::GridY(owner) * kOverworldMapSizePx;
    int width = kOverworldMapSizePx;
    int height = kOverworldMapSizePx;
    if (parent) {
      const auto size = parent->area_size();
      if (size == zelda3::AreaSizeEnum::LargeArea ||
          size == zelda3::AreaSizeEnum::WideArea) {
        width = 1024;
      }
      if (size == zelda3::AreaSizeEnum::LargeArea ||
          size == zelda3::AreaSizeEnum::TallArea) {
        height = 1024;
      }
    }
    return ImVec2(std::clamp(local_pos.x, static_cast<float>(min_x),
                             static_cast<float>(min_x + width - grid_size)),
                  std::clamp(local_pos.y, static_cast<float>(min_y),
                             static_cast<float>(min_y + height - grid_size)));
  }

  const int max_x = kOverworldWorldBlockPx - grid_size;
  int max_rows = 8;
  if (entity_world == 2) {
    if (entity.entity_type_ == zelda3::GameEntity::EntityType::kItem) {
      max_rows = 4;
    } else {
      max_rows = std::clamp(
          (displayed_map_count - OverworldCanvasLayout::WorldStartMap(2) + 7) /
              8,
          1, 8);
    }
  }
  const int max_y = max_rows * kOverworldMapSizePx - grid_size;
  return ImVec2(std::clamp(local_pos.x, 0.0f, static_cast<float>(max_x)),
                std::clamp(local_pos.y, 0.0f, static_cast<float>(max_y)));
}

}  // namespace

void DrawSheetScaleControl(const char* id, gui::AdaptiveSheetScaleMode* mode) {
  if (mode == nullptr) {
    return;
  }
  constexpr const char* kLabels[] = {"Fit", "1x", "2x", "4x"};
  int selected = static_cast<int>(*mode);
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", tr("Scale"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::ScaledSize(80.0f, 0.0f).x);
  if (ImGui::Combo(id, &selected, kLabels, IM_ARRAYSIZE(kLabels))) {
    *mode = static_cast<gui::AdaptiveSheetScaleMode>(selected);
  }
}

gui::TileSelectorWidget::Config AreaGraphicsSelectorConfig() {
  // The area sheet is 0x80 pixels wide: Tile8 ID n sits at column n % 16,
  // row n / 16 (see zelda3/overworld/graphics_preview.h).
  constexpr int kTile8Size = 8;
  constexpr int kColumns = 0x80 / kTile8Size;
  gui::TileSelectorWidget::Config config;
  config.tile_size = kTile8Size;
  config.display_scale = 1.0f;
  config.tiles_per_row = kColumns;
  config.total_tiles =
      kColumns * static_cast<int>(kOverworldMapSize) / kTile8Size;
  config.draw_offset = ImVec2(kTile16SelectorDrawOffsetX, 0.0f);
  config.show_hover_tooltip = true;
  return config;
}

std::string Tile8CollisionLabel(int tile8_id, absl::Span<const uint8_t> types) {
  // Same wording as the Tile16 workbench quadrant readout.
  if (tile8_id < 0 || tile8_id >= zelda3::kNumTileTypes) {
    return "unknown (Tile8 ID outside collision table)";
  }
  if (types.size() < static_cast<size_t>(zelda3::kNumTileTypes)) {
    return "unavailable for this map/layout";
  }
  return zelda3::TileTypeNames[types[tile8_id]];
}

namespace {}  // namespace

OverworldCanvasRenderer::OverworldCanvasRenderer(OverworldEditor* editor)
    : editor_(editor) {
  if (editor_) {
    editor_->ow_map_canvas_.SetShowBuiltinContextMenu(false);
    editor_->ow_map_canvas_.SetContextMenuOpenCallback(
        [this](const ImVec2& position) {
          return PrepareContextMenu(position);
        });
  }
}

// =============================================================================
// Main Canvas Drawing
// =============================================================================

void OverworldCanvasRenderer::DrawOverworldCanvas() {
  if (!editor_) {
    return;
  }
  const bool lyra_preview = editor_->overworld_.is_lyra_preview();
  const bool lyra_writable = editor_->lyra_a0_terrain_writable();
  if (lyra_preview) {
    const auto layout = editor_->rom_->InspectLyraLayout();
    ImGui::Text("Lyra ROM rev%d: %d stored / %d active / %s", layout.revision,
                layout.capacity, layout.active_count,
                lyra_writable ? "A0 terrain Save As" : "read-only");
    ImGui::SameLine();
    ImGui::TextDisabled("Sprite lists and area overlays unavailable");
    int map_id = editor_->current_map_;
    ImGui::SetNextItemWidth(210.0f);
    if (ImGui::SliderInt("Inspect map", &map_id, 0x80, 0xBF, "0x%02X")) {
      editor_->JumpToMap(map_id);
    }
    ImGui::SameLine();
    ImGui::TextDisabled(
        layout.offline_only
            ? "Offline map data; gameplay requires an Oracle build"
            : "Runtime-declared map data; inspection only");
    if (lyra_writable) {
      ImGui::RadioButton("Walkable floor", &editor_->lyra_terrain_brush_,
                         0x2103);
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          "OverworldLyra/radio:WalkableFloor", "radio", ImGui::GetItemID());
      ImGui::SameLine();
      ImGui::RadioButton("Blocking border", &editor_->lyra_terrain_brush_,
                         0x20B2);
      gui::WidgetIdRegistry::Instance().RegisterWidget(
          "OverworldLyra/radio:BlockingBorder", "radio", ImGui::GetItemID());
      ImGui::SameLine();
      ImGui::TextDisabled(
          "Click A0 terrain. Center entry/return cells stay walkable.");
    }
  }

  // Keep appearance edits beside the canvas, using the same undo/refresh path
  // as the full Map Properties panel.
  if (!lyra_preview && editor_->rom_ != nullptr && editor_->rom_->is_loaded() &&
      editor_->overworld_.is_loaded() && editor_->toolbar_) {
    editor_->toolbar_->shortcuts = editor_->dependencies_.shortcut_manager;
    editor_->toolbar_->Draw(
        editor_->current_world_, editor_->current_map_,
        editor_->current_map_lock_, editor_->current_mode,
        editor_->entity_edit_mode_, editor_->dependencies_.window_manager,
        editor_->rom_, &editor_->overworld_, editor_->dependencies_.project,
        editor_->game_state_, [this]() {
          if (editor_->sidebar_) {
            editor_->sidebar_->DrawQuickProperties(
                editor_->current_map_, editor_->current_map_lock_,
                editor_->dependencies_.project, /*compact=*/true);
          }
        });
    editor_->NormalizeCurrentSelectionState();
  }
  const OverworldCanvasLayout layout = editor_->GetCanvasLayout();
  const bool unified_layout = layout.IsUnified();
  if (!lyra_preview && (editor_->current_world_ == 2 || unified_layout) &&
      !editor_->lyra_canvas_status_.ok()) {
    ImGui::TextWrapped("%s", editor_->lyra_canvas_status_.ToString().c_str());
  }

  // ==========================================================================
  // PHASE 3: Modern BeginCanvas/EndCanvas Pattern
  // ==========================================================================
  // Menu actions are captured by PrepareContextMenu only when it opens.
  // Keep rendering an open menu even after entity hover changes. Paint modes
  // keep right-click for sampling; Shift+right-click opens the menu there
  // (PrepareContextMenu vetoes a plain right-click).
  const bool show_context_menu = !lyra_preview;

  // Configure canvas frame options
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = layout.CanvasSize();
  // In Unified mode we draw aligned per-block grids so the 32px gutter and
  // empty south-east quadrant remain ungridded.
  frame_opts.draw_grid = !unified_layout;
  frame_opts.draw_context_menu = show_context_menu;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = !lyra_preview;
  frame_opts.use_child_window = false;  // CRITICAL: Canvas has own pan logic

  // Wrap in child window for scrollbars. The navigation manager owns all
  // scrolling (wheel, drag pan, zoom anchoring), so ImGui's own wheel scroll
  // is disabled on this child.
  gui::BeginNoPadding();
  if (editor_->canvas_nav_) {
    editor_->canvas_nav_->BeginCanvasViewport();
  }
  ImGui::BeginChild(ImGui::GetID(reinterpret_cast<void*>(intptr_t{7})),
                    ImGui::GetContentRegionAvail(), true,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar |
                        ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  // Keep canvas scroll at 0 - ImGui's child window handles all scrolling
  // The scrollbars scroll the child window which moves the entire canvas
  editor_->ow_map_canvas_.set_scrolling(ImVec2(0, 0));

  // Begin canvas frame - this handles DrawBackground + DrawContextMenu
  auto canvas_rt = gui::BeginCanvas(editor_->ow_map_canvas_, frame_opts);
  gui::EndNoPadding();

  if (editor_->canvas_nav_) {
    editor_->canvas_nav_->EndCanvasViewport(canvas_rt.hovered);
  }

  if (editor_->overworld_.is_loaded()) {
    // Draw the active world's maps (Single) or all 3 worlds (Unified).
    DrawOverworldMaps();
    if (lyra_preview) {
      editor_->status_ = editor_->CheckForCurrentMap();
      if (lyra_writable && canvas_rt.hovered &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const auto target = ResolveOverworldContextTarget(
            layout, editor_->current_world_, editor_->game_state_,
            ImGui::GetMousePos(), canvas_rt.canvas_p0, canvas_rt.scrolling,
            canvas_rt.scale, zelda3::kExpandedMapCount);
        if (target && target->map_id == 0xA0) {
          editor_->status_ = editor_->PaintLyraA0Terrain(
              target->map_id,
              static_cast<int>(target->world_position.x) % 512 / 32,
              static_cast<int>(target->world_position.y) % 512 / 32,
              static_cast<uint16_t>(editor_->lyra_terrain_brush_));
        }
      }
      if (unified_layout) {
        DrawUnifiedBlockChrome(canvas_rt, layout);
        DrawUnifiedReadOnlyBanner();
      }
      gui::EndCanvas(editor_->ow_map_canvas_, canvas_rt, frame_opts);
      ImGui::EndChild();
      return;
    }

    // The lower four Special World rows are project terrain. Navigation uses
    // the same canvas, while these cells bypass the legacy ROM editing model.
    editor_->status_ = editor_->CheckForCurrentMap();
    gui::TileDragPayload tile_drop;
    if (gui::AcceptTileDrop(&tile_drop)) {
      editor_->RequestTile16Selection(tile_drop.tile_id);
      editor_->current_mode = EditingMode::DRAW_TILE;
      editor_->ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
    }
    const bool placing_project_entity =
        editor_->lyra_canvas_.entity_placement_active();
    const bool draw_special_project_terrain =
        editor_->lyra_canvas_.loaded() &&
        (unified_layout || editor_->current_world_ == 2);
    if (draw_special_project_terrain) {
      const auto assets = editor_->lyra_canvas_.PrepareMapAssets(
          editor_->overworld_, editor_->game_state_);
      if (!assets.ok())
        editor_->status_ = assets;
      editor_->lyra_canvas_.DrawSpecialWorld(canvas_rt,
                                             layout.LyraTailOrigin());
      if (editor_->show_entities_) {
        editor_->lyra_canvas_.DrawEntities(canvas_rt, editor_->current_world_,
                                           editor_->game_state_, layout);
        editor_->lyra_canvas_.HandleEntityInput(
            canvas_rt, editor_->current_world_, editor_->overworld_,
            editor_->undo_manager_, layout);
      }
      if (editor_->IsProjectTerrainMap(editor_->current_map_)) {
        const ImVec2 outline_origin = layout.MapOrigin(editor_->current_map_);
        editor_->ow_map_canvas_.DrawOutline(static_cast<int>(outline_origin.x),
                                            static_cast<int>(outline_origin.y),
                                            kOverworldMapSize,
                                            kOverworldMapSize);
      }
    }
    if (!draw_special_project_terrain) {
      editor_->lyra_canvas_.FinishStroke(editor_->undo_manager_);
    }
    const bool tile_tool = !placing_project_entity &&
                           (editor_->current_mode == EditingMode::DRAW_TILE ||
                            editor_->current_mode == EditingMode::FILL_TILE);
    if (!tile_tool && editor_->tile_painting_)
      editor_->tile_painting_->SuspendCanvasGestures();
    // Live proposal ghost: drawn under entities, only while the inbox is open.
    editor_->DrawLiveProposalGhost();

    if (!unified_layout && editor_->current_world_ != 2 &&
        editor_->lyra_canvas_.loaded() && editor_->show_entities_) {
      editor_->lyra_canvas_.DrawEntities(canvas_rt, editor_->current_world_,
                                         editor_->game_state_, layout);
      editor_->lyra_canvas_.HandleEntityInput(
          canvas_rt, editor_->current_world_, editor_->overworld_,
          editor_->undo_manager_, layout);
    }
    // Draw all entities using the new CanvasRuntime-based methods
    if (editor_->entity_renderer_ && editor_->show_entities_) {
      editor_->entity_renderer_->DrawAllEntities(
          canvas_rt, layout, editor_->current_world_, editor_->game_state_);
      FilterHoveredEntityByMode();
      DrawEntityHoverTooltip();
    } else if (editor_->entity_renderer_) {
      // Hidden entities can't be hovered, dragged or right-clicked.
      editor_->entity_renderer_->ResetHoveredEntity();
    }

    // Area subscreen overlays are composited per map in DrawOverworldMaps()
    // (toggle: show_overlay_preview_).

    // Always refresh hover preview: when the canvas is not hovered this clears
    // hovered_map_ so the status bar falls back to the selected map.
    if (canvas_rt.hovered) {
      editor_->HandleMapInteraction();
    }

    if (tile_tool) {
      editor_->CheckForOverworldEdits();
    }

    if (placing_project_entity) {
      gui::EndCanvas(editor_->ow_map_canvas_, canvas_rt, frame_opts);
      ImGui::EndChild();
      return;
    }
    // --- BEGIN ENTITY DRAG/DROP LOGIC ---
    if (editor_->current_mode == EditingMode::MOUSE &&
        editor_->TryPlaceArmedItem()) {
      // The click placed the item chosen from Insert Item.
    } else if (editor_->current_mode == EditingMode::MOUSE &&
               editor_->entity_renderer_) {
      auto hovered_entity = editor_->entity_renderer_->hovered_entity();

      if (hovered_entity && !editor_->is_dragging_entity_ &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        // Plain select is a read: always update selection, even if a sprite
        // cannot be dragged.
        editor_->SetCurrentEntity(hovered_entity);
        if (hovered_entity->entity_type_ ==
            zelda3::GameEntity::EntityType::kItem) {
          editor_->SelectItemByIdentity(
              *static_cast<zelda3::OverworldItem*>(hovered_entity));
        } else if (auto* workbench = editor_->GetWorkbench()) {
          workbench->SetActiveEntity(hovered_entity);
        }
        if (hovered_entity->entity_type_ ==
            zelda3::GameEntity::EntityType::kSprite) {
          auto* sprite = static_cast<zelda3::Sprite*>(hovered_entity);
          if (!editor_->ValidateSpriteEdit(sprite, *sprite).ok())
            hovered_entity = nullptr;
        }
      }
      // 1. Initiate drag
      if (!editor_->is_dragging_entity_ && hovered_entity &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        editor_->dragged_entity_ = hovered_entity;
        entity_drag_origin_ = ImVec2(hovered_entity->x_, hovered_entity->y_);
        entity_drag_position_ = entity_drag_origin_;
        entity_drag_world_ = OverworldCanvasLayout::ClampedWorldForMap(
            EntityMapId(*hovered_entity));
        entity_drag_moved_ = false;
        editor_->is_dragging_entity_ = true;
        editor_->drag_item_snapshot_.reset();
        if (hovered_entity->entity_type_ ==
            zelda3::GameEntity::EntityType::kItem) {
          // Items have snapshot undo; capture before the drag moves it.
          editor_->drag_item_snapshot_ = editor_->CaptureItemUndoSnapshot();
        }
        if (editor_->dragged_entity_->entity_type_ ==
            zelda3::GameEntity::EntityType::kExit) {
          editor_->dragged_entity_free_movement_ = true;
        }
      }

      // 2. Update drag
      if (editor_->is_dragging_entity_ && editor_->dragged_entity_ &&
          ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        entity_drag_moved_ = true;
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
        float scale = canvas_rt.scale;
        if (scale > 0.0f) {
          if (editor_->dragged_entity_->entity_type_ ==
                  zelda3::GameEntity::EntityType::kSprite &&
              entity_drag_position_) {
            entity_drag_position_->x += mouse_delta.x / scale;
            entity_drag_position_->y += mouse_delta.y / scale;
            ImVec2 candidate = *entity_drag_position_;
            if (unified_layout) {
              candidate = ClampUnifiedEntityLocalPos(
                  editor_->overworld_, *editor_->dragged_entity_,
                  entity_drag_world_, editor_->DisplayedMapCount(), candidate,
                  16);
            }
            auto* sprite =
                static_cast<zelda3::Sprite*>(editor_->dragged_entity_);
            auto draft = *sprite;
            draft.set_x(static_cast<int>(candidate.x / 16) * 16);
            draft.set_y(static_cast<int>(candidate.y / 16) * 16);
            if ((draft.x() != sprite->x() || draft.y() != sprite->y()) &&
                editor_->ValidateSpriteEdit(sprite, draft).ok())
              *sprite = std::move(draft);
          } else {
            editor_->dragged_entity_->x_ += mouse_delta.x / scale;
            editor_->dragged_entity_->y_ += mouse_delta.y / scale;
            if (unified_layout) {
              const int grid_size =
                  editor_->dragged_entity_free_movement_ ? 8 : 16;
              const ImVec2 clamped = ClampUnifiedEntityLocalPos(
                  editor_->overworld_, *editor_->dragged_entity_,
                  entity_drag_world_, editor_->DisplayedMapCount(),
                  ImVec2(static_cast<float>(editor_->dragged_entity_->x_),
                         static_cast<float>(editor_->dragged_entity_->y_)),
                  grid_size);
              editor_->dragged_entity_->x_ = static_cast<int>(clamped.x);
              editor_->dragged_entity_->y_ = static_cast<int>(clamped.y);
            }
          }
        }
      }

      // 3. End drag
      if (editor_->is_dragging_entity_ &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (editor_->dragged_entity_ && entity_drag_moved_) {
          float end_scale = canvas_rt.scale > 0.0f ? canvas_rt.scale : 1.0f;
          if (editor_->dragged_entity_->entity_type_ ==
                  zelda3::GameEntity::EntityType::kSprite &&
              entity_drag_origin_) {
            auto* sprite =
                static_cast<zelda3::Sprite*>(editor_->dragged_entity_);
            auto draft = *sprite;
            if (unified_layout) {
              const ImVec2 block_origin =
                  layout.BlockOrigin(entity_drag_world_);
              const ImGuiIO& io = ImGui::GetIO();
              const ImVec2 raw_local((io.MousePos.x - canvas_rt.canvas_p0.x -
                                      canvas_rt.scrolling.x) /
                                             end_scale -
                                         block_origin.x,
                                     (io.MousePos.y - canvas_rt.canvas_p0.y -
                                      canvas_rt.scrolling.y) /
                                             end_scale -
                                         block_origin.y);
              const ImVec2 clamped = ClampUnifiedEntityLocalPos(
                  editor_->overworld_, *sprite, entity_drag_world_,
                  editor_->DisplayedMapCount(), raw_local, 16);
              draft.set_x(static_cast<int>(clamped.x) / 16 * 16);
              draft.set_y(static_cast<int>(clamped.y) / 16 * 16);
            } else {
              MoveEntityOnGrid(&draft, canvas_rt.canvas_p0, canvas_rt.scrolling,
                               false, end_scale);
            }
            // Restore the preview origin, then commit one checked Undo action.
            sprite->set_x(static_cast<int>(entity_drag_origin_->x));
            sprite->set_y(static_cast<int>(entity_drag_origin_->y));
            editor_->status_.Update(editor_->ApplySpriteEdit(sprite, draft));
          } else {
            if (unified_layout) {
              const ImVec2 block_origin =
                  layout.BlockOrigin(entity_drag_world_);
              const ImGuiIO& io = ImGui::GetIO();
              const int grid_size =
                  editor_->dragged_entity_free_movement_ ? 8 : 16;
              const ImVec2 raw_local((io.MousePos.x - canvas_rt.canvas_p0.x -
                                      canvas_rt.scrolling.x) /
                                             end_scale -
                                         block_origin.x,
                                     (io.MousePos.y - canvas_rt.canvas_p0.y -
                                      canvas_rt.scrolling.y) /
                                             end_scale -
                                         block_origin.y);
              const ImVec2 clamped = ClampUnifiedEntityLocalPos(
                  editor_->overworld_, *editor_->dragged_entity_,
                  entity_drag_world_, editor_->DisplayedMapCount(), raw_local,
                  grid_size);
              editor_->dragged_entity_->set_x(static_cast<int>(clamped.x) /
                                              grid_size * grid_size);
              editor_->dragged_entity_->set_y(static_cast<int>(clamped.y) /
                                              grid_size * grid_size);
            } else {
              MoveEntityOnGrid(editor_->dragged_entity_, canvas_rt.canvas_p0,
                               canvas_rt.scrolling,
                               editor_->dragged_entity_free_movement_,
                               end_scale);
            }
            const int world_for_entity =
                unified_layout ? entity_drag_world_ : editor_->current_world_;
            // Pass overworld context for proper area size detection
            const int new_map = OverworldCanvasLayout::MapIdFromLocal(
                world_for_entity, editor_->dragged_entity_->x_ / 512,
                editor_->dragged_entity_->y_ / 512);
            if (new_map >= 0 && new_map < editor_->DisplayedMapCount()) {
              editor_->dragged_entity_->UpdateMapProperties(
                  new_map, &editor_->overworld_);
            } else {
              editor_->dragged_entity_->UpdateMapProperties(
                  EntityMapId(*editor_->dragged_entity_), &editor_->overworld_);
            }
            if (editor_->drag_item_snapshot_) {
              // No-op if the item ended where it started.
              editor_->PushItemUndoAction(
                  std::move(*editor_->drag_item_snapshot_),
                  "Move overworld item");
            }
            // TODO(overworld): entrances and exits have no undo action
            // yet; their drags only mark the ROM dirty.
            editor_->rom_->set_dirty(true);
          }
        }
        editor_->drag_item_snapshot_.reset();
        editor_->is_dragging_entity_ = false;
        editor_->dragged_entity_ = nullptr;
        editor_->dragged_entity_free_movement_ = false;
        CancelEntityDrag();
      }
    }
    // --- END ENTITY DRAG/DROP LOGIC ---
  }

  if (unified_layout) {
    DrawUnifiedBlockChrome(canvas_rt, layout);
    DrawUnifiedReadOnlyBanner();
  }

  // End canvas frame - draws grid/overlay based on frame_opts
  gui::EndCanvas(editor_->ow_map_canvas_, canvas_rt, frame_opts);
  ImGui::EndChild();
  editor_->DrawItemInsertPopup();
}

void OverworldCanvasRenderer::DrawUnifiedBlockChrome(
    const gui::CanvasRuntime& canvas_rt, const OverworldCanvasLayout& layout) {
  if (!canvas_rt.draw_list || canvas_rt.scale <= 0.0f) {
    return;
  }
  const float scale = canvas_rt.scale;
  const ImVec2 origin(canvas_rt.canvas_p0.x + canvas_rt.scrolling.x,
                      canvas_rt.canvas_p0.y + canvas_rt.scrolling.y);
  const float block_extent = static_cast<float>(kOverworldWorldBlockPx) * scale;
  const ImU32 border_col = ImGui::GetColorU32(ImGuiCol_Border);
  const ImU32 badge_bg = ImGui::GetColorU32(ImGuiCol_PopupBg);
  const ImU32 text_col = ImGui::GetColorU32(ImGuiCol_Text);
  const bool draw_grid = editor_->grid_visible();
  const bool has_tail =
      editor_->overworld_.inspected_map_count() > zelda3::kNumOverworldMaps ||
      editor_->lyra_canvas_.loaded();
  const char* kBlockLabels[kOverworldWorldCount] = {
      "Light World ($00-$3F)",
      "Dark World ($40-$7F)",
      has_tail ? "Special Overworld ($80-$9F, tail $A0-$BF)"
               : "Special Overworld ($80-$9F)",
  };

  for (int world = 0; world < kOverworldWorldCount; ++world) {
    const ImVec2 block_origin = layout.BlockOrigin(world);
    const ImVec2 p0(origin.x + block_origin.x * scale,
                    origin.y + block_origin.y * scale);
    const ImVec2 p1(p0.x + block_extent, p0.y + block_extent);
    if (draw_grid) {
      canvas_rt.draw_list->PushClipRect(p0, p1, true);
      gui::CanvasUtils::DrawCanvasGridLines(canvas_rt.draw_list, p0, p1,
                                            ImVec2(0.0f, 0.0f), 64.0f, scale);
      canvas_rt.draw_list->PopClipRect();
    }
    canvas_rt.draw_list->AddRect(p0, p1, border_col, 0.0f, 0, 1.5f);

    const char* label = kBlockLabels[world];
    const ImVec2 text_sz = ImGui::CalcTextSize(label);
    const ImVec2 pad(6.0f, 3.0f);
    const ImVec2 badge_min(p0.x + 6.0f, p0.y + 6.0f);
    const ImVec2 badge_max(badge_min.x + text_sz.x + pad.x * 2.0f,
                           badge_min.y + text_sz.y + pad.y * 2.0f);
    canvas_rt.draw_list->AddRectFilled(badge_min, badge_max, badge_bg, 3.0f);
    canvas_rt.draw_list->AddRect(badge_min, badge_max, border_col, 3.0f);
    canvas_rt.draw_list->AddText(
        ImVec2(badge_min.x + pad.x, badge_min.y + pad.y), text_col, label);
  }
}

void OverworldCanvasRenderer::DrawUnifiedReadOnlyBanner() {
  if (!editor_->IsUnifiedCanvas() ||
      ImGui::GetTime() >= editor_->unified_readonly_banner_until_) {
    return;
  }
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  if (!draw_list) {
    return;
  }
  const ImVec2 clip_min = draw_list->GetClipRectMin();
  const char* text = ICON_MD_LOCK " Unified view is read-only in Phase 1";
  const ImVec2 text_sz = ImGui::CalcTextSize(text);
  const ImVec2 pad(10.0f, 6.0f);
  const ImVec2 p0(clip_min.x + 12.0f, clip_min.y + 12.0f);
  const ImVec2 p1(p0.x + text_sz.x + pad.x * 2.0f,
                  p0.y + text_sz.y + pad.y * 2.0f);
  draw_list->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_PopupBg), 4.0f);
  draw_list->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), 4.0f);
  draw_list->AddText(ImVec2(p0.x + pad.x, p0.y + pad.y),
                     ImGui::GetColorU32(ImGuiCol_Text), text);
}

// =============================================================================
// Internal Canvas Drawing Helpers
// =============================================================================

// Draws one map the way the game layers it: the area backdrop color (palette
// color 0, which the map bitmap keeps transparent), the map, then the area's
// subscreen overlay layer. The layer is built in the map's own palette;
// background overlays (sky, pyramid, lava) only cover backdrop pixels and are
// drawn opaque, front overlays (fog, canopy, rain, curtains) at half alpha.
void OverworldCanvasRenderer::DrawMapWithAreaLayers(int map_index, int map_x,
                                                    int map_y, float scale) {
  auto& canvas = editor_->ow_map_canvas_;
  auto& bitmap = editor_->maps_bmp_[map_index];
  const float size = kOverworldMapSize * scale;
  const ImVec2 origin(canvas.zero_point().x + canvas.scrolling().x + map_x,
                      canvas.zero_point().y + canvas.scrolling().y + map_y);

  if (!bitmap.palette().empty()) {
    const ImVec4 rgb = bitmap.palette()[0].rgb();
    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + size, origin.y + size),
        IM_COL32(static_cast<int>(rgb.x), static_cast<int>(rgb.y),
                 static_cast<int>(rgb.z), 255));
  }

  canvas.DrawBitmap(bitmap, map_x, map_y, scale);

  if (!editor_->show_overlay_preview_) {
    return;
  }
  auto* layer = EnsureOverlayLayer(map_index);
  if (layer && layer->bitmap.is_active() && layer->bitmap.texture()) {
    canvas.DrawBitmap(layer->bitmap, map_x, map_y, scale,
                      layer->background ? 255 : 128);
  }
}

// Returns the cached overlay layer for a map. The layer is built once per
// map (building the map if the build cache evicted it) and rebuilt when the
// map's pixels change while it is built, so an evicted map keeps its last
// layer instead of being rebuilt every frame.
OverworldCanvasRenderer::OverlayLayer*
OverworldCanvasRenderer::EnsureOverlayLayer(int map_index) {
  const auto* map = editor_->overworld_.overworld_map(map_index);
  if (!map) {
    return nullptr;
  }
  const bool has_overlay =
      zelda3::SubscreenOverlayScreen(map->render_subscreen_overlay()) >= 0;
  auto it = overlay_layers_.find(map_index);
  const bool missing =
      it == overlay_layers_.end() || it->second.has_overlay != has_overlay;
  const bool stale = !missing && map->is_built() &&
                     it->second.bitmap_serial != map->bitmap_serial();
  if (missing || stale) {
    auto& entry = overlay_layers_[map_index];
    entry.has_overlay = has_overlay;
    entry.bitmap_serial = map->bitmap_serial();
    if (has_overlay) {
      auto layer = editor_->overworld_.BuildSubscreenOverlayLayer(map_index);
      if (!layer.ok() || layer->overlay_screen < 0) {
        entry.has_overlay = false;
        return nullptr;
      }
      map = editor_->overworld_.overworld_map(map_index);
      entry.bitmap_serial = map->bitmap_serial();
      entry.background = layer->background;
      if (!entry.bitmap.is_active()) {
        entry.bitmap.Create(kOverworldMapSize, kOverworldMapSize, 8,
                            layer->pixels);
      } else {
        entry.bitmap.set_data(layer->pixels);
      }
      entry.bitmap.SetPalette(map->current_palette());
      gfx::Arena::Get().QueueTextureCommand(
          entry.bitmap.texture() ? gfx::Arena::TextureCommandType::UPDATE
                                 : gfx::Arena::TextureCommandType::CREATE,
          &entry.bitmap);
    }
    it = overlay_layers_.find(map_index);
  }
  if (!it->second.has_overlay) {
    return nullptr;
  }
  return &it->second;
}

void OverworldCanvasRenderer::DrawOverworldMaps() {
  // Get the current zoom scale for positioning and sizing
  float scale = editor_->ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  // Build only maps that intersect the visible canvas, and at most a few per
  // frame: building all maps in the first frame stalls the UI.
  // Unified mode shares this single budget (kMaxMapBuildsPerFrame = 4) across
  // all three worlds.
  constexpr int kMaxMapBuildsPerFrame = 4;
  int builds_this_frame = 0;
  const OverworldCanvasLayout layout = editor_->GetCanvasLayout();
  const bool unified_layout = layout.IsUnified();
  const ImVec2 clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
  const ImVec2 clip_max = ImGui::GetWindowDrawList()->GetClipRectMax();
  const ImVec2 canvas_origin(editor_->ow_map_canvas_.zero_point().x +
                                 editor_->ow_map_canvas_.scrolling().x,
                             editor_->ow_map_canvas_.zero_point().y +
                                 editor_->ow_map_canvas_.scrolling().y);
  const float map_extent = kOverworldMapSize * scale;
  const auto& theme = AgentUI::GetTheme();
  const ImU32 empty_fill = ImGui::GetColorU32(theme.editor_background);
  const ImU32 empty_border = ImGui::GetColorU32(ImGuiCol_Border);

  const int first_world = unified_layout ? 0 : editor_->current_world_;
  const int end_world = unified_layout ? kOverworldWorldCount : first_world + 1;

  for (int world = first_world; world < end_world; ++world) {
    for (int i = 0; i < kOverworldMapsPerWorld; i++) {
      const int world_index =
          OverworldCanvasLayout::MapIdFromLocalIndex(world, i);
      const ImVec2 map_origin_px = layout.MapOrigin(world_index);
      const int map_x = static_cast<int>(map_origin_px.x * scale);
      const int map_y = static_cast<int>(map_origin_px.y * scale);

      const float left = canvas_origin.x + map_x;
      const float top = canvas_origin.y + map_y;
      if (left >= clip_max.x || top >= clip_max.y ||
          left + map_extent <= clip_min.x || top + map_extent <= clip_min.y) {
        continue;  // Not visible
      }

      if (world_index < 0 ||
          world_index >= editor_->overworld_.inspected_map_count()) {
        // ROMs without tail maps ($A0-$BF): draw empty lower special rows as
        // clean empty cells in Unified when no Lyra sidecar supplies them.
        if (unified_layout && world == 2 && i >= 0x20 &&
            !editor_->lyra_canvas_.loaded()) {
          ImDrawList* draw_list = ImGui::GetWindowDrawList();
          const ImVec2 cell_max(left + map_extent, top + map_extent);
          draw_list->AddRectFilled(ImVec2(left, top), cell_max, empty_fill);
          draw_list->AddRect(ImVec2(left, top), cell_max, empty_border);
        }
        continue;
      }

      // Ensure visible maps are materialized on demand before drawing. A map
      // whose bitmap exists only waits for its queued texture (cheap).
      auto& map_bitmap = editor_->maps_bmp_[world_index];
      if (!map_bitmap.is_active()) {
        if (builds_this_frame < kMaxMapBuildsPerFrame) {
          editor_->EnsureMapTexture(world_index);
          ++builds_this_frame;
        }
      } else if (!map_bitmap.texture()) {
        editor_->EnsureMapTexture(world_index);
      }

      // Only draw if the map has a valid texture AND is active (has bitmap data)
      bool can_draw = editor_->maps_bmp_[world_index].texture() &&
                      editor_->maps_bmp_[world_index].is_active();

      if (can_draw) {
        DrawMapWithAreaLayers(world_index, map_x, map_y, scale);
      } else {
        // Draw a placeholder for maps that haven't loaded yet
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 placeholder_pos(left, top);
        const ImVec2 placeholder_size(map_extent, map_extent);

        // Modern loading indicator with theme colors
        draw_list->AddRectFilled(placeholder_pos,
                                 ImVec2(placeholder_pos.x + placeholder_size.x,
                                        placeholder_pos.y + placeholder_size.y),
                                 empty_fill);

        // Animated loading spinner - scale spinner radius with zoom
        const ImVec2 spinner_pos(placeholder_pos.x + placeholder_size.x / 2,
                                 placeholder_pos.y + placeholder_size.y / 2);

        const float spinner_radius = 8.0f * scale;
        const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
        const float start_angle = rotation;
        const float end_angle = rotation + IM_PI * 1.5f;

        draw_list->PathArcTo(spinner_pos, spinner_radius, start_angle,
                             end_angle, 12);
        draw_list->PathStroke(ImGui::GetColorU32(theme.status_active), 0,
                              2.5f * scale);
      }
    }
  }
}

// =============================================================================
// Panel Drawing Methods
// =============================================================================

absl::Status OverworldCanvasRenderer::DrawTile16Selector() {
  if (!editor_->blockset_selector_) {
    gui::TileSelectorWidget::Config selector_config;
    const auto& theme = AgentUI::GetTheme();
    selector_config.tile_size = 16;
    selector_config.display_scale = kTile16SelectorScale;
    selector_config.tiles_per_row = kTile16SelectorColumns;
    selector_config.total_tiles = zelda3::kNumTile16Individual;
    selector_config.draw_offset = ImVec2(kTile16SelectorDrawOffsetX, 0.0f);
    selector_config.highlight_color = theme.selection_primary;

    selector_config.enable_drag = true;
    selector_config.show_hover_tooltip = true;

    editor_->blockset_selector_ = std::make_unique<gui::TileSelectorWidget>(
        "OwBlocksetSelector", selector_config);
    editor_->blockset_selector_->AttachCanvas(&editor_->blockset_canvas_);
    // Only the selector's own items (Edit Tile16, Copy Tile ID) belong here;
    // the canvas debug/scale items do not apply to a fixed tile grid.
    editor_->blockset_canvas_.SetShowBuiltinContextMenu(false);
    editor_->blockset_selector_->SetHoverDetailProvider(
        [this](int tile_id) -> std::string {
          if (tile_id < 0 ||
              tile_id >= static_cast<int>(tile16_usage_counts_.size())) {
            return {};
          }
          return absl::StrFormat("Placed %d times (%s)",
                                 tile16_usage_counts_[tile_id],
                                 Tile16ScopeLabel());
        });
  }

  editor_->UpdateBlocksetSelectorState();
  editor_->blockset_canvas_.ClearContextMenuItems();

  auto open_tile16_editor = [this](int tile_id) {
    editor_->RequestTile16Selection(tile_id);
    if (editor_->dependencies_.window_manager) {
      const size_t session_id =
          editor_->dependencies_.window_manager->GetActiveSessionId();
      editor_->dependencies_.window_manager->OpenWindowFloating(
          session_id, OverworldPanelIds::kTile16Editor);
      editor_->dependencies_.window_manager->MarkWindowRecentlyUsed(
          OverworldPanelIds::kTile16Editor);
    }
  };

  gui::CanvasMenuItem edit_tile_item;
  edit_tile_item.label = "Edit Tile16";
  edit_tile_item.icon = ICON_MD_GRID_VIEW;
  edit_tile_item.shortcut = "Double-click";
  edit_tile_item.enabled_condition = [this]() {
    return editor_->blockset_selector_ &&
           editor_->blockset_selector_->GetSelectedTileID() >= 0;
  };
  edit_tile_item.callback = [this, open_tile16_editor]() {
    if (!editor_->blockset_selector_) {
      return;
    }
    open_tile16_editor(editor_->blockset_selector_->GetSelectedTileID());
  };
  editor_->blockset_canvas_.AddContextMenuItem(edit_tile_item);
  auto graphics_item = edit_tile_item;
  graphics_item.label = "Edit Graphics...";
  graphics_item.icon = ICON_MD_IMAGE;
  graphics_item.callback = [this]() {
    if (editor_->blockset_selector_) {
      editor_->status_ = editor_->OpenGraphicsForTile16Quadrant(
          editor_->blockset_selector_->GetSelectedTileID(), 0);
    }
  };
  editor_->blockset_canvas_.AddContextMenuItem(std::move(graphics_item));
  editor_->blockset_canvas_.AddContextMenuItem(
      editor_->blockset_selector_->CopyTileIdMenuItem());
  AddTile16DefinitionMenuItems();

  const float selector_available_width = ImGui::GetContentRegionAvail().x;
  DrawSheetScaleControl("##Tile16Scale", &tile16_scale_mode_);
  const int tile_rows =
      (zelda3::kNumTile16Individual + kTile16SelectorColumns - 1) /
      kTile16SelectorColumns;
  const gui::AdaptiveSheetLayout selector_layout =
      gui::ResolveAdaptiveSheetLayout(
          selector_available_width, 16 * kTile16SelectorColumns, 16 * tile_rows,
          tile16_scale_mode_, 0.25f, 4.0f,
          gui::TileSelectorWidget::CurrentScrollbarSize());
  editor_->blockset_selector_->SetDisplayScale(selector_layout.display_scale);
  ImGui::SameLine();
  ImGui::TextDisabled("%.2fx", selector_layout.display_scale);

  // Tile lookup and filtering share the scale row; only the grid scrolls.
  // The child fills the panel width (the panel's preferred width is grid +
  // scrollbar), has no padding or border, and never scrolls horizontally.
  ImGui::SameLine();
  if (editor_->blockset_selector_->DrawFilterBar()) {
    editor_->RequestTile16Selection(
        editor_->blockset_selector_->GetSelectedTileID());
  }
  DrawTile16ViewControls();
  RefreshTile16Order(tile16_grid_hovered_);
  // A range exists only in Tile ID order and only around the current brush.
  if (editor_->blockset_selector_->has_display_order() ||
      tile16_range_.anchor != editor_->current_tile16_) {
    tile16_range_ = {};
  }
  DrawTile16RangeStatus();

  // Padding is read at BeginChild; pop it right away so the grid's tooltip
  // and context menu keep normal popup padding.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool grid_visible = ImGui::BeginChild(
      "##Tile16Grid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
      ImGuiWindowFlags_AlwaysVerticalScrollbar);
  ImGui::PopStyleVar();
  if (!grid_visible) {
    ImGui::EndChild();
    return absl::OkStatus();
  }

  const bool project_terrain =
      editor_->IsProjectTerrainMap(editor_->current_map_);
  gfx::Bitmap& atlas =
      project_terrain ? editor_->lyra_canvas_.MapAtlas(editor_->current_map_)
                      : editor_->tile16_blockset_.atlas;
  bool atlas_ready =
      atlas.is_active() && (project_terrain || editor_->map_blockset_loaded_);
  // Adaptive tile cells can extend beyond the legacy fixed canvas bounds.
  // Size this selector's hit area to its displayed grid, including filters.
  const ImVec2 grid_size = editor_->blockset_selector_->GetGridContentSize();
  const float canvas_scale = editor_->blockset_canvas_.global_scale();
  editor_->blockset_canvas_.SetCanvasSize(
      ImVec2(grid_size.x / canvas_scale, grid_size.y / canvas_scale));
  auto result = editor_->blockset_selector_->Render(atlas, atlas_ready);
  DrawTile16RangeOverlay();

  bool range_only = false;
  if (result.tile_clicked || result.tile_right_clicked) {
    range_only =
        HandleTile16RangeClick(result.selected_tile, result.tile_right_clicked);
  }
  if (result.selection_changed && !range_only) {
    editor_->RequestTile16Selection(result.selected_tile);
    // Note: We do NOT auto-scroll here because it breaks user interaction.
    // The canvas should only scroll when explicitly requested (e.g., when
    // selecting a tile from the overworld canvas via
    // ScrollBlocksetCanvasToCurrentTile).
  }

  if (result.tile_double_clicked) {
    open_tile16_editor(result.selected_tile);
  }

  tile16_grid_hovered_ = ImGui::IsWindowHovered();
  if (tile16_grid_hovered_ && !ImGui::GetIO().WantTextInput) {
    if (tile16_range_.active() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
      tile16_range_ = {};
    }
    if (ImGui::GetIO().KeyShift &&
        editor_->blockset_selector_->has_display_order()) {
      ImGui::SetTooltip(
          "%s", tr("Range select works in Tile ID order. Sort by Tile ID to "
                   "use it."));
    }
  }
  ImGui::EndChild();
  return absl::OkStatus();
}

bool OverworldCanvasRenderer::HandleTile16RangeClick(int tile_id,
                                                     bool right_click) {
  auto* selector = editor_->blockset_selector_.get();
  if (tile_id < 0) {
    return false;
  }
  if (right_click) {
    // Right-click inside a range opens the menu for the whole range.
    if (tile16_range_.count() > 1 && tile16_range_.Contains(tile_id)) {
      selector->SetSelectedTile(tile16_range_.anchor);
      return true;
    }
    tile16_range_ = {};
    return false;
  }
  if (!ImGui::GetIO().KeyShift || selector->has_display_order()) {
    tile16_range_ = {};
    return false;
  }
  const int brush = editor_->current_tile16_;
  if (tile16_range_.anchor != brush) {
    tile16_range_ = {brush, brush};
  }
  tile16_range_.extent = tile_id;
  selector->SetSelectedTile(brush);
  return true;
}

void OverworldCanvasRenderer::DrawTile16RangeStatus() {
  if (tile16_range_.count() < 2) {
    return;
  }
  ImGui::SameLine();
  ImGui::TextDisabled("$%03X-$%03X (%d)", tile16_range_.first(),
                      tile16_range_.last(), tile16_range_.count());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tr("Tile16 range for copying definitions. "
                               "Shift+click extends it; Esc clears it."));
  }
}

void OverworldCanvasRenderer::DrawTile16RangeOverlay() {
  auto* selector = editor_->blockset_selector_.get();
  ImDrawList* draw_list = editor_->blockset_canvas_.draw_list();
  if (tile16_range_.count() < 2 || draw_list == nullptr) {
    return;
  }
  const ImVec2 origin = editor_->blockset_canvas_.zero_point();
  const ImVec2 scroll = editor_->blockset_canvas_.scrolling();
  const float cell = selector->TileCellSize();
  ImVec4 fill = AgentUI::GetTheme().selection_primary;
  fill.w = 0.25f;
  const ImU32 fill_color = ImGui::GetColorU32(fill);
  for (int id = tile16_range_.first(); id <= tile16_range_.last(); ++id) {
    const ImVec2 tile = selector->TileOrigin(id);
    if (tile.x < 0.0f) {
      continue;
    }
    const ImVec2 p0(origin.x + scroll.x + tile.x, origin.y + scroll.y + tile.y);
    draw_list->AddRectFilled(p0, ImVec2(p0.x + cell, p0.y + cell), fill_color);
  }
}

void OverworldCanvasRenderer::AddTile16DefinitionMenuItems() {
  auto* selector = editor_->blockset_selector_.get();
  const bool project_terrain =
      editor_->IsProjectTerrainMap(editor_->current_map_);
  const bool ranged = tile16_range_.count() > 1;
  const int first =
      ranged ? tile16_range_.first() : selector->GetSelectedTileID();
  const int count = ranged ? tile16_range_.count() : 1;
  auto show_toast = [this](const std::string& message, ToastType type) {
    if (editor_->dependencies_.toast_manager) {
      editor_->dependencies_.toast_manager->Show(message, type);
    }
  };

  gui::CanvasMenuItem copy_item;
  copy_item.label =
      absl::StrFormat("%s (%d)", tr("Copy Tile16 definitions"), count);
  copy_item.icon = ICON_MD_CONTENT_COPY;
  const bool can_copy = !project_terrain && first >= 0;
  copy_item.enabled_condition = [can_copy]() {
    return can_copy;
  };
  if (project_terrain) {
    copy_item.tooltip =
        tr("This map uses project terrain. Tile16 definition copy works on ROM "
           "maps.");
  }
  copy_item.callback = [this, first, count, show_toast]() {
    const auto status =
        editor_->tile16_editor_.CopyTile16RangeToClipboard(first, count);
    if (!status.ok()) {
      show_toast(std::string(status.message()), ToastType::kError);
      return;
    }
    show_toast(absl::StrFormat("Copied %d Tile16 definitions ($%03X-$%03X)",
                               count, first, first + count - 1),
               ToastType::kSuccess);
  };
  editor_->blockset_canvas_.AddContextMenuItem(std::move(copy_item));

  const auto& clipboard = editor_->tile16_editor_.range_clipboard();
  const int paste_count = clipboard.count();
  std::string blocker =
      clipboard.has_data() ? Tile16RangePasteBlocker(
                                 paste_count, first,
                                 selector->has_display_order(), project_terrain)
                           : std::string(tr("Copy Tile16 definitions first."));
  gui::CanvasMenuItem paste_item;
  paste_item.label =
      clipboard.has_data()
          ? absl::StrFormat("%s (%d, $%03X-$%03X)",
                            tr("Paste Tile16 definitions"), paste_count, first,
                            first + paste_count - 1)
          : std::string(tr("Paste Tile16 definitions"));
  paste_item.icon = ICON_MD_CONTENT_PASTE;
  const bool can_paste = blocker.empty() && first >= 0;
  paste_item.enabled_condition = [can_paste]() {
    return can_paste;
  };
  paste_item.tooltip =
      can_paste ? std::string(tr("Overwrites these definitions, so every "
                                 "placement of them changes. One Undo "
                                 "restores all of them."))
                : blocker;
  paste_item.callback = [this, first, paste_count, show_toast]() {
    int placements = 0;
    for (int id = first; id < first + paste_count; ++id) {
      placements += tile16_usage_count(id);
    }
    const auto status = editor_->tile16_editor_.PasteTile16RangeAt(first);
    if (!status.ok()) {
      show_toast(std::string(status.message()), ToastType::kError);
      return;
    }
    tile16_order_dirty_ = true;
    show_toast(
        absl::StrFormat("Pasted %d Tile16 over $%03X-$%03X (placed %d times "
                        "in %s). Undo restores all %d.",
                        paste_count, first, first + paste_count - 1, placements,
                        Tile16ScopeLabel(), paste_count),
        ToastType::kSuccess);
  };
  editor_->blockset_canvas_.AddContextMenuItem(std::move(paste_item));
}

std::string Tile16RangePasteBlocker(int count, int dest, bool has_display_order,
                                    bool project_terrain) {
  if (project_terrain) {
    return tr(
        "This map uses project terrain. Tile16 definition paste works "
        "on ROM maps.");
  }
  if (has_display_order && count > 1) {
    return tr(
        "Paste writes consecutive Tile IDs. Sort by Tile ID and show "
        "unused tiles to use it.");
  }
  if (count <= 0 || dest < 0) {
    return tr("Select a destination Tile16.");
  }
  if (dest + count > zelda3::kNumTile16Individual) {
    return absl::StrFormat("%s ($%03X)",
                           tr("The pasted range runs past the last Tile16"),
                           zelda3::kNumTile16Individual - 1);
  }
  return {};
}

std::string OverworldCanvasRenderer::Tile16ScopeLabel() const {
  switch (tile16_scope_) {
    case Tile16UsageScope::kAllMaps:
      return tr("All maps");
    case Tile16UsageScope::kCurrentWorld:
      switch (editor_->current_world_) {
        case 1:
          return tr("Dark World");
        case 2:
          return tr("Special World");
        default:
          return tr("Light World");
      }
    case Tile16UsageScope::kCurrentArea:
      return tr("This area");
    case Tile16UsageScope::kArea:
      return absl::StrFormat("%02X %s", tile16_scope_area_,
                             zelda3::GetOverworldMapLabel(tile16_scope_area_));
  }
  return {};
}

std::vector<int> OverworldCanvasRenderer::Tile16ScopeMaps() const {
  const auto& maps = editor_->overworld_.overworld_maps();
  const int map_count = static_cast<int>(maps.size());
  std::vector<int> map_ids;
  map_ids.reserve(map_count);
  switch (tile16_scope_) {
    case Tile16UsageScope::kAllMaps:
      for (int id = 0; id < map_count; ++id) {
        map_ids.push_back(id);
      }
      break;
    case Tile16UsageScope::kCurrentWorld:
      for (int id = 0; id < map_count; ++id) {
        if (OverworldCanvasLayout::ClampedWorldForMap(id) ==
            editor_->current_world_) {
          map_ids.push_back(id);
        }
      }
      break;
    case Tile16UsageScope::kCurrentArea:
    case Tile16UsageScope::kArea: {
      const int anchor = tile16_scope_ == Tile16UsageScope::kArea
                             ? tile16_scope_area_
                             : editor_->current_map_;
      if (anchor < 0 || anchor >= map_count) {
        break;
      }
      const int parent = maps[anchor].parent();
      for (int id = 0; id < map_count; ++id) {
        if (maps[id].parent() == parent) {
          map_ids.push_back(id);
        }
      }
      break;
    }
  }
  return map_ids;
}

void OverworldCanvasRenderer::RefreshTile16Order(bool grid_hovered) {
  constexpr double kRecountIntervalSeconds = 0.5;
  auto* selector = editor_->blockset_selector_.get();
  if (selector == nullptr) {
    return;
  }
  if (!editor_->overworld_.is_loaded()) {
    ResetTile16Order();
    return;
  }

  const auto* current_map =
      editor_->overworld_.overworld_map(editor_->current_map_);
  const int current_parent =
      current_map != nullptr ? current_map->parent() : editor_->current_map_;
  const bool inputs_changed =
      (tile16_scope_ == Tile16UsageScope::kCurrentWorld &&
       editor_->current_world_ != tile16_counted_world_) ||
      (tile16_scope_ == Tile16UsageScope::kCurrentArea &&
       current_parent != tile16_counted_parent_);
  const double now = ImGui::GetTime();
  const bool busy = grid_hovered || ImGui::IsAnyMouseDown();
  const bool stale = now - tile16_last_count_time_ >= kRecountIntervalSeconds;
  if (!tile16_order_dirty_ && (busy || (!inputs_changed && !stale))) {
    return;
  }

  std::vector<int> counts(zelda3::kNumTile16Individual, 0);
  for (const int map_id : Tile16ScopeMaps()) {
    if (map_id <
        static_cast<int>(editor_->overworld_.overworld_maps().size())) {
      zelda3::AccumulateTile16MapUsage(
          editor_->overworld_.GetMapTiles(
              OverworldCanvasLayout::ClampedWorldForMap(map_id)),
          OverworldCanvasLayout::LocalIndex(map_id), &counts);
    }
  }
  if (editor_->lyra_canvas_.loaded() && editor_->lyra_canvas_.document()) {
    const auto* doc = editor_->lyra_canvas_.document();
    const auto& lyra_maps = doc->maps();
    for (int m = 0; m < core::LyraTerrainDocument::kMapCount; ++m) {
      const int map_id = core::LyraTerrainDocument::kFirstMap + m;
      bool include_map = false;
      if (tile16_scope_ == Tile16UsageScope::kAllMaps) {
        include_map = true;
      } else if (tile16_scope_ == Tile16UsageScope::kCurrentWorld &&
                 editor_->current_world_ == 2) {
        include_map = true;
      } else if (tile16_scope_ == Tile16UsageScope::kCurrentArea &&
                 editor_->current_map_ == map_id) {
        include_map = true;
      } else if (tile16_scope_ == Tile16UsageScope::kArea &&
                 tile16_scope_area_ == map_id) {
        include_map = true;
      }
      if (include_map) {
        for (uint16_t tile : lyra_maps[m]) {
          if (tile < counts.size()) {
            counts[tile]++;
          }
        }
      }
    }
  }
  tile16_last_count_time_ = now;
  tile16_counted_world_ = editor_->current_world_;
  tile16_counted_parent_ = current_parent;
  const bool order_changed =
      tile16_order_dirty_ || counts != tile16_usage_counts_;
  tile16_usage_counts_ = std::move(counts);
  tile16_order_dirty_ = false;
  if (!order_changed) {
    return;
  }

  if (tile16_sort_ == zelda3::Tile16SortMode::kTileId && !tile16_hide_unused_) {
    selector->ClearDisplayOrder();
  } else {
    selector->SetDisplayOrder(zelda3::BuildTile16DisplayOrder(
        tile16_usage_counts_, tile16_sort_, tile16_hide_unused_));
  }
}

void OverworldCanvasRenderer::ResetTile16Order() {
  if (editor_->blockset_selector_) {
    editor_->blockset_selector_->ClearDisplayOrder();
  }
  tile16_usage_counts_.clear();
  tile16_order_dirty_ = true;
}

void OverworldCanvasRenderer::DrawTile16ViewControls() {
  constexpr const char* kSortLabels[] = {"Most used", "Tile ID"};
  int sort = tile16_sort_ == zelda3::Tile16SortMode::kUsage ? 0 : 1;
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%s", tr("Sort"));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(gui::ScaledSize(96.0f, 0.0f).x);
  if (ImGui::Combo("##Tile16Sort", &sort, kSortLabels,
                   IM_ARRAYSIZE(kSortLabels))) {
    tile16_sort_ = sort == 0 ? zelda3::Tile16SortMode::kUsage
                             : zelda3::Tile16SortMode::kTileId;
    tile16_order_dirty_ = true;
  }
  gui::AutoRegisterLastItem("combo", "tile16_sort");

  ImGui::SameLine();
  std::string scope_label = Tile16ScopeLabel();
  constexpr size_t kMaxScopeButtonChars = 18;
  if (scope_label.size() > kMaxScopeButtonChars) {
    scope_label = scope_label.substr(0, kMaxScopeButtonChars - 3) + "...";
  }
  const std::string button_label =
      absl::StrFormat("%s %s%s###Tile16UsageScopeButton", ICON_MD_FILTER_LIST,
                      scope_label, tile16_hide_unused_ ? "*" : "");
  if (ImGui::Button(button_label.c_str())) {
    ImGui::OpenPopup("Tile16UsageScope");
  }
  gui::AutoRegisterLastItem("button", "tile16_usage_scope");
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr("Maps that usage counts and \"Hide unused\" use"));
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f),
                                      ImVec2(380.0f, 480.0f));
  if (ImGui::BeginPopup("Tile16UsageScope")) {
    ImGui::TextDisabled("%s", tr("Count placements in"));
    const auto scope_radio = [this](const char* label, Tile16UsageScope scope) {
      if (ImGui::RadioButton(label, tile16_scope_ == scope)) {
        tile16_scope_ = scope;
        tile16_order_dirty_ = true;
      }
    };
    scope_radio(tr("All maps"), Tile16UsageScope::kAllMaps);
    scope_radio(tr("Current world"), Tile16UsageScope::kCurrentWorld);
    scope_radio(tr("Current area"), Tile16UsageScope::kCurrentArea);
    if (ImGui::Checkbox(tr("Hide unused tiles"), &tile16_hide_unused_)) {
      tile16_order_dirty_ = true;
    }
    ImGui::Separator();
    DrawTile16AreaPicker();
    ImGui::EndPopup();
  }
}

void OverworldCanvasRenderer::DrawTile16AreaPicker() {
  ImGui::TextDisabled("%s", tr("Area"));
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##Tile16AreaSearch",
                           tr("Search map names or IDs..."),
                           tile16_area_search_, sizeof(tile16_area_search_));
  const std::string needle = absl::AsciiStrToLower(tile16_area_search_);

  if (ImGui::BeginChild("##Tile16AreaList",
                        ImVec2(0.0f, gui::ScaledSize(0.0f, 200.0f).y),
                        ImGuiChildFlags_Borders)) {
    const auto& maps = editor_->overworld_.overworld_maps();
    for (int id = 0; id < static_cast<int>(maps.size()); ++id) {
      if (maps[id].parent() != id) {
        continue;  // One row per area; large areas list only their parent.
      }
      const char* world_tag = id < 0x40 ? "LW" : (id < 0x80 ? "DW" : "SW");
      const std::string row = absl::StrFormat("%s %02X  %s", world_tag, id,
                                              zelda3::GetOverworldMapLabel(id));
      if (!needle.empty() &&
          absl::AsciiStrToLower(row).find(needle) == std::string::npos) {
        continue;
      }
      const bool selected =
          tile16_scope_ == Tile16UsageScope::kArea && tile16_scope_area_ == id;
      if (ImGui::Selectable(row.c_str(), selected)) {
        tile16_scope_ = Tile16UsageScope::kArea;
        tile16_scope_area_ = id;
        tile16_order_dirty_ = true;
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::EndChild();
}

void OverworldCanvasRenderer::DrawTile8Selector() {
  // Configure canvas frame options for graphics bin
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = kGraphicsBinCanvasSize;
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 16.0f;  // Tile8 grid
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  auto canvas_rt = gui::BeginCanvas(editor_->graphics_bin_canvas_, frame_opts);

  if (editor_->all_gfx_loaded_) {
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
          ImVec2(canvas_rt.canvas_p0.x + 0x100, canvas_rt.canvas_p0.y + offset),
          ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));
      key++;
    }
  }

  gui::EndCanvas(editor_->graphics_bin_canvas_, canvas_rt, frame_opts);
}

absl::Status OverworldCanvasRenderer::DrawAreaGraphics() {
  const int graphics_map = editor_->IsProjectTerrainMap(editor_->current_map_)
                               ? 0x3A
                               : editor_->current_map_;
  const bool project_graphics =
      editor_->IsProjectTerrainMap(editor_->current_map_);
  if (project_graphics) {
    RETURN_IF_ERROR(editor_->lyra_canvas_.PrepareMapAssets(
        editor_->overworld_, editor_->game_state_));
  }
  if (!project_graphics && editor_->overworld_.is_loaded()) {
    // Always ensure current map graphics are loaded
    if (!editor_->current_graphics_set_.contains(graphics_map)) {
      auto status = editor_->overworld_.EnsureMapBuilt(graphics_map);
      if (!status.ok()) {
        return status;
      }
      const auto* map = editor_->overworld_.overworld_map(graphics_map);
      if (!map) {
        return absl::FailedPreconditionError(
            "Current overworld map not loaded");
      }
      editor_->palette_ = map->current_palette();
      auto bmp = std::make_unique<gfx::Bitmap>();
      bmp->Create(0x80, kOverworldMapSize, 0x08, map->current_graphics());
      bmp->SetPalette(editor_->palette_);
      editor_->current_graphics_set_[graphics_map] = std::move(bmp);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          editor_->current_graphics_set_[graphics_map].get());
    }
  }

  DrawSheetScaleControl("##AreaGraphicsScale", &area_graphics_scale_mode_);
  gfx::Bitmap* sheet = nullptr;
  if (project_graphics) {
    sheet = editor_->lyra_canvas_.MapGraphics(editor_->current_map_);
  } else if (editor_->current_graphics_set_.contains(graphics_map)) {
    sheet = editor_->current_graphics_set_[graphics_map].get();
  }
  if (sheet == nullptr) {
    ImGui::TextDisabled("%s", tr("Area graphics are not loaded"));
    return absl::OkStatus();
  }
  DrawAreaGraphicsSheet(*sheet, 0.0f);
  return absl::OkStatus();
}

void OverworldCanvasRenderer::DrawAreaGraphicsSheet(gfx::Bitmap& sheet,
                                                    float grid_height) {
  if (!area_graphics_selector_) {
    auto config = AreaGraphicsSelectorConfig();
    config.highlight_color = AgentUI::GetTheme().selection_primary;
    area_graphics_selector_ = std::make_unique<gui::TileSelectorWidget>(
        "OwAreaGraphicsSelector", config);
    area_graphics_selector_->AttachCanvas(&editor_->current_gfx_canvas_);
    // The canvas scale/grid items do not apply to a fixed tile grid.
    editor_->current_gfx_canvas_.SetShowBuiltinContextMenu(false);
    area_graphics_selector_->SetHoverDetailProvider([this](int tile8_id) {
      return absl::StrFormat("Type: %s",
                             Tile8CollisionLabel(tile8_id, area_tile8_types_));
    });
  }
  auto* selector = area_graphics_selector_.get();
  const auto config = AreaGraphicsSelectorConfig();
  if (sheet.width() == config.tiles_per_row * config.tile_size &&
      sheet.height() > 0) {
    selector->SetTileCount(config.tiles_per_row *
                           (sheet.height() / config.tile_size));
  }

  // Project terrain and unsupported layouts have no collision table; zeros
  // there would read as real "Normal" types.
  area_tile8_types_.clear();
  if (!editor_->IsProjectTerrainMap(editor_->current_map_)) {
    if (auto types =
            editor_->overworld_.GetTile8CollisionTypes(editor_->current_map_);
        types.ok()) {
      area_tile8_types_.assign(types->begin(), types->end());
    }
  }

  editor_->current_gfx_canvas_.ClearContextMenuItems();
  editor_->current_gfx_canvas_.AddContextMenuItem(
      selector->CopyTileIdMenuItem());

  // Fit leaves room for the vertical scrollbar and both draw offsets, and the
  // selector snaps the scale to whole-pixel cells, so the grid never needs a
  // horizontal scrollbar at Fit. Fixed zooms scroll both ways.
  const gui::AdaptiveSheetLayout layout = gui::ResolveAdaptiveSheetLayout(
      ImGui::GetContentRegionAvail().x, config.tiles_per_row * config.tile_size,
      (selector->GetMaxTileId() + 1) / config.tiles_per_row * config.tile_size,
      area_graphics_scale_mode_, 0.25f, 4.0f,
      gui::TileSelectorWidget::CurrentScrollbarSize() +
          2.0f * config.draw_offset.x);
  selector->SetDisplayScale(layout.display_scale);

  const int selected = selector->GetSelectedTileID();
  ImGui::Text("Tile8 $%03X; type: %s", selected,
              Tile8CollisionLabel(selected, area_tile8_types_).c_str());

  // Padding is read at BeginChild; pop it right away so the grid's tooltip
  // and context menu keep normal popup padding.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  const bool grid_visible = ImGui::BeginChild(
      "##AreaGraphicsGrid", ImVec2(0.0f, grid_height), ImGuiChildFlags_None,
      ImGuiWindowFlags_AlwaysVerticalScrollbar |
          ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::PopStyleVar();
  if (grid_visible) {
    const ImVec2 grid_size = selector->GetGridContentSize();
    const float canvas_scale = editor_->current_gfx_canvas_.global_scale();
    editor_->current_gfx_canvas_.SetCanvasSize(
        ImVec2(grid_size.x / canvas_scale, grid_size.y / canvas_scale));
    selector->Render(sheet, sheet.is_active());
  }
  ImGui::EndChild();
}

void OverworldCanvasRenderer::DrawV3Settings() {
  // Lightweight v3 controls for map size and feature visibility.
  ImGui::TextWrapped(tr("ZSCustomOverworld v3 settings"));
  ImGui::Separator();

  if (!editor_->rom_ || !editor_->rom_->is_loaded()) {
    gui::CenterText("No ROM loaded");
    return;
  }

  const uint8_t asm_version =
      (*editor_->rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  ImGui::Text(tr("ASM Version: 0x%02X"), asm_version);

  if (asm_version == 0x00 || asm_version == 0xFF || asm_version < 0x03) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                       tr("v3 controls require ZSCustomOverworld v3+"));
    ImGui::TextDisabled(tr("Apply the v3 patch to enable map-size editing."));
    return;
  }

  auto* map = editor_->overworld_.overworld_map(editor_->current_map_);
  if (!map) {
    ImGui::TextDisabled(tr("Current map is unavailable."));
    return;
  }

  ImGui::Spacing();
  ImGui::Text(tr("Current map: 0x%02X"), editor_->current_map_);
  ImGui::Text(tr("Parent map:  0x%02X"), map->parent());

  static int selected_area_size = 0;
  selected_area_size = static_cast<int>(map->area_size());
  const char* area_size_labels[] = {"Small", "Wide", "Tall", "Large"};
  ImGui::SetNextItemWidth(220.0f);
  ImGui::Combo(tr("Area Size"), &selected_area_size, area_size_labels,
               IM_ARRAYSIZE(area_size_labels));

  if (ImGui::Button(ICON_MD_SAVE " Apply Area Size")) {
    auto status = editor_->overworld_.ConfigureMultiAreaMap(
        map->parent(), static_cast<zelda3::AreaSizeEnum>(selected_area_size));
    if (status.ok()) {
      editor_->RefreshOverworldMap();
      editor_->RefreshSiblingMapGraphics(editor_->current_map_, true);
    }
  }

  ImGui::Spacing();
  ImGui::TextDisabled(
      tr("Area-size changes update sibling map relationships for this parent "
         "map."));
}

void OverworldCanvasRenderer::FilterHoveredEntityByMode() {
  auto* entity = editor_->entity_renderer_->hovered_entity();
  if (!entity) {
    return;
  }
  using Type = zelda3::GameEntity::EntityType;
  bool allowed = true;
  switch (editor_->entity_edit_mode_) {
    case EntityEditMode::ENTRANCES:
      allowed = entity->entity_type_ == Type::kEntrance;
      break;
    case EntityEditMode::EXITS:
      allowed = entity->entity_type_ == Type::kExit;
      break;
    case EntityEditMode::ITEMS:
      allowed = entity->entity_type_ == Type::kItem;
      break;
    case EntityEditMode::SPRITES:
      allowed = entity->entity_type_ == Type::kSprite;
      break;
    default:
      break;  // NONE / TRANSPORTS / MUSIC: every entity stays interactive.
  }
  if (!allowed) {
    editor_->entity_renderer_->ResetHoveredEntity();
  }
}

void OverworldCanvasRenderer::DrawEntityHoverTooltip() {
  if (!editor_->entity_renderer_ ||
      !editor_->ow_map_canvas_.IsMouseHovering()) {
    return;
  }
  const auto* entity = editor_->entity_renderer_->hovered_entity();
  if (!entity) {
    return;
  }
  if (ImGui::BeginTooltip()) {
    switch (entity->entity_type_) {
      case zelda3::GameEntity::EntityType::kEntrance: {
        const auto* entrance =
            static_cast<const zelda3::OverworldEntrance*>(entity);
        ImGui::Text("%s", entrance->is_hole_ ? ICON_MD_CYCLONE " Hole"
                                             : ICON_MD_DOOR_FRONT " Entrance");
        ImGui::Text("Entrance ID: 0x%02X | Map: 0x%02X", entrance->entrance_id_,
                    entrance->map_id_);
        ImGui::TextDisabled("Pos: (%d, %d) · Game: (%d, %d)", entrance->x_,
                            entrance->y_, entrance->game_x_, entrance->game_y_);
        break;
      }
      case zelda3::GameEntity::EntityType::kExit: {
        const auto* exit = static_cast<const zelda3::OverworldExit*>(entity);
        ImGui::Text("%s", ICON_MD_DOOR_BACK " Exit");
        ImGui::Text("Room ID: 0x%02X | Map: 0x%02X", exit->room_id_,
                    exit->map_id_);
        ImGui::TextDisabled("Pos: (%d, %d)", exit->x_, exit->y_);
        break;
      }
      case zelda3::GameEntity::EntityType::kItem: {
        const auto* item = static_cast<const zelda3::OverworldItem*>(entity);
        std::string item_name = (item->id_ < zelda3::kSecretItemNames.size())
                                    ? zelda3::kSecretItemNames[item->id_]
                                    : absl::StrFormat("0x%02X", item->id_);
        ImGui::Text("%s %s", ICON_MD_GRASS, item_name.c_str());
        ImGui::Text("Item ID: 0x%02X | Map: 0x%02X", item->id_,
                    item->room_map_id_);
        ImGui::TextDisabled("Pos: (%d, %d) · Game: (%d, %d)", item->x_,
                            item->y_, item->game_x_, item->game_y_);
        break;
      }
      case zelda3::GameEntity::EntityType::kSprite: {
        const auto* sprite = static_cast<const zelda3::Sprite*>(entity);
        ImGui::Text("%s %s", ICON_MD_PEST_CONTROL_RODENT,
                    sprite->name().c_str());
        ImGui::Text("Sprite ID: 0x%02X | Map: 0x%02X", sprite->id(),
                    sprite->map_id());
        ImGui::TextDisabled("Pos: (%d, %d) · Game: (%d, %d)", sprite->x_,
                            sprite->y_, sprite->game_x_, sprite->game_y_);
        break;
      }
      default:
        break;
    }
    ImGui::EndTooltip();
  }
}

void OverworldCanvasRenderer::DrawMapProperties() {
  if (editor_->IsProjectTerrainMap(editor_->current_map_)) {
    if (ImGui::BeginChild("OverworldSidebar", ImVec2(0, 0), false))
      editor_->lyra_canvas_.DrawProperties(
          editor_->current_map_, editor_->game_state_, editor_->overworld_,
          editor_->undo_manager_);
    ImGui::EndChild();
    if (editor_->lyra_canvas_.entity_placement_active())
      editor_->show_entities_ = true;
    return;
  }
  // The Background Color and Visual Effects buttons open the same floating
  // editors the canvas context menu uses (drawn in OverworldEditor::Update).
  if (editor_->sidebar_) {
    // Update live hover only while the canvas owns the cursor. Leaving the
    // canvas must not clear the last valid selection (sidebar Pin needs it).
    if (editor_->tile_painting_ && editor_->overworld_.is_loaded() &&
        editor_->ow_map_canvas_.IsMouseHovering()) {
      const float scale = editor_->ow_map_canvas_.global_scale() > 0.0f
                              ? editor_->ow_map_canvas_.global_scale()
                              : 1.0f;
      const ImVec2 mouse = editor_->ow_map_canvas_.hover_mouse_pos();
      const ImVec2 canvas_pt(mouse.x / scale, mouse.y / scale);
      const auto wp = editor_->GetCanvasLayout().WorldLocalFromCanvas(
          canvas_pt, editor_->current_world_);
      absl::StatusOr<cli::overworld::AreaTileLocation> location =
          absl::OutOfRangeError("Outside overworld canvas");
      if (wp) {
        const int tx = static_cast<int>(wp->local_position.x / kTile16Size);
        const int ty = static_cast<int>(wp->local_position.y / kTile16Size);
        const int map_id =
            OverworldCanvasLayout::MapIdFromLocal(wp->world, tx / 32, ty / 32);
        location = cli::overworld::LocateScreenTile(editor_->overworld_, map_id,
                                                    tx % 32, ty % 32);
      }
      if (location.ok()) {
        editor_->sidebar_->SetTile16ReferenceHover(
            location->map_id, location->area_x, location->area_y);
      } else {
        editor_->sidebar_->ClearTile16ReferenceHover();
      }
    } else {
      editor_->sidebar_->ClearTile16ReferenceHover();
    }
    editor_->sidebar_->Draw(editor_->current_world_, editor_->current_map_,
                            editor_->current_map_lock_, editor_->game_state_,
                            editor_->show_custom_bg_color_editor_,
                            editor_->show_overlay_editor_,
                            editor_->dependencies_.project);
  }
}

void OverworldCanvasRenderer::DrawOverworldProperties() {
  static bool init_properties = false;

  if (!init_properties) {
    for (int i = 0; i < 0x40; i++) {
      std::string area_graphics_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->area_graphics());
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_AREA_GFX)
          ->push_back(area_graphics_str);

      area_graphics_str = absl::StrFormat(
          "%02hX",
          editor_->overworld_.overworld_map(i + 0x40)->area_graphics());
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_AREA_GFX)
          ->push_back(area_graphics_str);

      std::string area_palette_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->area_palette());
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_AREA_PAL)
          ->push_back(area_palette_str);

      area_palette_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i + 0x40)->area_palette());
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_AREA_PAL)
          ->push_back(area_palette_str);

      std::string sprite_gfx_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->sprite_graphics(1));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->sprite_graphics(2));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX",
          editor_->overworld_.overworld_map(i + 0x40)->sprite_graphics(1));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_SPR_GFX_PART1)
          ->push_back(sprite_gfx_str);

      sprite_gfx_str = absl::StrFormat(
          "%02hX",
          editor_->overworld_.overworld_map(i + 0x40)->sprite_graphics(2));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_SPR_GFX_PART2)
          ->push_back(sprite_gfx_str);

      std::string sprite_palette_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->sprite_palette(1));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX", editor_->overworld_.overworld_map(i)->sprite_palette(2));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::LW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX",
          editor_->overworld_.overworld_map(i + 0x40)->sprite_palette(1));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_SPR_PAL_PART1)
          ->push_back(sprite_palette_str);

      sprite_palette_str = absl::StrFormat(
          "%02hX",
          editor_->overworld_.overworld_map(i + 0x40)->sprite_palette(2));
      editor_->properties_canvas_
          .mutable_labels(OverworldEditor::OverworldProperty::DW_SPR_PAL_PART2)
          ->push_back(sprite_palette_str);
    }
    init_properties = true;
  }

  ImGui::Text(tr("Area Gfx LW/DW"));
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::LW_AREA_GFX);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  ImGui::Text(tr("Sprite Gfx LW/DW"));
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32,
      OverworldEditor::OverworldProperty::LW_SPR_GFX_PART1);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32,
      OverworldEditor::OverworldProperty::DW_SPR_GFX_PART1);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32,
      OverworldEditor::OverworldProperty::LW_SPR_GFX_PART2);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32,
      OverworldEditor::OverworldProperty::DW_SPR_GFX_PART2);
  ImGui::Separator();

  ImGui::Text(tr("Area Pal LW/DW"));
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::LW_AREA_PAL);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::DW_AREA_PAL);

  static bool show_gfx_group = false;
  ImGui::Checkbox(tr("Show Gfx Group Editor"), &show_gfx_group);
  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    editor_->status_ = editor_->gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }
}

}  // namespace yaze::editor
