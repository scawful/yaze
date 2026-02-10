#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_ITEM_EDITOR_PANEL_H_

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
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
    const auto& theme = AgentUI::GetTheme();
    // Placement mode indicator
    if (placement_mode_) {
      const char* item_name = (selected_item_id_ < kPotItemCount)
          ? kPotItemNames[selected_item_id_]
          : "Unknown";
      ImGui::TextColored(theme.status_warning,
          ICON_MD_PLACE " Placing: %s (0x%02X)", item_name, selected_item_id_);
      if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
        placement_mode_ = false;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetItemPlacementMode(false, 0);
        }
      }
    } else {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " Select an item to place");
    }
  }

  void DrawItemSelector() {
    const auto& theme = AgentUI::GetTheme();
    ImGui::Text(ICON_MD_INVENTORY " Select Item:");

    // Item grid with responsive sizing
    float available_height = ImGui::GetContentRegionAvail().y;
    // Reserve space for room items section (header + list + some margin)
    float reserved_height = 120.0f;
    // Calculate grid height: at least 150px, but responsive to available space
    float grid_height = std::max(150.0f, std::min(400.0f, available_height - reserved_height));

    // Responsive item size based on panel width
    float panel_width = ImGui::GetContentRegionAvail().x;
    float item_size = std::max(36.0f, std::min(48.0f, (panel_width - 40.0f) / 6.0f));
    int items_per_row = std::max(1, static_cast<int>(panel_width / (item_size + 8)));

    ImGui::BeginChild("##ItemGrid", ImVec2(0, grid_height), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    
    int col = 0;
    for (size_t i = 0; i < kPotItemCount; ++i) {
      bool is_selected = (selected_item_id_ == static_cast<int>(i));

      ImGui::PushID(static_cast<int>(i));

      // Color-coded button based on item type using theme colors
      ImVec4 button_color = GetItemTypeColor(static_cast<int>(i), theme);
      if (is_selected) {
        button_color.x = std::min(1.0f, button_color.x + 0.2f);
        button_color.y = std::min(1.0f, button_color.y + 0.2f);
        button_color.z = std::min(1.0f, button_color.z + 0.2f);
      }

      {
        gui::StyleColorGuard btn_colors({
            {ImGuiCol_Button, button_color},
            {ImGuiCol_ButtonHovered,
             ImVec4(std::min(1.0f, button_color.x + 0.1f),
                    std::min(1.0f, button_color.y + 0.1f),
                    std::min(1.0f, button_color.z + 0.1f), 1.0f)},
            {ImGuiCol_ButtonActive,
             ImVec4(std::min(1.0f, button_color.x + 0.2f),
                    std::min(1.0f, button_color.y + 0.2f),
                    std::min(1.0f, button_color.z + 0.2f), 1.0f)},
        });

        // Get icon and short name for item
        const char* icon = GetItemTypeIcon(static_cast<int>(i));
        std::string label =
            absl::StrFormat("%s\n%02X", icon, static_cast<int>(i));
        if (ImGui::Button(label.c_str(), ImVec2(item_size, item_size))) {
          selected_item_id_ = static_cast<int>(i);
          placement_mode_ = true;
          if (canvas_viewer_) {
            canvas_viewer_->object_interaction().SetItemPlacementMode(
                true, static_cast<uint8_t>(i));
          }
        }
      }
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s (0x%02X)\nClick to select for placement",
            kPotItemNames[i], static_cast<int>(i));
      }
      
      // Selection highlight using theme color
      if (is_selected) {
        ImVec2 min = ImGui::GetItemRectMin();
        ImVec2 max = ImGui::GetItemRectMax();
        ImU32 sel_color = ImGui::ColorConvertFloat4ToU32(theme.dungeon_selection_primary);
        ImGui::GetWindowDrawList()->AddRect(min, max, sel_color, 0.0f, 0, 2.0f);
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
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    const auto& items = room.GetPotItems();

    ImGui::Text(ICON_MD_LIST " Room Items (%zu):", items.size());

    if (items.empty()) {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " No items in this room");
      return;
    }

    // Responsive list height - use remaining available space
    float list_height = std::max(120.0f, ImGui::GetContentRegionAvail().y - 10.0f);
    ImGui::BeginChild("##ItemList", ImVec2(0, list_height), true);
    for (size_t i = 0; i < items.size(); ++i) {
      const auto& item = items[i];

      ImGui::PushID(static_cast<int>(i));

      const char* item_name = (item.item < kPotItemCount)
          ? kPotItemNames[item.item]
          : "Unknown";

      ImGui::Text("[%zu] %s (0x%02X)", i, item_name, item.item);
      ImGui::SameLine();
      ImGui::TextColored(theme.text_secondary_gray,
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

  ImVec4 GetItemTypeColor(int item_id, const AgentUITheme& theme) {
    // Color-code based on item type using theme colors
    if (item_id == 0 || item_id == 22) {
      return theme.dungeon_object_default;  // Gray for "Nothing"
    } else if (item_id >= 1 && item_id <= 7) {
      return theme.dungeon_sprite_layer0;   // Green for rupees/items
    } else if (item_id == 8) {
      return theme.dungeon_object_chest;    // Gold for key
    } else if (item_id >= 15 && item_id <= 17) {
      return theme.status_error;            // Red for enemies
    } else if (item_id >= 23 && item_id <= 27) {
      return theme.dungeon_object_stairs;   // Yellow for special
    }
    return theme.dungeon_object_pot;        // Pot color for default
  }

  const char* GetItemTypeIcon(int item_id) {
    // Return item-type-appropriate icons
    if (item_id == 0 || item_id == 22) {
      return ICON_MD_BLOCK;  // Nothing
    } else if (item_id == 1 || item_id == 7) {
      return ICON_MD_MONETIZATION_ON;  // Rupees (green/blue)
    } else if (item_id == 4 || item_id == 6 || item_id == 11 || item_id == 19 || item_id == 21) {
      return ICON_MD_FAVORITE;  // Hearts
    } else if (item_id == 8) {
      return ICON_MD_KEY;  // Key
    } else if (item_id == 5 || item_id == 10) {
      return ICON_MD_CIRCLE;  // Bombs
    } else if (item_id == 9) {
      return ICON_MD_ARROW_UPWARD;  // Arrows
    } else if (item_id == 12 || item_id == 13) {
      return ICON_MD_AUTO_AWESOME;  // Magic
    } else if (item_id == 14) {
      return ICON_MD_EGG;  // Cucco
    } else if (item_id >= 15 && item_id <= 17) {
      return ICON_MD_PERSON;  // Soldiers
    } else if (item_id == 18) {
      return ICON_MD_WARNING;  // Landmine
    } else if (item_id == 20) {
      return ICON_MD_FLUTTER_DASH;  // Fairy
    } else if (item_id == 23) {
      return ICON_MD_TERRAIN;  // Hole
    } else if (item_id == 24) {
      return ICON_MD_SWAP_HORIZ;  // Warp
    } else if (item_id == 25) {
      return ICON_MD_STAIRS;  // Staircase
    } else if (item_id == 26) {
      return ICON_MD_BROKEN_IMAGE;  // Bombable
    } else if (item_id == 27) {
      return ICON_MD_TOGGLE_ON;  // Switch
    } else if (item_id == 2) {
      return ICON_MD_LANDSCAPE;  // Rock
    } else if (item_id == 3) {
      return ICON_MD_BUG_REPORT;  // Bee
    }
    return ICON_MD_HELP;  // Unknown
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

