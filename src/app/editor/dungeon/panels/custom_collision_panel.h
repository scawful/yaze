#ifndef YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H
#define YAZE_APP_EDITOR_DUNGEON_PANELS_CUSTOM_COLLISION_PANEL_H

#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_object_interaction.h"
#include "app/editor/system/editor_panel.h"
#include "zelda3/zelda3_labels.h"
#include "app/gui/core/icons.h"
#include "absl/strings/str_format.h"

namespace yaze::editor {

class CustomCollisionPanel : public EditorPanel {
 public:
  CustomCollisionPanel(DungeonCanvasViewer* viewer, DungeonObjectInteraction* interaction)
      : viewer_(viewer), interaction_(interaction) {}

  std::string GetId() const override { return "dungeon.custom_collision"; }
  std::string GetDisplayName() const override { return "Custom Collision"; }
  std::string GetIcon() const override { return ICON_MD_GRID_ON; }
  std::string GetEditorCategory() const override { return "Dungeon"; }

  void SetCanvasViewer(DungeonCanvasViewer* viewer) { viewer_ = viewer; }
  void SetInteraction(DungeonObjectInteraction* interaction) {
    interaction_ = interaction;
  }

  void Draw(bool* p_open) override {
    (void)p_open;
    const auto& theme = AgentUI::GetTheme();

    if (!viewer_ || !viewer_->HasRooms() || !viewer_->rom() ||
        !viewer_->rom()->is_loaded()) {
      ImGui::TextDisabled(ICON_MD_INFO " No dungeon rooms loaded.");
      return;
    }

    auto* rooms = viewer_->rooms();
    int room_id = viewer_->current_room_id();
    if (room_id < 0 || room_id >= 296) {
      ImGui::TextDisabled(ICON_MD_INFO " Invalid room ID.");
      return;
    }

    auto& room = (*rooms)[room_id];
    bool has_collision = room.has_custom_collision();

    if (ImGui::Button(has_collision ? "Disable Custom Collision" : "Enable Custom Collision")) {
        room.set_has_custom_collision(!has_collision);
        viewer_->set_show_custom_collision_overlay(room.has_custom_collision());
    }

    ImGui::Separator();

    if (has_collision) {
        bool show_overlay = viewer_->show_custom_collision_overlay();
        if (ImGui::Checkbox("Show Collision Overlay", &show_overlay)) {
            viewer_->set_show_custom_collision_overlay(show_overlay);
        }

        if (!interaction_) {
          ImGui::TextDisabled("Painting requires an active interaction context.");
          return;
        }

        bool is_painting = (interaction_->mode_manager().GetMode() ==
                            InteractionMode::PaintCollision);
        if (ImGui::Checkbox("Paint Mode", &is_painting)) {
            if (is_painting) {
                interaction_->mode_manager().SetMode(InteractionMode::PaintCollision);
            } else {
                interaction_->mode_manager().SetMode(InteractionMode::Select);
            }
        }

        if (is_painting) {
            ImGui::TextColored(theme.text_warning_yellow,
                               "Click/Drag on canvas to paint");
            
            auto& state = interaction_->mode_manager().GetModeState();
            int current_val = state.paint_collision_value;
            
            const auto& tile_types = zelda3::Zelda3Labels::GetTileTypeNames();
            
            if (ImGui::BeginCombo("Collision Type", absl::StrFormat("%02X: %s", current_val, (current_val < tile_types.size() ? tile_types[current_val] : "Unknown")).c_str())) {
                for (int i = 0; i < tile_types.size(); ++i) {
                    bool selected = (current_val == i);
                    if (ImGui::Selectable(absl::StrFormat("%02X: %s", i, tile_types[i]).c_str(), selected)) {
                        state.paint_collision_value = static_cast<uint8_t>(i);
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Separator();
            ImGui::Text("Quick Select:");
            auto quick_button = [&](const char* label, uint8_t val) {
              if (ImGui::Button(label)) {
                state.paint_collision_value = val;
              }
            };
            quick_button("Floor (00)", 0x00); ImGui::SameLine();
            quick_button("Solid (02)", 0x02); ImGui::SameLine();
            quick_button("D.Water (08)", 0x08);
            quick_button("S.Water (09)", 0x09); ImGui::SameLine();
            quick_button("Pit (1B)", 0x1B); ImGui::SameLine();
            quick_button("Spikes (0E)", 0x0E);
        }
        
        ImGui::Separator();
        if (ImGui::Button("Clear All Custom Collision")) {
            room.custom_collision().tiles.fill(0);
            // Clearing should remove the override (room falls back to vanilla).
            room.custom_collision().has_data = false;
            room.MarkCustomCollisionDirty();
        }
    } else {
        ImGui::TextWrapped("Custom collision allows you to override the physics of individual 8x8 tiles in the room. This is useful for creating water, pits, or other effects that don't match the background tiles.");
    }
  }

 private:
  DungeonCanvasViewer* viewer_;
  DungeonObjectInteraction* interaction_;
};

} // namespace yaze::editor

#endif
