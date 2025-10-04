#include "dungeon_editor_v2.h"

#include <cstdio>

#include "absl/strings/str_format.h"
#include "app/gfx/snes_palette.h"
#include "app/zelda3/dungeon/room.h"
#include "app/gui/icons.h"
#include "imgui/imgui.h"

namespace yaze::editor {

using ImGui::BeginTable;
using ImGui::BeginTabBar;
using ImGui::BeginTabItem;
using ImGui::EndTable;
using ImGui::EndTabBar;
using ImGui::EndTabItem;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;

void DungeonEditorV2::Initialize() {
  // No complex initialization needed - components handle themselves
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

  is_loaded_ = true;
  return absl::OkStatus();
}

absl::Status DungeonEditorV2::Update() {
  if (!is_loaded_) {
    ImGui::Text("Loading...");
    return absl::OkStatus();
  }

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

void DungeonEditorV2::DrawLayout() {
  // Simple 3-column layout as designed
  if (BeginTable("##DungeonEditTable", 3,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV,
                 ImVec2(0, 0))) {
    TableSetupColumn("Room Selector", ImGuiTableColumnFlags_WidthFixed, 250);
    TableSetupColumn("Canvas", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Object Selector", ImGuiTableColumnFlags_WidthFixed, 300);
    TableHeadersRow();
    TableNextRow();

    // Column 1: Room Selector (fully delegated)
    TableNextColumn();
    room_selector_.Draw();

    // Column 2: Canvas (fully delegated)
    TableNextColumn();
    if (BeginTabBar("##RoomTabs")) {
      for (int i = 0; i < active_rooms_.Size; i++) {
        int room_id = active_rooms_[i];
        bool open = true;

        std::string tab_name_str;
        const char* tab_name;
        if (room_id >= 0 && static_cast<size_t>(room_id) < std::size(zelda3::kRoomNames)) {
          tab_name = zelda3::kRoomNames[room_id].data();
        } else {
          tab_name_str = absl::StrFormat("Room %03X", room_id);
          tab_name = tab_name_str.c_str();
        }

        if (BeginTabItem(tab_name, &open)) {
          DrawRoomTab(room_id);
          EndTabItem();
        }

        if (!open) {
          active_rooms_.erase(active_rooms_.Data + i);
          i--;
        }
      }

      // Add tab button
      if (ImGui::TabItemButton(ICON_MD_ADD, ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
        OnRoomSelected(room_selector_.current_room_id());
      }
      
      EndTabBar();
    }

    // Column 3: Object Selector (fully delegated)
    TableNextColumn();
    object_selector_.Draw();

    EndTable();
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
      return;  // Already open
    }
  }

  // Add new tab
  active_rooms_.push_back(room_id);
  room_selector_.set_active_rooms(active_rooms_);
}

}  // namespace yaze::editor

