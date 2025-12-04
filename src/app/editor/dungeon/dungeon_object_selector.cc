// Related header
#include "dungeon_object_selector.h"

// C system headers
#include <cstring>

// C++ standard library headers
#include <algorithm>
#include <iterator>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas.h"
#include "app/gui/widgets/asset_browser.h"
#include "app/platform/window.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_editor_system.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/dungeon_object_registry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"  // For GetObjectName()

namespace yaze::editor {

using ImGui::BeginChild;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;

void DungeonObjectSelector::DrawTileSelector() {
  EnsureRegistryInitialized();
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)3);
          BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                     ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawRoomGraphics();
      }
      EndChild();
      EndTabItem();
    }

    if (ImGui::BeginTabItem("Object Renderer")) {
      DrawObjectRenderer();
      EndTabItem();
    }
    EndTabBar();
  }
}

void DungeonObjectSelector::DrawObjectRenderer() {
  EnsureRegistryInitialized();
  // Use AssetBrowser for better object selection
  if (ImGui::BeginTable(
          "DungeonObjectEditorTable", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
              ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
              ImGuiTableFlags_BordersV,
          ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Object Browser", ImGuiTableColumnFlags_WidthFixed,
                            400);
    ImGui::TableSetupColumn("Preview Canvas",
                            ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Left column: AssetBrowser for object selection
    ImGui::TableNextColumn();
    ImGui::BeginChild("AssetBrowser", ImVec2(0, 0), true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    DrawObjectAssetBrowser();

    ImGui::EndChild();

    // Right column: Preview and placement controls
    ImGui::TableNextColumn();
    ImGui::BeginChild("PreviewCanvas", ImVec2(0, 0), true);

    // Object placement controls
    ImGui::SeparatorText("Object Placement");
    ImGui::InputInt("X Position", &place_x_);
    ImGui::InputInt("Y Position", &place_y_);

    if (ImGui::Button("Place Object") && object_loaded_) {
      PlaceObjectAtPosition(place_x_, place_y_);
    }

    ImGui::Separator();

    // Preview canvas
    object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    object_canvas_.DrawContextMenu();
    object_canvas_.DrawGrid(32.0f);

    // Render selected object preview with primitive fallback
    if (object_loaded_ && preview_object_.id_ >= 0) {
      int preview_x = 128 - 16;  // Center horizontally
      int preview_y = 128 - 16;  // Center vertically

      // TODO: Implement preview using ObjectDrawer + small BackgroundBuffer
      // For now, use primitive shape rendering (shows object ID and rough
      // dimensions)
      RenderObjectPrimitive(preview_object_, preview_x, preview_y);
    }

    object_canvas_.DrawOverlay();
    ImGui::EndChild();
    ImGui::EndTable();
  }

  // Object details window
  if (object_loaded_) {
    ImGui::Begin("Object Details", &object_loaded_, 0);
    ImGui::Text("Object ID: 0x%02X", preview_object_.id_);
    ImGui::Text("Position: (%d, %d)", preview_object_.x_, preview_object_.y_);
    ImGui::Text("Size: 0x%02X", preview_object_.size_);
    ImGui::Text("Layer: %d", static_cast<int>(preview_object_.layer_));

    // Add object placement controls
    ImGui::Separator();
    ImGui::Text("Placement Controls:");
    ImGui::InputInt("X Position", &place_x_);
    ImGui::InputInt("Y Position", &place_y_);

    if (ImGui::Button("Place Object")) {
      PlaceObjectAtPosition(place_x_, place_y_);
    }

    ImGui::End();
  }
}

void DungeonObjectSelector::Draw() {
  if (ImGui::BeginTabBar("##ObjectSelectorTabBar")) {
    // Object Selector tab - for placing objects with new AssetBrowser
    if (ImGui::BeginTabItem("Object Selector")) {
      DrawObjectRenderer();
      ImGui::EndTabItem();
    }

    // Room Graphics tab - 8 bitmaps viewer
    if (ImGui::BeginTabItem("Room Graphics")) {
      DrawRoomGraphics();
      ImGui::EndTabItem();
    }

    // Object Editor tab - experimental editor
    if (ImGui::BeginTabItem("Object Editor")) {
      DrawIntegratedEditingPanels();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void DungeonObjectSelector::DrawRoomGraphics() {
  const auto height = 0x40;
  room_gfx_canvas_.DrawBackground();
  room_gfx_canvas_.DrawContextMenu();
  room_gfx_canvas_.DrawTileSelector(32);

  if (rom_ && rom_->is_loaded() && rooms_) {
    int active_room_id = current_room_id_;
    auto& room = (*rooms_)[active_room_id];
    auto blocks = room.blocks();

    // Load graphics for this room if not already loaded
    if (blocks.empty()) {
      room.LoadRoomGraphics(room.blockset);
      blocks = room.blocks();
    }

    int current_block = 0;
    const int max_blocks_per_row = 2;  // 2 blocks per row for 300px column
    const int block_width = 128;       // Reduced size to fit column
    const int block_height = 32;       // Reduced height

    for (int block : blocks) {
      if (current_block >= 16)
        break;  // Only show first 16 blocks

      // Ensure the graphics sheet is loaded and has a valid texture
      if (block < gfx::Arena::Get().gfx_sheets().size()) {
        auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block];

        // Calculate position in a grid layout instead of horizontal
        // concatenation
        int row = current_block / max_blocks_per_row;
        int col = current_block % max_blocks_per_row;

        int x = room_gfx_canvas_.zero_point().x + 2 + (col * block_width);
        int y = room_gfx_canvas_.zero_point().y + 2 + (row * block_height);

        // Ensure we don't exceed canvas bounds
        if (x + block_width <=
                room_gfx_canvas_.zero_point().x + room_gfx_canvas_.width() &&
            y + block_height <=
                room_gfx_canvas_.zero_point().y + room_gfx_canvas_.height()) {
          // Only draw if the texture is valid
          if (gfx_sheet.texture() != 0) {
            room_gfx_canvas_.draw_list()->AddImage(
                (ImTextureID)(intptr_t)gfx_sheet.texture(), ImVec2(x, y),
                ImVec2(x + block_width, y + block_height));
          }
        }
      }
      current_block += 1;
    }
  }
  room_gfx_canvas_.DrawGrid(32.0f);
  room_gfx_canvas_.DrawOverlay();
}

void DungeonObjectSelector::DrawIntegratedEditingPanels() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_ || !object_editor_) {
    ImGui::Text("Editor systems not initialized");
    return;
  }

  // Create a tabbed interface for different editing modes
  if (ImGui::BeginTabBar("##EditingPanels")) {
    // Object Editor Tab
    if (ImGui::BeginTabItem("Objects")) {
      DrawCompactObjectEditor();
      ImGui::EndTabItem();
    }

    // Sprite Editor Tab
    if (ImGui::BeginTabItem("Sprites")) {
      DrawCompactSpriteEditor();
      ImGui::EndTabItem();
    }

    // Item Editor Tab
    if (ImGui::BeginTabItem("Items")) {
      DrawCompactItemEditor();
      ImGui::EndTabItem();
    }

    // Entrance Editor Tab
    if (ImGui::BeginTabItem("Entrances")) {
      DrawCompactEntranceEditor();
      ImGui::EndTabItem();
    }

    // Door Editor Tab
    if (ImGui::BeginTabItem("Doors")) {
      DrawCompactDoorEditor();
      ImGui::EndTabItem();
    }

    // Chest Editor Tab
    if (ImGui::BeginTabItem("Chests")) {
      DrawCompactChestEditor();
      ImGui::EndTabItem();
    }

    // Properties Tab
    if (ImGui::BeginTabItem("Properties")) {
      DrawCompactPropertiesEditor();
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void DungeonObjectSelector::DrawCompactObjectEditor() {
  if (!object_editor_) {
    ImGui::Text("Object editor not initialized");
    return;
  }

  auto& editor = *object_editor_;

  ImGui::Text("Object Editor");
  Separator();

  // Display current editing mode
  auto mode = editor.GetMode();
  const char* mode_names[] = {"Select", "Insert", "Delete",
                              "Edit",   "Layer",  "Preview"};
  ImGui::Text("Mode: %s", mode_names[static_cast<int>(mode)]);

  // Compact mode selection
  if (ImGui::Button("Select"))
    editor.SetMode(zelda3::DungeonObjectEditor::Mode::kSelect);
  ImGui::SameLine();
  if (ImGui::Button("Insert"))
    editor.SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
  ImGui::SameLine();
  if (ImGui::Button("Edit"))
    editor.SetMode(zelda3::DungeonObjectEditor::Mode::kEdit);

  // Layer and object type selection
  int current_layer = editor.GetCurrentLayer();
  if (ImGui::SliderInt("Layer", &current_layer, 0, 2)) {
    editor.SetCurrentLayer(current_layer);
  }

  int current_object_type = editor.GetCurrentObjectType();
  if (ImGui::InputInt("Object Type", &current_object_type, 1, 16)) {
    if (current_object_type >= 0 && current_object_type <= 0x3FF) {
      editor.SetCurrentObjectType(current_object_type);
    }
  }

  // Quick configuration checkboxes
  auto config = editor.GetConfig();
  if (ImGui::Checkbox("Snap to Grid", &config.snap_to_grid)) {
    editor.SetConfig(config);
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Show Grid", &config.show_grid)) {
    editor.SetConfig(config);
  }

  // Object count and selection info
  Separator();
  ImGui::Text("Objects: %zu", editor.GetObjectCount());

  auto selection = editor.GetSelection();
  if (!selection.selected_objects.empty()) {
    ImGui::Text("Selected: %zu", selection.selected_objects.size());
  }

  // Undo/Redo buttons
  Separator();
  if (ImGui::Button("Undo") && editor.CanUndo()) {
    (void)editor.Undo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Redo") && editor.CanRedo()) {
    (void)editor.Redo();
  }
}

ImU32 DungeonObjectSelector::GetObjectTypeColor(int object_id) {
  const auto& theme = AgentUI::GetTheme();

  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (object_id >= 0xF80 && object_id <= 0xF8F) {
      return IM_COL32(100, 200, 255, 255);  // Light blue for layer indicators
    } else if (object_id >= 0xF90 && object_id <= 0xF9F) {
      return IM_COL32(255, 200, 100, 255);  // Orange for door indicators
    } else {
      return IM_COL32(200, 150, 255, 255);  // Purple for misc Type 3
    }
  }

  // Type 2 objects (0x100-0x141) - Torches, blocks, switches
  if (object_id >= 0x100 && object_id < 0x200) {
    if (object_id >= 0x100 && object_id <= 0x10F) {
      return IM_COL32(255, 150, 50, 255);  // Orange for torches
    } else if (object_id >= 0x110 && object_id <= 0x11F) {
      return IM_COL32(150, 150, 200, 255);  // Blue-gray for blocks
    } else if (object_id >= 0x120 && object_id <= 0x12F) {
      return IM_COL32(100, 200, 100, 255);  // Green for switches
    } else if (object_id >= 0x130 && object_id <= 0x13F) {
      return ImGui::GetColorU32(theme.dungeon_selection_primary);  // Yellow for stairs
    } else {
      return IM_COL32(180, 180, 180, 255);  // Gray for other Type 2
    }
  }

  // Type 1 objects (0x00-0xFF) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return ImGui::GetColorU32(theme.dungeon_object_wall);  // Gray for walls
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for floors
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return ImGui::GetColorU32(theme.dungeon_object_chest);  // Gold for chests
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for doors
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return ImGui::GetColorU32(theme.dungeon_object_pot);  // Saddle brown for pots
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return ImGui::GetColorU32(theme.dungeon_object_decoration);  // Dim gray for decorations
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return IM_COL32(120, 120, 180, 255);  // Blue-gray for corners
  } else {
    return ImGui::GetColorU32(theme.dungeon_object_default);  // Default gray
  }
}

std::string DungeonObjectSelector::GetObjectTypeSymbol(int object_id) {
  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (object_id >= 0xF80 && object_id <= 0xF8F) {
      return "L";  // Layer
    } else if (object_id >= 0xF90 && object_id <= 0xF9F) {
      return "D";  // Door indicator
    } else {
      return "S";  // Special
    }
  }

  // Type 2 objects (0x100-0x141) - Torches, blocks, switches
  if (object_id >= 0x100 && object_id < 0x200) {
    if (object_id >= 0x100 && object_id <= 0x10F) {
      return "*";  // Torch (flame)
    } else if (object_id >= 0x110 && object_id <= 0x11F) {
      return "#";  // Block
    } else if (object_id >= 0x120 && object_id <= 0x12F) {
      return "o";  // Switch
    } else if (object_id >= 0x130 && object_id <= 0x13F) {
      return "^";  // Stairs
    } else {
      return "2";  // Type 2
    }
  }

  // Type 1 objects (0x00-0xFF) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return "|";  // Wall
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return "_";  // Floor
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return "C";  // Chest
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return "+";  // Door
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return "o";  // Pot
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return "~";  // Decoration
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return "/";  // Corner
  } else {
    return "?";  // Unknown
  }
}

void DungeonObjectSelector::RenderObjectPrimitive(
    const zelda3::RoomObject& object, int x, int y) {
  const auto& theme = AgentUI::GetTheme();
  // Render object as primitive shape on canvas
  ImU32 color = GetObjectTypeColor(object.id_);

  // Calculate object size with proper wall length handling
  int obj_width, obj_height;
  CalculateObjectDimensions(object, obj_width, obj_height);

  // Draw object rectangle
  ImVec4 color_vec = ImGui::ColorConvertU32ToFloat4(color);
  object_canvas_.DrawRect(x, y, obj_width, obj_height, color_vec);
  object_canvas_.DrawRect(x, y, obj_width, obj_height, theme.panel_bg_darker);

  // Draw object ID as text
  std::string obj_text = absl::StrFormat("0x%X", object.id_);
  object_canvas_.DrawText(obj_text, x + obj_width + 2, y + 4);
}

void DungeonObjectSelector::SelectObject(int obj_id) {
  selected_object_id_ = obj_id;

  // Create and update preview object
  preview_object_ = zelda3::RoomObject(obj_id, 0, 0, 0x12, 0);
  preview_object_.SetRom(rom_);
  if (game_data_) {
    auto palette =
        game_data_->palette_groups.dungeon_main[current_palette_group_id_];
    preview_palette_ = palette;
  }
  object_loaded_ = true;

  // Notify callback
  if (object_selected_callback_) {
    object_selected_callback_(preview_object_);
  }
}

void DungeonObjectSelector::DrawObjectAssetBrowser() {
  const auto& theme = AgentUI::GetTheme();

  // Subtype tabs
  if (ImGui::BeginTabBar("##ObjectSubtypeTabs")) {
    if (ImGui::BeginTabItem("Type 1 (0x00-0xFF)")) {
      object_subtype_tab_ = 0;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Type 2 (0x100-0x141)")) {
      object_subtype_tab_ = 1;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Type 3 (0xF80-0xFFF)")) {
      object_subtype_tab_ = 2;
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  // Determine ID range based on subtype tab
  int start_id, end_id;
  switch (object_subtype_tab_) {
    case 1:  // Type 2
      start_id = 0x100;
      end_id = 0x141;  // ~66 objects
      break;
    case 2:  // Type 3
      start_id = 0xF80;
      end_id = 0xFFF;  // ~128 objects
      break;
    default:  // Type 1
      start_id = 0x00;
      end_id = 0xFF;  // 256 objects
      break;
  }

  // Preview toggle (disabled by default for performance)
  ImGui::Checkbox("Show Graphical Previews", &enable_object_previews_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Enable to show actual object graphics.\n"
        "Requires a room to be loaded.\n"
        "May impact performance.");
  }
  ImGui::SameLine();
  ImGui::TextDisabled("(%d objects)", end_id - start_id + 1);

  // Create asset browser-style grid
  const float item_size = 80.0f;
  const float item_spacing = 8.0f;
  const int columns = std::max(
      1, static_cast<int>((ImGui::GetContentRegionAvail().x - item_spacing) /
                          (item_size + item_spacing)));

  // Scrollable child region for grid
  float child_height = ImGui::GetContentRegionAvail().y - 20;
  if (ImGui::BeginChild("##ObjectGrid", ImVec2(0, child_height), false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    int current_column = 0;
    int items_drawn = 0;

    for (int obj_id = start_id; obj_id <= end_id; ++obj_id) {
      if (current_column > 0) {
        ImGui::SameLine();
      }

      ImGui::PushID(obj_id);

      // Create selectable button for object
      bool is_selected = (selected_object_id_ == obj_id);
      ImVec2 button_size(item_size, item_size);

      if (ImGui::Selectable("", is_selected,
                            ImGuiSelectableFlags_AllowDoubleClick,
                            button_size)) {
        selected_object_id_ = obj_id;

        // Create and update preview object
        preview_object_ = zelda3::RoomObject(obj_id, 0, 0, 0x12, 0);
        preview_object_.SetRom(rom_);
        if (game_data_ && current_palette_group_id_ <
                              game_data_->palette_groups.dungeon_main.size()) {
          auto palette =
              game_data_->palette_groups.dungeon_main[current_palette_group_id_];
          preview_palette_ = palette;
        }
        object_loaded_ = true;

        // Notify callbacks
        if (object_selected_callback_) {
          object_selected_callback_(preview_object_);
        }

        // Handle double-click to open static object editor
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          if (object_double_click_callback_) {
            object_double_click_callback_(obj_id);
          }
        }
      }

      // Draw object preview on the button; fall back to styled placeholder
      ImVec2 button_pos = ImGui::GetItemRectMin();
      ImDrawList* draw_list = ImGui::GetWindowDrawList();

      // Only attempt graphical preview if enabled (performance optimization)
      bool rendered = false;
      if (enable_object_previews_) {
        rendered = DrawObjectPreview(MakePreviewObject(obj_id), button_pos,
                                     item_size);
      }

      if (!rendered) {
        // Draw a styled fallback with gradient background
        ImU32 obj_color = GetObjectTypeColor(obj_id);
        ImU32 darker_color = IM_COL32((obj_color & 0xFF) * 0.6f,
                                      ((obj_color >> 8) & 0xFF) * 0.6f,
                                      ((obj_color >> 16) & 0xFF) * 0.6f, 255);

        // Gradient background
        draw_list->AddRectFilledMultiColor(
            button_pos,
            ImVec2(button_pos.x + item_size, button_pos.y + item_size),
            darker_color, darker_color, obj_color, obj_color);

        // Draw object type symbol in center
        std::string symbol = GetObjectTypeSymbol(obj_id);
        ImVec2 symbol_size = ImGui::CalcTextSize(symbol.c_str());
        ImVec2 symbol_pos(button_pos.x + (item_size - symbol_size.x) / 2,
                          button_pos.y + (item_size - symbol_size.y) / 2 - 10);
        draw_list->AddText(symbol_pos, IM_COL32(255, 255, 255, 180),
                           symbol.c_str());
      }

      // Draw border with special highlight for static editor object
      bool is_static_editor_obj = (obj_id == static_editor_object_id_);
      ImU32 border_color;
      float border_thickness;

      if (is_static_editor_obj) {
        border_color = IM_COL32(0, 200, 255, 255);
        border_thickness = 3.0f;
      } else if (is_selected) {
        border_color = ImGui::GetColorU32(theme.dungeon_selection_primary);
        border_thickness = 3.0f;
      } else {
        border_color = ImGui::GetColorU32(theme.panel_bg_darker);
        border_thickness = 1.0f;
      }

      draw_list->AddRect(
          button_pos,
          ImVec2(button_pos.x + item_size, button_pos.y + item_size),
          border_color, 0.0f, 0, border_thickness);

      // Static editor indicator icon
      if (is_static_editor_obj) {
        ImVec2 icon_pos(button_pos.x + item_size - 14, button_pos.y + 2);
        draw_list->AddCircleFilled(ImVec2(icon_pos.x + 6, icon_pos.y + 6), 6,
                                   IM_COL32(0, 200, 255, 200));
        draw_list->AddText(icon_pos, IM_COL32(255, 255, 255, 255), "i");
      }

      // Get object name for display
      std::string full_name = zelda3::GetObjectName(obj_id);

      // Truncate name for display
      std::string display_name = full_name;
      const size_t kMaxDisplayChars = 12;
      if (display_name.length() > kMaxDisplayChars) {
        display_name = display_name.substr(0, kMaxDisplayChars - 2) + "..";
      }

      // Draw object name (smaller, above ID)
      ImVec2 name_size = ImGui::CalcTextSize(display_name.c_str());
      ImVec2 name_pos = ImVec2(button_pos.x + (item_size - name_size.x) / 2,
                               button_pos.y + item_size - 26);
      draw_list->AddText(name_pos,
                         ImGui::GetColorU32(theme.text_secondary_gray),
                         display_name.c_str());

      // Draw object ID at bottom (hex format)
      std::string id_text = absl::StrFormat("%03X", obj_id);
      ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
      ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                             button_pos.y + item_size - id_size.y - 2);
      draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                         id_text.c_str());

      // Enhanced tooltip
      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Object 0x%03X",
                           obj_id);
        ImGui::Text("%s", full_name.c_str());
        int subtype = zelda3::GetObjectSubtype(obj_id);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Subtype %d",
                           subtype);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "Click to select for placement");
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                           "Double-click to view details");
        ImGui::EndTooltip();
      }

      ImGui::PopID();

      current_column = (current_column + 1) % columns;
      items_drawn++;
    }

    ImGui::EndChild();
  }

  // Status line
  ImGui::Text("Range: 0x%03X - 0x%03X (%d objects)", start_id, end_id,
              end_id - start_id + 1);
}

bool DungeonObjectSelector::MatchesObjectFilter(int obj_id, int filter_type) {
  switch (filter_type) {
    case 1:  // Walls
      return obj_id >= 0x10 && obj_id <= 0x1F;
    case 2:  // Floors
      return obj_id >= 0x20 && obj_id <= 0x2F;
    case 3:  // Chests
      return obj_id == 0xF9 || obj_id == 0xFA;
    case 4:  // Doors
      return obj_id >= 0x17 && obj_id <= 0x1E;
    case 5:  // Decorations
      return obj_id >= 0x30 && obj_id <= 0x3F;
    case 6:  // Stairs
      return obj_id >= 0x138 && obj_id <= 0x13B;
    default:  // All
      return true;
  }
}

void DungeonObjectSelector::CalculateObjectDimensions(
    const zelda3::RoomObject& object, int& width, int& height) {
  // Default base size
  width = 16;
  height = 16;

  // For walls, use the size field to determine length
  if (object.id_ >= 0x10 && object.id_ <= 0x1F) {
    // Wall objects: size determines length and orientation
    uint8_t size_x = object.size_ & 0x0F;
    uint8_t size_y = (object.size_ >> 4) & 0x0F;

    // Walls can be horizontal or vertical based on size parameters
    if (size_x > size_y) {
      // Horizontal wall
      width = 16 + size_x * 16;  // Each unit adds 16 pixels
      height = 16;
    } else if (size_y > size_x) {
      // Vertical wall
      width = 16;
      height = 16 + size_y * 16;
    } else {
      // Square wall or corner
      width = 16 + size_x * 8;
      height = 16 + size_y * 8;
    }
  } else {
    // For other objects, use standard size calculation
    width = 16 + (object.size_ & 0x0F) * 8;
    height = 16 + ((object.size_ >> 4) & 0x0F) * 8;
  }

  // Clamp to reasonable limits
  width = std::min(width, 256);
  height = std::min(height, 256);
}

void DungeonObjectSelector::PlaceObjectAtPosition(int x, int y) {
  if (!object_loaded_ || !object_placement_callback_) {
    return;
  }

  // Create object with specified position
  auto placed_object = preview_object_;
  placed_object.set_x(static_cast<uint8_t>(x));
  placed_object.set_y(static_cast<uint8_t>(y));

  // Call placement callback
  object_placement_callback_(placed_object);
}

void DungeonObjectSelector::DrawCompactSpriteEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Sprite Editor");
  Separator();

  // Display current room sprites
  auto current_room = system.GetCurrentRoom();
  auto sprites_result = system.GetSpritesByRoom(current_room);

  if (sprites_result.ok()) {
    auto sprites = sprites_result.value();
    ImGui::Text("Sprites in room %d: %zu", current_room, sprites.size());

    // Show first few sprites in compact format
    int display_count = std::min(3, static_cast<int>(sprites.size()));
    for (int i = 0; i < display_count; ++i) {
      const auto& sprite = sprites[i];
      ImGui::Text("ID:%d Type:%d (%d,%d)", sprite.sprite_id,
                  static_cast<int>(sprite.type), sprite.x, sprite.y);
    }
    if (sprites.size() > 3) {
      ImGui::Text("... and %zu more", sprites.size() - 3);
    }
  } else {
    ImGui::Text("Error loading sprites");
  }

  // Quick sprite placement
  Separator();
  ImGui::Text("Quick Add Sprite");

  ImGui::InputInt("ID", &new_sprite_id_);
  ImGui::InputInt("X", &new_sprite_x_);
  ImGui::InputInt("Y", &new_sprite_y_);

  if (ImGui::Button("Add Sprite")) {
    zelda3::DungeonEditorSystem::SpriteData sprite_data;
    sprite_data.sprite_id = new_sprite_id_;
    sprite_data.type = zelda3::DungeonEditorSystem::SpriteType::kEnemy;
    sprite_data.x = new_sprite_x_;
    sprite_data.y = new_sprite_y_;
    sprite_data.layer = 0;

    auto status = system.AddSprite(sprite_data);
    if (!status.ok()) {
      ImGui::Text("Error adding sprite");
    }
  }
}

void DungeonObjectSelector::DrawCompactItemEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Item Editor");
  Separator();

  // Display current room items
  auto current_room = system.GetCurrentRoom();
  auto items_result = system.GetItemsByRoom(current_room);

  if (items_result.ok()) {
    auto items = items_result.value();
    ImGui::Text("Items in room %d: %zu", current_room, items.size());

    // Show first few items in compact format
    int display_count = std::min(3, static_cast<int>(items.size()));
    for (int i = 0; i < display_count; ++i) {
      const auto& item = items[i];
      ImGui::Text("ID:%d Type:%d (%d,%d)", item.item_id,
                  static_cast<int>(item.type), item.x, item.y);
    }
    if (items.size() > 3) {
      ImGui::Text("... and %zu more", items.size() - 3);
    }
  } else {
    ImGui::Text("Error loading items");
  }

  // Quick item placement
  Separator();
  ImGui::Text("Quick Add Item");

  ImGui::InputInt("ID", &new_item_id_);
  ImGui::InputInt("X", &new_item_x_);
  ImGui::InputInt("Y", &new_item_y_);

  if (ImGui::Button("Add Item")) {
    zelda3::DungeonEditorSystem::ItemData item_data;
    item_data.item_id = new_item_id_;
    item_data.type = zelda3::DungeonEditorSystem::ItemType::kKey;
    item_data.x = new_item_x_;
    item_data.y = new_item_y_;
    item_data.room_id = current_room;
    item_data.is_hidden = false;

    auto status = system.AddItem(item_data);
    if (!status.ok()) {
      ImGui::Text("Error adding item");
    }
  }
}

void DungeonObjectSelector::DrawCompactEntranceEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Entrance Editor");
  Separator();

  // Display current room entrances
  auto current_room = system.GetCurrentRoom();
  auto entrances_result = system.GetEntrancesByRoom(current_room);

  if (entrances_result.ok()) {
    auto entrances = entrances_result.value();
    ImGui::Text("Entrances: %zu", entrances.size());

    for (const auto& entrance : entrances) {
      ImGui::Text("ID:%d -> Room:%d (%d,%d)", entrance.entrance_id,
                  entrance.target_room_id, entrance.target_x,
                  entrance.target_y);
    }
  } else {
    ImGui::Text("Error loading entrances");
  }

  // Quick room connection
  Separator();
  ImGui::Text("Connect Rooms");

  ImGui::InputInt("Target Room", &entrance_target_room_id_);
  ImGui::InputInt("Source X", &entrance_source_x_);
  ImGui::InputInt("Source Y", &entrance_source_y_);
  ImGui::InputInt("Target X", &entrance_target_x_);
  ImGui::InputInt("Target Y", &entrance_target_y_);

  if (ImGui::Button("Connect")) {
    auto status = system.ConnectRooms(current_room, entrance_target_room_id_,
                                      entrance_source_x_, entrance_source_y_,
                                      entrance_target_x_, entrance_target_y_);
    if (!status.ok()) {
      ImGui::Text("Error connecting rooms");
    }
  }
}

void DungeonObjectSelector::DrawCompactDoorEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Door Editor");
  Separator();

  // Display current room doors
  auto current_room = system.GetCurrentRoom();
  auto doors_result = system.GetDoorsByRoom(current_room);

  if (doors_result.ok()) {
    auto doors = doors_result.value();
    ImGui::Text("Doors: %zu", doors.size());

    for (const auto& door : doors) {
      ImGui::Text("ID:%d (%d,%d) -> Room:%d", door.door_id, door.x, door.y,
                  door.target_room_id);
    }
  } else {
    ImGui::Text("Error loading doors");
  }

  // Quick door creation
  Separator();
  ImGui::Text("Add Door");

  ImGui::InputInt("X", &door_x_);
  ImGui::InputInt("Y", &door_y_);
  ImGui::SliderInt("Dir", &door_direction_, 0, 3);
  ImGui::InputInt("Target", &door_target_room_);

  if (ImGui::Button("Add Door")) {
    zelda3::DungeonEditorSystem::DoorData door_data;
    door_data.room_id = current_room;
    door_data.x = door_x_;
    door_data.y = door_y_;
    door_data.direction = door_direction_;
    door_data.target_room_id = door_target_room_;
    door_data.target_x = door_x_;
    door_data.target_y = door_y_;
    door_data.is_locked = false;
    door_data.requires_key = false;
    door_data.key_type = 0;

    auto status = system.AddDoor(door_data);
    if (!status.ok()) {
      ImGui::Text("Error adding door");
    }
  }
}

void DungeonObjectSelector::DrawCompactChestEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Chest Editor");
  Separator();

  // Display current room chests
  auto current_room = system.GetCurrentRoom();
  auto chests_result = system.GetChestsByRoom(current_room);

  if (chests_result.ok()) {
    auto chests = chests_result.value();
    ImGui::Text("Chests: %zu", chests.size());

    for (const auto& chest : chests) {
      ImGui::Text("ID:%d (%d,%d) Item:%d", chest.chest_id, chest.x, chest.y,
                  chest.item_id);
    }
  } else {
    ImGui::Text("Error loading chests");
  }

  // Quick chest creation
  Separator();
  ImGui::Text("Add Chest");

  ImGui::InputInt("X", &chest_x_);
  ImGui::InputInt("Y", &chest_y_);
  ImGui::InputInt("Item ID", &chest_item_id_);
  ImGui::Checkbox("Big", &chest_big_);

  if (ImGui::Button("Add Chest")) {
    zelda3::DungeonEditorSystem::ChestData chest_data;
    chest_data.room_id = current_room;
    chest_data.x = chest_x_;
    chest_data.y = chest_y_;
    chest_data.is_big_chest = chest_big_;
    chest_data.item_id = chest_item_id_;
    chest_data.item_quantity = 1;

    auto status = system.AddChest(chest_data);
    if (!status.ok()) {
      ImGui::Text("Error adding chest");
    }
  }
}

void DungeonObjectSelector::DrawCompactPropertiesEditor() {
  if (!dungeon_editor_system_ || !*dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  auto& system = **dungeon_editor_system_;

  ImGui::Text("Room Properties");
  Separator();

  auto current_room = system.GetCurrentRoom();
  auto properties_result = system.GetRoomProperties(current_room);

  if (properties_result.ok()) {
    auto properties = properties_result.value();

    // Copy current values (only update from ROM data, not every frame)
    // Safe string copy with bounds checking
    size_t name_len =
        std::min(properties.name.length(), sizeof(room_name_) - 1);
    std::memcpy(room_name_, properties.name.c_str(), name_len);
    room_name_[name_len] = '\0';
    dungeon_id_ = properties.dungeon_id;
    floor_level_ = properties.floor_level;
    is_boss_room_ = properties.is_boss_room;
    is_save_room_ = properties.is_save_room;
    music_id_ = properties.music_id;

    ImGui::InputText("Name", room_name_, sizeof(room_name_));
    ImGui::InputInt("Dungeon ID", &dungeon_id_);
    ImGui::InputInt("Floor", &floor_level_);
    ImGui::InputInt("Music", &music_id_);
    ImGui::Checkbox("Boss Room", &is_boss_room_);
    ImGui::Checkbox("Save Room", &is_save_room_);

    if (ImGui::Button("Save Properties")) {
      zelda3::DungeonEditorSystem::RoomProperties new_properties;
      new_properties.room_id = current_room;
      new_properties.name = room_name_;
      new_properties.dungeon_id = dungeon_id_;
      new_properties.floor_level = floor_level_;
      new_properties.is_boss_room = is_boss_room_;
      new_properties.is_save_room = is_save_room_;
      new_properties.music_id = music_id_;

      auto status = system.SetRoomProperties(current_room, new_properties);
      if (!status.ok()) {
        ImGui::Text("Error saving properties");
      }
    }
  } else {
    ImGui::Text("Error loading properties");
  }

  // Dungeon settings summary
  Separator();
  ImGui::Text("Dungeon Settings");

  auto dungeon_settings_result = system.GetDungeonSettings();
  if (dungeon_settings_result.ok()) {
    auto settings = dungeon_settings_result.value();
    ImGui::Text("Dungeon: %s", settings.name.c_str());
    ImGui::Text("Rooms: %d", settings.total_rooms);
    ImGui::Text("Start: %d", settings.starting_room_id);
    ImGui::Text("Boss: %d", settings.boss_room_id);
  }
}

void DungeonObjectSelector::EnsureRegistryInitialized() {
  if (registry_initialized_) return;
  object_registry_.RegisterVanillaRange(0x000, 0x1FF);
  registry_initialized_ = true;
}

zelda3::RoomObject DungeonObjectSelector::MakePreviewObject(int obj_id) const {
  zelda3::RoomObject obj(obj_id, 0, 0, 0x12, 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();
  return obj;
}

bool DungeonObjectSelector::DrawObjectPreview(
    const zelda3::RoomObject& object, ImVec2 top_left, float size) {
  if (!rom_ || !rom_->is_loaded()) {
    return false;
  }

  // Check if we have valid graphics data from a loaded room
  const uint8_t* gfx_data = nullptr;
  if (rooms_ && current_room_id_ < static_cast<int>(rooms_->size())) {
    const auto& room = (*rooms_)[current_room_id_];
    // Room must be loaded (graphics populated) to use its gfx buffer
    if (room.IsLoaded()) {
      gfx_data = room.get_gfx_buffer().data();
    }
  }

  // If no room graphics available (room not loaded), we can't render a preview
  if (!gfx_data) {
    return false;
  }

  // Check if tiles were loaded
  if (object.tiles().empty()) {
    return false;
  }

  // Create preview buffer with proper size
  gfx::BackgroundBuffer preview_bg(static_cast<int>(size),
                                   static_cast<int>(size));

  // Initialize drawer and render object
  zelda3::ObjectDrawer drawer(rom_, current_room_id_, gfx_data);
  drawer.InitializeDrawRoutines();

  auto status =
      drawer.DrawObject(object, preview_bg, preview_bg, current_palette_group_);
  if (!status.ok()) {
    return false;
  }

  // Get bitmap and ensure it has valid data
  auto& bitmap = preview_bg.bitmap();
  if (bitmap.size() == 0) {
    return false;
  }

  // Create texture if needed
  if (!bitmap.texture()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
  }

  if (!bitmap.texture()) {
    return false;
  }

  // Draw the preview image
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 bottom_right(top_left.x + size, top_left.y + size);
  draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(), top_left,
                      bottom_right);
  return true;
}

}  // namespace yaze::editor
