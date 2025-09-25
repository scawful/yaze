#include "dungeon_editor.h"

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "app/zelda3/dungeon/unified_object_renderer.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/hex.h"

namespace yaze::editor {

using core::Renderer;

using ImGui::BeginChild;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::EndChild;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::RadioButton;
using ImGui::SameLine;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

constexpr ImGuiTableFlags kDungeonObjectTableFlags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter |
    ImGuiTableFlags_BordersV;

void DungeonEditor::Initialize() {
  if (rom_ && !dungeon_editor_system_) {
    dungeon_editor_system_ = std::make_unique<zelda3::DungeonEditorSystem>(rom_);
    object_editor_ = std::make_shared<zelda3::DungeonObjectEditor>(rom_);
  }
}

absl::Status DungeonEditor::Load() {
  auto dungeon_man_pal_group = rom()->palette_group().dungeon_main;

  for (int i = 0; i < 0x100 + 40; i++) {
    rooms_[i] = zelda3::LoadRoomFromRom(rom_, i);

    auto room_size = zelda3::CalculateRoomSize(rom_, i);
    room_size_pointers_.push_back(room_size.room_size_pointer);
    room_sizes_.push_back(room_size.room_size);
    if (room_size.room_size_pointer != 0x0A8000) {
      room_size_addresses_[i] = room_size.room_size_pointer;
    }

    rooms_[i].LoadObjects();

    auto dungeon_palette_ptr = rom()->paletteset_ids[rooms_[i].palette][0];
    auto palette_id = rom()->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.status() != absl::OkStatus()) {
      continue;
    }
    int p_id = palette_id.value() / 180;
    auto color = dungeon_man_pal_group[p_id][3];
    room_palette_[rooms_[i].palette] = color.rgb();
  }

  LoadDungeonRoomSize();
  // LoadRoomEntrances
  for (int i = 0; i < 0x07; ++i) {
    entrances_[i] = zelda3::RoomEntrance(rom(), i, true);
  }

  for (int i = 0; i < 0x85; ++i) {
    entrances_[i + 0x07] = zelda3::RoomEntrance(rom(), i, false);
  }

  // Load the palette group and palette for the dungeon
  full_palette_ = dungeon_man_pal_group[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(full_palette_));

  CalculateUsageStats();
  
  // Initialize the new editor system
  if (dungeon_editor_system_) {
    auto status = dungeon_editor_system_->Initialize();
    if (!status.ok()) {
      return status;
    }
  }
  
  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditor::Update() {
  if (refresh_graphics_) {
    RETURN_IF_ERROR(RefreshGraphics());
    refresh_graphics_ = false;
  }

  if (ImGui::BeginTabBar("##DungeonEditorTabBar")) {
    if (ImGui::BeginTabItem("Room Editor")) {
      status_ = UpdateDungeonRoomView();
      ImGui::EndTabItem();
    }
    
    // New editor system tabs
    if (ImGui::BeginTabItem("Object Editor")) {
      DrawObjectEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Sprite Editor")) {
      DrawSpriteEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Item Editor")) {
      DrawItemEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Entrance Editor")) {
      DrawEntranceEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Door Editor")) {
      DrawDoorEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Chest Editor")) {
      DrawChestEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Properties")) {
      DrawPropertiesEditor();
      ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Usage Statistics")) {
      if (is_loaded_) {
        DrawUsageStats();
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  return absl::OkStatus();
}

absl::Status DungeonEditor::Undo() {
  if (dungeon_editor_system_) {
    return dungeon_editor_system_->Undo();
  }
  return absl::UnimplementedError("Undo not available");
}

absl::Status DungeonEditor::Redo() {
  if (dungeon_editor_system_) {
    return dungeon_editor_system_->Redo();
  }
  return absl::UnimplementedError("Redo not available");
}

absl::Status DungeonEditor::Save() {
  if (dungeon_editor_system_) {
    return dungeon_editor_system_->SaveDungeon();
  }
  return absl::UnimplementedError("Save not available");
}

absl::Status DungeonEditor::RefreshGraphics() {
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin(), 8, [this](int block) {
        gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
            current_palette_group_[current_palette_id_], 0);
        Renderer::Get().UpdateBitmap(&gfx::Arena::Get().gfx_sheets()[block]);
      });

  auto sprites_aux1_pal_group = rom()->palette_group().sprites_aux1;
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin() + 8, 8,
      [this, &sprites_aux1_pal_group](int block) {
        gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
            sprites_aux1_pal_group[current_palette_id_], 0);
        Renderer::Get().UpdateBitmap(&gfx::Arena::Get().gfx_sheets()[block]);
      });
  return absl::OkStatus();
}

void DungeonEditor::LoadDungeonRoomSize() {
  std::map<int, std::vector<int>> rooms_by_bank;
  for (const auto &room : room_size_addresses_) {
    int bank = room.second >> 16;
    rooms_by_bank[bank].push_back(room.second);
  }

  // Process and calculate room sizes within each bank
  for (auto &bank_rooms : rooms_by_bank) {
    // Sort the rooms within this bank
    std::ranges::sort(bank_rooms.second);

    for (size_t i = 0; i < bank_rooms.second.size(); ++i) {
      int room_ptr = bank_rooms.second[i];

      // Identify the room ID for the current room pointer
      int room_id =
          std::ranges::find_if(room_size_addresses_, [room_ptr](
                                                         const auto &entry) {
            return entry.second == room_ptr;
          })->first;

      if (room_ptr != 0x0A8000) {
        if (i < bank_rooms.second.size() - 1) {
          // Calculate size as difference between current room and next room
          // in the same bank
          room_sizes_[room_id] = bank_rooms.second[i + 1] - room_ptr;
        } else {
          // Calculate size for the last room in this bank
          int bank_end_address = (bank_rooms.first << 16) | 0xFFFF;
          room_sizes_[room_id] = bank_end_address - room_ptr + 1;
        }
        total_room_size_ += room_sizes_[room_id];
      } else {
        // Room with address 0x0A8000
        room_sizes_[room_id] = 0x00;
      }
    }
  }
}

absl::Status DungeonEditor::UpdateDungeonRoomView() {
  DrawToolset();

  if (palette_showing_) {
    ImGui::Begin("Palette Editor", &palette_showing_, 0);
    auto dungeon_main_pal_group = rom()->palette_group().dungeon_main;
    current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
    gui::SelectablePalettePipeline(current_palette_id_, refresh_graphics_,
                                   current_palette_);
    ImGui::End();
  }

  if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Room Selector");
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector");
    TableHeadersRow();
    TableNextRow();

    TableNextColumn();
    if (ImGui::BeginTabBar("##DungeonRoomTabBar")) {
      TAB_ITEM("Rooms");
      DrawRoomSelector();
      END_TAB_ITEM();
      TAB_ITEM("Entrances");
      DrawEntranceSelector();
      END_TAB_ITEM();
      ImGui::EndTabBar();
    }

    TableNextColumn();
    DrawDungeonTabView();

    TableNextColumn();
    DrawTileSelector();
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void DungeonEditor::DrawToolset() {
  if (BeginTable("DWToolset", 16, ImGuiTableFlags_SizingFixedFit,
                 ImVec2(0, 0))) {
    static std::array<const char *, 16> tool_names = {
        "Undo",      "Redo",   "Separator", "Any",  "BG1",   "BG2",    "BG3",
        "Separator", "Object", "Sprite",    "Item", "Entrance", "Door", "Chest", "Block", "Palette"};
    std::ranges::for_each(tool_names,
                          [](const char *name) { TableSetupColumn(name); });

    TableNextColumn();
    if (Button(ICON_MD_UNDO)) {
      PRINT_IF_ERROR(Undo());
    }

    TableNextColumn();
    if (Button(ICON_MD_REDO)) {
      PRINT_IF_ERROR(Redo());
    }

    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_NONE, background_type_ == kBackgroundAny)) {
      background_type_ = kBackgroundAny;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_1, background_type_ == kBackground1)) {
      background_type_ = kBackground1;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_2, background_type_ == kBackground2)) {
      background_type_ = kBackground2;
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_FILTER_3, background_type_ == kBackground3)) {
      background_type_ = kBackground3;
    }

    TableNextColumn();
    Text(ICON_MD_MORE_VERT);

    TableNextColumn();
    if (RadioButton(ICON_MD_SQUARE, placement_type_ == kObject)) {
      placement_type_ = kObject;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Objects");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_PEST_CONTROL, placement_type_ == kSprite)) {
      placement_type_ = kSprite;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Sprites");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_GRASS, placement_type_ == kItem)) {
      placement_type_ = kItem;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Items");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_NAVIGATION, placement_type_ == kEntrance)) {
      placement_type_ = kEntrance;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Entrances");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_SENSOR_DOOR, placement_type_ == kDoor)) {
      placement_type_ = kDoor;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Doors");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_INVENTORY, placement_type_ == kChest)) {
      placement_type_ = kChest;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Chests");
    }

    TableNextColumn();
    if (RadioButton(ICON_MD_VIEW_MODULE, placement_type_ == kBlock)) {
      placement_type_ = kBlock;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Blocks");
    }

    TableNextColumn();
    if (Button(ICON_MD_PALETTE)) {
      palette_showing_ = !palette_showing_;
    }

    ImGui::EndTable();
  }
}

void DungeonEditor::DrawRoomSelector() {
  if (rom()->is_loaded()) {
    gui::InputHexWord("Room ID", &current_room_id_);
    gui::InputHex("Palette ID", &current_palette_id_);

    if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)9);
        BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      int i = 0;
      for (const auto each_room_name : zelda3::kRoomNames) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_room_id_ == i, "Dungeon Room Names", util::HexByte(i),
            each_room_name.data());
        if (ImGui::IsItemClicked()) {
          // TODO: Jump to tab if room is already open
          current_room_id_ = i;
          if (!active_rooms_.contains(i)) {
            active_rooms_.push_back(i);
          }
        }
        i += 1;
      }
    }
    EndChild();
  }
}

using ImGui::Separator;

void DungeonEditor::DrawEntranceSelector() {
  if (rom()->is_loaded()) {
    auto current_entrance = entrances_[current_entrance_id_];
    gui::InputHexWord("Entrance ID", &current_entrance.entrance_id_);
    gui::InputHexWord("Room ID", &current_entrance.room_, 50.f, true);
    SameLine();

    gui::InputHexByte("Dungeon ID", &current_entrance.dungeon_id_, 50.f, true);
    gui::InputHexByte("Blockset", &current_entrance.blockset_, 50.f, true);
    SameLine();

    gui::InputHexByte("Music", &current_entrance.music_, 50.f, true);
    SameLine();
    gui::InputHexByte("Floor", &current_entrance.floor_);
    Separator();

    gui::InputHexWord("Player X   ", &current_entrance.x_position_);
    SameLine();
    gui::InputHexWord("Player Y   ", &current_entrance.y_position_);

    gui::InputHexWord("Camera X", &current_entrance.camera_trigger_x_);
    SameLine();
    gui::InputHexWord("Camera Y", &current_entrance.camera_trigger_y_);

    gui::InputHexWord("Scroll X    ", &current_entrance.camera_x_);
    SameLine();
    gui::InputHexWord("Scroll Y    ", &current_entrance.camera_y_);

    gui::InputHexWord("Exit", &current_entrance.exit_, 50.f, true);

    Separator();
    Text("Camera Boundaries");
    Separator();
    Text("\t\t\t\t\tNorth         East         South         West");
    gui::InputHexByte("Quadrant", &current_entrance.camera_boundary_qn_, 50.f,
                      true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qe_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qs_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_qw_, 50.f, true);

    gui::InputHexByte("Full room", &current_entrance.camera_boundary_fn_, 50.f,
                      true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fe_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fs_, 50.f, true);
    SameLine();
    gui::InputHexByte("", &current_entrance.camera_boundary_fw_, 50.f, true);

    if (BeginChild("EntranceSelector", ImVec2(0, 0), true,
                   ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < 0x85 + 7; i++) {
        rom()->resource_label()->SelectableLabelWithNameEdit(
            current_entrance_id_ == i, "Dungeon Entrance Names",
            util::HexByte(i), zelda3::kEntranceNames[i].data());

        if (ImGui::IsItemClicked()) {
          current_entrance_id_ = i;
          if (!active_rooms_.contains(i)) {
            active_rooms_.push_back(entrances_[i].room_);
          }
        }
      }
    }
    EndChild();
  }
}

void DungeonEditor::DrawDungeonTabView() {
  static int next_tab_id = 0;

  if (BeginTabBar("MyTabBar", kDungeonTabBarFlags)) {
    if (ImGui::TabItemButton(ICON_MD_ADD, kDungeonTabFlags)) {
      if (std::find(active_rooms_.begin(), active_rooms_.end(),
                    current_room_id_) != active_rooms_.end()) {
        // Room is already open
        next_tab_id++;
      }
      active_rooms_.push_back(next_tab_id++);  // Add new tab
    }

    // Submit our regular tabs
    for (int n = 0; n < active_rooms_.Size;) {
      bool open = true;

      if (active_rooms_[n] > sizeof(zelda3::kRoomNames) / 4) {
        active_rooms_.erase(active_rooms_.Data + n);
        continue;
      }

      if (BeginTabItem(zelda3::kRoomNames[active_rooms_[n]].data(), &open,
                       ImGuiTabItemFlags_None)) {
        DrawDungeonCanvas(active_rooms_[n]);
        EndTabItem();
      }

      if (!open)
        active_rooms_.erase(active_rooms_.Data + n);
      else
        n++;
    }

    EndTabBar();
  }
  Separator();
}

void DungeonEditor::DrawDungeonCanvas(int room_id) {
  ImGui::BeginGroup();

  gui::InputHexByte("Layout", &rooms_[room_id].layout);
  SameLine();

  gui::InputHexByte("Blockset", &rooms_[room_id].blockset);
  SameLine();

  gui::InputHexByte("Spriteset", &rooms_[room_id].spriteset);
  SameLine();

  gui::InputHexByte("Palette", &rooms_[room_id].palette);

  gui::InputHexByte("Floor1", &rooms_[room_id].floor1);
  SameLine();

  gui::InputHexByte("Floor2", &rooms_[room_id].floor2);
  SameLine();

  gui::InputHexWord("Message ID", &rooms_[room_id].message_id_);
  SameLine();

  if (Button("Load Room")) {
    rooms_[room_id].LoadRoomGraphics(rooms_[room_id].blockset);
    rooms_[room_id].RenderRoomGraphics();
  }

  static bool show_objects = false;
  ImGui::Checkbox("Show Object Outlines", &show_objects);

  static bool render_objects = true;
  ImGui::Checkbox("Render Objects", &render_objects);

  static bool show_object_info = false;
  ImGui::Checkbox("Show Object Info", &show_object_info);

  static bool show_layout_objects = false;
  ImGui::Checkbox("Show Layout Objects", &show_layout_objects);

  static bool show_debug_info = false;
  ImGui::Checkbox("Show Debug Info", &show_debug_info);

  if (ImGui::Button("Clear Object Cache")) {
    object_render_cache_.clear();
  }

  ImGui::SameLine();
  ImGui::Text("Cache: %zu objects", object_render_cache_.size());

  // Object statistics and metadata
  if (show_debug_info) {
    ImGui::Separator();
    ImGui::Text("Room Statistics:");
    ImGui::Text("Objects: %zu", rooms_[room_id].tile_objects_.size());
    ImGui::Text("Layout Objects: %zu",
                rooms_[room_id].GetLayout().GetObjects().size());
    ImGui::Text("Sprites: %zu", rooms_[room_id].sprites_.size());
    ImGui::Text("Chests: %zu", rooms_[room_id].chests_in_room_.size());

    // Palette information
    ImGui::Text("Current Palette Group: %d", current_palette_group_id_);
    ImGui::Text("Palette Hash: %#016llx", last_palette_hash_);

    // Object type breakdown
    ImGui::Separator();
    ImGui::Text("Object Type Breakdown:");
    std::map<int, int> object_type_counts;
    for (const auto &obj : rooms_[room_id].tile_objects_) {
      object_type_counts[obj.id_]++;
    }
    for (const auto &[type, count] : object_type_counts) {
      ImGui::Text("Type 0x%02X: %d objects", type, count);
    }

    // Layout object type breakdown
    ImGui::Separator();
    ImGui::Text("Layout Object Types:");
    auto walls = rooms_[room_id].GetLayout().GetObjectsByType(
        zelda3::RoomLayoutObject::Type::kWall);
    auto floors = rooms_[room_id].GetLayout().GetObjectsByType(
        zelda3::RoomLayoutObject::Type::kFloor);
    auto doors = rooms_[room_id].GetLayout().GetObjectsByType(
        zelda3::RoomLayoutObject::Type::kDoor);
    ImGui::Text("Walls: %zu", walls.size());
    ImGui::Text("Floors: %zu", floors.size());
    ImGui::Text("Doors: %zu", doors.size());
  }

  // Object selection and editing
  static int selected_object_id = -1;
  if (ImGui::Button("Select Object")) {
    // This would open an object selection dialog
    // For now, just cycle through objects
    if (!rooms_[room_id].tile_objects_.empty()) {
      selected_object_id =
          (selected_object_id + 1) % rooms_[room_id].tile_objects_.size();
    }
  }

  if (selected_object_id >= 0 &&
      selected_object_id < (int)rooms_[room_id].tile_objects_.size()) {
    const auto &selected_obj =
        rooms_[room_id].tile_objects_[selected_object_id];
    ImGui::Separator();
    ImGui::Text("Selected Object:");
    ImGui::Text("ID: 0x%02X", selected_obj.id_);
    ImGui::Text("Position: (%d, %d)", selected_obj.x_, selected_obj.y_);
    ImGui::Text("Size: 0x%02X", selected_obj.size_);
    ImGui::Text("Layer: %d", static_cast<int>(selected_obj.layer_));
    ImGui::Text("Tile Count: %d", selected_obj.GetTileCount());

    // Object editing controls
    if (ImGui::Button("Edit Object")) {
      // This would open an object editing dialog
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete Object")) {
      // This would remove the object from the room
    }
  }

  ImGui::EndGroup();

  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  if (is_loaded_) {
    canvas_.DrawBitmap(gfx::Arena::Get().bg1().bitmap(), 1.0f, 1.0f);
    canvas_.DrawBitmap(gfx::Arena::Get().bg2().bitmap(), 1.0f, 1.0f);

    if (show_objects || render_objects) {
      // Get the current room's palette for object rendering
      auto current_palette =
          rom()->palette_group().dungeon_main[current_palette_group_id_];

      // Clear cache if palette changed
      uint64_t current_palette_hash = 0;
      for (size_t i = 0; i < current_palette.size() && i < 16; ++i) {
        current_palette_hash ^=
            std::hash<uint16_t>{}(current_palette[i].snes()) + 0x9e3779b9 +
            (current_palette_hash << 6) + (current_palette_hash >> 2);
      }

      if (current_palette_hash != last_palette_hash_) {
        object_render_cache_.clear();
        last_palette_hash_ = current_palette_hash;
      }

      // Render layout objects (walls, floors, etc.) first
      if (show_layout_objects) {
        RenderLayoutObjects(rooms_[room_id].GetLayout(), current_palette);
      }

      if (rooms_[room_id].tile_objects_.empty()) {
        // Load the objects for the room
        rooms_[room_id].LoadObjects();
      }

      // Render regular room objects
      for (const auto &object : rooms_[room_id].tile_objects_) {
        // Convert room coordinates to canvas coordinates
        int canvas_x = object.x_ * 16;
        int canvas_y = object.y_ * 16;

        if (show_objects) {
          // Draw object outline - use size_ to determine dimensions
          int outline_width = 16;  // Default 16x16
          int outline_height = 16;

          // Calculate dimensions based on object size
          if (object.size_ > 0) {
            // Size encoding: bits 0-1 = width, bits 2-3 = height
            int width_bits = object.size_ & 0x03;
            int height_bits = (object.size_ >> 2) & 0x03;

            outline_width = (width_bits + 1) * 16;
            outline_height = (height_bits + 1) * 16;
          }

          canvas_.DrawOutline(object.x_, object.y_, outline_width,
                              outline_height);
        }

        if (render_objects) {
          // Render the actual object using ObjectRenderer
          RenderObjectInCanvas(object, current_palette);
        }

        if (show_object_info) {
          // Display object information
          DisplayObjectInfo(object, canvas_x, canvas_y);
        }
      }
    }
  }
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonEditor::RenderObjectInCanvas(const zelda3::RoomObject &object,
                                         const gfx::SnesPalette &palette) {
  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Check if tiles were loaded successfully using the new method
  auto tiles_result = mutable_object.GetTiles();
  if (!tiles_result.ok() || tiles_result->empty()) {
    return;  // Skip objects without tiles
  }

  // Calculate palette hash for caching
  uint64_t palette_hash = 0;
  for (size_t i = 0; i < palette.size() && i < 16; ++i) {
    palette_hash ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 +
                    (palette_hash << 6) + (palette_hash >> 2);
  }

  // Check cache first
  for (auto &cached : object_render_cache_) {
    if (cached.object_id == object.id_ && cached.object_x == object.x_ &&
        cached.object_y == object.y_ && cached.object_size == object.size_ &&
        cached.palette_hash == palette_hash && cached.is_valid) {
      // Use cached bitmap
      int canvas_x = object.x_ * 16;
      int canvas_y = object.y_ * 16;
      canvas_.DrawBitmap(cached.rendered_bitmap, canvas_x, canvas_y, 1.0f, 255);
      return;
    }
  }

  // Render the object to a bitmap
  auto render_result = object_renderer_.RenderObject(mutable_object, palette);
  if (!render_result.ok()) {
    return;  // Skip if rendering failed
  }

  auto object_bitmap = std::move(render_result.value());

  // Set the palette for the bitmap
  object_bitmap.SetPalette(palette);

  // Render the bitmap to a texture so it can be drawn
  core::Renderer::Get().RenderBitmap(&object_bitmap);

  // Convert room coordinates to canvas coordinates
  // Room coordinates are in 16x16 tile units, canvas coordinates are in pixels
  int canvas_x = object.x_ * 16;
  int canvas_y = object.y_ * 16;

  // Draw the object bitmap to the canvas immediately
  canvas_.DrawBitmap(object_bitmap, canvas_x, canvas_y, 1.0f, 255);

  // Cache the rendered bitmap (create a copy for caching)
  ObjectRenderCache cache_entry;
  cache_entry.object_id = object.id_;
  cache_entry.object_x = object.x_;
  cache_entry.object_y = object.y_;
  cache_entry.object_size = object.size_;
  cache_entry.palette_hash = palette_hash;
  cache_entry.rendered_bitmap = object_bitmap;  // Copy instead of move
  cache_entry.is_valid = true;

  // Add to cache (limit cache size)
  if (object_render_cache_.size() >= 100) {
    object_render_cache_.erase(object_render_cache_.begin());
  }
  object_render_cache_.push_back(std::move(cache_entry));
}

void DungeonEditor::DisplayObjectInfo(const zelda3::RoomObject &object,
                                      int canvas_x, int canvas_y) {
  // Display object information as text overlay
  std::string info_text = absl::StrFormat("ID:%d X:%d Y:%d S:%d", object.id_,
                                          object.x_, object.y_, object.size_);

  // Draw text at the object position
  canvas_.DrawText(info_text, canvas_x, canvas_y - 12);
}

void DungeonEditor::RenderLayoutObjects(const zelda3::RoomLayout &layout,
                                        const gfx::SnesPalette &palette) {
  // Render layout objects (walls, floors, etc.) as simple colored rectangles
  // This provides a visual representation of the room's structure

  for (const auto &layout_obj : layout.GetObjects()) {
    int canvas_x = layout_obj.x() * 16;
    int canvas_y = layout_obj.y() * 16;

    // Choose color based on object type
    gfx::SnesColor color;
    switch (layout_obj.type()) {
      case zelda3::RoomLayoutObject::Type::kWall:
        color = gfx::SnesColor(0x7FFF);  // Gray
        break;
      case zelda3::RoomLayoutObject::Type::kFloor:
        color = gfx::SnesColor(0x4210);  // Dark brown
        break;
      case zelda3::RoomLayoutObject::Type::kCeiling:
        color = gfx::SnesColor(0x739C);  // Light gray
        break;
      case zelda3::RoomLayoutObject::Type::kPit:
        color = gfx::SnesColor(0x0000);  // Black
        break;
      case zelda3::RoomLayoutObject::Type::kWater:
        color = gfx::SnesColor(0x001F);  // Blue
        break;
      case zelda3::RoomLayoutObject::Type::kStairs:
        color = gfx::SnesColor(0x7E0F);  // Yellow
        break;
      case zelda3::RoomLayoutObject::Type::kDoor:
        color = gfx::SnesColor(0xF800);  // Red
        break;
      default:
        color = gfx::SnesColor(0x7C1F);  // Magenta for unknown
        break;
    }

    // Draw a simple rectangle for the layout object
    // This is a placeholder - in a real implementation, you'd render the actual
    // tile
    canvas_.DrawRect(canvas_x, canvas_y, 16, 16,
                     gui::ConvertSnesColorToImVec4(color));
  }
}

void DungeonEditor::DrawRoomGraphics() {
  const auto height = 0x40;
  room_gfx_canvas_.DrawBackground();
  room_gfx_canvas_.DrawContextMenu();
  room_gfx_canvas_.DrawTileSelector(32);
  if (is_loaded_) {
    auto blocks = rooms_[current_room_id_].blocks();
    int current_block = 0;
    for (int block : blocks) {
      int offset = height * (current_block + 1);
      int top_left_y = room_gfx_canvas_.zero_point().y + 2;
      if (current_block >= 1) {
        top_left_y = room_gfx_canvas_.zero_point().y + height * current_block;
      }
      room_gfx_canvas_.draw_list()->AddImage(
          (ImTextureID)(intptr_t)gfx::Arena::Get()
              .gfx_sheets()[block]
              .texture(),
          ImVec2(room_gfx_canvas_.zero_point().x + 2, top_left_y),
          ImVec2(room_gfx_canvas_.zero_point().x + 0x100,
                 room_gfx_canvas_.zero_point().y + offset));
      current_block += 1;
    }
  }
  room_gfx_canvas_.DrawGrid(32.0f);
  room_gfx_canvas_.DrawOverlay();
}

void DungeonEditor::DrawTileSelector() {
  if (BeginTabBar("##TabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
    if (BeginTabItem("Room Graphics")) {
      if (ImGuiID child_id = ImGui::GetID((void *)(intptr_t)3);
          BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                     ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        DrawRoomGraphics();
      }
      EndChild();
      EndTabItem();
    }

    if (BeginTabItem("Object Renderer")) {
      DrawObjectRenderer();
      EndTabItem();
    }
    EndTabBar();
  }
}

void DungeonEditor::DrawObjectRenderer() {
  if (BeginTable("DungeonObjectEditorTable", 2, kDungeonObjectTableFlags,
                 ImVec2(0, 0))) {
    TableSetupColumn("Dungeon Objects", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Canvas");

    TableNextColumn();
    BeginChild("DungeonObjectButtons", ImVec2(250, 0), true);

    static int selected_object = 0;
    int i = 0;
    for (const auto object_name : zelda3::Type1RoomObjectNames) {
      if (ImGui::Selectable(object_name.data(), selected_object == i)) {
        selected_object = i;

        // Create a test object and render it
        auto test_object = zelda3::RoomObject(i, 0, 0, 0x12, 0);
        test_object.set_rom(rom_);
        test_object.EnsureTilesLoaded();

        // Get current palette
        auto palette =
            rom()->palette_group().dungeon_main[current_palette_group_id_];

        // Render object preview
        auto result = object_renderer_.GetObjectPreview(test_object, palette);
        if (result.ok()) {
          object_loaded_ = true;
        }
      }
      i += 1;
    }

    EndChild();

    // Right side of the table - Canvas
    TableNextColumn();
    BeginChild("DungeonObjectCanvas", ImVec2(276, 0x10 * 0x40 + 1), true);

    object_canvas_.DrawBackground(ImVec2(256 + 1, 0x10 * 0x40 + 1));
    object_canvas_.DrawContextMenu();
    object_canvas_.DrawTileSelector(32);
    object_canvas_.DrawGrid(32.0f);
    object_canvas_.DrawOverlay();

    EndChild();
    ImGui::EndTable();
  }

  if (object_loaded_) {
    ImGui::Begin("Object Preview", &object_loaded_, 0);
    ImGui::Text("Object rendered successfully using improved renderer!");
    ImGui::End();
  }
}

void DungeonEditor::CalculateUsageStats() {
  for (const auto &room : rooms_) {
    if (blockset_usage_.find(room.blockset) == blockset_usage_.end()) {
      blockset_usage_[room.blockset] = 1;
    } else {
      blockset_usage_[room.blockset] += 1;
    }

    if (spriteset_usage_.find(room.spriteset) == spriteset_usage_.end()) {
      spriteset_usage_[room.spriteset] = 1;
    } else {
      spriteset_usage_[room.spriteset] += 1;
    }

    if (palette_usage_.find(room.palette) == palette_usage_.end()) {
      palette_usage_[room.palette] = 1;
    } else {
      palette_usage_[room.palette] += 1;
    }
  }
}

void DungeonEditor::RenderSetUsage(
    const absl::flat_hash_map<uint16_t, int> &usage_map, uint16_t &selected_set,
    int spriteset_offset) {
  // Sort the usage map by set number
  std::vector<std::pair<uint16_t, int>> sorted_usage(usage_map.begin(),
                                                     usage_map.end());
  std::sort(sorted_usage.begin(), sorted_usage.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });

  for (const auto &[set, count] : sorted_usage) {
    std::string display_str;
    if (spriteset_offset != 0x00) {
      display_str = absl::StrFormat("%#02x, %#02x: %d", set,
                                    (set + spriteset_offset), count);
    } else {
      display_str =
          absl::StrFormat("%#02x: %d", (set + spriteset_offset), count);
    }
    if (ImGui::Selectable(display_str.c_str(), selected_set == set)) {
      selected_set = set;  // Update the selected set when clicked
    }
  }
}

namespace {
// Calculate the unused sets in a usage map
// Range for blocksets 0-0x24
// Range for spritesets 0-0x8F
// Range for palettes 0-0x47
template <typename T>
void RenderUnusedSets(const absl::flat_hash_map<T, int> &usage_map, int max_set,
                      int spriteset_offset = 0x00) {
  std::vector<int> unused_sets;
  for (int i = 0; i < max_set; i++) {
    if (usage_map.find(i) == usage_map.end()) {
      unused_sets.push_back(i);
    }
  }
  for (const auto &set : unused_sets) {
    if (spriteset_offset != 0x00) {
      Text("%#02x, %#02x", set, (set + spriteset_offset));
    } else {
      Text("%#02x", set);
    }
  }
}
}  // namespace

void DungeonEditor::DrawUsageStats() {
  if (Button("Refresh")) {
    selected_blockset_ = 0xFFFF;
    selected_spriteset_ = 0xFFFF;
    selected_palette_ = 0xFFFF;
    spriteset_usage_.clear();
    blockset_usage_.clear();
    palette_usage_.clear();
    CalculateUsageStats();
  }

  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  if (BeginTable("DungeonUsageStatsTable", 8,
                 kDungeonTableFlags | ImGuiTableFlags_SizingFixedFit,
                 ImGui::GetContentRegionAvail())) {
    TableSetupColumn("Blockset Usage");
    TableSetupColumn("Unused Blockset");
    TableSetupColumn("Palette Usage");
    TableSetupColumn("Unused Palette");
    TableSetupColumn("Spriteset Usage");
    TableSetupColumn("Unused Spriteset");
    TableSetupColumn("Usage Grid");
    TableSetupColumn("Group Preview");
    TableHeadersRow();
    ImGui::PopStyleVar(2);

    TableNextColumn();
    BeginChild("BlocksetUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(blockset_usage_, selected_blockset_);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedBlocksetScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(blockset_usage_, 0x25);
    EndChild();

    TableNextColumn();
    BeginChild("PaletteUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(palette_usage_, selected_palette_);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedPaletteScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(palette_usage_, 0x48);
    EndChild();

    TableNextColumn();

    BeginChild("SpritesetUsageScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderSetUsage(spriteset_usage_, selected_spriteset_, 0x40);
    EndChild();

    TableNextColumn();
    BeginChild("UnusedSpritesetScroll", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    RenderUnusedSets(spriteset_usage_, 0x90, 0x40);
    EndChild();

    TableNextColumn();
    BeginChild("UsageGrid", ImVec2(0, 0), true,
               ImGuiWindowFlags_HorizontalScrollbar);
    Text("%s", absl::StrFormat("Total size of all rooms: %d hex format: %#06x",
                               total_room_size_, total_room_size_)
                   .c_str());
    DrawUsageGrid();
    EndChild();

    TableNextColumn();
    if (selected_blockset_ < 0x25) {
      gfx_group_editor_.SetSelectedBlockset(selected_blockset_);
      gfx_group_editor_.DrawBlocksetViewer(true);
    } else if (selected_spriteset_ < 0x90) {
      gfx_group_editor_.SetSelectedSpriteset(selected_spriteset_ + 0x40);
      gfx_group_editor_.DrawSpritesetViewer(true);
    }
  }
  ImGui::EndTable();
}

void DungeonEditor::DrawUsageGrid() {
  int totalSquares = 296;
  int squaresWide = 16;
  int squaresTall = (totalSquares + squaresWide - 1) /
                    squaresWide;  // Ceiling of totalSquares/squaresWide

  for (int row = 0; row < squaresTall; ++row) {
    ImGui::NewLine();

    for (int col = 0; col < squaresWide; ++col) {
      // Check if we have reached 295 squares
      if (row * squaresWide + col >= totalSquares) {
        break;
      }
      // Determine if this square should be highlighted
      const auto &room = rooms_[row * squaresWide + col];

      // Create a button or selectable for each square
      ImGui::BeginGroup();
      ImVec4 color = room_palette_[room.palette];
      color.x = color.x / 255;
      color.y = color.y / 255;
      color.z = color.z / 255;
      color.w = 1.0f;
      if (room_sizes_[row * squaresWide + col] > 0xFFFF) {
        color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);  // Or any highlight color
      }
      if (room_sizes_[row * squaresWide + col] == 0) {
        color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);  // Or any highlight color
      }
      ImGui::PushStyleColor(ImGuiCol_Button, color);
      // Make the button text darker
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

      bool highlight = room.blockset == selected_blockset_ ||
                       room.spriteset == selected_spriteset_ ||
                       room.palette == selected_palette_;

      // Set highlight color if needed
      if (highlight) {
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            ImVec4(1.0f, 0.5f, 0.0f, 1.0f));  // Or any highlight color
      }
      if (Button(absl::StrFormat("%#x", room_sizes_[row * squaresWide + col])
                     .c_str(),
                 ImVec2(55, 30))) {
        // Switch over to the room editor tab
        // and add a room tab by the ID of the square
        // that was clicked
      }
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(
            absl::StrFormat("RoomContextMenu%d", row * squaresWide + col)
                .c_str());
      }
      ImGui::PopStyleColor(2);
      ImGui::EndGroup();

      // Reset style if it was highlighted
      if (highlight) {
        ImGui::PopStyleColor();
      }

      // Check if the square is hovered
      if (ImGui::IsItemHovered()) {
        // Display a tooltip with all the room properties
        ImGui::BeginTooltip();
        Text("Room ID: %d", row * squaresWide + col);
        Text("Blockset: %#02x", room.blockset);
        Text("Spriteset: %#02x", room.spriteset);
        Text("Palette: %#02x", room.palette);
        Text("Floor1: %#02x", room.floor1);
        Text("Floor2: %#02x", room.floor2);
        Text("Message ID: %#04x", room.message_id_);
        Text("Size: %#016llx", room_sizes_[row * squaresWide + col]);
        Text("Size Pointer: %#016llx",
             room_size_pointers_[row * squaresWide + col]);
        ImGui::EndTooltip();
      }

      // Keep squares in the same line
      SameLine();
    }
  }
}

// New editor method implementations
void DungeonEditor::DrawObjectEditor() {
  if (!object_editor_) {
    ImGui::Text("Object editor not initialized");
    return;
  }

  ImGui::Text("Object Editor");
  ImGui::Separator();

  // Display current editing mode
  auto mode = object_editor_->GetMode();
  const char* mode_names[] = {
    "Select", "Insert", "Delete", "Edit", "Layer", "Preview"
  };
  ImGui::Text("Mode: %s", mode_names[static_cast<int>(mode)]);

  // Mode selection
  if (ImGui::Button("Select Mode")) {
    object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kSelect);
  }
  ImGui::SameLine();
  if (ImGui::Button("Insert Mode")) {
    object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kInsert);
  }
  ImGui::SameLine();
  if (ImGui::Button("Edit Mode")) {
    object_editor_->SetMode(zelda3::DungeonObjectEditor::Mode::kEdit);
  }

  // Layer selection
  int current_layer = object_editor_->GetCurrentLayer();
  if (ImGui::SliderInt("Layer", &current_layer, 0, 2)) {
    object_editor_->SetCurrentLayer(current_layer);
  }

  // Object type selection
  int current_object_type = object_editor_->GetCurrentObjectType();
  if (ImGui::InputInt("Object Type", &current_object_type, 1, 16)) {
    if (current_object_type >= 0 && current_object_type <= 0x3FF) {
      object_editor_->SetCurrentObjectType(current_object_type);
    }
  }

  // Editor configuration
  auto config = object_editor_->GetConfig();
  if (ImGui::Checkbox("Snap to Grid", &config.snap_to_grid)) {
    object_editor_->SetConfig(config);
  }
  if (ImGui::Checkbox("Show Grid", &config.show_grid)) {
    object_editor_->SetConfig(config);
  }
  if (ImGui::Checkbox("Show Preview", &config.show_preview)) {
    object_editor_->SetConfig(config);
  }

  // Object count and selection info
  ImGui::Separator();
  ImGui::Text("Objects: %zu", object_editor_->GetObjectCount());
  
  auto selection = object_editor_->GetSelection();
  if (!selection.selected_objects.empty()) {
    ImGui::Text("Selected: %zu objects", selection.selected_objects.size());
  }

  // Undo/Redo status
  ImGui::Separator();
  ImGui::Text("Undo: %s", object_editor_->CanUndo() ? "Available" : "Not Available");
  ImGui::Text("Redo: %s", object_editor_->CanRedo() ? "Available" : "Not Available");
}

void DungeonEditor::DrawSpriteEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Sprite Editor");
  ImGui::Separator();

  // Display current room sprites
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto sprites_result = dungeon_editor_system_->GetSpritesByRoom(current_room);
  
  if (sprites_result.ok()) {
    auto sprites = sprites_result.value();
    ImGui::Text("Sprites in room %d: %zu", current_room, sprites.size());
    
    for (const auto& sprite : sprites) {
      ImGui::Text("ID: %d, Type: %d, Position: (%d, %d)", 
                  sprite.sprite_id, static_cast<int>(sprite.type), 
                  sprite.x, sprite.y);
    }
  } else {
    ImGui::Text("Error loading sprites: %s", sprites_result.status().message().c_str());
  }

  // Sprite placement controls
  static int new_sprite_id = 0;
  static int new_sprite_x = 0;
  static int new_sprite_y = 0;
  static int new_sprite_layer = 0;

  ImGui::Separator();
  ImGui::Text("Add New Sprite");
  ImGui::InputInt("Sprite ID", &new_sprite_id);
  ImGui::InputInt("X Position", &new_sprite_x);
  ImGui::InputInt("Y Position", &new_sprite_y);
  ImGui::SliderInt("Layer", &new_sprite_layer, 0, 2);

  if (ImGui::Button("Add Sprite")) {
    zelda3::DungeonEditorSystem::SpriteData sprite_data;
    sprite_data.sprite_id = new_sprite_id;
    sprite_data.type = zelda3::DungeonEditorSystem::SpriteType::kEnemy;
    sprite_data.x = new_sprite_x;
    sprite_data.y = new_sprite_y;
    sprite_data.layer = new_sprite_layer;
    
    auto status = dungeon_editor_system_->AddSprite(sprite_data);
    if (!status.ok()) {
      ImGui::Text("Error adding sprite: %s", status.message().c_str());
    }
  }
}

void DungeonEditor::DrawItemEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Item Editor");
  ImGui::Separator();

  // Display current room items
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto items_result = dungeon_editor_system_->GetItemsByRoom(current_room);
  
  if (items_result.ok()) {
    auto items = items_result.value();
    ImGui::Text("Items in room %d: %zu", current_room, items.size());
    
    for (const auto& item : items) {
      ImGui::Text("ID: %d, Type: %d, Position: (%d, %d), Hidden: %s", 
                  item.item_id, static_cast<int>(item.type), 
                  item.x, item.y, item.is_hidden ? "Yes" : "No");
    }
  } else {
    ImGui::Text("Error loading items: %s", items_result.status().message().c_str());
  }

  // Item placement controls
  static int new_item_id = 0;
  static int new_item_x = 0;
  static int new_item_y = 0;
  static bool new_item_hidden = false;

  ImGui::Separator();
  ImGui::Text("Add New Item");
  ImGui::InputInt("Item ID", &new_item_id);
  ImGui::InputInt("X Position", &new_item_x);
  ImGui::InputInt("Y Position", &new_item_y);
  ImGui::Checkbox("Hidden", &new_item_hidden);

  if (ImGui::Button("Add Item")) {
    zelda3::DungeonEditorSystem::ItemData item_data;
    item_data.item_id = new_item_id;
    item_data.type = zelda3::DungeonEditorSystem::ItemType::kKey;
    item_data.x = new_item_x;
    item_data.y = new_item_y;
    item_data.room_id = current_room;
    item_data.is_hidden = new_item_hidden;
    
    auto status = dungeon_editor_system_->AddItem(item_data);
    if (!status.ok()) {
      ImGui::Text("Error adding item: %s", status.message().c_str());
    }
  }
}

void DungeonEditor::DrawEntranceEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Entrance Editor");
  ImGui::Separator();

  // Display current room entrances
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto entrances_result = dungeon_editor_system_->GetEntrancesByRoom(current_room);
  
  if (entrances_result.ok()) {
    auto entrances = entrances_result.value();
    ImGui::Text("Entrances in room %d: %zu", current_room, entrances.size());
    
    for (const auto& entrance : entrances) {
      ImGui::Text("ID: %d, Type: %d, Target Room: %d, Target Position: (%d, %d)", 
                  entrance.entrance_id, static_cast<int>(entrance.type),
                  entrance.target_room_id, entrance.target_x, entrance.target_y);
    }
  } else {
    ImGui::Text("Error loading entrances: %s", entrances_result.status().message().c_str());
  }

  // Entrance creation controls
  static int target_room_id = 0;
  static int target_x = 0;
  static int target_y = 0;
  static int source_x = 0;
  static int source_y = 0;

  ImGui::Separator();
  ImGui::Text("Create New Entrance");
  ImGui::InputInt("Target Room ID", &target_room_id);
  ImGui::InputInt("Source X", &source_x);
  ImGui::InputInt("Source Y", &source_y);
  ImGui::InputInt("Target X", &target_x);
  ImGui::InputInt("Target Y", &target_y);

  if (ImGui::Button("Connect Rooms")) {
    auto status = dungeon_editor_system_->ConnectRooms(current_room, target_room_id, 
                                                       source_x, source_y, 
                                                       target_x, target_y);
    if (!status.ok()) {
      ImGui::Text("Error connecting rooms: %s", status.message().c_str());
    }
  }
}

void DungeonEditor::DrawDoorEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Door Editor");
  ImGui::Separator();

  // Display current room doors
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto doors_result = dungeon_editor_system_->GetDoorsByRoom(current_room);
  
  if (doors_result.ok()) {
    auto doors = doors_result.value();
    ImGui::Text("Doors in room %d: %zu", current_room, doors.size());
    
    for (const auto& door : doors) {
      ImGui::Text("ID: %d, Position: (%d, %d), Direction: %d, Target Room: %d", 
                  door.door_id, door.x, door.y, door.direction, door.target_room_id);
    }
  } else {
    ImGui::Text("Error loading doors: %s", doors_result.status().message().c_str());
  }

  // Door creation controls
  static int door_x = 0;
  static int door_y = 0;
  static int door_direction = 0;
  static int door_target_room = 0;
  static int door_target_x = 0;
  static int door_target_y = 0;
  static bool door_locked = false;
  static bool door_requires_key = false;
  static int door_key_type = 0;

  ImGui::Separator();
  ImGui::Text("Create New Door");
  ImGui::InputInt("Door X", &door_x);
  ImGui::InputInt("Door Y", &door_y);
  ImGui::SliderInt("Direction", &door_direction, 0, 3);
  ImGui::InputInt("Target Room", &door_target_room);
  ImGui::InputInt("Target X", &door_target_x);
  ImGui::InputInt("Target Y", &door_target_y);
  ImGui::Checkbox("Locked", &door_locked);
  ImGui::Checkbox("Requires Key", &door_requires_key);
  ImGui::InputInt("Key Type", &door_key_type);

  if (ImGui::Button("Add Door")) {
    zelda3::DungeonEditorSystem::DoorData door_data;
    door_data.room_id = current_room;
    door_data.x = door_x;
    door_data.y = door_y;
    door_data.direction = door_direction;
    door_data.target_room_id = door_target_room;
    door_data.target_x = door_target_x;
    door_data.target_y = door_target_y;
    door_data.is_locked = door_locked;
    door_data.requires_key = door_requires_key;
    door_data.key_type = door_key_type;
    
    auto status = dungeon_editor_system_->AddDoor(door_data);
    if (!status.ok()) {
      ImGui::Text("Error adding door: %s", status.message().c_str());
    }
  }
}

void DungeonEditor::DrawChestEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Chest Editor");
  ImGui::Separator();

  // Display current room chests
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto chests_result = dungeon_editor_system_->GetChestsByRoom(current_room);
  
  if (chests_result.ok()) {
    auto chests = chests_result.value();
    ImGui::Text("Chests in room %d: %zu", current_room, chests.size());
    
    for (const auto& chest : chests) {
      ImGui::Text("ID: %d, Position: (%d, %d), Big: %s, Item: %d, Quantity: %d", 
                  chest.chest_id, chest.x, chest.y, 
                  chest.is_big_chest ? "Yes" : "No",
                  chest.item_id, chest.item_quantity);
    }
  } else {
    ImGui::Text("Error loading chests: %s", chests_result.status().message().c_str());
  }

  // Chest creation controls
  static int chest_x = 0;
  static int chest_y = 0;
  static bool chest_big = false;
  static int chest_item_id = 0;
  static int chest_item_quantity = 1;

  ImGui::Separator();
  ImGui::Text("Create New Chest");
  ImGui::InputInt("Chest X", &chest_x);
  ImGui::InputInt("Chest Y", &chest_y);
  ImGui::Checkbox("Big Chest", &chest_big);
  ImGui::InputInt("Item ID", &chest_item_id);
  ImGui::InputInt("Item Quantity", &chest_item_quantity);

  if (ImGui::Button("Add Chest")) {
    zelda3::DungeonEditorSystem::ChestData chest_data;
    chest_data.room_id = current_room;
    chest_data.x = chest_x;
    chest_data.y = chest_y;
    chest_data.is_big_chest = chest_big;
    chest_data.item_id = chest_item_id;
    chest_data.item_quantity = chest_item_quantity;
    
    auto status = dungeon_editor_system_->AddChest(chest_data);
    if (!status.ok()) {
      ImGui::Text("Error adding chest: %s", status.message().c_str());
    }
  }
}

void DungeonEditor::DrawPropertiesEditor() {
  if (!dungeon_editor_system_) {
    ImGui::Text("Dungeon editor system not initialized");
    return;
  }

  ImGui::Text("Properties Editor");
  ImGui::Separator();

  // Display current room properties
  auto current_room = dungeon_editor_system_->GetCurrentRoom();
  auto properties_result = dungeon_editor_system_->GetRoomProperties(current_room);
  
  if (properties_result.ok()) {
    auto properties = properties_result.value();
    ImGui::Text("Room Properties for room %d", current_room);
    
    static char room_name[256] = {0};
    static char room_description[512] = {0};
    static int dungeon_id = 0;
    static int floor_level = 0;
    static bool is_boss_room = false;
    static bool is_save_room = false;
    static bool is_shop_room = false;
    static int music_id = 0;
    static int ambient_sound_id = 0;

    // Copy current values to static variables for editing
    strncpy(room_name, properties.name.c_str(), sizeof(room_name) - 1);
    strncpy(room_description, properties.description.c_str(), sizeof(room_description) - 1);
    dungeon_id = properties.dungeon_id;
    floor_level = properties.floor_level;
    is_boss_room = properties.is_boss_room;
    is_save_room = properties.is_save_room;
    is_shop_room = properties.is_shop_room;
    music_id = properties.music_id;
    ambient_sound_id = properties.ambient_sound_id;

    ImGui::InputText("Room Name", room_name, sizeof(room_name));
    ImGui::InputTextMultiline("Description", room_description, sizeof(room_description));
    ImGui::InputInt("Dungeon ID", &dungeon_id);
    ImGui::InputInt("Floor Level", &floor_level);
    ImGui::Checkbox("Boss Room", &is_boss_room);
    ImGui::Checkbox("Save Room", &is_save_room);
    ImGui::Checkbox("Shop Room", &is_shop_room);
    ImGui::InputInt("Music ID", &music_id);
    ImGui::InputInt("Ambient Sound ID", &ambient_sound_id);

    if (ImGui::Button("Save Properties")) {
      zelda3::DungeonEditorSystem::RoomProperties new_properties;
      new_properties.room_id = current_room;
      new_properties.name = room_name;
      new_properties.description = room_description;
      new_properties.dungeon_id = dungeon_id;
      new_properties.floor_level = floor_level;
      new_properties.is_boss_room = is_boss_room;
      new_properties.is_save_room = is_save_room;
      new_properties.is_shop_room = is_shop_room;
      new_properties.music_id = music_id;
      new_properties.ambient_sound_id = ambient_sound_id;
      
      auto status = dungeon_editor_system_->SetRoomProperties(current_room, new_properties);
      if (!status.ok()) {
        ImGui::Text("Error saving properties: %s", status.message().c_str());
      }
    }
  } else {
    ImGui::Text("Error loading properties: %s", properties_result.status().message().c_str());
  }

  // Dungeon-wide settings
  ImGui::Separator();
  ImGui::Text("Dungeon Settings");
  
  auto dungeon_settings_result = dungeon_editor_system_->GetDungeonSettings();
  if (dungeon_settings_result.ok()) {
    auto settings = dungeon_settings_result.value();
    ImGui::Text("Dungeon: %s", settings.name.c_str());
    ImGui::Text("Total Rooms: %d", settings.total_rooms);
    ImGui::Text("Starting Room: %d", settings.starting_room_id);
    ImGui::Text("Boss Room: %d", settings.boss_room_id);
  }
}

}  // namespace yaze::editor
