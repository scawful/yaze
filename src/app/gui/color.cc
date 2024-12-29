#include "color.h"

#include "imgui/imgui.h"

#include <cmath>
#include <string>

#include "app/gfx/bitmap.h"
#include "app/gfx/snes_palette.h"
#include "app/gfx/snes_color.h"

namespace yaze {
namespace gui {

ImVec4 ConvertSnesColorToImVec4(const SnesColor& color) {
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
  // Convert the SNES color values to ImGui color values
  ImVec4 displayColor = ConvertSnesColorToImVec4(color);

  // Call the original ImGui::ColorButton with the converted color
  bool pressed = ImGui::ColorButton(id.data(), displayColor, flags, size_arg);
  // Add the SNES color representation to the tooltip
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("SNES: $%04X", color.snes());
    ImGui::EndTooltip();
  }
  return pressed;
}

IMGUI_API bool SnesColorEdit4(absl::string_view label, SnesColor* color,
                              ImGuiColorEditFlags flags) {
  ImVec4 displayColor = ConvertSnesColorToImVec4(*color);

  // Call the original ImGui::ColorEdit4 with the converted color
  bool pressed =
      ImGui::ColorEdit4(label.data(), (float*)&displayColor.x, flags);

  color->set_rgb(displayColor);
  color->set_snes(gfx::ConvertRgbToSnes(displayColor));

  return pressed;
}

absl::Status DisplayPalette(gfx::SnesPalette& palette, bool loaded) {
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
  return absl::OkStatus();
}

void SelectablePalettePipeline(uint64_t& palette_id, bool& refresh_graphics,
                               gfx::SnesPalette& palette) {
  const auto palette_row_size = 7;
  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)100);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    ImGui::BeginGroup();  // Lock X position
    ImGui::Text("Palette");
    for (int n = 0; n < palette.size(); n++) {
      ImGui::PushID(n);
      if ((n % palette_row_size) != 0)
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      // Check if the current row is selected
      bool is_selected = (palette_id == n / palette_row_size);

      // Add outline rectangle to the selected row
      if (is_selected) {
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
      }

      if (gui::SnesColorButton("##palette", palette[n],
                               ImGuiColorEditFlags_NoAlpha |
                                   ImGuiColorEditFlags_NoPicker |
                                   ImGuiColorEditFlags_NoTooltip,
                               ImVec2(20, 20))) {
        palette_id = n / palette_row_size;
        refresh_graphics = true;
      }

      if (is_selected) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
      }

      ImGui::PopID();
    }
    ImGui::EndGroup();
  }
  ImGui::EndChild();
}

}  // namespace gui

}  // namespace yaze
