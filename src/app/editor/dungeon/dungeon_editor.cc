#include "dungeon_editor.h"

#include "absl/strings/str_format.h"
#include "app/core/window.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/dungeon_editor_system.h"
#include "app/zelda3/dungeon/dungeon_object_editor.h"
#include "app/zelda3/dungeon/room.h"
#include "imgui/imgui.h"

namespace yaze::editor {

using core::Renderer;

using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::BeginTable;
using ImGui::Button;
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
    dungeon_editor_system_ =
        std::make_unique<zelda3::DungeonEditorSystem>(rom_);
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
  
  // Initialize the new UI components with loaded data
  room_selector_.set_rom(rom_);
  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.set_room_selected_callback([this](int room_id) { OnRoomSelected(room_id); });
  
  canvas_viewer_.SetRom(rom_);
  canvas_viewer_.SetRooms(&rooms_);
  canvas_viewer_.SetCurrentPaletteGroup(current_palette_group_);
  canvas_viewer_.SetCurrentPaletteId(current_palette_id_);
  
  object_selector_.SetRom(rom_);
  object_selector_.SetCurrentPaletteGroup(current_palette_group_);
  object_selector_.SetCurrentPaletteId(current_palette_id_);
  object_selector_.set_dungeon_editor_system(&dungeon_editor_system_);
  object_selector_.set_object_editor(&object_editor_);
  object_selector_.set_rooms(&rooms_);
  
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

  // Simplified 3-column layout: Room/Entrance Selector | Canvas | Object Selector/Editor
  if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Room/Entrance Selector", ImGuiTableColumnFlags_WidthFixed, 250);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch,
                     ImGui::GetContentRegionAvail().x);
    TableSetupColumn("Object Selector/Editor", ImGuiTableColumnFlags_WidthFixed, 300);
    TableHeadersRow();
    TableNextRow();

    // Column 1: Room and Entrance Selector (using new component)
    TableNextColumn();
    room_selector_.Draw();

    // Column 2: Main Canvas with tabbed room view functionality
    TableNextColumn();
    DrawDungeonTabView();

    // Column 3: Object Selector and Editor (using new component)
    TableNextColumn();
    int current_room = current_room_id_;
    if (!active_rooms_.empty() && current_active_room_tab_ < active_rooms_.Size) {
      current_room = active_rooms_[current_active_room_tab_];
    }
    object_selector_.set_current_room_id(current_room);
    object_selector_.Draw();
    
    ImGui::EndTable();
  }
  return absl::OkStatus();
}

void DungeonEditor::OnRoomSelected(int room_id) {
  // Update current room ID
  current_room_id_ = room_id;
  
  // Check if room is already open in a tab
  int existing_tab_index = -1;
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      existing_tab_index = i;
      break;
    }
  }
  
  if (existing_tab_index >= 0) {
    // Room is already open, switch to that tab
    current_active_room_tab_ = existing_tab_index;
  } else {
    // Room is not open, add it as a new tab
    active_rooms_.push_back(room_id);
    current_active_room_tab_ = active_rooms_.Size - 1;
  }
  
  // Update the room selector's active rooms list
  room_selector_.set_active_rooms(active_rooms_);
}

void DungeonEditor::DrawToolset() {
  if (BeginTable("DWToolset", 16, ImGuiTableFlags_SizingFixedFit,
                 ImVec2(0, 0))) {
    static std::array<const char *, 16> tool_names = {
        "Undo", "Redo",      "Separator", "Any",    "BG1",  "BG2",
        "BG3",  "Separator", "Object",    "Sprite", "Item", "Entrance",
        "Door", "Chest",     "Block",     "Palette"};
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
  
  // Add layer selector below the main toolset
  ImGui::Separator();
  ImGui::Text("Layer:");
  ImGui::SameLine();
  if (ImGui::RadioButton("BG1", current_layer_ == 0)) {
    current_layer_ = 0;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("BG2", current_layer_ == 1)) {
    current_layer_ = 1;
  }
  ImGui::SameLine();
  if (ImGui::RadioButton("Both", current_layer_ == 2)) {
    current_layer_ = 2;
  }
  
  // Add interaction mode info
  ImGui::SameLine();
  ImGui::Text("| Click to place, Ctrl+Click to select");
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
        current_active_room_tab_ = n; // Track which tab is currently active
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
  ImGui::Separator();
}

void DungeonEditor::DrawDungeonCanvas(int room_id) {
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= rooms_.size()) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

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

  if (Button("Load Room Graphics")) {
    (void)LoadAndRenderRoomGraphics(room_id);
  }
  
  ImGui::SameLine();
  if (ImGui::Button("Reload All Graphics")) {
    (void)ReloadAllRoomGraphics();
  }

  // Debug and control popup
  static bool show_debug_popup = false;
  if (ImGui::Button("Room Debug Info")) {
    show_debug_popup = true;
  }

  if (show_debug_popup) {
    ImGui::OpenPopup("Room Debug Info");
    show_debug_popup = false;
  }

  if (ImGui::BeginPopupModal("Room Debug Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static bool show_objects = false;
    ImGui::Checkbox("Show Object Outlines", &show_objects);

    static bool render_objects = true;
    ImGui::Checkbox("Render Objects", &render_objects);

    static bool show_object_info = false;
    ImGui::Checkbox("Show Object Info", &show_object_info);

    static bool show_layout_objects = false;
    ImGui::Checkbox("Show Layout Objects", &show_layout_objects);

    if (ImGui::Button("Clear Object Cache")) {
      object_render_cache_.clear();
    }

    ImGui::SameLine();
    ImGui::Text("Cache: %zu objects", object_render_cache_.size());

    // Object statistics and metadata
    ImGui::Separator();
    ImGui::Text("Room Statistics:");
    ImGui::Text("Objects: %zu", rooms_[room_id].GetTileObjects().size());
    ImGui::Text("Layout Objects: %zu",
                rooms_[room_id].GetLayout().GetObjects().size());
    ImGui::Text("Sprites: %llu", static_cast<unsigned long long>(rooms_[room_id].GetSprites().size()));
    ImGui::Text("Chests: %zu", rooms_[room_id].GetChests().size());

    // Palette information
    ImGui::Text("Current Palette Group: %llu", static_cast<unsigned long long>(current_palette_group_id_));
    ImGui::Text("Palette Hash: %#016llx", last_palette_hash_);

    // Object type breakdown
    ImGui::Separator();
    ImGui::Text("Object Type Breakdown:");
    std::map<int, int> object_type_counts;
    for (const auto &obj : rooms_[room_id].GetTileObjects()) {
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

    // Object selection and editing
    static int selected_object_id = -1;
    if (ImGui::Button("Select Object")) {
      // This would open an object selection dialog
      // For now, just cycle through objects
      if (!rooms_[room_id].GetTileObjects().empty()) {
        selected_object_id =
            (selected_object_id + 1) % rooms_[room_id].GetTileObjects().size();
      }
    }

    if (selected_object_id >= 0 &&
        selected_object_id < (int)rooms_[room_id].GetTileObjects().size()) {
      const auto &selected_obj =
          rooms_[room_id].GetTileObjects()[selected_object_id];
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

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  ImGui::EndGroup();

  canvas_.DrawBackground();
  canvas_.DrawContextMenu();
  
  // Handle mouse input for drag and select functionality
  HandleCanvasMouseInput();
  
  if (is_loaded_) {
    // Automatically load room graphics if not already loaded
    if (rooms_[room_id].blocks().empty()) {
      (void)LoadAndRenderRoomGraphics(room_id);
    }
    
    // Load room objects if not already loaded
    if (rooms_[room_id].GetTileObjects().empty()) {
      rooms_[room_id].LoadObjects();
    }
    
    // Render background layers with proper positioning
    RenderRoomBackgroundLayers(room_id);
    
    // Render room objects on top of background using the room's palette
    if (current_palette_id_ < current_palette_group_.size()) {
      auto room_palette = current_palette_group_[current_palette_id_];
      for (const auto& object : rooms_[room_id].GetTileObjects()) {
        RenderObjectInCanvas(object, room_palette);
      }
    }
  }
  
  // Draw selection box and drag preview
  DrawSelectBox();
  DrawDragPreview();
  
  canvas_.DrawGrid();
  canvas_.DrawOverlay();
}

void DungeonEditor::RenderObjectInCanvas(const zelda3::RoomObject &object,
                                         const gfx::SnesPalette &palette) {
  // Validate ROM is loaded
  if (!rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Create a mutable copy of the object to ensure tiles are loaded
  auto mutable_object = object;
  mutable_object.set_rom(rom_);
  mutable_object.EnsureTilesLoaded();

  // Check if tiles were loaded successfully
  if (mutable_object.tiles().empty()) {
    return;  // Skip objects without tiles
  }

  // Calculate palette hash for caching
  uint64_t palette_hash = 0;
  for (size_t i = 0; i < palette.size() && i < 16; ++i) {
    palette_hash ^= std::hash<uint16_t>{}(palette[i].snes()) + 0x9e3779b9 +
                    (palette_hash << 6) + (palette_hash >> 2);
  }

  // Convert room coordinates to canvas coordinates using helper function
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);

  // Check if object is within canvas bounds (accounting for scrolling)
  if (!IsWithinCanvasBounds(canvas_x, canvas_y, 32)) {
    return;  // Skip objects outside visible area
  }

  // Check cache first
  for (auto &cached : object_render_cache_) {
    if (cached.object_id == object.id_ && cached.object_x == object.x_ &&
        cached.object_y == object.y_ && cached.object_size == object.size_ &&
        cached.palette_hash == palette_hash && cached.is_valid) {
      // Use cached bitmap - canvas handles scrolling internally
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

  // Draw the object bitmap to the canvas
  // Canvas will handle scrolling and coordinate transformation
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
    // Convert room coordinates to canvas coordinates using helper function
    auto [canvas_x, canvas_y] =
        RoomToCanvasCoordinates(layout_obj.x(), layout_obj.y());

    // Check if layout object is within canvas bounds
    if (!IsWithinCanvasBounds(canvas_x, canvas_y, 16)) {
      continue;  // Skip objects outside visible area
    }

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

// Coordinate conversion helper functions
std::pair<int, int> DungeonEditor::RoomToCanvasCoordinates(int room_x,
                                                           int room_y) const {
  // Convert room coordinates (16x16 tile units) to canvas coordinates (pixels)
  // Note: The canvas applies global_scale_ and scrolling internally, so we return
  // the base coordinates and let the canvas handle the transformation
  return {room_x * 16, room_y * 16};
}

std::pair<int, int> DungeonEditor::CanvasToRoomCoordinates(int canvas_x,
                                                           int canvas_y) const {
  // Convert canvas coordinates (pixels) to room coordinates (16x16 tile units)
  // Note: This assumes the canvas coordinates are already adjusted for scaling and scrolling
  return {canvas_x / 16, canvas_y / 16};
}

bool DungeonEditor::IsWithinCanvasBounds(int canvas_x, int canvas_y,
                                         int margin) const {
  // Check if coordinates are within canvas bounds with optional margin
  // Get the actual canvas size (accounting for scaling)
  auto canvas_size = canvas_.canvas_size();
  auto global_scale = canvas_.global_scale();
  int scaled_width = static_cast<int>(canvas_size.x * global_scale);
  int scaled_height = static_cast<int>(canvas_size.y * global_scale);
  
  return (canvas_x >= -margin && canvas_y >= -margin &&
          canvas_x <= scaled_width + margin &&
          canvas_y <= scaled_height + margin);
}

// Room graphics management methods
absl::Status DungeonEditor::LoadAndRenderRoomGraphics(int room_id) {
  if (room_id < 0 || room_id >= rooms_.size()) {
    return absl::InvalidArgumentError("Invalid room ID");
  }
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  auto& room = rooms_[room_id];
  
  // Load room graphics with proper blockset
  (void)room.LoadRoomGraphics(room.blockset);
  
  // Load the room's palette with bounds checking
  if (room.palette < rom()->paletteset_ids.size() && 
      !rom()->paletteset_ids[room.palette].empty()) {
    auto dungeon_palette_ptr = rom()->paletteset_ids[room.palette][0];
    auto palette_id = rom()->ReadWord(0xDEC4B + dungeon_palette_ptr);
    if (palette_id.ok()) {
      current_palette_group_id_ = palette_id.value() / 180;
      if (current_palette_group_id_ < rom()->palette_group().dungeon_main.size()) {
        full_palette_ = rom()->palette_group().dungeon_main[current_palette_group_id_];
        ASSIGN_OR_RETURN(current_palette_group_,
                         gfx::CreatePaletteGroupFromLargePalette(full_palette_));
      }
    }
  }
  
  // Render the room graphics to the graphics arena
  (void)room.RenderRoomGraphics();
  
  return absl::OkStatus();
}

absl::Status DungeonEditor::ReloadAllRoomGraphics() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  // Clear existing graphics cache
  object_render_cache_.clear();
  
  // Reload graphics for all rooms
  for (int i = 0; i < rooms_.size(); i++) {
    auto status = LoadAndRenderRoomGraphics(i);
    if (!status.ok()) {
      // Log error but continue with other rooms
      continue;
    }
  }
  
  return absl::OkStatus();
}

void DungeonEditor::RenderRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= rooms_.size()) {
    return;
  }
  
  auto& room = rooms_[room_id];
  
  // Get canvas dimensions
  int canvas_width = canvas_.width();
  int canvas_height = canvas_.height();
  
  // BG1 (background layer 1) - room graphics
  auto& bg1_bitmap = gfx::Arena::Get().bg1().bitmap();
  if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0 && bg1_bitmap.height() > 0) {
    // Scale the background to fit the canvas
    float scale_x = static_cast<float>(canvas_width) / bg1_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg1_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg1_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg1_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_.DrawBitmap(bg1_bitmap, offset_x, offset_y, scale, 255);
  }
  
  // BG2 (background layer 2) - sprite graphics (overlay)
  auto& bg2_bitmap = gfx::Arena::Get().bg2().bitmap();
  if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0 && bg2_bitmap.height() > 0) {
    // Scale the background to fit the canvas
    float scale_x = static_cast<float>(canvas_width) / bg2_bitmap.width();
    float scale_y = static_cast<float>(canvas_height) / bg2_bitmap.height();
    float scale = std::min(scale_x, scale_y);
    
    int scaled_width = static_cast<int>(bg2_bitmap.width() * scale);
    int scaled_height = static_cast<int>(bg2_bitmap.height() * scale);
    int offset_x = (canvas_width - scaled_width) / 2;
    int offset_y = (canvas_height - scaled_height) / 2;
    
    canvas_.DrawBitmap(bg2_bitmap, offset_x, offset_y, scale, 200); // Semi-transparent overlay
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

void DungeonEditor::DrawUsageStats() {
  if (ImGui::Button("Refresh")) {
    selected_blockset_ = 0xFFFF;
    selected_spriteset_ = 0xFFFF;
    selected_palette_ = 0xFFFF;
    spriteset_usage_.clear();
    blockset_usage_.clear();
    palette_usage_.clear();
    CalculateUsageStats();
  }
  
  ImGui::Text("Usage Statistics");
  ImGui::Separator();
  
  ImGui::Text("Blocksets: %zu used", blockset_usage_.size());
  ImGui::Text("Spritesets: %zu used", spriteset_usage_.size());
  ImGui::Text("Palettes: %zu used", palette_usage_.size());
}

// Drag and select box functionality
void DungeonEditor::HandleCanvasMouseInput() {
  const ImGuiIO& io = ImGui::GetIO();
  
  // Check if mouse is over the canvas
  if (!canvas_.IsMouseHovering()) {
    return;
  }
  
  // Get mouse position relative to canvas
  ImVec2 mouse_pos = io.MousePos;
  ImVec2 canvas_pos = canvas_.zero_point();
  ImVec2 canvas_size = canvas_.canvas_size();
  
  // Convert to canvas coordinates
  ImVec2 canvas_mouse_pos = ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
  
  // Handle mouse clicks
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl)) {
      // Start selection box
      is_selecting_ = true;
      select_start_pos_ = canvas_mouse_pos;
      select_current_pos_ = canvas_mouse_pos;
      selected_objects_.clear();
    } else {
      // Start dragging or place object
      if (object_loaded_) {
        // Convert canvas coordinates to room coordinates
        auto [room_x, room_y] = CanvasToRoomCoordinates(
          static_cast<int>(canvas_mouse_pos.x), 
          static_cast<int>(canvas_mouse_pos.y)
        );
        PlaceObjectAtPosition(room_x, room_y);
      } else {
        // Start dragging existing objects
        is_dragging_ = true;
        drag_start_pos_ = canvas_mouse_pos;
        drag_current_pos_ = canvas_mouse_pos;
      }
    }
  }
  
  // Handle mouse drag
  if (is_selecting_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    select_current_pos_ = canvas_mouse_pos;
    UpdateSelectedObjects();
  }
  
  if (is_dragging_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    drag_current_pos_ = canvas_mouse_pos;
    DrawDragPreview();
  }
  
  // Handle mouse release
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    if (is_selecting_) {
      is_selecting_ = false;
      UpdateSelectedObjects();
    }
    if (is_dragging_) {
      is_dragging_ = false;
      // TODO: Apply drag transformation to selected objects
    }
  }
}

void DungeonEditor::DrawSelectBox() {
  if (!is_selecting_) return;
  
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_.zero_point();
  
  // Calculate select box bounds
  ImVec2 start = ImVec2(
    canvas_pos.x + std::min(select_start_pos_.x, select_current_pos_.x),
    canvas_pos.y + std::min(select_start_pos_.y, select_current_pos_.y)
  );
  ImVec2 end = ImVec2(
    canvas_pos.x + std::max(select_start_pos_.x, select_current_pos_.x),
    canvas_pos.y + std::max(select_start_pos_.y, select_current_pos_.y)
  );
  
  // Draw selection box
  draw_list->AddRect(start, end, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
  draw_list->AddRectFilled(start, end, IM_COL32(255, 255, 0, 32));
}

void DungeonEditor::DrawDragPreview() {
  if (!is_dragging_) return;
  
  // Draw drag preview for selected objects
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 canvas_pos = canvas_.zero_point();
  ImVec2 drag_delta = ImVec2(
    drag_current_pos_.x - drag_start_pos_.x,
    drag_current_pos_.y - drag_start_pos_.y
  );
  
  // Draw preview of where objects would be moved
  for (int obj_id : selected_objects_) {
    // TODO: Draw preview of object at new position
    // This would require getting the object's current position and drawing it offset by drag_delta
  }
}

void DungeonEditor::UpdateSelectedObjects() {
  if (!is_selecting_) return;
  
  selected_objects_.clear();
  
  // Get current room
  int current_room = current_room_id_;
  if (!active_rooms_.empty() && current_active_room_tab_ < active_rooms_.Size) {
    current_room = active_rooms_[current_active_room_tab_];
  }
  
  if (current_room < 0 || current_room >= rooms_.size()) return;
  
  auto& room = rooms_[current_room];
  
  // Check each object in the room
  for (const auto& object : room.GetTileObjects()) {
    if (IsObjectInSelectBox(object)) {
      selected_objects_.push_back(object.id_);
    }
  }
}

bool DungeonEditor::IsObjectInSelectBox(const zelda3::RoomObject& object) const {
  if (!is_selecting_) return false;
  
  // Convert object position to canvas coordinates
  auto [canvas_x, canvas_y] = RoomToCanvasCoordinates(object.x_, object.y_);
  
  // Calculate select box bounds
  float min_x = std::min(select_start_pos_.x, select_current_pos_.x);
  float max_x = std::max(select_start_pos_.x, select_current_pos_.x);
  float min_y = std::min(select_start_pos_.y, select_current_pos_.y);
  float max_y = std::max(select_start_pos_.y, select_current_pos_.y);
  
  // Check if object is within select box
  return (canvas_x >= min_x && canvas_x <= max_x && 
          canvas_y >= min_y && canvas_y <= max_y);
}

void DungeonEditor::PlaceObjectAtPosition(int room_x, int room_y) {
  if (!object_loaded_) return;
  
  // Get current room
  int current_room = current_room_id_;
  if (!active_rooms_.empty() && current_active_room_tab_ < active_rooms_.Size) {
    current_room = active_rooms_[current_active_room_tab_];
  }
  
  if (current_room < 0 || current_room >= rooms_.size()) return;
  
  // Create new object at the specified position
  auto new_object = preview_object_;
  new_object.x_ = room_x;
  new_object.y_ = room_y;
  
  // Add object to room
  auto& room = rooms_[current_room];
  room.AddTileObject(new_object);
  
  // TODO: Update the room's object list and trigger a redraw
  // This would require integration with the room management system
}

}  // namespace yaze::editor
