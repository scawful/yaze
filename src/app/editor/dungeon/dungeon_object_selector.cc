#include "dungeon_object_selector.h"

#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_renderer.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "imgui/imgui.h"
#include "util/hex.h"

namespace yaze::editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::Separator;
using ImGui::Text;

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
  if (ImGui::BeginTable("DungeonObjectEditorTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV, ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Dungeon Objects", ImGuiTableColumnFlags_WidthStretch, ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Canvas");

    ImGui::TableNextColumn();
    BeginChild("DungeonObjectButtons", ImVec2(250, 0), true);

    static int selected_object = 0;
    int i = 0;
    for (const auto object_name : zelda3::Type1RoomObjectNames) {
      if (ImGui::Selectable(object_name.data(), selected_object == i)) {
        selected_object = i;

        // Create a test object and render it
        auto test_object = zelda3::RoomObject(i, 8, 8, 0x12, 0);  // Center in canvas
        test_object.set_rom(rom_);
        test_object.EnsureTilesLoaded();

        // Get current palette
        if (rom_ && rom_->is_loaded()) {
          auto palette = rom_->palette_group().dungeon_main[current_palette_group_id_];

          // Render object preview
          auto result = object_renderer_.GetObjectPreview(test_object, palette);
          if (result.ok()) {
            object_loaded_ = true;
            preview_object_ = test_object;  // Store for rendering
            preview_palette_ = palette;
          }
        }
      }
      i += 1;
    }

    EndChild();

    // Right side of the table - Canvas
    ImGui::TableNextColumn();
    BeginChild("DungeonObjectCanvas", ImVec2(276, 0x10 * 0x40 + 1), true);

    object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    object_canvas_.DrawContextMenu();
    object_canvas_.DrawTileSelector(32);
    object_canvas_.DrawGrid(32.0f);

    // Render object preview if available
    if (object_loaded_ && preview_object_.id_ >= 0) {
      // Render preview object at center of canvas (object_canvas_ is 256x256)
      int preview_x = 128 - 16;  // Center horizontally (256/2 - 16 for 32x32 object)
      int preview_y = 128 - 16;  // Center vertically

      auto preview_result = object_renderer_.RenderObject(preview_object_, preview_palette_);
      if (preview_result.ok()) {
        auto preview_bitmap = std::move(preview_result.value());
        preview_bitmap.SetPalette(preview_palette_);
        core::Renderer::Get().RenderBitmap(&preview_bitmap);
        object_canvas_.DrawBitmap(preview_bitmap, preview_x, preview_y, 1.0f, 255);
      }
    }

    object_canvas_.DrawOverlay();

    EndChild();
    ImGui::EndTable();
  }

  if (object_loaded_) {
    ImGui::Begin("Object Preview", &object_loaded_, 0);
    ImGui::Text("Object ID: 0x%02X", preview_object_.id_);
    ImGui::Text("Position: (%d, %d)", preview_object_.x_, preview_object_.y_);
    ImGui::Text("Size: 0x%02X", preview_object_.size_);
    ImGui::Text("Layer: %d", static_cast<int>(preview_object_.layer_));
    ImGui::End();
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
    const int max_blocks_per_row = 4; // Limit blocks per row to fit canvas
    const int block_width = 0x100; // 256 pixels per block
    const int block_height = 0x40; // 64 pixels per block
    
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
    editor.Undo();
  }
  ImGui::SameLine();
  if (ImGui::Button("Redo") && editor.CanRedo()) {
    editor.Redo();
  }
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
    strncpy(room_name, properties.name.c_str(), sizeof(room_name) - 1);
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
