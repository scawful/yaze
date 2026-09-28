// Related header
#include "app/editor/overworld/canvas/overworld_canvas_renderer.h"
#include "util/i18n/tr.h"

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

// C++ standard library headers
#include <memory>
#include <string>

// Third-party library headers
#include "absl/status/status.h"
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
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "rom/rom.h"
#include "util/log.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

namespace {

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

}  // namespace

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

  // Toolbar: world, map, tools, entity focus, view and panels. Per-map
  // fields live in the Map Properties panel only.
  if (editor_->rom_ != nullptr && editor_->rom_->is_loaded() &&
      editor_->overworld_.is_loaded() && editor_->toolbar_) {
    editor_->toolbar_->shortcuts = editor_->dependencies_.shortcut_manager;
    editor_->toolbar_->Draw(
        editor_->current_world_, editor_->current_map_,
        editor_->current_map_lock_, editor_->current_mode,
        editor_->entity_edit_mode_, editor_->dependencies_.window_manager,
        editor_->rom_, &editor_->overworld_, editor_->dependencies_.project,
        editor_->game_state_);
    editor_->NormalizeCurrentSelectionState();
  }

  // ==========================================================================
  // PHASE 3: Modern BeginCanvas/EndCanvas Pattern
  // ==========================================================================
  // Menu actions are captured by PrepareContextMenu only when it opens.
  // Keep rendering an open menu even after entity hover changes. Paint modes
  // keep right-click for sampling; Shift+right-click opens the menu there
  // (PrepareContextMenu vetoes a plain right-click).
  const bool show_context_menu = true;

  // Configure canvas frame options
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = kOverworldCanvasSize;
  frame_opts.draw_grid = true;
  frame_opts.draw_context_menu = show_context_menu;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
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
    // Draw the 64 overworld map bitmaps
    DrawOverworldMaps();

    // Draw all entities using the new CanvasRuntime-based methods
    if (editor_->entity_renderer_ && editor_->show_entities_) {
      editor_->entity_renderer_->DrawExits(canvas_rt, editor_->current_world_);
      editor_->entity_renderer_->DrawEntrances(canvas_rt,
                                               editor_->current_world_);
      editor_->entity_renderer_->DrawItems(canvas_rt, editor_->current_world_);
      editor_->entity_renderer_->DrawSprites(canvas_rt, editor_->current_world_,
                                             editor_->game_state_);
      FilterHoveredEntityByMode();
    } else if (editor_->entity_renderer_) {
      // Hidden entities can't be hovered, dragged or right-clicked.
      editor_->entity_renderer_->ResetHoveredEntity();
    }

    // Area subscreen overlays are composited per map in DrawOverworldMaps()
    // (toggle: show_overlay_preview_).

    // Always refresh hover preview: when the canvas is not hovered this clears
    // hovered_map_ so the status bar falls back to the selected map.
    editor_->status_ = editor_->CheckForCurrentMap();
    if (canvas_rt.hovered) {
      editor_->HandleMapInteraction();
    }

    if (editor_->current_mode == EditingMode::DRAW_TILE ||
        editor_->current_mode == EditingMode::FILL_TILE) {
      editor_->CheckForOverworldEdits();
    }

    // --- BEGIN ENTITY DRAG/DROP LOGIC ---
    if (editor_->current_mode == EditingMode::MOUSE &&
        editor_->entity_renderer_) {
      auto hovered_entity = editor_->entity_renderer_->hovered_entity();

      // 1. Initiate drag
      if (!editor_->is_dragging_entity_ && hovered_entity &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        editor_->dragged_entity_ = hovered_entity;
        editor_->is_dragging_entity_ = true;
        editor_->drag_item_snapshot_.reset();
        if (hovered_entity->entity_type_ ==
            zelda3::GameEntity::EntityType::kItem) {
          editor_->SelectItemByIdentity(
              *static_cast<zelda3::OverworldItem*>(hovered_entity));
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
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImVec2 mouse_delta = ImGui::GetIO().MouseDelta;
        float scale = canvas_rt.scale;
        if (scale > 0.0f) {
          editor_->dragged_entity_->x_ += mouse_delta.x / scale;
          editor_->dragged_entity_->y_ += mouse_delta.y / scale;
        }
      }

      // 3. End drag
      if (editor_->is_dragging_entity_ &&
          ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (editor_->dragged_entity_) {
          float end_scale = canvas_rt.scale;
          MoveEntityOnGrid(editor_->dragged_entity_, canvas_rt.canvas_p0,
                           canvas_rt.scrolling,
                           editor_->dragged_entity_free_movement_, end_scale);
          // Pass overworld context for proper area size detection
          editor_->dragged_entity_->UpdateMapProperties(
              editor_->dragged_entity_->map_id_, &editor_->overworld_);
          if (editor_->drag_item_snapshot_) {
            // No-op if the item ended where it started.
            editor_->PushItemUndoAction(
                std::move(*editor_->drag_item_snapshot_),
                "Move overworld item");
          }
          // TODO(overworld): entrances, exits and sprites have no undo action
          // yet; their drags only mark the ROM dirty.
          editor_->rom_->set_dirty(true);
        }
        editor_->drag_item_snapshot_.reset();
        editor_->is_dragging_entity_ = false;
        editor_->dragged_entity_ = nullptr;
        editor_->dragged_entity_free_movement_ = false;
      }
    }
    // --- END ENTITY DRAG/DROP LOGIC ---

    // --- TILE DROP TARGET ---
    // Accept tile drops from the blockset selector onto the map canvas.
    gui::TileDragPayload tile_drop;
    if (gui::AcceptTileDrop(&tile_drop)) {
      editor_->RequestTile16Selection(tile_drop.tile_id);
      if (editor_->current_mode != EditingMode::DRAW_TILE) {
        editor_->current_mode = EditingMode::DRAW_TILE;
        editor_->ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
      }
    }
  }

  // End canvas frame - draws grid/overlay based on frame_opts
  gui::EndCanvas(editor_->ow_map_canvas_, canvas_rt, frame_opts);
  ImGui::EndChild();
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
        entry.bitmap.Create(kOverworldMapSize, kOverworldMapSize, 0x80,
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
  // frame: building all 64 maps of a world in the first frame stalled the UI.
  // Off-screen maps are built when scrolled into view; visible ones that are
  // over the budget show the loading placeholder for a frame or two.
  constexpr int kMaxMapBuildsPerFrame = 4;
  int builds_this_frame = 0;
  const ImVec2 clip_min = ImGui::GetWindowDrawList()->GetClipRectMin();
  const ImVec2 clip_max = ImGui::GetWindowDrawList()->GetClipRectMax();
  const ImVec2 canvas_origin(editor_->ow_map_canvas_.zero_point().x +
                                 editor_->ow_map_canvas_.scrolling().x,
                             editor_->ow_map_canvas_.zero_point().y +
                                 editor_->ow_map_canvas_.scrolling().y);
  const float map_extent = kOverworldMapSize * scale;

  for (int i = 0; i < 0x40; i++) {
    const int xx = i % 8;
    const int yy = i / 8;
    int world_index = i + (editor_->current_world_ * 0x40);

    // Bounds checking to prevent crashes
    if (world_index < 0 ||
        world_index >= static_cast<int>(editor_->maps_bmp_.size())) {
      continue;  // Skip invalid map index
    }

    // Apply scale to positions for proper zoom support
    int map_x = static_cast<int>(xx * kOverworldMapSize * scale);
    int map_y = static_cast<int>(yy * kOverworldMapSize * scale);

    const float left = canvas_origin.x + map_x;
    const float top = canvas_origin.y + map_y;
    if (left >= clip_max.x || top >= clip_max.y ||
        left + map_extent <= clip_min.x || top + map_extent <= clip_min.y) {
      continue;  // Not visible
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
    // The current_map_ check was causing crashes when hovering over unbuilt maps
    // because the bitmap would be drawn before EnsureMapBuilt() was called
    bool can_draw = editor_->maps_bmp_[world_index].texture() &&
                    editor_->maps_bmp_[world_index].is_active();

    if (can_draw) {
      DrawMapWithAreaLayers(world_index, map_x, map_y, scale);
    } else {
      // Draw a placeholder for maps that haven't loaded yet
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 canvas_pos = editor_->ow_map_canvas_.zero_point();
      ImVec2 scrolling = editor_->ow_map_canvas_.scrolling();
      // Apply scrolling offset and use already-scaled map_x/map_y
      ImVec2 placeholder_pos = ImVec2(canvas_pos.x + scrolling.x + map_x,
                                      canvas_pos.y + scrolling.y + map_y);
      // Scale the placeholder size to match zoomed maps
      float scaled_size = kOverworldMapSize * scale;
      ImVec2 placeholder_size = ImVec2(scaled_size, scaled_size);

      // Modern loading indicator with theme colors
      const auto& theme = AgentUI::GetTheme();
      draw_list->AddRectFilled(
          placeholder_pos,
          ImVec2(placeholder_pos.x + placeholder_size.x,
                 placeholder_pos.y + placeholder_size.y),
          ImGui::GetColorU32(
              theme.editor_background));  // Theme-aware background

      // Animated loading spinner - scale spinner radius with zoom
      ImVec2 spinner_pos = ImVec2(placeholder_pos.x + placeholder_size.x / 2,
                                  placeholder_pos.y + placeholder_size.y / 2);

      const float spinner_radius = 8.0f * scale;
      const float rotation = static_cast<float>(ImGui::GetTime()) * 3.0f;
      const float start_angle = rotation;
      const float end_angle = rotation + IM_PI * 1.5f;

      draw_list->PathArcTo(spinner_pos, spinner_radius, start_angle, end_angle,
                           12);
      draw_list->PathStroke(ImGui::GetColorU32(theme.status_active), 0,
                            2.5f * scale);
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
  editor_->blockset_canvas_.AddContextMenuItem(
      editor_->blockset_selector_->CopyTileIdMenuItem());

  const float selector_available_width = ImGui::GetContentRegionAvail().x;
  DrawSheetScaleControl("##Tile16Scale", &tile16_scale_mode_);
  const int tile_rows =
      (zelda3::kNumTile16Individual + kTile16SelectorColumns - 1) /
      kTile16SelectorColumns;
  const gui::AdaptiveSheetLayout selector_layout =
      gui::ResolveAdaptiveSheetLayout(
          selector_available_width, 16 * kTile16SelectorColumns, 16 * tile_rows,
          tile16_scale_mode_, 0.35f, 4.0f,
          gui::TileSelectorWidget::CurrentScrollbarSize());
  editor_->blockset_selector_->SetDisplayScale(selector_layout.display_scale);
  ImGui::SameLine();
  ImGui::TextDisabled("%.2fx", selector_layout.display_scale);

  // Filter bar sits directly in the panel; only the grid scrolls. The grid
  // child fills the panel width (the panel's preferred width is grid +
  // scrollbar), has no padding or border, and never scrolls horizontally.
  if (editor_->blockset_selector_->DrawFilterBar()) {
    editor_->RequestTile16Selection(
        editor_->blockset_selector_->GetSelectedTileID());
  }

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

  gfx::Bitmap& atlas = editor_->tile16_blockset_.atlas;
  bool atlas_ready = editor_->map_blockset_loaded_ && atlas.is_active();
  auto result = editor_->blockset_selector_->Render(atlas, atlas_ready);

  if (result.selection_changed) {
    editor_->RequestTile16Selection(result.selected_tile);
    // Note: We do NOT auto-scroll here because it breaks user interaction.
    // The canvas should only scroll when explicitly requested (e.g., when
    // selecting a tile from the overworld canvas via
    // ScrollBlocksetCanvasToCurrentTile).
  }

  if (result.tile_double_clicked) {
    open_tile16_editor(result.selected_tile);
  }

  ImGui::EndChild();
  return absl::OkStatus();
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
          ImVec2(canvas_rt.canvas_p0.x + 0x100,
                 canvas_rt.canvas_p0.y + offset));
      key++;
    }
  }

  gui::EndCanvas(editor_->graphics_bin_canvas_, canvas_rt, frame_opts);
}

absl::Status OverworldCanvasRenderer::DrawAreaGraphics() {
  if (editor_->overworld_.is_loaded()) {
    // Always ensure current map graphics are loaded
    if (!editor_->current_graphics_set_.contains(editor_->current_map_)) {
      auto status = editor_->overworld_.EnsureMapBuilt(editor_->current_map_);
      if (!status.ok()) {
        return status;
      }
      const auto* map =
          editor_->overworld_.overworld_map(editor_->current_map_);
      if (!map) {
        return absl::FailedPreconditionError(
            "Current overworld map not loaded");
      }
      editor_->palette_ = map->current_palette();
      auto bmp = std::make_unique<gfx::Bitmap>();
      bmp->Create(0x80, kOverworldMapSize, 0x08, map->current_graphics());
      bmp->SetPalette(editor_->palette_);
      editor_->current_graphics_set_[editor_->current_map_] = std::move(bmp);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          editor_->current_graphics_set_[editor_->current_map_].get());
    }
  }

  const float available_width = ImGui::GetContentRegionAvail().x;
  DrawSheetScaleControl("##AreaGraphicsScale", &area_graphics_scale_mode_);
  const gui::AdaptiveSheetLayout sheet_layout = gui::ResolveAdaptiveSheetLayout(
      available_width, 0x80, kOverworldMapSize, area_graphics_scale_mode_,
      0.35f, 4.0f, ImGui::GetStyle().ScrollbarSize);
  ImGui::SameLine();
  ImGui::TextDisabled("%.2fx", sheet_layout.display_scale);

  // Configure canvas frame options for area graphics
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = ImVec2(sheet_layout.displayed_width + 4.0f,
                                  sheet_layout.displayed_height + 4.0f);
  frame_opts.draw_grid = true;
  frame_opts.grid_step = 16.0f * sheet_layout.display_scale;
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;
  frame_opts.use_child_window = false;

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar("##AreaGraphicsScrollRegion");

  auto canvas_rt = gui::BeginCanvas(editor_->current_gfx_canvas_, frame_opts);
  gui::EndPadding();

  if (editor_->current_graphics_set_.contains(editor_->current_map_) &&
      editor_->current_graphics_set_[editor_->current_map_]->is_active()) {
    editor_->current_gfx_canvas_.DrawBitmap(
        *editor_->current_graphics_set_[editor_->current_map_], 2, 2,
        sheet_layout.display_scale);
  }
  editor_->current_gfx_canvas_.DrawTileSelector(16.0f *
                                                sheet_layout.display_scale);

  gui::EndCanvas(editor_->current_gfx_canvas_, canvas_rt, frame_opts);
  ImGui::EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
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

void OverworldCanvasRenderer::DrawMapProperties() {
  // The Background Color and Visual Effects buttons open the same floating
  // editors the canvas context menu uses (drawn in OverworldEditor::Update).
  if (editor_->sidebar_) {
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
