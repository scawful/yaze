#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

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
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace editor {

/**
 * @class SpriteEditorPanel
 * @brief EditorPanel for placing and managing dungeon sprites
 *
 * This panel provides sprite selection and placement functionality
 * for dungeon rooms, similar to ObjectEditorPanel.
 *
 * @see EditorPanel - Base interface
 * @see ObjectEditorPanel - Similar panel for tile objects
 */
class SpriteEditorPanel : public EditorPanel {
 public:
  SpriteEditorPanel(int* current_room_id,
                    std::array<zelda3::Room, 0x128>* rooms,
                    DungeonCanvasViewer* canvas_viewer = nullptr)
      : current_room_id_(current_room_id),
        rooms_(rooms),
        canvas_viewer_(canvas_viewer) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "dungeon.sprite_editor"; }
  std::string GetDisplayName() const override { return "Sprite Editor"; }
  std::string GetIcon() const override { return ICON_MD_PERSON; }
  std::string GetEditorCategory() const override { return "Dungeon"; }
  int GetPriority() const override { return 65; }

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
    DrawSpriteSelector();
    ImGui::Separator();
    DrawRoomSprites();
  }

  // ==========================================================================
  // Panel-Specific Methods
  // ==========================================================================

  void SetCanvasViewer(DungeonCanvasViewer* viewer) {
    canvas_viewer_ = viewer;
  }

  void SetSpritePlacedCallback(
      std::function<void(const zelda3::Sprite&)> callback) {
    sprite_placed_callback_ = std::move(callback);
  }

 private:
  void DrawPlacementControls() {
    const auto& theme = AgentUI::GetTheme();
    // Placement mode indicator
    if (placement_mode_) {
      ImGui::TextColored(theme.status_warning,
          ICON_MD_PLACE " Placing: %s (0x%02X)",
          zelda3::ResolveSpriteName(selected_sprite_id_), selected_sprite_id_);
      if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
        placement_mode_ = false;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetSpritePlacementMode(false, 0);
        }
      }
    } else {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " Select a sprite to place");
    }
  }

  void DrawSpriteSelector() {
    const auto& theme = AgentUI::GetTheme();
    ImGui::Text(ICON_MD_PERSON " Select Sprite:");

    // Filter by category
    static const char* kCategories[] = {
        "All", "Enemies", "NPCs", "Bosses", "Items"
    };
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##Category", &selected_category_, kCategories, IM_ARRAYSIZE(kCategories));
    ImGui::SameLine();

    // Search filter
    ImGui::SetNextItemWidth(120);
    ImGui::InputTextWithHint("##Search", "Search...", search_filter_, sizeof(search_filter_));

    // Sprite grid with responsive sizing
    float available_height = ImGui::GetContentRegionAvail().y;
    // Reserve space for room sprites section
    float reserved_height = 120.0f;
    // Calculate grid height: at least 150px, responsive to available space
    float grid_height = std::max(150.0f, std::min(400.0f, available_height - reserved_height));

    // Responsive sprite size based on panel width
    float panel_width = ImGui::GetContentRegionAvail().x;
    float sprite_size = std::max(28.0f, std::min(40.0f, (panel_width - 40.0f) / 8.0f));
    int items_per_row = std::max(1, static_cast<int>(panel_width / (sprite_size + 6)));

    ImGui::BeginChild("##SpriteGrid", ImVec2(0, grid_height), true,
                      ImGuiWindowFlags_HorizontalScrollbar);

    int col = 0;
    for (int i = 0; i < 256; ++i) {
      // Apply filters
      if (!MatchesFilter(i)) continue;

      bool is_selected = (selected_sprite_id_ == i);

      ImGui::PushID(i);

      // Color-coded button based on sprite type using theme colors
      ImVec4 button_color = GetSpriteTypeColor(i, theme);
      if (is_selected) {
        button_color.x = std::min(1.0f, button_color.x + 0.2f);
        button_color.y = std::min(1.0f, button_color.y + 0.2f);
        button_color.z = std::min(1.0f, button_color.z + 0.2f);
      }

      ImGui::PushStyleColor(ImGuiCol_Button, button_color);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
          ImVec4(std::min(1.0f, button_color.x + 0.1f),
                 std::min(1.0f, button_color.y + 0.1f),
                 std::min(1.0f, button_color.z + 0.1f), 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
          ImVec4(std::min(1.0f, button_color.x + 0.2f),
                 std::min(1.0f, button_color.y + 0.2f),
                 std::min(1.0f, button_color.z + 0.2f), 1.0f));

      // Get category icon based on sprite type
      const char* icon = GetSpriteTypeIcon(i);
      std::string label = absl::StrFormat("%s\n%02X", icon, i);
      if (ImGui::Button(label.c_str(), ImVec2(sprite_size, sprite_size))) {
        selected_sprite_id_ = i;
        placement_mode_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetSpritePlacementMode(true, i);
        }
      }

      ImGui::PopStyleColor(3);

      if (ImGui::IsItemHovered()) {
        const char* category = GetSpriteCategoryName(i);
        ImGui::SetTooltip("%s (0x%02X)\n[%s]\nClick to select for placement",
            zelda3::ResolveSpriteName(i), i, category);
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

  void DrawRoomSprites() {
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    auto& sprites = room.GetSprites();

    // Sprite count with limit warning
    int sprite_count = static_cast<int>(sprites.size());
    ImVec4 count_color =
        sprite_count > 16 ? theme.text_error_red : theme.text_primary;
    ImGui::TextColored(count_color, ICON_MD_LIST " Room Sprites: %d/16", 
                       sprite_count);

    if (sprite_count > 16) {
      ImGui::SameLine();
      ImGui::TextColored(theme.text_warning_yellow, ICON_MD_WARNING);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Room exceeds sprite limit (16 max)!\n"
                          "This may cause game crashes.");
      }
    }

    if (sprites.empty()) {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " No sprites in this room");
      return;
    }

    // Split view: list on top, properties below
    float available = ImGui::GetContentRegionAvail().y;
    float list_height = std::max(100.0f, available * 0.4f);
    
    ImGui::BeginChild("##SpriteList", ImVec2(0, list_height), true);
    for (size_t i = 0; i < sprites.size(); ++i) {
      const auto& sprite = sprites[i];
      bool is_selected = (selected_sprite_list_index_ == static_cast<int>(i));

      ImGui::PushID(static_cast<int>(i));

      // Build display string with indicators
      std::string label = absl::StrFormat("[%02X] %s", 
          sprite.id(), zelda3::ResolveSpriteName(sprite.id()));
      
      // Add key drop indicator
      if (sprite.key_drop() == 1) {
        label += " " ICON_MD_KEY;  // Small key
      } else if (sprite.key_drop() == 2) {
        label += " " ICON_MD_VPN_KEY;  // Big key
      }
      
      // Add overlord indicator
      if (sprite.IsOverlord()) {
        label += " " ICON_MD_STAR;
      }

      if (ImGui::Selectable(label.c_str(), is_selected)) {
        selected_sprite_list_index_ = static_cast<int>(i);
      }
      
      // Show position on same line
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 80);
      ImGui::TextColored(theme.text_secondary_gray,
          "(%d,%d) L%d", sprite.x(), sprite.y(), sprite.layer());

      ImGui::PopID();
    }
    ImGui::EndChild();

    // Sprite properties panel
    DrawSpriteProperties();
  }

  void DrawSpriteProperties() {
    const auto& theme = AgentUI::GetTheme();
    auto& room = (*rooms_)[*current_room_id_];
    auto& sprites = room.GetSprites();

    if (selected_sprite_list_index_ < 0 || 
        selected_sprite_list_index_ >= static_cast<int>(sprites.size())) {
      ImGui::TextColored(theme.text_secondary_gray,
          ICON_MD_INFO " Select a sprite to edit properties");
      return;
    }

    auto& sprite = sprites[selected_sprite_list_index_];
    
    ImGui::Separator();
    ImGui::Text(ICON_MD_EDIT " Sprite Properties");

    // Overlord badge (read-only)
    if (sprite.IsOverlord()) {
      ImGui::SameLine();
      ImGui::TextColored(theme.status_warning, 
          ICON_MD_STAR " OVERLORD");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("This is an Overlord sprite.\n"
                          "Overlords have separate limits (8 max).");
      }
    }

    // ID and Name (read-only)
    ImGui::Text("ID: 0x%02X - %s", sprite.id(), 
                zelda3::ResolveSpriteName(sprite.id()));

    // Position (editable)
    int pos_x = sprite.x();
    int pos_y = sprite.y();
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputInt("X##SpriteX", &pos_x, 1, 8)) {
      pos_x = std::clamp(pos_x, 0, 63);
      // Note: Need setter in Sprite class
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputInt("Y##SpriteY", &pos_y, 1, 8)) {
      pos_y = std::clamp(pos_y, 0, 63);
      // Note: Need setter in Sprite class
    }

    // Subtype selector (0-7)
    int subtype = sprite.subtype();
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("Subtype##SpriteSubtype", &subtype, 
                     "0\0001\0002\0003\0004\0005\0006\0007\0")) {
      sprite.set_subtype(subtype);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Controls sprite behavior variant.\n"
                        "Effect varies by sprite type.");
    }

    // Layer selector
    int layer = sprite.layer();
    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("Layer##SpriteLayer", &layer, 
                     "Upper (0)\0Lower (1)\0Both (2)\0")) {
      sprite.set_layer(layer);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Which layer the sprite appears on.\n"
                        "Upper = main floor, Lower = basement.");
    }

    // Key drop selector
    int key_drop = sprite.key_drop();
    ImGui::Text("Key Drop:");
    ImGui::SameLine();
    if (ImGui::RadioButton("None##KeyNone", key_drop == 0)) {
      sprite.set_key_drop(0);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(ICON_MD_KEY " Small##KeySmall", key_drop == 1)) {
      sprite.set_key_drop(1);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(ICON_MD_VPN_KEY " Big##KeyBig", key_drop == 2)) {
      sprite.set_key_drop(2);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Key dropped when sprite is defeated.");
    }

    // Delete button
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, theme.status_error);
    if (ImGui::Button(ICON_MD_DELETE " Delete Sprite")) {
      sprites.erase(sprites.begin() + selected_sprite_list_index_);
      selected_sprite_list_index_ = -1;
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CONTENT_COPY " Duplicate")) {
      zelda3::Sprite copy = sprite;
      sprites.push_back(copy);
    }
  }

  bool MatchesFilter(int sprite_id) {
    // Category filter
    if (selected_category_ > 0) {
      // Simplified category matching - in real implementation, use proper categorization
      bool is_enemy = (sprite_id >= 0x09 && sprite_id <= 0x7F);
      bool is_npc = (sprite_id >= 0x80 && sprite_id <= 0xBF);
      bool is_boss = (sprite_id >= 0xC0 && sprite_id <= 0xD8);
      bool is_item = (sprite_id >= 0xD9 && sprite_id <= 0xFF);
      
      if (selected_category_ == 1 && !is_enemy) return false;
      if (selected_category_ == 2 && !is_npc) return false;
      if (selected_category_ == 3 && !is_boss) return false;
      if (selected_category_ == 4 && !is_item) return false;
    }
    
    // Text search filter
    if (search_filter_[0] != '\0') {
      const char* name = zelda3::ResolveSpriteName(sprite_id);
      // Simple case-insensitive substring search
      std::string name_lower = name;
      std::string filter_lower = search_filter_;
      for (auto& c : name_lower) c = static_cast<char>(tolower(c));
      for (auto& c : filter_lower) c = static_cast<char>(tolower(c));
      if (name_lower.find(filter_lower) == std::string::npos) {
        return false;
      }
    }
    
    return true;
  }

  ImVec4 GetSpriteTypeColor(int sprite_id, const AgentUITheme& theme) {
    // Color-code based on sprite type using theme colors
    if (sprite_id >= 0xC0 && sprite_id <= 0xD8) {
      return theme.status_error;              // Red for bosses
    } else if (sprite_id >= 0x80 && sprite_id <= 0xBF) {
      return theme.dungeon_sprite_layer0;     // Green for NPCs
    } else if (sprite_id >= 0xD9) {
      return theme.dungeon_object_chest;      // Gold for items
    }
    return theme.dungeon_sprite_layer1;       // Blue for enemies
  }

  const char* GetSpriteTypeIcon(int sprite_id) {
    // Return category-appropriate icons
    if (sprite_id >= 0xC0 && sprite_id <= 0xD8) {
      return ICON_MD_DANGEROUS;  // Skull for bosses
    } else if (sprite_id >= 0x80 && sprite_id <= 0xBF) {
      return ICON_MD_PERSON;  // Person for NPCs
    } else if (sprite_id >= 0xD9) {
      return ICON_MD_STAR;  // Star for items
    }
    return ICON_MD_PEST_CONTROL;  // Bug for enemies
  }

  const char* GetSpriteCategoryName(int sprite_id) {
    if (sprite_id >= 0xC0 && sprite_id <= 0xD8) {
      return "Boss";
    } else if (sprite_id >= 0x80 && sprite_id <= 0xBF) {
      return "NPC";
    } else if (sprite_id >= 0xD9) {
      return "Item";
    }
    return "Enemy";
  }

  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  
  // Selection state
  int selected_sprite_id_ = 0;
  int selected_sprite_list_index_ = -1;  // Selected sprite in room list
  int selected_category_ = 0;
  char search_filter_[64] = {0};
  bool placement_mode_ = false;
  
  std::function<void(const zelda3::Sprite&)> sprite_placed_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_
