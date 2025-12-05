#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/input.h"
#include "dungeon_canvas_viewer.h"
#include "canvas/canvas_menu.h"
#include "core/icons.h"
#include "absl/status/status.h"
#include "editor/dungeon/object_selection.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

constexpr int kRoomMatrixCols = 16;
constexpr int kRoomMatrixRows = 19;
constexpr int kRoomPropertyColumns = 2;

}  // namespace

// Use shared GetObjectName() from zelda3/dungeon/room_object.h
using zelda3::GetObjectName;
using zelda3::GetObjectSubtype;

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  // Handle pending scroll request
  if (pending_scroll_target_.has_value()) {
    auto [target_x, target_y] = pending_scroll_target_.value();

    // Convert tile coordinates to pixels
    float scale = canvas_.global_scale();
    if (scale <= 0.0f)
      scale = 1.0f;

    float pixel_x = target_x * 8 * scale;
    float pixel_y = target_y * 8 * scale;

    // Center in view
    ImVec2 view_size = ImGui::GetWindowSize();
    float scroll_x = pixel_x - (view_size.x * 0.5f);
    float scroll_y = pixel_y - (view_size.y * 0.5f);

    // Account for canvas position offset if possible, but roughly centering is
    // usually enough Ideally we'd add the cursor position y-offset to scroll_y
    // to account for the UI above canvas but GetCursorPosY() might not be
    // accurate before content is laid out. For X, canvas usually starts at
    // left, so it's fine.

    ImGui::SetScrollX(scroll_x);
    ImGui::SetScrollY(scroll_y);

    pending_scroll_target_.reset();
  }

  ImGui::BeginGroup();

  // CRITICAL: Canvas coordinate system for dungeons
  // The canvas system uses a two-stage scaling model:
  // 1. Canvas size: UNSCALED content dimensions (512x512 for dungeon rooms)
  // 2. Viewport size: canvas_size * global_scale (handles zoom)
  // 3. Grid lines: grid_step * global_scale (auto-scales with zoom)
  // 4. Bitmaps: drawn with scale = global_scale (matches viewport)
  constexpr int kRoomPixelWidth = 512;  // 64 tiles * 8 pixels (UNSCALED)
  constexpr int kRoomPixelHeight = 512;
  constexpr int kDungeonTileSize = 8;  // Dungeon tiles are 8x8 pixels

  // Configure canvas for dungeon display
  canvas_.SetCanvasSize(ImVec2(kRoomPixelWidth, kRoomPixelHeight));
  canvas_.SetCustomGridStep(static_cast<float>(custom_grid_size_));
  canvas_.SetShowBuiltinContextMenu(false);  // Hide default canvas debug items

  // DEBUG: Log canvas configuration
  static int debug_frame_count = 0;
  if (debug_frame_count++ % 60 == 0) {  // Log once per second (assuming 60fps)
    LOG_DEBUG("[DungeonCanvas]",
              "Canvas config: size=(%.0f,%.0f) scale=%.2f grid=%.0f",
              canvas_.width(), canvas_.height(), canvas_.global_scale(),
              canvas_.custom_step());
    LOG_DEBUG(
        "[DungeonCanvas]", "Canvas viewport: p0=(%.0f,%.0f) p1=(%.0f,%.0f)",
        canvas_.zero_point().x, canvas_.zero_point().y,
        canvas_.zero_point().x + canvas_.width() * canvas_.global_scale(),
        canvas_.zero_point().y + canvas_.height() * canvas_.global_scale());
  }

  if (rooms_) {
    auto& room = (*rooms_)[room_id];

    // Check if critical properties changed and trigger reload
    if (prev_blockset_ != room.blockset || prev_palette_ != room.palette ||
        prev_layout_ != room.layout || prev_spriteset_ != room.spriteset) {
      // Only reload if ROM is properly loaded
      if (room.rom() && room.rom()->is_loaded()) {
        // Force reload of room graphics
        // Room buffers are now self-contained - no need for separate palette
        // operations
        room.LoadRoomGraphics(room.blockset);
        room.RenderRoomGraphics();  // Applies palettes internally
      }

      prev_blockset_ = room.blockset;
      prev_palette_ = room.palette;
      prev_layout_ = room.layout;
      prev_spriteset_ = room.spriteset;
    }
    ImGui::Separator();

    auto draw_navigation = [&]() {
      if (!room_navigation_callback_)
        return;

      const int col = room_id % kRoomMatrixCols;
      const int row = room_id / kRoomMatrixCols;

      auto room_if_valid = [](int candidate) -> std::optional<int> {
        if (candidate < 0 || candidate >= zelda3::NumberOfRooms)
          return std::nullopt;
        return candidate;
      };

      const auto north =
          room_if_valid(row > 0 ? room_id - kRoomMatrixCols : -1);
      const auto south = room_if_valid(
          row < kRoomMatrixRows - 1 ? room_id + kRoomMatrixCols : -1);
      const auto west = room_if_valid(col > 0 ? room_id - 1 : -1);
      const auto east =
          room_if_valid(col < kRoomMatrixCols - 1 ? room_id + 1 : -1);

      auto nav_button = [&](const char* id, ImGuiDir dir,
                            const std::optional<int>& target,
                            const char* tooltip) {
        const bool enabled = target.has_value();
        if (!enabled)
          ImGui::BeginDisabled();
        if (ImGui::ArrowButton(id, dir) && enabled &&
            room_navigation_callback_) {
          room_navigation_callback_(*target);
        }
        if (!enabled)
          ImGui::EndDisabled();
        if (enabled && ImGui::IsItemHovered() && tooltip && tooltip[0] != '\0')
          ImGui::SetTooltip("%s", tooltip);
      };

      ImGui::BeginGroup();
      nav_button("RoomNavNorth", ImGuiDir_Up, north, "Go to room above");
      ImGui::SameLine();
      nav_button("RoomNavSouth", ImGuiDir_Down, south, "Go to room below");
      ImGui::SameLine();
      nav_button("RoomNavWest", ImGuiDir_Left, west, "Previous room");
      ImGui::SameLine();
      nav_button("RoomNavEast", ImGuiDir_Right, east, "Next room");
      ImGui::EndGroup();
      ImGui::SameLine();
    };

    auto& layer_mgr = GetRoomLayerManager(room_id);
    layer_mgr.ApplyLayerMerging(room.layer_merging());

    uint8_t blockset_val = room.blockset;
    uint8_t spriteset_val = room.spriteset;
    uint8_t palette_val = room.palette;
    uint8_t floor1_val = room.floor1();
    uint8_t floor2_val = room.floor2();
    int effect_val = static_cast<int>(room.effect());
    int tag1_val = static_cast<int>(room.tag1());
    int tag2_val = static_cast<int>(room.tag2());
    uint8_t layout_val = room.layout;

    // Effect names matching RoomEffect array in room.cc (8 entries, 0-7)
    const char* effect_names[] = {
        "Nothing",              // 0
        "Nothing (1)",          // 1 - unused but exists in ROM
        "Moving Floor",         // 2
        "Moving Water",         // 3
        "Trinexx Shell",        // 4
        "Red Flashes",          // 5
        "Light Torch to See",   // 6
        "Ganon's Darkness"      // 7
    };
    
    // Tag names matching RoomTag array in room.cc
    const char* tag_names[] = {
        "Nothing",                    // 0
        "NW Kill Enemy to Open",      // 1
        "NE Kill Enemy to Open",      // 2
        "SW Kill Enemy to Open",      // 3
        "SE Kill Enemy to Open",      // 4
        "W Kill Enemy to Open",       // 5
        "E Kill Enemy to Open",       // 6
        "N Kill Enemy to Open",       // 7
        "S Kill Enemy to Open",       // 8
        "Clear Quadrant to Open",     // 9
        "Clear Full Tile to Open",    // 10
        "NW Push Block to Open",      // 11
        "NE Push Block to Open",      // 12
        "SW Push Block to Open",      // 13
        "SE Push Block to Open",      // 14
        "W Push Block to Open",       // 15
        "E Push Block to Open",       // 16
        "N Push Block to Open",       // 17
        "S Push Block to Open",       // 18
        "Push Block to Open",         // 19
        "Pull Lever to Open",         // 20
        "Collect Prize to Open",      // 21
        "Hold Switch Open Door",      // 22
        "Toggle Switch to Open",      // 23
        "Turn off Water",             // 24
        "Turn on Water",              // 25
        "Water Gate",                 // 26
        "Water Twin",                 // 27
        "Moving Wall Right",          // 28
        "Moving Wall Left",           // 29
        "Crash (30)",                 // 30
        "Crash (31)",                 // 31
        "Push Switch Exploding Wall", // 32
        "Holes 0",                    // 33
        "Open Chest (Holes 0)",       // 34
        "Holes 1",                    // 35
        "Holes 2",                    // 36
        "Defeat Boss for Prize",      // 37
        "SE Kill Enemy Push Block",   // 38
        "Trigger Switch Chest",       // 39
        "Pull Lever Exploding Wall",  // 40
        "NW Kill Enemy for Chest",    // 41
        "NE Kill Enemy for Chest",    // 42
        "SW Kill Enemy for Chest",    // 43
        "SE Kill Enemy for Chest",    // 44
        "W Kill Enemy for Chest",     // 45
        "E Kill Enemy for Chest",     // 46
        "N Kill Enemy for Chest",     // 47
        "S Kill Enemy for Chest",     // 48
        "Clear Quadrant for Chest",   // 49
        "Clear Full Tile for Chest",  // 50
        "Light Torches to Open",      // 51
        "Holes 3",                    // 52
        "Holes 4",                    // 53
        "Holes 5",                    // 54
        "Holes 6",                    // 55
        "Agahnim Room",               // 56
        "Holes 7",                    // 57
        "Holes 8",                    // 58
        "Open Chest for Holes 8",     // 59
        "Push Block for Chest",       // 60
        "Clear Room for Triforce",    // 61
        "Light Torches for Chest",    // 62
        "Kill Boss Again",            // 63
        "64 (Unused)"                 // 64
    };
    constexpr int kNumTags = IM_ARRAYSIZE(tag_names);
    
    const char* merge_types[] = {"Off",    "Parallax",    "Dark",
                                 "On top", "Translucent", "Addition",
                                 "Normal", "Transparent", "Dark room"};
    const char* blend_modes[] = {"Normal", "Trans", "Add", "Dark", "Off"};

    // ========================================================================
    // ROOM PROPERTIES TABLE - Compact layout for docking
    // ========================================================================
    if (ImGui::BeginTable("##RoomPropsTable", 2, ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("NavCol", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableSetupColumn("PropsCol", ImGuiTableColumnFlags_WidthStretch);
      
      // Row 1: Navigation + Room ID + Hex inputs
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      draw_navigation();
      ImGui::TableNextColumn();
      // Room ID and hex property inputs with icons
      ImGui::Text(ICON_MD_TUNE " %03X", room_id);
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_VIEW_MODULE);
      ImGui::SameLine(0, 2);
      // Blockset: max 81 (kNumRoomBlocksets = 82)
      if (auto res = gui::InputHexByteEx("##Blockset", &blockset_val, 81, 32.f, true);
          res.ShouldApply()) {
        room.SetBlockset(blockset_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Blockset (0-51)");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_PALETTE);
      ImGui::SameLine(0, 2);
      // Palette: max 71 (kNumPalettesets = 72)
      if (auto res = gui::InputHexByteEx("##Palette", &palette_val, 71, 32.f, true);
          res.ShouldApply()) {
        room.SetPalette(palette_val);
        SetCurrentPaletteId(palette_val);
        if (game_data_ && rom_) {
          if (palette_val < game_data_->paletteset_ids.size() &&
              !game_data_->paletteset_ids[palette_val].empty()) {
            auto palette_ptr = game_data_->paletteset_ids[palette_val][0];
            if (auto palette_id_res = rom_->ReadWord(0xDEC4B + palette_ptr);
                palette_id_res.ok()) {
              current_palette_group_id_ = palette_id_res.value() / 180;
              if (current_palette_group_id_ <
                  game_data_->palette_groups.dungeon_main.size()) {
                auto full_palette =
                    game_data_->palette_groups
                        .dungeon_main[current_palette_group_id_];
                if (auto res =
                        gfx::CreatePaletteGroupFromLargePalette(full_palette, 16);
                    res.ok()) {
                  current_palette_group_ = res.value();
                }
              }
            }
          }
        }
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Palette (0-47)");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_GRID_VIEW);
      ImGui::SameLine(0, 2);
      // Layout: 8 valid layouts (0-7)
      if (auto res = gui::InputHexByteEx("##Layout", &layout_val, 7, 32.f, true);
          res.ShouldApply()) {
        room.layout = layout_val;
        room.MarkLayoutDirty();
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Layout (0-7)");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_PEST_CONTROL);
      ImGui::SameLine(0, 2);
      // Spriteset: max 143 (kNumSpritesets = 144)
      if (auto res = gui::InputHexByteEx("##Spriteset", &spriteset_val, 143, 32.f, true);
          res.ShouldApply()) {
        room.SetSpriteset(spriteset_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Spriteset (0-8F)");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_SQUARE);
      ImGui::SameLine(0, 2);
      // Floor graphics: max 15 (4-bit value, 0-F)
      if (auto res = gui::InputHexByteEx("##Floor1", &floor1_val, 15, 32.f, true);
          res.ShouldApply()) {
        room.set_floor1(floor1_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Floor 1 (0-F)");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_SQUARE_FOOT);
      ImGui::SameLine(0, 2);
      // Floor graphics: max 15 (4-bit value, 0-F)
      if (auto res = gui::InputHexByteEx("##Floor2", &floor2_val, 15, 32.f, true);
          res.ShouldApply()) {
        room.set_floor2(floor2_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Floor 2 (0-F)");

      // Row 2: Effect + Tags (all in one flow, not separated by table columns)
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_AUTO_AWESOME " FX/Tags");
      ImGui::TableNextColumn();
      constexpr int kNumEffects = IM_ARRAYSIZE(effect_names);
      if (effect_val < 0) effect_val = 0;
      if (effect_val >= kNumEffects) effect_val = kNumEffects - 1;
      ImGui::SetNextItemWidth(100);
      if (ImGui::BeginCombo("##Effect", effect_names[effect_val])) {
        for (int i = 0; i < kNumEffects; i++) {
          if (ImGui::Selectable(effect_names[i], effect_val == i)) {
            room.SetEffect(static_cast<zelda3::EffectKey>(i));
            if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Effect");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_LABEL);
      ImGui::SameLine(0, 2);
      int tag1_idx = std::clamp(tag1_val, 0, kNumTags - 1);
      ImGui::SetNextItemWidth(120);
      if (ImGui::BeginCombo("##Tag1", tag_names[tag1_idx])) {
        for (int i = 0; i < kNumTags; i++) {
          if (ImGui::Selectable(tag_names[i], tag1_idx == i)) {
            room.SetTag1(static_cast<zelda3::TagKey>(i));
            if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tag 1");
      ImGui::SameLine();
      ImGui::TextDisabled(ICON_MD_LABEL_OUTLINE);
      ImGui::SameLine(0, 2);
      int tag2_idx = std::clamp(tag2_val, 0, kNumTags - 1);
      ImGui::SetNextItemWidth(120);
      if (ImGui::BeginCombo("##Tag2", tag_names[tag2_idx])) {
        for (int i = 0; i < kNumTags; i++) {
          if (ImGui::Selectable(tag_names[i], tag2_idx == i)) {
            room.SetTag2(static_cast<zelda3::TagKey>(i));
            if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
          }
        }
        ImGui::EndCombo();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Tag 2");

      // Row 3: Layer visibility + Blend/Merge
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_LAYERS " Layers");
      ImGui::TableNextColumn();
      bool bg1_layout = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
      bool bg1_objects = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
      bool bg2_layout = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
      bool bg2_objects = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
      
      // Helper to mark layer bitmaps as needing texture update
      auto mark_layers_dirty = [&]() {
        if (rooms_) {
          auto& r = (*rooms_)[room_id];
          r.bg1_buffer().bitmap().set_modified(true);
          r.bg2_buffer().bitmap().set_modified(true);
          r.object_bg1_buffer().bitmap().set_modified(true);
          r.object_bg2_buffer().bitmap().set_modified(true);
          r.MarkCompositeDirty();
        }
      };
      
      if (ImGui::Checkbox("BG1##L", &bg1_layout)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1_layout);
        mark_layers_dirty();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BG1 Layout: Main floor tiles (rendered on top of BG2)");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("O1##O", &bg1_objects)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1_objects);
        mark_layers_dirty();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BG1 Objects: Walls, pots, interactive objects (topmost layer)");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("BG2##L2", &bg2_layout)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2_layout);
        mark_layers_dirty();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BG2 Layout: Background floor patterns (behind BG1)");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("O2##O2", &bg2_objects)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2_objects);
        mark_layers_dirty();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("BG2 Objects: Background details (behind BG1)");
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("M##Comp", &use_composite_mode_)) {
        // Mark everything dirty when switching composite modes
        mark_layers_dirty();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Merge Mode: Composite all layers into single output.\n"
            "Layer order (SNES Mode 1): BG2 behind, BG1 on top.\n"
            "Uncheck for debug view of individual layer buffers.");
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      int bg2_blend = static_cast<int>(
          layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
      if (ImGui::Combo("##Bld", &bg2_blend, blend_modes, IM_ARRAYSIZE(blend_modes))) {
        auto mode = static_cast<zelda3::LayerBlendMode>(bg2_blend);
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout, mode);
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects, mode);
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "BG2 Blend Mode (color math effect):\n"
            "- Normal: Opaque pixels\n"
            "- Translucent: 50% alpha\n"
            "- Addition: Additive blending\n"
            "Does not change layer order (BG1 always on top)");
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(70);
      int merge_val = room.layer_merging().ID;
      if (ImGui::Combo("##Mrg", &merge_val, merge_types, IM_ARRAYSIZE(merge_types))) {
        room.SetLayerMerging(zelda3::kLayerMergeTypeList[merge_val]);
        layer_mgr.ApplyLayerMergingPreserveVisibility(room.layer_merging());
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Merge type");

      // Row 4: Selection filter
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_SELECT_ALL " Select");
      ImGui::TableNextColumn();
      object_interaction_.SetLayersMerged(layer_mgr.AreLayersMerged());
      int current_filter = object_interaction_.GetLayerFilter();
      if (ImGui::RadioButton("All", current_filter == ObjectSelection::kLayerAll))
        object_interaction_.SetLayerFilter(ObjectSelection::kLayerAll);
      ImGui::SameLine();
      if (ImGui::RadioButton("L1", current_filter == ObjectSelection::kLayer1))
        object_interaction_.SetLayerFilter(ObjectSelection::kLayer1);
      ImGui::SameLine();
      if (ImGui::RadioButton("L2", current_filter == ObjectSelection::kLayer2))
        object_interaction_.SetLayerFilter(ObjectSelection::kLayer2);
      ImGui::SameLine();
      if (ImGui::RadioButton("L3", current_filter == ObjectSelection::kLayer3))
        object_interaction_.SetLayerFilter(ObjectSelection::kLayer3);
      if (object_interaction_.IsLayerFilterActive()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), ICON_MD_FILTER_ALT);
      }
      if (layer_mgr.AreLayersMerged()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), ICON_MD_MERGE_TYPE);
      }

      ImGui::EndTable();
    }
    ImGui::Separator();
  }

  ImGui::EndGroup();

  // Set up context menu items BEFORE DrawBackground so DrawContextMenu can be
  // called immediately after (OpenPopupOnItemClick requires this ordering)
  canvas_.ClearContextMenuItems();

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // === Entity Placement Menu ===
    gui::CanvasMenuItem place_menu;
    place_menu.label = "Place Entity";
    place_menu.icon = ICON_MD_ADD;
    
    // Place Object option
    place_menu.subitems.push_back(gui::CanvasMenuItem(
        "Object", ICON_MD_WIDGETS,
        [this]() {
          if (show_object_panel_callback_) {
            show_object_panel_callback_();
          }
        }));
    
    // Place Sprite option
    place_menu.subitems.push_back(gui::CanvasMenuItem(
        "Sprite", ICON_MD_PERSON,
        [this]() {
          bool active = object_interaction_.IsSpritePlacementActive();
          object_interaction_.SetSpritePlacementMode(!active, 0x09);
        }));
    
    // Place Item option
    place_menu.subitems.push_back(gui::CanvasMenuItem(
        "Item", ICON_MD_INVENTORY,
        [this]() {
          bool active = object_interaction_.IsItemPlacementActive();
          object_interaction_.SetItemPlacementMode(!active, 1);
        }));
    
    // Place Door option
    place_menu.subitems.push_back(gui::CanvasMenuItem(
        "Door", ICON_MD_DOOR_FRONT,
        [this]() {
          bool active = object_interaction_.IsDoorPlacementActive();
          object_interaction_.SetDoorPlacementMode(!active, 
              zelda3::DoorType::NormalDoor);
        }));
    
    canvas_.AddContextMenuItem(place_menu);

    // Add room property quick toggles (4-way layer visibility)
    gui::CanvasMenuItem layer_menu;
    layer_menu.label = "Layer Visibility";
    layer_menu.icon = ICON_MD_LAYERS;

    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG1 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG1 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG2 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG2 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects));
        }));

    canvas_.AddContextMenuItem(layer_menu);

    // Entity Visibility menu
    gui::CanvasMenuItem entity_menu;
    entity_menu.label = "Entity Visibility";
    entity_menu.icon = ICON_MD_PERSON;

    entity_menu.subitems.push_back(
        gui::CanvasMenuItem("Show Sprites", [this]() {
          entity_visibility_.show_sprites = !entity_visibility_.show_sprites;
        }));
    entity_menu.subitems.push_back(
        gui::CanvasMenuItem("Show Pot Items", [this]() {
          entity_visibility_.show_pot_items = !entity_visibility_.show_pot_items;
        }));

    canvas_.AddContextMenuItem(entity_menu);

    // Add re-render option
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        "Re-render Room", ICON_MD_REFRESH,
        [&room]() { room.RenderRoomGraphics(); }, "Ctrl+R"));

    // Grid Options
    gui::CanvasMenuItem grid_menu;
    grid_menu.label = "Grid Options";
    grid_menu.icon = ICON_MD_GRID_ON;

    // Toggle grid visibility
    gui::CanvasMenuItem toggle_grid_item(
        show_grid_ ? "Hide Grid" : "Show Grid", 
        show_grid_ ? ICON_MD_GRID_OFF : ICON_MD_GRID_ON,
        [this]() { show_grid_ = !show_grid_; }, "G");
    grid_menu.subitems.push_back(toggle_grid_item);

    // Grid size options (only show if grid is visible)
    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("8x8", [this]() { custom_grid_size_ = 8; show_grid_ = true; }));
    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("16x16", [this]() { custom_grid_size_ = 16; show_grid_ = true; }));
    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("32x32", [this]() { custom_grid_size_ = 32; show_grid_ = true; }));

    canvas_.AddContextMenuItem(grid_menu);

    // === DEBUG MENU ===
    gui::CanvasMenuItem debug_menu;
    debug_menu.label = "Debug";
    debug_menu.icon = ICON_MD_BUG_REPORT;

    // Show room info
    gui::CanvasMenuItem room_info_item(
        "Show Room Info", ICON_MD_INFO,
        [this]() { show_room_debug_info_ = !show_room_debug_info_; });
    debug_menu.subitems.push_back(room_info_item);

    // Show texture info
    gui::CanvasMenuItem texture_info_item(
        "Show Texture Debug", ICON_MD_IMAGE,
        [this]() { show_texture_debug_ = !show_texture_debug_; });
    debug_menu.subitems.push_back(texture_info_item);

    // Show object bounds with sub-menu for categories
    gui::CanvasMenuItem object_bounds_menu;
    object_bounds_menu.label = "Show Object Bounds";
    object_bounds_menu.icon = ICON_MD_CROP_SQUARE;
    object_bounds_menu.callback = [this]() {
      show_object_bounds_ = !show_object_bounds_;
    };

    // Sub-menu for filtering by type
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 1 (0x00-0xFF)", [this]() {
          object_outline_toggles_.show_type1_objects =
              !object_outline_toggles_.show_type1_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 2 (0x100-0x1FF)", [this]() {
          object_outline_toggles_.show_type2_objects =
              !object_outline_toggles_.show_type2_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 3 (0xF00-0xFFF)", [this]() {
          object_outline_toggles_.show_type3_objects =
              !object_outline_toggles_.show_type3_objects;
        }));

    // Separator
    gui::CanvasMenuItem sep;
    sep.label = "---";
    sep.enabled_condition = []() {
      return false;
    };
    object_bounds_menu.subitems.push_back(sep);

    // Sub-menu for filtering by layer
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 0 (BG1)", [this]() {
          object_outline_toggles_.show_layer0_objects =
              !object_outline_toggles_.show_layer0_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 1 (BG2)", [this]() {
          object_outline_toggles_.show_layer1_objects =
              !object_outline_toggles_.show_layer1_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 2 (BG3)", [this]() {
          object_outline_toggles_.show_layer2_objects =
              !object_outline_toggles_.show_layer2_objects;
        }));

    debug_menu.subitems.push_back(object_bounds_menu);

    // Show layer info
    gui::CanvasMenuItem layer_info_item(
        "Show Layer Info", ICON_MD_LAYERS,
        [this]() { show_layer_info_ = !show_layer_info_; });
    debug_menu.subitems.push_back(layer_info_item);

    // Force reload room
    gui::CanvasMenuItem force_reload_item(
        "Force Reload", ICON_MD_REFRESH, [&room]() {
          room.LoadObjects();
          room.LoadRoomGraphics(room.blockset);
          room.RenderRoomGraphics();
        });
    debug_menu.subitems.push_back(force_reload_item);

    // Log room state
    gui::CanvasMenuItem log_item(
        "Log Room State", ICON_MD_PRINT, [&room, room_id]() {
          LOG_DEBUG("DungeonDebug", "=== Room %03X Debug ===", room_id);
          LOG_DEBUG("DungeonDebug", "Blockset: %d, Palette: %d, Layout: %d",
                    room.blockset, room.palette, room.layout);
          LOG_DEBUG("DungeonDebug", "Objects: %zu, Sprites: %zu",
                    room.GetTileObjects().size(), room.GetSprites().size());
          LOG_DEBUG("DungeonDebug", "BG1: %dx%d, BG2: %dx%d",
                    room.bg1_buffer().bitmap().width(),
                    room.bg1_buffer().bitmap().height(),
                    room.bg2_buffer().bitmap().width(),
                    room.bg2_buffer().bitmap().height());
        });
    debug_menu.subitems.push_back(log_item);

    canvas_.AddContextMenuItem(debug_menu);
  }

  // Add object interaction menu items to canvas context menu
  if (object_interaction_enabled_) {
    auto& interaction = object_interaction_;
    auto selected = interaction.GetSelectedObjectIndices();
    const bool has_selection = !selected.empty();
    const bool single_selection = selected.size() == 1;
    const bool has_clipboard = interaction.HasClipboardData();
    const bool placing_object = interaction.IsObjectLoaded();

    if (single_selection && rooms_) {
      auto& room = (*rooms_)[room_id];
      const auto& objects = room.GetTileObjects();
      if (selected[0] < objects.size()) {
        const auto& obj = objects[selected[0]];
        std::string name = GetObjectName(obj.id_);
        canvas_.AddContextMenuItem(gui::CanvasMenuItem::Disabled(
            absl::StrFormat("Object 0x%02X: %s", obj.id_, name.c_str())));
      }
    }

    auto enabled_if = [](bool enabled) {
      return [enabled]() {
        return enabled;
      };
    };

    gui::CanvasMenuItem cut_item(
        "Cut", ICON_MD_CONTENT_CUT,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandleDeleteSelected();
        },
        "Ctrl+X");
    cut_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(cut_item);

    gui::CanvasMenuItem copy_item(
        "Copy", ICON_MD_CONTENT_COPY,
        [&interaction]() { interaction.HandleCopySelected(); }, "Ctrl+C");
    copy_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(copy_item);

    gui::CanvasMenuItem duplicate_item(
        "Duplicate", ICON_MD_CONTENT_PASTE,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandlePasteObjects();
        },
        "Ctrl+D");
    duplicate_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(duplicate_item);

    gui::CanvasMenuItem delete_item(
        "Delete", ICON_MD_DELETE,
        [&interaction]() { interaction.HandleDeleteSelected(); }, "Del");
    delete_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(delete_item);

    gui::CanvasMenuItem paste_item(
        "Paste", ICON_MD_CONTENT_PASTE,
        [&interaction]() { interaction.HandlePasteObjects(); }, "Ctrl+V");
    paste_item.enabled_condition = enabled_if(has_clipboard);
    canvas_.AddContextMenuItem(paste_item);

    gui::CanvasMenuItem cancel_item(
        "Cancel Placement", ICON_MD_CANCEL,
        [&interaction]() { interaction.CancelPlacement(); }, "Esc");
    cancel_item.enabled_condition = enabled_if(placing_object);
    canvas_.AddContextMenuItem(cancel_item);

    // Send to Layer submenu
    gui::CanvasMenuItem layer_menu;
    layer_menu.label = "Send to Layer";
    layer_menu.icon = ICON_MD_LAYERS;
    layer_menu.enabled_condition = enabled_if(has_selection);

    gui::CanvasMenuItem layer1_item(
        "Layer 1 (BG1)", ICON_MD_LOOKS_ONE,
        [&interaction]() { interaction.SendSelectedToLayer(0); }, "1");
    layer1_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer1_item);

    gui::CanvasMenuItem layer2_item(
        "Layer 2 (BG2)", ICON_MD_LOOKS_TWO,
        [&interaction]() { interaction.SendSelectedToLayer(1); }, "2");
    layer2_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer2_item);

    gui::CanvasMenuItem layer3_item(
        "Layer 3 (BG3)", ICON_MD_LOOKS_3,
        [&interaction]() { interaction.SendSelectedToLayer(2); }, "3");
    layer3_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer3_item);

    canvas_.AddContextMenuItem(layer_menu);

    // === Entity Selection Actions (Doors, Sprites, Items) ===
    const auto& selected_entity = interaction.GetSelectedEntity();
    const bool has_entity_selection = interaction.HasEntitySelection();
    
    if (has_entity_selection && rooms_) {
      auto& room = (*rooms_)[room_id];
      
      // Show selected entity info header
      std::string entity_info;
      switch (selected_entity.type) {
        case EntityType::Door: {
          const auto& doors = room.GetDoors();
          if (selected_entity.index < doors.size()) {
            const auto& door = doors[selected_entity.index];
            entity_info = absl::StrFormat(ICON_MD_DOOR_FRONT " Door: %s",
                std::string(zelda3::GetDoorTypeName(door.type)).c_str());
          }
          break;
        }
        case EntityType::Sprite: {
          const auto& sprites = room.GetSprites();
          if (selected_entity.index < sprites.size()) {
            const auto& sprite = sprites[selected_entity.index];
            entity_info = absl::StrFormat(ICON_MD_PERSON " Sprite: %s (0x%02X)",
                zelda3::ResolveSpriteName(sprite.id()), sprite.id());
          }
          break;
        }
        case EntityType::Item: {
          const auto& items = room.GetPotItems();
          if (selected_entity.index < items.size()) {
            const auto& item = items[selected_entity.index];
            entity_info = absl::StrFormat(ICON_MD_INVENTORY " Item: 0x%02X", item.item);
          }
          break;
        }
        default:
          break;
      }
      
      if (!entity_info.empty()) {
        canvas_.AddContextMenuItem(gui::CanvasMenuItem::Disabled(entity_info));
        
        // Delete entity action
        gui::CanvasMenuItem delete_entity_item(
            "Delete Entity", ICON_MD_DELETE,
            [this, &room, selected_entity]() {
              switch (selected_entity.type) {
                case EntityType::Door: {
                  auto& doors = room.GetDoors();
                  if (selected_entity.index < doors.size()) {
                    doors.erase(doors.begin() + 
                        static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                case EntityType::Sprite: {
                  auto& sprites = room.GetSprites();
                  if (selected_entity.index < sprites.size()) {
                    sprites.erase(sprites.begin() + 
                        static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                case EntityType::Item: {
                  auto& items = room.GetPotItems();
                  if (selected_entity.index < items.size()) {
                    items.erase(items.begin() + 
                        static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                default:
                  break;
              }
              object_interaction_.ClearEntitySelection();
            },
            "Del");
        canvas_.AddContextMenuItem(delete_entity_item);
      }
    }
  }

  // CRITICAL: Draw canvas with explicit size to ensure viewport matches content
  // DrawBackground must be called right before DrawContextMenu so that
  // OpenPopupOnItemClick targets the canvas's InvisibleButton
  canvas_.DrawBackground(ImVec2(kRoomPixelWidth, kRoomPixelHeight));
  canvas_.DrawContextMenu();

  // Draw persistent debug overlays
  if (show_room_debug_info_ && rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 10, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Room Debug Info", &show_room_debug_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Room: 0x%03X (%d)", room_id, room_id);
      ImGui::Separator();
      ImGui::Text("Graphics");
      ImGui::Text("  Blockset: 0x%02X", room.blockset);
      ImGui::Text("  Palette: 0x%02X", room.palette);
      ImGui::Text("  Layout: 0x%02X", room.layout);
      ImGui::Text("  Spriteset: 0x%02X", room.spriteset);
      ImGui::Separator();
      ImGui::Text("Content");
      ImGui::Text("  Objects: %zu", room.GetTileObjects().size());
      ImGui::Text("  Sprites: %zu", room.GetSprites().size());
      ImGui::Separator();
      ImGui::Text("Buffers");
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();
      ImGui::Text("  BG1: %dx%d %s", bg1.width(), bg1.height(),
                  bg1.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Text("  BG2: %dx%d %s", bg2.width(), bg2.height(),
                  bg2.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Separator();
      ImGui::Text("Layers (4-way)");
      auto& layer_mgr = GetRoomLayerManager(room_id);
      bool bg1l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
      bool bg1o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
      bool bg2l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
      bool bg2o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
      if (ImGui::Checkbox("BG1 Layout", &bg1l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1l);
      if (ImGui::Checkbox("BG1 Objects", &bg1o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1o);
      if (ImGui::Checkbox("BG2 Layout", &bg2l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2l);
      if (ImGui::Checkbox("BG2 Objects", &bg2o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2o);
      int blend = static_cast<int>(
          layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
      if (ImGui::SliderInt("BG2 Blend", &blend, 0, 4)) {
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                    static_cast<zelda3::LayerBlendMode>(blend));
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                    static_cast<zelda3::LayerBlendMode>(blend));
      }

      ImGui::Separator();
      ImGui::Text("Layout Override");
      static bool enable_override = false;
      ImGui::Checkbox("Enable Override", &enable_override);
      if (enable_override) {
        ImGui::SliderInt("Layout ID", &layout_override_, 0, 7);
      } else {
        layout_override_ = -1;  // Disable override
      }

      if (show_object_bounds_) {
        ImGui::Separator();
        ImGui::Text("Object Outline Filters");
        ImGui::Text("By Type:");
        ImGui::Checkbox("Type 1", &object_outline_toggles_.show_type1_objects);
        ImGui::Checkbox("Type 2", &object_outline_toggles_.show_type2_objects);
        ImGui::Checkbox("Type 3", &object_outline_toggles_.show_type3_objects);
        ImGui::Text("By Layer:");
        ImGui::Checkbox("Layer 0",
                        &object_outline_toggles_.show_layer0_objects);
        ImGui::Checkbox("Layer 1",
                        &object_outline_toggles_.show_layer1_objects);
        ImGui::Checkbox("Layer 2",
                        &object_outline_toggles_.show_layer2_objects);
      }
    }
    ImGui::End();
  }

  if (show_texture_debug_ && rooms_ && rom_->is_loaded()) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 320, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Texture Debug", &show_texture_debug_,
                     ImGuiWindowFlags_NoCollapse)) {
      auto& room = (*rooms_)[room_id];
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();

      ImGui::Text("BG1 Bitmap");
      ImGui::Text("  Size: %dx%d", bg1.width(), bg1.height());
      ImGui::Text("  Active: %s", bg1.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg1.texture());
      ImGui::Text("  Modified: %s", bg1.modified() ? "YES" : "NO");

      if (bg1.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg1.texture(), ImVec2(128, 128));
      }

      ImGui::Separator();
      ImGui::Text("BG2 Bitmap");
      ImGui::Text("  Size: %dx%d", bg2.width(), bg2.height());
      ImGui::Text("  Active: %s", bg2.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg2.texture());
      ImGui::Text("  Modified: %s", bg2.modified() ? "YES" : "NO");

      if (bg2.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg2.texture(), ImVec2(128, 128));
      }
    }
    ImGui::End();
  }

  if (show_layer_info_) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 580, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Layer Info", &show_layer_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Canvas Scale: %.2f", canvas_.global_scale());
      ImGui::Text("Canvas Size: %.0fx%.0f", canvas_.width(), canvas_.height());
      auto& layer_mgr = GetRoomLayerManager(room_id);
      ImGui::Separator();
      ImGui::Text("Layer Visibility (4-way):");

      // Display each layer with visibility and blend mode
      for (int i = 0; i < 4; ++i) {
        auto layer = static_cast<zelda3::LayerType>(i);
        bool visible = layer_mgr.IsLayerVisible(layer);
        auto blend = layer_mgr.GetLayerBlendMode(layer);
        ImGui::Text("  %s: %s (%s)",
                    zelda3::RoomLayerManager::GetLayerName(layer),
                    visible ? "VISIBLE" : "hidden",
                    zelda3::RoomLayerManager::GetBlendModeName(blend));
      }

      ImGui::Separator();
      ImGui::Text("Draw Order:");
      auto draw_order = layer_mgr.GetDrawOrder();
      for (int i = 0; i < 4; ++i) {
        ImGui::Text("  %d: %s", i + 1,
                    zelda3::RoomLayerManager::GetLayerName(draw_order[i]));
      }
      ImGui::Text("BG2 On Top: %s", layer_mgr.IsBG2OnTop() ? "YES" : "NO");
    }
    ImGui::End();
  }

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Update object interaction context
    object_interaction_.SetCurrentRoom(rooms_, room_id);

    // Check if THIS ROOM's buffers need rendering (not global arena!)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    bool needs_render = !bg1_bitmap.is_active() || bg1_bitmap.width() == 0;

    // Render immediately if needed (but only once per room change)
    static int last_rendered_room = -1;
    static bool has_rendered = false;
    if (needs_render && (last_rendered_room != room_id || !has_rendered)) {
      printf(
          "[DungeonCanvasViewer] Loading and rendering graphics for room %d\n",
          room_id);
      (void)LoadAndRenderRoomGraphics(room_id);
      last_rendered_room = room_id;
      has_rendered = true;
    }

    // Load room objects if not already loaded
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    
    // Load sprites if not already loaded
    if (room.GetSprites().empty()) {
      room.LoadSprites();
    }
    
    // Load pot items if not already loaded
    if (room.GetPotItems().empty()) {
      room.LoadPotItems();
    }

    // CRITICAL: Process texture queue BEFORE drawing to ensure textures are
    // ready This must happen before DrawRoomBackgroundLayers() attempts to draw
    // bitmaps
    if (rom_ && rom_->is_loaded()) {
      gfx::Arena::Get().ProcessTextureQueue(renderer_);
    }

    // Draw the room's background layers to canvas
    // This already includes objects rendered by ObjectDrawer in
    // Room::RenderObjectsToBackground()
    DrawRoomBackgroundLayers(room_id);

    // Render entity overlays (sprites, pot items) as colored squares with labels
    // (Entities are not part of the background buffers)
    RenderEntityOverlay(room);

    // Handle object interaction if enabled
    if (object_interaction_enabled_) {
      object_interaction_.HandleCanvasMouseInput();
      object_interaction_.CheckForObjectSelection();
      object_interaction_.DrawSelectBox();
      object_interaction_
          .DrawSelectionHighlights();  // Draw object selection highlights
      object_interaction_
          .DrawEntitySelectionHighlights();  // Draw door/sprite/item selection
      object_interaction_.DrawGhostPreview();  // Draw placement preview
      // Context menu is handled by canvas_.DrawContextMenu() above
    }
  }

  // Draw optional overlays on top of background bitmap
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Draw the room layout first as the base layer

    // VISUALIZATION: Draw object position rectangles (for debugging)
    // This shows where objects are placed regardless of whether graphics render
    if (show_object_bounds_) {
      DrawObjectPositionOutlines(room);
    }
  }

  // Only draw grid when enabled (off by default for dungeon editor)
  if (show_grid_) {
    canvas_.DrawGrid();
  }
  canvas_.DrawOverlay();
}

void DungeonCanvasViewer::DisplayObjectInfo(const zelda3::RoomObject& object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay with hex ID and name
  std::string name = GetObjectName(object.id_);
  std::string info_text;
  if (object.id_ >= 0x100) {
    info_text =
        absl::StrFormat("0x%03X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  } else {
    info_text =
        absl::StrFormat("0x%02X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  }

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderSprites(const zelda3::Room& room) {
  // Skip if sprites are not visible
  if (!entity_visibility_.show_sprites) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();

  // Render sprites as 16x16 colored squares with sprite name/ID
  // NOTE: Sprite coordinates are in 16-pixel units (0-31 range = 512 pixels)
  // unlike object coordinates which are in 8-pixel tile units
  for (const auto& sprite : room.GetSprites()) {
    // Sprites use 16-pixel coordinate system
    int canvas_x = sprite.x() * 16;
    int canvas_y = sprite.y() * 16;

    if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      // Draw 16x16 square for sprite (like overworld entities)
      ImVec4 sprite_color;

      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = theme.dungeon_sprite_layer0;  // Green for layer 0
      } else {
        sprite_color = theme.dungeon_sprite_layer1;  // Blue for layer 1
      }

      // Draw filled square
      canvas_.DrawRect(canvas_x, canvas_y, 16, 16, sprite_color);

      // Draw sprite ID and name
      std::string sprite_text;
      if (sprite.id() >= 0) {  // sprite.id() is uint8_t so always < 256
        // Extract just the sprite name part (remove ID prefix)
        std::string full_name = zelda3::kSpriteDefaultNames[sprite.id()];
        auto space_pos = full_name.find(' ');
        if (space_pos != std::string::npos &&
            space_pos < full_name.length() - 1) {
          std::string sprite_name = full_name.substr(space_pos + 1);
          // Truncate long names
          if (sprite_name.length() > 10) {
            sprite_name = sprite_name.substr(0, 8) + "..";
          }
          sprite_text =
              absl::StrFormat("%02X %s", sprite.id(), sprite_name.c_str());
        } else {
          sprite_text = absl::StrFormat("%02X", sprite.id());
        }
      } else {
        sprite_text = absl::StrFormat("%02X", sprite.id());
      }

      canvas_.DrawText(sprite_text, canvas_x, canvas_y);
    }
  }
}

void DungeonCanvasViewer::RenderPotItems(const zelda3::Room& room) {
  // Skip if pot items are not visible
  if (!entity_visibility_.show_pot_items) {
    return;
  }

  const auto& pot_items = room.GetPotItems();
  
  // If no pot items in this room, nothing to render
  if (pot_items.empty()) {
    return;
  }

  // Pot item names
  static const char* kPotItemNames[] = {
      "Nothing",        // 0
      "Green Rupee",    // 1
      "Rock",           // 2
      "Bee",            // 3
      "Health",         // 4
      "Bomb",           // 5
      "Heart",          // 6
      "Blue Rupee",     // 7
      "Key",            // 8
      "Arrow",          // 9
      "Bomb",           // 10
      "Heart",          // 11
      "Magic",          // 12
      "Full Magic",     // 13
      "Cucco",          // 14
      "Green Soldier",  // 15
      "Bush Stal",      // 16
      "Blue Soldier",   // 17
      "Landmine",       // 18
      "Heart",          // 19
      "Fairy",          // 20
      "Heart",          // 21
      "Nothing",        // 22
      "Hole",           // 23
      "Warp",           // 24
      "Staircase",      // 25
      "Bombable",       // 26
      "Switch"          // 27
  };
  constexpr size_t kPotItemNameCount = sizeof(kPotItemNames) / sizeof(kPotItemNames[0]);

  // Pot items now have their own position data from ROM
  // No need to match to objects - each item has exact pixel coordinates
  for (const auto& pot_item : pot_items) {
    // Get pixel coordinates from PotItem structure
    int pixel_x = pot_item.GetPixelX();
    int pixel_y = pot_item.GetPixelY();
    
    // Convert to canvas coordinates (already in pixels, just need offset)
    // Note: pot item coords are already in full room pixel space
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(pixel_x / 8, pixel_y / 8);

    if (IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      // Draw colored square
      ImVec4 pot_item_color;
      if (pot_item.item == 0) {
        pot_item_color = ImVec4(0.5f, 0.5f, 0.5f, 0.5f);  // Gray for Nothing
      } else {
        pot_item_color = ImVec4(1.0f, 0.85f, 0.2f, 0.75f);  // Yellow for items
      }

      canvas_.DrawRect(canvas_x, canvas_y, 16, 16, pot_item_color);

      // Get item name
      std::string item_name;
      if (pot_item.item < kPotItemNameCount) {
        item_name = kPotItemNames[pot_item.item];
      } else {
        item_name = absl::StrFormat("Unk%02X", pot_item.item);
      }

      // Draw label above the box
      std::string item_text = absl::StrFormat("%02X %s", pot_item.item, item_name.c_str());
      canvas_.DrawText(item_text, canvas_x, canvas_y - 10);
    }
  }
}

void DungeonCanvasViewer::RenderEntityOverlay(const zelda3::Room& room) {
  // Render all entity overlays
  RenderSprites(room);
  RenderPotItems(room);
}

// Coordinate conversion helper functions
std::pair<int, int> DungeonCanvasViewer::RoomToCanvasCoordinates(
    int room_x, int room_y) const {
  // Convert room coordinates (tile units) to UNSCALED canvas pixel coordinates
  // Dungeon tiles are 8x8 pixels (not 16x16!)
  // IMPORTANT: Return UNSCALED coordinates - Canvas drawing functions apply
  // scale internally Do NOT multiply by scale here or we get double-scaling!

  // Simple conversion: tile units → pixel units (no scale, no offset)
  return {room_x * 8, room_y * 8};
}

std::pair<int, int> DungeonCanvasViewer::CanvasToRoomCoordinates(
    int canvas_x, int canvas_y) const {
  // Convert canvas screen coordinates (pixels) to room coordinates (tile units)
  // Input: Screen-space coordinates (affected by zoom/scale)
  // Output: Logical tile coordinates (0-63 for each axis)

  // IMPORTANT: Mouse coordinates are in screen space, must undo scale first
  float scale = canvas_.global_scale();
  if (scale <= 0.0f)
    scale = 1.0f;  // Prevent division by zero

  // Step 1: Convert screen space → logical pixel space
  int logical_x = static_cast<int>(canvas_x / scale);
  int logical_y = static_cast<int>(canvas_y / scale);

  // Step 2: Convert logical pixels → tile units (8 pixels per tile)
  return {logical_x / 8, logical_y / 8};
}

bool DungeonCanvasViewer::IsWithinCanvasBounds(int canvas_x, int canvas_y,
                                               int margin) const {
  // Check if coordinates are within canvas bounds with optional margin
  auto canvas_width = canvas_.width();
  auto canvas_height = canvas_.height();
  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= canvas_width + margin &&
          canvas_y <= canvas_height + margin);
}
// Room layout visualization

// Object visualization methods
void DungeonCanvasViewer::DrawObjectPositionOutlines(const zelda3::Room& room) {
  // Draw colored rectangles showing object positions
  // This helps visualize object placement even if graphics don't render
  // correctly

  const auto& theme = AgentUI::GetTheme();
  const auto& objects = room.GetTileObjects();

  // Create ObjectDrawer for accurate dimension calculation
  // ObjectDrawer uses game-accurate draw routine mapping to determine sizes
  // Note: const_cast needed because rom() accessor is non-const, but we don't modify ROM
  zelda3::ObjectDrawer drawer(const_cast<zelda3::Room&>(room).rom(), room.id(),
                              nullptr);

  for (const auto& obj : objects) {
    // Filter by object type (default to true if unknown type)
    bool show_this_type = true;  // Default to showing
    if (obj.id_ < 0x100) {
      show_this_type = object_outline_toggles_.show_type1_objects;
    } else if (obj.id_ >= 0x100 && obj.id_ < 0x200) {
      show_this_type = object_outline_toggles_.show_type2_objects;
    } else if (obj.id_ >= 0xF00) {
      show_this_type = object_outline_toggles_.show_type3_objects;
    }
    // else: unknown type, use default (true)

    // Filter by layer (default to true if unknown layer)
    bool show_this_layer = true;  // Default to showing
    if (obj.GetLayerValue() == 0) {
      show_this_layer = object_outline_toggles_.show_layer0_objects;
    } else if (obj.GetLayerValue() == 1) {
      show_this_layer = object_outline_toggles_.show_layer1_objects;
    } else if (obj.GetLayerValue() == 2) {
      show_this_layer = object_outline_toggles_.show_layer2_objects;
    }
    // else: unknown layer, use default (true)

    // Skip if filtered out
    if (!show_this_type || !show_this_layer) {
      continue;
    }

    // Convert object position (tile coordinates) to canvas pixel coordinates
    // (UNSCALED)
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(obj.x(), obj.y());

    // Calculate object dimensions using the shared dimension table when loaded
    int width = 16;
    int height = 16;
    auto& dim_table = zelda3::ObjectDimensionTable::Get();
    if (dim_table.IsLoaded()) {
      auto [w_tiles, h_tiles] = dim_table.GetDimensions(obj.id_, obj.size_);
      width = w_tiles * 8;
      height = h_tiles * 8;
    } else {
      auto [w, h] = drawer.CalculateObjectDimensions(obj);
      width = w;
      height = h;
    }

    // IMPORTANT: Do NOT apply canvas scale here - DrawRect handles it
    // Clamp to reasonable sizes (in logical space)
    width = std::min(width, 512);
    height = std::min(height, 512);

    // Color-code by layer
    ImVec4 outline_color;
    if (obj.GetLayerValue() == 0) {
      outline_color = theme.dungeon_outline_layer0;  // Red for layer 0
    } else if (obj.GetLayerValue() == 1) {
      outline_color = theme.dungeon_outline_layer1;  // Green for layer 1
    } else {
      outline_color = theme.dungeon_outline_layer2;  // Blue for layer 2
    }

    // Draw outline rectangle
    canvas_.DrawRect(canvas_x, canvas_y, width, height, outline_color);

    // Draw object ID label with hex ID and abbreviated name
    // Format: "0xNN Name" where name is truncated if needed
    std::string name = GetObjectName(obj.id_);
    // Truncate name to fit (approx 12 chars for small objects)
    if (name.length() > 12) {
      name = name.substr(0, 10) + "..";
    }
    std::string label;
    if (obj.id_ >= 0x100) {
      label = absl::StrFormat("0x%03X\n%s\n[%dx%d]", obj.id_, name.c_str(),
                              width, height);
    } else {
      label = absl::StrFormat("0x%02X\n%s\n[%dx%d]", obj.id_, name.c_str(),
                              width, height);
    }
    canvas_.DrawText(label, canvas_x + 1, canvas_y + 1);
  }
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  LOG_DEBUG("[LoadAndRender]", "START room_id=%d", room_id);

  if (room_id < 0 || room_id >= 128) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Invalid room ID");
    return absl::InvalidArgumentError("Invalid room ID");
  }

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: ROM not loaded");
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!rooms_) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data not available");
    return absl::FailedPreconditionError("Room data not available");
  }

  auto& room = (*rooms_)[room_id];
  LOG_DEBUG("[LoadAndRender]", "Got room reference");

  // Load room graphics with proper blockset
  LOG_DEBUG("[LoadAndRender]", "Loading graphics for blockset %d",
            room.blockset);
  room.LoadRoomGraphics(room.blockset);
  LOG_DEBUG("[LoadAndRender]", "Graphics loaded");

  // Load the room's palette with bounds checking
  if (!game_data_) {
    LOG_ERROR("[LoadAndRender]", "GameData not available");
    return absl::FailedPreconditionError("GameData not available");
  }
  const auto& dungeon_main = game_data_->palette_groups.dungeon_main;
  if (!dungeon_main.empty()) {
    int palette_id = room.palette;
    if (room.palette < game_data_->paletteset_ids.size()) {
      palette_id = game_data_->paletteset_ids[room.palette][0];
    }
    current_palette_group_id_ =
        std::min<uint64_t>(std::max(0, palette_id),
                           static_cast<int>(dungeon_main.size() - 1));

    auto full_palette = dungeon_main[current_palette_group_id_];
    ASSIGN_OR_RETURN(
        current_palette_group_,
        gfx::CreatePaletteGroupFromLargePalette(full_palette, 16));
    LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu",
              current_palette_group_id_);
  }

  // Render the room graphics (self-contained - handles all palette application)
  LOG_DEBUG("[LoadAndRender]", "Calling room.RenderRoomGraphics()...");
  room.RenderRoomGraphics();
  LOG_DEBUG("[LoadAndRender]",
            "RenderRoomGraphics() complete - room buffers self-contained");

  LOG_DEBUG("[LoadAndRender]", "SUCCESS");
  return absl::OkStatus();
}

void DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= zelda3::NumberOfRooms || !rooms_)
    return;

  auto& room = (*rooms_)[room_id];
  auto& layer_mgr = GetRoomLayerManager(room_id);

  // Apply room's layer merging settings to the manager
  layer_mgr.ApplyLayerMerging(room.layer_merging());

  float scale = canvas_.global_scale();

  // Composite mode: Use single merged bitmap instead of 4-layer sequential draw
  if (use_composite_mode_) {
    auto& composite = room.GetCompositeBitmap(layer_mgr);
    if (composite.is_active() && composite.width() > 0) {
      // Ensure texture exists or is updated when bitmap data changes
      if (!composite.texture()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &composite);
        composite.set_modified(false);  // Will be created fresh
      } else if (composite.modified()) {
        // CRITICAL: Update texture when bitmap was regenerated
        // This happens after property changes (blockset, palette, etc.)
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, &composite);
        composite.set_modified(false);  // Avoid redundant updates
      }
      if (composite.texture()) {
        canvas_.DrawBitmap(composite, 0, 0, scale, 255);
      }
    }
    return;
  }

  // Standard mode: Draw 4 layers sequentially (back to front)
  // Helper lambda to draw a single layer
  auto draw_layer = [&](zelda3::LayerType layer_type) {
    if (!layer_mgr.IsLayerVisible(layer_type))
      return;

    auto& buffer = zelda3::RoomLayerManager::GetLayerBuffer(room, layer_type);
    auto& bitmap = buffer.bitmap();

    if (!bitmap.is_active() || bitmap.width() == 0 || bitmap.height() == 0)
      return;

    // Ensure texture exists and is up-to-date
    if (!bitmap.texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bitmap);
      bitmap.set_modified(false);
    } else if (bitmap.modified()) {
      // Update texture when bitmap data has changed (e.g., after doors rendered)
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &bitmap);
      bitmap.set_modified(false);
    }

    if (!bitmap.texture())
      return;

    // Apply DarkRoom color modulation to match composite mode appearance
    // This ensures separate layer mode looks the same as merged mode
    layer_mgr.ApplySurfaceColorMod(bitmap.surface());

    // Get alpha from layer manager
    uint8_t alpha = layer_mgr.GetLayerAlpha(layer_type);

    LOG_DEBUG("DungeonCanvasViewer",
              "Drawing %s to canvas with texture %p, alpha=%d, scale=%.2f",
              zelda3::RoomLayerManager::GetLayerName(layer_type),
              bitmap.texture(), alpha, scale);

    canvas_.DrawBitmap(bitmap, 0, 0, scale, alpha);
  };

  // Draw layers in correct order (back to front)
  auto draw_order = layer_mgr.GetDrawOrder();
  for (auto layer_type : draw_order) {
    draw_layer(layer_type);
  }

  // DEBUG: Check if background buffers have content
  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  auto& bg2_bitmap = room.bg2_buffer().bitmap();

  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0) {
    LOG_DEBUG("DungeonCanvasViewer",
              "BG1 bitmap: %dx%d, active=%d, visible=%d, texture=%p",
              bg1_bitmap.width(), bg1_bitmap.height(), bg1_bitmap.is_active(),
              layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout),
              bg1_bitmap.texture());

    // Check bitmap data content
    auto& bg1_data = bg1_bitmap.mutable_data();
    int non_zero_pixels = 0;
    for (size_t i = 0; i < bg1_data.size();
         i += 100) {  // Sample every 100th pixel
      if (bg1_data[i] != 0)
        non_zero_pixels++;
    }
    LOG_DEBUG("DungeonCanvasViewer",
              "BG1 bitmap data: %zu pixels, ~%d non-zero samples",
              bg1_data.size(), non_zero_pixels);
  }
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0) {
    auto bg2_blend = layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout);
    LOG_DEBUG("DungeonCanvasViewer",
              "BG2 bitmap: %dx%d, active=%d, visible=%d, blend=%s, texture=%p",
              bg2_bitmap.width(), bg2_bitmap.height(), bg2_bitmap.is_active(),
              layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout),
              zelda3::RoomLayerManager::GetBlendModeName(bg2_blend),
              bg2_bitmap.texture());

    // Check bitmap data content
    auto& bg2_data = bg2_bitmap.mutable_data();
    int non_zero_pixels = 0;
    for (size_t i = 0; i < bg2_data.size();
         i += 100) {  // Sample every 100th pixel
      if (bg2_data[i] != 0)
        non_zero_pixels++;
    }
    LOG_DEBUG("DungeonCanvasViewer",
              "BG2 bitmap data: %zu pixels, ~%d non-zero samples",
              bg2_data.size(), non_zero_pixels);
  }

  // DEBUG: Show canvas and bitmap info
  LOG_DEBUG("DungeonCanvasViewer",
            "Canvas pos: (%.1f, %.1f), Canvas size: (%.1f, %.1f)",
            canvas_.zero_point().x, canvas_.zero_point().y, canvas_.width(),
            canvas_.height());
  LOG_DEBUG("DungeonCanvasViewer",
            "BG1 bitmap size: %dx%d, BG2 bitmap size: %dx%d",
            bg1_bitmap.width(), bg1_bitmap.height(), bg2_bitmap.width(),
            bg2_bitmap.height());
}

}  // namespace yaze::editor
