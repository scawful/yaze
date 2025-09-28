#include "dungeon_object_selector.h"

#include <algorithm>
#include <iterator>
#include <cstring>

#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/modules/asset_browser.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "imgui/imgui.h"

namespace yaze::editor {

using ImGui::BeginChild;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;

void DungeonObjectSelector::DrawTileSelector() {
  if (ImGui::BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (ImGui::BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
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
  // Use AssetBrowser for better object selection
  if (ImGui::BeginTable("DungeonObjectEditorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Object Browser", ImGuiTableColumnFlags_WidthFixed, 400);
    ImGui::TableSetupColumn("Preview Canvas", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Left column: AssetBrowser for object selection
    ImGui::TableNextColumn();
    ImGui::BeginChild("AssetBrowser", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    DrawObjectAssetBrowser();
    
    ImGui::EndChild();

    // Right column: Preview and placement controls
    ImGui::TableNextColumn();
    ImGui::BeginChild("PreviewCanvas", ImVec2(0, 0), true);

    // Object placement controls
    ImGui::SeparatorText("Object Placement");
    static int place_x = 0, place_y = 0;
    ImGui::InputInt("X Position", &place_x);
    ImGui::InputInt("Y Position", &place_y);
    
    if (ImGui::Button("Place Object") && object_loaded_) {
      PlaceObjectAtPosition(place_x, place_y);
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

      auto preview_result = object_renderer_.RenderObject(preview_object_, preview_palette_);
      if (preview_result.ok()) {
        auto preview_bitmap = std::move(preview_result.value());
        if (preview_bitmap.width() > 0 && preview_bitmap.height() > 0) {
          preview_bitmap.SetPalette(preview_palette_);
          core::Renderer::Get().RenderBitmap(&preview_bitmap);
          object_canvas_.DrawBitmap(preview_bitmap, preview_x, preview_y, 1.0f, 255);
        } else {
          // Fallback: Draw primitive shape
          RenderObjectPrimitive(preview_object_, preview_x, preview_y);
        }
      } else {
        // Fallback: Draw primitive shape
        RenderObjectPrimitive(preview_object_, preview_x, preview_y);
      }
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
    static int place_x = 0, place_y = 0;
    ImGui::InputInt("X Position", &place_x);
    ImGui::InputInt("Y Position", &place_y);
    
    if (ImGui::Button("Place Object")) {
      // TODO: Implement object placement in the main canvas
      ImGui::Text("Object placed at (%d, %d)", place_x, place_y);
    }
    
    ImGui::End();
  }
}

void DungeonObjectSelector::DrawObjectBrowser() {
  static int selected_object_type = 0;
  static int selected_object_id = 0;
  
  // Object type selector
  const char* object_types[] = {"Type 1 (0x00-0xFF)", "Type 2 (0x100-0x1FF)", "Type 3 (0x200+)"};
  if (ImGui::Combo("Object Type", &selected_object_type, object_types, 3)) {
    selected_object_id = 0; // Reset selection when changing type
  }
  
  ImGui::Separator();
  
  // Object list with previews - optimized for 300px column width
  const int preview_size = 48; // Larger 48x48 pixel preview for better visibility
  const int items_per_row = 5; // 5 items per row to fit in 300px column
  
  if (rom_ && rom_->is_loaded()) {
    auto palette = rom_->palette_group().dungeon_main[current_palette_group_id_];
    
    // Determine object range based on type
    int start_id, end_id;
    switch (selected_object_type) {
      case 0: start_id = 0x00; end_id = 0xFF; break;
      case 1: start_id = 0x100; end_id = 0x1FF; break;
      case 2: start_id = 0x200; end_id = 0x2FF; break;
      default: start_id = 0x00; end_id = 0xFF; break;
    }
    
    // Create a grid layout for object previews
    int current_row = 0;
    int current_col = 0;
    
    for (int obj_id = start_id; obj_id <= end_id && obj_id <= start_id + 63; ++obj_id) { // Limit to 64 objects for performance
      // Create object for preview
      auto test_object = zelda3::RoomObject(obj_id, 0, 0, 0x12, 0);
      test_object.set_rom(rom_);
      test_object.EnsureTilesLoaded();
      
      // Calculate position in grid - better sizing for 300px column
      float available_width = ImGui::GetContentRegionAvail().x;
      float spacing = ImGui::GetStyle().ItemSpacing.x;
      float item_width = (available_width - (items_per_row - 1) * spacing) / items_per_row;
      float item_height = preview_size + 30; // Preview + text (reduced padding)
      
      ImGui::PushID(obj_id);
      
      // Create a selectable button with preview
      bool is_selected = (selected_object_id == obj_id);
      if (ImGui::Selectable("", is_selected, ImGuiSelectableFlags_None, ImVec2(item_width, item_height))) {
        selected_object_id = obj_id;
        
        // Update preview object
        preview_object_ = test_object;
        preview_palette_ = palette;
        object_loaded_ = true;
        
        // Notify the main editor that an object was selected
        if (object_selected_callback_) {
          object_selected_callback_(preview_object_);
        }
      }
      
      // Draw preview image
      ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
      ImVec2 preview_pos = ImVec2(cursor_pos.x + (item_width - preview_size) / 2, 
                                  cursor_pos.y - item_height + 5);
      
      // Draw simplified primitive preview for object selector
      ImGui::SetCursorScreenPos(preview_pos);
      
      // Draw object as colored rectangle with ID
      ImU32 object_color = GetObjectTypeColor(obj_id);
      ImGui::GetWindowDrawList()->AddRectFilled(
        preview_pos, 
        ImVec2(preview_pos.x + preview_size, preview_pos.y + preview_size),
        object_color);
      
      // Draw border
      ImGui::GetWindowDrawList()->AddRect(
        preview_pos, 
        ImVec2(preview_pos.x + preview_size, preview_pos.y + preview_size),
        IM_COL32(0, 0, 0, 255), 0.0f, 0, 2.0f);
      
      // Draw object type symbol in center
      std::string symbol = GetObjectTypeSymbol(obj_id);
      ImVec2 text_size = ImGui::CalcTextSize(symbol.c_str());
      ImVec2 text_pos = ImVec2(
        preview_pos.x + (preview_size - text_size.x) / 2,
        preview_pos.y + (preview_size - text_size.y) / 2);
      
      ImGui::GetWindowDrawList()->AddText(
        text_pos, IM_COL32(255, 255, 255, 255), symbol.c_str());
      
      // Draw object ID below preview
      ImGui::SetCursorScreenPos(ImVec2(preview_pos.x, preview_pos.y + preview_size + 2));
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
      ImGui::Text("0x%02X", obj_id);
      ImGui::PopStyleColor();
      
      // Try to get object name
      std::string object_name = "Unknown";
      if (obj_id < 0x100) { // Type1RoomObjectNames has 248 elements (0-247, 0x00-0xF7)
        if (obj_id < std::size(zelda3::Type1RoomObjectNames)) {
          const char* name_ptr = zelda3::Type1RoomObjectNames[obj_id];
          if (name_ptr != nullptr) {
            object_name = std::string(name_ptr);
          }
        }
      } else if (obj_id < 0x140) { // Type2RoomObjectNames has 64 elements (0x100-0x13F)
        int type2_index = obj_id - 0x100;
        if (type2_index >= 0 && type2_index < std::size(zelda3::Type2RoomObjectNames)) {
          const char* name_ptr = zelda3::Type2RoomObjectNames[type2_index];
          if (name_ptr != nullptr) {
            object_name = std::string(name_ptr);
          }
        }
      } else if (obj_id < 0x1C0) { // Type3RoomObjectNames has 128 elements (0x140-0x1BF)
        int type3_index = obj_id - 0x140;
        if (type3_index >= 0 && type3_index < std::size(zelda3::Type3RoomObjectNames)) {
          const char* name_ptr = zelda3::Type3RoomObjectNames[type3_index];
          if (name_ptr != nullptr) {
            object_name = std::string(name_ptr);
          }
        }
      }
      
      // Draw object name with better sizing
      ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x + 2, cursor_pos.y - 8));
      ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
      // Truncate long names to fit
      if (object_name.length() > 8) {
        object_name = object_name.substr(0, 8) + "...";
      }
      ImGui::Text("%s", object_name.c_str());
      ImGui::PopStyleColor();
      
      ImGui::PopID();
      
      // Move to next position
      current_col++;
      if (current_col >= items_per_row) {
        current_col = 0;
        current_row++;
        ImGui::NewLine();
      } else {
        ImGui::SameLine();
      }
    }
  } else {
    ImGui::Text("ROM not loaded");
  }
  
  ImGui::Separator();
  
  // Selected object info
  if (object_loaded_) {
    ImGui::Text("Selected: 0x%03X", selected_object_id);
    ImGui::Text("Layer: %d", static_cast<int>(preview_object_.layer_));
    ImGui::Text("Size: 0x%02X", preview_object_.size_);
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
    const int max_blocks_per_row = 2; // 2 blocks per row for 300px column
    const int block_width = 128; // Reduced size to fit column
    const int block_height = 32; // Reduced height
    
    for (int block : blocks) {
      if (current_block >= 16) break; // Only show first 16 blocks
      
      // Ensure the graphics sheet is loaded and has a valid texture
      if (block < gfx::Arena::Get().gfx_sheets().size()) {
        auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block];
        
        // Calculate position in a grid layout instead of horizontal concatenation
        int row = current_block / max_blocks_per_row;
        int col = current_block % max_blocks_per_row;
        
        int x = room_gfx_canvas_.zero_point().x + 2 + (col * block_width);
        int y = room_gfx_canvas_.zero_point().y + 2 + (row * block_height);
        
        // Ensure we don't exceed canvas bounds
        if (x + block_width <= room_gfx_canvas_.zero_point().x + room_gfx_canvas_.width() &&
            y + block_height <= room_gfx_canvas_.zero_point().y + room_gfx_canvas_.height()) {
          
          // Only draw if the texture is valid
          if (gfx_sheet.texture() != 0) {
            room_gfx_canvas_.draw_list()->AddImage(
                (ImTextureID)(intptr_t)gfx_sheet.texture(),
                ImVec2(x, y),
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
  if (!dungeon_editor_system_ || !object_editor_ || !*dungeon_editor_system_ || !*object_editor_) {
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
  if (!object_editor_ || !*object_editor_) {
    ImGui::Text("Object editor not initialized");
    return;
  }

  auto& editor = **object_editor_;
  
  ImGui::Text("Object Editor");
  Separator();

  // Display current editing mode
  auto mode = editor.GetMode();
  const char *mode_names[] = {"Select", "Insert", "Delete", "Edit", "Layer", "Preview"};
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
  // Color-code objects based on their type and function
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return IM_COL32(128, 128, 128, 255); // Gray for walls
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return IM_COL32(139, 69, 19, 255); // Brown for floors
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return IM_COL32(255, 215, 0, 255); // Gold for chests
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return IM_COL32(139, 69, 19, 255); // Brown for doors
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return IM_COL32(160, 82, 45, 255); // Saddle brown for pots
  } else if (object_id >= 0x138 && object_id <= 0x13B) {
    return IM_COL32(255, 255, 0, 255); // Yellow for stairs
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return IM_COL32(105, 105, 105, 255); // Dim gray for decorations
  } else {
    return IM_COL32(96, 96, 96, 255); // Default gray
  }
}

std::string DungeonObjectSelector::GetObjectTypeSymbol(int object_id) {
  // Return symbol representing object type
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return "■"; // Wall
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return "□"; // Floor
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return "⬛"; // Chest
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return "◊"; // Door
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return "●"; // Pot
  } else if (object_id >= 0x138 && object_id <= 0x13B) {
    return "▲"; // Stairs
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return "◆"; // Decoration
  } else {
    return "?"; // Unknown
  }
}

void DungeonObjectSelector::RenderObjectPrimitive(const zelda3::RoomObject& object, int x, int y) {
  // Render object as primitive shape on canvas
  ImU32 color = GetObjectTypeColor(object.id_);
  
  // Calculate object size with proper wall length handling
  int obj_width, obj_height;
  CalculateObjectDimensions(object, obj_width, obj_height);
  
  // Draw object rectangle
  ImVec4 color_vec = ImGui::ColorConvertU32ToFloat4(color);
  object_canvas_.DrawRect(x, y, obj_width, obj_height, color_vec);
  object_canvas_.DrawRect(x, y, obj_width, obj_height, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
  
  // Draw object ID as text
  std::string obj_text = absl::StrFormat("0x%X", object.id_);
  object_canvas_.DrawText(obj_text, x + obj_width + 2, y + 4);
}

void DungeonObjectSelector::DrawObjectAssetBrowser() {
  ImGui::SeparatorText("Dungeon Objects");
  
  // Debug info
  ImGui::Text("Asset Browser Debug: Available width: %.1f", ImGui::GetContentRegionAvail().x);
  
  // Object type filter
  static int object_type_filter = 0;
  const char* object_types[] = {"All", "Walls", "Floors", "Chests", "Doors", "Decorations", "Stairs"};
  if (ImGui::Combo("Object Type", &object_type_filter, object_types, 7)) {
    // Filter will be applied in the loop below
  }
  
  ImGui::Separator();
  
  // Create asset browser-style grid
  const float item_size = 64.0f;
  const float item_spacing = 8.0f;
  const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x - item_spacing) / (item_size + item_spacing)));
  
  ImGui::Text("Columns: %d, Item size: %.1f", columns, item_size);
  
  int current_column = 0;
  int items_drawn = 0;
  
  // Draw object grid based on filter
  for (int obj_id = 0; obj_id <= 0xFF && items_drawn < 100; ++obj_id) {
    // Apply object type filter
    if (object_type_filter > 0 && !MatchesObjectFilter(obj_id, object_type_filter)) {
      continue;
    }
    
    if (current_column > 0) {
      ImGui::SameLine();
    }
    
    ImGui::PushID(obj_id);
    
    // Create selectable button for object
    bool is_selected = (selected_object_id_ == obj_id);
    ImVec2 button_size(item_size, item_size);
    
    if (ImGui::Selectable("", is_selected, ImGuiSelectableFlags_None, button_size)) {
      selected_object_id_ = obj_id;
      
      // Create and update preview object
      preview_object_ = zelda3::RoomObject(obj_id, 0, 0, 0x12, 0);
      preview_object_.set_rom(rom_);
      if (rom_) {
        auto palette = rom_->palette_group().dungeon_main[current_palette_group_id_];
        preview_palette_ = palette;
      }
      object_loaded_ = true;
      
      // Notify callback
      if (object_selected_callback_) {
        object_selected_callback_(preview_object_);
      }
    }
    
    // Draw object preview on the button
    ImVec2 button_pos = ImGui::GetItemRectMin();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Draw object as colored rectangle with symbol
    ImU32 obj_color = GetObjectTypeColor(obj_id);
    draw_list->AddRectFilled(button_pos, 
                            ImVec2(button_pos.x + item_size, button_pos.y + item_size), 
                            obj_color);
    
    // Draw border
    ImU32 border_color = is_selected ? IM_COL32(255, 255, 0, 255) : IM_COL32(0, 0, 0, 255);
    draw_list->AddRect(button_pos, 
                      ImVec2(button_pos.x + item_size, button_pos.y + item_size), 
                      border_color, 0.0f, 0, is_selected ? 3.0f : 1.0f);
    
    // Draw object symbol
    std::string symbol = GetObjectTypeSymbol(obj_id);
    ImVec2 text_size = ImGui::CalcTextSize(symbol.c_str());
    ImVec2 text_pos = ImVec2(
      button_pos.x + (item_size - text_size.x) / 2,
      button_pos.y + (item_size - text_size.y) / 2);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), symbol.c_str());
    
    // Draw object ID at bottom
    std::string id_text = absl::StrFormat("%02X", obj_id);
    ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
    ImVec2 id_pos = ImVec2(
      button_pos.x + (item_size - id_size.x) / 2,
      button_pos.y + item_size - id_size.y - 2);
    draw_list->AddText(id_pos, IM_COL32(255, 255, 255, 255), id_text.c_str());
    
    ImGui::PopID();
    
    current_column = (current_column + 1) % columns;
    if (current_column == 0) {
      // Force new line
    }
    
    items_drawn++;
  }
  
  ImGui::Separator();
  ImGui::Text("Items drawn: %d", items_drawn);
}

bool DungeonObjectSelector::MatchesObjectFilter(int obj_id, int filter_type) {
  switch (filter_type) {
    case 1: // Walls
      return obj_id >= 0x10 && obj_id <= 0x1F;
    case 2: // Floors
      return obj_id >= 0x20 && obj_id <= 0x2F;
    case 3: // Chests
      return obj_id == 0xF9 || obj_id == 0xFA;
    case 4: // Doors
      return obj_id >= 0x17 && obj_id <= 0x1E;
    case 5: // Decorations
      return obj_id >= 0x30 && obj_id <= 0x3F;
    case 6: // Stairs
      return obj_id >= 0x138 && obj_id <= 0x13B;
    default: // All
      return true;
  }
}

void DungeonObjectSelector::CalculateObjectDimensions(const zelda3::RoomObject& object, int& width, int& height) {
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
      width = 16 + size_x * 16; // Each unit adds 16 pixels
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
      const auto &sprite = sprites[i];
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

  static int new_sprite_id = 0;
  static int new_sprite_x = 0;
  static int new_sprite_y = 0;

  ImGui::InputInt("ID", &new_sprite_id);
  ImGui::InputInt("X", &new_sprite_x);
  ImGui::InputInt("Y", &new_sprite_y);

  if (ImGui::Button("Add Sprite")) {
    zelda3::DungeonEditorSystem::SpriteData sprite_data;
    sprite_data.sprite_id = new_sprite_id;
    sprite_data.type = zelda3::DungeonEditorSystem::SpriteType::kEnemy;
    sprite_data.x = new_sprite_x;
    sprite_data.y = new_sprite_y;
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
      const auto &item = items[i];
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

  static int new_item_id = 0;
  static int new_item_x = 0;
  static int new_item_y = 0;

  ImGui::InputInt("ID", &new_item_id);
  ImGui::InputInt("X", &new_item_x);
  ImGui::InputInt("Y", &new_item_y);

  if (ImGui::Button("Add Item")) {
    zelda3::DungeonEditorSystem::ItemData item_data;
    item_data.item_id = new_item_id;
    item_data.type = zelda3::DungeonEditorSystem::ItemType::kKey;
    item_data.x = new_item_x;
    item_data.y = new_item_y;
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

    for (const auto &entrance : entrances) {
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

  static int target_room_id = 0;
  static int source_x = 0;
  static int source_y = 0;
  static int target_x = 0;
  static int target_y = 0;

  ImGui::InputInt("Target Room", &target_room_id);
  ImGui::InputInt("Source X", &source_x);
  ImGui::InputInt("Source Y", &source_y);
  ImGui::InputInt("Target X", &target_x);
  ImGui::InputInt("Target Y", &target_y);

  if (ImGui::Button("Connect")) {
    auto status = system.ConnectRooms(current_room, target_room_id, source_x, source_y, target_x, target_y);
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

    for (const auto &door : doors) {
      ImGui::Text("ID:%d (%d,%d) -> Room:%d", door.door_id, door.x, door.y,
                  door.target_room_id);
    }
  } else {
    ImGui::Text("Error loading doors");
  }

  // Quick door creation
  Separator();
  ImGui::Text("Add Door");

  static int door_x = 0;
  static int door_y = 0;
  static int door_direction = 0;
  static int door_target_room = 0;

  ImGui::InputInt("X", &door_x);
  ImGui::InputInt("Y", &door_y);
  ImGui::SliderInt("Dir", &door_direction, 0, 3);
  ImGui::InputInt("Target", &door_target_room);

  if (ImGui::Button("Add Door")) {
    zelda3::DungeonEditorSystem::DoorData door_data;
    door_data.room_id = current_room;
    door_data.x = door_x;
    door_data.y = door_y;
    door_data.direction = door_direction;
    door_data.target_room_id = door_target_room;
    door_data.target_x = door_x;
    door_data.target_y = door_y;
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

    for (const auto &chest : chests) {
      ImGui::Text("ID:%d (%d,%d) Item:%d", chest.chest_id, chest.x, chest.y,
                  chest.item_id);
    }
  } else {
    ImGui::Text("Error loading chests");
  }

  // Quick chest creation
  Separator();
  ImGui::Text("Add Chest");

  static int chest_x = 0;
  static int chest_y = 0;
  static int chest_item_id = 0;
  static bool chest_big = false;

  ImGui::InputInt("X", &chest_x);
  ImGui::InputInt("Y", &chest_y);
  ImGui::InputInt("Item ID", &chest_item_id);
  ImGui::Checkbox("Big", &chest_big);

  if (ImGui::Button("Add Chest")) {
    zelda3::DungeonEditorSystem::ChestData chest_data;
    chest_data.room_id = current_room;
    chest_data.x = chest_x;
    chest_data.y = chest_y;
    chest_data.is_big_chest = chest_big;
    chest_data.item_id = chest_item_id;
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
    
    static char room_name[128] = {0};
    static int dungeon_id = 0;
    static int floor_level = 0;
    static bool is_boss_room = false;
    static bool is_save_room = false;
    static int music_id = 0;

    // Copy current values
    // Safe string copy with bounds checking
    size_t name_len = std::min(properties.name.length(), sizeof(room_name) - 1);
    std::memcpy(room_name, properties.name.c_str(), name_len);
    room_name[name_len] = '\0';
    dungeon_id = properties.dungeon_id;
    floor_level = properties.floor_level;
    is_boss_room = properties.is_boss_room;
    is_save_room = properties.is_save_room;
    music_id = properties.music_id;

    ImGui::InputText("Name", room_name, sizeof(room_name));
    ImGui::InputInt("Dungeon ID", &dungeon_id);
    ImGui::InputInt("Floor", &floor_level);
    ImGui::InputInt("Music", &music_id);
    ImGui::Checkbox("Boss Room", &is_boss_room);
    ImGui::Checkbox("Save Room", &is_save_room);

    if (ImGui::Button("Save Properties")) {
      zelda3::DungeonEditorSystem::RoomProperties new_properties;
      new_properties.room_id = current_room;
      new_properties.name = room_name;
      new_properties.dungeon_id = dungeon_id;
      new_properties.floor_level = floor_level;
      new_properties.is_boss_room = is_boss_room;
      new_properties.is_save_room = is_save_room;
      new_properties.music_id = music_id;
      
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

}  // namespace yaze::editor
