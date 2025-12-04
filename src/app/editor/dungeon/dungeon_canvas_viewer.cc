#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/core/input.h"
#include "dungeon_canvas_viewer.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/log.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {
// Helper to get object name from ID
std::string GetObjectName(int object_id) {
  if (object_id < 0x100) {
    // Type 1: Subtype 1 objects
    constexpr size_t kType1Count = sizeof(zelda3::Type1RoomObjectNames) /
                                   sizeof(zelda3::Type1RoomObjectNames[0]);
    if (object_id < (int)kType1Count) {
      return zelda3::Type1RoomObjectNames[object_id];
    }
    return absl::StrFormat("Unknown Type1 (0x%02X)", object_id);
  } else if (object_id >= 0x100 && object_id < 0x200) {
    // Type 2: Subtype 2 objects
    int idx = object_id - 0x100;
    constexpr size_t kType2Count = sizeof(zelda3::Type2RoomObjectNames) /
                                   sizeof(zelda3::Type2RoomObjectNames[0]);
    if (idx < (int)kType2Count) {
      return zelda3::Type2RoomObjectNames[idx];
    }
    return absl::StrFormat("Unknown Type2 (0x%03X)", object_id);
  } else if (object_id >= 0xF00) {
    // Type 3: Doors/special objects
    int idx = object_id - 0xF00;
    constexpr size_t kType3Count = sizeof(zelda3::Type3RoomObjectNames) /
                                   sizeof(zelda3::Type3RoomObjectNames[0]);
    if (idx < (int)kType3Count) {
      return zelda3::Type3RoomObjectNames[idx];
    }
    return absl::StrFormat("Unknown Type3 (0x%03X)", object_id);
  }
  return absl::StrFormat("Unknown (0x%03X)", object_id);
}
}  // namespace

void DungeonCanvasViewer::Draw(int room_id) { DrawDungeonCanvas(room_id); }

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
    if (scale <= 0.0f) scale = 1.0f;

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
    ImGui::Text(ICON_MD_TUNE " Room Properties");

    if (ImGui::BeginTable(
            "RoomPropsTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Value");

      // Static UI values need to be updated when switching rooms
      // Track the room ID to detect room changes
      static int prev_ui_room_id = -1;
      static uint8_t blockset_val = 0;
      static uint8_t spriteset_val = 0;
      static uint8_t palette_val = 0;
      static uint8_t floor1_val = 0;
      static uint8_t floor2_val = 0;
      static int effect_val = 0;
      static uint8_t tag1_val = 0;
      static uint8_t tag2_val = 0;

      // Update cached UI values when room changes
      if (prev_ui_room_id != room_id) {
        blockset_val = room.blockset;
        spriteset_val = room.spriteset;
        palette_val = room.palette;
        floor1_val = room.floor1();
        floor2_val = room.floor2();
        effect_val = static_cast<int>(room.effect());
        tag1_val = static_cast<uint8_t>(room.tag1());
        tag2_val = static_cast<uint8_t>(room.tag2());
        prev_ui_room_id = room_id;
      }

      // Graphics Section
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Blockset");
      ImGui::TableNextColumn();
      if (gui::InputHexByte("##Blockset", &blockset_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.SetBlockset(blockset_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Spriteset");
      ImGui::TableNextColumn();
      if (gui::InputHexByte("##Spriteset", &spriteset_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.SetSpriteset(spriteset_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Palette");
      ImGui::TableNextColumn();
      if (gui::InputHexByte("##Palette", &palette_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.SetPalette(palette_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      // Layout Section
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Layout ID");
      ImGui::TableNextColumn();
      uint8_t layout_val = room.layout;
      if (gui::InputHexByte("##Layout", &layout_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.layout = layout_val;
        room.MarkLayoutDirty();
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Floor 1");
      ImGui::TableNextColumn();
      if (gui::InputHexByte("##Floor1", &floor1_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.set_floor1(floor1_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Floor 2");
      ImGui::TableNextColumn();
      if (gui::InputHexByte("##Floor2", &floor2_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.set_floor2(floor2_val);
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      // Advanced Section
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Effect");
      ImGui::TableNextColumn();
      static const char* effect_names[] = {
          "Nothing", "One",         "Moving Floor",     "Moving Water",
          "Four",    "Red Flashes", "Torch Show Floor", "Ganon Room"};
      if (ImGui::Combo("##Effect", &effect_val, effect_names, 8)) {
        room.SetEffect(static_cast<zelda3::EffectKey>(effect_val));
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Tags");
      ImGui::TableNextColumn();
      ImGui::SetNextItemWidth(80);
      if (gui::InputHexByte("##Tag1Val", &tag1_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.SetTag1(static_cast<zelda3::TagKey>(tag1_val));
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }
      ImGui::SameLine();
      ImGui::Text("Tag 2:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      if (gui::InputHexByte("##Tag2Val", &tag2_val) &&
          ImGui::IsItemDeactivatedAfterEdit()) {
        room.SetTag2(static_cast<zelda3::TagKey>(tag2_val));
        if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
      }

      ImGui::EndTable();
    }

    // Layer visibility controls (4-way: Layout/Objects × BG1/BG2)
    ImGui::Separator();
    ImGui::Text(ICON_MD_LAYERS " Layer Controls");

    auto& layer_mgr = GetRoomLayerManager(room_id);

    // Apply room's layer merging settings
    layer_mgr.ApplyLayerMerging(room.layer_merging());

    if (ImGui::BeginTable("##LayerControls", 3,
                          ImGuiTableFlags_SizingStretchSame |
                              ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f);
      ImGui::TableSetupColumn("Layout");
      ImGui::TableSetupColumn("Objects");
      ImGui::TableHeadersRow();

      // BG1 row
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("BG1");

      ImGui::TableNextColumn();
      bool bg1_layout =
          layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
      if (ImGui::Checkbox("##BG1L", &bg1_layout)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1_layout);
      }

      ImGui::TableNextColumn();
      bool bg1_objects =
          layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
      if (ImGui::Checkbox("##BG1O", &bg1_objects)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1_objects);
      }

      // BG2 row
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("BG2");

      ImGui::TableNextColumn();
      bool bg2_layout =
          layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
      if (ImGui::Checkbox("##BG2L", &bg2_layout)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2_layout);
      }

      ImGui::TableNextColumn();
      bool bg2_objects =
          layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
      if (ImGui::Checkbox("##BG2O", &bg2_objects)) {
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2_objects);
      }

      ImGui::EndTable();
    }

    // Blend mode and merge controls
    if (ImGui::BeginTable("##BlendControls", 2,
                          ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      // BG2 blend mode
      const char* blend_modes[] = {"Normal", "Trans", "Add", "Dark", "Off"};
      int bg2_blend = static_cast<int>(
          layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Combo("BG2##Blend", &bg2_blend, blend_modes, 5)) {
        auto mode = static_cast<zelda3::LayerBlendMode>(bg2_blend);
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout, mode);
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects, mode);
      }

      ImGui::TableNextColumn();
      // Layer merge type (ROM property)
      const char* merge_types[] = {"Off",    "Parallax",    "Dark",
                                   "On top", "Translucent", "Addition",
                                   "Normal", "Transparent", "Dark room"};
      int merge_val = room.layer_merging().ID;
      ImGui::SetNextItemWidth(-FLT_MIN);
      if (ImGui::Combo("Merge##Type", &merge_val, merge_types, 9)) {
        room.SetLayerMerging(zelda3::kLayerMergeTypeList[merge_val]);
        layer_mgr.ApplyLayerMerging(room.layer_merging());
      }

      ImGui::EndTable();
    }
  }

  ImGui::EndGroup();

  // CRITICAL: Draw canvas with explicit size to ensure viewport matches content
  // Pass the unscaled room size directly to DrawBackground
  canvas_.DrawBackground(ImVec2(kRoomPixelWidth, kRoomPixelHeight));

  // DEBUG: Log canvas state after DrawBackground
  if (debug_frame_count % 60 == 1) {
    LOG_DEBUG("[DungeonCanvas]",
              "After DrawBackground: canvas_sz=(%.0f,%.0f) "
              "canvas_p0=(%.0f,%.0f) canvas_p1=(%.0f,%.0f)",
              canvas_.canvas_size().x, canvas_.canvas_size().y,
              canvas_.zero_point().x, canvas_.zero_point().y,
              canvas_.zero_point().x + canvas_.canvas_size().x,
              canvas_.zero_point().y + canvas_.canvas_size().y);
  }

  // Add dungeon-specific context menu items
  canvas_.ClearContextMenuItems();

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Add object placement option
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        "Place Object", ICON_MD_ADD,
        []() {
          // TODO: Show object palette/selector
        },
        "Ctrl+P"));

    // Add object deletion for selected objects
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        "Delete Selected", ICON_MD_DELETE,
        [this]() { object_interaction_.HandleDeleteSelected(); }, "Del"));

    // Add room property quick toggles (4-way layer visibility)
    gui::CanvasMenuItem layer_menu;
    layer_menu.label = ICON_MD_LAYERS " Layer Visibility";

    layer_menu.subitems.push_back(gui::CanvasMenuItem(
        "BG1 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout));
        }));
    layer_menu.subitems.push_back(gui::CanvasMenuItem(
        "BG1 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects));
        }));
    layer_menu.subitems.push_back(gui::CanvasMenuItem(
        "BG2 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout));
        }));
    layer_menu.subitems.push_back(gui::CanvasMenuItem(
        "BG2 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects));
        }));

    canvas_.AddContextMenuItem(layer_menu);

    // Add re-render option
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        " Re-render Room", ICON_MD_REFRESH,
        [&room]() { room.RenderRoomGraphics(); }, "Ctrl+R"));

    // Grid Options
    gui::CanvasMenuItem grid_menu;
    grid_menu.label = ICON_MD_GRID_ON " Grid Options";

    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("8x8", [this]() { custom_grid_size_ = 8; }));
    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("16x16", [this]() { custom_grid_size_ = 16; }));
    grid_menu.subitems.push_back(
        gui::CanvasMenuItem("32x32", [this]() { custom_grid_size_ = 32; }));

    canvas_.AddContextMenuItem(grid_menu);

    // === DEBUG MENU ===
    gui::CanvasMenuItem debug_menu;
    debug_menu.label = ICON_MD_BUG_REPORT " Debug";

    // Show room info
    debug_menu.subitems.push_back(gui::CanvasMenuItem(
        ICON_MD_INFO " Show Room Info", ICON_MD_INFO,
        [this]() { show_room_debug_info_ = !show_room_debug_info_; }));

    // Show texture info
    debug_menu.subitems.push_back(gui::CanvasMenuItem(
        ICON_MD_IMAGE " Show Texture Debug", ICON_MD_IMAGE,
        [this]() { show_texture_debug_ = !show_texture_debug_; }));

    // Show object bounds with sub-menu for categories
    gui::CanvasMenuItem object_bounds_menu;
    object_bounds_menu.label = ICON_MD_CROP_SQUARE " Show Object Bounds";
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
    sep.enabled_condition = []() { return false; };
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
    debug_menu.subitems.push_back(gui::CanvasMenuItem(
        ICON_MD_LAYERS " Show Layer Info", ICON_MD_LAYERS,
        [this]() { show_layer_info_ = !show_layer_info_; }));

    // Force reload room
    debug_menu.subitems.push_back(gui::CanvasMenuItem(
        ICON_MD_REFRESH " Force Reload", ICON_MD_REFRESH, [&room]() {
          room.LoadObjects();
          room.LoadRoomGraphics(room.blockset);
          room.RenderRoomGraphics();
        }));

    // Log room state
    debug_menu.subitems.push_back(gui::CanvasMenuItem(
        ICON_MD_PRINT " Log Room State", ICON_MD_PRINT, [&room, room_id]() {
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
        }));

    canvas_.AddContextMenuItem(debug_menu);
  }

  // Add object interaction menu items to canvas context menu
  if (object_interaction_enabled_) {
    auto& interaction = object_interaction_;
    auto selected = interaction.GetSelectedObjectIndices();

    if (!selected.empty()) {
      // Show selection info
      if (selected.size() == 1 && rooms_) {
        auto& room = (*rooms_)[room_id];
        const auto& objects = room.GetTileObjects();
        if (selected[0] < objects.size()) {
          const auto& obj = objects[selected[0]];
          std::string name = GetObjectName(obj.id_);
          canvas_.AddContextMenuItem(gui::CanvasMenuItem::Disabled(
              absl::StrFormat("Object 0x%02X: %s", obj.id_, name.c_str())));
        }
      }

      canvas_.AddContextMenuItem(gui::CanvasMenuItem(
          ICON_MD_CONTENT_CUT " Cut", ICON_MD_CONTENT_CUT,
          [&interaction]() {
            interaction.HandleCopySelected();
            interaction.HandleDeleteSelected();
          },
          "Ctrl+X"));

      canvas_.AddContextMenuItem(gui::CanvasMenuItem(
          ICON_MD_CONTENT_COPY " Copy", ICON_MD_CONTENT_COPY,
          [&interaction]() { interaction.HandleCopySelected(); }, "Ctrl+C"));

      canvas_.AddContextMenuItem(gui::CanvasMenuItem(
          ICON_MD_CONTENT_PASTE " Duplicate", ICON_MD_CONTENT_PASTE,
          [&interaction]() {
            interaction.HandleCopySelected();
            interaction.HandlePasteObjects();
          },
          "Ctrl+D"));

      canvas_.AddContextMenuItem(gui::CanvasMenuItem(
          ICON_MD_DELETE " Delete", ICON_MD_DELETE,
          [&interaction]() { interaction.HandleDeleteSelected(); }, "Del"));
    }

    if (interaction.HasClipboardData()) {
      canvas_.AddContextMenuItem(gui::CanvasMenuItem(
          ICON_MD_CONTENT_PASTE " Paste", ICON_MD_CONTENT_PASTE,
          [&interaction]() { interaction.HandlePasteObjects(); }, "Ctrl+V"));
    }
  }

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

    // Render sprites as simple 16x16 squares with labels
    // (Sprites are not part of the background buffers)
    RenderSprites(room);

    // Handle object interaction if enabled
    if (object_interaction_enabled_) {
      object_interaction_.HandleCanvasMouseInput();
      object_interaction_.CheckForObjectSelection();
      object_interaction_.DrawSelectBox();
      object_interaction_
          .DrawSelectionHighlights();  // Draw selection highlights on top
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

  canvas_.DrawGrid();
  canvas_.DrawOverlay();

  // Draw layer information overlay
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    std::string layer_info =
        absl::StrFormat("Room %03X - Objects: %zu, Sprites: %zu\n", room_id,
                        room.GetTileObjects().size(), room.GetSprites().size());

    canvas_.DrawText(layer_info, 10, canvas_.height() - 60);
  }
}

void DungeonCanvasViewer::DisplayObjectInfo(const zelda3::RoomObject& object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay with hex ID and name
  std::string name = GetObjectName(object.id_);
  std::string info_text;
  if (object.id_ >= 0x100) {
    info_text = absl::StrFormat("0x%03X %s (X:%d Y:%d S:0x%02X)", object.id_,
                                name.c_str(), object.x_, object.y_,
                                object.size_);
  } else {
    info_text = absl::StrFormat("0x%02X %s (X:%d Y:%d S:0x%02X)", object.id_,
                                name.c_str(), object.x_, object.y_,
                                object.size_);
  }

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderSprites(const zelda3::Room& room) {
  const auto& theme = AgentUI::GetTheme();

  // Render sprites as simple 8x8 squares with sprite name/ID
  for (const auto& sprite : room.GetSprites()) {
    auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(sprite.x(), sprite.y());

    if (IsWithinCanvasBounds(canvas_x, canvas_y, 8)) {
      // Draw 8x8 square for sprite
      ImVec4 sprite_color;

      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = theme.dungeon_sprite_layer0;  // Green for layer 0
      } else {
        sprite_color = theme.dungeon_sprite_layer1;  // Blue for layer 1
      }

      canvas_.DrawRect(canvas_x, canvas_y, 8, 8, sprite_color);

      // Draw sprite border
      canvas_.DrawRect(canvas_x, canvas_y, 8, 8, theme.panel_border_color);

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
          if (sprite_name.length() > 8) {
            sprite_name = sprite_name.substr(0, 8) + "...";
          }
          sprite_text =
              absl::StrFormat("%02X\n%s", sprite.id(), sprite_name.c_str());
        } else {
          sprite_text = absl::StrFormat("%02X", sprite.id());
        }
      } else {
        sprite_text = absl::StrFormat("%02X", sprite.id());
      }

      canvas_.DrawText(sprite_text, canvas_x + 18, canvas_y);
    }
  }
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
  if (scale <= 0.0f) scale = 1.0f;  // Prevent division by zero

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

void DungeonCanvasViewer::CalculateWallDimensions(
    const zelda3::RoomObject& object, int& width, int& height) {
  // Default base size
  width = 8;
  height = 8;

  // For walls, use the size field to determine length and orientation
  if (object.id_ >= 0x10 && object.id_ <= 0x1F) {
    // Wall objects: size determines length and orientation
    uint8_t size_x = object.size_ & 0x0F;
    uint8_t size_y = (object.size_ >> 4) & 0x0F;

    // Walls can be horizontal or vertical based on size parameters
    if (size_x > size_y) {
      // Horizontal wall
      width = 8 + size_x * 8;  // Each unit adds 8 pixels
      height = 8;
    } else if (size_y > size_x) {
      // Vertical wall
      width = 8;
      height = 8 + size_y * 8;
    } else {
      // Square wall or corner
      width = 8 + size_x * 4;
      height = 8 + size_y * 4;
    }
  } else {
    // For other objects, use standard size calculation
    width = 8 + (object.size_ & 0x0F) * 4;
    height = 8 + ((object.size_ >> 4) & 0x0F) * 4;
  }

  // Clamp to reasonable limits
  width = std::min(width, 256);
  height = std::min(height, 256);
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
  zelda3::ObjectDrawer drawer(const_cast<zelda3::Room&>(room).rom(), room.id(), nullptr);

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

    // Calculate object dimensions using ObjectDrawer for game-accurate sizes
    // This uses the same draw routine mapping as the actual rendering
    auto [width, height] = drawer.CalculateObjectDimensions(obj);

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
  if (room.palette < game_data_->paletteset_ids.size() &&
      !game_data_->paletteset_ids[room.palette].empty()) {
    auto dungeon_palette_ptr = game_data_->paletteset_ids[room.palette][0];
    auto palette_id = rom_->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.ok()) {
      current_palette_group_id_ = palette_id.value() / 180;
      if (current_palette_group_id_ <
          game_data_->palette_groups.dungeon_main.size()) {
        auto full_palette =
            game_data_->palette_groups.dungeon_main[current_palette_group_id_];
        // TODO: Fix palette assignment to buffer.
        ASSIGN_OR_RETURN(
            current_palette_group_,
            gfx::CreatePaletteGroupFromLargePalette(full_palette, 16));
        LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu",
                  current_palette_group_id_);
      }
    }
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
  if (room_id < 0 || room_id >= zelda3::NumberOfRooms || !rooms_) return;

  auto& room = (*rooms_)[room_id];
  auto& layer_mgr = GetRoomLayerManager(room_id);

  // Apply room's layer merging settings to the manager
  layer_mgr.ApplyLayerMerging(room.layer_merging());

  float scale = canvas_.global_scale();

  // Helper lambda to draw a single layer
  auto draw_layer = [&](zelda3::LayerType layer_type) {
    if (!layer_mgr.IsLayerVisible(layer_type)) return;

    auto& buffer = zelda3::RoomLayerManager::GetLayerBuffer(room, layer_type);
    auto& bitmap = buffer.bitmap();

    if (!bitmap.is_active() || bitmap.width() == 0 || bitmap.height() == 0)
      return;

    // Ensure texture exists
    if (!bitmap.texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &bitmap);
    }

    if (!bitmap.texture()) return;

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
      if (bg1_data[i] != 0) non_zero_pixels++;
    }
    LOG_DEBUG("DungeonCanvasViewer",
              "BG1 bitmap data: %zu pixels, ~%d non-zero samples",
              bg1_data.size(), non_zero_pixels);
  }
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0) {
    auto bg2_blend = layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout);
    LOG_DEBUG(
        "DungeonCanvasViewer",
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
      if (bg2_data[i] != 0) non_zero_pixels++;
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
