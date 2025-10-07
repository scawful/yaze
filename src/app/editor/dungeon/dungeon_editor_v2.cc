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

namespace yaze::editor {

// No table layout needed - all cards are independent

void DungeonEditorV2::Initialize() {
  // Don't initialize emulator preview yet - ROM might not be loaded
  // Will be initialized in Load() instead
  
  // Setup docking class for room windows
  room_window_class_.ClassId = ImGui::GetID("DungeonRoomClass");
  room_window_class_.DockingAllowUnclassed = false;  // Room windows dock together
}

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
  object_emulator_preview_.Initialize(rom_);
  
  // Initialize palette editor with loaded ROM
  palette_editor_.Initialize(rom_);
  
  // Initialize unified object editor card
  object_editor_card_ = std::make_unique<ObjectEditorCard>(rom_, &canvas_viewer_);
  
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
    // Show minimal loading message in parent window
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Dungeon Editor Loading...");
    ImGui::TextWrapped("Independent editor cards will appear once ROM data is loaded.");
    return absl::OkStatus();
  }

  // Minimize parent window content - just show a toolbar
  DrawToolset();
  
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
      "Editor cards are independent windows - dock them anywhere!");
  ImGui::TextWrapped(
      "Room Selector, Object Selector, and Room cards can be freely arranged. "
      "This parent window can be minimized or closed.");

  // Render all independent cards (these create their own top-level windows)
  object_emulator_preview_.Render();
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
      std::printf("Failed to save room: %s\n", status.message().data());
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
  
  if (toolbar.AddToggle(ICON_MD_CATEGORY, &show_object_selector_, "Toggle Object Selector (Legacy)")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_CONSTRUCTION, &show_object_editor_, "Toggle Object Editor (Unified)")) {
    // Toggled
  }
  
  if (toolbar.AddToggle(ICON_MD_PALETTE, &show_palette_editor_, "Toggle Palette Editor")) {
    // Toggled
  }

  toolbar.End();
}

void DungeonEditorV2::DrawLayout() {
  // NO TABLE LAYOUT - All independent dockable EditorCards
  
  // 1. Room Selector Card (independent, dockable)
  if (show_room_selector_) {
    DrawRoomsListCard();
  }
  
  // 2. Room Matrix Card (visual navigation)
  if (show_room_matrix_) {
    DrawRoomMatrixCard();
  }
  
  // 3. Entrances List Card
  if (show_entrances_list_) {
    DrawEntrancesListCard();
  }
  
  // 3b. Room Graphics Card
  if (show_room_graphics_) {
    DrawRoomGraphicsCard();
  }

  // 4. Legacy Object Selector Card (independent, dockable)
  if (show_object_selector_) {
    gui::EditorCard object_card(
        MakeCardTitle("Object Selector").c_str(), 
        ICON_MD_CATEGORY, &show_object_selector_);
    if (object_card.Begin()) {
      object_selector_.Draw();
    }
    object_card.End();
  }
  
  // 4b. Unified Object Editor Card (new, combines selector + preview + interaction)
  if (show_object_editor_ && object_editor_card_) {
    object_editor_card_->Draw(&show_object_editor_);
  }
  
  // 5. Palette Editor Card (independent, dockable)
  if (show_palette_editor_) {
    gui::EditorCard palette_card(
        MakeCardTitle("Palette Editor").c_str(), 
        ICON_MD_PALETTE, &show_palette_editor_);
    if (palette_card.Begin()) {
      palette_editor_.Draw();
    }
    palette_card.End();
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
    }
    
    auto& room_card = room_cards_[room_id];
    
    // Use docking class to make room cards dock together
    ImGui::SetNextWindowClass(&room_window_class_);
    
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
  if (room_id < 0 || room_id >= static_cast<int>(rooms_.size())) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  // Lazy load room data
  if (!rooms_[room_id].IsLoaded()) {
    auto status = room_loader_.LoadRoom(room_id, rooms_[room_id]);
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load room: %s",
                       status.message().data());
      return;
    }
  }

  // Room info header
  ImGui::Text("Room %03X", room_id);
  ImGui::SameLine();
  if (rooms_[room_id].IsLoaded()) {
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
  
  matrix_card.SetDefaultSize(520, 620);
  
  if (matrix_card.Begin()) {
    // 16 wide x 19 tall = 304 cells (295 rooms + 9 empty)
    constexpr int kRoomsPerRow = 16;
    constexpr int kRoomsPerCol = 19;
    constexpr int kTotalRooms = 0x128;  // 296 rooms (0x00-0x127)
    constexpr float kRoomCellSize = 28.0f;  // Compact cells
    constexpr float kCellSpacing = 2.0f;
    
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
          // ALWAYS use palette-based color (lazy load if needed)
          ImU32 bg_color = IM_COL32(60, 60, 70, 255);  // Fallback
          
          // Try to get palette color
          uint8_t palette = 0;
          if (room_id < static_cast<int>(rooms_.size())) {
            // Lazy load room if needed to get palette
            if (!rooms_[room_id].IsLoaded()) {
              auto status = room_loader_.LoadRoom(room_id, rooms_[room_id]);
              if (status.ok()) {
                palette = rooms_[room_id].palette;
              }
            } else {
              palette = rooms_[room_id].palette;
            }
            
            // Create a color variation based on palette ID
            float r = 0.3f + (palette % 4) * 0.15f;
            float g = 0.3f + ((palette / 4) % 4) * 0.15f;
            float b = 0.4f + ((palette / 16) % 2) * 0.2f;
            bg_color = IM_COL32(
                static_cast<int>(r * 255), 
                static_cast<int>(g * 255), 
                static_cast<int>(b * 255), 255);
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
          
          // Hover tooltip with room name
          if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            if (room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
              ImGui::Text("%s", zelda3::kRoomNames[room_id].data());
            } else {
              ImGui::Text("Room %03X", room_id);
            }
            ImGui::Text("Palette: %02X", 
                       room_id < static_cast<int>(rooms_.size()) ? 
                       rooms_[room_id].palette : 0);
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
      
      int current_block = 0;
      constexpr int max_blocks_per_row = 2;
      constexpr int block_width = 128;
      constexpr int block_height = 32;
      
      for (int block : blocks) {
        if (current_block >= 16) break;  // Show first 16 blocks
        
        // Ensure the graphics sheet is loaded
        if (block < static_cast<int>(gfx::Arena::Get().gfx_sheets().size())) {
          auto& gfx_sheet = gfx::Arena::Get().gfx_sheets()[block];
          
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

}  // namespace yaze::editor

