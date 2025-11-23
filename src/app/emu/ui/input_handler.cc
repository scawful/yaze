#include "app/emu/ui/input_handler.h"

#include "app/gui/core/icons.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui.h"

namespace yaze {
namespace emu {
namespace ui {

void RenderKeyboardConfig(input::InputManager* manager) {
  if (!manager || !manager->backend())
    return;

  auto config = manager->GetConfig();

  ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                     ICON_MD_INFO " Keyboard Configuration");
  ImGui::Separator();

  ImGui::Text("Backend: %s", manager->backend()->GetBackendName().c_str());
  ImGui::Separator();

  ImGui::TextWrapped(
      "Configure keyboard bindings for SNES controller emulation. "
      "Click a button and press a key to rebind.");
  ImGui::Spacing();

  auto RenderKeyBind = [&](const char* label, int* key) {
    ImGui::Text("%s:", label);
    ImGui::SameLine(150);

    // Show current key
    const char* key_name = SDL_GetKeyName(*key);
    ImGui::PushID(label);
    if (ImGui::Button(key_name, ImVec2(120, 0))) {
      ImGui::OpenPopup("Rebind");
    }

    if (ImGui::BeginPopup("Rebind")) {
      ImGui::Text("Press any key...");
      ImGui::Separator();

      // Poll for key press (cross-version compatible)
      SDL_Event event;
      if (SDL_PollEvent(&event) &&
          event.type == platform::kEventKeyDown) {
        SDL_Keycode keycode = platform::GetKeyFromEvent(event);
        if (keycode != SDLK_UNKNOWN && keycode != SDLK_ESCAPE) {
          *key = keycode;
          ImGui::CloseCurrentPopup();
        }
      }

      if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
        ImGui::CloseCurrentPopup();
      }

      ImGui::EndPopup();
    }
    ImGui::PopID();
  };

  // Face Buttons
  if (ImGui::CollapsingHeader("Face Buttons", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderKeyBind("A Button", &config.key_a);
    RenderKeyBind("B Button", &config.key_b);
    RenderKeyBind("X Button", &config.key_x);
    RenderKeyBind("Y Button", &config.key_y);
  }

  // D-Pad
  if (ImGui::CollapsingHeader("D-Pad", ImGuiTreeNodeFlags_DefaultOpen)) {
    RenderKeyBind("Up", &config.key_up);
    RenderKeyBind("Down", &config.key_down);
    RenderKeyBind("Left", &config.key_left);
    RenderKeyBind("Right", &config.key_right);
  }

  // Shoulder Buttons
  if (ImGui::CollapsingHeader("Shoulder Buttons")) {
    RenderKeyBind("L Button", &config.key_l);
    RenderKeyBind("R Button", &config.key_r);
  }

  // Start/Select
  if (ImGui::CollapsingHeader("Start/Select")) {
    RenderKeyBind("Start", &config.key_start);
    RenderKeyBind("Select", &config.key_select);
  }

  ImGui::Spacing();
  ImGui::Separator();

  // Input mode
  ImGui::Checkbox("Continuous Polling", &config.continuous_polling);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Recommended: ON for games (detects held buttons)\nOFF for event-based "
        "input");
  }

  ImGui::Spacing();

  // Apply button
  if (ImGui::Button("Apply Changes", ImVec2(-1, 30))) {
    manager->SetConfig(config);
  }

  ImGui::Spacing();

  // Defaults
  if (ImGui::Button("Reset to Defaults", ImVec2(-1, 30))) {
    config = input::InputConfig();  // Reset to defaults
    manager->SetConfig(config);
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Default Bindings:");
  ImGui::BulletText("A/B/X/Y: X/Z/S/A");
  ImGui::BulletText("L/R: D/C");
  ImGui::BulletText("D-Pad: Arrow Keys");
  ImGui::BulletText("Start/Select: Enter/RShift");
}

}  // namespace ui
}  // namespace emu
}  // namespace yaze
