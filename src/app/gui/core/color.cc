#include "color.h"

#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Convert SnesColor to standard ImVec4 for display
 * 
 * IMPORTANT: SnesColor.rgb() returns 0-255 values in ImVec4 (unconventional!)
 * This function converts them to standard ImGui format (0.0-1.0)
 * 
 * @param color SnesColor with internal 0-255 storage
 * @return ImVec4 with standard 0.0-1.0 RGBA values for ImGui
 */
ImVec4 ConvertSnesColorToImVec4(const gfx::SnesColor& color) {
  // SnesColor stores RGB as 0-255 in ImVec4, convert to standard 0-1 range
  ImVec4 rgb_255 = color.rgb();
  return ImVec4(rgb_255.x / 255.0f, rgb_255.y / 255.0f,
                rgb_255.z / 255.0f, 1.0f);
}

/**
 * @brief Convert standard ImVec4 to SnesColor
 * 
 * @param color ImVec4 with standard 0.0-1.0 RGBA values
 * @return SnesColor with converted values
 */
gfx::SnesColor ConvertImVec4ToSnesColor(const ImVec4& color) {
  // SnesColor constructor expects 0-1 range and handles conversion internally
  return gfx::SnesColor(color);
}

IMGUI_API bool SnesColorButton(absl::string_view id, gfx::SnesColor& color,
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

IMGUI_API bool SnesColorEdit4(absl::string_view label, gfx::SnesColor* color,
                              ImGuiColorEditFlags flags) {
  // Convert from internal 0-255 storage to 0-1 for ImGui
  ImVec4 displayColor = ConvertSnesColorToImVec4(*color);

  // Call the original ImGui::ColorEdit4 with the converted color
  bool changed =
      ImGui::ColorEdit4(label.data(), (float*)&displayColor.x, flags);

  // Only update if the user actually changed the color
  if (changed) {
    // set_rgb() handles conversion from 0-1 (ImGui) to 0-255 (internal)
    // and automatically calculates snes_ value - no need to call set_snes separately
    color->set_rgb(displayColor);
  }

  return changed;
}

// ============================================================================
// New Standardized Palette Widgets
// ============================================================================

IMGUI_API bool InlinePaletteSelector(gfx::SnesPalette &palette, 
                                     int num_colors,
                                     int* selected_index) {
  bool selection_made = false;
  int colors_to_show = std::min(num_colors, static_cast<int>(palette.size()));
  
  ImGui::BeginGroup();
  for (int n = 0; n < colors_to_show; n++) {
    ImGui::PushID(n);
    if (n > 0 && (n % 8) != 0) {
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
    }
    
    bool is_selected = selected_index && (*selected_index == n);
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    }
    
    if (SnesColorButton("##palettesel", palette[n],
                       ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
                       ImVec2(20, 20))) {
      if (selected_index) {
        *selected_index = n;
        selection_made = true;
      }
    }
    
    if (is_selected) {
      ImGui::PopStyleVar();
      ImGui::PopStyleColor();
    }
    
    ImGui::PopID();
  }
  ImGui::EndGroup();
  
  return selection_made;
}

IMGUI_API absl::Status InlinePaletteEditor(gfx::SnesPalette &palette,
                                           const std::string &title,
                                           ImGuiColorEditFlags flags) {
  if (!title.empty()) {
    ImGui::Text("%s", title.c_str());
  }
  
  static int selected_color = 0;
  static ImVec4 current_color = ImVec4(0, 0, 0, 1.0f);
  
  // Color picker
  ImGui::Separator();
  if (ImGui::ColorPicker4("##colorpicker", (float*)&current_color,
                          ImGuiColorEditFlags_NoSidePreview |
                          ImGuiColorEditFlags_NoSmallPreview)) {
    gfx::SnesColor snes_color(current_color);
    palette.UpdateColor(selected_color, snes_color);
  }
  
  ImGui::Separator();
  
  // Palette grid
  ImGui::BeginGroup();
  for (int n = 0; n < palette.size(); n++) {
    ImGui::PushID(n);
    if ((n % 8) != 0) {
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
    }
    
    if (flags == 0) {
      flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker;
    }
    
    if (SnesColorButton("##palettedit", palette[n], flags, ImVec2(20, 20))) {
      selected_color = n;
      current_color = ConvertSnesColorToImVec4(palette[n]);
    }
    
    // Context menu
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Copy as SNES")) {
        std::string clipboard = absl::StrFormat("$%04X", palette[n].snes());
        ImGui::SetClipboardText(clipboard.c_str());
      }
      if (ImGui::MenuItem("Copy as RGB")) {
        auto rgb = palette[n].rgb();
        std::string clipboard = absl::StrFormat("(%d,%d,%d)", 
                                                (int)rgb.x, (int)rgb.y, (int)rgb.z);
        ImGui::SetClipboardText(clipboard.c_str());
      }
      if (ImGui::MenuItem("Copy as Hex")) {
        auto rgb = palette[n].rgb();
        std::string clipboard = absl::StrFormat("#%02X%02X%02X", 
                                                (int)rgb.x, (int)rgb.y, (int)rgb.z);
        ImGui::SetClipboardText(clipboard.c_str());
      }
      ImGui::EndPopup();
    }
    
    ImGui::PopID();
  }
  ImGui::EndGroup();
  
  return absl::OkStatus();
}

IMGUI_API bool PopupPaletteEditor(const char* popup_id,
                                  gfx::SnesPalette &palette,
                                  ImGuiColorEditFlags flags) {
  bool modified = false;
  
  if (ImGui::BeginPopup(popup_id)) {
    static int selected_color = 0;
    static ImVec4 current_color = ImVec4(0, 0, 0, 1.0f);
    
    // Compact color picker
    if (ImGui::ColorPicker4("##popuppicker", (float*)&current_color,
                            ImGuiColorEditFlags_NoSidePreview |
                            ImGuiColorEditFlags_NoSmallPreview)) {
      gfx::SnesColor snes_color(current_color);
      palette.UpdateColor(selected_color, snes_color);
      modified = true;
    }
    
    ImGui::Separator();
    
    // Palette grid
    ImGui::BeginGroup();
    for (int n = 0; n < palette.size() && n < 64; n++) {  // Limit to 64 for popup
      ImGui::PushID(n);
      if ((n % 8) != 0) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
      }
      
      if (SnesColorButton("##popuppal", palette[n], 
                         ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker,
                         ImVec2(20, 20))) {
        selected_color = n;
        current_color = ConvertSnesColorToImVec4(palette[n]);
      }
      
      ImGui::PopID();
    }
    ImGui::EndGroup();
    
    ImGui::EndPopup();
  }
  
  return modified;
}

// ============================================================================
// Legacy Functions (for compatibility)
// ============================================================================

IMGUI_API bool DisplayPalette(gfx::SnesPalette& palette, bool loaded) {
  static ImVec4 color = ImVec4(0, 0, 0, 255.f);
  ImGuiColorEditFlags misc_flags = ImGuiColorEditFlags_AlphaPreview |
                                   ImGuiColorEditFlags_NoDragDrop |
                                   ImGuiColorEditFlags_NoOptions;

  // Generate a default palette. The palette will persist and can be edited.
  static bool init = false;
  static ImVec4 saved_palette[32] = {};
  if (loaded && !init) {
    for (int n = 0; n < palette.size(); n++) {
      auto color = palette[n];
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
  return true;
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

absl::Status DisplayEditablePalette(gfx::SnesPalette& palette,
                                    const std::string& title,
                                    bool show_color_picker, int colors_per_row,
                                    ImGuiColorEditFlags flags) {
  // Default flags if none provided
  if (flags == 0) {
    flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker |
            ImGuiColorEditFlags_NoTooltip;
  }

  // Display title if provided
  if (!title.empty()) {
    ImGui::Text("%s", title.c_str());
  }
  static int selected_color = 0;

  if (show_color_picker) {
    ImGui::Separator();
    static ImVec4 current_color = ImVec4(0, 0, 0, 1.0f);

    if (ImGui::ColorPicker4("Color Picker", (float*)&current_color,
                            ImGuiColorEditFlags_NoSidePreview |
                                ImGuiColorEditFlags_NoSmallPreview)) {
      // Convert the selected color to SNES format and add it to the palette
      gfx::SnesColor snes_color(current_color);
      palette.UpdateColor(selected_color, snes_color);
    }
  }

  // Display the palette colors in a grid
  ImGui::BeginGroup();  // Lock X position
  for (int n = 0; n < palette.size(); n++) {
    ImGui::PushID(n);
    if ((n % colors_per_row) != 0) {
      ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);
    }

    // Create a unique ID for this color button
    std::string button_id = "##palette_" + std::to_string(n);

    // Display the color button
    if (SnesColorButton(button_id, palette[n], flags, ImVec2(20, 20))) {
      // Color was clicked, could be used to select this color
      selected_color = n;
    }

    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Edit Color")) {
        // Open color picker for this color
        ImGui::OpenPopup(("Edit Color##" + std::to_string(n)).c_str());
      }

      if (ImGui::MenuItem("Copy as SNES Value")) {
        std::string clipboard = absl::StrFormat("$%04X", palette[n].snes());
        ImGui::SetClipboardText(clipboard.c_str());
      }

      if (ImGui::MenuItem("Copy as RGB")) {
        auto rgb = palette[n].rgb();
        // rgb is already in 0-255 range, no need to multiply
        std::string clipboard =
            absl::StrFormat("(%d,%d,%d)", (int)rgb.x, (int)rgb.y, (int)rgb.z);
        ImGui::SetClipboardText(clipboard.c_str());
      }

      if (ImGui::MenuItem("Copy as Hex")) {
        auto rgb = palette[n].rgb();
        // rgb is already in 0-255 range, no need to multiply
        std::string clipboard =
            absl::StrFormat("#%02X%02X%02X", (int)rgb.x, (int)rgb.y, (int)rgb.z);
        ImGui::SetClipboardText(clipboard.c_str());
      }

      ImGui::EndPopup();
    }

    // Color picker popup
    if (ImGui::BeginPopup(("Edit Color##" + std::to_string(n)).c_str())) {
      ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_NoSidePreview |
                                         ImGuiColorEditFlags_NoSmallPreview;

      ImVec4 color = ConvertSnesColorToImVec4(palette[n]);
      if (ImGui::ColorPicker4("##picker", (float*)&color, picker_flags)) {
        // Update the SNES color when the picker changes
        palette[n] = ConvertImVec4ToSnesColor(color);
      }

      ImGui::EndPopup();
    }

    ImGui::PopID();
  }
  ImGui::EndGroup();

  return absl::OkStatus();
}

IMGUI_API bool PaletteColorButton(const char* id, const gfx::SnesColor& color,
                                  bool is_selected, bool is_modified,
                                  const ImVec2& size,
                                  ImGuiColorEditFlags flags) {
  ImVec4 display_color = ConvertSnesColorToImVec4(color);
  
  // Add visual indicators for selection and modification
  ImGui::PushID(id);
  
  // Selection border
  if (is_selected) {
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
  }
  
  bool clicked = ImGui::ColorButton(id, display_color, flags, size);
  
  if (is_selected) {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
  }
  
  // Modification indicator (small dot in corner)
  if (is_modified) {
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 dot_pos = ImVec2(pos.x + size.x - 6, pos.y + 2);
    ImGui::GetWindowDrawList()->AddCircleFilled(dot_pos, 3.0f, 
                                                IM_COL32(255, 128, 0, 255));
  }
  
  // Tooltip with color info
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("SNES: $%04X", color.snes());
    auto rgb = color.rgb();
    ImGui::Text("RGB: (%d, %d, %d)", 
                static_cast<int>(rgb.x), 
                static_cast<int>(rgb.y), 
                static_cast<int>(rgb.z));
    if (is_modified) {
      ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Modified");
    }
    ImGui::EndTooltip();
  }
  
  ImGui::PopID();
  return clicked;
}

}  // namespace gui
}  // namespace yaze
