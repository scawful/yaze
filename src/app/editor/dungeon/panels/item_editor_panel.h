#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace editor {

/**
 * @class ItemEditorPanel
 * @brief EditorPanel for placing and managing dungeon pot items
 *
 * This panel provides item selection and placement functionality
 * for dungeon rooms, similar to ObjectEditorPanel and SpriteEditorPanel.
 *
 * @see EditorPanel - Base interface
 * @see ObjectEditorPanel - Similar panel for tile objects
 * @see SpriteEditorPanel - Similar panel for sprites
 */
class ItemEditorPanel : public EditorPanel {
 public:
  ItemEditorPanel(int* current_room_id,
                  std::array<zelda3::Room, 0x128>* rooms,
                  DungeonCanvasViewer* canvas_viewer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        canvas_viewer_(canvas_viewer) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.item_editor"; }
  std::string GetDisplayName() const override { return "Item Editor"; }
  std::string GetIcon() const override { return ICON_MD_INVENTORY; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 66; }

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

    DrawPlacementControls();
    ImGui::Separator();
    DrawItemSelector();
    ImGui::Separator();
    DrawRoomItems();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  void SetCanvasViewer(DungeonCanvasViewer* viewer) {
    canvas_viewer_ = viewer;
  }

  void SetItemPlacedCallback(
      std::function<void(const zelda3::PotItem&)> callback) {
    item_placed_callback_ = std::move(callback);
  }

 private:
  // Pot item names from ZELDA3_DUNGEON_SPEC.md Section 7.2
  static constexpr const char* kPotItemNames[] = {
      "Nothing",        // 0
      "Green Rupee",    // 1
      "Rock",           // 2
      "Bee",            // 3
      "Health",         // 4
      "Bomb",           // 5
      "Heart",          // 6
      "Blue Rupee",     // 7
      "Key",            // 8
      "Arrow",          // 9
      "Bomb",           // 10
      "Heart",          // 11
      "Magic",          // 12
      "Full Magic",     // 13
      "Cucco",          // 14
      "Green Soldier",  // 15
      "Bush Stal",      // 16
      "Blue Soldier",   // 17
      "Landmine",       // 18
      "Heart",          // 19
      "Fairy",          // 20
      "Heart",          // 21
      "Nothing",        // 22
      "Hole",           // 23
      "Warp",           // 24
      "Staircase",      // 25
      "Bombable",       // 26
      "Switch"          // 27
  };
  static constexpr size_t kPotItemCount = sizeof(kPotItemNames) / sizeof(kPotItemNames[0]);

  void DrawPlacementControls() {
    // Placement mode indicator
    if (placement_mode_) {
      const char* item_name = (selected_item_id_ < kPotItemCount) 
          ? kPotItemNames[selected_item_id_] 
          : "Unknown";
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
          ICON_MD_PLACE " Placing: %s (0x%02X)", item_name, selected_item_id_);
      if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
        placement_mode_ = false;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetItemPlacementMode(false, 0);
        }
      }
    } else {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          ICON_MD_INFO " Select an item to place");
    }
  }

  void DrawItemSelector() {
    ImGui::Text(ICON_MD_INVENTORY " Select Item:");
    
    // Item grid
    constexpr float kPreviewSize = 48.0f;
    float panel_width = ImGui::GetContentRegionAvail().x;
    int items_per_row = std::max(1, static_cast<int>(panel_width / (kPreviewSize + 8)));
    
    ImGui::BeginChild("##ItemGrid", ImVec2(0, 160), true, 
                      ImGuiWindowFlags_HorizontalScrollbar);
    
    int col = 0;
    for (size_t i = 0; i < kPotItemCount; ++i) {
      bool is_selected = (selected_item_id_ == static_cast<int>(i));
      
      ImGui::PushID(static_cast<int>(i));
      
      // Color-coded button based on item type
      ImVec4 button_color = GetItemTypeColor(static_cast<int>(i));
      if (is_selected) {
        button_color.x += 0.2f;
        button_color.y += 0.2f;
        button_color.z += 0.2f;
      }
      
      ImGui::PushStyleColor(ImGuiCol_Button, button_color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
          ImVec4(button_color.x + 0.1f, button_color.y + 0.1f, button_color.z + 0.1f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
          ImVec4(button_color.x + 0.2f, button_color.y + 0.2f, button_color.z + 0.2f, 1.0f));
      
      std::string label = absl::StrFormat("%02X", static_cast<int>(i));
      if (ImGui::Button(label.c_str(), ImVec2(kPreviewSize, kPreviewSize))) {
        selected_item_id_ = static_cast<int>(i);
        placement_mode_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetItemPlacementMode(true, 
              static_cast<uint8_t>(i));
        }
      }
      
      ImGui::PopStyleColor(3);
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s (0x%02X)\nClick to select for placement",
            kPotItemNames[i], static_cast<int>(i));
      }
      
      // Selection highlight
      if (is_selected) {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRect(
            min, max, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
      }
      
      ImGui::PopID();
      
      col++;
      if (col < items_per_row) {
        ImGui::SameLine();
      } else {
        col = 0;
      }
    }
    
    ImGui::EndChild();
  }

  void DrawRoomItems() {
    auto& room = (*rooms_)[*current_room_id_];
    const auto& items = room.GetPotItems();
    
    ImGui::Text(ICON_MD_LIST " Room Items (%zu):", items.size());
    
    if (items.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          ICON_MD_INFO " No items in this room");
      return;
    }
    
    ImGui::BeginChild("##ItemList", ImVec2(0, 120), true);
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& item = items[i];
      
      ImGui::PushID(static_cast<int>(i));
      
      const char* item_name = (item.item < kPotItemCount) 
          ? kPotItemNames[item.item] 
          : "Unknown";
      
      ImGui::Text("[%zu] %s (0x%02X)", i, item_name, item.item);
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
          "@ (%d,%d)", item.GetTileX(), item.GetTileY());
      
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_DELETE "##Del")) {
        auto& mutable_room = (*rooms_)[*current_room_id_];
        mutable_room.GetPotItems().erase(
            mutable_room.GetPotItems().begin() + static_cast<long>(i));
      }
      
      ImGui::PopID();
    }
    ImGui::EndChild();
  }

  ImVec4 GetItemTypeColor(int item_id) {
    // Color-code based on item type
    if (item_id == 0 || item_id == 22) {
      return ImVec4(0.4f, 0.4f, 0.4f, 1.0f);  // Gray for "Nothing"
    } else if (item_id >= 1 && item_id <= 7) {
      return ImVec4(0.3f, 0.7f, 0.3f, 1.0f);  // Green for rupees/items
    } else if (item_id == 8) {
      return ImVec4(0.7f, 0.7f, 0.3f, 1.0f);  // Yellow for key
    } else if (item_id >= 15 && item_id <= 17) {
      return ImVec4(0.7f, 0.3f, 0.3f, 1.0f);  // Red for enemies
    } else if (item_id >= 23 && item_id <= 27) {
      return ImVec4(0.5f, 0.3f, 0.7f, 1.0f);  // Purple for special
    }
    return ImVec4(0.3f, 0.5f, 0.7f, 1.0f);  // Blue default
  }

  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  
  // Selection state
  int selected_item_id_ = 0;
  bool placement_mode_ = false;
  
  std::function<void(const zelda3::PotItem&)> item_placed_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_

