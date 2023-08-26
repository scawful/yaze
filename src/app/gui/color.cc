#include "color.h"

#include <imgui/imgui.h>

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"

namespace yaze {
namespace app {

namespace gui {

void DisplayPalette(app::gfx::SNESPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  static ImVec4 saved_palette[32] = {};
  if (loaded && !init) {
    for (int n = 0; n < palette.size(); n++) {
      saved_palette[n].x = palette.GetColor(n).GetRGB().x / 255;
      saved_palette[n].y = palette.GetColor(n).GetRGB().y / 255;
      saved_palette[n].z = palette.GetColor(n).GetRGB().z / 255;
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