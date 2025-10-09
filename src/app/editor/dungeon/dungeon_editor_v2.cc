#include "dungeon_editor_v2.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/arena.h"
#include "app/gfx/snes_palette.h"
#include "app/zelda3/dungeon/room.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "imgui/imgui.h"
#include "util/log.h"

namespace yaze::editor {

// No table layout needed - all cards are independent

void DungeonEditorV2::Initialize(gfx::IRenderer* renderer, Rom* rom) {
  renderer_ = renderer;
  rom_ = rom;
  // Don't initialize emulator preview yet - ROM might not be loaded
  // Will be initialized in Load() instead
  
  // Setup docking class for room windows
  room_window_class_.ClassId = ImGui::GetID("DungeonRoomClass");
  room_window_class_.DockingAllowUnclassed = false;  // Room windows dock together
  
  // Register all cards with the card manager (done once during initialization)
  auto& card_manager = gui::EditorCardManager::Get();
  
  card_manager.RegisterCard({
      .card_id = "dungeon.control_panel",
      .display_name = "Dungeon Controls",
      .icon = ICON_MD_CASTLE,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+D",
      .visibility_flag = &show_control_panel_,
      .priority = 10
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.room_selector",
      .display_name = "Room Selector",
      .icon = ICON_MD_LIST,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+R",
      .visibility_flag = &show_room_selector_,
      .priority = 20
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.room_matrix",
      .display_name = "Room Matrix",
      .icon = ICON_MD_GRID_VIEW,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+M",
      .visibility_flag = &show_room_matrix_,
      .priority = 30
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.entrances",
      .display_name = "Entrances",
      .icon = ICON_MD_DOOR_FRONT,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+E",
      .visibility_flag = &show_entrances_list_,
      .priority = 40
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.room_graphics",
      .display_name = "Room Graphics",
      .icon = ICON_MD_IMAGE,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+G",
      .visibility_flag = &show_room_graphics_,
      .priority = 50
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.object_editor",
      .display_name = "Object Editor",
      .icon = ICON_MD_CONSTRUCTION,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+O",
      .visibility_flag = &show_object_editor_,
      .priority = 60
  });
  
  card_manager.RegisterCard({
      .card_id = "dungeon.palette_editor",
      .display_name = "Palette Editor",
      .icon = ICON_MD_PALETTE,
      .category = "Dungeon",
      .shortcut_hint = "Ctrl+Shift+P",
      .visibility_flag = &show_palette_editor_,
      .priority = 70
  });
}

void DungeonEditorV2::Initialize() {}

absl::Status DungeonEditorV2::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Load all rooms using the loader component - DEFERRED for lazy loading
  // RETURN_IF_ERROR(room_loader_.LoadAllRooms(rooms_));
  RETURN_IF_ERROR(room_loader_.LoadRoomEntrances(entrances_));

  // Load palette group
  auto dungeon_main_pal_group = rom_->palette_group().dungeon_main;
  current_palette_ = dungeon_main_pal_group[current_palette_group_id_];
  ASSIGN_OR_RETURN(current_palette_group_,
                   gfx::CreatePaletteGroupFromLargePalette(current_palette_));

  // Initialize components with loaded data
  room_selector_.set_rooms(&rooms_);
  room_selector_.set_entrances(&entrances_);
  room_selector_.set_active_rooms(active_rooms_);
  room_selector_.set_room_selected_callback(
      [this](int room_id) { OnRoomSelected(room_id); });

  canvas_viewer_.SetRooms(&rooms_);
  canvas_viewer_.SetCurrentPaletteGroup(current_palette_group_);
  canvas_viewer_.SetCurrentPaletteId(current_palette_id_);

  object_selector_.SetCurrentPaletteGroup(current_palette_group_);
  object_selector_.SetCurrentPaletteId(current_palette_id_);
  object_selector_.set_rooms(&rooms_);

  // NOW initialize emulator preview with loaded ROM
  object_emulator_preview_.Initialize(renderer_, rom_);
  
  // Initialize palette editor with loaded ROM
  palette_editor_.Initialize(rom_);
  
  // Initialize unified object editor card
  object_editor_card_ = std::make_unique<ObjectEditorCard>(renderer_, rom_, &canvas_viewer_);
  
  // Wire palette changes to trigger room re-renders
  palette_editor_.SetOnPaletteChanged([this](int /*palette_id*/) {
    // Re-render all active rooms when palette changes
    for (int i = 0; i < active_rooms_.Size; i++) {
      int room_id = active_rooms_[i];
      if (room_id >= 0 && room_id < (int)rooms_.size()) {
        rooms_[room_id].RenderRoomGraphics();
      }
    }
  });

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Update() {
  if (!is_loaded_) {
    // CARD-BASED EDITOR: Create a minimal loading card
    gui::EditorCard loading_card("Dungeon Editor Loading", ICON_MD_CASTLE);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loading dungeon data...");
      ImGui::TextWrapped("Independent editor cards will appear once ROM data is loaded.");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  // CARD-BASED EDITOR: All windows are independent top-level cards
  // No parent wrapper - this allows closing control panel without affecting rooms
  
  // Optional control panel (can be hidden/minimized)
  if (show_control_panel_) {
    DrawControlPanel();
  } else if (control_panel_minimized_) {
    // Draw floating icon button to reopen
    ImGui::SetNextWindowPos(ImVec2(10, 100));
    ImGui::SetNextWindowSize(ImVec2(50, 50));
    ImGuiWindowFlags icon_flags = ImGuiWindowFlags_NoTitleBar | 
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoDocking;
    
    if (ImGui::Begin("##DungeonControlIcon", nullptr, icon_flags)) {
      if (ImGui::Button(ICON_MD_CASTLE, ImVec2(40, 40))) {
        show_control_panel_ = true;
        control_panel_minimized_ = false;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open Dungeon Controls");
      }
    }
    ImGui::End();
  }

  // Render all independent cards (these are ALL top-level windows now)
  DrawLayout();
  
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Save() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  // Save all rooms (SaveObjects will handle which ones need saving)
  for (auto& room : rooms_) {
    auto status = room.SaveObjects();
    if (!status.ok()) {
      // Log error but continue with other rooms
      LOG_ERROR("DungeonEditorV2", "Failed to save room: %s", status.message().data());
    }
  }

  return absl::OkStatus();
}

void DungeonEditorV2::DrawToolset() {
  static gui::Toolset toolbar;
  toolbar.Begin();

  if (toolbar.AddAction(ICON_MD_ADD, "Open Room")) {
    OnRoomSelected(room_selector_.current_room_id());
  }
  
  toolbar.AddSeparator();
  
  if (toolbar.AddToggle(ICON_MD_LIST, &show_room_selector_, "Toggle Room Selector")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_GRID_VIEW, &show_room_matrix_, "Toggle Room Matrix")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_DOOR_FRONT, &show_entrances_list_, "Toggle Entrances List")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_IMAGE, &show_room_graphics_, "Toggle Room Graphics")) {
    // Toggled
  }
  
  toolbar.AddSeparator();
  
  if (toolbar.AddToggle(ICON_MD_CONSTRUCTION, &show_object_editor_, "Toggle Object Editor")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_PALETTE, &show_palette_editor_, "Toggle Palette Editor")) {
    // Toggled
  }

  toolbar.End();
}

void DungeonEditorV2::DrawControlPanel() {
  // Small, collapsible control panel for dungeon editor
  ImGui::SetNextWindowSize(ImVec2(280, 280), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);
  
  ImGuiWindowFlags flags = ImGuiWindowFlags_None;
  
  if (ImGui::Begin(ICON_MD_CASTLE " Dungeon Controls", &show_control_panel_, flags)) {
    ImGui::TextWrapped("Welcome to Dungeon Editor V2!");
    ImGui::TextDisabled("Use checkboxes below to open cards");
    ImGui::Separator();
    
    DrawToolset();
    
    ImGui::Separator();
    ImGui::Text("Quick Toggles:");
    
    // Checkbox grid for quick toggles
    if (ImGui::BeginTable("##QuickToggles", 2, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Rooms", &show_room_selector_);
      
      ImGui::TableNextColumn();
      ImGui::Checkbox("Matrix", &show_room_matrix_);
      
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Entrances", &show_entrances_list_);
      
      ImGui::TableNextColumn();
      ImGui::Checkbox("Graphics", &show_room_graphics_);
      
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("Objects", &show_object_editor_);
      
      ImGui::TableNextColumn();
      ImGui::Checkbox("Palette", &show_palette_editor_);
      
      ImGui::EndTable();
    }
    
    ImGui::Separator();
    
    // Minimize button
    if (ImGui::SmallButton(ICON_MD_MINIMIZE " Minimize to Icon")) {
      control_panel_minimized_ = true;
      show_control_panel_ = false;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Collapse to floating icon. Rooms stay open.");
    }
  }
  ImGui::End();
}

void DungeonEditorV2::DrawLayout() {
  // NO TABLE LAYOUT - All independent dockable EditorCards
  // All cards check their visibility flags and can be closed with X button
  
  // 1. Room Selector Card (independent, dockable)
  if (show_room_selector_) {
    DrawRoomsListCard();
    // Card handles its own closing via &show_room_selector_ in constructor
  }
  
  // 2. Room Matrix Card (visual navigation)
  if (show_room_matrix_) {
    DrawRoomMatrixCard();
    // Card handles its own closing via &show_room_matrix_ in constructor
  }
  
  // 3. Entrances List Card
  if (show_entrances_list_) {
    DrawEntrancesListCard();
    // Card handles its own closing via &show_entrances_list_ in constructor
  }
  
  // 4. Room Graphics Card
  if (show_room_graphics_) {
    DrawRoomGraphicsCard();
    // Card handles its own closing via &show_room_graphics_ in constructor
  }

  // 5. Unified Object Editor Card
  if (show_object_editor_ && object_editor_card_) {
    object_editor_card_->Draw(&show_object_editor_);
    // ObjectEditorCard handles closing via p_open parameter
  }
  
  // 6. Palette Editor Card (independent, dockable)
  if (show_palette_editor_) {
    gui::EditorCard palette_card(
        MakeCardTitle("Palette Editor").c_str(), 
        ICON_MD_PALETTE, &show_palette_editor_);
    if (palette_card.Begin()) {
      palette_editor_.Draw();
    }
    palette_card.End();
    // Card handles its own closing via &show_palette_editor_ in constructor
  }

  // 6. Active Room Cards (independent, dockable, tracked for jump-to)
  for (int i = 0; i < active_rooms_.Size; i++) {
    int room_id = active_rooms_[i];
    bool open = true;

    // Create session-aware card title
    std::string base_name;
    if (room_id >= 0 && static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
      base_name = absl::StrFormat("%s", zelda3::kRoomNames[room_id].data());
    } else {
      base_name = absl::StrFormat("Room %03X", room_id);
    }
    
    std::string card_name_str = absl::StrFormat("%s###RoomCard%d", 
                                                MakeCardTitle(base_name).c_str(), room_id);

    // Track or create card for jump-to functionality
    if (room_cards_.find(room_id) == room_cards_.end()) {
      room_cards_[room_id] = std::make_shared<gui::EditorCard>(
          card_name_str.c_str(), ICON_MD_GRID_ON, &open);
      room_cards_[room_id]->SetDefaultSize(700, 600);
      
      // Set default position for first room to be docked with main window
      if (active_rooms_.Size == 1) {
        room_cards_[room_id]->SetPosition(gui::EditorCard::Position::Floating);
      }
    }
    
    auto& room_card = room_cards_[room_id];
    
    // CRITICAL: Use docking class BEFORE Begin() to make rooms dock together
    // This creates a separate docking space for all room cards
    ImGui::SetNextWindowClass(&room_window_class_);
    
    // Make room cards fully dockable and independent
    if (room_card->Begin(&open)) {
      DrawRoomTab(room_id);
    }
    room_card->End();

    if (!open) {
      room_cards_.erase(room_id);
      active_rooms_.erase(active_rooms_.Data + i);
      i--;
    }
  }
}

void DungeonEditorV2::DrawRoomTab(int room_id) {
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  auto& room = rooms_[room_id];

  // Lazy load room data
  if (!room.IsLoaded()) {
    auto status = room_loader_.LoadRoom(room_id, room);
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load room: %s",
                       status.message().data());
      return;
    }
  }

  // Initialize room graphics and objects if not already done
  // This ensures objects are drawn to background buffers before canvas displays them
  if (room.IsLoaded()) {
    // Load room graphics (populates blocks, gfx sheets)
    if (room.blocks().empty()) {
      room.RenderRoomGraphics();
    }
    
    // Load room objects (populates tile_objects_)
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    
    // Render objects to background buffers (CRITICAL: this must happen before canvas drawing)
    // This uses ObjectDrawer to draw all objects into bg1_buffer_ and bg2_buffer_
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    if (!bg1_bitmap.is_active() || bg1_bitmap.width() == 0) {
      room.RenderObjectsToBackground();
    }
  }

  // Room info header
  ImGui::Text("Room %03X", room_id);
  ImGui::SameLine();
  if (room.IsLoaded()) {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), ICON_MD_CHECK " Loaded");
  } else {
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.4f, 1.0f), ICON_MD_PENDING " Not Loaded");
  }

  ImGui::Separator();

  // Canvas - fully delegated to DungeonCanvasViewer
  // DungeonCanvasViewer has DrawDungeonCanvas() method
  canvas_viewer_.DrawDungeonCanvas(room_id);
}

void DungeonEditorV2::OnRoomSelected(int room_id) {
  current_room_id_ = room_id;

  // Check if already open
  for (int i = 0; i < active_rooms_.Size; i++) {
    if (active_rooms_[i] == room_id) {
      // Focus the existing room card
      FocusRoom(room_id);
      return;
    }
  }

  // Add new room to be opened as a card
  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);
}

void DungeonEditorV2::OnEntranceSelected(int entrance_id) {
  if (entrance_id < 0 || entrance_id >= static_cast<int>(entrances_.size())) {
    return;
  }
  
  // Get the room ID associated with this entrance
  int room_id = entrances_[entrance_id].room_;
  
  // Open and focus the room
  OnRoomSelected(room_id);
}

void DungeonEditorV2::add_room(int room_id) {
  OnRoomSelected(room_id);
}

void DungeonEditorV2::FocusRoom(int room_id) {
  // Focus the room card if it exists
  auto it = room_cards_.find(room_id);
  if (it != room_cards_.end()) {
    it->second->Focus();
  }
}

void DungeonEditorV2::DrawRoomsListCard() {
  gui::EditorCard selector_card(
      MakeCardTitle("Rooms List").c_str(), 
      ICON_MD_LIST, &show_room_selector_);
  
  selector_card.SetDefaultSize(350, 600);
  
  if (selector_card.Begin()) {
    if (!rom_ || !rom_->is_loaded()) {
      ImGui::Text("ROM not loaded");
      selector_card.End();
      return;
    }
    
    // Add text filter
    static char room_filter[256] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##RoomFilter", ICON_MD_SEARCH " Filter rooms...", 
                                 room_filter, sizeof(room_filter))) {
      // Filter updated
    }
    
    ImGui::Separator();
    
    // Scrollable room list - simple and reliable
    if (ImGui::BeginChild("##RoomsList", ImVec2(0, 0), true,
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      std::string filter_str = room_filter;
      std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);
      
      for (int i = 0; i < 0x128; i++) {
        // Get room name
        std::string room_name;
        if (i < static_cast<int>(std::size(zelda3::kRoomNames))) {
          room_name = std::string(zelda3::kRoomNames[i]);
        } else {
          room_name = absl::StrFormat("Room %03X", i);
        }
        
        // Apply filter
        if (!filter_str.empty()) {
          std::string name_lower = room_name;
          std::transform(name_lower.begin(), name_lower.end(), 
                        name_lower.begin(), ::tolower);
          if (name_lower.find(filter_str) == std::string::npos) {
            continue;
          }
        }
        
        // Simple selectable with room ID and name
        std::string label = absl::StrFormat("[%03X] %s", i, room_name.c_str());
        bool is_selected = (current_room_id_ == i);
        
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          OnRoomSelected(i);
        }
        
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
          OnRoomSelected(i);
        }
      }
      ImGui::EndChild();
    }
  }
  selector_card.End();
}

void DungeonEditorV2::DrawEntrancesListCard() {
  gui::EditorCard entrances_card(
      MakeCardTitle("Entrances").c_str(), 
      ICON_MD_DOOR_FRONT, &show_entrances_list_);
  
  entrances_card.SetDefaultSize(400, 700);
  
  if (entrances_card.Begin()) {
    if (!rom_ || !rom_->is_loaded()) {
      ImGui::Text("ROM not loaded");
      entrances_card.End();
      return;
    }
    
    // Full entrance configuration UI (matching dungeon_room_selector layout)
    auto& current_entrance = entrances_[current_entrance_id_];
    
    gui::InputHexWord("Entrance ID", &current_entrance.entrance_id_);
    gui::InputHexWord("Room ID", reinterpret_cast<uint16_t*>(&current_entrance.room_));
    ImGui::SameLine();
    gui::InputHexByte("Dungeon ID", &current_entrance.dungeon_id_, 50.f, true);
    
    gui::InputHexByte("Blockset", &current_entrance.blockset_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("Music", &current_entrance.music_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("Floor", &current_entrance.floor_);
    
    ImGui::Separator();
    
    gui::InputHexWord("Player X   ", &current_entrance.x_position_);
    ImGui::SameLine();
    gui::InputHexWord("Player Y   ", &current_entrance.y_position_);
    
    gui::InputHexWord("Camera X", &current_entrance.camera_trigger_x_);
    ImGui::SameLine();
    gui::InputHexWord("Camera Y", &current_entrance.camera_trigger_y_);
    
    gui::InputHexWord("Scroll X    ", &current_entrance.camera_x_);
    ImGui::SameLine();
    gui::InputHexWord("Scroll Y    ", &current_entrance.camera_y_);
    
    gui::InputHexWord("Exit", reinterpret_cast<uint16_t*>(&current_entrance.exit_), 50.f, true);
    
    ImGui::Separator();
    ImGui::Text("Camera Boundaries");
    ImGui::Separator();
    ImGui::Text("\t\t\t\t\tNorth         East         South         West");
    
    gui::InputHexByte("Quadrant", &current_entrance.camera_boundary_qn_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##QE", &current_entrance.camera_boundary_qe_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##QS", &current_entrance.camera_boundary_qs_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##QW", &current_entrance.camera_boundary_qw_, 50.f, true);
    
    gui::InputHexByte("Full room", &current_entrance.camera_boundary_fn_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##FE", &current_entrance.camera_boundary_fe_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##FS", &current_entrance.camera_boundary_fs_, 50.f, true);
    ImGui::SameLine();
    gui::InputHexByte("##FW", &current_entrance.camera_boundary_fw_, 50.f, true);
    
    ImGui::Separator();
    
    // Entrance list - simple and reliable
    if (ImGui::BeginChild("##EntrancesList", ImVec2(0, 0), true, 
                          ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
      for (int i = 0; i < 0x8C; i++) {
        // The last seven are spawn points
        std::string entrance_name;
        if (i < 0x85) {
          entrance_name = std::string(zelda3::kEntranceNames[i]);
        } else {
          entrance_name = absl::StrFormat("Spawn Point %d", i - 0x85);
        }
        
        // Get associated room name
        int room_id = entrances_[i].room_;
        std::string room_name = "Unknown";
        if (room_id >= 0 && room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
          room_name = std::string(zelda3::kRoomNames[room_id]);
        }
        
        std::string label = absl::StrFormat("[%02X] %s -> %s", 
                                            i, entrance_name.c_str(), room_name.c_str());
        
        bool is_selected = (current_entrance_id_ == i);
        if (ImGui::Selectable(label.c_str(), is_selected)) {
          current_entrance_id_ = i;
          OnEntranceSelected(i);
        }
      }
      ImGui::EndChild();
    }
  }
  entrances_card.End();
}

void DungeonEditorV2::DrawRoomMatrixCard() {
  gui::EditorCard matrix_card(
      MakeCardTitle("Room Matrix").c_str(), 
      ICON_MD_GRID_VIEW, &show_room_matrix_);
  
  matrix_card.SetDefaultSize(440, 520);
  
  if (matrix_card.Begin()) {
    // 16 wide x 19 tall = 304 cells (296 rooms + 8 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;  // 296 rooms (0x00-0x127)
    constexpr float kRoomCellSize = 24.0f;  // Smaller cells like ZScream
    constexpr float kCellSpacing = 1.0f;   // Tighter spacing
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    
    int room_index = 0;
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = room_index;
        bool is_valid_room = (room_id < kTotalRooms);
        
        ImVec2 cell_min = ImVec2(
            canvas_pos.x + col * (kRoomCellSize + kCellSpacing), 
            canvas_pos.y + row * (kRoomCellSize + kCellSpacing));
        ImVec2 cell_max = ImVec2(
            cell_min.x + kRoomCellSize, 
            cell_min.y + kRoomCellSize);
        
        if (is_valid_room) {
          // Use simple deterministic color based on room ID (no loading needed)
          ImU32 bg_color;
          
          // Generate color from room ID - much faster than loading
          int hue = (room_id * 37) % 360;  // Distribute colors across spectrum
          int saturation = 40 + (room_id % 3) * 15;
          int brightness = 50 + (room_id % 5) * 10;
          
          // Convert HSV to RGB
          float h = hue / 60.0f;
          float s = saturation / 100.0f;
          float v = brightness / 100.0f;
          
          int i = static_cast<int>(h);
          float f = h - i;
          int p = static_cast<int>(v * (1 - s) * 255);
          int q = static_cast<int>(v * (1 - s * f) * 255);
          int t = static_cast<int>(v * (1 - s * (1 - f)) * 255);
          int val = static_cast<int>(v * 255);
          
          switch (i % 6) {
            case 0: bg_color = IM_COL32(val, t, p, 255); break;
            case 1: bg_color = IM_COL32(q, val, p, 255); break;
            case 2: bg_color = IM_COL32(p, val, t, 255); break;
            case 3: bg_color = IM_COL32(p, q, val, 255); break;
            case 4: bg_color = IM_COL32(t, p, val, 255); break;
            case 5: bg_color = IM_COL32(val, p, q, 255); break;
            default: bg_color = IM_COL32(60, 60, 70, 255); break;
          }
          
          // Check if room is currently selected
          bool is_current = (current_room_id_ == room_id);
          
          // Check if room is open in a card
          bool is_open = false;
          for (int i = 0; i < active_rooms_.Size; i++) {
            if (active_rooms_[i] == room_id) {
              is_open = true;
              break;
            }
          }
          
          // Draw cell background with palette color
          draw_list->AddRectFilled(cell_min, cell_max, bg_color);
          
          // Draw outline ONLY for current/open rooms
          if (is_current) {
            // Light green for current room
            draw_list->AddRect(cell_min, cell_max, 
                             IM_COL32(144, 238, 144, 255), 0.0f, 0, 2.5f);
          } else if (is_open) {
            // Green for open rooms
            draw_list->AddRect(cell_min, cell_max, 
                             IM_COL32(0, 200, 0, 255), 0.0f, 0, 2.0f);
          } else {
            // Subtle gray border for all rooms
            draw_list->AddRect(cell_min, cell_max, 
                             IM_COL32(80, 80, 80, 200), 0.0f, 0, 1.0f);
          }
          
          // Draw room ID (small text)
          std::string room_label = absl::StrFormat("%02X", room_id);
          ImVec2 text_size = ImGui::CalcTextSize(room_label.c_str());
          ImVec2 text_pos = ImVec2(
              cell_min.x + (kRoomCellSize - text_size.x) * 0.5f,
              cell_min.y + (kRoomCellSize - text_size.y) * 0.5f);
          
          // Use smaller font if available
          draw_list->AddText(text_pos, IM_COL32(220, 220, 220, 255), 
                           room_label.c_str());
          
          // Handle clicks
          ImGui::SetCursorScreenPos(cell_min);
          ImGui::InvisibleButton(
              absl::StrFormat("##room%d", room_id).c_str(), 
              ImVec2(kRoomCellSize, kRoomCellSize));
          
          if (ImGui::IsItemClicked()) {
            OnRoomSelected(room_id);
          }
          
          // Hover tooltip with room name and status
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
              ImGui::Text("%s", zelda3::kRoomNames[room_id].data());
            } else {
              ImGui::Text("Room %03X", room_id);
            }
            ImGui::Text("Status: %s", is_open ? "Open" : is_current ? "Current" : "Closed");
            ImGui::Text("Click to %s", is_open ? "focus" : "open");
            ImGui::EndTooltip();
          }
        } else {
          // Empty cell
          draw_list->AddRectFilled(cell_min, cell_max, 
                                 IM_COL32(30, 30, 30, 255));
          draw_list->AddRect(cell_min, cell_max, 
                           IM_COL32(50, 50, 50, 255));
        }
        
        room_index++;
      }
    }
    
    // Advance cursor past the grid
    ImGui::Dummy(ImVec2(
        kRoomsPerRow * (kRoomCellSize + kCellSpacing), 
        kRoomsPerCol * (kRoomCellSize + kCellSpacing)));
  }
  matrix_card.End();
}

void DungeonEditorV2::DrawRoomGraphicsCard() {
  gui::EditorCard graphics_card(
      MakeCardTitle("Room Graphics").c_str(), 
      ICON_MD_IMAGE, &show_room_graphics_);
  
  graphics_card.SetDefaultSize(350, 500);
  graphics_card.SetPosition(gui::EditorCard::Position::Right);
  
  if (graphics_card.Begin()) {
    if (!rom_ || !rom_->is_loaded()) {
      ImGui::Text("ROM not loaded");
      graphics_card.End();
      return;
    }
    
    // Show graphics for current room
    if (current_room_id_ >= 0 && current_room_id_ < static_cast<int>(rooms_.size())) {
      auto& room = rooms_[current_room_id_];
      
      ImGui::Text("Room %03X Graphics", current_room_id_);
      ImGui::Text("Blockset: %02X", room.blockset);
      ImGui::Separator();
      
      // Create a canvas for displaying room graphics
      static gui::Canvas room_gfx_canvas("##RoomGfxCanvas", ImVec2(0x100 + 1, 0x10 * 0x40 + 1));
      
      room_gfx_canvas.DrawBackground();
      room_gfx_canvas.DrawContextMenu();
      room_gfx_canvas.DrawTileSelector(32);
      
      auto blocks = room.blocks();
      
      // Load graphics for this room if not already loaded
      if (blocks.empty()) {
        room.LoadRoomGraphics(room.blockset);
        blocks = room.blocks();
      }
      
      // Only render room graphics if ROM is properly loaded
      if (room.rom() && room.rom()->is_loaded()) {
        room.RenderRoomGraphics();
      }
      
      int current_block = 0;
      constexpr int max_blocks_per_row = 2;
      constexpr int block_width = 128;
      constexpr int block_height = 32;
      
      for (int block : blocks) {
        if (current_block >= 16) break;  // Show first 16 blocks
        
        // Ensure the graphics sheet is loaded
        if (block < static_cast<int>(gfx::Arena::Get().gfx_sheets().size())) {
          auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block];
          
          // Create texture if it doesn't exist
          if (!gfx_sheet.texture() && gfx_sheet.is_active() && gfx_sheet.width() > 0) {
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::CREATE, &gfx_sheet);
            gfx::Arena::Get().ProcessTextureQueue(nullptr);
          }
          
          // Calculate grid position
          int row = current_block / max_blocks_per_row;
          int col = current_block % max_blocks_per_row;
          
          int x = room_gfx_canvas.zero_point().x + 2 + (col * block_width);
          int y = room_gfx_canvas.zero_point().y + 2 + (row * block_height);
          
          // Draw if texture is valid
          if (gfx_sheet.texture() != 0) {
            room_gfx_canvas.draw_list()->AddImage(
                (ImTextureID)(intptr_t)gfx_sheet.texture(),
                ImVec2(x, y),
                ImVec2(x + block_width, y + block_height));
          } else {
            // Draw placeholder for missing graphics
            room_gfx_canvas.draw_list()->AddRectFilled(
                ImVec2(x, y),
                ImVec2(x + block_width, y + block_height),
                IM_COL32(64, 64, 64, 255));
            room_gfx_canvas.draw_list()->AddText(
                ImVec2(x + 10, y + 10),
                IM_COL32(255, 255, 255, 255),
                "No Graphics");
          }
        }
        current_block++;
      }
      
      room_gfx_canvas.DrawGrid(32.0f);
      room_gfx_canvas.DrawOverlay();
    } else {
      ImGui::TextDisabled("No room selected");
    }
  }
  graphics_card.End();
}

void DungeonEditorV2::ProcessDeferredTextures() {
  // Process queued texture commands via Arena's deferred system
  // This is critical for ensuring textures are actually created and updated
  gfx::Arena::Get().ProcessTextureQueue(renderer_);
}

}  // namespace yaze::editor

