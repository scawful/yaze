// Related header
#include "app/editor/overworld/overworld_canvas_renderer.h"

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
#include "app/editor/overworld/entity.h"
#include "app/editor/overworld/map_properties.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/overworld_entity_renderer.h"
#include "app/editor/overworld/overworld_sidebar.h"
#include "app/editor/overworld/overworld_toolbar.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/editor/overworld/ui_constants.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/tile_selector_widget.h"
#include "rom/rom.h"
#include "util/log.h"
#include "zelda3/overworld/overworld.h"

namespace yaze::editor {

OverworldCanvasRenderer::OverworldCanvasRenderer(OverworldEditor* editor)
    : editor_(editor) {}

// =============================================================================
// Main Canvas Drawing
// =============================================================================

void OverworldCanvasRenderer::DrawOverworldCanvas() {
  // Simplified map settings - compact row with popup panels for detailed
  // editing
  if (editor_->rom_->is_loaded() && editor_->overworld_.is_loaded() &&
      editor_->map_properties_system_) {
    const EditingMode old_mode = editor_->current_mode;
    bool has_selection = editor_->ow_map_canvas_.select_rect_active() &&
                         !editor_->ow_map_canvas_.selected_tiles().empty();

    // Check if scratch space has data
    bool scratch_has_data = editor_->scratch_space_.in_use;

    // Pass WorkspaceWindowManager to toolbar for panel visibility management
    editor_->toolbar_->Draw(
        editor_->current_world_, editor_->current_map_,
        editor_->current_map_lock_, editor_->current_mode,
        editor_->entity_edit_mode_, editor_->dependencies_.window_manager,
        has_selection, scratch_has_data, editor_->rom_, &editor_->overworld_);

    // Toolbar toggles don't currently update canvas usage mode.
    if (old_mode != editor_->current_mode) {
      if (editor_->current_mode == EditingMode::MOUSE) {
        editor_->ow_map_canvas_.SetUsageMode(
            gui::CanvasUsage::kEntityManipulation);
      } else {
        editor_->ow_map_canvas_.SetUsageMode(gui::CanvasUsage::kTilePainting);
      }
    }
  }

  // ==========================================================================
  // PHASE 3: Modern BeginCanvas/EndCanvas Pattern
  // ==========================================================================
  // Context menu setup MUST happen BEFORE BeginCanvas (lesson from dungeon)
  bool show_context_menu =
      (editor_->current_mode == EditingMode::MOUSE) &&
      (!editor_->entity_renderer_ ||
       editor_->entity_renderer_->hovered_entity() == nullptr);

  if (editor_->rom_->is_loaded() && editor_->overworld_.is_loaded() &&
      editor_->map_properties_system_) {
    editor_->ow_map_canvas_.ClearContextMenuItems();
    editor_->map_properties_system_->SetupCanvasContextMenu(
        editor_->ow_map_canvas_, editor_->current_map_,
        editor_->current_map_lock_, editor_->show_map_properties_panel_,
        editor_->show_custom_bg_color_editor_, editor_->show_overlay_editor_,
        static_cast<int>(editor_->current_mode), [this]() {
          if (editor_->dependencies_.window_manager) {
            const size_t session_id =
                editor_->dependencies_.window_manager->GetActiveSessionId();
            editor_->dependencies_.window_manager->OpenWindow(
                session_id, OverworldPanelIds::kMapProperties);
            editor_->dependencies_.window_manager->MarkWindowRecentlyUsed(
                OverworldPanelIds::kMapProperties);
          }
          editor_->show_map_properties_panel_ = true;
        });
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
  editor_->ow_map_canvas_.set_scrolling(ImVec2(0, 0));

  // Begin canvas frame - this handles DrawBackground + DrawContextMenu
  auto canvas_rt = gui::BeginCanvas(editor_->ow_map_canvas_, frame_opts);
  gui::EndNoPadding();

  // Handle pan via ImGui scrolling (instead of canvas internal scroll)
  editor_->HandleOverworldPan();
  editor_->HandleOverworldZoom();

  // Tile painting mode - handle tile edits and right-click tile picking
  if (editor_->current_mode == EditingMode::DRAW_TILE ||
      editor_->current_mode == EditingMode::FILL_TILE) {
    editor_->HandleMapInteraction();
  }

  if (editor_->overworld_.is_loaded()) {
    // Draw the 64 overworld map bitmaps
    DrawOverworldMaps();

    // Draw all entities using the new CanvasRuntime-based methods
    if (editor_->entity_renderer_) {
      editor_->entity_renderer_->DrawExits(canvas_rt, editor_->current_world_);
      editor_->entity_renderer_->DrawEntrances(canvas_rt,
                                               editor_->current_world_);
      editor_->entity_renderer_->DrawItems(canvas_rt, editor_->current_world_);
      editor_->entity_renderer_->DrawSprites(canvas_rt, editor_->current_world_,
                                             editor_->game_state_);
    }

    // Draw overlay preview if enabled
    if (editor_->show_overlay_preview_ && editor_->map_properties_system_) {
      editor_->map_properties_system_->DrawOverlayPreviewOnMap(
          editor_->current_map_, editor_->current_world_,
          editor_->show_overlay_preview_);
    }

    if (editor_->current_mode == EditingMode::DRAW_TILE ||
        editor_->current_mode == EditingMode::FILL_TILE) {
      editor_->CheckForOverworldEdits();
    }

    // Use canvas runtime hover state for map detection
    if (canvas_rt.hovered) {
      editor_->status_ = editor_->CheckForCurrentMap();
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
        if (hovered_entity->entity_type_ ==
            zelda3::GameEntity::EntityType::kItem) {
          editor_->SelectItemByIdentity(
              *static_cast<zelda3::OverworldItem*>(hovered_entity));
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
          editor_->rom_->set_dirty(true);
        }
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

void OverworldCanvasRenderer::DrawOverworldMaps() {
  // Get the current zoom scale for positioning and sizing
  float scale = editor_->ow_map_canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;

  int xx = 0;
  int yy = 0;
  for (int i = 0; i < 0x40; i++) {
    int world_index = i + (editor_->current_world_ * 0x40);

    // Bounds checking to prevent crashes
    if (world_index < 0 ||
        world_index >= static_cast<int>(editor_->maps_bmp_.size())) {
      continue;  // Skip invalid map index
    }

    // Apply scale to positions for proper zoom support
    int map_x = static_cast<int>(xx * kOverworldMapSize * scale);
    int map_y = static_cast<int>(yy * kOverworldMapSize * scale);

    // Ensure visible maps are materialized on demand before drawing.
    if (!editor_->maps_bmp_[world_index].is_active() ||
        !editor_->maps_bmp_[world_index].texture()) {
      editor_->EnsureMapTexture(world_index);
    }

    // Only draw if the map has a valid texture AND is active (has bitmap data)
    // The current_map_ check was causing crashes when hovering over unbuilt maps
    // because the bitmap would be drawn before EnsureMapBuilt() was called
    bool can_draw = editor_->maps_bmp_[world_index].texture() &&
                    editor_->maps_bmp_[world_index].is_active();

    if (can_draw) {
      // Draw bitmap at scaled position with scale applied to size
      editor_->ow_map_canvas_.DrawBitmap(editor_->maps_bmp_[world_index], map_x,
                                         map_y, scale);
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

    xx++;
    if (xx >= 8) {
      yy++;
      xx = 0;
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
    selector_config.display_scale = 2.0f;
    selector_config.tiles_per_row = 8;
    selector_config.total_tiles = zelda3::kNumTile16Individual;
    selector_config.draw_offset = ImVec2(2.0f, 0.0f);
    selector_config.highlight_color = theme.selection_primary;

    selector_config.enable_drag = true;
    selector_config.show_hover_tooltip = true;

    editor_->blockset_selector_ = std::make_unique<gui::TileSelectorWidget>(
        "OwBlocksetSelector", selector_config);
    editor_->blockset_selector_->AttachCanvas(&editor_->blockset_canvas_);
  }

  editor_->UpdateBlocksetSelectorState();

  gui::BeginPadding(3);
  ImGui::BeginGroup();
  gui::BeginChildWithScrollbar(
      "##Tile16SelectorScrollRegion",
      ImVec2(editor_->blockset_selector_->GetPreferredViewportWidth(), 0.0f),
      true);
  gui::EndPadding();

  // Tile ID search/jump bar
  if (editor_->blockset_selector_->DrawFilterBar()) {
    editor_->RequestTile16Selection(
        editor_->blockset_selector_->GetSelectedTileID());
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
    if (editor_->dependencies_.window_manager) {
      editor_->dependencies_.window_manager->OpenWindow(
          OverworldPanelIds::kTile16Editor);
    }
  }

  ImGui::EndChild();
  ImGui::EndGroup();
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
      editor_->overworld_.set_current_map(editor_->current_map_);
      editor_->palette_ = editor_->overworld_.current_area_palette();
      auto bmp = std::make_unique<gfx::Bitmap>();
      bmp->Create(0x80, kOverworldMapSize, 0x08,
                  editor_->overworld_.current_graphics());
      bmp->SetPalette(editor_->palette_);
      editor_->current_graphics_set_[editor_->current_map_] = std::move(bmp);
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE,
          editor_->current_graphics_set_[editor_->current_map_].get());
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

  auto canvas_rt = gui::BeginCanvas(editor_->current_gfx_canvas_, frame_opts);
  gui::EndPadding();

  if (editor_->current_graphics_set_.contains(editor_->current_map_) &&
      editor_->current_graphics_set_[editor_->current_map_]->is_active()) {
    editor_->current_gfx_canvas_.DrawBitmap(
        *editor_->current_graphics_set_[editor_->current_map_], 2, 2, 2.0f);
  }
  editor_->current_gfx_canvas_.DrawTileSelector(32.0f);

  gui::EndCanvas(editor_->current_gfx_canvas_, canvas_rt, frame_opts);
  ImGui::EndChild();
  ImGui::EndGroup();
  return absl::OkStatus();
}

void OverworldCanvasRenderer::DrawV3Settings() {
  // Lightweight v3 controls for map size and feature visibility.
  ImGui::TextWrapped("ZSCustomOverworld v3 settings");
  ImGui::Separator();

  if (!editor_->rom_ || !editor_->rom_->is_loaded()) {
    gui::CenterText("No ROM loaded");
    return;
  }

  const uint8_t asm_version =
      (*editor_->rom_)[zelda3::OverworldCustomASMHasBeenApplied];
  ImGui::Text("ASM Version: 0x%02X", asm_version);

  if (asm_version == 0x00 || asm_version == 0xFF || asm_version < 0x03) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f),
                       "v3 controls require ZSCustomOverworld v3+");
    ImGui::TextDisabled("Apply the v3 patch to enable map-size editing.");
    return;
  }

  auto* map = editor_->overworld_.overworld_map(editor_->current_map_);
  if (!map) {
    ImGui::TextDisabled("Current map is unavailable.");
    return;
  }

  ImGui::Spacing();
  ImGui::Text("Current map: 0x%02X", editor_->current_map_);
  ImGui::Text("Parent map:  0x%02X", map->parent());

  static int selected_area_size = 0;
  selected_area_size = static_cast<int>(map->area_size());
  const char* area_size_labels[] = {"Small", "Wide", "Tall", "Large"};
  ImGui::SetNextItemWidth(220.0f);
  ImGui::Combo("Area Size", &selected_area_size, area_size_labels,
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
      "Area-size changes update sibling map relationships for this parent "
      "map.");
}

void OverworldCanvasRenderer::DrawMapProperties() {
  // Area Configuration panel
  static bool show_custom_bg_color_editor = false;
  static bool show_overlay_editor = false;
  static int game_state = 0;  // 0=Beginning, 1=Zelda Saved, 2=Master Sword

  if (editor_->sidebar_) {
    editor_->sidebar_->Draw(editor_->current_world_, editor_->current_map_,
                            editor_->current_map_lock_, game_state,
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
    if (editor_->map_properties_system_) {
      editor_->map_properties_system_->DrawCustomBackgroundColorEditor(
          editor_->current_map_, show_custom_bg_color_editor);
    }
    ImGui::EndPopup();
  }

  if (ImGui::BeginPopup("OverlayEditor")) {
    if (editor_->map_properties_system_) {
      editor_->map_properties_system_->DrawOverlayEditor(editor_->current_map_,
                                                         show_overlay_editor);
    }
    ImGui::EndPopup();
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

  ImGui::Text("Area Gfx LW/DW");
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::LW_AREA_GFX);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::DW_AREA_GFX);
  ImGui::Separator();

  ImGui::Text("Sprite Gfx LW/DW");
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

  ImGui::Text("Area Pal LW/DW");
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::LW_AREA_PAL);
  ImGui::SameLine();
  editor_->properties_canvas_.UpdateInfoGrid(
      ImVec2(256, 256), 32, OverworldEditor::OverworldProperty::DW_AREA_PAL);

  static bool show_gfx_group = false;
  ImGui::Checkbox("Show Gfx Group Editor", &show_gfx_group);
  if (show_gfx_group) {
    gui::BeginWindowWithDisplaySettings("Gfx Group Editor", &show_gfx_group);
    editor_->status_ = editor_->gfx_group_editor_.Update();
    gui::EndWindowWithDisplaySettings();
  }
}

}  // namespace yaze::editor
