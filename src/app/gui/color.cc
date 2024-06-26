#include "color.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {
namespace gui {

ImVec4 ConvertSNESColorToImVec4(const SnesColor& color) {
  return ImVec4(static_cast<float>(color.rgb().x) / 255.0f,
                static_cast<float>(color.rgb().y) / 255.0f,
                static_cast<float>(color.rgb().z) / 255.0f,
                1.0f  // Assuming alpha is always fully opaque for SNES colors,
                      // adjust if necessary
  );
}

IMGUI_API bool SnesColorButton(absl::string_view id, SnesColor& color,
                               ImGuiColorEditFlags flags,
                               const ImVec2& size_arg) {
  // Convert the SNES color values to ImGui color values (normalized to 0-1
  // range)
  ImVec4 displayColor = ConvertSNESColorToImVec4(color);

  // Call the original ImGui::ColorButton with the converted color
  bool pressed = ImGui::ColorButton(id.data(), displayColor, flags, size_arg);

  return pressed;
}

IMGUI_API bool SnesColorEdit4(absl::string_view label, SnesColor& color,
                              ImGuiColorEditFlags flags) {
  // Convert the SNES color values to ImGui color values (normalized to 0-1
  // range)
  ImVec4 displayColor = ConvertSNESColorToImVec4(color);

  // Call the original ImGui::ColorEdit4 with the converted color
  bool pressed = ImGui::ColorEdit4(label.data(), (float*)&displayColor, flags);

  // Convert the ImGui color values back to SNES color values (normalized to
  // 0-255 range)
  color = SnesColor(static_cast<uint8_t>(displayColor.x * 255.0f),
                    static_cast<uint8_t>(displayColor.y * 255.0f),
                    static_cast<uint8_t>(displayColor.z * 255.0f));

  return pressed;
}

absl::Status DisplayPalette(app::gfx::SnesPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  static ImVec4 saved_palette[32] = {};
  if (loaded && !init) {
    for (int n = 0; n < palette.size(); n++) {
      ASSIGN_OR_RETURN(auto color, palette.GetColor(n));
      saved_palette[n].x = color.rgb().x / 255;
      saved_palette[n].y = color.rgb().y / 255;
      saved_palette[n].z = color.rgb().z / 255;
      saved_palette[n].w = 255;  // Alpha
    }
    init = true;
  }

  static ImVec4 backup_color;
  ImGui::Text("Current ==>");
  ImGui::SameLine();
  ImGui::Text("Previous");

  ImGui::ColorButton(
      "##current", color,
      ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
      ImVec2(60, 40));
  ImGui::SameLine();

  if (ImGui::ColorButton(
          "##previous", backup_color,
          ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
          ImVec2(60, 40)))
    color = backup_color;
  ImGui::Separator();

  ImGui::BeginGroup();  // Lock X position
  ImGui::Text("Palette");
  for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++) {
    ImGui::PushID(n);
    if ((n % 4) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

    ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha |
                                               ImGuiColorEditFlags_NoPicker |
                                               ImGuiColorEditFlags_NoTooltip;
    if (ImGui::ColorButton("##palette", saved_palette[n], palette_button_flags,
                           ImVec2(20, 20)))
      color = ImVec4(saved_palette[n].x, saved_palette[n].y, saved_palette[n].z,
                     color.w);  // Preserve alpha!

    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 3);
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 4);
      ImGui::EndDragDropTarget();
    }

    ImGui::PopID();
  }
  ImGui::EndGroup();
  ImGui::SameLine();

  ImGui::ColorPicker4("##picker", (float*)&color,
                      misc_flags | ImGuiColorEditFlags_NoSidePreview |
                          ImGuiColorEditFlags_NoSmallPreview);
}

}  // namespace gui
}  // namespace app
}  // namespace yaze