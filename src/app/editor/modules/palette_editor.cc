#include "palette_editor.h"

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"

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
  if (buf == nullptr) return w;
  if (w == -1 || w >= (int)buf_size) w = (int)buf_size - 1;
  buf[w] = 0;
  return w;
}

namespace yaze {
namespace app {
namespace editor {

absl::Status PaletteEditor::Update() {
  if (ImGui::BeginTable("paletteEditorTable", 2,
                        ImGuiTableFlags_Reorderable |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchSame,
                        ImVec2(0, 0))) {
    ImGui::TableSetupColumn("Palette Groups",
                            ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch,
                            ImGui::GetContentRegionAvail().x);
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    for (int category = 0; category < kNumPalettes; ++category) {
      if (ImGui::TreeNode(kPaletteCategoryNames[category].data())) {
        status_ = DrawPaletteGroup(category);
        ImGui::TreePop();
      }
    }
    ImGui::TableNextColumn();
    ImGui::Text("Test Column");
    ImGui::EndTable();
  }

  CLEAR_AND_RETURN_STATUS(status_)

  return absl::OkStatus();
}

void PaletteEditor::EditColorInPalette(gfx::SNESPalette& palette, int index) {
  if (index >= palette.size()) {
    // Handle error: the index is out of bounds
    return;
  }

  // Get the current color
  auto currentColor = palette.GetColor(index).GetRGB();
  if (ImGui::ColorPicker4("Color Picker", (float*)&palette[index])) {
    // The color was modified, update it in the palette
    palette(index, currentColor);
  }
}

void PaletteEditor::ResetColorToOriginal(
    gfx::SNESPalette& palette, int index,
    const gfx::SNESPalette& originalPalette) {
  if (index >= palette.size() || index >= originalPalette.size()) {
    // Handle error: the index is out of bounds
    return;
  }

  auto originalColor = originalPalette.GetColor(index).GetRGB();
  palette(index, originalColor);
}

absl::Status PaletteEditor::DrawPaletteGroup(int category) {
  if (!rom()->isLoaded()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  const auto size =
      rom()->palette_group(kPaletteGroupNames[category].data()).size();
  auto palettes = rom()->palette_group(kPaletteGroupNames[category].data());
  static bool edit_color = false;
  for (int j = 0; j < size; j++) {
    ImGui::Text("%d", j);

    auto palette = palettes[j];
    auto pal_size = palette.size();

    for (int n = 0; n < pal_size; n++) {
      ImGui::PushID(n);
      if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      auto popup_id =
          absl::StrCat(kPaletteCategoryNames[category].data(), j, "_", n);

      // Small icon of the color in the palette
      if (gui::SNESColorButton(popup_id, palette[n], palette_button_flags)) {
        edit_color = true;
      }

      if (ImGui::BeginPopupContextItem(popup_id.c_str())) {
        RETURN_IF_ERROR(HandleColorPopup(palette, category, j, n))
      }

      if (edit_color) {
        // The color button was clicked, open the popup
        if (ImGui::ColorEdit4(popup_id.c_str(),
                              gfx::ToFloatArray(palette[n]).data(),
                              palette_button_flags)) {
          EditColorInPalette(palette, n);
        }
      }

      ImGui::PopID();
    }
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::HandleColorPopup(gfx::SNESPalette& palette, int i,
                                             int j, int n) {
  auto col = gfx::ToFloatArray(palette[n]);
  if (ImGui::ColorEdit4("Edit Color", col.data(), color_popup_flags)) {
    RETURN_IF_ERROR(rom()->UpdatePaletteColor(kPaletteGroupNames[i].data(), j,
                                              n, palette[n]))
  }

  if (ImGui::Button("Copy as..", ImVec2(-1, 0))) ImGui::OpenPopup("Copy");

  if (ImGui::BeginPopup("Copy")) {
    int cr = IM_F32_TO_INT8_SAT(col[0]);
    int cg = IM_F32_TO_INT8_SAT(col[1]);
    int cb = IM_F32_TO_INT8_SAT(col[2]);
    char buf[64];

    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff)", col[0],
                       col[1], col[2]);

    if (ImGui::Selectable(buf)) ImGui::SetClipboardText(buf);
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d)", cr, cg, cb);
    if (ImGui::Selectable(buf)) ImGui::SetClipboardText(buf);
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg, cb);
    if (ImGui::Selectable(buf)) ImGui::SetClipboardText(buf);
    ImGui::EndPopup();
  }

  ImGui::EndPopup();
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
    InitializeSavedPalette(palette);
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

    if (ImGui::Button("Update Map Palette")) {
    }

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

    // List of Colors in Overworld Palette
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

void PaletteEditor::DrawPortablePalette(gfx::SNESPalette& palette) {
  static bool init = false;
  if (!init) {
    InitializeSavedPalette(palette);
    init = true;
  }

  if (ImGuiID child_id = ImGui::GetID((void*)(intptr_t)100);
      ImGui::BeginChild(child_id, ImGui::GetContentRegionAvail(), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    ImGui::BeginGroup();  // Lock X position
    ImGui::Text("Palette");
    for (int n = 0; n < IM_ARRAYSIZE(saved_palette_); n++) {
      ImGui::PushID(n);
      if ((n % 8) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      if (ImGui::ColorButton("##palette", saved_palette_[n],
                             palette_button_flags_2, ImVec2(20, 20)))
        ImVec4(saved_palette_[n].x, saved_palette_[n].y, saved_palette_[n].z,
               1.0f);  // Preserve alpha!

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
  }
  ImGui::EndChild();
}

}  // namespace editor
}  // namespace app
}  // namespace yaze