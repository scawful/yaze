#include "dungeon_editor.h"

/**
 * @file dungeon_editor.cc
 * @deprecated This file is deprecated in favor of dungeon_editor_v2.cc
 * 
 * Migration notes:
 * ✅ ManualObjectRenderer - Migrated to V2
 * ✅ ProcessDeferredTextures() - Migrated to V2
 * ✅ Object interaction - Already in DungeonObjectInteraction component
 * ✅ Primitive rendering - Already in DungeonRenderer component
 * 
 * All critical features have been migrated. This file should be removed
 * once DungeonEditorV2 is confirmed working in production.
 */

#include "absl/strings/str_format.h"
#include "app/gfx/performance_profiler.h"
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
#include "app/zelda3/dungeon/room_visual_diagnostic.h"
#include "imgui/imgui.h"

namespace yaze::editor {

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
  }
  
  // Phase 5: Initialize integrated object editor
  if (rom_ && !object_editor_) {
    object_editor_ = std::make_unique<zelda3::DungeonObjectEditor>(rom_);
    
    // Configure editor for dungeon editing
    auto config = object_editor_->GetConfig();
    config.show_selection_highlight = enable_selection_highlight_;
    config.show_layer_colors = enable_layer_visualization_;
    config.show_property_panel = show_object_property_panel_;
    config.snap_to_grid = true;
    config.grid_size = 16;  // 16x16 tiles
    object_editor_->SetConfig(config);
  }
}

absl::Status DungeonEditor::Load() {
  gfx::ScopedTimer timer("DungeonEditor::Load");
  
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  
  auto dungeon_man_pal_group = rom()->palette_group().dungeon_main;

  // Use room loader component for loading rooms
  {
    gfx::ScopedTimer rooms_timer("DungeonEditor::LoadAllRooms");
    RETURN_IF_ERROR(room_loader_.LoadAllRooms(rooms_));
  }
  
  {
    gfx::ScopedTimer entrances_timer("DungeonEditor::LoadRoomEntrances");
    RETURN_IF_ERROR(room_loader_.LoadRoomEntrances(entrances_));
  }

  // Load the palette group and palette for the dungeon
  {
    gfx::ScopedTimer palette_timer("DungeonEditor::LoadPalettes");
    full_palette_ = dungeon_man_pal_group[current_palette_group_id_];
    ASSIGN_OR_RETURN(current_palette_group_,
                     gfx::CreatePaletteGroupFromLargePalette(full_palette_));
  }

  // Calculate usage statistics
  {
    gfx::ScopedTimer usage_timer("DungeonEditor::CalculateUsageStats");
    usage_tracker_.CalculateUsageStats(rooms_);
  }

  // Initialize the new editor system
  {
    gfx::ScopedTimer init_timer("DungeonEditor::InitializeSystem");
    if (dungeon_editor_system_) {
      auto status = dungeon_editor_system_->Initialize();
      if (!status.ok()) {
        return status;
      }
    }
  }

  // Initialize the new UI components with loaded data
  room_selector_.set_rom(rom_);
  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.set_room_selected_callback(
      [this](int room_id) { OnRoomSelected(room_id); });

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

  // Set up object selection callback
  object_selector_.SetObjectSelectedCallback(
      [this](const zelda3::RoomObject& object) {
        preview_object_ = object;
        object_loaded_ = true;
        toolset_.set_placement_type(DungeonToolset::kObject);
        object_interaction_.SetPreviewObject(object, true);
      });
  
  // Set up component callbacks
  object_interaction_.SetCurrentRoom(&rooms_, current_room_id_);
  
  // Set up toolset callbacks
  toolset_.SetUndoCallback([this]() { PRINT_IF_ERROR(Undo()); });
  toolset_.SetRedoCallback([this]() { PRINT_IF_ERROR(Redo()); });
  toolset_.SetPaletteToggleCallback([this]() { palette_showing_ = !palette_showing_; });

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditor::Update() {
  if (refresh_graphics_) {
    RETURN_IF_ERROR(RefreshGraphics());
    refresh_graphics_ = false;
  }

  status_ = UpdateDungeonRoomView();

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
  // Update graphics sheet textures via Arena's deferred texture queue
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin(), 8, [this](int block) {
        gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
            current_palette_group_[current_palette_id_], 0);
        // Queue texture update for the modified graphics sheet
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, 
            &gfx::Arena::Get().gfx_sheets()[block]);
      });

  auto sprites_aux1_pal_group = rom()->palette_group().sprites_aux1;
  std::for_each_n(
      rooms_[current_room_id_].blocks().begin() + 8, 8,
      [this, &sprites_aux1_pal_group](int block) {
        gfx::Arena::Get().gfx_sheets()[block].SetPaletteWithTransparent(
            sprites_aux1_pal_group[current_palette_id_], 0);
        // Queue texture update for the modified graphics sheet
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, 
            &gfx::Arena::Get().gfx_sheets()[block]);
      });
  return absl::OkStatus();
}

// LoadDungeonRoomSize moved to DungeonRoomLoader component

absl::Status DungeonEditor::UpdateDungeonRoomView() {
  toolset_.Draw();

  if (palette_showing_) {
    ImGui::Begin("Palette Editor", &palette_showing_, 0);
    auto dungeon_main_pal_group = rom()->palette_group().dungeon_main;
    current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
    gui::SelectablePalettePipeline(current_palette_id_, refresh_graphics_,
                                   current_palette_);
    ImGui::End();
  }

  // Correct 3-column layout as specified
  if (BeginTable("#DungeonEditTable", 3, kDungeonTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Room/Entrance Selector", ImGuiTableColumnFlags_WidthFixed,
                     250);
    TableSetupColumn("Canvas & Properties", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Object Selector/Editor", ImGuiTableColumnFlags_WidthFixed,
                     300);
    TableHeadersRow();
    TableNextRow();

    // Column 1: Room and Entrance Selector (unchanged)
    TableNextColumn();
    room_selector_.Draw();

    // Column 2: Canvas and room properties with tabs
    TableNextColumn();
    DrawCanvasAndPropertiesPanel();

    // Column 3: Object selector, room graphics, and object editor
    TableNextColumn();
    object_selector_.Draw();
    
    // Phase 5: Draw integrated object editor panels below object selector
    ImGui::Separator();
    DrawObjectEditorPanels();

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

// DrawToolset() method moved to DungeonToolset component

void DungeonEditor::DrawCanvasAndPropertiesPanel() {
  if (ImGui::BeginTabBar("CanvasPropertiesTabBar")) {
    // Canvas tab - main editing view
    if (ImGui::BeginTabItem("Canvas")) {
      DrawDungeonTabView();
      ImGui::EndTabItem();
    }

    // Visual Diagnostic tab - for debugging rendering
    if (ImGui::BeginTabItem("Visual Diagnostic")) {
      if (!active_rooms_.empty()) {
        int room_id = active_rooms_[current_active_room_tab_];
        auto& room = rooms_[room_id];
        
        // Show button to toggle diagnostic window
        if (ImGui::Button("Open Diagnostic Window")) {
          show_visual_diagnostic_ = true;
        }
        
        // Render visual diagnostic
        if (show_visual_diagnostic_) {
          // Get the global graphics buffer for tile decoding
          static std::vector<uint8_t> empty_gfx;
          const auto& gfx_buffer = rom()->graphics_buffer();
          
          // Get the actual palette being used by this room
          const auto& dungeon_pal_group = rom()->palette_group().dungeon_main;
          int room_palette_id = rooms_[room_id].palette;
          
          // Validate and clamp palette ID
          if (room_palette_id < 0 || room_palette_id >= static_cast<int>(dungeon_pal_group.size())) {
            room_palette_id = 0;
          }
          
          auto room_palette = dungeon_pal_group[room_palette_id];
          
          zelda3::dungeon::RoomVisualDiagnostic::DrawDiagnosticWindow(
              &show_visual_diagnostic_,
              gfx::Arena::Get().bg1(),
              gfx::Arena::Get().bg2(),
              room_palette,
              gfx_buffer.empty() ? empty_gfx : gfx_buffer);
        }
      } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No room selected. Open a room to see diagnostics.");
      }
      ImGui::EndTabItem();
    }

    // Room Properties tab - debug and editing controls
    if (ImGui::BeginTabItem("Room Properties")) {
      if (ImGui::Button("Room Debug Info")) {
        ImGui::OpenPopup("RoomDebugPopup");
      }

      // Room properties popup
      if (ImGui::BeginPopup("RoomDebugPopup")) {
        DrawRoomPropertiesDebugPopup();
        ImGui::EndPopup();
      }

      // Quick room info display
      int current_room = current_room_id_;
      if (!active_rooms_.empty() &&
          current_active_room_tab_ < active_rooms_.Size) {
        current_room = active_rooms_[current_active_room_tab_];
      }

      if (current_room >= 0 && current_room < rooms_.size()) {
        auto& room = rooms_[current_room];

        ImGui::Text("Current Room: %03X (%d)", current_room, current_room);
        ImGui::Text("Objects: %zu", room.GetTileObjects().size());
        ImGui::Text("Sprites: %zu", room.GetSprites().size());
        ImGui::Text("Chests: %zu", room.GetChests().size());

        // Selection info
        const auto& selected_indices = object_interaction_.GetSelectedObjectIndices();
        if (!selected_indices.empty()) {
          ImGui::Separator();
          ImGui::Text("Selected Objects: %zu", selected_indices.size());
          if (ImGui::Button("Clear Selection")) {
            object_interaction_.ClearSelection();
          }
        }

        ImGui::Separator();

        // Quick edit controls
        gui::InputHexByte("Layout", &room.layout);
        gui::InputHexByte("Blockset", &room.blockset);
        gui::InputHexByte("Spriteset", &room.spriteset);
        gui::InputHexByte("Palette", &room.palette);

        if (ImGui::Button("Reload Room Graphics")) {
          (void)LoadAndRenderRoomGraphics(room);
        }
      }

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void DungeonEditor::DrawRoomPropertiesDebugPopup() {
  int current_room = current_room_id_;
  if (!active_rooms_.empty() && current_active_room_tab_ < active_rooms_.Size) {
    current_room = active_rooms_[current_active_room_tab_];
  }

  if (current_room < 0 || current_room >= rooms_.size()) {
    ImGui::Text("Invalid room");
    return;
  }

  auto& room = rooms_[current_room];

  ImGui::Text("Room %03X Debug Information", current_room);
  ImGui::Separator();

  // Room properties table
  if (ImGui::BeginTable("RoomPropertiesPopup", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 120);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Room ID");
    ImGui::TableNextColumn();
    ImGui::Text("%03X (%d)", current_room, current_room);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Layout");
    ImGui::TableNextColumn();
    gui::InputHexByte("##layout", &room.layout);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Blockset");
    ImGui::TableNextColumn();
    gui::InputHexByte("##blockset", &room.blockset);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Spriteset");
    ImGui::TableNextColumn();
    gui::InputHexByte("##spriteset", &room.spriteset);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Palette");
    ImGui::TableNextColumn();
    gui::InputHexByte("##palette", &room.palette);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Floor 1");
    ImGui::TableNextColumn();
    gui::InputHexByte("##floor1", &room.floor1);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Floor 2");
    ImGui::TableNextColumn();
    gui::InputHexByte("##floor2", &room.floor2);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::Text("Message ID");
    ImGui::TableNextColumn();
    gui::InputHexWord("##message_id", &room.message_id_);

    ImGui::EndTable();
  }

  ImGui::Separator();

  // Object statistics
  ImGui::Text("Object Statistics:");
  ImGui::Text("Total Objects: %zu", room.GetTileObjects().size());
  ImGui::Text("Layout Objects: %zu", room.GetLayout().GetObjects().size());
  ImGui::Text("Sprites: %zu", room.GetSprites().size());
  ImGui::Text("Chests: %zu", room.GetChests().size());

  ImGui::Separator();

  if (ImGui::Button("Reload Objects")) {
    room.LoadObjects();
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    ImGui::CloseCurrentPopup();
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
        current_active_room_tab_ = n;  // Track which tab is currently active
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
    (void)LoadAndRenderRoomGraphics(rooms_[room_id]);
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

  if (ImGui::BeginPopupModal("Room Debug Info", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    static bool show_objects = false;
    ImGui::Checkbox("Show Object Outlines", &show_objects);

    static bool render_objects = true;
    ImGui::Checkbox("Render Objects", &render_objects);

    static bool show_object_info = false;
    ImGui::Checkbox("Show Object Info", &show_object_info);

    static bool show_layout_objects = false;
    ImGui::Checkbox("Show Layout Objects", &show_layout_objects);
    // Object statistics and metadata
    ImGui::Separator();
    ImGui::Text("Room Statistics:");
    ImGui::Text("Objects: %zu", rooms_[room_id].GetTileObjects().size());
    ImGui::Text("Layout Objects: %zu",
                rooms_[room_id].GetLayout().GetObjects().size());
    ImGui::Text("Sprites: %llu", static_cast<unsigned long long>(
                                     rooms_[room_id].GetSprites().size()));
    ImGui::Text("Chests: %zu", rooms_[room_id].GetChests().size());

    // Palette information
    ImGui::Text("Current Palette Group: %llu",
                static_cast<unsigned long long>(current_palette_group_id_));

    // Object type breakdown
    ImGui::Separator();
    ImGui::Text("Object Type Breakdown:");
    std::map<int, int> object_type_counts;
    for (const auto& obj : rooms_[room_id].GetTileObjects()) {
      object_type_counts[obj.id_]++;
    }
    for (const auto& [type, count] : object_type_counts) {
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
      const auto& selected_obj =
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

  // Handle object selection and placement using component
  object_interaction_.CheckForObjectSelection();

  // Handle mouse input for drag and select functionality  
  object_interaction_.HandleCanvasMouseInput();

  // Update preview object position based on mouse cursor
  if (object_loaded_ && preview_object_.id_ >= 0 && canvas_.IsMouseHovering()) {
    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse_pos = io.MousePos;
    ImVec2 canvas_pos = canvas_.zero_point();
    ImVec2 canvas_mouse_pos =
        ImVec2(mouse_pos.x - canvas_pos.x, mouse_pos.y - canvas_pos.y);
    auto [room_x, room_y] =
         object_interaction_.CanvasToRoomCoordinates(static_cast<int>(canvas_mouse_pos.x),
                                                     static_cast<int>(canvas_mouse_pos.y));
    preview_object_.x_ = room_x;
    preview_object_.y_ = room_y;
  }

  if (is_loaded_) {
    // Automatically load room graphics if not already loaded
    if (rooms_[room_id].blocks().empty()) {
      (void)LoadAndRenderRoomGraphics(rooms_[room_id]);
    }

    // Load room objects if not already loaded
    if (rooms_[room_id].GetTileObjects().empty()) {
      rooms_[room_id].LoadObjects();
    }

    // Render background layers with proper positioning
    // This uses per-room buffers which already include objects drawn by ObjectDrawer
    auto& room = rooms_[room_id];
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    auto& bg2_bitmap = room.bg2_buffer().bitmap();
    
    if (bg1_bitmap.is_active() && bg1_bitmap.width() > 0) {
      if (!bg1_bitmap.texture()) {
        // Queue texture creation for background layer 1
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &bg1_bitmap);
      }
      canvas_.DrawBitmap(bg1_bitmap, 0, 0, 1.0f, 255);
    }
    
    if (bg2_bitmap.is_active() && bg2_bitmap.width() > 0) {
      if (!bg2_bitmap.texture()) {
        // Queue texture creation for background layer 2
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &bg2_bitmap);
      }
      canvas_.DrawBitmap(bg2_bitmap, 0, 0, 1.0f, 200);
    }
    
  }

  // Phase 5: Render with integrated object editor
  RenderRoomWithObjects(room_id);
  
  // Draw selection box and drag preview using component
  object_interaction_.DrawSelectBox();
  object_interaction_.DrawDragPreview();

  canvas_.DrawGrid();
  canvas_.DrawOverlay();
  
  // Process queued texture commands
  ProcessDeferredTextures();
}

// ============================================================================
// Phase 5: Integrated Object Editor Methods
// ============================================================================

void DungeonEditor::UpdateObjectEditor() {
  if (!object_editor_ || !rom_ || !rom_->is_loaded()) {
    return;
  }
  
  // Get current room ID
  int room_id = current_room_id_;
  if (!active_rooms_.empty() && current_active_room_tab_ < active_rooms_.Size) {
    room_id = active_rooms_[current_active_room_tab_];
  }
  
  if (room_id < 0 || room_id >= rooms_.size()) {
    return;
  }
  
  // Ensure room graphics and objects are loaded
  auto& room = rooms_[room_id];
  
  // Load room graphics if not already loaded (this populates arena buffers)
  if (room.blocks().empty()) {
    auto status = LoadAndRenderRoomGraphics(room);
    if (!status.ok()) {
      // Log error but continue
      return;
    }
  }
  
  // Load room objects if not already loaded
  if (room.GetTileObjects().empty()) {
    room.LoadObjects();
  }
  
  // Sync object editor with current room's objects
  // The object editor should work with the room's tile_objects_ directly
  // rather than maintaining its own copy
}

void DungeonEditor::RenderRoomWithObjects(int room_id) {
  if (!object_editor_ || room_id < 0 || room_id >= rooms_.size()) {
    return;
  }
  
  // Ensure room graphics are loaded and rendered to arena buffers first
  if (rooms_[room_id].blocks().empty()) {
    // Room graphics not loaded yet, will be loaded by LoadAndRenderRoomGraphics
    return;
  }
  
  // Get the arena buffers for rendering - these should already be populated
  // by Room::RenderRoomGraphics() which was called in LoadAndRenderRoomGraphics
  auto& bg1_bitmap = gfx::Arena::Get().bg1().bitmap();
  auto& bg2_bitmap = gfx::Arena::Get().bg2().bitmap();
  
  if (!bg1_bitmap.is_active() || !bg2_bitmap.is_active()) {
    // Arena bitmaps not initialized, this means RenderRoomGraphics wasn't called
    return;
  }
  
  // Render layer visualization if enabled (draws on top of existing bitmap)
  if (enable_layer_visualization_) {
    object_editor_->RenderLayerVisualization(bg1_bitmap);
  }
  
  // Render selection highlights if enabled (draws on top of existing bitmap)
  if (enable_selection_highlight_) {
    object_editor_->RenderSelectionHighlight(bg1_bitmap);
  }
}

void DungeonEditor::DrawObjectEditorPanels() {
  if (!object_editor_) {
    return;
  }
  
  // Update editor state
  UpdateObjectEditor();
  
  // Render ImGui panels
  if (show_object_property_panel_) {
    object_editor_->RenderObjectPropertyPanel();
  }
  
  if (show_layer_controls_) {
    object_editor_->RenderLayerControls();
  }
}

// Legacy method implementations that delegate to components
absl::Status DungeonEditor::LoadAndRenderRoomGraphics(zelda3::Room& room) {
  return room_loader_.LoadAndRenderRoomGraphics(room);
}

absl::Status DungeonEditor::ReloadAllRoomGraphics() {
  return room_loader_.ReloadAllRoomGraphics(rooms_);
}

absl::Status DungeonEditor::UpdateRoomBackgroundLayers(int /*room_id*/) {
  // This method is deprecated - rendering is handled by DungeonRenderer component
  return absl::OkStatus();
}

void DungeonEditor::ProcessDeferredTextures() {
  // Process queued texture commands via Arena's deferred system
  // Note: Arena uses its stored renderer reference (initialized in EditorManager)
  // The parameter is ignored, but we pass nullptr to indicate we're using the stored renderer
  gfx::Arena::Get().ProcessTextureQueue(nullptr);
  
  // NOTE: This is deprecated - use DungeonEditorV2 instead
}

}  // namespace yaze::editor
