#include "palette_editor.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "app/gfx/snes_palette.h"
#include "app/gui/color.h"
#include "app/gui/style.h"
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
using ImGui::TableSetColumnIndex;
using ImGui::TableSetupColumn;
using ImGui::Text;
using ImGui::TreeNode;
using ImGui::TreePop;

using namespace gfx;

constexpr ImGuiTableFlags kPaletteTableFlags =
    ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable |
    ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_Hideable;

constexpr ImGuiColorEditFlags kPalNoAlpha = ImGuiColorEditFlags_NoAlpha;

constexpr ImGuiColorEditFlags kPalButtonFlags2 = ImGuiColorEditFlags_NoAlpha |
                                                 ImGuiColorEditFlags_NoPicker |
                                                 ImGuiColorEditFlags_NoTooltip;

constexpr ImGuiColorEditFlags kColorPopupFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha;

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
      ASSIGN_OR_RETURN(auto color, palette.GetColor(n));
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

      if (ColorButton("##palette", current_palette[n], kPalButtonFlags2,
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
  if (BeginTable("paletteEditorTable", 2, kPaletteTableFlags, ImVec2(0, 0))) {
    TableSetupColumn("Palette Groups", ImGuiTableColumnFlags_WidthStretch,
                     GetContentRegionAvail().x);
    TableSetupColumn("Palette Sets and Metadata",
                     ImGuiTableColumnFlags_WidthStretch,
                     GetContentRegionAvail().x);
    TableHeadersRow();
    TableNextRow();
    TableNextColumn();
    DrawModifiedColors();

    DrawCustomPalette();
    Separator();
    gui::SnesColorEdit4("Current Color Picker", &current_color_,
                        ImGuiColorEditFlags_NoAlpha);
    Separator();
    DisplayCategoryTable();

    TableNextColumn();
    gfx_group_editor_.DrawPaletteViewer();
    Separator();
    static bool in_use = false;
    ImGui::Checkbox("Palette in use? ", &in_use);
    Separator();
    static std::string palette_notes = "Notes about the palette";
    ImGui::InputTextMultiline("Notes", palette_notes.data(), 1024,
                              ImVec2(-1, ImGui::GetTextLineHeight() * 4),
                              ImGuiInputTextFlags_AllowTabInput);

    EndTable();
  }

  CLEAR_AND_RETURN_STATUS(status_)

  return absl::OkStatus();
}

void PaletteEditor::DrawCustomPalette() {
  if (BeginChild("ColorPalette", ImVec2(0, 40), true,
                 ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int i = 0; i < custom_palette_.size(); i++) {
      PushID(i);
      SameLine(0.0f, GetStyle().ItemSpacing.y);
      gui::SnesColorEdit4("##customPalette", &custom_palette_[i],
                          ImGuiColorEditFlags_NoInputs);
      // Accept a drag drop target which adds a color to the custom_palette_
      if (BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F)) {
          ImVec4 color = ImVec4(0, 0, 0, 1.0f);
          memcpy((float*)&color, payload->Data, sizeof(float));
          custom_palette_.push_back(SnesColor(color));
        }
        EndDragDropTarget();
      }

      PopID();
    }
    SameLine();
    if (ImGui::Button("Add Color")) {
      custom_palette_.push_back(SnesColor(0x7FFF));
    }
    SameLine();
    if (ImGui::Button("Export to Clipboard")) {
      std::string clipboard;
      for (const auto& color : custom_palette_) {
        clipboard += absl::StrFormat("$%04X,", color.snes());
      }
      SetClipboardText(clipboard.c_str());
    }
  }
  EndChild();
}

void PaletteEditor::DisplayCategoryTable() {
  if (BeginTable("Category Table", 8,
                 ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                     ImGuiTableFlags_SizingStretchSame |
                     ImGuiTableFlags_Hideable,
                 ImVec2(0, 0))) {
    TableSetupColumn("Weapons and Gear");
    TableSetupColumn("Overworld and Area Colors");
    TableSetupColumn("Global Sprites");
    TableSetupColumn("Sprites Aux1");
    TableSetupColumn("Sprites Aux2");
    TableSetupColumn("Sprites Aux3");
    TableSetupColumn("Maps and Items");
    TableSetupColumn("Dungeons");
    TableHeadersRow();
    TableNextRow();

    TableSetColumnIndex(0);
    if (TreeNode("Sword")) {
      status_ = DrawPaletteGroup(PaletteCategory::kSword);
      TreePop();
    }
    if (TreeNode("Shield")) {
      status_ = DrawPaletteGroup(PaletteCategory::kShield);
      TreePop();
    }
    if (TreeNode("Clothes")) {
      status_ = DrawPaletteGroup(PaletteCategory::kClothes, true);
      TreePop();
    }

    TableSetColumnIndex(1);
    gui::BeginChildWithScrollbar("##WorldPaletteScrollRegion");
    if (TreeNode("World Colors")) {
      status_ = DrawPaletteGroup(PaletteCategory::kWorldColors);
      TreePop();
    }
    if (TreeNode("Area Colors")) {
      status_ = DrawPaletteGroup(PaletteCategory::kAreaColors);
      TreePop();
    }
    EndChild();

    TableSetColumnIndex(2);
    status_ = DrawPaletteGroup(PaletteCategory::kGlobalSprites, true);

    TableSetColumnIndex(3);
    status_ = DrawPaletteGroup(PaletteCategory::kSpritesAux1);

    TableSetColumnIndex(4);
    status_ = DrawPaletteGroup(PaletteCategory::kSpritesAux2);

    TableSetColumnIndex(5);
    status_ = DrawPaletteGroup(PaletteCategory::kSpritesAux3);

    TableSetColumnIndex(6);
    gui::BeginChildWithScrollbar("##MapPaletteScrollRegion");
    if (TreeNode("World Map")) {
      status_ = DrawPaletteGroup(PaletteCategory::kWorldMap, true);
      TreePop();
    }
    if (TreeNode("Dungeon Map")) {
      status_ = DrawPaletteGroup(PaletteCategory::kDungeonMap);
      TreePop();
    }
    if (TreeNode("Triforce")) {
      status_ = DrawPaletteGroup(PaletteCategory::kTriforce);
      TreePop();
    }
    if (TreeNode("Crystal")) {
      status_ = DrawPaletteGroup(PaletteCategory::kCrystal);
      TreePop();
    }
    EndChild();

    TableSetColumnIndex(7);
    gui::BeginChildWithScrollbar("##DungeonPaletteScrollRegion");
    status_ = DrawPaletteGroup(PaletteCategory::kDungeons, true);
    EndChild();

    EndTable();
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

  static bool edit_color = false;
  for (int j = 0; j < size; j++) {
    gfx::SnesPalette* palette = palette_group->mutable_palette(j);
    auto pal_size = palette->size();

    for (int n = 0; n < pal_size; n++) {
      PushID(n);
      if (!right_side) {
        if ((n % 7) != 0) SameLine(0.0f, GetStyle().ItemSpacing.y);
      } else {
        if ((n % 15) != 0) SameLine(0.0f, GetStyle().ItemSpacing.y);
      }

      auto popup_id =
          absl::StrCat(kPaletteCategoryNames[category].data(), j, "_", n);

      // Small icon of the color in the palette
      if (gui::SnesColorButton(popup_id, *palette->mutable_color(n),
                               kPalNoAlpha)) {
        ASSIGN_OR_RETURN(current_color_, palette->GetColor(n));
      }

      if (BeginPopupContextItem(popup_id.c_str())) {
        RETURN_IF_ERROR(HandleColorPopup(*palette, category, j, n))
      }
      PopID();
    }
    SameLine();
    rom()->resource_label()->SelectableLabelWithNameEdit(
        false, palette_group_name.data(), /*key=*/std::to_string(j),
        "Unnamed Palette");
    if (right_side) Separator();
  }
  return absl::OkStatus();
}

void PaletteEditor::DrawModifiedColors() {
  if (BeginChild("ModifiedColors", ImVec2(0, 100), true,
                 ImGuiWindowFlags_HorizontalScrollbar)) {
    for (int i = 0; i < history_.size(); i++) {
      PushID(i);
      gui::SnesColorEdit4("Original ", &history_.GetOriginalColor(i),
                          ImGuiColorEditFlags_NoInputs);
      SameLine(0.0f, GetStyle().ItemSpacing.y);
      gui::SnesColorEdit4("Modified ", &history_.GetModifiedColor(i),
                          ImGuiColorEditFlags_NoInputs);
      PopID();
    }
  }
  EndChild();
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
  }

  if (Button("Copy as..", ImVec2(-1, 0))) OpenPopup("Copy");
  if (BeginPopup("Copy")) {
    int cr = F32_TO_INT8_SAT(col[0]);
    int cg = F32_TO_INT8_SAT(col[1]);
    int cb = F32_TO_INT8_SAT(col[2]);
    char buf[64];

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

  EndPopup();
  return absl::OkStatus();
}

absl::Status PaletteEditor::EditColorInPalette(gfx::SnesPalette& palette,
                                               int index) {
  if (index >= palette.size()) {
    return absl::InvalidArgumentError("Index out of bounds");
  }

  // Get the current color
  ASSIGN_OR_RETURN(auto color, palette.GetColor(index));
  auto currentColor = color.rgb();
  if (ColorPicker4("Color Picker", (float*)&palette[index])) {
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

}  // namespace editor
}  // namespace yaze
