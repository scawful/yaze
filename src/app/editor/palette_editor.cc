#include "palette_editor.h"

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "gui/canvas.h"
#include "gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status PaletteEditor::Update() {
  for (int i = 0; i < 11; ++i) {
    if (ImGui::TreeNode(kPaletteCategoryNames[i].data())) {
      auto size = rom_.GetPaletteGroup(kPaletteGroupNames[i].data()).size;
      auto palettes = rom_.GetPaletteGroup(kPaletteGroupNames[i].data());
      for (int j = 0; j < size; j++) {
        ImGui::Text("%d", j);
        auto palette = palettes[j];
        for (int n = 0; n < size; n++) {
          ImGui::PushID(n);
          if ((n % 8) != 0)
            ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

          ImGuiColorEditFlags palette_button_flags =
              ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker;
          if (ImGui::ColorButton("##palette", palette[n].RGB(),
                                 palette_button_flags, ImVec2(20, 20)))
            current_color_ =
                ImVec4(palette[n].rgb.x, palette[n].rgb.y, palette[n].rgb.z,
                       current_color_.w);  // Preserve alpha!

          ImGui::PopID();
        }
      }
      ImGui::TreePop();
    }
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::DisplayPalette(gfx::SNESPalette& palette,
                                           bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  static ImVec4 saved_palette[256] = {};
  if (loaded && !init) {
    for (int n = 0; n < palette.size_; n++) {
      saved_palette[n].x = palette.GetColor(n).rgb.x / 255;
      saved_palette[n].y = palette.GetColor(n).rgb.y / 255;
      saved_palette[n].z = palette.GetColor(n).rgb.z / 255;
      saved_palette[n].w = 255;  // Alpha
    }
    init = true;
  }

  static ImVec4 backup_color;
  bool open_popup = ImGui::ColorButton("MyColor##3b", color, misc_flags);
  ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
  open_popup |= ImGui::Button("Palette");
  if (open_popup) {
    ImGui::OpenPopup("mypicker");
    backup_color = color;
  }
  if (ImGui::BeginPopup("mypicker")) {
    ImGui::Text("Current Overworld Palette");
    ImGui::Separator();
    ImGui::ColorPicker4("##picker", (float*)&color,
                        misc_flags | ImGuiColorEditFlags_NoSidePreview |
                            ImGuiColorEditFlags_NoSmallPreview);
    ImGui::SameLine();

    ImGui::BeginGroup();  // Lock X position
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
    ImGui::Text("Palette");
    for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++) {
      ImGui::PushID(n);
      if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha |
                                                 ImGuiColorEditFlags_NoPicker |
                                                 ImGuiColorEditFlags_NoTooltip;
      if (ImGui::ColorButton("##palette", saved_palette[n],
                             palette_button_flags, ImVec2(20, 20)))
        color = ImVec4(saved_palette[n].x, saved_palette[n].y,
                       saved_palette[n].z, color.w);  // Preserve alpha!

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
    ImGui::EndPopup();
  }
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze