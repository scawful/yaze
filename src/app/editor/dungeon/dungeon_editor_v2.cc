#include "dungeon_editor_v2.h"

#include <algorithm>
#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_palette.h"
#include "app/zelda3/dungeon/room.h"
#include "app/gui/icons.h"
#include "app/gui/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze::editor {

// No table layout needed - all cards are independent

void DungeonEditorV2::Initialize() {
  // Don't initialize emulator preview yet - ROM might not be loaded
  // Will be initialized in Load() instead
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
  
  toolbar.AddSeparator();
  
  if (toolbar.AddToggle(ICON_MD_CATEGORY, &show_object_selector_, "Toggle Object Selector")) {
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

  // 4. Object Selector/Manager Card (independent, dockable)
  if (show_object_selector_) {
    gui::EditorCard object_card(
        MakeCardTitle("Object Selector").c_str(), 
        ICON_MD_CATEGORY, &show_object_selector_);
    if (object_card.Begin()) {
      object_selector_.Draw();
    }
    object_card.End();
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
    }
    
    auto& room_card = room_cards_[room_id];
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

  // Quick controls
  ImGui::Text("Room %03X", room_id);
  ImGui::SameLine();
  if (ImGui::Button("Load Graphics")) {
    (void)room_loader_.LoadAndRenderRoomGraphics(rooms_[room_id]);
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    auto status = rooms_[room_id].SaveObjects();
    if (!status.ok()) {
      ImGui::TextColored(ImVec4(1, 0, 0, 1), "Save failed: %s",
                         status.message().data());
    }
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
  
  if (selector_card.Begin()) {
    // Add text filter
    static char room_filter[256] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##RoomFilter", ICON_MD_SEARCH " Filter rooms...", 
                                 room_filter, sizeof(room_filter))) {
      // Filter updated
    }
    
    ImGui::Separator();
    
    // Scrollable room list with resource labels
    if (ImGui::BeginChild("##RoomsList", ImVec2(0, 0), true)) {
      std::string filter_str = room_filter;
      std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);
      
      for (int i = 0; i < 0x128; i++) {
        std::string room_name;
        if (i < static_cast<int>(std::size(zelda3::kRoomNames))) {
          room_name = absl::StrFormat("%03X - %s", i, zelda3::kRoomNames[i].data());
        } else {
          room_name = absl::StrFormat("%03X - Room %d", i, i);
        }
        
        // Apply filter
        if (!filter_str.empty()) {
          std::string room_name_lower = room_name;
          std::transform(room_name_lower.begin(), room_name_lower.end(), 
                        room_name_lower.begin(), ::tolower);
          if (room_name_lower.find(filter_str) == std::string::npos) {
            continue;
          }
        }
        
        bool is_selected = (current_room_id_ == i);
        if (ImGui::Selectable(room_name.c_str(), is_selected)) {
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
      MakeCardTitle("Entrances List").c_str(), 
      ICON_MD_DOOR_FRONT, &show_entrances_list_);
  
  if (entrances_card.Begin()) {
    // Add text filter
    static char entrance_filter[256] = "";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##EntranceFilter", ICON_MD_SEARCH " Filter entrances...", 
                                 entrance_filter, sizeof(entrance_filter))) {
      // Filter updated
    }
    
    ImGui::Separator();
    
    // Scrollable entrance list with associated room names
    if (ImGui::BeginChild("##EntrancesList", ImVec2(0, 0), true)) {
      std::string filter_str = entrance_filter;
      std::transform(filter_str.begin(), filter_str.end(), filter_str.begin(), ::tolower);
      
      for (int i = 0; i < static_cast<int>(entrances_.size()); i++) {
        int room_id = entrances_[i].room_;
        
        std::string room_name = "Unknown";
        if (room_id >= 0 && room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
          room_name = zelda3::kRoomNames[room_id].data();
        }
        
        std::string entrance_label = absl::StrFormat("%02X - %s (Room %03X)", 
                                                     i, room_name.c_str(), room_id);
        
        // Apply filter
        if (!filter_str.empty()) {
          std::string entrance_label_lower = entrance_label;
          std::transform(entrance_label_lower.begin(), entrance_label_lower.end(), 
                        entrance_label_lower.begin(), ::tolower);
          if (entrance_label_lower.find(filter_str) == std::string::npos) {
            continue;
          }
        }
        
        if (ImGui::Selectable(entrance_label.c_str())) {
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
  
  matrix_card.SetDefaultSize(600, 600);
  
  if (matrix_card.Begin()) {
    // Draw 8x8 grid of rooms (first 64 rooms)
    constexpr int kRoomsPerRow = 8;
    constexpr int kRoomsPerCol = 8;
    constexpr float kRoomCellSize = 64.0f;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    
    for (int row = 0; row < kRoomsPerCol; row++) {
      for (int col = 0; col < kRoomsPerRow; col++) {
        int room_id = row * kRoomsPerRow + col;
        
        ImVec2 cell_min = ImVec2(canvas_pos.x + col * kRoomCellSize, 
                                 canvas_pos.y + row * kRoomCellSize);
        ImVec2 cell_max = ImVec2(cell_min.x + kRoomCellSize, 
                                 cell_min.y + kRoomCellSize);
        
        // Check if room is active
        bool is_active = false;
        for (int i = 0; i < active_rooms_.Size; i++) {
          if (active_rooms_[i] == room_id) {
            is_active = true;
            break;
          }
        }
        
        // Draw cell background
        ImU32 bg_color = is_active ? IM_COL32(100, 150, 255, 255) 
                                   : IM_COL32(50, 50, 50, 255);
        draw_list->AddRectFilled(cell_min, cell_max, bg_color);
        
        // Draw cell border
        draw_list->AddRect(cell_min, cell_max, IM_COL32(150, 150, 150, 255));
        
        // Draw room ID
        std::string room_label = absl::StrFormat("%02X", room_id);
        ImVec2 text_size = ImGui::CalcTextSize(room_label.c_str());
        ImVec2 text_pos = ImVec2(cell_min.x + (kRoomCellSize - text_size.x) * 0.5f,
                                cell_min.y + (kRoomCellSize - text_size.y) * 0.5f);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), room_label.c_str());
        
        // Handle clicks
        ImGui::SetCursorScreenPos(cell_min);
        ImGui::InvisibleButton(absl::StrFormat("##room%d", room_id).c_str(), 
                              ImVec2(kRoomCellSize, kRoomCellSize));
        
        if (ImGui::IsItemClicked()) {
          OnRoomSelected(room_id);
        }
        
        // Hover preview (TODO: implement room bitmap preview)
        if (ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          if (room_id < static_cast<int>(std::size(zelda3::kRoomNames))) {
            ImGui::Text("%s", zelda3::kRoomNames[room_id].data());
          } else {
            ImGui::Text("Room %03X", room_id);
          }
          ImGui::EndTooltip();
        }
      }
    }
    
    // Advance cursor past the grid
    ImGui::Dummy(ImVec2(kRoomsPerRow * kRoomCellSize, kRoomsPerCol * kRoomCellSize));
  }
  matrix_card.End();
}

}  // namespace yaze::editor

