#include "palette_editor.h"

#include <imgui/imgui.h>

#include "absl/status/status.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/canvas.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"

namespace yaze {
namespace app {
namespace editor {

absl::Status PaletteEditor::Update() {
  if (rom()->is_loaded()) {
    // Initialize the labels
    for (int i = 0; i < kNumPalettes; i++) {
      rom()->resource_label()->CreateOrGetLabel(
          "Palette Group Name", std::to_string(i),
          std::string(kPaletteGroupNames[i]));
    }
  }

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
    DisplayCategoryTable();
    ImGui::TableNextColumn();
    if (gui::SnesColorEdit4("Color Picker", current_color_,
                            ImGuiColorEditFlags_NoAlpha)) {
    }
    ImGui::EndTable();
  }

  CLEAR_AND_RETURN_STATUS(status_)

  return absl::OkStatus();
}

void PaletteEditor::DisplayCategoryTable() {
  // Check if the table is created successfully with 3 columns
  if (ImGui::BeginTable("Category Table", 3)) {  // 3 columns

    // Headers (optional, remove if you don't want headers)
    ImGui::TableSetupColumn("Weapons and Gear");
    ImGui::TableSetupColumn("World and Enemies");
    ImGui::TableSetupColumn("Maps and Items");
    ImGui::TableHeadersRow();

    // Start the first row
    ImGui::TableNextRow();

    // Column 1 - Weapons and Gear
    ImGui::TableSetColumnIndex(0);

    if (ImGui::TreeNode("Sword")) {
      status_ = DrawPaletteGroup(0);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Shield")) {
      status_ = DrawPaletteGroup(1);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Clothes")) {
      status_ = DrawPaletteGroup(2);
      ImGui::TreePop();
    }

    // Column 2 - World and Enemies
    ImGui::TableSetColumnIndex(1);
    if (ImGui::TreeNode("World Colors")) {
      status_ = DrawPaletteGroup(3);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Area Colors")) {
      status_ = DrawPaletteGroup(4);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Enemies")) {
      status_ = DrawPaletteGroup(5);
      ImGui::TreePop();
    }

    // Column 3 - Maps and Items
    ImGui::TableSetColumnIndex(2);
    if (ImGui::TreeNode("Dungeons")) {
      status_ = DrawPaletteGroup(6);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("World Map")) {
      status_ = DrawPaletteGroup(7);
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Dungeon Map")) {
      status_ = DrawPaletteGroup(8);
      ImGui::TreePop();
    }

    // Additional items in the last column, if any
    {
      if (ImGui::TreeNode("Triforce")) {
        status_ = DrawPaletteGroup(9);
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Crystal")) {
        status_ = DrawPaletteGroup(10);
        ImGui::TreePop();
      }
    }
    // End the table
    ImGui::EndTable();
  }
}

absl::Status PaletteEditor::EditColorInPalette(gfx::SnesPalette& palette,
                                               int index) {
  if (index >= palette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }

  // Get the current color
  ASSIGN_OR_RETURN(auto color, palette.GetColor(index));
  auto currentColor = color.rgb();
  if (ImGui::ColorPicker4("Color Picker", (float*)&palette[index])) {
    // The color was modified, update it in the palette
    palette(index, currentColor);
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::ResetColorToOriginal(
    gfx::SnesPalette& palette, int index,
    const gfx::SnesPalette& originalPalette) {
  if (index >= palette.size() || index >= originalPalette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }
  ASSIGN_OR_RETURN(auto color, originalPalette.GetColor(index));
  auto originalColor = color.rgb();
  palette(index, originalColor);
  return absl::OkStatus();
}

absl::Status PaletteEditor::DrawPaletteGroup(int category) {
  if (!rom()->is_loaded()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  gfx::PaletteGroup palette_group =
      *rom()->palette_group().get_group(kPaletteGroupNames[category].data());
  const auto size = palette_group.size();

  static bool edit_color = false;
  for (int j = 0; j < size; j++) {
    ImGui::Text("%d", j);
    // rom()->resource_label()->SelectableLabelWithNameEdit(
    //     false, "Palette Group Name", std::to_string(j),
    //     std::string(kPaletteGroupNames[category]));
    gfx::SnesPalette* palette = palette_group.mutable_palette(j);
    auto pal_size = palette->size();

    for (int n = 0; n < pal_size; n++) {
      ImGui::PushID(n);
      if ((n % 7) != 0) ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

      auto popup_id =
          absl::StrCat(kPaletteCategoryNames[category].data(), j, "_", n);

      // Small icon of the color in the palette
      if (gui::SnesColorButton(popup_id, *palette->mutable_color(n),
                               palette_button_flags)) {
        ASSIGN_OR_RETURN(current_color_, palette->GetColor(n));
        // EditColorInPalette(*palette, n);
      }

      if (ImGui::BeginPopupContextItem(popup_id.c_str())) {
        RETURN_IF_ERROR(HandleColorPopup(*palette, category, j, n))
      }

      // if (gui::SnesColorEdit4(popup_id.c_str(), (*palette)[n],
      //                         palette_button_flags)) {
      //   EditColorInPalette(*palette, n);
      // }

      ImGui::PopID();
    }
  }
  return absl::OkStatus();
}

namespace {
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

static inline float color_saturate(float f) {
  return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
}

#define F32_TO_INT8_SAT(_VAL)            \
  ((int)(color_saturate(_VAL) * 255.0f + \
         0.5f))  // Saturated, always output 0..255
}  // namespace

absl::Status PaletteEditor::HandleColorPopup(gfx::SnesPalette& palette, int i,
                                             int j, int n) {
  auto col = gfx::ToFloatArray(palette[n]);
  if (gui::SnesColorEdit4("Edit Color", palette[n], color_popup_flags)) {
    // TODO: Implement new update color function
  }

  if (ImGui::Button("Copy as..", ImVec2(-1, 0))) ImGui::OpenPopup("Copy");
  if (ImGui::BeginPopup("Copy")) {
    int cr = F32_TO_INT8_SAT(col[0]);
    int cg = F32_TO_INT8_SAT(col[1]);
    int cb = F32_TO_INT8_SAT(col[2]);
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

void PaletteEditor::DisplayPalette(gfx::SnesPalette& palette, bool loaded) {
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

void PaletteEditor::DrawPortablePalette(gfx::SnesPalette& palette) {
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