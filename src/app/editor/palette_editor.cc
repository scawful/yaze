#include "palette_editor.h"

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "gui/canvas.h"
#include "gui/icons.h"

static inline float ImSaturate(float f) {
  return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
}

#define IM_F32_TO_INT8_SAT(_VAL) \
  ((int)(ImSaturate(_VAL) * 255.0f + 0.5f))  // Saturated, always output 0..255

int CustomFormatString(char* buf, size_t buf_size, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
#ifdef IMGUI_USE_STB_SPRINTF
  int w = stbsp_vsnprintf(buf, (int)buf_size, fmt, args);
#else
  int w = vsnprintf(buf, buf_size, fmt, args);
#endif
  va_end(args);
  if (buf == NULL) return w;
  if (w == -1 || w >= (int)buf_size) w = (int)buf_size - 1;
  buf[w] = 0;
  return w;
}

namespace yaze {
namespace app {
namespace editor {

namespace {
void DrawPaletteTooltips(gfx::SNESPalette& palette, int size) {}

using namespace ImGui;

}  // namespace

void PaletteEditor::DrawPaletteGroup(int i) {
  const int palettesPerRow = 4;
  ImGui::BeginTable("palette_table", palettesPerRow,
                    ImGuiTableFlags_BordersOuter | ImGuiTableFlags_Resizable);

  auto size = rom_.GetPaletteGroup(kPaletteGroupNames[i].data()).size();
  auto palettes = rom_.GetPaletteGroup(kPaletteGroupNames[i].data());
  for (int j = 0; j < size; j++) {
    if (j % palettesPerRow == 0) {
      ImGui::TableNextRow();
    }
    ImGui::TableSetColumnIndex(j % palettesPerRow);
    ImGui::Text("%d", j);

    auto palette = palettes[j];
    auto pal_size = palette.size_;

    for (int n = 0; n < pal_size; n++) {
      ImGui::PushID(n);
      if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      std::string popupId = kPaletteCategoryNames[i].data() +
                            std::to_string(j) + "_" + std::to_string(n);
      if (ImGui::ColorButton(popupId.c_str(), palette[n].RGB(),
                             palette_button_flags)) {
        if (ImGui::ColorEdit4(popupId.c_str(), palette[n].ToFloatArray(),
                              palette_button_flags))
          current_color_ =
              ImVec4(palette[n].rgb.x, palette[n].rgb.y, palette[n].rgb.z,
                     palette[n].rgb.w);  // Preserve alpha!
      }

      if (ImGui::BeginPopupContextItem(popupId.c_str())) {
        auto col = palette[n].ToFloatArray();
        if (ImGui::ColorEdit4(
                "Edit Color", col,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha)) {
          // palette[n].rgb.x = current_color_rgba.x;
          // palette[n].rgb.y = current_color_rgba.y;
          // palette[n].rgb.z = current_color_rgba.z;
          // rom_.UpdatePaletteColor(kPaletteGroupNames[groupIndex].data(), j,
          // n, palette[n]);
        }
        if (Button("Copy as..", ImVec2(-1, 0))) OpenPopup("Copy");
        if (BeginPopup("Copy")) {
          int cr = IM_F32_TO_INT8_SAT(col[0]);
          int cg = IM_F32_TO_INT8_SAT(col[1]);
          int cb = IM_F32_TO_INT8_SAT(col[2]);
          char buf[64];
          CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff)",
                             col[0], col[1], col[2]);
          if (Selectable(buf)) SetClipboardText(buf);
          CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d)", cr, cg, cb);
          if (Selectable(buf)) SetClipboardText(buf);
          CustomFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg,
                             cb);
          if (Selectable(buf)) SetClipboardText(buf);
          EndPopup();
        }

        ImGui::EndPopup();
      }

      ImGui::PopID();
    }
  }

  ImGui::EndTable();
}

absl::Status PaletteEditor::Update() {
  for (int i = 0; i < kNumPalettes; ++i) {
    if (ImGui::TreeNode(kPaletteCategoryNames[i].data())) {
      DrawPaletteGroup(i);
      ImGui::TreePop();
    }
  }
  return absl::OkStatus();
}

void PaletteEditor::DisplayPalette(gfx::SNESPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  if (loaded && !init) {
    for (int n = 0; n < palette.size_; n++) {
      saved_palette_[n].x = palette.GetColor(n).rgb.x / 255;
      saved_palette_[n].y = palette.GetColor(n).rgb.y / 255;
      saved_palette_[n].z = palette.GetColor(n).rgb.z / 255;
      saved_palette_[n].w = 255;  // Alpha
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
    TEXT_WITH_SEPARATOR("Current Overworld Palette");
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
    for (int n = 0; n < IM_ARRAYSIZE(saved_palette_); n++) {
      ImGui::PushID(n);
      if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      if (ImGui::ColorButton("##palette", saved_palette_[n],
                             palette_button_flags_2, ImVec2(20, 20)))
        color = ImVec4(saved_palette_[n].x, saved_palette_[n].y,
                       saved_palette_[n].z, color.w);  // Preserve alpha!

      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
          memcpy((float*)&saved_palette_[n], payload->Data, sizeof(float) * 3);
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
          memcpy((float*)&saved_palette_[n], payload->Data, sizeof(float) * 4);
        ImGui::EndDragDropTarget();
      }

      ImGui::PopID();
    }
    ImGui::EndGroup();
    ImGui::EndPopup();
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze