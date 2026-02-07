#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_CHEST_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_CHEST_EDITOR_PANEL_H_

#include <array>
#include <cctype>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/zelda3_labels.h"

namespace yaze {
namespace editor {

/**
 * @class ChestEditorPanel
 * @brief EditorPanel for managing chest contents in dungeon rooms
 *
 * This panel provides chest item editing functionality, similar to
 * ZScream's chest editor. Displays all chests in the current room
 * with their contents and allows editing item type and chest size.
 *
 * @see EditorPanel - Base interface
 * @see SpriteEditorPanel - Similar panel for sprites
 */
class ChestEditorPanel : public EditorPanel {
 public:
  ChestEditorPanel(int* current_room_id,
                   std::array<zelda3::Room, 0x128>* rooms,
                   DungeonCanvasViewer* canvas_viewer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        canvas_viewer_(canvas_viewer) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.chest_editor"; }
  std::string GetDisplayName() const override { return "Chest Editor"; }
  std::string GetIcon() const override { return ICON_MD_INVENTORY_2; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 70; }  // After sprite editor

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!current_room_id_ || !rooms_) {
      ImGui::TextDisabled("No room data available");
      return;
    }

    if (*current_room_id_ < 0 ||
        *current_room_id_ >= static_cast<int>(rooms_->size())) {
      ImGui::TextDisabled("No room selected");
      return;
    }

    DrawChestList();
    ImGui::Separator();
    DrawChestProperties();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  void SetCanvasViewer(DungeonCanvasViewer* viewer) {
    canvas_viewer_ = viewer;
  }

  void SetChestModifiedCallback(std::function<void(int, int)> callback) {
    chest_modified_callback_ = std::move(callback);
  }

 private:
  void DrawChestList() {
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    auto& chests = room.GetChests();

    // Header with count and limit warning
    int chest_count = static_cast<int>(chests.size());
    ImVec4 count_color =
        chest_count > 6 ? theme.text_error_red : theme.text_primary;
    ImGui::TextColored(count_color, ICON_MD_INVENTORY_2 " Chests: %d/6", 
                       chest_count);
    
    if (chest_count > 6) {
      ImGui::SameLine();
      ImGui::TextColored(theme.text_warning_yellow, ICON_MD_WARNING);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Room exceeds chest limit (6 max)!\n"
                          "This may cause game crashes.");
      }
    }

    // Add chest button
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_ADD " Add")) {
      // Add new chest with default values
      zelda3::chest_data new_chest;
      new_chest.id = 0;      // Default item: nothing
      new_chest.size = false; // Small chest
      chests.push_back(new_chest);
      selected_chest_index_ = static_cast<int>(chests.size()) - 1;
      if (chest_modified_callback_) {
        chest_modified_callback_(*current_room_id_, selected_chest_index_);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add new chest to room");
    }

    // Chest list
    if (chests.empty()) {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " No chests in this room");
      return;
    }

    const auto& item_names = zelda3::Zelda3Labels::GetItemNames();
    float list_height = std::min(200.0f, ImGui::GetContentRegionAvail().y * 0.5f);
    ImGui::BeginChild("##ChestList", ImVec2(0, list_height), true);
    
    for (size_t i = 0; i < chests.size(); ++i) {
      const auto& chest = chests[i];
      bool is_selected = (selected_chest_index_ == static_cast<int>(i));
      
      ImGui::PushID(static_cast<int>(i));
      
      // Chest icon based on size
      const char* size_icon = chest.size ? ICON_MD_INVENTORY_2 : ICON_MD_INVENTORY;
      const char* size_label = chest.size ? "Big" : "Small";
      
      // Get item name
      std::string item_name = (chest.id < item_names.size()) 
          ? item_names[chest.id] 
          : absl::StrFormat("Unknown (0x%02X)", chest.id);
      
      // Selectable list item
      std::string label = absl::StrFormat("%s [%zu] %s: %s", 
          size_icon, i + 1, size_label, item_name.c_str());
      
      if (ImGui::Selectable(label.c_str(), is_selected)) {
        selected_chest_index_ = static_cast<int>(i);
      }
      
      // Highlight with theme color
      if (is_selected) {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImU32 sel_color = ImGui::ColorConvertFloat4ToU32(
            theme.dungeon_selection_primary);
        ImGui::GetWindowDrawList()->AddRect(min, max, sel_color, 0.0f, 0, 2.0f);
      }
      
      ImGui::PopID();
    }
    
    ImGui::EndChild();
  }

  void DrawChestProperties() {
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    auto& chests = room.GetChests();

    if (selected_chest_index_ < 0 || 
        selected_chest_index_ >= static_cast<int>(chests.size())) {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " Select a chest to edit");
      return;
    }

    auto& chest = chests[selected_chest_index_];
    const auto& item_names = zelda3::Zelda3Labels::GetItemNames();

    ImGui::Text(ICON_MD_EDIT " Editing Chest #%d", selected_chest_index_ + 1);

    // Chest type (size)
    ImGui::Text("Type:");
    ImGui::SameLine();
    bool is_big = chest.size;
    if (ImGui::RadioButton("Small", !is_big)) {
      chest.size = false;
      if (chest_modified_callback_) {
        chest_modified_callback_(*current_room_id_, selected_chest_index_);
      }
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Big", is_big)) {
      chest.size = true;
      if (chest_modified_callback_) {
        chest_modified_callback_(*current_room_id_, selected_chest_index_);
      }
    }

    // Item selector dropdown
    ImGui::Text("Item:");
    ImGui::SameLine();
    
    std::string current_item = (chest.id < item_names.size())
        ? absl::StrFormat("[%02X] %s", chest.id, item_names[chest.id].c_str())
        : absl::StrFormat("[%02X] Unknown", chest.id);
    
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##ItemSelect", current_item.c_str())) {
      // Search filter
      static char search_buf[64] = "";
      ImGui::InputTextWithHint("##Search", "Search items...", 
                               search_buf, sizeof(search_buf));
      ImGui::Separator();
      
      for (size_t i = 0; i < item_names.size(); ++i) {
        // Apply search filter
        if (search_buf[0] != '\0') {
          std::string name_lower = item_names[i];
          std::string filter_lower = search_buf;
          for (auto& c : name_lower) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
          }
          for (auto& c : filter_lower) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
          }
          if (name_lower.find(filter_lower) == std::string::npos) {
            continue;
          }
        }
        
        std::string item_label = absl::StrFormat("[%02X] %s", 
            static_cast<int>(i), item_names[i].c_str());
        bool is_selected = (chest.id == static_cast<uint8_t>(i));
        
        if (ImGui::Selectable(item_label.c_str(), is_selected)) {
          chest.id = static_cast<uint8_t>(i);
          if (chest_modified_callback_) {
            chest_modified_callback_(*current_room_id_, selected_chest_index_);
          }
        }
        
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      
      ImGui::EndCombo();
    }

    // Delete button
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, theme.status_error);
    if (ImGui::Button(ICON_MD_DELETE " Delete Chest")) {
      chests.erase(chests.begin() + selected_chest_index_);
      selected_chest_index_ = -1;
      if (chest_modified_callback_) {
        chest_modified_callback_(*current_room_id_, -1);
      }
    }
    ImGui::PopStyleColor();
  }

  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;

  // Selection state
  int selected_chest_index_ = -1;

  std::function<void(int, int)> chest_modified_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_CHEST_EDITOR_PANEL_H_
