#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

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
    // Placement mode indicator
    if (placement_mode_) {
      ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
          ICON_MD_PLACE " Placing: %s (0x%02X)",
          zelda3::ResolveSpriteName(selected_sprite_id_), selected_sprite_id_);
      if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
        placement_mode_ = false;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetSpritePlacementMode(false, 0);
        }
      }
    } else {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          ICON_MD_INFO " Select a sprite to place");
    }
  }

  void DrawSpriteSelector() {
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
    
    // Sprite grid
    constexpr float kPreviewSize = 32.0f;
    float panel_width = ImGui::GetContentRegionAvail().x;
    int items_per_row = std::max(1, static_cast<int>(panel_width / (kPreviewSize + 8)));
    
    ImGui::BeginChild("##SpriteGrid", ImVec2(0, 200), true, 
                      ImGuiWindowFlags_HorizontalScrollbar);
    
    int col = 0;
    for (int i = 0; i < 256; ++i) {
      // Apply filters
      if (!MatchesFilter(i)) continue;
      
      bool is_selected = (selected_sprite_id_ == i);
      
      ImGui::PushID(i);
      
      // Color-coded button based on sprite type
      ImVec4 button_color = GetSpriteTypeColor(i);
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
      
      std::string label = absl::StrFormat("%02X", i);
      if (ImGui::Button(label.c_str(), ImVec2(kPreviewSize, kPreviewSize))) {
        selected_sprite_id_ = i;
        placement_mode_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetSpritePlacementMode(true, i);
        }
      }
      
      ImGui::PopStyleColor(3);
      
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s (0x%02X)\nClick to select for placement",
            zelda3::ResolveSpriteName(i), i);
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

  void DrawRoomSprites() {
    auto& room = (*rooms_)[*current_room_id_];
    const auto& sprites = room.GetSprites();
    
    ImGui::Text(ICON_MD_LIST " Room Sprites (%zu):", sprites.size());
    
    if (sprites.empty()) {
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
          ICON_MD_INFO " No sprites in this room");
      return;
    }
    
    ImGui::BeginChild("##SpriteList", ImVec2(0, 120), true);
    for (size_t i = 0; i < sprites.size(); ++i) {
      const auto& sprite = sprites[i];
      
      ImGui::PushID(static_cast<int>(i));
      
      ImGui::Text("[%zu] %s (0x%02X)", i, 
          zelda3::ResolveSpriteName(sprite.id()), sprite.id());
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
          "@ (%d,%d) L%d", sprite.x(), sprite.y(), sprite.layer());
      
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_DELETE "##Del")) {
        auto& mutable_room = (*rooms_)[*current_room_id_];
        mutable_room.GetSprites().erase(
            mutable_room.GetSprites().begin() + static_cast<long>(i));
      }
      
      ImGui::PopID();
    }
    ImGui::EndChild();
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

  ImVec4 GetSpriteTypeColor(int sprite_id) {
    // Color-code based on sprite type
    if (sprite_id >= 0xC0 && sprite_id <= 0xD8) {
      return ImVec4(0.7f, 0.3f, 0.3f, 1.0f);  // Red for bosses
    } else if (sprite_id >= 0x80 && sprite_id <= 0xBF) {
      return ImVec4(0.3f, 0.7f, 0.3f, 1.0f);  // Green for NPCs
    } else if (sprite_id >= 0xD9) {
      return ImVec4(0.7f, 0.7f, 0.3f, 1.0f);  // Yellow for items
    }
    return ImVec4(0.3f, 0.5f, 0.7f, 1.0f);  // Blue for enemies
  }

  int* current_room_id_ = nullptr;
  std::array<zelda3::Room, 0x128>* rooms_ = nullptr;
  DungeonCanvasViewer* canvas_viewer_ = nullptr;
  
  // Selection state
  int selected_sprite_id_ = 0;
  int selected_category_ = 0;
  char search_filter_[64] = {0};
  bool placement_mode_ = false;
  
  std::function<void(const zelda3::Sprite&)> sprite_placed_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_DUNGEON_PANELS_SPRITE_EDITOR_PANEL_H_

