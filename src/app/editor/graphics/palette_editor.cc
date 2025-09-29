#include "palette_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/core/performance_monitor.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

using ImGui::AcceptDragDropPayload;
using ImGui::BeginChild;
using ImGui::BeginDragDropTarget;
using ImGui::BeginGroup;
using ImGui::BeginPopup;
using ImGui::BeginPopupContextItem;
using ImGui::BeginTable;
using ImGui::Button;
using ImGui::ColorButton;
using ImGui::ColorPicker4;
using ImGui::EndChild;
using ImGui::EndDragDropTarget;
using ImGui::EndGroup;
using ImGui::EndPopup;
using ImGui::EndTable;
using ImGui::GetContentRegionAvail;
using ImGui::GetStyle;
using ImGui::OpenPopup;
using ImGui::PopID;
using ImGui::PushID;
using ImGui::SameLine;
using ImGui::Selectable;
using ImGui::Separator;
using ImGui::SetClipboardText;
using ImGui::TableHeadersRow;
using ImGui::TableNextColumn;
using ImGui::TableNextRow;
using ImGui::TableSetupColumn;
using ImGui::Text;

using namespace gfx;

constexpr ImGuiTableFlags kPaletteTableFlags =
    ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable |
    ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Hideable;

constexpr ImGuiColorEditFlags kPalNoAlpha = ImGuiColorEditFlags_NoAlpha;

constexpr ImGuiColorEditFlags kPalButtonFlags = ImGuiColorEditFlags_NoAlpha |
                                                ImGuiColorEditFlags_NoPicker |
                                                ImGuiColorEditFlags_NoTooltip;

constexpr ImGuiColorEditFlags kColorPopupFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha |
    ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
    ImGuiColorEditFlags_DisplayHex;

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

absl::Status DisplayPalette(gfx::SnesPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  static ImVec4 current_palette[256] = {};
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  if (loaded && !init) {
    for (int n = 0; n < palette.size(); n++) {
      auto color = palette[n];
      current_palette[n].x = color.rgb().x / 255;
      current_palette[n].y = color.rgb().y / 255;
      current_palette[n].z = color.rgb().z / 255;
      current_palette[n].w = 255;  // Alpha
    }
    init = true;
  }

  static ImVec4 backup_color;
  bool open_popup = ColorButton("MyColor##3b", color, misc_flags);
  SameLine(0, GetStyle().ItemInnerSpacing.x);
  open_popup |= Button("Palette");
  if (open_popup) {
    OpenPopup("mypicker");
    backup_color = color;
  }

  if (BeginPopup("mypicker")) {
    TEXT_WITH_SEPARATOR("Current Overworld Palette");
    ColorPicker4("##picker", (float*)&color,
                 misc_flags | ImGuiColorEditFlags_NoSidePreview |
                     ImGuiColorEditFlags_NoSmallPreview);
    SameLine();

    BeginGroup();  // Lock X position
    Text("Current ==>");
    SameLine();
    Text("Previous");

    if (Button("Update Map Palette")) {
    }

    ColorButton(
        "##current", color,
        ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
        ImVec2(60, 40));
    SameLine();

    if (ColorButton(
            "##previous", backup_color,
            ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_AlphaPreviewHalf,
            ImVec2(60, 40)))
      color = backup_color;

    // List of Colors in Overworld Palette
    Separator();
    Text("Palette");
    for (int n = 0; n < IM_ARRAYSIZE(current_palette); n++) {
      PushID(n);
      if ((n % 8) != 0) SameLine(0.0f, GetStyle().ItemSpacing.y);

      if (ColorButton("##palette", current_palette[n], kPalButtonFlags,
                      ImVec2(20, 20)))
        color = ImVec4(current_palette[n].x, current_palette[n].y,
                       current_palette[n].z, color.w);  // Preserve alpha!

      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
          memcpy((float*)&current_palette[n], payload->Data, sizeof(float) * 3);
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
          memcpy((float*)&current_palette[n], payload->Data, sizeof(float) * 4);
        EndDragDropTarget();
      }

      PopID();
    }
    EndGroup();
    EndPopup();
  }

  return absl::OkStatus();
}

void PaletteEditor::Initialize() {}

absl::Status PaletteEditor::Load() {
  core::ScopedTimer timer("PaletteEditor::Load");
  
  if (rom()->is_loaded()) {
    // Initialize the labels
    for (int i = 0; i < kNumPalettes; i++) {
      rom()->resource_label()->CreateOrGetLabel(
          "Palette Group Name", std::to_string(i),
          std::string(kPaletteGroupNames[i]));
    }
  } else {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::Update() {
  static int current_palette_group = 0;
  if (BeginTable("paletteGroupsTable", 3, kPaletteTableFlags)) {
    TableSetupColumn("Categories", ImGuiTableColumnFlags_WidthFixed, 200);
    TableSetupColumn("Palette Editor", ImGuiTableColumnFlags_WidthStretch);
    TableSetupColumn("Quick Access", ImGuiTableColumnFlags_WidthStretch);
    TableHeadersRow();

    TableNextRow();
    TableNextColumn();

    static int selected_category = 0;
    BeginChild("CategoryList", ImVec2(0, GetContentRegionAvail().y), true);

    for (int i = 0; i < kNumPalettes; i++) {
      const bool is_selected = (selected_category == i);
      if (Selectable(std::string(kPaletteCategoryNames[i]).c_str(),
                     is_selected)) {
        selected_category = i;
      }
    }

    EndChild();

    TableNextColumn();
    BeginChild("PaletteEditor", ImVec2(0, 0), true);

    Text("%s", std::string(kPaletteCategoryNames[selected_category]).c_str());

    Separator();

    if (rom()->is_loaded()) {
      status_ = DrawPaletteGroup(selected_category, true);
    }

    EndChild();

    TableNextColumn();
    DrawQuickAccessTab();

    EndTable();
  }

  return absl::OkStatus();
}

void PaletteEditor::DrawQuickAccessTab() {
  BeginChild("QuickAccessPalettes", ImVec2(0, 0), true);

  Text("Custom Palette");
  DrawCustomPalette();

  Separator();

  // Current color picker with more options
  BeginGroup();
  Text("Current Color");
  gui::SnesColorEdit4("##CurrentColorPicker", &current_color_,
                      kColorPopupFlags);

  char buf[64];
  auto col = current_color_.rgb();
  int cr = F32_TO_INT8_SAT(col.x / 255.0f);
  int cg = F32_TO_INT8_SAT(col.y / 255.0f);
  int cb = F32_TO_INT8_SAT(col.z / 255.0f);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "RGB: %d, %d, %d", cr, cg, cb);
  Text("%s", buf);

  CustomFormatString(buf, IM_ARRAYSIZE(buf), "SNES: $%04X",
                     current_color_.snes());
  Text("%s", buf);

  if (Button("Copy to Clipboard")) {
    SetClipboardText(buf);
  }
  EndGroup();

  Separator();

  // Recently used colors
  Text("Recently Used Colors");
  for (int i = 0; i < recently_used_colors_.size(); i++) {
    PushID(i);
    if (i % 8 != 0) SameLine();
    ImVec4 displayColor =
        gui::ConvertSnesColorToImVec4(recently_used_colors_[i]);
    if (ImGui::ColorButton("##recent", displayColor)) {
      // Set as current color
      current_color_ = recently_used_colors_[i];
    }
    PopID();
  }

  EndChild();
}

void PaletteEditor::DrawCustomPalette() {
  if (BeginChild("ColorPalette", ImVec2(0, 40), ImGuiChildFlags_None,
                 ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int i = 0; i < custom_palette_.size(); i++) {
      PushID(i);
      if (i > 0) SameLine(0.0f, GetStyle().ItemSpacing.y);

      // Add a context menu to each color
      ImVec4 displayColor = gui::ConvertSnesColorToImVec4(custom_palette_[i]);
      bool open_color_picker = ImGui::ColorButton(
          absl::StrFormat("##customPal%d", i).c_str(), displayColor);

      if (open_color_picker) {
        current_color_ = custom_palette_[i];
        edit_palette_index_ = i;
        ImGui::OpenPopup("CustomPaletteColorEdit");
      }

      if (BeginPopupContextItem()) {
        // Edit color directly in the popup
        SnesColor original_color = custom_palette_[i];
        if (gui::SnesColorEdit4("Edit Color", &custom_palette_[i],
                                kColorPopupFlags)) {
          // Color was changed, add to recently used
          AddRecentlyUsedColor(custom_palette_[i]);
        }

        if (Button("Delete", ImVec2(-1, 0))) {
          custom_palette_.erase(custom_palette_.begin() + i);
        }
      }

      // Handle drag/drop for palette rearrangement
      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
          ImVec4 color;
          memcpy((float*)&color, payload->Data, sizeof(float) * 3);
          color.w = 1.0f;  // Set alpha to 1.0
          custom_palette_[i] = SnesColor(color);
          AddRecentlyUsedColor(custom_palette_[i]);
        }
        EndDragDropTarget();
      }

      PopID();
    }

    SameLine();
    if (ImGui::Button("+")) {
      custom_palette_.push_back(SnesColor(0x7FFF));
    }

    SameLine();
    if (ImGui::Button("Clear")) {
      custom_palette_.clear();
    }

    SameLine();
    if (ImGui::Button("Export")) {
      std::string clipboard;
      for (const auto& color : custom_palette_) {
        clipboard += absl::StrFormat("$%04X,", color.snes());
      }
      SetClipboardText(clipboard.c_str());
    }
  }
  EndChild();

  // Color picker popup for custom palette editing
  if (ImGui::BeginPopup("CustomPaletteColorEdit")) {
    if (edit_palette_index_ >= 0 &&
        edit_palette_index_ < custom_palette_.size()) {
      SnesColor original_color = custom_palette_[edit_palette_index_];
      if (gui::SnesColorEdit4(
              "Edit Color", &custom_palette_[edit_palette_index_],
              kColorPopupFlags | ImGuiColorEditFlags_PickerHueWheel)) {
        // Color was changed, add to recently used
        AddRecentlyUsedColor(custom_palette_[edit_palette_index_]);
      }
    }
    ImGui::EndPopup();
  }
}

absl::Status PaletteEditor::DrawPaletteGroup(int category, bool right_side) {
  if (!rom()->is_loaded()) {
    return absl::NotFoundError("ROM not open, no palettes to display");
  }

  auto palette_group_name = kPaletteGroupNames[category];
  gfx::PaletteGroup* palette_group =
      rom()->mutable_palette_group()->get_group(palette_group_name.data());
  const auto size = palette_group->size();

  for (int j = 0; j < size; j++) {
    gfx::SnesPalette* palette = palette_group->mutable_palette(j);
    auto pal_size = palette->size();

    BeginGroup();

    PushID(j);
    BeginGroup();
    rom()->resource_label()->SelectableLabelWithNameEdit(
        false, palette_group_name.data(), /*key=*/std::to_string(j),
        "Unnamed Palette");
    EndGroup();

    for (int n = 0; n < pal_size; n++) {
      PushID(n);
      if (n > 0 && n % 8 != 0) SameLine(0.0f, 2.0f);

      auto popup_id =
          absl::StrCat(kPaletteCategoryNames[category].data(), j, "_", n);

      ImVec4 displayColor = gui::ConvertSnesColorToImVec4((*palette)[n]);
      if (ImGui::ColorButton(popup_id.c_str(), displayColor)) {
        current_color_ = (*palette)[n];
        AddRecentlyUsedColor(current_color_);
      }

      if (BeginPopupContextItem(popup_id.c_str())) {
        RETURN_IF_ERROR(HandleColorPopup(*palette, category, j, n))
      }
      PopID();
    }
    PopID();
    EndGroup();

    if (j < size - 1) {
      Separator();
    }
  }
  return absl::OkStatus();
}

void PaletteEditor::AddRecentlyUsedColor(const SnesColor& color) {
  // Check if color already exists in recently used
  auto it = std::find_if(
      recently_used_colors_.begin(), recently_used_colors_.end(),
      [&color](const SnesColor& c) { return c.snes() == color.snes(); });

  // If found, remove it to re-add at front
  if (it != recently_used_colors_.end()) {
    recently_used_colors_.erase(it);
  }

  // Add at front
  recently_used_colors_.insert(recently_used_colors_.begin(), color);

  // Limit size
  if (recently_used_colors_.size() > 16) {
    recently_used_colors_.pop_back();
  }
}

absl::Status PaletteEditor::HandleColorPopup(gfx::SnesPalette& palette, int i,
                                             int j, int n) {
  auto col = gfx::ToFloatArray(palette[n]);
  auto original_color = palette[n];

  if (gui::SnesColorEdit4("Edit Color", &palette[n], kColorPopupFlags)) {
    history_.RecordChange(/*group_name=*/std::string(kPaletteGroupNames[i]),
                          /*palette_index=*/j, /*color_index=*/n,
                          original_color, palette[n]);
    palette[n].set_modified(true);

    // Add to recently used colors
    AddRecentlyUsedColor(palette[n]);
  }

  // Color information display
  char buf[64];
  int cr = F32_TO_INT8_SAT(col[0]);
  int cg = F32_TO_INT8_SAT(col[1]);
  int cb = F32_TO_INT8_SAT(col[2]);

  Text("RGB: %d, %d, %d", cr, cg, cb);
  Text("SNES: $%04X", palette[n].snes());

  Separator();

  if (Button("Copy as..", ImVec2(-1, 0))) OpenPopup("Copy");
  if (BeginPopup("Copy")) {
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%.3ff, %.3ff, %.3ff)", col[0],
                       col[1], col[2]);
    if (Selectable(buf)) SetClipboardText(buf);

    CustomFormatString(buf, IM_ARRAYSIZE(buf), "(%d,%d,%d)", cr, cg, cb);
    if (Selectable(buf)) SetClipboardText(buf);

    CustomFormatString(buf, IM_ARRAYSIZE(buf), "#%02X%02X%02X", cr, cg, cb);
    if (Selectable(buf)) SetClipboardText(buf);

    // SNES Format
    CustomFormatString(buf, IM_ARRAYSIZE(buf), "$%04X",
                       ConvertRgbToSnes(ImVec4(col[0], col[1], col[2], 1.0f)));
    if (Selectable(buf)) SetClipboardText(buf);

    EndPopup();
  }

  // Add a button to add this color to custom palette
  if (Button("Add to Custom Palette", ImVec2(-1, 0))) {
    custom_palette_.push_back(palette[n]);
  }

  EndPopup();
  return absl::OkStatus();
}

absl::Status PaletteEditor::EditColorInPalette(gfx::SnesPalette& palette,
                                               int index) {
  if (index >= palette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }

  // Get the current color
  auto color = palette[index];
  auto currentColor = color.rgb();
  if (ColorPicker4("Color Picker", (float*)&palette[index])) {
    // The color was modified, update it in the palette
    palette[index] = gui::ConvertImVec4ToSnesColor(currentColor);

    // Add to recently used colors
    AddRecentlyUsedColor(palette[index]);
  }
  return absl::OkStatus();
}

absl::Status PaletteEditor::ResetColorToOriginal(
    gfx::SnesPalette& palette, int index,
    const gfx::SnesPalette& originalPalette) {
  if (index >= palette.size() || index >= originalPalette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }
  auto color = originalPalette[index];
  auto originalColor = color.rgb();
  palette[index] = gui::ConvertImVec4ToSnesColor(originalColor);
  return absl::OkStatus();
}

}  // namespace editor
}  // namespace yaze
