#include "dungeon_settings_panel.h"

#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/room_layer_manager.h"

namespace yaze::editor {

void DungeonSettingsPanel::DrawLayerCompositingControls(int room_id) {
  if (!viewer_) {
    ImGui::TextDisabled("No viewer available");
    return;
  }

  auto& layer_manager = viewer_->GetRoomLayerManager(room_id);

  // Helper to draw a combo box for blend mode selection
  auto DrawBlendModeCombo = [&](const char* label,
                                zelda3::LayerType layer_type) {
    zelda3::LayerBlendMode current_mode =
        layer_manager.GetLayerBlendMode(layer_type);
    const char* current_name =
        zelda3::RoomLayerManager::GetBlendModeName(current_mode);

    if (ImGui::BeginCombo(label, current_name)) {
      // Iterate through all blend modes
      for (int mode_int = 0; mode_int <= 4; ++mode_int) {
        zelda3::LayerBlendMode mode =
            static_cast<zelda3::LayerBlendMode>(mode_int);
        const char* mode_name = zelda3::RoomLayerManager::GetBlendModeName(mode);
        bool is_selected = (current_mode == mode);

        if (ImGui::Selectable(mode_name, is_selected)) {
          layer_manager.SetLayerBlendMode(layer_type, mode);
        }

        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
  };

  // Draw combo boxes for each layer
  ImGui::Text("BG1 Layers:");
  ImGui::Indent();
  DrawBlendModeCombo("BG1 Layout##blend", zelda3::LayerType::BG1_Layout);
  DrawBlendModeCombo("BG1 Objects##blend", zelda3::LayerType::BG1_Objects);
  ImGui::Unindent();

  ImGui::Spacing();

  ImGui::Text("BG2 Layers:");
  ImGui::Indent();
  DrawBlendModeCombo("BG2 Layout##blend", zelda3::LayerType::BG2_Layout);
  DrawBlendModeCombo("BG2 Objects##blend", zelda3::LayerType::BG2_Objects);
  ImGui::Unindent();

  ImGui::Spacing();

  // Reset to Default button
  if (ImGui::Button(ICON_MD_REFRESH " Reset to Default")) {
    // Reset all layers to Normal blend mode
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG1_Layout,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG1_Objects,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                    zelda3::LayerBlendMode::Normal);
    layer_manager.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                    zelda3::LayerBlendMode::Normal);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reset all layers to Normal blend mode");
  }
}

}  // namespace yaze::editor
